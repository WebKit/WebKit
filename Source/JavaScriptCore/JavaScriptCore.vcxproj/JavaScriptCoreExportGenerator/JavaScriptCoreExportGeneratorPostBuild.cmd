set GeneratorDirectory=%CONFIGURATIONBUILDDIR%\obj\JavaScriptCoreExportGenerator

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WebKit_Libraries%\bin32\pthreadVC2.dll" xcopy /y /d "%WebKit_Libraries%\bin32\pthreadVC2.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WebKit_Libraries%\bin32\libicuuc.dll" xcopy /y /d "%WebKit_Libraries%\bin32\libicuuc.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WebKit_Libraries%\bin32\icudt46.dll" xcopy /y /d "%WebKit_Libraries%\bin32\icudt46.dll" "%CONFIGURATIONBUILDDIR%\bin"

echo Exporting link definition output (%GeneratorDirectory%\JavaScriptCoreExports.def)
if exist "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe" "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe" > "%GeneratorDirectory%\JavaScriptCoreExports.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
