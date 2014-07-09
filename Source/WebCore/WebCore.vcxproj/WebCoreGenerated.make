make:
	if exist "%CONFIGURATIONBUILDDIR%\buildfailed" perl -wnle "if (/XXWebCoreGeneratedXX/) { print } else { exit 1 }" "%ConfigurationBuildDir%\buildfailed"
	if errorlevel 1 exit 1
	echo XXWebCoreGeneratedXX > "%ConfigurationBuildDir%\buildfailed"

	perl build-generated-files.pl "%ConfigurationBuildDir%" "%WebKit_Libraries%" windows "%PlatformArchitecture%"
	perl migrate-scripts.pl "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts"
	cmd /C copyForwardingHeaders.cmd cg cf
	cmd /C copyWebCoreResourceFiles.cmd
	
clean:
	if exist "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\DerivedSources" del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\DerivedSources"
	if exist "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts" del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\WebCore\scripts"
	if exist "%ConfigurationBuildDir%\buildfailed" del "%ConfigurationBuildDir%\buildfailed"
