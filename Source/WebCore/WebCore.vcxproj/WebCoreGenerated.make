make:
	if exist "%ConfigurationBuildDir%\buildfailed" grep XXWebCoreGeneratedXX "%ConfigurationBuildDir%\buildfailed"
	if errorlevel 1 exit 1
	echo XXWebCoreGeneratedXX > "%ConfigurationBuildDir%\buildfailed"

	bash build-generated-files.sh "%ConfigurationBuildDir%" "%WebKit_Libraries%" windows "%PlatformArchitecture%"
	bash migrate-scripts.sh "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts"
	cmd /C copyForwardingHeaders.cmd cg cf
	cmd /C copyWebCoreResourceFiles.cmd
	
clean:
	if exist "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\DerivedSources" del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\DerivedSources"
	if exist "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts" del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts"
	if exist "%ConfigurationBuildDir%\buildfailed" del "%ConfigurationBuildDir%\buildfailed"
