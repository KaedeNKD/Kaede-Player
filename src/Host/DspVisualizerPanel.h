#pragma once
#ifndef DSPVISUALIZERPANEL_H
#define DSPVISUALIZERPANEL_H

#include <QQuickWidget>
#include <vector>

// ⚡ 預先宣告我們在 VisualizerNodes.h 中定義好的節點，避免編譯器找不到型別
class VisWaveformItem;
class VisFftItem;
class VisGonioItem;

class DspVisualizerPanel : public QQuickWidget {
    Q_OBJECT
public:
    explicit DspVisualizerPanel(QWidget* parent = nullptr);

public slots:
    void updateAudioData(const std::vector<float>& pcm, const std::vector<float>& fft);

private:
    void initRhiEngine();
    
    VisWaveformItem* m_osc = nullptr;
    VisFftItem* m_psd = nullptr;
    VisFftItem* m_fft = nullptr;
    VisGonioItem* m_gonio = nullptr; 
};

#endif // DSPVISUALIZERPANEL_H