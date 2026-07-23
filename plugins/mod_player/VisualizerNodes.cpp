#include "VisualizerNodes.h"
#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <cmath>
#include <algorithm>
#include <QPainter>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class QsgLineNode : public QSGGeometryNode {
public:
    QsgLineNode(const QColor& c, float w, int mode = QSGGeometry::DrawLineStrip) {
        geo = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geo->setLineWidth(w); geo->setDrawingMode(mode); setGeometry(geo); setFlag(QSGNode::OwnsGeometry);
        mat = new QSGFlatColorMaterial; mat->setColor(c); setMaterial(mat); setFlag(QSGNode::OwnsMaterial);
    }
    QSGGeometry* geo; QSGFlatColorMaterial* mat;
};

class QsgPolygonNode : public QSGGeometryNode {
public:
    QsgPolygonNode(const QColor& c) {
        geo = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geo->setDrawingMode(QSGGeometry::DrawTriangles); setGeometry(geo); setFlag(QSGNode::OwnsGeometry);
        mat = new QSGFlatColorMaterial; mat->setColor(c); setMaterial(mat); setFlag(QSGNode::OwnsMaterial);
    }
    QSGGeometry* geo; QSGFlatColorMaterial* mat;
};

// =======================================================================
// 👑 DUAL 示波器：過零觸發 + 時空平滑演算法
// =======================================================================
VisWaveformItem::VisWaveformItem(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

void VisWaveformItem::pushData(const std::vector<float>& pcm) {
    if(pcm.empty()) return;
    
    float pL = 0.0f, pR = 0.0f; 
    for(size_t i = 0; i < pcm.size() - 1; i += 2) { 
        if(std::abs(pcm[i]) > pL) pL = std::abs(pcm[i]); 
        if(std::abs(pcm[i+1]) > pR) pR = std::abs(pcm[i+1]); 
    }
    
    float dL = pL > 0.0001f ? 20.0f * std::log10(pL) : -60.0f;
    float dR = pR > 0.0001f ? 20.0f * std::log10(pR) : -60.0f;
    
    auto phys = [](float target, float& current, float& peak, float& velocity) {
        target = std::max<float>(-60.0f, std::min<float>(6.0f, target)); 
        if(target >= peak) peak = target; else peak += (target - peak) * 0.008f; 
        if (std::isnan(current)) current = -60.0f; if (std::isnan(velocity)) velocity = 0.0f;
        
        if (target > current) {
            current = target; 
            velocity = 0.0f;
        } else {
            velocity += (target - current) * 0.08f; 
            velocity *= 0.78f; 
            current = std::max<float>(-60.0f, std::min<float>(6.0f, current + velocity));
        }
    };
    
    auto getNorm = [](float db) { 
        if(std::isnan(db) || db <= -60.0f) return 0.0f; if(db >= 6.0f) return 1.0f;
        float norm = db >= 0.0f ? (0.85f + 0.15f * (db / 6.0f)) : db >= -12.0f ? (0.50f + 0.35f * ((db + 12.0f) / 12.0f)) : (0.50f * ((db + 60.0f) / 48.0f)); 
        return std::clamp(norm, 0.0f, 1.0f); 
    };

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pcm = pcm;
        phys(dL, m_l_db, m_l_peak, m_l_vel); 
        phys(dR, m_r_db, m_r_peak, m_r_vel);
        
        m_normL = getNorm(m_l_db); 
        m_normR = getNorm(m_r_db); 
        m_normPeakL = getNorm(m_l_peak); 
        m_normPeakR = getNorm(m_r_peak); 
    }
    
    emit levelsChanged(); 
    update(); 
}

