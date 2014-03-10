mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\WebKit"

set ResourcesDirectory=%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources

xcopy /y /d "%PROJECTDIR%\..\..\win\WebLocalizableStrings.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\..\win\WebKitGraphics.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\..\win\WebKitCOMAPI.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\..\win\WebPreferenceKeysPrivate.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"

xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npapi.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npfunctions.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npruntime.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npruntime_internal.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\nptypes.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"

echo Copying resources...
mkdir "%ResourcesDirectory%" 2>NUL
xcopy /y /d "%PROJECTDIR%\..\..\win\WebKit.resources\Info.plist" "%ResourcesDirectory%" >NUL

if exist "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
