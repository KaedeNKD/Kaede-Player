#include "KaedeLibraryPanel.h"
#include "../KaedeDatabase.h" 

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QPixmapCache>
#include <QApplication>
#include <QScrollBar>
#include <QShortcut>
#include <QKeySequence>

class KaedeTrackDelegate : public QStyledItemDelegate {
    KaedeLibraryPanel* m_host; 
    bool m_isGridMode = false;
public:
    explicit KaedeTrackDelegate(KaedeLibraryPanel* host, QObject* parent = nullptr) : QStyledItemDelegate(parent), m_host(host) { 
        QPixmapCache::setCacheLimit(307200); 
    }

    void setGridMode(bool grid) { m_isGridMode = grid; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save(); 
        painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform); 
        
        QRect rect = option.rect;
        QString coverUrl = index.data(KaedeTrackModel::CoverUrlRole).toString(); 
        QString localPath = coverUrl.startsWith("file:///") ? coverUrl.mid(8) : coverUrl;
        
        QPixmap pixmap;
        if (!localPath.isEmpty()) {
            // 👑 恢復原版：綁定實際畫布(device)的 DPI，而不是外部 Widget，避免非同步崩潰
            qreal dpr = painter->device()->devicePixelRatioF();
            int minLogicalSize = m_isGridMode ? 200 : 54;
            int physicalSize = static_cast<int>(minLogicalSize * dpr); 
            
            QString cacheKey = localPath + (m_isGridMode ? "_grid_" : "_list_") + QString::number(physicalSize);
            if (!QPixmapCache::find(cacheKey, &pixmap)) { 
                QImage orig;
                if (orig.load(localPath)) { 
                    int minSide = std::min(orig.width(), orig.height());
                    QRect cropRect((orig.width() - minSide) / 2, (orig.height() - minSide) / 2, minSide, minSide);
                    QImage cropped = orig.copy(cropRect);
                    QImage scaled = cropped.scaled(physicalSize, physicalSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    
                    pixmap = QPixmap::fromImage(scaled);
                    pixmap.setDevicePixelRatio(dpr); 
                    QPixmapCache::insert(cacheKey, pixmap); 
                } 
            }
        }
        
        QColor mainColor = m_host ? m_host->getTextColor() : QColor(255, 255, 255);

        if (m_isGridMode) {
            if (option.state & QStyle::State_MouseOver) { 
                QPainterPath bgPath; bgPath.addRoundedRect(rect, 8, 8); 
                painter->fillPath(bgPath, QColor(255, 255, 255, 12)); 
            }
            
            QRect coverRect(rect.x() + 10, rect.y() + 10, 200, 200);
            painter->setPen(QPen(QColor(255, 255, 255, 15), 1)); 
            painter->setBrush(QColor(0, 0, 0, 80)); 
            painter->drawRoundedRect(coverRect, 8, 8);
            
            if (!pixmap.isNull()) { 
                painter->save();
                QPainterPath clipPath; clipPath.addRoundedRect(coverRect, 8, 8); 
                painter->setClipPath(clipPath); 
                painter->drawPixmap(coverRect, pixmap); 
                painter->restore();
            }
            
            QString album = index.data(KaedeTrackModel::AlbumRole).toString(); 
            QString artist = index.data(KaedeTrackModel::ArtistRole).toString(); 
            
            painter->setPen(mainColor); 
            painter->setFont(QFont("Segoe UI", 11, QFont::Bold)); 
            QRect titleRect(rect.x() + 15, rect.y() + 220, 190, 24);
            painter->setClipRect(titleRect);
            painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, album);
            painter->setClipping(false);
            
            painter->setPen(QColor(255, 255, 255, 120)); 
            painter->setFont(QFont("Segoe UI", 9)); 
            QRect artistRect(rect.x() + 15, rect.y() + 244, 190, 16);
            painter->setClipRect(artistRect);
            painter->drawText(artistRect, Qt::AlignLeft | Qt::AlignVCenter, artist);
            painter->setClipping(false);
            
        } else {
            if (option.state & QStyle::State_MouseOver) { 
                QPainterPath bgPath; bgPath.addRoundedRect(rect, 6, 6); 
                painter->fillPath(bgPath, QColor(255, 255, 255, 20)); 
            }
            painter->fillRect(rect.x() + 15, rect.bottom(), rect.width() - 30, 1, QColor(255, 255, 255, 15));
            
            QRect coverRect(rect.x() + 15, rect.y() + 10, 54, 54);
            painter->setPen(QPen(QColor(255, 255, 255, 20), 1)); 
            painter->setBrush(QColor(0, 0, 0, 128)); 
            painter->drawRoundedRect(coverRect, 6, 6);
            
            if (!pixmap.isNull()) { 
                painter->save();
                QPainterPath clipPath; clipPath.addRoundedRect(coverRect, 6, 6); 
                painter->setClipPath(clipPath); 
                painter->drawPixmap(coverRect, pixmap); 
                painter->restore();
            }
            
            int textX = coverRect.right() + 15; int textW = rect.width() - textX - 15;
            QString title = index.data(KaedeTrackModel::TitleRole).toString(); 
            QString artist = index.data(KaedeTrackModel::ArtistRole).toString(); 
            QString specs = index.data(KaedeTrackModel::SpecsRole).toString();
            
            painter->setPen(mainColor); painter->setFont(QFont("Segoe UI", 11, QFont::Bold)); 
            QRect titleRectL(textX, rect.y() + 12, textW, 20);
            painter->setClipRect(titleRectL);
            painter->drawText(titleRectL, Qt::AlignLeft | Qt::AlignVCenter, title);
            painter->setClipping(false);
            
            painter->setPen(QColor(255, 255, 255, 178)); painter->setFont(QFont("Segoe UI", 9)); 
            QRect artistRectL(textX, rect.y() + 34, textW, 16);
            painter->setClipRect(artistRectL);
            painter->drawText(artistRectL, Qt::AlignLeft | Qt::AlignVCenter, artist);
            painter->setClipping(false);
            
            painter->setPen(QColor(255, 255, 255, 128)); painter->setFont(QFont("Consolas", 8)); 
            QRect specsRectL(textX, rect.y() + 52, textW, 14);
            painter->setClipRect(specsRectL);
            painter->drawText(specsRectL, Qt::AlignLeft | Qt::AlignVCenter, specs);
            painter->setClipping(false);
        }
        painter->restore();
    }
    
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override { 
        Q_UNUSED(index);
        if (m_isGridMode) return QSize(220, 280); 
        return QSize(option.rect.width(), 74); 
    }
};

