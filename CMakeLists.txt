cmake_minimum_required(VERSION 3.22)
project(Silina VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(CTest)
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(JUCE
  URL https://codeload.github.com/juce-framework/JUCE/tar.gz/51a8a6d7aeae7326956d747737ccf1575e61e209
  URL_HASH SHA256=20b740351cc030086951d87614bc08b42d072f1e4db68db1ef1d988e66dbbdbe)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Silina
  COMPANY_NAME "Voyagersquare"
  BUNDLE_ID "com.voyagersquare.silina"
  PLUGIN_MANUFACTURER_CODE Vysq
  PLUGIN_CODE Slna
  PRODUCT_NAME "Silina"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Distortion
  FORMATS VST3 Standalone)

juce_generate_juce_header(Silina)
target_sources(Silina PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/TapeEngine.cpp)
target_compile_definitions(Silina PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_REPORT_APP_USAGE=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Silina PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(silina_processor_tests tests/ProcessorTests.cpp)
target_include_directories(silina_processor_tests PRIVATE Source "${juce_SOURCE_DIR}/modules"
  "${CMAKE_CURRENT_BINARY_DIR}/Silina_artefacts/JuceLibraryCode")
target_compile_definitions(silina_processor_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(silina_processor_tests PRIVATE Silina)
add_test(NAME processor_dsp_and_editor COMMAND silina_processor_tests)
