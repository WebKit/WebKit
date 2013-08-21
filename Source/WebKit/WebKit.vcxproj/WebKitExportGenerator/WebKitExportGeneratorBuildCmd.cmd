%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c

set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj%PlatformArchitecture%/WebKitExportGenerator

echo Generating export definitions
del /F /Q "%GeneratorDirectory%/DerivedSources/WebKitExportGenerator.cpp"
bash -c "./make-export-file-generator ./WebKitExports.def.in '%GeneratorDirectory%/DerivedSources/WebKitExportGenerator.cpp'"
