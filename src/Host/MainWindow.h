#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QVariant>
#include <QImage>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QWidget>
#include <QMoveEvent> 
#include <QFrame> 
#include <QListWidget>
#include <QTextEdit>
#include <QGraphicsOpacityEffect> // 👑 加回硬體透明度引擎

// 引入解耦的組件
#include "Views/KaedeLibraryPanel.h"
#include "DropZoneDialog.h" 
#include "PlaybackConsole.h"
#include "PlayerExpandedPanel.h" 
#include "ProgressBarIsland.h" 
#include "DspVisualizerPanel.h" 
#include "KaedeAudioEngine.h" 
#include "MediaMetadataParser.h" 
#include "KaedeDatabase.h"
#include "TrackAnalyzerPanel.h"
#include "TrackPeqPanel.h" 

class GBBackgroundWidget; 

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    bool experimentalAdaptiveFontColor = true;
    
    QColor m_currentTextColor;
    QColor m_targetTextColor;
    QColor m_currentBgColor;
    QColor m_targetBgColor;
    
    QVariantAnimation* m_colorWaveAnim = nullptr;
    QTimer* m_themeDelayTimer = nullptr;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override; 
    void resizeEvent(QResizeEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *event) override; 

private slots:
    void toggleSettingsMatrix();
    void toggleMaximize();
    void selectLibraryFolder();
    void selectCustomBackground();
    void showImportDialog();
    
    void playTrackFromModel(QAbstractItemModel* model, int index); 
    void playNextTrack();
    void playPrevTrack();

    void populateDeviceList();
    void onApiOrDeviceChanged();
    void toggleLibMode();
    void updateDbStats();

private:
    void setupUi();
    void applyStaticTheme(); 
    void animateWave(double progress); 
    void updateDominantColor(const QString& path);
    void updateDynamicLayout();

    QWidget* m_centralWidget = nullptr;
    GBBackgroundWidget* m_fluidBg = nullptr; 

    QWidget* m_titleBar = nullptr;
    QPushButton* m_btnSettings = nullptr;
    QPushButton* m_btnMin = nullptr;
    QPushButton* m_btnMax = nullptr;
    QPushButton* m_btnClose = nullptr;
    
    // 👑 找回 Library 硬體淡出動畫
    QWidget* m_libraryContainer = nullptr;
    QGraphicsOpacityEffect* m_libOpacity = nullptr;
    QPropertyAnimation* m_libFadeAnim = nullptr;
    KaedeLibraryPanel* m_libraryPanel = nullptr;
    
    QWidget* m_settingsContainer = nullptr;
    QFrame* m_settingsPanel = nullptr;
    QPropertyAnimation* m_settingsAnim = nullptr; 
    
    TrackAnalyzerPanel* m_analyzerPanel = nullptr;
    QPushButton* m_btnAnalyzerToggle = nullptr;

    TrackPeqPanel* m_peqPanel = nullptr;
    QPushButton* m_btnPeqToggle = nullptr;
    
    QPushButton* m_btnSetBg = nullptr;   
    QPushButton* m_btnClearBg = nullptr; 
    QCheckBox* m_chkAdaptiveColor = nullptr; 
    
    QComboBox* m_cmbCoreMode = nullptr;
    QComboBox* m_cmbFirTaps = nullptr;    
    QComboBox* m_cmbTargetRate = nullptr; 
    QCheckBox* m_chkNoiseShaping = nullptr;
    QComboBox* m_cmbApi = nullptr;
    QComboBox* m_cmbDevice = nullptr;
    QPushButton* m_btnLibConfig = nullptr; 

    PlaybackConsole* m_playbackConsole = nullptr;
    PlayerExpandedPanel* m_expandedPanel = nullptr;
    ProgressBarIsland* m_progressBar = nullptr; 
    DspVisualizerPanel* m_dspPanel = nullptr; 
    
    bool m_isExpandedPanelOpen = false;
    bool m_isDspMode = false;
    
    // 👑 狀態機與渲染鎖
    QVariantAnimation* m_dspTransitionAnim = nullptr;
    double m_dspProgress = 0.0;
    bool m_isDspTransitioning = false; 
    
    bool m_isLibMode = false;
    QWidget* m_libPanel = nullptr; 
    QPushButton* m_btnLibClose = nullptr; 
    QPushButton* m_btnScan = nullptr; 
    QListWidget* m_dirList = nullptr; 
    QTextEdit* m_scanLog = nullptr;   
    QVariantAnimation* m_libTransitionAnim = nullptr;
    double m_libProgress = 0.0;
    
    KaedeAudioEngine* m_audioEngine = nullptr;
    int m_currentTrackIndex = -1;
    QAbstractItemModel* m_currentPlayModel = nullptr; 
    bool m_isSettingsOpen = false;
};

#endif // MAINWINDOW_H