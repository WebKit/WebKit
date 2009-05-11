!IF !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ELSEIF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_Internal
!ENDIF

install:
    set PRODUCTION=1
    set WebKitLibrariesDir=$(SRCROOT)\AppleInternal
    set WebKitOutputDir=$(OBJROOT)
!IF "$(BUILDSTYLE)"=="Release"
    devenv "JavaScriptCoreSubmit.sln" /rebuild Release_PGOInstrument
    -mkdir "$(OBJROOT)\tests\SunSpider"
    set PATH=$(SYSTEMDRIVE)\cygwin\bin;$(PATH)
    xcopy "$(SRCROOT)\AppleInternal\tests\SunSpider\*" "$(OBJROOT)\tests\SunSpider" /e/v/i/h/y
    cd "$(OBJROOT)\tests\SunSpider"
    perl sunspider --shell ../../bin/jsc.exe --runs 3
    devenv "JavaScriptCoreSubmit.sln" /rebuild Release_PGOOptimize
!ELSE
    devenv "JavaScriptCoreSubmit.sln" /rebuild $(BUILDSTYLE)
!ENDIF
    -xcopy "$(OBJROOT)\bin\*.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    xcopy "$(OBJROOT)\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    xcopy "$(OBJROOT)\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
    xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y    
    xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y    
    xcopy "$(OBJROOT)\bin\JavaScriptCore.resources\*" "$(DSTROOT)\AppleInternal\bin\JavaScriptCore.resources" /e/v/i/h/y
