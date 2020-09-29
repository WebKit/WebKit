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
#include <WebKit/WKData.h>
#include <WebKit/WKDictionary.h>
#include <WebKit/WKHTTPCookieStoreRef.h>
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
    , m_waitToDumpWatchdogTimer(RunLoop::main(), this, &TestInvocation::waitToDumpWatchdogTimerFired)
{
    WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyString(m_url.get()));

    size_t stringLength = WKStringGetLength(urlString.get());

    Vector<char> urlVector;
    urlVector.resize(stringLength + 1);

    WKStringGetUTF8CString(urlString.get(), urlVector.data(), stringLength + 1);

    m_urlString = String(urlVector.data(), stringLength);

    // FIXME: Avoid mutating the setting via a test directory like this.
    m_dumpFrameLoadCallbacks = urlContains("loading/") && !urlContains("://localhost");
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

    WKRetainPtr<WKStringRef> timeoutKey = adoptWK(WKStringCreateWithUTF8CString("Timeout"));
    WKRetainPtr<WKUInt64Ref> timeoutValue = adoptWK(WKUInt64Create(m_timeout.milliseconds()));
    WKDictionarySetItem(beginTestMessageBody.get(), timeoutKey.get(), timeoutValue.get());

    WKRetainPtr<WKStringRef> dumpJSConsoleLogInStdErrKey = adoptWK(WKStringCreateWithUTF8CString("DumpJSConsoleLogInStdErr"));
    WKRetainPtr<WKBooleanRef> dumpJSConsoleLogInStdErrValue = adoptWK(WKBooleanCreate(m_dumpJSConsoleLogInStdErr));
    WKDictionarySetItem(beginTestMessageBody.get(), dumpJSConsoleLogInStdErrKey.get(), dumpJSConsoleLogInStdErrValue.get());

    WKRetainPtr<WKStringRef> additionalSupportedImageTypesKey = adoptWK(WKStringCreateWithUTF8CString("additionalSupportedImageTypes"));
    WKRetainPtr<WKStringRef> additionalSupportedImageTypesValue = adoptWK(WKStringCreateWithUTF8CString(options().additionalSupportedImageTypes.c_str()));
    WKDictionarySetItem(beginTestMessageBody.get(), additionalSupportedImageTypesKey.get(), additionalSupportedImageTypesValue.get());

    return beginTestMessageBody;
}

