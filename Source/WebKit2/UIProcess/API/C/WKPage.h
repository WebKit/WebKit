/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef WKPage_h
#define WKPage_h

#include <WebKit2/WKBase.h>
#include <WebKit2/WKEvent.h>
#include <WebKit2/WKFindOptions.h>
#include <WebKit2/WKGeometry.h>
#include <WebKit2/WKNativeEvent.h>
#include <WebKit2/WKPageLoadTypes.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKFocusDirectionBackward = 0,
    kWKFocusDirectionForward = 1
};
typedef uint32_t WKFocusDirection;

typedef void (*WKPageCallback)(WKPageRef page, const void* clientInfo);

// FrameLoad Client
typedef void (*WKPageDidStartProvisionalLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidReceiveServerRedirectForProvisionalLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidFailProvisionalLoadWithErrorForFrameCallback)(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidCommitLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidFinishDocumentLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidFinishLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidFailLoadWithErrorForFrameCallback)(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidSameDocumentNavigationForFrameCallback)(WKPageRef page, WKFrameRef frame, WKSameDocumentNavigationType type, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidReceiveTitleForFrameCallback)(WKPageRef page, WKStringRef title, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidFirstLayoutForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidFirstVisuallyNonEmptyLayoutForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidRemoveFrameFromHierarchyCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidDisplayInsecureContentForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageDidRunInsecureContentForFrameCallback)(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void *clientInfo);
typedef bool (*WKPageCanAuthenticateAgainstProtectionSpaceInFrameCallback)(WKPageRef page, WKFrameRef frame, WKProtectionSpaceRef protectionSpace, const void *clientInfo);
typedef void (*WKPageDidReceiveAuthenticationChallengeInFrameCallback)(WKPageRef page, WKFrameRef frame, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo);
typedef void (*WKPageDidChangeBackForwardListCallback)(WKPageRef page, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void *clientInfo);
typedef bool (*WKPageShouldGoToBackForwardListItemCallback)(WKPageRef page, WKBackForwardListItemRef item, const void *clientInfo);
typedef void (*WKPageDidFailToInitializePluginCallback)(WKPageRef page, WKStringRef mimeType, const void* clientInfo);

struct WKPageLoaderClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageDidStartProvisionalLoadForFrameCallback                       didStartProvisionalLoadForFrame;
    WKPageDidReceiveServerRedirectForProvisionalLoadForFrameCallback    didReceiveServerRedirectForProvisionalLoadForFrame;
    WKPageDidFailProvisionalLoadWithErrorForFrameCallback               didFailProvisionalLoadWithErrorForFrame;
    WKPageDidCommitLoadForFrameCallback                                 didCommitLoadForFrame;
    WKPageDidFinishDocumentLoadForFrameCallback                         didFinishDocumentLoadForFrame;
    WKPageDidFinishLoadForFrameCallback                                 didFinishLoadForFrame;
    WKPageDidFailLoadWithErrorForFrameCallback                          didFailLoadWithErrorForFrame;
    WKPageDidSameDocumentNavigationForFrameCallback                     didSameDocumentNavigationForFrame;
    WKPageDidReceiveTitleForFrameCallback                               didReceiveTitleForFrame;
    WKPageDidFirstLayoutForFrameCallback                                didFirstLayoutForFrame;
    WKPageDidFirstVisuallyNonEmptyLayoutForFrameCallback                didFirstVisuallyNonEmptyLayoutForFrame;
    WKPageDidRemoveFrameFromHierarchyCallback                           didRemoveFrameFromHierarchy;
    WKPageDidDisplayInsecureContentForFrameCallback                     didDisplayInsecureContentForFrame;
    WKPageDidRunInsecureContentForFrameCallback                         didRunInsecureContentForFrame;
    WKPageCanAuthenticateAgainstProtectionSpaceInFrameCallback          canAuthenticateAgainstProtectionSpaceInFrame;
    WKPageDidReceiveAuthenticationChallengeInFrameCallback              didReceiveAuthenticationChallengeInFrame;

    // FIXME: Move to progress client.
    WKPageCallback                                                      didStartProgress;
    WKPageCallback                                                      didChangeProgress;
    WKPageCallback                                                      didFinishProgress;

    // FIXME: These three functions should not be part of this client.
    WKPageCallback                                                      processDidBecomeUnresponsive;
    WKPageCallback                                                      processDidBecomeResponsive;
    WKPageCallback                                                      processDidCrash;
    WKPageDidChangeBackForwardListCallback                              didChangeBackForwardList;
    WKPageShouldGoToBackForwardListItemCallback                         shouldGoToBackForwardListItem;
    WKPageDidFailToInitializePluginCallback                             didFailToInitializePlugin;
};
typedef struct WKPageLoaderClient WKPageLoaderClient;

