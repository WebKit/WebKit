/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#include "TestRunner.h"

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSTestRunner.h"
#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestController.h"
#include <JavaScriptCore/JSCTestRunnerUtils.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundleFramePrivate.h>
#include <WebKit/WKBundleInspector.h>
#include <WebKit/WKBundleNodeHandlePrivate.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKBundleScriptWorld.h>
#include <WebKit/WKData.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSerializedScriptValue.h>
#include <WebKit/WebKit2_C.h>
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WTR {

Ref<TestRunner> TestRunner::create()
{
    return adoptRef(*new TestRunner);
}

TestRunner::TestRunner()
    : m_shouldDumpAllFrameScrollPositions(false)
    , m_shouldDumpBackForwardListsForAllWindows(false)
    , m_shouldAllowEditing(true)
    , m_shouldCloseExtraWindows(false)
    , m_dumpEditingCallbacks(false)
    , m_dumpStatusCallbacks(false)
    , m_dumpTitleChanges(false)
    , m_dumpSelectionRect(false)
    , m_dumpFullScreenCallbacks(false)
    , m_dumpProgressFinishedCallback(false)
    , m_dumpResourceLoadCallbacks(false)
    , m_dumpResourceResponseMIMETypes(false)
    , m_dumpWillCacheResponse(false)
    , m_dumpApplicationCacheDelegateCallbacks(false)
    , m_dumpDatabaseCallbacks(false)
    , m_disallowIncreaseForApplicationCacheQuota(false)
    , m_testRepaint(false)
    , m_testRepaintSweepHorizontally(false)
    , m_isPrinting(false)
    , m_willSendRequestReturnsNull(false)
    , m_willSendRequestReturnsNullOnRedirect(false)
    , m_shouldStopProvisionalFrameLoads(false)
    , m_policyDelegateEnabled(false)
    , m_policyDelegatePermissive(false)
    , m_globalFlag(false)
    , m_customFullScreenBehavior(false)
    , m_timeout(30000)
    , m_databaseDefaultQuota(-1)
    , m_databaseMaxQuota(-1)
    , m_userStyleSheetEnabled(false)
    , m_userStyleSheetLocation(adoptWK(WKStringCreateWithUTF8CString("")))
#if !PLATFORM(COCOA)
    , m_waitToDumpWatchdogTimer(RunLoop::main(), this, &TestRunner::waitToDumpWatchdogTimerFired)
#endif
{
    platformInitialize();
}

TestRunner::~TestRunner()
{
}

JSClassRef TestRunner::wrapperClass()
{
    return JSTestRunner::testRunnerClass();
}

void TestRunner::display()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundlePageForceRepaint(page);
}

void TestRunner::displayAndTrackRepaints()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundlePageForceRepaint(page);
    WKBundlePageSetTracksRepaints(page, true);
    WKBundlePageResetTrackedRepaints(page);
}

bool TestRunner::shouldDumpPixels() const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("GetDumpPixels"));
    WKTypeRef result = 0;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, &result);
    ASSERT(WKGetTypeID(result) == WKBooleanGetTypeID());
    return WKBooleanGetValue(static_cast<WKBooleanRef>(result));
}

void TestRunner::setDumpPixels(bool dumpPixels)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetDumpPixels"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(dumpPixels));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::dumpAsText(bool dumpPixels)
{
    if (whatToDump() < WhatToDump::MainFrameText)
        setWhatToDump(WhatToDump::MainFrameText);
    setDumpPixels(dumpPixels);
}
    
WhatToDump TestRunner::whatToDump() const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("GetWhatToDump"));
    WKTypeRef result = 0;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, &result);
    ASSERT(WKGetTypeID(result) == WKUInt64GetTypeID());
    auto uint64value = WKUInt64GetValue(static_cast<WKUInt64Ref>(result));
    return static_cast<WhatToDump>(uint64value);
}

void TestRunner::setWhatToDump(WhatToDump whatToDump)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetWhatToDump"));
    WKRetainPtr<WKUInt64Ref> messageBody(AdoptWK, WKUInt64Create(static_cast<uint64_t>(whatToDump)));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    m_policyDelegateEnabled = enabled;
    m_policyDelegatePermissive = permissive;

    InjectedBundle::singleton().setCustomPolicyDelegate(enabled, permissive);
}

void TestRunner::waitForPolicyDelegate()
{
    setCustomPolicyDelegate(true);
    waitUntilDone();
}

void TestRunner::waitUntilDownloadFinished()
{
    m_shouldFinishAfterDownload = true;
    waitUntilDone();
}

void TestRunner::waitUntilDone()
{
    auto& injectedBundle = InjectedBundle::singleton();
    RELEASE_ASSERT(injectedBundle.isTestRunning());

    setWaitUntilDone(true);
    // FIXME: Watchdog timer should be moved to UI process in order to take the elapsed time in anotehr process into account.
    if (injectedBundle.useWaitToDumpWatchdogTimer())
        initializeWaitToDumpWatchdogTimerIfNeeded();
}

void TestRunner::setWaitUntilDone(bool value)
{
    WKRetainPtr<WKStringRef> messsageName(AdoptWK, WKStringCreateWithUTF8CString("SetWaitUntilDone"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messsageName.get(), messageBody.get(), nullptr);
}

bool TestRunner::shouldWaitUntilDone() const
{
    WKRetainPtr<WKStringRef> messsageName(AdoptWK, WKStringCreateWithUTF8CString("GetWaitUntilDone"));
    WKTypeRef result = 0;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messsageName.get(), 0, &result);
    ASSERT(WKGetTypeID(result) == WKBooleanGetTypeID());
    return WKBooleanGetValue(static_cast<WKBooleanRef>(result));
}

void TestRunner::waitToDumpWatchdogTimerFired()
{
    invalidateWaitToDumpWatchdogTimer();
    auto& injectedBundle = InjectedBundle::singleton();
#if PLATFORM(COCOA)
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "#PID UNRESPONSIVE - %s (pid %d)\n", getprogname(), getpid());
    injectedBundle.outputText(buffer);
#endif
    injectedBundle.outputText("FAIL: Timed out waiting for notifyDone to be called\n\n");
    injectedBundle.done();
}

void TestRunner::notifyDone()
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (shouldWaitUntilDone() && !injectedBundle.topLoadingFrame())
        injectedBundle.page()->dump();

    // We don't call invalidateWaitToDumpWatchdogTimer() here, even if we continue to wait for a load to finish.
    // The test is still subject to timeout checking - it is better to detect an async timeout inside WebKitTestRunner
    // than to let webkitpy do that, because WebKitTestRunner will dump partial results.

    setWaitUntilDone(false);
}

void TestRunner::forceImmediateCompletion()
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (shouldWaitUntilDone() && injectedBundle.page())
        injectedBundle.page()->dump();

    setWaitUntilDone(false);
}

void TestRunner::setShouldDumpFrameLoadCallbacks(bool value)
{
    WKRetainPtr<WKStringRef> messsageName(AdoptWK, WKStringCreateWithUTF8CString("SetDumpFrameLoadCallbacks"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messsageName.get(), messageBody.get(), nullptr);
}

bool TestRunner::shouldDumpFrameLoadCallbacks()
{
    WKRetainPtr<WKStringRef> messsageName(AdoptWK, WKStringCreateWithUTF8CString("GetDumpFrameLoadCallbacks"));
    WKTypeRef result = 0;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messsageName.get(), 0, &result);
    ASSERT(WKGetTypeID(result) == WKBooleanGetTypeID());
    return WKBooleanGetValue(static_cast<WKBooleanRef>(result));
}

unsigned TestRunner::imageCountInGeneralPasteboard() const
{
    return InjectedBundle::singleton().imageCountInGeneralPasteboard();
}

void TestRunner::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    WKRetainPtr<WKStringRef> sourceWK = toWK(source);

    WKBundlePageAddUserScript(InjectedBundle::singleton().page()->page(), sourceWK.get(),
        (runAtStart ? kWKInjectAtDocumentStart : kWKInjectAtDocumentEnd),
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void TestRunner::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    WKRetainPtr<WKStringRef> sourceWK = toWK(source);

    WKBundlePageAddUserStyleSheet(InjectedBundle::singleton().page()->page(), sourceWK.get(),
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void TestRunner::keepWebHistory()
{
    InjectedBundle::singleton().postSetAddsVisitedLinks(true);
}

void TestRunner::execCommand(JSStringRef name, JSStringRef showUI, JSStringRef value)
{
    WKBundlePageExecuteEditingCommand(InjectedBundle::singleton().page()->page(), toWK(name).get(), toWK(value).get());
}

static std::optional<WKFindOptions> findOptionsFromArray(JSValueRef optionsArrayAsValue)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(injectedBundle.page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    auto lengthPropertyName = adopt(JSStringCreateWithUTF8CString("length"));
    JSObjectRef optionsArray = JSValueToObject(context, optionsArrayAsValue, 0);
    JSValueRef lengthValue = JSObjectGetProperty(context, optionsArray, lengthPropertyName.get(), 0);
    if (!JSValueIsNumber(context, lengthValue))
        return std::nullopt;

    WKFindOptions options = 0;
    size_t length = static_cast<size_t>(JSValueToNumber(context, lengthValue, 0));
    for (size_t i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, 0);
        if (!JSValueIsString(context, value))
            continue;

        auto optionName = adopt(JSValueToStringCopy(context, value, 0));

        if (JSStringIsEqualToUTF8CString(optionName.get(), "CaseInsensitive"))
            options |= kWKFindOptionsCaseInsensitive;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "AtWordStarts"))
            options |= kWKFindOptionsAtWordStarts;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "TreatMedialCapitalAsWordStart"))
            options |= kWKFindOptionsTreatMedialCapitalAsWordStart;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "Backwards"))
            options |= kWKFindOptionsBackwards;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "WrapAround"))
            options |= kWKFindOptionsWrapAround;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "StartInSelection")) {
            // FIXME: No kWKFindOptionsStartInSelection.
        }
    }
    return options;
}

