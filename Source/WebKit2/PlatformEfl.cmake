list(APPEND WebKit2_SOURCES
    DatabaseProcess/efl/DatabaseProcessMainEfl.cpp

    NetworkProcess/CustomProtocols/soup/CustomProtocolManagerImpl.cpp
    NetworkProcess/CustomProtocols/soup/CustomProtocolManagerSoup.cpp

    NetworkProcess/Downloads/efl/DownloadSoupErrorsEfl.cpp

    NetworkProcess/Downloads/soup/DownloadSoup.cpp

    NetworkProcess/efl/NetworkProcessMainEfl.cpp

    NetworkProcess/soup/NetworkProcessSoup.cpp
    NetworkProcess/soup/RemoteNetworkingContextSoup.cpp

    Platform/IPC/unix/AttachmentUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp

    Platform/efl/LoggingEfl.cpp
    Platform/efl/ModuleEfl.cpp

    Platform/unix/SharedMemoryUnix.cpp

    PluginProcess/unix/PluginControllerProxyUnix.cpp
    PluginProcess/unix/PluginProcessMainUnix.cpp
    PluginProcess/unix/PluginProcessUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/API/c/efl/WKArrayEfl.cpp

    Shared/Authentication/soup/AuthenticationManagerSoup.cpp

    Shared/CoordinatedGraphics/CoordinatedBackingStore.cpp
    Shared/CoordinatedGraphics/CoordinatedGraphicsArgumentCoders.cpp
    Shared/CoordinatedGraphics/CoordinatedGraphicsScene.cpp
    Shared/CoordinatedGraphics/WebCoordinatedSurface.cpp

    Shared/Plugins/Netscape/x11/NetscapePluginModuleX11.cpp

    Shared/Plugins/unix/PluginSearchPath.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/efl/NativeWebKeyboardEventEfl.cpp
    Shared/efl/NativeWebTouchEventEfl.cpp
    Shared/efl/NativeWebWheelEventEfl.cpp
    Shared/efl/ProcessExecutablePathEfl.cpp
    Shared/efl/WebEventFactory.cpp

    Shared/linux/WebMemorySamplerLinux.cpp

    Shared/linux/SeccompFilters/OpenSyscall.cpp
    Shared/linux/SeccompFilters/SeccompBroker.cpp
    Shared/linux/SeccompFilters/SeccompFilters.cpp
    Shared/linux/SeccompFilters/SigactionSyscall.cpp
    Shared/linux/SeccompFilters/SigprocmaskSyscall.cpp
    Shared/linux/SeccompFilters/Syscall.cpp
    Shared/linux/SeccompFilters/SyscallPolicy.cpp
    Shared/linux/SeccompFilters/XDGBaseDirectoryGLib.cpp

    Shared/soup/WebCoreArgumentCodersSoup.cpp

    Shared/unix/ChildProcessMain.cpp

    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp

    UIProcess/API/C/CoordinatedGraphics/WKView.cpp

    UIProcess/API/C/cairo/WKIconDatabaseCairo.cpp

    UIProcess/API/C/efl/WKColorPickerResultListener.cpp
    UIProcess/API/C/efl/WKEventEfl.cpp
    UIProcess/API/C/efl/WKPageEfl.cpp
    UIProcess/API/C/efl/WKPopupItem.cpp
    UIProcess/API/C/efl/WKPopupMenuListener.cpp
    UIProcess/API/C/efl/WKViewEfl.cpp

    UIProcess/API/C/soup/WKCookieManagerSoup.cpp
    UIProcess/API/C/soup/WKSoupCustomProtocolRequestManager.cpp

    UIProcess/API/CoordinatedGraphics/WKCoordinatedScene.cpp

    UIProcess/API/cpp/efl/WKEinaSharedString.cpp

    UIProcess/API/efl/APIWebsiteDataStoreEfl.cpp
    UIProcess/API/efl/EwkView.cpp
    UIProcess/API/efl/GestureRecognizer.cpp
    UIProcess/API/efl/SnapshotImageGL.cpp
    UIProcess/API/efl/WebAccessibility.cpp
    UIProcess/API/efl/ewk_application_cache_manager.cpp
    UIProcess/API/efl/ewk_auth_request.cpp
    UIProcess/API/efl/ewk_back_forward_list.cpp
    UIProcess/API/efl/ewk_back_forward_list_item.cpp
    UIProcess/API/efl/ewk_color_picker.cpp
    UIProcess/API/efl/ewk_context.cpp
    UIProcess/API/efl/ewk_context_menu.cpp
    UIProcess/API/efl/ewk_context_menu_item.cpp
    UIProcess/API/efl/ewk_cookie_manager.cpp
    UIProcess/API/efl/ewk_database_manager.cpp
    UIProcess/API/efl/ewk_download_job.cpp
    UIProcess/API/efl/ewk_error.cpp
    UIProcess/API/efl/ewk_favicon_database.cpp
    UIProcess/API/efl/ewk_file_chooser_request.cpp
    UIProcess/API/efl/ewk_form_submission_request.cpp
    UIProcess/API/efl/ewk_main.cpp
    UIProcess/API/efl/ewk_navigation_data.cpp
    UIProcess/API/efl/ewk_navigation_policy_decision.cpp
    UIProcess/API/efl/ewk_object.cpp
    UIProcess/API/efl/ewk_page_group.cpp
    UIProcess/API/efl/ewk_popup_menu.cpp
    UIProcess/API/efl/ewk_popup_menu_item.cpp
    UIProcess/API/efl/ewk_security_origin.cpp
    UIProcess/API/efl/ewk_settings.cpp
    UIProcess/API/efl/ewk_storage_manager.cpp
    UIProcess/API/efl/ewk_text_checker.cpp
    UIProcess/API/efl/ewk_url_request.cpp
    UIProcess/API/efl/ewk_url_response.cpp
    UIProcess/API/efl/ewk_url_scheme_request.cpp
    UIProcess/API/efl/ewk_view_configuration.cpp
    UIProcess/API/efl/ewk_view.cpp
    UIProcess/API/efl/ewk_window_features.cpp

    UIProcess/CoordinatedGraphics/CoordinatedDrawingAreaProxy.cpp
    UIProcess/CoordinatedGraphics/CoordinatedLayerTreeHostProxy.cpp
    UIProcess/CoordinatedGraphics/PageViewportController.cpp
    UIProcess/CoordinatedGraphics/WebPageProxyCoordinatedGraphics.cpp
    UIProcess/CoordinatedGraphics/WebView.cpp
    UIProcess/CoordinatedGraphics/WebViewClient.cpp

    UIProcess/Databases/efl/DatabaseProcessProxyEfl.cpp

    UIProcess/InspectorServer/efl/WebInspectorServerEfl.cpp

    UIProcess/InspectorServer/soup/WebSocketServerSoup.cpp

    UIProcess/Launcher/efl/ProcessLauncherEfl.cpp

    UIProcess/Network/CustomProtocols/soup/CustomProtocolManagerProxySoup.cpp
    UIProcess/Network/CustomProtocols/soup/WebSoupCustomProtocolRequestManager.cpp
    UIProcess/Network/CustomProtocols/soup/WebSoupCustomProtocolRequestManagerClient.cpp

    UIProcess/Network/soup/NetworkProcessProxySoup.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp
    UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

    UIProcess/Storage/StorageManager.cpp

    UIProcess/WebsiteData/unix/WebsiteDataStoreUnix.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/efl/BatteryProvider.cpp
    UIProcess/efl/ContextHistoryClientEfl.cpp
    UIProcess/efl/ContextMenuClientEfl.cpp
    UIProcess/efl/DownloadManagerEfl.cpp
    UIProcess/efl/EasingCurves.cpp
    UIProcess/efl/EwkTouchEvent.cpp
    UIProcess/efl/EwkTouchPoint.cpp
    UIProcess/efl/FindClientEfl.cpp
    UIProcess/efl/FormClientEfl.cpp
    UIProcess/efl/InputMethodContextEfl.cpp
    UIProcess/efl/PageLoadClientEfl.cpp
    UIProcess/efl/PagePolicyClientEfl.cpp
    UIProcess/efl/PageUIClientEfl.cpp
    UIProcess/efl/PageViewportControllerClientEfl.cpp
    UIProcess/efl/RequestManagerClientEfl.cpp
    UIProcess/efl/TextCheckerClientEfl.cpp
    UIProcess/efl/TextCheckerEfl.cpp
    UIProcess/efl/VibrationClientEfl.cpp
    UIProcess/efl/ViewClientEfl.cpp
    UIProcess/efl/WebColorPickerClient.cpp
    UIProcess/efl/WebColorPickerEfl.cpp
    UIProcess/efl/WebColorPickerResultListenerProxy.cpp
    UIProcess/efl/WebContextMenuProxyEfl.cpp
    UIProcess/efl/WebInspectorProxyEfl.cpp
    UIProcess/efl/WebPageProxyEfl.cpp
    UIProcess/efl/WebPopupItemEfl.cpp
    UIProcess/efl/WebPopupMenuListenerEfl.cpp
    UIProcess/efl/WebPreferencesEfl.cpp
    UIProcess/efl/WebProcessPoolEfl.cpp
    UIProcess/efl/WebProcessProxyEfl.cpp
    UIProcess/efl/WebUIPopupMenuClient.cpp
    UIProcess/efl/WebViewEfl.cpp

    UIProcess/gstreamer/InstallMissingMediaPluginsPermissionRequest.cpp
    UIProcess/gstreamer/WebPageProxyGStreamer.cpp

    UIProcess/soup/WebCookieManagerProxySoup.cpp
    UIProcess/soup/WebProcessPoolSoup.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp
    WebProcess/Cookies/soup/WebKitSoupCookieJarSqlite.cpp

    WebProcess/InjectedBundle/API/efl/ewk_extension.cpp
    WebProcess/InjectedBundle/API/efl/ewk_page.cpp

    WebProcess/InjectedBundle/efl/InjectedBundleEfl.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/Plugins/Netscape/unix/NetscapePluginUnix.cpp
    WebProcess/Plugins/Netscape/unix/PluginProxyUnix.cpp

    WebProcess/Plugins/Netscape/x11/NetscapePluginX11.cpp

    WebProcess/WebCoreSupport/efl/WebContextMenuClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebEditorClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebErrorsEfl.cpp
    WebProcess/WebCoreSupport/efl/WebPopupMenuEfl.cpp

    WebProcess/WebCoreSupport/soup/WebFrameNetworkingContext.cpp

    WebProcess/WebPage/DrawingAreaImpl.cpp

    WebProcess/WebPage/CoordinatedGraphics/CoordinatedDrawingArea.cpp
    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/WebPageCoordinatedGraphics.cpp

    WebProcess/WebPage/atk/WebPageAccessibilityObjectAtk.cpp

    WebProcess/WebPage/efl/WebInspectorUIEfl.cpp
    WebProcess/WebPage/efl/WebPageEfl.cpp

    WebProcess/WebPage/gstreamer/WebPageGStreamer.cpp

    WebProcess/efl/ExtensionManagerEfl.cpp
    WebProcess/efl/SeccompFiltersWebProcessEfl.cpp
    WebProcess/efl/WebProcessMainEfl.cpp

    WebProcess/soup/WebKitSoupRequestInputStream.cpp
    WebProcess/soup/WebProcessSoup.cpp
)

