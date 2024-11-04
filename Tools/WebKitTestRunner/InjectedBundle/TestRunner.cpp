/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

TestRunner::~TestRunner()
{
}

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

void TestRunner::displayAndTrackRepaints(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "DisplayAndTrackRepaints", callback);
}

static WKRetainPtr<WKDoubleRef> toWK(double value)
{
    return adoptWK(WKDoubleCreate(value));
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
    return adoptWK(WKDictionaryCreate(keys.data(), values.data(), keys.size()));
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
    RELEASE_ASSERT(InjectedBundle::singleton().isTestRunning());

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

static std::optional<WKFindOptions> findOptionsFromArray(JSContextRef context, JSValueRef optionsArrayAsValue)
{
    auto optionsArray = JSValueToObject(context, optionsArrayAsValue, nullptr);
    auto length = arrayLength(context, optionsArray);
    WKFindOptions options = 0;
    for (unsigned i = 0; i < length; ++i) {
        auto optionName = createJSString(context, JSObjectGetPropertyAtIndex(context, optionsArray, i, 0));
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

bool TestRunner::findString(JSContextRef context, JSStringRef target, JSValueRef optionsArrayAsValue)
{
    if (auto options = findOptionsFromArray(context, optionsArrayAsValue))
        return WKBundlePageFindString(page(), toWK(target).get(), *options);

    return false;
}

void TestRunner::findStringMatchesInPage(JSContextRef context, JSStringRef target, JSValueRef optionsArrayAsValue)
{
    if (auto options = findOptionsFromArray(context, optionsArrayAsValue)) {
        postPageMessage("FindStringMatches", createWKDictionary({
            { "String", toWK(target) },
            { "FindOptions", toWK(*options) },
        }));
    }
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

void TestRunner::clearBackForwardList(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "ClearBackForwardList", callback);
}

void TestRunner::makeWindowObject(JSContextRef context)
{
    setGlobalObjectProperty(context, "testRunner", this);
}

void TestRunner::showWebInspector()
{
    WKBundlePageShowInspectorForTest(page());
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
    DidBeginSwipeCallbackID = 1,
    WillEndSwipeCallbackID,
    DidEndSwipeCallbackID,
    DidRemoveSwipeSnapshotCallbackID,
    TextDidChangeInTextFieldCallbackID,
    TextFieldDidBeginEditingCallbackID,
    TextFieldDidEndEditingCallbackID,
    EnterFullscreenForElementCallbackID,
    ExitFullscreenForElementCallbackID,
    FirstUIScriptCallbackID = 100
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

void TestRunner::addChromeInputField(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "AddChromeInputField", callback);
}

void TestRunner::removeChromeInputField(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "RemoveChromeInputField", callback);
}

void TestRunner::setTextInChromeInputField(JSContextRef context, JSStringRef text, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "SetTextInChromeInputField", toWK(text), callback);
}

void TestRunner::selectChromeInputField(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "SelectChromeInputField", callback);
}

void TestRunner::getSelectedTextInChromeInputField(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "GetSelectedTextInChromeInputField", callback);
}

void TestRunner::focusWebView(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "FocusWebView", callback);
}

void TestRunner::setBackingScaleFactor(JSContextRef context, double backingScaleFactor, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "SetBackingScaleFactor", adoptWK(WKDoubleCreate(backingScaleFactor)), callback);
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

void TestRunner::removeAllCookies(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "RemoveAllCookies", callback);
}

void TestRunner::setEnterFullscreenForElementCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, EnterFullscreenForElementCallbackID, callback);
}

void TestRunner::callEnterFullscreenForElementCallback()
{
    callTestRunnerCallback(EnterFullscreenForElementCallbackID);
}

void TestRunner::setExitFullscreenForElementCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, ExitFullscreenForElementCallbackID, callback);
}

void TestRunner::callExitFullscreenForElementCallback()
{
    callTestRunnerCallback(ExitFullscreenForElementCallbackID);
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

void TestRunner::setUserMediaPersistentPermissionForOrigin(bool permission, JSStringRef origin, JSStringRef parentOrigin)
{
    InjectedBundle::singleton().setUserMediaPersistentPermissionForOrigin(permission, toWK(origin).get(), toWK(parentOrigin).get());
}

unsigned TestRunner::userMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin) const
{
    return InjectedBundle::singleton().userMediaPermissionRequestCountForOrigin(toWK(origin).get(), toWK(parentOrigin).get());
}

