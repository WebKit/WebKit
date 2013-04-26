if exist "%WEBKIT_LIBRARIES%\tools32\VersionStamper\VersionStamper.exe" "%WEBKIT_LIBRARIES%\tools32\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
