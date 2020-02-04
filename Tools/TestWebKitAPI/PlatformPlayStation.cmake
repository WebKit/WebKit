set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

set(test_main_SOURCES
    generic/main.cpp
)

list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}

    generic/UtilitiesGeneric.cpp
)

list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}
)

# TestWebKit
if (ENABLE_WEBKIT)
    add_dependencies(TestWebKitAPIBase WebKitFrameworkHeaders)
    add_dependencies(TestWebKitAPIInjectedBundle WebKitFrameworkHeaders)

    target_sources(TestWebKitAPIInjectedBundle PRIVATE
        generic/UtilitiesGeneric.cpp

        playstation/PlatformUtilitiesPlayStation.cpp
    )

    list(APPEND TestWebKit_SOURCES
        ${test_main_SOURCES}

        Tests/WebKit/curl/Certificates.cpp

        generic/UtilitiesGeneric.cpp

        playstation/PlatformUtilitiesPlayStation.cpp
        playstation/PlatformWebViewPlayStation.cpp
    )

    list(APPEND TestWebKit_DEPENDENCIES
        WebKitFrameworkHeaders
    )
endif ()