void TestInvocation::invoke()
{
    TestController::singleton().configureViewForTest(*this);

    WKPageSetAddsVisitedLinks(TestController::singleton().mainWebView()->page(), false);

    m_textOutput.clear();

    TestController::singleton().setShouldLogHistoryClientCallbacks(shouldLogHistoryClientCallbacks());

    WKHTTPCookieStoreSetHTTPCookieAcceptPolicy(WKWebsiteDataStoreGetHTTPCookieStore(TestController::singleton().websiteDataStore()), kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain, nullptr, nullptr);

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

    if (m_shouldDumpAdClickAttribution)
        m_textOutput.append(TestController::singleton().dumpAdClickAttribution());
    
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

        done();
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

        WKRetainPtr<WKStringRef> latitudeKeyWK = adoptWK(WKStringCreateWithUTF8CString("latitude"));
        WKDoubleRef latitudeWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, latitudeKeyWK.get()));
        double latitude = WKDoubleGetValue(latitudeWK);

        WKRetainPtr<WKStringRef> longitudeKeyWK = adoptWK(WKStringCreateWithUTF8CString("longitude"));
        WKDoubleRef longitudeWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, longitudeKeyWK.get()));
        double longitude = WKDoubleGetValue(longitudeWK);

        WKRetainPtr<WKStringRef> accuracyKeyWK = adoptWK(WKStringCreateWithUTF8CString("accuracy"));
        WKDoubleRef accuracyWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, accuracyKeyWK.get()));
        double accuracy = WKDoubleGetValue(accuracyWK);

        WKRetainPtr<WKStringRef> providesAltitudeKeyWK = adoptWK(WKStringCreateWithUTF8CString("providesAltitude"));
        WKBooleanRef providesAltitudeWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesAltitudeKeyWK.get()));
        bool providesAltitude = WKBooleanGetValue(providesAltitudeWK);

        WKRetainPtr<WKStringRef> altitudeKeyWK = adoptWK(WKStringCreateWithUTF8CString("altitude"));
        WKDoubleRef altitudeWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, altitudeKeyWK.get()));
        double altitude = WKDoubleGetValue(altitudeWK);

        WKRetainPtr<WKStringRef> providesAltitudeAccuracyKeyWK = adoptWK(WKStringCreateWithUTF8CString("providesAltitudeAccuracy"));
        WKBooleanRef providesAltitudeAccuracyWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesAltitudeAccuracyKeyWK.get()));
        bool providesAltitudeAccuracy = WKBooleanGetValue(providesAltitudeAccuracyWK);

        WKRetainPtr<WKStringRef> altitudeAccuracyKeyWK = adoptWK(WKStringCreateWithUTF8CString("altitudeAccuracy"));
        WKDoubleRef altitudeAccuracyWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, altitudeAccuracyKeyWK.get()));
        double altitudeAccuracy = WKDoubleGetValue(altitudeAccuracyWK);

        WKRetainPtr<WKStringRef> providesHeadingKeyWK = adoptWK(WKStringCreateWithUTF8CString("providesHeading"));
        WKBooleanRef providesHeadingWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesHeadingKeyWK.get()));
        bool providesHeading = WKBooleanGetValue(providesHeadingWK);

        WKRetainPtr<WKStringRef> headingKeyWK = adoptWK(WKStringCreateWithUTF8CString("heading"));
        WKDoubleRef headingWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, headingKeyWK.get()));
        double heading = WKDoubleGetValue(headingWK);

        WKRetainPtr<WKStringRef> providesSpeedKeyWK = adoptWK(WKStringCreateWithUTF8CString("providesSpeed"));
        WKBooleanRef providesSpeedWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesSpeedKeyWK.get()));
        bool providesSpeed = WKBooleanGetValue(providesSpeedWK);

        WKRetainPtr<WKStringRef> speedKeyWK = adoptWK(WKStringCreateWithUTF8CString("speed"));
        WKDoubleRef speedWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, speedKeyWK.get()));
        double speed = WKDoubleGetValue(speedWK);

        WKRetainPtr<WKStringRef> providesFloorLevelKeyWK = adoptWK(WKStringCreateWithUTF8CString("providesFloorLevel"));
        WKBooleanRef providesFloorLevelWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, providesFloorLevelKeyWK.get()));
        bool providesFloorLevel = WKBooleanGetValue(providesFloorLevelWK);

        WKRetainPtr<WKStringRef> floorLevelKeyWK = adoptWK(WKStringCreateWithUTF8CString("floorLevel"));
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

        WKRetainPtr<WKStringRef> permissionKeyWK = adoptWK(WKStringCreateWithUTF8CString("permission"));
        WKBooleanRef permissionWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, permissionKeyWK.get()));
        bool permission = WKBooleanGetValue(permissionWK);

        WKRetainPtr<WKStringRef> originKey = adoptWK(WKStringCreateWithUTF8CString("origin"));
        WKStringRef originWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, originKey.get()));

        WKRetainPtr<WKStringRef> parentOriginKey = adoptWK(WKStringCreateWithUTF8CString("parentOrigin"));
        WKStringRef parentOriginWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, parentOriginKey.get()));

        TestController::singleton().setUserMediaPersistentPermissionForOrigin(permission, originWK, parentOriginWK);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetUserMediaPermissionRequestCountForOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> originKey = adoptWK(WKStringCreateWithUTF8CString("origin"));
        WKStringRef originWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, originKey.get()));

        WKRetainPtr<WKStringRef> parentOriginKey = adoptWK(WKStringCreateWithUTF8CString("parentOrigin"));
        WKStringRef parentOriginWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, parentOriginKey.get()));
        
        TestController::singleton().resetUserMediaPermissionRequestCountForOrigin(originWK, parentOriginWK);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCustomPolicyDelegate")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> enabledKeyWK = adoptWK(WKStringCreateWithUTF8CString("enabled"));
        WKBooleanRef enabledWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, enabledKeyWK.get()));
        bool enabled = WKBooleanGetValue(enabledWK);

        WKRetainPtr<WKStringRef> permissiveKeyWK = adoptWK(WKStringCreateWithUTF8CString("permissive"));
        WKBooleanRef permissiveWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, permissiveKeyWK.get()));
        bool permissive = WKBooleanGetValue(permissiveWK);

        TestController::singleton().setCustomPolicyDelegate(enabled, permissive);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetHidden")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> isInitialKeyWK = adoptWK(WKStringCreateWithUTF8CString("hidden"));
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

        WKRetainPtr<WKStringRef> urlKey = adoptWK(WKStringCreateWithUTF8CString("url"));
        WKStringRef urlWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, urlKey.get()));

        WKRetainPtr<WKStringRef> targetKey = adoptWK(WKStringCreateWithUTF8CString("target"));
        WKStringRef targetWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, targetKey.get()));

        WKRetainPtr<WKStringRef> shouldOpenExternalURLsKey = adoptWK(WKStringCreateWithUTF8CString("shouldOpenExternalURLs"));
        WKBooleanRef shouldOpenExternalURLsValueWK = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(loadDataDictionary, shouldOpenExternalURLsKey.get()));

        TestController::singleton().workQueueManager().queueLoad(toWTFString(urlWK), toWTFString(targetWK), WKBooleanGetValue(shouldOpenExternalURLsValueWK));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoadHTMLString")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef loadDataDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> contentKey = adoptWK(WKStringCreateWithUTF8CString("content"));
        WKStringRef contentWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, contentKey.get()));

        WKRetainPtr<WKStringRef> baseURLKey = adoptWK(WKStringCreateWithUTF8CString("baseURL"));
        WKStringRef baseURLWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(loadDataDictionary, baseURLKey.get()));

        WKRetainPtr<WKStringRef> unreachableURLKey = adoptWK(WKStringCreateWithUTF8CString("unreachableURL"));
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

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldAllowDeviceOrientationAndMotionAccess")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldAllowDeviceOrientationAndMotionAccess(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsShouldBlockThirdPartyCookiesOnSitesWithoutUserInteraction")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsShouldBlockThirdPartyCookies(WKBooleanGetValue(value), true);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsShouldBlockThirdPartyCookies")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsShouldBlockThirdPartyCookies(WKBooleanGetValue(value), false);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RunUIProcessScript")) {
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> scriptKey = adoptWK(WKStringCreateWithUTF8CString("Script"));
        WKRetainPtr<WKStringRef> callbackIDKey = adoptWK(WKStringCreateWithUTF8CString("CallbackID"));

        UIScriptInvocationData* invocationData = new UIScriptInvocationData();
        invocationData->testInvocation = this;
        invocationData->callbackID = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, callbackIDKey.get())));
        invocationData->scriptString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, scriptKey.get()));
        m_pendingUIScriptInvocationData = invocationData;
        WKPageCallAfterNextPresentationUpdate(TestController::singleton().mainWebView()->page(), invocationData, runUISideScriptAfterUpdateCallback);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RunUIProcessScriptImmediately")) {
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> scriptKey = adoptWK(WKStringCreateWithUTF8CString("Script"));
        WKRetainPtr<WKStringRef> callbackIDKey = adoptWK(WKStringCreateWithUTF8CString("CallbackID"));

        UIScriptInvocationData* invocationData = new UIScriptInvocationData();
        invocationData->testInvocation = this;
        invocationData->callbackID = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, callbackIDKey.get())));
        invocationData->scriptString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, scriptKey.get()));
        m_pendingUIScriptInvocationData = invocationData;
        runUISideScriptImmediately(nullptr, invocationData);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "InstallCustomMenuAction")) {
        auto messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> nameKey = adoptWK(WKStringCreateWithUTF8CString("name"));
        WKRetainPtr<WKStringRef> name = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, nameKey.get()));
        WKRetainPtr<WKStringRef> dismissesAutomaticallyKey = adoptWK(WKStringCreateWithUTF8CString("dismissesAutomatically"));
        auto dismissesAutomatically = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, dismissesAutomaticallyKey.get()));
        TestController::singleton().installCustomMenuAction(toWTFString(name.get()), WKBooleanGetValue(dismissesAutomatically));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowedMenuActions")) {
        auto messageBodyArray = static_cast<WKArrayRef>(messageBody);
        auto size = WKArrayGetSize(messageBodyArray);
        Vector<String> actions;
        actions.reserveInitialCapacity(size);
        for (size_t index = 0; index < size; ++index)
            actions.append(toWTFString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(messageBodyArray, index))));
        TestController::singleton().setAllowedMenuActions(actions);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOpenPanelFileURLs")) {
        TestController::singleton().setOpenPanelFileURLs(static_cast<WKArrayRef>(messageBody));
        return;
    }

