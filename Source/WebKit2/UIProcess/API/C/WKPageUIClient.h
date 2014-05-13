/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKPageUIClient_h
#define WKPageUIClient_h

#include <WebKit/WKBase.h>
#include <WebKit/WKEvent.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKNativeEvent.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKFocusDirectionBackward = 0,
    kWKFocusDirectionForward = 1
};
typedef uint32_t WKFocusDirection;

enum {
    kWKPluginUnavailabilityReasonPluginMissing,
    kWKPluginUnavailabilityReasonPluginCrashed,
    kWKPluginUnavailabilityReasonInsecurePluginVersion
};
typedef uint32_t WKPluginUnavailabilityReason;

typedef void (*WKPageUIClientCallback)(WKPageRef page, const void* clientInfo);
typedef WKPageRef (*WKPageCreateNewPageCallback)(WKPageRef page, WKURLRequestRef urlRequest, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void *clientInfo);
typedef void (*WKPageRunJavaScriptAlertCallback)(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void *clientInfo);
typedef bool (*WKPageRunJavaScriptConfirmCallback)(WKPageRef page, WKStringRef message, WKFrameRef frame, const void *clientInfo);
typedef WKStringRef (*WKPageRunJavaScriptPromptCallback)(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageTakeFocusCallback)(WKPageRef page, WKFocusDirection direction, const void *clientInfo);
typedef void (*WKPageFocusCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageUnfocusCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetStatusTextCallback)(WKPageRef page, WKStringRef text, const void *clientInfo);
typedef void (*WKPageMouseDidMoveOverElementCallback)(WKPageRef page, WKHitTestResultRef hitTestResult, WKEventModifiers modifiers, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidNotHandleKeyEventCallback)(WKPageRef page, WKNativeEventPtr event, const void *clientInfo);
typedef void (*WKPageDidNotHandleWheelEventCallback)(WKPageRef page, WKNativeEventPtr event, const void *clientInfo);
typedef bool (*WKPageGetToolbarsAreVisibleCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetToolbarsAreVisibleCallback)(WKPageRef page, bool toolbarsVisible, const void *clientInfo);
typedef bool (*WKPageGetMenuBarIsVisibleCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetMenuBarIsVisibleCallback)(WKPageRef page, bool menuBarVisible, const void *clientInfo);
typedef bool (*WKPageGetStatusBarIsVisibleCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetStatusBarIsVisibleCallback)(WKPageRef page, bool statusBarVisible, const void *clientInfo);
typedef bool (*WKPageGetIsResizableCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetIsResizableCallback)(WKPageRef page, bool resizable, const void *clientInfo);
typedef WKRect (*WKPageGetWindowFrameCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetWindowFrameCallback)(WKPageRef page, WKRect frame, const void *clientInfo);
typedef bool (*WKPageRunBeforeUnloadConfirmPanelCallback)(WKPageRef page, WKStringRef message, WKFrameRef frame, const void *clientInfo);
typedef unsigned long long (*WKPageExceededDatabaseQuotaCallback)(WKPageRef page, WKFrameRef frame, WKSecurityOriginRef origin, WKStringRef databaseName, WKStringRef displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage, const void *clientInfo);
typedef void (*WKPageRunOpenPanelCallback)(WKPageRef page, WKFrameRef frame, WKOpenPanelParametersRef parameters, WKOpenPanelResultListenerRef listener, const void *clientInfo);
typedef void (*WKPageDecidePolicyForGeolocationPermissionRequestCallback)(WKPageRef page, WKFrameRef frame, WKSecurityOriginRef origin, WKGeolocationPermissionRequestRef permissionRequest, const void* clientInfo);
typedef float (*WKPageHeaderHeightCallback)(WKPageRef page, WKFrameRef frame, const void* clientInfo);
typedef float (*WKPageFooterHeightCallback)(WKPageRef page, WKFrameRef frame, const void* clientInfo);
typedef void (*WKPageDrawHeaderCallback)(WKPageRef page, WKFrameRef frame, WKRect rect, const void* clientInfo);
typedef void (*WKPageDrawFooterCallback)(WKPageRef page, WKFrameRef frame, WKRect rect, const void* clientInfo);
typedef void (*WKPagePrintFrameCallback)(WKPageRef page, WKFrameRef frame, const void* clientInfo);
typedef void (*WKPageSaveDataToFileInDownloadsFolderCallback)(WKPageRef page, WKStringRef suggestedFilename, WKStringRef mimeType, WKURLRef originatingURL, WKDataRef data, const void* clientInfo);
typedef bool (*WKPageShouldInterruptJavaScriptCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageDecidePolicyForNotificationPermissionRequestCallback)(WKPageRef page, WKSecurityOriginRef origin, WKNotificationPermissionRequestRef permissionRequest, const void *clientInfo);
typedef void (*WKPageShowColorPickerCallback)(WKPageRef page, WKStringRef initialColor, WKColorPickerResultListenerRef listener, const void* clientInfo);
typedef void (*WKPageHideColorPickerCallback)(WKPageRef page, const void* clientInfo);
typedef void (*WKPageUnavailablePluginButtonClickedCallback)(WKPageRef page, WKPluginUnavailabilityReason pluginUnavailabilityReason, WKDictionaryRef pluginInfoDictionary, const void* clientInfo);
typedef void (*WKPagePinnedStateDidChangeCallback)(WKPageRef page, const void* clientInfo);

