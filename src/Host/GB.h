#pragma once
#ifndef GB_H
#define GB_H

#include <QQuickWidget>
#include <QQmlEngine>
#include <QQuickItem>

// =========================================================================================
// ⚡ GB (Glow Background) 獨立引擎完全體
// 內建 Win11 霧化溶解、非同步解碼監聽，以及 UI 預模糊狀態機
// =========================================================================================
class GBBackgroundWidget : public QQuickWidget {
    Q_OBJECT
public:
    explicit GBBackgroundWidget(QWidget* parent = nullptr);
    ~GBBackgroundWidget() override = default;

    void setBackgroundImage(const QString& path);
    void clearBackground();

    // ⚡ 神級 UX 接口：控制預先起霧與解除起霧
    void setSelectingState(bool isSelecting);

    void setNeonColors(const QString& color1, const QString& color2, const QString& color3);
    void setBreathingSpeed(int durationMs);
};

#endif // GB_H