void TestRunner::resetUserMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin)
{
    InjectedBundle::singleton().resetUserMediaPermissionRequestCountForOrigin(toWK(origin).get(), toWK(parentOrigin).get());
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

void TestRunner::stopLoading()
{
    postPageMessage("StopLoading");
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

void TestRunner::setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(bool value)
{
    postPageMessage("SetRejectsProtectionSpaceAndContinueForAuthenticationChallenges", value);
}
    
void TestRunner::setHandlesAuthenticationChallenges(bool handlesAuthenticationChallenges)
{
    postPageMessage("SetHandlesAuthenticationChallenges", handlesAuthenticationChallenges);
}

void TestRunner::setShouldLogCanAuthenticateAgainstProtectionSpace(bool value)
{
    postPageMessage("SetShouldLogCanAuthenticateAgainstProtectionSpace", value);
}

void TestRunner::setShouldLogDownloadCallbacks(bool value)
{
    postPageMessage("SetShouldLogDownloadCallbacks", value);
}

void TestRunner::setShouldDownloadContentDispositionAttachments(bool value)
{
    postPageMessage("SetShouldDownloadContentDispositionAttachments", value);
}

void TestRunner::setShouldLogDownloadSize(bool value)
{
    postPageMessage("SetShouldLogDownloadSize", value);
}

void TestRunner::setShouldLogDownloadExpectedSize(bool value)
{
    postPageMessage("SetShouldLogDownloadExpectedSize", value);
}

void TestRunner::setAuthenticationUsername(JSStringRef username)
{
    postPageMessage("SetAuthenticationUsername", toWK(username));
}

void TestRunner::setAuthenticationPassword(JSStringRef password)
{
    postPageMessage("SetAuthenticationPassword", toWK(password));
}

bool TestRunner::secureEventInputIsEnabled() const
{
    return postSynchronousPageMessageReturningBoolean("SecureEventInputIsEnabled");
}

void TestRunner::setBlockAllPlugins(bool shouldBlock)
{
    postPageMessage("SetBlockAllPlugins", shouldBlock);
}

void TestRunner::setPluginSupportedMode(JSStringRef mode)
{
    postPageMessage("SetPluginSupportedMode", toWK(mode));
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

void TestRunner::setShouldDecideNavigationPolicyAfterDelay(bool value)
{
    m_shouldDecideNavigationPolicyAfterDelay = value;
    postPageMessage("SetShouldDecideNavigationPolicyAfterDelay", value);
}

void TestRunner::setShouldDecideResponsePolicyAfterDelay(bool value)
{
    m_shouldDecideResponsePolicyAfterDelay = value;
    postPageMessage("SetShouldDecideResponsePolicyAfterDelay", value);
}

void TestRunner::setNavigationGesturesEnabled(bool value)
{
    postPageMessage("SetNavigationGesturesEnabled", value);
}

void TestRunner::setIgnoresViewportScaleLimits(bool value)
{
    postPageMessage("SetIgnoresViewportScaleLimits", value);
}

void TestRunner::setUseDarkAppearanceForTesting(bool useDarkAppearance)
{
    postPageMessage("SetUseDarkAppearanceForTesting", useDarkAppearance);
}

void TestRunner::setShouldDownloadUndisplayableMIMETypes(bool value)
{
    postPageMessage("SetShouldDownloadUndisplayableMIMETypes", value);
}

void TestRunner::setShouldAllowDeviceOrientationAndMotionAccess(bool value)
{
    postPageMessage("SetShouldAllowDeviceOrientationAndMotionAccess", value);
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

static unsigned nextUIScriptCallbackID()
{
    static unsigned callbackID = FirstUIScriptCallbackID;
    return callbackID++;
}

void TestRunner::runUIScript(JSContextRef context, JSStringRef script, JSValueRef callback)
{
    unsigned callbackID = nextUIScriptCallbackID();
    cacheTestRunnerCallback(context, callbackID, callback);
    postPageMessage("RunUIProcessScript", createWKDictionary({
        { "Script", toWK(script) },
        { "CallbackID", adoptWK(WKUInt64Create(callbackID)).get() },
    }));
}

void TestRunner::runUIScriptImmediately(JSContextRef context, JSStringRef script, JSValueRef callback)
{
    unsigned callbackID = nextUIScriptCallbackID();
    cacheTestRunnerCallback(context, callbackID, callback);
    postPageMessage("RunUIProcessScriptImmediately", createWKDictionary({
        { "Script", toWK(script) },
        { "CallbackID", adoptWK(WKUInt64Create(callbackID)).get() },
    }));
}

void TestRunner::runUIScriptCallback(unsigned callbackID, JSStringRef result)
{
    callTestRunnerCallback(callbackID, result);
}

void TestRunner::setAllowedMenuActions(JSContextRef context, JSValueRef actions)
{
    auto messageBody = adoptWK(WKMutableArrayCreate());
    auto actionsArray = JSValueToObject(context, actions, nullptr);
    auto length = arrayLength(context, actionsArray);
    for (unsigned i = 0; i < length; ++i) {
        auto value = JSObjectGetPropertyAtIndex(context, actionsArray, i, nullptr);
        WKArrayAppendItem(messageBody.get(), toWKString(context, value).get());
    }
    postPageMessage("SetAllowedMenuActions", messageBody);
}

void TestRunner::installDidBeginSwipeCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, DidBeginSwipeCallbackID, callback);
}

void TestRunner::installWillEndSwipeCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, WillEndSwipeCallbackID, callback);
}