bool TestRunner::findString(JSStringRef target, JSValueRef optionsArrayAsValue)
{
    if (auto options = findOptionsFromArray(optionsArrayAsValue))
        return WKBundlePageFindString(InjectedBundle::singleton().page()->page(), toWK(target).get(), *options);

    return false;
}

void TestRunner::findStringMatchesInPage(JSStringRef target, JSValueRef optionsArrayAsValue)
{
    if (auto options = findOptionsFromArray(optionsArrayAsValue))
        return WKBundlePageFindStringMatches(InjectedBundle::singleton().page()->page(), toWK(target).get(), *options);
}

void TestRunner::replaceFindMatchesAtIndices(JSValueRef matchIndicesAsValue, JSStringRef replacementText, bool selectionOnly)
{
    auto& bundle = InjectedBundle::singleton();
    auto mainFrame = WKBundlePageGetMainFrame(bundle.page()->page());
    auto context = WKBundleFrameGetJavaScriptContext(mainFrame);
    auto lengthPropertyName = adopt(JSStringCreateWithUTF8CString("length"));
    auto matchIndicesObject = JSValueToObject(context, matchIndicesAsValue, 0);
    auto lengthValue = JSObjectGetProperty(context, matchIndicesObject, lengthPropertyName.get(), 0);
    if (!JSValueIsNumber(context, lengthValue))
        return;

    auto indices = adoptWK(WKMutableArrayCreate());
    auto length = static_cast<size_t>(JSValueToNumber(context, lengthValue, 0));
    for (size_t i = 0; i < length; ++i) {
        auto value = JSObjectGetPropertyAtIndex(context, matchIndicesObject, i, 0);
        if (!JSValueIsNumber(context, value))
            continue;

        auto index = adoptWK(WKUInt64Create(std::round(JSValueToNumber(context, value, nullptr))));
        WKArrayAppendItem(indices.get(), index.get());
    }
    WKBundlePageReplaceStringMatches(bundle.page()->page(), indices.get(), toWK(replacementText).get(), selectionOnly);
}

void TestRunner::clearAllDatabases()
{
    WKBundleClearAllDatabases(InjectedBundle::singleton().bundle());

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("DeleteAllIndexedDatabases"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(true));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setDatabaseQuota(uint64_t quota)
{
    return WKBundleSetDatabaseQuota(InjectedBundle::singleton().bundle(), quota);
}

void TestRunner::clearAllApplicationCaches()
{
    WKBundlePageClearApplicationCache(InjectedBundle::singleton().page()->page());
}

void TestRunner::clearApplicationCacheForOrigin(JSStringRef origin)
{
    WKBundlePageClearApplicationCacheForOrigin(InjectedBundle::singleton().page()->page(), toWK(origin).get());
}

void TestRunner::setAppCacheMaximumSize(uint64_t size)
{
    WKBundlePageSetAppCacheMaximumSize(InjectedBundle::singleton().page()->page(), size);
}

long long TestRunner::applicationCacheDiskUsageForOrigin(JSStringRef origin)
{
    return WKBundlePageGetAppCacheUsageForOrigin(InjectedBundle::singleton().page()->page(), toWK(origin).get());
}

void TestRunner::disallowIncreaseForApplicationCacheQuota()
{
    m_disallowIncreaseForApplicationCacheQuota = true;
}

void TestRunner::setIDBPerOriginQuota(uint64_t quota)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetIDBPerOriginQuota"));
    WKRetainPtr<WKUInt64Ref> messageBody(AdoptWK, WKUInt64Create(quota));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

static inline JSValueRef stringArrayToJS(JSContextRef context, WKArrayRef strings)
{
    const size_t count = WKArrayGetSize(strings);

    JSValueRef arrayResult = JSObjectMakeArray(context, 0, 0, 0);
    JSObjectRef arrayObj = JSValueToObject(context, arrayResult, 0);
    for (size_t i = 0; i < count; ++i) {
        WKStringRef stringRef = static_cast<WKStringRef>(WKArrayGetItemAtIndex(strings, i));
        JSRetainPtr<JSStringRef> stringJS = toJS(stringRef);
        JSObjectSetPropertyAtIndex(context, arrayObj, i, JSValueMakeString(context, stringJS.get()), 0);
    }

    return arrayResult;
}

JSValueRef TestRunner::originsWithApplicationCache()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();

    WKRetainPtr<WKArrayRef> origins(AdoptWK, WKBundlePageCopyOriginsWithApplicationCache(page));

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    return stringArrayToJS(context, origins.get());
}

bool TestRunner::isCommandEnabled(JSStringRef name)
{
    return WKBundlePageIsEditingCommandEnabled(InjectedBundle::singleton().page()->page(), toWK(name).get());
}

