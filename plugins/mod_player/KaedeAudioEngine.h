#pragma once
#ifndef KAEDEAUDIOENGINE_H
#define KAEDEAUDIOENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QThread>
#include <vector>
#include <atomic>
#include <thread>
#include <complex>
#include <memory>
#include <cstdint>
#include <cstring> 
#include <mutex> 

#include "../../bass/bass.h"
#include "../../bass/basswasapi.h"
#include "../../bass/bassasio.h"
#include "../../bass/bassdsd.h" 

enum class OutputMode { SharedMixer = 0, WASAPI_Exclusive = 1, ASIO = 2 };
enum class DspCoreMode { Standard_64 = 0, Alien_FIR_128 = 1 };

struct AudioDeviceInfo { int id; QString name; OutputMode mode; };
Q_DECLARE_METATYPE(AudioDeviceInfo)

struct PipelineInfo { QString formatSpec; bool hasSrc; QString apiMode; QString deviceName; QString latency; };
Q_DECLARE_METATYPE(PipelineInfo)

struct DspBiquad { double b0 = 1, b1 = 0, b2 = 0; double a1 = 0, a2 = 0; };
Q_DECLARE_METATYPE(std::vector<DspBiquad>)

struct PeqConfig { 
    bool masterEnabled = true; 
    std::vector<DspBiquad> filters; 
    bool bypassed = false; 
    double preampLinear = 1.0; 
    double wetRatio = 1.0; 
};
struct PeqState { double x1[2] = {0}, x2[2] = {0}; double y1[2] = {0}, y2[2] = {0}; };

template <typename T>
class SpscRingBuffer {
    std::vector<T> buffer;
    std::atomic<size_t> read_idx{0};
    std::atomic<size_t> write_idx{0};
public:
    SpscRingBuffer(size_t size) : buffer(size) {}
    size_t write(const T* data, size_t count);
    size_t read(T* data, size_t count);
    size_t read_available() const;
    size_t write_available() const;
    void reset() { read_idx.store(0); write_idx.store(0); }
};

template <typename T>
size_t SpscRingBuffer<T>::write(const T* data, size_t count) {
    size_t r = read_idx.load(std::memory_order_acquire);
    size_t w = write_idx.load(std::memory_order_relaxed);
    size_t available = (r > w) ? (r - w - 1) : (buffer.size() - w + r - 1);
    if (available == 0) return 0;
    size_t to_write = std::min(count, available);
    size_t first_part = std::min(to_write, buffer.size() - w);
    std::memcpy(buffer.data() + w, data, first_part * sizeof(T));
    if (to_write > first_part) std::memcpy(buffer.data(), data + first_part, (to_write - first_part) * sizeof(T));
    write_idx.store((w + to_write) % buffer.size(), std::memory_order_release);
    return to_write;
}

template <typename T>
size_t SpscRingBuffer<T>::read(T* data, size_t count) {
    size_t r = read_idx.load(std::memory_order_relaxed);
    size_t w = write_idx.load(std::memory_order_acquire);
    size_t available = (w >= r) ? (w - r) : (buffer.size() - r + w);
    if (available == 0) return 0;
    size_t to_read = std::min(count, available);
    size_t first_part = std::min(to_read, buffer.size() - r);
    std::memcpy(data, buffer.data() + r, first_part * sizeof(T));
    if (to_read > first_part) std::memcpy(data + first_part, buffer.data(), (to_read - first_part) * sizeof(T));
    read_idx.store((r + to_read) % buffer.size(), std::memory_order_release);
    return to_read;
}

template <typename T>
size_t SpscRingBuffer<T>::read_available() const {
    size_t r = read_idx.load(std::memory_order_acquire);
    size_t w = write_idx.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (buffer.size() - r + w);
}

template <typename T>
size_t SpscRingBuffer<T>::write_available() const {
    size_t r = read_idx.load(std::memory_order_acquire);
    size_t w = write_idx.load(std::memory_order_acquire);
    return (r > w) ? (r - w - 1) : (buffer.size() - w + r - 1);
}

class KaedePolyphaseResampler;

class KaedeAudioWorker : public QObject {
    Q_OBJECT
public:
    explicit KaedeAudioWorker(QObject* parent = nullptr);
    ~KaedeAudioWorker();

    DWORD processAudioData(void *buffer, DWORD length);
    void processSharedDSP(void *buffer, DWORD length); 

public slots:
    void initEngine(); void destroyEngine();
    QList<AudioDeviceInfo> getDeviceListWorker(OutputMode mode) const;
    void setOutputDeviceWorker(OutputMode mode, int deviceId);
    void setDspCoreModeWorker(DspCoreMode mode); 
    void setAlienFirConfigWorker(int taps, int targetRate); 
    bool loadTrack(const QString& filePath);
    void playTrack(); void pauseTrack(); void stopTrack();
    void seekTrack(double targetSeconds); void setVolume(double vol); void setLooping(bool loop);
    double getDurationWorker() const; PipelineInfo getPipelineInfoWorker() const;
    
    void updatePeqConfigWorker(bool masterEnabled, const std::vector<DspBiquad>& filters, bool bypassed, double preampLinear, double wetRatio);
    void setCrossfeedWorker(bool enabled, double level, double cutoffFreq);

