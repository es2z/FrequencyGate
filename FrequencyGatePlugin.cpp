/*
 * FrequencyGate - Frequency-selective noise gate
 * DSP Implementation - Fixed FFT normalization and level detection
 */

#include "FrequencyGatePlugin.hpp"
#include <cstring>
#include <cstdlib>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

START_NAMESPACE_DISTRHO

// Memory helpers
void* FrequencyGatePlugin::alignedAlloc(size_t size) {
#ifdef _MSC_VER
    return _aligned_malloc(size, 64);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    return __mingw_aligned_malloc(size, 64);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, size) != 0) return nullptr;
    return ptr;
#endif
}

void FrequencyGatePlugin::alignedFree(void* ptr) {
    if (!ptr) return;
#ifdef _MSC_VER
    _aligned_free(ptr);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Constructor
FrequencyGatePlugin::FrequencyGatePlugin()
    : Plugin(kParamCount, 0, 0)
    , fFreqLow(100.0f), fFreqHigh(500.0f), fThreshold(-30.0f)
    , fDetectionMethod(0.0f), fPreOpen(0.0f), fAttack(5.0f)
    , fHold(50.0f), fRelease(100.0f), fHysteresis(3.0f)
    , fRange(-96.0f), fFFTSizeOption(2.0f)
    , mSampleRate(48000.0), mCurrentFFTSize(DEFAULT_FFT_SIZE)
    , mHopSize(DEFAULT_FFT_SIZE / FFT_OVERLAP), mNeedsReinit(false)
#ifdef USE_PFFFT
    , mPffftSetup(nullptr)
#endif
    , mFftInput(nullptr), mFftOutput(nullptr), mWorkBuffer(nullptr)
    , mWindowGain(1.0f), mLookaheadWritePos(0), mLookaheadSamples(0)
    , mInputWritePos(0), mOutputReadPos(0), mHopCounter(0)
    , mEnvelopeLevel(0.0f), mGateGain(0.0f), mGateOpen(false)
    , mHoldCounter(0), mStartBin(0), mEndBin(0)
{
}

FrequencyGatePlugin::~FrequencyGatePlugin() { freeFFT(); }

// FFT Management
void FrequencyGatePlugin::initFFT()
{
    freeFFT();
    mCurrentFFTSize = getFFTSizeFromOption(static_cast<int>(fFFTSizeOption));
    mHopSize = mCurrentFFTSize / FFT_OVERLAP;
    
#ifdef USE_PFFFT
    mPffftSetup = pffft_new_setup(mCurrentFFTSize, PFFFT_REAL);
#endif
    
    mFftInput = static_cast<float*>(alignedAlloc(mCurrentFFTSize * sizeof(float)));
    mFftOutput = static_cast<float*>(alignedAlloc(mCurrentFFTSize * sizeof(float)));
    mWorkBuffer = static_cast<float*>(alignedAlloc(mCurrentFFTSize * sizeof(float)));
    
    if (mFftInput) std::memset(mFftInput, 0, mCurrentFFTSize * sizeof(float));
    if (mFftOutput) std::memset(mFftOutput, 0, mCurrentFFTSize * sizeof(float));
    if (mWorkBuffer) std::memset(mWorkBuffer, 0, mCurrentFFTSize * sizeof(float));
    
    const size_t bufSize = mCurrentFFTSize * 2;
    mInputBufferL.resize(bufSize, 0.0f);
    mInputBufferR.resize(bufSize, 0.0f);
    
    mWindow.resize(mCurrentFFTSize);
    mWindowSum.resize(mCurrentFFTSize, 0.0f);
    createWindow();
    
    mMagnitudes.resize(mCurrentFFTSize / 2 + 1, 0.0f);
    mInputWritePos = 0;
    mHopCounter = 0;
    
    computeBandBins();
    
    mLookaheadSamples = static_cast<int>(fPreOpen * mSampleRate / 1000.0);
    if (mLookaheadSamples > 0) {
        mLookaheadBufferL.resize(mLookaheadSamples, 0.0f);
        mLookaheadBufferR.resize(mLookaheadSamples, 0.0f);
    } else {
        mLookaheadBufferL.clear();
        mLookaheadBufferR.clear();
    }
    mLookaheadWritePos = 0;
}

void FrequencyGatePlugin::freeFFT()
{
#ifdef USE_PFFFT
    if (mPffftSetup) { pffft_destroy_setup(mPffftSetup); mPffftSetup = nullptr; }
#endif
    alignedFree(mFftInput); mFftInput = nullptr;
    alignedFree(mFftOutput); mFftOutput = nullptr;
    alignedFree(mWorkBuffer); mWorkBuffer = nullptr;
}

void FrequencyGatePlugin::reinitFFT() { initFFT(); mNeedsReinit = false; }

void FrequencyGatePlugin::createWindow()
{
    // Hann window
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    float sum = 0.0f;
    for (int i = 0; i < mCurrentFFTSize; i++) {
        mWindow[i] = 0.5f * (1.0f - std::cos(twoPi * i / (mCurrentFFTSize - 1)));
        sum += mWindow[i];
    }
    // Coherent gain compensation: Hann ~= 0.5, so multiply by ~2
    mWindowGain = static_cast<float>(mCurrentFFTSize) / sum;
}

void FrequencyGatePlugin::computeBandBins()
{
    const double binWidth = mSampleRate / mCurrentFFTSize;
    const int nyquistBin = mCurrentFFTSize / 2;
    const double nyquistFreq = mSampleRate / 2.0;
    
    double lowFreq = std::max(20.0, static_cast<double>(fFreqLow));
    double highFreq = std::min(nyquistFreq, static_cast<double>(fFreqHigh));
    if (lowFreq >= highFreq) highFreq = lowFreq + binWidth;
    
    mStartBin = std::max(1, static_cast<int>(std::floor(lowFreq / binWidth)));
    mEndBin = std::min(nyquistBin, static_cast<int>(std::ceil(highFreq / binWidth)));
    if (mEndBin <= mStartBin) mEndBin = mStartBin + 1;
}

// Level detection
float FrequencyGatePlugin::linearToDb(float linear)
{
    if (linear < 1e-10f) return -96.0f;
    return std::max(-96.0f, 20.0f * std::log10(linear));
}

float FrequencyGatePlugin::dbToLinear(float db)
{
    if (db <= -96.0f) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

float FrequencyGatePlugin::detectLevel()
{
#ifdef USE_PFFFT
    if (!mPffftSetup || !mFftInput || !mFftOutput) return -96.0f;
    
    pffft_transform_ordered(mPffftSetup, mFftInput, mFftOutput, mWorkBuffer, PFFFT_FORWARD);
    
    // PFFFT real FFT output (ordered):
    // [0] = DC, [1] = Nyquist, [2k] = Re(k), [2k+1] = Im(k) for k=1..N/2-1
    
    // Normalization: divide by N, multiply by 2 for single-sided (except DC/Nyquist)
    // Also apply window gain compensation
    const int N = mCurrentFFTSize;
    const int halfSize = N / 2;
    
    int binCount = 0;
    for (int bin = mStartBin; bin <= mEndBin && bin <= halfSize; bin++) {
        float re, im;
        
        if (bin == 0) {
            re = mFftOutput[0] / N * mWindowGain;
            im = 0.0f;
        } else if (bin == halfSize) {
            re = mFftOutput[1] / N * mWindowGain;
            im = 0.0f;
        } else {
            // Single-sided: multiply by 2
            re = mFftOutput[bin * 2] * 2.0f / N * mWindowGain;
            im = mFftOutput[bin * 2 + 1] * 2.0f / N * mWindowGain;
        }
        
        // Magnitude (amplitude)
        mMagnitudes[bin] = std::sqrt(re * re + im * im);
        binCount++;
    }
    
    if (binCount == 0) return -96.0f;
    
    // Apply detection method on LINEAR magnitudes
    float level = 0.0f;
    switch (static_cast<int>(fDetectionMethod)) {
        case kDetectPeak:
            level = computePeak(mMagnitudes, mStartBin, binCount);
            break;
        case kDetectMedian:
            level = computeMedian(mMagnitudes, mStartBin, binCount);
            break;
        case kDetectRMS:
            level = computeRMS(mMagnitudes, mStartBin, binCount);
            break;
        case kDetectTrimmedMean:
            level = computeTrimmedMean(mMagnitudes, mStartBin, binCount);
            break;
        case kDetectAverage:
        default:
            level = computeAverage(mMagnitudes, mStartBin, binCount);
            break;
    }
    
    // Convert to dBFS
    return linearToDb(level);
#else
    return -96.0f;
#endif
}

// Detection algorithms
float FrequencyGatePlugin::computeAverage(const std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < count; i++) sum += mags[start + i];
    return sum / count;
}

float FrequencyGatePlugin::computePeak(const std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < count; i++) {
        if (mags[start + i] > peak) peak = mags[start + i];
    }
    return peak;
}

