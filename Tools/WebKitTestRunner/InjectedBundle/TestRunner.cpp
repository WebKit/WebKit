/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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

#include "ActivateFonts.h"
#include "DictionaryFunctions.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSTestRunner.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include <JavaScriptCore/JSCTestRunnerUtils.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebKit/WKBase.h>
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundleFramePrivate.h>
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
#include <WebKit/WKStringPrivate.h>
#include <WebKit/WebKit2_C.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if OS(WINDOWS)
#include <shlwapi.h>
#include <wininet.h>
#endif

namespace WTR {

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

Ref<TestRunner> TestRunner::create()
{
    return adoptRef(*new TestRunner);
}

TestRunner::TestRunner()
    : m_userStyleSheetLocation(toWK(""))
{
    platformInitialize();
}

TestRunner::~TestRunner() = default;

JSClassRef TestRunner::wrapperClass()
{
    return JSTestRunner::testRunnerClass();
}

static WKBundlePageRef page()
{
    return InjectedBundle::singleton().page()->page();
}

void TestRunner::display()
{
    WKBundlePageForceRepaint(page());
}

static WKRetainPtr<WKDictionaryRef> createWKDictionary(std::initializer_list<std::pair<const char*, WKRetainPtr<WKTypeRef>>> pairs)
{
    Vector<WKStringRef> keys;
    Vector<WKTypeRef> values;
    Vector<WKRetainPtr<WKStringRef>> strings;
    for (auto& pair : pairs) {
        auto key = toWK(pair.first);
        keys.append(key.get());
        values.append(pair.second.get());
        strings.append(WTFMove(key));
    }
    return adoptWK(WKDictionaryCreate(keys.span().data(), values.span().data(), keys.size()));
}

template<typename T> static WKRetainPtr<WKTypeRef> postSynchronousMessageWithReturnValue(const char* name, const WKRetainPtr<T>& value)
{
    WKTypeRef rawReturnValue = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK(name).get(), value.get(), &rawReturnValue);
    return adoptWK(rawReturnValue);
}

template<typename T> static bool postSynchronousMessageReturningBoolean(const char* name, const WKRetainPtr<T>& value)
{
    return booleanValue(postSynchronousMessageWithReturnValue(name, value));
}

static bool postSynchronousMessageReturningBoolean(const char* name)
{
    return postSynchronousMessageReturningBoolean(name, WKRetainPtr<WKTypeRef> { });
}

template<typename T> static WKRetainPtr<WKTypeRef> postSynchronousPageMessageWithReturnValue(const char* name, const WKRetainPtr<T>& value)
{
    WKTypeRef rawReturnValue = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page(), toWK(name).get(), value.get(), &rawReturnValue);
    return adoptWK(rawReturnValue);
}

template<typename T> static bool postSynchronousPageMessageReturningBoolean(const char* name, const WKRetainPtr<T>& value)
{
    return booleanValue(postSynchronousPageMessageWithReturnValue(name, value));
}

static bool postSynchronousPageMessageReturningBoolean(const char* name)
{
    return postSynchronousPageMessageReturningBoolean(name, WKRetainPtr<WKTypeRef> { });
}

static bool postSynchronousPageMessageReturningBoolean(const char* name, JSStringRef string)
{
    return postSynchronousPageMessageReturningBoolean(name, toWK(string));
}

template<typename T> static uint64_t postSynchronousPageMessageReturningUInt64(const char* name, const WKRetainPtr<T>& value)
{
    return uint64Value(postSynchronousPageMessageWithReturnValue(name, value));
}

static uint64_t postSynchronousMessageReturningUInt64(const char* name)
{
    return uint64Value(postSynchronousMessageWithReturnValue(name, WKRetainPtr<WKTypeRef> { }));
}

static bool postSynchronousPageMessageReturningUInt64(const char* name, JSStringRef string)
{
    return postSynchronousPageMessageReturningUInt64(name, toWK(string));
}

bool TestRunner::shouldDumpPixels() const
{
    return postSynchronousMessageReturningBoolean("GetDumpPixels");
}

void TestRunner::setDumpPixels(bool dumpPixels)
{
    postSynchronousMessage("SetDumpPixels", dumpPixels);
}

void TestRunner::dumpAsText(bool dumpPixels)
{
    if (whatToDump() < WhatToDump::MainFrameText)
        setWhatToDump(WhatToDump::MainFrameText);
    setDumpPixels(dumpPixels);
}
    
WhatToDump TestRunner::whatToDump() const
{
    return static_cast<WhatToDump>(postSynchronousMessageReturningUInt64("GetWhatToDump"));
}

void TestRunner::setWhatToDump(WhatToDump whatToDump)
{
    postSynchronousMessage("SetWhatToDump", static_cast<uint64_t>(whatToDump));
}

void TestRunner::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    InjectedBundle::singleton().setCustomPolicyDelegate(enabled, permissive);
}

void TestRunner::skipPolicyDelegateNotifyDone()
{
    postMessage("SkipPolicyDelegateNotifyDone");
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
    if (!InjectedBundle::singleton().isTestRunning()) {
        [[maybe_unused]] WTF::String testURL = "(unknown test)"_s;
        if (WKURLRef url = m_testURL.get())
            testURL = toWTFString(adoptWK(WKURLCopyString(url)));
        LOG_ERROR("(%s) testRunner.waitUntilDone() called after test has terminated. Possibly an async handler was not awaited.", testURL.utf8().data());
        return;
    }

    setWaitUntilDone(true);
}

void TestRunner::setWaitUntilDone(bool value)
{
    postSynchronousMessage("SetWaitUntilDone", value);
}

bool TestRunner::shouldWaitUntilDone() const
{
    return postSynchronousMessageReturningBoolean("GetWaitUntilDone");
}

void TestRunner::notifyDone()
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;
    if (!postSynchronousMessageReturningBoolean("ResolveNotifyDone"))
        return;
    if (!injectedBundle.page())
        return;
    injectedBundle.page()->notifyDone();
}

void TestRunner::forceImmediateCompletion()
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;
    if (!postSynchronousMessageReturningBoolean("ResolveForceImmediateCompletion"))
        return;
    if (!injectedBundle.page())
        return;
    injectedBundle.page()->forceImmediateCompletion();
}

void TestRunner::setShouldDumpFrameLoadCallbacks(bool value)
{
    postSynchronousMessage("SetDumpFrameLoadCallbacks", value);
}

