%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c

if exist "%WEBKITOUTPUTDIR%\buildfailed" grep XX%PROJECTNAME%XX "%WEBKITOUTPUTDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%WEBKITOUTPUTDIR%\buildfailed"

bash "%WEBKITLIBRARIESDIR%\tools\scripts\auto-version.sh" "%INTDIR%"
