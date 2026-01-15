/*
 * FrequencyGate - Frequency-selective noise gate
 * NanoVG UI Implementation with font fallback
 */

#include "DistrhoUI.hpp"
#include "DistrhoPluginInfo.h"
#include <cmath>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

START_NAMESPACE_DISTRHO

// ============================================================================
// 7-Segment Display Fallback (When Fonts Fail)
// ============================================================================

class SegmentDisplay {
public:
    // Draw a single digit using 7-segment style
    static void drawDigit(NanoVG* nvg, float x, float y, float w, float h, int digit, 
                          float r, float g, float b) {
        // Segment patterns for digits 0-9
        // Segments: top, top-right, bottom-right, bottom, bottom-left, top-left, middle
        static const bool segs[10][7] = {
            {true,  true,  true,  true,  true,  true,  false}, // 0
            {false, true,  true,  false, false, false, false}, // 1
            {true,  true,  false, true,  true,  false, true},  // 2
            {true,  true,  true,  true,  false, false, true},  // 3
            {false, true,  true,  false, false, true,  true},  // 4
            {true,  false, true,  true,  false, true,  true},  // 5
            {true,  false, true,  true,  true,  true,  true},  // 6
            {true,  true,  true,  false, false, false, false}, // 7
            {true,  true,  true,  true,  true,  true,  true},  // 8
            {true,  true,  true,  true,  false, true,  true},  // 9
        };
        
        if (digit < 0 || digit > 9) return;
        
        float sw = w * 0.15f;  // Segment thickness
        float gap = w * 0.05f; // Gap between segments
        float halfH = (h - sw) / 2.0f;
        
        nvg->fillColor(r, g, b);
        
        // Top segment (0)
        if (segs[digit][0]) {
            nvg->beginPath();
            nvg->rect(x + gap + sw, y, w - 2*gap - 2*sw, sw);
            nvg->fill();
        }
        
        // Top-right segment (1)
        if (segs[digit][1]) {
            nvg->beginPath();
            nvg->rect(x + w - sw - gap, y + gap + sw, sw, halfH - gap - sw);
            nvg->fill();
        }
        
        // Bottom-right segment (2)
        if (segs[digit][2]) {
            nvg->beginPath();
            nvg->rect(x + w - sw - gap, y + halfH + gap, sw, halfH - gap - sw);
            nvg->fill();
        }
        
        // Bottom segment (3)
        if (segs[digit][3]) {
            nvg->beginPath();
            nvg->rect(x + gap + sw, y + h - sw, w - 2*gap - 2*sw, sw);
            nvg->fill();
        }
        
        // Bottom-left segment (4)
        if (segs[digit][4]) {
            nvg->beginPath();
            nvg->rect(x + gap, y + halfH + gap, sw, halfH - gap - sw);
            nvg->fill();
        }
        
        // Top-left segment (5)
        if (segs[digit][5]) {
            nvg->beginPath();
            nvg->rect(x + gap, y + gap + sw, sw, halfH - gap - sw);
            nvg->fill();
        }
        
        // Middle segment (6)
        if (segs[digit][6]) {
            nvg->beginPath();
            nvg->rect(x + gap + sw, y + halfH - sw/2, w - 2*gap - 2*sw, sw);
            nvg->fill();
        }
    }
    
    // Draw minus sign
    static void drawMinus(NanoVG* nvg, float x, float y, float w, float h,
                          float r, float g, float b) {
        float sw = w * 0.15f;
        float gap = w * 0.05f;
        float halfH = (h - sw) / 2.0f;
        
        nvg->fillColor(r, g, b);
        nvg->beginPath();
        nvg->rect(x + gap + sw, y + halfH - sw/2, w - 2*gap - 2*sw, sw);
        nvg->fill();
    }
    
    // Draw decimal point
    static void drawDot(NanoVG* nvg, float x, float y, float w, float h,
                        float r, float g, float b) {
        float sw = w * 0.2f;
        
        nvg->fillColor(r, g, b);
        nvg->beginPath();
        nvg->rect(x + w/2 - sw/2, y + h - sw, sw, sw);
        nvg->fill();
    }
    