bool TestRunner::shouldDumpFrameLoadCallbacks()
{
    return postSynchronousMessageReturningBoolean("GetDumpFrameLoadCallbacks");
}

unsigned TestRunner::imageCountInGeneralPasteboard() const
{
    return InjectedBundle::singleton().imageCountInGeneralPasteboard();
}

void TestRunner::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    WKBundlePageAddUserScript(page(), toWK(source).get(),
        (runAtStart ? kWKInjectAtDocumentStart : kWKInjectAtDocumentEnd),
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void TestRunner::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    WKBundlePageAddUserStyleSheet(page(), toWK(source).get(),
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void TestRunner::keepWebHistory()
{
    InjectedBundle::singleton().postSetAddsVisitedLinks(true);
}

void TestRunner::execCommand(JSStringRef command, JSStringRef, JSStringRef value)
{
    postSynchronousPageMessage("ExecuteCommand", createWKDictionary({
        { "Command", toWK(command) },
        { "Value", toWK(value) },
    }));
}

void TestRunner::replaceFindMatchesAtIndices(JSContextRef context, JSValueRef matchIndicesAsValue, JSStringRef replacementText, bool selectionOnly)
{
    auto& bundle = InjectedBundle::singleton();
    auto matchIndicesObject = JSValueToObject(context, matchIndicesAsValue, 0);
    auto length = arrayLength(context, matchIndicesObject);

    auto indices = adoptWK(WKMutableArrayCreate());
    for (unsigned i = 0; i < length; ++i) {
        auto value = JSObjectGetPropertyAtIndex(context, matchIndicesObject, i, nullptr);
        if (!JSValueIsNumber(context, value))
            continue;

        auto index = adoptWK(WKUInt64Create(JSValueToNumber(context, value, nullptr)));
        WKArrayAppendItem(indices.get(), index.get());
    }
    WKBundlePageReplaceStringMatches(bundle.page()->page(), indices.get(), toWK(replacementText).get(), selectionOnly);
}

void TestRunner::clearAllDatabases()
{
    postSynchronousMessage("DeleteAllIndexedDatabases", true);
}

void TestRunner::setDatabaseQuota(uint64_t quota)
{
    return WKBundleSetDatabaseQuota(InjectedBundle::singleton().bundle(), quota);
}

void TestRunner::syncLocalStorage()
{
    postSynchronousMessage("SyncLocalStorage", true);
}

bool TestRunner::isCommandEnabled(JSStringRef name)
{
    return postSynchronousPageMessageReturningBoolean("IsCommandEnabled", toWK(name));
}

void TestRunner::preventPopupWindows()
{
    postSynchronousMessage("SetCanOpenWindows", false);
}

void TestRunner::setCustomUserAgent(JSStringRef userAgent)
{
    postSynchronousMessage("SetCustomUserAgent", toWK(userAgent));
}

void TestRunner::setAllowsAnySSLCertificate(bool enabled)
{
    InjectedBundle::singleton().setAllowsAnySSLCertificate(enabled);
    postSynchronousPageMessage("SetAllowsAnySSLCertificate", enabled);
}

void TestRunner::setBackgroundFetchPermission(bool enabled)
{
    postSynchronousPageMessage("SetBackgroundFetchPermission", enabled);
}

JSRetainPtr<JSStringRef>  TestRunner::lastAddedBackgroundFetchIdentifier() const
{
    auto identifier = InjectedBundle::singleton().lastAddedBackgroundFetchIdentifier();
    return WKStringCopyJSString(identifier.get());
}

JSRetainPtr<JSStringRef>  TestRunner::lastRemovedBackgroundFetchIdentifier() const
{
    auto identifier = InjectedBundle::singleton().lastRemovedBackgroundFetchIdentifier();
    return WKStringCopyJSString(identifier.get());
}

JSRetainPtr<JSStringRef> TestRunner::lastUpdatedBackgroundFetchIdentifier() const
{
    auto identifier = InjectedBundle::singleton().lastUpdatedBackgroundFetchIdentifier();
    return WKStringCopyJSString(identifier.get());
}

JSRetainPtr<JSStringRef> TestRunner::backgroundFetchState(JSStringRef identifier)
{
    auto state = InjectedBundle::singleton().backgroundFetchState(toWK(identifier).get());
    return WKStringCopyJSString(state.get());
}

void TestRunner::setShouldSwapToEphemeralSessionOnNextNavigation(bool shouldSwap)
{
    postSynchronousPageMessage("SetShouldSwapToEphemeralSessionOnNextNavigation", shouldSwap);
}

void TestRunner::setShouldSwapToDefaultSessionOnNextNavigation(bool shouldSwap)
{
    postSynchronousPageMessage("SetShouldSwapToDefaultSessionOnNextNavigation", shouldSwap);
}

void TestRunner::addOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WKBundleAddOriginAccessAllowListEntry(InjectedBundle::singleton().bundle(), toWK(sourceOrigin).get(), toWK(destinationProtocol).get(), toWK(destinationHost).get(), allowDestinationSubdomains);
}

void TestRunner::removeOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WKBundleRemoveOriginAccessAllowListEntry(InjectedBundle::singleton().bundle(), toWK(sourceOrigin).get(), toWK(destinationProtocol).get(), toWK(destinationHost).get(), allowDestinationSubdomains);
}

bool TestRunner::isPageBoxVisible(JSContextRef context, int pageIndex)
{
    auto frame = WKBundleFrameForJavaScriptContext(context);
    auto& injectedBundle = InjectedBundle::singleton();
    return WKBundleIsPageBoxVisible(injectedBundle.bundle(), frame, pageIndex);
}

void TestRunner::setValueForUser(JSContextRef context, JSValueRef element, JSStringRef value)
{
    if (!element || !JSValueIsObject(context, element))
        return;

    WKBundleNodeHandleSetHTMLInputElementValueForUser(adoptWK(WKBundleNodeHandleCreate(context, const_cast<JSObjectRef>(element))).get(), toWK(value).get());
}

void TestRunner::setAudioResult(JSContextRef context, JSValueRef data)
{
    auto& injectedBundle = InjectedBundle::singleton();
    // FIXME (123058): Use a JSC API to get buffer contents once such is exposed.
    injectedBundle.setAudioResult(adoptWK(WKBundleCreateWKDataFromUInt8Array(injectedBundle.bundle(), context, data)).get());
    setWhatToDump(WhatToDump::Audio);
    setDumpPixels(false);
}

