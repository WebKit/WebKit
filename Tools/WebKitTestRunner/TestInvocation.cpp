/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "UIScriptController.h"
#include "WebCoreTestSupport.h"
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKCookieManager.h>
#include <WebKit/WKData.h>
#include <WebKit/WKDictionary.h>
#include <WebKit/WKInspector.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <climits>
#include <cstdio>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
#include <Carbon/Carbon.h>
#endif

#if PLATFORM(COCOA)
#include <WebKit/WKPagePrivateMac.h>
#endif

#if PLATFORM(WIN)
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif

using namespace JSC;
using namespace WebKit;
using namespace std;

namespace WTR {

TestInvocation::TestInvocation(WKURLRef url, const TestOptions& options)
    : m_options(options)
    , m_url(url)
{
    WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyString(m_url.get()));

    size_t stringLength = WKStringGetLength(urlString.get());

    Vector<char> urlVector;
    urlVector.resize(stringLength + 1);

    WKStringGetUTF8CString(urlString.get(), urlVector.data(), stringLength + 1);

    m_urlString = String(urlVector.data(), stringLength);

    // FIXME: Avoid mutating the setting via a test directory like this.
    m_dumpFrameLoadCallbacks = urlContains("loading/");
}

TestInvocation::~TestInvocation()
{
    if (m_pendingUIScriptInvocationData)
        m_pendingUIScriptInvocationData->testInvocation = nullptr;
}

WKURLRef TestInvocation::url() const
{
    return m_url.get();
}

bool TestInvocation::urlContains(const char* searchString) const
{
    return m_urlString.containsIgnoringASCIICase(searchString);
}

void TestInvocation::setIsPixelTest(const std::string& expectedPixelHash)
{
    m_dumpPixels = true;
    m_expectedPixelHash = expectedPixelHash;
}

WTF::Seconds TestInvocation::shortTimeout() const
{
    if (!m_timeout) {
        // Running WKTR directly, without webkitpy.
        return TestController::defaultShortTimeout;
    }

    // This is not exactly correct for the way short timeout is used - it should not depend on whether a test is "slow",
    // but it currently does. There is no way to know what a normal test's timeout is, as webkitpy only passes timeouts
    // for each test individually.
    // But there shouldn't be any observable negative consequences from this.
    return m_timeout / 4;
}

bool TestInvocation::shouldLogHistoryClientCallbacks() const
{
    return urlContains("globalhistory/");
}

WKRetainPtr<WKMutableDictionaryRef> TestInvocation::createTestSettingsDictionary()
{
    WKRetainPtr<WKMutableDictionaryRef> beginTestMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> useFlexibleViewportKey = adoptWK(WKStringCreateWithUTF8CString("UseFlexibleViewport"));
    WKRetainPtr<WKBooleanRef> useFlexibleViewportValue = adoptWK(WKBooleanCreate(options().useFlexibleViewport));
    WKDictionarySetItem(beginTestMessageBody.get(), useFlexibleViewportKey.get(), useFlexibleViewportValue.get());

    WKRetainPtr<WKStringRef> dumpPixelsKey = adoptWK(WKStringCreateWithUTF8CString("DumpPixels"));
    WKRetainPtr<WKBooleanRef> dumpPixelsValue = adoptWK(WKBooleanCreate(m_dumpPixels));
    WKDictionarySetItem(beginTestMessageBody.get(), dumpPixelsKey.get(), dumpPixelsValue.get());

    WKRetainPtr<WKStringRef> useWaitToDumpWatchdogTimerKey = adoptWK(WKStringCreateWithUTF8CString("UseWaitToDumpWatchdogTimer"));
    WKRetainPtr<WKBooleanRef> useWaitToDumpWatchdogTimerValue = adoptWK(WKBooleanCreate(TestController::singleton().useWaitToDumpWatchdogTimer()));
    WKDictionarySetItem(beginTestMessageBody.get(), useWaitToDumpWatchdogTimerKey.get(), useWaitToDumpWatchdogTimerValue.get());

    WKRetainPtr<WKStringRef> timeoutKey = adoptWK(WKStringCreateWithUTF8CString("Timeout"));
    WKRetainPtr<WKUInt64Ref> timeoutValue = adoptWK(WKUInt64Create(m_timeout.milliseconds()));
    WKDictionarySetItem(beginTestMessageBody.get(), timeoutKey.get(), timeoutValue.get());

    WKRetainPtr<WKStringRef> dumpJSConsoleLogInStdErrKey = adoptWK(WKStringCreateWithUTF8CString("DumpJSConsoleLogInStdErr"));
    WKRetainPtr<WKBooleanRef> dumpJSConsoleLogInStdErrValue = adoptWK(WKBooleanCreate(m_dumpJSConsoleLogInStdErr));
    WKDictionarySetItem(beginTestMessageBody.get(), dumpJSConsoleLogInStdErrKey.get(), dumpJSConsoleLogInStdErrValue.get());
    
    return beginTestMessageBody;
}

void TestInvocation::invoke()
{
    TestController::singleton().configureViewForTest(*this);

    WKPageSetAddsVisitedLinks(TestController::singleton().mainWebView()->page(), false);

    m_textOutput.clear();

    TestController::singleton().setShouldLogHistoryClientCallbacks(shouldLogHistoryClientCallbacks());

    WKCookieManagerSetHTTPCookieAcceptPolicy(WKContextGetCookieManager(TestController::singleton().context()), kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain);

    // FIXME: We should clear out visited links here.

    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("BeginTest"));
    auto beginTestMessageBody = createTestSettingsDictionary();
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), beginTestMessageBody.get());

    m_startedTesting = true;

    bool shouldOpenExternalURLs = false;

    TestController::singleton().runUntil(m_gotInitialResponse, TestController::noTimeout);
    if (m_error)
        goto end;

    WKPageLoadURLWithShouldOpenExternalURLsPolicy(TestController::singleton().mainWebView()->page(), m_url.get(), shouldOpenExternalURLs);

    TestController::singleton().runUntil(m_gotFinalMessage, TestController::noTimeout);
    if (m_error)
        goto end;

    dumpResults();