// Policy Client.
typedef void (*WKPageDecidePolicyForNavigationActionCallback)(WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo);
typedef void (*WKPageDecidePolicyForNewWindowActionCallback)(WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo);
typedef void (*WKPageDecidePolicyForResponseCallback)(WKPageRef page, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo);
typedef void (*WKPageUnableToImplementPolicyCallback)(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo);

struct WKPagePolicyClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageDecidePolicyForNavigationActionCallback                       decidePolicyForNavigationAction;
    WKPageDecidePolicyForNewWindowActionCallback                        decidePolicyForNewWindowAction;
    WKPageDecidePolicyForResponseCallback                               decidePolicyForResponse;
    WKPageUnableToImplementPolicyCallback                               unableToImplementPolicy;
};
typedef struct WKPagePolicyClient WKPagePolicyClient;

// Form Client.
typedef void (*WKPageWillSubmitFormCallback)(WKPageRef page, WKFrameRef frame, WKFrameRef sourceFrame, WKDictionaryRef values, WKTypeRef userData, WKFormSubmissionListenerRef listener, const void* clientInfo);

struct WKPageFormClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageWillSubmitFormCallback                                        willSubmitForm;
};
typedef struct WKPageFormClient WKPageFormClient;

// Resource Load Client.
typedef void (*WKPageDidInitiateLoadForResourceCallback)(WKPageRef page, WKFrameRef frame, uint64_t resourceIdentifier, WKURLRequestRef request, bool pageIsProvisionallyLoading, const void* clientInfo);
typedef void (*WKPageDidSendRequestForResourceCallback)(WKPageRef page, WKFrameRef frame, uint64_t resourceIdentifier, WKURLRequestRef request, WKURLResponseRef redirectResponse, const void* clientInfo);
typedef void (*WKPageDidReceiveResponseForResourceCallback)(WKPageRef page, WKFrameRef frame, uint64_t resourceIdentifier, WKURLResponseRef response, const void* clientInfo);
typedef void (*WKPageDidReceiveContentLengthForResourceCallback)(WKPageRef page, WKFrameRef frame, uint64_t resourceIdentifier, uint64_t contentLength, const void* clientInfo);
typedef void (*WKPageDidFinishLoadForResourceCallback)(WKPageRef page, WKFrameRef frame, uint64_t resourceIdentifier, const void* clientInfo);
typedef void (*WKPageDidFailLoadForResourceCallback)(WKPageRef page, WKFrameRef frame, uint64_t resourceIdentifier, WKErrorRef error, const void* clientInfo);

struct WKPageResourceLoadClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageDidInitiateLoadForResourceCallback                            didInitiateLoadForResource;
    WKPageDidSendRequestForResourceCallback                             didSendRequestForResource;
    WKPageDidReceiveResponseForResourceCallback                         didReceiveResponseForResource;
    WKPageDidReceiveContentLengthForResourceCallback                    didReceiveContentLengthForResource;
    WKPageDidFinishLoadForResourceCallback                              didFinishLoadForResource;
    WKPageDidFailLoadForResourceCallback                                didFailLoadForResource;
};
typedef struct WKPageResourceLoadClient WKPageResourceLoadClient;

// UI Client
typedef WKPageRef (*WKPageCreateNewPageCallback)(WKPageRef page, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void *clientInfo);
typedef void (*WKPageRunJavaScriptAlertCallback)(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void *clientInfo);
typedef bool (*WKPageRunJavaScriptConfirmCallback)(WKPageRef page, WKStringRef message, WKFrameRef frame, const void *clientInfo);
typedef WKStringRef (*WKPageRunJavaScriptPromptCallback)(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageTakeFocusCallback)(WKPageRef page, WKFocusDirection direction, const void *clientInfo);
typedef void (*WKPageFocusCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageUnfocusCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageSetStatusTextCallback)(WKPageRef page, WKStringRef text, const void *clientInfo);
typedef void (*WKPageMouseDidMoveOverElementCallback)(WKPageRef page, WKEventModifiers modifiers, WKTypeRef userData, const void *clientInfo);
typedef void (*WKPageMissingPluginButtonClickedCallback)(WKPageRef page, WKStringRef mimeType, WKStringRef url, WKStringRef pluginsPageURL, const void* clientInfo);
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
typedef void (*WKPageDidCompleteRubberBandForMainFrameCallback)(WKPageRef page, WKSize initialOverhang, const void* clientInfo);
typedef void (*WKPageSaveDataToFileInDownloadsFolderCallback)(WKPageRef page, WKStringRef suggestedFilename, WKStringRef mimeType, WKURLRef originatingURL, WKDataRef data, const void* clientInfo);
typedef bool (*WKPageShouldInterruptJavaScriptCallback)(WKPageRef page, const void *clientInfo);

