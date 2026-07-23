#include "KaedeAudioEngine.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QMetaObject>
#include <QLibrary>
#include <QFile>
#include <QFileInfo>
#include <algorithm> 
#include <cstring>   
#include <chrono>

#ifdef _WIN32
#include <avrt.h>
#pragma comment(lib, "Avrt.lib") 
#undef min
#undef max
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifdef _WIN32
#define BASS_PATH(x) reinterpret_cast<const wchar_t*>(x.utf16())
#else
#define BASS_PATH(x) x.toUtf8().constData()
#endif

#ifndef BASS_DSD_DOP
#define BASS_DSD_DOP 0x400
#endif

inline uint32_t xorshift32(uint32_t& state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state;
}

// =======================================================================
// 👑 獵鷹九號：SIMD 極速優化版 Polyphase Sinc 卷積升頻引擎 
// (整合 2階/5階/9階 動態適應心理聲學矩陣)
// =======================================================================
class KaedePolyphaseResampler {
private:
    int m_factor; int m_taps;
    std::vector<std::vector<double>> m_polyphaseFilter;
    std::vector<double> m_historyL; std::vector<double> m_historyR;
    int m_histIdx;
    
    // 👑 擴展至 9 階的殘差歷史暫存器
    double m_nsErrorL[9] = {0};
    double m_nsErrorR[9] = {0};
    
    double besselI0(double x) {
        double sum = 1.0, term = 1.0;
        for (int i = 1; i <= 50; ++i) {
            term *= (x * x) / (4.0 * i * i); sum += term;
            if (term < 1e-15) break;
        } return sum;
    }

public:
    KaedePolyphaseResampler(int factor, int taps, double beta = 9.0) : m_factor(factor), m_taps(taps), m_histIdx(taps - 1) {
        if (m_factor < 1) m_factor = 1;
        m_polyphaseFilter.resize(m_factor, std::vector<double>(m_taps, 0.0));
        m_historyL.resize(m_taps * 2, 0.0); m_historyR.resize(m_taps * 2, 0.0); 
        
        int halfTaps = m_taps / 2; double i0Beta = besselI0(beta);
        for (int phase = 0; phase < m_factor; ++phase) {
            double phaseOffset = static_cast<double>(phase) / m_factor;
            for (int k = 0; k < m_taps; ++k) {
                double n = k - halfTaps + phaseOffset;
                double sincVal = (n == 0.0) ? 1.0 : std::sin(M_PI * n) / (M_PI * n);
                double windowVal = besselI0(beta * std::sqrt(1.0 - std::pow((k - halfTaps) / static_cast<double>(halfTaps), 2))) / i0Beta;
                m_polyphaseFilter[phase][k] = sincVal * windowVal;
            }
        }
    }
    
    int getFactor() const { return m_factor; }

    void reset() {
        std::fill(m_historyL.begin(), m_historyL.end(), 0.0);
        std::fill(m_historyR.begin(), m_historyR.end(), 0.0);
        m_histIdx = m_taps - 1;
        std::fill(m_nsErrorL, m_nsErrorL + 9, 0.0);
        std::fill(m_nsErrorR, m_nsErrorR + 9, 0.0);
    }

    void process(const float* in, int inFrames, int channels, std::vector<float>& out, uint32_t& seed1, uint32_t& seed2, bool enableNS, int targetRate) {
        out.resize(inFrames * m_factor * channels);
        const double FIR_HEADROOM = 0.70710678; 
        const double LSB24 = 1.1920928955078125e-07;
        int outIdx = 0;
        
        // 👑 動態頻率判定與母帶係數矩陣分配
        int order = 2; const double* nsCoeffs = nullptr;
        static const double c2[2] = {2.0, -1.0}; 
        static const double c5[5] = {2.24, -2.39, 1.83, -0.81, 0.17}; // POW-R 2 Approximation
        static const double c9[9] = {2.412, -2.970, 2.738, -2.033, 1.492, -0.890, 0.441, -0.164, 0.034}; // POW-R 3 Approximation
        
        if (targetRate >= 700000) { order = 2; nsCoeffs = c2; } 
        else if (targetRate >= 350000) { order = 5; nsCoeffs = c5; } 
        else { order = 9; nsCoeffs = c9; }
        
        for (int i = 0; i < inFrames; ++i) {
            m_histIdx = (m_histIdx - 1 + m_taps) % m_taps;
            double inL = static_cast<double>(in[i * channels]);
            m_historyL[m_histIdx] = inL; m_historyL[m_histIdx + m_taps] = inL;
            if (channels > 1) {
                double inR = static_cast<double>(in[i * channels + 1]);
                m_historyR[m_histIdx] = inR; m_historyR[m_histIdx + m_taps] = inR;
            }

            const double* pHistL = &m_historyL[m_histIdx];
            const double* pHistR = &m_historyR[m_histIdx];

            for (int phase = 0; phase < m_factor; ++phase) {
                double outL = 0.0; double outR = 0.0;
                const double* pFilter = m_polyphaseFilter[phase].data();
                
                for (int k = 0; k < m_taps; ++k) outL += pHistL[k] * pFilter[k];
                if (channels > 1) { for (int k = 0; k < m_taps; ++k) outR += pHistR[k] * pFilter[k]; }
                
                outL *= FIR_HEADROOM; 
                if (channels > 1) outR *= FIR_HEADROOM;
                
                // --- Left Channel ---
                double r1L = static_cast<double>(xorshift32(seed1)) / 4294967295.0; 
                double r2L = static_cast<double>(xorshift32(seed2)) / 4294967295.0; 
                double ditherL = (r1L - r2L) * LSB24;
                
                double shaped_errorL = 0.0;
                if (enableNS) { for(int k = 0; k < order; ++k) shaped_errorL += m_nsErrorL[k] * nsCoeffs[k]; }
                double ditheredL = outL + ditherL + shaped_errorL;
                
                float quantL = static_cast<float>(std::clamp(ditheredL, -1.0, 1.0));
                out[outIdx++] = quantL;
                
                if (enableNS) {
                    double errL = ditheredL - static_cast<double>(quantL);
                    if (std::isnan(errL) || std::isinf(errL) || std::abs(errL) > 1.0) { std::fill(m_nsErrorL, m_nsErrorL + 9, 0.0); } 
                    else { for(int k = order - 1; k > 0; --k) m_nsErrorL[k] = m_nsErrorL[k-1]; m_nsErrorL[0] = errL; }
                } else { std::fill(m_nsErrorL, m_nsErrorL + 9, 0.0); }
                
                // --- Right Channel ---
                if (channels > 1) { 
                    double r1R = static_cast<double>(xorshift32(seed1)) / 4294967295.0; 
                    double r2R = static_cast<double>(xorshift32(seed2)) / 4294967295.0; 
                    double ditherR = (r1R - r2R) * LSB24;
                    
                    double shaped_errorR = 0.0;
                    if (enableNS) { for(int k = 0; k < order; ++k) shaped_errorR += m_nsErrorR[k] * nsCoeffs[k]; }
                    double ditheredR = outR + ditherR + shaped_errorR;
                    
                    float quantR = static_cast<float>(std::clamp(ditheredR, -1.0, 1.0));
                    out[outIdx++] = quantR;
                    
                    if (enableNS) {
                        double errR = ditheredR - static_cast<double>(quantR);
                        if (std::isnan(errR) || std::isinf(errR) || std::abs(errR) > 1.0) { std::fill(m_nsErrorR, m_nsErrorR + 9, 0.0); } 
                        else { for(int k = order - 1; k > 0; --k) m_nsErrorR[k] = m_nsErrorR[k-1]; m_nsErrorR[0] = errR; }
                    } else { std::fill(m_nsErrorR, m_nsErrorR + 9, 0.0); }
                } 
            }
        }
    }
};

