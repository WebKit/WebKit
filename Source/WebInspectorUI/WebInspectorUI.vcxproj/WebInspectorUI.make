make:
	bash build-webinspectorui.sh "%ConfigurationBuildDir%" "%WebKit_Libraries%" windows "%PlatformArchitecture%"
	if errorlevel 1 exit 1

	xcopy /y /e "..\Localizations\en.lproj\*" "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
	if errorlevel 1 exit 1

	if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"

clean:
	if exist "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI" del /s /q "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