    // Draw a number string (supports negative, decimal)
    static void drawNumber(NanoVG* nvg, float cx, float cy, float digitW, float digitH,
                           const char* str, float r, float g, float b) {
        int len = static_cast<int>(strlen(str));
        if (len == 0) return;
        
        float spacing = digitW * 0.15f;
        float dotW = digitW * 0.3f;
        
        // Calculate total width
        float totalW = 0;
        for (int i = 0; i < len; i++) {
            if (str[i] == '.') {
                totalW += dotW;
            } else {
                totalW += digitW;
            }
            if (i < len - 1) totalW += spacing;
        }
        
        float x = cx - totalW / 2;
        float y = cy - digitH / 2;
        
        for (int i = 0; i < len; i++) {
            char c = str[i];
            float charW = (c == '.') ? dotW : digitW;
            
            if (c == '-') {
                drawMinus(nvg, x, y, charW, digitH, r, g, b);
            } else if (c == '.') {
                drawDot(nvg, x, y, charW, digitH, r, g, b);
            } else if (c >= '0' && c <= '9') {
                drawDigit(nvg, x, y, charW, digitH, c - '0', r, g, b);
            }
            
            x += charW + spacing;
        }
    }
};

// ============================================================================
// UI Constants
// ============================================================================

namespace UIConstants {
    // Colors (RGB 0-1)
    constexpr float BG_COLOR[] = {0.098f, 0.098f, 0.118f};          // #19191E
    constexpr float HEADER_COLOR[] = {0.137f, 0.137f, 0.165f};      // #232329
    constexpr float TEXT_COLOR[] = {0.784f, 0.784f, 0.863f};        // #C8C8DC
    constexpr float TITLE_COLOR[] = {0.902f, 0.902f, 0.941f};       // #E6E6F0
    constexpr float MUTED_COLOR[] = {0.392f, 0.392f, 0.471f};       // #646478
    constexpr float KNOB_ARC_COLOR[] = {0.314f, 0.627f, 0.941f};    // #50A0F0
    constexpr float KNOB_BG_COLOR[] = {0.059f, 0.059f, 0.078f};     // #0F0F14
    constexpr float SLIDER_BG_COLOR[] = {0.059f, 0.059f, 0.078f};   // #0F0F14
    constexpr float ACCENT_COLOR[] = {0.941f, 0.627f, 0.314f};      // #F0A050
    
    // Dimensions
    constexpr int HEADER_HEIGHT = 60;
    constexpr int KNOB_SIZE = 60;
    constexpr int KNOB_SPACING = 90;
    constexpr int SLIDER_WIDTH = 200;
    constexpr int SLIDER_HEIGHT = 24;
    constexpr int PADDING = 20;
    constexpr int SECTION_SPACING = 30;
    
    // Font sizes
    constexpr float TITLE_FONT_SIZE = 24.0f;
    constexpr float LABEL_FONT_SIZE = 13.0f;
    constexpr float VALUE_FONT_SIZE = 11.0f;
    constexpr float UNIT_FONT_SIZE = 9.0f;
    
    // 7-segment display sizes
    constexpr float SEG_DIGIT_WIDTH = 8.0f;
    constexpr float SEG_DIGIT_HEIGHT = 14.0f;
}

// ============================================================================
// Detection method names
// ============================================================================

static const char* const kDetectionMethodNames[] = {
    "Average",
    "Peak",
    "Median",
    "RMS",
    "Trimmed Mean"
};

static const char* const kFFTSizeNames[] = {
    "512 (~5ms)",
    "1024 (~10ms)",
    "2048 (~21ms)",
    "4096 (~42ms)"
};

// ============================================================================
// FrequencyGateUI
// ============================================================================

