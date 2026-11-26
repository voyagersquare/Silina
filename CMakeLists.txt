cmake_minimum_required(VERSION 3.22)
project(Sila VERSION 0.3.0 LANGUAGES C CXX)
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

juce_add_plugin(Sila
  COMPANY_NAME "Voyagersquare"
  BUNDLE_ID "com.voyagersquare.sila"
  PLUGIN_MANUFACTURER_CODE Vysq
  PLUGIN_CODE Slna
  PRODUCT_NAME "Sila"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Distortion
  FORMATS VST3 Standalone)

juce_generate_juce_header(Sila)
target_sources(Sila PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/TapeEngine.cpp)
juce_add_binary_data(SilaFonts NAMESPACE SilaFonts
  HEADER_NAME SilaFonts.h SOURCES Assets/Fonts/CoveredByYourGrace.ttf)
target_link_libraries(Sila PRIVATE SilaFonts)
target_compile_definitions(Sila PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_REPORT_APP_USAGE=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Sila PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(sila_processor_tests tests/ProcessorTests.cpp)
target_include_directories(sila_processor_tests PRIVATE Source "${juce_SOURCE_DIR}/modules"
  "${CMAKE_CURRENT_BINARY_DIR}/Sila_artefacts/JuceLibraryCode")
target_compile_definitions(sila_processor_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(sila_processor_tests PRIVATE Sila)
add_test(NAME processor_dsp_and_editor COMMAND sila_processor_tests)

add_executable(sila_benchmark EXCLUDE_FROM_ALL tests/Benchmark.cpp)
target_include_directories(sila_benchmark PRIVATE Source "${juce_SOURCE_DIR}/modules"
  "${CMAKE_CURRENT_BINARY_DIR}/Sila_artefacts/JuceLibraryCode")
target_compile_definitions(sila_benchmark PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(sila_benchmark PRIVATE Sila)
