!IF !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ENDIF

install:
    set BuildBot=1
	set WebKitLibrariesDir="$(SRCROOT)\AppleInternal"
	set WebKitOutputDir=$(OBJROOT)
	devenv "dftables.vcproj" /rebuild $(BUILDSTYLE)
	devenv "WTF.vcproj" /rebuild $(BUILDSTYLE)
	devenv "JavaScriptCore.vcproj" /rebuild $(BUILDSTYLE)
	xcopy "$(OBJROOT)\bin\*" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	
