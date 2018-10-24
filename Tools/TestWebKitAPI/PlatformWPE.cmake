set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

# This is necessary because it is possible to build TestWebKitAPI with WebKit2
# disabled and this triggers the inclusion of the WebKit2 headers.
add_definitions(-DBUILDING_WEBKIT2__)

add_custom_target(TestWebKitAPI-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
    DEPENDS webkitwpe-forwarding-headers
)

list(APPEND TestWebKitAPI_DEPENDENCIES TestWebKitAPI-forwarding-headers)

include_directories(
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib
    ${DERIVED_SOURCES_JAVASCRIPCOREWPE_DIR}
    ${TOOLS_DIR}/wpe/backends
)

include_directories(SYSTEM
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/generic/main.cpp
)

set(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/wpe/InjectedBundleControllerWPE.cpp
    ${TESTWEBKITAPI_DIR}/wpe/PlatformUtilitiesWPE.cpp
)

set(webkit_api_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/wpe/PlatformUtilitiesWPE.cpp
    ${TESTWEBKITAPI_DIR}/wpe/PlatformWebViewWPE.cpp
)

# TestWTF

list(APPEND TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/glib/GUniquePtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/glib/WorkQueueGLib.cpp
)

# TestWebCore

add_executable(TestWebCore
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/glib/UtilitiesGLib.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/HTMLParserIdioms.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/MIMETypeRegistry.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBuffer.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBufferTest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FileMonitor.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FileSystem.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/PublicSuffix.cpp
)

target_link_libraries(TestWebCore ${test_webcore_LIBRARIES})
add_dependencies(TestWebCore ${TestWebKitAPI_DEPENDENCIES})

add_test(TestWebCore ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore/TestWebCore)
set_tests_properties(TestWebCore PROPERTIES TIMEOUT 60)
set_target_properties(TestWebCore PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore)

# TestWebKit

list(APPEND test_webkit_api_LIBRARIES
    WPEToolingBackends
)

add_executable(TestWebKit ${test_webkit_api_SOURCES})

target_link_libraries(TestWebKit ${test_webkit_api_LIBRARIES})
add_test(TestWebKit ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit/TestWebKit)
set_tests_properties(TestWebKit PROPERTIES TIMEOUT 60)
set_target_properties(TestWebKit PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit)

# TestWebKitAPIBase
list(APPEND TestWebKitAPIBase_LIBRARIES
    WPEBackend-fdo-0.1
)
find_package(WPEBackend-fdo REQUIRED)
list(APPEND TestWebKitAPI_LIBRARIES ${WPEBACKEND_FDO_LIBRARIES})
list(APPEND TestWebKitAPIBase_SOURCES
    ${TOOLS_DIR}/wpe/backends/ViewBackend.cpp
    ${TOOLS_DIR}/wpe/backends/HeadlessViewBackend.cpp
)

# TestJSC

add_definitions(-DWEBKIT_SRC_DIR="${CMAKE_SOURCE_DIR}")
add_executable(TestJSC ${TESTWEBKITAPI_DIR}/Tests/JavaScriptCore/glib/TestJSC.cpp)
target_link_libraries(TestJSC
    ${GLIB_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    JavaScriptCore
)
add_test(TestJSC ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/JavaScriptCore/TestJSC)
add_dependencies(TestJSC ${TestWebKitAPI_DEPENDENCIES})
set_tests_properties(TestJSC PROPERTIES TIMEOUT 60)
set_target_properties(TestJSC PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/JavaScriptCore)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebCore -Wno-sign-compare
                                            -Wno-undef
                                            -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKit -Wno-sign-compare
                                           -Wno-undef
                                           -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestJSC -Wno-sign-compare
                                        -Wno-undef
                                        -Wno-unused-parameter)
endif ()
