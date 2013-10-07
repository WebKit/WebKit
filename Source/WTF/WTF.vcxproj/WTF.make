!IF defined(BUILDSTYLE) && "$(BUILDSTYLE)"=="DEBUG"
BUILDSTYLE=DebugSuffix
!ELSE
BUILDSTYLE=Production
!ENDIF

install:
    set OFFICIAL_BUILD=1
	set WebKit_Libraries=$(SRCROOT)\AppleInternal
	set WebKit_OutputDir=$(OBJROOT)
    set OriginalPath = %PATH%
    
    set ArchitectureBuildStyle=$(BUILDSTYLE)|Win32
    set ProgramFilesAAS=Program Files (x86)\Common Files\Apple\Apple Application Support
    set Path=%OriginalPath%;$(SRCROOT)\%ProgramFilesAAS%
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    devenv "WTF.submit.sln" /clean "%ArchitectureBuildStyle%"
    devenv "WTF.submit.sln" /build "%ArchitectureBuildStyle%"
    echo "%ConfigurationBuildDir%\include\*"
    xcopy "%ConfigurationBuildDir%\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\lib32\*" "$(DSTROOT)\AppleInternal\lib32\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\bin32\*" "$(DSTROOT)\%ProgramFilesAAS%" /e/v/i/h/y

    set ArchitectureBuildStyle=$(BUILDSTYLE)|x64
    set ProgramFilesAAS=Program Files\Common Files\Apple\Apple Application Support
    set Path=%OriginalPath%;$(SRCROOT)\%ProgramFilesAAS%
	set ConfigurationBuildDir=$(OBJROOT)\$(BUILDSTYLE)
    devenv "WTF.submit.sln" /clean "%ArchitectureBuildStyle%"
    devenv "WTF.submit.sln" /build "%ArchitectureBuildStyle%"
    xcopy "%ConfigurationBuildDir%\lib64\*" "$(DSTROOT)\AppleInternal\lib64\" /e/v/i/h/y
    xcopy "%ConfigurationBuildDir%\bin64\*" "$(DSTROOT)\%ProgramFilesAAS%" /e/v/i/h/y