typedef BOOL (WINAPI *P_BASS_WASAPI_Init)(int, DWORD, DWORD, DWORD, float, float, WASAPIPROC*, void*);
typedef BOOL (WINAPI *P_BASS_WASAPI_Free)();
typedef BOOL (WINAPI *P_BASS_WASAPI_GetDeviceInfo)(DWORD, BASS_WASAPI_DEVICEINFO*);
typedef BOOL (WINAPI *P_BASS_WASAPI_GetInfo)(BASS_WASAPI_INFO*);
typedef BOOL (WINAPI *P_BASS_WASAPI_Start)();
typedef BOOL (WINAPI *P_BASS_WASAPI_Stop)(BOOL);
typedef DWORD (WINAPI *P_BASS_WASAPI_GetDevice)();
typedef DWORD (WINAPI *P_BASS_WASAPI_GetData)(void*, DWORD);
typedef BOOL (WINAPI *P_BASS_ASIO_Init)(int, DWORD);
typedef BOOL (WINAPI *P_BASS_ASIO_Free)();
typedef BOOL (WINAPI *P_BASS_ASIO_GetDeviceInfo)(DWORD, BASS_ASIO_DEVICEINFO*);
typedef BOOL (WINAPI *P_BASS_ASIO_GetInfo)(BASS_ASIO_INFO*);
typedef BOOL (WINAPI *P_BASS_ASIO_SetRate)(double);
typedef double (WINAPI *P_BASS_ASIO_GetRate)();
typedef BOOL (WINAPI *P_BASS_ASIO_Start)(DWORD, DWORD);
typedef BOOL (WINAPI *P_BASS_ASIO_Stop)();
typedef BOOL (WINAPI *P_BASS_ASIO_ChannelEnable)(BOOL, DWORD, ASIOPROC*, void*);
typedef BOOL (WINAPI *P_BASS_ASIO_ChannelJoin)(BOOL, DWORD, int);
typedef BOOL (WINAPI *P_BASS_ASIO_ChannelSetFormat)(BOOL, DWORD, DWORD);
typedef DWORD (WINAPI *P_BASS_ASIO_GetDevice)();
typedef DWORD (WINAPI *P_BASS_ASIO_GetLatency)(BOOL);
typedef HSTREAM (WINAPI *P_BASS_DSD_StreamCreateFile)(BOOL, const void*, QWORD, QWORD, DWORD, DWORD);

static P_BASS_WASAPI_Init dyn_BASS_WASAPI_Init = nullptr; static P_BASS_WASAPI_Free dyn_BASS_WASAPI_Free = nullptr; static P_BASS_WASAPI_GetDeviceInfo dyn_BASS_WASAPI_GetDeviceInfo = nullptr; static P_BASS_WASAPI_GetInfo dyn_BASS_WASAPI_GetInfo = nullptr; static P_BASS_WASAPI_Start dyn_BASS_WASAPI_Start = nullptr; static P_BASS_WASAPI_Stop dyn_BASS_WASAPI_Stop = nullptr; static P_BASS_WASAPI_GetDevice dyn_BASS_WASAPI_GetDevice = nullptr; static P_BASS_WASAPI_GetData dyn_BASS_WASAPI_GetData = nullptr;
static P_BASS_ASIO_Init dyn_BASS_ASIO_Init = nullptr; static P_BASS_ASIO_Free dyn_BASS_ASIO_Free = nullptr; static P_BASS_ASIO_GetDeviceInfo dyn_BASS_ASIO_GetDeviceInfo = nullptr; static P_BASS_ASIO_GetInfo dyn_BASS_ASIO_GetInfo = nullptr; static P_BASS_ASIO_SetRate dyn_BASS_ASIO_SetRate = nullptr; static P_BASS_ASIO_GetRate dyn_BASS_ASIO_GetRate = nullptr; static P_BASS_ASIO_Start dyn_BASS_ASIO_Start = nullptr; static P_BASS_ASIO_Stop dyn_BASS_ASIO_Stop = nullptr; static P_BASS_ASIO_ChannelEnable dyn_BASS_ASIO_ChannelEnable = nullptr; static P_BASS_ASIO_ChannelJoin dyn_BASS_ASIO_ChannelJoin = nullptr; static P_BASS_ASIO_ChannelSetFormat dyn_BASS_ASIO_ChannelSetFormat = nullptr; static P_BASS_ASIO_GetDevice dyn_BASS_ASIO_GetDevice = nullptr; static P_BASS_ASIO_GetLatency dyn_BASS_ASIO_GetLatency = nullptr;
static P_BASS_DSD_StreamCreateFile dyn_BASS_DSD_StreamCreateFile = nullptr;

static void LoadDynamicBassPlugins() {
    static bool isLoaded = false; if (isLoaded) return;
    QLibrary wasapi("basswasapi"); if (wasapi.load()) { dyn_BASS_WASAPI_Init = (P_BASS_WASAPI_Init)wasapi.resolve("BASS_WASAPI_Init"); dyn_BASS_WASAPI_Free = (P_BASS_WASAPI_Free)wasapi.resolve("BASS_WASAPI_Free"); dyn_BASS_WASAPI_GetDeviceInfo = (P_BASS_WASAPI_GetDeviceInfo)wasapi.resolve("BASS_WASAPI_GetDeviceInfo"); dyn_BASS_WASAPI_GetInfo = (P_BASS_WASAPI_GetInfo)wasapi.resolve("BASS_WASAPI_GetInfo"); dyn_BASS_WASAPI_Start = (P_BASS_WASAPI_Start)wasapi.resolve("BASS_WASAPI_Start"); dyn_BASS_WASAPI_Stop = (P_BASS_WASAPI_Stop)wasapi.resolve("BASS_WASAPI_Stop"); dyn_BASS_WASAPI_GetDevice = (P_BASS_WASAPI_GetDevice)wasapi.resolve("BASS_WASAPI_GetDevice"); dyn_BASS_WASAPI_GetData = (P_BASS_WASAPI_GetData)wasapi.resolve("BASS_WASAPI_GetData"); }
    QLibrary asio("bassasio"); if (asio.load()) { dyn_BASS_ASIO_Init = (P_BASS_ASIO_Init)asio.resolve("BASS_ASIO_Init"); dyn_BASS_ASIO_Free = (P_BASS_ASIO_Free)asio.resolve("BASS_ASIO_Free"); dyn_BASS_ASIO_GetDeviceInfo = (P_BASS_ASIO_GetDeviceInfo)asio.resolve("BASS_ASIO_GetDeviceInfo"); dyn_BASS_ASIO_GetInfo = (P_BASS_ASIO_GetInfo)asio.resolve("BASS_ASIO_GetInfo"); dyn_BASS_ASIO_SetRate = (P_BASS_ASIO_SetRate)asio.resolve("BASS_ASIO_SetRate"); dyn_BASS_ASIO_GetRate = (P_BASS_ASIO_GetRate)asio.resolve("BASS_ASIO_GetRate"); dyn_BASS_ASIO_Start = (P_BASS_ASIO_Start)asio.resolve("BASS_ASIO_Start"); dyn_BASS_ASIO_Stop = (P_BASS_ASIO_Stop)asio.resolve("BASS_ASIO_Stop"); dyn_BASS_ASIO_ChannelEnable = (P_BASS_ASIO_ChannelEnable)asio.resolve("BASS_ASIO_ChannelEnable"); dyn_BASS_ASIO_ChannelJoin = (P_BASS_ASIO_ChannelJoin)asio.resolve("BASS_ASIO_ChannelJoin"); dyn_BASS_ASIO_ChannelSetFormat = (P_BASS_ASIO_ChannelSetFormat)asio.resolve("BASS_ASIO_ChannelSetFormat"); dyn_BASS_ASIO_GetDevice = (P_BASS_ASIO_GetDevice)asio.resolve("BASS_ASIO_GetDevice"); dyn_BASS_ASIO_GetLatency = (P_BASS_ASIO_GetLatency)asio.resolve("BASS_ASIO_GetLatency"); }
    QLibrary dsd("bassdsd"); if (dsd.load()) { dyn_BASS_DSD_StreamCreateFile = (P_BASS_DSD_StreamCreateFile)dsd.resolve("BASS_DSD_StreamCreateFile"); }
    isLoaded = true;
}

