set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

set(test_main_SOURCES
    playstation/main.cpp
)

list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}
)
list(APPEND TestWTF_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBKIT_LIBRARIES_DIR}/include
)

list(APPEND TestJavaScriptCore_SOURCES
    ${test_main_SOURCES}
)
list(APPEND TestJavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBKIT_LIBRARIES_DIR}/include
)

list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}

    Tests/WebCore/curl/OpenSSLHelperTests.cpp
)
list(APPEND TestWebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBKIT_LIBRARIES_DIR}/include
)

# TestWebKit
if (ENABLE_WEBKIT)
    target_sources(TestWebKitAPIInjectedBundle PRIVATE
        playstation/PlatformUtilitiesPlayStation.cpp
    )

    list(APPEND TestWebKit_SOURCES
        ${test_main_SOURCES}

        Tests/WebKit/curl/Certificates.cpp

        playstation/PlatformUtilitiesPlayStation.cpp
        playstation/PlatformWebViewPlayStation.cpp
    )
    list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
        ${WEBKIT_LIBRARIES_DIR}/include
    )

    # Exclude tests which don't finish.
    list(REMOVE_ITEM TestWebKit_SOURCES
        Tests/WebKit/ForceRepaint.cpp
        Tests/WebKit/Geolocation.cpp
    )

    list(APPEND TestWebKit_PRIVATE_LIBRARIES
        ${ProcessLauncher_LIBRARY}
    )
endif ()

# Set the debugger working directory for Visual Studio
if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    set_target_properties(TestWTF PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if (ENABLE_WEBCORE)
        set_target_properties(TestWebCore PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif ()
    if (ENABLE_WEBCORE)
        set_target_properties(TestWebKit PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif ()
endif ()

add_definitions(
    -DTEST_WEBKIT_RESOURCES_DIR=\"${TOOLS_DIR}/TestWebKitAPI/Tests/WebKit\"
)
