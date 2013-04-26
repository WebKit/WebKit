mkdir 2>NUL "%IntDir%\lib32"

if exist "%WebKit_Libraries%\lib32\icuuc%DebugSuffix%.lib" copy /y "%WebKit_Libraries%\lib32\icuuc%DebugSuffix%.lib" "%IntDir%\lib32\libicuuc%DebugSuffix%.lib"
if exist "%WebKit_Libraries%\lib32\icuin%DebugSuffix%.lib" copy /y "%WebKit_Libraries%\lib32\icuin%DebugSuffix%.lib" "%IntDir%\lib32\libicuin%DebugSuffix%.lib"

if exist "%WebKit_Libraries%\lib32\libicuuc%DebugSuffix%.lib" copy /y "%WebKit_Libraries%\lib32\libicuuc%DebugSuffix%.lib" "%IntDir%\lib32"
if exist "%WebKit_Libraries%\lib32\libicuin%DebugSuffix%.lib" copy /y "%WebKit_Libraries%\lib32\libicuin%DebugSuffix%.lib" "%IntDir%\lib32"

cmd /c
