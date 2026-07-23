#include "TrackPeqPanel.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QFileDialog>
#include <QTextStream>
#include <QRegularExpression>
#include <QApplication>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =======================================================================
// 👑 0. 實體滾輪編碼器
// =======================================================================
KaedeEncoder::KaedeEncoder(const QString& title, double min, double max, double step, const QString& suffix, double defVal, QWidget* parent) 
    : QWidget(parent), m_title(title), m_min(min), m_max(max), m_step(step), m_suffix(suffix), m_default(defVal), m_value(defVal), m_savedValue(defVal) {
    setMinimumSize(60, 45);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::SizeVerCursor); 
}

void KaedeEncoder::setValue(double v) { m_value = std::clamp(v, m_min, m_max); update(); }

void KaedeEncoder::wheelEvent(QWheelEvent* event) {
    if (m_isLocked) return;
    double delta = event->angleDelta().y() > 0 ? m_step : -m_step;
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) delta /= 10.0; 
    setValue(m_value + delta);
    emit valueChanged(m_value);
}

void KaedeEncoder::mousePressEvent(QMouseEvent* event) {
    if (m_isLocked) return;
    if (event->button() == Qt::LeftButton) {
        if (std::abs(m_value - m_default) > 1e-5) { m_savedValue = m_value; setValue(m_default); } 
        else { setValue(m_savedValue); }
        emit valueChanged(m_value);
    }
}

void KaedeEncoder::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event); QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QRect r = rect();
    
    QPainterPath bg; bg.addRoundedRect(r, 4, 4);
    QLinearGradient bgGrad(0, 0, 0, r.height());
    bgGrad.setColorAt(0.0, QColor(0, 0, 0, 180)); bgGrad.setColorAt(0.2, QColor(25, 25, 30, 255));
    bgGrad.setColorAt(0.8, QColor(25, 25, 30, 255)); bgGrad.setColorAt(1.0, QColor(0, 0, 0, 180));
    p.fillPath(bg, bgGrad);
    p.setPen(QPen(QColor(255, 255, 255, m_isLocked ? 5 : 15), 1)); p.drawPath(bg);

    p.setClipPath(bg);
    double stepMod = m_step >= 1.0 ? 1.0 : m_step;
    double offset = std::fmod(m_value / stepMod * 6.0, 6.0); 
    for (int y = -6; y < r.height() + 6; y += 6) {
        double drawY = y + offset;
        double distFromCenter = std::abs(drawY - r.height() / 2.0);
        double alpha = std::clamp(1.0 - (distFromCenter / (r.height() / 2.0)), 0.0, 1.0);
        p.setPen(QPen(QColor(0, 0, 0, static_cast<int>(200 * alpha)), 2)); p.drawLine(r.width() - 12, drawY, r.width() - 4, drawY); 
        p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(40 * alpha)), 1)); p.drawLine(r.width() - 12, drawY + 1, r.width() - 4, drawY + 1);
    }
    p.setClipping(false);

    bool isModified = std::abs(m_value - m_default) > 1e-5;
    QColor textColor = m_isLocked ? QColor(80, 80, 80) : (isModified ? QColor(255, 255, 255) : QColor(160, 160, 160));
    if (isModified && !m_isLocked) { p.fillRect(r.width()/2 - 12, r.height() - 3, 16, 2, m_themeColor); }

    p.setPen(QColor(110, 110, 110)); p.setFont(QFont("Segoe UI", 7, QFont::Bold));
    p.drawText(QRect(0, 4, r.width() - 14, 15), Qt::AlignHCenter | Qt::AlignTop, m_title);
    p.setPen(textColor); p.setFont(QFont("Consolas", 11, QFont::Bold));
    QString valStr = (m_step >= 1.0) ? QString::number(m_value, 'f', 1) : QString::number(m_value, 'f', 2);
    p.drawText(QRect(0, 18, r.width() - 14, 25), Qt::AlignHCenter | Qt::AlignVCenter, valStr + m_suffix);
}

// =======================================================================
// 👑 1. 頻率響應動態曲線實作與塌陷生長動效
// =======================================================================
PeqCurveCanvas::PeqCurveCanvas(QWidget* parent) : QWidget(parent) {}
void PeqCurveCanvas::updateBands(const std::vector<DspBiquad>& coeffsList) { m_coeffsList = coeffsList; rebuildCache(); update(); }
void PeqCurveCanvas::setThemeColor(const QColor& color) { m_themeColor = color; update(); }
void PeqCurveCanvas::setPowerAlpha(double alpha) { m_powerAlpha = std::clamp(alpha, 0.0, 1.0); update(); }
void PeqCurveCanvas::setMorphRatio(double ratio) { m_morphRatio = std::clamp(ratio, 0.0, 1.0); rebuildCache(); update(); }
void PeqCurveCanvas::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); rebuildCache(); }

void PeqCurveCanvas::rebuildCache() {
    int w = width(); int h = height(); if (w <= 0 || h <= 0) return;
    m_cachedCurvePath = QPainterPath(); const double fs = 44100.0; 
    for (int x = 0; x < w; ++x) {
        double t = static_cast<double>(x) / w; double freq = 20.0 * std::pow(20000.0 / 20.0, t); double omega = 2.0 * M_PI * freq / fs;
        std::complex<double> z1 = std::polar(1.0, -omega); std::complex<double> z2 = std::polar(1.0, -2.0 * omega); std::complex<double> totalH(1.0, 0.0);
        for (const auto& c : m_coeffsList) { std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2; std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2; totalH *= (num / den); }
        double db = 20.0 * std::log10(std::max(std::abs(totalH), 1e-9)); 
        db *= m_morphRatio; // ⚡ 塌陷/生長動畫：乘上比例係數動態壓平曲線
        int y = h / 2 - static_cast<int>(db * h / 40.0); y = std::clamp(y, 0, h);
        if (x == 0) m_cachedCurvePath.moveTo(x, y); else m_cachedCurvePath.lineTo(x, y);
    }
    m_cachedFillPath = m_cachedCurvePath; m_cachedFillPath.lineTo(w, h); m_cachedFillPath.lineTo(0, h);
}

void PeqCurveCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event); QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); int w = width(); int h = height(); 
    painter.fillRect(rect(), QColor(10, 10, 12));
    
    int gridAlpha = static_cast<int>(12 * m_powerAlpha);
    int textAlpha = static_cast<int>(100 * (0.3 + 0.7 * m_powerAlpha)); 
    
    for (int db = -18; db <= 18; db += 6) { int y = h / 2 - (db * h / 40); painter.setPen(QPen(QColor(255, 255, 255, gridAlpha), 1, Qt::DashLine)); painter.drawLine(0, y, w, y); painter.setPen(QColor(100, 100, 100, textAlpha)); painter.setFont(QFont("Consolas", 8)); painter.drawText(5, y - 2, QString("%1 dB").arg(db > 0 ? "+" + QString::number(db) : QString::number(db))); }
    std::vector<double> freqs = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    for (double f : freqs) { double logX = (std::log10(f) - std::log10(20.0)) / (std::log10(20000.0) - std::log10(20.0)); int x = static_cast<int>(logX * w); painter.setPen(QPen(QColor(255, 255, 255, gridAlpha), 1, Qt::DashLine)); painter.drawLine(x, 0, x, h); painter.setPen(QColor(100, 100, 100, textAlpha)); painter.setFont(QFont("Consolas", 8)); QString fStr = f >= 1000 ? QString("%1k").arg(f/1000) : QString::number(f); painter.drawText(x + 4, h - 5, fStr); }
    
    if (!m_cachedCurvePath.isEmpty()) { 
        QLinearGradient fillGrad(0, 0, 0, h); QColor fillColor = m_themeColor; 
        fillColor.setAlpha(static_cast<int>(40 * m_powerAlpha)); 
        fillGrad.setColorAt(0.0, fillColor); fillGrad.setColorAt(0.5, QColor(0,0,0,0)); fillGrad.setColorAt(1.0, fillColor); 
        painter.fillPath(m_cachedFillPath, fillGrad); 
        
        QColor lineCol = m_themeColor;
        lineCol.setAlpha(static_cast<int>(255 * (0.1 + 0.9 * m_powerAlpha))); 
        painter.setPen(QPen(lineCol, 2, Qt::SolidLine, Qt::RoundCap)); 
        painter.drawPath(m_cachedCurvePath); 
    }
}

