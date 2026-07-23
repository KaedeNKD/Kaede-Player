#include "PlayerExpandedPanel.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QPainter>
#include <QStringList>
#include <algorithm>

QImage CoverImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    Q_UNUSED(id);
    QImage img = m_image;
    if (img.isNull()) {
        img = QImage(1, 1, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
    }
    if (size) *size = img.size();
    if (requestedSize.width() > 0 && requestedSize.height() > 0) {
        return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}

QImage LyricsTextureProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    Q_UNUSED(requestedSize);
    QImage img;
    if (id.contains("base")) img = m_baseTexture;
    else if (id.contains("high")) img = m_highTexture;
    
    if (img.isNull()) {
        img = QImage(1, 1, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
    }
    if (size) *size = img.size();
    return img;
}

const char* EXPANDED_PANEL_QML = R"QML(
import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item {
    id: root
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 720

    property bool isOpen: false
    property real startX: 0
    property real startY: 0
    property real startW: 0
    property real startH: 0

    property real finalW: width * 0.88
    property real finalH: height * 0.95 
    property real finalX: (width - finalW) / 2
    property real finalY: (height - finalH) / 2

    property bool hasActiveTrack: false
    property bool isLyricsOpen: false
    
    property bool isTimecoded: true
    property string plainLyricsText: ""
    
    property color panelBgColor: Qt.rgba(0.965, 0.976, 0.992, 0.95)
    property color panelBorderColor: Qt.rgba(1.0, 1.0, 1.0, 0.9)
    property color lyricsTextColor: Qt.rgba(0.2, 0.2, 0.22, 0.85)
    
    property string coverSource: ""
    property string baseLyricsSource: ""
    property string highLyricsSource: ""
    property int currentLyricIndex: 0

    MouseArea {
        anchors.fill: parent
        enabled: root.isOpen
        onClicked: { root.isOpen = false; root.isLyricsOpen = false; }
    }

    Rectangle {
        id: micaPanel
        radius: 24
        color: root.panelBgColor
        border.color: root.panelBorderColor
        border.width: 1

        MouseArea { anchors.fill: parent; hoverEnabled: true }

        Item {
            id: contentArea
            anchors.fill: parent

            Item {
                id: coverContainer
                anchors.verticalCenter: parent.verticalCenter
                width: parent.height * 0.65
                height: parent.height * 0.65
                
                property real closedX: (contentArea.width - width) / 2
                property real openX: (contentArea.width / 4) - (width / 2) 

                x: root.isLyricsOpen ? openX : closedX
                
                Behavior on x { NumberAnimation { duration: 280; easing.type: Easing.OutQuart } }

                Rectangle { id: coverMask; anchors.fill: parent; radius: 24; visible: false; layer.enabled: true }

                Item {
                    id: coverImgSource
                    anchors.fill: parent
                    visible: false 

                    property int activeSlot: 0

                    Connections {
                        target: root
                        function onCoverSourceChanged() {
                            if (root.coverSource === "") {
                                cover0.source = ""; cover1.source = "";
                                cover0.opacity = 0.0; cover1.opacity = 0.0;
                                coverImgSource.activeSlot = 0;
                            } else {
                                if (coverImgSource.activeSlot === 0) {
                                    cover1.source = root.coverSource;
                                } else {
                                    cover0.source = root.coverSource;
                                }
                            }
                        }
                    }

                    Image {
                        id: cover0
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        sourceSize: Qt.size(1500, 1500)
                        mipmap: true; cache: false; asynchronous: true
                        opacity: 1.0

                        onStatusChanged: {
                            if (status === Image.Ready && coverImgSource.activeSlot === 1) {
                                coverImgSource.activeSlot = 0;
                                anim0In.restart();
                                anim1Out.restart();
                            }
                        }
                    }

                    Image {
                        id: cover1
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        sourceSize: Qt.size(1500, 1500)
                        mipmap: true; cache: false; asynchronous: true
                        opacity: 0.0 

                        onStatusChanged: {
                            if (status === Image.Ready && coverImgSource.activeSlot === 0) {
                                coverImgSource.activeSlot = 1;
                                anim1In.restart();
                                anim0Out.restart();
                            }
                        }
                    }

                    NumberAnimation { id: anim0In; target: cover0; property: "opacity"; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                    NumberAnimation { id: anim0Out; target: cover0; property: "opacity"; to: 0.0; duration: 400; easing.type: Easing.InOutQuad }
                    NumberAnimation { id: anim1In; target: cover1; property: "opacity"; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                    NumberAnimation { id: anim1Out; target: cover1; property: "opacity"; to: 0.0; duration: 400; easing.type: Easing.InOutQuad }
                }

                MultiEffect { anchors.fill: parent; source: coverImgSource; maskEnabled: true; maskSource: coverMask }

                Rectangle { anchors.fill: parent; radius: 24; color: "transparent"; border.color: Qt.rgba(0, 0, 0, 0.15); border.width: 1 }

                Text {
                    anchors.centerIn: parent
                    text: root.hasActiveTrack ? (root.coverSource === "" ? "NO COVER" : "") : "無活動中的音訊"
                    color: Qt.rgba(0, 0, 0, 0.3)
                    font.family: "Segoe UI"; font.pointSize: 20; font.bold: true
                    visible: text !== ""
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: root.hasActiveTrack ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: { if (root.hasActiveTrack) root.isLyricsOpen = !root.isLyricsOpen; }
                    onDoubleClicked: { if (root.hasActiveTrack) root.isLyricsOpen = !root.isLyricsOpen; }
                }
            }

            Item {
                id: lyricsPane
                anchors.right: parent.right
                anchors.rightMargin: 40
                width: (contentArea.width / 2) - 40
                height: parent.height - 120
                
                anchors.verticalCenter: parent.verticalCenter
                
                property real yAnimOffset: root.isLyricsOpen ? 0 : 30 
                Behavior on yAnimOffset { NumberAnimation { duration: 280; easing.type: Easing.OutQuart } }
                anchors.verticalCenterOffset: yAnimOffset
                
                clip: true 
                opacity: (root.isLyricsOpen && root.isTimecoded) ? 1.0 : 0.0
                
                Behavior on opacity { NumberAnimation { duration: 200 } }

                property real lineSpacing: 60
                property real highlightHeight: 60
                property real centerY: height / 2
                
                property real targetY: centerY - (root.currentLyricIndex * lineSpacing + (lineSpacing / 2))
                property real animatedY: targetY
                Behavior on animatedY { NumberAnimation { duration: 280; easing.type: Easing.OutQuart } }

                Item {
                    y: 0; width: parent.width; height: lyricsPane.centerY - (lyricsPane.highlightHeight / 2)
                    clip: true
                    Image {
                        width: parent.width
                        source: root.hasActiveTrack && root.baseLyricsSource !== "" ? root.baseLyricsSource : ""
                        y: lyricsPane.animatedY - parent.y
                        fillMode: Image.Pad; horizontalAlignment: Image.AlignLeft; verticalAlignment: Image.AlignTop; asynchronous: true
                    }
                }

                Item {
                    y: lyricsPane.centerY - (lyricsPane.highlightHeight / 2)
                    width: parent.width; height: lyricsPane.highlightHeight
                    clip: true
                    Image {
                        width: parent.width
                        source: root.hasActiveTrack && root.highLyricsSource !== "" ? root.highLyricsSource : ""
                        y: lyricsPane.animatedY - parent.y
                        fillMode: Image.Pad; horizontalAlignment: Image.AlignLeft; verticalAlignment: Image.AlignTop; asynchronous: true
                    }
                }

                Item {
                    y: lyricsPane.centerY + (lyricsPane.highlightHeight / 2)
                    width: parent.width; height: lyricsPane.height - y
                    clip: true
                    Image {
                        width: parent.width
                        source: root.hasActiveTrack && root.baseLyricsSource !== "" ? root.baseLyricsSource : ""
                        y: lyricsPane.animatedY - parent.y
                        fillMode: Image.Pad; horizontalAlignment: Image.AlignLeft; verticalAlignment: Image.AlignTop; asynchronous: true
                    }
                }
            }

            Flickable {
                id: plainLyricsPane
                anchors.right: parent.right
                anchors.rightMargin: 40
                width: (contentArea.width / 2) - 40
                height: parent.height - 120
                
                anchors.verticalCenter: parent.verticalCenter
                
                property real yAnimOffset: root.isLyricsOpen ? 0 : 30 
                Behavior on yAnimOffset { NumberAnimation { duration: 280; easing.type: Easing.OutQuart } }
                anchors.verticalCenterOffset: yAnimOffset
                
                clip: true 
                opacity: (root.isLyricsOpen && !root.isTimecoded) ? 1.0 : 0.0
                
                contentWidth: width
                contentHeight: plainTextItem.height + 60
                interactive: true
                boundsBehavior: Flickable.StopAtBounds

                Behavior on opacity { NumberAnimation { duration: 200 } }

                Text {
                    id: plainTextItem
                    y: 20
                    width: parent.width
                    text: root.plainLyricsText
                    color: root.lyricsTextColor 
                    font.family: "Segoe UI"
                    font.pointSize: 15
                    font.weight: Font.Medium
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                    lineHeight: 1.8 
                }
            }
        }

        states: [
            State {
                name: "closed"
                when: !root.isOpen
                PropertyChanges { target: micaPanel; x: root.startX; y: root.startY; width: root.startW; height: root.startH; opacity: 0.0 }
            },
            State {
                name: "open"
                when: root.isOpen
                PropertyChanges { target: micaPanel; x: root.finalX; y: root.finalY; width: root.finalW; height: root.finalH; opacity: 1.0 }
            }
        ]

        transitions: [
            Transition {
                from: "closed"; to: "open"
                ParallelAnimation {
                    NumberAnimation { properties: "x,y,width,height"; duration: 280; easing.type: Easing.OutQuart }
                    NumberAnimation { property: "opacity"; duration: 220; easing.type: Easing.OutCubic }
                }
            },
            Transition {
                from: "open"; to: "closed"
                ParallelAnimation {
                    NumberAnimation { properties: "x,y,width,height"; duration: 240; easing.type: Easing.InQuart }
                    NumberAnimation { property: "opacity"; duration: 180; easing.type: Easing.OutCubic }
                }
            }
        ]
    }

    onIsOpenChanged: { if (!isOpen) hideTimer.start() }
    Timer { id: hideTimer; interval: 250; onTriggered: root.parentIsClosed() }
    signal parentIsClosed()
}
)QML";

PlayerExpandedPanel::PlayerExpandedPanel(QWidget* parent) : QQuickWidget(parent) {
    this->setResizeMode(QQuickWidget::SizeRootObjectToView);
    this->setAttribute(Qt::WA_TranslucentBackground);
    
    // ⚡ 核心提權：特權歸位，完美消滅底部黑影畫布
    this->setAttribute(Qt::WA_AlwaysStackOnTop, true); 
    
    this->setClearColor(Qt::transparent);
    this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    initRhiEngine();
}

void PlayerExpandedPanel::initRhiEngine() {
    m_coverProvider = new CoverImageProvider();
    m_lyricsProvider = new LyricsTextureProvider(); 
    
    if (this->engine()) {
        this->engine()->addImageProvider("trackcover", m_coverProvider);
        this->engine()->addImageProvider("lyrics", m_lyricsProvider);
    }

    QString qmlPath = QDir::tempPath() + "/Kaede_ExpandedPanel.qml";
    QFile f(qmlPath);
    if (f.open(QIODevice::WriteOnly)) { 
        f.write(EXPANDED_PANEL_QML); 
        f.close(); 
    }
    this->setSource(QUrl::fromLocalFile(qmlPath));

    if (QQuickItem* rootObj = this->rootObject()) {
        connect(rootObj, SIGNAL(parentIsClosed()), this, SLOT(hide())); 
    }
}

void PlayerExpandedPanel::updateTrackData(const QImage& cover, const std::vector<LyricLine>& lyrics) {
    QQuickItem* rootObj = this->rootObject();
    if (!rootObj) return;

    rootObj->setProperty("hasActiveTrack", true);

    m_coverProvider->m_image = cover;
    rootObj->setProperty("coverSource", "image://trackcover/cover_" + QString::number(QDateTime::currentMSecsSinceEpoch()));

    m_lyricTimes.clear();
    m_currentLyricIndex = 0;

    bool isTimecoded = false;
    QString plainTextOut = "無內嵌歌詞";

    if (!lyrics.empty()) {
        for (const auto& line : lyrics) {
            if (line.time > 0.5) {
                isTimecoded = true;
                break;
            }
        }
        
        if (!isTimecoded) {
            QStringList plainLines;
            for (const auto& line : lyrics) {
                if (!line.text.trimmed().isEmpty()) {
                    plainLines << line.text;
                }
            }
            plainTextOut = plainLines.join("\n");
            if (plainTextOut.trimmed().isEmpty()) plainTextOut = "系統: 無效的歌詞資料";
        }
    }

    rootObj->setProperty("isTimecoded", isTimecoded);

    if (isTimecoded) {
        int lineSpacing = 60;
        int texWidth = 800;
        int texHeight = qMax(1, (int)lyrics.size()) * lineSpacing;
        
        QImage baseTex(texWidth, texHeight, QImage::Format_ARGB32_Premultiplied);
        QImage highTex(texWidth, texHeight, QImage::Format_ARGB32_Premultiplied);
        baseTex.fill(Qt::transparent);
        highTex.fill(Qt::transparent);

        QPainter pb(&baseTex);
        QPainter ph(&highTex);
        pb.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        ph.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

        pb.setPen(QColor(0, 0, 0, 100)); 
        pb.setFont(QFont("Segoe UI", 16, QFont::Normal));
        
        ph.setPen(QColor(26, 26, 26, 255)); 
        ph.setFont(QFont("Segoe UI", 24, QFont::Bold)); 

        for (size_t i = 0; i < lyrics.size(); ++i) {
            QString txt = lyrics[i].text;
            if (txt == "SYSTEM: NO LYRICS DATA AVAILABLE") txt = "無內嵌歌詞";
            
            m_lyricTimes.push_back(lyrics[i].time);
            
            QRect rect(10, i * lineSpacing, texWidth - 20, lineSpacing);
            pb.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, txt);
            ph.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, txt);
        }

        m_lyricsProvider->m_baseTexture = baseTex;
        m_lyricsProvider->m_highTexture = highTex;

        QString stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
        rootObj->setProperty("baseLyricsSource", "image://lyrics/base_" + stamp);
        rootObj->setProperty("highLyricsSource", "image://lyrics/high_" + stamp);
    } else {
        rootObj->setProperty("plainLyricsText", plainTextOut);
        
        m_lyricsProvider->m_baseTexture = QImage();
        m_lyricsProvider->m_highTexture = QImage();
        rootObj->setProperty("baseLyricsSource", "");
        rootObj->setProperty("highLyricsSource", "");
    }
    
    rootObj->setProperty("currentLyricIndex", 0);
}