void TestRunner::setCanOpenWindows()
{
    WKRetainPtr<WKStringRef> messsageName(AdoptWK, WKStringCreateWithUTF8CString("SetCanOpenWindows"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(true));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messsageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setXSSAuditorEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitXSSAuditorEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setMediaDevicesEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitMediaDevicesEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setWebRTCMDNSICECandidatesEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitWebRTCMDNSICECandidatesEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setWebRTCUnifiedPlanEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitWebRTCUnifiedPlanEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setCustomUserAgent(JSStringRef userAgent)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetCustomUserAgent"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), toWK(userAgent).get(), nullptr);
}

void TestRunner::setWebAPIStatisticsEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitWebAPIStatisticsEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setModernMediaControlsEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitModernMediaControlsEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setWebGL2Enabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitWebGL2Enabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setWebMetalEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitWebMetalEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setWritableStreamAPIEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitWritableStreamAPIEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setReadableByteStreamAPIEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitReadableByteStreamAPIEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setEncryptedMediaAPIEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitEncryptedMediaAPIEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setAllowsAnySSLCertificate(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    injectedBundle.setAllowsAnySSLCertificate(enabled);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetAllowsAnySSLCertificate"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(enabled));
    WKBundlePagePostSynchronousMessageForTesting(injectedBundle.page()->page(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetAllowUniversalAccessFromFileURLs(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::setAllowFileAccessFromFileURLs(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetAllowFileAccessFromFileURLs(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::setNeedsStorageAccessFromFileURLsQuirk(bool needsQuirk)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetAllowStorageAccessFromFileURLS(injectedBundle.bundle(), injectedBundle.pageGroup(), needsQuirk);
}
    
void TestRunner::setPluginsEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitPluginsEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setJavaScriptCanAccessClipboard(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetJavaScriptCanAccessClipboard(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::setPrivateBrowsingEnabled(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetPrivateBrowsingEnabled(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::setUseDashboardCompatibilityMode(bool enabled)
{
#if ENABLE(DASHBOARD_SUPPORT)
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetUseDashboardCompatibilityMode(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}
    
void TestRunner::setPopupBlockingEnabled(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetPopupBlockingEnabled(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::setAuthorAndUserStylesEnabled(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetAuthorAndUserStylesEnabled(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WKBundleAddOriginAccessWhitelistEntry(InjectedBundle::singleton().bundle(), toWK(sourceOrigin).get(), toWK(destinationProtocol).get(), toWK(destinationHost).get(), allowDestinationSubdomains);
}

void TestRunner::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WKBundleRemoveOriginAccessWhitelistEntry(InjectedBundle::singleton().bundle(), toWK(sourceOrigin).get(), toWK(destinationProtocol).get(), toWK(destinationHost).get(), allowDestinationSubdomains);
}

bool TestRunner::isPageBoxVisible(int pageIndex)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(injectedBundle.page()->page());
    return WKBundleIsPageBoxVisible(injectedBundle.bundle(), mainFrame, pageIndex);
}

void TestRunner::setValueForUser(JSContextRef context, JSValueRef element, JSStringRef value)
{
    if (!element || !JSValueIsObject(context, element))
        return;

    WKRetainPtr<WKBundleNodeHandleRef> nodeHandle(AdoptWK, WKBundleNodeHandleCreate(context, const_cast<JSObjectRef>(element)));
    WKBundleNodeHandleSetHTMLInputElementValueForUser(nodeHandle.get(), toWK(value).get());
}

void TestRunner::setAudioResult(JSContextRef context, JSValueRef data)
{
    auto& injectedBundle = InjectedBundle::singleton();
    // FIXME (123058): Use a JSC API to get buffer contents once such is exposed.
    WKRetainPtr<WKDataRef> audioData(AdoptWK, WKBundleCreateWKDataFromUInt8Array(injectedBundle.bundle(), context, data));
    injectedBundle.setAudioResult(audioData.get());
    setWhatToDump(WhatToDump::Audio);
    setDumpPixels(false);
}

unsigned TestRunner::windowCount()
{
    return InjectedBundle::singleton().pageCount();
}

void TestRunner::clearBackForwardList()
{
    WKBundleClearHistoryForTesting(InjectedBundle::singleton().page()->page());
}

// Object Creation

void TestRunner::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "testRunner", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

void TestRunner::showWebInspector()
{
    WKBundleInspectorShow(WKBundlePageGetInspector(InjectedBundle::singleton().page()->page()));
}

void TestRunner::closeWebInspector()
{
    WKBundleInspectorClose(WKBundlePageGetInspector(InjectedBundle::singleton().page()->page()));
}

void TestRunner::evaluateInWebInspector(JSStringRef script)
{
    WKRetainPtr<WKStringRef> scriptWK = toWK(script);
    WKBundleInspectorEvaluateScriptForTest(WKBundlePageGetInspector(InjectedBundle::singleton().page()->page()), scriptWK.get());
}

typedef WTF::HashMap<unsigned, WKRetainPtr<WKBundleScriptWorldRef> > WorldMap;
static WorldMap& worldMap()
{
    static WorldMap& map = *new WorldMap;
    return map;
}

unsigned TestRunner::worldIDForWorld(WKBundleScriptWorldRef world)
{
    WorldMap::const_iterator end = worldMap().end();
    for (WorldMap::const_iterator it = worldMap().begin(); it != end; ++it) {
        if (it->value == world)
            return it->key;
    }

    return 0;
}

void TestRunner::evaluateScriptInIsolatedWorld(JSContextRef context, unsigned worldID, JSStringRef script)
{
    // A worldID of 0 always corresponds to a new world. Any other worldID corresponds to a world
    // that is created once and cached forever.
    WKRetainPtr<WKBundleScriptWorldRef> world;
    if (!worldID)
        world.adopt(WKBundleScriptWorldCreateWorld());
    else {
        WKRetainPtr<WKBundleScriptWorldRef>& worldSlot = worldMap().add(worldID, nullptr).iterator->value;
        if (!worldSlot)
            worldSlot.adopt(WKBundleScriptWorldCreateWorld());
        world = worldSlot;
    }

    WKBundleFrameRef frame = WKBundleFrameForJavaScriptContext(context);
    if (!frame)
        frame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());

    JSGlobalContextRef jsContext = WKBundleFrameGetJavaScriptContextForWorld(frame, world.get());
    JSEvaluateScript(jsContext, script, 0, 0, 0, 0); 
}

void TestRunner::setPOSIXLocale(JSStringRef locale)
{
    char localeBuf[32];
    JSStringGetUTF8CString(locale, localeBuf, sizeof(localeBuf));
    setlocale(LC_ALL, localeBuf);
}

void TestRunner::setTextDirection(JSStringRef direction)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    return WKBundleFrameSetTextDirection(mainFrame, toWK(direction).get());
}
    
void TestRunner::setShouldStayOnPageAfterHandlingBeforeUnload(bool shouldStayOnPage)
{
    InjectedBundle::singleton().postNewBeforeUnloadReturnValue(!shouldStayOnPage);
}

void TestRunner::setDefersLoading(bool shouldDeferLoading)
{
    WKBundlePageSetDefersLoading(InjectedBundle::singleton().page()->page(), shouldDeferLoading);
}

bool TestRunner::didReceiveServerRedirectForProvisionalNavigation() const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("DidReceiveServerRedirectForProvisionalNavigation"));
    WKTypeRef returnData = 0;

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), 0, &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::clearDidReceiveServerRedirectForProvisionalNavigation()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ClearDidReceiveServerRedirectForProvisionalNavigation"));
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), 0, nullptr);
}

void TestRunner::setPageVisibility(JSStringRef state)
{
    InjectedBundle::singleton().setHidden(JSStringIsEqualToUTF8CString(state, "hidden") || JSStringIsEqualToUTF8CString(state, "prerender"));
}

void TestRunner::resetPageVisibility()
{
    InjectedBundle::singleton().setHidden(false);
}

typedef WTF::HashMap<unsigned, JSValueRef> CallbackMap;
static CallbackMap& callbackMap()
{
    static CallbackMap& map = *new CallbackMap;
    return map;
}

enum {
    AddChromeInputFieldCallbackID = 1,
    RemoveChromeInputFieldCallbackID,
    FocusWebViewCallbackID,
    SetBackingScaleFactorCallbackID,
    DidBeginSwipeCallbackID,
    WillEndSwipeCallbackID,
    DidEndSwipeCallbackID,
    DidRemoveSwipeSnapshotCallbackID,
    SetStatisticsDebugModeCallbackID,
    SetStatisticsPrevalentResourceForDebugModeCallbackID,
    SetStatisticsLastSeenCallbackID,
    SetStatisticsPrevalentResourceCallbackID,
    SetStatisticsVeryPrevalentResourceCallbackID,
    SetStatisticsHasHadUserInteractionCallbackID,
    StatisticsDidModifyDataRecordsCallbackID,
    StatisticsDidScanDataRecordsCallbackID,
    StatisticsDidRunTelemetryCallbackID,
    StatisticsDidClearThroughWebsiteDataRemovalCallbackID,
    StatisticsDidResetToConsistentStateCallbackID,
    StatisticsDidSetBlockCookiesForHostCallbackID,
    AllStorageAccessEntriesCallbackID,
    DidRemoveAllSessionCredentialsCallbackID,
    GetApplicationManifestCallbackID,
    TextDidChangeInTextFieldCallbackID,
    TextFieldDidBeginEditingCallbackID,
    TextFieldDidEndEditingCallbackID,
    FirstUIScriptCallbackID = 100
};

static void cacheTestRunnerCallback(unsigned index, JSValueRef callback)
{
    if (!callback)
        return;

    if (callbackMap().contains(index)) {
        InjectedBundle::singleton().outputText(String::format("FAIL: Tried to install a second TestRunner callback for the same event (id %d)\n\n", index));
        return;
    }

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSValueProtect(context, callback);
    callbackMap().add(index, callback);
}

static void callTestRunnerCallback(unsigned index, size_t argumentCount = 0, const JSValueRef arguments[] = nullptr)
{
    if (!callbackMap().contains(index))
        return;
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSObjectRef callback = JSValueToObject(context, callbackMap().take(index), 0);
    JSObjectCallAsFunction(context, callback, JSContextGetGlobalObject(context), argumentCount, arguments, 0);
    JSValueUnprotect(context, callback);
}

void TestRunner::clearTestRunnerCallbacks()
{
    for (auto& iter : callbackMap()) {
        WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
        JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
        JSObjectRef callback = JSValueToObject(context, iter.value, 0);
        JSValueUnprotect(context, callback);
    }

    callbackMap().clear();
}

void TestRunner::accummulateLogsForChannel(JSStringRef)
{
    // FIXME: Implement getting the call to all processes.
}

void TestRunner::addChromeInputField(JSValueRef callback)
{
    cacheTestRunnerCallback(AddChromeInputFieldCallbackID, callback);
    InjectedBundle::singleton().postAddChromeInputField();
}

void TestRunner::removeChromeInputField(JSValueRef callback)
{
    cacheTestRunnerCallback(RemoveChromeInputFieldCallbackID, callback);
    InjectedBundle::singleton().postRemoveChromeInputField();
}

void TestRunner::focusWebView(JSValueRef callback)
{
    cacheTestRunnerCallback(FocusWebViewCallbackID, callback);
    InjectedBundle::singleton().postFocusWebView();
}

void TestRunner::setBackingScaleFactor(double backingScaleFactor, JSValueRef callback)
{
    cacheTestRunnerCallback(SetBackingScaleFactorCallbackID, callback);
    InjectedBundle::singleton().postSetBackingScaleFactor(backingScaleFactor);
}

void TestRunner::setWindowIsKey(bool isKey)
{
    InjectedBundle::singleton().postSetWindowIsKey(isKey);
}

void TestRunner::setViewSize(double width, double height)
{
    InjectedBundle::singleton().postSetViewSize(width, height);
}

void TestRunner::callAddChromeInputFieldCallback()
{
    callTestRunnerCallback(AddChromeInputFieldCallbackID);
}

void TestRunner::callRemoveChromeInputFieldCallback()
{
    callTestRunnerCallback(RemoveChromeInputFieldCallbackID);
}

void TestRunner::callFocusWebViewCallback()
{
    callTestRunnerCallback(FocusWebViewCallbackID);
}

void TestRunner::callSetBackingScaleFactorCallback()
{
    callTestRunnerCallback(SetBackingScaleFactorCallbackID);
}

static inline bool toBool(JSStringRef value)
{
    return JSStringIsEqualToUTF8CString(value, "true") || JSStringIsEqualToUTF8CString(value, "1");
}

void TestRunner::overridePreference(JSStringRef preference, JSStringRef value)
{
    auto& injectedBundle = InjectedBundle::singleton();
    // FIXME: handle non-boolean preferences.
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), toWK(preference).get(), toBool(value));
}

void TestRunner::setAlwaysAcceptCookies(bool accept)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetAlwaysAcceptCookies"));

    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(accept));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

double TestRunner::preciseTime()
{
    return WallTime::now().secondsSinceEpoch().seconds();
}

void TestRunner::setUserStyleSheetEnabled(bool enabled)
{
    m_userStyleSheetEnabled = enabled;

    WKRetainPtr<WKStringRef> emptyUrl = adoptWK(WKStringCreateWithUTF8CString(""));
    WKStringRef location = enabled ? m_userStyleSheetLocation.get() : emptyUrl.get();
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetUserStyleSheetLocation(injectedBundle.bundle(), injectedBundle.pageGroup(), location);
}

void TestRunner::setUserStyleSheetLocation(JSStringRef location)
{
    m_userStyleSheetLocation = adoptWK(WKStringCreateWithJSString(location));

    if (m_userStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void TestRunner::setSpatialNavigationEnabled(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetSpatialNavigationEnabled(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::setTabKeyCyclesThroughElements(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetTabKeyCyclesThroughElements(injectedBundle.bundle(), injectedBundle.page()->page(), enabled);
}

void TestRunner::setSerializeHTTPLoads()
{
    // WK2 doesn't reorder loads.
}

void TestRunner::dispatchPendingLoadRequests()
{
    // WK2 doesn't keep pending requests.
}

void TestRunner::setCacheModel(int model)
{
    InjectedBundle::singleton().setCacheModel(model);
}

void TestRunner::setAsynchronousSpellCheckingEnabled(bool enabled)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetAsynchronousSpellCheckingEnabled(injectedBundle.bundle(), injectedBundle.pageGroup(), enabled);
}

void TestRunner::grantWebNotificationPermission(JSStringRef origin)
{
    WKRetainPtr<WKStringRef> originWK = toWK(origin);
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetWebNotificationPermission(injectedBundle.bundle(), injectedBundle.page()->page(), originWK.get(), true);
}

void TestRunner::denyWebNotificationPermission(JSStringRef origin)
{
    WKRetainPtr<WKStringRef> originWK = toWK(origin);
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetWebNotificationPermission(injectedBundle.bundle(), injectedBundle.page()->page(), originWK.get(), false);
}

void TestRunner::removeAllWebNotificationPermissions()
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleRemoveAllWebNotificationPermissions(injectedBundle.bundle(), injectedBundle.page()->page());
}

void TestRunner::simulateWebNotificationClick(JSValueRef notification)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(injectedBundle.page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    uint64_t notificationID = WKBundleGetWebNotificationID(injectedBundle.bundle(), context, notification);
    injectedBundle.postSimulateWebNotificationClick(notificationID);
}

void TestRunner::setGeolocationPermission(bool enabled)
{
    // FIXME: this should be done by frame.
    InjectedBundle::singleton().setGeolocationPermission(enabled);
}

bool TestRunner::isGeolocationProviderActive()
{
    return InjectedBundle::singleton().isGeolocationProviderActive();
}

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy, JSValueRef jsAltitude, JSValueRef jsAltitudeAccuracy, JSValueRef jsHeading, JSValueRef jsSpeed, JSValueRef jsFloorLevel)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(injectedBundle.page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    bool providesAltitude = false;
    double altitude = 0.;
    if (!JSValueIsUndefined(context, jsAltitude)) {
        providesAltitude = true;
        altitude = JSValueToNumber(context, jsAltitude, 0);
    }

    bool providesAltitudeAccuracy = false;
    double altitudeAccuracy = 0.;
    if (!JSValueIsUndefined(context, jsAltitudeAccuracy)) {
        providesAltitudeAccuracy = true;
        altitudeAccuracy = JSValueToNumber(context, jsAltitudeAccuracy, 0);
    }

    bool providesHeading = false;
    double heading = 0.;
    if (!JSValueIsUndefined(context, jsHeading)) {
        providesHeading = true;
        heading = JSValueToNumber(context, jsHeading, 0);
    }

    bool providesSpeed = false;
    double speed = 0.;
    if (!JSValueIsUndefined(context, jsSpeed)) {
        providesSpeed = true;
        speed = JSValueToNumber(context, jsSpeed, 0);
    }

    bool providesFloorLevel = false;
    double floorLevel = 0.;
    if (!JSValueIsUndefined(context, jsFloorLevel)) {
        providesFloorLevel = true;
        floorLevel = JSValueToNumber(context, jsFloorLevel, 0);
    }

    injectedBundle.setMockGeolocationPosition(latitude, longitude, accuracy, providesAltitude, altitude, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed, providesFloorLevel, floorLevel);
}

void TestRunner::setMockGeolocationPositionUnavailableError(JSStringRef message)
{
    WKRetainPtr<WKStringRef> messageWK = toWK(message);
    InjectedBundle::singleton().setMockGeolocationPositionUnavailableError(messageWK.get());
}

void TestRunner::setUserMediaPermission(bool enabled)
{
    // FIXME: this should be done by frame.
    InjectedBundle::singleton().setUserMediaPermission(enabled);
}

void TestRunner::resetUserMediaPermission()
{
    // FIXME: this should be done by frame.
    InjectedBundle::singleton().resetUserMediaPermission();
}

void TestRunner::setUserMediaPersistentPermissionForOrigin(bool permission, JSStringRef origin, JSStringRef parentOrigin)
{
    WKRetainPtr<WKStringRef> originWK = toWK(origin);
    WKRetainPtr<WKStringRef> parentOriginWK = toWK(parentOrigin);
    InjectedBundle::singleton().setUserMediaPersistentPermissionForOrigin(permission, originWK.get(), parentOriginWK.get());
}

unsigned TestRunner::userMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin) const
{
    WKRetainPtr<WKStringRef> originWK = toWK(origin);
    WKRetainPtr<WKStringRef> parentOriginWK = toWK(parentOrigin);
    return InjectedBundle::singleton().userMediaPermissionRequestCountForOrigin(originWK.get(), parentOriginWK.get());
}

void TestRunner::resetUserMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin)
{
    WKRetainPtr<WKStringRef> originWK = toWK(origin);
    WKRetainPtr<WKStringRef> parentOriginWK = toWK(parentOrigin);
    InjectedBundle::singleton().resetUserMediaPermissionRequestCountForOrigin(originWK.get(), parentOriginWK.get());
}

bool TestRunner::callShouldCloseOnWebView()
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    return WKBundleFrameCallShouldCloseOnWebView(mainFrame);
}

