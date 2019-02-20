set(WebKitGLibAPITests_SOURCES
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/LoadTrackingTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/WebKitTestBus.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/WebKitTestServer.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/TestMain.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/WebViewTest.cpp
)

set(WebKitGLibAPITests_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Source/WTF
    ${FORWARDING_HEADERS_DIR}
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib
)

set(WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(WebKitAPITest_LIBRARIES
    JavaScriptCore
    WebKit
)

set(WebKitGLibAPITest_LIBRARIES
    ${WebKitAPITest_LIBRARIES}
    WebKitGLibAPITestsCore
    ${GLIB_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

set(WebKitGLibAPITestExtension_LIBRARIES
    JavaScriptCore
    WebKit
    ${GLIB_LIBRARIES}
)

set(WebKitGLibAPIWebProcessTests
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/DOMElementTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/EditorTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/FrameTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/WebProcessTest.cpp
)

macro(ADD_WK2_TEST_WEB_EXTENSION extension_name)
    add_library(${extension_name} MODULE ${ARGN})
    add_dependencies(${extension_name} WebKit)
    set_property(
        TARGET ${extension_name}
        APPEND
        PROPERTY COMPILE_DEFINITIONS WEBKIT2_COMPILATION
    )
    set_target_properties(${extension_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${TEST_LIBRARY_DIR}
    )
    target_link_libraries(${extension_name} ${WebKitGLibAPITestExtension_LIBRARIES})

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${extension_name} -Wno-unused-parameter)
    endif ()
endmacro()

macro(ADD_WK2_TEST test_name)
    add_executable(${test_name} ${ARGN})
    add_dependencies(${test_name}
        test-gresource-bundle
        WebExtensionTest
    )
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TEST_BINARY_DIR}
    )
    target_link_libraries(${test_name} ${WebKitGLibAPITest_LIBRARIES})

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${test_name} -Wno-unused-parameter)
    endif ()
endmacro()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

set(TEST_RESOURCES_DIR ${TEST_BINARY_DIR}/resources)
file(MAKE_DIRECTORY ${TEST_RESOURCES_DIR})

add_definitions(
    -DWEBKIT_TEST_PLUGIN_DIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins"
    -DWEBKIT_EXEC_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    -DWEBKIT_SRC_DIR="${CMAKE_SOURCE_DIR}"
    -DWEBKIT_TEST_WEB_EXTENSIONS_DIR="${TEST_LIBRARY_DIR}"
    -DWEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    -DWEBKIT_TEST_RESOURCES_DIR="${TEST_RESOURCES_DIR}"
)

include_directories(${WebKitGLibAPITests_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES})

add_library(WebKitGLibAPITestsCore STATIC ${WebKitGLibAPITests_SOURCES})
target_link_libraries(WebKitGLibAPITestsCore WebKit)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitGLibAPITestsCore -Wno-unused-parameter)
endif ()

add_custom_command(
    OUTPUT ${TEST_RESOURCES_DIR}/webkitglib-tests-resources.gresource
    DEPENDS ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/resources/webkitglib-tests.gresource.xml
            ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/resources/link-title.js
    COMMAND glib-compile-resources
            --target=${TEST_RESOURCES_DIR}/webkitglib-tests-resources.gresource
            --sourcedir=${CMAKE_SOURCE_DIR}
            ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/resources/webkitglib-tests.gresource.xml
)

add_custom_target(test-gresource-bundle
    DEPENDS ${TEST_RESOURCES_DIR}/webkitglib-tests-resources.gresource
)

ADD_WK2_TEST_WEB_EXTENSION(WebExtensionTest ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/WebExtensionTest.cpp)
ADD_WK2_TEST_WEB_EXTENSION(WebProcessTest ${WebKitGLibAPIWebProcessTests})

ADD_WK2_TEST(TestAuthentication ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestAuthentication.cpp)
ADD_WK2_TEST(TestAutomationSession ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestAutomationSession.cpp)
ADD_WK2_TEST(TestBackForwardList ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestBackForwardList.cpp)
ADD_WK2_TEST(TestDownloads ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestDownloads.cpp)
ADD_WK2_TEST(TestWebKitFaviconDatabase ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitFaviconDatabase.cpp)
ADD_WK2_TEST(TestWebKitFindController ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitFindController.cpp)
ADD_WK2_TEST(TestEditor ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestEditor.cpp)
ADD_WK2_TEST(TestFrame ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestFrame.cpp)
ADD_WK2_TEST(TestLoaderClient ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestLoaderClient.cpp)
ADD_WK2_TEST(TestMultiprocess ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestMultiprocess.cpp)
ADD_WK2_TEST(TestResources ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestResources.cpp)
ADD_WK2_TEST(TestSSL ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestSSL.cpp)
ADD_WK2_TEST(TestUIClient ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestUIClient.cpp)
ADD_WK2_TEST(TestWebExtensions ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebExtensions.cpp)
ADD_WK2_TEST(TestWebKitPolicyClient ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitPolicyClient.cpp)
ADD_WK2_TEST(TestWebKitSecurityOrigin ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitSecurityOrigin.cpp)
ADD_WK2_TEST(TestWebKitSettings ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitSettings.cpp)
ADD_WK2_TEST(TestWebKitURIUtilities ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitURIUtilities.cpp)
ADD_WK2_TEST(TestWebKitWebContext ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitWebContext.cpp)
ADD_WK2_TEST(TestWebKitWebView ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitWebView.cpp)
ADD_WK2_TEST(TestWebKitUserContentFilterStore ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitUserContentFilterStore.cpp)
ADD_WK2_TEST(TestWebKitUserContentManager ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitUserContentManager.cpp)
ADD_WK2_TEST(TestWebsiteData ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebsiteData.cpp)
ADD_WK2_TEST(TestConsoleMessage ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestConsoleMessage.cpp)
ADD_WK2_TEST(TestDOMElement ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestDOMElement.cpp)

# FIXME: Enable for WPE
if (PORT STREQUAL "GTK")
    ADD_WK2_TEST(TestCookieManager ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestCookieManager.cpp)
endif ()

macro(ADD_WPE_QT_TEST test_name)
    add_executable(${test_name} ${ARGN} ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/WPEQtTest.cpp)
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WPEQt
        AUTOMOC ON
        CXX_STANDARD 14
    )
    target_compile_definitions(${test_name} PUBLIC QT_NO_KEYWORDS=1)
    target_link_libraries(${test_name} ${WPEQtAPITest_LIBRARIES})
    target_include_directories(${test_name} SYSTEM PRIVATE ${WPEQtAPITests_INCLUDE_DIRECTORIES})
    target_include_directories(${test_name} PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/wpe/qt)
endmacro()

if (PORT STREQUAL "WPE" AND ENABLE_WPE_QT_API)
    set(WPEQtAPITests_INCLUDE_DIRECTORIES
        ${Qt5_INCLUDE_DIRS}
        ${Qt5Test_INCLUDE_DIRS}
    )
    set(WPEQtAPITest_LIBRARIES
         ${WebKitAPITest_LIBRARIES}
         Qt5::Test qtwpe
    )
    ADD_WPE_QT_TEST(TestLoad ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestLoad.cpp)
    ADD_WPE_QT_TEST(TestLoadHtml ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestLoadHtml.cpp)
    ADD_WPE_QT_TEST(TestLoadRequest ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestLoadRequest.cpp)
    ADD_WPE_QT_TEST(TestRunJavaScript ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestRunJavaScript.cpp)
endif ()
