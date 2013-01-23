make:
	if exist "%ConfigurationBuildDir%\buildfailed" grep XXWebCoreGeneratedXX "%ConfigurationBuildDir%\buildfailed"
	if errorlevel 1 exit 1
	echo XXWebCoreGeneratedXX > "%ConfigurationBuildDir%\buildfailed"

	bash build-generated-files.sh "%ConfigurationBuildDir%" "%WebKit_Libraries%" "%WebKitVSPropsRedirectionDir%..\..\..\..\..\WebKitLibraries\win" windows
	bash migrate-scripts.sh "%ConfigurationBuildDir%\obj\WebCore\scripts"
	cmd /C copyForwardingHeaders.cmd cg cf
	cmd /C copyWebCoreResourceFiles.cmd
	
clean:
	del /s /q "%ConfigurationBuildDir%\obj\WebCore\DerivedSources"
	del /s /q "%ConfigurationBuildDir%\obj\WebCore\scripts"
	if exist "%ConfigurationBuildDir%\buildfailed" del "%ConfigurationBuildDir%\buildfailed"
