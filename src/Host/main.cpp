#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // 剛性整流：強制放行高 DPI 螢幕的幾何線性穿透縮放，防止高解析度下 UI 線條與字體發虛
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    // 初始化 Qt 萬能圖形總管驅動
    QApplication app(argc, argv);

    // 實例化我們的 C++ 轉生無邊框外殼主視窗
    MainWindow window;
    window.show();

    // 讓主執行緒進入無盡的事件循環，等待音訊總線與 UI 的訊號喚醒
    return app.exec();
}