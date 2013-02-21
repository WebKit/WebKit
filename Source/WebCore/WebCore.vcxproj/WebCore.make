!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
    set OFFICIAL_BUILD=1
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    set WebKit_Source=$(SRCROOT)\..\..
	-mkdir 2>NUL "%ConfigurationBuildDir%\include\private"
	xcopy "%WebKitLibrariesDir%\include\private\*" "%ConfigurationBuildDir%\include\private" /e/v/i/h/y
	devenv "WebCore.submit.sln" /rebuild $(BUILDSTYLE)
	xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "%ConfigurationBuildDir%\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	
	xcopy "%ConfigurationBuildDir%\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\obj\WebCore\scripts\*" "$(DSTROOT)\AppleInternal\tools\scripts" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	-mkdir "$(DSTROOT)\AppleInternal\Sources\WebCore"
	xcopy "%ConfigurationBuildDir%\obj\WebCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources\WebCore" /e/v/i/h/y
