mkdir 2>NUL "%INTDIR%\lib"

if exist "%WebKit_Libraries%\lib\icuuc%LIBRARYCONFIGSUFFIX%.lib" copy /y "%WebKit_Libraries%\lib\icuuc%LIBRARYCONFIGSUFFIX%.lib" "%INTDIR%\lib\libicuuc%LIBRARYCONFIGSUFFIX%.lib"
if exist "%WebKit_Libraries%\lib\icuin%LIBRARYCONFIGSUFFIX%.lib" copy /y "%WebKit_Libraries%\lib\icuin%LIBRARYCONFIGSUFFIX%.lib" "%INTDIR%\lib\libicuin%LIBRARYCONFIGSUFFIX%.lib"

if exist "%WebKit_Libraries%\lib\libicuuc%LIBRARYCONFIGSUFFIX%.lib" copy /y "%WebKit_Libraries%\lib\libicuuc%LIBRARYCONFIGSUFFIX%.lib" "%INTDIR%\lib"
if exist "%WebKit_Libraries%\lib\libicuin%LIBRARYCONFIGSUFFIX%.lib" copy /y "%WebKit_Libraries%\lib\libicuin%LIBRARYCONFIGSUFFIX%.lib" "%INTDIR%\lib"

cmd /c