unsigned TestRunner::windowCount()
{
    return InjectedBundle::singleton().pageCount();
}

void TestRunner::makeWindowObject(JSContextRef context)
{
    setGlobalObjectProperty(context, "testRunner", this);
}

void TestRunner::showWebInspector()
{
    postMessage("ShowWebInspector");
}

void TestRunner::closeWebInspector()
{
    WKBundlePageCloseInspectorForTest(page());
}

void TestRunner::evaluateInWebInspector(JSStringRef script)
{
    WKBundlePageEvaluateScriptInInspectorForTest(page(), toWK(script).get());
}

using WorldMap = WTF::HashMap<unsigned, WKRetainPtr<WKBundleScriptWorldRef>>;
static WorldMap& worldMap()
{
    static WorldMap& map = *new WorldMap;
    return map;
}

unsigned TestRunner::worldIDForWorld(WKBundleScriptWorldRef world)
{
    // FIXME: This is the anti-pattern of iterating an entire map. Typically we use a pair of maps or just a vector in a case like this.
    for (auto& mapEntry : worldMap()) {
        if (mapEntry.value == world)
            return mapEntry.key;
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

    auto frame = WKBundleFrameForJavaScriptContext(context);
    ASSERT(frame);

    JSGlobalContextRef jsContext = WKBundleFrameGetJavaScriptContextForWorld(frame, world.get());
    JSEvaluateScript(jsContext, script, 0, 0, 0, 0); 
}

void TestRunner::setPOSIXLocale(JSStringRef locale)
{
    char localeBuf[32];
    JSStringGetUTF8CString(locale, localeBuf, sizeof(localeBuf));
    setlocale(LC_ALL, localeBuf);
}

void TestRunner::setTextDirection(JSContextRef context, JSStringRef direction)
{
    auto frame = WKBundleFrameForJavaScriptContext(context);
    return WKBundleFrameSetTextDirection(frame, toWK(direction).get());
}
    
void TestRunner::setShouldStayOnPageAfterHandlingBeforeUnload(bool shouldStayOnPage)
{
    InjectedBundle::singleton().postNewBeforeUnloadReturnValue(!shouldStayOnPage);
}

bool TestRunner::didReceiveServerRedirectForProvisionalNavigation() const
{
    return postSynchronousPageMessageReturningBoolean("DidReceiveServerRedirectForProvisionalNavigation");
}

void TestRunner::clearDidReceiveServerRedirectForProvisionalNavigation()
{
    postSynchronousPageMessage("ClearDidReceiveServerRedirectForProvisionalNavigation");
}

void TestRunner::setPageVisibility(JSStringRef state)
{
    InjectedBundle::singleton().setHidden(JSStringIsEqualToUTF8CString(state, "hidden"));
}

void TestRunner::resetPageVisibility()
{
    InjectedBundle::singleton().setHidden(false);
}

struct Callback {
    JSObjectRef callback;
    JSRetainPtr<JSGlobalContextRef> context;
};

using CallbackMap = WTF::HashMap<unsigned, Callback>;
static CallbackMap& callbackMap()
{
    static CallbackMap& map = *new CallbackMap;
    return map;
}

enum {
    TextDidChangeInTextFieldCallbackID = 1,
    TextFieldDidBeginEditingCallbackID,
    TextFieldDidEndEditingCallbackID,
};

static void cacheTestRunnerCallback(JSContextRef context, unsigned index, JSValueRef callback)
{
    if (!callback)
        return;
    if (!JSValueIsObject(context, callback))
        return;
    if (callbackMap().contains(index)) {
        InjectedBundle::singleton().outputText(makeString("FAIL: Tried to install a second TestRunner callback for the same event (id "_s, index, ")\n\n"_s));
        return;
    }
    JSValueProtect(context, callback);
    callbackMap().add(index, Callback { const_cast<JSObjectRef>(callback), JSContextGetGlobalContext(context) });
}

static void callTestRunnerCallback(unsigned index, JSStringRef argument = nullptr)
{
    auto callback = callbackMap().take(index);
    if (!callback.callback)
        return;
    auto context = callback.context.get();

    size_t argumentCount = 0;
    JSValueRef argumentValue { nullptr };
    JSValueRef* arguments { nullptr };
    if (argument) {
        argumentCount = 1;
        argumentValue = JSValueMakeString(context, argument);
        arguments = &argumentValue;
    }

    JSObjectCallAsFunction(context, callback.callback, JSContextGetGlobalObject(context), argumentCount, arguments, 0);
    JSValueUnprotect(context, callback.callback);
}

void TestRunner::clearTestRunnerCallbacks()
{
    for (auto& value : callbackMap().values()) {
        auto context = value.context.get();
        JSValueUnprotect(context, JSValueToObject(context, value.callback, nullptr));
    }
    callbackMap().clear();
}

void TestRunner::accummulateLogsForChannel(JSStringRef)
{
    // FIXME: Implement getting the call to all processes.
}

void TestRunner::setWindowIsKey(bool isKey)
{
    InjectedBundle::singleton().postSetWindowIsKey(isKey);
}

void TestRunner::setViewSize(double width, double height)
{
    InjectedBundle::singleton().postSetViewSize(width, height);
}

void TestRunner::setAlwaysAcceptCookies(bool accept)
{
    postSynchronousMessage("SetAlwaysAcceptCookies", accept);
}

void TestRunner::setOnlyAcceptFirstPartyCookies(bool accept)
{
    postSynchronousMessage("SetOnlyAcceptFirstPartyCookies", accept);
}

double TestRunner::preciseTime()
{
    return WallTime::now().secondsSinceEpoch().seconds();
}

void TestRunner::setRenderTreeDumpOptions(unsigned short options)
{
    m_renderTreeDumpOptions = options;
}

void TestRunner::setUserStyleSheetEnabled(bool enabled)
{
    m_userStyleSheetEnabled = enabled;

    auto emptyString = toWK("");
    auto location = enabled ? m_userStyleSheetLocation.get() : emptyString.get();
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleSetUserStyleSheetLocationForTesting(injectedBundle.bundle(), location);
}

void TestRunner::setUserStyleSheetLocation(JSStringRef location)
{
    m_userStyleSheetLocation = toWK(location);

    if (m_userStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void TestRunner::setTabKeyCyclesThroughElements(bool enabled)
{
    WKBundleSetTabKeyCyclesThroughElements(InjectedBundle::singleton().bundle(), page(), enabled);
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
    WKBundleSetAsynchronousSpellCheckingEnabledForTesting(injectedBundle.bundle(), enabled);
}

void TestRunner::grantWebNotificationPermission(JSStringRef origin)
{
    postSynchronousPageMessageWithReturnValue("GrantNotificationPermission", toWK(origin));
}

void TestRunner::denyWebNotificationPermission(JSStringRef origin)
{
    postSynchronousPageMessageWithReturnValue("DenyNotificationPermission", toWK(origin));
}

void TestRunner::denyWebNotificationPermissionOnPrompt(JSStringRef origin)
{
    postSynchronousPageMessageWithReturnValue("DenyNotificationPermissionOnPrompt", toWK(origin));
}

void TestRunner::removeAllWebNotificationPermissions()
{
    WKBundleRemoveAllWebNotificationPermissions(InjectedBundle::singleton().bundle(), page());
}

void TestRunner::simulateWebNotificationClick(JSContextRef context, JSValueRef notification)
{
    auto& injectedBundle = InjectedBundle::singleton();

    auto notificationID = adoptWK(WKBundleCopyWebNotificationID(injectedBundle.bundle(), context, notification));
    injectedBundle.postSimulateWebNotificationClick(notificationID.get());
}

void TestRunner::simulateWebNotificationClickForServiceWorkerNotifications()
{
    InjectedBundle::singleton().postSimulateWebNotificationClickForServiceWorkerNotifications();
}

JSRetainPtr<JSStringRef> TestRunner::getBackgroundFetchIdentifier()
{
    auto identifier = InjectedBundle::singleton().getBackgroundFetchIdentifier();
    return WKStringCopyJSString(identifier.get());
}

void TestRunner::abortBackgroundFetch(JSStringRef identifier)
{
    postSynchronousPageMessageWithReturnValue("AbortBackgroundFetch", toWK(identifier));
}

void TestRunner::pauseBackgroundFetch(JSStringRef identifier)
{
    postSynchronousPageMessageWithReturnValue("PauseBackgroundFetch", toWK(identifier));
}

void TestRunner::resumeBackgroundFetch(JSStringRef identifier)
{
    postSynchronousPageMessageWithReturnValue("ResumeBackgroundFetch", toWK(identifier));
}

void TestRunner::simulateClickBackgroundFetch(JSStringRef identifier)
{
    postSynchronousPageMessageWithReturnValue("SimulateClickBackgroundFetch", toWK(identifier));
}

void TestRunner::setGeolocationPermission(bool enabled)
{
    // FIXME: This should be done by frame.
    InjectedBundle::singleton().setGeolocationPermission(enabled);
}

void TestRunner::setScreenWakeLockPermission(bool enabled)
{
    InjectedBundle::singleton().setScreenWakeLockPermission(enabled);
}

bool TestRunner::isGeolocationProviderActive()
{
    return InjectedBundle::singleton().isGeolocationProviderActive();
}

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy, std::optional<double> altitude, std::optional<double> altitudeAccuracy, std::optional<double> heading, std::optional<double> speed, std::optional<double> floorLevel)
{
    InjectedBundle::singleton().setMockGeolocationPosition(latitude, longitude, accuracy, altitude, altitudeAccuracy, heading, speed, floorLevel);
}

void TestRunner::setMockGeolocationPositionUnavailableError(JSStringRef message)
{
    InjectedBundle::singleton().setMockGeolocationPositionUnavailableError(toWK(message).get());
}

void TestRunner::setCameraPermission(bool enabled)
{
    InjectedBundle::singleton().setCameraPermission(enabled);
}

void TestRunner::setMicrophonePermission(bool enabled)
{
    InjectedBundle::singleton().setMicrophonePermission(enabled);
}

void TestRunner::setUserMediaPermission(bool enabled)
{
    InjectedBundle::singleton().setCameraPermission(enabled);
    InjectedBundle::singleton().setMicrophonePermission(enabled);
}

void TestRunner::resetUserMediaPermission()
{
    InjectedBundle::singleton().resetUserMediaPermission();
}

bool TestRunner::isDoingMediaCapture() const
{
    return postSynchronousPageMessageReturningBoolean("IsDoingMediaCapture");
}

void TestRunner::delayUserMediaRequestDecision()
{
    InjectedBundle::singleton().delayUserMediaRequestDecision();
}

unsigned TestRunner::userMediaPermissionRequestCount() const
{
    return InjectedBundle::singleton().userMediaPermissionRequestCount();
}

void TestRunner::resetUserMediaPermissionRequestCount()
{
    InjectedBundle::singleton().resetUserMediaPermissionRequestCount();
}

bool TestRunner::callShouldCloseOnWebView(JSContextRef context)
{
    auto frame = WKBundleFrameForJavaScriptContext(context);
    return WKBundleFrameCallShouldCloseOnWebView(frame);
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
    InjectedBundle::singleton().queueLoad(toWK(url).get(), toWK(target).get(), shouldOpenExternalURLs);
}

void TestRunner::queueLoadHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL)
{
    auto baseURLWK = baseURL ? toWK(baseURL) : WKRetainPtr<WKStringRef>();
    auto unreachableURLWK = unreachableURL ? toWK(unreachableURL) : WKRetainPtr<WKStringRef>();
    InjectedBundle::singleton().queueLoadHTMLString(toWK(content).get(), baseURLWK.get(), unreachableURLWK.get());
}

void TestRunner::queueReload()
{
    InjectedBundle::singleton().queueReload();
}

void TestRunner::queueLoadingScript(JSStringRef script)
{
    InjectedBundle::singleton().queueLoadingScript(toWK(script).get());
}

void TestRunner::queueNonLoadingScript(JSStringRef script)
{
    InjectedBundle::singleton().queueNonLoadingScript(toWK(script).get());
}

bool TestRunner::secureEventInputIsEnabled() const
{
    return postSynchronousPageMessageReturningBoolean("SecureEventInputIsEnabled");
}

JSValueRef TestRunner::failNextNewCodeBlock(JSContextRef context)
{
    return JSC::failNextNewCodeBlock(context);
}

JSValueRef TestRunner::numberOfDFGCompiles(JSContextRef context, JSValueRef function)
{
    return JSC::numberOfDFGCompiles(context, function);
}

JSValueRef TestRunner::neverInlineFunction(JSContextRef context, JSValueRef function)
{
    return JSC::setNeverInline(context, function);
}

void TestRunner::terminateGPUProcess()
{
    postSynchronousPageMessage("TerminateGPUProcess");
}

void TestRunner::terminateNetworkProcess()
{
    postSynchronousPageMessage("TerminateNetworkProcess");
}

void TestRunner::terminateServiceWorkers()
{
    postSynchronousPageMessage("TerminateServiceWorkers");
}

void TestRunner::setUseSeparateServiceWorkerProcess(bool value)
{
    postSynchronousPageMessage("SetUseSeparateServiceWorkerProcess", value);
}

void TestRunner::clearStatisticsDataForDomain(JSStringRef domain)
{
    postSynchronousMessage("ClearStatisticsDataForDomain", toWK(domain));
}

bool TestRunner::doesStatisticsDomainIDExistInDatabase(unsigned domainID)
{
    return postSynchronousPageMessageReturningBoolean("DoesStatisticsDomainIDExistInDatabase", createWKDictionary({
        { "DomainID", adoptWK(WKUInt64Create(domainID)) }
    }));
}

void TestRunner::setStatisticsEnabled(bool value)
{
    postSynchronousMessage("SetStatisticsEnabled", value);
}

bool TestRunner::isStatisticsEphemeral()
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsEphemeral");
}