struct WKPageUIClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageCreateNewPageCallback                                         createNewPage;
    WKPageCallback                                                      showPage;
    WKPageCallback                                                      close;
    WKPageTakeFocusCallback                                             takeFocus;
    WKPageFocusCallback                                                 focus;
    WKPageUnfocusCallback                                               unfocus;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageSetStatusTextCallback                                         setStatusText;
    WKPageMouseDidMoveOverElementCallback                               mouseDidMoveOverElement;
    WKPageMissingPluginButtonClickedCallback                            missingPluginButtonClicked;
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
    WKPageCallback                                                      didDraw;
    WKPageCallback                                                      pageDidScroll;
    WKPageExceededDatabaseQuotaCallback                                 exceededDatabaseQuota;
    WKPageRunOpenPanelCallback                                          runOpenPanel;
    WKPageDecidePolicyForGeolocationPermissionRequestCallback           decidePolicyForGeolocationPermissionRequest;
    WKPageHeaderHeightCallback                                          headerHeight;
    WKPageFooterHeightCallback                                          footerHeight;
    WKPageDrawHeaderCallback                                            drawHeader;
    WKPageDrawFooterCallback                                            drawFooter;
    WKPagePrintFrameCallback                                            printFrame;
    WKPageCallback                                                      runModal;
    WKPageDidCompleteRubberBandForMainFrameCallback                     didCompleteRubberBandForMainFrame;
    WKPageSaveDataToFileInDownloadsFolderCallback                       saveDataToFileInDownloadsFolder;
    WKPageShouldInterruptJavaScriptCallback                             shouldInterruptJavaScript;
};
typedef struct WKPageUIClient WKPageUIClient;

// Find client.
typedef void (*WKPageDidFindStringCallback)(WKPageRef page, WKStringRef string, unsigned matchCount, const void* clientInfo);
typedef void (*WKPageDidFailToFindStringCallback)(WKPageRef page, WKStringRef string, const void* clientInfo);
typedef void (*WKPageDidCountStringMatchesCallback)(WKPageRef page, WKStringRef string, unsigned matchCount, const void* clientInfo);

struct WKPageFindClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageDidFindStringCallback                                         didFindString;
    WKPageDidFailToFindStringCallback                                   didFailToFindString;
    WKPageDidCountStringMatchesCallback                                 didCountStringMatches;
};
typedef struct WKPageFindClient WKPageFindClient;

enum {
    kWKMoreThanMaximumMatchCount = -1
};

// ContextMenu client
typedef void (*WKPageGetContextMenuFromProposedContextMenuCallback)(WKPageRef page, WKArrayRef proposedMenu, WKArrayRef* newMenu, WKTypeRef userData, const void* clientInfo);
typedef void (*WKPageCustomContextMenuItemSelectedCallback)(WKPageRef page, WKContextMenuItemRef contextMenuItem, const void* clientInfo);

struct WKPageContextMenuClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageGetContextMenuFromProposedContextMenuCallback                 getContextMenuFromProposedMenu;
    WKPageCustomContextMenuItemSelectedCallback                         customContextMenuItemSelected;
};
typedef struct WKPageContextMenuClient WKPageContextMenuClient;

WK_EXPORT WKTypeID WKPageGetTypeID();

WK_EXPORT WKContextRef WKPageGetContext(WKPageRef page);
WK_EXPORT WKPageGroupRef WKPageGetPageGroup(WKPageRef page);

WK_EXPORT void WKPageLoadURL(WKPageRef page, WKURLRef url);
WK_EXPORT void WKPageLoadURLRequest(WKPageRef page, WKURLRequestRef urlRequest);
WK_EXPORT void WKPageLoadHTMLString(WKPageRef page, WKStringRef htmlString, WKURLRef baseURL);
WK_EXPORT void WKPageLoadAlternateHTMLString(WKPageRef page, WKStringRef htmlString, WKURLRef baseURL, WKURLRef unreachableURL);
WK_EXPORT void WKPageLoadPlainTextString(WKPageRef page, WKStringRef plainTextString);

WK_EXPORT void WKPageStopLoading(WKPageRef page);
WK_EXPORT void WKPageReload(WKPageRef page);
WK_EXPORT void WKPageReloadFromOrigin(WKPageRef page);

