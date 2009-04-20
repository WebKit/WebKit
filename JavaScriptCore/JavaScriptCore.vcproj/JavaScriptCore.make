!IF !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ELSEIF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=Debug_Internal
!ENDIF

install:
    set PRODUCTION=1
	set WebKitLibrariesDir="$(SRCROOT)\AppleInternal"
	set WebKitOutputDir=$(OBJROOT)
	devenv "JavaScriptCoreSubmit.sln" /rebuild $(BUILDSTYLE)
	-xcopy "$(OBJROOT)\bin\*.exe" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.pdb" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\bin\*.dll" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(OBJROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	
	xcopy "$(OBJROOT)\bin\JavaScriptCore.resources\*" "$(DSTROOT)\AppleInternal\bin\JavaScriptCore.resources" /e/v/i/h/y
