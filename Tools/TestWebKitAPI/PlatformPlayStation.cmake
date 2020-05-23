set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

set(test_main_SOURCES
    playstation/main.cpp
)

list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}

    generic/UtilitiesGeneric.cpp
)

# Both bmalloc and WTF are built as object libraries. The WebKit:: interface
# targets are used. A limitation of that is the object files are not propagated
# so they are added here.
list(APPEND TestWTF_PRIVATE_LIBRARIES
    $<TARGET_OBJECTS:WTF>
    $<TARGET_OBJECTS:bmalloc>
)

list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}
)

# Both PAL and WebCore are built as object libraries. The WebKit:: interface
# targets are used. A limitation of that is the object files are not propagated
# so they are added here.
list(APPEND TestWebCore_PRIVATE_LIBRARIES
    $<TARGET_OBJECTS:PAL>
    $<TARGET_OBJECTS:WebCore>
)

# TestWebKit
if (ENABLE_WEBKIT)
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

    # Exclude tests which don't finish.
    list(REMOVE_ITEM TestWebKit_SOURCES
        Tests/WebKit/ForceRepaint.cpp
        Tests/WebKit/Geolocation.cpp
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
