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
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

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

static WKBundleFrameRef mainFrame()
{
    return WKBundlePageGetMainFrame(page());
}

static JSContextRef mainFrameJSContext()
{
    return WKBundleFrameGetJavaScriptContext(mainFrame());
}

void TestRunner::display()
{
    WKBundlePageForceRepaint(page());
}

void TestRunner::displayAndTrackRepaints()
{
    auto page = WTR::page();
    WKBundlePageForceRepaint(page);
    WKBundlePageSetTracksRepaints(page, true);
    WKBundlePageResetTrackedRepaints(page);
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

void TestRunner::execCommand(JSStringRef name, JSStringRef showUI, JSStringRef value)
{
    WKBundlePageExecuteEditingCommand(page(), toWK(name).get(), toWK(value).get());
}

static std::optional<WKFindOptions> findOptionsFromArray(JSValueRef optionsArrayAsValue)
{
    auto context = mainFrameJSContext();
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

bool TestRunner::findString(JSStringRef target, JSValueRef optionsArrayAsValue)
{
    if (auto options = findOptionsFromArray(optionsArrayAsValue))
        return WKBundlePageFindString(page(), toWK(target).get(), *options);

    return false;
}

void TestRunner::findStringMatchesInPage(JSStringRef target, JSValueRef optionsArrayAsValue)
{
    if (auto options = findOptionsFromArray(optionsArrayAsValue))
        WKBundlePageFindStringMatches(page(), toWK(target).get(), *options);
}

void TestRunner::replaceFindMatchesAtIndices(JSValueRef matchIndicesAsValue, JSStringRef replacementText, bool selectionOnly)
{
    auto& bundle = InjectedBundle::singleton();
    auto context = mainFrameJSContext();
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

void TestRunner::clearAllApplicationCaches()
{
    WKBundlePageClearApplicationCache(page());
}

void TestRunner::clearApplicationCacheForOrigin(JSStringRef origin)
{
    WKBundlePageClearApplicationCacheForOrigin(page(), toWK(origin).get());
}

void TestRunner::setAppCacheMaximumSize(uint64_t size)
{
    WKBundlePageSetAppCacheMaximumSize(page(), size);
}

long long TestRunner::applicationCacheDiskUsageForOrigin(JSStringRef origin)
{
    return WKBundlePageGetAppCacheUsageForOrigin(page(), toWK(origin).get());
}

void TestRunner::disallowIncreaseForApplicationCacheQuota()
{
    m_disallowIncreaseForApplicationCacheQuota = true;
}

static inline JSValueRef stringArrayToJS(JSContextRef context, WKArrayRef strings)
{
    const size_t count = WKArrayGetSize(strings);
    auto array = JSObjectMakeArray(context, 0, 0, nullptr);
    for (size_t i = 0; i < count; ++i) {
        auto stringRef = static_cast<WKStringRef>(WKArrayGetItemAtIndex(strings, i));
        JSObjectSetPropertyAtIndex(context, array, i, JSValueMakeString(context, toJS(stringRef).get()), nullptr);
    }
    return array;
}

JSValueRef TestRunner::originsWithApplicationCache()
{
    return stringArrayToJS(mainFrameJSContext(), adoptWK(WKBundlePageCopyOriginsWithApplicationCache(page())).get());
}

bool TestRunner::isCommandEnabled(JSStringRef name)
{
    return WKBundlePageIsEditingCommandEnabled(page(), toWK(name).get());
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

bool TestRunner::isPageBoxVisible(int pageIndex)
{
    auto& injectedBundle = InjectedBundle::singleton();
    return WKBundleIsPageBoxVisible(injectedBundle.bundle(), mainFrame(), pageIndex);
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

void TestRunner::clearBackForwardList()
{
    WKBundleClearHistoryForTesting(page());
}

void TestRunner::makeWindowObject(JSContextRef context)
{
    setGlobalObjectProperty(context, "testRunner", this);
}

void TestRunner::showWebInspector()
{
    WKBundleInspectorShow(WKBundlePageGetInspector(page()));
}

void TestRunner::closeWebInspector()
{
    WKBundleInspectorClose(WKBundlePageGetInspector(page()));
}

void TestRunner::evaluateInWebInspector(JSStringRef script)
{
    WKBundleInspectorEvaluateScriptForTest(WKBundlePageGetInspector(page()), toWK(script).get());
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

    WKBundleFrameRef frame = WKBundleFrameForJavaScriptContext(context);
    if (!frame)
        frame = mainFrame();

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
    return WKBundleFrameSetTextDirection(mainFrame(), toWK(direction).get());
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

using CallbackMap = WTF::HashMap<unsigned, JSObjectRef>;
static CallbackMap& callbackMap()
{
    static CallbackMap& map = *new CallbackMap;
    return map;
}

enum {
    AddChromeInputFieldCallbackID = 1,
    RemoveChromeInputFieldCallbackID,
    SetTextInChromeInputFieldCallbackID,
    SelectChromeInputFieldCallbackID,
    GetSelectedTextInChromeInputFieldCallbackID,
    FocusWebViewCallbackID,
    SetBackingScaleFactorCallbackID,
    DidBeginSwipeCallbackID,
    WillEndSwipeCallbackID,
    DidEndSwipeCallbackID,
    DidRemoveSwipeSnapshotCallbackID,
    SetStatisticsDebugModeCallbackID,
    SetStatisticsPrevalentResourceForDebugModeCallbackID,
    SetStatisticsLastSeenCallbackID,
    SetStatisticsMergeStatisticCallbackID,
    SetStatisticsExpiredStatisticCallbackID,
    SetStatisticsPrevalentResourceCallbackID,
    SetStatisticsVeryPrevalentResourceCallbackID,
    SetStatisticsHasHadUserInteractionCallbackID,
    StatisticsDidModifyDataRecordsCallbackID,
    StatisticsDidScanDataRecordsCallbackID,
    StatisticsDidClearThroughWebsiteDataRemovalCallbackID,
    StatisticsDidClearInMemoryAndPersistentStoreCallbackID,
    StatisticsDidResetToConsistentStateCallbackID,
    StatisticsDidSetBlockCookiesForHostCallbackID,
    StatisticsDidSetShouldDowngradeReferrerCallbackID,
    StatisticsDidSetShouldBlockThirdPartyCookiesCallbackID,
    StatisticsDidSetFirstPartyWebsiteDataRemovalModeCallbackID,
    StatisticsDidSetToSameSiteStrictCookiesCallbackID,
    StatisticsDidSetFirstPartyHostCNAMEDomainCallbackID,
    StatisticsDidSetThirdPartyCNAMEDomainCallbackID,
    AllStorageAccessEntriesCallbackID,
    LoadedSubresourceDomainsCallbackID,
    DidRemoveAllSessionCredentialsCallbackID,
    GetApplicationManifestCallbackID,
    TextDidChangeInTextFieldCallbackID,
    TextFieldDidBeginEditingCallbackID,
    TextFieldDidEndEditingCallbackID,
    CustomMenuActionCallbackID,
    DidSetAppBoundDomainsCallbackID,
    DidSetManagedDomainsCallbackID,
    EnterFullscreenForElementCallbackID,
    ExitFullscreenForElementCallbackID,
    AppBoundRequestContextDataForDomainCallbackID,
    TakeViewPortSnapshotCallbackID,
    RemoveAllCookiesCallbackID,
    FirstUIScriptCallbackID = 100
};

static void cacheTestRunnerCallback(unsigned index, JSValueRef callback)
{
    if (!callback)
        return;
    auto context = mainFrameJSContext();
    if (!JSValueIsObject(context, callback))
        return;
    if (callbackMap().contains(index)) {
        InjectedBundle::singleton().outputText(makeString("FAIL: Tried to install a second TestRunner callback for the same event (id ", index, ")\n\n"));
        return;
    }
    JSValueProtect(context, callback);
    callbackMap().add(index, const_cast<JSObjectRef>(callback));
}

static void callTestRunnerCallback(unsigned index, size_t argumentCount = 0, const JSValueRef arguments[] = nullptr)
{
    auto callback = callbackMap().take(index);
    if (!callback)
        return;
    auto context = mainFrameJSContext();
    JSObjectCallAsFunction(context, callback, JSContextGetGlobalObject(context), argumentCount, arguments, 0);
    JSValueUnprotect(context, callback);
}

void TestRunner::clearTestRunnerCallbacks()
{
    auto context = mainFrameJSContext();
    for (auto& value : callbackMap().values())
        JSValueUnprotect(context, JSValueToObject(context, value, nullptr));
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

void TestRunner::setTextInChromeInputField(JSStringRef text, JSValueRef callback)
{
    cacheTestRunnerCallback(SetTextInChromeInputFieldCallbackID, callback);
    InjectedBundle::singleton().postSetTextInChromeInputField(toWTFString(text));
}

void TestRunner::selectChromeInputField(JSValueRef callback)
{
    cacheTestRunnerCallback(SelectChromeInputFieldCallbackID, callback);
    InjectedBundle::singleton().postSelectChromeInputField();
}

void TestRunner::getSelectedTextInChromeInputField(JSValueRef callback)
{
    cacheTestRunnerCallback(GetSelectedTextInChromeInputFieldCallbackID, callback);
    InjectedBundle::singleton().postGetSelectedTextInChromeInputField();
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

void TestRunner::callSetTextInChromeInputFieldCallback()
{
    callTestRunnerCallback(SetTextInChromeInputFieldCallbackID);
}

void TestRunner::callSelectChromeInputFieldCallback()
{
    callTestRunnerCallback(SelectChromeInputFieldCallbackID);
}

void TestRunner::callGetSelectedTextInChromeInputFieldCallback(JSStringRef text)
{
    auto textValue = JSValueMakeString(mainFrameJSContext(), text);
    callTestRunnerCallback(GetSelectedTextInChromeInputFieldCallbackID, 1, &textValue);
}

void TestRunner::callFocusWebViewCallback()
{
    callTestRunnerCallback(FocusWebViewCallbackID);
}

void TestRunner::callSetBackingScaleFactorCallback()
{
    callTestRunnerCallback(SetBackingScaleFactorCallbackID);
}

void TestRunner::setAlwaysAcceptCookies(bool accept)
{
    postSynchronousMessage("SetAlwaysAcceptCookies", accept);
}

void TestRunner::setOnlyAcceptFirstPartyCookies(bool accept)
{
    postSynchronousMessage("SetOnlyAcceptFirstPartyCookies", accept);
}

void TestRunner::removeAllCookies(JSValueRef callback)
{
    cacheTestRunnerCallback(RemoveAllCookiesCallbackID, callback);
    postMessage("RemoveAllCookies");
}

void TestRunner::callRemoveAllCookiesCallback()
{
    callTestRunnerCallback(RemoveAllCookiesCallbackID);
}

void TestRunner::setEnterFullscreenForElementCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(EnterFullscreenForElementCallbackID, callback);
}

void TestRunner::callEnterFullscreenForElementCallback()
{
    callTestRunnerCallback(EnterFullscreenForElementCallbackID);
}

void TestRunner::setExitFullscreenForElementCallback(JSValueRef callback)
{
    cacheTestRunnerCallback(ExitFullscreenForElementCallbackID, callback);
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
    WKBundleSetWebNotificationPermission(InjectedBundle::singleton().bundle(), page(), toWK(origin).get(), true);
    postSynchronousPageMessageWithReturnValue("GrantNotificationPermission", toWK(origin));
}

void TestRunner::denyWebNotificationPermission(JSStringRef origin)
{
    WKBundleSetWebNotificationPermission(InjectedBundle::singleton().bundle(), page(), toWK(origin).get(), false);
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

void TestRunner::simulateWebNotificationClick(JSValueRef notification)
{
    auto& injectedBundle = InjectedBundle::singleton();

    auto notificationID = adoptWK(WKBundleCopyWebNotificationID(injectedBundle.bundle(), mainFrameJSContext(), notification));
    injectedBundle.postSimulateWebNotificationClick(notificationID.get());
}

void TestRunner::simulateWebNotificationClickForServiceWorkerNotifications()
{
    InjectedBundle::singleton().postSimulateWebNotificationClickForServiceWorkerNotifications();
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

void TestRunner::setUserMediaPermission(bool enabled)
{
    // FIXME: This should be done by frame.
    InjectedBundle::singleton().setUserMediaPermission(enabled);
}

void TestRunner::resetUserMediaPermission()
{
    // FIXME: This should be done by frame.
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

bool TestRunner::callShouldCloseOnWebView()
{
    return WKBundleFrameCallShouldCloseOnWebView(mainFrame());
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
    auto baseURLWK = adoptWK(WKBundleFrameCopyURL(mainFrame()));
    auto urlWK = adoptWK(WKURLCreateWithBaseURL(baseURLWK.get(), toWTFString(url).utf8().data()));
    auto urlStringWK = adoptWK(WKURLCopyString(urlWK.get()));
    injectedBundle.queueLoad(urlStringWK.get(), toWK(target).get(), shouldOpenExternalURLs);
}

void TestRunner::queueLoadHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL)
{
    auto baseURLWK = baseURL ? toWK(baseURL) : WKRetainPtr<WKStringRef>();
    auto unreachableURLWK = unreachableURL ? toWK(unreachableURL) : WKRetainPtr<WKStringRef>();
    InjectedBundle::singleton().queueLoadHTMLString(toWK(content).get(), baseURLWK.get(), unreachableURLWK.get());
}

void TestRunner::stopLoading()
{
    WKBundlePageStopLoading(page());
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

void TestRunner::setShouldLogDownloadSize(bool value)
{
    postPageMessage("SetShouldLogDownloadSize", value);
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

JSValueRef TestRunner::failNextNewCodeBlock()
{
    return JSC::failNextNewCodeBlock(mainFrameJSContext());
}

JSValueRef TestRunner::numberOfDFGCompiles(JSValueRef function)
{
    return JSC::numberOfDFGCompiles(mainFrameJSContext(), function);
}

JSValueRef TestRunner::neverInlineFunction(JSValueRef function)
{
    return JSC::setNeverInline(mainFrameJSContext(), function);
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

void TestRunner::runUIScript(JSStringRef script, JSValueRef callback)
{
    unsigned callbackID = nextUIScriptCallbackID();
    cacheTestRunnerCallback(callbackID, callback);
    postPageMessage("RunUIProcessScript", createWKDictionary({
        { "Script", toWK(script) },
        { "CallbackID", adoptWK(WKUInt64Create(callbackID)).get() },
    }));
}

void TestRunner::runUIScriptImmediately(JSStringRef script, JSValueRef callback)
{
    unsigned callbackID = nextUIScriptCallbackID();
    cacheTestRunnerCallback(callbackID, callback);
    postPageMessage("RunUIProcessScriptImmediately", createWKDictionary({
        { "Script", toWK(script) },
        { "CallbackID", adoptWK(WKUInt64Create(callbackID)).get() },
    }));
}

void TestRunner::runUIScriptCallback(unsigned callbackID, JSStringRef result)
{
    JSValueRef resultValue = JSValueMakeString(mainFrameJSContext(), result);
    callTestRunnerCallback(callbackID, 1, &resultValue);
}

void TestRunner::setAllowedMenuActions(JSValueRef actions)
{
    auto messageBody = adoptWK(WKMutableArrayCreate());
    auto context = mainFrameJSContext();
    auto actionsArray = JSValueToObject(context, actions, nullptr);
    auto length = arrayLength(context, actionsArray);
    for (unsigned i = 0; i < length; ++i) {
        auto value = JSObjectGetPropertyAtIndex(context, actionsArray, i, nullptr);
        WKArrayAppendItem(messageBody.get(), toWKString(context, value).get());
    }
    postPageMessage("SetAllowedMenuActions", messageBody);
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

void TestRunner::setStatisticsDebugMode(bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsDebugModeCallbackID, completionHandler);
    postMessage("SetStatisticsDebugMode", value);
}

void TestRunner::statisticsCallDidSetDebugModeCallback()
{
    callTestRunnerCallback(SetStatisticsDebugModeCallbackID);
}

void TestRunner::setStatisticsPrevalentResourceForDebugMode(JSStringRef hostName, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsPrevalentResourceForDebugModeCallbackID, completionHandler);
    postMessage("SetStatisticsPrevalentResourceForDebugMode", hostName);
}

void TestRunner::statisticsCallDidSetPrevalentResourceForDebugModeCallback()
{
    callTestRunnerCallback(SetStatisticsPrevalentResourceForDebugModeCallbackID);
}

void TestRunner::setStatisticsLastSeen(JSStringRef hostName, double seconds, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsLastSeenCallbackID, completionHandler);

    postMessage("SetStatisticsLastSeen", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", toWK(seconds) },
    }));
}

void TestRunner::statisticsCallDidSetLastSeenCallback()
{
    callTestRunnerCallback(SetStatisticsLastSeenCallbackID);
}

void TestRunner::setStatisticsMergeStatistic(JSStringRef hostName, JSStringRef topFrameDomain1, JSStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsMergeStatisticCallbackID, completionHandler);

    postMessage("SetStatisticsMergeStatistic", createWKDictionary({
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
    }));
}

void TestRunner::statisticsCallDidSetMergeStatisticCallback()
{
    callTestRunnerCallback(SetStatisticsMergeStatisticCallbackID);
}

void TestRunner::setStatisticsExpiredStatistic(JSStringRef hostName, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsExpiredStatisticCallbackID, completionHandler);

    postMessage("SetStatisticsExpiredStatistic", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "NumberOfOperatingDaysPassed", adoptWK(WKUInt64Create(numberOfOperatingDaysPassed)) },
        { "HadUserInteraction", adoptWK(WKBooleanCreate(hadUserInteraction)) },
        { "IsScheduledForAllButCookieDataRemoval", adoptWK(WKBooleanCreate(isScheduledForAllButCookieDataRemoval)) },
        { "IsPrevalent", adoptWK(WKBooleanCreate(isPrevalent)) }
    }));
}

void TestRunner::statisticsCallDidSetExpiredStatisticCallback()
{
    callTestRunnerCallback(SetStatisticsExpiredStatisticCallbackID);
}

void TestRunner::setStatisticsPrevalentResource(JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsPrevalentResourceCallbackID, completionHandler);

    postMessage("SetStatisticsPrevalentResource", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }));
}

