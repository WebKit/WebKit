all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-generated-files.sh "%ConfigurationBuildDir%" "$(WEBKITLIBRARIESDIR)"
!IF "$(OFFICIAL_BUILD)"!="1"
    bash -c "python work-around-vs-dependency-tracking-bugs.py"
!ENDIF
    copy-files.cmd

    -del "%ConfigurationBuildDir%\include\private\JavaScriptCore\stdbool.h" "%ConfigurationBuildDir%\include\private\JavaScriptCore\stdint.h"
    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    copy-files.cmd clean
    -del /s /q "%ConfigurationBuildDir%\obj\JavaScriptCore\DerivedSources"
