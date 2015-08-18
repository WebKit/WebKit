set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}")
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/win/main.cpp
)

include_directories(
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    ${TESTWEBKITAPI_DIR}/win
)

add_definitions(-DWEBCORE_EXPORT=)

set(test_webcore_LIBRARIES
    Crypt32
    Iphlpapi
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
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
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
        CFNetwork
        CoreMedia
        WebKitSystemInterface
    )
endif ()

add_library(TestWTFLib SHARED
    ${test_main_SOURCES}
    ${TestWTF_SOURCES}
)
set_target_properties(TestWTFLib PROPERTIES OUTPUT_NAME "TestWTF")
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
set_target_properties(TestWebCoreLib PROPERTIES OUTPUT_NAME "TestWebCore")

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
set_target_properties(TestWebKitLib PROPERTIES OUTPUT_NAME "TestWebKit")

add_executable(TestWebKit
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)
target_link_libraries(TestWebKit shlwapi)

add_test(TestWebKit ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebKit)
set_tests_properties(TestWebKit PROPERTIES TIMEOUT 60)

set(test_main_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)
