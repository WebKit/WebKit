set GeneratorDirectory=%CONFIGURATIONBUILDDIR%\obj\JavaScriptCoreExportGenerator
echo Exporting link definition output (%GeneratorDirectory%\JavaScriptCoreExports.def)
if exist "%OUTDIR%\JavaScriptCoreExportGenerator.exe" "%OUTDIR%\JavaScriptCoreExportGenerator.exe" > "%GeneratorDirectory%\JavaScriptCoreExports.def"

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WebKit_Libraries%\bin\pthreadVC2.dll" xcopy /y /d "%WebKit_Libraries%\bin\pthreadVC2.dll" "%CONFIGURATIONBUILDDIR%\bin"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
