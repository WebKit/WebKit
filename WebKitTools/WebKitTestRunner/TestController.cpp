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
#include "TestInvocation.h"
#include <WebKit2/WKContextPrivate.h>
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
        0
    };
    WKPageSetPageUIClient(newPage, &otherPageUIClient);

    WKRetain(newPage);
    return newPage;
}

void TestController::initialize(int argc, const char* argv[])
{
    platformInitialize();

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
        
        // Skip any other arguments that begin with '--'.
        if (argument.length() >= 2 && argument[0] == '-' && argument[1] == '-')
            continue;

        m_paths.push_back(argument);
    }

    m_usingServerMode = (m_paths.size() == 1 && m_paths[0] == "-");
    if (m_usingServerMode)
        m_printSeparators = true;
    else
        m_printSeparators = m_paths.size() > 1;

    initializeInjectedBundlePath();
    initializeTestPluginDirectory();

    m_context.adopt(WKContextCreateWithInjectedBundlePath(injectedBundlePath()));

    WKContextInjectedBundleClient injectedBundleClient = {
        0,
        this,
        didReceiveMessageFromInjectedBundle
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
        0
    };
    WKPageSetPageUIClient(m_mainWebView->page(), &pageUIClient);
}

void TestController::runTest(const char* test)
{
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

void TestController::didReceiveMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void TestController::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    m_currentInvocation->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

} // namespace WTR
