mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources\en.lproj"
xcopy /y /d /s /exclude:xcopy.excludes "%ProjectDir%..\English.lproj\Localizable.strings" "%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources\en.lproj"
