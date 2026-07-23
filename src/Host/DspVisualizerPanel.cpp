#include "DspVisualizerPanel.h"
#include "VisualizerNodes.h" 
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QDir>
#include <QFile>

const char* DSP_PANEL_QML = R"QML(
import QtQuick
import Kaede.DSP 1.0

Item {
    id: root
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 720

    Item {
        id: mainContent
        anchors.fill: parent
        anchors.margins: 40

        Column {
            width: parent.width * 0.48
            height: parent.height
            spacing: 15

            Item {
                id: oscWrapper
                width: parent.width; height: (parent.height - 30) / 3
                VisGrid { anchors.fill: parent; title: "DUAL-CHANNEL OSCILLOSCOPE"; isOsc: true }
                VisWaveform { id: osc; objectName: "osc"; anchors.fill: parent; anchors.topMargin: 25 }
            }

            Item {
                id: psdWrapper
                width: parent.width; height: (parent.height - 30) / 3
                VisGrid { anchors.fill: parent; title: "PSD (POWER SPECTRAL DENSITY)" }
                VisFft { objectName: "psd"; anchors.fill: parent; anchors.topMargin: 25; isPsd: true }
            }

            Item {
                id: fftWrapper
                width: parent.width; height: (parent.height - 30) / 3
                VisGrid { anchors.fill: parent; title: "FFT (FAST FOURIER TRANSFORM) - RAW" }
                VisFft { objectName: "fft"; anchors.fill: parent; anchors.topMargin: 25; isPsd: false }
            }
        }

        Item {
            id: rightPanel
            x: parent.width * 0.52
            width: parent.width * 0.48
            height: parent.height

            VisGrid { anchors.fill: parent; title: "FERROFLUID GONIOMETER & LEVEL METER" }

            Row {
                anchors.fill: parent
                anchors.topMargin: 45
                anchors.bottomMargin: 20
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 20

                VisGonio {
                    objectName: "gonio"
                    width: parent.width - 80 
                    height: parent.height
                }

                Item {
                    width: 60
                    height: parent.height

                    Row {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 8
                        Text { text: "L\n" + osc.dbL.toFixed(1); color: "#ccc"; font.pixelSize: 11; font.family: "Consolas"; horizontalAlignment: Text.AlignHCenter }
                        Text { text: "R\n" + osc.dbR.toFixed(1); color: "#ccc"; font.pixelSize: 11; font.family: "Consolas"; horizontalAlignment: Text.AlignHCenter }
                    }

                    Item {
                        anchors.top: parent.top; anchors.topMargin: 40
                        anchors.bottom: parent.bottom
                        width: 32
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            x: 0; width: 12; height: parent.height; color: Qt.rgba(0,0,0,0.6); radius: 2; clip: true
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width; height: parent.height * osc.normL 
                                gradient: Gradient {
                                    GradientStop { position: 1.0; color: "#008cff" }
                                    GradientStop { position: 0.2; color: "#00ffb4" }
                                    GradientStop { position: 0.0; color: "#ff3232" } 
                                }
                            }
                            Rectangle { y: parent.height - (parent.height * osc.normPeakL) - 2; width: parent.width; height: 2; color: "white" }
                        }

                        Rectangle {
                            x: 20; width: 12; height: parent.height; color: Qt.rgba(0,0,0,0.6); radius: 2; clip: true
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width; height: parent.height * osc.normR 
                                gradient: Gradient {
                                    GradientStop { position: 1.0; color: "#008cff" }
                                    GradientStop { position: 0.2; color: "#00ffb4" }
                                    GradientStop { position: 0.0; color: "#ff3232" }
                                }
                            }
                            Rectangle { y: parent.height - (parent.height * osc.normPeakR) - 2; width: parent.width; height: 2; color: "white" }
                        }
                    }
                }
            }
        }
    }
}
)QML";

DspVisualizerPanel::DspVisualizerPanel(QWidget* parent) : QQuickWidget(parent) {
    this->setResizeMode(QQuickWidget::SizeRootObjectToView);
    this->setAttribute(Qt::WA_TranslucentBackground);
    
    // ⚡ 降維打擊復原：把置頂特權還給它，消滅黑色背景板
    this->setAttribute(Qt::WA_AlwaysStackOnTop, true); 
    
    this->setClearColor(Qt::transparent);
    initRhiEngine();
}

void DspVisualizerPanel::initRhiEngine() {
    qmlRegisterType<VisWaveformItem>("Kaede.DSP", 1, 0, "VisWaveform");
    qmlRegisterType<VisFftItem>("Kaede.DSP", 1, 0, "VisFft");
    qmlRegisterType<VisGridItem>("Kaede.DSP", 1, 0, "VisGrid");
    qmlRegisterType<VisGonioItem>("Kaede.DSP", 1, 0, "VisGonio"); 
    qmlRegisterType<VisMeterItem>("Kaede.DSP", 1, 0, "VisMeter"); 

    QString qmlPath = QDir::tempPath() + "/Kaede_DspPanel.qml";
    QFile f(qmlPath);
    if (f.open(QIODevice::WriteOnly)) { 
        f.write(DSP_PANEL_QML); 
        f.close(); 
    }
    this->setSource(QUrl::fromLocalFile(qmlPath));
}

void DspVisualizerPanel::updateAudioData(const std::vector<float>& pcm, const std::vector<float>& fft) {
    if (!rootObject()) return;

    if (!m_osc) m_osc = rootObject()->findChild<VisWaveformItem*>(QString("osc"));
    if (!m_psd) m_psd = rootObject()->findChild<VisFftItem*>(QString("psd"));
    if (!m_fft) m_fft = rootObject()->findChild<VisFftItem*>(QString("fft"));
    if (!m_gonio) m_gonio = rootObject()->findChild<VisGonioItem*>(QString("gonio")); 

    if (m_osc) m_osc->pushData(pcm);
    if (m_psd) m_psd->pushData(fft);
    if (m_fft) m_fft->pushData(fft);
    if (m_gonio) m_gonio->pushData(pcm); 
}