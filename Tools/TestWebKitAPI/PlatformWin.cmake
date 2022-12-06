set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND wrapper_DEFINITIONS WIN_CAIRO)
endif ()

set(test_main_SOURCES
    win/main.cpp
)

# TestWTF
list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}
)

WEBKIT_WRAP_EXECUTABLE(TestWTF
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(TestWTF PRIVATE ${wrapper_DEFINITIONS})
set(TestWTF_OUTPUT_NAME TestWTF${DEBUG_SUFFIX})

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}

    Tests/WebCore/curl/OpenSSLHelperTests.cpp
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

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND TestWebCore_SOURCES
        Tests/WebCore/CryptoDigest.cpp

        Tests/WebCore/curl/Cookies.cpp

        Tests/WebCore/win/BitmapImage.cpp
    )
else ()
    list(APPEND TestWebCore_LIBRARIES
        Apple::ApplicationServices
        Apple::CFNetwork
        Apple::CoreGraphics
        Apple::CoreText
        Apple::QuartzCore
        Apple::libdispatch
        LibXslt::LibExslt
        WebKitQuartzCoreAdditions${DEBUG_SUFFIX}
    )
endif ()

if (USE_CF)
    list(APPEND TestWebCore_LIBRARIES
        Apple::CoreFoundation
    )
endif ()

WEBKIT_WRAP_EXECUTABLE(TestWebCore
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(TestWebCore PRIVATE ${wrapper_DEFINITIONS})
set(TestWebCore_OUTPUT_NAME TestWebCore${DEBUG_SUFFIX})

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

    WEBKIT_WRAP_EXECUTABLE(TestWebKitLegacy
        SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(TestWebKitLegacy PRIVATE ${wrapper_DEFINITIONS})
    set(TestWebKitLegacy_OUTPUT_NAME TestWebKitLegacy${DEBUG_SUFFIX})
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

    if (${WTF_PLATFORM_WIN_CAIRO})
        list(APPEND TestWebKit_SOURCES
            Tests/WebKit/curl/Certificates.cpp
        )
    endif ()

    WEBKIT_WRAP_EXECUTABLE(TestWebKit
        SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(TestWebKit PRIVATE ${wrapper_DEFINITIONS})
    set(TestWebKit_OUTPUT_NAME TestWebKit${DEBUG_SUFFIX})
endif ()