void TestRunner::installDidEndSwipeCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, DidEndSwipeCallbackID, callback);
}

void TestRunner::installDidRemoveSwipeSnapshotCallback(JSContextRef context, JSValueRef callback)
{
    cacheTestRunnerCallback(context, DidRemoveSwipeSnapshotCallbackID, callback);
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

void TestRunner::setStatisticsDebugMode(JSContextRef context, bool value, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsDebugMode", adoptWK(WKBooleanCreate(value)), completionHandler);
}

void TestRunner::setStatisticsPrevalentResourceForDebugMode(JSContextRef context, JSStringRef hostName, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsPrevalentResourceForDebugMode", toWK(hostName), completionHandler);
}

void TestRunner::setStatisticsLastSeen(JSContextRef context, JSStringRef hostName, double seconds, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsLastSeen", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", toWK(seconds) },
    }), completionHandler);
}

void TestRunner::setStatisticsMergeStatistic(JSContextRef context, JSStringRef hostName, JSStringRef topFrameDomain1, JSStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsMergeStatistic", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "TopFrameDomain1", toWK(topFrameDomain1) },
        { "TopFrameDomain2", toWK(topFrameDomain2) },
        { "LastSeen", adoptWK(WKDoubleCreate(lastSeen)) },
        { "HadUserInteraction", adoptWK(WKBooleanCreate(hadUserInteraction)) },
        { "MostRecentUserInteraction", adoptWK(WKDoubleCreate(mostRecentUserInteraction)) },
        { "IsGrandfathered", adoptWK(WKBooleanCreate(isGrandfathered)) },
        { "IsPrevalent", adoptWK(WKBooleanCreate(isPrevalent)) },
        { "IsVeryPrevalent", adoptWK(WKBooleanCreate(isVeryPrevalent)) },
        { "DataRecordsRemoved", adoptWK(WKUInt64Create(dataRecordsRemoved)) },
    }), completionHandler);
}

void TestRunner::setStatisticsExpiredStatistic(JSContextRef context, JSStringRef hostName, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsExpiredStatistic", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "NumberOfOperatingDaysPassed", adoptWK(WKUInt64Create(numberOfOperatingDaysPassed)) },
        { "HadUserInteraction", adoptWK(WKBooleanCreate(hadUserInteraction)) },
        { "IsScheduledForAllButCookieDataRemoval", adoptWK(WKBooleanCreate(isScheduledForAllButCookieDataRemoval)) },
        { "IsPrevalent", adoptWK(WKBooleanCreate(isPrevalent)) }
    }), completionHandler);
}

void TestRunner::setStatisticsPrevalentResource(JSContextRef context, JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsPrevalentResource", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }), completionHandler);
}