KaedeLibraryPanel::KaedeLibraryPanel(QWidget *parent) : QFrame(parent) {
    setObjectName("LibraryInnerGlass");
    setStyleSheet("#LibraryInnerGlass { background: rgba(12, 10, 9, 0.45); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 16px; }");
    installEventFilter(this);

    m_drawerBgFadeAnim = new QVariantAnimation(this);
    m_drawerBgFadeAnim->setDuration(450);
    m_drawerBgFadeAnim->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_drawerBgFadeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_drawerBgFade = val.toDouble();
        if (m_detailDrawer) m_detailDrawer->update(); 
    });

    setupUi();
}

void KaedeLibraryPanel::setupUi() {
    QVBoxLayout* libInnerLayout = new QVBoxLayout(this); 
    libInnerLayout->setContentsMargins(20, 20, 20, 20); 
    libInnerLayout->setSpacing(20);
    
    QHBoxLayout* libHeaderLayout = new QHBoxLayout();
    QLabel* libHeader = new QLabel("KAEDE RELATIONAL LIBRARY", this);
    libHeader->setStyleSheet("color: #FFFFFF; font-family: 'Consolas'; font-size: 20px; font-weight: bold; background: transparent; border: none;");
    libHeaderLayout->addWidget(libHeader);
    libHeaderLayout->addStretch();
    
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("SEARCH TRACKS...");
    m_searchBox->setFixedSize(250, 32);
    m_searchBox->setStyleSheet("QLineEdit { background: rgba(0,0,0,0.4); color: #FFF; border: 1px solid rgba(255,255,255,0.2); border-radius: 16px; padding: 0 15px; font-family: 'Segoe UI'; font-weight: bold; } QLineEdit:focus { border: 1px solid #38B2CE; background: rgba(0,0,0,0.6); }");
    libHeaderLayout->addWidget(m_searchBox);

    m_btnSearch = new QPushButton("SEARCH", this);
    m_btnSearch->setFixedSize(70, 32);
    m_btnSearch->setStyleSheet("QPushButton { background: rgba(255,255,255,0.1); color: #FFF; border: 1px solid rgba(255,255,255,0.2); border-radius: 16px; font-family: 'Consolas'; font-weight: bold; font-size: 11px; letter-spacing: 1px; } QPushButton:hover { background: rgba(56,178,206,0.3); border: 1px solid #38B2CE; } QPushButton:pressed { background: rgba(0,0,0,0.6); }");
    libHeaderLayout->addWidget(m_btnSearch);
    
    m_btnViewMode = new QPushButton("▦ GRID", this);
    m_btnViewMode->setFixedSize(70, 32);
    m_btnViewMode->setStyleSheet("QPushButton { background: rgba(255,255,255,0.1); color: #FFF; border: 1px solid rgba(255,255,255,0.2); border-radius: 16px; font-family: 'Consolas'; font-weight: bold; font-size: 11px; letter-spacing: 1px; margin-left: 10px; } QPushButton:hover { background: rgba(255,255,255,0.2); border: 1px solid #FFF; } QPushButton:pressed { background: rgba(0,0,0,0.6); }");
    libHeaderLayout->addWidget(m_btnViewMode);
    
    libInnerLayout->addLayout(libHeaderLayout);

    connect(m_searchBox, &QLineEdit::textChanged, this, [](const QString& text) { KaedeDatabase::instance().searchTracks(text); });
    connect(m_btnSearch, &QPushButton::clicked, this, [this]() { KaedeDatabase::instance().searchTracks(m_searchBox->text()); });

    m_shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_shortcutSearch, &QShortcut::activated, this, [this]() {
        if (m_searchBox) { m_searchBox->setFocus(); m_searchBox->selectAll(); }
    });

    m_libraryView = new QListView(this); 
    m_libraryView->setAttribute(Qt::WA_TranslucentBackground);
    m_libraryView->setStyleSheet("QListView { background: transparent; border: none; outline: none; } QListView::item { border: none; outline: none; }");
    m_libraryView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); 
    m_libraryView->setMouseTracking(true); 
    m_libraryView->setSelectionMode(QAbstractItemView::NoSelection); 
    m_libraryView->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { background: transparent; width: 8px; } QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.2); border-radius: 4px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
    
    // 👑 修復崩潰點 2：找回遺失的保命符，防止切換 IconMode 瞬間引發上千次 sizeHint 導致的 TDR 崩潰！
    m_libraryView->setUniformItemSizes(true);
    
    m_libraryView->setModel(KaedeDatabase::instance().getTrackModel()); 
    m_libraryView->setItemDelegate(new KaedeTrackDelegate(this, m_libraryView));
    libInnerLayout->addWidget(m_libraryView, 1);

    m_detailDrawer = new QFrame(this);
    m_detailDrawer->setObjectName("DetailDrawer");
    m_detailDrawer->setStyleSheet("#DetailDrawer { border-bottom-left-radius: 16px; border-bottom-right-radius: 16px; background: transparent; }");
    m_detailDrawer->setMaximumHeight(0); 
    m_detailDrawer->setMouseTracking(true);
    m_detailDrawer->installEventFilter(this);
    
    libInnerLayout->addWidget(m_detailDrawer, 0); 
    
    QVBoxLayout* drawerLayout = new QVBoxLayout(m_detailDrawer);
    drawerLayout->setContentsMargins(25, 15, 25, 15);
    
    QHBoxLayout* drawerHeader = new QHBoxLayout();
    m_lblDrawerTitle = new QLabel("ALBUM DETAILS", m_detailDrawer);
    m_lblDrawerTitle->setStyleSheet("color: #FFFFFF; font-weight: bold; font-size: 16px; font-family: 'Segoe UI';");
    m_btnDrawerClose = new QPushButton("✕", m_detailDrawer);
    m_btnDrawerClose->setFixedSize(28, 28);
    m_btnDrawerClose->setStyleSheet("QPushButton { color: #A0A0A0; background: rgba(255,255,255,0.1); border: none; border-radius: 14px; font-family: 'Segoe UI'; font-size: 12px; font-weight: bold; } QPushButton:hover { color: #FFF; background: #E81123; } QPushButton:pressed { background: #B00D1B; }");
    
    drawerHeader->addWidget(m_lblDrawerTitle);
    drawerHeader->addStretch();
    drawerHeader->addWidget(m_btnDrawerClose);
    
    m_detailView = new QListView(m_detailDrawer);
    m_detailView->setAttribute(Qt::WA_TranslucentBackground);
    m_detailView->setStyleSheet("QListView { background: transparent; border: none; outline: none; } QListView::item { border: none; outline: none; }");
    m_detailView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); 
    m_detailView->setMouseTracking(true); 
    m_detailView->setSelectionMode(QAbstractItemView::NoSelection);
    m_detailView->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { background: transparent; width: 6px; } QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.15); border-radius: 3px; }");
    
    // 👑 修復崩潰點 2：一併補上子視圖的保命符
    m_detailView->setUniformItemSizes(true);
    
    m_detailView->setModel(KaedeDatabase::instance().getAlbumDetailModel()); 
    auto detailDelegate = new KaedeTrackDelegate(this, m_detailView);
    detailDelegate->setGridMode(false); 
    m_detailView->setItemDelegate(detailDelegate);
    
    drawerLayout->addLayout(drawerHeader);
    drawerLayout->addWidget(m_detailView, 1);
    
    m_drawerAnim = new QPropertyAnimation(m_detailDrawer, "maximumHeight", this);
    m_drawerAnim->setDuration(350);
    m_drawerAnim->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_btnDrawerClose, &QPushButton::clicked, this, [this]() {
        m_drawerAnim->setStartValue(m_detailDrawer->height());
        m_drawerAnim->setEndValue(0);
        m_drawerAnim->start();
    });

    connect(m_libraryView, &QListView::clicked, this, &KaedeLibraryPanel::onLibraryClicked);
    connect(m_btnViewMode, &QPushButton::clicked, this, &KaedeLibraryPanel::toggleViewMode);

    m_viewCurtain = new QWidget(this);
    m_viewCurtain->setAttribute(Qt::WA_TransparentForMouseEvents); 
    m_viewCurtain->setStyleSheet("background-color: transparent; border-radius: 16px;");
    m_viewCurtain->hide();
    
    m_viewFadeAnim = new QVariantAnimation(this);
    m_viewFadeAnim->setDuration(350); 
    m_viewFadeAnim->setStartValue(0.0);
    m_viewFadeAnim->setEndValue(1.0);
    m_viewFadeAnim->setEasingCurve(QEasingCurve::InOutSine);
    
    connect(m_viewFadeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val){
        double v = val.toDouble();
        int alpha = 0;
        if (v <= 0.5) alpha = static_cast<int>((v / 0.5) * 255.0);
        else alpha = static_cast<int>(((1.0 - v) / 0.5) * 255.0);
        
        m_viewCurtain->setStyleSheet(QString("background-color: rgba(12, 10, 9, %1); border-radius: 16px;").arg(alpha));
        
        static bool swapped = false;
        if (v == 0.0) swapped = false;
        if (v >= 0.5 && !swapped) {
            swapped = true;
            m_isGridMode = !m_isGridMode;
            m_btnViewMode->setText(m_isGridMode ? "▤ LIST" : "▦ GRID");
            
            if (m_detailDrawer->maximumHeight() > 0) {
                m_drawerAnim->stop();
                m_detailDrawer->setMaximumHeight(0);
            }
            
            auto delegate = static_cast<KaedeTrackDelegate*>(m_libraryView->itemDelegate());
            if (delegate) delegate->setGridMode(m_isGridMode);
            
            if (m_isGridMode) {
                m_libraryView->setViewMode(QListView::IconMode);
                m_libraryView->setResizeMode(QListView::Adjust);
                m_libraryView->setGridSize(QSize(220, 280));
                m_libraryView->setSpacing(10);
            } else {
                m_libraryView->setViewMode(QListView::ListMode);
                m_libraryView->setResizeMode(QListView::Fixed);
                m_libraryView->setGridSize(QSize());
                m_libraryView->setSpacing(0);
            }
            
            KaedeDatabase::instance().setAlbumMode(m_isGridMode);
            KaedeDatabase::instance().searchTracks(m_searchBox ? m_searchBox->text() : "");
        }
    });
    
    connect(m_viewFadeAnim, &QVariantAnimation::finished, this, [this]() {
        m_viewCurtain->hide();
    });

    connect(m_libraryView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!m_isGridMode) emit sigPlayTrackRequest(KaedeDatabase::instance().getTrackModel(), index.row());
    });
    connect(m_detailView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        emit sigPlayTrackRequest(KaedeDatabase::instance().getAlbumDetailModel(), index.row());
    });
}