QSGNode* VisWaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QSGNode* root = oldNode;
    if (!root) {
        root = new QSGNode;
        root->appendChildNode(new QsgLineNode(QColor(255, 255, 255, 240), 2.0f)); 
        root->appendChildNode(new QsgLineNode(QColor(100, 220, 255, 255), 1.5f));          
        root->appendChildNode(new QsgLineNode(QColor(255, 210, 140, 255), 1.5f)); 
    }
    std::vector<float> pcmCopy;
    { std::lock_guard<std::mutex> lock(m_mutex); pcmCopy = m_pcm; }
    if (pcmCopy.empty() || width() <= 0 || height() <= 0) return root;

    int targetSamples = 512;
    int maxAvailable = pcmCopy.size() / 2;
    if (maxAvailable < targetSamples) targetSamples = maxAvailable;

    // 👑 第一維度：CRT 過零觸發 (鎖定相位)
    int searchEnd = maxAvailable - targetSamples;
    int searchStart = std::max(0, searchEnd - 1024);
    int triggerOffset = searchEnd;

    for (int i = searchEnd; i >= searchStart; --i) {
        if (pcmCopy[i * 2] <= 0.0f && pcmCopy[(i + 1) * 2] > 0.0f) {
            triggerOffset = i;
            break;
        }
    }

    if (m_smoothWaveL.size() != targetSamples) m_smoothWaveL.assign(targetSamples, 0.0f);
    if (m_smoothWaveR.size() != targetSamples) m_smoothWaveR.assign(targetSamples, 0.0f);

    float w = static_cast<float>(width()), h = static_cast<float>(height()), h2 = h / 2.0f;
    float step = w / std::max<float>(1.0f, static_cast<float>(targetSamples - 1));

    auto* nAvg = static_cast<QsgLineNode*>(root->childAtIndex(0));
    auto* nL = static_cast<QsgLineNode*>(root->childAtIndex(1));
    auto* nR = static_cast<QsgLineNode*>(root->childAtIndex(2));
    nAvg->geo->allocate(targetSamples); nL->geo->allocate(targetSamples); nR->geo->allocate(targetSamples);

    auto* vAvg = nAvg->geo->vertexDataAsPoint2D(); auto* vL = nL->geo->vertexDataAsPoint2D(); auto* vR = nR->geo->vertexDataAsPoint2D();
    
    int edgeSamples = targetSamples * 0.15f; // 邊緣 15% 進行衰減

    for(int i = 0; i < targetSamples; ++i) {
        int pcmIdx = (triggerOffset + i) * 2;
        float l = std::isnan(pcmCopy[pcmIdx]) ? 0.0f : pcmCopy[pcmIdx]; 
        float r = (pcmIdx + 1 < pcmCopy.size() && !std::isnan(pcmCopy[pcmIdx + 1])) ? pcmCopy[pcmIdx + 1] : l;
        
        // 👑 第二維度：空間羽化衰減 (Spatial Windowing)
        float window = 1.0f;
        if (i < edgeSamples) {
            window = 0.5f * (1.0f - std::cos(M_PI * i / edgeSamples));
        } else if (i > targetSamples - edgeSamples) {
            int dist = targetSamples - i;
            window = 0.5f * (1.0f - std::cos(M_PI * dist / edgeSamples));
        }
        l *= window;
        r *= window;

        // 👑 第三維度：螢光粉時序殘影 (Temporal Phosphor Decay) 
        // 在 ASIO 斷層期間，透過 0.45(殘影) + 0.55(新訊號) 的混合，賦予柔軟的黏滯感！
        m_smoothWaveL[i] = m_smoothWaveL[i] * 0.45f + l * 0.55f;
        m_smoothWaveR[i] = m_smoothWaveR[i] * 0.45f + r * 0.55f;

        float finalL = m_smoothWaveL[i];
        float finalR = m_smoothWaveR[i];
        float avg = (finalL + finalR) * 0.5f; 
        
        float posX = i * step;
        vAvg[i].set(posX, h2 - avg * h2); 
        vL[i].set(posX, h2 - finalL * h2); 
        vR[i].set(posX, h2 - finalR * h2);
    }
    
    nAvg->markDirty(QSGNode::DirtyGeometry); nL->markDirty(QSGNode::DirtyGeometry); nR->markDirty(QSGNode::DirtyGeometry);
    return root;
}

