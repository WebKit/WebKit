!if !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ENDIF

install:
    set BuildBot=1
	set WebKitSDKDir=$(SRCROOT)\AppleInternal
	set WebKitOutputDir=$(OBJROOT)
	devenv "WebCore.submit.sln" /rebuild $(BUILDSTYLE)
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	
