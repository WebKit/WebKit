mkdir 2>NUL "%WEBKITOUTPUTDIR%\include\WebKit"

xcopy /y /d "%PROJECTDIR%\..\WebLocalizableStrings.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\WebKitGraphics.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\WebKitCOMAPI.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%PROJECTDIR%\..\WebPreferenceKeysPrivate.h" "%WEBKITOUTPUTDIR%\include\WebKit"

xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\npapi.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\npfunctions.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\npruntime.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\npruntime_internal.h" "%WEBKITOUTPUTDIR%\include\WebKit"
xcopy /y /d "%WEBKITOUTPUTDIR%\include\WebCore\nptypes.h" "%WEBKITOUTPUTDIR%\include\WebKit"

mkdir 2>NUL "%OUTDIR%\..\bin\WebKit.resources"
xcopy /y /d "%PROJECTDIR%..\WebKit.resources\*" "%OUTDIR%\..\bin\WebKit.resources"
mkdir 2>NUL "%OUTDIR%\..\bin\WebKit.resources\en.lproj"
xcopy /y /d "%PROJECTDIR%..\..\English.lproj\Localizable.strings" "%OUTDIR%\..\bin\WebKit.resources\en.lproj\"

if exist "%WEBKITOUTPUTDIR%\buildfailed" del "%WEBKITOUTPUTDIR%\buildfailed"
