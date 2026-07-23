#pragma once
#ifndef MEDIAMETADATAPARSER_H
#define MEDIAMETADATAPARSER_H

#include <QString>
#include <QImage>
#include <vector>

// 單行歌詞結構
struct LyricLine { 
    float time; 
    QString text; 
};

// 純淨的音訊元數據封裝體
struct TrackInfo {
    QString path;
    QString title;
    QString artist;
    QString album;
    QString specs; // 例如: "FLAC | 192.0kHz | 3000kbps | 04:30"
    QImage coverImg;
    std::vector<LyricLine> lyrics;
};

class MediaMetadataParser {
public:
    static MediaMetadataParser& instance() {
        static MediaMetadataParser parser;
        return parser;
    }

    // ⚡ 核心解析入口 (支援所有主流 PCM 陣營格式)
    TrackInfo parse(const QString& filePath);
    
    // ⚡ 預留通道：未來 PDM / DSD (.dsf, .dff) 專屬解析
    TrackInfo parseDSD(const QString& filePath);

private:
    MediaMetadataParser() = default;
    ~MediaMetadataParser() = default;

    // 內部歌詞時間軸切割引擎
    void extractLyrics(const QString& rawLyrics, std::vector<LyricLine>& outLyrics);
};

#endif // MEDIAMETADATAPARSER_H