void PlayerExpandedPanel::updateLyricPosition(double currentSec) {
    if (m_lyricTimes.empty()) return;
    
    if (QQuickItem* rootObj = this->rootObject()) {
        if (!rootObj->property("isTimecoded").toBool()) return;
    }
    
    auto it = std::upper_bound(m_lyricTimes.begin(), m_lyricTimes.end(), currentSec);
    int newIndex = std::distance(m_lyricTimes.begin(), it) - 1;
    if (newIndex < 0) newIndex = 0;

    if (newIndex != m_currentLyricIndex) {
        m_currentLyricIndex = newIndex;
        if (QQuickItem* rootObj = this->rootObject()) {
            rootObj->setProperty("currentLyricIndex", m_currentLyricIndex);
        }
    }
}

void PlayerExpandedPanel::clearTrackData() {
    QQuickItem* rootObj = this->rootObject();
    if (!rootObj) return;
    rootObj->setProperty("hasActiveTrack", false);
    rootObj->setProperty("coverSource", "");
    rootObj->setProperty("baseLyricsSource", "");
    rootObj->setProperty("highLyricsSource", "");
    rootObj->setProperty("plainLyricsText", "");
    rootObj->setProperty("isTimecoded", true);
    rootObj->setProperty("isLyricsOpen", false); 
}

void PlayerExpandedPanel::animateToggle(const QRect& triggerRect) {
    QQuickItem* rootObj = this->rootObject();
    if (!rootObj) return;

    m_isOpen = !m_isOpen;

    if (m_isOpen) {
        this->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        this->show();
    } else {
        this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    rootObj->setProperty("startX", triggerRect.x());
    rootObj->setProperty("startY", triggerRect.y());
    rootObj->setProperty("startW", triggerRect.width());
    rootObj->setProperty("startH", triggerRect.height());
    rootObj->setProperty("isOpen", m_isOpen);
}