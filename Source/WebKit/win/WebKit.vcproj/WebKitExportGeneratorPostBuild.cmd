set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/WebKitExportGenerator
echo Exporting link definition output (%GeneratorDirectory%\WebKit.def)
if exist "%OUTDIR%\WebKitExportGenerator.exe" "%OUTDIR%\WebKitExportGenerator.exe" > "%GeneratorDirectory%\WebKit.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
