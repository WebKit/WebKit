%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" grep XX%PROJECTNAME%XX "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"

set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/WebKit2ExportGenerator
mkdir "%GeneratorDirectory%" 2>NUL
mkdir "%GeneratorDirectory%\DerivedSources" 2>NUL

echo Generating export definitions
bash -c "../../WebCore/make-export-file-generator ./WebKit2.def.in '%GeneratorDirectory%/DerivedSources/WebKit2ExportGenerator.cpp'"