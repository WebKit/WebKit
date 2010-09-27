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

#ifndef WKBundlePage_h
#define WKBundlePage_h

#include <WebKit2/WKBase.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKInsertActionTyped = 0,
    kWKInsertActionPasted = 1,
    kWKInsertActionDropped = 2
};
typedef uint32_t WKInsertActionType;

enum {
    kWKAffinityUpstream,
    kWKAffinityDownstream
};
typedef uint32_t WKAffinityType;

enum {
    WKInputFieldActionTypeMoveUp,
    WKInputFieldActionTypeMoveDown,
    WKInputFieldActionTypeCancel,
    WKInputFieldActionTypeInsertTab,
    WKInputFieldActionTypeInsertBacktab,
    WKInputFieldActionTypeInsertNewline,
    WKInputFieldActionTypeInsertDelete
};
typedef uint32_t WKInputFieldActionType;

// Loader Client
typedef void (*WKBundlePageDidStartProvisionalLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidReceiveServerRedirectForProvisionalLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidFailProvisionalLoadWithErrorForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo); // FIXME: Add WKErrorRef.
typedef void (*WKBundlePageDidCommitLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidDocumentFinishLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidFinishLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidFinishDocumentLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidFailLoadWithErrorForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo); // FIXME: Add WKErrorRef.
typedef void (*WKBundlePageDidReceiveTitleForFrameCallback)(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidFirstLayoutForFrame)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidFirstVisuallyNonEmptyLayoutForFrame)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
typedef void (*WKBundlePageDidRemoveFrameFromHierarchyCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo);
// FIXME: There are no WKPage equivilent of these functions yet.
typedef void (*WKBundlePageDidClearWindowObjectForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleScriptWorldRef world, const void *clientInfo);
typedef void (*WKBundlePageDidCancelClientRedirectForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageWillPerformClientRedirectForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, WKURLRef url, double delay, double date, const void *clientInfo);
typedef void (*WKBundlePageDidChangeLocationWithinPageForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidHandleOnloadEventsForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidDisplayInsecureContentForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidRunInsecureContentForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo); // FIXME: Add WKSecurityOriginRef.


struct WKBundlePageLoaderClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundlePageDidStartProvisionalLoadForFrameCallback                 didStartProvisionalLoadForFrame;
    WKBundlePageDidReceiveServerRedirectForProvisionalLoadForFrameCallback    didReceiveServerRedirectForProvisionalLoadForFrame;
    WKBundlePageDidFailProvisionalLoadWithErrorForFrameCallback         didFailProvisionalLoadWithErrorForFrame;
    WKBundlePageDidCommitLoadForFrameCallback                           didCommitLoadForFrame;
    WKBundlePageDidFinishDocumentLoadForFrameCallback                   didFinishDocumentLoadForFrame;
    WKBundlePageDidFinishLoadForFrameCallback                           didFinishLoadForFrame;
    WKBundlePageDidFailLoadWithErrorForFrameCallback                    didFailLoadWithErrorForFrame;
    WKBundlePageDidReceiveTitleForFrameCallback                         didReceiveTitleForFrame;
    WKBundlePageDidFirstLayoutForFrame                                  didFirstLayoutForFrame;
    WKBundlePageDidFirstVisuallyNonEmptyLayoutForFrame                  didFirstVisuallyNonEmptyLayoutForFrame;
    WKBundlePageDidRemoveFrameFromHierarchyCallback                     didRemoveFrameFromHierarchy;

    // FIXME: There are no WKPage equivilent of these functions yet.
    WKBundlePageDidClearWindowObjectForFrameCallback                    didClearWindowObjectForFrame;
    WKBundlePageDidCancelClientRedirectForFrameCallback                 didCancelClientRedirectForFrame;
    WKBundlePageWillPerformClientRedirectForFrameCallback               willPerformClientRedirectForFrame;
    WKBundlePageDidChangeLocationWithinPageForFrameCallback             didChangeLocationWithinPageForFrame;
    WKBundlePageDidHandleOnloadEventsForFrameCallback                   didHandleOnloadEventsForFrame;
    WKBundlePageDidDisplayInsecureContentForFrameCallback               didDisplayInsecureContentForFrame;
    WKBundlePageDidRunInsecureContentForFrameCallback                   didRunInsecureContentForFrame;
};
typedef struct WKBundlePageLoaderClient WKBundlePageLoaderClient;

// UI Client
typedef void (*WKBundlePageWillAddMessageToConsoleCallback)(WKBundlePageRef page, WKStringRef message, uint32_t lineNumber, const void *clientInfo);
typedef void (*WKBundlePageWillSetStatusbarTextCallback)(WKBundlePageRef page, WKStringRef statusbarText, const void *clientInfo);
typedef void (*WKBundlePageWillRunJavaScriptAlertCallback)(WKBundlePageRef page, WKStringRef alertText, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageWillRunJavaScriptConfirmCallback)(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageWillRunJavaScriptPromptCallback)(WKBundlePageRef page, WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageMouseDidMoveOverElementCallback)(WKBundlePageRef page, WKBundleHitTestResultRef hitTestResult, WKTypeRef* userData, const void *clientInfo);