void TestRunner::dumpResourceLoadStatistics()
{
    InjectedBundle::singleton().clearResourceLoadStatistics();
    postSynchronousPageMessage("dumpResourceLoadStatistics");
}

void TestRunner::dumpPolicyDelegateCallbacks()
{
    postMessage("DumpPolicyDelegateCallbacks");
}

bool TestRunner::isStatisticsPrevalentResource(JSStringRef hostName)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsPrevalentResource", hostName);
}

bool TestRunner::isStatisticsVeryPrevalentResource(JSStringRef hostName)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsVeryPrevalentResource", hostName);
}

bool TestRunner::isStatisticsRegisteredAsSubresourceUnder(JSStringRef subresourceHost, JSStringRef topFrameHost)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsRegisteredAsSubresourceUnder", createWKDictionary({
        { "SubresourceHost", toWK(subresourceHost) },
        { "TopFrameHost", toWK(topFrameHost) },
    }));
}

bool TestRunner::isStatisticsRegisteredAsSubFrameUnder(JSStringRef subFrameHost, JSStringRef topFrameHost)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsRegisteredAsSubFrameUnder", createWKDictionary({
        { "SubFrameHost", toWK(subFrameHost) },
        { "TopFrameHost", toWK(topFrameHost) },
    }));
}

