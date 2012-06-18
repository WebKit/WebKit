/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TestInvocation.h"

#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestController.h"
#include <climits>
#include <cstdio>
#include <WebKit2/WKDictionary.h>
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKInspector.h>
#include <WebKit2/WKRetainPtr.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/text/CString.h>

#if OS(WINDOWS)
#include <direct.h> // For _getcwd.
#define getcwd _getcwd // MSDN says getcwd is deprecated.
#define PATH_MAX _MAX_PATH
#else
#include <unistd.h> // For getcwd.
#endif

using namespace WebKit;
using namespace std;

namespace WTR {

static WKURLRef createWKURL(const char* pathOrURL)
{
    if (strstr(pathOrURL, "http://") || strstr(pathOrURL, "https://") || strstr(pathOrURL, "file://"))
        return WKURLCreateWithUTF8CString(pathOrURL);

    // Creating from filesytem path.
    size_t length = strlen(pathOrURL);
    if (!length)
        return 0;

#if OS(WINDOWS)
    const char separator = '\\';
    bool isAbsolutePath = length >= 3 && pathOrURL[1] == ':' && pathOrURL[2] == separator;
    // FIXME: Remove the "localhost/" suffix once <http://webkit.org/b/55683> is fixed.
    const char* filePrefix = "file://localhost/";
#else
    const char separator = '/';
    bool isAbsolutePath = pathOrURL[0] == separator;
    const char* filePrefix = "file://";
#endif
    static const size_t prefixLength = strlen(filePrefix);

    OwnArrayPtr<char> buffer;
    if (isAbsolutePath) {
        buffer = adoptArrayPtr(new char[prefixLength + length + 1]);
        strcpy(buffer.get(), filePrefix);
        strcpy(buffer.get() + prefixLength, pathOrURL);
    } else {
        buffer = adoptArrayPtr(new char[prefixLength + PATH_MAX + length + 2]); // 1 for the separator
        strcpy(buffer.get(), filePrefix);
        if (!getcwd(buffer.get() + prefixLength, PATH_MAX))
            return 0;
        size_t numCharacters = strlen(buffer.get());
        buffer[numCharacters] = separator;
        strcpy(buffer.get() + numCharacters + 1, pathOrURL);
    }

    return WKURLCreateWithUTF8CString(buffer.get());
}

TestInvocation::TestInvocation(const std::string& pathOrURL)
    : m_url(AdoptWK, createWKURL(pathOrURL.c_str()))
    , m_pathOrURL(pathOrURL)
    , m_dumpPixels(false)
    , m_gotInitialResponse(false)
    , m_gotFinalMessage(false)
    , m_gotRepaint(false)
    , m_error(false)
{
}

TestInvocation::~TestInvocation()
{
}

void TestInvocation::setIsPixelTest(const std::string& expectedPixelHash)
{
    m_dumpPixels = true;
    m_expectedPixelHash = expectedPixelHash;
}

static const unsigned w3cSVGWidth = 480;
static const unsigned w3cSVGHeight = 360;
static const unsigned normalWidth = 800;
static const unsigned normalHeight = 600;

static void sizeWebViewForCurrentTest(const char* pathOrURL)
{
    bool isSVGW3CTest = strstr(pathOrURL, "svg/W3C-SVG-1.1") || strstr(pathOrURL, "svg\\W3C-SVG-1.1");

    if (isSVGW3CTest)
        TestController::shared().mainWebView()->resizeTo(w3cSVGWidth, w3cSVGHeight);
    else
        TestController::shared().mainWebView()->resizeTo(normalWidth, normalHeight);
}
static bool shouldLogFrameLoadDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "loading/");
}

#if ENABLE(INSPECTOR)
static bool shouldOpenWebInspector(const char* pathOrURL)
{
    return strstr(pathOrURL, "inspector/") || strstr(pathOrURL, "inspector\\");
}
#endif

void TestInvocation::invoke()
{
    sizeWebViewForCurrentTest(m_pathOrURL.c_str());

    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("BeginTest"));
    WKRetainPtr<WKMutableDictionaryRef> beginTestMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> dumpFrameLoadDelegatesKey = adoptWK(WKStringCreateWithUTF8CString("DumpFrameLoadDelegates"));
    WKRetainPtr<WKBooleanRef> dumpFrameLoadDelegatesValue = adoptWK(WKBooleanCreate(shouldLogFrameLoadDelegates(m_pathOrURL.c_str())));
    WKDictionaryAddItem(beginTestMessageBody.get(), dumpFrameLoadDelegatesKey.get(), dumpFrameLoadDelegatesValue.get());

    WKRetainPtr<WKStringRef> dumpPixelsKey = adoptWK(WKStringCreateWithUTF8CString("DumpPixels"));
    WKRetainPtr<WKBooleanRef> dumpPixelsValue = adoptWK(WKBooleanCreate(m_dumpPixels));
    WKDictionaryAddItem(beginTestMessageBody.get(), dumpPixelsKey.get(), dumpPixelsValue.get());

    WKRetainPtr<WKStringRef> useWaitToDumpWatchdogTimerKey = adoptWK(WKStringCreateWithUTF8CString("UseWaitToDumpWatchdogTimer"));
    WKRetainPtr<WKBooleanRef> useWaitToDumpWatchdogTimerValue = adoptWK(WKBooleanCreate(TestController::shared().useWaitToDumpWatchdogTimer()));
    WKDictionaryAddItem(beginTestMessageBody.get(), useWaitToDumpWatchdogTimerKey.get(), useWaitToDumpWatchdogTimerValue.get());

    WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), beginTestMessageBody.get());

    TestController::shared().runUntil(m_gotInitialResponse, TestController::ShortTimeout);
    if (!m_gotInitialResponse) {
        dump("Timed out waiting for initial response from web process\n");
        return;
    }
    if (m_error) {
        dump("FAIL\n");
        return;
    }