end:
#if !PLATFORM(IOS_FAMILY)
    if (m_gotInitialResponse)
        WKInspectorClose(WKPageGetInspector(TestController::singleton().mainWebView()->page()));
#endif // !PLATFORM(IOS_FAMILY)

    if (TestController::singleton().resetStateToConsistentValues(m_options, TestController::ResetStage::AfterTest))
        return;

    // The process is unresponsive, so let's start a new one.
    TestController::singleton().terminateWebContentProcess();
    // Make sure that we have a process, as invoke() will need one to send bundle messages for the next test.
    TestController::singleton().reattachPageToWebProcess();
}

void TestInvocation::dumpWebProcessUnresponsiveness(const char* errorMessage)
{
    fprintf(stderr, "%s", errorMessage);
    char buffer[1024] = { };
#if PLATFORM(COCOA)
    pid_t pid = WKPageGetProcessIdentifier(TestController::singleton().mainWebView()->page());
    snprintf(buffer, sizeof(buffer), "#PROCESS UNRESPONSIVE - %s (pid %ld)\n", TestController::webProcessName(), static_cast<long>(pid));
#else
    snprintf(buffer, sizeof(buffer), "#PROCESS UNRESPONSIVE - %s\n", TestController::webProcessName());
#endif

    dump(errorMessage, buffer, true);
    
    if (!TestController::singleton().usingServerMode())
        return;
    
    if (isatty(fileno(stdin)) || isatty(fileno(stderr)))
        fputs("Grab an image of the stack, then hit enter...\n", stderr);
    
    if (!fgets(buffer, sizeof(buffer), stdin) || strcmp(buffer, "#SAMPLE FINISHED\n"))
        fprintf(stderr, "Failed receive expected sample response, got:\n\t\"%s\"\nContinuing...\n", buffer);
}

void TestInvocation::dump(const char* textToStdout, const char* textToStderr, bool seenError)
{
    printf("Content-Type: text/plain\n");
    if (textToStdout)
        fputs(textToStdout, stdout);
    if (textToStderr)
        fputs(textToStderr, stderr);

    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stderr);
    if (seenError)
        fputs("#EOF\n", stdout);
    fflush(stdout);
    fflush(stderr);
}

void TestInvocation::forceRepaintDoneCallback(WKErrorRef error, void* context)
{
    // The context may not be valid any more, e.g. if WebKit is invalidating callbacks at process exit.
    if (error)
        return;

    TestInvocation* testInvocation = static_cast<TestInvocation*>(context);
    RELEASE_ASSERT(TestController::singleton().isCurrentInvocation(testInvocation));

    testInvocation->m_gotRepaint = true;
    TestController::singleton().notifyDone();
}

void TestInvocation::dumpResults()
{
    if (m_shouldDumpResourceLoadStatistics)
        m_textOutput.append(m_savedResourceLoadStatistics.isNull() ? TestController::singleton().dumpResourceLoadStatistics() : m_savedResourceLoadStatistics);

    if (m_textOutput.length() || !m_audioResult)
        dump(m_textOutput.toString().utf8().data());
    else
        dumpAudio(m_audioResult.get());

    if (m_dumpPixels) {
        if (m_pixelResult)
            dumpPixelsAndCompareWithExpected(SnapshotResultType::WebContents, m_repaintRects.get(), m_pixelResult.get());
        else if (m_pixelResultIsPending) {
            m_gotRepaint = false;
            WKPageForceRepaint(TestController::singleton().mainWebView()->page(), this, TestInvocation::forceRepaintDoneCallback);
            TestController::singleton().runUntil(m_gotRepaint, TestController::noTimeout);
            dumpPixelsAndCompareWithExpected(SnapshotResultType::WebView, m_repaintRects.get());
        }
    }

    fputs("#EOF\n", stdout);
    fflush(stdout);
    fflush(stderr);
}

