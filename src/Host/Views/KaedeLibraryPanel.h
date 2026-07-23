#pragma once
#ifndef KAEDELIBRARYPANEL_H
#define KAEDELIBRARYPANEL_H

#include <QFrame>
#include <QListView>
#include <QLineEdit>
#include <QPushButton>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <QLabel>
#include <QShortcut>
#include <QAbstractItemModel>
#include <QColor>
#include <QPixmap>

class KaedeLibraryPanel : public QFrame {
    Q_OBJECT
public:
    explicit KaedeLibraryPanel(QWidget *parent = nullptr);
    ~KaedeLibraryPanel() override = default;

    // 接收來自 MainWindow 的顏色波動
    void updateThemeColors(const QColor& text, const QColor& bg);
    
    QColor getTextColor() const { return m_textColor; }
    QColor getBgColor() const { return m_bgColor; }

    // 👑 安全的環境光控制介面：一次性切換，拒絕高頻 CSS 解析崩潰
    void setDimMode(bool dimmed);

signals:
    void sigPlayTrackRequest(QAbstractItemModel* model, int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void toggleViewMode();
    void onLibraryClicked(const QModelIndex& index);

private:
    void setupUi();
    void updateDrawerBackground(const QString& coverUrl);

    QColor m_textColor = Qt::white;
    QColor m_bgColor = Qt::black;

    QListView* m_libraryView = nullptr;
    QWidget* m_viewCurtain = nullptr;
    QVariantAnimation* m_viewFadeAnim = nullptr;

    QFrame* m_detailDrawer = nullptr;
    QListView* m_detailView = nullptr;
    QPropertyAnimation* m_drawerAnim = nullptr; 
    QLabel* m_lblDrawerTitle = nullptr;
    QPushButton* m_btnDrawerClose = nullptr;

    QLineEdit* m_searchBox = nullptr;
    QPushButton* m_btnSearch = nullptr;
    QShortcut* m_shortcutSearch = nullptr;
    QPushButton* m_btnViewMode = nullptr;

    bool m_isGridMode = false;
    
    // 抽屜拖曳控制
    bool m_isResizingDrawer = false;
    int m_dragStartGlobalY = 0;
    int m_dragStartHeight = 0;
    int m_targetDrawerHeight = 340; 
    
    // 氛圍漸變
    QPixmap m_drawerBgOld;
    QPixmap m_drawerBgNew;
    QVariantAnimation* m_drawerBgFadeAnim = nullptr;
    double m_drawerBgFade = 1.0;
};

#endif // KAEDELIBRARYPANEL_H