// =======================================================================
// 👑 2. 參數均衡器主面板 (TrackPeqPanel)
// =======================================================================
TrackPeqPanel::TrackPeqPanel(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint); setAttribute(Qt::WA_TranslucentBackground); hide();
    m_mainPanel = new QFrame(this); m_mainPanel->setObjectName("PeqMain"); m_mainPanel->setStyleSheet("#PeqMain { background: rgba(10, 10, 12, 0.95); border-left: 1px solid rgba(255, 255, 255, 0.1); border-top-left-radius: 16px; border-bottom-left-radius: 16px; }");
    m_slideAnim = new QPropertyAnimation(m_mainPanel, "pos", this); m_slideAnim->setDuration(350); m_slideAnim->setEasingCurve(QEasingCurve::OutCubic); connect(m_slideAnim, &QPropertyAnimation::finished, this, [this]() { if (!m_isOpen) hide(); });
    QVBoxLayout* mainLayout = new QVBoxLayout(m_mainPanel); mainLayout->setContentsMargins(20, 25, 20, 30); mainLayout->setSpacing(15);
    
    m_syncThrottleTimer = new QTimer(this); m_syncThrottleTimer->setInterval(40); m_syncThrottleTimer->setSingleShot(true); connect(m_syncThrottleTimer, &QTimer::timeout, this, &TrackPeqPanel::commitToEngine);
    
    // ⚡ 實裝 FX 防抖動定時器，徹底粉碎 Event Loop Flooding 崩潰 Bug
    m_fxThrottleTimer = new QTimer(this); m_fxThrottleTimer->setInterval(40); m_fxThrottleTimer->setSingleShot(true); connect(m_fxThrottleTimer, &QTimer::timeout, this, &TrackPeqPanel::commitFxToEngine);

    QHBoxLayout* titleLayout = new QHBoxLayout(); 
    
    m_btnMasterPower = new QPushButton("⏻", m_mainPanel); m_btnMasterPower->setFixedSize(30, 30); m_btnMasterPower->setCheckable(true); m_btnMasterPower->setChecked(true);
    m_btnMasterPower->setStyleSheet("QPushButton { border-radius: 15px; background: rgba(255,255,255,0.05); color: #555; font-size: 16px; } QPushButton:checked { color: #38B2CE; background: rgba(56, 178, 206, 0.1); border: 1px solid #38B2CE; }");
    titleLayout->addWidget(m_btnMasterPower);

    m_lblTitle = new QLabel("TRACK PEQ", m_mainPanel); m_lblTitle->setStyleSheet("color: #FFFFFF; font-family: 'Segoe UI'; font-size: 18px; font-weight: bold; letter-spacing: 2px; margin-left: 10px;");
    m_lblPreamp = new QLabel("AUTO-PRE: 0.0 dB", m_mainPanel); m_lblPreamp->setStyleSheet("color: #666; font-family: 'Consolas'; font-size: 10px; font-weight: bold; padding-right: 15px; border-right: 1px solid #444; margin-right: 15px;");
    m_lblCoreMode = new QLabel("CORE: STD-64", m_mainPanel); m_lblCoreMode->setStyleSheet("color: #666; font-family: 'Consolas'; font-size: 10px; font-weight: bold;"); m_lblCoreMode->setCursor(Qt::PointingHandCursor); m_lblCoreMode->installEventFilter(this); 
    
    titleLayout->addWidget(m_lblTitle); titleLayout->addStretch(); 
    titleLayout->addWidget(m_lblPreamp); titleLayout->addWidget(m_lblCoreMode); 
    mainLayout->addLayout(titleLayout);

    m_curveCanvas = new PeqCurveCanvas(m_mainPanel); m_curveCanvas->setFixedHeight(160); m_curveCanvas->setStyleSheet("border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 8px;"); mainLayout->addWidget(m_curveCanvas);

    m_hubFrame = new QFrame(m_mainPanel); m_hubFrame->setFixedHeight(65);
    m_hubFrame->setStyleSheet("QFrame { background: rgba(0, 0, 0, 0.3); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; }");
    QHBoxLayout* hubLayout = new QHBoxLayout(m_hubFrame); hubLayout->setContentsMargins(8, 8, 8, 8); hubLayout->setSpacing(8);
    
    m_btnHubToggle = new QPushButton("⏻", m_hubFrame); m_btnHubToggle->setFixedSize(32, 45);
    m_btnHubToggle->setStyleSheet("QPushButton { font-size: 18px; border-radius: 6px; background: rgba(255,255,255,0.05); color: #555; } QPushButton:checked { color: #38B2CE; background: rgba(56, 178, 206, 0.1); border: 1px solid #38B2CE; }");
    m_btnHubToggle->setCheckable(true);
    
    m_cmbHubType = new QComboBox(m_hubFrame); m_cmbHubType->setMinimumWidth(80); m_cmbHubType->setFixedHeight(45); m_cmbHubType->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_cmbHubType->addItems({"Peaking", "LowShelf", "HighShelf", "HP 12dB", "HP 24dB", "LP 12dB", "LP 24dB", "Notch", "AllPass", "BandPass"});
    m_cmbHubType->setStyleSheet("QComboBox { background: rgba(0,0,0,0.5); color: #EAEAEA; font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; border: 1px solid #444; border-radius: 6px; padding-left: 8px; } QComboBox::drop-down { border: none; } QComboBox QAbstractItemView { background: #1A1A1A; color: #FFF; border: 1px solid #444; selection-background-color: #38B2CE; }");
    
    m_encFreq = new KaedeEncoder("FREQ", 20.0, 20000.0, 10.0, "", 1000.0, m_hubFrame); m_encGain = new KaedeEncoder("GAIN", -30.0, 30.0, 0.1, " dB", 0.0, m_hubFrame); m_encQ = new KaedeEncoder("Q/SLOPE", 0.1, 20.0, 0.1, "", 1.4, m_hubFrame);
    m_btnHubDelete = new QPushButton("✕", m_hubFrame); m_btnHubDelete->setMinimumWidth(40); m_btnHubDelete->setFixedHeight(45); m_btnHubDelete->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnHubDelete->setStyleSheet("QPushButton { background: transparent; color: #888; font-family: 'Segoe UI'; font-weight: bold; border: 1px solid #444; border-radius: 6px; font-size: 14px; } QPushButton:hover { background: #E81123; color: #FFF; border: 1px solid #E81123; }");

    hubLayout->addWidget(m_btnHubToggle); hubLayout->addWidget(m_cmbHubType); hubLayout->addWidget(m_encFreq); hubLayout->addWidget(m_encGain); hubLayout->addWidget(m_encQ); hubLayout->addWidget(m_btnHubDelete); mainLayout->addWidget(m_hubFrame);

    connect(m_btnHubToggle, &QPushButton::toggled, this, &TrackPeqPanel::syncHubToTable); connect(m_cmbHubType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrackPeqPanel::syncHubToTable); connect(m_encFreq, &KaedeEncoder::valueChanged, this, &TrackPeqPanel::syncHubToTable); connect(m_encGain, &KaedeEncoder::valueChanged, this, &TrackPeqPanel::syncHubToTable); connect(m_encQ, &KaedeEncoder::valueChanged, this, &TrackPeqPanel::syncHubToTable); connect(m_btnHubDelete, &QPushButton::clicked, this, &TrackPeqPanel::removeSelectedBand);

    // 👑 FX 空間音效矩陣 - 修復文字過長擠壓版
    m_fxFrame = new QFrame(m_mainPanel); m_fxFrame->setFixedHeight(75);
    m_fxFrame->setStyleSheet("QFrame { background: rgba(15, 10, 20, 0.5); border: 1px solid rgba(56, 178, 206, 0.15); border-radius: 8px; }");
    m_fxFrame->hide();
    QHBoxLayout* fxLayout = new QHBoxLayout(m_fxFrame); fxLayout->setContentsMargins(12, 10, 12, 10); fxLayout->setSpacing(15);
    
    m_btnCfPower = new QPushButton("⏻ CROSSFEED\nMATRIX", m_fxFrame); m_btnCfPower->setFixedSize(100, 50);
    m_btnCfPower->setCheckable(true); m_btnCfPower->setChecked(false);
    m_btnCfPower->setStyleSheet("QPushButton { font-family: 'Segoe UI'; font-weight: bold; font-size: 10px; border-radius: 6px; background: rgba(255,255,255,0.05); color: #888; border: 1px solid #444; } QPushButton:checked { color: #38B2CE; background: rgba(56, 178, 206, 0.1); border: 1px solid #38B2CE; }");
    
    // ⚡ 縮減導覽文字，並強制設定寬高策略，保證滾輪不被擠壓
    QLabel* lblCfInfo = new QLabel("HEAD-STAGE SPATIALIZER\nBLEND: 15-25% | CUTOFF: 600-800Hz\nReduces L/R fatigue for headphones.", m_fxFrame);
    lblCfInfo->setStyleSheet("color: #A38C5B; font-family: 'Consolas'; font-size: 9px; font-weight: bold; background: rgba(0,0,0,0.4); border: 1px solid rgba(163, 140, 91, 0.3); border-radius: 4px; padding: 4px;");
    lblCfInfo->setWordWrap(true);
    lblCfInfo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    m_encCfLevel = new KaedeEncoder("BLEND", 0.0, 100.0, 1.0, " %", 22.0, m_fxFrame); m_encCfLevel->setFixedSize(85, 50);
    m_encCfCutoff = new KaedeEncoder("CUTOFF", 100.0, 3000.0, 10.0, " Hz", 700.0, m_fxFrame); m_encCfCutoff->setFixedSize(85, 50);
    
    fxLayout->addWidget(m_btnCfPower); fxLayout->addWidget(lblCfInfo, 1); fxLayout->addWidget(m_encCfLevel); fxLayout->addWidget(m_encCfCutoff);
    mainLayout->addWidget(m_fxFrame);

    connect(m_btnCfPower, &QPushButton::toggled, this, &TrackPeqPanel::syncFxToEngine); connect(m_encCfLevel, &KaedeEncoder::valueChanged, this, &TrackPeqPanel::syncFxToEngine); connect(m_encCfCutoff, &KaedeEncoder::valueChanged, this, &TrackPeqPanel::syncFxToEngine);

    setupTable(); mainLayout->addWidget(m_table, 1);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_btnAddBand = new QPushButton("＋ ADD", m_mainPanel); m_btnAddBand->setFixedHeight(36);
    m_btnAddBand->setStyleSheet("QPushButton { padding: 0 15px; background: rgba(56, 178, 206, 0.1); color: #38B2CE; border: 1px dashed rgba(56, 178, 206, 0.4); border-radius: 6px; font-family: 'Segoe UI'; font-size: 12px; font-weight: bold; } QPushButton:hover { background: rgba(56, 178, 206, 0.2); border: 1px solid #38B2CE; color: #FFF; } QPushButton:pressed { background: rgba(56, 178, 206, 0.4); }");
    connect(m_btnAddBand, &QPushButton::clicked, this, &TrackPeqPanel::addNewBand); bottomLayout->addWidget(m_btnAddBand);
    bottomLayout->addStretch();
    
    m_btnFxToggle = new QPushButton("FX MATRIX", m_mainPanel); m_btnFxToggle->setFixedHeight(36); m_btnFxToggle->setCheckable(true);
    m_btnFxToggle->setStyleSheet("QPushButton { background: transparent; color: #AAA; border: 1px solid #444; border-radius: 6px; font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; padding: 0 15px; } QPushButton:checked { background: rgba(56, 178, 206, 0.15); color: #FFF; border: 1px solid #38B2CE; }");
    connect(m_btnFxToggle, &QPushButton::toggled, m_fxFrame, &QFrame::setVisible); bottomLayout->addWidget(m_btnFxToggle);

    m_btnImportApo = new QPushButton("IMPORT APO", m_mainPanel); m_btnImportApo->setFixedHeight(36);
    m_btnImportApo->setStyleSheet("QPushButton { background: transparent; color: #AAA; border: 1px solid #444; border-radius: 6px; font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; padding: 0 15px; } QPushButton:hover { background: rgba(255,255,255,0.1); color: #FFF; }");
    connect(m_btnImportApo, &QPushButton::clicked, this, &TrackPeqPanel::importApoPreset); bottomLayout->addWidget(m_btnImportApo);
    m_btnExportApo = new QPushButton("EXPORT APO", m_mainPanel); m_btnExportApo->setFixedHeight(36);
    m_btnExportApo->setStyleSheet("QPushButton { background: transparent; color: #AAA; border: 1px solid #444; border-radius: 6px; font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; padding: 0 15px; } QPushButton:hover { background: rgba(255,255,255,0.1); color: #FFF; }");
    connect(m_btnExportApo, &QPushButton::clicked, this, &TrackPeqPanel::exportApoPreset); bottomLayout->addWidget(m_btnExportApo);
    mainLayout->addLayout(bottomLayout);

    // 👑 斷電動畫與曲線塌陷硬體加速特效
    m_effHub = new QGraphicsOpacityEffect(this); m_hubFrame->setGraphicsEffect(m_effHub);
    m_effTable = new QGraphicsOpacityEffect(this); m_table->setGraphicsEffect(m_effTable);
    
    m_powerAnim = new QVariantAnimation(this);
    m_powerAnim->setDuration(300); m_powerAnim->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_powerAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_currentPowerAlpha = val.toDouble();
        m_effHub->setOpacity(0.15 + 0.85 * m_currentPowerAlpha); 
        m_effTable->setOpacity(0.15 + 0.85 * m_currentPowerAlpha);
        m_curveCanvas->setPowerAlpha(m_currentPowerAlpha);        
        m_hubFrame->setEnabled(m_currentPowerAlpha > 0.1);        
        m_table->setEnabled(m_currentPowerAlpha > 0.1);
    });

    m_morphAnim = new QVariantAnimation(this);
    m_morphAnim->setDuration(350); m_morphAnim->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_morphAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        double ratio = val.toDouble();
        m_curveCanvas->setMorphRatio(ratio);
        // 👑 參數滑移核心：將 UI 動畫狀態同步化為引擎的 wetRatio 乾溼比
        KaedeAudioEngine::instance().updatePeqConfig(
            m_btnMasterPower->isChecked() || ratio > 0.001, // 智慧 Bypass：塌陷尚未完成前保持 DSP 啟動
            m_pendingCoeffs, 
            false, 
            m_currentPreampLinear, 
            ratio // 動態傳遞 Wet/Dry 比例
        );
    });

    // 👑 雙階段動畫串聯：開機(亮燈再起伏)，關機(塌陷再關燈)
    connect(m_powerAnim, &QVariantAnimation::finished, this, [this]() {
        if (m_btnMasterPower->isChecked() && m_curveCanvas->getMorphRatio() < 0.99) {
            m_morphAnim->setStartValue(m_curveCanvas->getMorphRatio()); m_morphAnim->setEndValue(1.0); m_morphAnim->start();
        }
    });
    connect(m_morphAnim, &QVariantAnimation::finished, this, [this]() {
        if (!m_btnMasterPower->isChecked() && m_currentPowerAlpha > 0.01) {
            m_powerAnim->setStartValue(m_currentPowerAlpha); m_powerAnim->setEndValue(0.0); m_powerAnim->start();
        }
    });

    connect(m_btnMasterPower, &QPushButton::toggled, this, [this](bool checked) {
        m_powerAnim->stop(); m_morphAnim->stop();
        if (checked) {
            m_powerAnim->setStartValue(m_currentPowerAlpha); m_powerAnim->setEndValue(1.0); m_powerAnim->start();
            m_lblPreamp->setText(QString("AUTO-PRE: %1 dB").arg(m_targetPreampDb, 0, 'f', 1));
            m_lblPreamp->setStyleSheet(m_targetPreampDb < 0 ? "color: #E81123; font-family: 'Consolas'; font-size: 10px; font-weight: bold; padding-right: 15px; border-right: 1px solid #444; margin-right: 15px;" : "color: #666; font-family: 'Consolas'; font-size: 10px; font-weight: bold; padding-right: 15px; border-right: 1px solid #444; margin-right: 15px;");
        } else {
            m_morphAnim->setStartValue(m_curveCanvas->getMorphRatio()); m_morphAnim->setEndValue(0.0); m_morphAnim->start();
            m_lblPreamp->setText("AUTO-PRE: BYPASS");
            m_lblPreamp->setStyleSheet("color: #E81123; font-family: 'Consolas'; font-size: 10px; font-weight: bold; padding-right: 15px; border-right: 1px solid #444; margin-right: 15px;");
        }
    });

    m_bands.push_back({true, FilterType::HighPass12, 20.0, 0.0, 0.707});
    m_bands.push_back({true, FilterType::Peaking, 1000.0, 0.0, 1.4});
    updateTableDisplay(); if(m_table->rowCount() > 0) m_table->selectRow(0);
}