void TestInvocation::dumpAudio(WKDataRef audioData)
{
    size_t length = WKDataGetSize(audioData);
    if (!length)
        return;

    const unsigned char* data = WKDataGetBytes(audioData);

    printf("Content-Type: audio/wav\n");
    printf("Content-Length: %lu\n", static_cast<unsigned long>(length));

    fwrite(data, 1, length, stdout);
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
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
        TestController::singleton().notifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Ack")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef messageBodyString = static_cast<WKStringRef>(messageBody);
        if (WKStringIsEqualToUTF8CString(messageBodyString, "BeginTest")) {
            m_gotInitialResponse = true;
            TestController::singleton().notifyDone();
            return;
        }

        ASSERT_NOT_REACHED();
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Done")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> pixelResultIsPendingKey = adoptWK(WKStringCreateWithUTF8CString("PixelResultIsPending"));
        WKBooleanRef pixelResultIsPending = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, pixelResultIsPendingKey.get()));
        m_pixelResultIsPending = WKBooleanGetValue(pixelResultIsPending);

        if (!m_pixelResultIsPending) {
            WKRetainPtr<WKStringRef> pixelResultKey = adoptWK(WKStringCreateWithUTF8CString("PixelResult"));
            m_pixelResult = static_cast<WKImageRef>(WKDictionaryGetItemForKey(messageBodyDictionary, pixelResultKey.get()));
            ASSERT(!m_pixelResult || m_dumpPixels);
        }

        WKRetainPtr<WKStringRef> repaintRectsKey = adoptWK(WKStringCreateWithUTF8CString("RepaintRects"));
        m_repaintRects = static_cast<WKArrayRef>(WKDictionaryGetItemForKey(messageBodyDictionary, repaintRectsKey.get()));

        WKRetainPtr<WKStringRef> audioResultKey =  adoptWK(WKStringCreateWithUTF8CString("AudioResult"));
        m_audioResult = static_cast<WKDataRef>(WKDictionaryGetItemForKey(messageBodyDictionary, audioResultKey.get()));

        m_gotFinalMessage = true;
        TestController::singleton().notifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TextOutput")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef textOutput = static_cast<WKStringRef>(messageBody);
        m_textOutput.append(toWTFString(textOutput));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DumpToStdErr")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef textOutput = static_cast<WKStringRef>(messageBody);
        fprintf(stderr, "%s", toWTFString(textOutput).utf8().data());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "BeforeUnloadReturnValue")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef beforeUnloadReturnValue = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setBeforeUnloadReturnValue(WKBooleanGetValue(beforeUnloadReturnValue));
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "AddChromeInputField")) {
        TestController::singleton().mainWebView()->addChromeInputField();
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallAddChromeInputFieldCallback"));
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveChromeInputField")) {
        TestController::singleton().mainWebView()->removeChromeInputField();
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallRemoveChromeInputFieldCallback"));
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "FocusWebView")) {
        TestController::singleton().mainWebView()->makeWebViewFirstResponder();
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallFocusWebViewCallback"));
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetBackingScaleFactor")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        double backingScaleFactor = WKDoubleGetValue(static_cast<WKDoubleRef>(messageBody));
        WKPageSetCustomBackingScaleFactor(TestController::singleton().mainWebView()->page(), backingScaleFactor);

        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallSetBackingScaleFactorCallback"));
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SimulateWebNotificationClick")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        uint64_t notificationID = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        TestController::singleton().simulateWebNotificationClick(notificationID);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAddsVisitedLinks")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef enabledWK = static_cast<WKBooleanRef>(messageBody);
        WKPageSetAddsVisitedLinks(TestController::singleton().mainWebView()->page(), WKBooleanGetValue(enabledWK));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetGeolocationPermission")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef enabledWK = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setGeolocationPermission(WKBooleanGetValue(enabledWK));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGeolocationPosition")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> latitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("latitude"));
        WKDoubleRef latitudeWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, latitudeKeyWK.get()));
        double latitude = WKDoubleGetValue(latitudeWK);

        WKRetainPtr<WKStringRef> longitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("longitude"));
        WKDoubleRef longitudeWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, longitudeKeyWK.get()));
        double longitude = WKDoubleGetValue(longitudeWK);

        WKRetainPtr<WKStringRef> accuracyKeyWK(AdoptWK, WKStringCreateWithUTF8CString("accuracy"));
        WKDoubleRef accuracyWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, accuracyKeyWK.get()));
        double accuracy = WKDoubleGetValue(accuracyWK);

        WKRetainPtr<WKStringRef> providesAltitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesAltitude"));
        WKBooleanRef providesAltitudeWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesAltitudeKeyWK.get()));
        bool providesAltitude = WKBooleanGetValue(providesAltitudeWK);

        WKRetainPtr<WKStringRef> altitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("altitude"));
        WKDoubleRef altitudeWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, altitudeKeyWK.get()));
        double altitude = WKDoubleGetValue(altitudeWK);

        WKRetainPtr<WKStringRef> providesAltitudeAccuracyKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesAltitudeAccuracy"));
        WKBooleanRef providesAltitudeAccuracyWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesAltitudeAccuracyKeyWK.get()));
        bool providesAltitudeAccuracy = WKBooleanGetValue(providesAltitudeAccuracyWK);

        WKRetainPtr<WKStringRef> altitudeAccuracyKeyWK(AdoptWK, WKStringCreateWithUTF8CString("altitudeAccuracy"));
        WKDoubleRef altitudeAccuracyWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, altitudeAccuracyKeyWK.get()));
        double altitudeAccuracy = WKDoubleGetValue(altitudeAccuracyWK);

        WKRetainPtr<WKStringRef> providesHeadingKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesHeading"));
        WKBooleanRef providesHeadingWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesHeadingKeyWK.get()));
        bool providesHeading = WKBooleanGetValue(providesHeadingWK);

        WKRetainPtr<WKStringRef> headingKeyWK(AdoptWK, WKStringCreateWithUTF8CString("heading"));
        WKDoubleRef headingWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, headingKeyWK.get()));
        double heading = WKDoubleGetValue(headingWK);

        WKRetainPtr<WKStringRef> providesSpeedKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesSpeed"));
        WKBooleanRef providesSpeedWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesSpeedKeyWK.get()));
        bool providesSpeed = WKBooleanGetValue(providesSpeedWK);

        WKRetainPtr<WKStringRef> speedKeyWK(AdoptWK, WKStringCreateWithUTF8CString("speed"));
        WKDoubleRef speedWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, speedKeyWK.get()));
        double speed = WKDoubleGetValue(speedWK);

        WKRetainPtr<WKStringRef> providesFloorLevelKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesFloorLevel"));
        WKBooleanRef providesFloorLevelWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesFloorLevelKeyWK.get()));
        bool providesFloorLevel = WKBooleanGetValue(providesFloorLevelWK);

        WKRetainPtr<WKStringRef> floorLevelKeyWK(AdoptWK, WKStringCreateWithUTF8CString("floorLevel"));
        WKDoubleRef floorLevelWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, floorLevelKeyWK.get()));
        double floorLevel = WKDoubleGetValue(floorLevelWK);

        TestController::singleton().setMockGeolocationPosition(latitude, longitude, accuracy, providesAltitude, altitude, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed, providesFloorLevel, floorLevel);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGeolocationPositionUnavailableError")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef errorMessage = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setMockGeolocationPositionUnavailableError(errorMessage);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetUserMediaPermission")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef enabledWK = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setUserMediaPermission(WKBooleanGetValue(enabledWK));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetUserMediaPermission")) {
        TestController::singleton().resetUserMediaPermission();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetUserMediaPersistentPermissionForOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> permissionKeyWK(AdoptWK, WKStringCreateWithUTF8CString("permission"));
        WKBooleanRef permissionWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, permissionKeyWK.get()));
        bool permission = WKBooleanGetValue(permissionWK);

        WKRetainPtr<WKStringRef> originKey(AdoptWK, WKStringCreateWithUTF8CString("origin"));
        WKStringRef originWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, originKey.get()));

        WKRetainPtr<WKStringRef> parentOriginKey(AdoptWK, WKStringCreateWithUTF8CString("parentOrigin"));
        WKStringRef parentOriginWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, parentOriginKey.get()));

        TestController::singleton().setUserMediaPersistentPermissionForOrigin(permission, originWK, parentOriginWK);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetUserMediaPermissionRequestCountForOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> originKey(AdoptWK, WKStringCreateWithUTF8CString("origin"));
        WKStringRef originWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, originKey.get()));

        WKRetainPtr<WKStringRef> parentOriginKey(AdoptWK, WKStringCreateWithUTF8CString("parentOrigin"));
        WKStringRef parentOriginWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, parentOriginKey.get()));
        
        TestController::singleton().resetUserMediaPermissionRequestCountForOrigin(originWK, parentOriginWK);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCacheModel")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        uint64_t model = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        WKContextSetCacheModel(TestController::singleton().context(), model);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCustomPolicyDelegate")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> enabledKeyWK(AdoptWK, WKStringCreateWithUTF8CString("enabled"));
        WKBooleanRef enabledWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, enabledKeyWK.get()));
        bool enabled = WKBooleanGetValue(enabledWK);

        WKRetainPtr<WKStringRef> permissiveKeyWK(AdoptWK, WKStringCreateWithUTF8CString("permissive"));
        WKBooleanRef permissiveWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, permissiveKeyWK.get()));
        bool permissive = WKBooleanGetValue(permissiveWK);

        TestController::singleton().setCustomPolicyDelegate(enabled, permissive);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetHidden")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> isInitialKeyWK(AdoptWK, WKStringCreateWithUTF8CString("hidden"));
        WKBooleanRef hiddenWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, isInitialKeyWK.get()));
        bool hidden = WKBooleanGetValue(hiddenWK);

        TestController::singleton().setHidden(hidden);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ProcessWorkQueue")) {
        if (TestController::singleton().workQueueManager().processWorkQueue()) {
            WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("WorkQueueProcessedCallback"));
            WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
        }
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueBackNavigation")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        uint64_t stepCount = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        TestController::singleton().workQueueManager().queueBackNavigation(stepCount);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueForwardNavigation")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        uint64_t stepCount = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        TestController::singleton().workQueueManager().queueForwardNavigation(stepCount);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoad")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef loadDataDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> urlKey(AdoptWK, WKStringCreateWithUTF8CString("url"));
        WKStringRef urlWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, urlKey.get()));

        WKRetainPtr<WKStringRef> targetKey(AdoptWK, WKStringCreateWithUTF8CString("target"));
        WKStringRef targetWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, targetKey.get()));

        WKRetainPtr<WKStringRef> shouldOpenExternalURLsKey(AdoptWK, WKStringCreateWithUTF8CString("shouldOpenExternalURLs"));
        WKBooleanRef shouldOpenExternalURLsValueWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(loadDataDictionary, shouldOpenExternalURLsKey.get()));

        TestController::singleton().workQueueManager().queueLoad(toWTFString(urlWK), toWTFString(targetWK), WKBooleanGetValue(shouldOpenExternalURLsValueWK));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoadHTMLString")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef loadDataDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> contentKey(AdoptWK, WKStringCreateWithUTF8CString("content"));
        WKStringRef contentWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, contentKey.get()));

        WKRetainPtr<WKStringRef> baseURLKey(AdoptWK, WKStringCreateWithUTF8CString("baseURL"));
        WKStringRef baseURLWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, baseURLKey.get()));

        WKRetainPtr<WKStringRef> unreachableURLKey(AdoptWK, WKStringCreateWithUTF8CString("unreachableURL"));
        WKStringRef unreachableURLWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, unreachableURLKey.get()));

        TestController::singleton().workQueueManager().queueLoadHTMLString(toWTFString(contentWK), baseURLWK ? toWTFString(baseURLWK) : String(), unreachableURLWK ? toWTFString(unreachableURLWK) : String());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueReload")) {
        TestController::singleton().workQueueManager().queueReload();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoadingScript")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef script = static_cast<WKStringRef>(messageBody);
        TestController::singleton().workQueueManager().queueLoadingScript(toWTFString(script));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueNonLoadingScript")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef script = static_cast<WKStringRef>(messageBody);
        TestController::singleton().workQueueManager().queueNonLoadingScript(toWTFString(script));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetRejectsProtectionSpaceAndContinueForAuthenticationChallenges")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetHandlesAuthenticationChallenges")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setHandlesAuthenticationChallenges(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldLogCanAuthenticateAgainstProtectionSpace")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldLogCanAuthenticateAgainstProtectionSpace(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldLogDownloadCallbacks")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldLogDownloadCallbacks(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAuthenticationUsername")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef username = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setAuthenticationUsername(toWTFString(username));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAuthenticationPassword")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef password = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setAuthenticationPassword(toWTFString(password));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetBlockAllPlugins")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef shouldBlock = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setBlockAllPlugins(WKBooleanGetValue(shouldBlock));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPluginSupportedMode")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef mode = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setPluginSupportedMode(toWTFString(mode));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDecideNavigationPolicyAfterDelay")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldDecideNavigationPolicyAfterDelay(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDecideResponsePolicyAfterDelay")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldDecideResponsePolicyAfterDelay(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetNavigationGesturesEnabled")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setNavigationGesturesEnabled(WKBooleanGetValue(value));
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetIgnoresViewportScaleLimits")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setIgnoresViewportScaleLimits(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDownloadUndisplayableMIMETypes")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldDownloadUndisplayableMIMETypes(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RunUIProcessScript")) {
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> scriptKey(AdoptWK, WKStringCreateWithUTF8CString("Script"));
        WKRetainPtr<WKStringRef> callbackIDKey(AdoptWK, WKStringCreateWithUTF8CString("CallbackID"));

        UIScriptInvocationData* invocationData = new UIScriptInvocationData();
        invocationData->testInvocation = this;
        invocationData->callbackID = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, callbackIDKey.get())));
        invocationData->scriptString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, scriptKey.get()));
        m_pendingUIScriptInvocationData = invocationData;
        WKPageCallAfterNextPresentationUpdate(TestController::singleton().mainWebView()->page(), invocationData, runUISideScriptAfterUpdateCallback);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOpenPanelFileURLs")) {
        TestController::singleton().setOpenPanelFileURLs(static_cast<WKArrayRef>(messageBody));
        return;
    }

    ASSERT_NOT_REACHED();
}

