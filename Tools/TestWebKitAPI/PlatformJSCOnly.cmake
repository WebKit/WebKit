set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

include_directories(
    ${FORWARDING_HEADERS_DIR}
)

if (LOWERCASE_EVENT_LOOP_TYPE STREQUAL "glib")
    include_directories(SYSTEM
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND TestWTF_SOURCES
        ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    )
else ()
    list(APPEND TestWTF_SOURCES
        ${TESTWEBKITAPI_DIR}/generic/UtilitiesGeneric.cpp
    )
endif ()

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/generic/main.cpp
)
