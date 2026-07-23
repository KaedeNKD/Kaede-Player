#pragma once
#ifndef PLAYEREXPANDEDPANEL_H
#define PLAYEREXPANDEDPANEL_H

#include <QQuickWidget>
#include <QRect>
#include <QVariant>
#include <QImage>
#include <QQuickImageProvider>
#include <vector>
#include "MediaMetadataParser.h"

// 封面影像提供者
class CoverImageProvider : public QQuickImageProvider {
public:
    CoverImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    QImage m_image;
};

// 👑 最終奧義：歌詞雙層紋理提供者
class LyricsTextureProvider : public QQuickImageProvider {
public:
    LyricsTextureProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    
    QImage m_baseTexture;
    QImage m_highTexture;
};

class PlayerExpandedPanel : public QQuickWidget {
    Q_OBJECT
public:
    explicit PlayerExpandedPanel(QWidget* parent = nullptr);
    ~PlayerExpandedPanel() override = default;

    void animateToggle(const QRect& triggerRect);
    
    void updateTrackData(const QImage& cover, const std::vector<LyricLine>& lyrics);
    void updateLyricPosition(double currentSec);
    void clearTrackData();

private:
    void initRhiEngine();
    
    CoverImageProvider* m_coverProvider = nullptr;
    LyricsTextureProvider* m_lyricsProvider = nullptr; 
    bool m_isOpen = false;

    // ⚡ 用於 C++ 端的極速二分搜尋
    std::vector<double> m_lyricTimes;
    int m_currentLyricIndex = -1;
};

#endif // PLAYEREXPANDEDPANEL_H