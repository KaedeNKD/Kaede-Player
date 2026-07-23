#include "MainWindow.h"
#include "GB.h"  
#include "ThemeManager.h"
#include "AdaptiveColorEngine.h" 

#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QApplication>
#include <QEvent>
#include <QWindow> 
#include <QDebug>
#include <QDateTime>
#include <QScrollBar>
#include <cmath> // 👑 補上數學函式庫
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment (lib, "Dwmapi.lib")
#pragma comment (lib, "User32.lib")
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // 👑 堅持使用最穩定的 D3D11，防止切換高頻 Resize 時的渲染管線崩潰
    qputenv("QSG_RHI_BACKEND", "d3d11");
    
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint); 
    resize(1280, 720); setMinimumSize(800, 600);
#ifdef _WIN32
    HWND hwnd = (HWND)this->winId(); LONG style = GetWindowLong(hwnd, GWL_STYLE); SetWindowLong(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);
    MARGINS margins = { 1, 1, 1, 1 }; DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif

    m_currentTextColor = ThemeManager::instance().textHigh(); m_targetTextColor = m_currentTextColor;
    m_currentBgColor = QColor(180, 225, 255); m_targetBgColor = m_currentBgColor;
    m_colorWaveAnim = new QVariantAnimation(this); m_colorWaveAnim->setDuration(850); m_colorWaveAnim->setStartValue(0.0); m_colorWaveAnim->setEndValue(1.0);
    connect(m_colorWaveAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) { animateWave(value.toDouble()); });
    connect(m_colorWaveAnim, &QVariantAnimation::finished, this, [this]() { m_currentTextColor = m_targetTextColor; m_currentBgColor = m_targetBgColor; animateWave(1.0); });
    m_themeDelayTimer = new QTimer(this); m_themeDelayTimer->setSingleShot(true);
    connect(m_themeDelayTimer, &QTimer::timeout, this, [this]() { if (m_currentTextColor != m_targetTextColor || m_currentBgColor != m_targetBgColor) m_colorWaveAnim->start(); });

    setupUi(); 
    AdaptiveColorEngine::instance().extractColorFromImage(":/img/BG.png");
    m_currentTextColor = AdaptiveColorEngine::instance().getTextColor(experimentalAdaptiveFontColor); m_targetTextColor = m_currentTextColor;
    m_currentBgColor = AdaptiveColorEngine::instance().getPanelBackgroundColor(experimentalAdaptiveFontColor); m_targetBgColor = m_currentBgColor;
    animateWave(1.0); 
}

void MainWindow::playTrackFromModel(QAbstractItemModel* model, int index) {
    if (index < 0 || index >= model->rowCount() || !m_audioEngine) return;
    
    m_currentPlayModel = model; 
    m_currentTrackIndex = index; 
    QModelIndex idx = model->index(index, 0);
    
    QString path = model->data(idx, KaedeTrackModel::PathRole).toString(); 
    QString title = model->data(idx, KaedeTrackModel::TitleRole).toString();
    QString artist = model->data(idx, KaedeTrackModel::ArtistRole).toString(); 
    QString coverUrl = model->data(idx, KaedeTrackModel::CoverUrlRole).toString();
    
    QImage coverImg; if (coverUrl.startsWith("file:///")) coverImg.load(coverUrl.mid(8)); 
    TrackInfo info = MediaMetadataParser::instance().parse(path);
    if (info.coverImg.isNull() && !coverImg.isNull()) info.coverImg = coverImg; 
    else if (!info.coverImg.isNull()) coverImg = info.coverImg;
    
    m_playbackConsole->updateTrackInfo(title, artist, coverImg);
    if (m_expandedPanel) m_expandedPanel->updateTrackData(coverImg, info.lyrics);
    if (m_analyzerPanel) m_analyzerPanel->loadTrack(path);
    if (m_audioEngine->load(path)) m_audioEngine->play(); else qDebug() << "[Host] Failed to load track:" << path;
}

void MainWindow::playNextTrack() { 
    if (!m_currentPlayModel) return;
    int count = m_currentPlayModel->rowCount(); if (count == 0) return; 
    int nextIndex = m_currentTrackIndex + 1; if (nextIndex >= count) nextIndex = 0; 
    playTrackFromModel(m_currentPlayModel, nextIndex); 
}

void MainWindow::playPrevTrack() { 
    if (!m_currentPlayModel) return;
    int count = m_currentPlayModel->rowCount(); if (count == 0) return; 
    int prevIndex = m_currentTrackIndex - 1; if (prevIndex < 0) prevIndex = count - 1; 
    playTrackFromModel(m_currentPlayModel, prevIndex); 
}