void TestRunner::queueBackNavigation(unsigned howFarBackward)
{
    InjectedBundle::singleton().queueBackNavigation(howFarBackward);
}

void TestRunner::queueForwardNavigation(unsigned howFarForward)
{
    InjectedBundle::singleton().queueForwardNavigation(howFarForward);
}

void TestRunner::queueLoad(JSStringRef url, JSStringRef target, bool shouldOpenExternalURLs)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKRetainPtr<WKURLRef> baseURLWK(AdoptWK, WKBundleFrameCopyURL(WKBundlePageGetMainFrame(injectedBundle.page()->page())));
    WKRetainPtr<WKURLRef> urlWK(AdoptWK, WKURLCreateWithBaseURL(baseURLWK.get(), toWTFString(toWK(url)).utf8().data()));
    WKRetainPtr<WKStringRef> urlStringWK(AdoptWK, WKURLCopyString(urlWK.get()));

    injectedBundle.queueLoad(urlStringWK.get(), toWK(target).get(), shouldOpenExternalURLs);
}

void TestRunner::queueLoadHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL)
{
    WKRetainPtr<WKStringRef> contentWK = toWK(content);
    WKRetainPtr<WKStringRef> baseURLWK = baseURL ? toWK(baseURL) : WKRetainPtr<WKStringRef>();
    WKRetainPtr<WKStringRef> unreachableURLWK = unreachableURL ? toWK(unreachableURL) : WKRetainPtr<WKStringRef>();

    InjectedBundle::singleton().queueLoadHTMLString(contentWK.get(), baseURLWK.get(), unreachableURLWK.get());
}

