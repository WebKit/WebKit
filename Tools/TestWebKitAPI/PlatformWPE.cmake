set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

file(REMOVE_RECURSE ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY})
file(MAKE_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY})

add_custom_target(TestWebKitAPI-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
    DEPENDS webkitwpe-forwarding-headers
)

list(APPEND TestWebKit_DEPENDENCIES TestWebKitAPI-forwarding-headers)
add_dependencies(TestWebKitAPIInjectedBundle TestWebKitAPI-forwarding-headers)

set(test_main_SOURCES generic/main.cpp)

# TestWTF
list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}

    Tests/WTF/glib/ActivityObserver.cpp
    Tests/WTF/glib/GMallocString.cpp
    Tests/WTF/glib/GRefPtr.cpp
    Tests/WTF/glib/GUniquePtr.cpp
    Tests/WTF/glib/GWeakPtr.cpp
    Tests/WTF/glib/WorkQueueGLib.cpp
)

# TestJavaScriptCore
list(APPEND TestJavaScriptCore_SOURCES
    ${test_main_SOURCES}
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}

    Tests/WebCore/UserAgentQuirks.cpp

    Tests/WebCore/glib/Damage.cpp
    Tests/WebCore/glib/GraphicsContextGLTextureMapper.cpp
    Tests/WebCore/glib/RunLoopObserver.cpp

    Tests/WebCore/gstreamer/GStreamerTest.cpp
    Tests/WebCore/gstreamer/GstElementHarness.cpp
    Tests/WebCore/gstreamer/GstMappedBuffer.cpp
)

list(APPEND TestWebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
)

list(APPEND TestWebCore_LIBRARIES
    HarfBuzz::HarfBuzz
    HarfBuzz::ICU
)

# TestWebKit
list(APPEND TestWebKit_SOURCES
    wpe/PlatformUtilitiesWPE.cpp
    wpe/PlatformWebViewWPE.cpp
    wpe/WebKitTestMain.cpp
)

list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${FORWARDING_HEADERS_DIR}
)

if (ENABLE_WPE_LEGACY_API)
    list(APPEND TestWebKit_PRIVATE_LIBRARIES WebKit::WPEToolingBackends)
endif ()

if (ENABLE_WPE_PLATFORM)
    list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
        ${WPEPlatform_DERIVED_SOURCES_DIR}
        ${WEBKIT_DIR}/WPEPlatform
    )
endif ()

# TestWebKitAPIBase
target_include_directories(TestWebKitAPIBase PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${FORWARDING_HEADERS_DIR}
)

# TestWebKitAPIInjectedBundle
target_sources(TestWebKitAPIInjectedBundle PRIVATE
    wpe/PlatformUtilitiesWPE.cpp
)
target_include_directories(TestWebKitAPIInjectedBundle PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${FORWARDING_HEADERS_DIR}
)

# TestJSC
set(TestJSC_SOURCES
    Tests/JavaScriptCore/glib/TestJSC.cpp
)

set(TestJSC_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TESTWEBKITAPI_DIR}
    "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc"
)

# If developer_mode is enabled, to reduce binary bloat, link this binaries
# against the shared libWPEWebKit library rather than embedding the object
# files from the frameworks (statically linking).
# See detailed explanation at Source/JavaScriptCore/shell/PlatformWPE.cmake
if (DEVELOPER_MODE)
    list(APPEND TestJSC_PRIVATE_INCLUDE_DIRECTORIES
        "${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}"
        "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}"
    )
    set(TestJSC_FRAMEWORKS WebKit)
    set(TestJavaScriptCore_FRAMEWORKS WebKit)
else ()
    set(TestJSC_FRAMEWORKS
        JavaScriptCore
        WTF
    )
    if (NOT USE_SYSTEM_MALLOC)
        list(APPEND TestJSC_FRAMEWORKS bmalloc)
    endif ()
endif ()

set(TestJSC_DEFINITIONS
    WEBKIT_SRC_DIR="${CMAKE_SOURCE_DIR}"
)

WEBKIT_EXECUTABLE_DECLARE(TestJSC)
WEBKIT_TEST(TestJSC)
