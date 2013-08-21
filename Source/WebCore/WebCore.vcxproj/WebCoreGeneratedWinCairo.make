make:
	if exist "%ConfigurationBuildDir%\buildfailed" grep XXWebCoreGeneratedXX "%ConfigurationBuildDir%\buildfailed"
	if errorlevel 1 exit 1
	echo XXWebCoreGeneratedXX > "%ConfigurationBuildDir%\buildfailed"

	bash build-generated-files.sh "%ConfigurationBuildDir%" "%WebKit_Libraries%" cairo "%PlatformArchitecture%"
	bash migrate-scripts.sh "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts"
	cmd /C copyForwardingHeaders.cmd cairo curl
	cmd /C copyWebCoreResourceFiles.cmd
	
clean:
	del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\DerivedSources"
	del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts"
	if exist "%ConfigurationBuildDir%\buildfailed" del "%ConfigurationBuildDir%\buildfailed"
