!IF defined(BUILDSTYLE) && "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
    set OFFICIAL_BUILD=1
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
    set ProgramFilesAAS=Program Files (x86)\Common Files\Apple\Apple Application Support
    set Path=%PATH%;$(SRCROOT)\%ProgramFilesAAS%
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    -mkdir "%ConfigurationBuildDir%\include\private"
    xcopy "%WebKit_Libraries%\include\private\*" "%ConfigurationBuildDir%\include\private" /e/v/i/h/y
    devenv "JavaScriptCore.submit.sln" /clean $(BUILDSTYLE)
    devenv "JavaScriptCore.submit.sln" /build $(BUILDSTYLE)
    -xcopy "%ConfigurationBuildDir%\bin32\JavaScriptCore.dll" "$(DSTROOT)\%ProgramFilesAAS%\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\JavaScriptCore_debug.dll" "$(DSTROOT)\%ProgramFilesAAS%\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\JavaScriptCore.pdb" "$(DSTROOT)\%ProgramFilesAAS%\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\JavaScriptCore_debug.pdb" "$(DSTROOT)\%ProgramFilesAAS%\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\jsc.exe" "$(DSTROOT)\AppleInternal\bin32\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\jsc_debug.exe" "$(DSTROOT)\AppleInternal\bin32\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\jsc.pdb" "$(DSTROOT)\AppleInternal\bin32\" /e/v/i/h/y
    -xcopy "%ConfigurationBuildDir%\bin32\jsc_debug.pdb" "$(DSTROOT)\AppleInternal\bin32\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y    
    xcopy "%ConfigurationBuildDir%\lib32\*" "$(DSTROOT)\AppleInternal\lib32\" /e/v/i/h/y    
    xcopy "%ConfigurationBuildDir%\bin32\JavaScriptCore.resources\*" "$(DSTROOT)\%ProgramFilesAAS%\JavaScriptCore.resources" /e/v/i/h/y
    -mkdir "$(DSTROOT)\AppleInternal\Sources32\JavaScriptCore"
    xcopy "%ConfigurationBuildDir%\obj32\JavaScriptCore\DerivedSources\*" "$(DSTROOT)\AppleInternal\Sources32\JavaScriptCore" /e/v/i/h/y