WKRetainPtr<WKTypeRef> TestInvocation::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "Initialization")) {
        auto settings = createTestSettingsDictionary();
        WKRetainPtr<WKStringRef> resumeTestingKey = adoptWK(WKStringCreateWithUTF8CString("ResumeTesting"));
        WKRetainPtr<WKBooleanRef> resumeTestingValue = adoptWK(WKBooleanCreate(m_startedTesting));
        WKDictionarySetItem(settings.get(), resumeTestingKey.get(), resumeTestingValue.get());
        return settings;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetDumpPixels")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        m_dumpPixels = WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetDumpPixels"))
        return WKRetainPtr<WKTypeRef>(AdoptWK, WKBooleanCreate(m_dumpPixels));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWhatToDump")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        m_whatToDump = static_cast<WhatToDump>(WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody)));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetWhatToDump"))
        return WKRetainPtr<WKTypeRef>(AdoptWK, WKUInt64Create(static_cast<uint64_t>(m_whatToDump)));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWaitUntilDone")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        m_waitUntilDone = static_cast<unsigned char>(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetWaitUntilDone"))
        return WKRetainPtr<WKTypeRef>(AdoptWK, WKBooleanCreate(m_waitUntilDone));

    if (WKStringIsEqualToUTF8CString(messageName, "SetDumpFrameLoadCallbacks")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        m_dumpFrameLoadCallbacks = static_cast<unsigned char>(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetDumpFrameLoadCallbacks"))
        return WKRetainPtr<WKTypeRef>(AdoptWK, WKBooleanCreate(m_dumpFrameLoadCallbacks));

    if (WKStringIsEqualToUTF8CString(messageName, "SetCanOpenWindows")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        m_canOpenWindows = static_cast<unsigned char>(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetWindowIsKey")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef isKeyValue = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().mainWebView()->setWindowIsKey(WKBooleanGetValue(isKeyValue));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetViewSize")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> widthKey(AdoptWK, WKStringCreateWithUTF8CString("width"));
        WKRetainPtr<WKStringRef> heightKey(AdoptWK, WKStringCreateWithUTF8CString("height"));

        WKDoubleRef widthWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, widthKey.get()));
        WKDoubleRef heightWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, heightKey.get()));

        TestController::singleton().mainWebView()->resizeTo(WKDoubleGetValue(widthWK), WKDoubleGetValue(heightWK));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsGeolocationClientActive")) {
        bool isActive = TestController::singleton().isGeolocationProviderActive();
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isActive));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsWorkQueueEmpty")) {
        bool isEmpty = TestController::singleton().workQueueManager().isWorkQueueEmpty();
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isEmpty));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DidReceiveServerRedirectForProvisionalNavigation")) {
        WKRetainPtr<WKBooleanRef> result(AdoptWK, WKBooleanCreate(TestController::singleton().didReceiveServerRedirectForProvisionalNavigation()));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDidReceiveServerRedirectForProvisionalNavigation")) {
        TestController::singleton().clearDidReceiveServerRedirectForProvisionalNavigation();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SecureEventInputIsEnabled")) {