#if ENABLE(INSPECTOR)
    if (shouldOpenWebInspector(m_pathOrURL.c_str()))
        WKInspectorShow(WKPageGetInspector(TestController::shared().mainWebView()->page()));
#endif // ENABLE(INSPECTOR)        

    WKPageLoadURL(TestController::shared().mainWebView()->page(), m_url.get());

    TestController::shared().runUntil(m_gotFinalMessage, TestController::shared().useWaitToDumpWatchdogTimer() ? TestController::LongTimeout : TestController::NoTimeout);
    if (!m_gotFinalMessage)
        dump("Timed out waiting for final message from web process\n");
    else if (m_error)
        dump("FAIL\n");

#if ENABLE(INSPECTOR)
    WKInspectorClose(WKPageGetInspector(TestController::shared().mainWebView()->page()));
#endif // ENABLE(INSPECTOR)
}

void TestInvocation::dump(const char* stringToDump, bool singleEOF)
{
    printf("Content-Type: text/plain\n");
    printf("%s", stringToDump);

    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stderr);
    if (!singleEOF)
        fputs("#EOF\n", stdout);
    fflush(stdout);
    fflush(stderr);
}

bool TestInvocation::compareActualHashToExpectedAndDumpResults(const char actualHash[33])
{
    // Compute the hash of the bitmap context pixels
    fprintf(stdout, "\nActualHash: %s\n", actualHash);

    if (!m_expectedPixelHash.length())
        return false;

    ASSERT(m_expectedPixelHash.length() == 32);
    fprintf(stdout, "\nExpectedHash: %s\n", m_expectedPixelHash.c_str());

    // FIXME: Do case insensitive compare.
    return m_expectedPixelHash == actualHash;
}

void TestInvocation::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "Error")) {
        // Set all states to true to stop spinning the runloop.
        m_gotInitialResponse = true;
        m_gotFinalMessage = true;
        m_error = true;
        TestController::shared().notifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Ack")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef messageBodyString = static_cast<WKStringRef>(messageBody);
        if (WKStringIsEqualToUTF8CString(messageBodyString, "BeginTest")) {
            m_gotInitialResponse = true;
            TestController::shared().notifyDone();
            return;
        }

        ASSERT_NOT_REACHED();
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Done")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> textOutputKey(AdoptWK, WKStringCreateWithUTF8CString("TextOutput"));
        WKStringRef textOutput = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, textOutputKey.get()));

        WKRetainPtr<WKStringRef> pixelResultKey = adoptWK(WKStringCreateWithUTF8CString("PixelResult"));
        WKImageRef pixelResult = static_cast<WKImageRef>(WKDictionaryGetItemForKey(messageBodyDictionary, pixelResultKey.get()));
        ASSERT(!pixelResult || m_dumpPixels);
        
        WKRetainPtr<WKStringRef> repaintRectsKey = adoptWK(WKStringCreateWithUTF8CString("RepaintRects"));
        WKArrayRef repaintRects = static_cast<WKArrayRef>(WKDictionaryGetItemForKey(messageBodyDictionary, repaintRectsKey.get()));        

        // Dump text.
        dump(toWTFString(textOutput).utf8().data(), true);

        // Dump pixels (if necessary).
        if (m_dumpPixels && pixelResult)
            dumpPixelsAndCompareWithExpected(pixelResult, repaintRects);

        fputs("#EOF\n", stdout);
        fflush(stdout);
        fflush(stderr);
        
        m_gotFinalMessage = true;
        TestController::shared().notifyDone();
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "BeforeUnloadReturnValue")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef beforeUnloadReturnValue = static_cast<WKBooleanRef>(messageBody);
        TestController::shared().setBeforeUnloadReturnValue(WKBooleanGetValue(beforeUnloadReturnValue));
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "AddChromeInputField")) {
        TestController::shared().mainWebView()->addChromeInputField();
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallAddChromeInputFieldCallback"));
        WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), 0);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveChromeInputField")) {
        TestController::shared().mainWebView()->removeChromeInputField();
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallRemoveChromeInputFieldCallback"));
        WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), 0);
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "FocusWebView")) {
        TestController::shared().mainWebView()->makeWebViewFirstResponder();
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallFocusWebViewCallback"));
        WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), 0);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetBackingScaleFactor")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        double backingScaleFactor = WKDoubleGetValue(static_cast<WKDoubleRef>(messageBody));
        WKPageSetCustomBackingScaleFactor(TestController::shared().mainWebView()->page(), backingScaleFactor);

        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallSetBackingScaleFactorCallback"));
        WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), 0);
        return;
    }

    ASSERT_NOT_REACHED();
}

WKRetainPtr<WKTypeRef> TestInvocation::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "SetWindowIsKey")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef isKeyValue = static_cast<WKBooleanRef>(messageBody);
        TestController::shared().mainWebView()->setWindowIsKey(WKBooleanGetValue(isKeyValue));
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WTR