void TrackPeqPanel::setupTable() {
    m_table = new QTableWidget(0, 5, m_mainPanel); 
    m_table->setHorizontalHeaderLabels({"STATE", "TYPE", "FREQ", "GAIN", "Q"}); m_table->verticalHeader()->setVisible(false); 
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection); m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->setFocusPolicy(Qt::NoFocus); m_table->setShowGrid(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed); m_table->setColumnWidth(0, 60); m_table->setColumnWidth(1, 140); m_table->setColumnWidth(2, 70); m_table->setColumnWidth(3, 70); m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch); 
    m_table->setStyleSheet("QTableWidget { background: transparent; border: none; color: #FFF; outline: none; } QHeaderView::section { background: rgba(255,255,255,0.05); color: #888; border: none; font-family: 'Consolas'; font-size: 10px; font-weight: bold; border-bottom: 1px solid rgba(255,255,255,0.1); padding-left: 5px; } QTableWidget::item { border-bottom: 1px solid rgba(255,255,255,0.05); } QTableWidget::item:selected { background: rgba(56, 178, 206, 0.2); }"); 
    m_table->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { background: transparent; width: 6px; } QScrollBar::handle:vertical { background: rgba(255,255,255,0.2); border-radius: 3px; }");
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &TrackPeqPanel::onTableSelectionChanged);
}