// =======================================================================
// 頻譜儀 (VisFftItem)
// =======================================================================
VisFftItem::VisFftItem(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); m_smoothL.assign(256, -60.0f); m_smoothR.assign(256, -60.0f); }
void VisFftItem::pushData(const std::vector<float>& fft) { std::lock_guard<std::mutex> lock(m_mutex); m_fft = fft; update(); }
QSGNode* VisFftItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QSGNode* root = oldNode;
    if (!root) {
        root = new QSGNode;
        if (property("isPsd").toBool()) { root->appendChildNode(new QsgLineNode(QColor(0, 200, 255, 220), 2.0f)); root->appendChildNode(new QsgLineNode(QColor(0, 100, 255, 180), 2.0f)); } 
        else { root->appendChildNode(new QsgLineNode(QColor(255, 100, 200, 220), 1.0f)); root->appendChildNode(new QsgLineNode(QColor(200, 50, 150, 180), 1.0f)); }
    }
    std::vector<float> fCopy; { std::lock_guard<std::mutex> lock(m_mutex); fCopy = m_fft; }
    if (fCopy.empty() || width() <= 0 || height() <= 0) return root;

    float w = static_cast<float>(width()), h = static_cast<float>(height());
    if (static_cast<int>(w) != m_lastW) {
        m_xTable.resize(256);
        for(int i = 0; i < 256; ++i) m_xTable[i] = w * std::pow(static_cast<float>(i) / 255.0f, 0.8f); 
        m_lastW = static_cast<int>(w);
    }
    auto* nL = static_cast<QsgLineNode*>(root->childAtIndex(0)); auto* nR = static_cast<QsgLineNode*>(root->childAtIndex(1));
    nL->geo->allocate(256); nR->geo->allocate(256);
    auto* vLPtr = nL->geo->vertexDataAsPoint2D(); auto* vRPtr = nR->geo->vertexDataAsPoint2D();
    float smoothFactor = property("isPsd").toBool() ? 0.5f : 1.0f; int halfOffset = fCopy.size() / 2;

    for(int i = 0; i < 256; ++i) {
        float rawL = (i < fCopy.size() && !std::isnan(fCopy[i])) ? fCopy[i] : 0.0f; 
        float rawR = (halfOffset + i < fCopy.size() && !std::isnan(fCopy[halfOffset + i])) ? fCopy[halfOffset + i] : rawL;
        float vL = rawL > 0.000001f ? 20.0f * std::log10(rawL) : -60.0f; float vR = rawR > 0.000001f ? 20.0f * std::log10(rawR) : -60.0f;
        m_smoothL[i] = m_smoothL[i] * (1.0f - smoothFactor) + vL * smoothFactor; m_smoothR[i] = m_smoothR[i] * (1.0f - smoothFactor) + vR * smoothFactor;
        float normL = std::clamp((m_smoothL[i] + 60.0f) / 60.0f, 0.0f, 1.0f); float normR = std::clamp((m_smoothR[i] + 60.0f) / 60.0f, 0.0f, 1.0f);
        vLPtr[i].set(m_xTable[i], h - normL * h); vRPtr[i].set(m_xTable[i], h - normR * h);
    }
    nL->markDirty(QSGNode::DirtyGeometry); nR->markDirty(QSGNode::DirtyGeometry); return root;
}

