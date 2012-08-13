LIST(APPEND WebKit2_LINK_FLAGS
    ${CAIRO_LDFLAGS}
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EFREET_LDFLAGS}
    ${EVAS_LDFLAGS}
)

LIST(APPEND WebKit2_SOURCES
    Platform/efl/ModuleEfl.cpp
    Platform/efl/WorkQueueEfl.cpp
    Platform/unix/SharedMemoryUnix.cpp

    Platform/CoreIPC/unix/ConnectionUnix.cpp
    Platform/CoreIPC/unix/AttachmentUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/API/c/gtk/WKGraphicsContextGtk.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/efl/LayerTreeContextEfl.cpp
    Shared/efl/NativeWebKeyboardEventEfl.cpp
    Shared/efl/NativeWebWheelEventEfl.cpp
    Shared/efl/NativeWebMouseEventEfl.cpp
    Shared/efl/ProcessExecutablePathEfl.cpp
    Shared/efl/WebEventFactory.cpp

    Shared/soup/PlatformCertificateInfo.cpp
    Shared/soup/WebCoreArgumentCodersSoup.cpp

    UIProcess/API/C/efl/WKView.cpp
    
    UIProcess/API/cpp/efl/WKEinaSharedString.cpp

    UIProcess/API/C/soup/WKContextSoup.cpp
    UIProcess/API/C/soup/WKSoupRequestManager.cpp

    UIProcess/API/efl/BatteryProvider.cpp
    UIProcess/API/efl/EflViewportHandler.cpp
    UIProcess/API/efl/NetworkInfoProvider.cpp
    UIProcess/API/efl/PageClientImpl.cpp
    UIProcess/API/efl/VibrationProvider.cpp
    UIProcess/API/efl/ewk_back_forward_list.cpp
    UIProcess/API/efl/ewk_back_forward_list_item.cpp
    UIProcess/API/efl/ewk_context.cpp
    UIProcess/API/efl/ewk_context_download_client.cpp
    UIProcess/API/efl/ewk_context_request_manager_client.cpp
    UIProcess/API/efl/ewk_cookie_manager.cpp
    UIProcess/API/efl/ewk_download_job.cpp
    UIProcess/API/efl/ewk_form_submission_request.cpp
    UIProcess/API/efl/ewk_intent.cpp
    UIProcess/API/efl/ewk_intent_service.cpp
    UIProcess/API/efl/ewk_main.cpp
    UIProcess/API/efl/ewk_navigation_policy_decision.cpp
    UIProcess/API/efl/ewk_url_request.cpp
    UIProcess/API/efl/ewk_url_response.cpp
    UIProcess/API/efl/ewk_url_scheme_request.cpp
    UIProcess/API/efl/ewk_view.cpp
    UIProcess/API/efl/ewk_view_find_client.cpp
    UIProcess/API/efl/ewk_view_form_client.cpp
    UIProcess/API/efl/ewk_view_loader_client.cpp
    UIProcess/API/efl/ewk_view_policy_client.cpp
    UIProcess/API/efl/ewk_view_resource_load_client.cpp
    UIProcess/API/efl/ewk_view_ui_client.cpp
    UIProcess/API/efl/ewk_web_error.cpp
    UIProcess/API/efl/ewk_web_resource.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/efl/TextCheckerEfl.cpp
    UIProcess/efl/WebContextEfl.cpp
    UIProcess/efl/WebFullScreenManagerProxyEfl.cpp
    UIProcess/efl/WebInspectorProxyEfl.cpp
    UIProcess/efl/WebPageProxyEfl.cpp
    UIProcess/efl/WebPreferencesEfl.cpp

    UIProcess/soup/WebCookieManagerProxySoup.cpp
    UIProcess/soup/WebSoupRequestManagerClient.cpp
    UIProcess/soup/WebSoupRequestManagerProxy.cpp

    UIProcess/Launcher/efl/ProcessLauncherEfl.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp
    WebProcess/Cookies/soup/WebKitSoupCookieJarSqlite.cpp

    WebProcess/Downloads/efl/DownloadSoupErrorsEfl.cpp
    WebProcess/Downloads/soup/DownloadSoup.cpp

    WebProcess/efl/WebProcessEfl.cpp
    WebProcess/efl/WebProcessMainEfl.cpp

    WebProcess/InjectedBundle/efl/InjectedBundleEfl.cpp

    WebProcess/WebCoreSupport/efl/WebContextMenuClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebEditorClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebErrorsEfl.cpp
    WebProcess/WebCoreSupport/efl/WebPopupMenuEfl.cpp

    WebProcess/WebPage/efl/WebInspectorEfl.cpp
    WebProcess/WebPage/efl/WebPageEfl.cpp

    WebProcess/soup/WebSoupRequestManager.cpp
    WebProcess/soup/WebKitSoupRequestGeneric.cpp
    WebProcess/soup/WebKitSoupRequestInputStream.cpp
)

LIST(APPEND WebKit2_MESSAGES_IN_FILES
    UIProcess/soup/WebSoupRequestManagerProxy.messages.in
    WebProcess/soup/WebSoupRequestManager.messages.in
)

LIST(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/llint"
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBKIT2_DIR}/Shared/efl"
    "${WEBKIT2_DIR}/Shared/soup"
    "${WEBKIT2_DIR}/UIProcess/API/C/efl"
    "${WEBKIT2_DIR}/UIProcess/API/C/soup"
    "${WEBKIT2_DIR}/UIProcess/API/cpp/efl"
    "${WEBKIT2_DIR}/UIProcess/API/efl"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/Downloads/soup"
    "${WEBKIT2_DIR}/WebProcess/efl"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/efl"
    "${WTF_DIR}/wtf/gobject"
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EFREET_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WTF_DIR}
)