void MainWindow::setupUi() {
    KaedeDatabase::instance().init();
    m_currentPlayModel = KaedeDatabase::instance().getTrackModel(); 
    m_audioEngine = &KaedeAudioEngine::instance(); 
    m_audioEngine->init(); 

    m_centralWidget = new QWidget(this); m_centralWidget->setObjectName("MainCentralWidget");
    m_fluidBg = new GBBackgroundWidget(m_centralWidget); m_fluidBg->setGeometry(this->rect());
    
    m_libraryContainer = new QWidget(m_centralWidget); 
    m_libraryContainer->setGeometry(this->rect()); 
    
    m_libOpacity = new QGraphicsOpacityEffect(m_libraryContainer);
    m_libOpacity->setOpacity(1.0);
    m_libraryContainer->setGraphicsEffect(m_libOpacity);

    m_libFadeAnim = new QPropertyAnimation(m_libOpacity, "opacity", this);
    m_libFadeAnim->setDuration(450); 
    m_libFadeAnim->setEasingCurve(QEasingCurve::InOutSine);
    
    QVBoxLayout* libOuterLayout = new QVBoxLayout(m_libraryContainer); libOuterLayout->setContentsMargins(20, 45, 20, 110); 
    
    m_libraryPanel = new KaedeLibraryPanel(m_libraryContainer);
    libOuterLayout->addWidget(m_libraryPanel);
    connect(m_libraryPanel, &KaedeLibraryPanel::sigPlayTrackRequest, this, &MainWindow::playTrackFromModel);

    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget); mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);
    m_titleBar = new QWidget(m_centralWidget); m_titleBar->setFixedHeight(38); m_titleBar->setObjectName("AppTitleBar");
    QHBoxLayout *titleLayout = new QHBoxLayout(m_titleBar); titleLayout->setContentsMargins(15, 0, 0, 0);
    QLabel *titleLabel = new QLabel("Kaede Player", m_titleBar); titleLabel->setObjectName("TitleLabel"); titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLayout->addWidget(titleLabel); titleLayout->addStretch();
    
    m_btnSettings = new QPushButton(QString::fromUtf8("設定"), m_centralWidget); m_btnSettings->setFixedSize(80, 36); 
    connect(m_btnSettings, &QPushButton::clicked, this, &MainWindow::toggleSettingsMatrix); 
    
    m_btnMin = new QPushButton("—", m_titleBar); m_btnMin->setFixedSize(45, 38); connect(m_btnMin, &QPushButton::clicked, this, &MainWindow::showMinimized); titleLayout->addWidget(m_btnMin);
    m_btnMax = new QPushButton(QString::fromUtf8("\xE2\x96\xA1"), m_titleBar); m_btnMax->setFixedSize(45, 38); connect(m_btnMax, &QPushButton::clicked, this, &MainWindow::toggleMaximize); titleLayout->addWidget(m_btnMax);
    m_btnClose = new QPushButton("✕", m_titleBar); m_btnClose->setFixedSize(45, 38); connect(m_btnClose, &QPushButton::clicked, this, &MainWindow::close); titleLayout->addWidget(m_btnClose);
    mainLayout->addWidget(m_titleBar); mainLayout->addStretch(); 
    
    m_playbackConsole = new PlaybackConsole(m_centralWidget); m_progressBar = new ProgressBarIsland(m_centralWidget);
    
    m_dspPanel = new DspVisualizerPanel(m_centralWidget); 
    m_dspPanel->setStyleSheet("background: #0A0806; border-top-left-radius: 16px; border-top-right-radius: 16px; border-top: 1px solid rgba(255, 255, 255, 0.05);");
    m_dspPanel->hide();
    
    m_analyzerPanel = new TrackAnalyzerPanel(this);
    m_peqPanel = new TrackPeqPanel(this); 
    
    m_btnAnalyzerToggle = new QPushButton("INFO", m_centralWidget); m_btnAnalyzerToggle->setFixedSize(70, 36);
    m_btnAnalyzerToggle->setStyleSheet("QPushButton { background: rgba(0, 0, 0, 0.4); color: #EAEAEA; border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 18px; font-family: 'Segoe UI', 'Consolas'; font-size: 13px; font-weight: bold; letter-spacing: 1px; } QPushButton:hover { background: rgba(255, 255, 255, 0.15); border: 1px solid rgba(255, 255, 255, 0.3); color: #FFFFFF; } QPushButton:pressed { background: rgba(0, 0, 0, 0.6); }");
    connect(m_btnAnalyzerToggle, &QPushButton::clicked, this, [this]() {
        if (m_analyzerPanel) {
            QPoint globalTopLeft = this->mapToGlobal(QPoint(0, 38)); int consoleY = this->height() - 80 - 20; int targetHeight = consoleY - 38 - 15; int pWidth = static_cast<int>(this->width() * 0.6); 
            m_analyzerPanel->syncGeometry(QRect(globalTopLeft.x(), globalTopLeft.y(), pWidth, targetHeight)); m_analyzerPanel->togglePanel();
        }
    });

    m_btnPeqToggle = new QPushButton("EQ", m_centralWidget); m_btnPeqToggle->setFixedSize(70, 36);
    m_btnPeqToggle->setStyleSheet("QPushButton { background: rgba(0, 0, 0, 0.4); color: #EAEAEA; border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 18px; font-family: 'Segoe UI', 'Consolas'; font-size: 13px; font-weight: bold; letter-spacing: 1px; } QPushButton:hover { background: rgba(255, 255, 255, 0.15); border: 1px solid rgba(255, 255, 255, 0.3); color: #FFFFFF; } QPushButton:pressed { background: rgba(0, 0, 0, 0.6); }");
    connect(m_btnPeqToggle, &QPushButton::clicked, this, [this]() {
        if (m_peqPanel) {
            QPoint globalTopLeft = this->mapToGlobal(QPoint(0, 38)); int consoleY = this->height() - 80 - 20; int targetHeight = consoleY - 38 - 15; int pWidth = static_cast<int>(this->width() * 0.4); int pLeft = this->width() - pWidth; 
            m_peqPanel->syncGeometry(QRect(globalTopLeft.x() + pLeft, globalTopLeft.y(), pWidth, targetHeight)); m_peqPanel->togglePanel();
        }
    });

    auto injectMinimizeBtn = [this](QWidget* panel, QPushButton* toggleBtn) {
        QFrame* targetFrame = nullptr;
        for (QFrame* f : panel->findChildren<QFrame*>()) { if (f->objectName().contains("Main", Qt::CaseInsensitive)) { targetFrame = f; break; } }
        if (!targetFrame) { for (QFrame* f : panel->findChildren<QFrame*>()) { if (f->parent() == panel) { targetFrame = f; break; } } }
        if (targetFrame && targetFrame->layout()) {
            QPushButton* btnMin = new QPushButton("—", targetFrame); btnMin->setCursor(Qt::PointingHandCursor);
            btnMin->setStyleSheet("QPushButton { color: #A0A0A0; background: transparent; border: none; font-family: 'Segoe UI'; font-size: 16px; font-weight: bold; margin-bottom: 2px; } QPushButton:hover { color: #FFFFFF; background: rgba(255,255,255,0.15); border-radius: 4px; } QPushButton:pressed { background: rgba(0,0,0,0.4); }");
            QObject::connect(btnMin, &QPushButton::clicked, toggleBtn, &QPushButton::click);
            bool injected = false; QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(targetFrame->layout());
            if (mainLayout && mainLayout->count() > 0) { QHBoxLayout* titleLayout = qobject_cast<QHBoxLayout*>(mainLayout->itemAt(0)->layout()); if (titleLayout) { btnMin->setFixedSize(30, 24); titleLayout->addSpacing(10); titleLayout->addWidget(btnMin); injected = true; } }
            if (!injected) { btnMin->setObjectName("InjectedMinBtnFallback"); btnMin->setGeometry(targetFrame->width() - 40, 20, 30, 24); targetFrame->installEventFilter(this); }
        }
    };
    injectMinimizeBtn(m_analyzerPanel, m_btnAnalyzerToggle); injectMinimizeBtn(m_peqPanel, m_btnPeqToggle);

    m_settingsContainer = new QWidget(this); m_settingsContainer->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint); m_settingsContainer->setAttribute(Qt::WA_TranslucentBackground); m_settingsContainer->hide();
    m_settingsPanel = new QFrame(m_settingsContainer); m_settingsPanel->setObjectName("SettingsPanel"); m_settingsPanel->setFixedWidth(380);
    m_settingsAnim = new QPropertyAnimation(m_settingsPanel, "pos", this); m_settingsAnim->setDuration(300); m_settingsAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_settingsAnim, &QPropertyAnimation::finished, this, [this]() { if (!m_isSettingsOpen) m_settingsContainer->hide(); });

    QVBoxLayout* setPageLayout = new QVBoxLayout(m_settingsPanel); setPageLayout->setContentsMargins(25, 25, 25, 30); setPageLayout->setSpacing(15);
    QLabel* lblCfg = new QLabel("CONFIG MATRIX", m_settingsPanel); lblCfg->setStyleSheet("color: #FFFFFF; font-family: 'Segoe UI'; font-size: 20px; font-weight: bold; background: transparent; letter-spacing: 1px;"); setPageLayout->addWidget(lblCfg);

    auto makeGroupCard = [this, setPageLayout](const QString& title) -> QVBoxLayout* {
        QWidget* card = new QWidget(m_settingsPanel); card->setStyleSheet("QWidget { background: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255, 255, 255, 0.06); border-radius: 8px; }");
        QVBoxLayout* l = new QVBoxLayout(card); l->setContentsMargins(15, 15, 15, 15); l->setSpacing(12);
        QLabel* t = new QLabel(title, card); t->setStyleSheet("color: #A0A0A0; font-family: 'Consolas'; font-size: 11px; font-weight: bold; border: none; background: transparent;"); l->addWidget(t);
        QFrame* line = new QFrame(card); line->setFrameShape(QFrame::HLine); line->setStyleSheet("background: rgba(255, 255, 255, 0.08); border: none; max-height: 1px;"); l->addWidget(line);
        setPageLayout->addWidget(card); return l;
    };

    QString comboQSS = "QComboBox { background: rgba(0,0,0,0.5); color: #FFFFFF; font-family: 'Segoe UI'; font-size: 12px; font-weight: bold; border: 1px solid #444; border-radius: 4px; padding: 6px 12px; } QComboBox::drop-down { border: none; width: 30px; } QComboBox QAbstractItemView { background: #1A1A1A; color: #FFFFFF; border: 1px solid #444; selection-background-color: #38B2CE; }";

    QVBoxLayout* dspLayout = makeGroupCard("DSP CORE & UPSAMPLING");
    m_cmbCoreMode = new QComboBox(m_settingsPanel); m_cmbCoreMode->setStyleSheet(comboQSS);
    m_cmbCoreMode->addItem("64-bit IIR", static_cast<int>(DspCoreMode::Standard_64));
    m_cmbCoreMode->addItem("FIR (Polyphase Sinc)", static_cast<int>(DspCoreMode::Alien_FIR_128));
    dspLayout->addWidget(m_cmbCoreMode);

    m_cmbFirTaps = new QComboBox(m_settingsPanel); m_cmbFirTaps->setStyleSheet(comboQSS);
    m_cmbFirTaps->addItem("64 Taps", 64); m_cmbFirTaps->addItem("128 Taps", 128); m_cmbFirTaps->addItem("256 Taps", 256); m_cmbFirTaps->addItem("512 Taps", 512);
    dspLayout->addWidget(m_cmbFirTaps);

    m_cmbTargetRate = new QComboBox(m_settingsPanel); m_cmbTargetRate->setStyleSheet(comboQSS);
    m_cmbTargetRate->addItem("Native (1x PCM)", 0); m_cmbTargetRate->addItem("192 kHz (4x PCM)", 192000); m_cmbTargetRate->addItem("384 kHz (8x PCM)", 384000); m_cmbTargetRate->addItem("768 kHz (16x PCM)", 768000); m_cmbTargetRate->addItem("Auto (Hardware Limit)", -1);
    dspLayout->addWidget(m_cmbTargetRate);

    m_chkNoiseShaping = new QCheckBox(" 2nd-Order Noise Shaping (TPDF)", m_settingsPanel);
    m_chkNoiseShaping->setStyleSheet("QCheckBox { color: #EAEAEA; font-family: 'Segoe UI'; font-size: 13px; font-weight: bold; background: transparent; border: none; spacing: 10px; margin-top: 4px; } QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 2px solid #666; background: rgba(0, 0, 0, 0.4); } QCheckBox::indicator:checked { background: #EAEAEA; border: 2px solid #EAEAEA; }");
    dspLayout->addWidget(m_chkNoiseShaping);

    int savedCoreMode = KaedeDatabase::instance().getConfig("audio_core_mode", static_cast<int>(DspCoreMode::Standard_64)).toInt();
    int savedTaps = KaedeDatabase::instance().getConfig("audio_fir_taps", 128).toInt();
    int savedRate = KaedeDatabase::instance().getConfig("audio_target_rate", -1).toInt();
    bool savedNS = KaedeDatabase::instance().getConfig("audio_noise_shaping", true).toBool();
    m_cmbCoreMode->setCurrentIndex(m_cmbCoreMode->findData(savedCoreMode) != -1 ? m_cmbCoreMode->findData(savedCoreMode) : 0);
    m_cmbFirTaps->setCurrentIndex(m_cmbFirTaps->findData(savedTaps) != -1 ? m_cmbFirTaps->findData(savedTaps) : 1);
    m_cmbTargetRate->setCurrentIndex(m_cmbTargetRate->findData(savedRate) != -1 ? m_cmbTargetRate->findData(savedRate) : 4);
    m_chkNoiseShaping->setChecked(savedNS); 
    
    auto updateDspUi = [this, comboQSS]() {
        bool isFir = m_cmbCoreMode->currentData().toInt() == static_cast<int>(DspCoreMode::Alien_FIR_128);
        m_cmbFirTaps->setEnabled(isFir); m_cmbTargetRate->setEnabled(isFir);
        QString disabledQSS = comboQSS + " QComboBox { color: #555; border: 1px solid #333; background: rgba(0,0,0,0.2); }";
        m_cmbFirTaps->setStyleSheet(isFir ? comboQSS : disabledQSS);
        m_cmbTargetRate->setStyleSheet(isFir ? comboQSS : disabledQSS);
    };
    updateDspUi();
    connect(m_cmbCoreMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, updateDspUi](int index) { updateDspUi(); DspCoreMode mode = static_cast<DspCoreMode>(m_cmbCoreMode->itemData(index).toInt()); KaedeDatabase::instance().setConfig("audio_core_mode", static_cast<int>(mode)); m_audioEngine->setDspCoreMode(mode); });
    auto onFirConfigChanged = [this]() { int taps = m_cmbFirTaps->currentData().toInt(); int rate = m_cmbTargetRate->currentData().toInt(); KaedeDatabase::instance().setConfig("audio_fir_taps", taps); KaedeDatabase::instance().setConfig("audio_target_rate", rate); m_audioEngine->setAlienFirConfig(taps, rate); };
    connect(m_cmbFirTaps, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onFirConfigChanged);
    connect(m_cmbTargetRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onFirConfigChanged);
    connect(m_chkNoiseShaping, &QCheckBox::toggled, this, [this](bool checked) { KaedeDatabase::instance().setConfig("audio_noise_shaping", checked); if (m_audioEngine) m_audioEngine->setNoiseShaping(checked); });

    QVBoxLayout* audioLayout = makeGroupCard("ASIO/WASAPI HARDWARE ROUTING");
    m_cmbApi = new QComboBox(m_settingsPanel); m_cmbApi->setStyleSheet(comboQSS);
    m_cmbApi->addItem("WASAPI Shared (Mixer)", QVariant::fromValue(OutputMode::SharedMixer)); m_cmbApi->addItem("WASAPI Exclusive", QVariant::fromValue(OutputMode::WASAPI_Exclusive)); m_cmbApi->addItem("ASIO (Bit-Perfect)", QVariant::fromValue(OutputMode::ASIO));
    audioLayout->addWidget(m_cmbApi); m_cmbDevice = new QComboBox(m_settingsPanel); m_cmbDevice->setStyleSheet(comboQSS); audioLayout->addWidget(m_cmbDevice);
    connect(m_cmbApi, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::populateDeviceList);
    connect(m_cmbDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onApiOrDeviceChanged);

    QVBoxLayout* appLayout = makeGroupCard("UI APPEARANCE & THEME");
    m_chkAdaptiveColor = new QCheckBox(" Adaptive Font Color", m_settingsPanel); m_chkAdaptiveColor->setChecked(experimentalAdaptiveFontColor);
    m_chkAdaptiveColor->setStyleSheet("QCheckBox { color: #EAEAEA; font-family: 'Segoe UI'; font-size: 13px; font-weight: bold; background: transparent; border: none; spacing: 10px; } QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 2px solid #666; background: rgba(0, 0, 0, 0.4); } QCheckBox::indicator:checked { background: #EAEAEA; border: 2px solid #EAEAEA; }");
    connect(m_chkAdaptiveColor, &QCheckBox::toggled, this, [this](bool checked) { experimentalAdaptiveFontColor = checked; AdaptiveColorEngine::instance().setAdaptiveEnabled(checked); QColor newTargetTxt = AdaptiveColorEngine::instance().getTextColor(experimentalAdaptiveFontColor); QColor newTargetBg = AdaptiveColorEngine::instance().getPanelBackgroundColor(experimentalAdaptiveFontColor); if (newTargetTxt != m_targetTextColor || newTargetBg != m_targetBgColor) { m_targetTextColor = newTargetTxt; m_targetBgColor = newTargetBg; if (m_colorWaveAnim->state() == QAbstractAnimation::Running) m_colorWaveAnim->stop(); m_colorWaveAnim->start(); } });
    appLayout->addWidget(m_chkAdaptiveColor);
    QHBoxLayout* bgLayoutBtn = new QHBoxLayout(); m_btnSetBg = new QPushButton("CUSTOM BG", m_settingsPanel); m_btnSetBg->setFixedHeight(36); m_btnClearBg = new QPushButton("RESET", m_settingsPanel); m_btnClearBg->setFixedSize(80, 36);
    connect(m_btnSetBg, &QPushButton::clicked, this, &MainWindow::selectCustomBackground); connect(m_btnClearBg, &QPushButton::clicked, this, [this](){ if(m_fluidBg) m_fluidBg->clearBackground(); updateDominantColor(":/img/BG.png"); });
    bgLayoutBtn->addWidget(m_btnSetBg); bgLayoutBtn->addWidget(m_btnClearBg); appLayout->addLayout(bgLayoutBtn);
    // 👑 補回遺失的媒體庫設定區塊
    QVBoxLayout* dbLayout = makeGroupCard("DATABASE & LIBRARY");
    m_btnLibConfig = new QPushButton("MEDIA LIBRARY CONFIG", m_settingsPanel); m_btnLibConfig->setFixedHeight(44);
    m_btnLibConfig->setStyleSheet("QPushButton { background: rgba(56, 178, 206, 0.15); color: #38B2CE; border: 1px solid rgba(56, 178, 206, 0.4); border-radius: 6px; font-family: 'Consolas'; font-size: 13px; font-weight: bold; letter-spacing: 1px; } QPushButton:hover { background: rgba(56, 178, 206, 0.3); color: #FFFFFF; border: 1px solid #38B2CE; } QPushButton:pressed { background: rgba(56, 178, 206, 0.5); }");
    dbLayout->addWidget(m_btnLibConfig);
    connect(m_btnLibConfig, &QPushButton::clicked, this, &MainWindow::toggleLibMode);

    setPageLayout->addStretch();
    setCentralWidget(m_centralWidget);

    setPageLayout->addStretch(); 
    setCentralWidget(m_centralWidget); 
    
    m_libPanel = new QWidget(m_centralWidget); m_libPanel->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 0.95), stop:1 rgba(240, 245, 250, 0.88)); border: 1px solid rgba(255, 255, 255, 0.9); border-radius: 16px;"); m_libPanel->hide();

    QVBoxLayout* libLayout = new QVBoxLayout(m_libPanel); libLayout->setContentsMargins(40, 40, 40, 40); libLayout->setSpacing(20);
    QHBoxLayout* workspaceHeader = new QHBoxLayout(); QLabel* libTitle = new QLabel("NEXT-GEN RELATIONAL DATABASE WORKSPACE", m_libPanel);
    libTitle->setStyleSheet("color: #1A1A1A; font-family: 'Consolas'; font-size: 24px; font-weight: bold; background: transparent; border: none; letter-spacing: 2px;");
    m_btnLibClose = new QPushButton("✕ CLOSE WORKSPACE", m_libPanel); m_btnLibClose->setFixedSize(160, 36);
    m_btnLibClose->setStyleSheet("QPushButton { background: rgba(0, 0, 0, 0.04); color: #4A4A4A; border: 1px solid #B0B0B0; border-radius: 4px; font-family: 'Segoe UI'; font-weight: bold; } QPushButton:hover { background: #E81123; color: #FFFFFF; border: 1px solid #E81123; } QPushButton:pressed { background: #B00D1B; color: #FFFFFF; }");
    connect(m_btnLibClose, &QPushButton::clicked, this, &MainWindow::toggleLibMode);
    workspaceHeader->addWidget(libTitle); workspaceHeader->addStretch(); workspaceHeader->addWidget(m_btnLibClose); libLayout->addLayout(workspaceHeader);

    QHBoxLayout* statsLayout = new QHBoxLayout();
    auto makeStatCard = [this](const QString& title, const QString& id) { QWidget* card = new QWidget(m_libPanel); card->setStyleSheet("background: rgba(0, 0, 0, 0.05); border: 1px solid rgba(0, 0, 0, 0.1); border-radius: 8px;"); QVBoxLayout* l = new QVBoxLayout(card); QLabel* t = new QLabel(title, card); t->setStyleSheet("color: #666; font-family: 'Consolas'; font-size: 12px; font-weight: bold; border: none; background: transparent;"); QLabel* v = new QLabel("0", card); v->setObjectName(id); v->setStyleSheet("color: #1A1A1A; font-family: 'Segoe UI'; font-size: 42px; font-weight: bold; border: none; background: transparent;"); l->addWidget(t); l->addWidget(v); return card; };
    statsLayout->addWidget(makeStatCard("TRACKS INDEXED", "statTracks")); statsLayout->addWidget(makeStatCard("EST. TOTAL SIZE (MB)", "statCache")); libLayout->addLayout(statsLayout);
    
    QHBoxLayout* terminalLayout = new QHBoxLayout(); terminalLayout->setSpacing(20);
    QVBoxLayout* dirLayout = new QVBoxLayout(); QLabel* dirLbl = new QLabel("MOUNTED DIRECTORIES", m_libPanel); dirLbl->setStyleSheet("color: #4A4A4A; font-family: 'Consolas'; font-size: 14px; font-weight: bold; background: transparent; border: none;");
    m_dirList = new QListWidget(m_libPanel); m_dirList->setStyleSheet("QListWidget { background: rgba(0, 0, 0, 0.03); color: #1A1A1A; border: 1px solid rgba(0, 0, 0, 0.1); border-radius: 8px; font-family: 'Segoe UI'; font-size: 13px; padding: 5px; } QListWidget::item { padding: 5px; border-bottom: 1px solid rgba(0,0,0,0.05); }");
    dirLayout->addWidget(dirLbl); dirLayout->addWidget(m_dirList);
    
    QVBoxLayout* logLayout = new QVBoxLayout(); QLabel* logLbl = new QLabel("LIVE SCAN TERMINAL", m_libPanel); logLbl->setStyleSheet("color: #4A4A4A; font-family: 'Consolas'; font-size: 14px; font-weight: bold; background: transparent; border: none;");
    m_scanLog = new QTextEdit(m_libPanel); m_scanLog->setReadOnly(true); m_scanLog->setStyleSheet("QTextEdit { background: rgba(10, 15, 20, 0.95); color: #00FFB4; border: 1px solid rgba(0, 255, 180, 0.4); border-radius: 8px; font-family: 'Consolas'; font-size: 12px; padding: 10px; }"); m_scanLog->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { background: transparent; width: 10px; } QScrollBar::handle:vertical { background: rgba(0, 255, 180, 0.4); border-radius: 5px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
    logLayout->addWidget(logLbl); logLayout->addWidget(m_scanLog);
    terminalLayout->addLayout(dirLayout, 1); terminalLayout->addLayout(logLayout, 2); libLayout->addLayout(terminalLayout, 1); 

    QHBoxLayout* actionLayout = new QHBoxLayout(); m_btnScan = new QPushButton("INDEX DIRECTORY", m_libPanel); m_btnScan->setFixedHeight(44);
    m_btnScan->setStyleSheet("QPushButton { background: #38B2CE; color: #FFFFFF; border: none; border-radius: 6px; font-family: 'Segoe UI'; font-size: 14px; font-weight: bold; } QPushButton:hover { background: #4AC0DC; } QPushButton:pressed { background: #2A9AB5; }");
    connect(m_btnScan, &QPushButton::clicked, this, &MainWindow::selectLibraryFolder);
    QPushButton* btnPurge = new QPushButton("PURGE DATABASE", m_libPanel); btnPurge->setFixedHeight(44); btnPurge->setStyleSheet("QPushButton { background: transparent; color: #E81123; border: 1px solid #E81123; border-radius: 6px; font-family: 'Segoe UI'; font-size: 14px; font-weight: bold; } QPushButton:hover { background: #E81123; color: #FFFFFF; } QPushButton:pressed { background: #B00D1B; border: 1px solid #B00D1B; }");
    connect(btnPurge, &QPushButton::clicked, this, [this](){ KaedeDatabase::instance().purgeDatabase(); if(m_dirList) m_dirList->clear(); if(m_scanLog) m_scanLog->append("[SYSTEM] 數據庫已銷毀，所有結構與緩存已清理並強迫重建。"); updateDbStats(); });
    actionLayout->addWidget(m_btnScan); actionLayout->addWidget(btnPurge); libLayout->addLayout(actionLayout);

    m_libTransitionAnim = new QVariantAnimation(this); m_libTransitionAnim->setDuration(600); m_libTransitionAnim->setEasingCurve(QEasingCurve::OutCubic); m_libTransitionAnim->setStartValue(0.0); m_libTransitionAnim->setEndValue(1.0);
    connect(m_libTransitionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) { m_libProgress = val.toDouble(); updateDynamicLayout(); });

    connect(&KaedeDatabase::instance(), &KaedeDatabase::scanStarted, this, [this](){ m_btnScan->setText("SCANNING..."); m_btnScan->setEnabled(false); m_scanLog->clear(); m_scanLog->append("[SYSTEM] 啟深層掃描引擎..."); m_scanLog->append("[SYSTEM] 正在為執行緒分配 SQLite 安全連線..."); });
    connect(&KaedeDatabase::instance(), &KaedeDatabase::scanProgress, this, [this](int count, const QString& file){ m_btnScan->setText(QString("INDEXING... %1 TRACKS").arg(count)); m_scanLog->append(QString("> FOUND: %1").arg(file)); m_scanLog->verticalScrollBar()->setValue(m_scanLog->verticalScrollBar()->maximum()); });
    connect(&KaedeDatabase::instance(), &KaedeDatabase::scanFinished, this, [this](int total, int added){ m_btnScan->setText("INDEX DIRECTORY"); m_btnScan->setEnabled(true); m_scanLog->append(QString("\n[SYSTEM] 掃描完畢！共檢查 %1 個檔案，新增/修改 %2 條資料。").arg(total).arg(added)); m_scanLog->verticalScrollBar()->setValue(m_scanLog->verticalScrollBar()->maximum()); updateDbStats(); });

    m_expandedPanel = new PlayerExpandedPanel(m_centralWidget);
    if (m_fluidBg) m_fluidBg->lower(); if (m_libraryContainer) m_libraryContainer->raise(); if (m_titleBar) m_titleBar->raise(); if (m_playbackConsole) m_playbackConsole->raise(); if (m_progressBar) m_progressBar->raise(); if (m_dspPanel) m_dspPanel->raise(); if (m_libPanel) m_libPanel->raise(); if (m_expandedPanel) m_expandedPanel->raise(); if (m_btnAnalyzerToggle) m_btnAnalyzerToggle->raise(); if (m_btnPeqToggle) m_btnPeqToggle->raise(); if (m_settingsContainer) m_settingsContainer->raise();

    m_dspTransitionAnim = new QVariantAnimation(this); 
    m_dspTransitionAnim->setDuration(800); 
    connect(m_dspTransitionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) { m_dspProgress = val.toDouble(); updateDynamicLayout(); });
    
    // 👑 嚴格生命週期管理：動畫開始時切斷音訊渲染，結束時恢復並清理殘留
    connect(m_dspTransitionAnim, &QAbstractAnimation::stateChanged, this, [this](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) { 
        Q_UNUSED(oldState); 
        if (newState == QAbstractAnimation::Running) { 
            m_isDspTransitioning = true; // 上鎖：拒絕高頻重繪
        } else if (newState == QAbstractAnimation::Stopped) { 
            m_isDspTransitioning = false; // 解鎖
            
            // 如果是徹底收起狀態，做最終的資源回收
            if (!m_isDspMode) {
                if (m_dspPanel) m_dspPanel->hide();
                if (m_progressBar) m_progressBar->setDspMode(false);
                if (m_fluidBg) m_fluidBg->setSelectingState(false);
            } else {
                // 如果是徹底展開狀態，隱藏底層清單省資源
                if (m_libraryContainer) m_libraryContainer->hide();
            }
        } 
    });

    m_audioEngine->setDspCoreMode(static_cast<DspCoreMode>(savedCoreMode)); m_audioEngine->setAlienFirConfig(savedTaps, savedRate); m_audioEngine->setNoiseShaping(savedNS);
    int savedApiMode = KaedeDatabase::instance().getConfig("audio_api_mode", static_cast<int>(OutputMode::SharedMixer)).toInt();
    m_cmbApi->blockSignals(true); int apiIndex = m_cmbApi->findData(savedApiMode); if (apiIndex != -1) m_cmbApi->setCurrentIndex(apiIndex); m_cmbApi->blockSignals(false);
    populateDeviceList();

    connect(m_audioEngine, &KaedeAudioEngine::playbackStateChanged, this, [this](bool playing) { if (m_playbackConsole) m_playbackConsole->setPlayState(playing); if (m_progressBar) m_progressBar->setPlaybackState(playing); });
    connect(m_audioEngine, &KaedeAudioEngine::trackFinished, this, &MainWindow::playNextTrack);
    connect(m_audioEngine, &KaedeAudioEngine::positionChanged, this, [this](double currSec, double totalSec) { static qint64 lastUpdate = 0; qint64 now = QDateTime::currentMSecsSinceEpoch(); if (now - lastUpdate >= 50) { if (m_progressBar) m_progressBar->setProgress(currSec, totalSec); if (m_expandedPanel) m_expandedPanel->updateLyricPosition(currSec); lastUpdate = now; } });
    connect(m_progressBar, &ProgressBarIsland::sigSeekRequested, this, [this](double percent) { if (m_audioEngine) m_audioEngine->seek(percent * m_audioEngine->getDuration()); });
    connect(m_playbackConsole, &PlaybackConsole::sigPlayClicked, this, [this]() { if (!m_audioEngine) return; if (m_audioEngine->isPlaying()) { m_audioEngine->pause(); } else { if (m_currentTrackIndex == -1 && KaedeDatabase::instance().getTrackModel()->rowCount() > 0) playTrackFromModel(m_currentPlayModel, 0); else m_audioEngine->play(); } });
    connect(m_playbackConsole, &PlaybackConsole::sigStopClicked, this, [this]() { if (m_audioEngine) m_audioEngine->stop(); if (m_playbackConsole) m_playbackConsole->resetToIdle(); if (m_progressBar) { m_progressBar->setProgress(0, 0); m_progressBar->setPlaybackState(false); } if (m_expandedPanel) m_expandedPanel->clearTrackData(); m_currentTrackIndex = -1; });
    connect(m_playbackConsole, &PlaybackConsole::sigPrevClicked, this, &MainWindow::playPrevTrack); connect(m_playbackConsole, &PlaybackConsole::sigNextClicked, this, &MainWindow::playNextTrack);
    connect(m_playbackConsole, &PlaybackConsole::sigConsoleClicked, this, [this]() { if (m_isDspMode || m_isLibMode || m_isDspTransitioning) return; m_isExpandedPanelOpen = !m_isExpandedPanelOpen; if (m_expandedPanel) m_expandedPanel->animateToggle(m_playbackConsole->geometry()); });
    
    // 👑 終極無縫並行動畫觸發點
    connect(m_playbackConsole, &PlaybackConsole::sigDspClicked, this, [this]() {
        if (m_isLibMode || m_isDspTransitioning) return; 
        
        if (m_isExpandedPanelOpen) { 
            m_isExpandedPanelOpen = false; 
            if (m_expandedPanel) m_expandedPanel->animateToggle(m_playbackConsole->geometry()); 
        }
        
        m_isDspMode = !m_isDspMode;
        
        if (m_isDspMode) {
            if (m_analyzerPanel && m_analyzerPanel->isOpen()) m_analyzerPanel->closePanel();
            if (m_peqPanel && m_peqPanel->isOpen()) m_peqPanel->closePanel();
            if (m_libraryContainer) m_libraryContainer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            
            // 同步呼叫內部深色玻璃特效，與主畫面的淡出疊加，效果更立體
            if (m_libraryPanel) m_libraryPanel->setDimMode(true);
            
            m_libFadeAnim->stop();
            m_libFadeAnim->setStartValue(m_libOpacity->opacity());
            m_libFadeAnim->setEndValue(0.0);
            m_libFadeAnim->start();

            if (m_progressBar) m_progressBar->setDspMode(true); 
            if (m_fluidBg) m_fluidBg->setSelectingState(true);

            // 啟動主過渡（拔除了這裡愚蠢的 pre-show，將其延後到 updateDynamicLayout 處理）
            m_dspTransitionAnim->stop();
            m_dspTransitionAnim->setStartValue(m_dspProgress);
            m_dspTransitionAnim->setEndValue(1.0);
            m_dspTransitionAnim->start();
        } else {
            // 收起時
            if (m_libraryContainer) {
                m_libraryContainer->show(); 
                m_libraryContainer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
            }
            if (m_libraryPanel) m_libraryPanel->setDimMode(false);
            
            m_libFadeAnim->stop();
            m_libFadeAnim->setStartValue(m_libOpacity->opacity());
            m_libFadeAnim->setEndValue(1.0);
            m_libFadeAnim->start();

            m_dspTransitionAnim->stop();
            m_dspTransitionAnim->setStartValue(m_dspProgress);
            m_dspTransitionAnim->setEndValue(0.0);
            m_dspTransitionAnim->start();
        }
    });
    
    // 👑 保護傘：過渡期間直接 Drop 掉所有的 FFT 寫入請求，確保幀率平滑，絕不閃退
    connect(m_audioEngine, &KaedeAudioEngine::dspDataReady, this, [this](const std::vector<float>& pcm, const std::vector<float>& fft) { 
        if (!m_isDspTransitioning && m_dspPanel) {
            m_dspPanel->updateAudioData(pcm, fft); 
        }
    });

    applyStaticTheme(); updateDbStats(); 
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Resize) {
        if (QFrame* frame = qobject_cast<QFrame*>(watched)) {
            QPushButton* btnMin = frame->findChild<QPushButton*>("InjectedMinBtnFallback");
            if (btnMin) btnMin->setGeometry(frame->width() - 40, 20, 30, 24);
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::populateDeviceList() { 
    if (!m_cmbApi || !m_cmbDevice || !m_audioEngine) return; 
    m_cmbDevice->blockSignals(true); m_cmbDevice->clear(); 
    OutputMode selectedMode = m_cmbApi->currentData().value<OutputMode>(); 
    QList<AudioDeviceInfo> devices = m_audioEngine->getDeviceList(selectedMode); 
    for (const auto& dev : std::as_const(devices)) { m_cmbDevice->addItem(dev.name, dev.id); } 
    if (devices.isEmpty()) { m_cmbDevice->addItem("No Compatible Devices Found", -1); } 
    QString savedDeviceName = KaedeDatabase::instance().getConfig("audio_device_name", "").toString();
    int devIndex = m_cmbDevice->findText(savedDeviceName);
    if (devIndex == -1) devIndex = m_cmbDevice->findData(KaedeDatabase::instance().getConfig("audio_device_id", -1).toInt());
    if (devIndex != -1) { m_cmbDevice->setCurrentIndex(devIndex); } else if (m_cmbDevice->count() > 0) { m_cmbDevice->setCurrentIndex(0); }
    m_cmbDevice->blockSignals(false); onApiOrDeviceChanged(); 
}

void MainWindow::onApiOrDeviceChanged() { 
    if (!m_cmbApi || !m_cmbDevice || !m_audioEngine) return; 
    OutputMode mode = m_cmbApi->currentData().value<OutputMode>(); int deviceId = m_cmbDevice->currentData().toInt(); QString deviceName = m_cmbDevice->currentText(); 
    if (deviceId != -1) { m_audioEngine->setOutputDevice(mode, deviceId); KaedeDatabase::instance().setConfig("audio_api_mode", static_cast<int>(mode)); KaedeDatabase::instance().setConfig("audio_device_id", deviceId); KaedeDatabase::instance().setConfig("audio_device_name", deviceName); } 
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef _WIN32
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) { NCCALCSIZE_PARAMS *pncsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam); if (IsZoomed(msg->hwnd)) { HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONULL); if (monitor) { MONITORINFO mi; mi.cbSize = sizeof(mi); GetMonitorInfo(monitor, &mi); pncsp->rgrc[0] = mi.rcWork; } } *result = 0; return true; }
    if (msg->message == WM_NCHITTEST) {
        long physX = GET_X_LPARAM(msg->lParam); long physY = GET_Y_LPARAM(msg->lParam); RECT winrect; GetWindowRect(msg->hwnd, &winrect);
        int lx = physX - winrect.left; int ly = physY - winrect.top; int w = winrect.right - winrect.left; int h = winrect.bottom - winrect.top; qreal dpr = this->devicePixelRatioF(); int border = static_cast<int>(8 * dpr);
        if (ly < border && lx < border) { *result = HTTOPLEFT; return true; } if (ly < border && lx >= w - border) { *result = HTTOPRIGHT; return true; } if (ly >= h - border && lx < border) { *result = HTBOTTOMLEFT; return true; } if (ly >= h - border && lx >= w - border) { *result = HTBOTTOMRIGHT; return true; } if (lx < border) { *result = HTLEFT; return true; } if (lx >= w - border) { *result = HTRIGHT; return true; } if (ly >= h - border) { *result = HTBOTTOM; return true; } if (ly < border) { *result = HTTOP; return true; } int titleH = static_cast<int>(38 * dpr); int btnW = static_cast<int>(220 * dpr); if (ly <= titleH && lx < w - btnW) { *result = HTCAPTION; return true; } *result = HTCLIENT; return true;
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::applyStaticTheme() {
    if (!m_centralWidget) return; 
    m_centralWidget->setStyleSheet("#MainCentralWidget { background: #0A0806; border: 1px solid rgba(255,255,255,0.05); }");
    m_titleBar->setStyleSheet("#AppTitleBar { background-color: transparent; border-bottom: 1px solid rgba(255, 255, 255, 0.1); }");
    QString glassQSS = "background: rgba(12, 10, 9, 0.96); border-left: 1px solid rgba(255, 255, 255, 0.1);";
    if (m_settingsPanel) m_settingsPanel->setStyleSheet(QString("#SettingsPanel { %1 }").arg(glassQSS));
}

void MainWindow::toggleLibMode() {
    m_isLibMode = !m_isLibMode;
    if (m_isLibMode) { if (m_isDspMode) { m_isDspMode = false; m_dspTransitionAnim->setDirection(QAbstractAnimation::Backward); m_dspTransitionAnim->start(); if (m_progressBar) m_progressBar->setDspMode(false); } if (m_isSettingsOpen) toggleSettingsMatrix(); updateDbStats(); m_libTransitionAnim->setDirection(QAbstractAnimation::Forward); } else { m_libTransitionAnim->setDirection(QAbstractAnimation::Backward); }
    m_libTransitionAnim->start(); if (m_fluidBg) m_fluidBg->setSelectingState(m_isLibMode || m_isDspMode); if (m_libraryContainer) m_libraryContainer->setAttribute(Qt::WA_TransparentForMouseEvents, m_isLibMode || m_isDspMode);
}

void MainWindow::updateDbStats() {
    if (!m_libPanel) return; QLabel* lblTracks = m_libPanel->findChild<QLabel*>("statTracks"); if (lblTracks) lblTracks->setText(QString::number(KaedeDatabase::instance().getTrackModel()->rowCount()));
    QLabel* lblCache = m_libPanel->findChild<QLabel*>("statCache"); if (lblCache) lblCache->setText(QString::number(KaedeDatabase::instance().getTotalAudioSizeMB(), 'f', 1));
    if (m_dirList) { m_dirList->clear(); m_dirList->addItems(KaedeDatabase::instance().getMountedDirectories()); }
}

// 👑 完整補回：同時支援 DSP 與媒體庫面板的 Smoothstep 幾何動畫引擎
void MainWindow::updateDynamicLayout() {
    int consoleW = width() * 0.8; if(consoleW > 1200) consoleW = 1200; if(consoleW < 700) consoleW = width() - 40;
    int consoleH = 80; int progW = consoleW - 30; int progH = 20;
    int normConsoleX = (width() - consoleW) / 2; int normConsoleY = height() - consoleH - 20; int normProgX = (width() - progW) / 2; int normProgY = normConsoleY - progH - 5;

    if (m_btnSettings) { m_btnSettings->setGeometry(20, height() - 56, 80, 36); m_btnSettings->raise(); }
    if (m_btnPeqToggle) { m_btnPeqToggle->setGeometry(width() - 95, normConsoleY + 2, 70, 36); m_btnPeqToggle->raise(); }
    if (m_btnAnalyzerToggle) { m_btnAnalyzerToggle->setGeometry(width() - 95, normConsoleY + 42, 70, 36); m_btnAnalyzerToggle->raise(); }

    // 👑 案發現場：這裡把你遺失的媒體庫展開動畫補回來了！
    if (m_libProgress > 0.01) {
        if (m_libPanel) m_libPanel->show();
        double p = m_libProgress;
        double ease_p = p * p * (3.0 - 2.0 * p); // 一樣上了高質感的 Smoothstep 曲線

        int curConsoleY = normConsoleY + (150 * ease_p);
        int curProgY = normProgY + (150 * ease_p);

        int targetW = width() * 0.85; if (targetW > 1400) targetW = 1400;
        int targetH = height() - 140;
        int targetX = (width() - targetW) / 2;
        int targetY = 70;
        int startY = height() + 50;
        int curLibY = startY + (targetY - startY) * ease_p;

        if (m_libPanel->width() != targetW || m_libPanel->height() != targetH) m_libPanel->resize(targetW, targetH);
        m_libPanel->move(targetX, curLibY);
        m_libPanel->raise();
        m_titleBar->raise();

        if (m_playbackConsole) m_playbackConsole->setGeometry(normConsoleX, curConsoleY, consoleW, consoleH);
        if (m_progressBar) m_progressBar->setGeometry(normProgX, curProgY, progW, progH);
        if (m_btnPeqToggle) m_btnPeqToggle->setGeometry(width() - 95, curConsoleY + 2, 70, 36);
        if (m_btnAnalyzerToggle) m_btnAnalyzerToggle->setGeometry(width() - 95, curConsoleY + 42, 70, 36);

        return; // 展開圖書館時，中斷後續 DSP 佈局的運算
    } else {
        if (m_libPanel) m_libPanel->hide();
    }

    // 👑 DSP 佈局區塊 (保持上一版的穩定狀態)
    int dspConsoleY = 50; int dspProgX = (width() - progW) / 2; int dspProgY = dspConsoleY + consoleH + 15;
    int curConsoleX = normConsoleX; int curConsoleY, curProgX, curProgY;

    if (m_dspProgress <= 0.5) {
        double p = m_dspProgress * 2.0;
        p = p * p * (3.0 - 2.0 * p);
        curConsoleY = normConsoleY + (150 * p);
        curProgX = normProgX - ((width()/2 + progW) * p);
        curProgY = normProgY;
        if (m_dspPanel && !m_dspPanel->isHidden()) m_dspPanel->hide();
    } else {
        double p = (m_dspProgress - 0.5) * 2.0;
        p = p * p * (3.0 - 2.0 * p);
        curConsoleY = -100 + ((dspConsoleY + 100) * p);
        curProgX = -progW + ((dspProgX + progW) * p);
        curProgY = dspProgY;

        if (m_dspPanel) {
            int dspPanelW = width();
            int dspPanelY = dspProgY + progH + 20;
            int dspPanelH = height() - dspPanelY - 10;
            int curDspX = width() - (width() * p);
            m_dspPanel->setGeometry(curDspX, dspPanelY, dspPanelW, dspPanelH);
            if (m_dspPanel->isHidden()) m_dspPanel->show();
            m_dspPanel->raise();
            m_titleBar->raise();
        }
    }

    if (m_playbackConsole) m_playbackConsole->setGeometry(curConsoleX, curConsoleY, consoleW, consoleH);
    if (m_progressBar) m_progressBar->setGeometry(curProgX, curProgY, progW, progH);
    if (m_btnPeqToggle) m_btnPeqToggle->setGeometry(width() - 95, curConsoleY + 2, 70, 36);
    if (m_btnAnalyzerToggle) m_btnAnalyzerToggle->setGeometry(width() - 95, curConsoleY + 42, 70, 36);
}

void MainWindow::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event); QPoint globalTopLeft = this->mapToGlobal(QPoint(0, 38));
    if (m_settingsContainer && m_isSettingsOpen) m_settingsContainer->move(globalTopLeft.x() + this->width() - 380, globalTopLeft.y());
    if (m_analyzerPanel) { int consoleY = this->height() - 80 - 20; int targetHeight = consoleY - 38 - 15; int pWidth = static_cast<int>(this->width() * 0.6); m_analyzerPanel->syncGeometry(QRect(globalTopLeft.x(), globalTopLeft.y(), pWidth, targetHeight)); }
    if (m_peqPanel) { int consoleY = this->height() - 80 - 20; int targetHeight = consoleY - 38 - 15; int pWidth = static_cast<int>(this->width() * 0.4); int pLeft = this->width() - pWidth; m_peqPanel->syncGeometry(QRect(globalTopLeft.x() + pLeft, globalTopLeft.y(), pWidth, targetHeight)); }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    if(m_fluidBg) m_fluidBg->setGeometry(this->rect()); 
    if(m_libraryContainer) m_libraryContainer->setGeometry(this->rect()); 
    
    QPoint globalTopLeft = this->mapToGlobal(QPoint(0, 38)); int targetHeight = this->height() - 38;
    if (m_settingsContainer) { m_settingsContainer->setGeometry(globalTopLeft.x() + this->width() - 380, globalTopLeft.y(), 380, targetHeight); if (m_isSettingsOpen) m_settingsPanel->setGeometry(0, 0, 380, targetHeight); else m_settingsPanel->setGeometry(380, 0, 380, targetHeight); }
    if (m_analyzerPanel) { int consoleY = this->height() - 80 - 20; int pHeight = consoleY - 38 - 15; int pWidth = static_cast<int>(this->width() * 0.6); m_analyzerPanel->syncGeometry(QRect(globalTopLeft.x(), globalTopLeft.y(), pWidth, pHeight)); }
    if (m_peqPanel) { int consoleY = this->height() - 80 - 20; int pHeight = consoleY - 38 - 15; int pWidth = static_cast<int>(this->width() * 0.4); int pLeft = this->width() - pWidth; m_peqPanel->syncGeometry(QRect(globalTopLeft.x() + pLeft, globalTopLeft.y(), pWidth, pHeight)); }
    if (m_expandedPanel) m_expandedPanel->setGeometry(0, 0, width(), height() - 135); updateDynamicLayout(); QMainWindow::resizeEvent(event);
}

