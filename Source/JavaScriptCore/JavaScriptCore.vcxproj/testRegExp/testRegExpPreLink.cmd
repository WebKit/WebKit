mkdir 2>NUL "%INTDIR%\lib"

if exist "%WebKit_Libraries%\lib\icuuc.lib" copy /y "%WebKit_Libraries%\lib\icuuc.lib" "%INTDIR%\lib\libicuuc.lib"
if exist "%WebKit_Libraries%\lib\icuin.lib" copy /y "%WebKit_Libraries%\lib\icuin.lib" "%INTDIR%\lib\libicuin.lib"

if exist "%WebKit_Libraries%\lib\libicuuc.lib" copy /y "%WebKit_Libraries%\lib\libicuuc.lib" "%INTDIR%\lib"
if exist "%WebKit_Libraries%\lib\libicuin.lib" copy /y "%WebKit_Libraries%\lib\libicuin.lib" "%INTDIR%\lib"

cmd /c