void TestRunner::queueReload()
{
    InjectedBundle::singleton().queueReload();
}

void TestRunner::queueLoadingScript(JSStringRef script)
{
    WKRetainPtr<WKStringRef> scriptWK = toWK(script);
    InjectedBundle::singleton().queueLoadingScript(scriptWK.get());
}

void TestRunner::queueNonLoadingScript(JSStringRef script)
{
    WKRetainPtr<WKStringRef> scriptWK = toWK(script);
    InjectedBundle::singleton().queueNonLoadingScript(scriptWK.get());
}

void TestRunner::setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetRejectsProtectionSpaceAndContinueForAuthenticationChallenges"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}
    
void TestRunner::setHandlesAuthenticationChallenges(bool handlesAuthenticationChallenges)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetHandlesAuthenticationChallenges"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(handlesAuthenticationChallenges));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setShouldLogCanAuthenticateAgainstProtectionSpace(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetShouldLogCanAuthenticateAgainstProtectionSpace"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setShouldLogDownloadCallbacks(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetShouldLogDownloadCallbacks"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setAuthenticationUsername(JSStringRef username)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetAuthenticationUsername"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(username));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setAuthenticationPassword(JSStringRef password)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetAuthenticationPassword"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(password));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

bool TestRunner::secureEventInputIsEnabled() const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SecureEventInputIsEnabled"));
    WKTypeRef returnData = 0;

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), 0, &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::setBlockAllPlugins(bool shouldBlock)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetBlockAllPlugins"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(shouldBlock));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setPluginSupportedMode(JSStringRef mode)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetPluginSupportedMode"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(mode));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

JSValueRef TestRunner::failNextNewCodeBlock()
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    return JSC::failNextNewCodeBlock(context);
}

JSValueRef TestRunner::numberOfDFGCompiles(JSValueRef theFunction)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    return JSC::numberOfDFGCompiles(context, theFunction);
}

JSValueRef TestRunner::neverInlineFunction(JSValueRef theFunction)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    return JSC::setNeverInline(context, theFunction);
}

void TestRunner::setShouldDecideNavigationPolicyAfterDelay(bool value)
{
    m_shouldDecideNavigationPolicyAfterDelay = value;
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetShouldDecideNavigationPolicyAfterDelay"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setShouldDecideResponsePolicyAfterDelay(bool value)
{
    m_shouldDecideResponsePolicyAfterDelay = value;
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetShouldDecideResponsePolicyAfterDelay"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setNavigationGesturesEnabled(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetNavigationGesturesEnabled"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setIgnoresViewportScaleLimits(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetIgnoresViewportScaleLimits"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::setShouldDownloadUndisplayableMIMETypes(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetShouldDownloadUndisplayableMIMETypes"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get());
}

void TestRunner::terminateNetworkProcess()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("TerminateNetworkProcess"));
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), nullptr, nullptr);
}

void TestRunner::terminateServiceWorkerProcess()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("TerminateServiceWorkerProcess"));
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), nullptr, nullptr);
}

static unsigned nextUIScriptCallbackID()
{
    static unsigned callbackID = FirstUIScriptCallbackID;
    return callbackID++;
}

void TestRunner::runUIScript(JSStringRef script, JSValueRef callback)
{
    unsigned callbackID = nextUIScriptCallbackID();
    cacheTestRunnerCallback(callbackID, callback);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("RunUIProcessScript"));

    WKRetainPtr<WKMutableDictionaryRef> testDictionary(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> scriptKey(AdoptWK, WKStringCreateWithUTF8CString("Script"));
    WKRetainPtr<WKStringRef> scriptValue(AdoptWK, WKStringCreateWithJSString(script));

    WKRetainPtr<WKStringRef> callbackIDKey(AdoptWK, WKStringCreateWithUTF8CString("CallbackID"));
    WKRetainPtr<WKUInt64Ref> callbackIDValue = adoptWK(WKUInt64Create(callbackID));

    WKDictionarySetItem(testDictionary.get(), scriptKey.get(), scriptValue.get());
    WKDictionarySetItem(testDictionary.get(), callbackIDKey.get(), callbackIDValue.get());

    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), messageName.get(), testDictionary.get());
}

void TestRunner::runUIScriptCallback(unsigned callbackID, JSStringRef result)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    JSValueRef resultValue = JSValueMakeString(context, result);
    callTestRunnerCallback(callbackID, 1, &resultValue);
}

void TestRunner::installDidBeginSwipeCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(DidBeginSwipeCallbackID, callback);
}

void TestRunner::installWillEndSwipeCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(WillEndSwipeCallbackID, callback);
}

void TestRunner::installDidEndSwipeCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(DidEndSwipeCallbackID, callback);
}

void TestRunner::installDidRemoveSwipeSnapshotCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(DidRemoveSwipeSnapshotCallbackID, callback);
}

void TestRunner::callDidBeginSwipeCallback()
{
    callTestRunnerCallback(DidBeginSwipeCallbackID);
}

void TestRunner::callWillEndSwipeCallback()
{
    callTestRunnerCallback(WillEndSwipeCallbackID);
}

void TestRunner::callDidEndSwipeCallback()
{
    callTestRunnerCallback(DidEndSwipeCallbackID);
}

void TestRunner::callDidRemoveSwipeSnapshotCallback()
{
    callTestRunnerCallback(DidRemoveSwipeSnapshotCallbackID);
}

void TestRunner::setStatisticsDebugMode(bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsDebugModeCallbackID, completionHandler);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsDebugMode"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);

}

void TestRunner::statisticsCallDidSetDebugModeCallback()
{
    callTestRunnerCallback(SetStatisticsDebugModeCallbackID);
}

void TestRunner::setStatisticsPrevalentResourceForDebugMode(JSStringRef hostName, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsPrevalentResourceForDebugModeCallbackID, completionHandler);
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsPrevalentResourceForDebugMode"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsCallDidSetPrevalentResourceForDebugModeCallback()
{
    callTestRunnerCallback(SetStatisticsPrevalentResourceForDebugModeCallbackID);
}

void TestRunner::setStatisticsLastSeen(JSStringRef hostName, double seconds, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsLastSeenCallbackID, completionHandler);

    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKDoubleCreate(seconds) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsLastSeen"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsCallDidSetLastSeenCallback()
{
    callTestRunnerCallback(SetStatisticsLastSeenCallbackID);
}

void TestRunner::setStatisticsPrevalentResource(JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsPrevalentResourceCallbackID, completionHandler);

    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKBooleanCreate(value) });
    
    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsPrevalentResource"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsCallDidSetPrevalentResourceCallback()
{
    callTestRunnerCallback(SetStatisticsPrevalentResourceCallbackID);
}

void TestRunner::setStatisticsVeryPrevalentResource(JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsVeryPrevalentResourceCallbackID, completionHandler);

    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKBooleanCreate(value) });
    
    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsVeryPrevalentResource"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsCallDidSetVeryPrevalentResourceCallback()
{
    callTestRunnerCallback(SetStatisticsVeryPrevalentResourceCallbackID);
}
    
void TestRunner::dumpResourceLoadStatistics()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("dumpResourceLoadStatistics"));
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), nullptr, nullptr);
}

bool TestRunner::isStatisticsPrevalentResource(JSStringRef hostName)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsPrevalentResource"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

bool TestRunner::isStatisticsVeryPrevalentResource(JSStringRef hostName)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsVeryPrevalentResource"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

bool TestRunner::isStatisticsRegisteredAsSubresourceUnder(JSStringRef subresourceHost, JSStringRef topFrameHost)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("SubresourceHost") });
    values.append({ AdoptWK, WKStringCreateWithJSString(subresourceHost) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("TopFrameHost") });
    values.append({ AdoptWK, WKStringCreateWithJSString(topFrameHost) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsRegisteredAsSubresourceUnder"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

bool TestRunner::isStatisticsRegisteredAsSubFrameUnder(JSStringRef subFrameHost, JSStringRef topFrameHost)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("SubFrameHost") });
    values.append({ AdoptWK, WKStringCreateWithJSString(subFrameHost) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("TopFrameHost") });
    values.append({ AdoptWK, WKStringCreateWithJSString(topFrameHost) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsRegisteredAsSubFrameUnder"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

bool TestRunner::isStatisticsRegisteredAsRedirectingTo(JSStringRef hostRedirectedFrom, JSStringRef hostRedirectedTo)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostRedirectedFrom") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostRedirectedFrom) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostRedirectedTo") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostRedirectedTo) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsRegisteredAsRedirectingTo"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::setStatisticsHasHadUserInteraction(JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsHasHadUserInteractionCallbackID, completionHandler);

    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKBooleanCreate(value) });
    
    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsHasHadUserInteraction"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsCallDidSetHasHadUserInteractionCallback()
{
    callTestRunnerCallback(SetStatisticsHasHadUserInteractionCallbackID);
}

