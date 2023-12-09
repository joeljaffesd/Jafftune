
cmake_minimum_required(VERSION 3.15)

project(plugin VERSION "1.0.1")

add_subdirectory(JUCE)

#juce_set_aax_sdk_path(../aax-sdk-2-5-1)
#juce_set_vst2_sdk_path(../vstsdk2.4)

juce_add_plugin(plugin
    PRODUCT_NAME "Joel"
    COMPANY_NAME "Joel"
    COMPANY_COPYRIGHT "Joel"
    BUNDLE_ID "com.joel.JaffTune"
    PLUGIN_MANUFACTURER_CODE "Jftn"
    PLUGIN_CODE "Tune"
    FORMATS AU VST3 Standalone
    VST2_CATEGORY "kPlugCategEffect"
    VST3_CATEGORIES "Fx"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    AAX_CATEGORY "AAX_ePlugInCategory_Harmonic"
)

juce_generate_juce_header(plugin)

target_sources(plugin
    PRIVATE
    "Source/PluginEditor.h"
    "Source/PluginProcessor.h"
    "Source/PluginProcessor.cpp"
    "Source/PluginEditor.cpp"
)

target_compile_definitions(plugin
    PUBLIC
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)



target_link_libraries(plugin
    PRIVATE
        juce::juce_audio_utils
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
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
