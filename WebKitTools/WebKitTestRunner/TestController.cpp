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

#include "TestController.h"

#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestInvocation.h"
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKPreferencesPrivate.h>
#include <wtf/PassOwnPtr.h>

namespace WTR {

static TestController* controller;

TestController& TestController::shared()
{
    ASSERT(controller);
    return *controller;
}

TestController::TestController(int argc, const char* argv[])
    : m_dumpPixels(false)
    , m_verbose(false)
    , m_printSeparators(false)
    , m_usingServerMode(false)
    , m_state(Initial)
    , m_doneResetting(false)
{
    initialize(argc, argv);
    controller = this;
    run();
    controller = 0;
}

TestController::~TestController()
{
}

static void closeOtherPage(WKPageRef page, const void* clientInfo)
{
    WKPageClose(page);
    const PlatformWebView* view = static_cast<const PlatformWebView*>(clientInfo);
    delete view;
}

static WKPageRef createOtherPage(WKPageRef oldPage, const void*)
{
    PlatformWebView* view = new PlatformWebView(WKPageGetPageNamespace(oldPage));
    WKPageRef newPage = view->page();

    view->resizeTo(800, 600);

    WKPageUIClient otherPageUIClient = {
        0,
        view,
        createOtherPage,
        0,
        closeOtherPage,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };
    WKPageSetPageUIClient(newPage, &otherPageUIClient);

    WKRetain(newPage);
    return newPage;
}

void TestController::initialize(int argc, const char* argv[])
{
    platformInitialize();

    bool printSupportedFeatures = false;

    for (int i = 1; i < argc; ++i) {
        std::string argument(argv[i]);

        if (argument == "--pixel-tests") {
            m_dumpPixels = true;
            continue;
        }
        if (argument == "--verbose") {
            m_verbose = true;
            continue;
        }
        if (argument == "--print-supported-features") {
            printSupportedFeatures = true;
            break;
        }

        // Skip any other arguments that begin with '--'.
        if (argument.length() >= 2 && argument[0] == '-' && argument[1] == '-')
            continue;

        m_paths.push_back(argument);
    }

    if (printSupportedFeatures) {
        // FIXME: On Windows, DumpRenderTree uses this to expose whether it supports 3d
        // transforms and accelerated compositing. When we support those features, we
        // should match DRT's behavior.
        exit(0);
    }

    m_usingServerMode = (m_paths.size() == 1 && m_paths[0] == "-");
    if (m_usingServerMode)
        m_printSeparators = true;
    else
        m_printSeparators = m_paths.size() > 1;

    initializeInjectedBundlePath();
    initializeTestPluginDirectory();

    m_context.adopt(WKContextCreateWithInjectedBundlePath(injectedBundlePath()));
    platformInitializeContext();

    WKContextInjectedBundleClient injectedBundleClient = {
        0,
        this,
        didReceiveMessageFromInjectedBundle,
        0
    };
    WKContextSetInjectedBundleClient(m_context.get(), &injectedBundleClient);

    _WKContextSetAdditionalPluginsDirectory(m_context.get(), testPluginDirectory());

    m_pageNamespace.adopt(WKPageNamespaceCreate(m_context.get()));
    m_mainWebView = adoptPtr(new PlatformWebView(m_pageNamespace.get()));

    WKPageUIClient pageUIClient = {
        0,
        this,
        createOtherPage,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };
    WKPageSetPageUIClient(m_mainWebView->page(), &pageUIClient);

    WKPageLoaderClient pageLoaderClient = {
        0,
        this,
        0,
        0,
        0,
        0,
        0,
        didFinishLoadForFrame,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };
    WKPageSetPageLoaderClient(m_mainWebView->page(), &pageLoaderClient);
}

void TestController::resetStateToConsistentValues()
{
    m_state = Resetting;

    // FIXME: This function should also ensure that there is only one page open.

    // Reset preferences
    WKPreferencesRef preferences = WKContextGetPreferences(m_context.get());
    WKPreferencesSetOfflineWebApplicationCacheEnabled(preferences, true);
    WKPreferencesSetFontSmoothingLevel(preferences, kWKFontSmoothingLevelNoSubpixelAntiAliasing);
    WKPreferencesSetXSSAuditorEnabled(preferences, false);

    static WKStringRef standardFontFamily = WKStringCreateWithCFString(CFSTR("Times"));
    static WKStringRef cursiveFontFamily = WKStringCreateWithCFString(CFSTR("Apple Chancery"));
    static WKStringRef fantasyFontFamily = WKStringCreateWithCFString(CFSTR("Papyrus"));
    static WKStringRef fixedFontFamily = WKStringCreateWithCFString(CFSTR("Courier"));
    static WKStringRef sansSerifFontFamily = WKStringCreateWithCFString(CFSTR("Helvetica"));
    static WKStringRef serifFontFamily = WKStringCreateWithCFString(CFSTR("Times"));

    WKPreferencesSetStandardFontFamily(preferences, standardFontFamily);
    WKPreferencesSetCursiveFontFamily(preferences, cursiveFontFamily);
    WKPreferencesSetFantasyFontFamily(preferences, fantasyFontFamily);
    WKPreferencesSetFixedFontFamily(preferences, fixedFontFamily);
    WKPreferencesSetSansSerifFontFamily(preferences, sansSerifFontFamily);
    WKPreferencesSetSerifFontFamily(preferences, serifFontFamily);

    m_mainWebView->focus();

    // Reset main page back to about:blank
    m_doneResetting = false;

    WKRetainPtr<WKURLRef> url(AdoptWK, createWKURL("about:blank"));
    WKPageLoadURL(m_mainWebView->page(), url.get());
    TestController::runUntil(m_doneResetting);
}

void TestController::runTest(const char* test)
{
    resetStateToConsistentValues();

    m_state = RunningTest;
    m_currentInvocation.set(new TestInvocation(test));
    m_currentInvocation->invoke();
    m_currentInvocation.clear();
}

void TestController::runTestingServerLoop()
{
    char filenameBuffer[2048];
    while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
        char *newLineCharacter = strchr(filenameBuffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (strlen(filenameBuffer) == 0)
            continue;

        runTest(filenameBuffer);
    }
}

void TestController::run()
{
    if (m_usingServerMode)
        runTestingServerLoop();
    else {
        for (size_t i = 0; i < m_paths.size(); ++i)
            runTest(m_paths[i].c_str());
    }
}

// WKContextInjectedBundleClient

void TestController::didReceiveMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void TestController::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    m_currentInvocation->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

// WKPageLoaderClient

void TestController::didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didFinishLoadForFrame(page, frame);
}

void TestController::didFinishLoadForFrame(WKPageRef page, WKFrameRef frame)
{
    if (m_state != Resetting)
        return;

    if (!WKFrameIsMainFrame(frame))
        return;

    WKRetainPtr<WKURLRef> wkURL(AdoptWK, WKFrameCopyURL(frame));
    RetainPtr<CFURLRef> cfURL= toCF(wkURL);
    CFStringRef cfURLString = CFURLGetString(cfURL.get());
    if (!CFEqual(cfURLString, CFSTR("about:blank")))
        return;

    m_doneResetting = true;
}

} // namespace WTR
