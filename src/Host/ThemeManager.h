#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QColor>
#include <QString>

class ThemeManager {
public:
    static ThemeManager& instance() {
        static ThemeManager tm;
        return tm;
    }

    bool isDarkTheme = true; 

    // =========================================================
    // 🎨 純淨高對比中性色 (徹底拋棄特定色彩，保證在光流背景上絕對清晰)
    // =========================================================
    QColor bgWindow() const { return QColor("#121212"); } // 預設深色基底 (若無背景圖時的備案)
    QColor bgPanel() const  { return QColor("#1E1E1E"); } // 面板備用底色
    QColor textHigh() const { return QColor("#FFFFFF"); } // 純白：保證主標題與核心數據絕對不會「融入黑夜」
    QColor textMute() const { return QColor("#C8C8C8"); } // 亮灰：副標題與次要資訊，柔和但不隱形
    QColor accent() const   { return QColor("#EAEAEA"); } // 珍珠白：用於點綴與高亮，保持全域中性極簡
    QColor border() const   { return QColor("#4A4A4A"); } // 灰邊框：清晰勾勒物件輪廓

    // =========================================================
    // 📝 字體與按鈕/分頁基礎 QSS 庫 (留作備用與未來擴充)
    // =========================================================
    QString getTabStyleSheet() const {
        return QString(
            "QTabWidget::pane { border: none; background: transparent; } "
            "QTabBar::tab { "
            "    background: transparent; "
            "    color: %1; "
            "    padding: 10px 20px; "
            "    font-family: 'Segoe UI', 'Consolas'; "
            "    font-weight: bold; "
            "    font-size: 14px; "
            "} "
            "QTabBar::tab:hover { color: #FFFFFF; } "
            "QTabBar::tab:selected { color: #FFFFFF; border-bottom: 2px solid #FFFFFF; }"
        ).arg(textMute().name());
    }

private:
    ThemeManager() {}
};

#endif // THEMEMANAGER_H