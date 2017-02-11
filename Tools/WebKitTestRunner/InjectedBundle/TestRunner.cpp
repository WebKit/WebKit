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
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSerializedScriptValue.h>
#include <WebKit/WebKit2_C.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WTR {

Ref<TestRunner> TestRunner::create()
{
    return adoptRef(*new TestRunner);
}

TestRunner::TestRunner()
    : m_whatToDump(RenderTree)
    , m_shouldDumpAllFrameScrollPositions(false)
    , m_shouldDumpBackForwardListsForAllWindows(false)
    , m_shouldAllowEditing(true)
    , m_shouldCloseExtraWindows(false)
    , m_dumpEditingCallbacks(false)
    , m_dumpStatusCallbacks(false)
    , m_dumpTitleChanges(false)
    , m_dumpPixels(true)
    , m_dumpSelectionRect(false)
    , m_dumpFullScreenCallbacks(false)
    , m_dumpFrameLoadCallbacks(false)
    , m_dumpProgressFinishedCallback(false)
    , m_dumpResourceLoadCallbacks(false)
    , m_dumpResourceResponseMIMETypes(false)
    , m_dumpWillCacheResponse(false)
    , m_dumpApplicationCacheDelegateCallbacks(false)
    , m_dumpDatabaseCallbacks(false)
    , m_disallowIncreaseForApplicationCacheQuota(false)
    , m_waitToDump(false)
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
#if PLATFORM(GTK)
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
    WKBundlePageSetTracksRepaints(page, true);
    WKBundlePageResetTrackedRepaints(page);
}

void TestRunner::dumpAsText(bool dumpPixels)
{
    if (m_whatToDump < MainFrameText)
        m_whatToDump = MainFrameText;
    m_dumpPixels = dumpPixels;
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
    m_waitToDump = true;
    if (InjectedBundle::singleton().useWaitToDumpWatchdogTimer())
        initializeWaitToDumpWatchdogTimerIfNeeded();
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

    if (m_waitToDump && !injectedBundle.topLoadingFrame())
        injectedBundle.page()->dump();

    // We don't call invalidateWaitToDumpWatchdogTimer() here, even if we continue to wait for a load to finish.
    // The test is still subject to timeout checking - it is better to detect an async timeout inside WebKitTestRunner
    // than to let webkitpy do that, because WebKitTestRunner will dump partial results.

    m_waitToDump = false;
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

void TestRunner::execCommand(JSStringRef name, JSStringRef argument)
{
    WKBundlePageExecuteEditingCommand(InjectedBundle::singleton().page()->page(), toWK(name).get(), toWK(argument).get());
}

bool TestRunner::findString(JSStringRef target, JSValueRef optionsArrayAsValue)
{
    WKFindOptions options = 0;

    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(injectedBundle.page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSRetainPtr<JSStringRef> lengthPropertyName(Adopt, JSStringCreateWithUTF8CString("length"));
    JSObjectRef optionsArray = JSValueToObject(context, optionsArrayAsValue, 0);
    JSValueRef lengthValue = JSObjectGetProperty(context, optionsArray, lengthPropertyName.get(), 0);
    if (!JSValueIsNumber(context, lengthValue))
        return false;

    size_t length = static_cast<size_t>(JSValueToNumber(context, lengthValue, 0));
    for (size_t i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, 0);
        if (!JSValueIsString(context, value))
            continue;

        JSRetainPtr<JSStringRef> optionName(Adopt, JSValueToStringCopy(context, value, 0));

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

    return WKBundlePageFindString(injectedBundle.page()->page(), toWK(target).get(), options);
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

void TestRunner::setCanOpenWindows(bool)
{
    // The test plugins/get-url-with-blank-target.html requires that the embedding client forbid
    // opening windows (by omitting a call to this function) so as to test that NPN_GetURL()
    // with a blank target will return an error.
    //
    // It is not clear if we should implement this functionality or remove it and plugins/get-url-with-blank-target.html
    // per the remark in <https://trac.webkit.org/changeset/64504/trunk/LayoutTests/platform/mac-wk2/Skipped>.
    // For now, just ignore this setting.
}

void TestRunner::setXSSAuditorEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitXSSAuditorEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setShadowDOMEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitShadowDOMEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setCustomElementsEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitCustomElementsEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setSubtleCryptoEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitSubtleCryptoEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setMediaStreamEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitMediaStreamEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setPeerConnectionEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitPeerConnectionEnabled"));
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

void TestRunner::setFetchAPIEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitFetchAPIEnabled"));
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundleOverrideBoolPreferenceForTestRunner(injectedBundle.bundle(), injectedBundle.pageGroup(), key.get(), enabled);
}

void TestRunner::setDownloadAttributeEnabled(bool enabled)
{
    WKRetainPtr<WKStringRef> key(AdoptWK, WKStringCreateWithUTF8CString("WebKitDownloadAttributeEnabled"));
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
    InjectedBundle::singleton().setAllowsAnySSLCertificate(enabled);
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
    m_whatToDump = Audio;
    m_dumpPixels = false;
}

unsigned TestRunner::windowCount()
{
    return InjectedBundle::singleton().pageCount();
}

void TestRunner::clearBackForwardList()
{
    WKBundleBackForwardListClear(WKBundlePageGetBackForwardList(InjectedBundle::singleton().page()->page()));
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
    StatisticsDidModifyDataRecordsCallbackID,
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
    return currentTime();
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

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy, JSValueRef jsAltitude, JSValueRef jsAltitudeAccuracy, JSValueRef jsHeading, JSValueRef jsSpeed)
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

    injectedBundle.setMockGeolocationPosition(latitude, longitude, accuracy, providesAltitude, altitude, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed);
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

void TestRunner::setStatisticsPrevalentResource(JSStringRef hostName, bool value)
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
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsPrevalentResource"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));

    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

bool TestRunner::isStatisticsPrevalentResource(JSStringRef hostName)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsPrevalentResource"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
}

void TestRunner::setStatisticsHasHadUserInteraction(JSStringRef hostName, bool value)
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
    
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsHasHadUserInteraction"));
    WKRetainPtr<WKDictionaryRef> messageBody(AdoptWK, WKDictionaryCreate(rawKeys.data(), rawValues.data(), rawKeys.size()));
    
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}

bool TestRunner::isStatisticsHasHadUserInteraction(JSStringRef hostName)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsStatisticsHasHadUserInteraction"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithJSString(hostName));
    WKTypeRef returnData = 0;
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), messageName.get(), messageBody.get(), &returnData);
    return WKBooleanGetValue(static_cast<WKBooleanRef>(returnData));
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

void TestRunner::statisticsFireDataModificationHandler()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsFireDataModificationHandler"));
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

void TestRunner::setStatisticsMinimumTimeBetweeenDataRecordsRemoval(double seconds)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetStatisticsMinimumTimeBetweeenDataRecordsRemoval"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(seconds));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), messageBody.get(), nullptr);
}
    
void TestRunner::statisticsResetToConsistentState()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("StatisticsResetToConsistentState"));
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), messageName.get(), 0, nullptr);
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

} // namespace WTR
