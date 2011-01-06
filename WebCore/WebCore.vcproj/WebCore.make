!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ELSE
BUILDSTYLE=Release_LTCG
!ENDIF

install:
    set PRODUCTION=1
	set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
	set WebKitOutputDir=$(OBJROOT)
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
	set WebKitVSPropsRedirectionDir=$(SRCROOT)\AppleInternal\tools\vsprops\OpenSource\1\2\ 
	-mkdir 2>NUL "%WebKitOutputDir%\include\private\JavaScriptCore"
	xcopy "%WebKitLibrariesDir%\include\private\JavaScriptCore\*" "%WebKitOutputDir%\include\private\JavaScriptCore" /e/v/i/h/y
	devenv "WebCore.submit.sln" /rebuild $(BUILDSTYLE)
	xcopy "$(ConfigurationBuildDir)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(ConfigurationBuildDir)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	
	xcopy "$(ConfigurationBuildDir)\bin\WebKit.resources\*" "$(DSTROOT)\AppleInternal\bin\WebKit.resources" /e/v/i/h/y
	xcopy "$(ConfigurationBuildDir)\obj\WebCore\scripts\*" "$(DSTROOT)\AppleInternal\tools\scripts" /e/v/i/h/y
	xcopy "$(ConfigurationBuildDir)\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(ConfigurationBuildDir)\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	-mkdir "$(DSTROOT)\AppleInternal\Sources\WebCore"
	xcopy "$(ConfigurationBuildDir)\obj\WebCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources\WebCore" /e/v/i/h/y
