#pragma once
#ifndef PLAYBACKCONSOLE_H
#define PLAYBACKCONSOLE_H

#include <QWidget>
#include <QPushButton>
#include <QVariantAnimation>
#include <QImage>
#include <QString>
#include <QMouseEvent> 
#include <QWheelEvent>
#include <QLabel>
#include <QColor>
#include <QEvent> 

class VolumeWheelButton : public QPushButton {
    Q_OBJECT
public:
    explicit VolumeWheelButton(QWidget* parent = nullptr);
signals:
    void volumeScrolled(float newVolume);
protected:
    void wheelEvent(QWheelEvent* event) override;
private:
    float m_volume = 1.0f;
};

class AudioPipelineOverlay : public QWidget {
    Q_OBJECT
public:
    explicit AudioPipelineOverlay(QWidget* parent = nullptr);
    void updateInfo(const QString& spec, bool hasSrc, const QString& api, const QString& device, const QString& latency);
    void showAboveButton(QWidget* btn);
protected:
    void paintEvent(QPaintEvent* event) override;
    void leaveEvent(QEvent* event) override;
private:
    QLabel* m_lblTrack;
    QLabel* m_lblSrc;
    QLabel* m_lblApi;
    QLabel* m_lblDevice;
    QLabel* m_lblLatency;
};

class TrackInfoCanvas : public QWidget {
    Q_OBJECT
public:
    explicit TrackInfoCanvas(QWidget* parent = nullptr);
    void updateTrackData(const QString& title, const QString& artist, const QImage& cover);
    void setFadeProgress(double progress);
    void setTextColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QString m_title = "SYSTEM IDLE";
    QString m_artist = "NO MEDIA LOADED";
    QImage m_cover;
    QString m_nextTitle;
    QString m_nextArtist;
    QImage m_nextCover;
    double m_progress = 1.0; 
    
    QColor m_textColor = QColor(26, 26, 26); 
};

class PlaybackConsole : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackConsole(QWidget* parent = nullptr);
    ~PlaybackConsole() override = default;
    
    void updateTrackInfo(const QString& title, const QString& artist, const QImage& cover);
    void setPlayState(bool isPlaying);
    void resetToIdle();
    
    void updateAdaptiveTheme(const QColor& bgBase, const QColor& fgColor);

signals:
    void sigPlayClicked();
    void sigStopClicked();
    void sigPrevClicked();
    void sigNextClicked();
    void sigConsoleClicked(); 
    void sigVolumeChanged(float volume);
    void sigDspClicked(); 

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit sigConsoleClicked();
        }
        QWidget::mousePressEvent(event);
    }
    
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();

    QWidget* m_contentLayer = nullptr;
    TrackInfoCanvas* m_trackCanvas = nullptr;
    
    QWidget* m_centerArea = nullptr;
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnPlay = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnNext = nullptr;

    QWidget* m_rightArea = nullptr;
    VolumeWheelButton* m_btnVolume = nullptr;
    QPushButton* m_btnPipeline = nullptr;
    QPushButton* m_btnDsp = nullptr; 
    AudioPipelineOverlay* m_pipelineOverlay = nullptr;

    QVariantAnimation* m_crossfadeAnim = nullptr;
    
    QColor m_currentBgBase;
    QColor m_currentFgColor;
};

#endif // PLAYBACKCONSOLE_H