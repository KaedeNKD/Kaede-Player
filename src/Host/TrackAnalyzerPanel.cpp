#include "TrackAnalyzerPanel.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QtConcurrent>
#include <cmath>
#include <algorithm>
#include <QFileInfo>
#include "bass.h" 

#ifdef _WIN32
#define BASS_PATH(x) reinterpret_cast<const wchar_t*>(x.utf16())
#else
#define BASS_PATH(x) x.toUtf8().constData()
#endif

// =======================================================================
// 👑 熱力圖光譜映射演算法
// =======================================================================
static QColor getHeatmapColor(float db) {
    float v = std::clamp(db, -110.0f, -10.0f);
    float t = (v + 110.0f) / 100.0f; 

    int r = 0, g = 0, b = 0;
    if (t < 0.25f) { 
        float n = t / 0.25f;
        b = static_cast<int>(128 + 127 * n);
    } else if (t < 0.5f) { 
        float n = (t - 0.25f) / 0.25f;
        g = static_cast<int>(255 * n);
        b = 255;
    } else if (t < 0.75f) { 
        float n = (t - 0.5f) / 0.25f;
        r = static_cast<int>(255 * n);
        g = 255;
        b = static_cast<int>(255 * (1.0f - n));
    } else { 
        float n = (t - 0.75f) / 0.25f;
        r = 255;
        g = static_cast<int>(255 * (1.0f - n));
    }
    return QColor(r, g, b);
}

// =======================================================================
// 👑 STFT 全景熱力圖畫布 
// =======================================================================
SpectrogramCanvas::SpectrogramCanvas(QWidget* parent) : QWidget(parent) {}

void SpectrogramCanvas::setImage(const QImage& img, int sampleRate) {
    m_image = img;
    m_sampleRate = sampleRate;
    update();
}

void SpectrogramCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    int w = width();
    int h = height();

    if (m_image.isNull()) {
        painter.setPen(QColor(100, 100, 100));
        painter.setFont(QFont("Consolas", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "STFT SPECTROGRAM");
        return;
    }
    
    painter.drawImage(rect(), m_image);

    painter.setFont(QFont("Consolas", 9, QFont::Bold));
    float nyquist = m_sampleRate / 2.0f;
    
    for (int freq = 10000; freq <= nyquist; freq += 10000) {
        float ratio = static_cast<float>(freq) / nyquist;
        int y = static_cast<int>(h * (1.0f - ratio)); 
        
        painter.setPen(QPen(QColor(255, 255, 255, 180), 2));
        painter.drawLine(w - 10, y, w, y);
        
        QString text = QString("%1k").arg(freq / 1000);
        QRect textRect(w - 45, y - 8, 30, 16);
        painter.fillRect(textRect, QColor(0, 0, 0, 150));
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text);
        
        painter.setPen(QPen(QColor(255, 255, 255, 30), 1, Qt::DashLine));
        painter.drawLine(0, y, w - 10, y);
    }
}

// =======================================================================
// 👑 PSD 繪圖引擎實作
// =======================================================================
PsdCanvas::PsdCanvas(QWidget* parent) : QWidget(parent) {}

void PsdCanvas::setPsdData(const std::vector<float>& psdData, int sampleRate, double cutoffFreq) {
    m_psdData = psdData;
    m_sampleRate = sampleRate;
    m_cutoffFreq = cutoffFreq;
    update();
}

void PsdCanvas::setThemeColor(const QColor& color) {
    m_themeColor = color;
    update();
}

void PsdCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    if (m_psdData.empty()) {
        painter.setPen(QColor(100, 100, 100));
        painter.setFont(QFont("Consolas", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "POWER SPECTRAL DENSITY");
        return;
    }

    const float minDb = -120.0f;
    const float maxDb = 0.0f;
    float nyquist = m_sampleRate / 2.0f;

    painter.setFont(QFont("Consolas", 9));
    
    for (int db = -120; db <= 0; db += 30) {
        float normalizedY = 1.0f - ((db - minDb) / (maxDb - minDb));
        int y = std::clamp(static_cast<int>(normalizedY * h), 0, h);
        
        painter.setPen(QPen(QColor(255, 255, 255, 25), 1, Qt::DashLine));
        painter.drawLine(0, y, w, y);
        
        if (y > 10 && y < h - 10) {
            painter.setPen(QColor(255, 255, 255, 120));
            painter.drawText(5, y - 3, QString("%1 dB").arg(db));
        }
    }

    std::vector<int> targetFreqs = {1000, 5000, 10000, 20000, 40000};
    for (int freq : targetFreqs) {
        if (freq >= nyquist) continue;
        float ratio = static_cast<float>(freq) / nyquist;
        float logX = std::log10(1.0f + 9.0f * ratio);
        int x = static_cast<int>(logX * w);
        
        painter.setPen(QPen(QColor(255, 255, 255, 25), 1, Qt::DashLine));
        painter.drawLine(x, 0, x, h);
        
        painter.setPen(QColor(255, 255, 255, 120));
        painter.drawText(x + 4, h - 5, QString("%1k").arg(freq / 1000));
    }

    QPainterPath psdPath;
    int bins = m_psdData.size();

    for (int i = 0; i < bins; ++i) {
        float logX = std::log10(1.0f + 9.0f * (static_cast<float>(i) / bins));
        int x = static_cast<int>(logX * w);
        float db = m_psdData[i];
        float normalizedY = 1.0f - ((db - minDb) / (maxDb - minDb));
        int y = std::clamp(static_cast<int>(normalizedY * h), 0, h);

        if (i == 0) psdPath.moveTo(x, y);
        else psdPath.lineTo(x, y);
    }

    QLinearGradient fillGrad(0, 0, 0, h);
    QColor fillColor = m_themeColor;
    fillColor.setAlpha(100);
    fillGrad.setColorAt(0.0, fillColor);
    fillColor.setAlpha(0);
    fillGrad.setColorAt(1.0, fillColor);

    QPainterPath fillPath = psdPath;
    fillPath.lineTo(w, h);
    fillPath.lineTo(0, h);
    painter.fillPath(fillPath, fillGrad);

    painter.setPen(QPen(m_themeColor, 1.5));
    painter.drawPath(psdPath);

    // ⚡ 去除多餘加戲文字，只保留冷酷無情的紅色絕對基準線
    if (m_cutoffFreq > 0 && m_cutoffFreq < nyquist - 1000.0) { 
        float ratio = m_cutoffFreq / nyquist;
        float logX = std::log10(1.0f + 9.0f * ratio);
        int cutoffX = static_cast<int>(logX * w);

        painter.setPen(QPen(QColor(232, 17, 35), 2)); 
        painter.drawLine(cutoffX, 0, cutoffX, h);
    }
}

