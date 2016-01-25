list(APPEND WTF_SOURCES
    text/cf/AtomicStringImplCF.cpp
    text/cf/StringCF.cpp
    text/cf/StringImplCF.cpp
    text/cf/StringViewCF.cpp

    win/MainThreadWin.cpp
    win/RunLoopWin.cpp
    win/WTFDLL.cpp
    win/WorkItemWin.cpp
    win/WorkQueueWin.cpp
)

list(APPEND WTF_LIBRARIES
    winmm
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND WTF_LIBRARIES
        cflite
    )
else ()
    list(APPEND WTF_LIBRARIES
        CoreFoundation${DEBUG_SUFFIX}
    )
endif ()

list(APPEND WTF_HEADERS
    "${DERIVED_SOURCES_WTF_DIR}/WTFHeaderDetection.h"
)

set(WTF_PRE_BUILD_COMMAND "${CMAKE_BINARY_DIR}/DerivedSources/WTF/preBuild.cmd")
file(WRITE "${WTF_PRE_BUILD_COMMAND}" "@xcopy /y /s /d /f \"${WTF_DIR}/wtf/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/WTF\" >nul 2>nul\n@xcopy /y /s /d /f \"${DERIVED_SOURCES_DIR}/WTF/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/WTF\" >nul 2>nul\n")
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WTF)

# FIXME: This should run testOSXLevel.cmd if it is available.
# https://bugs.webkit.org/show_bug.cgi?id=135861
add_custom_command(
    OUTPUT "${DERIVED_SOURCES_WTF_DIR}/WTFHeaderDetection.h"
    WORKING_DIRECTORY "${DERIVED_SOURCES_WTF_DIR}"
    COMMAND echo /* No Legible Output Support Found */ > WTFHeaderDetection.h
    VERBATIM)

set(WTF_OUTPUT_NAME WTF${DEBUG_SUFFIX})
