#pragma once
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPaintEvent>
#include <QStringList>

class DropZoneDialog : public QDialog {
    Q_OBJECT
public:
    explicit DropZoneDialog(QWidget *parent = nullptr);
    QStringList getImportedFiles() const { return importedFiles; }

signals:
    void filesReady(const QStringList& files);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList importedFiles;
    bool isDragging = false; // 用於判斷檔案是否懸停在上方
};