mkdir 2>NUL "%IntDir%\lib"

if exist "%WebKit_Libraries%\lib\icuuc%LibraryConfigSuffix%.lib" copy /y "%WebKit_Libraries%\lib\icuuc%LibraryConfigSuffix%.lib" "%IntDir%\lib\libicuuc%LibraryConfigSuffix%.lib"
if exist "%WebKit_Libraries%\lib\icuin%LibraryConfigSuffix%.lib" copy /y "%WebKit_Libraries%\lib\icuin%LibraryConfigSuffix%.lib" "%IntDir%\lib\libicuin%LibraryConfigSuffix%.lib"

if exist "%WebKit_Libraries%\lib\libicuuc%LibraryConfigSuffix%.lib" copy /y "%WebKit_Libraries%\lib\libicuuc%LibraryConfigSuffix%.lib" "%IntDir%\lib"
if exist "%WebKit_Libraries%\lib\libicuin%LibraryConfigSuffix%.lib" copy /y "%WebKit_Libraries%\lib\libicuin%LibraryConfigSuffix%.lib" "%IntDir%\lib"

cmd /c
