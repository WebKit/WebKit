all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-LLIntAssembly.sh "%ConfigurationBuildDir%" "$(WEBKIT_LIBRARIES)" "$(DEBUGSUFFIX)"
    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    -del /s /q "%ConfigurationBuildDir%\obj%PlatformArchitecture%\JavaScriptCore\DerivedSources\LLIntAssembly.h"
