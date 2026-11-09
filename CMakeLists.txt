cmake_minimum_required(VERSION 3.22)
project(Epsilon808 VERSION 1.1.0 LANGUAGES C CXX)

# macOS: 12.0 target and JUCE 9.0.5 (bundled in ./JUCE in the Projucer package; CI clones it).
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

add_subdirectory(JUCE)
set(e808_formats VST3)
if(APPLE)
    list(APPEND e808_formats AU)
endif()

juce_add_plugin(Epsilon808
    COMPANY_NAME "Epsilon Sounds"
    BUNDLE_ID com.epsilonsounds.e808
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE EpsS
    PLUGIN_CODE E808
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${e808_formats}
    PRODUCT_NAME "Epsilon 808"
    DESCRIPTION "808 and sub bass instrument"
)
juce_generate_juce_header(Epsilon808)
# The layer sounds are whatever sits in Resources/Layers. The public repository holds none (licence);
# the Projucer package has the full set. The plugin runs with any subset: a missing layer is skipped.
file(GLOB e808_layer_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Layers/*.flac)
juce_add_binary_data(BoomAssets SOURCES
    Resources/BarlowSemiCondensed-Medium.ttf
    Resources/BarlowSemiCondensed-SemiBold.ttf
    Resources/BarlowSemiCondensed-Bold.ttf
    ${e808_layer_files}
)
set_target_properties(BoomAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(Epsilon808 PRIVATE
    Source/Boom808Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)
target_compile_features(Epsilon808 PRIVATE cxx_std_17)
target_compile_definitions(Epsilon808 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Epsilon808 PRIVATE
    BoomAssets
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Engine tests (no JUCE needed): cmake --build build --target BoomEngineTests && ctest
option(E808_BUILD_TESTS "Build the DSP engine tests" ON)
if(E808_BUILD_TESTS)
    enable_testing()
    add_executable(BoomEngineTests Tests/EngineTests.cpp Source/Boom808Engine.cpp)
    target_include_directories(BoomEngineTests PRIVATE Source)
    target_compile_features(BoomEngineTests PRIVATE cxx_std_17)
    add_test(NAME BoomEngineTests COMMAND BoomEngineTests)
endif()

# Plugin bench (presets loudness, layers, user samples, state). Off by default:
#   cmake -DE808_BUILD_BENCH=ON ... && cmake --build build --target PluginBench && ./build/PluginBench_artefacts/Release/PluginBench
option(E808_BUILD_BENCH "Build the preset / plugin bench" OFF)
if(E808_BUILD_BENCH)
    juce_add_console_app(PluginBench PRODUCT_NAME "PluginBench")
    target_sources(PluginBench PRIVATE Tests/PluginBench.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Boom808Engine.cpp)
    juce_generate_juce_header(PluginBench)
    target_include_directories(PluginBench PRIVATE Source)
    target_compile_features(PluginBench PRIVATE cxx_std_17)
    target_compile_definitions(PluginBench PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_VersionString="1.1.0" JucePlugin_Name="Epsilon 808")
    target_link_libraries(PluginBench PRIVATE BoomAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
