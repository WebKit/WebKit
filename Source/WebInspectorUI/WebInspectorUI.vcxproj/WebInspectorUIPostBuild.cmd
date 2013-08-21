mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin32\WebKit.resources\WebInspectorUI"
xcopy /y /e "%PROJECTDIR%..\UserInterface\*" "%CONFIGURATIONBUILDDIR%\bin32\WebKit.resources\WebInspectorUI"
xcopy /y /d "%PROJECTDIR%..\Localizations\en.lproj\*" "%CONFIGURATIONBUILDDIR%\bin32\WebKit.resources\WebInspectorUI"
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
