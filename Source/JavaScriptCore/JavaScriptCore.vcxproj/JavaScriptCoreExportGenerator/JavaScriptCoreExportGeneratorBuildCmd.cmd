%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c

set GeneratorDirectory=%CONFIGURATIONBUILDDIR%/obj/JavaScriptCoreExportGenerator

echo Generating export definitions
del /F /Q "%GeneratorDirectory%/DerivedSources/JavaScriptCorGenerator.cpp"
bash -c "${WEBKIT_SOURCE}/WebCore/make-export-file-generator ./JavaScriptCoreExports.def.in '%GeneratorDirectory%/DerivedSources/JavaScriptCoreExportGenerator.cpp'"