list(APPEND WebKit2_MESSAGES_IN_FILES
    UIProcess/CoordinatedGraphics/CoordinatedLayerTreeHostProxy.messages.in

    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.messages.in
)

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/efl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/x11"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/enchant"
    "${WEBKIT2_DIR}/DatabaseProcess/unix"
    "${WEBKIT2_DIR}/NetworkProcess/CustomProtocols/soup"
    "${WEBKIT2_DIR}/NetworkProcess/Downloads/soup"
    "${WEBKIT2_DIR}/NetworkProcess/efl"
    "${WEBKIT2_DIR}/NetworkProcess/unix"
    "${WEBKIT2_DIR}/Platform/efl"
    "${WEBKIT2_DIR}/Shared/API/c/efl"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT2_DIR}/Shared/Plugins/unix"
    "${WEBKIT2_DIR}/Shared/glib"
    "${WEBKIT2_DIR}/Shared/efl"
    "${WEBKIT2_DIR}/Shared/soup"
    "${WEBKIT2_DIR}/Shared/unix"
    "${WEBKIT2_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT2_DIR}/UIProcess/API/C/CoordinatedGraphics"
    "${WEBKIT2_DIR}/UIProcess/API/C/efl"
    "${WEBKIT2_DIR}/UIProcess/API/C/soup"
    "${WEBKIT2_DIR}/UIProcess/API/cpp/efl"
    "${WEBKIT2_DIR}/UIProcess/API/efl"
    "${WEBKIT2_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols/soup"
    "${WEBKIT2_DIR}/UIProcess/efl"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/efl"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/unix"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/efl"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape/unix"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape/x11"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/efl"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT2_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WTF_DIR}/wtf/efl"
    "${WTF_DIR}/wtf/glib"
    "${WTF_DIR}"
    "${WEBKIT2_DIR}"
)

