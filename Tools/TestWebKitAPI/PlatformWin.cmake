set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

set(test_main_SOURCES
    win/main.cpp
)

# TestWTF
list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}

    Tests/WebCore/CryptoDigest.cpp

    Tests/WebCore/curl/Cookies.cpp
    Tests/WebCore/curl/CurlMultipartHandleTests.cpp
    Tests/WebCore/curl/OpenSSLHelperTests.cpp

    Tests/WebCore/win/BitmapImage.cpp
    Tests/WebCore/win/DIBPixelData.cpp
    Tests/WebCore/win/LinkedFonts.cpp
    Tests/WebCore/win/WebCoreBundle.cpp

    win/TestWebCoreStubs.cpp
)

list(APPEND TestWebCore_LIBRARIES
    Crypt32
    D2d1
    Dwrite
    Iphlpapi
    Psapi
    Shlwapi
    Usp10
    WindowsCodecs
    dxguid
)

target_compile_definitions(TestWebCore PRIVATE ${wrapper_DEFINITIONS})

# TestWebKitLegacy
if (ENABLE_WEBKIT_LEGACY)
    list(APPEND TestWebKitLegacy_SOURCES
        ${test_main_SOURCES}

        Tests/WebKitLegacy/win/ScaleWebView.cpp
        Tests/WebKitLegacy/win/WebViewDestruction.cpp

        win/HostWindow.cpp
    )

    list(APPEND TestWebKitLegacy_LIBRARIES
        WebKit::WTF
    )

    list(APPEND TestWebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
        ${TESTWEBKITAPI_DIR}/win
    )
endif ()

# TestWebKit
if (ENABLE_WEBKIT)
    target_sources(TestWebKitAPIInjectedBundle PRIVATE
        win/PlatformUtilitiesWin.cpp
    )

    list(APPEND TestWebKit_SOURCES
        ${test_main_SOURCES}

        Tests/WebKit/CookieStorageFile.cpp

        win/PlatformUtilitiesWin.cpp
        win/PlatformWebViewWin.cpp
    )
endif ()
