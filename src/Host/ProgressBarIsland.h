#pragma once
#ifndef PROGRESSBARISLAND_H
#define PROGRESSBARISLAND_H

#include <QWidget>
#include <QString>
#include <QVariantAnimation> 
#include <QEnterEvent> // 👑 Qt6 專屬修正

class ProgressBarIsland : public QWidget {
    Q_OBJECT
public:
    explicit ProgressBarIsland(QWidget* parent = nullptr);
    ~ProgressBarIsland() override = default;

    void setProgress(double currentSec, double totalSec);
    void setPlaybackState(bool isPlaying);
    void setDspMode(bool isDspMode); 

signals:
    void sigSeekRequested(double percent);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override; 
    void leaveEvent(QEvent* event) override;

private:
    QString formatTime(double seconds) const;

    double m_currentSec = 0.0;
    double m_totalSec = 0.0;
    bool m_isPlaying = false;
    bool m_isDspMode = false;

    // 游標互動與尋軌
    bool m_isDragging = false;
    double m_hoverX = -1.0;
    bool m_isHovered = false;
    
    QVariantAnimation* m_hoverAnim = nullptr;
    double m_hoverBlend = 0.0;

    // ⚡ 點火呼吸燈與線性過渡引擎 (歸還你的神級數學引擎)
    QVariantAnimation* m_transitionAnim = nullptr; 
    double m_transitionProgress = 1.0;            
};

#endif // PROGRESSBARISLAND_H