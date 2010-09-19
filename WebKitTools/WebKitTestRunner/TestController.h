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

#ifndef TestController_h
#define TestController_h

#include <WebKit2/WKRetainPtr.h>
#include <string>
#include <vector>
#include <wtf/OwnPtr.h>

namespace WTR {

class TestInvocation;
class PlatformWebView;

// FIXME: Rename this TestRunner?
class TestController {
public:
    static TestController& shared();

    TestController(int argc, const char* argv[]);
    ~TestController();

    bool verbose() const { return m_verbose; }

    WKStringRef injectedBundlePath() { return m_injectedBundlePath.get(); }
    WKStringRef testPluginDirectory() { return m_testPluginDirectory.get(); }

    PlatformWebView* mainWebView() { return m_mainWebView.get(); }
    WKPageNamespaceRef pageNamespace() { return m_pageNamespace.get(); }
    WKContextRef context() { return m_context.get(); }

    // Helper
    static void runUntil(bool& done);

private:
    void initialize(int argc, const char* argv[]);
    void run();

    void runTestingServerLoop();
    void runTest(const char* pathOrURL);

    void platformInitialize();
    void platformInitializeContext();
    void initializeInjectedBundlePath();
    void initializeTestPluginDirectory();

    void resetStateToConsistentValues();

    // WKContextInjectedBundleClient
    static void didReceiveMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, const void*);
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);

    // WKPageLoaderClient
    static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void*);
    void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame);


    OwnPtr<TestInvocation> m_currentInvocation;

    bool m_dumpPixels;
    bool m_verbose;
    bool m_printSeparators;
    bool m_usingServerMode;
    std::vector<std::string> m_paths;
    WKRetainPtr<WKStringRef> m_injectedBundlePath;
    WKRetainPtr<WKStringRef> m_testPluginDirectory;

    OwnPtr<PlatformWebView> m_mainWebView;
    WKRetainPtr<WKContextRef> m_context;
    WKRetainPtr<WKPageNamespaceRef> m_pageNamespace;
    
    enum State {
        Initial,
        Resetting,
        RunningTest
    };
    State m_state;
    bool m_doneResetting;
};

} // namespace WTR

#endif // TestController_h