// Deprecated    
typedef WKPageRef (*WKPageCreateNewPageCallback_deprecatedForUseWithV0)(WKPageRef page, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void *clientInfo);
typedef void      (*WKPageMouseDidMoveOverElementCallback_deprecatedForUseWithV0)(WKPageRef page, WKEventModifiers modifiers, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageMissingPluginButtonClickedCallback_deprecatedForUseWithV0)(WKPageRef page, WKStringRef mimeType, WKStringRef url, WKStringRef pluginsPageURL, const void* clientInfo);
typedef void (*WKPageUnavailablePluginButtonClickedCallback_deprecatedForUseWithV1)(WKPageRef page, WKPluginUnavailabilityReason pluginUnavailabilityReason, WKStringRef mimeType, WKStringRef url, WKStringRef pluginsPageURL, const void* clientInfo);

typedef struct WKPageUIClientBase {
    int                                                                 version;
    const void *                                                        clientInfo;
} WKPageUIClientBase;

typedef struct WKPageUIClientV0 {
    WKPageUIClientBase                                                  base;

    // Version 0.
    WKPageCreateNewPageCallback_deprecatedForUseWithV0                  createNewPage_deprecatedForUseWithV0;
    WKPageUIClientCallback                                              showPage;
    WKPageUIClientCallback                                              close;
    WKPageTakeFocusCallback                                             takeFocus;
    WKPageFocusCallback                                                 focus;
    WKPageUnfocusCallback                                               unfocus;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageSetStatusTextCallback                                         setStatusText;
    WKPageMouseDidMoveOverElementCallback_deprecatedForUseWithV0        mouseDidMoveOverElement_deprecatedForUseWithV0;
    WKPageMissingPluginButtonClickedCallback_deprecatedForUseWithV0     missingPluginButtonClicked_deprecatedForUseWithV0;
    WKPageDidNotHandleKeyEventCallback                                  didNotHandleKeyEvent;
    WKPageDidNotHandleWheelEventCallback                                didNotHandleWheelEvent;
    WKPageGetToolbarsAreVisibleCallback                                 toolbarsAreVisible;
    WKPageSetToolbarsAreVisibleCallback                                 setToolbarsAreVisible;
    WKPageGetMenuBarIsVisibleCallback                                   menuBarIsVisible;
    WKPageSetMenuBarIsVisibleCallback                                   setMenuBarIsVisible;
    WKPageGetStatusBarIsVisibleCallback                                 statusBarIsVisible;
    WKPageSetStatusBarIsVisibleCallback                                 setStatusBarIsVisible;
    WKPageGetIsResizableCallback                                        isResizable;
    WKPageSetIsResizableCallback                                        setIsResizable;
    WKPageGetWindowFrameCallback                                        getWindowFrame;
    WKPageSetWindowFrameCallback                                        setWindowFrame;
    WKPageRunBeforeUnloadConfirmPanelCallback                           runBeforeUnloadConfirmPanel;
    WKPageUIClientCallback                                              didDraw;
    WKPageUIClientCallback                                              pageDidScroll;
    WKPageExceededDatabaseQuotaCallback                                 exceededDatabaseQuota;
    WKPageRunOpenPanelCallback                                          runOpenPanel;
    WKPageDecidePolicyForGeolocationPermissionRequestCallback           decidePolicyForGeolocationPermissionRequest;
    WKPageHeaderHeightCallback                                          headerHeight;
    WKPageFooterHeightCallback                                          footerHeight;
    WKPageDrawHeaderCallback                                            drawHeader;
    WKPageDrawFooterCallback                                            drawFooter;
    WKPagePrintFrameCallback                                            printFrame;
    WKPageUIClientCallback                                              runModal;
    void*                                                               unused1; // Used to be didCompleteRubberBandForMainFrame
    WKPageSaveDataToFileInDownloadsFolderCallback                       saveDataToFileInDownloadsFolder;
    WKPageShouldInterruptJavaScriptCallback                             shouldInterruptJavaScript;    
} WKPageUIClientV0;

