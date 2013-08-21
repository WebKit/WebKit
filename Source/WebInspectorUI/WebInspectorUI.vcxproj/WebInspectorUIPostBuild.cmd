mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
xcopy /y /e "%PROJECTDIR%..\UserInterface\*" "%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
xcopy /y /d "%PROJECTDIR%..\Localizations\en.lproj\*" "%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