static DWORD CALLBACK WasapiProc(void *buffer, DWORD length, void *user) { return static_cast<KaedeAudioWorker*>(user)->processAudioData(buffer, length); }
static DWORD CALLBACK AsioProc(BOOL input, DWORD channel, void *buffer, DWORD length, void *user) { return static_cast<KaedeAudioWorker*>(user)->processAudioData(buffer, length); }
static void CALLBACK SharedDspProc(HDSP handle, DWORD channel, void *buffer, DWORD length, void *user) { static_cast<KaedeAudioWorker*>(user)->processSharedDSP(buffer, length); }

KaedeAudioWorker::KaedeAudioWorker(QObject* parent) : QObject(parent) {
    m_peqConfig = std::make_shared<PeqConfig>(); 
    m_pcmBuffer.resize(8192, 0.0f); m_fftBuffer.resize(1024, 0.0f); 
    m_pcmRing.resize(1048576, 0.0f); 
    m_spscBuffer = std::make_unique<SpscRingBuffer<float>>(4194304); 
    m_firResampler = std::make_unique<KaedePolyphaseResampler>(8, 128); 
    m_workerExtractBuffer.resize(65536, 0.0f);
}

KaedeAudioWorker::~KaedeAudioWorker() {}

void KaedeAudioWorker::initEngine() {
    LoadDynamicBassPlugins(); BASS_Init(0, 44100, 0, 0, nullptr); BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10);
    if (m_outputMode == OutputMode::SharedMixer) { BASS_Init(m_deviceId, 44100, 0, 0, nullptr); } 
    loadBassPlugins(); updateHardwareLatency(); 
    m_analyzerRunning = true; m_analyzerThread = std::thread(&KaedeAudioWorker::analyzerLoop, this);
    m_resamplingRunning = true; m_resamplingThread = std::thread(&KaedeAudioWorker::resamplingLoop, this);
}

void KaedeAudioWorker::destroyEngine() { 
    stopTrack(); 
    m_analyzerRunning = false; m_resamplingRunning = false;
    if (m_analyzerThread.joinable()) m_analyzerThread.join(); 
    if (m_resamplingThread.joinable()) m_resamplingThread.join();
    if (dyn_BASS_ASIO_Free) dyn_BASS_ASIO_Free(); 
    if (dyn_BASS_WASAPI_Free) dyn_BASS_WASAPI_Free(); 
    BASS_Free(); 
}

void KaedeAudioWorker::loadBassPlugins() { 
    QDir dir(QCoreApplication::applicationDirPath()); 
    for (const QString& file : dir.entryList(QStringList() << "bass*.dll", QDir::Files)) { 
        if (file.toLower() == "bass.dll") continue; 
        BASS_PluginLoad(reinterpret_cast<const char*>(dir.absoluteFilePath(file).utf16()), BASS_UNICODE); 
    } 
}

void KaedeAudioWorker::updateHardwareLatency() {
    if (m_outputMode == OutputMode::SharedMixer) { BASS_INFO bInfo; BASS_GetInfo(&bInfo); m_latencyMs.store(bInfo.latency); } 
    else if (m_outputMode == OutputMode::WASAPI_Exclusive && dyn_BASS_WASAPI_GetInfo) { BASS_WASAPI_INFO wInfo; dyn_BASS_WASAPI_GetInfo(&wInfo); m_latencyMs.store(wInfo.buflen * 1000.0); } 
    else if (m_outputMode == OutputMode::ASIO) { 
        if (dyn_BASS_ASIO_GetRate) {
            double rate = dyn_BASS_ASIO_GetRate();
            if (rate > 0.0) {
                if (dyn_BASS_ASIO_GetLatency) { DWORD latSamples = dyn_BASS_ASIO_GetLatency(FALSE); m_latencyMs.store((latSamples / rate) * 1000.0); } 
                else if (dyn_BASS_ASIO_GetInfo) { BASS_ASIO_INFO aInfo; dyn_BASS_ASIO_GetInfo(&aInfo); m_latencyMs.store((aInfo.bufmax / rate) * 1000.0); }
            }
        } 
    }
}

QList<AudioDeviceInfo> KaedeAudioWorker::getDeviceListWorker(OutputMode mode) const {
    QList<AudioDeviceInfo> list;
    if (mode == OutputMode::SharedMixer) { BASS_DEVICEINFO info; for (int i = 1; BASS_GetDeviceInfo(i, &info); i++) { if (info.flags & BASS_DEVICE_ENABLED) list.append({i, QString::fromUtf8(info.name), mode}); } } 
    else if (mode == OutputMode::WASAPI_Exclusive && dyn_BASS_WASAPI_GetDeviceInfo) { BASS_WASAPI_DEVICEINFO info; for (int i = 0; dyn_BASS_WASAPI_GetDeviceInfo(i, &info); i++) { if ((info.flags & BASS_DEVICE_ENABLED) && !(info.flags & BASS_DEVICE_INPUT)) list.append({i, QString::fromUtf8(info.name), mode}); } } 
    else if (mode == OutputMode::ASIO && dyn_BASS_ASIO_GetDeviceInfo) { BASS_ASIO_DEVICEINFO info; for (int i = 0; dyn_BASS_ASIO_GetDeviceInfo(i, &info); i++) { list.append({i, QString::fromUtf8(info.name), mode}); } }
    return list;
}

void KaedeAudioWorker::setOutputDeviceWorker(OutputMode mode, int deviceId) { 
    bool wasPlaying = m_isPlaying; double currentPos = getCurrentPosition(); QString targetFile = m_currentFilePath; 
    destroyEngine(); 
    m_outputMode = mode; m_deviceId = deviceId; 
    m_isHardwareInitialized.store(false); 
    initEngine(); 
    if (!targetFile.isEmpty()) { loadTrack(targetFile); seekTrack(currentPos); if (wasPlaying) playTrack(); } 
}

void KaedeAudioWorker::setDspCoreModeWorker(DspCoreMode mode) {
    if (m_coreMode.load() == mode) return;
    bool wasPlaying = m_isPlaying; double currentPos = getCurrentPosition();
    m_coreMode.store(mode);
    if (!m_currentFilePath.isEmpty()) { loadTrack(m_currentFilePath); seekTrack(currentPos); if (wasPlaying) playTrack(); }
}

void KaedeAudioWorker::setAlienFirConfigWorker(int taps, int targetRate) {
    if (m_targetFirTaps.load() == taps && m_userTargetRate.load() == targetRate) return;
    bool wasPlaying = m_isPlaying; double currentPos = getCurrentPosition();
    m_targetFirTaps.store(taps); m_userTargetRate.store(targetRate);
    if (m_coreMode.load() == DspCoreMode::Alien_FIR_128 && !m_currentFilePath.isEmpty()) {
        loadTrack(m_currentFilePath); seekTrack(currentPos);
        if (wasPlaying) playTrack();
    }
}