#if PLATFORM(IOS_FAMILY)
    if (WKStringIsEqualToUTF8CString(messageName, "SetOpenPanelFileURLsMediaIcon")) {
        TestController::singleton().setOpenPanelFileURLsMediaIcon(static_cast<WKDataRef>(messageBody));
        return;
    }
#endif
    
    if (WKStringIsEqualToUTF8CString(messageName, "LoadedSubresourceDomains")) {
        TestController::singleton().loadedSubresourceDomains();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsDebugMode")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsDebugMode(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsPrevalentResourceForDebugMode")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setStatisticsPrevalentResourceForDebugMode(hostName);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsLastSeen")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKDoubleRef value = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        TestController::singleton().setStatisticsLastSeen(hostName, WKDoubleGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsMergeStatistic")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> topFrameDomain1Key = adoptWK(WKStringCreateWithUTF8CString("TopFrameDomain1"));
        WKRetainPtr<WKStringRef> topFrameDomain2Key = adoptWK(WKStringCreateWithUTF8CString("TopFrameDomain2"));
        WKRetainPtr<WKStringRef> lastSeenKey = adoptWK(WKStringCreateWithUTF8CString("LastSeen"));
        WKRetainPtr<WKStringRef> hadUserInteractionKey = adoptWK(WKStringCreateWithUTF8CString("HadUserInteraction"));
        WKRetainPtr<WKStringRef> mostRecentUserInteractionKey = adoptWK(WKStringCreateWithUTF8CString("MostRecentUserInteraction"));
        WKRetainPtr<WKStringRef> isGrandfatheredKey = adoptWK(WKStringCreateWithUTF8CString("IsGrandfathered"));
        WKRetainPtr<WKStringRef> isPrevalentKey = adoptWK(WKStringCreateWithUTF8CString("IsPrevalent"));
        WKRetainPtr<WKStringRef> isVeryPrevalentKey = adoptWK(WKStringCreateWithUTF8CString("IsVeryPrevalent"));
        WKRetainPtr<WKStringRef> dataRecordsRemovedKey = adoptWK(WKStringCreateWithUTF8CString("DataRecordsRemoved"));
        WKRetainPtr<WKStringRef> timesAccessedFirstPartyInteractionKey = adoptWK(WKStringCreateWithUTF8CString("TimesAccessedFirstPartyInteraction"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef topFrameDomain1 = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameDomain1Key.get()));
        WKStringRef topFrameDomain2 = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameDomain2Key.get()));
        WKDoubleRef lastSeen = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, lastSeenKey.get()));
        WKBooleanRef hadUserInteraction = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hadUserInteractionKey.get()));
        WKDoubleRef mostRecentUserInteraction = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, mostRecentUserInteractionKey.get()));
        WKBooleanRef isGrandfathered = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, isGrandfatheredKey.get()));
        WKBooleanRef isPrevalent = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, isPrevalentKey.get()));
        WKBooleanRef isVeryPrevalent = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, isVeryPrevalentKey.get()));
        WKUInt64Ref dataRecordsRemoved = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, dataRecordsRemovedKey.get()));

        TestController::singleton().setStatisticsMergeStatistic(hostName, topFrameDomain1, topFrameDomain2, WKDoubleGetValue(lastSeen), WKBooleanGetValue(hadUserInteraction), WKDoubleGetValue(mostRecentUserInteraction), WKBooleanGetValue(isGrandfathered), WKBooleanGetValue(isPrevalent), WKBooleanGetValue(isVeryPrevalent), WKUInt64GetValue(dataRecordsRemoved));
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsExpiredStatistic")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hadUserInteractionKey = adoptWK(WKStringCreateWithUTF8CString("HadUserInteraction"));
        WKRetainPtr<WKStringRef> mostRecentUserInteractionKey = adoptWK(WKStringCreateWithUTF8CString("MostRecentUserInteraction"));
        WKRetainPtr<WKStringRef> isScheduledForAllButCookieDataRemovalKey = adoptWK(WKStringCreateWithUTF8CString("IsScheduledForAllButCookieDataRemoval"));
        WKRetainPtr<WKStringRef> isPrevalentKey = adoptWK(WKStringCreateWithUTF8CString("IsPrevalent"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef hadUserInteraction = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hadUserInteractionKey.get()));
        WKBooleanRef isScheduledForAllButCookieDataRemoval = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, isScheduledForAllButCookieDataRemovalKey.get()));
        WKBooleanRef isPrevalent = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, isPrevalentKey.get()));

        TestController::singleton().setStatisticsExpiredStatistic(hostName, WKBooleanGetValue(hadUserInteraction), WKBooleanGetValue(isScheduledForAllButCookieDataRemoval), WKBooleanGetValue(isPrevalent));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        TestController::singleton().setStatisticsPrevalentResource(hostName, WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsVeryPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        TestController::singleton().setStatisticsVeryPrevalentResource(hostName, WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearInMemoryAndPersistentStore")) {
        TestController::singleton().statisticsClearInMemoryAndPersistentStore();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearThroughWebsiteDataRemoval")) {
        TestController::singleton().statisticsClearThroughWebsiteDataRemoval();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearInMemoryAndPersistentStoreModifiedSinceHours")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        WKUInt64Ref hours = static_cast<WKUInt64Ref>(messageBody);
        TestController::singleton().statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(WKUInt64GetValue(hours));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsShouldDowngradeReferrer")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsShouldDowngradeReferrer(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsFirstPartyWebsiteDataRemovalMode")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsFirstPartyWebsiteDataRemovalMode(WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetToSameSiteStrictCookies")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setStatisticsToSameSiteStrictCookies(hostName);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetFirstPartyHostCNAMEDomain")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> firstPartyURLStringKey = adoptWK(WKStringCreateWithUTF8CString("FirstPartyURL"));
        WKRetainPtr<WKStringRef> cnameURLStringKey = adoptWK(WKStringCreateWithUTF8CString("CNAME"));
        
        WKStringRef firstPartyURLString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, firstPartyURLStringKey.get()));
        WKStringRef cnameURLString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, cnameURLStringKey.get()));
        
        TestController::singleton().setStatisticsFirstPartyHostCNAMEDomain(firstPartyURLString, cnameURLString);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetThirdPartyCNAMEDomain")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef cnameURLString = static_cast<WKStringRef>(messageBody);
        TestController::singleton().setStatisticsThirdPartyCNAMEDomain(cnameURLString);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsResetToConsistentState")) {
        if (m_shouldDumpResourceLoadStatistics)
            m_savedResourceLoadStatistics = TestController::singleton().dumpResourceLoadStatistics();
        TestController::singleton().statisticsResetToConsistentState();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetAllStorageAccessEntries")) {
        TestController::singleton().getAllStorageAccessEntries();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveAllSessionCredentials")) {
        TestController::singleton().removeAllSessionCredentials();
        return;
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
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsHasHadUserInteraction")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));

        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        TestController::singleton().setStatisticsHasHadUserInteraction(hostName, WKBooleanGetValue(value));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsUpdateCookieBlocking")) {
        TestController::singleton().statisticsUpdateCookieBlocking();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAppBoundDomains")) {
        ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());
        WKArrayRef originURLs = static_cast<WKArrayRef>(messageBody);
        TestController::singleton().setAppBoundDomains(originURLs);
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
        return adoptWK(WKBooleanCreate(m_dumpPixels));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWhatToDump")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        m_whatToDump = static_cast<WhatToDump>(WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody)));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetWhatToDump"))
        return adoptWK(WKUInt64Create(static_cast<uint64_t>(m_whatToDump)));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWaitUntilDone")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        setWaitUntilDone(static_cast<unsigned char>(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody))));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetWaitUntilDone"))
        return adoptWK(WKBooleanCreate(m_waitUntilDone));

    if (WKStringIsEqualToUTF8CString(messageName, "SetDumpFrameLoadCallbacks")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        m_dumpFrameLoadCallbacks = static_cast<unsigned char>(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetDumpFrameLoadCallbacks"))
        return adoptWK(WKBooleanCreate(m_dumpFrameLoadCallbacks));

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
        WKRetainPtr<WKStringRef> widthKey = adoptWK(WKStringCreateWithUTF8CString("width"));
        WKRetainPtr<WKStringRef> heightKey = adoptWK(WKStringCreateWithUTF8CString("height"));

        WKDoubleRef widthWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, widthKey.get()));
        WKDoubleRef heightWK = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, heightKey.get()));

        TestController::singleton().mainWebView()->resizeTo(WKDoubleGetValue(widthWK), WKDoubleGetValue(heightWK));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsGeolocationClientActive")) {
        bool isActive = TestController::singleton().isGeolocationProviderActive();
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isActive));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCacheModel")) {
        ASSERT(WKGetTypeID(messageBody) == WKUInt64GetTypeID());
        uint64_t model = WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody));
        WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(TestController::singleton().websiteDataStore(), model);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsWorkQueueEmpty")) {
        bool isEmpty = TestController::singleton().workQueueManager().isWorkQueueEmpty();
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isEmpty));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DidReceiveServerRedirectForProvisionalNavigation")) {
        WKRetainPtr<WKBooleanRef> result = adoptWK(WKBooleanCreate(TestController::singleton().didReceiveServerRedirectForProvisionalNavigation()));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDidReceiveServerRedirectForProvisionalNavigation")) {
        TestController::singleton().clearDidReceiveServerRedirectForProvisionalNavigation();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SecureEventInputIsEnabled")) {
