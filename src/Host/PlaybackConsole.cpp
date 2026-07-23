#include "PlaybackConsole.h"
#include "KaedeAudioEngine.h" 
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QLinearGradient>

VolumeWheelButton::VolumeWheelButton(QWidget* parent) : QPushButton(parent) {
    setFixedSize(40, 40);
    setText(QString::fromUtf8("\xF0\x9F\x94\x8A")); 
    setStyleSheet(
        "QPushButton { color: rgba(0,0,0,0.4); font-size: 18px; border: none; background: transparent; } "
        "QPushButton:hover { color: #1A1A1A; background: rgba(0,0,0,0.05); border-radius: 20px; }"
    );
    setToolTip("VOL: 100%");
}

void VolumeWheelButton::wheelEvent(QWheelEvent* event) {
    float delta = event->angleDelta().y() > 0 ? 0.05f : -0.05f;
    m_volume = qBound(0.0f, m_volume + delta, 1.0f);
    
    setToolTip(QString("VOL: %1%").arg(static_cast<int>(m_volume * 100)));
    QToolTip::showText(event->globalPosition().toPoint(), this->toolTip(), this);
    
    emit volumeScrolled(m_volume);
    event->accept(); 
}

AudioPipelineOverlay::AudioPipelineOverlay(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setFixedSize(460, 220); 

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(6); 

    auto makeLbl = [this](int size, const QString& color, bool isBold = false) {
        QLabel* lbl = new QLabel(this);
        lbl->setStyleSheet(QString("color: %1; font-family: 'Consolas'; font-size: %2px; %3 background: transparent;")
                           .arg(color).arg(size).arg(isBold ? "font-weight: bold;" : ""));
        return lbl;
    };

    m_lblTrack = makeLbl(13, "#E6C282", true);
    m_lblSrc = makeLbl(12, "#E81123", true);
    m_lblApi = makeLbl(13, "#4DAAFB", true);
    m_lblDevice = makeLbl(13, "#FFFFFF", true);
    m_lblLatency = makeLbl(12, "#AAAAAA");

    layout->addWidget(m_lblTrack);
    layout->addWidget(m_lblSrc);
    layout->addWidget(m_lblApi);
    layout->addWidget(m_lblDevice);
    layout->addWidget(m_lblLatency);
    layout->addStretch();
}

void AudioPipelineOverlay::updateInfo(const QString& spec, bool hasSrc, const QString& api, const QString& device, const QString& latency) {
    m_lblTrack->setText(QString("SOURCE STREAM      %1").arg(spec));
    
    if (hasSrc) {
        m_lblSrc->setText(QString::fromUtf8("      \xE2\x86\x93\nDSP RESAMPLER      ASYNC CONVERSION ACTIVE"));
        m_lblSrc->setStyleSheet("color: #E81123; font-family: 'Consolas'; font-size: 12px; font-weight: bold; background: transparent;");
    } else {
        m_lblSrc->setText(QString::fromUtf8("      \xE2\x86\x93\nDSP RESAMPLER      BYPASSED (BIT-PERFECT)"));
        m_lblSrc->setStyleSheet("color: #4DAAFB; font-family: 'Consolas'; font-size: 12px; font-weight: bold; background: transparent;");
    }
    m_lblSrc->show(); 

    m_lblApi->setText(QString::fromUtf8("      \xE2\x86\x93\nAUDIO API          %1").arg(api));
    m_lblDevice->setText(QString::fromUtf8("      \xE2\x86\x93\nTARGET DAC         %1").arg(device));
    m_lblLatency->setText(QString("\nHARDWARE BUFFER    LATENCY: %1").arg(latency));
}

void AudioPipelineOverlay::showAboveButton(QWidget* btn) {
    if (!btn || !btn->window()) return;
    
    QPoint btnPos = btn->mapTo(btn->window(), QPoint(0, 0));
    QPoint globalWinPos = btn->window()->mapToGlobal(QPoint(0, 0));
    
    int x = globalWinPos.x() + btnPos.x() + (btn->width() / 2) - (this->width() / 2);
    int y;

    if (btnPos.y() < btn->window()->height() / 2) {
        y = globalWinPos.y() + btnPos.y() + btn->height() + 25; 
    } else {
        y = globalWinPos.y() + btnPos.y() - this->height() - 65; 
    }
    
    this->move(x, y);
    this->show();
}

void AudioPipelineOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(this->rect(), 14, 14);
    painter.fillPath(path, QColor(20, 18, 16, 240)); 
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.drawPath(path);
}

void AudioPipelineOverlay::leaveEvent(QEvent*) {
    this->hide();
}

