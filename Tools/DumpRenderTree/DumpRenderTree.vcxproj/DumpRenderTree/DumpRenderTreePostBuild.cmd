if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"

if not defined ARCHIVE_BUILD (if defined PRODUCTION exit /b)

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin32"

if not exist "%WEBKIT_LIBRARIES%\bin32\CoreFoundation%DebugSuffix%.dll" GOTO:CFLITE

xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CoreFoundation%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CoreFoundation%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CoreVideo%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CoreVideo%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CFNetwork%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CFNetwork%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d /e /i "%WEBKIT_LIBRARIES%\bin32\CFNetwork.resources" "%CONFIGURATIONBUILDDIR%\bin32\CFNetwork.resources"
xcopy /y /d /e /i "%WEBKIT_LIBRARIES%\bin32\CoreFoundation.resources" "%CONFIGURATIONBUILDDIR%\bin32\CoreFoundation.resources"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CoreGraphics%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CoreGraphics%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\QuartzCore%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\QuartzCore%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icudt40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icudt40.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icudt40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icudt40.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuin40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuin40.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuin40.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuin40.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuuc40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuuc40.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuuc40.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuuc40.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icudt42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icudt42.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icudt42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icudt42.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuin42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuin42.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuin42.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuin42.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuuc42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuuc42.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\icuuc42.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icuuc42.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libxml2%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libxslt%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\pthreadVC2%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\pthreadVC2%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\SQLite3%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\SQLite3%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\zlib1%DebugSuffix%.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\zlib1%DebugSuffix%.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
exit /b

:CFLITE
if not exist "%WEBKIT_LIBRARIES%\bin32\CFLite.dll" exit /b

xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CFLite.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\CFLite.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d /e /i "%WEBKIT_LIBRARIES%\bin32\CFLite.resources" "%CONFIGURATIONBUILDDIR%\bin32\CFLite.resources"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libcurl.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libeay32.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\ssleay32.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\cairo.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\icudt46.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libicuuc.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libicuin.dll" "%CONFIGURATIONBUILDDIR%\bin32"

xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libxml2.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\libxslt.dll" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\pthreadVC2.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\pthreadVC2.pdb" xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\pthreadVC2.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\SQLite3.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\SQLite3.pdb" xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\SQLite3.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\zlib1.dll" "%CONFIGURATIONBUILDDIR%\bin32"
if exist "%WEBKIT_LIBRARIES%\bin32\zlib1.pdb" xcopy /y /d "%WEBKIT_LIBRARIES%\bin32\zlib1.pdb" "%CONFIGURATIONBUILDDIR%\bin32"
