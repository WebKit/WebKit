set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/WebKit2ExportGenerator
if exist "%OUTDIR%\..\bin\WebKit2ExportGenerator.exe" "%OUTDIR%\..\bin\WebKit2ExportGenerator.exe" > "%GeneratorDirectory%\WebKit2.def"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