bool KaedeAudioWorker::loadTrack(const QString& filePath) {
    if (filePath.isEmpty()) return false; 
    
    m_isSeeking.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    stopTrack(); 
    
    m_isSeeking.store(true);
    std::fill(m_stdNsErrorL, m_stdNsErrorL + 9, 0.0);
    std::fill(m_stdNsErrorR, m_stdNsErrorR + 9, 0.0);
    
    m_currentFilePath = filePath;
    m_smoothedUnplayedMicroSec.store(0);
    
    QString ext = QFileInfo(filePath).suffix().toLower();
    m_isDsdMode.store(ext == "dsf" || ext == "dff");

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) { m_isSeeking.store(false); return false; }
    qint64 fileSize = file.size(); m_ramAudioData.resize(fileSize); file.read(reinterpret_cast<char*>(m_ramAudioData.data()), fileSize); file.close();

    DWORD flags = BASS_SAMPLE_FLOAT; 
    if (m_outputMode != OutputMode::SharedMixer) flags |= BASS_STREAM_DECODE;
    if (m_isLooping && m_outputMode == OutputMode::SharedMixer) flags |= BASS_SAMPLE_LOOP;
    
    if (m_isDsdMode.load() && dyn_BASS_DSD_StreamCreateFile) {
        DWORD dsdFlags = flags; if (m_outputMode != OutputMode::SharedMixer) dsdFlags |= BASS_DSD_DOP; 
        m_stream = dyn_BASS_DSD_StreamCreateFile(TRUE, m_ramAudioData.data(), 0, fileSize, dsdFlags, 0);
        m_shadowStream = dyn_BASS_DSD_StreamCreateFile(TRUE, m_ramAudioData.data(), 0, fileSize, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE, 0);
    } else {
        m_stream = BASS_StreamCreateFile(TRUE, m_ramAudioData.data(), 0, fileSize, flags); 
    }
    if (!m_stream) { m_isSeeking.store(false); return false; }

    m_dspHandle = BASS_ChannelSetDSP(m_stream, SharedDspProc, this, 0);

    BASS_CHANNELINFO info; BASS_ChannelGetInfo(m_stream, &info); 
    m_baseSampleRate.store(info.freq); m_channels.store(info.chans); 

    int targetRate = info.freq;
    
    if (!m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
        int requestedRate = m_userTargetRate.load();
        int alignedFactor = 1;

        if (requestedRate == 0) { 
            alignedFactor = 1;
        } else if (requestedRate == -1) { 
            int maxDacRate = 768000; 
            while (info.freq * alignedFactor * 2 <= maxDacRate && alignedFactor < 32) alignedFactor *= 2; 
        } else { 
            while (info.freq * alignedFactor * 2 <= requestedRate && alignedFactor < 32) alignedFactor *= 2;
        }
        
        targetRate = info.freq * alignedFactor; 
        
        size_t safeBufferSize = static_cast<size_t>(targetRate * m_channels.load() * 3.0);
        if (safeBufferSize < 1048576) safeBufferSize = 1048576; 
        
        std::lock_guard<std::mutex> lock(m_resamplerMutex);
        m_spscBuffer = std::make_unique<SpscRingBuffer<float>>(safeBufferSize);
        m_firResampler = std::make_unique<KaedePolyphaseResampler>(alignedFactor, m_targetFirTaps.load()); 
    }
    
    m_dacSampleRate.store(targetRate);

    bool needHardwareReinit = (!m_isHardwareInitialized.load()) || (targetRate != m_lastDacSampleRate.load());

    if (needHardwareReinit) {
        if (m_outputMode == OutputMode::WASAPI_Exclusive) { 
            if (dyn_BASS_WASAPI_Free) { dyn_BASS_WASAPI_Free(); std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
            if (dyn_BASS_WASAPI_Init) { 
                BOOL ok = dyn_BASS_WASAPI_Init(m_deviceId, targetRate, info.chans, BASS_WASAPI_EXCLUSIVE | BASS_WASAPI_EVENT | 0x400000 | BASS_WASAPI_BUFFER, 0.1f, 0.0f, WasapiProc, this); 
                if (!ok) dyn_BASS_WASAPI_Init(m_deviceId, targetRate, info.chans, BASS_WASAPI_EXCLUSIVE | BASS_WASAPI_AUTOFORMAT | BASS_WASAPI_EVENT | 0x400000 | BASS_WASAPI_BUFFER, 0.1f, 0.0f, WasapiProc, this); 
            } 
            if (dyn_BASS_WASAPI_Start) dyn_BASS_WASAPI_Start(); 
        } 
        else if (m_outputMode == OutputMode::ASIO) { 
            if (dyn_BASS_ASIO_Free) { dyn_BASS_ASIO_Free(); std::this_thread::sleep_for(std::chrono::milliseconds(400)); } 
            if (dyn_BASS_ASIO_Init) { 
                dyn_BASS_ASIO_Init(m_deviceId, BASS_ASIO_THREAD); 
                if (dyn_BASS_ASIO_ChannelEnable) dyn_BASS_ASIO_ChannelEnable(FALSE, 0, AsioProc, this);
                if (dyn_BASS_ASIO_ChannelJoin) dyn_BASS_ASIO_ChannelJoin(FALSE, 1, 0);
                if (dyn_BASS_ASIO_ChannelSetFormat) dyn_BASS_ASIO_ChannelSetFormat(FALSE, 0, BASS_ASIO_FORMAT_FLOAT); 
                if (dyn_BASS_ASIO_SetRate) dyn_BASS_ASIO_SetRate(targetRate);
            } 
            if (dyn_BASS_ASIO_Start) dyn_BASS_ASIO_Start(0, 0); 
        }
        m_lastDacSampleRate.store(targetRate);
        m_isHardwareInitialized.store(true);
    }

    if (m_outputMode == OutputMode::SharedMixer && !m_isDsdMode.load()) {
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, static_cast<float>(m_volume64.load())); 
    }
    updateHardwareLatency(); 
    m_duration = BASS_ChannelBytes2Seconds(m_stream, BASS_ChannelGetLength(m_stream, BASS_POS_BYTE)); 
    
    m_cfStateL = 0.0; m_cfStateR = 0.0;
    
    if (m_spscBuffer) m_spscBuffer->reset(); 
    if (m_firResampler) m_firResampler->reset();
    
    if (!m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
        m_isBuffering.store(true);
        QMetaObject::invokeMethod(this, "emitBufferingStart", Qt::QueuedConnection);
    }
    
    m_isSeeking.store(false); 
    return true;
}

