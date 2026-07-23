#include "KaedeDatabase.h"
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QMetaObject>
#include <QDateTime>
#include <QCryptographicHash>
#include <QImage>
#include <QBuffer>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/wavfile.h>
#include <taglib/dsffile.h>

KaedeDatabase::KaedeDatabase(QObject* parent) : QObject(parent) {
    m_trackModel = new KaedeTrackModel(this);
    m_albumDetailModel = new KaedeTrackModel(this); // ⚡ 初始化抽屜模型
}

KaedeDatabase::~KaedeDatabase() {
    m_isScanning = false;
    if (m_scanThread.joinable()) m_scanThread.join();
    if (m_db.isOpen()) m_db.close();
}

void KaedeDatabase::init() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/KaedeDAW";
    m_coverCachePath = dataDir + "/cache/thumbs/";
    QDir().mkpath(m_coverCachePath); 
    
    m_dbPath = dataDir + "/kaede_library.db";
    m_db = QSqlDatabase::addDatabase("QSQLITE", "MainConn");
    m_db.setDatabaseName(m_dbPath);
    
    if (!m_db.open()) return;
    
    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode = WAL;");
    q.exec("PRAGMA synchronous = NORMAL;");
    q.exec("PRAGMA mmap_size = 268435456;"); 
    q.exec("PRAGMA cache_size = -262144;");  
    q.exec("PRAGMA temp_store = MEMORY;");

    setupTables();
    
    m_watcher = new QFileSystemWatcher(this);
    m_watchDebounceTimer = new QTimer(this);
    m_watchDebounceTimer->setSingleShot(true);
    m_watchDebounceTimer->setInterval(2500); 
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &KaedeDatabase::onDirectoryChanged);
    connect(m_watchDebounceTimer, &QTimer::timeout, this, &KaedeDatabase::processPendingChanges);
    
    QSqlQuery qDir("SELECT path FROM directories", m_db);
    while (qDir.next()) {
        QString p = qDir.value(0).toString();
        m_mountedDirs.append(p);
        watchDirectoryRecursively(p);
    }

    reloadModel();
}

void KaedeDatabase::setupTables() {
    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS config (key TEXT PRIMARY KEY, value TEXT)");
    q.exec("CREATE TABLE IF NOT EXISTS directories (path TEXT PRIMARY KEY)");
    
    q.exec("CREATE TABLE IF NOT EXISTS tracks ("
           "path TEXT PRIMARY KEY, "
           "title TEXT, artist TEXT, album TEXT, genre TEXT, track_num INTEGER, "
           "specs TEXT, cover_url TEXT, size INTEGER, added_time INTEGER, last_modified INTEGER)");

    q.exec("CREATE VIRTUAL TABLE IF NOT EXISTS tracks_fts USING fts5("
           "title, artist, album, genre, path, "
           "content='tracks', content_rowid='rowid')");

    createFtsTriggers();
}

void KaedeDatabase::createFtsTriggers() {
    QSqlQuery q(m_db);
    q.exec("CREATE TRIGGER IF NOT EXISTS tracks_ai AFTER INSERT ON tracks BEGIN "
           "INSERT INTO tracks_fts(rowid, title, artist, album, genre, path) "
           "VALUES (new.rowid, new.title, new.artist, new.album, new.genre, new.path); "
           "END;");
           
    q.exec("CREATE TRIGGER IF NOT EXISTS tracks_ad AFTER DELETE ON tracks BEGIN "
           "INSERT INTO tracks_fts(tracks_fts, rowid, title, artist, album, genre, path) "
           "VALUES ('delete', old.rowid, old.title, old.artist, old.album, old.genre, old.path); "
           "END;");
           
    q.exec("CREATE TRIGGER IF NOT EXISTS tracks_au AFTER UPDATE ON tracks BEGIN "
           "INSERT INTO tracks_fts(tracks_fts, rowid, title, artist, album, genre, path) "
           "VALUES ('delete', old.rowid, old.title, old.artist, old.album, old.genre, old.path); "
           "INSERT INTO tracks_fts(rowid, title, artist, album, genre, path) "
           "VALUES (new.rowid, new.title, new.artist, new.album, new.genre, new.path); "
           "END;");
}

void KaedeDatabase::purgeDatabase() {
    if (m_isScanning) return;
    QSqlQuery q(m_db);
    q.exec("DROP TABLE IF EXISTS tracks");
    q.exec("DROP TABLE IF EXISTS tracks_fts");
    q.exec("DROP TABLE IF EXISTS directories");
    q.exec("VACUUM"); 
    
    if (m_watcher && !m_watcher->directories().isEmpty()) {
        m_watcher->removePaths(m_watcher->directories());
    }
    
    setupTables();
    m_trackModel->clear();
    m_albumDetailModel->clear();
    m_mountedDirs.clear();
    
    QDir dir(m_coverCachePath);
    for (const QString& f : dir.entryList(QStringList() << "*.jpg", QDir::Files)) dir.remove(f);
}

