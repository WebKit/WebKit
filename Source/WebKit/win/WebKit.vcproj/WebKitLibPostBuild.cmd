mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\WebKit"

xcopy /y /d "%PROJECTDIR%\..\WebLocalizableStrings.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\WebKitGraphics.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\WebKitCOMAPI.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\WebPreferenceKeysPrivate.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"

xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npapi.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npfunctions.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npruntime.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\npruntime_internal.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"
xcopy /y /d "%CONFIGURATIONBUILDDIR%\include\WebCore\nptypes.h" "%CONFIGURATIONBUILDDIR%\include\WebKit"

mkdir 2>NUL "%OUTDIR%\..\bin\WebKit.resources"
xcopy /y /d "%PROJECTDIR%..\WebKit.resources\*" "%OUTDIR%\..\bin\WebKit.resources"
mkdir 2>NUL "%OUTDIR%\..\bin\WebKit.resources\en.lproj"
xcopy /y /d "%PROJECTDIR%..\..\English.lproj\Localizable.strings" "%OUTDIR%\..\bin\WebKit.resources\en.lproj\"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