// =======================================================================
// 👑 分析器主面板
// =======================================================================
TrackAnalyzerPanel::TrackAnalyzerPanel(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    m_mainPanel = new QFrame(this);
    m_mainPanel->setObjectName("AnalyzerMain");
    m_mainPanel->setStyleSheet(
        "#AnalyzerMain { "
        "background: rgba(10, 10, 12, 0.92); " 
        "border-right: 1px solid rgba(255, 255, 255, 0.1); "
        "border-top-right-radius: 16px; "
        "border-bottom-right-radius: 16px; "
        "}"
    );

    m_slideAnim = new QPropertyAnimation(m_mainPanel, "pos", this);
    m_slideAnim->setDuration(350);
    m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slideAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_isOpen) hide();
    });

    QVBoxLayout* mainLayout = new QVBoxLayout(m_mainPanel);
    mainLayout->setContentsMargins(25, 25, 25, 30);
    mainLayout->setSpacing(15);

    m_lblTitle = new QLabel("TRACK INFO", m_mainPanel);
    m_lblTitle->setStyleSheet("color: #FFFFFF; font-family: 'Segoe UI'; font-size: 18px; font-weight: bold; letter-spacing: 2px;");
    mainLayout->addWidget(m_lblTitle);

    m_spectrogramCanvas = new SpectrogramCanvas(m_mainPanel);
    m_spectrogramCanvas->setStyleSheet("background: rgba(0, 0, 0, 0.8); border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 8px;");
    mainLayout->addWidget(m_spectrogramCanvas, 4); 

    m_psdCanvas = new PsdCanvas(m_mainPanel);
    m_psdCanvas->setStyleSheet("background: rgba(0, 0, 0, 0.5); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 8px;");
    mainLayout->addWidget(m_psdCanvas, 4); 

    QFrame* statsFrame = new QFrame(m_mainPanel);
    statsFrame->setStyleSheet("background: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 8px;");
    QVBoxLayout* statsLayout = new QVBoxLayout(statsFrame);
    
    // ⚡ 乾淨俐落的數據標籤
    m_lblStatus = new QLabel("狀態: 待命", statsFrame);
    m_lblStatus->setStyleSheet("color: #888; font-family: 'Consolas', 'Segoe UI'; font-size: 12px; font-weight: bold;");
    
    m_lblContainer = new QLabel("容器規格: -- kHz", statsFrame);
    m_lblContainer->setStyleSheet("color: #EAEAEA; font-family: 'Consolas', 'Segoe UI'; font-size: 13px; font-weight: bold;");
    
    m_lblActualSr = new QLabel("真實取樣率推定: -- kHz", statsFrame);
    m_lblActualSr->setStyleSheet("color: #EAEAEA; font-family: 'Consolas', 'Segoe UI'; font-size: 14px; font-weight: bold;");
    
    statsLayout->addWidget(m_lblStatus);
    statsLayout->addWidget(m_lblContainer);
    statsLayout->addWidget(m_lblActualSr);
    mainLayout->addWidget(statsFrame);

    connect(&m_watcher, &QFutureWatcher<TrackAnalysisResult>::finished, this, &TrackAnalyzerPanel::onAnalysisFinished);
}

void TrackAnalyzerPanel::syncGeometry(const QRect& portalRect) {
    this->setGeometry(portalRect);
    if (m_isOpen) {
        m_mainPanel->setGeometry(0, 0, portalRect.width(), portalRect.height());
    } else {
        m_mainPanel->setGeometry(-portalRect.width(), 0, portalRect.width(), portalRect.height());
    }
}

void TrackAnalyzerPanel::togglePanel() {
    m_isOpen = !m_isOpen;
    int panelW = this->width();

    if (m_isOpen) {
        this->show();
        this->raise();
        m_slideAnim->setStartValue(QPoint(-panelW, 0));
        m_slideAnim->setEndValue(QPoint(0, 0));
        m_slideAnim->start();
    } else {
        m_slideAnim->setStartValue(m_mainPanel->pos());
        m_slideAnim->setEndValue(QPoint(-panelW, 0));
        m_slideAnim->start();
    }
}

void TrackAnalyzerPanel::closePanel() {
    if (m_isOpen) {
        togglePanel();
    }
}

void TrackAnalyzerPanel::updateAdaptiveTheme(const QColor& fgColor) {
    m_lblTitle->setStyleSheet(QString("color: %1; font-family: 'Segoe UI'; font-size: 18px; font-weight: bold; letter-spacing: 2px;").arg(fgColor.name()));
    m_mainPanel->setStyleSheet(QString(
        "#AnalyzerMain { "
        "background: rgba(10, 10, 12, 0.92); "
        "border-right: 1px solid %1; "
        "border-top-right-radius: 16px; "
        "border-bottom-right-radius: 16px; "
        "}"
    ).arg(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 60).name(QColor::HexArgb)));
    
    if (m_psdCanvas) m_psdCanvas->setThemeColor(fgColor);
}

void TrackAnalyzerPanel::loadTrack(const QString& path) {
    m_lblStatus->setText(QString("狀態: 分析中 (%1)").arg(QFileInfo(path).fileName()));
    m_lblStatus->setStyleSheet("color: #00FFB4; font-family: 'Consolas', 'Segoe UI'; font-size: 12px; font-weight: bold;");
    
    m_lblContainer->setText("容器規格: 計算中...");
    m_lblActualSr->setText("真實取樣率推定: 運算中...");
    
    m_psdCanvas->setPsdData(std::vector<float>(), 44100, 0.0);
    m_spectrogramCanvas->setImage(QImage(), 44100); 

    QFuture<TrackAnalysisResult> future = QtConcurrent::run(performDeepAnalysis, path);
    m_watcher.setFuture(future);
}