// 👑 完整補回：設定面板按鈕的色彩動態推播
void MainWindow::animateWave(double progress) {
    if (m_currentTextColor == m_targetTextColor && m_currentBgColor == m_targetBgColor && progress < 1.0) return;
    double waveX = progress * (this->width() + 600.0) - 300.0; double waveWidth = 400.0;
    auto blendColor = [&](const QColor& c1, const QColor& c2, double ratio) { int r = c1.red() + ratio * (c2.red() - c1.red()); int g = c1.green() + ratio * (c2.green() - c1.green()); int b = c1.blue() + ratio * (c2.blue() - c1.blue()); return QColor(r, g, b); };
    auto applyToWidget = [&](QWidget* w, bool isSysBtn, bool isMaxBtn) {
        if (!w) return; int wx = w->mapTo(this, QPoint(0,0)).x(); double localP = qBound(0.0, (waveX - wx) / waveWidth, 1.0); localP = localP * localP * (3.0 - 2.0 * localP); QColor blendedFg = blendColor(m_currentTextColor, m_targetTextColor, localP);
        if (isSysBtn) w->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; font-family: 'Segoe UI'; font-size: %2px; } QPushButton:hover { background: rgba(255, 255, 255, 0.1); } QPushButton:pressed { background: rgba(255, 255, 255, 0.05); }").arg(blendedFg.name()).arg(isMaxBtn ? 14 : 12));
        else if (w == m_btnClose) w->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; font-family: 'Segoe UI'; font-size: 12px; } QPushButton:hover { background: #E81123; color: #FFFFFF; } QPushButton:pressed { background: #F1707A; color: #FFFFFF; }").arg(blendedFg.name()));
        else if (qobject_cast<QLabel*>(w)) w->setStyleSheet(QString("color: %1; font-weight: bold; font-family: 'Consolas'; font-size: 13px; background: transparent;").arg(blendedFg.name()));
        else w->setStyleSheet(QString("QPushButton { background: rgba(255, 255, 255, 0.05); color: %1; border: 1px solid %2; border-radius: 4px; font-family: 'Segoe UI', 'Consolas'; font-weight: bold; letter-spacing: 1px; } QPushButton:hover { background: rgba(255, 255, 255, 0.15); color: #FFFFFF; } QPushButton:pressed { background: rgba(0, 0, 0, 0.4); }").arg(blendedFg.name(), ThemeManager::instance().border().name()));
    };
    applyToWidget(m_titleBar->findChild<QLabel*>("TitleLabel"), false, false);
    applyToWidget(m_btnSettings, false, false);
    applyToWidget(m_btnMin, true, false);
    applyToWidget(m_btnMax, true, true);
    applyToWidget(m_btnClose, false, false);

    // 👑 案發現場：這裡把你設定面板裡的按鈕補回來了！
    applyToWidget(m_btnSetBg, false, false);
    applyToWidget(m_btnClearBg, false, false);
    applyToWidget(m_btnLibConfig, false, false);

    if (m_libraryPanel) {
        int panelCenter = m_libraryPanel->mapTo(this, QPoint(m_libraryPanel->width()/2, 0)).x();
        double panelP = qBound(0.0, (waveX - panelCenter) / waveWidth, 1.0);
        panelP = panelP * panelP * (3.0 - 2.0 * panelP);
        m_libraryPanel->updateThemeColors(blendColor(m_currentTextColor, m_targetTextColor, panelP), blendColor(m_currentBgColor, m_targetBgColor, panelP));
    }

    if (m_playbackConsole) { int consoleCenter = m_playbackConsole->mapTo(this, QPoint(m_playbackConsole->width()/2, 0)).x(); double consoleP = qBound(0.0, (waveX - consoleCenter) / waveWidth, 1.0); consoleP = consoleP * consoleP * (3.0 - 2.0 * consoleP); m_playbackConsole->updateAdaptiveTheme(blendColor(m_currentBgColor, m_targetBgColor, consoleP), blendColor(m_currentTextColor, m_targetTextColor, consoleP)); }
    if (m_analyzerPanel) { int pCenter = m_analyzerPanel->mapTo(this, QPoint(m_analyzerPanel->width()/2, 0)).x(); double pP = qBound(0.0, (waveX - pCenter) / waveWidth, 1.0); m_analyzerPanel->updateAdaptiveTheme(blendColor(m_currentTextColor, m_targetTextColor, pP * pP * (3.0 - 2.0 * pP))); }
    if (m_peqPanel) { int pCenter = m_peqPanel->mapTo(this, QPoint(m_peqPanel->width()/2, 0)).x(); double pP = qBound(0.0, (waveX - pCenter) / waveWidth, 1.0); m_peqPanel->updateAdaptiveTheme(blendColor(m_currentTextColor, m_targetTextColor, pP * pP * (3.0 - 2.0 * pP))); }
}

