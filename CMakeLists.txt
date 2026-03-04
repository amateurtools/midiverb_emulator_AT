cmake_minimum_required(VERSION 3.22)
project(VerbVerb VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DJUCE_WEB_BROWSER=0)
add_subdirectory(JUCE) # JUCE source in ./JUCE

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(APPLE)
    find_package(CURL)
    if(CURL_FOUND)
        message(STATUS "libcurl found on macOS, will link.")
        set(PLATFORM_LIBS CURL::libcurl)
    else()
        message(STATUS "libcurl not found on macOS, will not link.")
        set(PLATFORM_LIBS "")
    endif()
elseif(NOT APPLE)
    set(PLATFORM_LIBS curl)
endif()

# --- GTK3 workaround: force CMake to find and use GTK3 via pkg-config (Linux only) ---
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})
    add_definitions(${GTK3_CFLAGS_OTHER})
endif()

juce_add_plugin(VerbVerb
    COMPANY_NAME "AmateurTools"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Amat
    PLUGIN_CODE vBvB
    FORMATS VST3 AU
    PRODUCT_NAME "VerbVerbI"
)

target_sources(VerbVerb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp

    PUBLIC
        Source/GuiHelpers.h
        Source/PluginEditor.h
        Source/PluginProcessor.h
        Source/Lfo.h 
        Source/Effects.h
        Source/UILayout.h
)

target_include_directories(VerbVerb
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/MIDIVERB_DECOMPILED
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MIDIVERB_DECOMPILED/decompiled-midiverb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MIDIVERB_DECOMPILED/decompiled-midifex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MIDIVERB_DECOMPILED/decompiled-midiverb2.h
    PROPERTIES
        COMPILE_FLAGS "-w"
)

target_compile_definitions(VerbVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

juce_generate_juce_header(VerbVerb)

# BinaryData target for images
juce_add_binary_data(BinaryData
    SOURCES
        BinaryData/AnotherMansTreasureMIBRaw.ttf
        BinaryData/background.png
        BinaryData/tile_image.png
        BinaryData/knob.png
        BinaryData/route_left.png
        BinaryData/route_right.png
        BinaryData/route_parallel.png
        BinaryData/signal_indicator.png 
        BinaryData/toggle_led.png
        BinaryData/flame.png
)

target_link_libraries(VerbVerb PRIVATE
    BinaryData
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Only link curl on non-macOS systems if needed
if(NOT APPLE)
    target_link_libraries(VerbVerb PRIVATE curl)
endif()

# =============================================================================
# Compiler Optimizations (macOS-compatible)
# =============================================================================
# Set optimization flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(APPLE)
        # macOS optimizations - Apple Silicon (ARM) and Intel compatible
        target_compile_options(VerbVerb PRIVATE
            -O3                    # Maximum optimization
            -ffast-math            # Aggressive floating-point optimizations
            -funroll-loops         # Unroll loops for speed
            -fno-math-errno        # Don't set errno for math functions
            -ffinite-math-only     # Assume finite math (no NaN/Inf checks)
        )
        
        # ARM-specific optimizations for Apple Silicon
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            message(STATUS "Building for Apple Silicon (ARM64)")
            target_compile_options(VerbVerb PRIVATE
                -mcpu=apple-m1     # Optimize for Apple Silicon (M1/M2/M3)
            )
        else()
            # Intel Mac optimizations
            message(STATUS "Building for Intel Mac")
            target_compile_options(VerbVerb PRIVATE
                -msse4.2           # Enable SSE 4.2 instructions
            )
        endif()
        
    elseif(MSVC)
        # Visual Studio / Windows
        target_compile_options(VerbVerb PRIVATE
            /O2          # Maximize speed
            /Oi          # Enable intrinsic functions
            /Ot          # Favor fast code
            /GL          # Whole program optimization
            /fp:fast     # Fast floating point (like -ffast-math)
            /arch:AVX2   # Enable AVX2 instructions (or AVX, SSE2 for older CPUs)
        )
        
        # Link-time optimizations
        target_link_options(VerbVerb PRIVATE
            /LTCG        # Link-time code generation
        )
    else()
        # GCC / Clang / Linux
        target_compile_options(VerbVerb PRIVATE
            -O3                    # Maximum optimization
            -march=native          # Optimize for your CPU (use CPU-specific SIMD)
            -ffast-math            # Aggressive floating-point optimizations
            -funroll-loops         # Unroll loops for speed
            -fno-math-errno        # Don't set errno for math functions
            -ffinite-math-only     # Assume finite math (no NaN/Inf checks)
        )
    endif()
    
    # Enable interprocedural optimization (IPO/LTO) if supported
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_target_properties(VerbVerb PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
        )
        message(STATUS "IPO/LTO enabled for VerbVerb")
    else()
        message(STATUS "IPO/LTO not supported: ${ipo_error}")
    endif()
else()
    # Debug build - minimal optimizations, better debugging
    message(STATUS "Building in Debug mode - optimizations disabled")
endif()

# =============================================================================
# JUCE-specific optimizations
# =============================================================================
# Enable JUCE's SIMD optimizations
target_compile_definitions(VerbVerb PRIVATE
    JUCE_USE_SIMD=1                    # Enable JUCE SIMD where available
)

# Enable vDSP on macOS for accelerated DSP operations
if(APPLE)
    target_compile_definitions(VerbVerb PRIVATE
        JUCE_DSP_USE_VDSP_FRAMEWORK=1  # Use Apple's Accelerate framework
    )
endif()

# Disable debug features in Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(VerbVerb PRIVATE
        JUCE_DISABLE_ASSERTIONS=1      # Remove assertion checks
        NDEBUG=1                       # Standard release flag
    )
endif()

# =============================================================================
# Universal Binary for macOS (Intel + Apple Silicon)
# =============================================================================
if(APPLE)
    # Build universal binary that runs on both Intel and Apple Silicon
    set_target_properties(VerbVerb PROPERTIES
        OSX_ARCHITECTURES "arm64;x86_64"
    )
    message(STATUS "Building Universal Binary (arm64 + x86_64)")
endif()

# =============================================================================
# Build type message
# =============================================================================
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(NOT CMAKE_BUILD_TYPE)
    message(WARNING "CMAKE_BUILD_TYPE not set! Use: cmake -B build -DCMAKE_BUILD_TYPE=Release")
endif()