WK_EXPORT bool WKPageTryClose(WKPageRef page);
WK_EXPORT void WKPageClose(WKPageRef page);
WK_EXPORT bool WKPageIsClosed(WKPageRef page);

WK_EXPORT void WKPageGoForward(WKPageRef page);
WK_EXPORT bool WKPageCanGoForward(WKPageRef page);
WK_EXPORT void WKPageGoBack(WKPageRef page);
WK_EXPORT bool WKPageCanGoBack(WKPageRef page);
WK_EXPORT void WKPageGoToBackForwardListItem(WKPageRef page, WKBackForwardListItemRef item);
WK_EXPORT void WKPageTryRestoreScrollPosition(WKPageRef page);
WK_EXPORT WKBackForwardListRef WKPageGetBackForwardList(WKPageRef page);
WK_EXPORT bool WKPageWillHandleHorizontalScrollEvents(WKPageRef page);
    
WK_EXPORT WKStringRef WKPageCopyTitle(WKPageRef page);

WK_EXPORT WKURLRef WKPageCopyPendingAPIRequestURL(WKPageRef page);

WK_EXPORT WKFrameRef WKPageGetMainFrame(WKPageRef page);
WK_EXPORT WKFrameRef WKPageGetFocusedFrame(WKPageRef page); // The focused frame may be inactive.
WK_EXPORT WKFrameRef WKPageGetFrameSetLargestFrame(WKPageRef page);
WK_EXPORT double WKPageGetEstimatedProgress(WKPageRef page);

WK_EXPORT uint64_t WKPageGetRenderTreeSize(WKPageRef page);

WK_EXPORT void WKPageSetMemoryCacheClientCallsEnabled(WKPageRef page, bool memoryCacheClientCallsEnabled);

#if defined(ENABLE_INSPECTOR) && ENABLE_INSPECTOR
WK_EXPORT WKInspectorRef WKPageGetInspector(WKPageRef page);
#endif

WK_EXPORT WKStringRef WKPageCopyUserAgent(WKPageRef page);

WK_EXPORT WKStringRef WKPageCopyApplicationNameForUserAgent(WKPageRef page);
WK_EXPORT void WKPageSetApplicationNameForUserAgent(WKPageRef page, WKStringRef applicationName);

WK_EXPORT WKStringRef WKPageCopyCustomUserAgent(WKPageRef page);
WK_EXPORT void WKPageSetCustomUserAgent(WKPageRef page, WKStringRef userAgent);

WK_EXPORT bool WKPageSupportsTextEncoding(WKPageRef page);
WK_EXPORT WKStringRef WKPageCopyCustomTextEncodingName(WKPageRef page);
WK_EXPORT void WKPageSetCustomTextEncodingName(WKPageRef page, WKStringRef encodingName);

WK_EXPORT void WKPageTerminate(WKPageRef page);

WK_EXPORT WKStringRef WKPageGetSessionHistoryURLValueType(void);

typedef bool (*WKPageSessionStateFilterCallback)(WKPageRef page, WKStringRef valueType, WKTypeRef value, void* context);
WK_EXPORT WKDataRef WKPageCopySessionState(WKPageRef page, void* context, WKPageSessionStateFilterCallback urlAllowedCallback);
WK_EXPORT void WKPageRestoreFromSessionState(WKPageRef page, WKDataRef sessionStateData);

WK_EXPORT double WKPageGetBackingScaleFactor(WKPageRef page);
WK_EXPORT void WKPageSetCustomBackingScaleFactor(WKPageRef page, double customScaleFactor);

WK_EXPORT bool WKPageSupportsTextZoom(WKPageRef page);
WK_EXPORT double WKPageGetTextZoomFactor(WKPageRef page);
WK_EXPORT void WKPageSetTextZoomFactor(WKPageRef page, double zoomFactor);
WK_EXPORT double WKPageGetPageZoomFactor(WKPageRef page);
WK_EXPORT void WKPageSetPageZoomFactor(WKPageRef page, double zoomFactor);
WK_EXPORT void WKPageSetPageAndTextZoomFactors(WKPageRef page, double pageZoomFactor, double textZoomFactor);

WK_EXPORT void WKPageSetScaleFactor(WKPageRef page, double scale, WKPoint origin);
WK_EXPORT double WKPageGetScaleFactor(WKPageRef page);

WK_EXPORT void WKPageSetUseFixedLayout(WKPageRef page, bool fixed);
WK_EXPORT void WKPageSetFixedLayoutSize(WKPageRef page, WKSize size);
WK_EXPORT bool WKPageUseFixedLayout(WKPageRef page);
WK_EXPORT WKSize WKPageFixedLayoutSize(WKPageRef page);

