#pragma once
#ifndef ADAPTIVECOLORENGINE_H
#define ADAPTIVECOLORENGINE_H

#include <QColor>
#include <QString>
#include <QImage>
#include "ThemeManager.h"

class AdaptiveColorEngine {
public:
    static AdaptiveColorEngine& instance() {
        static AdaptiveColorEngine engine;
        return engine;
    }

    // ⚡ 控制全域自適應開關
    bool isAdaptiveEnabled() const { return m_isAdaptiveEnabled; }
    void setAdaptiveEnabled(bool enabled) { m_isAdaptiveEnabled = enabled; }

    // ⚡ 提取圖片主色調 (從路徑)
    void extractColorFromImage(const QString& path) {
        QString cleanPath = path;
        if (cleanPath.startsWith("qrc:/")) {
            cleanPath.replace("qrc:/", ":/");
        }
        
        QImage img(cleanPath);
        extractColorFromImage(img);
    }

    // ⚡ 提取圖片主色調 (直接傳入記憶體 QImage)
    void extractColorFromImage(const QImage& img) {
        if (!img.isNull()) {
            QImage scaled = img.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            m_dominantColor = scaled.pixelColor(0, 0);
        } else {
            m_dominantColor = QColor("#0A0806"); 
        }
    }

    // ⚡ 1. 獲取自適應 UI 核心字體顏色 (Monet 引擎邏輯)
    QColor getTextColor(bool isAdaptiveEnabled) const {
        if (!isAdaptiveEnabled) { 
            return ThemeManager::instance().textHigh(); 
        }

        int h, s, l, a;
        m_dominantColor.getHsl(&h, &s, &l, &a);

        if (s < 20) {
            return (l > 128) ? QColor("#121212") : ThemeManager::instance().textHigh();
        }

        s = qMin(255, s + 60); 
        l = (l > 140) ? 45 : 215; 

        return QColor::fromHsl(h, s, l);
    }

    // ⚡ 2. 獲取自適應面板背景色 (雲母/黑曜石自適應玻璃)
    QColor getPanelBackgroundColor(bool isAdaptiveEnabled, bool isDarkTheme = true) const {
        if (!isAdaptiveEnabled) {
            return isDarkTheme ? QColor(20, 20, 22, 242) : QColor(246, 247, 249, 242);
        }

        int h, s, l, a;
        m_dominantColor.getHsl(&h, &s, &l, &a);

        // 如果主色接近灰階，退回中性半透明底色
        if (s < 15) {
            return isDarkTheme ? QColor(15, 15, 15, 235) : QColor(245, 245, 247, 242);
        }

        if (isDarkTheme) {
            // 黑曜石自適應：色相保留，飽和度極限壓低，亮度鎖死極低值，保證暗色系統一
            int targetS = qMin(35, s / 3 + 5); 
            int targetL = 11; 
            return QColor::fromHslF(h / 360.0, targetS / 255.0, targetL / 100.0, 0.92);
        } else {
            // 雲母自適應：飽和度極限壓低，亮度推向極亮值，保證白皙純淨
            int targetS = qMin(20, s / 4 + 4);
            int targetL = 96; 
            return QColor::fromHslF(h / 360.0, targetS / 255.0, targetL / 100.0, 0.94);
        }
    }

    // ⚡ 3. 獲取自適應面板微弱邊框色
    QColor getPanelBorderColor(bool isAdaptiveEnabled, bool isDarkTheme = true) const {
        if (!isAdaptiveEnabled) {
            return isDarkTheme ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 25);
        }

        int h, s, l, a;
        m_dominantColor.getHsl(&h, &s, &l, &a);

        if (s < 15) {
            return isDarkTheme ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 22);
        }

        if (isDarkTheme) {
            int targetS = qMin(50, s / 2 + 10);
            int targetL = 22; // 邊框亮度略亮於背景
            return QColor::fromHslF(h / 360.0, targetS / 255.0, targetL / 100.0, 0.45);
        } else {
            int targetS = qMin(30, s / 3 + 10);
            int targetL = 88; // 邊框亮度略暗於亮色背景
            return QColor::fromHslF(h / 360.0, targetS / 255.0, targetL / 100.0, 0.60);
        }
    }

    // ⚡ 4. 獲取自適應歌詞/文字基底色
    QColor getLyricsTextColor(bool isAdaptiveEnabled, bool isDarkTheme = true) const {
        if (!isAdaptiveEnabled) {
            return isDarkTheme ? QColor(180, 180, 185, 200) : QColor(60, 60, 65, 200);
        }

        int h, s, l, a;
        m_dominantColor.getHsl(&h, &s, &l, &a);

        if (isDarkTheme) {
            // 文字高亮：低飽和高明度，與面板拉開對比
            int targetS = qMin(25, s / 5);
            int targetL = 82; 
            return QColor::fromHslF(h / 360.0, targetS / 255.0, targetL / 100.0, 0.85);
        } else {
            // 文字暗色：適度帶有環境色溫的暗灰色
            int targetS = qMin(45, s / 3 + 10);
            int targetL = 24; 
            return QColor::fromHslF(h / 360.0, targetS / 255.0, targetL / 100.0, 0.88);
        }
    }

private:
    AdaptiveColorEngine() : m_dominantColor("#0A0806"), m_isAdaptiveEnabled(false) {}
    QColor m_dominantColor;
    bool m_isAdaptiveEnabled;
};

#endif // ADAPTIVECOLORENGINE_H