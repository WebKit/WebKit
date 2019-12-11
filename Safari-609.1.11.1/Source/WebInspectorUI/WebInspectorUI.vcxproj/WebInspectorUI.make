make:
	-@mkdir "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
	@perl build-webinspectorui.pl "%ConfigurationBuildDir%" "%WebKit_Libraries%" windows "%PlatformArchitecture%" "%OFFICIAL_BUILD%"
	if errorlevel 1 exit 1

	@xcopy /y /e "..\Localizations\en.lproj\*" "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
	if errorlevel 1 exit 1

	@xcopy /y /e "..\UserInterface\*" "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
	if errorlevel 1 exit 1

	@xcopy /y /e "..\AppleInternal\include\private\JavaScriptCore\inspector\InspectorBackendCommands.js" "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI\Protocol"
	if errorlevel 1 exit 1

	if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"

clean:
	if exist "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI" del /s /q "%ConfigurationBuildDir%\bin%PlatformArchitecture%\WebKit.resources\WebInspectorUI"