struct WKBundlePageUIClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundlePageWillAddMessageToConsoleCallback                         willAddMessageToConsole;
    WKBundlePageWillSetStatusbarTextCallback                            willSetStatusbarText;
    WKBundlePageWillRunJavaScriptAlertCallback                          willRunJavaScriptAlert;
    WKBundlePageWillRunJavaScriptConfirmCallback                        willRunJavaScriptConfirm;
    WKBundlePageWillRunJavaScriptPromptCallback                         willRunJavaScriptPrompt;
    WKBundlePageMouseDidMoveOverElementCallback                         mouseDidMoveOverElement;
};
typedef struct WKBundlePageUIClient WKBundlePageUIClient;

// Editor client
typedef bool (*WKBundlePageShouldBeginEditingCallback)(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo);
typedef bool (*WKBundlePageShouldEndEditingCallback)(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo);
typedef bool (*WKBundlePageShouldInsertNodeCallback)(WKBundlePageRef page, WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action, const void* clientInfo);
typedef bool (*WKBundlePageShouldInsertTextCallback)(WKBundlePageRef page, WKStringRef string, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action, const void* clientInfo);
typedef bool (*WKBundlePageShouldDeleteRangeCallback)(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo);
typedef bool (*WKBundlePageShouldChangeSelectedRange)(WKBundlePageRef page, WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting, const void* clientInfo);
typedef bool (*WKBundlePageShouldApplyStyle)(WKBundlePageRef page, WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range, const void* clientInfo);
typedef void (*WKBundlePageEditingNotification)(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo);

struct WKBundlePageEditorClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundlePageShouldBeginEditingCallback                              shouldBeginEditing;
    WKBundlePageShouldEndEditingCallback                                shouldEndEditing;
    WKBundlePageShouldInsertNodeCallback                                shouldInsertNode;
    WKBundlePageShouldInsertTextCallback                                shouldInsertText;
    WKBundlePageShouldDeleteRangeCallback                               shouldDeleteRange;
    WKBundlePageShouldChangeSelectedRange                               shouldChangeSelectedRange;
    WKBundlePageShouldApplyStyle                                        shouldApplyStyle;
    WKBundlePageEditingNotification                                     didBeginEditing;
    WKBundlePageEditingNotification                                     didEndEditing;
    WKBundlePageEditingNotification                                     didChange;
    WKBundlePageEditingNotification                                     didChangeSelection;
};
typedef struct WKBundlePageEditorClient WKBundlePageEditorClient;

// Form client
typedef void (*WKBundlePageTextFieldDidBeginEditingCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlInputElementHandle, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageTextFieldDidEndEditingCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlInputElementHandle, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageTextDidChangeInTextFieldCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlInputElementHandle, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageTextDidChangeInTextAreaCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlTextAreaElementHandle, WKBundleFrameRef frame, const void* clientInfo);
typedef bool (*WKBundlePageShouldPerformActionInTextFieldCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlInputElementHandle, WKInputFieldActionType actionType, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageWillSubmitFormCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlFormElementHandle, WKBundleFrameRef frame, WKBundleFrameRef sourceFrame, WKDictionaryRef values, WKTypeRef* userData, const void* clientInfo);

struct WKBundlePageFormClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundlePageTextFieldDidBeginEditingCallback                        textFieldDidBeginEditing;
    WKBundlePageTextFieldDidEndEditingCallback                          textFieldDidEndEditing;
    WKBundlePageTextDidChangeInTextFieldCallback                        textDidChangeInTextField;
    WKBundlePageTextDidChangeInTextAreaCallback                         textDidChangeInTextArea;
    WKBundlePageShouldPerformActionInTextFieldCallback                  shouldPerformActionInTextField;
    WKBundlePageWillSubmitFormCallback                                  willSubmitForm;
};
typedef struct WKBundlePageFormClient WKBundlePageFormClient;

WK_EXPORT WKTypeID WKBundlePageGetTypeID();

WK_EXPORT void WKBundlePageSetEditorClient(WKBundlePageRef page, WKBundlePageEditorClient* client);
WK_EXPORT void WKBundlePageSetFormClient(WKBundlePageRef page, WKBundlePageFormClient* client);
WK_EXPORT void WKBundlePageSetLoaderClient(WKBundlePageRef page, WKBundlePageLoaderClient* client);
WK_EXPORT void WKBundlePageSetUIClient(WKBundlePageRef page, WKBundlePageUIClient* client);

WK_EXPORT WKBundleFrameRef WKBundlePageGetMainFrame(WKBundlePageRef page);

#ifdef __cplusplus
}
#endif

#endif /* WKBundlePage_h */