#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
        WKRetainPtr<WKBooleanRef> result = adoptWK(WKBooleanCreate(IsSecureEventInputEnabled()));
#else
        WKRetainPtr<WKBooleanRef> result = adoptWK(WKBooleanCreate(false));
#endif
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCustomUserAgent")) {
        WKStringRef userAgent = static_cast<WKStringRef>(messageBody);
        WKPageSetCustomUserAgent(TestController::singleton().mainWebView()->page(), userAgent);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowsAnySSLCertificate")) {
        TestController::singleton().setAllowsAnySSLCertificate(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldSwapToEphemeralSessionOnNextNavigation")) {
        TestController::singleton().setShouldSwapToEphemeralSessionOnNextNavigation(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldSwapToDefaultSessionOnNextNavigation")) {
        TestController::singleton().setShouldSwapToDefaultSessionOnNextNavigation(WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody)));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ImageCountInGeneralPasteboard")) {
        unsigned count = TestController::singleton().imageCountInGeneralPasteboard();
        WKRetainPtr<WKUInt64Ref> result = adoptWK(WKUInt64Create(count));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "DeleteAllIndexedDatabases")) {
        WKWebsiteDataStoreRemoveAllIndexedDatabases(TestController::singleton().websiteDataStore(), nullptr, { });
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AddMockMediaDevice")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> persistentIDKey = adoptWK(WKStringCreateWithUTF8CString("PersistentID"));
        WKRetainPtr<WKStringRef> labelKey = adoptWK(WKStringCreateWithUTF8CString("Label"));
        WKRetainPtr<WKStringRef> typeKey = adoptWK(WKStringCreateWithUTF8CString("Type"));

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

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockCameraOrientation")) {
        TestController::singleton().setMockCameraOrientation(WKUInt64GetValue(static_cast<WKUInt64Ref>(messageBody)));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsMockRealtimeMediaSourceCenterEnabled")) {
        bool isMockRealtimeMediaSourceCenterEnabled = TestController::singleton().isMockRealtimeMediaSourceCenterEnabled();
        return adoptWK(WKBooleanCreate(isMockRealtimeMediaSourceCenterEnabled));
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "HasAppBoundSession")) {
        bool hasAppBoundSession = TestController::singleton().hasAppBoundSession();
        return adoptWK(WKBooleanCreate(hasAppBoundSession));
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
        WKRetainPtr<WKStringRef> gamepadIndexKey = adoptWK(WKStringCreateWithUTF8CString("GamepadIndex"));
        WKRetainPtr<WKStringRef> gamepadIDKey = adoptWK(WKStringCreateWithUTF8CString("GamepadID"));
        WKRetainPtr<WKStringRef> mappingKey = adoptWK(WKStringCreateWithUTF8CString("Mapping"));
        WKRetainPtr<WKStringRef> axisCountKey = adoptWK(WKStringCreateWithUTF8CString("AxisCount"));
        WKRetainPtr<WKStringRef> buttonCountKey = adoptWK(WKStringCreateWithUTF8CString("ButtonCount"));

        WKUInt64Ref gamepadIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIndexKey.get()));
        WKStringRef gamepadID = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIDKey.get()));
        WKStringRef mapping = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, mappingKey.get()));
        WKUInt64Ref axisCount = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, axisCountKey.get()));
        WKUInt64Ref buttonCount = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonCountKey.get()));

        WebCoreTestSupport::setMockGamepadDetails(WKUInt64GetValue(gamepadIndex), toWTFString(gamepadID), toWTFString(mapping), WKUInt64GetValue(axisCount), WKUInt64GetValue(buttonCount));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadAxisValue")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> gamepadIndexKey = adoptWK(WKStringCreateWithUTF8CString("GamepadIndex"));
        WKRetainPtr<WKStringRef> axisIndexKey = adoptWK(WKStringCreateWithUTF8CString("AxisIndex"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));

        WKUInt64Ref gamepadIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, gamepadIndexKey.get()));
        WKUInt64Ref axisIndex = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, axisIndexKey.get()));
        WKDoubleRef value = static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));

        WebCoreTestSupport::setMockGamepadAxisValue(WKUInt64GetValue(gamepadIndex), WKUInt64GetValue(axisIndex), WKDoubleGetValue(value));

        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadButtonValue")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());

        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> gamepadIndexKey = adoptWK(WKStringCreateWithUTF8CString("GamepadIndex"));
        WKRetainPtr<WKStringRef> buttonIndexKey = adoptWK(WKStringCreateWithUTF8CString("ButtonIndex"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));

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

        WKRetainPtr<WKStringRef> originKey = adoptWK(WKStringCreateWithUTF8CString("origin"));
        WKStringRef originWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, originKey.get()));

        WKRetainPtr<WKStringRef> parentOriginKey = adoptWK(WKStringCreateWithUTF8CString("parentOrigin"));
        WKStringRef parentOriginWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, parentOriginKey.get()));
        
        unsigned count = TestController::singleton().userMediaPermissionRequestCountForOrigin(originWK, parentOriginWK);
        WKRetainPtr<WKUInt64Ref> result = adoptWK(WKUInt64Create(count));
        return result;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "IsDoingMediaCapture")) {
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(TestController::singleton().isDoingMediaCapture()));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "ClearStatisticsDataForDomain")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef domain = static_cast<WKStringRef>(messageBody);

        TestController::singleton().clearStatisticsDataForDomain(domain);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DoesStatisticsDomainIDExistInDatabase")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> domainIDKey = adoptWK(WKStringCreateWithUTF8CString("DomainID"));
        WKUInt64Ref domainID = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, domainIDKey.get()));
        bool domainIDExists = TestController::singleton().doesStatisticsDomainIDExistInDatabase(WKUInt64GetValue(domainID));
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(domainIDExists));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsEnabled")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsEnabled(WKBooleanGetValue(value));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsEphemeral")) {
        bool isEphemeral = TestController::singleton().isStatisticsEphemeral();
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isEphemeral));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "dumpResourceLoadStatistics")) {
        dumpResourceLoadStatistics();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());

        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool isPrevalent = TestController::singleton().isStatisticsPrevalentResource(hostName);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isPrevalent));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsVeryPrevalentResource")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool isPrevalent = TestController::singleton().isStatisticsVeryPrevalentResource(hostName);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isPrevalent));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsSubresourceUnder")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> subresourceHostKey = adoptWK(WKStringCreateWithUTF8CString("SubresourceHost"));
        WKRetainPtr<WKStringRef> topFrameHostKey = adoptWK(WKStringCreateWithUTF8CString("TopFrameHost"));
        
        WKStringRef subresourceHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subresourceHostKey.get()));
        WKStringRef topFrameHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostKey.get()));
        
        bool isRegisteredAsSubresourceUnder = TestController::singleton().isStatisticsRegisteredAsSubresourceUnder(subresourceHost, topFrameHost);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isRegisteredAsSubresourceUnder));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsSubFrameUnder")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> subFrameHostKey = adoptWK(WKStringCreateWithUTF8CString("SubFrameHost"));
        WKRetainPtr<WKStringRef> topFrameHostKey = adoptWK(WKStringCreateWithUTF8CString("TopFrameHost"));
        
        WKStringRef subFrameHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subFrameHostKey.get()));
        WKStringRef topFrameHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostKey.get()));

        bool isRegisteredAsSubFrameUnder = TestController::singleton().isStatisticsRegisteredAsSubFrameUnder(subFrameHost, topFrameHost);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isRegisteredAsSubFrameUnder));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsRedirectingTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostRedirectedFromKey = adoptWK(WKStringCreateWithUTF8CString("HostRedirectedFrom"));
        WKRetainPtr<WKStringRef> hostRedirectedToKey = adoptWK(WKStringCreateWithUTF8CString("HostRedirectedTo"));
        
        WKStringRef hostRedirectedFrom = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostRedirectedFromKey.get()));
        WKStringRef hostRedirectedTo = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostRedirectedToKey.get()));
        
        bool isRegisteredAsRedirectingTo = TestController::singleton().isStatisticsRegisteredAsRedirectingTo(hostRedirectedFrom, hostRedirectedTo);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isRegisteredAsRedirectingTo));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsHasHadUserInteraction")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool hasHadUserInteraction = TestController::singleton().isStatisticsHasHadUserInteraction(hostName);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(hasHadUserInteraction));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsOnlyInDatabaseOnce")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        auto subHostKey = adoptWK(WKStringCreateWithUTF8CString("SubHost"));
        auto topHostKey = adoptWK(WKStringCreateWithUTF8CString("TopHost"));
        
        WKStringRef subHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subHostKey.get()));
        WKStringRef topHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topHostKey.get()));

        bool statisticInDatabaseOnce = TestController::singleton().isStatisticsOnlyInDatabaseOnce(subHost, topHost);
        return adoptWK(WKBooleanCreate(statisticInDatabaseOnce));
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsGrandfathered")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("Value"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef value = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));
        
        TestController::singleton().setStatisticsGrandfathered(hostName, WKBooleanGetValue(value));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsGrandfathered")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool isGrandfathered = TestController::singleton().isStatisticsGrandfathered(hostName);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(isGrandfathered));
        return result;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubframeUnderTopFrameOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> topFrameHostNameKey = adoptWK(WKStringCreateWithUTF8CString("TopFrameHostName"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef topFrameHostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostNameKey.get()));

        TestController::singleton().setStatisticsSubframeUnderTopFrameOrigin(hostName, topFrameHostName);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUnderTopFrameOrigin")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> topFrameHostNameKey = adoptWK(WKStringCreateWithUTF8CString("TopFrameHostName"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef topFrameHostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, topFrameHostNameKey.get()));
        
        TestController::singleton().setStatisticsSubresourceUnderTopFrameOrigin(hostName, topFrameHostName);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUniqueRedirectTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedToKey = adoptWK(WKStringCreateWithUTF8CString("HostNameRedirectedTo"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedTo = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedToKey.get()));
        
        TestController::singleton().setStatisticsSubresourceUniqueRedirectTo(hostName, hostNameRedirectedTo);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUniqueRedirectFrom")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedFromKey = adoptWK(WKStringCreateWithUTF8CString("HostNameRedirectedFrom"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedFrom = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedFromKey.get()));
        
        TestController::singleton().setStatisticsSubresourceUniqueRedirectFrom(hostName, hostNameRedirectedFrom);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTopFrameUniqueRedirectTo")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedToKey = adoptWK(WKStringCreateWithUTF8CString("HostNameRedirectedTo"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedTo = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedToKey.get()));
        
        TestController::singleton().setStatisticsTopFrameUniqueRedirectTo(hostName, hostNameRedirectedTo);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTopFrameUniqueRedirectFrom")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> hostNameRedirectedFromKey = adoptWK(WKStringCreateWithUTF8CString("HostNameRedirectedFrom"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKStringRef hostNameRedirectedFrom = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameRedirectedFromKey.get()));
        
        TestController::singleton().setStatisticsTopFrameUniqueRedirectFrom(hostName, hostNameRedirectedFrom);
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsCrossSiteLoadWithLinkDecoration")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        auto fromHostKey = adoptWK(WKStringCreateWithUTF8CString("FromHost"));
        auto toHostKey = adoptWK(WKStringCreateWithUTF8CString("ToHost"));

        WKStringRef fromHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, fromHostKey.get()));
        WKStringRef toHost = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, toHostKey.get()));
        
        TestController::singleton().setStatisticsCrossSiteLoadWithLinkDecoration(fromHost, toHost);
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

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetIsRunningTest")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setStatisticsIsRunningTest(WKBooleanGetValue(value));
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

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsDeleteCookiesForHost")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> hostNameKey = adoptWK(WKStringCreateWithUTF8CString("HostName"));
        WKRetainPtr<WKStringRef> valueKey = adoptWK(WKStringCreateWithUTF8CString("IncludeHttpOnlyCookies"));
        
        WKStringRef hostName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, hostNameKey.get()));
        WKBooleanRef includeHttpOnlyCookies = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, valueKey.get()));
        
        TestController::singleton().statisticsDeleteCookiesForHost(hostName, WKBooleanGetValue(includeHttpOnlyCookies));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsHasLocalStorage")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool hasLocalStorage = TestController::singleton().isStatisticsHasLocalStorage(hostName);
        auto result = adoptWK(WKBooleanCreate(hasLocalStorage));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsCacheMaxAgeCap")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        WKDoubleRef seconds = static_cast<WKDoubleRef>(messageBody);
        TestController::singleton().setStatisticsCacheMaxAgeCap(WKDoubleGetValue(seconds));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "HasStatisticsIsolatedSession")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        
        WKStringRef hostName = static_cast<WKStringRef>(messageBody);
        bool hasIsolatedSession = TestController::singleton().hasStatisticsIsolatedSession(hostName);
        auto result = adoptWK(WKBooleanCreate(hasIsolatedSession));
        return result;
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
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(hasDOMCache));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DOMCacheSize")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef origin = static_cast<WKStringRef>(messageBody);

        auto domCacheSize = TestController::singleton().domCacheSize(origin);
        WKRetainPtr<WKTypeRef> result = adoptWK(WKUInt64Create(domCacheSize));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowStorageQuotaIncrease")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        auto canIncrease = WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody));
        TestController::singleton().setAllowStorageQuotaIncrease(canIncrease);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "InjectUserScript")) {
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef script = static_cast<WKStringRef>(messageBody);

        TestController::singleton().injectUserScript(script);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SendDisplayConfigurationChangedMessageForTesting")) {
        TestController::singleton().sendDisplayConfigurationChangedMessageForTesting();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetServiceWorkerFetchTimeout")) {
        ASSERT(WKGetTypeID(messageBody) == WKDoubleGetTypeID());
        WKDoubleRef seconds = static_cast<WKDoubleRef>(messageBody);
        TestController::singleton().setServiceWorkerFetchTimeoutForTesting(WKDoubleGetValue(seconds));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetUseSeparateServiceWorkerProcess")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        auto useSeparateServiceWorkerProcess = WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody));
        WKContextSetUseSeparateServiceWorkerProcess(TestController::singleton().context(), useSeparateServiceWorkerProcess);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateNetworkProcess")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateNetworkProcess();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateServiceWorkers")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateServiceWorkers();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AddTestKeyToKeychain")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef testKeyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> privateKeyKey = adoptWK(WKStringCreateWithUTF8CString("PrivateKey"));
        WKStringRef privateKeyWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testKeyDictionary, privateKeyKey.get()));

        WKRetainPtr<WKStringRef> attrLabelKey = adoptWK(WKStringCreateWithUTF8CString("AttrLabel"));
        WKStringRef attrLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testKeyDictionary, attrLabelKey.get()));

        WKRetainPtr<WKStringRef> applicationTagKey = adoptWK(WKStringCreateWithUTF8CString("ApplicationTag"));
        WKStringRef applicationTagWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testKeyDictionary, applicationTagKey.get()));

        TestController::singleton().addTestKeyToKeychain(toWTFString(privateKeyWK), toWTFString(attrLabelWK), toWTFString(applicationTagWK));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CleanUpKeychain")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef testDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> attrLabelKey = adoptWK(WKStringCreateWithUTF8CString("AttrLabel"));
        WKStringRef attrLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testDictionary, attrLabelKey.get()));

        WKRetainPtr<WKStringRef> applicationLabelKey = adoptWK(WKStringCreateWithUTF8CString("ApplicationLabel"));
        WKStringRef applicationLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testDictionary, applicationLabelKey.get()));

        TestController::singleton().cleanUpKeychain(toWTFString(attrLabelWK), applicationLabelWK ? toWTFString(applicationLabelWK) : String());
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "KeyExistsInKeychain")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef testDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> attrLabelKey = adoptWK(WKStringCreateWithUTF8CString("AttrLabel"));
        WKStringRef attrLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testDictionary, attrLabelKey.get()));

        WKRetainPtr<WKStringRef> applicationLabelKey = adoptWK(WKStringCreateWithUTF8CString("ApplicationLabel"));
        WKStringRef applicationLabelWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(testDictionary, applicationLabelKey.get()));

        bool keyExistsInKeychain = TestController::singleton().keyExistsInKeychain(toWTFString(attrLabelWK), toWTFString(applicationLabelWK));
        WKRetainPtr<WKTypeRef> result = adoptWK(WKBooleanCreate(keyExistsInKeychain));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ServerTrustEvaluationCallbackCallsCount")) {
        WKRetainPtr<WKTypeRef> result = adoptWK(WKUInt64Create(TestController::singleton().serverTrustEvaluationCallbackCallsCount()));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ShouldDismissJavaScriptAlertsAsynchronously")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setShouldDismissJavaScriptAlertsAsynchronously(WKBooleanGetValue(value));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AbortModal")) {
        TestController::singleton().abortModal();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DumpAdClickAttribution")) {
        dumpAdClickAttribution();
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "ClearAdClickAttribution")) {
        TestController::singleton().clearAdClickAttribution();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearAdClickAttributionsThroughWebsiteDataRemoval")) {
        TestController::singleton().clearAdClickAttributionsThroughWebsiteDataRemoval();
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "ClearAppBoundSession")) {
        TestController::singleton().clearAppBoundSession();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAdClickAttributionOverrideTimerForTesting")) {
        ASSERT(WKGetTypeID(messageBody) == WKBooleanGetTypeID());
        WKBooleanRef value = static_cast<WKBooleanRef>(messageBody);
        TestController::singleton().setAdClickAttributionOverrideTimerForTesting(WKBooleanGetValue(value));
        return nullptr;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "SetAdClickAttributionConversionURLForTesting")) {
        ASSERT(WKGetTypeID(messageBody) == WKURLGetTypeID());
        WKURLRef url = static_cast<WKURLRef>(messageBody);
        TestController::singleton().setAdClickAttributionConversionURLForTesting(url);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "MarkAdClickAttributionsAsExpiredForTesting")) {
        TestController::singleton().markAdClickAttributionsAsExpiredForTesting();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SyncLocalStorage")) {
        TestController::singleton().syncLocalStorage();
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void TestInvocation::runUISideScriptImmediately(WKErrorRef, void* context)
{
    UIScriptInvocationData* data = static_cast<UIScriptInvocationData*>(context);
    if (TestInvocation* invocation = data->testInvocation) {
        RELEASE_ASSERT(TestController::singleton().isCurrentInvocation(invocation));
        invocation->runUISideScript(data->scriptString.get(), data->callbackID);
    }
    delete data;
}

void TestInvocation::runUISideScriptAfterUpdateCallback(WKErrorRef error, void* context)
{
    runUISideScriptImmediately(error, context);
}

void TestInvocation::runUISideScript(WKStringRef script, unsigned scriptCallbackID)
{
    m_pendingUIScriptInvocationData = nullptr;

    if (!m_UIScriptContext)
        m_UIScriptContext = makeUnique<UIScriptContext>(*this, UIScriptController::create);
    
    m_UIScriptContext->runUIScript(toWTFString(script), scriptCallbackID);
}

void TestInvocation::uiScriptDidComplete(const String& result, unsigned scriptCallbackID)
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallUISideScriptCallback"));

    WKRetainPtr<WKMutableDictionaryRef> messageBody = adoptWK(WKMutableDictionaryCreate());
    WKRetainPtr<WKStringRef> resultKey = adoptWK(WKStringCreateWithUTF8CString("Result"));
    WKRetainPtr<WKStringRef> callbackIDKey = adoptWK(WKStringCreateWithUTF8CString("CallbackID"));
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

void TestInvocation::didClearStatisticsInMemoryAndPersistentStore()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidClearStatisticsInMemoryAndPersistentStore"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didClearStatisticsThroughWebsiteDataRemoval()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidClearStatisticsThroughWebsiteDataRemoval"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetShouldDowngradeReferrer()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetShouldDowngradeReferrer"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetShouldBlockThirdPartyCookies()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetShouldBlockThirdPartyCookies"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
}