bool TestRunner::isStatisticsRegisteredAsRedirectingTo(JSStringRef hostRedirectedFrom, JSStringRef hostRedirectedTo)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsRegisteredAsRedirectingTo", createWKDictionary({
        { "HostRedirectedFrom", toWK(hostRedirectedFrom) },
        { "HostRedirectedTo", toWK(hostRedirectedTo) },
    }));
}

bool TestRunner::isStatisticsHasHadUserInteraction(JSStringRef hostName)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsHasHadUserInteraction", hostName);
}

bool TestRunner::isStatisticsOnlyInDatabaseOnce(JSStringRef subHost, JSStringRef topHost)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsOnlyInDatabaseOnce", createWKDictionary({
        { "SubHost", toWK(subHost) },
        { "TopHost", toWK(topHost) },
    }));
}

void TestRunner::setStatisticsGrandfathered(JSStringRef hostName, bool value)
{
    postSynchronousMessage("SetStatisticsGrandfathered", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }));
}

bool TestRunner::isStatisticsGrandfathered(JSStringRef hostName)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsGrandfathered", hostName);
}

void TestRunner::setStatisticsSubframeUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName)
{
    postSynchronousMessage("SetStatisticsSubframeUnderTopFrameOrigin", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "TopFrameHostName", toWK(topFrameHostName) },
    }));
}

void TestRunner::setStatisticsSubresourceUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName)
{
    postSynchronousMessage("SetStatisticsSubresourceUnderTopFrameOrigin", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "TopFrameHostName", toWK(topFrameHostName) },
    }));
}

void TestRunner::setStatisticsSubresourceUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo)
{
    postSynchronousMessage("SetStatisticsSubresourceUniqueRedirectTo", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "HostNameRedirectedTo", toWK(hostNameRedirectedTo) },
    }));
}


void TestRunner::setStatisticsSubresourceUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom)
{
    postSynchronousMessage("SetStatisticsSubresourceUniqueRedirectFrom", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "HostNameRedirectedFrom", toWK(hostNameRedirectedFrom) },
    }));
}

void TestRunner::setStatisticsTopFrameUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo)
{
    postSynchronousMessage("SetStatisticsTopFrameUniqueRedirectTo", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "HostNameRedirectedTo", toWK(hostNameRedirectedTo) },
    }));
}

void TestRunner::setStatisticsTopFrameUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom)
{
    postSynchronousMessage("SetStatisticsTopFrameUniqueRedirectFrom", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "HostNameRedirectedFrom", toWK(hostNameRedirectedFrom) },
    }));
}

void TestRunner::setStatisticsCrossSiteLoadWithLinkDecoration(JSStringRef fromHost, JSStringRef toHost, bool wasFiltered)
{
    postSynchronousMessage("SetStatisticsCrossSiteLoadWithLinkDecoration", createWKDictionary({
        { "FromHost", toWK(fromHost) },
        { "ToHost", toWK(toHost) },
        { "WasFiltered", adoptWK(WKBooleanCreate(wasFiltered)) },
    }));
}

void TestRunner::setStatisticsTimeToLiveUserInteraction(double seconds)
{
    postSynchronousMessage("SetStatisticsTimeToLiveUserInteraction", seconds);
}

void TestRunner::statisticsNotifyObserver(JSContextRef context, JSValueRef callback)
{
    auto globalContext = JSContextGetGlobalContext(context);
    JSValueProtect(globalContext, callback);
    InjectedBundle::singleton().statisticsNotifyObserver([callback, globalContext = JSRetainPtr { globalContext }] {
        JSContextRef context = globalContext.get();
        JSObjectCallAsFunction(context, JSValueToObject(context, callback, nullptr), JSContextGetGlobalObject(context), 0, nullptr, nullptr);
        JSValueUnprotect(context, callback);
    });
}

