!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
	set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
	set WebKitOutputDir=$(OBJROOT)
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
	set WebKitVSPropsRedirectionDir=$(SRCROOT)\AppleInternal\tools\vsprops\OpenSource\1\2\3\4\ 
	devenv "WebKit.submit.sln" /rebuild $(BUILDSTYLE)
	-xcopy "%ConfigurationBuildDir%\bin\*.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	-xcopy "%ConfigurationBuildDir%\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "%ConfigurationBuildDir%\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
