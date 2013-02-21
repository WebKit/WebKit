set GeneratorDirectory=%CONFIGURATIONBUILDDIR%\obj\JavaScriptCoreExportGenerator

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WebKit_Libraries%\bin\pthreadVC2.dll" xcopy /y /d "%WebKit_Libraries%\bin\pthreadVC2.dll" "%CONFIGURATIONBUILDDIR%\bin"

echo Exporting link definition output (%GeneratorDirectory%\JavaScriptCoreExports.def)
if exist "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe" "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe" > "%GeneratorDirectory%\JavaScriptCoreExports.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
