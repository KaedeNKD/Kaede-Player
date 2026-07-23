#pragma once
#ifndef TRACKANALYZERPANEL_H
#define TRACKANALYZERPANEL_H

#include <QWidget>
#include <QFrame>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>
#include <QFutureWatcher>
#include <QImage>
#include <vector>

// ⚡ STFT 靜態頻譜熱力圖畫布 
class SpectrogramCanvas : public QWidget {
    Q_OBJECT
public:
    explicit SpectrogramCanvas(QWidget* parent = nullptr);
    void setImage(const QImage& img, int sampleRate);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image;
    int m_sampleRate = 44100;
};

// ⚡ 獨立的 PSD 繪圖引擎
class PsdCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PsdCanvas(QWidget* parent = nullptr);
    void setPsdData(const std::vector<float>& psdData, int sampleRate, double cutoffFreq);
    void setThemeColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> m_psdData;
    int m_sampleRate = 44100;
    double m_cutoffFreq = 0.0;
    QColor m_themeColor = QColor(0, 255, 180);
};

// ⚡ 分析結果資料結構
struct TrackAnalysisResult {
    bool success = false;
    int declaredSampleRate = 0;
    double cutoffFrequency = 0.0;
    double estimatedTrueSampleRate = 0.0;
    std::vector<float> averagedPsd;
    QImage spectrogramImage; 
    QString errorMsg;
};

class TrackAnalyzerPanel : public QWidget {
    Q_OBJECT
public:
    explicit TrackAnalyzerPanel(QWidget* parent = nullptr);
    ~TrackAnalyzerPanel() override = default;

    void syncGeometry(const QRect& portalRect);
    void updateAdaptiveTheme(const QColor& fgColor);
    void loadTrack(const QString& path);
    bool isOpen() const { return m_isOpen; }

public slots:
    void togglePanel();
    void closePanel();

private slots:
    void onAnalysisFinished();

private:
    static TrackAnalysisResult performDeepAnalysis(const QString& filePath);

    QFrame* m_mainPanel = nullptr;
    QPropertyAnimation* m_slideAnim = nullptr;
    bool m_isOpen = false;

    // 極簡工業風 UI 標籤
    QLabel* m_lblTitle = nullptr;
    QLabel* m_lblStatus = nullptr;
    QLabel* m_lblContainer = nullptr; // 容器規格
    QLabel* m_lblActualSr = nullptr;  // 真實推定
    
    SpectrogramCanvas* m_spectrogramCanvas = nullptr; 
    PsdCanvas* m_psdCanvas = nullptr; 

    QFutureWatcher<TrackAnalysisResult> m_watcher;
};

#endif // TRACKANALYZERPANEL_H