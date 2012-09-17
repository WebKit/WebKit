all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-LLIntAssembly.sh "%ConfigurationBuildDir%" "$(WEBKITLIBRARIESDIR)"
    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    -del /s /q "%ConfigurationBuildDir%\obj\LLIntOffsetsExtractor\LLIntOffsetsExtractor.exe"
    -del /s /q "%ConfigurationBuildDir%\obj\JavaScriptCore\DerivedSources\LLIntAssembly.h"