QString TrackPeqPanel::getTypeName(FilterType type) {
    switch(type) {
        case FilterType::Peaking: return "Peaking"; case FilterType::LowShelf: return "Low Shelf"; case FilterType::HighShelf: return "High Shelf";
        case FilterType::HighPass12: return "HP 12dB"; case FilterType::HighPass24: return "HP 24dB";
        case FilterType::LowPass12: return "LP 12dB"; case FilterType::LowPass24: return "LP 24dB";
        case FilterType::Notch: return "Notch"; case FilterType::AllPass: return "All Pass"; case FilterType::BandPass: return "Band Pass";
    } return "";
}

void TrackPeqPanel::updateTableDisplay() {
    m_table->blockSignals(true); m_table->setRowCount(m_bands.size());
    for (size_t i = 0; i < m_bands.size(); ++i) {
        const auto& b = m_bands[i]; m_table->setRowHeight(i, 35);
        QTableWidgetItem* iState = new QTableWidgetItem(b.enabled ? " ON" : " OFF"); iState->setForeground(b.enabled ? m_currentThemeColor : QColor(100,100,100)); iState->setFont(QFont("Segoe UI", 10, QFont::Bold));
        QTableWidgetItem* iType = new QTableWidgetItem(" " + getTypeName(b.type)); iType->setFont(QFont("Segoe UI", 9));
        QTableWidgetItem* iFreq = new QTableWidgetItem(QString::number(b.freq, 'f', 1) + " Hz"); iFreq->setFont(QFont("Consolas", 10));
        QTableWidgetItem* iGain = new QTableWidgetItem((b.gain > 0 ? "+" : "") + QString::number(b.gain, 'f', 1) + " dB"); iGain->setFont(QFont("Consolas", 10));
        QTableWidgetItem* iQ = new QTableWidgetItem(QString::number(b.q, 'f', 2)); iQ->setFont(QFont("Consolas", 10));
        if(!b.enabled) { iType->setForeground(QColor(100,100,100)); iFreq->setForeground(QColor(100,100,100)); iGain->setForeground(QColor(100,100,100)); iQ->setForeground(QColor(100,100,100)); }
        m_table->setItem(i, 0, iState); m_table->setItem(i, 1, iType); m_table->setItem(i, 2, iFreq); m_table->setItem(i, 3, iGain); m_table->setItem(i, 4, iQ);
    }
    m_table->blockSignals(false);
}

void TrackPeqPanel::onTableSelectionChanged() {
    auto ranges = m_table->selectedRanges();
    if (ranges.isEmpty() || ranges[0].topRow() < 0 || ranges[0].topRow() >= (int)m_bands.size()) { m_hubFrame->setEnabled(false); m_selectedIndex = -1; return; }
    m_hubFrame->setEnabled(true); m_selectedIndex = ranges[0].topRow(); const auto& b = m_bands[m_selectedIndex];

    m_btnHubToggle->blockSignals(true); m_cmbHubType->blockSignals(true); m_encFreq->blockSignals(true); m_encGain->blockSignals(true); m_encQ->blockSignals(true);
    m_btnHubToggle->setChecked(b.enabled); m_cmbHubType->setCurrentIndex(static_cast<int>(b.type)); m_encFreq->setValue(b.freq); m_encGain->setValue(b.gain); m_encQ->setValue(b.q);

    bool noGain = (b.type == FilterType::HighPass12 || b.type == FilterType::HighPass24 || b.type == FilterType::LowPass12 || b.type == FilterType::LowPass24 || b.type == FilterType::Notch || b.type == FilterType::AllPass || b.type == FilterType::BandPass);
    bool noQ = (b.type == FilterType::HighPass24 || b.type == FilterType::LowPass24);
    m_encGain->setLocked(noGain); m_encQ->setLocked(noQ);

    m_btnHubToggle->blockSignals(false); m_cmbHubType->blockSignals(false); m_encFreq->blockSignals(false); m_encGain->blockSignals(false); m_encQ->blockSignals(false);
}

void TrackPeqPanel::syncHubToTable() {
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_bands.size()) return;
    m_bands[m_selectedIndex].enabled = m_btnHubToggle->isChecked(); m_bands[m_selectedIndex].type = static_cast<FilterType>(m_cmbHubType->currentIndex()); m_bands[m_selectedIndex].freq = m_encFreq->value(); m_bands[m_selectedIndex].gain = m_encGain->value(); m_bands[m_selectedIndex].q = m_encQ->value();
    updateTableDisplay(); m_table->selectRow(m_selectedIndex); 
    FilterType t = m_bands[m_selectedIndex].type; m_encGain->setLocked(t == FilterType::HighPass12 || t == FilterType::HighPass24 || t == FilterType::LowPass12 || t == FilterType::LowPass24 || t == FilterType::Notch || t == FilterType::AllPass || t == FilterType::BandPass); m_encQ->setLocked(t == FilterType::HighPass24 || t == FilterType::LowPass24);
    onParamChanged();
}

void TrackPeqPanel::addNewBand() { m_bands.push_back({true, FilterType::Peaking, 1000.0, 0.0, 1.4}); updateTableDisplay(); m_table->selectRow(m_bands.size() - 1); onParamChanged(); }
void TrackPeqPanel::removeSelectedBand() { if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_bands.size()) { m_bands.erase(m_bands.begin() + m_selectedIndex); updateTableDisplay(); if (m_bands.empty()) m_hubFrame->setEnabled(false); else m_table->selectRow(std::max(0, m_selectedIndex - 1)); onParamChanged(); } }
void TrackPeqPanel::clearAllBands() { m_bands.clear(); updateTableDisplay(); m_hubFrame->setEnabled(false); m_selectedIndex = -1; }

FilterType TrackPeqPanel::parseApoType(const QString& apoTypeCode) { QString code = apoTypeCode.toUpper().trimmed(); if (code == "PK") return FilterType::Peaking; if (code == "LS") return FilterType::LowShelf; if (code == "HS") return FilterType::HighShelf; if (code == "HP" || code == "HP12") return FilterType::HighPass12; if (code == "HP24" || code == "LR4HP") return FilterType::HighPass24; if (code == "LP" || code == "LP12") return FilterType::LowPass12; if (code == "LP24" || code == "LR4LP") return FilterType::LowPass24; if (code == "NO") return FilterType::Notch; if (code == "AP") return FilterType::AllPass; if (code == "BP") return FilterType::BandPass; return FilterType::Peaking; }
QString TrackPeqPanel::getApoTypeCode(FilterType type) { switch (type) { case FilterType::Peaking: return "PK"; case FilterType::LowShelf: return "LS"; case FilterType::HighShelf: return "HS"; case FilterType::HighPass12: return "HP"; case FilterType::HighPass24: return "HP24"; case FilterType::LowPass12: return "LP"; case FilterType::LowPass24: return "LP24"; case FilterType::Notch: return "NO"; case FilterType::AllPass: return "AP"; case FilterType::BandPass: return "BP"; } return "PK"; }

