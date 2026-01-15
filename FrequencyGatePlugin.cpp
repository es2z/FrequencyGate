/*
 * FrequencyGate - Frequency-selective noise gate
 * DSP Implementation
 */

#include "FrequencyGatePlugin.hpp"
#include <cstring>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

START_NAMESPACE_DISTRHO

// ============================================================================
// Memory allocation helpers

void* FrequencyGatePlugin::alignedAlloc(size_t size) {
#ifdef _MSC_VER
    return _aligned_malloc(size, 64);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    return __mingw_aligned_malloc(size, 64);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, size) != 0) {
        return nullptr;
    }
    return ptr;
#endif
}

void FrequencyGatePlugin::alignedFree(void* ptr) {
    if (ptr) {
#ifdef _MSC_VER
        _aligned_free(ptr);
#elif defined(__MINGW32__) || defined(__MINGW64__)
        __mingw_aligned_free(ptr);
#else
        free(ptr);
#endif
    }
}

// ============================================================================
// Constructor / Destructor

FrequencyGatePlugin::FrequencyGatePlugin()
    : Plugin(kParamCount, 0, 0)  // parameters, programs, states
    , fFreqLow(100.0f)
    , fFreqHigh(500.0f)
    , fThreshold(-30.0f)
    , fDetectionMethod(static_cast<float>(kDetectAverage))
    , fPreOpen(0.0f)
    , fAttack(5.0f)
    , fHold(50.0f)
    , fRelease(100.0f)
    , fHysteresis(3.0f)
    , fRange(-96.0f)
    , fFFTSizeOption(static_cast<float>(kFFTSize2048))
    , mSampleRate(48000.0)
    , mCurrentFFTSize(DEFAULT_FFT_SIZE)
    , mHopSize(DEFAULT_FFT_SIZE / FFT_OVERLAP)
    , mNeedsReinit(false)
#ifdef USE_PFFFT
    , mPffftSetup(nullptr)
#endif
    , mFftInput(nullptr)
    , mFftOutput(nullptr)
    , mWorkBuffer(nullptr)
    , mLookaheadWritePos(0)
    , mLookaheadSamples(0)
    , mInputWritePos(0)
    , mOutputReadPos(0)
    , mHopCounter(0)
    , mEnvelopeLevel(0.0f)
    , mGateGain(0.0f)
    , mGateOpen(false)
    , mHoldCounter(0)
    , mStartBin(0)
    , mEndBin(0)
{
}

FrequencyGatePlugin::~FrequencyGatePlugin()
{
    freeFFT();
}

// ============================================================================
// FFT Management

void FrequencyGatePlugin::initFFT()
{
    freeFFT();
    
    mCurrentFFTSize = getFFTSizeFromOption(static_cast<int>(fFFTSizeOption));
    mHopSize = mCurrentFFTSize / FFT_OVERLAP;
    
#ifdef USE_PFFFT
    mPffftSetup = pffft_new_setup(mCurrentFFTSize, PFFFT_REAL);
#endif
    
    // Allocate aligned buffers
    mFftInput = static_cast<float*>(alignedAlloc(mCurrentFFTSize * sizeof(float)));
    mFftOutput = static_cast<float*>(alignedAlloc(mCurrentFFTSize * sizeof(float)));
    mWorkBuffer = static_cast<float*>(alignedAlloc(mCurrentFFTSize * sizeof(float)));
    
    // Initialize to zero
    if (mFftInput) std::memset(mFftInput, 0, mCurrentFFTSize * sizeof(float));
    if (mFftOutput) std::memset(mFftOutput, 0, mCurrentFFTSize * sizeof(float));
    if (mWorkBuffer) std::memset(mWorkBuffer, 0, mCurrentFFTSize * sizeof(float));
    
    // Resize and initialize buffers (doubled for easy circular access)
    const size_t bufferSize = static_cast<size_t>(mCurrentFFTSize * 2);
    mInputBufferL.resize(bufferSize, 0.0f);
    mInputBufferR.resize(bufferSize, 0.0f);
    mOutputBufferL.resize(bufferSize, 0.0f);
    mOutputBufferR.resize(bufferSize, 0.0f);
    
    // Window and normalization
    mWindow.resize(mCurrentFFTSize);
    mWindowSum.resize(mCurrentFFTSize, 0.0f);
    createWindow();
    
    // Magnitude buffer for detection
    mMagnitudes.resize(mCurrentFFTSize / 2 + 1, 0.0f);
    
    // Reset positions
    mInputWritePos = 0;
    mOutputReadPos = 0;
    mHopCounter = 0;
    
    // Compute frequency bins
    computeBandBins();
    
    // Update lookahead
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
    if (mPffftSetup) {
        pffft_destroy_setup(mPffftSetup);
        mPffftSetup = nullptr;
    }
#endif
    
    alignedFree(mFftInput);
    alignedFree(mFftOutput);
    alignedFree(mWorkBuffer);
    
    mFftInput = nullptr;
    mFftOutput = nullptr;
    mWorkBuffer = nullptr;
}

