if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
perl "%WEBKIT_LIBRARIES%\tools\scripts\version-stamp.pl" "%INTDIR%" "%TARGETPATH%"
