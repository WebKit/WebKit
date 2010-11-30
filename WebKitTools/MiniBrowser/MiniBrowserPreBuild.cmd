set PATH=%SystemDrive%\cygwin\bin;%PATH%

if exist "%WEBKITOUTPUTDIR%\buildfailed" grep XX%PROJECTNAME%XX "%WEBKITOUTPUTDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%WEBKITOUTPUTDIR%\buildfailed"
