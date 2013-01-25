mkdir 2>NUL "%IntDir%\lib"

if exist "%WebKit_Libraries%\lib\icuuc.lib" copy /y "%WebKit_Libraries%\lib\icuuc.lib" "%IntDir%\lib\libicuuc.lib"
if exist "%WebKit_Libraries%\lib\icuin.lib" copy /y "%WebKit_Libraries%\lib\icuin.lib" "%IntDir%\lib\libicuin.lib"

if exist "%WebKit_Libraries%\lib\libicuuc.lib" copy /y "%WebKit_Libraries%\lib\libicuuc.lib" "%IntDir%\lib"
if exist "%WebKit_Libraries%\lib\libicuin.lib" copy /y "%WebKit_Libraries%\lib\libicuin.lib" "%IntDir%\lib"

cmd /c
