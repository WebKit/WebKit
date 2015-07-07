perl "%WEBKIT_LIBRARIES%\tools\scripts\version-stamp.pl" "%INTDIR%" "%TARGETPATH%"
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
