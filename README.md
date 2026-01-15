# FrequencyGate

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A frequency-selective noise gate VST3 plugin optimized for voice streaming.

[日本語版 README はこちら](README_JP.md)

## Overview

FrequencyGate is a noise gate that monitors only a specified frequency range to determine gate open/close decisions, then applies gating to the full audio spectrum. This approach is more efficient than traditional gates for voice applications, as human voice energy is concentrated in specific frequency bands (typically 100Hz-500Hz for fundamental frequencies).

### Key Features

- **Frequency-Selective Detection**: Monitor only the frequency range you specify for gate triggering
- **Multiple Detection Algorithms**: Average, Peak, Median, RMS, Trimmed Mean
- **Hysteresis**: Separate open/close thresholds to prevent chattering
- **Adjustable FFT Size**: Trade-off between frequency resolution and latency
- **Pre-Open (Lookahead)**: Open the gate before audio arrives to prevent clipping attack transients

---

## Prerequisites & Dependencies

### Required Software

Before building, you must install the following:

| Software | Version | Download |
|----------|---------|----------|
| **Visual Studio** | 2019 or 2022 | [Download](https://visualstudio.microsoft.com/downloads/) |
| **CMake** | 3.15 or later | [Download](https://cmake.org/download/) |
| **Git** | Any recent version | [Download](https://git-scm.com/downloads) |

### Visual Studio Installation Notes

When installing Visual Studio, make sure to select:
- **"Desktop development with C++"** workload
- Or install **"Build Tools for Visual Studio"** if you don't need the full IDE

### Verifying Installation

Open PowerShell and run:

```powershell
# Check CMake
cmake --version

# Check Git
git --version

# Check Visual Studio (should show path)
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
```

### Automatically Downloaded Dependencies

The build script will automatically download:
- **DPF** (DISTRHO Plugin Framework) - Plugin framework
- **PFFFT** (Pretty Fast FFT) - FFT library

You do **not** need to install these manually.

---

## Building

### Quick Start (Windows)

```powershell
# Clone the repository
git clone https://github.com/yourusername/FrequencyGate.git
cd FrequencyGate

# Build
.\build.ps1

# Build and install to system VST3 folder
.\build.ps1 -Install
```

### Build Options

```powershell
# Clean build (removes all generated files and re-downloads dependencies)
.\build.ps1 -Clean

# Release build (optimized, no debug info)
.\build.ps1 -Release

# Release build with installation
.\build.ps1 -Release -Install
```

### Build Output

After a successful build, the VST3 plugin will be located at:
```
build\bin\FrequencyGate.vst3
```

### Installation

#### Automatic
```powershell
.\build.ps1 -Install
```

#### Manual
Copy `FrequencyGate.vst3` folder to:
- **Windows**: `C:\Program Files\Common Files\VST3\`

---

## Usage

### Loading the Plugin

Load FrequencyGate as a VST3 plugin in your DAW or streaming software:
- **OBS Studio**: Add as VST 2.x/3.x plugin filter
- **Reaper**: Insert on track as FX
- **Other DAWs**: Standard VST3 plugin loading

### Parameters

#### Frequency Range
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **Freq Low** | 20 Hz - 20 kHz | 100 Hz | Lower bound of detection range |
| **Freq High** | 20 Hz - 20 kHz | 500 Hz | Upper bound of detection range |

#### Threshold Settings
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **Threshold** | -96 dB to 0 dB | -30 dB | Level at which gate opens |
| **Hysteresis** | 0 dB to 12 dB | 3 dB | Difference between open and close thresholds |
| **Range** | -96 dB to 0 dB | -96 dB | Attenuation when gate is closed |

#### Detection Method
| Method | Description | Best For |
|--------|-------------|----------|
| **Average** | Mean of all magnitudes in range | General voice (default) |
| **Peak** | Maximum magnitude in range | Transient-sensitive |
| **Median** | Middle value, ignores outliers | Noisy environments |
| **RMS** | Root mean square (energy-based) | Consistent levels |
| **Trimmed Mean** | Average excluding top/bottom 10% | Best noise rejection |

#### Envelope
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **Pre-Open** | 0 ms - 20 ms | 0 ms | Lookahead time (adds latency) |
| **Attack** | 0.1 ms - 100 ms | 5 ms | Time to fully open gate |
| **Hold** | 0 ms - 500 ms | 50 ms | Time to keep gate open after signal drops |
| **Release** | 1 ms - 1000 ms | 100 ms | Time to fully close gate |

#### FFT Size
| Option | Latency (approx.) | Frequency Resolution |
|--------|-------------------|---------------------|
| 512 | ~5 ms | Low |
| 1024 | ~10 ms | Medium |
| **2048** | ~21 ms | High (recommended) |
| 4096 | ~42 ms | Very High |

### Recommended Settings for Voice Streaming

```
Freq Low:         100 Hz
Freq High:        500 Hz
Threshold:        -30 dB  (adjust based on your mic/environment)
Detection:        Average or Trimmed Mean
Hysteresis:       3 dB
Pre-Open:         0 ms
Attack:           5 ms
Hold:             50 ms
Release:          100 ms
Range:            -96 dB
FFT Size:         2048
```

---

## How It Works

### Traditional Gate vs FrequencyGate

**Traditional Gate:**
- Monitors the entire frequency spectrum
- Environmental noise across all frequencies can trigger false opens
- Keyboard clicks, AC hum, and other non-voice sounds easily trigger the gate

**FrequencyGate:**
- Monitors only the frequency range where voice fundamentals exist (e.g., 100-500Hz)
- High-frequency noise (keyboard, mouse clicks) is ignored for triggering
- Low-frequency rumble outside the detection range doesn't affect gate decisions
- Result: More reliable voice detection with fewer false triggers

### Signal Flow

```
Input → FFT Analysis → Frequency Band Detection → Gate Decision → Gain Envelope → Output
              ↓
    [Only analyze specified frequency range]
              ↓
    [Apply gate to FULL spectrum]
```

---

## Troubleshooting

### No text visible in UI
The plugin attempts to load system fonts. If fonts fail to load, numeric values will display using a 7-segment style fallback. This is a cosmetic issue and does not affect functionality.

### High CPU usage
- Reduce FFT size (smaller = less CPU but lower frequency resolution)
- The plugin is optimized for real-time use and should have minimal CPU impact

### Gate not responding correctly
1. Check that your voice frequency range is within the Freq Low/High settings
2. Try the "Peak" detection method if "Average" isn't responsive enough
3. Lower the Threshold if the gate isn't opening
4. Increase Hysteresis if the gate is chattering

---

## Technical Details

- **Framework**: DPF (DISTRHO Plugin Framework)
- **FFT Library**: PFFFT (Pretty Fast FFT)
- **UI**: NanoVG-based custom UI
- **Format**: VST3
- **Platforms**: Windows (primary), Linux/macOS (secondary)

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Third-Party Libraries

- [DPF](https://github.com/DISTRHO/DPF) - ISC License
- [PFFFT](https://bitbucket.org/jpommier/pffft/) - BSD-like License

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Acknowledgments

- DISTRHO team for the excellent DPF framework
- Julien Pommier for PFFFT