void FrequencyGatePlugin::reinitFFT()
{
    initFFT();
    mNeedsReinit = false;
}

void FrequencyGatePlugin::createWindow()
{
    // Hann window
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    for (int i = 0; i < mCurrentFFTSize; i++) {
        mWindow[i] = 0.5f * (1.0f - std::cos(twoPi * i / (mCurrentFFTSize - 1)));
    }
    
    // Compute overlap-add normalization
    std::fill(mWindowSum.begin(), mWindowSum.end(), 0.0f);
    for (int hop = 0; hop < FFT_OVERLAP; hop++) {
        int offset = hop * mHopSize;
        for (int i = 0; i < mCurrentFFTSize; i++) {
            int idx = (i + offset) % mCurrentFFTSize;
            mWindowSum[idx] += mWindow[i] * mWindow[i];
        }
    }
    
    // Avoid division by zero
    for (size_t i = 0; i < mWindowSum.size(); i++) {
        if (mWindowSum[i] < 1e-6f) {
            mWindowSum[i] = 1e-6f;
        }
    }
}

void FrequencyGatePlugin::computeBandBins()
{
    const double binWidth = mSampleRate / mCurrentFFTSize;
    const int nyquistBin = mCurrentFFTSize / 2;
    const double nyquistFreq = mSampleRate / 2.0;
    
    // Clamp frequencies to valid range
    double lowFreq = std::max(20.0, static_cast<double>(fFreqLow));
    double highFreq = std::min(nyquistFreq, static_cast<double>(fFreqHigh));
    
    // Ensure low < high
    if (lowFreq >= highFreq) {
        highFreq = lowFreq + binWidth;
    }
    
    mStartBin = std::max(1, static_cast<int>(std::round(lowFreq / binWidth)));
    mEndBin = std::min(nyquistBin, static_cast<int>(std::round(highFreq / binWidth)));
    
    // Ensure at least one bin
    if (mEndBin <= mStartBin) {
        mEndBin = mStartBin + 1;
    }
}

// ============================================================================
// Detection Methods

float FrequencyGatePlugin::linearToDb(float linear)
{
    if (linear < 1e-10f) return -96.0f;
    return 20.0f * std::log10(linear);
}

float FrequencyGatePlugin::dbToLinear(float db)
{
    if (db <= -96.0f) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

float FrequencyGatePlugin::detectLevel()
{
#ifdef USE_PFFFT
    if (!mPffftSetup || !mFftInput || !mFftOutput) {
        return -96.0f;
    }
    
    // Perform FFT
    pffft_transform_ordered(mPffftSetup, mFftInput, mFftOutput, mWorkBuffer, PFFFT_FORWARD);
    
    // PFFFT output format (CRITICAL - see guide):
    // spectrum[0] = DC component (real only)
    // spectrum[1] = Nyquist component (real only)
    // spectrum[2*k], spectrum[2*k+1] = Re, Im of bin k (for k = 1 to N/2-1)
    
    const float normFactor = 2.0f / mCurrentFFTSize;
    const int halfSize = mCurrentFFTSize / 2;
    
    // Calculate magnitudes for detection range
    for (int bin = mStartBin; bin < mEndBin && bin <= halfSize; bin++) {
        float re, im;
        
        if (bin == 0) {
            re = mFftOutput[0] * normFactor * 0.5f;  // DC - no factor of 2
            im = 0.0f;
        } else if (bin == halfSize) {
            re = mFftOutput[1] * normFactor * 0.5f;  // Nyquist - no factor of 2
            im = 0.0f;
        } else {
            re = mFftOutput[bin * 2] * normFactor;
            im = mFftOutput[bin * 2 + 1] * normFactor;
        }
        
        mMagnitudes[bin] = std::sqrt(re * re + im * im);
    }
    
    // Apply selected detection method
    float level = 0.0f;
    const int binCount = mEndBin - mStartBin;
    
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
    
    return linearToDb(level);
#else
    return -96.0f;
#endif
}

float FrequencyGatePlugin::computeAverage(const std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += mags[start + i];
    }
    return sum / count;
}

float FrequencyGatePlugin::computePeak(const std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    
    float peak = 0.0f;
    for (int i = 0; i < count; i++) {
        if (mags[start + i] > peak) {
            peak = mags[start + i];
        }
    }
    return peak;
}

