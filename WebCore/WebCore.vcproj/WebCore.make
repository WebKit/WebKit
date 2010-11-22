!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ELSE
BUILDSTYLE=Release_LTCG
!ENDIF

install:
    set PRODUCTION=1
	set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
	set WebKitOutputDir=$(OBJROOT)
	set WebKitVSPropsRedirectionDir=$(SRCROOT)\AppleInternal\tools\vsprops\OpenSource\1\2\
	-mkdir 2>NUL "%WebKitOutputDir%\include\private\JavaScriptCore"
	xcopy "%WebKitLibrariesDir%\include\private\JavaScriptCore\*" "%WebKitOutputDir%\include\private\JavaScriptCore" /e/v/i/h/y
	devenv "WebCore.submit.sln" /rebuild $(BUILDSTYLE)
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
	xcopy "$(OBJROOT)\obj\WebCore\scripts\*" "$(DSTROOT)\AppleInternal\tools\scripts" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	-mkdir "$(DSTROOT)\AppleInternal\Sources\WebCore"
	xcopy "$(OBJROOT)\obj\WebCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources\WebCore" /e/v/i/h/y
