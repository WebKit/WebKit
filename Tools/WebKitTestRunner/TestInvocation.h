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

#ifndef TestInvocation_h
#define TestInvocation_h

#include "JSWrappable.h"
#include "TestOptions.h"
#include "UIScriptContext.h"
#include <WebKit/WKRetainPtr.h>
#include <string>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringBuilder.h>

namespace WTR {

class TestInvocation : public UIScriptContextDelegate {
    WTF_MAKE_NONCOPYABLE(TestInvocation);
public:
    explicit TestInvocation(WKURLRef, const TestOptions&);
    ~TestInvocation();

    WKURLRef url() const;
    bool urlContains(const char*) const;
    
    const TestOptions& options() const { return m_options; }

    void setIsPixelTest(const std::string& expectedPixelHash);

    void setCustomTimeout(int duration) { m_timeout = duration; }
    int customTimeout() const { return m_timeout; }

    void invoke();
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);
    WKRetainPtr<WKTypeRef> didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);

    void dumpWebProcessUnresponsiveness();
    static void dumpWebProcessUnresponsiveness(const char* errorMessage);
    void outputText(const WTF::String&);

    void didBeginSwipe();
    void willEndSwipe();
    void didEndSwipe();
    void didRemoveSwipeSnapshot();

    void notifyDownloadDone();

private:
    void dumpResults();
    static void dump(const char* textToStdout, const char* textToStderr = 0, bool seenError = false);
    enum class SnapshotResultType { WebView, WebContents };
    void dumpPixelsAndCompareWithExpected(WKImageRef, WKArrayRef repaintRects, SnapshotResultType);
    void dumpAudio(WKDataRef);
    bool compareActualHashToExpectedAndDumpResults(const char[33]);

    static void forceRepaintDoneCallback(WKErrorRef, void* context);
    
    struct UIScriptInvocationData {
        unsigned callbackID;
        WebKit::WKRetainPtr<WKStringRef> scriptString;
        TestInvocation* testInvocation;
    };
    static void runUISideScriptAfterUpdateCallback(WKErrorRef, void* context);

    bool shouldLogFrameLoadDelegates() const;
    bool shouldLogHistoryClientCallbacks() const;

    void runUISideScript(WKStringRef, unsigned callbackID);
    // UIScriptContextDelegate
    void uiScriptDidComplete(WKStringRef result, unsigned callbackID) override;

    const TestOptions m_options;
    
    WKRetainPtr<WKURLRef> m_url;
    WTF::String m_urlString;

    std::string m_expectedPixelHash;

    int m_timeout { 0 };

    // Invocation state
    bool m_gotInitialResponse { false };
    bool m_gotFinalMessage { false };
    bool m_gotRepaint { false };
    bool m_error { false };

    bool m_dumpPixels { false };
    bool m_pixelResultIsPending { false };
    bool m_webProcessIsUnresponsive { false };

    StringBuilder m_textOutput;
    WKRetainPtr<WKDataRef> m_audioResult;
    WKRetainPtr<WKImageRef> m_pixelResult;
    WKRetainPtr<WKArrayRef> m_repaintRects;
    std::string m_errorMessage;
    
    std::unique_ptr<UIScriptContext> m_UIScriptContext;
    UIScriptInvocationData* m_pendingUIScriptInvocationData { nullptr };
};

} // namespace WTR

#endif // TestInvocation_h
