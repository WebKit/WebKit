!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
    set WebKit_Source=$(SRCROOT)\..\..
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    devenv "WTF.sln" /clean $(BUILDSTYLE)
    devenv "WTF.sln" /build $(BUILDSTYLE)
    xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y    
    xcopy "%ConfigurationBuildDir%\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y    
