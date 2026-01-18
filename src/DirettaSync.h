/**
 * @file DirettaSync.h
 * @brief Unified Diretta sync implementation - SDK 148 compatible
 *
 * Based on MPD Diretta Output Plugin v0.4.0
 * Modified for SDK 148 diretta_stream buffer management
 */

#pragma once

#include "DirettaRingBuffer.h"  // Needed for member variable
#include <Host/Diretta/SyncBuffer>
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>
#include <iostream>

// Forward declarations for other dependencies
class DirettaCycleCalculator;
struct DirettaConfig;
struct AudioFormat;

extern bool g_verbose;

#define DIRETTA_LOG(msg) \
    do { if (g_verbose) { std::cout << "[DirettaSync] " << msg << std::endl; } } while(0)

//=============================================================================
// DirettaSync - Main Class
//=============================================================================

class DirettaSync : public DIRETTA::SyncBuffer {
public:
    DirettaSync();
    ~DirettaSync();

    // Initialization
    bool enable(const DirettaConfig& config);
    void disable();
    bool isEnabled() const { return m_enabled; }

    // Format handling
    bool open(const AudioFormat& format);
    void close();
    bool isOpen() const { return m_open; }

    // Playback control
    bool startPlayback();
    void stopPlayback(bool immediate = false);
    void pausePlayback();
    void resumePlayback();
    bool isPlaying() const { return m_playing && !m_paused; }

    // Audio data
    size_t sendAudio(const uint8_t* data, size_t bytes);
    void drain();
    void flush();

    // Status
    size_t getAvailableSpace() const;
    bool canAcceptData() const;
    int getMTU() const { return m_effectiveMTU; }

protected:
    //=========================================================================
    // DIRETTA::Sync Overrides
    //=========================================================================
    bool getNewStream(diretta_stream& stream) override;
    bool getNewStreamCmp() override { return true; }
    bool startSyncWorker() override;
    void statusUpdate() override {}

private:
    //=========================================================================
    // Internal Methods
    //=========================================================================
    
    // Discovery & Connection
    bool discoverTarget();
    bool measureMTU();
    bool openSyncConnection();
    
    // Configuration
    void configureRingPCM(int rate, int channels, int direttaBps, int inputBps);
    void configureRingDSD(uint32_t byteRate, int channels);
    void fullReset();
    
    // Reconfiguration support
    void beginReconfigure();
    void endReconfigure();
    
    //=========================================================================
    // State
    //=========================================================================
    
    bool m_enabled = false;
    bool m_open = false;
    bool m_playing = false;
    bool m_paused = false;
    bool m_sdkOpen = false;
    std::atomic<bool> m_running{false};
    
    DirettaConfig m_config;
    int m_effectiveMTU = 1500;
    int m_mtuOverride = 0;
    
    //=========================================================================
    // Format State (Atomic for thread-safe access)
    //=========================================================================
    
    std::atomic<int> m_sampleRate{0};
    std::atomic<int> m_channels{0};
    std::atomic<int> m_bytesPerSample{0};
    std::atomic<int> m_inputBytesPerSample{0};
    std::atomic<int> m_bytesPerBuffer{0};
    
    std::atomic<bool> m_isDsdMode{false};
    std::atomic<bool> m_need24BitPack{false};
    std::atomic<bool> m_need16To32Upsample{false};
    std::atomic<bool> m_needDsdBitReversal{false};
    std::atomic<bool> m_needDsdByteSwap{false};
    std::atomic<bool> m_isLowBitrate{false};
    
    DirettaRingBuffer::DSDConversionMode m_dsdConversionMode = 
        DirettaRingBuffer::DSDConversionMode::Passthrough;
    
    //=========================================================================
    // Generation Counters (Optimization)
    //=========================================================================
    
    // Format generation for sendAudio path
    std::atomic<uint32_t> m_formatGeneration{0};
    uint32_t m_cachedFormatGen = ~0u;
    
    // Consumer generation for getNewStream path
    std::atomic<uint32_t> m_consumerStateGen{0};
    uint32_t m_cachedConsumerGen = ~0u;
    
    // Cached values (thread-local to avoid repeated atomic loads)
    int m_cachedBytesPerSample = 0;
    int m_cachedInputBytesPerSample = 0;
    int m_cachedChannels = 0;
    bool m_cachedPack = false;
    bool m_cachedUpsample = false;
    bool m_cachedDsdMode = false;
    DirettaRingBuffer::DSDConversionMode m_cachedDsdConversionMode;
    
    // Consumer-side cached values
    int m_cachedBytesPerBuffer = 0;
    uint8_t m_cachedSilenceByte = 0;
    bool m_cachedConsumerIsDsd = false;
    int m_cachedConsumerSampleRate = 0;
    
    //=========================================================================
    // Ring Buffer & Synchronization
    //=========================================================================
    
    DirettaRingBuffer m_ringBuffer;
    std::mutex m_configMutex;
    std::mutex m_workerMutex;
    
    // Reconfiguration support
    std::atomic<bool> m_reconfiguring{false};
    std::atomic<int> m_ringUsers{0};
    
    //=========================================================================
    // SDK 148: Stream Buffer Management
    //=========================================================================
    
    // ⭐ NEW: Buffer pour diretta_stream (géré manuellement)
    std::vector<uint8_t> m_streamBuffer;
    size_t m_streamBufferSize = 0;
    
    //=========================================================================
    // Playback State
    //=========================================================================
    
    std::atomic<bool> m_stopRequested{false};
    std::atomic<int> m_silenceBuffersRemaining{0};
    std::atomic<bool> m_prefillComplete{false};
    size_t m_prefillTarget = 0;
    
    std::atomic<bool> m_postOnlineDelayDone{false};
    std::atomic<int> m_stabilizationCount{0};
    
    bool m_draining = false;
    std::atomic<bool> m_workerActive{false};
    
    //=========================================================================
    // Statistics
    //=========================================================================
    
    std::atomic<uint32_t> m_streamCount{0};
    std::atomic<uint32_t> m_underrunCount{0};
    std::atomic<uint64_t> m_pushCount{0};
    
    //=========================================================================
    // Format Change Tracking
    //=========================================================================
    
    AudioFormat m_currentFormat;
    AudioFormat m_previousFormat;
    bool m_hasPreviousFormat = false;
    
    //=========================================================================
    // Utilities
    //=========================================================================
    
    std::unique_ptr<DirettaCycleCalculator> m_calculator;
    
    // ReconfigureGuard helper
    friend class ReconfigureGuard;
};

//=============================================================================
// Helper: ReconfigureGuard
//=============================================================================

class ReconfigureGuard {
public:
    explicit ReconfigureGuard(DirettaSync& sync) : sync_(sync) {
        sync_.beginReconfigure();
    }
    
    ~ReconfigureGuard() {
        sync_.endReconfigure();
    }
    
private:
    DirettaSync& sync_;
};
