<#
.SYNOPSIS
    Build script for FrequencyGate VST2/VST3 plugin

.DESCRIPTION
    Downloads dependencies (DPF, PFFFT) and builds the plugin.
    Builds both VST2 and VST3 formats.

.PARAMETER Clean
    Remove build artifacts and dependencies before building

.PARAMETER Release
    Build in Release mode (default is RelWithDebInfo)

.PARAMETER Install
    Copy the built plugins to the system VST folders

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Clean
    .\build.ps1 -Release -Install
#>

param(
    [switch]$Clean,
    [switch]$Release,
    [switch]$Install
)

$ErrorActionPreference = "Stop"
$buildType = if ($Release) { "Release" } else { "RelWithDebInfo" }

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "FrequencyGate Build Script" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build, dpf, pffft -ErrorAction SilentlyContinue
    Write-Host "Clean complete." -ForegroundColor Green
    Write-Host ""
}

# Check for git
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Git is not installed or not in PATH" -ForegroundColor Red
    exit 1
}

# Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake is not installed or not in PATH" -ForegroundColor Red
    exit 1
}

# Clone DPF
if (-not (Test-Path "dpf")) {
    Write-Host "Downloading DPF (DISTRHO Plugin Framework)..." -ForegroundColor Yellow
    git clone --recursive https://github.com/DISTRHO/DPF.git dpf
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to clone DPF" -ForegroundColor Red
        exit 1
    }
    
    # CRITICAL: Apply CMake 4.0 compatibility patch
    Write-Host "Applying CMake 4.0 compatibility patch..." -ForegroundColor Yellow
    $dpfPluginCmake = "dpf/cmake/DPF-plugin.cmake"
    if (Test-Path $dpfPluginCmake) {
        $content = Get-Content $dpfPluginCmake -Raw
        
        if ($content -notmatch 'DPF_ROOT_DIR') {
            $patch = @'
# CMake 4.0 compatibility patch - added by build script
if(NOT DEFINED DPF_ROOT_DIR)
    get_filename_component(DPF_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

'@
            $content = $patch + $content
            
            # Replace problematic path references
            $content = $content -replace '\$\{CMAKE_CURRENT_LIST_DIR\}/\.\./','${DPF_ROOT_DIR}/'
            
            Set-Content $dpfPluginCmake $content -NoNewline
            Write-Host "Patch applied successfully." -ForegroundColor Green
        } else {
            Write-Host "Patch already applied or not needed." -ForegroundColor Green
        }
    }
    
    Write-Host "DPF downloaded." -ForegroundColor Green
} else {
    # Check if submodules are initialized
    if (-not (Test-Path "dpf/dgl/src/pugl-upstream/include/pugl/pugl.h")) {
        Write-Host "Initializing DPF submodules..." -ForegroundColor Yellow
        Push-Location dpf
        git submodule update --init --recursive
        Pop-Location
        Write-Host "Submodules initialized." -ForegroundColor Green
    }
    Write-Host "DPF already exists, skipping download." -ForegroundColor Gray
}

# Clone PFFFT
if (-not (Test-Path "pffft")) {
    Write-Host "Downloading PFFFT..." -ForegroundColor Yellow
    git clone --depth 1 https://bitbucket.org/jpommier/pffft.git pffft
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: Bitbucket clone failed, trying GitHub mirror..." -ForegroundColor Yellow
        git clone --depth 1 https://github.com/marton78/pffft.git pffft
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Failed to clone PFFFT" -ForegroundColor Red
            exit 1
        }
    }
    
    Write-Host "PFFFT downloaded." -ForegroundColor Green
} else {
    Write-Host "PFFFT already exists, skipping download." -ForegroundColor Gray
}

Write-Host ""

# Create build directory
Write-Host "Configuring build..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path build | Out-Null
Push-Location build