void TrackAnalyzerPanel::onAnalysisFinished() {
    TrackAnalysisResult res = m_watcher.result();

    if (!res.success) {
        m_lblStatus->setText("狀態: 失敗");
        m_lblStatus->setStyleSheet("color: #E81123; font-family: 'Consolas', 'Segoe UI'; font-size: 12px; font-weight: bold;");
        return;
    }

    m_lblStatus->setText("狀態: 完成");
    m_lblStatus->setStyleSheet("color: #888; font-family: 'Consolas', 'Segoe UI'; font-size: 12px; font-weight: bold;");
    
    // ⚡ 拒絕加戲，純粹數據
    m_lblContainer->setText(QString("容器規格: %1 kHz").arg(res.declaredSampleRate / 1000.0, 0, 'f', 1));
    m_lblActualSr->setText(QString("真實取樣率推定: %1 kHz").arg(res.estimatedTrueSampleRate / 1000.0, 0, 'f', 1));
    
    // 統一使用冷色調，不加額外情緒顏色
    m_lblActualSr->setStyleSheet("color: #EAEAEA; font-family: 'Consolas', 'Segoe UI'; font-size: 14px; font-weight: bold;");

    m_psdCanvas->setPsdData(res.averagedPsd, res.declaredSampleRate, res.cutoffFrequency);
    
    if (!res.spectrogramImage.isNull()) {
        m_spectrogramCanvas->setImage(res.spectrogramImage, res.declaredSampleRate);
    }
}

