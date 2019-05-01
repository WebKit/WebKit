set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

list(APPEND TestWTF_SOURCES generic/main.cpp)

if (LOWERCASE_EVENT_LOOP_TYPE STREQUAL "glib")
    list(APPEND TestWTF_PRIVATE_INCLUDE_DIRECTORIES
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
