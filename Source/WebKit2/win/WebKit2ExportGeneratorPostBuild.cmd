set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/WebKit2ExportGenerator
echo Exporting link definition output (%GeneratorDirectory%\WebKit2.def)
if exist "%OUTDIR%\WebKit2ExportGenerator.exe" "%OUTDIR%\WebKit2ExportGenerator.exe" > "%GeneratorDirectory%\WebKit2.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