void KaedeDatabase::watchDirectoryRecursively(const QString& path) {
    if (!m_watcher) return;
    m_watcher->addPath(path);
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) { m_watcher->addPath(it.next()); }
}

void KaedeDatabase::onDirectoryChanged(const QString& path) {
    Q_UNUSED(path);
    if (m_watchDebounceTimer) m_watchDebounceTimer->start();
}

void KaedeDatabase::processPendingChanges() {
    if (m_isScanning.load() || m_mountedDirs.isEmpty()) return;
    for (const QString& dir : m_mountedDirs) watchDirectoryRecursively(dir);
    scanDirectory(""); 
}

QString KaedeDatabase::extractAndCacheCover(const QString& filePath) {
    QImage rawCover;
    QString ext = QFileInfo(filePath).suffix().toLower();
    
#ifdef _WIN32
    std::wstring wpath = QDir::toNativeSeparators(filePath).toStdWString();
    const wchar_t* tag_path = wpath.c_str();
#else
    QByteArray utf8path = filePath.toUtf8();
    const char* tag_path = utf8path.constData();
#endif

    if (ext == "flac") { 
        TagLib::FLAC::File flacFile(tag_path); 
        if (flacFile.isValid() && !flacFile.pictureList().isEmpty()) { 
            auto pic = flacFile.pictureList().front(); 
            rawCover.loadFromData((const uchar*)pic->data().data(), pic->data().size()); 
        } 
    } 
    else if (ext == "mp3") { 
        TagLib::MPEG::File mpegFile(tag_path); 
        if (mpegFile.isValid() && mpegFile.ID3v2Tag()) { 
            if (!mpegFile.ID3v2Tag()->frameListMap()["APIC"].isEmpty()) { 
                auto pic = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(mpegFile.ID3v2Tag()->frameListMap()["APIC"].front()); 
                rawCover.loadFromData((const uchar*)pic->picture().data(), pic->picture().size()); 
            } 
        } 
    } 
    else if (ext == "m4a") { 
        TagLib::MP4::File m4aFile(tag_path); 
        if (m4aFile.isValid() && m4aFile.tag() && m4aFile.tag()->itemMap().contains("covr")) { 
            auto coverList = m4aFile.tag()->itemMap()["covr"].toCoverArtList(); 
            if (!coverList.isEmpty()) { 
                auto pic = coverList.front(); 
                rawCover.loadFromData((const uchar*)pic.data().data(), pic.data().size()); 
            } 
        } 
    }
    else if (ext == "wav") {
        TagLib::RIFF::WAV::File wavFile(tag_path);
        if (wavFile.isValid() && wavFile.ID3v2Tag() && !wavFile.ID3v2Tag()->frameListMap()["APIC"].isEmpty()) {
            auto pic = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(wavFile.ID3v2Tag()->frameListMap()["APIC"].front());
            rawCover.loadFromData((const uchar*)pic->picture().data(), pic->picture().size());
        }
    }
    else if (ext == "dsf") {
        TagLib::DSF::File dsfFile(tag_path);
        if (dsfFile.isValid() && dsfFile.tag()) {
            if (TagLib::ID3v2::Tag *id3v2Tag = dynamic_cast<TagLib::ID3v2::Tag*>(dsfFile.tag())) {
                if (!id3v2Tag->frameListMap()["APIC"].isEmpty()) {
                    auto pic = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(id3v2Tag->frameListMap()["APIC"].front());
                    rawCover.loadFromData((const uchar*)pic->picture().data(), pic->picture().size());
                }
            }
        }
    }

    if (rawCover.isNull()) return "";
    
    QByteArray imageBytes; QBuffer buffer(&imageBytes); buffer.open(QIODevice::WriteOnly);
    
    // 👑 核改：將 300x300 提升至 600x600 高解析度，徹底解決大圖發虛問題！
    rawCover.scaled(600, 600, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation).save(&buffer, "JPG", 88);
    
    QString hashStr = QString(QCryptographicHash::hash(imageBytes, QCryptographicHash::Sha256).toHex());
    QString cacheFile = m_coverCachePath + hashStr + ".jpg";
    
    if (!QFile::exists(cacheFile)) {
        QFile file(cacheFile);
        if (file.open(QIODevice::WriteOnly)) { file.write(imageBytes); file.close(); }
    }
    return "file:///" + cacheFile;
}

