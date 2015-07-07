all:
    touch "%ConfigurationBuildDir%\buildfailed"
    perl build-LLIntDesiredOffsets.pl "%ConfigurationBuildDir%" "$(WEBKIT_LIBRARIES)" "%PlatformArchitecture%"

    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    -del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\JavaScriptCore\DerivedSources\LLIntDesiredOffsets.h"