#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
        WKRetainPtr<WKBooleanRef> result(AdoptWK, WKBooleanCreate(IsSecureEventInputEnabled()));
#else
        WKRetainPtr<WKBooleanRef> result(AdoptWK, WKBooleanCreate(false));
#endif
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAlwaysAcceptCookies")) {
        WKBooleanRef accept = static_cast<WKBooleanRef>(messageBody);
        WKHTTPCookieAcceptPolicy policy = WKBooleanGetValue(accept) ? kWKHTTPCookieAcceptPolicyAlways : kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        // FIXME: This updates the policy in WebProcess and in NetworkProcess asynchronously, which might break some tests' expectations.
        WKCookieManagerSetHTTPCookieAcceptPolicy(WKContextGetCookieManager(TestController::singleton().context()), policy);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOnlyAcceptFirstPartyCookies")) {
        WKBooleanRef accept = static_cast<WKBooleanRef>(messageBody);
        WKHTTPCookieAcceptPolicy policy = WKBooleanGetValue(accept) ? kWKHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain : kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        // FIXME: This updates the policy in WebProcess and in NetworkProcess asynchronously, which might break some tests' expectations.
        WKCookieManagerSetHTTPCookieAcceptPolicy(WKContextGetCookieManager(TestController::singleton().context()), policy);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetCustomUserAgent")) {
        WKStringRef userAgent = static_cast<WKStringRef>(messageBody);
        WKPageSetCustomUserAgent(TestController::singleton().mainWebView()->page(), userAgent);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStorageAccessAPIEnabled")) {
        WKBooleanRef accept = static_cast<WKBooleanRef>(messageBody);
        WKCookieManagerSetStorageAccessAPIEnabled(WKContextGetCookieManager(TestController::singleton().context()), WKBooleanGetValue(accept));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetAllStorageAccessEntries")) {
        TestController::singleton().getAllStorageAccessEntries();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowsAnySSLCertificate")) {
        TestController::singleton().setAllowsAnySSLCertificate(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ImageCountInGeneralPasteboard")) {
        unsigned count = TestController::singleton().imageCountInGeneralPasteboard();
        WKRetainPtr<WKUInt64Ref> result(AdoptWK, WKUInt64Create(count));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "DeleteAllIndexedDatabases")) {
        WKWebsiteDataStoreRemoveAllIndexedDatabases(WKContextGetWebsiteDataStore(TestController::singleton().context()));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AddMockMediaDevice")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> persistentIDKey(AdoptWK, WKStringCreateWithUTF8CString("PersistentID"));
        WKRetainPtr<WKStringRef> labelKey(AdoptWK, WKStringCreateWithUTF8CString("Label"));
        WKRetainPtr<WKStringRef> typeKey(AdoptWK, WKStringCreateWithUTF8CString("Type"));

        auto persistentID = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, persistentIDKey.get()));
        auto label = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, labelKey.get()));
        auto type = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, typeKey.get()));

        TestController::singleton().addMockMediaDevice(persistentID, label, type);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearMockMediaDevices")) {
        TestController::singleton().clearMockMediaDevices();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveMockMediaDevice")) {
        WKStringRef persistentId = static_cast<WKStringRef>(messageBody);

        TestController::singleton().removeMockMediaDevice(persistentId);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetMockMediaDevices")) {
        TestController::singleton().resetMockMediaDevices();
        return nullptr;
    }

