%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%WEBKITOUTPUTDIR%\buildfailed" grep XX%PROJECTNAME%XX "%WEBKITOUTPUTDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%WEBKITOUTPUTDIR%\buildfailed"

mkdir 2>NUL "%WEBKITOUTPUTDIR%\include\DumpRenderTree"
mkdir 2>NUL "%WEBKITOUTPUTDIR%\include\DumpRenderTree\ForwardingHeaders"
mkdir 2>NUL "%WEBKITOUTPUTDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"

xcopy /y /d "%PROJECTDIR%\..\ForwardingHeaders\wtf\*.h" "%WEBKITOUTPUTDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"

if "%CONFIGURATIONNAME%"=="Debug_Cairo_CFLite" xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\ForwardingHeaders\wtf\MD5.h" "%WEBKITOUTPUTDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"
if "%CONFIGURATIONNAME%"=="Release_Cairo_CFLite" xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\ForwardingHeaders\wtf\MD5.h" "%WEBKITOUTPUTDIR%\include\DumpRenderTree\ForwardingHeaders\wtf"

if "%CONFIGURATIONNAME%"=="Debug_Cairo" xcopy /y /d "%TARGETDIR%\..\include\WebCore\ForwardingHeaders\wtf\MD5.h"
if "%CONFIGURATIONNAME%"=="Release_Cairo" xcopy /y /d "%TARGETDIR%\..\include\WebCore\ForwardingHeaders\wtf\MD5.h"
