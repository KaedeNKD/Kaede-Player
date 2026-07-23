#pragma once
#ifndef VISUALIZERNODES_H
#define VISUALIZERNODES_H

#include <QQuickItem>
#include <QQuickPaintedItem> 
#include <QSGNode>
#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <QPointF>
#include <vector>
#include <deque>
#include <mutex>
#include <QString>

// =======================================================================
// 👑 三軌波形可視化 (含類比 CRT 螢光粉時序殘影引擎)
// =======================================================================
class VisWaveformItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(float normL READ normL NOTIFY levelsChanged)
    Q_PROPERTY(float normR READ normR NOTIFY levelsChanged)
    Q_PROPERTY(float normPeakL READ normPeakL NOTIFY levelsChanged)
    Q_PROPERTY(float normPeakR READ normPeakR NOTIFY levelsChanged)
    Q_PROPERTY(float dbL READ dbL NOTIFY levelsChanged)
    Q_PROPERTY(float dbR READ dbR NOTIFY levelsChanged)
public:
    VisWaveformItem(QQuickItem* parent = nullptr);
    void pushData(const std::vector<float>& pcm);
    
    float normL() const { return m_normL; }
    float normR() const { return m_normR; }
    float normPeakL() const { return m_normPeakL; }
    float normPeakR() const { return m_normPeakR; }
    float dbL() const { return m_l_db; }
    float dbR() const { return m_r_db; }

signals:
    void levelsChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    std::vector<float> m_pcm;
    
    // 👑 類比示波器：螢光粉時序殘影緩衝區
    std::vector<float> m_smoothWaveL;
    std::vector<float> m_smoothWaveR;

    float m_l_db = -60.0f, m_r_db = -60.0f, m_l_peak = -60.0f, m_r_peak = -60.0f;
    float m_l_vel = 0.0f, m_r_vel = 0.0f;
    float m_normL = 0.0f, m_normR = 0.0f, m_normPeakL = 0.0f, m_normPeakR = 0.0f;
    std::mutex m_mutex;
};

// =======================================================================
// 雙通道頻譜可視化 (VisFftItem)
// =======================================================================
class VisFftItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(bool isPsd READ isPsd WRITE setIsPsd NOTIFY isPsdChanged)
public:
    VisFftItem(QQuickItem* parent = nullptr);
    void pushData(const std::vector<float>& fft);
    
    bool isPsd() const { return m_isPsd; }
    void setIsPsd(bool psd) { m_isPsd = psd; emit isPsdChanged(); }

signals:
    void isPsdChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    std::vector<float> m_fft;
    std::vector<float> m_smoothL;
    std::vector<float> m_smoothR;
    std::vector<float> m_xTable;
    bool m_isPsd = false;
    int m_lastW = 0;
    std::mutex m_mutex;
};

// =======================================================================
// 磁流體相位雷達 (VisGonioItem)
// =======================================================================
class VisGonioItem : public QQuickItem {
    Q_OBJECT
public:
    VisGonioItem(QQuickItem* parent = nullptr);
    void pushData(const std::vector<float>& pcm);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    std::vector<float> m_pcm;
    std::vector<float> m_energy;
    std::vector<float> m_sinTable;
    std::vector<float> m_cosTable;
    std::mutex m_mutex;
};

// =======================================================================
// 電平儀表 (VisMeterItem)
// =======================================================================
class VisMeterItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(float normL READ normL NOTIFY levelsChanged)
    Q_PROPERTY(float normR READ normR NOTIFY levelsChanged)
    Q_PROPERTY(float peakL READ peakL NOTIFY levelsChanged)
    Q_PROPERTY(float peakR READ peakR NOTIFY levelsChanged)
public:
    VisMeterItem(QQuickItem* parent = nullptr);
    void pushData(const std::vector<float>& pcm);
    
    float normL() const { return m_normL; }
    float normR() const { return m_normR; }
    float peakL() const { return m_normPeakL; }
    float peakR() const { return m_normPeakR; }

signals:
    void levelsChanged();

private:
    std::vector<float> m_pcm;
    float m_l_db = -60.0f, m_r_db = -60.0f, m_l_peak = -60.0f, m_r_peak = -60.0f;
    float m_l_vel = 0.0f, m_r_vel = 0.0f;
    float m_normL = 0.0f, m_normR = 0.0f, m_normPeakL = 0.0f, m_normPeakR = 0.0f;
    std::mutex m_mutex;
};

// =======================================================================
// 靜態網格快取 (VisGridItem)
// =======================================================================
class VisGridItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool isOsc READ isOsc WRITE setIsOsc NOTIFY isOscChanged)
public:
    VisGridItem(QQuickItem* parent = nullptr);
    
    QString title() const { return m_title; }
    void setTitle(const QString& t) { m_title = t; update(); emit titleChanged(); }
    
    bool isOsc() const { return m_isOsc; }
    void setIsOsc(bool o) { m_isOsc = o; update(); emit isOscChanged(); }

signals:
    void titleChanged();
    void isOscChanged();

protected:
    void paint(QPainter* painter) override;

private:
    QString m_title;
    bool m_isOsc = false;
};

#endif // VISUALIZERNODES_H