void TrackPeqPanel::importApoPreset() {
    QString filePath = QFileDialog::getOpenFileName(this, "讀取 Standard APO/AutoEQ 預設", "", "Text Files (*.txt);;All Files (*.*)"); if (filePath.isEmpty()) return; QFile file(filePath); if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    m_syncThrottleTimer->stop(); clearAllBands(); QTextStream in(&file); QRegularExpression re("Filter\\s*\\d*\\s*:\\s*(ON|OFF)\\s+([A-Za-z0-9]+)\\s+Fc\\s+([\\d\\.]+)\\s*Hz(?:\\s+Gain\\s+([\\-\\d\\.]+)\\s*dB)?(?:\\s+Q\\s+([\\d\\.]+))?", QRegularExpression::CaseInsensitiveOption);
    while (!in.atEnd()) { QString line = in.readLine().trimmed(); if (line.isEmpty() || line.startsWith("#") || line.startsWith("Preamp")) continue; QRegularExpressionMatch match = re.match(line); if (match.hasMatch()) { bool enabled = (match.captured(1).toUpper() == "ON"); FilterType type = parseApoType(match.captured(2)); double freq = match.captured(3).toDouble(); double gain = match.captured(4).isEmpty() ? 0.0 : match.captured(4).toDouble(); double q = match.captured(5).isEmpty() ? 1.0 : match.captured(5).toDouble(); m_bands.push_back({enabled, type, freq, gain, q}); } }
    updateTableDisplay(); if (!m_bands.empty()) m_table->selectRow(0); onParamChanged();
}

void TrackPeqPanel::exportApoPreset() {
    QString filePath = QFileDialog::getSaveFileName(this, "導出為 Standard APO 格式", "Kaede_AutoEQ_Export.txt", "Text Files (*.txt)"); if (filePath.isEmpty()) return; QFile file(filePath); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return; QTextStream out(&file); out << "# Exported by KaedeDAW 128-bit Alien DSP Engine\n";
    for (size_t i = 0; i < m_bands.size(); ++i) { const auto& b = m_bands[i]; out << QString("Filter %1: %2 %3 Fc %4 Hz Gain %5 dB Q %6\n").arg(i + 1).arg(b.enabled ? "ON" : "OFF").arg(getApoTypeCode(b.type)).arg(b.freq, 0, 'f', 1).arg(b.gain, 0, 'f', 1).arg(b.q, 0, 'f', 3); }
}

void TrackPeqPanel::onParamChanged() {
    m_pendingCoeffs.clear(); double fs = 44100.0; 
    std::vector<double> scanFreqs; scanFreqs.reserve(501 + m_bands.size());
    for (int i = 0; i <= 500; ++i) scanFreqs.push_back(20.0 * std::pow(1000.0, static_cast<double>(i) / 500.0));
    
    for (const auto& b : m_bands) {
        if (!b.enabled) continue; scanFreqs.push_back(b.freq); 
        switch (b.type) {
            case FilterType::Peaking: m_pendingCoeffs.push_back(calcPeakingEQ(fs, b.freq, b.q, b.gain)); break;
            case FilterType::LowShelf: m_pendingCoeffs.push_back(calcLowShelf(fs, b.freq, b.q, b.gain)); break;
            case FilterType::HighShelf: m_pendingCoeffs.push_back(calcHighShelf(fs, b.freq, b.q, b.gain)); break;
            case FilterType::HighPass12: m_pendingCoeffs.push_back(calcHighPass(fs, b.freq, b.q)); break;
            case FilterType::LowPass12: m_pendingCoeffs.push_back(calcLowPass(fs, b.freq, b.q)); break;
            case FilterType::Notch: m_pendingCoeffs.push_back(calcNotch(fs, b.freq, b.q)); break;
            case FilterType::AllPass: m_pendingCoeffs.push_back(calcAllPass(fs, b.freq, b.q)); break;
            case FilterType::BandPass: m_pendingCoeffs.push_back(calcBandPass(fs, b.freq, b.q)); break;
            case FilterType::HighPass24: { auto bq = calcHighPass(fs, b.freq, 0.7071); m_pendingCoeffs.push_back(bq); m_pendingCoeffs.push_back(bq); break; }
            case FilterType::LowPass24: { auto bq = calcLowPass(fs, b.freq, 0.7071); m_pendingCoeffs.push_back(bq); m_pendingCoeffs.push_back(bq); break; }
        }
    }

    double maxDb = 0.0;
    for (double freqScan : scanFreqs) {
        double omega = 2.0 * M_PI * freqScan / fs; std::complex<double> z1 = std::polar(1.0, -omega); std::complex<double> z2 = std::polar(1.0, -2.0 * omega); std::complex<double> totalH(1.0, 0.0);
        for (const auto& c : m_pendingCoeffs) { std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2; std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2; totalH *= (num / den); }
        double db = 20.0 * std::log10(std::max(std::abs(totalH), 1e-9)); if (db > maxDb) maxDb = db;
    }

    m_targetPreampDb = (maxDb > 0.0) ? (-maxDb - 0.2) : 0.0; 
    m_currentPreampLinear = std::pow(10.0, m_targetPreampDb / 20.0);

    if (m_btnMasterPower->isChecked()) {
        m_lblPreamp->setText(QString("AUTO-PRE: %1 dB").arg(m_targetPreampDb, 0, 'f', 1)); 
        m_lblPreamp->setStyleSheet(m_targetPreampDb < 0 ? "color: #E81123; font-family: 'Consolas'; font-size: 10px; font-weight: bold; padding-right: 15px; border-right: 1px solid #444; margin-right: 15px;" : "color: #666; font-family: 'Consolas'; font-size: 10px; font-weight: bold; padding-right: 15px; border-right: 1px solid #444; margin-right: 15px;"); 
    }

    m_curveCanvas->updateBands(m_pendingCoeffs); m_syncThrottleTimer->start(); 
}

void TrackPeqPanel::commitToEngine() { 
    KaedeAudioEngine::instance().updatePeqConfig(
        m_btnMasterPower->isChecked() || m_curveCanvas->getMorphRatio() > 0.001, 
        m_pendingCoeffs, 
        false, 
        m_currentPreampLinear, 
        m_curveCanvas->getMorphRatio()
    ); 
}

