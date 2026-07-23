#include "GB.h"
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QFileInfo>

// =======================================================================
// 👑 GB 核心 QML：Win11 級光流交織完全體 (支援 QRC 內部資源載入 + 狀態機修復)
// =======================================================================
const char* GB_QML_SOURCE = R"QML(
import QtQuick
import QtQuick.Effects

Rectangle {
    id: root
    color: '#0A0806'
    
    // ⚡ 預設背景資源路徑 (供清除後恢復使用)
    property string defaultBgUrl: ""
    
    property string bgUrl: ''
    property bool isSelecting: false
    property bool internalLock: false
    
    property int activeSlot: 0
    property int nextSlot: 0
    
    property string url1: ''
    property string url2: ''
    property real op1: 0.0
    property real op2: 0.0
    property int z1: 0
    property int z2: 0
    
    property real scale1: 1.0
    property real scale2: 1.0
    
    property real sceneBlur: 0.0
    property real glowIntensity: 0.0
    
    property bool ready1: customBg1.status === Image.Ready
    property bool ready2: customBg2.status === Image.Ready
    
    onIsSelectingChanged: {
        if (internalLock) return;
        if (isSelecting && activeSlot > 0) {
            uxBlurAnim.stop(); uxBlurAnim.to = 1.0; uxBlurAnim.start();
        } else if (!isSelecting && nextSlot === 0 && activeSlot > 0) {
            uxBlurAnim.stop(); uxBlurAnim.to = 0.0; uxBlurAnim.start();
        }
    }
    NumberAnimation { id: uxBlurAnim; target: root; property: 'sceneBlur'; duration: 600; easing.type: Easing.InOutQuad }
    
    onBgUrlChanged: {
        // ⚡ 攔截邏輯：如果是清空操作，則強制指向預設背景 (如果有)
        var targetUrl = bgUrl;
        if (targetUrl === '') {
            if (defaultBgUrl !== '') {
                targetUrl = defaultBgUrl;
            } else {
                // 如果連預設背景都沒有，才執行真正的清除
                if (activeSlot > 0) clearAnim.start();
                return;
            }
        }
        
        // 避免重複載入同一張圖片
        if ((activeSlot === 1 && url1 === targetUrl) || (activeSlot === 2 && url2 === targetUrl)) {
            return;
        }

        if (activeSlot === 0) {
            nextSlot = 1; url1 = targetUrl; z1 = 1; z2 = 0; scale1 = 1.10;
        } else {
            nextSlot = (activeSlot === 1) ? 2 : 1;
            if (nextSlot === 1) { url1 = targetUrl; op1 = 0.0; z1 = 2; z2 = 1; scale1 = 1.10; }
            else { url2 = targetUrl; op2 = 0.0; z2 = 2; z1 = 1; scale2 = 1.10; }
        }
    }
    
    Timer {
        id: warmupTimer
        interval: 100 
        onTriggered: {
            if (activeSlot === 0) firstLoadAnim.start();
            else crossfadeAnim.start();
        }
    }
    onReady1Changed: { if (ready1 && nextSlot === 1) warmupTimer.start(); }
    onReady2Changed: { if (ready2 && nextSlot === 2) warmupTimer.start(); }
    
    SequentialAnimation {
        id: crossfadeAnim
        NumberAnimation { target: root; property: 'sceneBlur'; to: 1.0; duration: 400; easing.type: Easing.OutQuad }
        ParallelAnimation {
            NumberAnimation { target: root; property: root.nextSlot === 1 ? 'op1' : 'op2'; to: 1.0; duration: 1400; easing.type: Easing.InOutSine }
            NumberAnimation { target: root; property: root.nextSlot === 1 ? 'scale1' : 'scale2'; to: 1.0; duration: 1400; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: root.activeSlot === 1 ? 'scale1' : 'scale2'; to: 1.10; duration: 1400; easing.type: Easing.InCubic }
            SequentialAnimation {
                NumberAnimation { target: root; property: 'glowIntensity'; to: 0.15; duration: 700; easing.type: Easing.OutQuad }
                NumberAnimation { target: root; property: 'glowIntensity'; to: 0.0; duration: 700; easing.type: Easing.InQuad }
            }
        }
        NumberAnimation { target: root; property: 'sceneBlur'; to: 0.0; duration: 800; easing.type: Easing.InOutQuad }
        ScriptAction { script: {
            if (root.activeSlot === 1) { root.url1 = ''; root.op1 = 0.0; root.scale1 = 1.0; }
            else { root.url2 = ''; root.op2 = 0.0; root.scale2 = 1.0; }
            root.activeSlot = root.nextSlot; root.nextSlot = 0;
            root.internalLock = true; root.isSelecting = false; root.internalLock = false;
        }}
    }
    
    SequentialAnimation {
        id: firstLoadAnim
        ScriptAction { script: { root.sceneBlur = 1.0; root.op1 = 0.0; root.scale1 = 1.10; } }
        ParallelAnimation {
            NumberAnimation { target: root; property: 'op1'; to: 1.0; duration: 1000; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: 'scale1'; to: 1.0; duration: 1000; easing.type: Easing.OutCubic }
        }
        NumberAnimation { target: root; property: 'sceneBlur'; to: 0.0; duration: 800; easing.type: Easing.InOutQuad }
        ScriptAction { script: { root.activeSlot = 1; root.nextSlot = 0; root.internalLock = true; root.isSelecting = false; root.internalLock = false; } }
    }
    
    SequentialAnimation {
        id: clearAnim
        NumberAnimation { target: root; property: 'sceneBlur'; to: 1.0; duration: 500; easing.type: Easing.OutQuad }
        ParallelAnimation {
            NumberAnimation { target: root; property: root.activeSlot === 1 ? 'op1' : 'op2'; to: 0.0; duration: 800; easing.type: Easing.InOutCubic }
            NumberAnimation { target: root; property: root.activeSlot === 1 ? 'scale1' : 'scale2'; to: 1.10; duration: 800; easing.type: Easing.InCubic }
        }
        ScriptAction { script: {
            root.activeSlot = 0; root.nextSlot = 0;
            root.url1 = ''; root.url2 = ''; root.op1 = 0; root.op2 = 0; 
            root.scale1 = 1.0; root.scale2 = 1.0; root.sceneBlur = 0;
            root.internalLock = true; root.isSelecting = false; root.internalLock = false;
        }}
    }
    
    Item {
        id: sceneContainer
        anchors.fill: parent
        visible: false
        
        Rectangle { anchors.fill: parent; color: '#0A0806' }
        Image { z: root.z1; scale: root.scale1; anchors.fill: parent; id: customBg1; source: root.url1; opacity: root.op1; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true; mipmap: true; smooth: true }
        Image { z: root.z2; scale: root.scale2; anchors.fill: parent; id: customBg2; source: root.url2; opacity: root.op2; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true; mipmap: true; smooth: true }
    }
    
    MultiEffect {
        source: sceneContainer
        anchors.fill: parent
        blurEnabled: true; blurMax: 96; blurMultiplier: 1.5
        blur: root.sceneBlur
    }
    
    Rectangle {
        anchors.fill: parent
        color: '#FFFFFF'
        opacity: (root.sceneBlur * 0.06) + root.glowIntensity
        visible: opacity > 0.001
    }
    
    // ⚡ 壓暗層修正：調降厚度，並將純黑改為更透氣的深灰色
    Rectangle {
        anchors.fill: parent; 
        color: '#151515' // 改為深灰色玻璃
        opacity: (root.activeSlot > 0 || root.nextSlot > 0) ? 0.08 : 0.0 // 從 0.20 大幅調降至 0.08
        visible: opacity > 0.001
        Behavior on opacity { NumberAnimation { duration: 800; easing.type: Easing.InOutCubic } }
    }
}
)QML";