typedef struct WKPageUIClientV1 {
    WKPageUIClientBase                                                  base;

    // Version 0.
    WKPageCreateNewPageCallback_deprecatedForUseWithV0                  createNewPage_deprecatedForUseWithV0;
    WKPageUIClientCallback                                              showPage;
    WKPageUIClientCallback                                              close;
    WKPageTakeFocusCallback                                             takeFocus;
    WKPageFocusCallback                                                 focus;
    WKPageUnfocusCallback                                               unfocus;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageSetStatusTextCallback                                         setStatusText;
    WKPageMouseDidMoveOverElementCallback_deprecatedForUseWithV0        mouseDidMoveOverElement_deprecatedForUseWithV0;
    WKPageMissingPluginButtonClickedCallback_deprecatedForUseWithV0     missingPluginButtonClicked_deprecatedForUseWithV0;
    WKPageDidNotHandleKeyEventCallback                                  didNotHandleKeyEvent;
    WKPageDidNotHandleWheelEventCallback                                didNotHandleWheelEvent;
    WKPageGetToolbarsAreVisibleCallback                                 toolbarsAreVisible;
    WKPageSetToolbarsAreVisibleCallback                                 setToolbarsAreVisible;
    WKPageGetMenuBarIsVisibleCallback                                   menuBarIsVisible;
    WKPageSetMenuBarIsVisibleCallback                                   setMenuBarIsVisible;
    WKPageGetStatusBarIsVisibleCallback                                 statusBarIsVisible;
    WKPageSetStatusBarIsVisibleCallback                                 setStatusBarIsVisible;
    WKPageGetIsResizableCallback                                        isResizable;
    WKPageSetIsResizableCallback                                        setIsResizable;
    WKPageGetWindowFrameCallback                                        getWindowFrame;
    WKPageSetWindowFrameCallback                                        setWindowFrame;
    WKPageRunBeforeUnloadConfirmPanelCallback                           runBeforeUnloadConfirmPanel;
    WKPageUIClientCallback                                              didDraw;
    WKPageUIClientCallback                                              pageDidScroll;
    WKPageExceededDatabaseQuotaCallback                                 exceededDatabaseQuota;
    WKPageRunOpenPanelCallback                                          runOpenPanel;
    WKPageDecidePolicyForGeolocationPermissionRequestCallback           decidePolicyForGeolocationPermissionRequest;
    WKPageHeaderHeightCallback                                          headerHeight;
    WKPageFooterHeightCallback                                          footerHeight;
    WKPageDrawHeaderCallback                                            drawHeader;
    WKPageDrawFooterCallback                                            drawFooter;
    WKPagePrintFrameCallback                                            printFrame;
    WKPageUIClientCallback                                              runModal;
    void*                                                               unused1; // Used to be didCompleteRubberBandForMainFrame
    WKPageSaveDataToFileInDownloadsFolderCallback                       saveDataToFileInDownloadsFolder;
    WKPageShouldInterruptJavaScriptCallback                             shouldInterruptJavaScript;    

    // Version 1.
    WKPageCreateNewPageCallback                                         createNewPage;
    WKPageMouseDidMoveOverElementCallback                               mouseDidMoveOverElement;
    WKPageDecidePolicyForNotificationPermissionRequestCallback          decidePolicyForNotificationPermissionRequest;
    WKPageUnavailablePluginButtonClickedCallback_deprecatedForUseWithV1 unavailablePluginButtonClicked_deprecatedForUseWithV1;
} WKPageUIClientV1;

