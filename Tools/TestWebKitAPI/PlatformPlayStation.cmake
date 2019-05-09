set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

set(test_main_SOURCES
    generic/main.cpp
)

list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}

    generic/UtilitiesGeneric.cpp
)

list(APPEND TestWTF_PRIVATE_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}
)