// =======================================================================
// 👑 核心演算法：雙引擎聯合探測 (Brickwall Gradient + Absolute Silence Wakeup)
// =======================================================================
TrackAnalysisResult TrackAnalyzerPanel::performDeepAnalysis(const QString& filePath) {
    TrackAnalysisResult res;

    HSTREAM stream = BASS_StreamCreateFile(FALSE, BASS_PATH(filePath), 0, 0, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
    if (!stream) {
        res.errorMsg = "BASS_DECODE_ERR";
        return res;
    }

    BASS_CHANNELINFO info;
    BASS_ChannelGetInfo(stream, &info);
    res.declaredSampleRate = info.freq;

    const int numBins = 4096;
    float fftBuffer[4096];
    std::vector<float> psdAccumulator(numBins, 0.0f);
    int frameCount = 0;

    const int specHeight = 512;
    int binsPerPixel = numBins / specHeight; 
    std::vector<std::vector<float>> spectrogramColumns;

    while (BASS_ChannelIsActive(stream) == BASS_ACTIVE_PLAYING) {
        int bytes = BASS_ChannelGetData(stream, fftBuffer, BASS_DATA_FFT8192);
        if (bytes == -1) break;

        std::vector<float> currentCol(specHeight, -120.0f);

        for (int i = 0; i < numBins; ++i) {
            float amplitude = std::max(fftBuffer[i], 1e-9f);
            float db = 20.0f * std::log10(amplitude);
            
            psdAccumulator[i] += db; 
            
            int yIndex = i / binsPerPixel;
            if (yIndex < specHeight) {
                if (db > currentCol[yIndex]) {
                    currentCol[yIndex] = db; 
                }
            }
        }
        spectrogramColumns.push_back(currentCol);
        frameCount++;
    }

    BASS_StreamFree(stream);

    if (frameCount == 0) {
        res.errorMsg = "ZERO_FRAMES";
        return res;
    }

    int specWidth = spectrogramColumns.size();
    if (specWidth > 0) {
        QImage specImg(specWidth, specHeight, QImage::Format_RGB32);
        for (int x = 0; x < specWidth; ++x) {
            for (int y = 0; y < specHeight; ++y) {
                int drawY = specHeight - 1 - y; 
                QColor c = getHeatmapColor(spectrogramColumns[x][y]);
                specImg.setPixelColor(x, drawY, c);
            }
        }
        res.spectrogramImage = specImg;
    }

    res.averagedPsd.resize(numBins);
    float maxDb = -999.0f;
    for (int i = 0; i < numBins; ++i) {
        res.averagedPsd[i] = psdAccumulator[i] / frameCount;
        if (res.averagedPsd[i] > maxDb) maxDb = res.averagedPsd[i];
    }

    std::vector<float> smoothPsd(numBins, 0.0f);
    int smoothRadius = 5; 
    for (int i = 0; i < numBins; ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int j = -smoothRadius; j <= smoothRadius; ++j) {
            if (i + j >= 0 && i + j < numBins) {
                sum += res.averagedPsd[i + j];
                count++;
            }
        }
        smoothPsd[i] = sum / count;
    }

    float binResolution = (res.declaredSampleRate / 2.0f) / numBins;
    float nyquist = res.declaredSampleRate / 2.0f;
    
    int bin1k = 1000 / binResolution;
    int bin5k = 5000 / binResolution;
    float coreEnergy = 0.0f;
    for (int i = bin1k; i < bin5k; ++i) coreEnergy += smoothPsd[i];
    coreEnergy /= (bin5k - bin1k);

    // 引擎 A：滑動差分
    double brickwallCutoff = nyquist;
    float windowHz = 1500.0f; 
    int halfWindowBins = static_cast<int>((windowHz / 2.0f) / binResolution);
    int startScanBin = numBins - halfWindowBins - 1;
    int endScanBin = static_cast<int>(12000.0f / binResolution); 
    
    float activeEnergyThreshold = maxDb - 85.0f; 
    float drasticDropThreshold = 6.5f; 

    for (int i = startScanBin; i > endScanBin; --i) {
        float leftEnergy = 0.0f;
        for (int j = i - halfWindowBins; j < i; ++j) leftEnergy += smoothPsd[j];
        leftEnergy /= halfWindowBins;

        float rightEnergy = 0.0f;
        for (int j = i; j < i + halfWindowBins; ++j) rightEnergy += smoothPsd[j];
        rightEnergy /= halfWindowBins;

        if (leftEnergy - rightEnergy >= drasticDropThreshold && leftEnergy >= activeEnergyThreshold) {
            brickwallCutoff = i * binResolution;
            break; 
        }
    }

    // 引擎 B：絕對靜音甦醒點
    double silenceCutoff = nyquist;
    float hfNoiseFloor = 0.0f;
    int hfCount = 0;
    int hfStartBin = static_cast<int>((nyquist - 2000.0f) / binResolution);
    for (int i = hfStartBin; i < numBins; ++i) {
        hfNoiseFloor += smoothPsd[i];
        hfCount++;
    }
    if (hfCount > 0) hfNoiseFloor /= hfCount;

    if (hfNoiseFloor < coreEnergy - 60.0f) {
        float wakeupThreshold = std::max(hfNoiseFloor + 10.0f, maxDb - 85.0f);
        for (int i = numBins - 1; i > endScanBin; --i) {
            if (smoothPsd[i] > wakeupThreshold) {
                bool sustained = true;
                int checkBins = static_cast<int>(1000.0f / binResolution);
                for (int k = 1; k <= checkBins && (i - k) > 0; ++k) {
                    if (smoothPsd[i - k] < wakeupThreshold) {
                        sustained = false;
                        break;
                    }
                }
                if (sustained) {
                    silenceCutoff = i * binResolution;
                    break;
                }
            }
        }
    }

    // 最終裁決
    double finalCutoff = nyquist;
    if (brickwallCutoff < finalCutoff) finalCutoff = brickwallCutoff;
    if (silenceCutoff < finalCutoff) finalCutoff = silenceCutoff;

    res.cutoffFrequency = finalCutoff;
    res.estimatedTrueSampleRate = finalCutoff * 2.0; 
    res.success = true;

    return res;
}