// 👑 修復崩潰點 1：摒棄會造成 UI 執行緒死鎖的逐幀高頻 CSS 更新，改為純淨的一次性 Snap，完全沒有效能消耗
void KaedeLibraryPanel::setDimMode(bool dimmed) {
    setStyleSheet(QString("#LibraryInnerGlass { background: rgba(12, 10, 9, %1); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 16px; }").arg(dimmed ? "0.85" : "0.45"));
}

void KaedeLibraryPanel::updateThemeColors(const QColor& text, const QColor& bg) {
    m_textColor = text;
    m_bgColor = bg;
    
    if (m_btnSearch) m_btnSearch->setStyleSheet(QString("QPushButton { background: rgba(255,255,255,0.1); color: %1; border: 1px solid rgba(255,255,255,0.2); border-radius: 16px; font-family: 'Consolas'; font-weight: bold; font-size: 11px; letter-spacing: 1px; } QPushButton:hover { background: rgba(255,255,255,0.2); border: 1px solid #FFF; } QPushButton:pressed { background: rgba(0,0,0,0.6); }").arg(m_textColor.name()));
    if (m_btnViewMode) m_btnViewMode->setStyleSheet(QString("QPushButton { background: rgba(255,255,255,0.1); color: %1; border: 1px solid rgba(255,255,255,0.2); border-radius: 16px; font-family: 'Consolas'; font-weight: bold; font-size: 11px; letter-spacing: 1px; margin-left: 10px; } QPushButton:hover { background: rgba(255,255,255,0.2); border: 1px solid #FFF; } QPushButton:pressed { background: rgba(0,0,0,0.6); }").arg(m_textColor.name()));
    if (m_libraryView) m_libraryView->viewport()->update();
    if (m_detailView) m_detailView->viewport()->update();
}

