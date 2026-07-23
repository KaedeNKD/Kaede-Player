#pragma once
#ifndef TRACKPEQPANEL_H
#define TRACKPEQPANEL_H

#include <QWidget>
#include <QFrame>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QGraphicsOpacityEffect> 
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QTableWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QWheelEvent>
#include <vector>
#include <complex>
#include <QPainterPath>

#include "KaedeAudioEngine.h"

enum class FilterType { Peaking, LowShelf, HighShelf, HighPass12, HighPass24, LowPass12, LowPass24, Notch, AllPass, BandPass };

struct PeqBandData {
    bool enabled = true; FilterType type = FilterType::Peaking; double freq = 1000.0; double gain = 0.0; double q = 1.0;
};

// 👑 實體滾輪編碼器
class KaedeEncoder : public QWidget {
    Q_OBJECT
public:
    explicit KaedeEncoder(const QString& title, double min, double max, double step, const QString& suffix, double defVal, QWidget* parent = nullptr);
    void setValue(double v);
    double value() const { return m_value; }
    void setThemeColor(const QColor& c) { m_themeColor = c; update(); }
    void setLocked(bool locked) { m_isLocked = locked; update(); }

signals:
    void valueChanged(double);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_title, m_suffix;
    double m_value, m_min, m_max, m_step, m_default, m_savedValue;
    bool m_isLocked = false;
    QColor m_themeColor = QColor(56, 178, 206);
};

class PeqCurveCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PeqCurveCanvas(QWidget* parent = nullptr);
    void updateBands(const std::vector<DspBiquad>& coeffsList);
    void setThemeColor(const QColor& color);
    void setPowerAlpha(double alpha); 
    void setMorphRatio(double ratio); // 👑 動態曲線生長/塌陷比例
    double getMorphRatio() const { return m_morphRatio; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildCache(); 
    std::vector<DspBiquad> m_coeffsList; 
    QColor m_themeColor = QColor(56, 178, 206);
    QPainterPath m_cachedCurvePath; 
    QPainterPath m_cachedFillPath;  
    double m_powerAlpha = 1.0; 
    double m_morphRatio = 1.0; 
};

class TrackPeqPanel : public QWidget {
    Q_OBJECT
public:
    explicit TrackPeqPanel(QWidget* parent = nullptr);
    ~TrackPeqPanel() override = default;

    void syncGeometry(const QRect& portalRect);
    void updateAdaptiveTheme(const QColor& fgColor);
    bool isOpen() const { return m_isOpen; }

public slots:
    void togglePanel(); void closePanel();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onParamChanged();
    void toggleAlienMode();
    void addNewBand(); 
    void removeSelectedBand();
    void commitToEngine(); 
    void importApoPreset(); void exportApoPreset(); void clearAllBands();
    void onTableSelectionChanged();
    void syncHubToTable();
    void syncFxToEngine();
    void commitFxToEngine(); // 👑 FX 防抖動最終提交口

private:
    void setupTable();
    void updateTableDisplay();
    FilterType parseApoType(const QString& apoTypeCode); QString getApoTypeCode(FilterType type);
    QString getTypeName(FilterType type);

    DspBiquad calcPeakingEQ(double sampleRate, double freq, double Q, double gainDb); 
    DspBiquad calcLowShelf(double sampleRate, double freq, double Q, double gainDb); 
    DspBiquad calcHighShelf(double sampleRate, double freq, double Q, double gainDb); 
    DspBiquad calcHighPass(double sampleRate, double freq, double Q); 
    DspBiquad calcLowPass(double sampleRate, double freq, double Q); 
    DspBiquad calcNotch(double sampleRate, double freq, double Q); 
    DspBiquad calcAllPass(double sampleRate, double freq, double Q); 
    DspBiquad calcBandPass(double sampleRate, double freq, double Q);

    QFrame* m_mainPanel = nullptr; QPropertyAnimation* m_slideAnim = nullptr; bool m_isOpen = false; bool m_isAlienMode = false; int m_clickCount = 0;
    
    QPushButton* m_btnMasterPower = nullptr; 
    
    // 👑 斷電與曲線塌陷雙效動畫
    QVariantAnimation* m_powerAnim = nullptr;
    QVariantAnimation* m_morphAnim = nullptr;
    double m_currentPowerAlpha = 1.0;
    QGraphicsOpacityEffect* m_effHub = nullptr;
    QGraphicsOpacityEffect* m_effTable = nullptr;
    
    QLabel* m_lblCoreMode = nullptr; QLabel* m_lblPreamp = nullptr; 
    QColor m_currentThemeColor = QColor(56, 178, 206);
    QLabel* m_lblTitle = nullptr; PeqCurveCanvas* m_curveCanvas = nullptr;
    
    QFrame* m_hubFrame = nullptr;
    QPushButton* m_btnHubToggle = nullptr;
    QComboBox* m_cmbHubType = nullptr;
    KaedeEncoder* m_encFreq = nullptr;
    KaedeEncoder* m_encGain = nullptr;
    KaedeEncoder* m_encQ = nullptr;
    QPushButton* m_btnHubDelete = nullptr;

    QFrame* m_fxFrame = nullptr;
    QPushButton* m_btnFxToggle = nullptr;     
    QPushButton* m_btnCfPower = nullptr;      
    KaedeEncoder* m_encCfLevel = nullptr;     
    KaedeEncoder* m_encCfCutoff = nullptr;    

    QTableWidget* m_table = nullptr; 
    QPushButton* m_btnAddBand = nullptr; QPushButton* m_btnImportApo = nullptr; QPushButton* m_btnExportApo = nullptr;
    
    // 👑 防事件洪流定時器
    QTimer* m_syncThrottleTimer = nullptr; 
    QTimer* m_fxThrottleTimer = nullptr; 

    std::vector<PeqBandData> m_bands;
    std::vector<DspBiquad> m_pendingCoeffs; 
    int m_selectedIndex = -1;
    double m_currentPreampLinear = 1.0; 
    double m_targetPreampDb = 0.0;
};

#endif // TRACKPEQPANEL_H