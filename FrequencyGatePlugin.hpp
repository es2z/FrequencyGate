/*
 * FrequencyGate - Frequency-selective noise gate
 * Plugin header file
 */

#ifndef FREQUENCY_GATE_PLUGIN_HPP_INCLUDED
#define FREQUENCY_GATE_PLUGIN_HPP_INCLUDED

#include "DistrhoPlugin.hpp"
#include "DistrhoPluginInfo.h"
#include <vector>
#include <cmath>
#include <algorithm>

#ifdef USE_PFFFT
extern "C" {
#include "pffft.h"
}
#endif

START_NAMESPACE_DISTRHO

class FrequencyGatePlugin : public Plugin
{
public:
    FrequencyGatePlugin();
    ~FrequencyGatePlugin() override;

protected:
    // --------------------------------------------------------------------------------------------------------
    // Information

    const char* getLabel() const noexcept override { return DISTRHO_PLUGIN_NAME; }
    const char* getMaker() const noexcept override { return DISTRHO_PLUGIN_BRAND; }
    const char* getLicense() const noexcept override { return "MIT"; }
    const char* getDescription() const noexcept override { 
        return "Frequency-selective noise gate for voice streaming"; 
    }
    const char* getHomePage() const noexcept override { return DISTRHO_PLUGIN_URI; }
    uint32_t getVersion() const noexcept override { return d_version(1, 0, 0); }
    int64_t getUniqueId() const noexcept override { return d_cconst('F', 'q', 'G', 't'); }

    // --------------------------------------------------------------------------------------------------------
    // Init

    void initParameter(uint32_t index, Parameter& parameter) override;

    // --------------------------------------------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

    // --------------------------------------------------------------------------------------------------------
    // Process

    void activate() override;
    void deactivate() override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;
    void sampleRateChanged(double newSampleRate) override;
    
    // --------------------------------------------------------------------------------------------------------
    // Latency
    
    uint32_t getLatency() const noexcept;

private:
    // Parameters
    float fFreqLow;          // Detection range low frequency (Hz)
    float fFreqHigh;         // Detection range high frequency (Hz)
    float fThreshold;        // Gate threshold (dB)
    float fDetectionMethod;  // Detection algorithm
    float fPreOpen;          // Lookahead (ms)
    float fAttack;           // Attack time (ms)
    float fHold;             // Hold time (ms)
    float fRelease;          // Release time (ms)
    float fHysteresis;       // Hysteresis (dB)
    float fRange;            // Gate attenuation (dB)
    float fFFTSizeOption;    // FFT size selection

    // Internal state
    double mSampleRate;
    int mCurrentFFTSize;
    int mHopSize;
    bool mNeedsReinit;
    
    // FFT setup (PFFFT)
#ifdef USE_PFFFT
    PFFFT_Setup* mPffftSetup;
#endif
    float* mFftInput;
    float* mFftOutput;
    float* mWorkBuffer;
    
    // Window function
    std::vector<float> mWindow;
    std::vector<float> mWindowSum;
    float mWindowGain;  // Amplitude correction factor for window
    
    // Circular buffers (doubled for easy access)
    std::vector<float> mInputBufferL;
    std::vector<float> mInputBufferR;
    std::vector<float> mOutputBufferL;
    std::vector<float> mOutputBufferR;
    
    // Lookahead delay line
    std::vector<float> mLookaheadBufferL;
    std::vector<float> mLookaheadBufferR;
    int mLookaheadWritePos;
    int mLookaheadSamples;
    
    // Buffer positions
    int mInputWritePos;
    int mOutputReadPos;
    int mHopCounter;
    
    // Gate envelope state
    float mEnvelopeLevel;      // Current envelope (0.0 to 1.0)
    float mGateGain;           // Current gate gain (linear)
    bool mGateOpen;            // Gate state for hysteresis
    int mHoldCounter;          // Hold timer (samples)
    
    // Frequency bin cache
    int mStartBin;
    int mEndBin;
    
    // Temporary buffer for detection
    std::vector<float> mMagnitudes;
    
    // Helper functions
    void initFFT();
    void freeFFT();
    void reinitFFT();
    void createWindow();
    void computeBandBins();
    float detectLevel();
    float computeAverage(const std::vector<float>& mags, int start, int count);
    float computePeak(const std::vector<float>& mags, int start, int count);
    float computeMedian(std::vector<float>& mags, int start, int count);
    float computeRMS(const std::vector<float>& mags, int start, int count);
    float computeTrimmedMean(std::vector<float>& mags, int start, int count);
    float linearToDb(float linear);
    float dbToLinear(float db);
    
    // Aligned memory allocation
    static void* alignedAlloc(size_t size);
    static void alignedFree(void* ptr);

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyGatePlugin)
};

END_NAMESPACE_DISTRHO

#endif // FREQUENCY_GATE_PLUGIN_HPP_INCLUDED
