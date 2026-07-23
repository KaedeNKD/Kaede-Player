#ifndef PLAYERPLUGIN_H
#define PLAYERPLUGIN_H

#include <QObject>
#include <QWidget> // ⚡ 改為引入標準的 QWidget
#include "../../include/PluginSDK.h"
// ⚡ 刪除了 #include "PlayerWidget.h"，因為殭屍已經被我們火化了！

// 升級為主程式核心後的空殼外掛 (已接管至 Host)
class KaedePlayerPlugin : public QObject, public IKaedePlugin {
    Q_OBJECT
    // IID Must be string literal for MOC
    Q_PLUGIN_METADATA(IID "org.kaede.Plugin")
    Q_INTERFACES(IKaedePlugin)

public:
    KaedePlayerPlugin() {
        // 不再建立 PlayerWidget
    }
    
    virtual ~KaedePlayerPlugin() override {
    }

    virtual QString getPluginName() const override {
        return "UNIVERSAL MASTER PLAYER (INTEGRATED)";
    }

    virtual QWidget* getPluginWidget() override {
        // ⚡ 播放器已經整合進 Host 的 MainWindow，所以外掛不需要再提供介面了
        return nullptr;
    }

    virtual void initializeAudioBus() override {
    }
};

#endif // PLAYERPLUGIN_H