float FrequencyGatePlugin::computeMedian(std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    std::vector<float> temp(mags.begin() + start, mags.begin() + start + count);
    std::sort(temp.begin(), temp.end());
    return (count % 2 == 0) ? (temp[count/2-1] + temp[count/2]) * 0.5f : temp[count/2];
}

float FrequencyGatePlugin::computeRMS(const std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    float sumSq = 0.0f;
    for (int i = 0; i < count; i++) {
        float m = mags[start + i];
        sumSq += m * m;
    }
    return std::sqrt(sumSq / count);
}

float FrequencyGatePlugin::computeTrimmedMean(std::vector<float>& mags, int start, int count)
{
    if (count <= 4) return computeAverage(mags, start, count);
    std::vector<float> temp(mags.begin() + start, mags.begin() + start + count);
    std::sort(temp.begin(), temp.end());
    int trim = std::max(1, count / 10);
    float sum = 0.0f;
    int tc = count - 2 * trim;
    for (int i = trim; i < count - trim; i++) sum += temp[i];
    return (tc > 0) ? sum / tc : computeAverage(mags, start, count);
}

// Parameters
void FrequencyGatePlugin::initParameter(uint32_t index, Parameter& parameter)
{
    switch (index) {
        case kParamFreqLow:
            parameter.name = "Freq Low"; parameter.symbol = "freq_low"; parameter.unit = "Hz";
            parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
            parameter.ranges.def = 100.0f; parameter.ranges.min = 20.0f; parameter.ranges.max = 20000.0f;
            break;
        case kParamFreqHigh:
            parameter.name = "Freq High"; parameter.symbol = "freq_high"; parameter.unit = "Hz";
            parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
            parameter.ranges.def = 500.0f; parameter.ranges.min = 20.0f; parameter.ranges.max = 20000.0f;
            break;
        case kParamThreshold:
            parameter.name = "Threshold"; parameter.symbol = "threshold"; parameter.unit = "dB";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = -30.0f; parameter.ranges.min = -96.0f; parameter.ranges.max = 0.0f;
            break;
        case kParamDetectionMethod:
            parameter.name = "Detection"; parameter.symbol = "detection";
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = kDetectCount - 1;
            parameter.enumValues.count = kDetectCount;
            parameter.enumValues.restrictedMode = true;
            {
                ParameterEnumerationValue* v = new ParameterEnumerationValue[kDetectCount];
                v[0].label = "Average"; v[0].value = 0;
                v[1].label = "Peak"; v[1].value = 1;
                v[2].label = "Median"; v[2].value = 2;
                v[3].label = "RMS"; v[3].value = 3;
                v[4].label = "Trimmed Mean"; v[4].value = 4;
                parameter.enumValues.values = v;
            }
            break;
        case kParamPreOpen:
            parameter.name = "Pre-Open"; parameter.symbol = "preopen"; parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 20.0f;
            break;
        case kParamAttack:
            parameter.name = "Attack"; parameter.symbol = "attack"; parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
            parameter.ranges.def = 5.0f; parameter.ranges.min = 0.1f; parameter.ranges.max = 100.0f;
            break;
        case kParamHold:
            parameter.name = "Hold"; parameter.symbol = "hold"; parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 50.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 500.0f;
            break;
        case kParamRelease:
            parameter.name = "Release"; parameter.symbol = "release"; parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
            parameter.ranges.def = 100.0f; parameter.ranges.min = 1.0f; parameter.ranges.max = 1000.0f;
            break;
        case kParamHysteresis:
            parameter.name = "Hysteresis"; parameter.symbol = "hysteresis"; parameter.unit = "dB";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 3.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 12.0f;
            break;
        case kParamRange:
            parameter.name = "Range"; parameter.symbol = "range"; parameter.unit = "dB";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = -96.0f; parameter.ranges.min = -96.0f; parameter.ranges.max = 0.0f;
            break;
        case kParamFFTSize:
            parameter.name = "FFT Size"; parameter.symbol = "fft_size";
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            parameter.ranges.def = 2.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = kFFTSizeCount - 1;
            parameter.enumValues.count = kFFTSizeCount;
            parameter.enumValues.restrictedMode = true;
            {
                ParameterEnumerationValue* v = new ParameterEnumerationValue[kFFTSizeCount];
                v[0].label = "512"; v[0].value = 0;
                v[1].label = "1024"; v[1].value = 1;
                v[2].label = "2048"; v[2].value = 2;
                v[3].label = "4096"; v[3].value = 3;
                parameter.enumValues.values = v;
            }
            break;
    }
}