void TestRunner::setStatisticsTimeAdvanceForTesting(double value)
{
    postSynchronousMessage("StatisticsSetTimeAdvanceForTesting", value);
}

void TestRunner::setStatisticsIsRunningTest(bool value)
{
    postSynchronousMessage("StatisticsSetIsRunningTest", value);
}

void TestRunner::setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    postSynchronousMessage("StatisticsShouldClassifyResourcesBeforeDataRecordsRemoval", value);
}

void TestRunner::setStatisticsMinimumTimeBetweenDataRecordsRemoval(double seconds)
{
    postSynchronousMessage("SetStatisticsMinimumTimeBetweenDataRecordsRemoval", seconds);
}

void TestRunner::setStatisticsGrandfatheringTime(double seconds)
{
    postSynchronousMessage("SetStatisticsGrandfatheringTime", seconds);
}

void TestRunner::setStatisticsMaxStatisticsEntries(unsigned entries)
{
    postSynchronousMessage("SetMaxStatisticsEntries", entries);
}
    
void TestRunner::setStatisticsPruneEntriesDownTo(unsigned entries)
{
    postSynchronousMessage("SetPruneEntriesDownTo", entries);
}

bool TestRunner::isStatisticsHasLocalStorage(JSStringRef hostName)
{
    return postSynchronousPageMessageReturningBoolean("IsStatisticsHasLocalStorage", hostName);
}

void TestRunner::setStatisticsCacheMaxAgeCap(double seconds)
{
    postSynchronousMessage("SetStatisticsCacheMaxAgeCap", seconds);
}

bool TestRunner::hasStatisticsIsolatedSession(JSStringRef hostName)
{
    return postSynchronousPageMessageReturningBoolean("HasStatisticsIsolatedSession", hostName);
}

void TestRunner::installTextDidChangeInTextFieldCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, TextDidChangeInTextFieldCallbackID, callback);
}

void TestRunner::textDidChangeInTextFieldCallback()
{
    callTestRunnerCallback(TextDidChangeInTextFieldCallbackID);
}

void TestRunner::installTextFieldDidBeginEditingCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, TextFieldDidBeginEditingCallbackID, callback);
}

void TestRunner::textFieldDidBeginEditingCallback()
{
    callTestRunnerCallback(TextFieldDidBeginEditingCallbackID);
}

void TestRunner::installTextFieldDidEndEditingCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, TextFieldDidEndEditingCallbackID, callback);
}

void TestRunner::textFieldDidEndEditingCallback()
{
    callTestRunnerCallback(TextFieldDidEndEditingCallbackID);
}

void TestRunner::setRequestStorageAccessThrowsExceptionUntilReload(bool enabled)
{
    postSynchronousPageMessage("SetRequestStorageAccessThrowsExceptionUntilReload", enabled);
}

void TestRunner::reloadFromOrigin()
{
    InjectedBundle::singleton().reloadFromOrigin();
}

void TestRunner::addMockMediaDevice(JSStringRef persistentId, JSStringRef label, const char* type, WKDictionaryRef properties)
{
    postSynchronousMessage("AddMockMediaDevice", createWKDictionary({
        { "PersistentID", toWK(persistentId) },
        { "Label", toWK(label) },
        { "Type", toWK(type) },
        { "Properties", properties },
    }));
}

static WKRetainPtr<WKDictionaryRef> captureDeviceProperties(JSContextRef context, JSValueRef properties)
{
    if (JSValueGetType(context, properties) == kJSTypeUndefined)
        return { };

    Vector<WKRetainPtr<WKStringRef>> strings;
    Vector<WKStringRef> keys;
    Vector<WKTypeRef> values;

    if (auto object = JSValueToObject(context, properties, nullptr)) {
        JSPropertyNameArrayRef propertyNameArray = JSObjectCopyPropertyNames(context, object);
        size_t length = JSPropertyNameArrayGetCount(propertyNameArray);

        for (size_t i = 0; i < length; ++i) {
            JSStringRef jsPropertyName = JSPropertyNameArrayGetNameAtIndex(propertyNameArray, i);
            auto jsPropertyValue = JSObjectGetProperty(context, object, jsPropertyName, 0);

            auto propertyName = toWK(jsPropertyName);
            auto propertyValue = toWKString(context, jsPropertyValue);

            keys.append(propertyName.get());
            values.append(propertyValue.get());
            strings.append(WTFMove(propertyName));
            strings.append(WTFMove(propertyValue));
        }
        JSPropertyNameArrayRelease(propertyNameArray);
    }

    return adoptWK(WKDictionaryCreate(keys.span().data(), values.span().data(), keys.size()));
}

void TestRunner::addMockCameraDevice(JSContextRef context, JSStringRef persistentId, JSStringRef label, JSValueRef properties)
{
    addMockMediaDevice(persistentId, label, "camera", captureDeviceProperties(context, properties).get());
}

void TestRunner::addMockMicrophoneDevice(JSContextRef context, JSStringRef persistentId, JSStringRef label, JSValueRef properties)
{
    addMockMediaDevice(persistentId, label, "microphone", captureDeviceProperties(context, properties).get());
}

void TestRunner::addMockScreenDevice(JSStringRef persistentId, JSStringRef label)
{
    addMockMediaDevice(persistentId, label, "screen", nullptr);
}

void TestRunner::clearMockMediaDevices()
{
    postSynchronousMessage("ClearMockMediaDevices");
}

void TestRunner::removeMockMediaDevice(JSStringRef persistentId)
{
    postSynchronousMessage("RemoveMockMediaDevice", toWK(persistentId));
}

void TestRunner::setMockMediaDeviceIsEphemeral(JSStringRef persistentId, bool isEphemeral)
{
    postSynchronousMessage("SetMockMediaDeviceIsEphemeral", createWKDictionary({
        { "PersistentID", toWK(persistentId) },
        { "IsEphemeral", adoptWK(WKBooleanCreate(isEphemeral)) },
    }));
}

void TestRunner::resetMockMediaDevices()
{
    postSynchronousMessage("ResetMockMediaDevices");
}

void TestRunner::setMockCameraOrientation(unsigned rotation, JSStringRef persistentId)
{
    postSynchronousMessage("SetMockCameraRotation", createWKDictionary({
        { "Rotation", adoptWK(WKUInt64Create(rotation)) },
        { "PersistentID", toWK(persistentId) },
    }));
}

