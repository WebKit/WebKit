mkdir 2>NUL "%WebKitOutputDir%\bin\WebKit.resources\inspector"
xcopy /y /d /s /exclude:xcopy.excludes "%ProjectDir%..\inspector\front-end\*" "%WebKitOutputDir%\bin\WebKit.resources\inspector"
mkdir 2>NUL "%WebKitOutputDir%\bin\WebKit.resources\en.lproj"
xcopy /y /d /s /exclude:xcopy.excludes "%ProjectDir%..\English.lproj\localizedStrings.js" "%WebKitOutputDir%\bin\WebKit.resources\en.lproj"
