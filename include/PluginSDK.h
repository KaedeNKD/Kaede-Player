#pragma once
#ifndef KAEDE_PLUGIN_SDK_H
#define KAEDE_PLUGIN_SDK_H

#include <QObject>
#include <QString>
#include <QWidget>
#include <QVariant>
#include <QtPlugin>

class IKaedePlugin {
public:
    virtual ~IKaedePlugin() = default;
    
    // ⚡ 剛性補回：確保你的 KaedePlayerPlugin 覆寫完全一致！
    virtual QString getPluginName() const = 0;
    
    virtual QWidget* getPluginWidget() = 0;
    
    // ⚡ 剛性補回：初始化音訊匯流排介面！
    virtual void initializeAudioBus() = 0;
    
    // ⚡ 全局設定熱重載血管
    virtual void onConfigChanged(const QString& key, const QVariant& value) {}
};

#define IKaedePlugin_iid "com.kaede.KaedeDAW.IKaedePlugin"
Q_DECLARE_INTERFACE(IKaedePlugin, IKaedePlugin_iid)

#endif // KAEDE_PLUGIN_SDK_H