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

#pragma once

#include "JSWrappable.h"
#include "TestOptions.h"
#include "UIScriptContext.h"
#include "WhatToDump.h"
#include <WebKit/WKRetainPtr.h>
#include <string>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/text/StringBuilder.h>

namespace WTR {

class TestInvocation final : public RefCounted<TestInvocation>, public UIScriptContextDelegate {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(TestInvocation);
public:
    static Ref<TestInvocation> create(WKURLRef, const TestOptions&);
    ~TestInvocation();

    WKURLRef url() const { return m_url.get(); }
    bool urlContains(StringView) const;
    
    const TestOptions& options() const { return m_options; }

    void setIsPixelTest(const std::string& expectedPixelHash);
    void setForceDumpPixels(bool forceDumpPixels) { m_forceDumpPixels = forceDumpPixels; }

    void setCustomTimeout(Seconds duration) { m_timeout = duration; }
    void setDumpJSConsoleLogInStdErr(bool value) { m_dumpJSConsoleLogInStdErr = value; }

    Seconds shortTimeout() const;

    void invoke();
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);
    WKRetainPtr<WKTypeRef> didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);

    static void dumpWebProcessUnresponsiveness(const char* errorMessage);
    void outputText(const String&);

    void didBeginSwipe();
    void willEndSwipe();
    void didEndSwipe();
    void didRemoveSwipeSnapshot();

    void notifyDownloadDone();

    void dumpResourceLoadStatistics();

    bool canOpenWindows() const { return m_canOpenWindows; }

    void dumpPrivateClickMeasurement();

    void willCreateNewPage();

    void loadTestInCrossOriginIframe();

    void dumpResourceLoadStatisticsIfNecessary();

private:
    TestInvocation(WKURLRef, const TestOptions&);

    WKRetainPtr<WKMutableDictionaryRef> createTestSettingsDictionary();

    void waitToDumpWatchdogTimerFired();
    void initializeWaitToDumpWatchdogTimerIfNeeded();
    void invalidateWaitToDumpWatchdogTimer();

    void waitForPostDumpWatchdogTimerFired();
    void initializeWaitForPostDumpWatchdogTimerIfNeeded();
    void invalidateWaitForPostDumpWatchdogTimer();
    
    void done();
    void setWaitUntilDone(bool);

    // Returns true if the caller bundle should proceed with dumping.
    // Returns false if the WKTR invokes dumping through page, asynchronously.
    // Resets waitUntilDone.
    bool resolveNotifyDone();
    bool resolveForceImmediateCompletion();

    void dumpResults();
    static void dump(const char* textToStdout, const char* textToStderr = 0, bool seenError = false);
    enum class SnapshotResultType { WebView, WebContents };
    void dumpPixelsAndCompareWithExpected(SnapshotResultType, WKArrayRef repaintRects, WKImageRef = nullptr);
    void dumpAudio(WKDataRef);
    bool compareActualHashToExpectedAndDumpResults(const std::string&);

    static void forceRepaintDoneCallback(WKErrorRef, void* context);
    
    struct UIScriptInvocationData {
        unsigned callbackID;
        WebKit::WKRetainPtr<WKStringRef> scriptString;
        TestInvocation* testInvocation;
    };
    static void runUISideScriptAfterUpdateCallback(WKErrorRef, void* context);
    static void runUISideScriptImmediately(WKErrorRef, void* context);

    bool shouldLogHistoryClientCallbacks() const;

    void runUISideScript(WKStringRef, unsigned callbackID);
    // UIScriptContextDelegate
    void uiScriptDidComplete(const String& result, unsigned callbackID) override;

    const TestOptions m_options;
    
    WKRetainPtr<WKURLRef> m_url;
    String m_urlString;
    RunLoop::Timer m_waitToDumpWatchdogTimer;
    RunLoop::Timer m_waitForPostDumpWatchdogTimer;

    std::string m_expectedPixelHash;

    Seconds m_timeout;
    bool m_dumpJSConsoleLogInStdErr { false };

    // Invocation state
    bool m_startedTesting { false };
    bool m_gotInitialResponse { false };
    bool m_gotFinalMessage { false };
    bool m_gotRepaint { false };
    bool m_error { false };

    bool m_waitUntilDone { false };
    bool m_dumpFrameLoadCallbacks { false };
    bool m_dumpPixels { false };
    bool m_forceDumpPixels { false };
    bool m_pixelResultIsPending { false };
    bool m_forceRepaint { true };
    bool m_shouldDumpResourceLoadStatistics { false };
    bool m_canOpenWindows { true };
    bool m_shouldDumpPrivateClickMeasurement { false };
    bool m_shouldDumpBackForwardListsForAllWindows { false };
    WhatToDump m_whatToDump { WhatToDump::RenderTree };

    StringBuilder m_textOutput;
    String m_savedResourceLoadStatistics;
    WKRetainPtr<WKDataRef> m_audioResult;
    WKRetainPtr<WKImageRef> m_pixelResult;
    WKRetainPtr<WKArrayRef> m_repaintRects;
    
    std::unique_ptr<UIScriptContext> m_UIScriptContext;
    UIScriptInvocationData* m_pendingUIScriptInvocationData { nullptr };
};

} // namespace WTR