list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_IMF_EVAS_INCLUDE_DIRS}
    ${ECORE_IMF_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFREET_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EO_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
)

list(APPEND WebKit2_LIBRARIES
    WTF
    ${CAIRO_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_IMF_EVAS_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFREET_LIBRARIES}
    ${EINA_LIBRARIES}
    ${EO_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND DatabaseProcess_SOURCES
    DatabaseProcess/EntryPoint/unix/DatabaseProcessMain.cpp
)

list(APPEND WebProcess_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_IMF_EVAS_LIBRARIES}
    ${ECORE_IMF_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

if (ENABLE_SECCOMP_FILTERS)
    list(APPEND WebKit2_LIBRARIES
        ${LIBSECCOMP_LIBRARIES}
    )
    list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
        ${LIBSECCOMP_INCLUDE_DIRS}
    )

    # If building with jhbuild, add the root build directory to the
    # filesystem access policy.
    if (DEVELOPER_MODE AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/WebKitBuild/DependenciesEFL)
        add_definitions(-DSOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")
    endif ()
endif ()

if (ENABLE_ECORE_X)
    list(APPEND WebProcess_LIBRARIES
        ${ECORE_X_LIBRARIES}
    )
    list(APPEND WebKit2_LIBRARIES
        ${ECORE_X_LIBRARIES}
        ${X11_Xext_LIB}
    )
endif ()

add_custom_target(forwarding-headersEflForWebKit2
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT2_DIR} --output ${DERIVED_SOURCES_WEBKIT2_DIR}/include --platform efl --platform CoordinatedGraphics --platform soup
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     forwarding-headersEflForWebKit2
)

configure_file(efl/ewebkit2.pc.in ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2.pc @ONLY)
configure_file(efl/ewebkit2-extension.pc.in ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2-extension.pc @ONLY)
configure_file(efl/EWebKit2Config.cmake.in ${CMAKE_BINARY_DIR}/WebKit2/efl/EWebKit2Config.cmake @ONLY)
configure_file(efl/EWebKit2ConfigVersion.cmake.in ${CMAKE_BINARY_DIR}/WebKit2/efl/EWebKit2ConfigVersion.cmake @ONLY)
configure_file(UIProcess/API/efl/EWebKit2.h.in ${DERIVED_SOURCES_WEBKIT2_DIR}/include/EWebKit2.h)

set(EWebKit2_HEADERS
    "${DERIVED_SOURCES_WEBKIT2_DIR}/include/EWebKit2.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_auth_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_back_forward_list.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_back_forward_list_item.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_application_cache_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_color_picker.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_context_menu.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_context_menu_item.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_cookie_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_database_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_download_job.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_error.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_favicon_database.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_file_chooser_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_form_submission_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_intro.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_main.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_navigation_data.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_navigation_policy_decision.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_object.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_page_group.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_popup_menu.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_popup_menu_item.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_security_origin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_settings.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_storage_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_text_checker.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_touch.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_url_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_url_response.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_url_scheme_request.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_view.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_window_features.h"
)

set(EWebKit2_Extension_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/WebProcess/InjectedBundle/API/efl/EWebKit_Extension.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/WebProcess/InjectedBundle/API/efl/ewk_extension.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/WebProcess/InjectedBundle/API/efl/ewk_page.h"
)

install(FILES ${EWebKit2_HEADERS} DESTINATION ${HEADER_INSTALL_DIR})
install(FILES ${EWebKit2_Extension_HEADERS} DESTINATION ${HEADER_INSTALL_DIR}/extension)

install(FILES ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2.pc DESTINATION lib/pkgconfig)
install(FILES ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2-extension.pc DESTINATION lib/pkgconfig)
install(FILES
        ${CMAKE_BINARY_DIR}/WebKit2/efl/EWebKit2Config.cmake
        ${CMAKE_BINARY_DIR}/WebKit2/efl/EWebKit2ConfigVersion.cmake
        DESTINATION lib/cmake/EWebKit2)

set(EWEBKIT_EXTENSION_MANAGER_INSTALL_DIR "${LIB_INSTALL_DIR}/${WebKit2_OUTPUT_NAME}-${PROJECT_VERSION_MAJOR}/" CACHE PATH "Absolute path to install injected bundle which controls the extension library")

add_library(ewebkit_extension_manager SHARED "${WEBKIT2_DIR}/WebProcess/efl/WebInjectedBundleMainEfl.cpp")
target_link_libraries(ewebkit_extension_manager WebKit2)

install(TARGETS ewebkit_extension_manager DESTINATION "${EWEBKIT_EXTENSION_MANAGER_INSTALL_DIR}")

if (ENABLE_PLUGIN_PROCESS)
    list(APPEND PluginProcess_INCLUDE_DIRECTORIES
        "${WEBKIT2_DIR}/PluginProcess/unix"
    )

    include_directories(${PluginProcess_INCLUDE_DIRECTORIES})

    list(APPEND PluginProcess_SOURCES
        ${WEBKIT2_DIR}/PluginProcess/EntryPoint/unix/PluginProcessMain.cpp
    )

    if (ENABLE_ECORE_X)
        list(APPEND PluginProcess_LIBRARIES
            ${ECORE_X_LIBRARIES}
        )
    endif ()
endif () # ENABLE_PLUGIN_PROCESS

include_directories(${THIRDPARTY_DIR}/gtest/include)

set(EWK2UnitTests_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_GTHREAD_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    JavaScriptCore
    WTF
    WebCore
    WebKit2
    gtest
)

if (ENABLE_ECORE_X)
    list(APPEND EWK2UnitTests_LIBRARIES
        ${ECORE_X_LIBRARIES}
    )
endif ()

set(WEBKIT2_EFL_TEST_DIR "${WEBKIT2_DIR}/UIProcess/API/efl/tests")
set(TEST_RESOURCES_DIR ${WEBKIT2_EFL_TEST_DIR}/resources)
set(TEST_EXTENSIONS_DIR ${WEBKIT2_EFL_TEST_DIR}/extensions)
set(WEBKIT2_EFL_TEST_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/EWebKit2)

add_definitions(-DTEST_RESOURCES_DIR=\"${TEST_RESOURCES_DIR}\"
    -DTEST_LIB_DIR=\"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}\"
    -DGTEST_LINKED_AS_SHARED_LIBRARY=1
    -DLIBDIR=\"${LIB_INSTALL_DIR}\"
    -DLIBEXECDIR=\"${EXEC_INSTALL_DIR}\"
    -DDATADIR=\"${CMAKE_INSTALL_PREFIX}/share\"
    -DEXTENSIONMANAGERDIR=\"${CMAKE_INSTALL_PREFIX}/${EWEBKIT_EXTENSION_MANAGER_INSTALL_DIR}\"
    -DWEBPROCESSNAME=\"WebKitWebProcess\"
    -DPLUGINPROCESSNAME=\"WebKitPluginProcess\"
    -DNETWORKPROCESSNAME=\"WebKitNetworkProcess\"
    -DDATABASEPROCESSNAME=\"WebKitDatabaseProcess\"
    -DEXTENSIONMANAGERNAME=\"libewebkit_extension_manager.so\"
    -DGTEST_HAS_RTTI=0
)

add_library(ewk2UnitTestUtils
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestBase.cpp
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestEnvironment.cpp
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestMain.cpp
    ${WEBKIT2_EFL_TEST_DIR}/UnitTestUtils/EWK2UnitTestServer.cpp
)

target_link_libraries(ewk2UnitTestUtils ${EWK2UnitTests_LIBRARIES})

# The "ewk" on the test name needs to be suffixed with "2", otherwise it
# will clash with tests from the WebKit 1 test suite.
set(EWK2UnitTests_BINARIES
    test_ewk2_accessibility
    test_ewk2_application_cache_manager
    test_ewk2_auth_request
    test_ewk2_back_forward_list
    test_ewk2_color_picker
    test_ewk2_context
    test_ewk2_context_history_callbacks
    test_ewk2_context_menu
    test_ewk2_cookie_manager
    test_ewk2_database_manager
    test_ewk2_download_job
    test_ewk2_eina_shared_string
    test_ewk2_favicon_database
    test_ewk2_file_chooser_request
    test_ewk2_javascript_binding
    test_ewk2_object
    test_ewk2_page
    test_ewk2_page_group
    test_ewk2_popup_menu
    test_ewk2_settings
    test_ewk2_ssl
    test_ewk2_storage_manager
    test_ewk2_text_checker
    test_ewk2_view
    test_ewk2_view_configuration
    test_ewk2_window_features
)

# Skipped unit tests list:
#
# webkit.org/b/107422: test_ewk2_auth_request

if (ENABLE_API_TESTS)
    foreach (testName ${EWK2UnitTests_BINARIES})
        add_executable(${testName} ${WEBKIT2_EFL_TEST_DIR}/${testName}.cpp)
        add_test(${testName} ${WEBKIT2_EFL_TEST_RUNTIME_OUTPUT_DIRECTORY}/${testName})
        set_tests_properties(${testName} PROPERTIES TIMEOUT 60)
        set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${WEBKIT2_EFL_TEST_RUNTIME_OUTPUT_DIRECTORY})
        target_link_libraries(${testName} ${EWK2UnitTests_LIBRARIES} ewk2UnitTestUtils)
    endforeach ()

    add_library(ewk2UnitTestExtensionSample SHARED ${TEST_EXTENSIONS_DIR}/extension_sample.cpp)
    target_link_libraries(ewk2UnitTestExtensionSample ${EINA_LIBRARIES} WebKit2)
endif ()

if (ENABLE_SPELLCHECK)
    list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
        ${ENCHANT_INCLUDE_DIRS}
    )
    list(APPEND WebKit2_LIBRARIES
        ${ENCHANT_LIBRARIES}
    )
endif ()

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebKit2_INCLUDE_DIRECTORIES
        "${WEBKIT2_DIR}/WebProcess/WebPage/atk"
    )
    list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
        ${ATK_INCLUDE_DIRS}
    )
    list(APPEND WebKit2_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

if (ENABLE_BATTERY_STATUS)
    list(APPEND WebKit2_LIBRARIES
        ${ELDBUS_LIBRARIES}
    )
    list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
        ${ELDBUS_INCLUDE_DIRS}
    )
endif ()