TrackInfoCanvas::TrackInfoCanvas(QWidget* parent) : QWidget(parent) {}

void TrackInfoCanvas::updateTrackData(const QString& title, const QString& artist, const QImage& cover) {
    m_nextTitle = title;
    m_nextArtist = artist;
    m_nextCover = cover;
}

void TrackInfoCanvas::setFadeProgress(double progress) {
    m_progress = progress;
    if (m_progress >= 1.0) {
        m_title = m_nextTitle;
        m_artist = m_nextArtist;
        m_cover = m_nextCover;
    }
    update();
}

void TrackInfoCanvas::setTextColor(const QColor& color) {
    m_textColor = color;
    update(); 
}

void TrackInfoCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    auto drawBlock = [&](double alpha, const QString& t, const QString& a, const QImage& c) {
        if (alpha <= 0.01) return;
        painter.save();
        painter.setOpacity(alpha);

        QRectF coverRect(0, 12, 56, 56);
        QPainterPath coverPath;
        coverPath.addRoundedRect(coverRect, 8, 8);
        painter.save();
        painter.setClipPath(coverPath);
        if (!c.isNull()) {
            painter.drawImage(coverRect, c);
        } else {
            painter.fillRect(coverRect, QColor(0, 0, 0, 15)); 
        }
        painter.restore();
        painter.setPen(QPen(QColor(0, 0, 0, 20), 1));
        painter.drawRoundedRect(coverRect, 8, 8);

        painter.setPen(m_textColor); 
        painter.setFont(QFont("Segoe UI", 10, QFont::Bold));
        painter.drawText(QRectF(71, 17, width() - 71, 22), Qt::AlignVCenter | Qt::AlignLeft, painter.fontMetrics().elidedText(t, Qt::ElideRight, width() - 75));

        QColor subColor = m_textColor;
        subColor.setAlpha(160);
        painter.setPen(subColor);
        painter.setFont(QFont("Segoe UI", 9, QFont::Normal));
        painter.drawText(QRectF(71, 41, width() - 71, 20), Qt::AlignVCenter | Qt::AlignLeft, painter.fontMetrics().elidedText(a, Qt::ElideRight, width() - 75));

        painter.restore();
    };

    if (m_progress < 1.0) {
        drawBlock(1.0 - m_progress, m_title, m_artist, m_cover);
        drawBlock(m_progress, m_nextTitle, m_nextArtist, m_nextCover);
    } else {
        drawBlock(1.0, m_title, m_artist, m_cover);
    }
}

PlaybackConsole::PlaybackConsole(QWidget* parent) : QWidget(parent) {
    this->setObjectName("PlaybackConsole");
    this->setAttribute(Qt::WA_TranslucentBackground);

    m_currentBgBase = QColor(180, 225, 255);
    m_currentFgColor = QColor(26, 26, 26);

    m_contentLayer = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(m_contentLayer);

    setupUi();

    m_crossfadeAnim = new QVariantAnimation(this);
    m_crossfadeAnim->setDuration(350); 
    m_crossfadeAnim->setStartValue(0.0);
    m_crossfadeAnim->setEndValue(1.0);

    connect(m_crossfadeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        if (m_trackCanvas) m_trackCanvas->setFadeProgress(val.toDouble());
    });
}