float FrequencyGatePlugin::computeMedian(std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    
    // Copy to temporary for sorting (to not disturb original)
    std::vector<float> temp(mags.begin() + start, mags.begin() + start + count);
    std::sort(temp.begin(), temp.end());
    
    if (count % 2 == 0) {
        return (temp[count / 2 - 1] + temp[count / 2]) / 2.0f;
    } else {
        return temp[count / 2];
    }
}

float FrequencyGatePlugin::computeRMS(const std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    
    float sumSquared = 0.0f;
    for (int i = 0; i < count; i++) {
        sumSquared += mags[start + i] * mags[start + i];
    }
    return std::sqrt(sumSquared / count);
}

float FrequencyGatePlugin::computeTrimmedMean(std::vector<float>& mags, int start, int count)
{
    if (count <= 0) return 0.0f;
    if (count <= 4) return computeAverage(mags, start, count);  // Not enough samples to trim
    
    // Copy and sort
    std::vector<float> temp(mags.begin() + start, mags.begin() + start + count);
    std::sort(temp.begin(), temp.end());
    
    // Trim top and bottom 10%
    const int trimCount = std::max(1, count / 10);
    const int trimmedStart = trimCount;
    const int trimmedEnd = count - trimCount;
    const int trimmedCount = trimmedEnd - trimmedStart;
    
    if (trimmedCount <= 0) return computeAverage(mags, start, count);
    
    float sum = 0.0f;
    for (int i = trimmedStart; i < trimmedEnd; i++) {
        sum += temp[i];
    }
    return sum / trimmedCount;
}

// ============================================================================
// Parameter Interface

void FrequencyGatePlugin::initParameter(uint32_t index, Parameter& parameter)
{
    switch (index) {
        case kParamFreqLow:
            parameter.name = "Freq Low";
            parameter.symbol = "freq_low";
            parameter.unit = "Hz";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 100.0f;
            parameter.ranges.min = 20.0f;
            parameter.ranges.max = 20000.0f;
            break;
            
        case kParamFreqHigh:
            parameter.name = "Freq High";
            parameter.symbol = "freq_high";
            parameter.unit = "Hz";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 500.0f;
            parameter.ranges.min = 20.0f;
            parameter.ranges.max = 20000.0f;
            break;
            
        case kParamThreshold:
            parameter.name = "Threshold";
            parameter.symbol = "threshold";
            parameter.unit = "dB";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = -30.0f;
            parameter.ranges.min = -96.0f;
            parameter.ranges.max = 0.0f;
            break;
            
        case kParamDetectionMethod:
            parameter.name = "Detection";
            parameter.symbol = "detection";
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            parameter.ranges.def = static_cast<float>(kDetectAverage);
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = static_cast<float>(kDetectCount - 1);
            parameter.enumValues.count = kDetectCount;
            parameter.enumValues.restrictedMode = true;
            {
                ParameterEnumerationValue* const values = new ParameterEnumerationValue[kDetectCount];
                values[kDetectAverage].label = "Average";
                values[kDetectAverage].value = kDetectAverage;
                values[kDetectPeak].label = "Peak";
                values[kDetectPeak].value = kDetectPeak;
                values[kDetectMedian].label = "Median";
                values[kDetectMedian].value = kDetectMedian;
                values[kDetectRMS].label = "RMS";
                values[kDetectRMS].value = kDetectRMS;
                values[kDetectTrimmedMean].label = "Trimmed Mean";
                values[kDetectTrimmedMean].value = kDetectTrimmedMean;
                parameter.enumValues.values = values;
            }
            break;
            
        case kParamPreOpen:
            parameter.name = "Pre-Open";
            parameter.symbol = "preopen";
            parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 20.0f;
            break;
            
        case kParamAttack:
            parameter.name = "Attack";
            parameter.symbol = "attack";
            parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 5.0f;
            parameter.ranges.min = 0.1f;
            parameter.ranges.max = 100.0f;
            break;
            
        case kParamHold:
            parameter.name = "Hold";
            parameter.symbol = "hold";
            parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 50.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 500.0f;
            break;
            
        case kParamRelease:
            parameter.name = "Release";
            parameter.symbol = "release";
            parameter.unit = "ms";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 100.0f;
            parameter.ranges.min = 1.0f;
            parameter.ranges.max = 1000.0f;
            break;
            
        case kParamHysteresis:
            parameter.name = "Hysteresis";
            parameter.symbol = "hysteresis";
            parameter.unit = "dB";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = 3.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 12.0f;
            break;
            
        case kParamRange:
            parameter.name = "Range";
            parameter.symbol = "range";
            parameter.unit = "dB";
            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.def = -96.0f;
            parameter.ranges.min = -96.0f;
            parameter.ranges.max = 0.0f;
            break;
            
        case kParamFFTSize:
            parameter.name = "FFT Size";
            parameter.symbol = "fft_size";
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            parameter.ranges.def = static_cast<float>(kFFTSize2048);
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = static_cast<float>(kFFTSizeCount - 1);
            parameter.enumValues.count = kFFTSizeCount;
            parameter.enumValues.restrictedMode = true;
            {
                ParameterEnumerationValue* const values = new ParameterEnumerationValue[kFFTSizeCount];
                values[kFFTSize512].label = "512 (~5ms)";
                values[kFFTSize512].value = kFFTSize512;
                values[kFFTSize1024].label = "1024 (~10ms)";
                values[kFFTSize1024].value = kFFTSize1024;
                values[kFFTSize2048].label = "2048 (~21ms)";
                values[kFFTSize2048].value = kFFTSize2048;
                values[kFFTSize4096].label = "4096 (~42ms)";
                values[kFFTSize4096].value = kFFTSize4096;
                parameter.enumValues.values = values;
            }
            break;
            
        default:
            break;
    }
}