    void setNoiseShapingWorker(bool enabled) { m_noiseShapingEnabled.store(enabled); }

    void emitBufferingStart() { emit bufferingStateChanged(true); }
    void emitBufferingDone() { emit bufferingStateChanged(false); }

signals:
    void playbackStateChanged(bool isPlaying);
    void positionChanged(double currentSec, double totalSec);
    void trackFinished();
    void dspDataReady(const std::vector<float>& pcm, const std::vector<float>& fft);
    void bufferingStateChanged(bool isBuffering);

private:
    void loadBassPlugins(); double getCurrentPosition() const; void updateHardwareLatency(); 
    void analyzerLoop(); void computeCustomFFT(const std::vector<float>& pcmInput, std::vector<float>& fftOutput);
    void resamplingLoop(); 
    
    std::thread m_analyzerThread; std::atomic<bool> m_analyzerRunning{false};
    std::thread m_resamplingThread; std::atomic<bool> m_resamplingRunning{false};
    std::mutex m_resamplerMutex; 
    
    OutputMode m_outputMode = OutputMode::SharedMixer; int m_deviceId = -1;
    std::atomic<DspCoreMode> m_coreMode{DspCoreMode::Standard_64}; 
    std::atomic<int> m_targetFirTaps{128};
    std::atomic<int> m_userTargetRate{-1};

    // 👑 擴充至 9階 動態適應暫存器 (Standard_64 專用)
    std::atomic<bool> m_noiseShapingEnabled{true};
    double m_stdNsErrorL[9] = {0};
    double m_stdNsErrorR[9] = {0};

    std::vector<uint8_t> m_ramAudioData;

    HSTREAM m_stream = 0;
    HSTREAM m_shadowStream = 0; 
    HDSP m_dspHandle = 0; 
    
    std::shared_ptr<PeqConfig> m_peqConfig; 
    std::vector<PeqState> m_peqStates;      

    std::atomic<bool> m_crossfeedEnabled{false};
    std::atomic<double> m_crossfeedLevel{0.22}; 
    std::atomic<double> m_crossfeedCutoff{700.0}; 
    double m_cfStateL = 0.0; double m_cfStateR = 0.0;

    std::atomic<double> m_volume64{1.0}; 
    std::atomic<int> m_channels{2}; 
    
    std::vector<float> m_pcmRing; std::atomic<int> m_ringIndex{0};
    
    std::unique_ptr<SpscRingBuffer<float>> m_spscBuffer;
    std::unique_ptr<KaedePolyphaseResampler> m_firResampler;
    std::vector<float> m_workerExtractBuffer;
    
    std::atomic<bool> m_isHardwareInitialized{false};
    
    std::atomic<double> m_latencyMs{0.0};
    std::atomic<int> m_baseSampleRate{44100}; 
    std::atomic<int> m_dacSampleRate{44100};  
    std::atomic<int> m_lastDacSampleRate{44100}; 

    uint32_t m_ditherSeed1 = 1337; uint32_t m_ditherSeed2 = 90210;

    QString m_currentFilePath = "";
    std::atomic<bool> m_isPlaying{false}; std::atomic<bool> m_isLooping{false};
    std::atomic<bool> m_isDsdMode{false}; std::atomic<bool> m_isAsioNativeDsd{false}; 
    std::atomic<bool> m_isSeeking{false};
    
    std::atomic<bool> m_isBuffering{false}; 
    mutable std::atomic<int64_t> m_smoothedUnplayedMicroSec{0};
    
    double m_duration = 0.0;
    std::vector<float> m_pcmBuffer; std::vector<float> m_fftBuffer;
};

class KaedeAudioEngine : public QObject {
    Q_OBJECT
public:
    static KaedeAudioEngine& instance() { static KaedeAudioEngine engine; return engine; }
    void init(); void destroy();
    QList<AudioDeviceInfo> getDeviceList(OutputMode mode) const;
    void setOutputDevice(OutputMode mode, int deviceId);
    
    void setDspCoreMode(DspCoreMode mode);
    void setAlienFirConfig(int taps, int targetRate); 
    void setNoiseShaping(bool enabled); 
    
    bool load(const QString& filePath); void play(); void pause(); void stop();
    void seek(double targetSeconds); void setVolume(double volume); void setLooping(bool loop);
    void updatePeqConfig(bool masterEnabled, const std::vector<DspBiquad>& filters, bool bypassed, double preampLinear, double wetRatio);
    void setCrossfeed(bool enabled, double level = 0.22, double cutoffFreq = 700.0);
    
    bool isPlaying() const { return m_isPlaying.load(); }
    double getDuration() const; PipelineInfo getPipelineInfo() const;

signals:
    void playbackStateChanged(bool isPlaying); 
    void positionChanged(double currentSec, double totalSec);
    void trackFinished(); 
    void dspDataReady(const std::vector<float>& pcm, const std::vector<float>& fft);
    void bufferingStateChanged(bool isBuffering); 

private:
    KaedeAudioEngine(QObject* parent = nullptr); ~KaedeAudioEngine();
    QThread* m_audioThread = nullptr; KaedeAudioWorker* m_worker = nullptr;
    std::atomic<bool> m_isPlaying{false};
};

#endif // KAEDEAUDIOENGINE_H