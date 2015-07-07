%SystemDrive%\cygwin\bin\which.exe perl
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" perl -wnle "if (/XX%PROJECTNAME%XX/) { print } else { exit 1 }" "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"

perl "%WEBKIT_LIBRARIES%\tools\scripts\auto-version.pl" "%INTDIR%"
