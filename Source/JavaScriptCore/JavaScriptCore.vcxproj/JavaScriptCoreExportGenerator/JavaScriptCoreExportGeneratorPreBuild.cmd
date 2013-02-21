%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%

echo Preparing generator output directory...
set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/JavaScriptCoreExportGenerator
mkdir "%GeneratorDirectory%" 2>NUL
mkdir "%GeneratorDirectory%\DerivedSources" 2>NUL

echo Clearing old definition file...
del /F /Q "%GeneratorDirectory%\JavaScriptCoreExports.def"
del /F /Q "%GeneratorDirectory%\DerivedSources\JavaScriptCoreExportGenerator.cpp"
del /F /Q "%OUTDIR%\JavaScriptCoreExportGenerator%DEBUGSUFFIX%.exe"

cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" grep XX%PROJECTNAME%XX "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"

"%PROJECTDIR%\%PROJECTNAME%BuildCmd.cmd"