bool TestRunner::isMockRealtimeMediaSourceCenterEnabled()
{
    return postSynchronousMessageReturningBoolean("IsMockRealtimeMediaSourceCenterEnabled");
}

void TestRunner::setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted)
{
    postSynchronousMessage("SetMockCaptureDevicesInterrupted", createWKDictionary({
        { "camera", adoptWK(WKBooleanCreate(isCameraInterrupted)) },
        { "microphone", adoptWK(WKBooleanCreate(isMicrophoneInterrupted)) },
    }));
}

void TestRunner::triggerMockCaptureConfigurationChange(bool forCamera, bool forMicrophone, bool forDisplay)
{
    postSynchronousMessage("TriggerMockCaptureConfigurationChange", createWKDictionary({
        { "camera", adoptWK(WKBooleanCreate(forCamera)) },
        { "microphone", adoptWK(WKBooleanCreate(forMicrophone)) },
        { "display", adoptWK(WKBooleanCreate(forDisplay)) },
    }));
}

void TestRunner::setCaptureState(bool cameraState, bool microphoneState, bool displayState)
{
    postSynchronousMessage("SetCaptureState", createWKDictionary({
        { "camera", adoptWK(WKBooleanCreate(cameraState)) },
        { "microphone", adoptWK(WKBooleanCreate(microphoneState)) },
        { "display", adoptWK(WKBooleanCreate(displayState)) },
    }));
}

#if ENABLE(GAMEPAD)

void TestRunner::connectMockGamepad(unsigned index)
{
    postSynchronousMessage("ConnectMockGamepad", index);
}

void TestRunner::disconnectMockGamepad(unsigned index)
{
    postSynchronousMessage("DisconnectMockGamepad", index);
}

void TestRunner::setMockGamepadDetails(unsigned index, JSStringRef gamepadID, JSStringRef mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble, bool wasConnected)
{
    postSynchronousMessage("SetMockGamepadDetails", createWKDictionary({
        { "GamepadID", toWK(gamepadID) },
        { "Mapping", toWK(mapping) },
        { "GamepadIndex", adoptWK(WKUInt64Create(index)) },
        { "AxisCount", adoptWK(WKUInt64Create(axisCount)) },
        { "ButtonCount", adoptWK(WKUInt64Create(buttonCount)) },
        { "SupportsDualRumble", adoptWK(WKBooleanCreate(supportsDualRumble)) },
        { "WasConnected", adoptWK(WKBooleanCreate(wasConnected)) },
    }));
}

void TestRunner::setMockGamepadAxisValue(unsigned index, unsigned axisIndex, double value)
{
    postSynchronousMessage("SetMockGamepadAxisValue", createWKDictionary({
        { "GamepadIndex", adoptWK(WKUInt64Create(index)) },
        { "AxisIndex", adoptWK(WKUInt64Create(axisIndex)) },
        { "Value", adoptWK(WKDoubleCreate(value)) },
    }));
}

void TestRunner::setMockGamepadButtonValue(unsigned index, unsigned buttonIndex, double value)
{
    postSynchronousMessage("SetMockGamepadButtonValue", createWKDictionary({
        { "GamepadIndex", adoptWK(WKUInt64Create(index)) },
        { "ButtonIndex", adoptWK(WKUInt64Create(buttonIndex)) },
        { "Value", adoptWK(WKDoubleCreate(value)) },
    }));
}

#else

void TestRunner::connectMockGamepad(unsigned)
{
}

void TestRunner::disconnectMockGamepad(unsigned)
{
}

void TestRunner::setMockGamepadDetails(unsigned, JSStringRef, JSStringRef, unsigned, unsigned, bool, bool)
{
}

void TestRunner::setMockGamepadAxisValue(unsigned, unsigned, double)
{
}

void TestRunner::setMockGamepadButtonValue(unsigned, unsigned, double)
{
}

#endif // ENABLE(GAMEPAD)

void TestRunner::clearDOMCache(JSStringRef origin)
{
    postSynchronousMessage("ClearDOMCache", toWK(origin));
}

void TestRunner::clearStorage()
{
    postSynchronousMessage("ClearStorage");
}

void TestRunner::clearDOMCaches()
{
    postSynchronousMessage("ClearDOMCaches");
}

bool TestRunner::hasDOMCache(JSStringRef origin)
{
    return postSynchronousPageMessageReturningBoolean("HasDOMCache", origin);
}

uint64_t TestRunner::domCacheSize(JSStringRef origin)
{
    return postSynchronousPageMessageReturningUInt64("DOMCacheSize", origin);
}

void TestRunner::setAllowStorageQuotaIncrease(bool willIncrease)
{
    postSynchronousPageMessage("SetAllowStorageQuotaIncrease", willIncrease);
}

void TestRunner::setQuota(uint64_t quota)
{
    postSynchronousMessage("SetQuota", quota);
}

void TestRunner::setOriginQuotaRatioEnabled(bool enabled)
{
    postSynchronousPageMessage("SetOriginQuotaRatioEnabled", enabled);
}

void TestRunner::installFakeHelvetica(JSStringRef configuration)
{
    WTR::installFakeHelvetica(toWK(configuration).get());
}

size_t TestRunner::userScriptInjectedCount() const
{
    return InjectedBundle::singleton().userScriptInjectedCount();
}

void TestRunner::injectUserScript(JSStringRef script)
{
    postSynchronousMessage("InjectUserScript", toWK(script));
}

void TestRunner::setServiceWorkerFetchTimeout(double seconds)
{
    postSynchronousMessage("SetServiceWorkerFetchTimeout", seconds);
}

// WebAuthn
void TestRunner::addTestKeyToKeychain(JSStringRef privateKeyBase64, JSStringRef attrLabel, JSStringRef applicationTagBase64)
{
    postSynchronousMessage("AddTestKeyToKeychain", createWKDictionary({
        { "PrivateKey", toWK(privateKeyBase64) },
        { "AttrLabel", toWK(attrLabel) },
        { "ApplicationTag", toWK(applicationTagBase64) },
    }));
}

void TestRunner::cleanUpKeychain(JSStringRef attrLabel, JSStringRef applicationLabelBase64)
{
    if (!applicationLabelBase64) {
        postSynchronousMessage("CleanUpKeychain", createWKDictionary({
            { "AttrLabel", toWK(attrLabel) },
        }));
        return;
    }
    postSynchronousMessage("CleanUpKeychain", createWKDictionary({
        { "AttrLabel", toWK(attrLabel) },
        { "ApplicationLabel", toWK(applicationLabelBase64) },
    }));
}

