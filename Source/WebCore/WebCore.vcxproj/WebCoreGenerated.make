make:
	if exist "%ConfigurationBuildDir%\buildfailed" grep XXWebCoreGeneratedXX "%ConfigurationBuildDir%\buildfailed"
	if errorlevel 1 exit 1
	echo XXWebCoreGeneratedXX > "%ConfigurationBuildDir%\buildfailed"

	bash build-generated-files.sh "%ConfigurationBuildDir%" "%WebKit_Libraries%" windows
	bash migrate-scripts.sh "%ConfigurationBuildDir%\obj32\WebCore\scripts"
	cmd /C copyForwardingHeaders.cmd cg cf
	cmd /C copyWebCoreResourceFiles.cmd
	
clean:
	if exist "%ConfigurationBuildDir%\obj32\WebCore\DerivedSources" del /s /q "%ConfigurationBuildDir%\obj32\WebCore\DerivedSources"
	if exist "%ConfigurationBuildDir%\obj32\WebCore\scripts" del /s /q "%ConfigurationBuildDir%\obj32\WebCore\scripts"
	if exist "%ConfigurationBuildDir%\buildfailed" del "%ConfigurationBuildDir%\buildfailed"
