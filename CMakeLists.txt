cmake_minimum_required(VERSION 3.15)
project(FrequencyGate VERSION 1.0.0 LANGUAGES C CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Paths
set(DPF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dpf")
set(PFFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pffft")

# Verify dependencies exist
if(NOT EXISTS "${DPF_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "DPF not found at ${DPF_DIR}. Please run build.ps1 first to download dependencies.")
endif()

if(NOT EXISTS "${PFFFT_DIR}/pffft.c")
    message(FATAL_ERROR "PFFFT not found at ${PFFFT_DIR}. Please run build.ps1 first to download dependencies.")
endif()

# ============================================================================
# PFFFT Library
# ============================================================================

add_library(pffft STATIC
    "${PFFFT_DIR}/pffft.c"
)

# Note: pffft_alloc.c is not needed for newer PFFFT versions
# which include pffft_aligned_malloc/free internally

target_include_directories(pffft PUBLIC "${PFFFT_DIR}")
target_compile_definitions(pffft PRIVATE _USE_MATH_DEFINES)

# Enable SIMD for PFFFT
if(MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("/arch:AVX" HAS_AVX)
    if(HAS_AVX)
        target_compile_options(pffft PRIVATE /arch:AVX)
    endif()
else()
    # GCC/Clang
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-msse2" HAS_SSE2)
    if(HAS_SSE2)
        target_compile_options(pffft PRIVATE -msse2)
    endif()
endif()

# ============================================================================
# DPF Framework
# ============================================================================

# CRITICAL: Set DPF_ROOT_DIR before adding subdirectory (CMake 4.0 compatibility)
set(DPF_ROOT_DIR "${DPF_DIR}" CACHE PATH "DPF root directory" FORCE)
add_subdirectory("${DPF_DIR}" dpf_build)

# ============================================================================
# FrequencyGate Plugin
# ============================================================================

dpf_add_plugin(FrequencyGate
    TARGETS vst2 vst3
    FILES_DSP
        "${CMAKE_CURRENT_SOURCE_DIR}/FrequencyGatePlugin.cpp"
    FILES_UI
        "${CMAKE_CURRENT_SOURCE_DIR}/FrequencyGateUI.cpp"
)

# Link PFFFT to DSP target
target_link_libraries(FrequencyGate-dsp PUBLIC pffft)
target_include_directories(FrequencyGate-dsp PUBLIC 
    "${PFFFT_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(FrequencyGate-dsp PRIVATE USE_PFFFT=1)

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(FrequencyGate-dsp PRIVATE
        _USE_MATH_DEFINES
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_compile_definitions(FrequencyGate-ui PRIVATE
        _USE_MATH_DEFINES
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# Include directories for UI
target_include_directories(FrequencyGate-ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ============================================================================
# Installation
# ============================================================================

# Install to common VST folders on Windows
if(WIN32)
    set(VST3_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/VST3" CACHE PATH "VST3 installation directory")
    set(VST2_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/Steinberg/VST" CACHE PATH "VST2 installation directory")

    # Custom target to show build info for VST2
    add_custom_command(TARGET FrequencyGate-vst2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "VST2 built: $<TARGET_FILE:FrequencyGate-vst2>"
    )

    # Custom target to show build info for VST3
    add_custom_command(TARGET FrequencyGate-vst3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "VST3 built: $<TARGET_FILE:FrequencyGate-vst3>"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "FrequencyGate Configuration:")
message(STATUS "  DPF Path: ${DPF_DIR}")
message(STATUS "  PFFFT Path: ${PFFFT_DIR}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
