set(WebKitGLibAPITests_SOURCES
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/LoadTrackingTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/TestMain.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/WebKitTestServer.cpp
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/WebViewTest.cpp
)

set(WebKitGLibAPITests_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${TOOLS_DIR}/TestWebKitAPI
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib
    ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib
    # FIXME Should we add a _PRIVATE_INCLUDE_DIRECTORIES for this?
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS}
    $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
)

if (USE_LIBWEBRTC)
    list(APPEND WebKitGLibAPITests_INCLUDE_DIRECTORIES
        "${THIRDPARTY_DIR}/libwebrtc/Source"
        "${THIRDPARTY_DIR}/libwebrtc/Source/third_party/abseil-cpp"
        "${THIRDPARTY_DIR}/libwebrtc/Source/webrtc"
    )
endif ()

set(WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(WebKitAPITest_LIBRARIES
    WebKit::WebKit
)

set(WebKitGLibAPITestsCore_LIBRARIES
    WebKit::WebKit
)

set(WebKitGLibAPITest_LIBRARIES
    ${GLIB_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${WebKitAPITest_LIBRARIES}
    WebKitGLibAPITestsCore
)

set(WebKitGLibAPITestExtension_LIBRARIES
    ${GLIB_LIBRARIES}
    WebKit::WebKit
)

set(WebKitGLibAPIWebProcessTests
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/DOMElementTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/EditorTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/FrameTest.cpp
    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/WebProcessTest.cpp
)

macro(ADD_WK2_TEST_WEB_PROCESS_EXTENSION extension_name)
    add_library(${extension_name} MODULE ${ARGN})
    add_dependencies(${extension_name} WebKit)
    set_target_properties(${extension_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${TEST_LIBRARY_DIR}
    )
    target_compile_definitions(${extension_name} PUBLIC ${WebKitGLibAPITests_DEFINITIONS})
    target_include_directories(${extension_name} PUBLIC ${WebKitGLibAPITests_INCLUDE_DIRECTORIES})
    target_include_directories(${extension_name} SYSTEM PUBLIC ${WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES})
    target_link_libraries(${extension_name} ${WebKitGLibAPITestExtension_LIBRARIES})

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${extension_name} -Wno-unused-parameter)
    endif ()
endmacro()

macro(ADD_WK2_TEST test_name)
    add_executable(${test_name} ${ARGN})
    add_dependencies(${test_name}
        test-gresource-bundle
        WebProcessExtensionTest
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

set(WebKitGLibAPITests_DEFINITIONS
    NO_GTEST_USAGE
    WEBKIT_TEST_PLUGIN_DIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins"
    WEBKIT_EXEC_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    WEBKIT_SRC_DIR="${CMAKE_SOURCE_DIR}"
    WEBKIT_TEST_WEB_PROCESS_EXTENSIONS_DIR="${TEST_LIBRARY_DIR}"
    WEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    WEBKIT_INSPECTOR_RESOURCES_PATH="${CMAKE_BINARY_DIR}/share"
    WEBKIT_TEST_RESOURCES_DIR="${TEST_RESOURCES_DIR}"
)

add_library(WebKitGLibAPITestsCore STATIC ${WebKitGLibAPITests_SOURCES})
target_include_directories(WebKitGLibAPITestsCore PUBLIC ${WebKitGLibAPITests_INCLUDE_DIRECTORIES})
target_include_directories(WebKitGLibAPITestsCore SYSTEM PUBLIC ${WebKitGLibAPITests_SYSTEM_INCLUDE_DIRECTORIES})
target_compile_definitions(WebKitGLibAPITestsCore PRIVATE BUILDING_TestWebKit)
target_compile_definitions(WebKitGLibAPITestsCore PUBLIC ${WebKitGLibAPITests_DEFINITIONS})
target_link_libraries(WebKitGLibAPITestsCore ${WebKitGLibAPITestsCore_LIBRARIES})

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitGLibAPITestsCore -Wno-unused-parameter)
endif ()

if (ENABLE_WPE_PLATFORM)
    add_executable(TestWPESettings
        ${TOOLS_DIR}/TestWebKitAPI/glib/WebKitGLib/TestMain.cpp
        ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEPlatform/TestSettings.cpp
    )
    target_link_libraries(TestWPESettings
        ${WebKitGLibAPITest_LIBRARIES}
        WPEPlatform-${WPE_API_VERSION}
    )
    target_include_directories(TestWPESettings PRIVATE
        ${WebKitGLibAPITests_INCLUDE_DIRECTORIES}
        ${WPEPlatform_DERIVED_SOURCES_DIR}
        ${WEBKIT_DIR}/WPEPlatform
    )
    target_compile_definitions(TestWPESettings PRIVATE ${WebKitGLibAPITests_DEFINITIONS})
    set_target_properties(TestWPESettings PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WPEPlatform)
    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWPESettings -Wno-unused-parameter)
    endif ()
endif ()

GLIB_COMPILE_RESOURCES(
    OUTPUT        ${TEST_RESOURCES_DIR}/webkitglib-tests-resources.gresource
    RESOURCE_DIRS ${CMAKE_SOURCE_DIR}
    SOURCE_XML    ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/resources/webkitglib-tests.gresource.xml
)

add_custom_target(test-gresource-bundle
    DEPENDS ${TEST_RESOURCES_DIR}/webkitglib-tests-resources.gresource
)

ADD_WK2_TEST_WEB_PROCESS_EXTENSION(WebProcessExtensionTest ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/WebProcessExtensionTest.cpp)
ADD_WK2_TEST_WEB_PROCESS_EXTENSION(WebProcessTest ${WebKitGLibAPIWebProcessTests})

ADD_WK2_TEST(TestAuthentication ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestAuthentication.cpp)
ADD_WK2_TEST(TestAutomationSession ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestAutomationSession.cpp)
ADD_WK2_TEST(TestBackForwardList ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestBackForwardList.cpp)
ADD_WK2_TEST(TestConsoleMessage ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestConsoleMessage.cpp)
ADD_WK2_TEST(TestCookieManager ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestCookieManager.cpp)
ADD_WK2_TEST(TestDownloads ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestDownloads.cpp)
ADD_WK2_TEST(TestWebKitFindController ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitFindController.cpp)
ADD_WK2_TEST(TestEditor ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestEditor.cpp)
ADD_WK2_TEST(TestFrame ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestFrame.cpp)
ADD_WK2_TEST(TestLoaderClient ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestLoaderClient.cpp)
ADD_WK2_TEST(TestMultiprocess ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestMultiprocess.cpp)
ADD_WK2_TEST(TestNetworkProcessMemoryPressure ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestNetworkProcessMemoryPressure.cpp)
ADD_WK2_TEST(TestOptionMenu ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestOptionMenu.cpp)
ADD_WK2_TEST(TestResources ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestResources.cpp)
ADD_WK2_TEST(TestSSL ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestSSL.cpp)
ADD_WK2_TEST(TestUIClient ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestUIClient.cpp)
ADD_WK2_TEST(TestWebProcessExtensions ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebProcessExtensions.cpp)
ADD_WK2_TEST(TestWebKitPolicyClient ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitPolicyClient.cpp)
ADD_WK2_TEST(TestWebKitSecurityOrigin ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitSecurityOrigin.cpp)
ADD_WK2_TEST(TestWebKitSettings ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitSettings.cpp)
ADD_WK2_TEST(TestWebKitURIUtilities ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitURIUtilities.cpp)
ADD_WK2_TEST(TestWebKitWebContext ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitWebContext.cpp)
ADD_WK2_TEST(TestWebKitWebView ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitWebView.cpp)
ADD_WK2_TEST(TestWebKitUserContentFilterStore ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitUserContentFilterStore.cpp)
ADD_WK2_TEST(TestWebKitUserContentManager ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitUserContentManager.cpp)
ADD_WK2_TEST(TestWebsiteData ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebsiteData.cpp)
ADD_WK2_TEST(TestDOMElement ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestDOMElement.cpp)
ADD_WK2_TEST(TestGeolocationManager ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestGeolocationManager.cpp)
ADD_WK2_TEST(TestInputMethodContext ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestInputMethodContext.cpp)

if (ENABLE_2022_GLIB_API)
    ADD_WK2_TEST(TestWebKitNetworkSession ${TOOLS_DIR}/TestWebKitAPI/Tests/WebKitGLib/TestWebKitNetworkSession.cpp)
endif ()

macro(ADD_WPE_QT_TEST test_name)
    add_executable(${test_name} ${ARGN} ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/WPEQtTest.cpp)
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/WPEQt
        AUTOMOC ON
    )
    target_include_directories(${test_name} SYSTEM PRIVATE ${WPEQtAPITests_INCLUDE_DIRECTORIES})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/wpe/qt${WPE_MAJOR_QT_VERSION}
    )
    target_compile_definitions(${test_name} PUBLIC
        ${WebKitGLibAPITests_DEFINITIONS}
        QT_NO_KEYWORDS=1
    )
    target_link_libraries(${test_name} ${WPEQtAPITest_LIBRARIES})
