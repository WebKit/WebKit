/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKFrameNavigationTypeLinkClicked = 0,
    kWKFrameNavigationTypeFormSubmitted = 1,
    kWKFrameNavigationTypeBackForward = 2,
    kWKFrameNavigationTypeReload = 3,
    kWKFrameNavigationTypeFormResubmitted = 4,
    kWKFrameNavigationTypeOther = 5
};
typedef uint32_t WKFrameNavigationType;

enum {
    kWKEventModifiersShiftKey = 1 << 0,
    kWKEventModifiersControlKey = 1 << 1,
    kWKEventModifiersAltKey = 1 << 2,
    kWKEventModifiersMetaKey = 1 << 3
};
typedef uint32_t WKEventModifiers;

// FrameLoad Client
typedef void (*WKPageDidStartProvisionalLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidReceiveServerRedirectForProvisionalLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidFailProvisionalLoadWithErrorForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo); // FIXME: Add WKErrorRef.
typedef void (*WKPageDidCommitLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidFinishDocumentLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidFinishLoadForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidFailLoadWithErrorForFrameCallback)(WKPageRef page, WKFrameRef frame, const void *clientInfo); // FIXME: Add WKErrorRef.
typedef void (*WKPageDidReceiveTitleForFrameCallback)(WKPageRef page, WKStringRef title, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidFirstLayoutForFrame)(WKPageRef page, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageDidFirstVisuallyNonEmptyLayoutForFrame)(WKPageRef page, WKFrameRef frame, const void *clientInfo);

// Progress Client
typedef void (*WKPageDidStartProgressCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageDidChangeProgressCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageDidFinishProgressCallback)(WKPageRef page, const void *clientInfo);

// WebPageNamespace Client
// FIXME: These three functions should not be part of this client.
typedef void (*WKPageDidBecomeUnresponsiveCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageDidBecomeResponsiveCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageProcessDidExitCallback)(WKPageRef page, const void *clientInfo);

typedef void (*WKPageDidChangeBackForwardListCallback)(WKPageRef page, const void *clientInfo);

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
    WKPageDidReceiveTitleForFrameCallback                               didReceiveTitleForFrame;
    WKPageDidFirstLayoutForFrame                                        didFirstLayoutForFrame;
    WKPageDidFirstVisuallyNonEmptyLayoutForFrame                        didFirstVisuallyNonEmptyLayoutForFrame;

    // FIXME: Move to progress client.
    WKPageDidStartProgressCallback                                      didStartProgress;
    WKPageDidChangeProgressCallback                                     didChangeProgress;
    WKPageDidFinishProgressCallback                                     didFinishProgress;

    // FIXME: These three functions should not be part of this client.
    WKPageDidBecomeUnresponsiveCallback                                 didBecomeUnresponsive;
    WKPageDidBecomeResponsiveCallback                                   didBecomeResponsive;
    WKPageProcessDidExitCallback                                        processDidExit;

    WKPageDidChangeBackForwardListCallback                              didChangeBackForwardList;
};
typedef struct WKPageLoaderClient WKPageLoaderClient;

// Policy Client.
typedef void (*WKPageDecidePolicyForNavigationActionCallback)(WKPageRef page, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo);
typedef void (*WKPageDecidePolicyForNewWindowActionCallback)(WKPageRef page, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo);
typedef void (*WKPageDecidePolicyForMIMETypeCallback)(WKPageRef page, WKStringRef MIMEType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo);

struct WKPagePolicyClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageDecidePolicyForNavigationActionCallback                       decidePolicyForNavigationAction;
    WKPageDecidePolicyForNewWindowActionCallback                        decidePolicyForNewWindowAction;
    WKPageDecidePolicyForMIMETypeCallback                               decidePolicyForMIMEType;
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

// UI Client
typedef WKPageRef (*WKPageCreateNewPageCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageShowPageCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageCloseCallback)(WKPageRef page, const void *clientInfo);
typedef void (*WKPageRunJavaScriptAlertCallback)(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void *clientInfo);
typedef bool (*WKPageRunJavaScriptConfirmCallback)(WKPageRef page, WKStringRef message, WKFrameRef frame, const void *clientInfo);
typedef WKStringRef (*WKPageRunJavaScriptPromptCallback)(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void *clientInfo);
typedef void (*WKPageContentsSizeChangedCallback)(WKPageRef page, int width, int height, WKFrameRef frame, const void *clientInfo);

struct WKPageUIClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKPageCreateNewPageCallback                                         createNewPage;
    WKPageShowPageCallback                                              showPage;
    WKPageCloseCallback                                                 close;
    WKPageRunJavaScriptAlertCallback                                    runJavaScriptAlert;
    WKPageRunJavaScriptConfirmCallback                                  runJavaScriptConfirm;
    WKPageRunJavaScriptPromptCallback                                   runJavaScriptPrompt;
    WKPageContentsSizeChangedCallback                                   contentsSizeChanged;
};
typedef struct WKPageUIClient WKPageUIClient;

WK_EXPORT WKTypeID WKPageGetTypeID();

WK_EXPORT WKPageNamespaceRef WKPageGetPageNamespace(WKPageRef page);

WK_EXPORT void WKPageLoadURL(WKPageRef page, WKURLRef url);
WK_EXPORT void WKPageLoadURLRequest(WKPageRef page, WKURLRequestRef urlRequest);

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
WK_EXPORT WKBackForwardListRef WKPageGetBackForwardList(WKPageRef page);

WK_EXPORT WKStringRef WKPageCopyTitle(WKPageRef page);

WK_EXPORT WKFrameRef WKPageGetMainFrame(WKPageRef page);
WK_EXPORT double WKPageGetEstimatedProgress(WKPageRef page);

WK_EXPORT void WKPageSetCustomUserAgent(WKPageRef page, WKStringRef userAgent);

WK_EXPORT void WKPageTerminate(WKPageRef page);

WK_EXPORT WKDataRef WKPageCopySessionState(WKPageRef page);
WK_EXPORT void WKPageRestoreFromSessionState(WKPageRef page, WKDataRef sessionStateData);

WK_EXPORT void WKPageSetPageLoaderClient(WKPageRef page, const WKPageLoaderClient* client);
WK_EXPORT void WKPageSetPagePolicyClient(WKPageRef page, const WKPagePolicyClient* client);
WK_EXPORT void WKPageSetPageFormClient(WKPageRef page, const WKPageFormClient* client);
WK_EXPORT void WKPageSetPageUIClient(WKPageRef page, const WKPageUIClient* client);

typedef void (*WKPageRunJavaScriptFunction)(WKStringRef, WKErrorRef, void*);
WK_EXPORT void WKPageRunJavaScriptInMainFrame(WKPageRef page, WKStringRef script, void *context, WKPageRunJavaScriptFunction function);
#ifdef __BLOCKS__
typedef void (^WKPageRunJavaScriptBlock)(WKStringRef, WKErrorRef);
WK_EXPORT void WKPageRunJavaScriptInMainFrame_b(WKPageRef page, WKStringRef script, WKPageRunJavaScriptBlock block);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WKPage_h */