// =======================================================================
// 相位雷達 (VisGonioItem)
// =======================================================================
VisGonioItem::VisGonioItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true); m_energy.assign(360, 0.0f);
    for(int i = 0; i < 360; ++i) { float a = -M_PI + i * ((2.0f * M_PI) / 360.0f); m_sinTable.push_back(std::sin(a)); m_cosTable.push_back(std::cos(a)); }
}
void VisGonioItem::pushData(const std::vector<float>& pcm) { std::lock_guard<std::mutex> lock(m_mutex); m_pcm = pcm; update(); }
QSGNode* VisGonioItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QSGNode* root = oldNode;
    if (!root) {
        root = new QSGNode; root->appendChildNode(new QsgPolygonNode(QColor(0, 150, 255, 30))); root->appendChildNode(new QsgPolygonNode(QColor(0, 200, 255, 80)));
        root->appendChildNode(new QsgPolygonNode(QColor(180, 240, 255, 255))); root->appendChildNode(new QsgLineNode(QColor(255, 255, 255, 180), 2.0f, QSGGeometry::DrawLineLoop)); 
    }
    std::vector<float> pcmCopy; { std::lock_guard<std::mutex> lock(m_mutex); pcmCopy = m_pcm; }
    if (pcmCopy.empty() || width() <= 0 || height() <= 0) return root;

    float cx = static_cast<float>(width()) / 2.0f, cy = static_cast<float>(height()) / 2.0f;
    float baseR = std::min<float>(cx, cy) * 0.20f, bulgeR = std::min<float>(cx, cy) * 0.65f; 
    std::vector<float> curEng(360, 0.0f);
    for(size_t i = 0; i < pcmCopy.size() - 1; i += 2) {
        float l = std::isnan(pcmCopy[i]) ? 0.0f : pcmCopy[i], r = std::isnan(pcmCopy[i+1]) ? 0.0f : pcmCopy[i+1];
        float mid = (l + r) * 0.70710678f, side = (r - l) * 0.70710678f; 
        float mag = std::sqrt(mid*mid + side*side), ang = std::atan2(side, mid); 
        int deg = static_cast<int>(((ang + M_PI) / (2.0f * M_PI)) * 360.0f) % 360; if(deg < 0) deg += 360;
        float norm = std::pow(mag, 0.5f) * 1.5f; norm = std::clamp(norm, 0.0f, 1.0f);
        if(norm > curEng[deg]) curEng[deg] = norm;
    }
    for(int i = 0; i < 360; ++i) m_energy[i] = m_energy[i] * 0.65f + curEng[i] * 0.35f; 
    std::vector<float> smooth(360, 0.0f);
    for(int i = 0; i < 360; ++i) {
        float sum = 0.0f, weightSum = 0.0f;
        for(int j = -12; j <= 12; ++j) { int idx = (i + j + 360) % 360; float w = 1.0f - (std::abs(j) / 12.0f); sum += m_energy[idx] * w; weightSum += w; }
        smooth[i] = sum / weightSum;
    }
    auto drawLayer = [&](int index, float scaleMultiplier, bool isLine) {
        auto* n = static_cast<QSGGeometryNode*>(root->childAtIndex(index));
        if (!isLine) {
            n->geometry()->allocate(1080); auto* v = n->geometry()->vertexDataAsPoint2D(); int vIdx = 0;
            for(int i = 0; i < 360; ++i) {
                int next = (i + 1) % 360; float r1 = (baseR + smooth[i] * bulgeR) * scaleMultiplier, r2 = (baseR + smooth[next] * bulgeR) * scaleMultiplier;
                v[vIdx++].set(cx, cy); v[vIdx++].set(cx + r1 * m_sinTable[i], cy - r1 * m_cosTable[i]); v[vIdx++].set(cx + r2 * m_sinTable[next], cy - r2 * m_cosTable[next]); 
            }
        } else {
            n->geometry()->allocate(360); auto* v = n->geometry()->vertexDataAsPoint2D();
            for(int i = 0; i < 360; ++i) { float r = (baseR + smooth[i] * bulgeR) * scaleMultiplier; v[i].set(cx + r * m_sinTable[i], cy - r * m_cosTable[i]); }
        }
        n->markDirty(QSGNode::DirtyGeometry);
    };
    drawLayer(0, 1.25f, false); drawLayer(1, 1.00f, false); drawLayer(2, 0.85f, false); drawLayer(3, 0.85f, true);  
    return root;
}

// =======================================================================
// 👑 電平儀表數學轉接橋接器
// =======================================================================
VisMeterItem::VisMeterItem(QQuickItem* parent) : QQuickItem(parent) {}
void VisMeterItem::pushData(const std::vector<float>& pcm) { Q_UNUSED(pcm); }

