mkdir 2>NUL "%INTDIR%\lib"

if exist "%WEBKITLIBRARIESDIR%\lib\icuuc.lib" copy /y "%WEBKITLIBRARIESDIR%\lib\icuuc.lib" "%INTDIR%\lib\libicuuc.lib"
if exist "%WEBKITLIBRARIESDIR%\lib\icuin.lib" copy /y "%WEBKITLIBRARIESDIR%\lib\icuin.lib" "%INTDIR%\lib\libicuin.lib"

if exist "%WEBKITLIBRARIESDIR%\lib\libicuuc.lib" copy /y "%WEBKITLIBRARIESDIR%\lib\libicuuc.lib" "%INTDIR%\lib"
if exist "%WEBKITLIBRARIESDIR%\lib\libicuin.lib" copy /y "%WEBKITLIBRARIESDIR%\lib\libicuin.lib" "%INTDIR%\lib"

cmd /c
