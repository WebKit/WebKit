set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}")
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

add_custom_target(forwarding-headersWinForTestWebKitAPI
    COMMAND ${CMAKE_BINARY_DIR}/DerivedSources/WebCore/preBuild.cmd VERBATIM
)
set(ForwardingHeadersForTestWebKitAPI_NAME forwarding-headersWinForTestWebKitAPI)

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/win/main.cpp
)

include_directories(
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    ${TESTWEBKITAPI_DIR}/win
    ${DERIVED_SOURCES_DIR}/WebKit/Interfaces
)

add_definitions(-DWEBCORE_EXPORT=)

set(test_webcore_LIBRARIES
    Crypt32
    Iphlpapi
    Psapi
    Shlwapi
    Usp10
    WebCore${DEBUG_SUFFIX}
    WebKit${DEBUG_SUFFIX}
    gtest
)

set(TestWebCoreLib_SOURCES
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/CalculationValue.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/CSSParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/HTMLParserIdioms.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/ParsedContentRange.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/TimeRanges.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URL.cpp
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND test_webcore_LIBRARIES
        cairo
        libANGLE
        libeay32
        mfuuid
        ssleay32
        strmiids
        vcruntime
    )
    list(APPEND TestWebCoreLib_SOURCES
        ${TESTWEBKITAPI_DIR}/Tests/WebCore/win/BitmapImage.cpp
    )
else ()
    list(APPEND test_webcore_LIBRARIES
        ASL${DEBUG_SUFFIX}
        CFNetwork${DEBUG_SUFFIX}
        CoreFoundation${DEBUG_SUFFIX}
        CoreGraphics${DEBUG_SUFFIX}
        QuartzCore${DEBUG_SUFFIX}
        SQLite3${DEBUG_SUFFIX}
        WebKitSystemInterface${DEBUG_SUFFIX}
        WebKitQuartzCoreAdditions${DEBUG_SUFFIX}
        libdispatch${DEBUG_SUFFIX}
        libexslt${DEBUG_SUFFIX}
        libicuin${DEBUG_SUFFIX}
        libicuuc${DEBUG_SUFFIX}
        libxml2${DEBUG_SUFFIX}
        libxslt${DEBUG_SUFFIX}
        zdll${DEBUG_SUFFIX}
    )
endif ()

add_library(TestWTFLib SHARED
    ${test_main_SOURCES}
    ${TestWTF_SOURCES}
)
set_target_properties(TestWTFLib PROPERTIES OUTPUT_NAME "TestWTFLib")
target_link_libraries(TestWTFLib ${test_wtf_LIBRARIES})

set(test_wtf_LIBRARIES
    shlwapi
)
set(TestWTF_SOURCES
)

add_library(TestWebCoreLib SHARED
    ${TestWebCoreLib_SOURCES}
)

target_link_libraries(TestWebCoreLib ${test_webcore_LIBRARIES})
set_target_properties(TestWebCoreLib PROPERTIES OUTPUT_NAME "TestWebCoreLib")

add_executable(TestWebCore
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)
target_link_libraries(TestWebCore shlwapi)


add_test(TestWebCore ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebCore)
set_tests_properties(TestWebCore PROPERTIES TIMEOUT 60)

add_library(TestWebKitLib SHARED
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/win/WebViewDestruction.cpp
    ${TESTWEBKITAPI_DIR}/win/HostWindow.cpp
)

target_link_libraries(TestWebKitLib ${test_webcore_LIBRARIES})

add_executable(TestWebKit
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)
target_link_libraries(TestWebKit shlwapi)

add_test(TestWebKit ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebKit)
set_tests_properties(TestWebKit PROPERTIES TIMEOUT 60)

set(test_main_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)

add_dependencies(TestWebCore TestWebCoreLib)
add_dependencies(TestWebKit TestWebKitLib)
