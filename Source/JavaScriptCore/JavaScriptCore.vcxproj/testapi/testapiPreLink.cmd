mkdir 2>NUL "%IntDir%\lib"

if exist "%WebKit_Libraries%\lib32\icuuc.lib" copy /y "%WebKit_Libraries%\lib32\icuuc.lib" "%IntDir%\lib\libicuuc.lib"
if exist "%WebKit_Libraries%\lib32\icuin.lib" copy /y "%WebKit_Libraries%\lib32\icuin.lib" "%IntDir%\lib\libicuin.lib"

if exist "%WebKit_Libraries%\lib32\libicuuc.lib" copy /y "%WebKit_Libraries%\lib32\libicuuc.lib" "%IntDir%\lib"
if exist "%WebKit_Libraries%\lib32\libicuin.lib" copy /y "%WebKit_Libraries%\lib32\libicuin.lib" "%IntDir%\lib"

cmd /c
