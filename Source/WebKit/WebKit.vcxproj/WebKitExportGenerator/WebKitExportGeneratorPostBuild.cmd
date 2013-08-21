set GeneratorDirectory=%CONFIGURATIONBUILDDIR%\obj%PlatformArchitecture%\WebKitExportGenerator
echo Exporting link definition output (%GeneratorDirectory%\WebKitExports.def)
if exist "%OUTDIR%\WebKitExportGenerator%DEBUGSUFFIX%.exe" "%OUTDIR%\WebKitExportGenerator%DEBUGSUFFIX%.exe" > "%GeneratorDirectory%\WebKitExports.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
