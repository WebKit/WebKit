mkdir 2>NUL "%IntDir%\lib"

if exist "%WebKitLibrariesDir%\lib\icuuc%LibraryConfigSuffix%.lib" copy /y "%WebKitLibrariesDir%\lib\icuuc%LibraryConfigSuffix%.lib" "%IntDir%\lib\libicuuc%LibraryConfigSuffix%.lib"
if exist "%WebKitLibrariesDir%\lib\icuin%LibraryConfigSuffix%.lib" copy /y "%WebKitLibrariesDir%\lib\icuin%LibraryConfigSuffix%.lib" "%IntDir%\lib\libicuin%LibraryConfigSuffix%.lib"

if exist "%WebKitLibrariesDir%\lib\libicuuc%LibraryConfigSuffix%.lib" copy /y "%WebKitLibrariesDir%\lib\libicuuc%LibraryConfigSuffix%.lib" "%IntDir%\lib"
if exist "%WebKitLibrariesDir%\lib\libicuin%LibraryConfigSuffix%.lib" copy /y "%WebKitLibrariesDir%\lib\libicuin%LibraryConfigSuffix%.lib" "%IntDir%\lib"

cmd /c