GBBackgroundWidget::GBBackgroundWidget(QWidget* parent) : QQuickWidget(parent) {
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setClearColor(Qt::black); 
    
    QString qmlPath = QDir::tempPath() + "/Kaede_GB_OpticalFlow_Recovery.qml"; 
    engine()->clearComponentCache();

    QFile f(qmlPath);
    if (f.open(QIODevice::WriteOnly)) { f.write(GB_QML_SOURCE); f.close(); } 
    setSource(QUrl::fromLocalFile(qmlPath));

    // ===================================================================
    // 📂 啟動時載入 Qt 資源系統內的預設背景，並綁定為「清除後恢復」的目標
    // ===================================================================
    QString defaultImagePath = "qrc:/img/BG.png";
    
    if (QFile::exists(":/img/BG.png")) {
        if (rootObject()) {
            // 寫入一個隱藏的屬性，告訴 QML 這是系統預設背景
            rootObject()->setProperty("defaultBgUrl", defaultImagePath);
            rootObject()->setProperty("bgUrl", defaultImagePath);
        }
    }
}

void GBBackgroundWidget::setBackgroundImage(const QString& path) {
    if (rootObject()) {
        if (path.isEmpty()) rootObject()->setProperty("bgUrl", "");
        else rootObject()->setProperty("bgUrl", QUrl::fromLocalFile(path).toString());
    }
}

void GBBackgroundWidget::clearBackground() {
    setBackgroundImage("");
}

void GBBackgroundWidget::setSelectingState(bool isSelecting) {
    if (rootObject()) {
        rootObject()->setProperty("isSelecting", isSelecting);
    }
}

void GBBackgroundWidget::setNeonColors(const QString&, const QString&, const QString&) {}
void GBBackgroundWidget::setBreathingSpeed(int) {}