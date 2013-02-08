if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"

if not defined ARCHIVE_BUILD (if defined PRODUCTION exit /b)

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin"

if not exist "%WEBKIT_LIBRARIES%\bin\CoreFoundation.dll" GOTO:CFLITE

xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CoreFoundation.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CoreFoundation.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CoreVideo.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CoreVideo.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CFNetwork.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CFNetwork.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d /e /i "%WEBKIT_LIBRARIES%\bin\CFNetwork.resources" "%CONFIGURATIONBUILDDIR%\bin\CFNetwork.resources"
xcopy /y /d /e /i "%WEBKIT_LIBRARIES%\bin\CoreFoundation.resources" "%CONFIGURATIONBUILDDIR%\bin\CoreFoundation.resources"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CoreGraphics.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CoreGraphics.pdb" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icudt40.dll" xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icudt40.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icudt40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icudt40.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuin40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuin40.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuin40.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuin40.pdb" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuuc40.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuuc40.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuuc40.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuuc40.pdb" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icudt42.dll" xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icudt42.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icudt42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icudt42.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuin42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuin42.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuin42.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuin42.pdb" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuuc42.dll"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuuc42.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\icuuc42.pdb"xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icuuc42.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libxml2.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libxslt.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\pthreadVC2.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\pthreadVC2.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\SQLite3.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\SQLite3.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\zlib1.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\zlib1.pdb" "%CONFIGURATIONBUILDDIR%\bin"
exit /b

:CFLITE
if not exist "%WEBKIT_LIBRARIES%\bin\CFLite.dll" exit /b

xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CFLite.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\CFLite.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d /e /i "%WEBKIT_LIBRARIES%\bin\CFLite.resources" "%CONFIGURATIONBUILDDIR%\bin\CFLite.resources"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libcurl.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libeay32.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\ssleay32.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\cairo.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\icudt46.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libicuuc.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libicuin.dll" "%CONFIGURATIONBUILDDIR%\bin"

xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libxml2.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\libxslt.dll" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\pthreadVC2.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\pthreadVC2.pdb" xcopy /y /d "%WEBKIT_LIBRARIES%\bin\pthreadVC2.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\SQLite3.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\SQLite3.pdb" xcopy /y /d "%WEBKIT_LIBRARIES%\bin\SQLite3.pdb" "%CONFIGURATIONBUILDDIR%\bin"
xcopy /y /d "%WEBKIT_LIBRARIES%\bin\zlib1.dll" "%CONFIGURATIONBUILDDIR%\bin"
if exist "%WEBKIT_LIBRARIES%\bin\zlib1.pdb" xcopy /y /d "%WEBKIT_LIBRARIES%\bin\zlib1.pdb" "%CONFIGURATIONBUILDDIR%\bin"
