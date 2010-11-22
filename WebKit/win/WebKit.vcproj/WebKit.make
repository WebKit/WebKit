!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ELSE
BUILDSTYLE=Release_LTCG
!ENDIF

install:
	set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
	set WebKitOutputDir=$(OBJROOT)
	set WebKitVSPropsRedirectionDir=$(SRCROOT)\AppleInternal\tools\vsprops\OpenSource\1\2\3\
	set PRODUCTION=1
	devenv "WebKit.submit.sln" /rebuild $(BUILDSTYLE)
	-xcopy "$(OBJROOT)\bin\*.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	-xcopy "$(OBJROOT)\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
