set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

# This is necessary because JSCOnly port does not support WebCore, WebKit and WebKit2.
# To build TestWTF, we need this flag not to include WebKit related headers except
# for WTF and JavaScriptCore.
add_definitions(-DBUILDING_JSCONLY__)

include_directories(
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    "${WTF_DIR}/icu"
)

if (LOWERCASE_EVENT_LOOP_TYPE STREQUAL "glib")
    include_directories(SYSTEM
        ${GLIB_INCLUDE_DIRS}
    )
endif ()

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/jsconly/main.cpp
)

list(APPEND TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/jsconly/PlatformUtilitiesJSCOnly.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RunLoop.cpp
)