class FrequencyGateUI : public UI
{
public:
    FrequencyGateUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
        , mFontId(-1)
        , mFontLoaded(false)
    {
        // Initialize parameter values with defaults
        for (int i = 0; i < kParamCount; i++) {
            fParamValues[i] = 0.0f;
        }
        
        fParamValues[kParamFreqLow] = 100.0f;
        fParamValues[kParamFreqHigh] = 500.0f;
        fParamValues[kParamThreshold] = -30.0f;
        fParamValues[kParamDetectionMethod] = 0.0f;
        fParamValues[kParamPreOpen] = 0.0f;
        fParamValues[kParamAttack] = 5.0f;
        fParamValues[kParamHold] = 50.0f;
        fParamValues[kParamRelease] = 100.0f;
        fParamValues[kParamHysteresis] = 3.0f;
        fParamValues[kParamRange] = -96.0f;
        fParamValues[kParamFFTSize] = 2.0f;
        
        mDraggingParam = -1;
        mDragStartY = 0;
        mDragStartValue = 0;
        
        // Initialize hit areas
        for (int i = 0; i < kParamCount; i++) {
            mKnobAreas[i] = {0, 0, 0, 0};
        }
        
        // Try to load font
        tryLoadFont();
    }

protected:
    // --------------------------------------------------------------------------------------------------------
    // DSP/Plugin Callbacks

    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount) {
            fParamValues[index] = value;
            repaint();
        }
    }

    // --------------------------------------------------------------------------------------------------------
    // Font Loading
    
    void tryLoadFont()
    {
        // Method 1: DPF shared resources
        if (loadSharedResources()) {
            mFontId = findFont("sans");
            if (mFontId >= 0) {
                mFontLoaded = true;
                return;
            }
        }
        
        // Method 2: Windows system fonts (REQUIRED FALLBACK)
#ifdef _WIN32
        char winDir[MAX_PATH];
        if (GetWindowsDirectoryA(winDir, MAX_PATH) > 0) {
            char fontPath[MAX_PATH];
            const char* fontFiles[] = {
                "\\Fonts\\segoeui.ttf",
                "\\Fonts\\arial.ttf",
                "\\Fonts\\tahoma.ttf",
                "\\Fonts\\verdana.ttf",
                nullptr
            };
            for (int i = 0; fontFiles[i] != nullptr; i++) {
                std::snprintf(fontPath, MAX_PATH, "%s%s", winDir, fontFiles[i]);
                mFontId = createFontFromFile("ui-font", fontPath);
                if (mFontId >= 0) {
                    mFontLoaded = true;
                    return;
                }
            }
        }
#endif
        
        mFontLoaded = false;
    }

    // --------------------------------------------------------------------------------------------------------
    // Widget Callbacks

    void onNanoDisplay() override
    {
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        
        // Background
        beginPath();
        rect(0, 0, width, height);
        fillColor(UIConstants::BG_COLOR[0], UIConstants::BG_COLOR[1], UIConstants::BG_COLOR[2]);
        fill();
        
        // Header
        beginPath();
        rect(0, 0, width, UIConstants::HEADER_HEIGHT);
        fillColor(UIConstants::HEADER_COLOR[0], UIConstants::HEADER_COLOR[1], UIConstants::HEADER_COLOR[2]);
        fill();
        
        // Title
        drawText(UIConstants::PADDING, UIConstants::HEADER_HEIGHT / 2, "FrequencyGate", 
                 UIConstants::TITLE_FONT_SIZE, UIConstants::TITLE_COLOR, ALIGN_LEFT | ALIGN_MIDDLE);
        
        // Version
        drawText(width - UIConstants::PADDING, UIConstants::HEADER_HEIGHT / 2, "v1.0.0",
                 UIConstants::VALUE_FONT_SIZE, UIConstants::MUTED_COLOR, ALIGN_RIGHT | ALIGN_MIDDLE);
        
        // ========== Frequency Range Section ==========
        float yPos = UIConstants::HEADER_HEIGHT + UIConstants::PADDING;
        drawText(UIConstants::PADDING, yPos + 8, "Detection Frequency Range",
                 UIConstants::LABEL_FONT_SIZE, UIConstants::ACCENT_COLOR, ALIGN_LEFT | ALIGN_TOP);
        yPos += 30;
        
        // Freq Low/High sliders
        float sliderX = UIConstants::PADDING;
        drawFrequencySlider(sliderX, yPos, "Low", kParamFreqLow, 20.0f, 20000.0f);
        sliderX += UIConstants::SLIDER_WIDTH + UIConstants::SECTION_SPACING;
        drawFrequencySlider(sliderX, yPos, "High", kParamFreqHigh, 20.0f, 20000.0f);
        
        yPos += 55;
        
        // ========== Threshold Section ==========
        drawText(UIConstants::PADDING, yPos + 8, "Threshold & Detection",
                 UIConstants::LABEL_FONT_SIZE, UIConstants::ACCENT_COLOR, ALIGN_LEFT | ALIGN_TOP);
        yPos += 30;
        
        float knobX = UIConstants::PADDING + UIConstants::KNOB_SIZE / 2;
        
        // Threshold knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Threshold", "dB", 
                 kParamThreshold, -96.0f, 0.0f, false);
        knobX += UIConstants::KNOB_SPACING;
        
        // Hysteresis knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Hysteresis", "dB",
                 kParamHysteresis, 0.0f, 12.0f, false);
        knobX += UIConstants::KNOB_SPACING;
        
        // Range knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Range", "dB",
                 kParamRange, -96.0f, 0.0f, false);
        knobX += UIConstants::KNOB_SPACING + 30;
        
        // Detection Method dropdown
        drawDropdown(knobX, yPos + 15, "Detection Method", kParamDetectionMethod,
                     kDetectionMethodNames, kDetectCount);
        
        yPos += UIConstants::KNOB_SIZE + 45;
        
        // ========== Envelope Section ==========
        drawText(UIConstants::PADDING, yPos + 8, "Envelope",
                 UIConstants::LABEL_FONT_SIZE, UIConstants::ACCENT_COLOR, ALIGN_LEFT | ALIGN_TOP);
        yPos += 30;
        
        knobX = UIConstants::PADDING + UIConstants::KNOB_SIZE / 2;
        
        // Pre-Open knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Pre-Open", "ms",
                 kParamPreOpen, 0.0f, 20.0f, false);
        knobX += UIConstants::KNOB_SPACING;
        
        // Attack knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Attack", "ms",
                 kParamAttack, 0.1f, 100.0f, true);  // logarithmic
        knobX += UIConstants::KNOB_SPACING;
        
        // Hold knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Hold", "ms",
                 kParamHold, 0.0f, 500.0f, false);
        knobX += UIConstants::KNOB_SPACING;
        
        // Release knob
        drawKnob(knobX, yPos + UIConstants::KNOB_SIZE / 2, "Release", "ms",
                 kParamRelease, 1.0f, 1000.0f, true);  // logarithmic
        
        yPos += UIConstants::KNOB_SIZE + 45;
        
        // ========== FFT Settings Section ==========
        drawText(UIConstants::PADDING, yPos + 8, "Analysis Settings",
                 UIConstants::LABEL_FONT_SIZE, UIConstants::ACCENT_COLOR, ALIGN_LEFT | ALIGN_TOP);
        yPos += 30;
        
        drawDropdown(UIConstants::PADDING, yPos, "FFT Size (Latency)", kParamFFTSize,
                     kFFTSizeNames, kFFTSizeCount);
        
        // Info text
        yPos += 50;
        drawText(UIConstants::PADDING, yPos, 
                 "Larger FFT = better frequency resolution but more latency. 2048 recommended for voice.",
                 UIConstants::VALUE_FONT_SIZE, UIConstants::MUTED_COLOR, ALIGN_LEFT | ALIGN_TOP);
    }
    
    // --------------------------------------------------------------------------------------------------------
    // Text Drawing (with font fallback)
    
    void drawText(float x, float y, const char* str, float size, const float* color, int align)
    {
        if (mFontLoaded && mFontId >= 0) {
            fontFaceId(mFontId);
            fontSize(size);
            fillColor(color[0], color[1], color[2]);
            textAlign(align);
            text(x, y, str, nullptr);
        } else {
            // Fallback: Draw simple text indicator or use segment display for numbers
            fillColor(color[0], color[1], color[2]);
            
            // Check if text is numeric
            bool isNumeric = true;
            int len = static_cast<int>(strlen(str));
            for (int i = 0; i < len; i++) {
                char c = str[i];
                if ((c < '0' || c > '9') && c != '.' && c != '-' && c != ' ') {
                    isNumeric = false;
                    break;
                }
            }
            
            if (isNumeric && len > 0) {
                // Use 7-segment display for numbers
                float digitH = size * 0.9f;
                float digitW = digitH * 0.6f;
                float cx = x, cy = y;
                
                // Adjust position based on alignment
                float totalW = len * (digitW * 0.8f);
                if (align & ALIGN_CENTER) {
                    // Already centered
                } else if (align & ALIGN_RIGHT) {
                    cx = x - totalW / 2;
                } else {
                    cx = x + totalW / 2;
                }
                
                SegmentDisplay::drawNumber(this, cx, cy, digitW, digitH, str,
                                           color[0], color[1], color[2]);
            } else {
                // For non-numeric text, draw a simple underline/indicator
                float textLen = len * size * 0.5f;
                float startX = x;
                float lineY = y;
                
                if (align & ALIGN_CENTER) {
                    startX = x - textLen / 2;
                } else if (align & ALIGN_RIGHT) {
                    startX = x - textLen;
                }
                
                if (align & ALIGN_MIDDLE) {
                    lineY = y;
                } else if (align & ALIGN_BOTTOM) {
                    lineY = y - size * 0.3f;
                } else {
                    lineY = y + size * 0.7f;
                }
                
                // Draw simple text box as fallback indicator
                beginPath();
                rect(startX, lineY - size * 0.4f, textLen, size * 0.8f);
                strokeColor(color[0], color[1], color[2]);
                strokeWidth(1.0f);
                stroke();
            }
        }
    }
    
    // --------------------------------------------------------------------------------------------------------
    // Drawing Helpers
    
    void drawKnob(float cx, float cy, const char* label, const char* unit,
                  int paramIndex, float minVal, float maxVal, bool logarithmic)
    {
        const float radius = UIConstants::KNOB_SIZE / 2.0f - 5.0f;
        const float arcWidth = 4.0f;
        
        // Store hit area
        mKnobAreas[paramIndex] = {cx - radius - 5, cy - radius - 5, 
                                   (radius + 5) * 2, (radius + 5) * 2};
        
        // Get normalized value
        float value = fParamValues[paramIndex];
        float normalized;
        if (logarithmic && minVal > 0) {
            normalized = (std::log(value) - std::log(minVal)) / (std::log(maxVal) - std::log(minVal));
        } else {
            normalized = (value - minVal) / (maxVal - minVal);
        }
        normalized = std::max(0.0f, std::min(1.0f, normalized));
        
        // Background circle
        beginPath();
        circle(cx, cy, radius);
        fillColor(UIConstants::KNOB_BG_COLOR[0], UIConstants::KNOB_BG_COLOR[1], UIConstants::KNOB_BG_COLOR[2]);
        fill();
        
        // Arc background
        const float startAngle = 0.75f * static_cast<float>(M_PI);
        const float endAngle = 2.25f * static_cast<float>(M_PI);
        
        beginPath();
        arc(cx, cy, radius - arcWidth / 2, startAngle, endAngle, CW);
        strokeColor(0.2f, 0.2f, 0.25f);
        strokeWidth(arcWidth);
        stroke();
        
        // Value arc
        if (normalized > 0.001f) {
            float valueAngle = startAngle + normalized * (endAngle - startAngle);
            beginPath();
            arc(cx, cy, radius - arcWidth / 2, startAngle, valueAngle, CW);
            strokeColor(UIConstants::KNOB_ARC_COLOR[0], UIConstants::KNOB_ARC_COLOR[1], UIConstants::KNOB_ARC_COLOR[2]);
            strokeWidth(arcWidth);
            stroke();
        }
        
        // Pointer line
        float pointerAngle = startAngle + normalized * (endAngle - startAngle);
        float pointerX = cx + std::cos(pointerAngle) * (radius - 12);
        float pointerY = cy + std::sin(pointerAngle) * (radius - 12);
        beginPath();
        moveTo(cx, cy);
        lineTo(pointerX, pointerY);
        strokeColor(UIConstants::TEXT_COLOR[0], UIConstants::TEXT_COLOR[1], UIConstants::TEXT_COLOR[2]);
        strokeWidth(2.0f);
        stroke();
        
        // Value text (center of knob)
        char valueStr[32];
        if (std::abs(value) < 10.0f) {
            std::snprintf(valueStr, sizeof(valueStr), "%.1f", value);
        } else {
            std::snprintf(valueStr, sizeof(valueStr), "%.0f", value);
        }
        drawText(cx, cy, valueStr, UIConstants::VALUE_FONT_SIZE, UIConstants::TEXT_COLOR, ALIGN_CENTER | ALIGN_MIDDLE);
        
        // Label below knob
        drawText(cx, cy + radius + 10, label, UIConstants::LABEL_FONT_SIZE, UIConstants::TEXT_COLOR, ALIGN_CENTER | ALIGN_TOP);
        
        // Unit below label
        drawText(cx, cy + radius + 25, unit, UIConstants::UNIT_FONT_SIZE, UIConstants::MUTED_COLOR, ALIGN_CENTER | ALIGN_TOP);
    }
    
    void drawFrequencySlider(float x, float y, const char* label, int paramIndex, 
                             float minVal, float maxVal)
    {
        const float sliderWidth = UIConstants::SLIDER_WIDTH;
        const float sliderHeight = UIConstants::SLIDER_HEIGHT;
        
        // Store hit area
        mKnobAreas[paramIndex] = {x, y, sliderWidth, sliderHeight};
        
        // Get value (logarithmic for frequency)
        float value = fParamValues[paramIndex];
        float normalized = (std::log(value) - std::log(minVal)) / (std::log(maxVal) - std::log(minVal));
        normalized = std::max(0.0f, std::min(1.0f, normalized));
        
        // Label above slider
        drawText(x, y - 18, label, UIConstants::LABEL_FONT_SIZE, UIConstants::TEXT_COLOR, ALIGN_LEFT | ALIGN_BOTTOM);
        
        // Background
        beginPath();
        roundedRect(x, y, sliderWidth, sliderHeight, 4);
        fillColor(UIConstants::SLIDER_BG_COLOR[0], UIConstants::SLIDER_BG_COLOR[1], UIConstants::SLIDER_BG_COLOR[2]);
        fill();
        
        // Fill bar
        if (normalized > 0.001f) {
            beginPath();
            roundedRect(x, y, sliderWidth * normalized, sliderHeight, 4);
            fillColor(UIConstants::KNOB_ARC_COLOR[0], UIConstants::KNOB_ARC_COLOR[1], UIConstants::KNOB_ARC_COLOR[2]);
            fill();
        }
        
        // Value text
        char valueStr[32];
        if (value >= 1000.0f) {
            std::snprintf(valueStr, sizeof(valueStr), "%.1f kHz", value / 1000.0f);
        } else {
            std::snprintf(valueStr, sizeof(valueStr), "%.0f Hz", value);
        }
        drawText(x + sliderWidth / 2, y + sliderHeight / 2, valueStr, 
                 UIConstants::VALUE_FONT_SIZE, UIConstants::TITLE_COLOR, ALIGN_CENTER | ALIGN_MIDDLE);
    }
    
    void drawDropdown(float x, float y, const char* label, int paramIndex,
                      const char* const* names, int nameCount)
    {
        const float dropWidth = 160;
        const float dropHeight = 28;
        
        // Store hit area
        mKnobAreas[paramIndex] = {x, y, dropWidth, dropHeight};
        
        // Label above dropdown
        drawText(x, y - 18, label, UIConstants::LABEL_FONT_SIZE, UIConstants::TEXT_COLOR, ALIGN_LEFT | ALIGN_BOTTOM);
        
        // Background
        beginPath();
        roundedRect(x, y, dropWidth, dropHeight, 4);
        fillColor(UIConstants::SLIDER_BG_COLOR[0], UIConstants::SLIDER_BG_COLOR[1], UIConstants::SLIDER_BG_COLOR[2]);
        fill();
        
        // Border
        strokeColor(UIConstants::MUTED_COLOR[0], UIConstants::MUTED_COLOR[1], UIConstants::MUTED_COLOR[2]);
        strokeWidth(1.0f);
        stroke();
        
        // Get label for current value
        int intValue = static_cast<int>(fParamValues[paramIndex] + 0.5f);
        if (intValue >= 0 && intValue < nameCount) {
            drawText(x + 10, y + dropHeight / 2, names[intValue],
                     UIConstants::VALUE_FONT_SIZE, UIConstants::TEXT_COLOR, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        
        // Dropdown arrow
        beginPath();
        moveTo(x + dropWidth - 18, y + dropHeight / 2 - 3);
        lineTo(x + dropWidth - 10, y + dropHeight / 2 + 3);
        lineTo(x + dropWidth - 2, y + dropHeight / 2 - 3);
        strokeColor(UIConstants::TEXT_COLOR[0], UIConstants::TEXT_COLOR[1], UIConstants::TEXT_COLOR[2]);
        strokeWidth(1.5f);
        stroke();
    }
    
    // --------------------------------------------------------------------------------------------------------
    // Mouse Handling
    
    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        
        if (ev.press) {
            for (int i = 0; i < kParamCount; i++) {
                const auto& area = mKnobAreas[i];
                if (area.w <= 0) continue;
                
                if (ev.pos.getX() >= area.x && ev.pos.getX() < area.x + area.w &&
                    ev.pos.getY() >= area.y && ev.pos.getY() < area.y + area.h) {
                    
                    // Dropdowns: cycle through values
                    if (i == kParamDetectionMethod) {
                        int newVal = (static_cast<int>(fParamValues[i]) + 1) % kDetectCount;
                        fParamValues[i] = static_cast<float>(newVal);
                        setParameterValue(i, fParamValues[i]);
                        repaint();
                        return true;
                    }
                    if (i == kParamFFTSize) {
                        int newVal = (static_cast<int>(fParamValues[i]) + 1) % kFFTSizeCount;
                        fParamValues[i] = static_cast<float>(newVal);
                        setParameterValue(i, fParamValues[i]);
                        repaint();
                        return true;
                    }
                    
                    // Start dragging
                    mDraggingParam = i;
                    mDragStartY = ev.pos.getY();
                    mDragStartValue = fParamValues[i];
                    return true;
                }
            }
        } else {
            mDraggingParam = -1;
        }
        
        return false;
    }
    
    bool onMotion(const MotionEvent& ev) override
    {
        if (mDraggingParam < 0) return false;
        
        float minVal, maxVal;
        bool logarithmic = false;
        
        switch (mDraggingParam) {
            case kParamFreqLow:
            case kParamFreqHigh:
                minVal = 20.0f; maxVal = 20000.0f; logarithmic = true;
                break;
            case kParamThreshold:
            case kParamRange:
                minVal = -96.0f; maxVal = 0.0f;
                break;
            case kParamPreOpen:
                minVal = 0.0f; maxVal = 20.0f;
                break;
            case kParamAttack:
                minVal = 0.1f; maxVal = 100.0f; logarithmic = true;
                break;
            case kParamHold:
                minVal = 0.0f; maxVal = 500.0f;
                break;
            case kParamRelease:
                minVal = 1.0f; maxVal = 1000.0f; logarithmic = true;
                break;
            case kParamHysteresis:
                minVal = 0.0f; maxVal = 12.0f;
                break;
            default:
                return false;
        }
        
        float deltaY = mDragStartY - ev.pos.getY();
        float newValue;
        
        if (logarithmic && minVal > 0) {
            float startNorm = (std::log(mDragStartValue) - std::log(minVal)) / (std::log(maxVal) - std::log(minVal));
            float newNorm = startNorm + deltaY / 200.0f;
            newNorm = std::max(0.0f, std::min(1.0f, newNorm));
            newValue = std::exp(std::log(minVal) + newNorm * (std::log(maxVal) - std::log(minVal)));
        } else {
            float sensitivity = (maxVal - minVal) / 200.0f;
            newValue = mDragStartValue + deltaY * sensitivity;
        }
        
        newValue = std::max(minVal, std::min(maxVal, newValue));
        fParamValues[mDraggingParam] = newValue;
        setParameterValue(mDraggingParam, newValue);
        repaint();
        
        return true;
    }
    
    bool onScroll(const ScrollEvent& ev) override
    {
        for (int i = 0; i < kParamCount; i++) {
            const auto& area = mKnobAreas[i];
            if (area.w <= 0) continue;
            
            if (ev.pos.getX() >= area.x && ev.pos.getX() < area.x + area.w &&
                ev.pos.getY() >= area.y && ev.pos.getY() < area.y + area.h) {
                
                // Dropdowns
                if (i == kParamDetectionMethod) {
                    int current = static_cast<int>(fParamValues[i]);
                    int newVal = current + (ev.delta.getY() > 0 ? -1 : 1);
                    newVal = std::max(0, std::min(kDetectCount - 1, newVal));
                    fParamValues[i] = static_cast<float>(newVal);
                    setParameterValue(i, fParamValues[i]);
                    repaint();
                    return true;
                }
                if (i == kParamFFTSize) {
                    int current = static_cast<int>(fParamValues[i]);
                    int newVal = current + (ev.delta.getY() > 0 ? -1 : 1);
                    newVal = std::max(0, std::min(kFFTSizeCount - 1, newVal));
                    fParamValues[i] = static_cast<float>(newVal);
                    setParameterValue(i, fParamValues[i]);
                    repaint();
                    return true;
                }
                
                // Knobs/sliders
                float minVal, maxVal;
                bool logarithmic = false;
                
                switch (i) {
                    case kParamFreqLow:
                    case kParamFreqHigh:
                        minVal = 20.0f; maxVal = 20000.0f; logarithmic = true;
                        break;
                    case kParamThreshold:
                    case kParamRange:
                        minVal = -96.0f; maxVal = 0.0f;
                        break;
                    case kParamPreOpen:
                        minVal = 0.0f; maxVal = 20.0f;
                        break;
                    case kParamAttack:
                        minVal = 0.1f; maxVal = 100.0f; logarithmic = true;
                        break;
                    case kParamHold:
                        minVal = 0.0f; maxVal = 500.0f;
                        break;
                    case kParamRelease:
                        minVal = 1.0f; maxVal = 1000.0f; logarithmic = true;
                        break;
                    case kParamHysteresis:
                        minVal = 0.0f; maxVal = 12.0f;
                        break;
                    default:
                        return false;
                }
                
                float newValue;
                if (logarithmic && minVal > 0) {
                    float ratio = std::pow(maxVal / minVal, 0.02f);
                    newValue = fParamValues[i] * (ev.delta.getY() > 0 ? ratio : 1.0f / ratio);
                } else {
                    float step = (maxVal - minVal) / 50.0f;
                    newValue = fParamValues[i] + ev.delta.getY() * step;
                }
                
                newValue = std::max(minVal, std::min(maxVal, newValue));
                fParamValues[i] = newValue;
                setParameterValue(i, newValue);
                repaint();
                return true;
            }
        }
        
        return false;
    }

private:
    // Font
    NanoVG::FontId mFontId;
    bool mFontLoaded;
    
    // Parameters
    float fParamValues[kParamCount];
    
    // UI state
    int mDraggingParam;
    float mDragStartY;
    float mDragStartValue;
    
    // Hit areas
    struct HitArea {
        float x, y, w, h;
    };
    HitArea mKnobAreas[kParamCount];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyGateUI)
};

// ============================================================================
// UI entry point

UI* createUI()
{
    return new FrequencyGateUI();
}

END_NAMESPACE_DISTRHO
