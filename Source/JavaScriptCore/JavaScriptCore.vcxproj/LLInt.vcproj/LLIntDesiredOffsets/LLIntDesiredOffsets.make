all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-LLIntDesiredOffsets.sh "%ConfigurationBuildDir%" "$(WEBKITLIBRARIESDIR)"

    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    -del /s /q "%ConfigurationBuildDir%\obj\JavaScriptCore\DerivedSources\LLIntDesiredOffsets.h"