typedef struct WKPageUIClientV2 {
    WKPageUIClientBase                                                  base;

    // Version 0.
    WKPageCreateNewPageCallback_deprecatedForUseWithV0                  createNewPage_deprecatedForUseWithV0;
    WKPageUIClientCallback                                              showPage;
    WKPageUIClientCallback                                              close;
    WKPageTakeFocusCallback                                             takeFocus;
    WKPageFocusCallback                                                 focus;
    WKPageUnfocusCallback                                               unfocus;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageSetStatusTextCallback                                         setStatusText;
    WKPageMouseDidMoveOverElementCallback_deprecatedForUseWithV0        mouseDidMoveOverElement_deprecatedForUseWithV0;
    WKPageMissingPluginButtonClickedCallback_deprecatedForUseWithV0     missingPluginButtonClicked_deprecatedForUseWithV0;
    WKPageDidNotHandleKeyEventCallback                                  didNotHandleKeyEvent;
    WKPageDidNotHandleWheelEventCallback                                didNotHandleWheelEvent;
    WKPageGetToolbarsAreVisibleCallback                                 toolbarsAreVisible;
    WKPageSetToolbarsAreVisibleCallback                                 setToolbarsAreVisible;
    WKPageGetMenuBarIsVisibleCallback                                   menuBarIsVisible;
    WKPageSetMenuBarIsVisibleCallback                                   setMenuBarIsVisible;
    WKPageGetStatusBarIsVisibleCallback                                 statusBarIsVisible;
    WKPageSetStatusBarIsVisibleCallback                                 setStatusBarIsVisible;
    WKPageGetIsResizableCallback                                        isResizable;
    WKPageSetIsResizableCallback                                        setIsResizable;
    WKPageGetWindowFrameCallback                                        getWindowFrame;
    WKPageSetWindowFrameCallback                                        setWindowFrame;
    WKPageRunBeforeUnloadConfirmPanelCallback                           runBeforeUnloadConfirmPanel;
    WKPageUIClientCallback                                              didDraw;
    WKPageUIClientCallback                                              pageDidScroll;
    WKPageExceededDatabaseQuotaCallback                                 exceededDatabaseQuota;
    WKPageRunOpenPanelCallback                                          runOpenPanel;
    WKPageDecidePolicyForGeolocationPermissionRequestCallback           decidePolicyForGeolocationPermissionRequest;
    WKPageHeaderHeightCallback                                          headerHeight;
    WKPageFooterHeightCallback                                          footerHeight;
    WKPageDrawHeaderCallback                                            drawHeader;
    WKPageDrawFooterCallback                                            drawFooter;
    WKPagePrintFrameCallback                                            printFrame;
    WKPageUIClientCallback                                              runModal;
    void*                                                               unused1; // Used to be didCompleteRubberBandForMainFrame
    WKPageSaveDataToFileInDownloadsFolderCallback                       saveDataToFileInDownloadsFolder;
    WKPageShouldInterruptJavaScriptCallback                             shouldInterruptJavaScript;    

    // Version 1.
    WKPageCreateNewPageCallback                                         createNewPage;
    WKPageMouseDidMoveOverElementCallback                               mouseDidMoveOverElement;
    WKPageDecidePolicyForNotificationPermissionRequestCallback          decidePolicyForNotificationPermissionRequest;
    WKPageUnavailablePluginButtonClickedCallback_deprecatedForUseWithV1 unavailablePluginButtonClicked_deprecatedForUseWithV1;

    // Version 2.
    WKPageShowColorPickerCallback                                       showColorPicker;
    WKPageHideColorPickerCallback                                       hideColorPicker;
    WKPageUnavailablePluginButtonClickedCallback                        unavailablePluginButtonClicked;
} WKPageUIClientV2;

