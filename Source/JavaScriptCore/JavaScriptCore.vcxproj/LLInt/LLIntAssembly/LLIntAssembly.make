all:
    touch "%ConfigurationBuildDir%\buildfailed"
    perl build-LLIntAssembly.pl "%ConfigurationBuildDir%" "$(WEBKIT_LIBRARIES)" "$(DEBUGSUFFIX)" "%PlatformArchitecture%"
    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    -del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\JavaScriptCore\DerivedSources\LLIntAssembly.h"
