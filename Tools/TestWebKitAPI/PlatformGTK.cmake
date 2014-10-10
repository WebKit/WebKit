# This is necessary because it is possible to build TestWebKitAPI with WebKit2
# disabled and this triggers the inclusion of the WebKit2 headers.
add_definitions(-DBUILDING_WEBKIT2__)

set(ForwardingHeadersForTestWebKitAPI_NAME TestWebKitAPI-forwarding-headers)

include_directories(
    ${FORWARDING_HEADERS_DIR}
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WEBKIT2_DIR}/UIProcess/API/C/gtk
    ${WEBKIT2_DIR}/UIProcess/API/gtk
    ${GDK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)
set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/main.cpp
)

set(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/InjectedBundleControllerGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformUtilitiesGtk.cpp
)

set(webkit2_api_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/PlatformUtilitiesGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformWebViewGtk.cpp
)

list(APPEND test_wtf_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webkit2_api_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webcore_LIBRARIES
    WebCorePlatformGTK
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND TestWebKitAPI_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND TestJavaScriptCore_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

add_executable(TestWebKit2
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/AboutBlankLoad.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/CanHandleRequest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/CookieManager.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DocumentStartUserScriptAlertCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DOMWindowExtensionBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DOMWindowExtensionNoCache.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DownloadDecideDestinationCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/EvaluateJavaScript.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/FailedLoad.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/Find.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ForceRepaint.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/FrameMIMETypeHTML.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/FrameMIMETypePNG.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/Geolocation.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/GetInjectedBundleInitializationUserDataCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/HitTestResultNodeHandle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/InjectedBundleBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/InjectedBundleFrameHitTest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/InjectedBundleInitializationUserDataCallbackWins.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/LoadAlternateHTMLStringWithNonDirectoryURL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/LoadCanceledNoServerRedirectCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/LoadPageOnCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/MouseMoveAfterCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayout.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayoutFails.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayoutForImages.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayoutFrames.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/PageLoadBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/PageLoadDidChangeLocationWithinPageForFrame.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ParentFrame.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/PreventEmptyUserAgent.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/PrivateBrowsingPushStateNoHistoryCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ReloadPageAfterCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ResizeWindowAfterCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/RestoreSessionStateContainingFormData.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ShouldGoToBackForwardListItem.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/UserMessage.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WillSendSubmitEvent.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WKPageGetScaleFactorNotZero.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WKPreferences.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WKString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WKStringJSString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WKURL.cpp
)

target_link_libraries(TestWebKit2 ${test_webkit2_api_LIBRARIES})
add_test(TestWebKit2 ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit2/TestWebKit2)
set_tests_properties(TestWebKit2 PROPERTIES TIMEOUT 60)
set_target_properties(TestWebKit2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit2)

set(TestWebCoreGtk_SOURCES
    ${WEBCORE_DIR}/platform/graphics/IntPoint.cpp
    ${WEBCORE_DIR}/platform/graphics/IntRect.cpp
    ${WEBCORE_DIR}/platform/graphics/IntSize.cpp
    ${WEBCORE_DIR}/platform/graphics/cairo/IntRectCairo.cpp
    ${WEBCORE_DIR}/platform/gtk/GtkInputMethodFilter.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/gtk/InputMethodFilter.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/gtk/UserAgentQuirks.cpp
)

add_executable(TestWebCore
    ${test_main_SOURCES}
    ${TestWebCoreGtk_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URL.cpp
)

target_link_libraries(TestWebCore ${test_webcore_LIBRARIES})
add_test(TestWebCore ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore/TestWebCore)
set_tests_properties(TestWebCore PROPERTIES TIMEOUT 60)
set_target_properties(TestWebCore PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore)

list(APPEND TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/Tests/WTF/gobject/GMainLoopSource.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/gobject/GUniquePtr.cpp
)

file(GLOB_RECURSE TestWebKitAPI_SOURCES
    *.cpp
    *.h
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/TestWebKitAPI-forwarding-headers.stamp
    DEPENDS ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl
            ${TestWebKitAPI_SOURCES}
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${FORWARDING_HEADERS_DIR} gtk
    COMMAND touch ${CMAKE_BINARY_DIR}/TestWebKitAPI-forwarding-headers.stamp
)
add_custom_target(TestWebKitAPI-forwarding-headers
    DEPENDS WebKit2-forwarding-headers
    DEPENDS ${CMAKE_BINARY_DIR}/TestWebKitAPI-forwarding-headers.stamp
)