unsigned long TestRunner::serverTrustEvaluationCallbackCallsCount()
{
    return postSynchronousMessageReturningUInt64("ServerTrustEvaluationCallbackCallsCount");
}

void TestRunner::dontForceRepaint() const
{
    postSynchronousMessage("DontForceRepaint");
}

void TestRunner::setShouldDismissJavaScriptAlertsAsynchronously(bool shouldDismissAsynchronously)
{
    postSynchronousMessage("ShouldDismissJavaScriptAlertsAsynchronously", shouldDismissAsynchronously);
}

void TestRunner::abortModal()
{
    postSynchronousMessage("AbortModal");
}

void TestRunner::dumpPrivateClickMeasurement()
{
    postSynchronousPageMessage("DumpPrivateClickMeasurement");
}

void TestRunner::setPrinting() const
{
    postSynchronousMessage("SetPrinting");
}

void TestRunner::clearMemoryCache()
{
    postSynchronousPageMessage("ClearMemoryCache");
}

void TestRunner::clearPrivateClickMeasurement()
{
    postSynchronousPageMessage("ClearPrivateClickMeasurement");
}

void TestRunner::clearPrivateClickMeasurementsThroughWebsiteDataRemoval()
{
    postSynchronousMessage("ClearPrivateClickMeasurementsThroughWebsiteDataRemoval");
}

void TestRunner::setPrivateClickMeasurementOverrideTimerForTesting(bool value)
{
    postSynchronousPageMessage("SetPrivateClickMeasurementOverrideTimerForTesting", value);
}

void TestRunner::markAttributedPrivateClickMeasurementsAsExpiredForTesting()
{
    postSynchronousPageMessage("MarkAttributedPrivateClickMeasurementsAsExpiredForTesting");
}

void TestRunner::setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value)
{
    postSynchronousPageMessage("SetPrivateClickMeasurementEphemeralMeasurementForTesting", value);
}

void TestRunner::simulatePrivateClickMeasurementSessionRestart()
{
    postSynchronousPageMessage("SimulatePrivateClickMeasurementSessionRestart");
}

void TestRunner::setPrivateClickMeasurementTokenPublicKeyURLForTesting(JSStringRef urlString)
{
    postSynchronousPageMessage("SetPrivateClickMeasurementTokenPublicKeyURLForTesting",
        adoptWK(WKURLCreateWithUTF8CString(toWTFString(urlString).utf8().data())));
}

void TestRunner::setPrivateClickMeasurementTokenSignatureURLForTesting(JSStringRef urlString)
{
    postSynchronousPageMessage("SetPrivateClickMeasurementTokenSignatureURLForTesting",
        adoptWK(WKURLCreateWithUTF8CString(toWTFString(urlString).utf8().data())));
}

void TestRunner::setPrivateClickMeasurementAttributionReportURLsForTesting(JSStringRef sourceURLString, JSStringRef destinationURLString)
{
    postSynchronousPageMessage("SetPrivateClickMeasurementAttributionReportURLsForTesting", createWKDictionary({
        { "SourceURLString", toWK(sourceURLString) },
        { "AttributeOnURLString", toWK(destinationURLString) },
    }));
}

void TestRunner::markPrivateClickMeasurementsAsExpiredForTesting()
{
    postSynchronousPageMessage("MarkPrivateClickMeasurementsAsExpiredForTesting");
}

void TestRunner::setPrivateClickMeasurementFraudPreventionValuesForTesting(JSStringRef unlinkableToken, JSStringRef secretToken, JSStringRef signature, JSStringRef keyID)
{
    postSynchronousMessage("SetPCMFraudPreventionValuesForTesting", createWKDictionary({
        { "UnlinkableToken", toWK(unlinkableToken) },
        { "SecretToken", toWK(secretToken) },
        { "Signature", toWK(signature) },
        { "KeyID", toWK(keyID) },
    }));
}

void TestRunner::setPrivateClickMeasurementAppBundleIDForTesting(JSStringRef appBundleID)
{
    postSynchronousPageMessage("SetPrivateClickMeasurementAppBundleIDForTesting",
        toWK(appBundleID));
}

bool TestRunner::hasAppBoundSession()
{
    return postSynchronousPageMessageReturningBoolean("HasAppBoundSession");
}

void TestRunner::clearAppBoundSession()
{
    postSynchronousMessage("ClearAppBoundSession");
}

bool TestRunner::didLoadAppInitiatedRequest()
{
    return postSynchronousPageMessageReturningBoolean("DidLoadAppInitiatedRequest");
}

bool TestRunner::didLoadNonAppInitiatedRequest()
{
    return postSynchronousPageMessageReturningBoolean("DidLoadNonAppInitiatedRequest");
}

void TestRunner::setIsSpeechRecognitionPermissionGranted(bool granted)
{
    postSynchronousPageMessage("SetIsSpeechRecognitionPermissionGranted", granted);
}

void TestRunner::setIsMediaKeySystemPermissionGranted(bool granted)
{
    postSynchronousPageMessage("SetIsMediaKeySystemPermissionGranted", granted);
}

void TestRunner::generateTestReport(JSContextRef context, JSStringRef message, JSStringRef group)
{
    auto frame = WKBundleFrameForJavaScriptContext(context);
    _WKBundleFrameGenerateTestReport(frame, toWK(message).get(), toWK(group).get());
}

void TestRunner::dumpBackForwardList()
{
    postSynchronousPageMessage("DumpBackForwardList");
}

bool TestRunner::shouldDumpBackForwardListsForAllWindows() const
{
    return postSynchronousPageMessageReturningBoolean("ShouldDumpBackForwardListsForAllWindows");
}

void TestRunner::dumpChildFrameScrollPositions()
{
    postSynchronousPageMessage("DumpChildFrameScrollPositions");
}

bool TestRunner::shouldDumpAllFrameScrollPositions() const
{
    return postSynchronousPageMessageReturningBoolean("ShouldDumpAllFrameScrollPositions");
}

void TestRunner::setHasMouseDeviceForTesting(bool hasMouseDevice)
{
    postSynchronousPageMessage("SetHasMouseDeviceForTesting", hasMouseDevice);
}

ALLOW_DEPRECATED_DECLARATIONS_END

} // namespace WTR
