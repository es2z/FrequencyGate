/*
 * FrequencyGate - Frequency-selective noise gate VST3 plugin
 * 
 * A noise gate that uses FFT analysis to detect audio level
 * only in a specified frequency range, then gates the full spectrum.
 * Optimized for voice streaming applications.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// Plugin metadata
#define DISTRHO_PLUGIN_BRAND           "StreamTools"
#define DISTRHO_PLUGIN_NAME            "FrequencyGate"
#define DISTRHO_PLUGIN_URI             "https://github.com/streamtools/frequencygate"
#define DISTRHO_PLUGIN_CLAP_ID         "com.streamtools.frequencygate"

// Plugin features
#define DISTRHO_PLUGIN_HAS_UI          1
#define DISTRHO_PLUGIN_IS_RT_SAFE      1
#define DISTRHO_PLUGIN_NUM_INPUTS      2
#define DISTRHO_PLUGIN_NUM_OUTPUTS     2
#define DISTRHO_PLUGIN_WANT_LATENCY    1  // Required for FFT + lookahead
#define DISTRHO_PLUGIN_WANT_STATE      0
#define DISTRHO_PLUGIN_WANT_TIMEPOS    0
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 0

// UI configuration
#define DISTRHO_UI_DEFAULT_WIDTH       800
#define DISTRHO_UI_DEFAULT_HEIGHT      500
#define DISTRHO_UI_USE_NANOVG          1

// Default FFT settings (can be changed at runtime)
#define DEFAULT_FFT_SIZE    2048   // ~10ms latency at 48kHz with 75% overlap
#define MAX_FFT_SIZE        4096   // Maximum supported FFT size
#define FFT_OVERLAP         4      // 75% overlap

// Parameter enumeration
enum Parameters {
    kParamFreqLow = 0,      // Detection frequency range lower bound (Hz)
    kParamFreqHigh,         // Detection frequency range upper bound (Hz)
    kParamThreshold,        // Gate threshold (dB)
    kParamDetectionMethod,  // Detection algorithm (Average, Peak, Median, RMS, TrimmedMean)
    kParamPreOpen,          // Lookahead time (ms)
    kParamAttack,           // Attack time (ms)
    kParamHold,             // Hold time (ms)
    kParamRelease,          // Release time (ms)
    kParamHysteresis,       // Hysteresis (dB) - difference between open and close thresholds
    kParamRange,            // Gate attenuation when closed (dB)
    kParamFFTSize,          // FFT size selection (0=512, 1=1024, 2=2048, 3=4096)
    kParamCount
};

// Detection method enumeration
enum DetectionMethod {
    kDetectAverage = 0,     // Average magnitude (default, good for voice)
    kDetectPeak,            // Peak magnitude (sensitive to transients)
    kDetectMedian,          // Median magnitude (robust to outliers)
    kDetectRMS,             // RMS magnitude (energy-based)
    kDetectTrimmedMean,     // Trimmed mean (removes top/bottom 10%, best for noise rejection)
    kDetectCount
};

// FFT size options
enum FFTSizeOption {
    kFFTSize512 = 0,
    kFFTSize1024,
    kFFTSize2048,
    kFFTSize4096,
    kFFTSizeCount
};

// Convert FFT size option to actual size
inline int getFFTSizeFromOption(int option) {
    switch (option) {
        case kFFTSize512:  return 512;
        case kFFTSize1024: return 1024;
        case kFFTSize2048: return 2048;
        case kFFTSize4096: return 4096;
        default:           return DEFAULT_FFT_SIZE;
    }
}

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
