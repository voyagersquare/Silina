@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake nije pronadjen. Instaliraj Visual Studio 2022 s "Desktop development with C++" i CMake alatima.
  exit /b 1
)
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel 2
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 exit /b 1
if not exist "build\Silina_artefacts\Release\VST3\Silina.vst3\Contents\x86_64-win\Silina.vst3" (
  echo Greska: Windows VST3 datoteka nije izgradjena.
  exit /b 1
)
echo Silina je izgradjena. Kopiraj cijelu mapu build\Silina_artefacts\Release\VST3\Silina.vst3 u C:\Program Files\Common Files\VST3
exit /b 0
