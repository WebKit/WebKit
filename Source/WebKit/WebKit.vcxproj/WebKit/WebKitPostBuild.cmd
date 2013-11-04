mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\WebKit"

xcopy /y /d "%PROJECTDIR%\..\..\win\WebLocalizableStrings.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\..\win\WebKitGraphics.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\..\win\WebKitCOMAPI.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\..\win\WebPreferenceKeysPrivate.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"

xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npapi.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npfunctions.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npruntime.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npruntime_internal.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\nptypes.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin32\WebKit.resources"
xcopy /y /d "%PROJECTDIR%..\..\WebKit.resources\*" "%CONFIGURATIONBUILDDIR%\bin32\WebKit.resources"
if exist "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