void TestRunner::statisticsCallDidSetPrevalentResourceCallback()
{
    callTestRunnerCallback(SetStatisticsPrevalentResourceCallbackID);
}

void TestRunner::setStatisticsVeryPrevalentResource(JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsVeryPrevalentResourceCallbackID, completionHandler);

    postMessage("SetStatisticsVeryPrevalentResource", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }));
}

void TestRunner::statisticsCallDidSetVeryPrevalentResourceCallback()
{
    callTestRunnerCallback(SetStatisticsVeryPrevalentResourceCallbackID);
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

void TestRunner::setStatisticsHasHadUserInteraction(JSStringRef hostName, bool value, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(SetStatisticsHasHadUserInteractionCallbackID, completionHandler);

    postMessage("SetStatisticsHasHadUserInteraction", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "Value", adoptWK(WKBooleanCreate(value)) },
    }));
}

void TestRunner::statisticsCallDidSetHasHadUserInteractionCallback()
{
    callTestRunnerCallback(SetStatisticsHasHadUserInteractionCallbackID);
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

void TestRunner::setStatisticsCrossSiteLoadWithLinkDecoration(JSStringRef fromHost, JSStringRef toHost)
{
    postSynchronousMessage("SetStatisticsCrossSiteLoadWithLinkDecoration", createWKDictionary({
        { "FromHost", toWK(fromHost) },
        { "ToHost", toWK(toHost) },
    }));
}

void TestRunner::setStatisticsTimeToLiveUserInteraction(double seconds)
{
    postSynchronousMessage("SetStatisticsTimeToLiveUserInteraction", seconds);
}

void TestRunner::installStatisticsDidModifyDataRecordsCallback(JSValueRef callback)
{
    if (!!callback) {
        cacheTestRunnerCallback(StatisticsDidModifyDataRecordsCallbackID, callback);
        // Setting a callback implies we expect to receive callbacks. So register for them.
        setStatisticsNotifyPagesWhenDataRecordsWereScanned(true);
    }
}

void TestRunner::statisticsDidModifyDataRecordsCallback()
{
    callTestRunnerCallback(StatisticsDidModifyDataRecordsCallbackID);
}

void TestRunner::installStatisticsDidScanDataRecordsCallback(JSValueRef callback)
{
    if (!!callback) {
        cacheTestRunnerCallback(StatisticsDidScanDataRecordsCallbackID, callback);
        // Setting a callback implies we expect to receive callbacks. So register for them.
        setStatisticsNotifyPagesWhenDataRecordsWereScanned(true);
    }
}

void TestRunner::statisticsDidScanDataRecordsCallback()
{
    callTestRunnerCallback(StatisticsDidScanDataRecordsCallbackID);
}

bool TestRunner::statisticsNotifyObserver()
{
    return InjectedBundle::singleton().statisticsNotifyObserver();
}

void TestRunner::statisticsProcessStatisticsAndDataRecords()
{
    postSynchronousMessage("StatisticsProcessStatisticsAndDataRecords");
}

void TestRunner::statisticsUpdateCookieBlocking(JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidSetBlockCookiesForHostCallbackID, completionHandler);

    postMessage("StatisticsUpdateCookieBlocking");
}

