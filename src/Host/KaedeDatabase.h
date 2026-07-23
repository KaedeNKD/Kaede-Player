#pragma once
#ifndef KAEDEDATABASE_H
#define KAEDEDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QAbstractListModel>
#include <QStringList>
#include <QVariant>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

#include <QFileSystemWatcher>
#include <QTimer>
#include <QSet>

struct TrackRecord {
    QString path;
    QString title;
    QString artist;
    QString album;
    QString genre;
    int trackNum = 0; 
    QString specs;
    QString coverUrl;
    qint64 size = 0;
};

class KaedeTrackModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum TrackRoles {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        SpecsRole,
        CoverUrlRole,
        AlbumRole
    };

    explicit KaedeTrackModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent); return static_cast<int>(m_tracks.size());
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= static_cast<int>(m_tracks.size())) return QVariant();
        const auto& t = m_tracks[index.row()];
        switch (role) {
            case PathRole: return t.path;
            case TitleRole: return t.title;
            case ArtistRole: return t.artist;
            case SpecsRole: return t.specs;
            case CoverUrlRole: return t.coverUrl;
            case AlbumRole: return t.album;
            default: return QVariant();
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[PathRole] = "path"; roles[TitleRole] = "title";
        roles[ArtistRole] = "artist"; roles[SpecsRole] = "specs";
        roles[CoverUrlRole] = "coverUrl"; roles[AlbumRole] = "album";
        return roles;
    }

    void setTracks(const std::vector<TrackRecord>& tracks) {
        beginResetModel(); m_tracks = tracks; endResetModel();
    }
    
    void clear() { beginResetModel(); m_tracks.clear(); endResetModel(); }

private:
    std::vector<TrackRecord> m_tracks;
};

class KaedeDatabase : public QObject {
    Q_OBJECT
public:
    static KaedeDatabase& instance() { static KaedeDatabase db; return db; }
    
    void init();
    void purgeDatabase();
    void scanDirectory(const QString& dirPath);
    
    void setAlbumMode(bool isAlbumMode);
    
    // 👑 新增：專為底部抽屜準備的專輯詳情查詢器
    void loadAlbumDetails(const QString& artist, const QString& album);
    
    void searchTracks(const QString& keyword); 
    void reloadModel(); 
    
    QAbstractItemModel* getTrackModel() { return m_trackModel; }
    QAbstractItemModel* getAlbumDetailModel() { return m_albumDetailModel; } // ⚡ 獨立 Model
    
    QVariant getConfig(const QString& key, const QVariant& defaultValue = QVariant());
    void setConfig(const QString& key, const QVariant& value);
    
    double getTotalAudioSizeMB() const;
    QStringList getMountedDirectories() const;

signals:
    void scanStarted();
    void scanProgress(int count, const QString& lastFile);
    void scanFinished(int totalFiles, int changedFiles);

private slots:
    void onDirectoryChanged(const QString& path);
    void processPendingChanges();

private:
    KaedeDatabase(QObject* parent = nullptr);
    ~KaedeDatabase();
    
    void setupTables();
    void createFtsTriggers(); 
    QString extractAndCacheCover(const QString& filePath); 
    void watchDirectoryRecursively(const QString& path);
    void scanWorker(const QStringList& dirs);
    
    QString m_dbPath;
    QString m_coverCachePath; 
    QSqlDatabase m_db;
    KaedeTrackModel* m_trackModel = nullptr;
    KaedeTrackModel* m_albumDetailModel = nullptr; // ⚡ 抽屜專用模型
    QStringList m_mountedDirs;
    
    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_watchDebounceTimer = nullptr;
    
    std::atomic<bool> m_isScanning{false};
    std::atomic<bool> m_isAlbumMode{false}; 
    std::thread m_scanThread;
};

#endif // KAEDEDATABASE_H