float FrequencyGatePlugin::getParameterValue(uint32_t index) const
{
    switch (index) {
        case kParamFreqLow:        return fFreqLow;
        case kParamFreqHigh:       return fFreqHigh;
        case kParamThreshold:      return fThreshold;
        case kParamDetectionMethod: return fDetectionMethod;
        case kParamPreOpen:        return fPreOpen;
        case kParamAttack:         return fAttack;
        case kParamHold:           return fHold;
        case kParamRelease:        return fRelease;
        case kParamHysteresis:     return fHysteresis;
        case kParamRange:          return fRange;
        case kParamFFTSize:        return fFFTSizeOption;
        default:                   return 0.0f;
    }
}

void FrequencyGatePlugin::setParameterValue(uint32_t index, float value)
{
    switch (index) {
        case kParamFreqLow:
            fFreqLow = value;
            computeBandBins();
            break;
        case kParamFreqHigh:
            fFreqHigh = value;
            computeBandBins();
            break;
        case kParamThreshold:
            fThreshold = value;
            break;
        case kParamDetectionMethod:
            fDetectionMethod = value;
            break;
        case kParamPreOpen:
            if (fPreOpen != value) {
                fPreOpen = value;
                // Recalculate lookahead samples
                mLookaheadSamples = static_cast<int>(fPreOpen * mSampleRate / 1000.0);
                if (mLookaheadSamples > 0) {
                    mLookaheadBufferL.resize(mLookaheadSamples, 0.0f);
                    mLookaheadBufferR.resize(mLookaheadSamples, 0.0f);
                    std::fill(mLookaheadBufferL.begin(), mLookaheadBufferL.end(), 0.0f);
                    std::fill(mLookaheadBufferR.begin(), mLookaheadBufferR.end(), 0.0f);
                } else {
                    mLookaheadBufferL.clear();
                    mLookaheadBufferR.clear();
                }
                mLookaheadWritePos = 0;
            }
            break;
        case kParamAttack:
            fAttack = value;
            break;
        case kParamHold:
            fHold = value;
            break;
        case kParamRelease:
            fRelease = value;
            break;
        case kParamHysteresis:
            fHysteresis = value;
            break;
        case kParamRange:
            fRange = value;
            break;
        case kParamFFTSize:
            if (static_cast<int>(fFFTSizeOption) != static_cast<int>(value)) {
                fFFTSizeOption = value;
                mNeedsReinit = true;
            }
            break;
        default:
            break;
    }
}

// ============================================================================
// Processing

void FrequencyGatePlugin::activate()
{
    initFFT();
    
    // Reset envelope state
    mEnvelopeLevel = 0.0f;
    mGateGain = dbToLinear(fRange);
    mGateOpen = false;
    mHoldCounter = 0;
}

void FrequencyGatePlugin::deactivate()
{
    // Keep FFT allocated for quick reactivation
}

void FrequencyGatePlugin::sampleRateChanged(double newSampleRate)
{
    mSampleRate = newSampleRate;
    mNeedsReinit = true;
}

uint32_t FrequencyGatePlugin::getLatency() const noexcept
{
    // Total latency = FFT hop size + lookahead
    return static_cast<uint32_t>(mHopSize + mLookaheadSamples);
}