LIST(APPEND WebKit2_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EFREET_LIBRARIES}
    ${Freetype_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${PNG_LIBRARY}
    ${JPEG_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

LIST (APPEND WebProcess_SOURCES
    efl/MainEfl.cpp
)

LIST (APPEND WebProcess_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

ADD_DEFINITIONS(-DDEFAULT_THEME_PATH=\"${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/themes\")

ADD_CUSTOM_TARGET(forwarding-headerEfl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
)
SET(ForwardingHeaders_NAME forwarding-headerEfl)

ADD_CUSTOM_TARGET(forwarding-headerSoup
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
)
SET(ForwardingNetworkHeaders_NAME forwarding-headerSoup)

CONFIGURE_FILE(efl/ewebkit2.pc.in ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2.pc @ONLY)
SET (EWebKit2_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/EWebKit2.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_back_forward_list.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_back_forward_list_item.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_cookie_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_download_job.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_form_submission_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_intent.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_intent_service.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_main.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_navigation_policy_decision.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_url_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_url_response.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_url_scheme_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_view.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_web_error.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_web_resource.h"
)

INSTALL(FILES ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2.pc DESTINATION lib/pkgconfig)
INSTALL(FILES ${EWebKit2_HEADERS} DESTINATION include/${WebKit2_LIBRARY_NAME}-${PROJECT_VERSION_MAJOR})

INCLUDE_DIRECTORIES(${THIRDPARTY_DIR}/gtest/include)

SET(EWK2UnitTests_LIBRARIES
    ${WTF_LIBRARY_NAME}
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebKit2_LIBRARY_NAME}
    ${CAIRO_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    gtest
)

IF (ENABLE_GLIB_SUPPORT)
    LIST(APPEND EWK2UnitTests_LIBRARIES
        ${GLIB_LIBRARIES}
        ${GLIB_GTHREAD_LIBRARIES}
    )
ENDIF()

SET(WEBKIT2_EFL_TEST_DIR "${WEBKIT2_DIR}/UIProcess/API/efl/tests")
SET(TEST_RESOURCES_DIR ${WEBKIT2_EFL_TEST_DIR}/resources)

ADD_DEFINITIONS(-DTEST_RESOURCES_DIR=\"${TEST_RESOURCES_DIR}\"
    -DTEST_THEME_DIR=\"${THEME_BINARY_DIR}\"
    -DGTEST_LINKED_AS_SHARED_LIBRARY=1
    -DLIBEXECDIR=\"${CMAKE_INSTALL_PREFIX}/${EXEC_INSTALL_DIR}\"
    -DWEBPROCESSNAME=\"${WebProcess_EXECUTABLE_NAME}\"
    -DPLUGINPROCESSNAME=\"${PluginProcess_EXECUTABLE_NAME}\"
)

ADD_LIBRARY(ewk2UnitTestUtils
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestBase.cpp
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestEnvironment.cpp
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestMain.cpp
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestServer.cpp
)

TARGET_LINK_LIBRARIES(ewk2UnitTestUtils ${EWK2UnitTests_LIBRARIES})

# The "ewk" on the test name needs to be suffixed with "2", otherwise it
# will clash with tests from the WebKit 1 test suite.
SET(EWK2UnitTests_BINARIES
    test_ewk2_context
    test_ewk2_cookie_manager
    test_ewk2_download_job
    test_ewk2_view
)

IF (ENABLE_API_TESTS)
    FOREACH (testName ${EWK2UnitTests_BINARIES})
        ADD_EXECUTABLE(${testName} ${WEBKIT2_EFL_TEST_DIR}/${testName}.cpp)
        ADD_TEST(${testName} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${testName})
        SET_TESTS_PROPERTIES(${testName} PROPERTIES TIMEOUT 60)
        TARGET_LINK_LIBRARIES(${testName} ${EWK2UnitTests_LIBRARIES} ewk2UnitTestUtils)
    ENDFOREACH ()
ENDIF ()

IF (ENABLE_INSPECTOR)
    SET(WK2_WEB_INSPECTOR_DIR ${CMAKE_BINARY_DIR}/WebKit2/efl/webinspector)
    SET(WK2_WEB_INSPECTOR_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${WebKit2_LIBRARY_NAME}-${PROJECT_VERSION_MAJOR})
    ADD_DEFINITIONS(-DWK2_WEB_INSPECTOR_DIR="${WK2_WEB_INSPECTOR_DIR}")
    ADD_DEFINITIONS(-DWK2_WEB_INSPECTOR_INSTALL_DIR="${WK2_WEB_INSPECTOR_INSTALL_DIR}/webinspector")
    ADD_CUSTOM_TARGET(
        wk2-web-inspector-resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${WEBCORE_DIR}/inspector/front-end ${WK2_WEB_INSPECTOR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${WEBCORE_DIR}/English.lproj/localizedStrings.js ${WK2_WEB_INSPECTOR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorBackendCommands.js ${WK2_WEB_INSPECTOR_DIR}/InspectorBackendCommands.js
        DEPENDS ${WebCore_LIBRARY_NAME}
    )
    INSTALL(DIRECTORY ${WK2_WEB_INSPECTOR_DIR}
        DESTINATION ${WK2_WEB_INSPECTOR_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.js"
                       PATTERN "*.html"
                       PATTERN "*.css"
                       PATTERN "*.gif"
                       PATTERN "*.png")
ENDIF ()
