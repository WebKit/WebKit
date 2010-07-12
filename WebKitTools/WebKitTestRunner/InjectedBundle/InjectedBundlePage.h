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

private:
    // Loader Client
    static void _didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
    static void _didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
    static void _didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
    static void _didCommitLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
    static void _didFinishLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
    static void _didFailLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
    static void _didReceiveTitleForFrame(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, const void *clientInfo);
    static void _didClearWindowForFrame(WKBundlePageRef page, WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window, const void *clientInfo);
    void didStartProvisionalLoadForFrame(WKBundleFrameRef frame);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef frame);
    void didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef frame);
    void didCommitLoadForFrame(WKBundleFrameRef frame);
    void didFinishLoadForFrame(WKBundleFrameRef frame);
    void didFailLoadWithErrorForFrame(WKBundleFrameRef frame);
    void didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef frame);
    void didClearWindowForFrame(WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window);

    // UI Client
    static void _addMessageToConsole(WKBundlePageRef page, WKStringRef message, uint32_t lineNumber, const void *clientInfo);
    void addMessageToConsole(WKStringRef message, uint32_t lineNumber);

    WKBundlePageRef m_page;
    bool m_isLoading;
};

} // namespace WTR

#endif // InjectedBundlePage_h
