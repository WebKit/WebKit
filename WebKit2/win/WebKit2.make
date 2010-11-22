!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ELSE
BUILDSTYLE=Release_LTCG
!ENDIF

install:
	set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
	set WebKitOutputDir=$(OBJROOT)
	set WebKitVSPropsRedirectionDir=$(SRCROOT)\AppleInternal\tools\vsprops\OpenSource\1\2\
	set PRODUCTION=1
	devenv "WebKit2.submit.sln" /rebuild $(BUILDSTYLE)
	-xcopy "$(OBJROOT)\bin\*.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y
	-xcopy "$(OBJROOT)\bin\WebKit2.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit2.resources" /e/v/i/h/y
	-mkdir "$(DSTROOT)\AppleInternal\Sources\WebKit2"
	xcopy "$(OBJROOT)\obj\WebKit\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources\WebKit2" /e/v/i/h/y
