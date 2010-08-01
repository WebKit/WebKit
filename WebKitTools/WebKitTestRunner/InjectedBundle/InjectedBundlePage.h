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

#ifndef InjectedBundlePage_h
#define InjectedBundlePage_h

#include <WebKit2/WKBundlePage.h>

namespace WTR {

class InjectedBundlePage {
public:
    InjectedBundlePage(WKBundlePageRef);
    ~InjectedBundlePage();

    WKBundlePageRef page() const { return m_page; }
    void dump();

    bool isLoading() { return m_isLoading; }

    void reset();

private:
    // Loader Client
    static void _didStartProvisionalLoadForFrame(WKBundlePageRef, WKBundleFrameRef, const void* clientInfo);
    static void _didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef, WKBundleFrameRef, const void* clientInfo);
    static void _didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef, WKBundleFrameRef, const void* clientInfo);
    static void _didCommitLoadForFrame(WKBundlePageRef, WKBundleFrameRef, const void* clientInfo);
    static void _didFinishLoadForFrame(WKBundlePageRef, WKBundleFrameRef, const void* clientInfo);
    static void _didFailLoadWithErrorForFrame(WKBundlePageRef, WKBundleFrameRef, const void* clientInfo);
    static void _didReceiveTitleForFrame(WKBundlePageRef, WKStringRef title, WKBundleFrameRef, const void* clientInfo);
    static void _didClearWindowForFrame(WKBundlePageRef, WKBundleFrameRef, JSGlobalContextRef, JSObjectRef window, const void* clientInfo);
    void didStartProvisionalLoadForFrame(WKBundleFrameRef);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef);
    void didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef);
    void didCommitLoadForFrame(WKBundleFrameRef);
    void didFinishLoadForFrame(WKBundleFrameRef);
    void didFailLoadWithErrorForFrame(WKBundleFrameRef);
    void didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef);
    void didClearWindowForFrame(WKBundleFrameRef, JSGlobalContextRef, JSObjectRef window);

    // UI Client
    static void _willAddMessageToConsole(WKBundlePageRef, WKStringRef message, uint32_t lineNumber, const void* clientInfo);
    static void _willSetStatusbarText(WKBundlePageRef, WKStringRef statusbarText, const void* clientInfo);
    static void _willRunJavaScriptAlert(WKBundlePageRef, WKStringRef message, WKBundleFrameRef frame, const void* clientInfo);
    static void _willRunJavaScriptConfirm(WKBundlePageRef, WKStringRef message, WKBundleFrameRef frame, const void* clientInfo);
    static void _willRunJavaScriptPrompt(WKBundlePageRef, WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef frame, const void* clientInfo);
    void willAddMessageToConsole(WKStringRef message, uint32_t lineNumber);
    void willSetStatusbarText(WKStringRef statusbarText);
    void willRunJavaScriptAlert(WKStringRef message, WKBundleFrameRef);
    void willRunJavaScriptConfirm(WKStringRef message, WKBundleFrameRef);
    void willRunJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef);

    // Editor client
    static bool _shouldBeginEditing(WKBundlePageRef, WKBundleRangeRef, const void* clientInfo);
    static bool _shouldEndEditing(WKBundlePageRef, WKBundleRangeRef, const void* clientInfo);
    static bool _shouldInsertNode(WKBundlePageRef, WKBundleNodeRef, WKBundleRangeRef rangeToReplace, WKInsertActionType, const void* clientInfo);
    static bool _shouldInsertText(WKBundlePageRef, WKStringRef, WKBundleRangeRef rangeToReplace, WKInsertActionType, const void* clientInfo);
    static bool _shouldDeleteRange(WKBundlePageRef, WKBundleRangeRef, const void* clientInfo);
    static bool _shouldChangeSelectedRange(WKBundlePageRef, WKBundleRangeRef fromRange, WKBundleRangeRef toRange, WKAffinityType, bool stillSelecting, const void* clientInfo);
    static bool _shouldApplyStyle(WKBundlePageRef, WKBundleCSSStyleDeclarationRef style, WKBundleRangeRef range, const void* clientInfo);
    static void _didBeginEditing(WKBundlePageRef, WKStringRef notificationName, const void* clientInfo);
    static void _didEndEditing(WKBundlePageRef, WKStringRef notificationName, const void* clientInfo);
    static void _didChange(WKBundlePageRef, WKStringRef notificationName, const void* clientInfo);
    static void _didChangeSelection(WKBundlePageRef, WKStringRef notificationName, const void* clientInfo);
    bool shouldBeginEditing(WKBundleRangeRef);
    bool shouldEndEditing(WKBundleRangeRef);
    bool shouldInsertNode(WKBundleNodeRef, WKBundleRangeRef rangeToReplace, WKInsertActionType);
    bool shouldInsertText(WKStringRef, WKBundleRangeRef rangeToReplace, WKInsertActionType);
    bool shouldDeleteRange(WKBundleRangeRef);
    bool shouldChangeSelectedRange(WKBundleRangeRef fromRange, WKBundleRangeRef toRange, WKAffinityType, bool stillSelecting);
    bool shouldApplyStyle(WKBundleCSSStyleDeclarationRef style, WKBundleRangeRef range);
    void didBeginEditing(WKStringRef notificationName);
    void didEndEditing(WKStringRef notificationName);
    void didChange(WKStringRef notificationName);
    void didChangeSelection(WKStringRef notificationName);

    void dumpAllFramesText();
    void dumpAllFrameScrollPositions();

    WKBundlePageRef m_page;
    bool m_isLoading;
};

} // namespace WTR

#endif // InjectedBundlePage_h