float FrequencyGatePlugin::getParameterValue(uint32_t index) const
{
    switch (index) {
        case kParamFreqLow: return fFreqLow;
        case kParamFreqHigh: return fFreqHigh;
        case kParamThreshold: return fThreshold;
        case kParamDetectionMethod: return fDetectionMethod;
        case kParamPreOpen: return fPreOpen;
        case kParamAttack: return fAttack;
        case kParamHold: return fHold;
        case kParamRelease: return fRelease;
        case kParamHysteresis: return fHysteresis;
        case kParamRange: return fRange;
        case kParamFFTSize: return fFFTSizeOption;
        default: return 0.0f;
    }
}

void FrequencyGatePlugin::setParameterValue(uint32_t index, float value)
{
    switch (index) {
        case kParamFreqLow: fFreqLow = value; computeBandBins(); break;
        case kParamFreqHigh: fFreqHigh = value; computeBandBins(); break;
        case kParamThreshold: fThreshold = value; break;
        case kParamDetectionMethod: fDetectionMethod = value; break;
        case kParamPreOpen:
            if (fPreOpen != value) {
                fPreOpen = value;
                mLookaheadSamples = static_cast<int>(fPreOpen * mSampleRate / 1000.0);
                if (mLookaheadSamples > 0) {
                    mLookaheadBufferL.resize(mLookaheadSamples, 0.0f);
                    mLookaheadBufferR.resize(mLookaheadSamples, 0.0f);
                } else {
                    mLookaheadBufferL.clear();
                    mLookaheadBufferR.clear();
                }
                mLookaheadWritePos = 0;
            }
            break;
        case kParamAttack: fAttack = value; break;
        case kParamHold: fHold = value; break;
        case kParamRelease: fRelease = value; break;
        case kParamHysteresis: fHysteresis = value; break;
        case kParamRange: fRange = value; break;
        case kParamFFTSize:
            if (static_cast<int>(fFFTSizeOption) != static_cast<int>(value)) {
                fFFTSizeOption = value;
                mNeedsReinit = true;
            }
            break;
    }
}