void TestRunner::setStatisticsVeryPrevalentResource(JSContextRef context, JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsVeryPrevalentResource", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }), completionHandler);
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

void TestRunner::setStatisticsHasHadUserInteraction(JSContextRef context, JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsHasHadUserInteraction", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }), completionHandler);
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

void TestRunner::statisticsProcessStatisticsAndDataRecords(JSContextRef context, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "StatisticsProcessStatisticsAndDataRecords", completionHandler);
}

void TestRunner::statisticsUpdateCookieBlocking(JSContextRef context, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "StatisticsUpdateCookieBlocking", completionHandler);
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

void TestRunner::statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(JSContextRef context, unsigned hours, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "StatisticsClearInMemoryAndPersistentStore", adoptWK(WKUInt64Create(hours)), callback);
}

void TestRunner::statisticsClearInMemoryAndPersistentStore(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "StatisticsClearInMemoryAndPersistentStore", callback);
}

void TestRunner::statisticsClearThroughWebsiteDataRemoval(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "StatisticsClearThroughWebsiteDataRemoval", callback);
}

void TestRunner::statisticsDeleteCookiesForHost(JSContextRef context, JSStringRef hostName, bool includeHttpOnlyCookies, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "StatisticsDeleteCookiesForHost", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "IncludeHttpOnlyCookies", adoptWK(WKBooleanCreate(includeHttpOnlyCookies)) },
    }), callback);
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

void TestRunner::setStatisticsShouldDowngradeReferrer(JSContextRef context, bool value, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsShouldDowngradeReferrer", adoptWK(WKBooleanCreate(value)), completionHandler);
}

void TestRunner::setStatisticsShouldBlockThirdPartyCookies(JSContextRef context, bool value, JSValueRef completionHandler, bool onlyOnSitesWithoutUserInteraction)
{
    auto messageName = "SetStatisticsShouldBlockThirdPartyCookies";
    if (onlyOnSitesWithoutUserInteraction)
        messageName = "SetStatisticsShouldBlockThirdPartyCookiesOnSitesWithoutUserInteraction";
    postMessageWithAsyncReply(context, messageName, adoptWK(WKBooleanCreate(value)), completionHandler);
}

void TestRunner::setStatisticsFirstPartyWebsiteDataRemovalMode(JSContextRef context, bool value, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "SetStatisticsFirstPartyWebsiteDataRemovalMode", adoptWK(WKBooleanCreate(value)), completionHandler);
}

void TestRunner::statisticsSetToSameSiteStrictCookies(JSContextRef context, JSStringRef hostName, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "StatisticsSetToSameSiteStrictCookies", toWK(hostName), completionHandler);
}

void TestRunner::statisticsSetFirstPartyHostCNAMEDomain(JSContextRef context, JSStringRef firstPartyURLString, JSStringRef cnameURLString, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "StatisticsSetFirstPartyHostCNAMEDomain", createWKDictionary({
        { "FirstPartyURL", toWK(firstPartyURLString) },
        { "CNAME", toWK(cnameURLString) },
    }), completionHandler);
}

void TestRunner::statisticsSetThirdPartyCNAMEDomain(JSContextRef context, JSStringRef cnameURLString, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "StatisticsSetThirdPartyCNAMEDomain", toWK(cnameURLString), completionHandler);
}

void TestRunner::statisticsResetToConsistentState(JSContextRef context, JSValueRef completionHandler)
{
    postMessageWithAsyncReply(context, "StatisticsResetToConsistentState", completionHandler);
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

void TestRunner::getAllStorageAccessEntries(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "GetAllStorageAccessEntries", callback);
}

void TestRunner::setRequestStorageAccessThrowsExceptionUntilReload(bool enabled)
{
    postSynchronousPageMessage("SetRequestStorageAccessThrowsExceptionUntilReload", enabled);
}

void TestRunner::loadedSubresourceDomains(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "LoadedSubresourceDomains", callback);
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

    return adoptWK(WKDictionaryCreate(keys.data(), values.data(), keys.size()));
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

