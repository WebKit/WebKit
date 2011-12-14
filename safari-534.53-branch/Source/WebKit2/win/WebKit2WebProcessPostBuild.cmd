mkdir 2>NUL "%OUTDIR%\..\bin\WebKit2WebProcess.resources"
xcopy /y /d "%PROJECTDIR%\WebKit2WebProcess.resources\*" "%OUTDIR%\..\bin\WebKit2WebProcess.resources"
if exist "%WEBKITLIBRARIESDIR%\tools\VersionStamper\VersionStamper.exe" "%WEBKITLIBRARIESDIR%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
