!IF defined(BUILDSTYLE) && "$(BUILDSTYLE)"=="DEBUG"
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
    devenv "WTF.submit.sln" /clean $(BUILDSTYLE)
    devenv "WTF.submit.sln" /build $(BUILDSTYLE)
    xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\lib32\*" "$(DSTROOT)\AppleInternal\lib32\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\bin32\*" "$(DSTROOT)\%ProgramFilesAAS%" /e/v/i/h/y