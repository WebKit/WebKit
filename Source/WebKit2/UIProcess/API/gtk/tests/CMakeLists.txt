set(TEST_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit2GObjectAPITests)
set(TEST_RESOURCES_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources)
file(MAKE_DIRECTORY ${TEST_RESOURCES_DIR})

add_definitions(
    -DWEBKIT_TEST_PLUGIN_DIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    -DWEBKIT_EXEC_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    -DWEBKIT_SRC_DIR="${CMAKE_SRC_DIR}"
    -DWEBKIT_TEST_WEB_EXTENSIONS_DIR="${TEST_LIBRARY_DIR}"
    -DWEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Source/WTF
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
    ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}
    ${ATSPI_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

add_library(WebKit2APITestCore STATIC
    LoadTrackingTest.cpp
    WebKitTestBus.cpp
    WebKitTestServer.cpp
    WebProcessTestRunner.cpp
    TestMain.cpp
    WebViewTest.cpp
)
add_dependencies(WebKit2APITestCore WebKit2)

add_custom_target(test-gresource-bundle
    DEPENDS resources/webkit2gtk-tests.gresource.xml
            resources/link-title.js
    COMMAND glib-compile-resources --target=${TEST_RESOURCES_DIR}/webkit2gtk-tests-resources.gresource --sourcedir=${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/tests/resources/webkit2gtk-tests.gresource.xml
)

macro(ADD_WK2_TEST_WEB_EXTENSION extension_name)
    add_library(${extension_name} MODULE ${ARGN})
    set_property(
        TARGET ${extension_name}
        APPEND
        PROPERTY COMPILE_DEFINITIONS WEBKIT2_COMPILATION
    )
    set_target_properties(${extension_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${TEST_LIBRARY_DIR}
    )
endmacro()

macro(ADD_WK2_TEST test_name)
    add_executable(${test_name} ${ARGN})
    add_dependencies(${test_name}
        test-gresource-bundle
        WebExtensionTest
    )
    target_link_libraries(${test_name}
        JavaScriptCore
        WebKit2
        WebKit2APITestCore
        ${ATSPI_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${GTK3_LIBRARIES}
        ${LIBSOUP_LIBRARIES}
    )
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit2GObjectAPITests
    )
endmacro()

ADD_WK2_TEST_WEB_EXTENSION(WebExtensionTest WebExtensionTest.cpp)
ADD_WK2_TEST_WEB_EXTENSION(WebProcessTest DOMNodeTest.cpp FrameTest.cpp WebProcessTest.cpp)

ADD_WK2_TEST(InspectorTestServer InspectorTestServer.cpp)
ADD_WK2_TEST(TestBackForwardList TestBackForwardList.cpp)
ADD_WK2_TEST(TestContextMenu TestContextMenu.cpp)
ADD_WK2_TEST(TestCookieManager TestCookieManager.cpp)
ADD_WK2_TEST(TestDOMNode TestDOMNode.cpp)
ADD_WK2_TEST(TestDownloads TestDownloads.cpp)
ADD_WK2_TEST(TestFrame TestFrame.cpp)
ADD_WK2_TEST(TestInspector TestInspector.cpp)
ADD_WK2_TEST(TestInspectorServer TestInspectorServer.cpp)
ADD_WK2_TEST(TestLoaderClient TestLoaderClient.cpp)
ADD_WK2_TEST(TestPrinting TestPrinting.cpp)
ADD_WK2_TEST(TestSSL TestSSL.cpp)
ADD_WK2_TEST(TestWebExtensions TestWebExtensions.cpp)
ADD_WK2_TEST(TestWebKitPolicyClient TestWebKitPolicyClient.cpp)
ADD_WK2_TEST(TestWebKitSettings TestWebKitSettings.cpp)
ADD_WK2_TEST(TestWebKitVersion TestWebKitVersion.cpp)
ADD_WK2_TEST(TestWebKitWebViewGroup TestWebKitWebViewGroup.cpp)
ADD_WK2_TEST(TestWebKitWebContext TestWebKitWebContext.cpp)
ADD_WK2_TEST(TestWebKitWebView TestWebKitWebView.cpp)

if (ATSPI_FOUND)
    ADD_WK2_TEST(AccessibilityTestServer AccessibilityTestServer.cpp)
    ADD_WK2_TEST(TestWebKitAccessibility TestWebKitAccessibility.cpp)
endif ()
