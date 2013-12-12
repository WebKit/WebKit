all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-generated-files.sh "%ConfigurationBuildDir%" "$(WEBKIT_LIBRARIES)" "%PlatformArchitecture%"
    copy-files.cmd

    -del "%ConfigurationBuildDir%\include\private\JavaScriptCore\stdbool.h" "%ConfigurationBuildDir%\include\private\JavaScriptCore\stdint.h"
    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    copy-files.cmd clean
    -del /s /q "%ConfigurationBuildDir%\obj32\JavaScriptCore\DerivedSources"
