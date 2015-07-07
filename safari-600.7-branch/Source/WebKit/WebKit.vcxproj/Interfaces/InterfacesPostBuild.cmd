%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c

perl FixMIDLHeaders.pl "%CONFIGURATIONBUILDDIR%/include/webkit/"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