WK_EXPORT bool WKPageHasHorizontalScrollbar(WKPageRef page);
WK_EXPORT bool WKPageHasVerticalScrollbar(WKPageRef page);

WK_EXPORT bool WKPageIsPinnedToLeftSide(WKPageRef page);
WK_EXPORT bool WKPageIsPinnedToRightSide(WKPageRef page);

WK_EXPORT bool WKPageCanDelete(WKPageRef page);
WK_EXPORT bool WKPageHasSelectedRange(WKPageRef page);
WK_EXPORT bool WKPageIsContentEditable(WKPageRef page);

WK_EXPORT void WKPageSetMaintainsInactiveSelection(WKPageRef page, bool maintainsInactiveSelection);
    
WK_EXPORT void WKPageFindString(WKPageRef page, WKStringRef string, WKFindOptions findOptions, unsigned maxMatchCount);
WK_EXPORT void WKPageHideFindUI(WKPageRef page);
WK_EXPORT void WKPageCountStringMatches(WKPageRef page, WKStringRef string, WKFindOptions findOptions, unsigned maxMatchCount);

WK_EXPORT void WKPageSetPageContextMenuClient(WKPageRef page, const WKPageContextMenuClient* client);
WK_EXPORT void WKPageSetPageFindClient(WKPageRef page, const WKPageFindClient* client);
WK_EXPORT void WKPageSetPageFormClient(WKPageRef page, const WKPageFormClient* client);
WK_EXPORT void WKPageSetPageLoaderClient(WKPageRef page, const WKPageLoaderClient* client);
WK_EXPORT void WKPageSetPagePolicyClient(WKPageRef page, const WKPagePolicyClient* client);
WK_EXPORT void WKPageSetPageResourceLoadClient(WKPageRef page, const WKPageResourceLoadClient* client);
WK_EXPORT void WKPageSetPageUIClient(WKPageRef page, const WKPageUIClient* client);

typedef void (*WKPageRunJavaScriptFunction)(WKSerializedScriptValueRef, WKErrorRef, void*);
WK_EXPORT void WKPageRunJavaScriptInMainFrame(WKPageRef page, WKStringRef script, void* context, WKPageRunJavaScriptFunction function);
#ifdef __BLOCKS__
typedef void (^WKPageRunJavaScriptBlock)(WKSerializedScriptValueRef, WKErrorRef);
WK_EXPORT void WKPageRunJavaScriptInMainFrame_b(WKPageRef page, WKStringRef script, WKPageRunJavaScriptBlock block);
#endif

typedef void (*WKPageGetSourceForFrameFunction)(WKStringRef, WKErrorRef, void*);
WK_EXPORT void WKPageGetSourceForFrame(WKPageRef page, WKFrameRef frame, void* context, WKPageGetSourceForFrameFunction function);
#ifdef __BLOCKS__
typedef void (^WKPageGetSourceForFrameBlock)(WKStringRef, WKErrorRef);
WK_EXPORT void WKPageGetSourceForFrame_b(WKPageRef page, WKFrameRef frame, WKPageGetSourceForFrameBlock block);
#endif

typedef void (*WKPageGetContentsAsStringFunction)(WKStringRef, WKErrorRef, void*);
WK_EXPORT void WKPageGetContentsAsString(WKPageRef page, void* context, WKPageGetContentsAsStringFunction function);
#ifdef __BLOCKS__
typedef void (^WKPageGetContentsAsStringBlock)(WKStringRef, WKErrorRef);
WK_EXPORT void WKPageGetContentsAsString_b(WKPageRef page, WKPageGetContentsAsStringBlock block);
#endif

typedef void (*WKPageForceRepaintFunction)(WKErrorRef, void*);
WK_EXPORT void WKPageForceRepaint(WKPageRef page, void* context, WKPageForceRepaintFunction function);

/*
    Some of the more common command name strings include the following, although any WebCore EditorCommand string is supported:
    
    "Cut"
    "Copy"
    "Paste"
    "SelectAll"
    "Undo"
    "Redo"
*/

// state represents the state of the command in a menu (on is 1, off is 0, and mixed is -1), typically used to add a checkmark next to the menu item.
typedef void (*WKPageValidateCommandCallback)(WKStringRef command, bool isEnabled, int32_t state, WKErrorRef, void* context);
WK_EXPORT void WKPageValidateCommand(WKPageRef page, WKStringRef command, void* context, WKPageValidateCommandCallback callback);
WK_EXPORT void WKPageExecuteCommand(WKPageRef page, WKStringRef command);

#ifdef __cplusplus
}
#endif

#endif /* WKPage_h */
