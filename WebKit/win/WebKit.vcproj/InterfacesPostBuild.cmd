%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c

perl FixMIDLHeaders.pl "%WEBKITOUTPUTDIR%/include/webkit/"

if exist "%WEBKITOUTPUTDIR%\buildfailed" del "%WEBKITOUTPUTDIR%\buildfailed"
