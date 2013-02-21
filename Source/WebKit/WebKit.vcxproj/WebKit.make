!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    set WebKit_Source=$(SRCROOT)\..\..
	devenv "WebKit.submit.sln" /rebuild $(BUILDSTYLE)
	-xcopy "%ConfigurationBuildDir%\bin\*.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	-xcopy "%ConfigurationBuildDir%\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "%ConfigurationBuildDir%\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