void TestRunner::setMockCameraOrientation(unsigned orientation)
{
    postSynchronousMessage("SetMockCameraOrientation", orientation);
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

void TestRunner::triggerMockCaptureConfigurationChange(bool forMicrophone, bool forDisplay)
{
    postSynchronousMessage("TriggerMockCaptureConfigurationChange", createWKDictionary({
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

void TestRunner::setMockGamepadDetails(unsigned index, JSStringRef gamepadID, JSStringRef mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble)
{
    postSynchronousMessage("SetMockGamepadDetails", createWKDictionary({
        { "GamepadID", toWK(gamepadID) },
        { "Mapping", toWK(mapping) },
        { "GamepadIndex", adoptWK(WKUInt64Create(index)) },
        { "AxisCount", adoptWK(WKUInt64Create(axisCount)) },
        { "ButtonCount", adoptWK(WKUInt64Create(buttonCount)) },
        { "SupportsDualRumble", adoptWK(WKBooleanCreate(supportsDualRumble)) },
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

void TestRunner::setMockGamepadDetails(unsigned, JSStringRef, JSStringRef, unsigned, unsigned, bool)
{
}

void TestRunner::setMockGamepadAxisValue(unsigned, unsigned, double)
{
}

void TestRunner::setMockGamepadButtonValue(unsigned, unsigned, double)
{
}

#endif // ENABLE(GAMEPAD)

static WKRetainPtr<WKURLRef> makeOpenPanelURL(WKURLRef baseURL, char* filePath)
{
#if OS(WINDOWS)
    if (!PathIsRelativeA(filePath)) {
        char fileURI[INTERNET_MAX_PATH_LENGTH];
        DWORD fileURILength = INTERNET_MAX_PATH_LENGTH;
        UrlCreateFromPathA(filePath, fileURI, &fileURILength, 0);
        return adoptWK(WKURLCreateWithUTF8CString(fileURI));
    }
#else
    WKRetainPtr<WKURLRef> fileURL;
    if (filePath[0] == '/') {
        fileURL = adoptWK(WKURLCreateWithUTF8CString("file://"));
        baseURL = fileURL.get();
    }
#endif
    return adoptWK(WKURLCreateWithBaseURL(baseURL, filePath));
}

void TestRunner::setOpenPanelFiles(JSContextRef context, JSValueRef filesValue)
{
    if (!JSValueIsArray(context, filesValue))
        return;

    auto files = (JSObjectRef)filesValue;
    auto fileURLs = adoptWK(WKMutableArrayCreate());
    auto filesLength = arrayLength(context, files);
    for (size_t i = 0; i < filesLength; ++i) {
        JSValueRef fileValue = JSObjectGetPropertyAtIndex(context, files, i, nullptr);
        if (!JSValueIsString(context, fileValue))
            continue;

        auto file = createJSString(context, fileValue);
        size_t fileBufferSize = JSStringGetMaximumUTF8CStringSize(file.get()) + 1;
        auto fileBuffer = makeUniqueArray<char>(fileBufferSize);
        JSStringGetUTF8CString(file.get(), fileBuffer.get(), fileBufferSize);

        WKArrayAppendItem(fileURLs.get(), makeOpenPanelURL(m_testURL.get(), fileBuffer.get()).get());
    }

    postPageMessage("SetOpenPanelFileURLs", fileURLs);
}

void TestRunner::setOpenPanelFilesMediaIcon(JSContextRef context, JSValueRef data)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME (123058): Use a JSC API to get buffer contents once such is exposed.
    auto iconData = adoptWK(WKBundleCreateWKDataFromUInt8Array(InjectedBundle::singleton().bundle(), context, data));
    postPageMessage("SetOpenPanelFileURLsMediaIcon", iconData);
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(data);
#endif
}

void TestRunner::removeAllSessionCredentials(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "RemoveAllSessionCredentials", callback);
}

void TestRunner::clearDOMCache(JSStringRef origin)
{
    postSynchronousMessage("ClearDOMCache", toWK(origin));
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

void TestRunner::getApplicationManifestThen(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "GetApplicationManifest", callback);
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

bool TestRunner::keyExistsInKeychain(JSStringRef attrLabel, JSStringRef applicationLabelBase64)
{
    return postSynchronousMessageReturningBoolean("KeyExistsInKeychain", createWKDictionary({
        { "AttrLabel", toWK(attrLabel) },
        { "ApplicationLabel", toWK(applicationLabelBase64) },
    }));
}

unsigned long TestRunner::serverTrustEvaluationCallbackCallsCount()
{
    return postSynchronousMessageReturningUInt64("ServerTrustEvaluationCallbackCallsCount");
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

void TestRunner::setAppBoundDomains(JSContextRef context, JSValueRef originArray, JSValueRef completionHandler)
{
    if (!JSValueIsArray(context, originArray))
        return;

    auto origins = JSValueToObject(context, originArray, nullptr);
    auto originURLs = adoptWK(WKMutableArrayCreate());
    auto originsLength = arrayLength(context, origins);
    for (unsigned i = 0; i < originsLength; ++i) {
        JSValueRef originValue = JSObjectGetPropertyAtIndex(context, origins, i, nullptr);
        if (!JSValueIsString(context, originValue))
            continue;

        auto origin = createJSString(context, originValue);
        size_t originBufferSize = JSStringGetMaximumUTF8CStringSize(origin.get()) + 1;
        auto originBuffer = makeUniqueArray<char>(originBufferSize);
        JSStringGetUTF8CString(origin.get(), originBuffer.get(), originBufferSize);

        WKArrayAppendItem(originURLs.get(), adoptWK(WKURLCreateWithUTF8CString(originBuffer.get())).get());
    }

    postMessageWithAsyncReply(context, "SetAppBoundDomains", originURLs, completionHandler);
}

void TestRunner::setManagedDomains(JSContextRef context, JSValueRef originArray, JSValueRef completionHandler)
{
    if (!JSValueIsArray(context, originArray))
        return;

    auto origins = JSValueToObject(context, originArray, nullptr);
    auto originURLs = adoptWK(WKMutableArrayCreate());
    auto originsLength = arrayLength(context, origins);
    for (unsigned i = 0; i < originsLength; ++i) {
        JSValueRef originValue = JSObjectGetPropertyAtIndex(context, origins, i, nullptr);
        if (!JSValueIsString(context, originValue))
            continue;

        auto origin = createJSString(context, originValue);
        size_t originBufferSize = JSStringGetMaximumUTF8CStringSize(origin.get()) + 1;
        auto originBuffer = makeUniqueArray<char>(originBufferSize);
        JSStringGetUTF8CString(origin.get(), originBuffer.get(), originBufferSize);

        WKArrayAppendItem(originURLs.get(), adoptWK(WKURLCreateWithUTF8CString(originBuffer.get())).get());
    }

    postMessageWithAsyncReply(context, "SetManagedDomains", originURLs, completionHandler);
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

void TestRunner::takeViewPortSnapshot(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "TakeViewPortSnapshot", callback);
}

void TestRunner::flushConsoleLogs(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "FlushConsoleLogs", callback);
}

void TestRunner::setPageScaleFactor(JSContextRef context, double scaleFactor, long x, long y, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "SetPageScaleFactor", createWKDictionary({
        { "scaleFactor", toWK(scaleFactor) },
        { "x", toWK(static_cast<double>(x)) },
        { "y", toWK(static_cast<double>(y)) },
        }), callback);
}

void TestRunner::generateTestReport(JSContextRef context, JSStringRef message, JSStringRef group)
{
    auto frame = WKBundleFrameForJavaScriptContext(context);
    _WKBundleFrameGenerateTestReport(frame, toWK(message).get(), toWK(group).get());
}

void TestRunner::getAndClearReportedWindowProxyAccessDomains(JSContextRef context, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "GetAndClearReportedWindowProxyAccessDomains", callback);
}

void TestRunner::dumpBackForwardList()
{
    postSynchronousPageMessage("DumpBackForwardList");
}

bool TestRunner::shouldDumpBackForwardListsForAllWindows() const
{
    return postSynchronousPageMessageReturningBoolean("ShouldDumpBackForwardListsForAllWindows");
}

void TestRunner::setTopContentInset(JSContextRef context, double contentInset, JSValueRef callback)
{
    postMessageWithAsyncReply(context, "SetTopContentInset", adoptWK(WKDoubleCreate(contentInset)), callback);
}

ALLOW_DEPRECATED_DECLARATIONS_END

} // namespace WTR