void KaedeAudioWorker::resamplingLoop() {
#ifdef _WIN32
    DWORD taskIndex = 0; HANDLE hTask = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
#endif

    while (m_resamplingRunning.load()) {
        if (m_isSeeking.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

        if (m_stream && !m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
            int factor = 1; int channels = 2; int targetRate = m_dacSampleRate.load();
            {
                std::lock_guard<std::mutex> lock(m_resamplerMutex);
                if (m_firResampler) { factor = m_firResampler->getFactor(); channels = m_channels.load(); }
            }
            
            size_t floatsNeededToSafelyWrite = 4096 * factor * channels;
            
            if (m_spscBuffer->write_available() >= floatsNeededToSafelyWrite) {
                int bytesToRead = 4096 * channels * sizeof(float);
                DWORD readBytes = BASS_ChannelGetData(m_stream, m_workerExtractBuffer.data(), bytesToRead);
                
                if (readBytes != (DWORD)-1 && readBytes > 0) {
                    int framesRead = readBytes / (channels * sizeof(float));
                    std::vector<float> upsampledData;
                    bool runNS = m_noiseShapingEnabled.load(); 
                    
                    {
                        std::lock_guard<std::mutex> lock(m_resamplerMutex);
                        // 👑 將 Target Rate 傳入核心，引爆動態矩陣
                        if (m_firResampler) m_firResampler->process(m_workerExtractBuffer.data(), framesRead, channels, upsampledData, m_ditherSeed1, m_ditherSeed2, runNS, targetRate);
                    }
                    if (!upsampledData.empty()) m_spscBuffer->write(upsampledData.data(), upsampledData.size());
                } else if (readBytes == (DWORD)-1 || readBytes == 0) {
                    if (m_isLooping) BASS_ChannelSetPosition(m_stream, 0, BASS_POS_BYTE); 
                    else std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
                }
            } else { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        } else { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    }

#ifdef _WIN32
    if (hTask) AvRevertMmThreadCharacteristics(hTask);
#endif
}

DWORD KaedeAudioWorker::processAudioData(void *buffer, DWORD length) {
    if (!m_stream || !m_isPlaying.load()) {
        std::memset(buffer, 0, length);
        return length; 
    }
    
    if (!m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
        float* fBuf = static_cast<float*>(buffer);
        size_t floatsNeeded = length / sizeof(float);
        
        if (m_isBuffering.load()) {
            std::memset(buffer, 0, length); 
            
            size_t requiredSafeMargin = static_cast<size_t>(m_dacSampleRate.load() * m_channels.load() * 0.8);
            if (m_spscBuffer->read_available() >= requiredSafeMargin || m_duration < 1.0) {
                m_isBuffering.store(false);
                QMetaObject::invokeMethod(this, "emitBufferingDone", Qt::QueuedConnection);
            }
            return length;
        }

        size_t readCount = 0;
        if (m_spscBuffer) readCount = m_spscBuffer->read(fBuf, floatsNeeded);
        if (readCount < floatsNeeded) std::fill(fBuf + readCount, fBuf + floatsNeeded, 0.0f); 
        return length; 
    }
    
    DWORD read = BASS_ChannelGetData(m_stream, buffer, length);
    if (read == (DWORD)-1 || read == 0) { 
        if (m_isLooping) { BASS_ChannelSetPosition(m_stream, 0, BASS_POS_BYTE); read = BASS_ChannelGetData(m_stream, buffer, length); } 
        else { 
            std::memset(buffer, 0, length);
            return length; 
        }
    }
    return read; 
}

void KaedeAudioWorker::updatePeqConfigWorker(bool masterEnabled, const std::vector<DspBiquad>& filters, bool bypassed, double preampLinear, double wetRatio) {
    auto newConfig = std::make_shared<PeqConfig>();
    newConfig->masterEnabled = masterEnabled; newConfig->filters = filters; newConfig->bypassed = bypassed; newConfig->preampLinear = preampLinear; newConfig->wetRatio = wetRatio; 
    std::atomic_store(&m_peqConfig, newConfig);
}

void KaedeAudioWorker::setCrossfeedWorker(bool enabled, double level, double cutoffFreq) { m_crossfeedEnabled.store(enabled); m_crossfeedLevel.store(std::clamp(level, 0.0, 1.0)); m_crossfeedCutoff.store(std::clamp(cutoffFreq, 100.0, 3000.0)); }

void KaedeAudioWorker::processSharedDSP(void *buffer, DWORD length) {
    if (!m_stream) return;
    if (m_isDsdMode.load() && m_outputMode != OutputMode::SharedMixer) return;

    float* fBuf = static_cast<float*>(buffer); int floatCount = length / sizeof(float);
    int chans = m_channels.load(); 
    double vol64 = (m_outputMode == OutputMode::SharedMixer) ? 1.0 : m_volume64.load();
    std::shared_ptr<PeqConfig> currentConfig = std::atomic_load(&m_peqConfig);
    
    bool masterBypass = !currentConfig || !currentConfig->masterEnabled;
    bool runEq = !masterBypass && !currentConfig->bypassed && !currentConfig->filters.empty();
    double preamp = runEq ? currentConfig->preampLinear : 1.0;
    double wetRatio = masterBypass ? 0.0 : currentConfig->wetRatio; 

    if (runEq && m_peqStates.size() != currentConfig->filters.size()) { m_peqStates.assign(currentConfig->filters.size(), PeqState()); }

    bool runCrossfeed = m_crossfeedEnabled.load() && chans == 2;
    double cfLevel = 0.0, cfAlpha = 0.0;
    if (runCrossfeed) { cfLevel = m_crossfeedLevel.load(); double dt = 1.0 / static_cast<double>(m_baseSampleRate.load()); double rc = 1.0 / (2.0 * M_PI * m_crossfeedCutoff.load()); cfAlpha = dt / (rc + dt); }

    int currentIdx = m_ringIndex.load(std::memory_order_relaxed);
    const double LSB24 = 1.1920928955078125e-07;

    for(int i = 0; i < floatCount; i += chans) {
        double rawL = static_cast<double>(fBuf[i]) * vol64;
        double rawR = (chans > 1) ? static_cast<double>(fBuf[i+1]) * vol64 : rawL;

        if (runCrossfeed) {
            m_cfStateL += cfAlpha * (rawL - m_cfStateL); m_cfStateR += cfAlpha * (rawR - m_cfStateR);
            double tmpL = rawL * (1.0 - cfLevel) + m_cfStateR * cfLevel; double tmpR = rawR * (1.0 - cfLevel) + m_cfStateL * cfLevel;
            rawL = tmpL; rawR = tmpR;
        }

        double dryL = rawL; double dryR = rawR;
        double wetL = dryL * preamp; double wetR = dryR * preamp;

        if (runEq) {
            for (size_t f = 0; f < currentConfig->filters.size(); ++f) {
                const auto& c = currentConfig->filters[f]; auto& s = m_peqStates[f];
                double outL = c.b0*wetL + c.b1*s.x1[0] + c.b2*s.x2[0] - c.a1*s.y1[0] - c.a2*s.y2[0];
                s.x2[0] = s.x1[0]; s.x1[0] = wetL; s.y2[0] = s.y1[0]; s.y1[0] = outL; wetL = outL;
                if (chans > 1) {
                    double outR = c.b0*wetR + c.b1*s.x1[1] + c.b2*s.x2[1] - c.a1*s.y1[1] - c.a2*s.y2[1];
                    s.x2[1] = s.x1[1]; s.x1[1] = wetR; s.y2[1] = s.y1[1]; s.y1[1] = outR; wetR = outR;
                }
            }
        }
        
        double dL = dryL * (1.0 - wetRatio) + wetL * wetRatio;
        double dR = dryR * (1.0 - wetRatio) + wetR * wetRatio;
        
        // 👑 Standard_64 (44.1k/48k 等原生直通) 專屬 9階 噪聲整形
        if (m_coreMode.load() == DspCoreMode::Standard_64) {
            bool runNS = m_noiseShapingEnabled.load();
            int targetRate = m_baseSampleRate.load();
            
            // 獨立的頻率判斷矩陣
            int order = 2; const double* nsCoeffs = nullptr;
            static const double c2[2] = {2.0, -1.0}; 
            static const double c5[5] = {2.24, -2.39, 1.83, -0.81, 0.17}; 
            static const double c9[9] = {2.412, -2.970, 2.738, -2.033, 1.492, -0.890, 0.441, -0.164, 0.034}; 
            
            if (targetRate >= 700000) { order = 2; nsCoeffs = c2; } 
            else if (targetRate >= 350000) { order = 5; nsCoeffs = c5; } 
            else { order = 9; nsCoeffs = c9; }
            
            // --- Left Channel ---
            double r1L = static_cast<double>(xorshift32(m_ditherSeed1)) / 4294967295.0; 
            double r2L = static_cast<double>(xorshift32(m_ditherSeed2)) / 4294967295.0; 
            double ditherL = (r1L - r2L) * LSB24;
            
            double shaped_errorL = 0.0;
            if (runNS) { for(int k = 0; k < order; ++k) shaped_errorL += m_stdNsErrorL[k] * nsCoeffs[k]; }
            double ditheredL = dL + ditherL + shaped_errorL;
            
            float quantL = static_cast<float>(std::clamp(ditheredL, -1.0, 1.0));
            
            if (runNS) {
                double errL = ditheredL - static_cast<double>(quantL);
                if (std::isnan(errL) || std::isinf(errL) || std::abs(errL) > 1.0) { std::fill(m_stdNsErrorL, m_stdNsErrorL + 9, 0.0); }
                else { for(int k = order - 1; k > 0; --k) m_stdNsErrorL[k] = m_stdNsErrorL[k-1]; m_stdNsErrorL[0] = errL; }
            } else { std::fill(m_stdNsErrorL, m_stdNsErrorL + 9, 0.0); }
            dL = quantL;
            
            // --- Right Channel ---
            if (chans > 1) { 
                double r1R = static_cast<double>(xorshift32(m_ditherSeed1)) / 4294967295.0; 
                double r2R = static_cast<double>(xorshift32(m_ditherSeed2)) / 4294967295.0; 
                double ditherR = (r1R - r2R) * LSB24;
                
                double shaped_errorR = 0.0;
                if (runNS) { for(int k = 0; k < order; ++k) shaped_errorR += m_stdNsErrorR[k] * nsCoeffs[k]; }
                double ditheredR = dR + ditherR + shaped_errorR;
                
                float quantR = static_cast<float>(std::clamp(ditheredR, -1.0, 1.0));
                
                if (runNS) {
                    double errR = ditheredR - static_cast<double>(quantR);
                    if (std::isnan(errR) || std::isinf(errR) || std::abs(errR) > 1.0) { std::fill(m_stdNsErrorR, m_stdNsErrorR + 9, 0.0); }
                    else { for(int k = order - 1; k > 0; --k) m_stdNsErrorR[k] = m_stdNsErrorR[k-1]; m_stdNsErrorR[0] = errR; }
                } else { std::fill(m_stdNsErrorR, m_stdNsErrorR + 9, 0.0); }
                dR = quantR;
            }
        } else {
            dL = std::clamp(dL, -1.0, 1.0); 
            dR = std::clamp(dR, -1.0, 1.0);
        }

        fBuf[i] = static_cast<float>(dL); m_pcmRing[currentIdx] = fBuf[i]; currentIdx = (currentIdx + 1) % 1048576;
        if (chans > 1) { fBuf[i+1] = static_cast<float>(dR); m_pcmRing[currentIdx] = fBuf[i+1]; currentIdx = (currentIdx + 1) % 1048576; }
    }
    m_ringIndex.store(currentIdx, std::memory_order_release);
}

void KaedeAudioWorker::playTrack() { 
    if (!m_stream || m_isPlaying) return; 
    
    if (m_outputMode == OutputMode::SharedMixer) BASS_ChannelPlay(m_stream, FALSE); 
    
    m_isPlaying.store(true); 
    emit playbackStateChanged(true); 
}

void KaedeAudioWorker::pauseTrack() { 
    if (!m_stream || !m_isPlaying) return; 
    
    if (m_outputMode == OutputMode::SharedMixer) BASS_ChannelPause(m_stream); 
    
    m_isPlaying.store(false); 
    emit playbackStateChanged(false); 
}

void KaedeAudioWorker::stopTrack() { 
    if (!m_stream) return; 
    
    m_isSeeking.store(true); 
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    std::lock_guard<std::mutex> lock(m_resamplerMutex);
    
    if (m_outputMode == OutputMode::SharedMixer) BASS_ChannelStop(m_stream); 
    
    if (m_dspHandle) { BASS_ChannelRemoveDSP(m_stream, m_dspHandle); m_dspHandle = 0; } 
    if (m_shadowStream) { BASS_StreamFree(m_shadowStream); m_shadowStream = 0; } 
    BASS_StreamFree(m_stream); m_stream = 0; 
    
    m_ramAudioData.clear(); m_ramAudioData.shrink_to_fit();
    m_isPlaying.store(false); m_duration = 0.0; std::fill(m_pcmRing.begin(), m_pcmRing.end(), 0.0f); m_ringIndex.store(0); std::vector<float> emptyPcm(8192, 0.0f); std::vector<float> emptyFft(1024, -60.0f); emit dspDataReady(emptyPcm, emptyFft); emit playbackStateChanged(false); emit positionChanged(0.0, 0.0); 
    
    m_isBuffering.store(false); 
    QMetaObject::invokeMethod(this, "emitBufferingDone", Qt::QueuedConnection);
    m_smoothedUnplayedMicroSec.store(0); 
    
    std::fill(m_stdNsErrorL, m_stdNsErrorL + 9, 0.0);
    std::fill(m_stdNsErrorR, m_stdNsErrorR + 9, 0.0);
    
    m_isSeeking.store(false); 
}

void KaedeAudioWorker::seekTrack(double targetSeconds) { 
    if (!m_stream) return; 
    
    m_isSeeking.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    std::lock_guard<std::mutex> lock(m_resamplerMutex);
    
    targetSeconds = qBound(0.0, targetSeconds, m_duration); 
    BASS_ChannelSetPosition(m_stream, BASS_ChannelSeconds2Bytes(m_stream, targetSeconds), BASS_POS_BYTE); 
    if (m_shadowStream) BASS_ChannelSetPosition(m_shadowStream, BASS_ChannelSeconds2Bytes(m_shadowStream, targetSeconds), BASS_POS_BYTE); 
    std::fill(m_pcmRing.begin(), m_pcmRing.end(), 0.0f); m_ringIndex.store(0); 
    if (m_spscBuffer) m_spscBuffer->reset(); 
    if (m_firResampler) m_firResampler->reset();
    
    m_smoothedUnplayedMicroSec.store(0); 
    
    std::fill(m_stdNsErrorL, m_stdNsErrorL + 9, 0.0);
    std::fill(m_stdNsErrorR, m_stdNsErrorR + 9, 0.0);
    
    if (!m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
        m_isBuffering.store(true);
        QMetaObject::invokeMethod(this, "emitBufferingStart", Qt::QueuedConnection);
    }
    
    m_isSeeking.store(false); 
    emit positionChanged(targetSeconds, m_duration); 
}

void KaedeAudioWorker::setVolume(double vol) { m_volume64.store(std::clamp(vol, 0.0, 1.0)); if (m_stream && m_outputMode == OutputMode::SharedMixer) BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, static_cast<float>(m_volume64.load())); }
void KaedeAudioWorker::setLooping(bool loop) { m_isLooping = loop; if (m_stream && m_outputMode == OutputMode::SharedMixer) BASS_ChannelFlags(m_stream, loop ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP); }

double KaedeAudioWorker::getCurrentPosition() const { 
    if (!m_stream) return 0.0;
    double pos = BASS_ChannelBytes2Seconds(m_stream, BASS_ChannelGetPosition(m_stream, BASS_POS_BYTE)); 
    if (!m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
        size_t unplayedFloats = 0;
        if (m_spscBuffer) unplayedFloats = m_spscBuffer->read_available();
        double currentUnplayed = static_cast<double>(unplayedFloats) / (m_dacSampleRate.load() * m_channels.load());
        
        double smoothedUnplayed = m_smoothedUnplayedMicroSec.load() / 1000000.0;
        if (smoothedUnplayed == 0.0) smoothedUnplayed = currentUnplayed; 
        
        double firGroupDelaySeconds = (m_targetFirTaps.load() / 2.0) / m_baseSampleRate.load();
        pos -= (smoothedUnplayed + firGroupDelaySeconds);
        if (pos < 0.0) pos = 0.0;
    }
    return pos;
}

double KaedeAudioWorker::getDurationWorker() const { return m_duration; }
void KaedeAudioWorker::computeCustomFFT(const std::vector<float>& pcmInput, std::vector<float>& fftOutput) { int N = 2048; int totalSamples = pcmInput.size() / 2; if (totalSamples < N) return; int offset = (totalSamples - N) * 2; std::vector<std::complex<float>> x(N); for (int i = 0; i < N; ++i) { float mono = (pcmInput[offset + i * 2] + pcmInput[offset + i * 2 + 1]) * 0.5f; float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (N - 1))); x[i] = std::complex<float>(mono * window, 0.0f); } int j = 0; for (int i = 1; i < N - 1; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(x[i], x[j]); } for (int len = 2; len <= N; len <<= 1) { float angle = -2.0f * M_PI / len; std::complex<float> wlen(std::cos(angle), std::sin(angle)); for (int i = 0; i < N; i += len) { std::complex<float> w(1.0f, 0.0f); for (int k = 0; k < len / 2; ++k) { std::complex<float> u = x[i + k]; std::complex<float> v = x[i + k + len / 2] * w; x[i + k] = u + v; x[i + k + len / 2] = u - v; w *= wlen; } } } for (int i = 0; i < N / 2; ++i) fftOutput[i] = (std::abs(x[i]) / (N / 2)) * 1.5f; }

