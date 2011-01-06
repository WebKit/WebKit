%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" grep XX%PROJECTNAME%XX "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\DumpRenderTree"
mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\DumpRenderTree\ForwardingHeaders"
mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"

xcopy /y /d "%PROJECTDIR%\..\ForwardingHeaders\wtf\*.h" "%CONFIGURATIONBUILDDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"

if "%CONFIGURATIONNAME%"=="Debug_Cairo_CFLite" xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\ForwardingHeaders\wtf\MD5.h" "%CONFIGURATIONBUILDDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"
if "%CONFIGURATIONNAME%"=="Release_Cairo_CFLite" xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\ForwardingHeaders\wtf\MD5.h" "%CONFIGURATIONBUILDDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"

if "%CONFIGURATIONNAME%"=="Debug_Cairo" xcopy /y /d "%TARGETDIR%\..\include\WebCore\ForwardingHeaders\wtf\MD5.h"
if "%CONFIGURATIONNAME%"=="Release_Cairo" xcopy /y /d "%TARGETDIR%\..\include\WebCore\ForwardingHeaders\wtf\MD5.h"
