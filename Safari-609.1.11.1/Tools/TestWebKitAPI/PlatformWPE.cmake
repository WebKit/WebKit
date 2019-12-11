set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

add_custom_target(TestWebKitAPI-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
    DEPENDS webkitwpe-forwarding-headers
)

list(APPEND TestWebKit_DEPENDENCIES TestWebKitAPI-forwarding-headers)
add_dependencies(TestWebKitAPIInjectedBundle TestWebKitAPI-forwarding-headers)

include_directories(SYSTEM
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

set(test_main_SOURCES generic/main.cpp)

# TestWTF
list(APPEND TestWTF_SOURCES
    ${test_main_SOURCES}

    Tests/WTF/glib/GUniquePtr.cpp
    Tests/WTF/glib/WorkQueueGLib.cpp

    glib/UtilitiesGLib.cpp
)

list(APPEND TestWTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}

    Tests/WebCore/gstreamer/GStreamerTest.cpp
    Tests/WebCore/gstreamer/GstMappedBuffer.cpp

    glib/UtilitiesGLib.cpp
)

list(APPEND TestWebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

# TestWebKit
list(APPEND TestWebKit_SOURCES
    ${test_main_SOURCES}

    glib/UtilitiesGLib.cpp

    wpe/PlatformUtilitiesWPE.cpp
    wpe/PlatformWebViewWPE.cpp
)

list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${FORWARDING_HEADERS_DIR}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
    ${TOOLS_DIR}/wpe/backends
)

list(APPEND TestWebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND TestWebKit_LIBRARIES
    ${WPEBACKEND_FDO_LIBRARIES}
    WPEToolingBackends
)

# TestWebKitAPIBase
target_include_directories(TestWebKitAPIBase PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${FORWARDING_HEADERS_DIR}
)

# TestWebKitAPIInjectedBundle
target_sources(TestWebKitAPIInjectedBundle PRIVATE
    glib/UtilitiesGLib.cpp

    wpe/InjectedBundleControllerWPE.cpp
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
    ${THIRDPARTY_DIR}/gtest/include
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib
    ${DERIVED_SOURCES_JAVASCRIPCOREWPE_DIR}
)

set(TestJSC_LIBRARIES
    ${GLIB_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    WebKit::JavaScriptCore
)

set(TestJSC_DEFINITIONS
    WEBKIT_SRC_DIR="${CMAKE_SOURCE_DIR}"
)

WEBKIT_EXECUTABLE_DECLARE(TestJSC)
WEBKIT_TEST(TestJSC)