void KaedeLibraryPanel::updateDrawerBackground(const QString& coverUrl) {
    QString localPath = coverUrl.startsWith("file:///") ? coverUrl.mid(8) : coverUrl;
    QPixmap orig;
    if (!localPath.isEmpty() && orig.load(localPath)) {
        QPixmap abstractBg = orig.scaled(32, 32, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                                 .scaled(800, 800, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        m_drawerBgOld = m_drawerBgNew;
        m_drawerBgNew = abstractBg;
        
        m_drawerBgFadeAnim->stop();
        m_drawerBgFadeAnim->setStartValue(0.0);
        m_drawerBgFadeAnim->setEndValue(1.0);
        m_drawerBgFadeAnim->start();
    }
}

void KaedeLibraryPanel::toggleViewMode() {
    if (m_viewFadeAnim->state() == QAbstractAnimation::Running) return;
    m_viewCurtain->setGeometry(this->rect());
    m_viewCurtain->show();
    m_viewCurtain->raise(); 
    m_viewFadeAnim->start();
}

void KaedeLibraryPanel::onLibraryClicked(const QModelIndex& index) {
    if (m_isGridMode) {
        QString artist = index.data(KaedeTrackModel::ArtistRole).toString();
        QString album = index.data(KaedeTrackModel::AlbumRole).toString();
        QString cover = index.data(KaedeTrackModel::CoverUrlRole).toString();
        
        KaedeDatabase::instance().loadAlbumDetails(artist, album);
        m_lblDrawerTitle->setText(album + "  —  " + artist);
        
        updateDrawerBackground(cover);
        
        if (m_detailDrawer->maximumHeight() < 10) { 
            m_drawerAnim->setStartValue(0);
            m_drawerAnim->setEndValue(m_targetDrawerHeight); 
            m_drawerAnim->start();
        }
    }
}

bool KaedeLibraryPanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_detailDrawer) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (m_isResizingDrawer) {
                int deltaY = qRound(me->globalPosition().y()) - m_dragStartGlobalY;
                int newHeight = m_dragStartHeight - deltaY;
                newHeight = qBound(100, newHeight, this->height() - 200);
                m_targetDrawerHeight = newHeight; 
                m_detailDrawer->setMaximumHeight(newHeight);
                return true;
            } else {
                if (me->pos().y() <= 8) m_detailDrawer->setCursor(Qt::SizeVerCursor);
                else m_detailDrawer->setCursor(Qt::ArrowCursor);
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->pos().y() <= 8 && me->button() == Qt::LeftButton) {
                m_isResizingDrawer = true;
                m_dragStartGlobalY = qRound(me->globalPosition().y());
                m_dragStartHeight = m_detailDrawer->height();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_isResizingDrawer && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                m_isResizingDrawer = false;
                return true;
            }
        } else if (event->type() == QEvent::Paint) {
            QPainter p(m_detailDrawer);
            p.setRenderHint(QPainter::Antialiasing);
            QRect r = m_detailDrawer->rect();
            
            QPainterPath path; path.addRoundedRect(r, 8, 8); p.setClipPath(path);

            if (!m_drawerBgOld.isNull() && m_drawerBgFade < 1.0) {
                p.setOpacity(1.0 - m_drawerBgFade);
                p.drawPixmap(r, m_drawerBgOld);
            }
            if (!m_drawerBgNew.isNull()) {
                p.setOpacity(m_drawerBgFade);
                p.drawPixmap(r, m_drawerBgNew);
            }
            p.setOpacity(1.0);
            p.fillRect(r, QColor(10, 10, 10, 210)); 
            p.setPen(QColor(255, 255, 255, 40));
            p.drawLine(r.topLeft(), r.topRight());
            return false; 
        }
    }

    if (event->type() == QEvent::Resize && m_viewCurtain && m_viewCurtain->isVisible()) {
        m_viewCurtain->setGeometry(this->rect());
    }
    return QFrame::eventFilter(watched, event);
}