void PlaybackConsole::setupUi() {
    QHBoxLayout* mainLayout = new QHBoxLayout(m_contentLayer);
    mainLayout->setContentsMargins(20, 0, 20, 0);
    mainLayout->setSpacing(0);

    m_trackCanvas = new TrackInfoCanvas(m_contentLayer);
    m_trackCanvas->setFixedWidth(280);
    mainLayout->addWidget(m_trackCanvas);

    mainLayout->addStretch();

    m_centerArea = new QWidget(m_contentLayer);
    QHBoxLayout* centerLayout = new QHBoxLayout(m_centerArea);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(15);
    centerLayout->setAlignment(Qt::AlignCenter);

    auto makeCtrlBtn = [this](const QString& text, int fontSize) {
        QPushButton* btn = new QPushButton(text, m_centerArea);
        btn->setFixedSize(50, 50);
        return btn;
    };

    m_btnPrev = makeCtrlBtn(QString::fromUtf8("\xE2\x97\x80\xE2\x97\x80"), 16); 
    m_btnStop = makeCtrlBtn(QString::fromUtf8("\xE2\x96\xA0"), 18);          
    
    m_btnPlay = new QPushButton(QString::fromUtf8("\xE2\x96\xB6"), m_centerArea);          
    m_btnPlay->setFixedSize(60, 60); 
    
    m_btnNext = makeCtrlBtn(QString::fromUtf8("\xE2\x96\xB6\xE2\x96\xB6"), 16); 

    centerLayout->addWidget(m_btnPrev);
    centerLayout->addWidget(m_btnStop);
    centerLayout->addWidget(m_btnPlay);
    centerLayout->addWidget(m_btnNext);
    mainLayout->addWidget(m_centerArea);

    mainLayout->addStretch();

    m_rightArea = new QWidget(m_contentLayer);
    m_rightArea->setFixedWidth(280);
    QHBoxLayout* rightLayout = new QHBoxLayout(m_rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(15);
    rightLayout->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    m_btnDsp = new QPushButton("DSP", m_rightArea);
    m_btnDsp->setFixedSize(40, 40);

    m_btnPipeline = new QPushButton(QString::fromUtf8("\xE2\x8F\x9A"), m_rightArea); 
    m_btnPipeline->setFixedSize(40, 40);
    m_btnPipeline->installEventFilter(this);
    
    m_btnVolume = new VolumeWheelButton(m_rightArea);
    
    m_pipelineOverlay = new AudioPipelineOverlay(this->window());
    m_pipelineOverlay->hide();

    rightLayout->addWidget(m_btnDsp);
    rightLayout->addWidget(m_btnPipeline);
    rightLayout->addWidget(m_btnVolume);
    mainLayout->addWidget(m_rightArea);

    connect(m_btnPlay, &QPushButton::clicked, this, &PlaybackConsole::sigPlayClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &PlaybackConsole::sigStopClicked);
    connect(m_btnPrev, &QPushButton::clicked, this, &PlaybackConsole::sigPrevClicked);
    connect(m_btnNext, &QPushButton::clicked, this, &PlaybackConsole::sigNextClicked);
    connect(m_btnVolume, &VolumeWheelButton::volumeScrolled, this, &PlaybackConsole::sigVolumeChanged);
    connect(m_btnDsp, &QPushButton::clicked, this, &PlaybackConsole::sigDspClicked);

    updateAdaptiveTheme(m_currentBgBase, m_currentFgColor);
}

bool PlaybackConsole::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_btnPipeline) {
        if (event->type() == QEvent::Enter) {
            auto info = KaedeAudioEngine::instance().getPipelineInfo();
            m_pipelineOverlay->updateInfo(info.formatSpec, info.hasSrc, info.apiMode, info.deviceName, info.latency);
            m_pipelineOverlay->showAboveButton(m_btnPipeline);
            return true;
        } else if (event->type() == QEvent::Leave) {
            m_pipelineOverlay->hide();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void PlaybackConsole::updateAdaptiveTheme(const QColor& bgBase, const QColor& fgColor) {
    m_currentBgBase = bgBase;
    m_currentFgColor = fgColor;

    if (m_trackCanvas) {
        m_trackCanvas->setTextColor(fgColor);
    }

    bool isDarkBg = bgBase.lightness() < 128;
    QString hoverBg = isDarkBg ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.05)";
    QString pressedBg = isDarkBg ? "rgba(255, 255, 255, 0.15)" : "rgba(0, 0, 0, 0.1)";
    QString btnBaseColor = fgColor.name();
    
    QString idleBtnStyle = QString(
        "QPushButton { font-family: 'Segoe UI Symbol', 'Arial'; background: transparent; border: none; color: %1; border-radius: %2px; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }"
    );

    if (m_btnPrev) { m_btnPrev->setStyleSheet(idleBtnStyle.arg(btnBaseColor).arg(25).arg(hoverBg).arg(pressedBg) + "font-size: 16px;"); }
    if (m_btnStop) { m_btnStop->setStyleSheet(idleBtnStyle.arg(btnBaseColor).arg(25).arg(hoverBg).arg(pressedBg) + "font-size: 18px;"); }
    if (m_btnNext) { m_btnNext->setStyleSheet(idleBtnStyle.arg(btnBaseColor).arg(25).arg(hoverBg).arg(pressedBg) + "font-size: 16px;"); }

    // ⚡ 終極視覺置中修正：
    // Play 鍵 (▶) 需要 padding-left: 3px 往右推。
    // Pause 鍵 (⏸) 因基線偏低，需要 padding-bottom: 4px 往上托！
    bool isPlayingState = m_btnPlay->text() == QString::fromUtf8("\xE2\x8F\xB8"); 
    QString padLeft = isPlayingState ? "0px" : "3px";
    QString padBottom = isPlayingState ? "4px" : "0px"; 

    if (m_btnPlay) {
        if (isPlayingState) {
            QString activeBg = fgColor.name(); 
            QString activeFg = bgBase.name();  
            QString activeHover = fgColor.lighter(115).name(); 
            
            m_btnPlay->setStyleSheet(QString(
                "QPushButton { font-family: 'Segoe UI Symbol', 'Arial'; background: %1; border: none; color: %2; font-size: 24px; border-radius: 30px; padding-left: %3; padding-bottom: %4; }"
                "QPushButton:hover { background: %5; }"
                "QPushButton:pressed { background: %1; }"
            ).arg(activeBg).arg(activeFg).arg(padLeft).arg(padBottom).arg(activeHover));
        } else {
            m_btnPlay->setStyleSheet(idleBtnStyle.arg(btnBaseColor).arg(30).arg(hoverBg).arg(pressedBg) + QString("font-size: 24px; padding-left: %1; padding-bottom: %2;").arg(padLeft).arg(padBottom));
        }
    }

    QString rightBtnStyle = QString(
        "QPushButton { font-family: 'Consolas'; color: %1; border: none; background: transparent; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 20px; }"
    );
    QColor idleColor = fgColor; idleColor.setAlpha(160);

    if (m_btnDsp) { m_btnDsp->setStyleSheet(rightBtnStyle.arg(idleColor.name(QColor::HexArgb)).arg(btnBaseColor).arg(hoverBg) + "font-size: 14px; font-weight: bold;"); }
    if (m_btnPipeline) { m_btnPipeline->setStyleSheet(rightBtnStyle.arg(idleColor.name(QColor::HexArgb)).arg(btnBaseColor).arg(hoverBg) + "font-family: 'Segoe UI Symbol'; font-size: 20px;"); }
    if (m_btnVolume) { 
        m_btnVolume->setStyleSheet(rightBtnStyle.arg(idleColor.name(QColor::HexArgb)).arg(btnBaseColor).arg(hoverBg) + "font-family: 'Segoe UI Symbol'; font-size: 18px;"); 
    }

    update();
}

void PlaybackConsole::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect = this->rect();
    QPainterPath path;
    path.addRoundedRect(rect, 20, 20);

    int h, s, l, a;
    m_currentBgBase.getHsl(&h, &s, &l, &a);

    QColor colorStart, colorEnd;
    
    if (l < 50) {
        colorStart = QColor::fromHsl(h, s, qMin(255, l + 8));
        colorEnd = QColor::fromHsl(h, s, qMax(0, l - 5));
        colorStart.setAlpha(220); 
        colorEnd.setAlpha(240);
    } else {
        colorStart = QColor::fromHsl(h, qMax(0, s - 20), qMin(255, l + 10));
        colorEnd = QColor::fromHsl(h, s, qMax(0, l - 5));
        colorStart.setAlpha(210); 
        colorEnd.setAlpha(230);
    }

    QLinearGradient grad(0, 0, rect.width(), 0);
    grad.setColorAt(0.0, colorStart);
    grad.setColorAt(1.0, colorEnd);
    painter.fillPath(path, grad);

    QColor topBorder = (l < 50) ? QColor(255, 255, 255, 25) : QColor(255, 255, 255, 200);
    QColor bottomBorder = (l < 50) ? QColor(0, 0, 0, 100) : QColor(0, 0, 0, 20);
    
    painter.setPen(QPen(bottomBorder, 1));
    painter.drawPath(path);
    
    painter.setPen(QPen(topBorder, 1));
    painter.drawArc(QRectF(0, 0, 40, 40), 90 * 16, 90 * 16); 
    painter.drawArc(QRectF(rect.width() - 40, 0, 40, 40), 0, 90 * 16); 
    painter.drawLine(20, 0, rect.width() - 20, 0); 
}

void PlaybackConsole::updateTrackInfo(const QString& title, const QString& artist, const QImage& cover) {
    if (m_crossfadeAnim->state() == QAbstractAnimation::Running) {
        m_crossfadeAnim->stop();
        m_trackCanvas->setFadeProgress(1.0); 
    }
    m_trackCanvas->updateTrackData(title, artist, cover);
    m_crossfadeAnim->start();
}

void PlaybackConsole::setPlayState(bool isPlaying) {
    if (isPlaying) {
        m_btnPlay->setText(QString::fromUtf8("\xE2\x8F\xB8")); 
    } else {
        m_btnPlay->setText(QString::fromUtf8("\xE2\x96\xB6")); 
    }
    
    updateAdaptiveTheme(m_currentBgBase, m_currentFgColor);
}

void PlaybackConsole::resetToIdle() {
    updateTrackInfo("SYSTEM IDLE", "NO MEDIA LOADED", QImage());
    setPlayState(false);
}