void TestRunner::statisticsCallDidSetBlockCookiesForHostCallback()
{
    callTestRunnerCallback(StatisticsDidSetBlockCookiesForHostCallbackID);
}

void TestRunner::setStatisticsNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    postSynchronousMessage("StatisticsNotifyPagesWhenDataRecordsWereScanned", value);
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
    
void TestRunner::statisticsClearInMemoryAndPersistentStore(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidClearInMemoryAndPersistentStoreCallbackID, callback);
    postMessage("StatisticsClearInMemoryAndPersistentStore");
}

void TestRunner::statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours, JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidClearInMemoryAndPersistentStoreCallbackID, callback);
    postMessage("StatisticsClearInMemoryAndPersistentStoreModifiedSinceHours", hours);
}

void TestRunner::statisticsClearThroughWebsiteDataRemoval(JSValueRef callback)
{
    cacheTestRunnerCallback(StatisticsDidClearThroughWebsiteDataRemovalCallbackID, callback);
    postMessage("StatisticsClearThroughWebsiteDataRemoval");
}

void TestRunner::statisticsDeleteCookiesForHost(JSStringRef hostName, bool includeHttpOnlyCookies)
{
    postSynchronousMessage("StatisticsDeleteCookiesForHost", createWKDictionary({
        { "HostName", toWK(hostName) },
        { "IncludeHttpOnlyCookies", adoptWK(WKBooleanCreate(includeHttpOnlyCookies)) },
    }));
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

void TestRunner::setStatisticsShouldDowngradeReferrer(bool value, JSValueRef completionHandler)
{
    if (m_hasSetDowngradeReferrerCallback)
        return;
    
    cacheTestRunnerCallback(StatisticsDidSetShouldDowngradeReferrerCallbackID, completionHandler);
    postMessage("SetStatisticsShouldDowngradeReferrer", value);
    m_hasSetDowngradeReferrerCallback = true;
}

void TestRunner::statisticsCallDidSetShouldDowngradeReferrerCallback()
{
    callTestRunnerCallback(StatisticsDidSetShouldDowngradeReferrerCallbackID);
    m_hasSetDowngradeReferrerCallback = false;
}

void TestRunner::setStatisticsShouldBlockThirdPartyCookies(bool value, JSValueRef completionHandler, bool onlyOnSitesWithoutUserInteraction)
{
    if (m_hasSetBlockThirdPartyCookiesCallback)
        return;

    cacheTestRunnerCallback(StatisticsDidSetShouldBlockThirdPartyCookiesCallbackID, completionHandler);
    auto messageName = "SetStatisticsShouldBlockThirdPartyCookies";
    if (onlyOnSitesWithoutUserInteraction)
        messageName = "SetStatisticsShouldBlockThirdPartyCookiesOnSitesWithoutUserInteraction";
    postMessage(messageName, value);
    m_hasSetBlockThirdPartyCookiesCallback = true;
}

void TestRunner::statisticsCallDidSetShouldBlockThirdPartyCookiesCallback()
{
    callTestRunnerCallback(StatisticsDidSetShouldBlockThirdPartyCookiesCallbackID);
    m_hasSetBlockThirdPartyCookiesCallback = false;
}

void TestRunner::setStatisticsFirstPartyWebsiteDataRemovalMode(bool value, JSValueRef completionHandler)
{
    if (m_hasSetFirstPartyWebsiteDataRemovalModeCallback)
        return;

    cacheTestRunnerCallback(StatisticsDidSetFirstPartyWebsiteDataRemovalModeCallbackID, completionHandler);
    postMessage("SetStatisticsFirstPartyWebsiteDataRemovalMode", value);
    m_hasSetFirstPartyWebsiteDataRemovalModeCallback = true;
}

void TestRunner::statisticsCallDidSetFirstPartyWebsiteDataRemovalModeCallback()
{
    callTestRunnerCallback(StatisticsDidSetFirstPartyWebsiteDataRemovalModeCallbackID);
    m_hasSetFirstPartyWebsiteDataRemovalModeCallback = false;
}

void TestRunner::statisticsCallClearInMemoryAndPersistentStoreCallback()
{
    callTestRunnerCallback(StatisticsDidClearInMemoryAndPersistentStoreCallbackID);
}

void TestRunner::statisticsCallClearThroughWebsiteDataRemovalCallback()
{
    callTestRunnerCallback(StatisticsDidClearThroughWebsiteDataRemovalCallbackID);
}

void TestRunner::statisticsSetToSameSiteStrictCookies(JSStringRef hostName, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidSetToSameSiteStrictCookiesCallbackID, completionHandler);
    postMessage("StatisticsSetToSameSiteStrictCookies", hostName);
}

