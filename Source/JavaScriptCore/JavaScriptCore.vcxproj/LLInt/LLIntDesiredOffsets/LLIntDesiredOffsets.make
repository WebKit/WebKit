all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-LLIntDesiredOffsets.sh "%ConfigurationBuildDir%" "$(WEBKIT_LIBRARIES)"

    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    -del /s /q "%ConfigurationBuildDir%\obj32\JavaScriptCore\DerivedSources\LLIntDesiredOffsets.h"
