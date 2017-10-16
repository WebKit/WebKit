set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

# This is necessary because it is possible to build TestWebKitAPI with WebKit
# disabled and this triggers the inclusion of the WebKit headers.
add_definitions(-DBUILDING_WEBKIT2__)

add_custom_target(TestWebKitAPI-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
    DEPENDS WebKit2-forwarding-headers
)

set(ForwardingHeadersForTestWebKitAPI_NAME TestWebKitAPI-forwarding-headers)

include_directories(
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WEBKIT2_DIR}/UIProcess/API/C/gtk
    ${WEBKIT2_DIR}/UIProcess/API/gtk
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
ADD_WHOLE_ARCHIVE_TO_LIBRARIES(test_webcore_LIBRARIES)

list(APPEND TestWebKitAPI_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND TestJavaScriptCore_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

add_executable(TestWebKit2
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/AboutBlankLoad.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/CanHandleRequest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/CookieManager.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/DocumentStartUserScriptAlertCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/DOMWindowExtensionBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/DOMWindowExtensionNoCache.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/DownloadDecideDestinationCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/EnumerateMediaDevices.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/EvaluateJavaScript.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/FailedLoad.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/Find.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/ForceRepaint.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/FrameMIMETypeHTML.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/FrameMIMETypePNG.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/Geolocation.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/GetInjectedBundleInitializationUserDataCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/HitTestResultNodeHandle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleFrameHitTest.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleInitializationUserDataCallbackWins.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadAlternateHTMLStringWithNonDirectoryURL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadCanceledNoServerRedirectCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadPageOnCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/MouseMoveAfterCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayout.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFails.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutForImages.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFrames.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/PageLoadBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/PageLoadDidChangeLocationWithinPageForFrame.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/ParentFrame.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/PendingAPIRequestURL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/PreventEmptyUserAgent.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/PrivateBrowsingPushStateNoHistoryCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/ProvisionalURLAfterWillSendRequestCallback.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/ReloadPageAfterCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/ResizeWindowAfterCrash.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/RestoreSessionStateContainingFormData.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/ShouldGoToBackForwardListItem.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/TextFieldDidBeginAndEndEditing.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/UserMedia.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/UserMessage.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WillSendSubmitEvent.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKPageCopySessionStateWithFiltering.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKPageGetScaleFactorNotZero.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKPreferences.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKRetainPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKStringJSString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKURL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/gtk/InputMethodFilter.cpp
)

target_link_libraries(TestWebKit2 ${test_webkit2_api_LIBRARIES})
add_test(TestWebKit2 ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit2/TestWebKit2)
set_tests_properties(TestWebKit2 PROPERTIES TIMEOUT 60)
set_target_properties(TestWebKit2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit2)

add_executable(TestWebCore
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/CSSParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/ComplexTextController.cpp
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
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKit2 -Wno-sign-compare
                                            -Wno-undef
                                            -Wno-unused-parameter)

    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebCore -Wno-sign-compare
                                            -Wno-undef
                                            -Wno-unused-parameter)
endif ()
