!IF !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ELSEIF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_All
!ENDIF

install:
    set PRODUCTION=1
    set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
    set WebKitOutputDir=$(OBJROOT)
!IF "$(BUILDSTYLE)"=="Release"
    devenv "JavaScriptCoreSubmit.sln" /rebuild Release_PGOInstrument
    set PATH=$(SYSTEMDRIVE)\cygwin\bin;$(PATH)
    xcopy "$(SRCROOT)\AppleInternal\tests\SunSpider\*" "$(OBJROOT)\tests\SunSpider" /e/v/i/h/y
    cd "$(OBJROOT)\tests\SunSpider"
    perl sunspider --shell ../../bin/jsc.exe --runs 3
    del "$(OBJROOT)\bin\JavaScriptCore.dll"
    cd "$(SRCROOT)\JavaScriptCore.vcproj"
    devenv "JavaScriptCoreSubmit.sln" /build Release_PGOOptimize
!ELSE
    devenv "JavaScriptCoreSubmit.sln" /rebuild $(BUILDSTYLE)
!ENDIF
    -xcopy "$(OBJROOT)\bin\JavaScriptCore.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\JavaScriptCore_debug.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\JavaScriptCore.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\JavaScriptCore_debug.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\jsc.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\jsc_debug.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\jsc.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    -xcopy "$(OBJROOT)\bin\jsc_debug.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y    
    xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y    
    xcopy "$(OBJROOT)\bin\JavaScriptCore.resources\*" "$(DSTROOT)\AppleInternal\bin\JavaScriptCore.resources" /e/v/i/h/y
    -mkdir "$(DSTROOT)\AppleInternal\Sources\JavaScriptCore"
    xcopy "$(OBJROOT)\obj\JavaScriptCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources\JavaScriptCore" /e/v/i/h/y
