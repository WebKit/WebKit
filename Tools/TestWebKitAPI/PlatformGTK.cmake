set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

# This is necessary because it is possible to build TestWebKitAPI with WebKit
# disabled and this triggers the inclusion of the WebKit headers.
add_definitions(-DBUILDING_WEBKIT2__)

add_custom_target(TestWebKitAPI-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
    DEPENDS WebKit-forwarding-headers
)

set(ForwardingHeadersForTestWebKitAPI_NAME TestWebKitAPI-forwarding-headers)

include_directories(
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore
    ${WEBKIT_DIR}/UIProcess/API/C/soup
    ${WEBKIT_DIR}/UIProcess/API/C/gtk
    ${WEBKIT_DIR}/UIProcess/API/gtk
)

include_directories(SYSTEM
    ${GDK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/main.cpp
)

set(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/gtk/InjectedBundleControllerGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformUtilitiesGtk.cpp
)

set(webkit_api_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformUtilitiesGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformWebViewGtk.cpp
)

list(APPEND test_wtf_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webkit_api_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webcore_LIBRARIES
    WebCorePlatformGTK
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)
ADD_WHOLE_ARCHIVE_TO_LIBRARIES(test_webcore_LIBRARIES)

list(APPEND TestWebKitAPI_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND TestJavaScriptCore_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webkit_api_SOURCES
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/gtk/InputMethodFilter.cpp
)

add_executable(TestWebKit ${test_webkit_api_SOURCES})

target_link_libraries(TestWebKit ${test_webkit_api_LIBRARIES})
add_test(TestWebKit ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit/TestWebKit)
set_tests_properties(TestWebKit PROPERTIES TIMEOUT 60)
set_target_properties(TestWebKit PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit)

add_executable(TestWebCore
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/CSSParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/ComplexTextController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FileMonitor.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FileSystem.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/GridPosition.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/HTMLParserIdioms.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/MIMETypeRegistry.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/PublicSuffix.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SecurityOrigin.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBuffer.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBufferTest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URLParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/UserAgentQuirks.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SampleMap.cpp
)

target_link_libraries(TestWebCore ${test_webcore_LIBRARIES})
add_dependencies(TestWebCore ${ForwardingHeadersForTestWebKitAPI_NAME})

add_test(TestWebCore ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore/TestWebCore)
set_tests_properties(TestWebCore PROPERTIES TIMEOUT 60)
set_target_properties(TestWebCore PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore)

list(APPEND TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/Tests/WTF/glib/GUniquePtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/glib/WorkQueueGLib.cpp
)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKit -Wno-sign-compare
                                           -Wno-undef
                                           -Wno-unused-parameter)

    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebCore -Wno-sign-compare
                                            -Wno-undef
                                            -Wno-unused-parameter)
endif ()
