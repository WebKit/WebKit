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

#include "TestInvocation.h"

#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestController.h"
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>

using namespace WebKit;
using namespace std;

namespace WTR {

TestInvocation::TestInvocation(const char* pathOrURL)
    : m_url(AdoptWK, createWKURL(pathOrURL))
    , m_pathOrURL(fastStrDup(pathOrURL))
    , m_gotInitialResponse(false)
    , m_gotFinalMessage(false)
    , m_error(false)
{
}

TestInvocation::~TestInvocation()
{
    fastFree(m_pathOrURL);
}

static const unsigned w3cSVGWidth = 480;
static const unsigned w3cSVGHeight = 360;
static const unsigned normalWidth = 800;
static const unsigned normalHeight = 600;

static void sizeWebViewForCurrentTest(char* pathOrURL)
{
    bool isSVGW3CTest = strstr(pathOrURL, "svg/W3C-SVG-1.1") || strstr(pathOrURL, "svg\\W3C-SVG-1.1");

    if (isSVGW3CTest)
        TestController::shared().mainWebView()->resizeTo(w3cSVGWidth, w3cSVGHeight);
    else
        TestController::shared().mainWebView()->resizeTo(normalWidth, normalHeight);
}

void TestInvocation::invoke()
{
    sizeWebViewForCurrentTest(m_pathOrURL);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("BeginTest"));
    WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), 0);

    TestController::runUntil(m_gotInitialResponse);
    if (m_error) {
        dump("FAIL\n");
        return;
    }

    WKPageLoadURL(TestController::shared().mainWebView()->page(), m_url.get());

    TestController::runUntil(m_gotFinalMessage);
    if (m_error) {
        dump("FAIL\n");
        return;
    }
}

void TestInvocation::dump(const char* stringToDump)
{
    printf("Content-Type: text/plain\n");
    printf("%s", stringToDump);

    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stderr);

    fflush(stdout);
    fflush(stderr);
}

void TestInvocation::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "Error")) {
        // Set all states to true to stop spinning the runloop.
        m_gotInitialResponse = true;
        m_gotFinalMessage = true;
        m_error = true;
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Ack")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef messageBodyString = static_cast<WKStringRef>(messageBody);
        if (WKStringIsEqualToUTF8CString(messageBodyString, "BeginTest")) {
            m_gotInitialResponse = true;
            return;
        }

        ASSERT_NOT_REACHED();
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Done")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef messageBodyString = static_cast<WKStringRef>(messageBody);

        dump(toSTD(messageBodyString).c_str());

        m_gotFinalMessage = true;
        return;
    }

    ASSERT_NOT_REACHED();
}

} // namespace WTR