typedef struct WKPageUIClientV3 {
    WKPageUIClientBase                                                  base;

    // Version 0.
    WKPageCreateNewPageCallback_deprecatedForUseWithV0                  createNewPage_deprecatedForUseWithV0;
    WKPageUIClientCallback                                              showPage;
    WKPageUIClientCallback                                              close;
    WKPageTakeFocusCallback                                             takeFocus;
    WKPageFocusCallback                                                 focus;
    WKPageUnfocusCallback                                               unfocus;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageSetStatusTextCallback                                         setStatusText;
    WKPageMouseDidMoveOverElementCallback_deprecatedForUseWithV0        mouseDidMoveOverElement_deprecatedForUseWithV0;
    WKPageMissingPluginButtonClickedCallback_deprecatedForUseWithV0     missingPluginButtonClicked_deprecatedForUseWithV0;
    WKPageDidNotHandleKeyEventCallback                                  didNotHandleKeyEvent;
    WKPageDidNotHandleWheelEventCallback                                didNotHandleWheelEvent;
    WKPageGetToolbarsAreVisibleCallback                                 toolbarsAreVisible;
    WKPageSetToolbarsAreVisibleCallback                                 setToolbarsAreVisible;
    WKPageGetMenuBarIsVisibleCallback                                   menuBarIsVisible;
    WKPageSetMenuBarIsVisibleCallback                                   setMenuBarIsVisible;
    WKPageGetStatusBarIsVisibleCallback                                 statusBarIsVisible;
    WKPageSetStatusBarIsVisibleCallback                                 setStatusBarIsVisible;
    WKPageGetIsResizableCallback                                        isResizable;
    WKPageSetIsResizableCallback                                        setIsResizable;
    WKPageGetWindowFrameCallback                                        getWindowFrame;
    WKPageSetWindowFrameCallback                                        setWindowFrame;
    WKPageRunBeforeUnloadConfirmPanelCallback                           runBeforeUnloadConfirmPanel;
    WKPageUIClientCallback                                              didDraw;
    WKPageUIClientCallback                                              pageDidScroll;
    WKPageExceededDatabaseQuotaCallback                                 exceededDatabaseQuota;
    WKPageRunOpenPanelCallback                                          runOpenPanel;
    WKPageDecidePolicyForGeolocationPermissionRequestCallback           decidePolicyForGeolocationPermissionRequest;
    WKPageHeaderHeightCallback                                          headerHeight;
    WKPageFooterHeightCallback                                          footerHeight;
    WKPageDrawHeaderCallback                                            drawHeader;
    WKPageDrawFooterCallback                                            drawFooter;
    WKPagePrintFrameCallback                                            printFrame;
    WKPageUIClientCallback                                              runModal;
    void*                                                               unused1; // Used to be didCompleteRubberBandForMainFrame
    WKPageSaveDataToFileInDownloadsFolderCallback                       saveDataToFileInDownloadsFolder;
    WKPageShouldInterruptJavaScriptCallback                             shouldInterruptJavaScript;    

    // Version 1.
    WKPageCreateNewPageCallback                                         createNewPage;
    WKPageMouseDidMoveOverElementCallback                               mouseDidMoveOverElement;
    WKPageDecidePolicyForNotificationPermissionRequestCallback          decidePolicyForNotificationPermissionRequest;
    WKPageUnavailablePluginButtonClickedCallback_deprecatedForUseWithV1 unavailablePluginButtonClicked_deprecatedForUseWithV1;

    // Version 2.
    WKPageShowColorPickerCallback                                       showColorPicker;
    WKPageHideColorPickerCallback                                       hideColorPicker;
    WKPageUnavailablePluginButtonClickedCallback                        unavailablePluginButtonClicked;

    // Version 3.
    WKPagePinnedStateDidChangeCallback                                  pinnedStateDidChange;
} WKPageUIClientV3;

