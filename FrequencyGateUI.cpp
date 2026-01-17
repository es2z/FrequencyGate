/*
 * FrequencyGate - NanoVG UI
 * Improved: Larger fonts, numeric input boxes
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

static const char* const kDetectNames[] = {"Average", "Peak", "Median", "RMS", "Trimmed Mean"};
static const char* const kFFTNames[] = {"512", "1024", "2048", "4096"};

class FrequencyGateUI : public UI
{
public:
    FrequencyGateUI()
        : UI(950, 620)
        , mFontId(-1), mFontLoaded(false)
        , mDragging(-1), mDragY(0), mDragVal(0)
    {
        for (int i = 0; i < kParamCount; i++) fP[i] = 0.0f;
        fP[kParamFreqLow] = 100.0f;
        fP[kParamFreqHigh] = 500.0f;
        fP[kParamThreshold] = -30.0f;
        fP[kParamAttack] = 5.0f;
        fP[kParamHold] = 50.0f;
        fP[kParamRelease] = 100.0f;
        fP[kParamHysteresis] = 3.0f;
        fP[kParamRange] = -96.0f;
        fP[kParamFFTSize] = 2.0f;
        
        for (int i = 0; i < kParamCount; i++) mA[i] = {0,0,0,0};
        tryLoadFont();
    }

protected:
    void parameterChanged(uint32_t i, float v) override { if (i < kParamCount) { fP[i] = v; repaint(); } }

    void tryLoadFont() {
        if (loadSharedResources()) { mFontId = findFont("sans"); if (mFontId >= 0) { mFontLoaded = true; return; } }
#ifdef _WIN32
        char wd[MAX_PATH]; if (GetWindowsDirectoryA(wd, MAX_PATH) > 0) {
            const char* fs[] = {"\\Fonts\\segoeui.ttf", "\\Fonts\\arial.ttf", nullptr};
            for (int i = 0; fs[i]; i++) { char p[MAX_PATH]; std::snprintf(p, MAX_PATH, "%s%s", wd, fs[i]);
                mFontId = createFontFromFile("f", p); if (mFontId >= 0) { mFontLoaded = true; return; } } }
#endif
    }

    void onNanoDisplay() override {
        const float W = getWidth(), H = getHeight();
        
        // Background
        beginPath(); rect(0, 0, W, H); fillColor(22, 22, 28); fill();
        
        // Header
        beginPath(); rect(0, 0, W, 60); fillColor(32, 32, 40); fill();
        txt(25, 30, "FrequencyGate", 32, Color(240, 240, 250), ALIGN_LEFT | ALIGN_MIDDLE);
        txt(W - 25, 30, "v1.0", 16, Color(100, 100, 120), ALIGN_RIGHT | ALIGN_MIDDLE);
        
        float y = 80;
        
        // === FREQUENCY SECTION ===
        txt(25, y, "Detection Frequency Range", 18, Color(255, 180, 100), ALIGN_LEFT | ALIGN_TOP);
        y += 35;
        
        // Freq Low
        txt(25, y, "Low Frequency", 16, Color(200, 200, 220), ALIGN_LEFT | ALIGN_TOP);
        drawNumBox(25, y + 25, 200, 50, kParamFreqLow, 20, 20000, true, "Hz");
        
        // Freq High
        txt(260, y, "High Frequency", 16, Color(200, 200, 220), ALIGN_LEFT | ALIGN_TOP);
        drawNumBox(260, y + 25, 200, 50, kParamFreqHigh, 20, 20000, true, "Hz");
        
        y += 100;
        
        // === THRESHOLD SECTION ===
        txt(25, y, "Gate Threshold", 18, Color(255, 180, 100), ALIGN_LEFT | ALIGN_TOP);
        y += 35;
        
        float kx = 60;
        drawKnob(kx, y + 45, "Threshold", "dB", kParamThreshold, -96, 0, false); kx += 120;
        drawKnob(kx, y + 45, "Hysteresis", "dB", kParamHysteresis, 0, 12, false); kx += 120;
        drawKnob(kx, y + 45, "Range", "dB", kParamRange, -96, 0, false); kx += 150;
        
        // Detection dropdown
        txt(kx, y, "Detection Method", 16, Color(200, 200, 220), ALIGN_LEFT | ALIGN_TOP);
        drawDropdown(kx, y + 25, 180, 40, kParamDetectionMethod, kDetectNames, kDetectCount);
        
        y += 140;
        
        // === ENVELOPE SECTION ===
        txt(25, y, "Envelope", 18, Color(255, 180, 100), ALIGN_LEFT | ALIGN_TOP);
        y += 35;
        
        kx = 60;
        drawKnob(kx, y + 45, "Pre-Open", "ms", kParamPreOpen, 0, 20, false); kx += 120;
        drawKnob(kx, y + 45, "Attack", "ms", kParamAttack, 0.1f, 100, true); kx += 120;
        drawKnob(kx, y + 45, "Hold", "ms", kParamHold, 0, 500, false); kx += 120;
        drawKnob(kx, y + 45, "Release", "ms", kParamRelease, 1, 1000, true);
        
        y += 140;
        
        // === FFT SECTION ===
        txt(25, y, "FFT Settings", 18, Color(255, 180, 100), ALIGN_LEFT | ALIGN_TOP);
        y += 35;
        
        txt(25, y, "FFT Size (Latency)", 16, Color(200, 200, 220), ALIGN_LEFT | ALIGN_TOP);
        drawDropdown(25, y + 25, 160, 40, kParamFFTSize, kFFTNames, kFFTSizeCount);
        
        // Info
        txt(220, y + 35, "2048 recommended for voice (~21ms latency)", 14, Color(120, 120, 140), ALIGN_LEFT | ALIGN_MIDDLE);
    }

    void txt(float x, float y, const char* s, float sz, Color c, int a) {
        if (mFontLoaded && mFontId >= 0) {
            fontFaceId(mFontId); fontSize(sz); fillColor(c); textAlign(a); text(x, y, s, nullptr);
        } else {
            fillColor(c);
            float len = strlen(s) * sz * 0.55f;
            float sx = (a & ALIGN_CENTER) ? x - len/2 : (a & ALIGN_RIGHT) ? x - len : x;
            float sy = (a & ALIGN_MIDDLE) ? y - sz/3 : (a & ALIGN_BOTTOM) ? y - sz*0.8f : y;
            beginPath(); rect(sx, sy, len, sz * 0.7f);
            strokeColor(c); strokeWidth(1); stroke();
        }
    }

    void drawNumBox(float x, float y, float w, float h, int p, float mn, float mx, bool lg, const char* unit) {
        mA[p] = {x, y, w, h};
        float v = fP[p];
        float norm = lg ? (std::log(v/mn) / std::log(mx/mn)) : ((v - mn) / (mx - mn));
        norm = std::max(0.0f, std::min(1.0f, norm));
        
        // Box background
        beginPath(); roundedRect(x, y, w, h, 6);
        fillColor(15, 15, 20); fill();
        
        // Progress bar
        if (norm > 0.01f) {
            beginPath(); roundedRect(x + 3, y + 3, (w - 6) * norm, h - 6, 4);
            fillColor(60, 100, 160); fill();
        }
        
        // Border
        beginPath(); roundedRect(x, y, w, h, 6);
        strokeColor(80, 80, 100); strokeWidth(1.5f); stroke();
        
        // Value (LARGE)
        char buf[32];
        if (v >= 1000) std::snprintf(buf, 32, "%.1f k%s", v / 1000.0f, unit);
        else std::snprintf(buf, 32, "%.0f %s", v, unit);
        txt(x + w/2, y + h/2, buf, 20, Color(255, 255, 255), ALIGN_CENTER | ALIGN_MIDDLE);
    }

    void drawKnob(float cx, float cy, const char* lbl, const char* unit, int p, float mn, float mx, bool lg) {
        const float R = 38;
        mA[p] = {cx - R, cy - R, R*2, R*2};
        
        float v = fP[p];
        float norm = lg && mn > 0 ? (std::log(v/mn) / std::log(mx/mn)) : ((v - mn) / (mx - mn));
        norm = std::max(0.0f, std::min(1.0f, norm));
        
        // BG
        beginPath(); circle(cx, cy, R); fillColor(15, 15, 22); fill();
        
        // Arc BG
        float sa = 0.75f * M_PI, ea = 2.25f * M_PI;
        beginPath(); arc(cx, cy, R - 5, sa, ea, CW);
        strokeColor(45, 45, 55); strokeWidth(6); stroke();
        
        // Arc value
        if (norm > 0.01f) {
            beginPath(); arc(cx, cy, R - 5, sa, sa + norm * (ea - sa), CW);
            strokeColor(80, 160, 255); strokeWidth(6); stroke();
        }
        
        // Pointer
        float ang = sa + norm * (ea - sa);
        beginPath(); moveTo(cx, cy);
        lineTo(cx + std::cos(ang) * (R - 12), cy + std::sin(ang) * (R - 12));
        strokeColor(220, 220, 240); strokeWidth(2.5f); stroke();
        
        // Value (center)
        char buf[32];
        if (std::abs(v) < 10) std::snprintf(buf, 32, "%.1f", v);
        else std::snprintf(buf, 32, "%.0f", v);
        txt(cx, cy, buf, 15, Color(220, 220, 240), ALIGN_CENTER | ALIGN_MIDDLE);
        
        // Label & unit
        txt(cx, cy + R + 14, lbl, 15, Color(200, 200, 220), ALIGN_CENTER | ALIGN_TOP);
        txt(cx, cy + R + 32, unit, 13, Color(120, 120, 140), ALIGN_CENTER | ALIGN_TOP);
    }

    void drawDropdown(float x, float y, float w, float h, int p, const char* const* names, int cnt) {
        mA[p] = {x, y, w, h};
        
        beginPath(); roundedRect(x, y, w, h, 5);
        fillColor(15, 15, 22); fill();
        strokeColor(80, 80, 100); strokeWidth(1.5f); stroke();
        
        int idx = static_cast<int>(fP[p] + 0.5f);
        if (idx >= 0 && idx < cnt) {
            txt(x + 15, y + h/2, names[idx], 16, Color(220, 220, 240), ALIGN_LEFT | ALIGN_MIDDLE);
        }
        
        // Arrow
        beginPath();
        moveTo(x + w - 22, y + h/2 - 5);
        lineTo(x + w - 12, y + h/2 + 5);
        lineTo(x + w - 2, y + h/2 - 5);
        strokeColor(160, 160, 180); strokeWidth(2); stroke();
    }

    bool onMouse(const MouseEvent& ev) override {
        if (ev.button != 1) return false;
        if (ev.press) {
            for (int i = 0; i < kParamCount; i++) {
                auto& a = mA[i];
                if (a.w <= 0) continue;
                if (ev.pos.getX() >= a.x && ev.pos.getX() < a.x + a.w &&
                    ev.pos.getY() >= a.y && ev.pos.getY() < a.y + a.h) {
                    
                    if (i == kParamDetectionMethod) {
                        int nv = (static_cast<int>(fP[i]) + 1) % kDetectCount;
                        fP[i] = nv; setParameterValue(i, nv); repaint(); return true;
                    }
                    if (i == kParamFFTSize) {
                        int nv = (static_cast<int>(fP[i]) + 1) % kFFTSizeCount;
                        fP[i] = nv; setParameterValue(i, nv); repaint(); return true;
                    }
                    
                    mDragging = i; mDragY = ev.pos.getY(); mDragVal = fP[i];
                    return true;
                }
            }
        } else { mDragging = -1; }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        if (mDragging < 0) return false;
        
        float mn, mx; bool lg = false;
        switch (mDragging) {
            case kParamFreqLow: case kParamFreqHigh: mn = 20; mx = 20000; lg = true; break;
            case kParamThreshold: case kParamRange: mn = -96; mx = 0; break;
            case kParamPreOpen: mn = 0; mx = 20; break;
            case kParamAttack: mn = 0.1f; mx = 100; lg = true; break;
            case kParamHold: mn = 0; mx = 500; break;
            case kParamRelease: mn = 1; mx = 1000; lg = true; break;
            case kParamHysteresis: mn = 0; mx = 12; break;
            default: return false;
        }
        
        float dy = mDragY - ev.pos.getY();
        float nv;
        if (lg && mn > 0) {
            float sn = std::log(mDragVal/mn) / std::log(mx/mn);
            float nn = sn + dy / 120.0f;
            nn = std::max(0.0f, std::min(1.0f, nn));
            nv = mn * std::pow(mx/mn, nn);
        } else {
            nv = mDragVal + dy * (mx - mn) / 120.0f;
        }
        
        nv = std::max(mn, std::min(mx, nv));
        fP[mDragging] = nv;
        setParameterValue(mDragging, nv);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override {
        for (int i = 0; i < kParamCount; i++) {
            auto& a = mA[i];
            if (a.w <= 0) continue;
            if (ev.pos.getX() >= a.x && ev.pos.getX() < a.x + a.w &&
                ev.pos.getY() >= a.y && ev.pos.getY() < a.y + a.h) {
                
                if (i == kParamDetectionMethod) {
                    int nv = static_cast<int>(fP[i]) + (ev.delta.getY() > 0 ? -1 : 1);
                    nv = std::max(0, std::min(kDetectCount - 1, nv));
                    fP[i] = nv; setParameterValue(i, nv); repaint(); return true;
                }
                if (i == kParamFFTSize) {
                    int nv = static_cast<int>(fP[i]) + (ev.delta.getY() > 0 ? -1 : 1);
                    nv = std::max(0, std::min(kFFTSizeCount - 1, nv));
                    fP[i] = nv; setParameterValue(i, nv); repaint(); return true;
                }
                
                float mn, mx; bool lg = false;
                switch (i) {
                    case kParamFreqLow: case kParamFreqHigh: mn = 20; mx = 20000; lg = true; break;
                    case kParamThreshold: case kParamRange: mn = -96; mx = 0; break;
                    case kParamPreOpen: mn = 0; mx = 20; break;
                    case kParamAttack: mn = 0.1f; mx = 100; lg = true; break;
                    case kParamHold: mn = 0; mx = 500; break;
                    case kParamRelease: mn = 1; mx = 1000; lg = true; break;
                    case kParamHysteresis: mn = 0; mx = 12; break;
                    default: return false;
                }
                
                float nv;
                if (lg && mn > 0) {
                    float r = std::pow(mx/mn, 0.04f);
                    nv = fP[i] * (ev.delta.getY() > 0 ? r : 1.0f/r);
                } else {
                    nv = fP[i] + ev.delta.getY() * (mx - mn) / 25.0f;
                }
                nv = std::max(mn, std::min(mx, nv));
                fP[i] = nv; setParameterValue(i, nv); repaint();
                return true;
            }
        }
        return false;
    }

private:
    NanoVG::FontId mFontId;
    bool mFontLoaded;
    float fP[kParamCount];
    int mDragging;
    float mDragY, mDragVal;
    struct A { float x, y, w, h; };
    A mA[kParamCount];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyGateUI)
};

UI* createUI() { return new FrequencyGateUI(); }

END_NAMESPACE_DISTRHO