// Processing
void FrequencyGatePlugin::activate()
{
    initFFT();
    mEnvelopeLevel = 0.0f;
    mGateGain = dbToLinear(fRange);
    mGateOpen = false;
    mHoldCounter = 0;
}

void FrequencyGatePlugin::deactivate() {}

void FrequencyGatePlugin::sampleRateChanged(double newSampleRate)
{
    mSampleRate = newSampleRate;
    mNeedsReinit = true;
}

uint32_t FrequencyGatePlugin::getLatency() const noexcept
{
    return static_cast<uint32_t>(mHopSize + mLookaheadSamples);
}

void FrequencyGatePlugin::run(const float** inputs, float** outputs, uint32_t frames)
{
    if (mNeedsReinit) reinitFFT();
    
    const float* inL = inputs[0];
    const float* inR = inputs[1];
    float* outL = outputs[0];
    float* outR = outputs[1];
    
    // Envelope coefficients
    const float attackCoeff = std::exp(-1.0f / (static_cast<float>(mSampleRate) * fAttack / 1000.0f));
    const float releaseCoeff = std::exp(-1.0f / (static_cast<float>(mSampleRate) * fRelease / 1000.0f));
    const int holdSamples = static_cast<int>(fHold * mSampleRate / 1000.0f);
    const float rangeGain = dbToLinear(fRange);
    
    // Thresholds with hysteresis
    const float openThresh = fThreshold;
    const float closeThresh = fThreshold - fHysteresis;
    
    for (uint32_t i = 0; i < frames; i++) {
        float sL = inL[i];
        float sR = inR[i];
        
        // Write to circular buffer (doubled for easy access)
        mInputBufferL[mInputWritePos] = sL;
        mInputBufferL[mInputWritePos + mCurrentFFTSize] = sL;
        mInputBufferR[mInputWritePos] = sR;
        mInputBufferR[mInputWritePos + mCurrentFFTSize] = sR;
        
        mInputWritePos = (mInputWritePos + 1) % mCurrentFFTSize;
        mHopCounter++;
        
        // FFT at hop intervals
        if (mHopCounter >= mHopSize) {
            mHopCounter = 0;
            
            // Fill FFT input with windowed mono signal
            int readPos = mInputWritePos; // Start from current pos (oldest sample in window)
            for (int j = 0; j < mCurrentFFTSize; j++) {
                float mono = (mInputBufferL[readPos + j] + mInputBufferR[readPos + j]) * 0.5f;
                mFftInput[j] = mono * mWindow[j];
            }
            
            float level = detectLevel();
            
            // Gate logic with hysteresis
            bool shouldOpen = mGateOpen ? (level >= closeThresh) : (level >= openThresh);
            
            if (shouldOpen) {
                mGateOpen = true;
                mHoldCounter = holdSamples;
            } else if (mHoldCounter > 0) {
                mHoldCounter--;
            } else {
                mGateOpen = false;
            }
        }
        
        // Envelope follower
        float target = mGateOpen ? 1.0f : 0.0f;
        if (target > mEnvelopeLevel) {
            mEnvelopeLevel = target - (target - mEnvelopeLevel) * attackCoeff;
        } else if (mHoldCounter <= 0) {
            mEnvelopeLevel = target + (mEnvelopeLevel - target) * releaseCoeff;
        }
        
        // Compute gain
        mGateGain = rangeGain + (1.0f - rangeGain) * mEnvelopeLevel;
        
        // Apply gate (with optional lookahead)
        float gL, gR;
        if (mLookaheadSamples > 0 && !mLookaheadBufferL.empty()) {
            gL = mLookaheadBufferL[mLookaheadWritePos] * mGateGain;
            gR = mLookaheadBufferR[mLookaheadWritePos] * mGateGain;
            mLookaheadBufferL[mLookaheadWritePos] = sL;
            mLookaheadBufferR[mLookaheadWritePos] = sR;
            mLookaheadWritePos = (mLookaheadWritePos + 1) % mLookaheadSamples;
        } else {
            gL = sL * mGateGain;
            gR = sR * mGateGain;
        }
        
        outL[i] = gL;
        outR[i] = gR;
    }
}

Plugin* createPlugin() { return new FrequencyGatePlugin(); }

END_NAMESPACE_DISTRHO
