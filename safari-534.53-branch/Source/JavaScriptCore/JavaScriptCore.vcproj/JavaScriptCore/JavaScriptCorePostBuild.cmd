if exist "%WEBKITLIBRARIESDIR%\tools\VersionStamper\VersionStamper.exe" "%WEBKITLIBRARIESDIR%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
