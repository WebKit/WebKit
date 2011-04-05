!IF defined(BUILDSTYLE) && "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ELSE
BUILDSTYLE=Release_PGO
!ENDIF

install:
    set OFFICIAL_BUILD=1
    set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
    set WebKitOutputDir=$(OBJROOT)
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    set WebKitVSPropsRedirectionDir=$(SRCROOT)\AppleInternal\tools\vsprops\OpenSource\1\2\3\4\ 
!IF "$(BUILDSTYLE)"=="Release_PGO"
    devenv "JavaScriptCoreSubmit.sln" /rebuild $(BUILDSTYLE)
    set PATH=$(SYSTEMDRIVE)\cygwin\bin;$(PATH)
    xcopy "$(SRCROOT)\AppleInternal\tests\SunSpider\*" "%ConfigurationBuildDir%\tests\SunSpider" /e/v/i/h/y
    cd "%ConfigurationBuildDir%\tests\SunSpider"
    perl sunspider --shell ../../bin/jsc.exe --runs 3
    del "%ConfigurationBuildDir%\bin\JavaScriptCore.dll"
    cd "$(SRCROOT)\JavaScriptCore.vcproj"
    devenv "JavaScriptCoreSubmit.sln" /build Release_PGO_Optimize
!ELSE
    devenv "JavaScriptCoreSubmit.sln" /rebuild $(BUILDSTYLE)
!ENDIF
    -xcopy "%ConfigurationBuildDir%\bin\JavaScriptCore.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\JavaScriptCore_debug.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\JavaScriptCore.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\JavaScriptCore_debug.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\jsc.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\jsc_debug.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\jsc.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin\jsc_debug.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y    
    xcopy "%ConfigurationBuildDir%\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y    
    xcopy "%ConfigurationBuildDir%\bin\JavaScriptCore.resources\*" "$(DSTROOT)\AppleInternal\bin\JavaScriptCore.resources" /e/v/i/h/y
    -mkdir "$(DSTROOT)\AppleInternal\Sources\JavaScriptCore"
    xcopy "%ConfigurationBuildDir%\obj\JavaScriptCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources\JavaScriptCore" /e/v/i/h/y
