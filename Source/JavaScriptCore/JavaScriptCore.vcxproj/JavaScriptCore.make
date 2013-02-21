!IF "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    set WebKit_Source=$(SRCROOT)\..\..
    -mkdir "%ConfigurationBuildDir%\include\private"
    xcopy "%WebKitLibrariesDir%\include\private\*" "%ConfigurationBuildDir%\include\private" /e/v/i/h/y
    devenv "JavaScriptCoreSubmit.sln" /clean $(BUILDSTYLE)
    devenv "JavaScriptCoreSubmit.sln" /build $(BUILDSTYLE)
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
