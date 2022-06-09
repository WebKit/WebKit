set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

add_custom_target(TestWebKitAPI-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
    DEPENDS WebKit-forwarding-headers
)

list(APPEND TestWebKit_DEPENDENCIES TestWebKitAPI-forwarding-headers)
add_dependencies(TestWebKitAPIInjectedBundle TestWebKitAPI-forwarding-headers)

set(test_main_SOURCES gtk/main.cpp)

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

list(APPEND TestWTF_LIBRARIES
    GTK::GTK
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${test_main_SOURCES}

    Tests/WebCore/UserAgentQuirks.cpp
    Tests/WebCore/gstreamer/GStreamerTest.cpp
    Tests/WebCore/gstreamer/GstMappedBuffer.cpp

    glib/UtilitiesGLib.cpp
)

list(APPEND TestWebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

list(APPEND TestWebCore_LIBRARIES
    GTK::GTK
)

# TestWebKit
list(APPEND TestWebKit_SOURCES
    ${test_main_SOURCES}

    glib/UtilitiesGLib.cpp

    gtk/PlatformUtilitiesGtk.cpp
    gtk/PlatformWebViewGtk.cpp
)

list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source"
)

list(APPEND TestWebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND TestWebKit_LIBRARIES
    GTK::GTK
)

# TestWebKitAPIBase
target_include_directories(TestWebKitAPIBase PRIVATE "${CMAKE_SOURCE_DIR}/Source")

# TestWebKitAPIInjectedBundle
target_sources(TestWebKitAPIInjectedBundle PRIVATE
    glib/UtilitiesGLib.cpp

    gtk/PlatformUtilitiesGtk.cpp
)
target_include_directories(TestWebKitAPIInjectedBundle PRIVATE
    "${CMAKE_SOURCE_DIR}/Source"
)
target_include_directories(TestWebKitAPIInjectedBundle SYSTEM PRIVATE
    ${GLIB_INCLUDE_DIRS}
)

# TestJSC
set(TestJSC_SOURCES
    Tests/JavaScriptCore/glib/TestJSC.cpp
)

set(TestJSC_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

set(TestJSC_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TESTWEBKITAPI_DIR}
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

# TestJavaScriptCore
list(APPEND TestJavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