void TestRunner::statisticsCallDidSetToSameSiteStrictCookiesCallback()
{
    callTestRunnerCallback(StatisticsDidSetToSameSiteStrictCookiesCallbackID);
}


void TestRunner::statisticsSetFirstPartyHostCNAMEDomain(JSStringRef firstPartyURLString, JSStringRef cnameURLString, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidSetFirstPartyHostCNAMEDomainCallbackID, completionHandler);
    postMessage("StatisticsSetFirstPartyHostCNAMEDomain", createWKDictionary({
        { "FirstPartyURL", toWK(firstPartyURLString) },
        { "CNAME", toWK(cnameURLString) },
    }));
}

void TestRunner::statisticsCallDidSetFirstPartyHostCNAMEDomainCallback()
{
    callTestRunnerCallback(StatisticsDidSetFirstPartyHostCNAMEDomainCallbackID);
}

void TestRunner::statisticsSetThirdPartyCNAMEDomain(JSStringRef cnameURLString, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidSetThirdPartyCNAMEDomainCallbackID, completionHandler);
    postMessage("StatisticsSetThirdPartyCNAMEDomain", toWK(cnameURLString));
}

void TestRunner::statisticsCallDidSetThirdPartyCNAMEDomainCallback()
{
    callTestRunnerCallback(StatisticsDidSetThirdPartyCNAMEDomainCallbackID);
}

