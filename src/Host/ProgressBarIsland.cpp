#include "ProgressBarIsland.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLinearGradient>
#include <cmath> 
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ProgressBarIsland::ProgressBarIsland(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFixedHeight(20);

    // 游標 Hover 動畫
    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(250);
    m_hoverAnim->setStartValue(0.0);
    m_hoverAnim->setEndValue(1.0);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val){
        m_hoverBlend = val.toDouble();
        update();
    });

    // ⚡ 啟動狀態過渡引擎 (1 秒鐘，專屬霓虹點火時長)
    m_transitionAnim = new QVariantAnimation(this);
    m_transitionAnim->setDuration(1000); 
    m_transitionAnim->setStartValue(0.0);
    m_transitionAnim->setEndValue(1.0);
    
    connect(m_transitionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_transitionProgress = val.toDouble();
        update();
    });
}

void ProgressBarIsland::setProgress(double currentSec, double totalSec) {
    if (!m_isDragging) {
        m_currentSec = currentSec;
        m_totalSec = totalSec;
        update();
    }
}

void ProgressBarIsland::setPlaybackState(bool isPlaying) {
    if (m_isPlaying != isPlaying) {
        m_isPlaying = isPlaying;
        // ⚡ 無論是播放(點火)還是暫停(熄火)，都觸發動畫進行線性過渡
        m_transitionAnim->stop();
        m_transitionAnim->start();
    }
}

void ProgressBarIsland::setDspMode(bool enabled) {
    m_isDspMode = enabled;
    update();
}

QString ProgressBarIsland::formatTime(double seconds) const {
    if (seconds < 0) seconds = 0;
    int m = static_cast<int>(seconds) / 60;
    int s = static_cast<int>(seconds) % 60;
    
    if (m_isDspMode) {
        int ms = static_cast<int>((seconds - std::floor(seconds)) * 1000.0);
        return QString("%1:%2.%3").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')).arg(ms, 3, 10, QChar('0'));
    }
    
    int h = static_cast<int>(seconds) / 3600;
    if (h > 0) {
        m = (static_cast<int>(seconds) % 3600) / 60;
        return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    }
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

void ProgressBarIsland::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 佈局計算
    int textW = m_isDspMode ? 85 : 45; 
    int barX = textW + 10;
    int barW = width() - (textW * 2) - 20;
    int barY = height() / 2;

    // 1. 繪製底部軌道 (結合 Hover 互動放大)
    int trackHeight = 4 + static_cast<int>(2 * m_hoverBlend);
    QRectF trackRect(barX, barY - trackHeight / 2.0, barW, trackHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 30));
    painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);

    // 2. 計算進度百分比 (支援拖曳覆蓋)
    double pct = (m_totalSec > 0.0) ? (m_currentSec / m_totalSec) : 0.0;
    if (m_isDragging && m_totalSec > 0.0) {
        pct = (m_hoverX - barX) / static_cast<double>(barW);
    }
    pct = std::clamp(pct, 0.0, 1.0);

    // 3. 繪製漸層進度 (歸還 QLinearGradient 靈魂)
    if (m_totalSec > 0.0 || m_isDragging) {
        QRectF fillRect(barX, barY - trackHeight / 2.0, barW * pct, trackHeight);
        
        QLinearGradient grad(fillRect.topLeft(), fillRect.bottomRight());
        grad.setColorAt(0.0, QColor(140, 210, 255)); 
        grad.setColorAt(1.0, QColor(0, 140, 255));   
        
        painter.setBrush(grad); 
        painter.drawRoundedRect(fillRect, trackHeight / 2.0, trackHeight / 2.0);
    }

    // 4. 繪製拖拉控制點
    if (m_hoverBlend > 0.0 || m_isDragging) {
        double hSize = 12.0 * m_hoverBlend;
        QRectF handle(barX + barW * pct - hSize / 2.0, barY - hSize / 2.0, hSize, hSize);
        painter.setBrush(QColor(255, 255, 255));
        painter.drawEllipse(handle);
    }

    // 👑 5. 雙軌並行數學引擎：線性基底 (Base) + 餘弦閃爍 (Flash)
    double p = m_transitionProgress;
    int baseAlpha = 120;
    double flashIntensity = 0.0;

    if (m_isPlaying) {
        // 播放時：透明度從 120 線性推升到 220
        baseAlpha = 120 + static_cast<int>(100 * p);
        
        // 點火閃爍：產生 3.5 次波動，並用平滑衰減確保收尾柔和
        flashIntensity = ((std::cos(p * M_PI * 7.0) + 1.0) / 2.0) * std::pow(1.0 - p, 1.5);
    } else {
        // 暫停時：透明度從 220 線性衰減回 120 (無閃爍)
        baseAlpha = 220 - static_cast<int>(100 * p);
    }

    // 合併 Hover 提亮，並套用閃爍乘數
    int finalBase = baseAlpha + static_cast<int>(35 * m_hoverBlend);
    int currentAlpha = std::clamp(finalBase + static_cast<int>(135 * flashIntensity), 0, 255);
    
    // 閃爍高峰時帶入微弱的高壓電弧光澤 (冰藍色偏)
    int r = 255;
    int g = 255 - static_cast<int>(40 * flashIntensity);
    int b = 255;

    painter.setPen(QColor(r, g, b, currentAlpha));
    painter.setFont(QFont("Consolas", 10, QFont::Bold));
    
    // 左側當前時間 (帶點火特效)
    painter.drawText(QRectF(0, 0, textW, height()), Qt::AlignRight | Qt::AlignVCenter, formatTime(m_currentSec));
    
    // 右側總時間 (較暗，無點火特效)
    int rightAlpha = std::clamp(static_cast<int>(finalBase * 0.7), 0, 255);
    painter.setPen(QColor(255, 255, 255, rightAlpha));
    painter.drawText(QRectF(width() - textW, 0, textW, height()), Qt::AlignLeft | Qt::AlignVCenter, formatTime(m_totalSec));
}

void ProgressBarIsland::enterEvent(QEnterEvent* /*event*/) {
    m_isHovered = true;
    m_hoverAnim->setDirection(QAbstractAnimation::Forward);
    m_hoverAnim->start();
}

void ProgressBarIsland::leaveEvent(QEvent* /*event*/) {
    m_isHovered = false;
    if (!m_isDragging) {
        m_hoverAnim->setDirection(QAbstractAnimation::Backward);
        m_hoverAnim->start();
    }
}

void ProgressBarIsland::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_hoverX = event->pos().x();
        update();
    }
}

void ProgressBarIsland::mouseMoveEvent(QMouseEvent* event) {
    if (m_isHovered || m_isDragging) {
        m_hoverX = event->pos().x();
        if (m_isDragging) update();
    }
}

void ProgressBarIsland::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        
        int textW = m_isDspMode ? 85 : 45; 
        int barX = textW + 10;
        int barW = width() - (textW * 2) - 20;
        
        double pct = (event->pos().x() - barX) / static_cast<double>(barW);
        pct = std::clamp(pct, 0.0, 1.0);
        
        emit sigSeekRequested(pct); 
        
        if (!m_isHovered) {
            m_hoverAnim->setDirection(QAbstractAnimation::Backward);
            m_hoverAnim->start();
        }
        update();
    }
}