void TrackPeqPanel::syncFxToEngine() { m_fxThrottleTimer->start(); }
void TrackPeqPanel::commitFxToEngine() { double level = m_encCfLevel->value() / 100.0; KaedeAudioEngine::instance().setCrossfeed(m_btnCfPower->isChecked(), level, m_encCfCutoff->value()); }

// =======================================================================
// 👑 3. 聲學數學模型
// =======================================================================
DspBiquad TrackPeqPanel::calcPeakingEQ(double fs, double f0, double Q, double gain) { DspBiquad c; double A = std::pow(10.0, gain / 40.0); double w0 = 2.0 * M_PI * f0 / fs; double alpha = std::sin(w0) / (2.0 * Q); double cos_w0 = std::cos(w0); double a0 = 1.0 + alpha / A; c.b0 = (1.0 + alpha * A) / a0; c.b1 = (-2.0 * cos_w0) / a0; c.b2 = (1.0 - alpha * A) / a0; c.a1 = (-2.0 * cos_w0) / a0; c.a2 = (1.0 - alpha / A) / a0; return c; }
DspBiquad TrackPeqPanel::calcLowShelf(double fs, double f0, double Q, double gain) { DspBiquad c; double A = std::pow(10.0, gain / 40.0); double w0 = 2.0 * M_PI * f0 / fs; double alpha = (std::sin(w0) / 2.0) * std::sqrt((A + 1.0/A) * (1.0/1.0 - 1.0) + 2.0) / Q; double cos_w0 = std::cos(w0); double sqA = std::sqrt(A); double a0 = (A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * sqA * alpha; c.b0 = (A * ((A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * sqA * alpha)) / a0; c.b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0)) / a0; c.b2 = (A * ((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * sqA * alpha)) / a0; c.a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cos_w0)) / a0; c.a2 = ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * sqA * alpha) / a0; return c; }
DspBiquad TrackPeqPanel::calcHighShelf(double fs, double f0, double Q, double gain) { DspBiquad c; double A = std::pow(10.0, gain / 40.0); double w0 = 2.0 * M_PI * f0 / fs; double alpha = (std::sin(w0) / 2.0) * std::sqrt((A + 1.0/A) * (1.0/1.0 - 1.0) + 2.0) / Q; double cos_w0 = std::cos(w0); double sqA = std::sqrt(A); double a0 = (A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * sqA * alpha; c.b0 = (A * ((A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * sqA * alpha)) / a0; c.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0)) / a0; c.b2 = (A * ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * sqA * alpha)) / a0; c.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cos_w0)) / a0; c.a2 = ((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * sqA * alpha) / a0; return c; }
DspBiquad TrackPeqPanel::calcHighPass(double fs, double f0, double Q) { DspBiquad c; double w0 = 2.0 * M_PI * f0 / fs; double alpha = std::sin(w0) / (2.0 * Q); double cos_w0 = std::cos(w0); double a0 = 1.0 + alpha; c.b0 = ((1.0 + cos_w0) / 2.0) / a0; c.b1 = -(1.0 + cos_w0) / a0; c.b2 = ((1.0 + cos_w0) / 2.0) / a0; c.a1 = (-2.0 * cos_w0) / a0; c.a2 = (1.0 - alpha) / a0; return c; }
DspBiquad TrackPeqPanel::calcLowPass(double fs, double f0, double Q) { DspBiquad c; double w0 = 2.0 * M_PI * f0 / fs; double alpha = std::sin(w0) / (2.0 * Q); double cos_w0 = std::cos(w0); double a0 = 1.0 + alpha; c.b0 = ((1.0 - cos_w0) / 2.0) / a0; c.b1 = (1.0 - cos_w0) / a0; c.b2 = ((1.0 - cos_w0) / 2.0) / a0; c.a1 = (-2.0 * cos_w0) / a0; c.a2 = (1.0 - alpha) / a0; return c; }
DspBiquad TrackPeqPanel::calcNotch(double fs, double f0, double Q) { DspBiquad c; double w0 = 2.0 * M_PI * f0 / fs; double alpha = std::sin(w0) / (2.0 * Q); double cos_w0 = std::cos(w0); double a0 = 1.0 + alpha; c.b0 = 1.0 / a0; c.b1 = (-2.0 * cos_w0) / a0; c.b2 = 1.0 / a0; c.a1 = (-2.0 * cos_w0) / a0; c.a2 = (1.0 - alpha) / a0; return c; }
DspBiquad TrackPeqPanel::calcAllPass(double fs, double f0, double Q) { DspBiquad c; double w0 = 2.0 * M_PI * f0 / fs; double alpha = std::sin(w0) / (2.0 * Q); double cos_w0 = std::cos(w0); double a0 = 1.0 + alpha; c.b0 = (1.0 - alpha) / a0; c.b1 = (-2.0 * cos_w0) / a0; c.b2 = (1.0 + alpha) / a0; c.a1 = (-2.0 * cos_w0) / a0; c.a2 = (1.0 - alpha) / a0; return c; }
DspBiquad TrackPeqPanel::calcBandPass(double fs, double f0, double Q) { DspBiquad c; double w0 = 2.0 * M_PI * f0 / fs; double alpha = std::sin(w0) / (2.0 * Q); double cos_w0 = std::cos(w0); double a0 = 1.0 + alpha; c.b0 = alpha / a0; c.b1 = 0.0; c.b2 = -alpha / a0; c.a1 = (-2.0 * cos_w0) / a0; c.a2 = (1.0 - alpha) / a0; return c; }

// =======================================================================
// 👑 4. 視窗控制與主題適應
// =======================================================================
void TrackPeqPanel::syncGeometry(const QRect& portalRect) { this->setGeometry(portalRect); if (m_isOpen) { m_mainPanel->setGeometry(0, 0, portalRect.width(), portalRect.height()); } else { m_mainPanel->setGeometry(portalRect.width(), 0, portalRect.width(), portalRect.height()); } }
void TrackPeqPanel::togglePanel() { m_isOpen = !m_isOpen; int panelW = this->width(); if (m_isOpen) { this->show(); this->raise(); m_slideAnim->setStartValue(QPoint(panelW, 0)); m_slideAnim->setEndValue(QPoint(0, 0)); m_slideAnim->start(); } else { m_slideAnim->setStartValue(m_mainPanel->pos()); m_slideAnim->setEndValue(QPoint(panelW, 0)); m_slideAnim->start(); } }
void TrackPeqPanel::closePanel() { if (m_isOpen) togglePanel(); }