enum { kWKPageUIClientCurrentVersion WK_ENUM_DEPRECATED("Use an explicit version number instead") = 2 };
typedef struct WKPageUIClient {
    int                                                                 version;
    const void *                                                        clientInfo;

    // Version 0.
    WKPageCreateNewPageCallback_deprecatedForUseWithV0                  createNewPage_deprecatedForUseWithV0;
    WKPageUIClientCallback                                              showPage;
    WKPageUIClientCallback                                              close;
    WKPageTakeFocusCallback                                             takeFocus;
    WKPageFocusCallback                                                 focus;
    WKPageUnfocusCallback                                               unfocus;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageSetStatusTextCallback                                         setStatusText;
    WKPageMouseDidMoveOverElementCallback_deprecatedForUseWithV0        mouseDidMoveOverElement_deprecatedForUseWithV0;
    WKPageMissingPluginButtonClickedCallback_deprecatedForUseWithV0     missingPluginButtonClicked_deprecatedForUseWithV0;
    WKPageDidNotHandleKeyEventCallback                                  didNotHandleKeyEvent;
    WKPageDidNotHandleWheelEventCallback                                didNotHandleWheelEvent;
    WKPageGetToolbarsAreVisibleCallback                                 toolbarsAreVisible;
    WKPageSetToolbarsAreVisibleCallback                                 setToolbarsAreVisible;
    WKPageGetMenuBarIsVisibleCallback                                   menuBarIsVisible;
    WKPageSetMenuBarIsVisibleCallback                                   setMenuBarIsVisible;
    WKPageGetStatusBarIsVisibleCallback                                 statusBarIsVisible;
    WKPageSetStatusBarIsVisibleCallback                                 setStatusBarIsVisible;
    WKPageGetIsResizableCallback                                        isResizable;
    WKPageSetIsResizableCallback                                        setIsResizable;
    WKPageGetWindowFrameCallback                                        getWindowFrame;
    WKPageSetWindowFrameCallback                                        setWindowFrame;
    WKPageRunBeforeUnloadConfirmPanelCallback                           runBeforeUnloadConfirmPanel;
    WKPageUIClientCallback                                              didDraw;
    WKPageUIClientCallback                                              pageDidScroll;
    WKPageExceededDatabaseQuotaCallback                                 exceededDatabaseQuota;
    WKPageRunOpenPanelCallback                                          runOpenPanel;
    WKPageDecidePolicyForGeolocationPermissionRequestCallback           decidePolicyForGeolocationPermissionRequest;
    WKPageHeaderHeightCallback                                          headerHeight;
    WKPageFooterHeightCallback                                          footerHeight;
    WKPageDrawHeaderCallback                                            drawHeader;
    WKPageDrawFooterCallback                                            drawFooter;
    WKPagePrintFrameCallback                                            printFrame;
    WKPageUIClientCallback                                              runModal;
    void*                                                               unused1; // Used to be didCompleteRubberBandForMainFrame
    WKPageSaveDataToFileInDownloadsFolderCallback                       saveDataToFileInDownloadsFolder;
    WKPageShouldInterruptJavaScriptCallback                             shouldInterruptJavaScript;    

    // Version 1.
    WKPageCreateNewPageCallback                                         createNewPage;
    WKPageMouseDidMoveOverElementCallback                               mouseDidMoveOverElement;
    WKPageDecidePolicyForNotificationPermissionRequestCallback          decidePolicyForNotificationPermissionRequest;
    WKPageUnavailablePluginButtonClickedCallback_deprecatedForUseWithV1 unavailablePluginButtonClicked_deprecatedForUseWithV1;

    // Version 2.
    WKPageShowColorPickerCallback                                       showColorPicker;
    WKPageHideColorPickerCallback                                       hideColorPicker;
    WKPageUnavailablePluginButtonClickedCallback                        unavailablePluginButtonClicked;
} WKPageUIClient WK_DEPRECATED("Use an explicit versioned struct instead");

#ifdef __cplusplus
}
#endif

#endif // WKPageUIClient_h