void TestInvocation::didSetFirstPartyWebsiteDataRemovalMode()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetFirstPartyWebsiteDataRemovalMode"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
}

void TestInvocation::didSetToSameSiteStrictCookies()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetToSameSiteStrictCookies"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
}

void TestInvocation::didSetFirstPartyHostCNAMEDomain()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetFirstPartyHostCNAMEDomain"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
}

void TestInvocation::didSetThirdPartyCNAMEDomain()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetThirdPartyCNAMEDomain"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
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

void TestInvocation::didMergeStatistic()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidMergeStatistic"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetExpiredStatistic()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetExpiredStatistic"));
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

void TestInvocation::didReceiveAllStorageAccessEntries(Vector<String>&& domains)
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidReceiveAllStorageAccessEntries"));
    
    WKRetainPtr<WKMutableArrayRef> messageBody = adoptWK(WKMutableArrayCreate());
    for (auto& domain : domains)
        WKArrayAppendItem(messageBody.get(), adoptWK(WKStringCreateWithUTF8CString(domain.utf8().data())).get());
    
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), messageBody.get());
}

void TestInvocation::didReceiveLoadedSubresourceDomains(Vector<String>&& domains)
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidReceiveLoadedSubresourceDomains"));
    
    WKRetainPtr<WKMutableArrayRef> messageBody = adoptWK(WKMutableArrayCreate());
    for (auto& domain : domains)
        WKArrayAppendItem(messageBody.get(), adoptWK(WKStringCreateWithUTF8CString(domain.utf8().data())).get());

    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), messageBody.get());
}