bool TestRunner::isStatisticsHasHadUserInteraction(JSStringRef hostName)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsHasHadUserInteraction"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::setStatisticsGrandfathered(JSStringRef hostName, bool value)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKBooleanCreate(value) });
    
    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsGrandfathered"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}
    
bool TestRunner::isStatisticsGrandfathered(JSStringRef hostName)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsGrandfathered"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::setStatisticsSubframeUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("TopFrameHostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(topFrameHostName) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsSubframeUnderTopFrameOrigin"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsSubresourceUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("TopFrameHostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(topFrameHostName) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsSubresourceUnderTopFrameOrigin"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsSubresourceUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedTo") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostNameRedirectedTo) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsSubresourceUniqueRedirectTo"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}


void TestRunner::setStatisticsSubresourceUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedFrom") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostNameRedirectedFrom) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsSubresourceUniqueRedirectFrom"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsTopFrameUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedTo") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostNameRedirectedTo) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsTopFrameUniqueRedirectTo"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsTopFrameUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostName") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostName) });
    
    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("HostNameRedirectedFrom") });
    values.append({ AdoptWK, WKStringCreateWithJSString(hostNameRedirectedFrom) });
    
    Vector<WKStringRef> rawKeys(keys.size());
    Vector<WKTypeRef> rawValues(values.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsTopFrameUniqueRedirectFrom"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}


void TestRunner::setStatisticsTimeToLiveUserInteraction(double seconds)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsTimeToLiveUserInteraction"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(seconds));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::installStatisticsDidModifyDataRecordsCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidModifyDataRecordsCallbackID, callback);
}

void TestRunner::statisticsDidModifyDataRecordsCallback()
{
    callTestRunnerCallback(StatisticsDidModifyDataRecordsCallbackID);
}

void TestRunner::installStatisticsDidScanDataRecordsCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidScanDataRecordsCallbackID, callback);
}

void TestRunner::statisticsDidScanDataRecordsCallback()
{
    callTestRunnerCallback(StatisticsDidScanDataRecordsCallbackID);
}

void TestRunner::installStatisticsDidRunTelemetryCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidRunTelemetryCallbackID, callback);
}
    
void TestRunner::statisticsDidRunTelemetryCallback(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    
    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("{ \"totalPrevalentResources\" : ");
    stringBuilder.appendNumber(totalPrevalentResources);
    stringBuilder.appendLiteral(", \"totalPrevalentResourcesWithUserInteraction\" : ");
    stringBuilder.appendNumber(totalPrevalentResourcesWithUserInteraction);
    stringBuilder.appendLiteral(", \"top3SubframeUnderTopFrameOrigins\" : ");
    stringBuilder.appendNumber(top3SubframeUnderTopFrameOrigins);
    stringBuilder.appendLiteral(" }");
    
    JSValueRef result = JSValueMakeFromJSONString(context, adopt(JSStringCreateWithUTF8CString(stringBuilder.toString().utf8().data())).get());

    callTestRunnerCallback(StatisticsDidRunTelemetryCallbackID, 1, &result);
}

void TestRunner::statisticsNotifyObserver()
{
    InjectedBundle::singleton().statisticsNotifyObserver();
}

void TestRunner::statisticsProcessStatisticsAndDataRecords()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsProcessStatisticsAndDataRecords"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::statisticsUpdateCookieBlocking(JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidSetBlockCookiesForHostCallbackID, completionHandler);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsUpdateCookieBlocking"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::statisticsCallDidSetBlockCookiesForHostCallback()
{
    callTestRunnerCallback(StatisticsDidSetBlockCookiesForHostCallbackID);
}

void TestRunner::statisticsSubmitTelemetry()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsSubmitTelemetry"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::setStatisticsNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsNotifyPagesWhenDataRecordsWereScanned"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsShouldClassifyResourcesBeforeDataRecordsRemoval"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsNotifyPagesWhenTelemetryWasCaptured"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsMinimumTimeBetweenDataRecordsRemoval(double seconds)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsMinimumTimeBetweenDataRecordsRemoval"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(seconds));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsGrandfatheringTime(double seconds)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsGrandfatheringTime"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(seconds));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setStatisticsMaxStatisticsEntries(unsigned entries)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetMaxStatisticsEntries"));
    WKRetainPtr<WKTypeRef> messageBody(AdoptWK, WKUInt64Create(entries));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}
    
void TestRunner::setStatisticsPruneEntriesDownTo(unsigned entries)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetPruneEntriesDownTo"));
    WKRetainPtr<WKTypeRef> messageBody(AdoptWK, WKUInt64Create(entries));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}
    
void TestRunner::statisticsClearInMemoryAndPersistentStore(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidClearThroughWebsiteDataRemovalCallbackID, callback);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsClearInMemoryAndPersistentStore"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours, JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidClearThroughWebsiteDataRemovalCallbackID, callback);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsClearInMemoryAndPersistentStoreModifiedSinceHours"));
    WKRetainPtr<WKTypeRef> messageBody(AdoptWK, WKUInt64Create(hours));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsClearThroughWebsiteDataRemoval(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidClearThroughWebsiteDataRemovalCallbackID, callback);
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsClearThroughWebsiteDataRemoval"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::setStatisticsCacheMaxAgeCap(double seconds)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsCacheMaxAgeCap"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(seconds));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::statisticsCallClearThroughWebsiteDataRemovalCallback()
{
    callTestRunnerCallback(StatisticsDidClearThroughWebsiteDataRemovalCallbackID);
}

void TestRunner::statisticsResetToConsistentState(JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidResetToConsistentStateCallbackID, completionHandler);
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsResetToConsistentState"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::statisticsCallDidResetToConsistentStateCallback()
{
    callTestRunnerCallback(StatisticsDidResetToConsistentStateCallbackID);
}

void TestRunner::installTextDidChangeInTextFieldCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(TextDidChangeInTextFieldCallbackID, callback);
}

void TestRunner::textDidChangeInTextFieldCallback()
{
    callTestRunnerCallback(TextDidChangeInTextFieldCallbackID);
}

void TestRunner::installTextFieldDidBeginEditingCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(TextFieldDidBeginEditingCallbackID, callback);
}

void TestRunner::textFieldDidBeginEditingCallback()
{
    callTestRunnerCallback(TextFieldDidBeginEditingCallbackID);
}

void TestRunner::installTextFieldDidEndEditingCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(TextFieldDidEndEditingCallbackID, callback);
}

void TestRunner::textFieldDidEndEditingCallback()
{
    callTestRunnerCallback(TextFieldDidEndEditingCallbackID);
}

void TestRunner::setStorageAccessAPIEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStorageAccessAPIEnabled"));
    
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(enabled));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::getAllStorageAccessEntries(JSValueRef callback)
{
    cacheTestRunnerCallback(AllStorageAccessEntriesCallbackID, callback);
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("GetAllStorageAccessEntries"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
}

void TestRunner::callDidReceiveAllStorageAccessEntriesCallback(Vector<String>& domains)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    
    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("[");
    bool firstDomain = true;
    for (auto& domain : domains) {
        if (firstDomain)
            firstDomain = false;
        else
            stringBuilder.appendLiteral(", ");
        stringBuilder.appendLiteral("\"");
        stringBuilder.append(domain);
        stringBuilder.appendLiteral("\"");
    }
    stringBuilder.appendLiteral("]");
    
    JSValueRef result = JSValueMakeFromJSONString(context, adopt(JSStringCreateWithUTF8CString(stringBuilder.toString().utf8().data())).get());

    callTestRunnerCallback(AllStorageAccessEntriesCallbackID, 1, &result);
}

void TestRunner::addMockMediaDevice(JSStringRef persistentId, JSStringRef label, const char* type)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("PersistentID") });
    values.append(toWK(persistentId));

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Label") });
    values.append(toWK(label));

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Type") });
    values.append({ AdoptWK, WKStringCreateWithUTF8CString(type) });

    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("AddMockMediaDevice"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::addMockCameraDevice(JSStringRef persistentId, JSStringRef label)
{
    addMockMediaDevice(persistentId, label, "camera");
}

void TestRunner::addMockMicrophoneDevice(JSStringRef persistentId, JSStringRef label)
{
    addMockMediaDevice(persistentId, label, "microphone");
}

void TestRunner::addMockScreenDevice(JSStringRef persistentId, JSStringRef label)
{
    addMockMediaDevice(persistentId, label, "screen");
}

void TestRunner::clearMockMediaDevices()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ClearMockMediaDevices"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), nullptr, nullptr);
}

void TestRunner::removeMockMediaDevice(JSStringRef persistentId)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("RemoveMockMediaDevice"));
    WKRetainPtr<WKTypeRef> messageBody(toWK(persistentId));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::resetMockMediaDevices()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ResetMockMediaDevices"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), nullptr, nullptr);
}

