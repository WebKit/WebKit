set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/WebKitExportGenerator
echo Exporting link definition output (%GeneratorDirectory%\WebKitExports.def)
if exist "%OUTDIR%\WebKitExportGenerator.exe" "%OUTDIR%\WebKitExportGenerator.exe" > "%GeneratorDirectory%\WebKitExports.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