try {
    # Configure with CMake
    # Try to find Visual Studio
    $vsGenerator = "Visual Studio 17 2022"  # VS 2022
    
    # Check if VS 2022 is available, fall back to VS 2019
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        if ($vsPath -match "2019") {
            $vsGenerator = "Visual Studio 16 2019"
        }
    }
    
    Write-Host "Using generator: $vsGenerator" -ForegroundColor Cyan
    
    cmake .. -G $vsGenerator -A x64 -DCMAKE_BUILD_TYPE=$buildType
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: CMake configuration failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host ""
    Write-Host "Building ($buildType)..." -ForegroundColor Yellow
    
    cmake --build . --config $buildType --parallel
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    
} finally {
    Pop-Location
}

# Find the built plugins
$vst2File = Get-ChildItem -Path "build" -Recurse -Filter "FrequencyGate.dll" -File |
            Where-Object { $_.DirectoryName -match "vst2" } |
            Select-Object -First 1
$vst3File = Get-ChildItem -Path "build" -Recurse -Filter "*.vst3" -Directory | Select-Object -First 1

Write-Host ""
if ($vst2File) {
    Write-Host "Built VST2: $($vst2File.FullName)" -ForegroundColor Cyan
}
if ($vst3File) {
    Write-Host "Built VST3: $($vst3File.FullName)" -ForegroundColor Cyan
}

# Install if requested
if ($Install) {
    # Install VST2
    if ($vst2File) {
        $vst2DestDir = "$env:COMMONPROGRAMFILES\Steinberg\VST"

        if (-not (Test-Path $vst2DestDir)) {
            Write-Host "Creating VST2 directory: $vst2DestDir" -ForegroundColor Yellow
            New-Item -ItemType Directory -Force -Path $vst2DestDir | Out-Null
        }

        $vst2DestPath = Join-Path $vst2DestDir $vst2File.Name

        Write-Host "Installing VST2 to: $vst2DestPath" -ForegroundColor Yellow

        # Remove existing installation
        if (Test-Path $vst2DestPath) {
            Remove-Item -Force $vst2DestPath
        }

        Copy-Item -Path $vst2File.FullName -Destination $vst2DestPath -Force

        Write-Host "VST2 installation complete!" -ForegroundColor Green
    }

    # Install VST3
    if ($vst3File) {
        $vst3DestDir = "$env:COMMONPROGRAMFILES\VST3"

        if (-not (Test-Path $vst3DestDir)) {
            Write-Host "Creating VST3 directory: $vst3DestDir" -ForegroundColor Yellow
            New-Item -ItemType Directory -Force -Path $vst3DestDir | Out-Null
        }

        $vst3DestPath = Join-Path $vst3DestDir $vst3File.Name

        Write-Host "Installing VST3 to: $vst3DestPath" -ForegroundColor Yellow

        # Remove existing installation
        if (Test-Path $vst3DestPath) {
            Remove-Item -Recurse -Force $vst3DestPath
        }

        Copy-Item -Path $vst3File.FullName -Destination $vst3DestDir -Recurse -Force

        Write-Host "VST3 installation complete!" -ForegroundColor Green
    }

    Write-Host ""
    if ($vst2File) {
        Write-Host "VST2 installed to: $env:COMMONPROGRAMFILES\Steinberg\VST\$($vst2File.Name)" -ForegroundColor Cyan
    }
    if ($vst3File) {
        Write-Host "VST3 installed to: $env:COMMONPROGRAMFILES\VST3\$($vst3File.Name)" -ForegroundColor Cyan
    }
} else {
    Write-Host ""
    Write-Host "To install, run: .\build.ps1 -Install" -ForegroundColor Gray
    Write-Host "VST2 folder: $env:COMMONPROGRAMFILES\Steinberg\VST" -ForegroundColor Gray
    Write-Host "VST3 folder: $env:COMMONPROGRAMFILES\VST3" -ForegroundColor Gray
}

if (-not $vst2File -and -not $vst3File) {
    Write-Host "WARNING: Could not find built plugin files" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Done!" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
