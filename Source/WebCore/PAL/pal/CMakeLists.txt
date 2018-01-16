set(PAL_SOURCES
    FileSizeFormatter.cpp
    Logging.cpp
    SessionID.cpp

    system/SleepDisabler.cpp
    system/SystemSleepListener.cpp
)

set(PAL_INCLUDE_DIRECTORIES
    "${PAL_DIR}"
)

set(PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_DIR}/pal"
    "${PAL_DIR}/pal/crypto"
    "${PAL_DIR}/pal/system"
    "${PAL_DIR}/pal/text"
)

set(PAL_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

set(PAL_LIBRARIES
    JavaScriptCore${DEBUG_SUFFIX}
)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_WRAP_SOURCELIST(${PAL_SOURCES})
WEBKIT_FRAMEWORK(PAL)
