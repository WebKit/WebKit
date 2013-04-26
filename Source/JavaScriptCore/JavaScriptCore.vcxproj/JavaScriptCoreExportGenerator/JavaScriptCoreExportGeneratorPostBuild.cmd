set GeneratorDirectory=%CONFIGURATIONBUILDDIR%\obj32\JavaScriptCoreExportGenerator

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WebKit_Libraries%\bin32\libicuuc%DebugSuffix%.dll" xcopy /y /d "%WebKit_Libraries%\bin32\libicuuc%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WebKit_Libraries%\bin32\icudt46%DebugSuffix%.dll" xcopy /y /d "%WebKit_Libraries%\bin32\icudt46%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"

echo Exporting link definition output (%GeneratorDirectory%\JavaScriptCoreExports.def)
if exist "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe" "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe" > "%GeneratorDirectory%\JavaScriptCoreExports.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