#if PLATFORM(MAC)
    if (WKStringIsEqualToUTF8CString(messageName, "ConnectMockGamepad")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());

        uint64_t index = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        WebCoreTestSupport::connectMockGamepad(index);
        
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DisconnectMockGamepad")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());

        uint64_t index = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        WebCoreTestSupport::disconnectMockGamepad(index);

        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadDetails")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> gamepadIndexKey(AdoptWK, WKStringCreateWithUTF8CString("GamepadIndex"));
        WKRetainPtr<WKStringRef> gamepadIDKey(AdoptWK, WKStringCreateWithUTF8CString("GamepadID"));
        WKRetainPtr<WKStringRef> axisCountKey(AdoptWK, WKStringCreateWithUTF8CString("AxisCount"));
        WKRetainPtr<WKStringRef> buttonCountKey(AdoptWK, WKStringCreateWithUTF8CString("ButtonCount"));

        WKUInt64Ref gamepadIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIndexKey.get()));
        WKStringRef gamepadID = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIDKey.get()));
        WKUInt64Ref axisCount = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, axisCountKey.get()));
        WKUInt64Ref buttonCount = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonCountKey.get()));

        WebCoreTestSupport::setMockGamepadDetails(WKUInt64GetValue(gamepadIndex), toWTFString(gamepadID), WKUInt64GetValue(axisCount), WKUInt64GetValue(buttonCount));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadAxisValue")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> gamepadIndexKey(AdoptWK, WKStringCreateWithUTF8CString("GamepadIndex"));
        WKRetainPtr<WKStringRef> axisIndexKey(AdoptWK, WKStringCreateWithUTF8CString("AxisIndex"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));

        WKUInt64Ref gamepadIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIndexKey.get()));
        WKUInt64Ref axisIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, axisIndexKey.get()));
        WKDoubleRef value = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        WebCoreTestSupport::setMockGamepadAxisValue(WKUInt64GetValue(gamepadIndex), WKUInt64GetValue(axisIndex), WKDoubleGetValue(value));

        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadButtonValue")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> gamepadIndexKey(AdoptWK, WKStringCreateWithUTF8CString("GamepadIndex"));
        WKRetainPtr<WKStringRef> buttonIndexKey(AdoptWK, WKStringCreateWithUTF8CString("ButtonIndex"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));

        WKUInt64Ref gamepadIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIndexKey.get()));
        WKUInt64Ref buttonIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonIndexKey.get()));
        WKDoubleRef value = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        WebCoreTestSupport::setMockGamepadButtonValue(WKUInt64GetValue(gamepadIndex), WKUInt64GetValue(buttonIndex), WKDoubleGetValue(value));

        return nullptr;
    }