// =======================================================================
// 背景網格
// =======================================================================
VisGridItem::VisGridItem(QQuickItem* parent) : QQuickPaintedItem(parent) { setOpaquePainting(false); setRenderTarget(QQuickPaintedItem::FramebufferObject); }
void VisGridItem::paint(QPainter* painter) {
    painter->setRenderHint(QPainter::Antialiasing); QRectF r = boundingRect();
    painter->setPen(QColor(255, 255, 255, 150)); painter->setFont(QFont("Consolas", 10, QFont::Bold)); painter->drawText(QRectF(0, 0, r.width(), 20), Qt::AlignLeft | Qt::AlignTop, property("title").toString());
    QRectF box(0, 25, r.width(), r.height() - 25); painter->setPen(QPen(QColor(255, 255, 255, 30), 1)); painter->setBrush(QColor(0, 0, 0, 120)); painter->drawRect(box);
    if (property("title").toString().contains("FERROFLUID")) {
        float cx = box.left() + (box.width() - 80) / 2.0f, cy = box.center().y(), maxR = std::min((box.width() - 80), box.height()) / 2.0f - 15.0f;
        painter->setPen(QPen(QColor(255, 255, 255, 15), 1, Qt::DashLine)); painter->drawEllipse(QPointF(cx, cy), maxR, maxR); painter->drawEllipse(QPointF(cx, cy), maxR * 0.5f, maxR * 0.5f);
        painter->drawLine(cx, cy - maxR, cx, cy + maxR); painter->drawLine(cx - maxR, cy, cx + maxR, cy);
        painter->setPen(QColor(255, 255, 255, 70)); painter->setFont(QFont("Consolas", 8, QFont::Bold));
        painter->drawText(QRectF(cx - 10, cy - maxR - 15, 20, 12), Qt::AlignCenter, "M"); painter->drawText(QRectF(cx + maxR + 5, cy - 6, 20, 12), Qt::AlignLeft | Qt::AlignVCenter, "S");
        painter->drawText(QRectF(cx - maxR * 0.707f - 15, cy - maxR * 0.707f - 15, 20, 12), Qt::AlignCenter, "L"); painter->drawText(QRectF(cx + maxR * 0.707f - 5, cy - maxR * 0.707f - 15, 20, 12), Qt::AlignCenter, "R");
    } else if (property("isOsc").toBool()) { painter->setPen(QPen(QColor(255, 255, 255, 20), 1, Qt::DashLine)); painter->drawLine(box.left(), box.center().y() + 15, box.right(), box.center().y() + 15); } 
    else {
        painter->setPen(QPen(QColor(255, 255, 255, 20), 1, Qt::DashLine)); painter->setFont(QFont("Consolas", 8));
        float minLog = std::log10(20.0f), maxLog = std::log10(20000.0f); std::vector<int> hzMarks = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        for (int hz : hzMarks) { float ratio = (std::log10((float)hz) - minLog) / (maxLog - minLog); float px = box.x() + (box.width() * std::pow(ratio, 0.8f)); painter->drawLine(QPointF(px, box.top()), QPointF(px, box.bottom())); QString hzStr = (hz >= 1000) ? QString("%1k").arg(hz/1000) : QString::number(hz); painter->setPen(QColor(255, 255, 255, 80)); painter->drawText(QRectF(px - 15, box.bottom() - 15, 30, 15), Qt::AlignHCenter | Qt::AlignBottom, hzStr); painter->setPen(QPen(QColor(255, 255, 255, 20), 1, Qt::DashLine)); }
        std::vector<int> yDbMarks = {-60, -40, -20}; for (int db : yDbMarks) { float ratio = (db + 60.0f) / 60.0f; float py = box.bottom() - ratio * box.height(); painter->drawLine(QPointF(box.left(), py), QPointF(box.right(), py)); painter->setPen(QColor(255, 255, 255, 80)); painter->drawText(QRectF(box.left() + 5, py - 15, 30, 15), Qt::AlignLeft | Qt::AlignVCenter, QString::number(db)); painter->setPen(QPen(QColor(255, 255, 255, 20), 1, Qt::DashLine)); }
    }
}