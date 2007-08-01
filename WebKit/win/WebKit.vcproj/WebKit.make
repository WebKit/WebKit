!IF !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ELSEIF "$(BUILDSTYLE)"=="Debug"
BUILDSTYLE=Debug_Internal
!ENDIF

install:
	set WebKitLibrariesDir="$(SRCROOT)\AppleInternal"
	set WebKitOutputDir=$(OBJROOT)
	set BuildBot=1
	devenv "WebKit.submit.sln" /rebuild $(BUILDSTYLE)
	xcopy "$(OBJROOT)\bin\*" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