void TestInvocation::didRemoveAllSessionCredentials()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidRemoveAllSessionCredentialsCallback"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::didSetAppBoundDomains()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CallDidSetAppBoundDomains"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
}

void TestInvocation::dumpResourceLoadStatistics()
{
    m_shouldDumpResourceLoadStatistics = true;
}

void TestInvocation::dumpAdClickAttribution()
{
    m_shouldDumpAdClickAttribution = true;
}

void TestInvocation::performCustomMenuAction()
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("PerformCustomMenuAction"));
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), 0);
}

void TestInvocation::initializeWaitToDumpWatchdogTimerIfNeeded()
{
    if (m_waitToDumpWatchdogTimer.isActive())
        return;

    m_waitToDumpWatchdogTimer.startOneShot(m_timeout);
}

void TestInvocation::invalidateWaitToDumpWatchdogTimer()
{
    m_waitToDumpWatchdogTimer.stop();
}

void TestInvocation::waitToDumpWatchdogTimerFired()
{
    invalidateWaitToDumpWatchdogTimer();

#if PLATFORM(COCOA)
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "#PID UNRESPONSIVE - %s (pid %d)\n", getprogname(), getpid());
    outputText(buffer);
#endif
    outputText("FAIL: Timed out waiting for notifyDone to be called\n\n");
    done();
}

void TestInvocation::setWaitUntilDone(bool waitUntilDone)
{
    m_waitUntilDone = waitUntilDone;
    if (waitUntilDone && TestController::singleton().useWaitToDumpWatchdogTimer())
        initializeWaitToDumpWatchdogTimerIfNeeded();
}

void TestInvocation::done()
{
    m_gotFinalMessage = true;
    invalidateWaitToDumpWatchdogTimer();
    RunLoop::main().dispatch([] {
        TestController::singleton().notifyDone();
    });
}

void TestInvocation::willCreateNewPage()
{
    if (m_UIScriptContext && m_UIScriptContext->callbackWithID(CallbackTypeWillCreateNewPage))
        m_UIScriptContext->fireCallback(CallbackTypeWillCreateNewPage);
}

} // namespace WTR