void KaedeDatabase::scanDirectory(const QString& dirPath) {
    if (m_isScanning.load()) return;
    
    if (!dirPath.isEmpty() && !m_mountedDirs.contains(dirPath)) {
        m_mountedDirs.append(dirPath);
        QSqlQuery q(m_db);
        q.prepare("INSERT OR IGNORE INTO directories (path) VALUES (?)");
        q.addBindValue(dirPath);
        q.exec();
        watchDirectoryRecursively(dirPath);
    }
    
    if (m_mountedDirs.isEmpty()) return; 

    m_isScanning.store(true);
    if (m_scanThread.joinable()) m_scanThread.join();
    
    m_scanThread = std::thread(&KaedeDatabase::scanWorker, this, m_mountedDirs);
}

void KaedeDatabase::scanWorker(const QStringList& dirs) {
    emit scanStarted();
    int total = 0; int changed = 0; int removed = 0;
    
    {
        QSqlDatabase scanDb = QSqlDatabase::addDatabase("QSQLITE", "ScanConn");
        scanDb.setDatabaseName(m_dbPath);
        if (!scanDb.open()) { m_isScanning = false; emit scanFinished(0, 0); return; }
        
        scanDb.exec("PRAGMA journal_mode = WAL;");
        scanDb.exec("PRAGMA synchronous = NORMAL;");
        
        QHash<QString, qint64> existingFiles;
        QSqlQuery qExist("SELECT path, last_modified FROM tracks", scanDb);
        while (qExist.next()) {
            existingFiles[qExist.value(0).toString()] = qExist.value(1).toLongLong();
        }

        scanDb.transaction();
        QSqlQuery q(scanDb);
        q.prepare("INSERT OR REPLACE INTO tracks (path, title, artist, album, genre, track_num, specs, cover_url, size, added_time, last_modified) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
                  
        qint64 now = QDateTime::currentSecsSinceEpoch();
        
        for (const QString& dirPath : dirs) {
            QDirIterator it(dirPath, QStringList() << "*.flac" << "*.wav" << "*.mp3" << "*.dsf" << "*.dff" << "*.m4a" << "*.ape", QDir::Files, QDirIterator::Subdirectories);
            
            while (it.hasNext() && m_isScanning) {
                QString path = it.next();
                QFileInfo fi(path);
                total++;
                
                qint64 lastMod = fi.lastModified().toSecsSinceEpoch();
                
                if (existingFiles.contains(path) && existingFiles[path] == lastMod) {
                    existingFiles.remove(path); 
                    continue; 
                }
                
                QString title = fi.completeBaseName(); QString artist = "Unknown Artist"; QString album = "Unknown Album";
                QString ext = fi.suffix().toLower(); QString specs = ext.toUpper(); 
                int trackNum = 0;
                
#ifdef _WIN32
                std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
                const wchar_t* tag_path = wpath.c_str();
#else
                QByteArray utf8path = path.toUtf8();
                const char* tag_path = utf8path.constData();
#endif

                {
                    TagLib::FileRef f(tag_path); 
                    if (!f.isNull() && f.tag()) {
                        if (!f.tag()->title().isEmpty()) title = QString::fromStdWString(f.tag()->title().toWString());
                        if (!f.tag()->artist().isEmpty()) artist = QString::fromStdWString(f.tag()->artist().toWString());
                        if (!f.tag()->album().isEmpty()) album = QString::fromStdWString(f.tag()->album().toWString());
                        trackNum = f.tag()->track();
                        
                        if (f.audioProperties()) { 
                            int br = f.audioProperties()->bitrate(); 
                            int sr = f.audioProperties()->sampleRate(); 
                            int len = f.audioProperties()->lengthInSeconds(); 
                            specs = QString("%1 | %2kHz | %3kbps | %4:%5")
                                    .arg(ext.toUpper()).arg(sr / 1000.0).arg(br)
                                    .arg(len / 60, 2, 10, QChar('0')).arg(len % 60, 2, 10, QChar('0')); 
                        }
                    }
                }
                
                QString coverUrl = extractAndCacheCover(path);
                
                q.addBindValue(path);
                q.addBindValue(title); 
                q.addBindValue(artist); 
                q.addBindValue(album);
                q.addBindValue(""); 
                q.addBindValue(trackNum);
                q.addBindValue(specs);
                q.addBindValue(coverUrl); 
                q.addBindValue(fi.size());
                q.addBindValue(now); 
                q.addBindValue(lastMod); 
                
                if (q.exec()) changed++;
                existingFiles.remove(path); 
                
                if (changed % 500 == 0) {
                    scanDb.commit();
                    emit scanProgress(total, path);
                    scanDb.transaction(); 
                }
            }
        }
        
        if (!existingFiles.isEmpty() && m_isScanning) {
            QSqlQuery qDel(scanDb);
            qDel.prepare("DELETE FROM tracks WHERE path = ?");
            for (auto it = existingFiles.constBegin(); it != existingFiles.constEnd(); ++it) {
                qDel.addBindValue(it.key());
                qDel.exec();
                removed++;
            }
        }
        
        scanDb.commit(); 
        scanDb.close();
    }
    QSqlDatabase::removeDatabase("ScanConn");
    m_isScanning = false;
    
    if (changed > 0 || removed > 0) {
        QMetaObject::invokeMethod(this, "reloadModel", Qt::BlockingQueuedConnection);
    }
    
    emit scanFinished(total, changed + removed);
}

void KaedeDatabase::setAlbumMode(bool isAlbumMode) {
    m_isAlbumMode.store(isAlbumMode);
}

// 👑 新增：專屬查詢，填滿抽屜裡的軌道清單
void KaedeDatabase::loadAlbumDetails(const QString& artist, const QString& album) {
    std::vector<TrackRecord> cache;
    QSqlQuery q(m_db);
    q.prepare("SELECT path, title, artist, album, specs, cover_url FROM tracks "
              "WHERE artist = ? AND album = ? ORDER BY track_num ASC, title ASC");
    q.addBindValue(artist);
    q.addBindValue(album);
    
    cache.reserve(100); 
    if (q.exec()) {
        while (q.next()) {
            TrackRecord t;
            t.path = q.value(0).toString(); t.title = q.value(1).toString();
            t.artist = q.value(2).toString(); t.album = q.value(3).toString();
            t.specs = q.value(4).toString(); t.coverUrl = q.value(5).toString();
            cache.push_back(std::move(t));
        }
    }
    m_albumDetailModel->setTracks(cache);
}

void KaedeDatabase::reloadModel() {
    std::vector<TrackRecord> cache;
    QString sql;
    
    if (m_isAlbumMode.load()) {
        sql = "SELECT MIN(path), title, artist, album, specs, cover_url FROM tracks "
              "GROUP BY artist, album ORDER BY artist ASC, album ASC";
    } else {
        sql = "SELECT path, title, artist, album, specs, cover_url FROM tracks "
              "ORDER BY artist ASC, album ASC, track_num ASC, title ASC";
    }
    
    QSqlQuery q(sql, m_db);
    cache.reserve(10000); 
    while (q.next()) {
        TrackRecord t;
        t.path = q.value(0).toString(); t.title = q.value(1).toString();
        t.artist = q.value(2).toString(); t.album = q.value(3).toString();
        t.specs = q.value(4).toString(); t.coverUrl = q.value(5).toString();
        cache.push_back(std::move(t));
    }
    m_trackModel->setTracks(cache);
}

void KaedeDatabase::searchTracks(const QString& keyword) {
    if (keyword.trimmed().isEmpty()) {
        reloadModel();
        return;
    }
    
    std::vector<TrackRecord> cache;
    QSqlQuery q(m_db);
    QString sql = "SELECT t.path, t.title, t.artist, t.album, t.specs, t.cover_url "
                  "FROM tracks t JOIN tracks_fts f ON t.rowid = f.rowid "
                  "WHERE tracks_fts MATCH ? ";
                  
    if (m_isAlbumMode.load()) {
        sql += "GROUP BY t.artist, t.album ORDER BY rank, t.artist ASC, t.album ASC";
    } else {
        sql += "ORDER BY rank, t.artist ASC, t.album ASC, t.track_num ASC, t.title ASC";
    }
    
    q.prepare(sql);
    q.addBindValue(keyword + "*");
    
    if (q.exec()) {
        while (q.next()) {
            TrackRecord t;
            t.path = q.value(0).toString(); t.title = q.value(1).toString();
            t.artist = q.value(2).toString(); t.album = q.value(3).toString();
            t.specs = q.value(4).toString(); t.coverUrl = q.value(5).toString();
            cache.push_back(std::move(t));
        }
    }
    m_trackModel->setTracks(cache);
}

QVariant KaedeDatabase::getConfig(const QString& key, const QVariant& defaultValue) {
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM config WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next()) return q.value(0);
    return defaultValue;
}

void KaedeDatabase::setConfig(const QString& key, const QVariant& value) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value.toString());
    q.exec();
}

double KaedeDatabase::getTotalAudioSizeMB() const {
    QSqlQuery q("SELECT SUM(size) FROM tracks", m_db);
    if (q.exec() && q.next()) return q.value(0).toLongLong() / (1024.0 * 1024.0);
    return 0.0;
}

QStringList KaedeDatabase::getMountedDirectories() const {
    return m_mountedDirs;
}