!if !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ENDIF

install:
    set BuildBot=1
	set WebKitSDKDir="$(SRCROOT)\AppleInternal"
	set WebKitOutputDir=$(OBJROOT)
	devenv "JavaScriptCore.sln" /rebuild $(BUILDSTYLE)
	xcopy "$(OBJROOT)\bin\*" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	