#endif // PLATFORM(MAC)

    if (WKStringIsEqualToUTF8CString(messageName, "UserMediaPermissionRequestCountForOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> originKey(AdoptWK, WKStringCreateWithUTF8CString("origin"));
        WKStringRef originWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, originKey.get()));

        WKRetainPtr<WKStringRef> parentOriginKey(AdoptWK, WKStringCreateWithUTF8CString("parentOrigin"));
        WKStringRef parentOriginWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, parentOriginKey.get()));
        
        unsigned count = TestController::singleton().userMediaPermissionRequestCountForOrigin(originWK, parentOriginWK);
        WKRetainPtr<WKUInt64Ref> result(AdoptWK, WKUInt64Create(count));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsDebugMode")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsDebugMode(WKBooleanGetValue(value));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsPrevalentResourceForDebugMode")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setStatisticsPrevalentResourceForDebugMode(hostName);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsLastSeen")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKDoubleRef value = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));
        
        TestController::singleton().setStatisticsLastSeen(hostName, WKDoubleGetValue(value));
        
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        TestController::singleton().setStatisticsPrevalentResource(hostName, WKBooleanGetValue(value));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsVeryPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));
        
        TestController::singleton().setStatisticsVeryPrevalentResource(hostName, WKBooleanGetValue(value));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "dumpResourceLoadStatistics")) {
        dumpResourceLoadStatistics();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());

        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool isPrevalent = TestController::singleton().isStatisticsPrevalentResource(hostName);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isPrevalent));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsVeryPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool isPrevalent = TestController::singleton().isStatisticsVeryPrevalentResource(hostName);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isPrevalent));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsSubresourceUnder")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> subresourceHostKey(AdoptWK, WKStringCreateWithUTF8CString("SubresourceHost"));
        WKRetainPtr<WKStringRef> topFrameHostKey(AdoptWK, WKStringCreateWithUTF8CString("TopFrameHost"));
        
        WKStringRef subresourceHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subresourceHostKey.get()));
        WKStringRef topFrameHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostKey.get()));
        
        bool isRegisteredAsSubresourceUnder = TestController::singleton().isStatisticsRegisteredAsSubresourceUnder(subresourceHost, topFrameHost);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isRegisteredAsSubresourceUnder));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsSubFrameUnder")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> subFrameHostKey(AdoptWK, WKStringCreateWithUTF8CString("SubFrameHost"));
        WKRetainPtr<WKStringRef> topFrameHostKey(AdoptWK, WKStringCreateWithUTF8CString("TopFrameHost"));
        
        WKStringRef subFrameHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subFrameHostKey.get()));
        WKStringRef topFrameHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostKey.get()));

        bool isRegisteredAsSubFrameUnder = TestController::singleton().isStatisticsRegisteredAsSubFrameUnder(subFrameHost, topFrameHost);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isRegisteredAsSubFrameUnder));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsRedirectingTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostRedirectedFromKey(AdoptWK, WKStringCreateWithUTF8CString("HostRedirectedFrom"));
        WKRetainPtr<WKStringRef> hostRedirectedToKey(AdoptWK, WKStringCreateWithUTF8CString("HostRedirectedTo"));
        
        WKStringRef hostRedirectedFrom = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostRedirectedFromKey.get()));
        WKStringRef hostRedirectedTo = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostRedirectedToKey.get()));
        
        bool isRegisteredAsRedirectingTo = TestController::singleton().isStatisticsRegisteredAsRedirectingTo(hostRedirectedFrom, hostRedirectedTo);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isRegisteredAsRedirectingTo));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsHasHadUserInteraction")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));
        
        TestController::singleton().setStatisticsHasHadUserInteraction(hostName, WKBooleanGetValue(value));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsHasHadUserInteraction")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool hasHadUserInteraction = TestController::singleton().isStatisticsHasHadUserInteraction(hostName);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(hasHadUserInteraction));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsGrandfathered")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey(AdoptWK, WKStringCreateWithUTF8CString("Value"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));
        
        TestController::singleton().setStatisticsGrandfathered(hostName, WKBooleanGetValue(value));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsGrandfathered")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool isGrandfathered = TestController::singleton().isStatisticsGrandfathered(hostName);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(isGrandfathered));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubframeUnderTopFrameOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> topFrameHostNameKey(AdoptWK, WKStringCreateWithUTF8CString("TopFrameHostName"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef topFrameHostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostNameKey.get()));

        TestController::singleton().setStatisticsSubframeUnderTopFrameOrigin(hostName, topFrameHostName);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUnderTopFrameOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> topFrameHostNameKey(AdoptWK, WKStringCreateWithUTF8CString("TopFrameHostName"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef topFrameHostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostNameKey.get()));
        
        TestController::singleton().setStatisticsSubresourceUnderTopFrameOrigin(hostName, topFrameHostName);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUniqueRedirectTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedToKey(AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedTo"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedTo = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedToKey.get()));
        
        TestController::singleton().setStatisticsSubresourceUniqueRedirectTo(hostName, hostNameRedirectedTo);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUniqueRedirectFrom")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedFromKey(AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedFrom"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedFrom = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedFromKey.get()));
        
        TestController::singleton().setStatisticsSubresourceUniqueRedirectFrom(hostName, hostNameRedirectedFrom);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTopFrameUniqueRedirectTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedToKey(AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedTo"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedTo = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedToKey.get()));
        
        TestController::singleton().setStatisticsTopFrameUniqueRedirectTo(hostName, hostNameRedirectedTo);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTopFrameUniqueRedirectFrom")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey(AdoptWK, WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedFromKey(AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedFrom"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedFrom = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedFromKey.get()));
        
        TestController::singleton().setStatisticsTopFrameUniqueRedirectFrom(hostName, hostNameRedirectedFrom);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTimeToLiveUserInteraction")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        WKDoubleRef seconds = static_cast<WKDoubleRef>(messageBody);
        TestController::singleton().setStatisticsTimeToLiveUserInteraction(WKDoubleGetValue(seconds));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsProcessStatisticsAndDataRecords")) {
        TestController::singleton().statisticsProcessStatisticsAndDataRecords();
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsUpdateCookieBlocking")) {
        TestController::singleton().statisticsUpdateCookieBlocking();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSubmitTelemetry")) {
        TestController::singleton().statisticsSubmitTelemetry();
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsNotifyPagesWhenDataRecordsWereScanned")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsNotifyPagesWhenDataRecordsWereScanned(WKBooleanGetValue(value));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsNotifyPagesWhenTelemetryWasCaptured")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsNotifyPagesWhenTelemetryWasCaptured(WKBooleanGetValue(value));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsShouldClassifyResourcesBeforeDataRecordsRemoval")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(WKBooleanGetValue(value));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsMinimumTimeBetweenDataRecordsRemoval")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        WKDoubleRef seconds = static_cast<WKDoubleRef>(messageBody);
        TestController::singleton().setStatisticsMinimumTimeBetweenDataRecordsRemoval(WKDoubleGetValue(seconds));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsGrandfatheringTime")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        WKDoubleRef seconds = static_cast<WKDoubleRef>(messageBody);
        TestController::singleton().setStatisticsGrandfatheringTime(WKDoubleGetValue(seconds));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetMaxStatisticsEntries")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        WKUInt64Ref entries = static_cast<WKUInt64Ref>(messageBody);
        TestController::singleton().setStatisticsMaxStatisticsEntries(WKUInt64GetValue(entries));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetPruneEntriesDownTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        WKUInt64Ref entries = static_cast<WKUInt64Ref>(messageBody);
        TestController::singleton().setStatisticsPruneEntriesDownTo(WKUInt64GetValue(entries));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearInMemoryAndPersistentStore")) {
        TestController::singleton().statisticsClearInMemoryAndPersistentStore();
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearInMemoryAndPersistentStoreModifiedSinceHours")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        WKUInt64Ref hours = static_cast<WKUInt64Ref>(messageBody);
        TestController::singleton().statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(WKUInt64GetValue(hours));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearThroughWebsiteDataRemoval")) {
        TestController::singleton().statisticsClearThroughWebsiteDataRemoval();
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsCacheMaxAgeCap")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        WKDoubleRef seconds = static_cast<WKDoubleRef>(messageBody);
        TestController::singleton().setStatisticsCacheMaxAgeCap(WKDoubleGetValue(seconds));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsResetToConsistentState")) {
        if (m_shouldDumpResourceLoadStatistics)
            m_savedResourceLoadStatistics = TestController::singleton().dumpResourceLoadStatistics();
        TestController::singleton().statisticsResetToConsistentState();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveAllSessionCredentials")) {
        TestController::singleton().removeAllSessionCredentials();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDOMCache")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef origin = static_cast<WKStringRef>(messageBody);

        TestController::singleton().clearDOMCache(origin);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDOMCaches")) {
        TestController::singleton().clearDOMCaches();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "HasDOMCache")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef origin = static_cast<WKStringRef>(messageBody);

        bool hasDOMCache = TestController::singleton().hasDOMCache(origin);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(hasDOMCache));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DOMCacheSize")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef origin = static_cast<WKStringRef>(messageBody);

        auto domCacheSize = TestController::singleton().domCacheSize(origin);
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKUInt64Create(domCacheSize));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AllowCacheStorageQuotaIncrease")) {
        TestController::singleton().allowCacheStorageQuotaIncrease();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetIDBPerOriginQuota")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        WKUInt64Ref quota = static_cast<WKUInt64Ref>(messageBody);
        TestController::singleton().setIDBPerOriginQuota(WKUInt64GetValue(quota));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "InjectUserScript")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef script = static_cast<WKStringRef>(messageBody);

        TestController::singleton().injectUserScript(script);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetApplicationManifest")) {