void MainWindow::closeEvent(QCloseEvent *event) { if (m_audioEngine) m_audioEngine->destroy(); QMainWindow::closeEvent(event); }

void MainWindow::selectCustomBackground() { 
    QString path = QFileDialog::getOpenFileName(this, "Select Background Image", "", "Images (*.png *.jpg *.jpeg *.bmp *.webp)"); 
    if (!path.isEmpty() && m_fluidBg) { 
        // 👑 傳入原始路徑即可，GBBackgroundWidget 內部會自行處理 QUrl
        m_fluidBg->setBackgroundImage(path); 
        updateDominantColor(path); 
    } 
}

void MainWindow::showImportDialog() { 
    DropZoneDialog dialog(this); 
    if (dialog.exec() == QDialog::Accepted) { 
        QStringList files = dialog.getImportedFiles(); 
        for (const QString& file : std::as_const(files)) { 
            if (file.endsWith(".png", Qt::CaseInsensitive) || file.endsWith(".jpg", Qt::CaseInsensitive) || file.endsWith(".webp", Qt::CaseInsensitive)) { 
                if (m_fluidBg) m_fluidBg->setBackgroundImage(file); 
                updateDominantColor(file); 
                break; 
            } 
        } 
    } 
}

void MainWindow::selectLibraryFolder() { QString dir = QFileDialog::getExistingDirectory(this, "Scan Music"); if(dir.isEmpty()) return; KaedeDatabase::instance().scanDirectory(dir); }


