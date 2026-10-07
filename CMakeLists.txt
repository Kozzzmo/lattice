cmake_minimum_required(VERSION 3.22)
project(Lattice VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LATTICE_BUILD_TESTS "Build unit tests" ON)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt)
    add_subdirectory(libs/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if (MSVC)
    add_compile_options(/utf-8 /bigobj)
endif()

juce_add_binary_data(LatticeAssets
    HEADER_NAME LatticeAssets.h
    NAMESPACE LatticeAssets
    SOURCES
        resources/fonts/JetBrainsMono-400.ttf
        resources/fonts/JetBrainsMono-700.ttf
        resources/fonts/Sora-400.ttf
        resources/fonts/Sora-600.ttf
        resources/fonts/Sora-700.ttf)

set(LATTICE_CORE_SOURCES
    src/core/Model.cpp
    src/core/Sequencer.cpp
    src/core/Serializer.cpp
    src/core/Generators.cpp
    src/core/Document.cpp
    src/synth/ChipSynth.cpp)

juce_add_plugin(Lattice
    COMPANY_NAME "Lattice Audio"
    COMPANY_WEBSITE "https://example.invalid"
    BUNDLE_ID "audio.lattice.tracker"
    PLUGIN_MANUFACTURER_CODE Ltce
    PLUGIN_CODE Ltc1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Lattice"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    DESCRIPTION "Generative tracker instrument")

target_sources(Lattice PRIVATE
    ${LATTICE_CORE_SOURCES}
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/ui/Theme.cpp
    src/ui/TopBar.cpp
    src/ui/MatrixPanel.cpp
    src/ui/PatternGrid.cpp
    src/ui/LanePanel.cpp
    src/ui/Inspector.cpp)

target_include_directories(Lattice PRIVATE src)

target_compile_definitions(Lattice PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Lattice
    PRIVATE
        LatticeAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (LATTICE_BUILD_TESTS)
    juce_add_console_app(LatticeTests PRODUCT_NAME "LatticeTests")
    target_sources(LatticeTests PRIVATE ${LATTICE_CORE_SOURCES} tests/Tests.cpp)
    target_include_directories(LatticeTests PRIVATE src)
    target_compile_definitions(LatticeTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(LatticeTests PRIVATE
        juce::juce_core juce::juce_data_structures juce::juce_audio_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

option(LATTICE_BUILD_TOOLS "Build the offline screenshot / render harness" ON)
if (LATTICE_BUILD_TOOLS AND NOT WIN32)
    juce_add_console_app(LatticeShot PRODUCT_NAME "LatticeShot")
    target_sources(LatticeShot PRIVATE tools/Shot.cpp)
    target_include_directories(LatticeShot PRIVATE src)
    target_compile_definitions(LatticeShot PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(LatticeShot PRIVATE Lattice juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
