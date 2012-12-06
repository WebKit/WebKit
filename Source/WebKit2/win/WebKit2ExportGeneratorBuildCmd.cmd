%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c

set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/WebKit2ExportGenerator

echo Generating export definitions
del /F /Q "%GeneratorDirectory%/DerivedSources/WebKit2ExportGenerator.cpp"
bash -c "../../WebCore/make-export-file-generator ./WebKit2.def.in '%GeneratorDirectory%/DerivedSources/WebKit2ExportGenerator.cpp'"
