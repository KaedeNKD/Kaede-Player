#include "DropZoneDialog.h"
#include <QFileDialog>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QMouseEvent>

DropZoneDialog::DropZoneDialog(QWidget *parent) : QDialog(parent) {
    setFixedSize(400, 300);
    setWindowTitle("導入檔案");
    setAcceptDrops(true); // ⚡ 核心：允許接收拖曳事件
    setStyleSheet("background-color: #1E1E1E;"); // 暗色基調
}

void DropZoneDialog::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction(); 
        isDragging = true;
        update(); // 觸發重繪，讓邊框發光
    }
}

void DropZoneDialog::dropEvent(QDropEvent *event) {
    isDragging = false;
    update();
    
    importedFiles.clear();
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            importedFiles.append(url.toLocalFile());
        }
    }
    
    if (!importedFiles.isEmpty()) {
        emit filesReady(importedFiles);
        accept(); // 關閉視窗並回傳成功狀態
    }
}

void DropZoneDialog::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    // 點擊空白處，呼叫系統總管 (支援多選)
    QStringList files = QFileDialog::getOpenFileNames(this, 
                        "選擇檔案 (支援多選)", "", 
                        "Audio/Media Files (*.wav *.mp3 *.flac *.mp4);;All Files (*.*)");
                        
    if (!files.isEmpty()) {
        importedFiles = files;
        emit filesReady(importedFiles);
        accept();
    }
}

void DropZoneDialog::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 繪製虛線邊框 (拖曳時變成主題藍色)
    QPen dashPen(isDragging ? QColor("#4DAAFB") : QColor("#555555"));
    dashPen.setWidth(2);
    dashPen.setStyle(Qt::DashLine);
    painter.setPen(dashPen);
    painter.drawRoundedRect(10, 10, width() - 20, height() - 20, 15, 15);

    // 繪製提示文字
    painter.setPen(isDragging ? QColor("#4DAAFB") : QColor("#AAAAAA"));
    painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
    painter.drawText(rect(), Qt::AlignCenter, isDragging ? "放開滑鼠以導入!" : "拖曳檔案至此\n或點擊選擇檔案");
}