void MainWindow::toggleSettingsMatrix() { 
    if (!m_settingsContainer || !m_settingsPanel || !m_settingsAnim) return; m_isSettingsOpen = !m_isSettingsOpen; int panelWidth = 380; QPoint globalTopLeft = this->mapToGlobal(QPoint(0, 38)); int targetHeight = this->height() - 38; m_settingsContainer->setGeometry(globalTopLeft.x() + this->width() - panelWidth, globalTopLeft.y(), panelWidth, targetHeight);
    if(m_isSettingsOpen) { m_settingsPanel->setGeometry(panelWidth, 0, panelWidth, targetHeight); m_settingsContainer->show(); m_settingsContainer->raise(); m_settingsAnim->setStartValue(QPoint(panelWidth, 0)); m_settingsAnim->setEndValue(QPoint(0, 0)); m_settingsAnim->start(); } else { m_settingsAnim->setStartValue(m_settingsPanel->pos()); m_settingsAnim->setEndValue(QPoint(panelWidth, 0)); m_settingsAnim->start(); }
}
void MainWindow::updateDominantColor(const QString& path) { if (!m_centralWidget) return; AdaptiveColorEngine::instance().extractColorFromImage(path); QColor newTargetTxt = AdaptiveColorEngine::instance().getTextColor(experimentalAdaptiveFontColor); QColor newTargetBg = AdaptiveColorEngine::instance().getPanelBackgroundColor(experimentalAdaptiveFontColor); if (newTargetTxt != m_targetTextColor || newTargetBg != m_targetBgColor) { m_targetTextColor = newTargetTxt; m_targetBgColor = newTargetBg; m_themeDelayTimer->start(2800); } }
void MainWindow::toggleMaximize() { if(isMaximized()) { showNormal(); if (m_btnMax) m_btnMax->setText(QString::fromUtf8("\xE2\x96\xA1")); } else { showMaximized(); if (m_btnMax) m_btnMax->setText(QString::fromUtf8("\xE2\x9D\x90")); } }