#if PLATFORM(MAC)
void TestRunner::connectMockGamepad(unsigned index)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ConnectMockGamepad"));
    WKRetainPtr<WKTypeRef> messageBody(AdoptWK, WKUInt64Create(index));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::disconnectMockGamepad(unsigned index)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("DisconnectMockGamepad"));
    WKRetainPtr<WKTypeRef> messageBody(AdoptWK, WKUInt64Create(index));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setMockGamepadDetails(unsigned index, JSStringRef gamepadID, unsigned axisCount, unsigned buttonCount)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("GamepadID") });
    values.append(toWK(gamepadID));

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("GamepadIndex") });
    values.append({ AdoptWK, WKUInt64Create(index) });

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("AxisCount") });
    values.append({ AdoptWK, WKUInt64Create(axisCount) });

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("ButtonCount") });
    values.append({ AdoptWK, WKUInt64Create(buttonCount) });

    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetMockGamepadDetails"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setMockGamepadAxisValue(unsigned index, unsigned axisIndex, double value)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("GamepadIndex") });
    values.append({ AdoptWK, WKUInt64Create(index) });

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("AxisIndex") });
    values.append({ AdoptWK, WKUInt64Create(axisIndex) });

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKDoubleCreate(value) });

    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetMockGamepadAxisValue"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::setMockGamepadButtonValue(unsigned index, unsigned buttonIndex, double value)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("GamepadIndex") });
    values.append({ AdoptWK, WKUInt64Create(index) });

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("ButtonIndex") });
    values.append({ AdoptWK, WKUInt64Create(buttonIndex) });

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("Value") });
    values.append({ AdoptWK, WKDoubleCreate(value) });

    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetMockGamepadButtonValue"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}
#else
void TestRunner::connectMockGamepad(unsigned)
{
}

void TestRunner::disconnectMockGamepad(unsigned)
{
}

void TestRunner::setMockGamepadDetails(unsigned, JSStringRef, unsigned, unsigned)
{
}

void TestRunner::setMockGamepadAxisValue(unsigned, unsigned, double)
{
}

void TestRunner::setMockGamepadButtonValue(unsigned, unsigned, double)
{
}
#endif // PLATFORM(MAC)

void TestRunner::setOpenPanelFiles(JSValueRef filesValue)
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    JSContextRef context = WKBundleFrameGetJavaScriptContext(WKBundlePageGetMainFrame(page));

    if (!JSValueIsArray(context, filesValue))
        return;

    JSObjectRef files = JSValueToObject(context, filesValue, nullptr);
    static auto lengthProperty = adopt(JSStringCreateWithUTF8CString("length"));
    JSValueRef filesLengthValue = JSObjectGetProperty(context, files, lengthProperty.get(), nullptr);
    if (!JSValueIsNumber(context, filesLengthValue))
        return;

    auto fileURLs = adoptWK(WKMutableArrayCreate());
    auto filesLength = static_cast<size_t>(JSValueToNumber(context, filesLengthValue, nullptr));
    for (size_t i = 0; i < filesLength; ++i) {
        JSValueRef fileValue = JSObjectGetPropertyAtIndex(context, files, i, nullptr);
        if (!JSValueIsString(context, fileValue))
            continue;

        auto file = adopt(JSValueToStringCopy(context, fileValue, nullptr));
        size_t fileBufferSize = JSStringGetMaximumUTF8CStringSize(file.get()) + 1;
        auto fileBuffer = std::make_unique<char[]>(fileBufferSize);
        JSStringGetUTF8CString(file.get(), fileBuffer.get(), fileBufferSize);

        WKArrayAppendItem(fileURLs.get(), adoptWK(WKURLCreateWithBaseURL(m_testURL.get(), fileBuffer.get())).get());
    }

    static auto messageName = adoptWK(WKStringCreateWithUTF8CString("SetOpenPanelFileURLs"));
    WKBundlePagePostMessage(page, messageName.get(), fileURLs.get());
}

void TestRunner::removeAllSessionCredentials(JSValueRef callback)
{
    cacheTestRunnerCallback(DidRemoveAllSessionCredentialsCallbackID, callback);
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("RemoveAllSessionCredentials"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(true));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::callDidRemoveAllSessionCredentialsCallback()
{
    callTestRunnerCallback(DidRemoveAllSessionCredentialsCallbackID);
}

void TestRunner::clearDOMCache(JSStringRef origin)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ClearDOMCache"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(origin));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::clearDOMCaches()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ClearDOMCaches"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), nullptr, nullptr);
}

bool TestRunner::hasDOMCache(JSStringRef origin)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("HasDOMCache"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(origin));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

uint64_t TestRunner::domCacheSize(JSStringRef origin)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("DOMCacheSize"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(origin));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKUInt64GetValue(static_cast<WKUInt64Ref>(returnData));
}

void TestRunner::getApplicationManifestThen(JSValueRef callback)
{
    cacheTestRunnerCallback(GetApplicationManifestCallbackID, callback);
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("GetApplicationManifest"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), nullptr, nullptr);
}

void TestRunner::didGetApplicationManifest()
{
    callTestRunnerCallback(GetApplicationManifestCallbackID);
}

size_t TestRunner::userScriptInjectedCount() const
{
    return InjectedBundle::singleton().userScriptInjectedCount();
}

void TestRunner::injectUserScript(JSStringRef script)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("InjectUserScript"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(script));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
}

void TestRunner::sendDisplayConfigurationChangedMessageForTesting()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SendDisplayConfigurationChangedMessageForTesting"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), nullptr, nullptr);
}

