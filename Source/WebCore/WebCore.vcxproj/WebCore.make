!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
    set OFFICIAL_BUILD=1
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
    set ProgramFilesAAS=Program Files (x86)\Common Files\Apple\Apple Application Support
    set Path=%PATH%;$(SRCROOT)\%ProgramFilesAAS%
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
	-mkdir 2>NUL "%ConfigurationBuildDir%\include\private"
	xcopy "%WebKit_Libraries%\include\private\*" "%ConfigurationBuildDir%\include\private" /e/v/i/h/y
	devenv "WebCore.submit.sln" /rebuild $(BUILDSTYLE)
	xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "%ConfigurationBuildDir%\lib32\*" "$(DSTROOT)\AppleInternal\lib32\" /e/v/i/h/y	
	xcopy "%ConfigurationBuildDir%\bin32\WebKit.resources\*" "$(DSTROOT)\%ProgramFilesAAS%\WebKit.resources" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\bin32\*.dll" "$(DSTROOT)\%ProgramFilesAAS%\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\bin32\*.pdb" "$(DSTROOT)\%ProgramFilesAAS%\" /e/v/i/h/y
	xcopy "%ConfigurationBuildDir%\obj32\WebCore\scripts\*" "$(DSTROOT)\AppleInternal\tools\scripts" /e/v/i/h/y
	-mkdir "$(DSTROOT)\AppleInternal\Sources32\WebCore"
	xcopy "%ConfigurationBuildDir%\obj32\WebCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources32\WebCore" /e/v/i/h/y