void FrequencyGatePlugin::run(const float** inputs, float** outputs, uint32_t frames)
{
    // Handle reinit if needed
    if (mNeedsReinit) {
        reinitFFT();
    }
    
    const float* inL = inputs[0];
    const float* inR = inputs[1];
    float* outL = outputs[0];
    float* outR = outputs[1];
    
    // Pre-calculate envelope coefficients
    const float attackCoeff = std::exp(-1.0f / (static_cast<float>(mSampleRate) * fAttack / 1000.0f));
    const float releaseCoeff = std::exp(-1.0f / (static_cast<float>(mSampleRate) * fRelease / 1000.0f));
    const int holdSamples = static_cast<int>(fHold * mSampleRate / 1000.0f);
    const float rangeGain = dbToLinear(fRange);
    
    // Thresholds with hysteresis
    const float openThreshold = fThreshold;
    const float closeThreshold = fThreshold - fHysteresis;
    
    for (uint32_t i = 0; i < frames; i++) {
        // Get input samples
        float sampleL = inL[i];
        float sampleR = inR[i];
        
        // Mix to mono for detection (L+R average)
        float monoSample = (sampleL + sampleR) * 0.5f;
        
        // Write to input circular buffer (doubled for easy access)
        mInputBufferL[mInputWritePos] = sampleL;
        mInputBufferL[mInputWritePos + mCurrentFFTSize] = sampleL;
        mInputBufferR[mInputWritePos] = sampleR;
        mInputBufferR[mInputWritePos + mCurrentFFTSize] = sampleR;
        
        mInputWritePos = (mInputWritePos + 1) % mCurrentFFTSize;
        mHopCounter++;
        
        // Process FFT when we have enough samples
        if (mHopCounter >= mHopSize) {
            mHopCounter = 0;
            
            // Copy input with window for mono detection
            int readPos = (mInputWritePos - mCurrentFFTSize + mCurrentFFTSize * 2) % (mCurrentFFTSize * 2);
            for (int j = 0; j < mCurrentFFTSize; j++) {
                // Use mono mix for detection
                float monoInput = (mInputBufferL[readPos + j] + mInputBufferR[readPos + j]) * 0.5f;
                mFftInput[j] = monoInput * mWindow[j];
            }
            
            // Detect level in frequency range
            float detectedLevel = detectLevel();
            
            // Gate logic with hysteresis
            bool shouldOpen;
            if (mGateOpen) {
                // Gate is open - use close threshold (lower)
                shouldOpen = detectedLevel >= closeThreshold;
            } else {
                // Gate is closed - use open threshold (higher)
                shouldOpen = detectedLevel >= openThreshold;
            }
            
            if (shouldOpen) {
                mGateOpen = true;
                mHoldCounter = holdSamples;
            } else if (mHoldCounter > 0) {
                mHoldCounter--;
            } else {
                mGateOpen = false;
            }
        }
        
        // Update envelope
        float targetEnvelope = mGateOpen ? 1.0f : 0.0f;
        
        if (targetEnvelope > mEnvelopeLevel) {
            // Attack
            mEnvelopeLevel = targetEnvelope - (targetEnvelope - mEnvelopeLevel) * attackCoeff;
        } else {
            // Release (only after hold)
            if (mHoldCounter <= 0) {
                mEnvelopeLevel = targetEnvelope + (mEnvelopeLevel - targetEnvelope) * releaseCoeff;
            }
        }
        
        // Calculate gate gain
        // When envelope = 1.0, gain = 1.0 (fully open)
        // When envelope = 0.0, gain = rangeGain (attenuated)
        mGateGain = rangeGain + (1.0f - rangeGain) * mEnvelopeLevel;
        
        // Apply gate with optional lookahead
        float gatedL, gatedR;
        
        if (mLookaheadSamples > 0 && !mLookaheadBufferL.empty()) {
            // Lookahead: delay the audio, apply current gain
            gatedL = mLookaheadBufferL[mLookaheadWritePos] * mGateGain;
            gatedR = mLookaheadBufferR[mLookaheadWritePos] * mGateGain;
            
            // Store current sample in delay line
            mLookaheadBufferL[mLookaheadWritePos] = sampleL;
            mLookaheadBufferR[mLookaheadWritePos] = sampleR;
            mLookaheadWritePos = (mLookaheadWritePos + 1) % mLookaheadSamples;
        } else {
            // No lookahead
            gatedL = sampleL * mGateGain;
            gatedR = sampleR * mGateGain;
        }
        
        outL[i] = gatedL;
        outR[i] = gatedR;
    }
}

// ============================================================================
// Plugin entry point

Plugin* createPlugin()
{
    return new FrequencyGatePlugin();
}

END_NAMESPACE_DISTRHO
