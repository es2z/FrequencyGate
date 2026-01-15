<# 
.SYNOPSIS
    Build script for FrequencyGate VST3 plugin

.DESCRIPTION
    Downloads dependencies (DPF, PFFFT) and builds the plugin.
    
.PARAMETER Clean
    Remove build artifacts and dependencies before building
    
.PARAMETER Release
    Build in Release mode (default is RelWithDebInfo)
    
.PARAMETER Install
    Copy the built VST3 to the system VST3 folder

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

# Find the built VST3
$vst3File = Get-ChildItem -Path "build" -Recurse -Filter "*.vst3" -Directory | Select-Object -First 1

if ($vst3File) {
    Write-Host ""
    Write-Host "Built VST3: $($vst3File.FullName)" -ForegroundColor Cyan
    
    # Install if requested
    if ($Install) {
        $destDir = "$env:COMMONPROGRAMFILES\VST3"
        
        if (-not (Test-Path $destDir)) {
            Write-Host "Creating VST3 directory: $destDir" -ForegroundColor Yellow
            New-Item -ItemType Directory -Force -Path $destDir | Out-Null
        }
        
        $destPath = Join-Path $destDir $vst3File.Name
        
        Write-Host "Installing to: $destPath" -ForegroundColor Yellow
        
        # Remove existing installation
        if (Test-Path $destPath) {
            Remove-Item -Recurse -Force $destPath
        }
        
        Copy-Item -Path $vst3File.FullName -Destination $destDir -Recurse -Force
        
        Write-Host "Installation complete!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Plugin installed to: $destPath" -ForegroundColor Cyan
    } else {
        Write-Host ""
        Write-Host "To install, run: .\build.ps1 -Install" -ForegroundColor Gray
        Write-Host "Or manually copy to: $env:COMMONPROGRAMFILES\VST3" -ForegroundColor Gray
    }
} else {
    Write-Host "WARNING: Could not find built VST3 file" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Done!" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