#ifdef __BLOCKS__
        WKPageGetApplicationManifest_b(TestController::singleton().mainWebView()->page(), ^{
            WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("DidGetApplicationManifest"));
            WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
        });
#else
        // FIXME: Add API for loading the manifest on non-__BLOCKS__ ports.
        ASSERT_NOT_REACHED();
#endif
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SendDisplayConfigurationChangedMessageForTesting")) {
        TestController::singleton().sendDisplayConfigurationChangedMessageForTesting();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateNetworkProcess")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateNetworkProcess();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateServiceWorkerProcess")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateServiceWorkerProcess();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetWebAuthenticationMockConfiguration")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        TestController::singleton().setWebAuthenticationMockConfiguration(static_cast<WKDictionaryRef>(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AddTestKeyToKeychain")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef testKeyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> privateKeyKey(AdoptWK, WKStringCreateWithUTF8CString("PrivateKey"));
        WKStringRef privateKeyWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testKeyDictionary, privateKeyKey.get()));

        WKRetainPtr<WKStringRef> attrLabelKey(AdoptWK, WKStringCreateWithUTF8CString("AttrLabel"));
        WKStringRef attrLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testKeyDictionary, attrLabelKey.get()));

        WKRetainPtr<WKStringRef> applicationTagKey(AdoptWK, WKStringCreateWithUTF8CString("ApplicationTag"));
        WKStringRef applicationTagWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testKeyDictionary, applicationTagKey.get()));

        TestController::singleton().addTestKeyToKeychain(toWTFString(privateKeyWK), toWTFString(attrLabelWK), toWTFString(applicationTagWK));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CleanUpKeychain")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        TestController::singleton().cleanUpKeychain(toWTFString(static_cast<WKStringRef>(messageBody)));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "KeyExistsInKeychain")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef testDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> attrLabelKey(AdoptWK, WKStringCreateWithUTF8CString("AttrLabel"));
        WKStringRef attrLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testDictionary, attrLabelKey.get()));

        WKRetainPtr<WKStringRef> applicationTagKey(AdoptWK, WKStringCreateWithUTF8CString("ApplicationTag"));
        WKStringRef applicationTagWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testDictionary, applicationTagKey.get()));

        bool keyExistsInKeychain = TestController::singleton().keyExistsInKeychain(toWTFString(attrLabelWK), toWTFString(applicationTagWK));
        WKRetainPtr<WKTypeRef> result(AdoptWK, WKBooleanCreate(keyExistsInKeychain));
        return result;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void TestInvocation::runUISideScriptAfterUpdateCallback(WKErrorRef, void* context)
{
    UIScriptInvocationData* data = static_cast<UIScriptInvocationData*>(context);
    if (TestInvocation* invocation = data->testInvocation) {
        RELEASE_ASSERT(TestController::singleton().isCurrentInvocation(invocation));
        invocation->runUISideScript(data->scriptString.get(), data->callbackID);
    }
    delete data;
}

void TestInvocation::runUISideScript(WKStringRef script, unsigned scriptCallbackID)
{
    m_pendingUIScriptInvocationData = nullptr;

    if (!m_UIScriptContext)
        m_UIScriptContext = std::make_unique<UIScriptContext>(*this);
    
    m_UIScriptContext->runUIScript(toWTFString(script), scriptCallbackID);
}

void TestInvocation::uiScriptDidComplete(const String& result, unsigned scriptCallbackID)
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallUISideScriptCallback"));

    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());
    WKRetainPtr<WKStringRef> resultKey(AdoptWK, WKStringCreateWithUTF8CString("Result"));
    WKRetainPtr<WKStringRef> callbackIDKey(AdoptWK, WKStringCreateWithUTF8CString("CallbackID"));
    WKRetainPtr<WKUInt64Ref> callbackIDValue = adoptWK(WKUInt64Create(scriptCallbackID));

    WKDictionarySetItem(messageBody.get(), resultKey.get(), toWK(result).get());
    WKDictionarySetItem(messageBody.get(), callbackIDKey.get(), callbackIDValue.get());

    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), messageBody.get());
}

void TestInvocation::outputText(const WTF::String& text)
{
    m_textOutput.append(text);
}

void TestInvocation::didBeginSwipe()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidBeginSwipeCallback"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::willEndSwipe()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallWillEndSwipeCallback"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didEndSwipe()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidEndSwipeCallback"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didRemoveSwipeSnapshot()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidRemoveSwipeSnapshotCallback"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::notifyDownloadDone()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("NotifyDownloadDone"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didClearStatisticsThroughWebsiteDataRemoval()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidClearStatisticsThroughWebsiteDataRemoval"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didResetStatisticsToConsistentState()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidResetStatisticsToConsistentState"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetBlockCookiesForHost()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetBlockCookiesForHost"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetStatisticsDebugMode()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetStatisticsDebugMode"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetPrevalentResourceForDebugMode()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetPrevalentResourceForDebugMode"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetLastSeen()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetLastSeen"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetPrevalentResource()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetPrevalentResource"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetVeryPrevalentResource()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetVeryPrevalentResource"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetHasHadUserInteraction()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetHasHadUserInteraction"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didReceiveAllStorageAccessEntries(Vector<String>& domains)
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidReceiveAllStorageAccessEntries"));
    
    WKRetainPtr<WKMutableArrayRef> messageBody(AdoptWK, WKMutableArrayCreate());
    for (auto& domain : domains)
        WKArrayAppendItem(messageBody.get(), adoptWK(WKStringCreateWithUTF8CString(domain.utf8().data())).get());
    
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), messageBody.get());
}

void TestInvocation::didRemoveAllSessionCredentials()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidRemoveAllSessionCredentialsCallback"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::dumpResourceLoadStatistics()
{
    m_shouldDumpResourceLoadStatistics = true;
}

} // namespace WTR
