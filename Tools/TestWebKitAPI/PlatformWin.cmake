set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}")
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/win/main.cpp
)

include_directories(
    ${DERIVED_SOURCES_DIR}
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore
    ${TESTWEBKITAPI_DIR}/win
    ${DERIVED_SOURCES_DIR}/WebKit/Interfaces
)

add_definitions(-DWEBCORE_EXPORT=)

set(test_webcore_LIBRARIES
    Crypt32
    D2d1
    Dwrite
    dxguid
    Iphlpapi
    Psapi
    Shlwapi
    Usp10
    WebCore${DEBUG_SUFFIX}
    WindowsCodecs
    gtest
)

set(TestWebCoreLib_SOURCES
    ${test_main_SOURCES}
    win/TestWebCoreStubs.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/AffineTransform.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/CalculationValue.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/ComplexTextController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/CSSParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FloatRect.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FloatPoint.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FloatSize.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/GridPosition.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/HTMLParserIdioms.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/IntRect.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/IntPoint.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/IntSize.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/MIMETypeRegistry.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/ParsedContentRange.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SecurityOrigin.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBuffer.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBufferTest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/TimeRanges.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/TransformationMatrix.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URLParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/win/DIBPixelData.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/win/LinkedFonts.cpp
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND test_webcore_LIBRARIES
        ${CAIRO_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        libANGLE
        mfuuid
        strmiids
        vcruntime
    )
    list(APPEND TestWebCoreLib_SOURCES
        ${TESTWEBKITAPI_DIR}/Tests/WebCore/curl/Cookies.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebCore/win/BitmapImage.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebCore/CryptoDigest.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebCore/PublicSuffix.cpp
    )
else ()
    list(APPEND test_webcore_LIBRARIES
        ASL${DEBUG_SUFFIX}
        CFNetwork${DEBUG_SUFFIX}
        CoreGraphics${DEBUG_SUFFIX}
        CoreText${DEBUG_SUFFIX}
        QuartzCore${DEBUG_SUFFIX}
        WebKitSystemInterface${DEBUG_SUFFIX}
        WebKitQuartzCoreAdditions${DEBUG_SUFFIX}
        libdispatch${DEBUG_SUFFIX}
        libexslt${DEBUG_SUFFIX}
        libicuin${DEBUG_SUFFIX}
        libicuuc${DEBUG_SUFFIX}
    )
endif ()

if (USE_CF)
    list(APPEND test_webcore_LIBRARIES
        ${COREFOUNDATION_LIBRARY}
    )
endif ()

list(APPEND TestWebKitAPI_DEPENDENCIES WebCoreForwardingHeaders)
if (ENABLE_WEBKIT)
    list(APPEND TestWebKitAPI_DEPENDENCIES WebKitForwardingHeaders)
endif ()

add_library(TestWTFLib SHARED
    ${test_main_SOURCES}
    ${TestWTF_SOURCES}
)
set_target_properties(TestWTFLib PROPERTIES OUTPUT_NAME "TestWTFLib")
target_link_libraries(TestWTFLib ${test_wtf_LIBRARIES})
add_dependencies(TestWTFLib ${TestWebKitAPI_DEPENDENCIES})

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
add_dependencies(TestWebCoreLib ${TestWebKitAPI_DEPENDENCIES})

if (PAL_LIBRARY_TYPE MATCHES STATIC)
    target_compile_definitions(TestWebCoreLib PRIVATE -DSTATICALLY_LINKED_WITH_PAL=1)
endif ()

add_executable(TestWebCore
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)
target_link_libraries(TestWebCore shlwapi)


add_test(TestWebCore ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebCore)
set_tests_properties(TestWebCore PROPERTIES TIMEOUT 60)

if (${WTF_PLATFORM_WIN_CAIRO})
    include_directories(
        ${CAIRO_INCLUDE_DIRS}
    )
endif ()

set(test_webkitlegacy_LIBRARIES
    WebCoreTestSupport
    WebKitLegacy${DEBUG_SUFFIX}
    gtest
)

if (ENABLE_WEBKIT_LEGACY)
    add_library(TestWebKitLegacyLib SHARED
        ${test_main_SOURCES}
        ${TESTWEBKITAPI_DIR}/TestsController.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKitLegacy/win/ScaleWebView.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKitLegacy/win/WebViewDestruction.cpp
        ${TESTWEBKITAPI_DIR}/win/HostWindow.cpp
    )

    target_link_libraries(TestWebKitLegacyLib ${test_webkitlegacy_LIBRARIES})
    add_dependencies(TestWebKitLegacyLib ${TestWebKitAPI_DEPENDENCIES})

    add_executable(TestWebKitLegacy
        ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    )
    target_link_libraries(TestWebKitLegacy shlwapi)

    add_test(TestWebKitLegacy ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitLegacy)
    set_tests_properties(TestWebKitLegacy PROPERTIES TIMEOUT 60)

    add_dependencies(TestWebKitLegacy TestWebKitLegacyLib)
endif ()

if (ENABLE_WEBKIT)
    set(bundle_harness_SOURCES
        ${TESTWEBKITAPI_DIR}/win/UtilitiesWin.cpp
        ${TESTWEBKITAPI_DIR}/win/InjectedBundleControllerWin.cpp
        ${TESTWEBKITAPI_DIR}/win/PlatformUtilitiesWin.cpp
    )

    set(webkit_api_harness_SOURCES
        ${TESTWEBKITAPI_DIR}/win/PlatformUtilitiesWin.cpp
        ${TESTWEBKITAPI_DIR}/win/PlatformWebViewWin.cpp
        ${TESTWEBKITAPI_DIR}/win/UtilitiesWin.cpp
    )

    if (${WTF_PLATFORM_WIN_CAIRO})
        list(APPEND test_webkit_api_SOURCES
            ${TESTWEBKITAPI_DIR}/Tests/WebKit/curl/Certificates.cpp
        )
    endif ()

    add_library(TestWebKitLib SHARED
        ${TESTWEBKITAPI_DIR}/win/main.cpp
        ${test_webkit_api_SOURCES}
    )

    target_link_libraries(TestWebKitLib ${test_webkit_api_LIBRARIES})

    add_executable(TestWebKit
        ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    )
    target_link_libraries(TestWebKit shlwapi)

    add_test(TestWebKit ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit/TestWebKit)
    set_tests_properties(TestWebKit PROPERTIES TIMEOUT 60)
    set_target_properties(TestWebKit PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit)

    add_dependencies(TestWebKit TestWebKitAPIBase)
endif ()

set(test_main_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)

add_dependencies(TestWebCore TestWebCoreLib)