void KaedeAudioWorker::analyzerLoop() {
#ifdef _WIN32
    DWORD taskIndex = 0; HANDLE hTask = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
#endif
    while (m_analyzerRunning.load()) {
        auto start = std::chrono::steady_clock::now();
        if (m_stream && m_isPlaying.load()) {
            double currSec = getCurrentPosition(); emit positionChanged(currSec, m_duration); if (currSec >= m_duration - 0.05 && !m_isLooping) { emit trackFinished(); std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
            if (m_isDsdMode.load() && m_shadowStream) {
                QWORD shadowPos = BASS_ChannelSeconds2Bytes(m_shadowStream, currSec); BASS_ChannelSetPosition(m_shadowStream, shadowPos, BASS_POS_BYTE); DWORD read = BASS_ChannelGetData(m_shadowStream, m_pcmBuffer.data(), 8192 * sizeof(float)); if (read != (DWORD)-1 && read > 0) { computeCustomFFT(m_pcmBuffer, m_fftBuffer); emit dspDataReady(m_pcmBuffer, m_fftBuffer); }
            } else {
                long long latencyFloats = 0;
                if (m_outputMode == OutputMode::SharedMixer) { DWORD availBytes = BASS_ChannelGetData(m_stream, nullptr, BASS_DATA_AVAILABLE); if (availBytes != (DWORD)-1) latencyFloats = availBytes / sizeof(float); } 
                else if (m_outputMode == OutputMode::WASAPI_Exclusive) { if (dyn_BASS_WASAPI_GetData) { DWORD availBytes = dyn_BASS_WASAPI_GetData(nullptr, BASS_DATA_AVAILABLE); if (availBytes != (DWORD)-1) latencyFloats = availBytes / sizeof(float); } } 
                else if (m_outputMode == OutputMode::ASIO) { if (dyn_BASS_ASIO_GetLatency && dyn_BASS_ASIO_GetRate) { DWORD latSamples = dyn_BASS_ASIO_GetLatency(FALSE); double asioRate = dyn_BASS_ASIO_GetRate(); if (asioRate > 0) { double latSec = latSamples / asioRate; latencyFloats = static_cast<long long>(latSec * m_baseSampleRate.load()) * 2; } } }
                
                if (!m_isDsdMode.load() && m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) {
                    size_t unplayedFloats = 0;
                    if (m_spscBuffer) unplayedFloats = m_spscBuffer->read_available();
                    double currentUnplayed = static_cast<double>(unplayedFloats) / (m_dacSampleRate.load() * m_channels.load());
                    
                    double lastUnplayed = m_smoothedUnplayedMicroSec.load() / 1000000.0;
                    if (lastUnplayed == 0.0 || std::abs(currentUnplayed - lastUnplayed) > 0.3) {
                        lastUnplayed = currentUnplayed; 
                    } else {
                        lastUnplayed += 0.08 * (currentUnplayed - lastUnplayed); 
                    }
                    m_smoothedUnplayedMicroSec.store(static_cast<int64_t>(lastUnplayed * 1000000.0));

                    double firGroupDelaySeconds = (m_targetFirTaps.load() / 2.0) / m_baseSampleRate.load();
                    latencyFloats += static_cast<long long>((lastUnplayed + firGroupDelaySeconds) * m_baseSampleRate.load() * m_channels.load());
                }

                int chans = m_channels.load();
                if (chans > 0) {
                    latencyFloats -= (latencyFloats % chans);
                }

                long long writeHead = m_ringIndex.load(std::memory_order_acquire); 
                long long ringSize = 1048576; 
                long long size = 8192; 
                
                if (latencyFloats < 0) latencyFloats = 0; 
                if (latencyFloats > ringSize - size) latencyFloats = ringSize - size;
                
                long long readHead = ((writeHead - latencyFloats) % ringSize + ringSize) % ringSize; 
                long long startIdx = ((readHead - size) % ringSize + ringSize) % ringSize; 
                int firstPart = static_cast<int>(ringSize - startIdx);
                
                if (firstPart >= size) { 
                    std::memcpy(m_pcmBuffer.data(), m_pcmRing.data() + startIdx, size * sizeof(float)); 
                } else { 
                    std::memcpy(m_pcmBuffer.data(), m_pcmRing.data() + startIdx, firstPart * sizeof(float)); 
                    std::memcpy(m_pcmBuffer.data() + firstPart, m_pcmRing.data(), (size - firstPart) * sizeof(float)); 
                }
                
                computeCustomFFT(m_pcmBuffer, m_fftBuffer); emit dspDataReady(m_pcmBuffer, m_fftBuffer);
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(); if (elapsed < 16) std::this_thread::sleep_for(std::chrono::milliseconds(16 - elapsed));
    }
#ifdef _WIN32
    if (hTask) AvRevertMmThreadCharacteristics(hTask);
#endif
}

PipelineInfo KaedeAudioWorker::getPipelineInfoWorker() const {
    PipelineInfo info;
    if (m_outputMode == OutputMode::SharedMixer) { info.apiMode = "WASAPI SHARED MIXER"; info.latency = QString("%1 ms").arg(static_cast<int>(m_latencyMs.load())); BASS_DEVICEINFO dInfo; BASS_GetDeviceInfo(BASS_GetDevice(), &dInfo); info.deviceName = QString::fromUtf8(dInfo.name); } 
    else if (m_outputMode == OutputMode::WASAPI_Exclusive) { info.apiMode = "WASAPI EVENT-DRIVEN"; if (dyn_BASS_WASAPI_GetInfo && dyn_BASS_WASAPI_GetDevice && dyn_BASS_WASAPI_GetDeviceInfo) { info.latency = QString("%1 ms").arg(static_cast<int>(m_latencyMs.load())); BASS_WASAPI_DEVICEINFO dInfo; dyn_BASS_WASAPI_GetDeviceInfo(dyn_BASS_WASAPI_GetDevice(), &dInfo); info.deviceName = QString::fromUtf8(dInfo.name); if (m_stream) { BASS_WASAPI_INFO wInfo; dyn_BASS_WASAPI_GetInfo(&wInfo); info.hasSrc = (m_dacSampleRate.load() != wInfo.freq); } } else { info.latency = "N/A"; info.deviceName = "WASAPI DLL MISSING"; } } 
    else if (m_outputMode == OutputMode::ASIO) { info.apiMode = "ASIO DIRECT BITSTREAM"; if (dyn_BASS_ASIO_GetInfo && dyn_BASS_ASIO_GetDevice && dyn_BASS_ASIO_GetDeviceInfo) { info.latency = QString("%1 ms").arg(static_cast<int>(m_latencyMs.load())); BASS_ASIO_DEVICEINFO dInfo; dyn_BASS_ASIO_GetDeviceInfo(dyn_BASS_ASIO_GetDevice(), &dInfo); info.deviceName = QString::fromUtf8(dInfo.name); if (m_stream && dyn_BASS_ASIO_GetRate) { info.hasSrc = (m_baseSampleRate.load() != dyn_BASS_ASIO_GetRate()); } } else { info.latency = "N/A"; info.deviceName = "ASIO DLL MISSING"; } }
    
    if (m_stream) { 
        if (m_isDsdMode.load() && m_outputMode != OutputMode::SharedMixer) { info.formatSpec = QString("%1Hz | DSD over PCM [DoP] (HW BYPASS)").arg(m_baseSampleRate.load()); info.hasSrc = false; } 
        else if (m_coreMode.load() == DspCoreMode::Alien_FIR_128 && m_outputMode != OutputMode::SharedMixer) { info.formatSpec = QString("%1Hz -> %2Hz | ALIEN-FIR %3 (SINC POLYPHASE)").arg(m_baseSampleRate.load()).arg(m_dacSampleRate.load()).arg(m_targetFirTaps.load()); }
        else { info.formatSpec = QString("%1Hz | STD-64 DOUBLE PRECISION IIR").arg(m_baseSampleRate.load()); }
        if (m_outputMode == OutputMode::SharedMixer) { BASS_INFO bInfo; BASS_GetInfo(&bInfo); info.hasSrc = (m_baseSampleRate.load() != bInfo.freq); } 
    } else { info.formatSpec = "IDLE"; info.hasSrc = false; } 
    return info;
}

KaedeAudioEngine::KaedeAudioEngine(QObject* parent) : QObject(parent) {
    qRegisterMetaType<PipelineInfo>("PipelineInfo"); qRegisterMetaType<std::vector<float>>("std::vector<float>"); qRegisterMetaType<std::vector<DspBiquad>>("std::vector<DspBiquad>");
    m_audioThread = new QThread(this); m_worker = new KaedeAudioWorker(); m_worker->moveToThread(m_audioThread);
    
    connect(m_worker, &KaedeAudioWorker::playbackStateChanged, this, [this](bool playing){ m_isPlaying.store(playing); emit playbackStateChanged(playing); });
    connect(m_worker, &KaedeAudioWorker::positionChanged, this, &KaedeAudioEngine::positionChanged); 
    connect(m_worker, &KaedeAudioWorker::trackFinished, this, &KaedeAudioEngine::trackFinished); 
    connect(m_worker, &KaedeAudioWorker::dspDataReady, this, &KaedeAudioEngine::dspDataReady);
    connect(m_worker, &KaedeAudioWorker::bufferingStateChanged, this, &KaedeAudioEngine::bufferingStateChanged); 
    
    m_audioThread->start(QThread::TimeCriticalPriority);
}
KaedeAudioEngine::~KaedeAudioEngine() { destroy(); }
void KaedeAudioEngine::init() { QMetaObject::invokeMethod(m_worker, "initEngine", Qt::BlockingQueuedConnection); }
void KaedeAudioEngine::destroy() { if (m_audioThread) { QMetaObject::invokeMethod(m_worker, "destroyEngine", Qt::BlockingQueuedConnection); m_audioThread->quit(); m_audioThread->wait(); delete m_worker; m_worker = nullptr; delete m_audioThread; m_audioThread = nullptr; } }
QList<AudioDeviceInfo> KaedeAudioEngine::getDeviceList(OutputMode mode) const { QList<AudioDeviceInfo> list; QMetaObject::invokeMethod(m_worker, "getDeviceListWorker", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QList<AudioDeviceInfo>, list), Q_ARG(OutputMode, mode)); return list; }
void KaedeAudioEngine::setOutputDevice(OutputMode mode, int deviceId) { QMetaObject::invokeMethod(m_worker, "setOutputDeviceWorker", Qt::BlockingQueuedConnection, Q_ARG(OutputMode, mode), Q_ARG(int, deviceId)); }
void KaedeAudioEngine::setDspCoreMode(DspCoreMode mode) { QMetaObject::invokeMethod(m_worker, "setDspCoreModeWorker", Qt::BlockingQueuedConnection, Q_ARG(DspCoreMode, mode)); }
void KaedeAudioEngine::setAlienFirConfig(int taps, int targetRate) { QMetaObject::invokeMethod(m_worker, "setAlienFirConfigWorker", Qt::BlockingQueuedConnection, Q_ARG(int, taps), Q_ARG(int, targetRate)); }
void KaedeAudioEngine::setNoiseShaping(bool enabled) { QMetaObject::invokeMethod(m_worker, "setNoiseShapingWorker", Qt::QueuedConnection, Q_ARG(bool, enabled)); }

bool KaedeAudioEngine::load(const QString& filePath) { bool success = false; QMetaObject::invokeMethod(m_worker, "loadTrack", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(QString, filePath)); return success; }
void KaedeAudioEngine::play() { QMetaObject::invokeMethod(m_worker, "playTrack", Qt::QueuedConnection); }
void KaedeAudioEngine::pause() { QMetaObject::invokeMethod(m_worker, "pauseTrack", Qt::QueuedConnection); }
void KaedeAudioEngine::stop() { QMetaObject::invokeMethod(m_worker, "stopTrack", Qt::QueuedConnection); }
void KaedeAudioEngine::seek(double targetSeconds) { QMetaObject::invokeMethod(m_worker, "seekTrack", Qt::QueuedConnection, Q_ARG(double, targetSeconds)); }
void KaedeAudioEngine::setVolume(double volume) { QMetaObject::invokeMethod(m_worker, "setVolume", Qt::QueuedConnection, Q_ARG(double, volume)); }
void KaedeAudioEngine::setLooping(bool loop) { QMetaObject::invokeMethod(m_worker, "setLooping", Qt::QueuedConnection, Q_ARG(bool, loop)); }
double KaedeAudioEngine::getDuration() const { double dur = 0.0; QMetaObject::invokeMethod(m_worker, "getDurationWorker", Qt::BlockingQueuedConnection, Q_RETURN_ARG(double, dur)); return dur; }
PipelineInfo KaedeAudioEngine::getPipelineInfo() const { PipelineInfo info; QMetaObject::invokeMethod(m_worker, "getPipelineInfoWorker", Qt::BlockingQueuedConnection, Q_RETURN_ARG(PipelineInfo, info)); return info; }
void KaedeAudioEngine::updatePeqConfig(bool masterEnabled, const std::vector<DspBiquad>& filters, bool bypassed, double preampLinear, double wetRatio) { QMetaObject::invokeMethod(m_worker, "updatePeqConfigWorker", Qt::QueuedConnection, Q_ARG(bool, masterEnabled), Q_ARG(std::vector<DspBiquad>, filters), Q_ARG(bool, bypassed), Q_ARG(double, preampLinear), Q_ARG(double, wetRatio)); }
void KaedeAudioEngine::setCrossfeed(bool enabled, double level, double cutoffFreq) { QMetaObject::invokeMethod(m_worker, "setCrossfeedWorker", Qt::QueuedConnection, Q_ARG(bool, enabled), Q_ARG(double, level), Q_ARG(double, cutoffFreq)); }