endmacro()

if (PORT STREQUAL "WPE" AND ENABLE_WPE_QT_API)
    set(WPEQtAPITests_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
        ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${LIBSOUP_INCLUDE_DIRS}
        ${TOOLS_DIR}/TestWebKitAPI
        ${WEBKIT_DIR}/WPEPlatform
        ${WPEPlatform_DERIVED_SOURCES_DIR}
    )

    if (USE_QT6)
        set(WPEQtAPITests_INCLUDE_DIRECTORIES
            ${WPEQtAPITests_INCLUDE_DIRECTORIES}
            ${Qt6Test_INCLUDE_DIRS}
            ${Qt6_INCLUDE_DIRS}
        )
    else ()
        set(WPEQtAPITests_INCLUDE_DIRECTORIES
            ${WPEQtAPITests_INCLUDE_DIRECTORIES}
            ${Qt5Test_INCLUDE_DIRS}
            ${Qt5_INCLUDE_DIRS}
        )
    endif ()

    set(WPEQtAPITest_LIBRARIES
         ${WebKitAPITest_LIBRARIES}
         Qt${WPE_MAJOR_QT_VERSION}::Test qtwpe
    )
    ADD_WPE_QT_TEST(TestLoad ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestLoad.cpp)
    ADD_WPE_QT_TEST(TestLoadHtml ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestLoadHtml.cpp)
    ADD_WPE_QT_TEST(TestLoadRequest ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestLoadRequest.cpp)
    ADD_WPE_QT_TEST(TestRunJavaScript ${TOOLS_DIR}/TestWebKitAPI/Tests/WPEQt/TestRunJavaScript.cpp)
endif ()