void TrackPeqPanel::updateAdaptiveTheme(const QColor& fgColor) {
    m_currentThemeColor = fgColor;
    if (!m_isAlienMode) {
        m_lblTitle->setStyleSheet(QString("color: %1; font-family: 'Segoe UI'; font-size: 18px; font-weight: bold; letter-spacing: 2px; margin-left: 10px;").arg(fgColor.name())); 
        m_mainPanel->setStyleSheet(QString("#PeqMain { background: rgba(10, 10, 12, 0.95); border-left: 1px solid %1; border-top-left-radius: 16px; border-bottom-left-radius: 16px; }").arg(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 60).name(QColor::HexArgb))); 
        if (m_curveCanvas) m_curveCanvas->setThemeColor(fgColor); 
        m_btnAddBand->setStyleSheet(QString("QPushButton { padding: 0 15px; background: %1; color: %2; border: 1px dashed %3; border-radius: 6px; font-family: 'Segoe UI'; font-size: 12px; font-weight: bold; } QPushButton:hover { background: %4; border: 1px solid %2; color: #FFF; }").arg(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 25).name(QColor::HexArgb), fgColor.name(), QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 100).name(QColor::HexArgb), QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 50).name(QColor::HexArgb)));
        
        m_btnMasterPower->setStyleSheet(QString("QPushButton { border-radius: 15px; background: rgba(255,255,255,0.05); color: #555; font-size: 16px; } QPushButton:checked { color: %1; background: %2; border: 1px solid %1; }").arg(fgColor.name(), QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 25).name(QColor::HexArgb)));
        m_btnHubToggle->setStyleSheet(QString("QPushButton { font-size: 20px; border-radius: 6px; background: rgba(255,255,255,0.05); color: #555; } QPushButton:checked { color: %1; background: %2; border: 1px solid %1; }").arg(fgColor.name(), QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 25).name(QColor::HexArgb)));
        
        m_btnFxToggle->setStyleSheet(QString("QPushButton { background: transparent; color: #AAA; border: 1px solid #444; border-radius: 6px; font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; padding: 0 15px; } QPushButton:checked { background: %2; color: #FFF; border: 1px solid %1; }").arg(fgColor.name(), QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 30).name(QColor::HexArgb)));
        m_btnCfPower->setStyleSheet(QString("QPushButton { font-family: 'Segoe UI'; font-weight: bold; font-size: 11px; border-radius: 6px; background: rgba(255,255,255,0.05); color: #888; border: 1px solid #444; } QPushButton:checked { color: %1; background: %2; border: 1px solid %1; }").arg(fgColor.name(), QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 25).name(QColor::HexArgb)));
        m_fxFrame->setStyleSheet(QString("QFrame { background: rgba(15, 10, 20, 0.5); border: 1px solid %1; border-radius: 8px; }").arg(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 40).name(QColor::HexArgb)));

        m_encFreq->setThemeColor(fgColor); m_encGain->setThemeColor(fgColor); m_encQ->setThemeColor(fgColor);
        m_encCfLevel->setThemeColor(fgColor); m_encCfCutoff->setThemeColor(fgColor);
        updateTableDisplay(); 
    }
}

void TrackPeqPanel::toggleAlienMode() {
    m_isAlienMode = !m_isAlienMode;
    if (m_isAlienMode) {
        QColor alertColor(232, 17, 35); 
        m_lblCoreMode->setText("CORE: ALIEN-128 [SIM]"); m_lblCoreMode->setStyleSheet("color: #E81123; font-family: 'Consolas'; font-size: 10px; font-weight: bold;"); 
        m_lblTitle->setStyleSheet("color: #E81123; font-family: 'Segoe UI'; font-size: 18px; font-weight: bold; letter-spacing: 2px; margin-left: 10px;"); 
        m_mainPanel->setStyleSheet("#PeqMain { background: rgba(8, 2, 2, 0.98); border-left: 2px solid #E81123; border-top-left-radius: 16px; border-bottom-left-radius: 16px; }"); 
        if (m_curveCanvas) m_curveCanvas->setThemeColor(alertColor); 
        
        m_btnAddBand->setStyleSheet("QPushButton { padding: 0 15px; background: rgba(232, 17, 35, 0.1); color: #E81123; border: 1px dashed #E81123; border-radius: 6px; font-weight: bold; } QPushButton:hover { background: rgba(232, 17, 35, 0.3); color: #FFF; }");
        m_btnMasterPower->setStyleSheet("QPushButton { border-radius: 15px; background: rgba(255,255,255,0.05); color: #555; font-size: 16px; } QPushButton:checked { color: #E81123; background: rgba(232, 17, 35, 0.1); border: 1px solid #E81123; }");
        m_btnHubToggle->setStyleSheet("QPushButton { font-size: 20px; border-radius: 6px; background: rgba(255,255,255,0.05); color: #555; } QPushButton:checked { color: #E81123; background: rgba(232, 17, 35, 0.1); border: 1px solid #E81123; }");
        
        m_btnFxToggle->setStyleSheet("QPushButton { background: transparent; color: #AAA; border: 1px solid #444; border-radius: 6px; font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; padding: 0 15px; } QPushButton:checked { background: rgba(232, 17, 35, 0.15); color: #FFF; border: 1px solid #E81123; }");
        m_btnCfPower->setStyleSheet("QPushButton { font-family: 'Segoe UI'; font-weight: bold; font-size: 11px; border-radius: 6px; background: rgba(255,255,255,0.05); color: #888; border: 1px solid #444; } QPushButton:checked { color: #E81123; background: rgba(232, 17, 35, 0.1); border: 1px solid #E81123; }");
        m_fxFrame->setStyleSheet("QFrame { background: rgba(15, 0, 0, 0.5); border: 1px solid rgba(232, 17, 35, 0.4); border-radius: 8px; }");

        m_encFreq->setThemeColor(alertColor); m_encGain->setThemeColor(alertColor); m_encQ->setThemeColor(alertColor);
        m_encCfLevel->setThemeColor(alertColor); m_encCfCutoff->setThemeColor(alertColor);
        updateTableDisplay();
    } else { 
        m_lblCoreMode->setText("CORE: STD-64"); m_lblCoreMode->setStyleSheet("color: #666; font-family: 'Consolas'; font-size: 10px; font-weight: bold;"); 
        updateAdaptiveTheme(m_currentThemeColor); 
    }
}
bool TrackPeqPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_lblCoreMode && event->type() == QEvent::MouseButtonPress) { m_clickCount++; if (m_clickCount >= 5) { m_clickCount = 0; toggleAlienMode(); } return true; } return QWidget::eventFilter(watched, event);
}