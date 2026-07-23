#include "MediaMetadataParser.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <algorithm>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>

// ⚡ 核心防呆：Windows 底層必須用 UTF-16 wchar_t 傳遞路徑，否則日文直接亂碼找不到檔案！
#ifdef _WIN32
#define TAGLIB_PATH(x) reinterpret_cast<const wchar_t*>(x.utf16())
#else
#define TAGLIB_PATH(x) x.toUtf8().constData()
#endif

TrackInfo MediaMetadataParser::parse(const QString& filePath) {
    TrackInfo info;
    info.path = filePath;
    info.title = QFileInfo(filePath).baseName(); 
    info.artist = "UNKNOWN ARTIST";
    info.album = "UNKNOWN ALBUM";
    
    QString ext = QFileInfo(filePath).suffix().toLower();
    info.specs = ext.toUpper();
    QString rawLyrics = "";

    // 1. 基礎標籤與音訊規格解析
    {
        // ⚡ 加上作用域 {} 強制釋放檔案鎖！避免跟後面的 FLAC::File 互相卡死！
        TagLib::FileRef f(TAGLIB_PATH(filePath)); 
        if (!f.isNull() && f.tag()) {
            if (!f.tag()->title().isEmpty()) info.title = QString::fromStdWString(f.tag()->title().toWString());
            if (!f.tag()->artist().isEmpty()) info.artist = QString::fromStdWString(f.tag()->artist().toWString());
            if (!f.tag()->album().isEmpty()) info.album = QString::fromStdWString(f.tag()->album().toWString());
            
            rawLyrics = QString::fromStdWString(f.tag()->comment().toWString());
            
            if (f.audioProperties()) { 
                int br = f.audioProperties()->bitrate(); 
                int sr = f.audioProperties()->sampleRate(); 
                int len = f.audioProperties()->lengthInSeconds(); 
                info.specs = QString("%1 | %2kHz | %3kbps | %4:%5")
                                .arg(ext.toUpper())
                                .arg(sr / 1000.0, 0, 'f', 1)
                                .arg(br)
                                .arg(len / 60, 2, 10, QChar('0'))
                                .arg(len % 60, 2, 10, QChar('0'));
            }
        }
    }

    // 2. 深度特化解析 (包含封面與專屬歌詞)
    if (ext == "flac") { 
        TagLib::FLAC::File flacFile(TAGLIB_PATH(filePath)); 
        if (flacFile.isValid() && !flacFile.pictureList().isEmpty()) { 
            auto pic = flacFile.pictureList().front(); 
            info.coverImg.loadFromData((const uchar*)pic->data().data(), pic->data().size()); 
        } 
        if (flacFile.isValid() && flacFile.xiphComment() && flacFile.xiphComment()->contains("LYRICS")) { 
            rawLyrics = QString::fromStdWString(flacFile.xiphComment()->fieldListMap()["LYRICS"].front().toWString()); 
        } 
    } 
    else if (ext == "mp3") { 
        TagLib::MPEG::File mpegFile(TAGLIB_PATH(filePath)); 
        if (mpegFile.isValid() && mpegFile.ID3v2Tag()) { 
            if (!mpegFile.ID3v2Tag()->frameListMap()["APIC"].isEmpty()) { 
                auto pic = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(mpegFile.ID3v2Tag()->frameListMap()["APIC"].front()); 
                info.coverImg.loadFromData((const uchar*)pic->picture().data(), pic->picture().size()); 
            } 
            if (!mpegFile.ID3v2Tag()->frameListMap()["USLT"].isEmpty()) { 
                auto txt = static_cast<TagLib::ID3v2::UnsynchronizedLyricsFrame*>(mpegFile.ID3v2Tag()->frameListMap()["USLT"].front()); 
                rawLyrics = QString::fromStdWString(txt->text().toWString()); 
            } 
        } 
    } 
    else if (ext == "m4a") { 
        TagLib::MP4::File m4aFile(TAGLIB_PATH(filePath)); 
        if (m4aFile.isValid() && m4aFile.tag() && m4aFile.tag()->itemMap().contains("covr")) { 
            auto coverList = m4aFile.tag()->itemMap()["covr"].toCoverArtList(); 
            if (!coverList.isEmpty()) { 
                auto pic = coverList.front(); 
                info.coverImg.loadFromData((const uchar*)pic.data().data(), pic.data().size()); 
            } 
        } 
    }

    if (!info.coverImg.isNull() && (info.coverImg.width() > 4500 || info.coverImg.height() > 4500)) { 
        info.coverImg = info.coverImg.scaled(4500, 4500, Qt::KeepAspectRatio, Qt::SmoothTransformation); 
    }

    extractLyrics(rawLyrics, info.lyrics);
    return info;
}

TrackInfo MediaMetadataParser::parseDSD(const QString& filePath) {
    TrackInfo info;
    info.path = filePath;
    info.title = QFileInfo(filePath).baseName();
    info.artist = "DSD/PDM STREAM";
    info.specs = "DSD | DIRECT BITSTREAM";
    return info;
}

void MediaMetadataParser::extractLyrics(const QString& rawLyrics, std::vector<LyricLine>& outLyrics) {
    outLyrics.clear();
    if (rawLyrics.trimmed().isEmpty()) { 
        outLyrics.push_back({-1.0f, "SYSTEM: NO LYRICS DATA AVAILABLE"}); 
        return; 
    }
    
    QStringList lines = rawLyrics.split('\n'); 
    QRegularExpression re("\\[(\\d+):(\\d+(?:\\.\\d+)?)\\]"); 
    bool hasTimecode = false;
    
    for (const QString& line : lines) { 
        QRegularExpressionMatchIterator i = re.globalMatch(line); 
        QString text = line; 
        text = text.remove(re).trimmed(); 
        if (i.hasNext()) { 
            hasTimecode = true; 
            while (i.hasNext()) { 
                QRegularExpressionMatch m = i.next(); 
                outLyrics.push_back({m.captured(1).toFloat() * 60.0f + m.captured(2).toFloat(), text.isEmpty() ? " " : text}); 
            } 
        } 
    }
    
    if (!hasTimecode) { 
        for (const QString& line : lines) outLyrics.push_back({-1.0f, line}); 
    } else { 
        std::sort(outLyrics.begin(), outLyrics.end(), [](const LyricLine& a, const LyricLine& b) { return a.time < b.time; }); 
    }
}