void TestRunner::statisticsResetToConsistentState(JSValueRef completionHandler)
{
    cacheTestRunnerCallback(StatisticsDidResetToConsistentStateCallbackID, completionHandler);

    postMessage("StatisticsResetToConsistentState");
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

void TestRunner::getAllStorageAccessEntries(JSValueRef callback)
{
    cacheTestRunnerCallback(AllStorageAccessEntriesCallbackID, callback);
    postMessage("GetAllStorageAccessEntries");
}

static JSValueRef makeDomainsValue(const Vector<String>& domains)
{
    StringBuilder builder;
    builder.append('[');
    bool firstDomain = true;
    for (auto& domain : domains) {
        builder.append(firstDomain ? "\"" : ", \"", domain, '"');
        firstDomain = false;
    }
    builder.append(']');
    return JSValueMakeFromJSONString(mainFrameJSContext(), createJSString(builder.toString().utf8().data()).get());
}
void TestRunner::callDidReceiveAllStorageAccessEntriesCallback(Vector<String>& domains)
{
    auto result = makeDomainsValue(domains);
    callTestRunnerCallback(AllStorageAccessEntriesCallbackID, 1, &result);
}

void TestRunner::loadedSubresourceDomains(JSValueRef callback)
{
    cacheTestRunnerCallback(LoadedSubresourceDomainsCallbackID, callback);
    postMessage("LoadedSubresourceDomains");
}

void TestRunner::callDidReceiveLoadedSubresourceDomainsCallback(Vector<String>&& domains)
{
    auto result = makeDomainsValue(domains);
    callTestRunnerCallback(LoadedSubresourceDomainsCallbackID, 1, &result);
}

void TestRunner::addMockMediaDevice(JSStringRef persistentId, JSStringRef label, const char* type)
{
    postSynchronousMessage("AddMockMediaDevice", createWKDictionary({
        { "PersistentID", toWK(persistentId) },
        { "Label", toWK(label) },
        { "Type", toWK(type) },
    }));
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

void TestRunner::triggerMockMicrophoneConfigurationChange()
{
    postSynchronousMessage("TriggerMockMicrophoneConfigurationChange");
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

void TestRunner::setMockGamepadDetails(unsigned index, JSStringRef gamepadID, JSStringRef mapping, unsigned axisCount, unsigned buttonCount)
{
    postSynchronousMessage("SetMockGamepadDetails", createWKDictionary({
        { "GamepadID", toWK(gamepadID) },
        { "Mapping", toWK(mapping) },
        { "GamepadIndex", adoptWK(WKUInt64Create(index)) },
        { "AxisCount", adoptWK(WKUInt64Create(axisCount)) },
        { "ButtonCount", adoptWK(WKUInt64Create(buttonCount)) },
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

void TestRunner::setMockGamepadDetails(unsigned, JSStringRef, JSStringRef, unsigned, unsigned)
{
}

void TestRunner::setMockGamepadAxisValue(unsigned, unsigned, double)
{
}

void TestRunner::setMockGamepadButtonValue(unsigned, unsigned, double)
{
}

#endif // ENABLE(GAMEPAD)

void TestRunner::setOpenPanelFiles(JSValueRef filesValue)
{
    JSContextRef context = mainFrameJSContext();

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

        auto baseURL = m_testURL.get();

        if (fileBuffer[0] == '/')
            baseURL = WKURLCreateWithUTF8CString("file://");

        WKArrayAppendItem(fileURLs.get(), adoptWK(WKURLCreateWithBaseURL(baseURL, fileBuffer.get())).get());

    }

    postPageMessage("SetOpenPanelFileURLs", fileURLs);
}

void TestRunner::setOpenPanelFilesMediaIcon(JSValueRef data)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME (123058): Use a JSC API to get buffer contents once such is exposed.
    auto iconData = adoptWK(WKBundleCreateWKDataFromUInt8Array(InjectedBundle::singleton().bundle(), mainFrameJSContext(), data));
    postPageMessage("SetOpenPanelFileURLsMediaIcon", iconData);
#else
    UNUSED_PARAM(data);
#endif
}

void TestRunner::removeAllSessionCredentials(JSValueRef callback)
{
    cacheTestRunnerCallback(DidRemoveAllSessionCredentialsCallbackID, callback);
    postMessage("RemoveAllSessionCredentials", true);
}

void TestRunner::callDidRemoveAllSessionCredentialsCallback()
{
    callTestRunnerCallback(DidRemoveAllSessionCredentialsCallbackID);
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

void TestRunner::getApplicationManifestThen(JSValueRef callback)
{
    cacheTestRunnerCallback(GetApplicationManifestCallbackID, callback);
    postMessage("GetApplicationManifest");
}

void TestRunner::didGetApplicationManifest()
{
    callTestRunnerCallback(GetApplicationManifestCallbackID);
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

void TestRunner::sendDisplayConfigurationChangedMessageForTesting()
{
    postSynchronousMessage("SendDisplayConfigurationChangedMessageForTesting");
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

void TestRunner::setAppBoundDomains(JSValueRef originArray, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(DidSetAppBoundDomainsCallbackID, completionHandler);

    auto context = mainFrameJSContext();
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

    auto messageName = toWK("SetAppBoundDomains");
    WKBundlePostMessage(InjectedBundle::singleton().bundle(), messageName.get(), originURLs.get());
}

void TestRunner::setManagedDomains(JSValueRef originArray, JSValueRef completionHandler)
{
    cacheTestRunnerCallback(DidSetManagedDomainsCallbackID, completionHandler);

    auto context = mainFrameJSContext();
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

    auto messageName = toWK("SetManagedDomains");
    WKBundlePostMessage(InjectedBundle::singleton().bundle(), messageName.get(), originURLs.get());
}

void TestRunner::didSetAppBoundDomainsCallback()
{
    callTestRunnerCallback(DidSetAppBoundDomainsCallbackID);
}

void TestRunner::didSetManagedDomainsCallback()
{
    callTestRunnerCallback(DidSetManagedDomainsCallbackID);
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

void TestRunner::takeViewPortSnapshot(JSValueRef callback)
{
    if (m_takeViewPortSnapshot)
        return;

    cacheTestRunnerCallback(TakeViewPortSnapshotCallbackID, callback);
    postMessage("TakeViewPortSnapshot");
    m_takeViewPortSnapshot = true;
}

void TestRunner::viewPortSnapshotTaken(WKStringRef value)
{
    auto jsValue = JSValueMakeString(mainFrameJSContext(), toJS(value).get());
    callTestRunnerCallback(TakeViewPortSnapshotCallbackID, 1, &jsValue);
    m_takeViewPortSnapshot = false;
}

void TestRunner::generateTestReport(JSStringRef message, JSStringRef group)
{
    _WKBundleFrameGenerateTestReport(mainFrame(), toWK(message).get(), toWK(group).get());
}

ALLOW_DEPRECATED_DECLARATIONS_END

} // namespace WTR