// WebAuthN
void TestRunner::setWebAuthenticationMockConfiguration(JSValueRef configurationValue)
{
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(injectedBundle.page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    if (!JSValueIsObject(context, configurationValue))
        return;
    JSObjectRef configuration = JSValueToObject(context, configurationValue, 0);

    Vector<WKRetainPtr<WKStringRef>> configurationKeys;
    Vector<WKRetainPtr<WKTypeRef>> configurationValues;

    JSRetainPtr<JSStringRef> silentFailurePropertyName(Adopt, JSStringCreateWithUTF8CString("silentFailure"));
    JSValueRef silentFailureValue = JSObjectGetProperty(context, configuration, silentFailurePropertyName.get(), 0);
    if (!JSValueIsUndefined(context, silentFailureValue)) {
        if (!JSValueIsBoolean(context, silentFailureValue))
            return;
        bool silentFailure = JSValueToBoolean(context, silentFailureValue);
        configurationKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("SilentFailure") });
        configurationValues.append(adoptWK(WKBooleanCreate(silentFailure)).get());
    }

    JSRetainPtr<JSStringRef> localPropertyName(Adopt, JSStringCreateWithUTF8CString("local"));
    JSValueRef localValue = JSObjectGetProperty(context, configuration, localPropertyName.get(), 0);
    if (!JSValueIsUndefined(context, localValue) && !JSValueIsNull(context, localValue)) {
        if (!JSValueIsObject(context, localValue))
            return;
        JSObjectRef local = JSValueToObject(context, localValue, 0);

        JSRetainPtr<JSStringRef> acceptAuthenticationPropertyName(Adopt, JSStringCreateWithUTF8CString("acceptAuthentication"));
        JSValueRef acceptAuthenticationValue = JSObjectGetProperty(context, local, acceptAuthenticationPropertyName.get(), 0);
        if (!JSValueIsBoolean(context, acceptAuthenticationValue))
            return;
        bool acceptAuthentication = JSValueToBoolean(context, acceptAuthenticationValue);

        JSRetainPtr<JSStringRef> acceptAttestationPropertyName(Adopt, JSStringCreateWithUTF8CString("acceptAttestation"));
        JSValueRef acceptAttestationValue = JSObjectGetProperty(context, local, acceptAttestationPropertyName.get(), 0);
        if (!JSValueIsBoolean(context, acceptAttestationValue))
            return;
        bool acceptAttestation = JSValueToBoolean(context, acceptAttestationValue);

        Vector<WKRetainPtr<WKStringRef>> localKeys;
        Vector<WKRetainPtr<WKTypeRef>> localValues;
        localKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("AcceptAuthentication") });
        localValues.append(adoptWK(WKBooleanCreate(acceptAuthentication)).get());
        localKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("AcceptAttestation") });
        localValues.append(adoptWK(WKBooleanCreate(acceptAttestation)).get());

        if (acceptAttestation) {
            JSRetainPtr<JSStringRef> privateKeyBase64PropertyName(Adopt, JSStringCreateWithUTF8CString("privateKeyBase64"));
            JSValueRef privateKeyBase64Value = JSObjectGetProperty(context, local, privateKeyBase64PropertyName.get(), 0);
            if (!JSValueIsString(context, privateKeyBase64Value))
                return;

            JSRetainPtr<JSStringRef> userCertificateBase64PropertyName(Adopt, JSStringCreateWithUTF8CString("userCertificateBase64"));
            JSValueRef userCertificateBase64Value = JSObjectGetProperty(context, local, userCertificateBase64PropertyName.get(), 0);
            if (!JSValueIsString(context, userCertificateBase64Value))
                return;

            JSRetainPtr<JSStringRef> intermediateCACertificateBase64PropertyName(Adopt, JSStringCreateWithUTF8CString("intermediateCACertificateBase64"));
            JSValueRef intermediateCACertificateBase64Value = JSObjectGetProperty(context, local, intermediateCACertificateBase64PropertyName.get(), 0);
            if (!JSValueIsString(context, intermediateCACertificateBase64Value))
            return;

            localKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("PrivateKeyBase64") });
            localValues.append(toWK(adopt(JSValueToStringCopy(context, privateKeyBase64Value, 0)).get()));
            localKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("UserCertificateBase64") });
            localValues.append(toWK(adopt(JSValueToStringCopy(context, userCertificateBase64Value, 0)).get()));
            localKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("IntermediateCACertificateBase64") });
            localValues.append(toWK(adopt(JSValueToStringCopy(context, intermediateCACertificateBase64Value, 0)).get()));
        }

        Vector<WKStringRef> rawLocalKeys;
        Vector<WKTypeRef> rawLocalValues;
        rawLocalKeys.resize(localKeys.size());
        rawLocalValues.resize(localValues.size());
        for (size_t i = 0; i < localKeys.size(); ++i) {
            rawLocalKeys[i] = localKeys[i].get();
            rawLocalValues[i] = localValues[i].get();
        }

        configurationKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("Local") });
        configurationValues.append({ AdoptWK, WKDictionaryCreate(rawLocalKeys.data(), rawLocalValues.data(), rawLocalKeys.size()) });
    }

    JSRetainPtr<JSStringRef> hidPropertyName(Adopt, JSStringCreateWithUTF8CString("hid"));
    JSValueRef hidValue = JSObjectGetProperty(context, configuration, hidPropertyName.get(), 0);
    if (!JSValueIsUndefined(context, hidValue) && !JSValueIsNull(context, hidValue)) {
        if (!JSValueIsObject(context, hidValue))
            return;
        JSObjectRef hid = JSValueToObject(context, hidValue, 0);

        JSRetainPtr<JSStringRef> stagePropertyName(Adopt, JSStringCreateWithUTF8CString("stage"));
        JSValueRef stageValue = JSObjectGetProperty(context, hid, stagePropertyName.get(), 0);
        if (!JSValueIsString(context, stageValue))
            return;

        JSRetainPtr<JSStringRef> subStagePropertyName(Adopt, JSStringCreateWithUTF8CString("subStage"));
        JSValueRef subStageValue = JSObjectGetProperty(context, hid, subStagePropertyName.get(), 0);
        if (!JSValueIsString(context, subStageValue))
            return;

        JSRetainPtr<JSStringRef> errorPropertyName(Adopt, JSStringCreateWithUTF8CString("error"));
        JSValueRef errorValue = JSObjectGetProperty(context, hid, errorPropertyName.get(), 0);
        if (!JSValueIsString(context, errorValue))
            return;

        Vector<WKRetainPtr<WKStringRef>> hidKeys;
        Vector<WKRetainPtr<WKTypeRef>> hidValues;
        hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("Stage") });
        hidValues.append(toWK(adopt(JSValueToStringCopy(context, stageValue, 0)).get()));
        hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("SubStage") });
        hidValues.append(toWK(adopt(JSValueToStringCopy(context, subStageValue, 0)).get()));
        hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("Error") });
        hidValues.append(toWK(adopt(JSValueToStringCopy(context, errorValue, 0)).get()));

        JSRetainPtr<JSStringRef> payloadBase64PropertyName(Adopt, JSStringCreateWithUTF8CString("payloadBase64"));
        JSValueRef payloadBase64Value = JSObjectGetProperty(context, hid, payloadBase64PropertyName.get(), 0);
        if (!JSValueIsUndefined(context, payloadBase64Value) && !JSValueIsNull(context, payloadBase64Value)) {
            if (!JSValueIsString(context, payloadBase64Value))
                return;
            hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("PayloadBase64") });
            hidValues.append(toWK(adopt(JSValueToStringCopy(context, payloadBase64Value, 0)).get()));
        }

        JSRetainPtr<JSStringRef> keepAlivePropertyName(Adopt, JSStringCreateWithUTF8CString("keepAlive"));
        JSValueRef keepAliveValue = JSObjectGetProperty(context, hid, keepAlivePropertyName.get(), 0);
        if (!JSValueIsUndefined(context, keepAliveValue) && !JSValueIsNull(context, keepAliveValue)) {
            if (!JSValueIsBoolean(context, keepAliveValue))
                return;
            bool keepAlive = JSValueToBoolean(context, keepAliveValue);
            hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("KeepAlive") });
            hidValues.append(adoptWK(WKBooleanCreate(keepAlive)).get());
        }

        JSRetainPtr<JSStringRef> fastDataArrivalPropertyName(Adopt, JSStringCreateWithUTF8CString("fastDataArrival"));
        JSValueRef fastDataArrivalValue = JSObjectGetProperty(context, hid, fastDataArrivalPropertyName.get(), 0);
        if (!JSValueIsUndefined(context, fastDataArrivalValue) && !JSValueIsNull(context, fastDataArrivalValue)) {
            if (!JSValueIsBoolean(context, fastDataArrivalValue))
                return;
            bool fastDataArrival = JSValueToBoolean(context, fastDataArrivalValue);
            hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("FastDataArrival") });
            hidValues.append(adoptWK(WKBooleanCreate(fastDataArrival)).get());
        }

        JSRetainPtr<JSStringRef> continueAfterErrorDataPropertyName(Adopt, JSStringCreateWithUTF8CString("continueAfterErrorData"));
        JSValueRef continueAfterErrorDataValue = JSObjectGetProperty(context, hid, continueAfterErrorDataPropertyName.get(), 0);
        if (!JSValueIsUndefined(context, continueAfterErrorDataValue) && !JSValueIsNull(context, continueAfterErrorDataValue)) {
            if (!JSValueIsBoolean(context, continueAfterErrorDataValue))
                return;
            bool continueAfterErrorData = JSValueToBoolean(context, continueAfterErrorDataValue);
            hidKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("ContinueAfterErrorData") });
            hidValues.append(adoptWK(WKBooleanCreate(continueAfterErrorData)).get());
        }

        Vector<WKStringRef> rawHidKeys;
        Vector<WKTypeRef> rawHidValues;
        rawHidKeys.resize(hidKeys.size());
        rawHidValues.resize(hidValues.size());
        for (size_t i = 0; i < hidKeys.size(); ++i) {
            rawHidKeys[i] = hidKeys[i].get();
            rawHidValues[i] = hidValues[i].get();
        }

        configurationKeys.append({ AdoptWK, WKStringCreateWithUTF8CString("Hid") });
        configurationValues.append({ AdoptWK, WKDictionaryCreate(rawHidKeys.data(), rawHidValues.data(), rawHidKeys.size()) });
    }

    Vector<WKStringRef> rawConfigurationKeys;
    Vector<WKTypeRef> rawConfigurationValues;
    rawConfigurationKeys.resize(configurationKeys.size());
    rawConfigurationValues.resize(configurationValues.size());
    for (size_t i = 0; i < configurationKeys.size(); ++i) {
        rawConfigurationKeys[i] = configurationKeys[i].get();
        rawConfigurationValues[i] = configurationValues[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetWebAuthenticationMockConfiguration"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawConfigurationKeys.data(), rawConfigurationValues.data(), rawConfigurationKeys.size()));
    
    WKBundlePostSynchronousMessage(injectedBundle.bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::addTestKeyToKeychain(JSStringRef privateKeyBase64, JSStringRef attrLabel, JSStringRef applicationTagBase64)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("PrivateKey") });
    values.append(toWK(privateKeyBase64));

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("AttrLabel") });
    values.append(toWK(attrLabel));

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("ApplicationTag") });
    values.append(toWK(applicationTagBase64));

    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("AddTestKeyToKeychain"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

void TestRunner::cleanUpKeychain(JSStringRef attrLabel)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("CleanUpKeychain"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(attrLabel));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

bool TestRunner::keyExistsInKeychain(JSStringRef attrLabel, JSStringRef applicationTagBase64)
{
    Vector<WKRetainPtr<WKStringRef>> keys;
    Vector<WKRetainPtr<WKTypeRef>> values;

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("AttrLabel") });
    values.append(toWK(attrLabel));

    keys.append({ AdoptWK, WKStringCreateWithUTF8CString("ApplicationTag") });
    values.append(toWK(applicationTagBase64));

    Vector<WKStringRef> rawKeys;
    Vector<WKTypeRef> rawValues;
    rawKeys.resize(keys.size());
    rawValues.resize(values.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        rawKeys[i] = keys[i].get();
        rawValues[i] = values[i].get();
    }

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("KeyExistsInKeychain"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKTypeRef returnData = 0;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::toggleCapsLock()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ToggleCapsLock"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), nullptr, nullptr);
}

} // namespace WTR
