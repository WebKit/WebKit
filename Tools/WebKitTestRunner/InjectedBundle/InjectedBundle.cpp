/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "InjectedBundle.h"

#include "ActivateFonts.h"
#include "InjectedBundlePage.h"
#include "StringFunctions.h"
#include "WebCoreTestSupport.h"
#include <JavaScriptCore/Options.h>
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WebKit2_C.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/Vector.h>

namespace WTR {

static void handleTextDidChangeInTextField(WKBundlePageRef, WKBundleNodeHandleRef, WKBundleFrameRef, const void* context)
{
    static_cast<InjectedBundle*>(const_cast<void*>(context))->textDidChangeInTextField();
}

static void handleTextFieldDidBeginEditing(WKBundlePageRef, WKBundleNodeHandleRef, WKBundleFrameRef, const void* context)
{
    static_cast<InjectedBundle*>(const_cast<void*>(context))->textFieldDidBeginEditing();
}

static void handleTextFieldDidEndEditing(WKBundlePageRef, WKBundleNodeHandleRef, WKBundleFrameRef, const void* context)
{
    static_cast<InjectedBundle*>(const_cast<void*>(context))->textFieldDidEndEditing();
}

InjectedBundle& InjectedBundle::singleton()
{
    static InjectedBundle& shared = *new InjectedBundle;
    return shared;
}

void InjectedBundle::didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didCreatePage(page);
}

void InjectedBundle::willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->willDestroyPage(page);
}

void InjectedBundle::didInitializePageGroup(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didInitializePageGroup(pageGroup);
}

void InjectedBundle::didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didReceiveMessage(messageName, messageBody);
}

void InjectedBundle::didReceiveMessageToPage(WKBundleRef bundle, WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didReceiveMessageToPage(page, messageName, messageBody);
}

void InjectedBundle::initialize(WKBundleRef bundle, WKTypeRef initializationUserData)
{
    m_bundle = bundle;

    WKBundleClientV1 client = {
        { 1, this },
        didCreatePage,
        willDestroyPage,
        didInitializePageGroup,
        didReceiveMessage,
        didReceiveMessageToPage
    };
    WKBundleSetClient(m_bundle, &client.base);
    WKBundleSetServiceWorkerProxyCreationCallback(m_bundle, WebCoreTestSupport::setupNewlyCreatedServiceWorker);
    platformInitialize(initializationUserData);

    activateFonts();
}

void InjectedBundle::didCreatePage(WKBundlePageRef page)
{
    bool isMainPage = m_pages.isEmpty();
    m_pages.append(std::make_unique<InjectedBundlePage>(page));

    setUpInjectedBundleClients(page);

    if (!isMainPage)
        return;

    WKRetainPtr<WKStringRef> messsageName(AdoptWK, WKStringCreateWithUTF8CString("Initialization"));
    WKTypeRef result = 0;
    WKBundlePostSynchronousMessage(m_bundle, messsageName.get(), 0, &result);
    ASSERT(WKGetTypeID(result) == WKDictionaryGetTypeID());
    WKDictionaryRef initializationDictionary = static_cast<WKDictionaryRef>(result);

    WKRetainPtr<WKStringRef> resumeTestingKey(AdoptWK, WKStringCreateWithUTF8CString("ResumeTesting"));
    WKTypeRef resumeTestingValue = WKDictionaryGetItemForKey(initializationDictionary, resumeTestingKey.get());
    ASSERT(WKGetTypeID(resumeTestingValue) == WKBooleanGetTypeID());
    if (WKBooleanGetValue(static_cast<WKBooleanRef>(resumeTestingValue)))
        beginTesting(initializationDictionary, BegingTestingMode::Resume);
}

void InjectedBundle::willDestroyPage(WKBundlePageRef page)
{
    m_pages.removeFirstMatching([page](auto& current) {
        return current->page() == page;
    });
}

void InjectedBundle::didInitializePageGroup(WKBundlePageGroupRef pageGroup)
{
    m_pageGroup = pageGroup;
}

void InjectedBundle::setUpInjectedBundleClients(WKBundlePageRef page)
{
    WKBundlePageFormClientV2 formClient = {
        { 2, this },
        handleTextFieldDidBeginEditing,
        handleTextFieldDidEndEditing,
        handleTextDidChangeInTextField,
        0, // textDidChangeInTextArea
        0, // shouldPerformActionInTextField
        0, // willSubmitForm
        0, // willSendSubmitEvent
        0, // didFocusTextField
        0, // shouldNotifyOnFormChanges
        0, // didAssociateFormControls
    };
    WKBundlePageSetFormClient(page, &formClient.base);
}

InjectedBundlePage* InjectedBundle::page() const
{
    // It might be better to have the UI process send over a reference to the main
    // page instead of just assuming it's the first one.
    return m_pages[0].get();
}

void InjectedBundle::resetLocalSettings()
{
    setlocale(LC_ALL, "");
}

void InjectedBundle::didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody)
{
    WKRetainPtr<WKStringRef> errorMessageName(AdoptWK, WKStringCreateWithUTF8CString("Error"));
    WKRetainPtr<WKStringRef> errorMessageBody(AdoptWK, WKStringCreateWithUTF8CString("Unknown"));
    WKBundlePostMessage(m_bundle, errorMessageName.get(), errorMessageBody.get());
}

static void postGCTask(void* context)
{
    WKBundlePageRef page = reinterpret_cast<WKBundlePageRef>(context);
    InjectedBundle::singleton().reportLiveDocuments(page);
    WKRelease(page);
}

void InjectedBundle::reportLiveDocuments(WKBundlePageRef page)
{
    // Release memory again, after the GC and timer fire. This is necessary to clear entries from CachedResourceLoader's m_documentResources in some scenarios.
    WKBundleReleaseMemory(m_bundle);

    const bool excludeDocumentsInPageGroup = true;
    auto documentURLs = adoptWK(WKBundleGetLiveDocumentURLs(m_bundle, m_pageGroup, excludeDocumentsInPageGroup));
    auto ackMessageName = adoptWK(WKStringCreateWithUTF8CString("LiveDocuments"));
    WKBundlePagePostMessage(page, ackMessageName.get(), documentURLs.get());
}

void InjectedBundle::didReceiveMessageToPage(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "BeginTest")) {
        ASSERT(messageBody);
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> dumpPixelsKey(AdoptWK, WKStringCreateWithUTF8CString("DumpPixels"));
        m_dumpPixels = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, dumpPixelsKey.get())));

        WKRetainPtr<WKStringRef> useWaitToDumpWatchdogTimerKey(AdoptWK, WKStringCreateWithUTF8CString("UseWaitToDumpWatchdogTimer"));
        m_useWaitToDumpWatchdogTimer = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, useWaitToDumpWatchdogTimerKey.get())));

        WKRetainPtr<WKStringRef> timeoutKey(AdoptWK, WKStringCreateWithUTF8CString("Timeout"));
        m_timeout = Seconds::fromMilliseconds(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, timeoutKey.get()))));

        WKRetainPtr<WKStringRef> dumpJSConsoleLogInStdErrKey(AdoptWK, WKStringCreateWithUTF8CString("DumpJSConsoleLogInStdErr"));
        m_dumpJSConsoleLogInStdErr = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, dumpJSConsoleLogInStdErrKey.get())));

        WKRetainPtr<WKStringRef> ackMessageName(AdoptWK, WKStringCreateWithUTF8CString("Ack"));
        WKRetainPtr<WKStringRef> ackMessageBody(AdoptWK, WKStringCreateWithUTF8CString("BeginTest"));
        WKBundlePagePostMessage(page, ackMessageName.get(), ackMessageBody.get());

        beginTesting(messageBodyDictionary, BegingTestingMode::New);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Reset")) {
        ASSERT(messageBody);
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        WKRetainPtr<WKStringRef> jscOptionsKey(AdoptWK, WKStringCreateWithUTF8CString("JSCOptions"));
        WKRetainPtr<WKStringRef> jscOptionsString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, jscOptionsKey.get()));
        if (jscOptionsString) {
            String options = toWTFString(jscOptionsString);
            JSC::Options::setOptions(options.utf8().data());
        }

        WKRetainPtr<WKStringRef> shouldGCKey(AdoptWK, WKStringCreateWithUTF8CString("ShouldGC"));
        bool shouldGC = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, shouldGCKey.get())));
        if (shouldGC)
            WKBundleGarbageCollectJavaScriptObjects(m_bundle);

        WKRetainPtr<WKStringRef> allowedHostsKey(AdoptWK, WKStringCreateWithUTF8CString("AllowedHosts"));
        WKTypeRef allowedHostsValue = WKDictionaryGetItemForKey(messageBodyDictionary, allowedHostsKey.get());
        if (allowedHostsValue && WKGetTypeID(allowedHostsValue) == WKArrayGetTypeID()) {
            m_allowedHosts.clear();

            WKArrayRef allowedHostsArray = static_cast<WKArrayRef>(allowedHostsValue);
            for (size_t i = 0, size = WKArrayGetSize(allowedHostsArray); i < size; ++i) {
                WKTypeRef item = WKArrayGetItemAtIndex(allowedHostsArray, i);
                if (item && WKGetTypeID(item) == WKStringGetTypeID())
                    m_allowedHosts.append(toWTFString(static_cast<WKStringRef>(item)));
            }
        }

        m_state = Idle;
        m_dumpPixels = false;
        m_pixelResultIsPending = false;

        resetLocalSettings();
        TestRunner::removeAllWebNotificationPermissions();

        InjectedBundle::page()->resetAfterTest();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetLiveDocuments")) {
        const bool excludeDocumentsInPageGroup = false;
        auto documentURLs = adoptWK(WKBundleGetLiveDocumentURLs(m_bundle, m_pageGroup, excludeDocumentsInPageGroup));
        auto ackMessageName = adoptWK(WKStringCreateWithUTF8CString("LiveDocuments"));
        WKBundlePagePostMessage(page, ackMessageName.get(), documentURLs.get());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CheckForWorldLeaks")) {
        WKBundleReleaseMemory(m_bundle);

        WKRetain(page); // Balanced by the release in postGCTask.
        WKBundlePageCallAfterTasksAndTimers(page, postGCTask, (void*)page);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallAddChromeInputFieldCallback")) {
        m_testRunner->callAddChromeInputFieldCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallRemoveChromeInputFieldCallback")) {
        m_testRunner->callRemoveChromeInputFieldCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallFocusWebViewCallback")) {
        m_testRunner->callFocusWebViewCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallSetBackingScaleFactorCallback")) {
        m_testRunner->callSetBackingScaleFactorCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidBeginSwipeCallback")) {
        m_testRunner->callDidBeginSwipeCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallWillEndSwipeCallback")) {
        m_testRunner->callWillEndSwipeCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidEndSwipeCallback")) {
        m_testRunner->callDidEndSwipeCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidRemoveSwipeSnapshotCallback")) {
        m_testRunner->callDidRemoveSwipeSnapshotCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidClearStatisticsThroughWebsiteDataRemoval")) {
        m_testRunner->statisticsCallClearThroughWebsiteDataRemovalCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidResetStatisticsToConsistentState")) {
        m_testRunner->statisticsCallDidResetToConsistentStateCallback();
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetBlockCookiesForHost")) {
        m_testRunner->statisticsCallDidSetBlockCookiesForHostCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetStatisticsDebugMode")) {
        m_testRunner->statisticsCallDidSetDebugModeCallback();
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetPrevalentResourceForDebugMode")) {
        m_testRunner->statisticsCallDidSetPrevalentResourceForDebugModeCallback();
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetLastSeen")) {
        m_testRunner->statisticsCallDidSetLastSeenCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetPrevalentResource")) {
        m_testRunner->statisticsCallDidSetPrevalentResourceCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetVeryPrevalentResource")) {
        m_testRunner->statisticsCallDidSetVeryPrevalentResourceCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidSetHasHadUserInteraction")) {
        m_testRunner->statisticsCallDidSetHasHadUserInteractionCallback();
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "CallDidReceiveAllStorageAccessEntries")) {
        ASSERT(messageBody);
        ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());

        WKArrayRef domainsArray = static_cast<WKArrayRef>(messageBody);
        auto size = WKArrayGetSize(domainsArray);
        Vector<String> domains;
        domains.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i) {
            WKTypeRef item = WKArrayGetItemAtIndex(domainsArray, i);
            if (item && WKGetTypeID(item) == WKStringGetTypeID())
                domains.append(toWTFString(static_cast<WKStringRef>(item)));
        }

        m_testRunner->callDidReceiveAllStorageAccessEntriesCallback(domains);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidRemoveAllSessionCredentialsCallback")) {
        m_testRunner->callDidRemoveAllSessionCredentialsCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "NotifyDownloadDone")) {
        if (m_testRunner->shouldFinishAfterDownload())
            m_testRunner->notifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallUISideScriptCallback")) {
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> resultKey(AdoptWK, WKStringCreateWithUTF8CString("Result"));
        WKRetainPtr<WKStringRef> callbackIDKey(AdoptWK, WKStringCreateWithUTF8CString("CallbackID"));

        unsigned callbackID = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, callbackIDKey.get())));

        WKStringRef resultString = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, resultKey.get()));
        auto resultJSString = toJS(resultString);

        m_testRunner->runUIScriptCallback(callbackID, resultJSString.get());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "WorkQueueProcessedCallback")) {
        if (!topLoadingFrame() && !m_testRunner->shouldWaitUntilDone())
            InjectedBundle::page()->dump();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "WebsiteDataDeletionForTopPrivatelyOwnedDomainsFinished")) {
        m_testRunner->statisticsDidModifyDataRecordsCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "WebsiteDataScanForTopPrivatelyControlledDomainsFinished")) {
        m_testRunner->statisticsDidScanDataRecordsCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResourceLoadStatisticsTelemetryFinished")) {
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);
        
        unsigned totalPrevalentResources = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, WKStringCreateWithUTF8CString("TotalPrevalentResources"))));
        unsigned totalPrevalentResourcesWithUserInteraction = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, WKStringCreateWithUTF8CString("TotalPrevalentResourcesWithUserInteraction"))));
        unsigned top3SubframeUnderTopFrameOrigins = (unsigned)WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, WKStringCreateWithUTF8CString("Top3SubframeUnderTopFrameOrigins"))));
        
        m_testRunner->statisticsDidRunTelemetryCallback(totalPrevalentResources, totalPrevalentResourcesWithUserInteraction, top3SubframeUnderTopFrameOrigins);
        return;
    }
    
    if (WKStringIsEqualToUTF8CString(messageName, "DidGetApplicationManifest")) {
        m_testRunner->didGetApplicationManifest();
        return;
    }
    
    WKRetainPtr<WKStringRef> errorMessageName(AdoptWK, WKStringCreateWithUTF8CString("Error"));
    WKRetainPtr<WKStringRef> errorMessageBody(AdoptWK, WKStringCreateWithUTF8CString("Unknown"));
    WKBundlePagePostMessage(page, errorMessageName.get(), errorMessageBody.get());
}

bool InjectedBundle::booleanForKey(WKDictionaryRef dictionary, const char* key)
{
    WKRetainPtr<WKStringRef> wkKey(AdoptWK, WKStringCreateWithUTF8CString(key));
    WKTypeRef value = WKDictionaryGetItemForKey(dictionary, wkKey.get());
    if (WKGetTypeID(value) != WKBooleanGetTypeID()) {
        outputText(makeString("Boolean value for key", key, " not found in dictionary\n"));
        return false;
    }
    return WKBooleanGetValue(static_cast<WKBooleanRef>(value));
}

void InjectedBundle::beginTesting(WKDictionaryRef settings, BegingTestingMode testingMode)
{
    m_state = Testing;

    m_pixelResult.clear();
    m_repaintRects.clear();

    m_testRunner = TestRunner::create();
    m_gcController = GCController::create();
    m_eventSendingController = EventSendingController::create();
    m_textInputController = TextInputController::create();
#if HAVE(ACCESSIBILITY)
    m_accessibilityController = AccessibilityController::create();
#endif

    WKBundleSetAllowUniversalAccessFromFileURLs(m_bundle, m_pageGroup, true);
    WKBundleSetJavaScriptCanAccessClipboard(m_bundle, m_pageGroup, true);
    WKBundleSetPrivateBrowsingEnabled(m_bundle, m_pageGroup, false);
    WKBundleSetUseDashboardCompatibilityMode(m_bundle, m_pageGroup, false);
    WKBundleSetAuthorAndUserStylesEnabled(m_bundle, m_pageGroup, true);
    WKBundleSetFrameFlatteningEnabled(m_bundle, m_pageGroup, false);
    WKBundleSetAsyncFrameScrollingEnabled(m_bundle, m_pageGroup, false);
    WKBundleSetMinimumLogicalFontSize(m_bundle, m_pageGroup, 9);
    WKBundleSetSpatialNavigationEnabled(m_bundle, m_pageGroup, false);
    WKBundleSetAllowFileAccessFromFileURLs(m_bundle, m_pageGroup, true);
    WKBundleSetPopupBlockingEnabled(m_bundle, m_pageGroup, false);
    WKBundleSetAllowStorageAccessFromFileURLS(m_bundle, m_pageGroup, false);

#if PLATFORM(IOS)
    WKBundlePageSetUseTestingViewportConfiguration(page()->page(), !booleanForKey(settings, "UseFlexibleViewport"));
#endif

    m_testRunner->setPluginsEnabled(true);

    m_testRunner->setUserStyleSheetEnabled(false);
    m_testRunner->setXSSAuditorEnabled(false);

    m_testRunner->setWebGL2Enabled(true);
    m_testRunner->setWebMetalEnabled(true);

    m_testRunner->setWritableStreamAPIEnabled(true);
    m_testRunner->setReadableByteStreamAPIEnabled(true);

    m_testRunner->setEncryptedMediaAPIEnabled(true);

    m_testRunner->setCloseRemainingWindowsWhenComplete(false);
    m_testRunner->setAcceptsEditing(true);
    m_testRunner->setTabKeyCyclesThroughElements(true);
    m_testRunner->clearTestRunnerCallbacks();

    if (m_timeout > 0_s)
        m_testRunner->setCustomTimeout(m_timeout);

    page()->prepare();

    if (testingMode != BegingTestingMode::New)
        return;

    WKBundleClearAllDatabases(m_bundle);
    WKBundlePageClearApplicationCache(page()->page());
    WKBundleResetOriginAccessWhitelists(m_bundle);
    WKBundleClearResourceLoadStatistics(m_bundle);

    // [WK2] REGRESSION(r128623): It made layout tests extremely slow
    // https://bugs.webkit.org/show_bug.cgi?id=96862
    // WKBundleSetDatabaseQuota(m_bundle, 5 * 1024 * 1024);
}

void InjectedBundle::done()
{
    m_state = Stopping;

    m_useWorkQueue = false;

    page()->stopLoading();
    setTopLoadingFrame(0);

    m_testRunner->invalidateWaitToDumpWatchdogTimer();

#if HAVE(ACCESSIBILITY)
    m_accessibilityController->resetToConsistentState();
#endif

    WKRetainPtr<WKStringRef> doneMessageName(AdoptWK, WKStringCreateWithUTF8CString("Done"));
    WKRetainPtr<WKMutableDictionaryRef> doneMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> pixelResultIsPendingKey = adoptWK(WKStringCreateWithUTF8CString("PixelResultIsPending"));
    WKRetainPtr<WKBooleanRef> pixelResultIsPending(AdoptWK, WKBooleanCreate(m_pixelResultIsPending));
    WKDictionarySetItem(doneMessageBody.get(), pixelResultIsPendingKey.get(), pixelResultIsPending.get());

    if (!m_pixelResultIsPending) {
        WKRetainPtr<WKStringRef> pixelResultKey = adoptWK(WKStringCreateWithUTF8CString("PixelResult"));
        WKDictionarySetItem(doneMessageBody.get(), pixelResultKey.get(), m_pixelResult.get());
    }

    WKRetainPtr<WKStringRef> repaintRectsKey = adoptWK(WKStringCreateWithUTF8CString("RepaintRects"));
    WKDictionarySetItem(doneMessageBody.get(), repaintRectsKey.get(), m_repaintRects.get());

    WKRetainPtr<WKStringRef> audioResultKey = adoptWK(WKStringCreateWithUTF8CString("AudioResult"));
    WKDictionarySetItem(doneMessageBody.get(), audioResultKey.get(), m_audioResult.get());

    WKBundlePagePostMessageIgnoringFullySynchronousMode(page()->page(), doneMessageName.get(), doneMessageBody.get());

    closeOtherPages();

    m_state = Idle;
}

void InjectedBundle::closeOtherPages()
{
    Vector<WKBundlePageRef> pagesToClose;
    size_t size = m_pages.size();
    for (size_t i = 1; i < size; ++i)
        pagesToClose.append(m_pages[i]->page());
    size = pagesToClose.size();
    for (size_t i = 0; i < size; ++i)
        WKBundlePageClose(pagesToClose[i]);
}

void InjectedBundle::dumpBackForwardListsForAllPages(StringBuilder& stringBuilder)
{
    size_t size = m_pages.size();
    for (size_t i = 0; i < size; ++i)
        m_pages[i]->dumpBackForwardList(stringBuilder);
}

void InjectedBundle::dumpToStdErr(const String& output)
{
    if (m_state != Testing)
        return;
    if (output.isEmpty())
        return;
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("DumpToStdErr"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithUTF8CString(output.utf8().data()));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::outputText(const String& output)
{
    if (m_state != Testing)
        return;
    if (output.isEmpty())
        return;
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("TextOutput"));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithUTF8CString(output.utf8().data()));
    // We use WKBundlePagePostMessageIgnoringFullySynchronousMode() instead of WKBundlePagePostMessage() to make sure that all text output
    // is done via asynchronous IPC, even if the connection is in fully synchronous mode due to a WKBundlePagePostSynchronousMessageForTesting()
    // call. Otherwise, messages logged via sync and async IPC may end up out of order and cause flakiness.
    WKBundlePagePostMessageIgnoringFullySynchronousMode(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::postNewBeforeUnloadReturnValue(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("BeforeUnloadReturnValue"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::postAddChromeInputField()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("AddChromeInputField"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), 0);
}

void InjectedBundle::postRemoveChromeInputField()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("RemoveChromeInputField"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), 0);
}

void InjectedBundle::postFocusWebView()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("FocusWebView"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), 0);
}

void InjectedBundle::postSetBackingScaleFactor(double backingScaleFactor)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetBackingScaleFactor"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(backingScaleFactor));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::postSetWindowIsKey(bool isKey)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetWindowIsKey"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(isKey));
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), messageName.get(), messageBody.get(), 0);
}

void InjectedBundle::postSetViewSize(double width, double height)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetViewSize"));

    WKRetainPtr<WKStringRef> widthKey(AdoptWK, WKStringCreateWithUTF8CString("width"));
    WKRetainPtr<WKStringRef> heightKey(AdoptWK, WKStringCreateWithUTF8CString("height"));

    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKDoubleRef> widthWK(AdoptWK, WKDoubleCreate(width));
    WKDictionarySetItem(messageBody.get(), widthKey.get(), widthWK.get());

    WKRetainPtr<WKDoubleRef> heightWK(AdoptWK, WKDoubleCreate(height));
    WKDictionarySetItem(messageBody.get(), heightKey.get(), heightWK.get());

    WKBundlePagePostSynchronousMessageForTesting(page()->page(), messageName.get(), messageBody.get(), 0);
}

void InjectedBundle::postSimulateWebNotificationClick(uint64_t notificationID)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SimulateWebNotificationClick"));
    WKRetainPtr<WKUInt64Ref> messageBody(AdoptWK, WKUInt64Create(notificationID));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::postSetAddsVisitedLinks(bool addsVisitedLinks)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetAddsVisitedLinks"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(addsVisitedLinks));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::setGeolocationPermission(bool enabled)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetGeolocationPermission"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(enabled));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, bool providesFloorLevel, double floorLevel)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetMockGeolocationPosition"));

    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> latitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("latitude"));
    WKRetainPtr<WKDoubleRef> latitudeWK(AdoptWK, WKDoubleCreate(latitude));
    WKDictionarySetItem(messageBody.get(), latitudeKeyWK.get(), latitudeWK.get());

    WKRetainPtr<WKStringRef> longitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("longitude"));
    WKRetainPtr<WKDoubleRef> longitudeWK(AdoptWK, WKDoubleCreate(longitude));
    WKDictionarySetItem(messageBody.get(), longitudeKeyWK.get(), longitudeWK.get());

    WKRetainPtr<WKStringRef> accuracyKeyWK(AdoptWK, WKStringCreateWithUTF8CString("accuracy"));
    WKRetainPtr<WKDoubleRef> accuracyWK(AdoptWK, WKDoubleCreate(accuracy));
    WKDictionarySetItem(messageBody.get(), accuracyKeyWK.get(), accuracyWK.get());

    WKRetainPtr<WKStringRef> providesAltitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesAltitude"));
    WKRetainPtr<WKBooleanRef> providesAltitudeWK(AdoptWK, WKBooleanCreate(providesAltitude));
    WKDictionarySetItem(messageBody.get(), providesAltitudeKeyWK.get(), providesAltitudeWK.get());

    WKRetainPtr<WKStringRef> altitudeKeyWK(AdoptWK, WKStringCreateWithUTF8CString("altitude"));
    WKRetainPtr<WKDoubleRef> altitudeWK(AdoptWK, WKDoubleCreate(altitude));
    WKDictionarySetItem(messageBody.get(), altitudeKeyWK.get(), altitudeWK.get());

    WKRetainPtr<WKStringRef> providesAltitudeAccuracyKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesAltitudeAccuracy"));
    WKRetainPtr<WKBooleanRef> providesAltitudeAccuracyWK(AdoptWK, WKBooleanCreate(providesAltitudeAccuracy));
    WKDictionarySetItem(messageBody.get(), providesAltitudeAccuracyKeyWK.get(), providesAltitudeAccuracyWK.get());

    WKRetainPtr<WKStringRef> altitudeAccuracyKeyWK(AdoptWK, WKStringCreateWithUTF8CString("altitudeAccuracy"));
    WKRetainPtr<WKDoubleRef> altitudeAccuracyWK(AdoptWK, WKDoubleCreate(altitudeAccuracy));
    WKDictionarySetItem(messageBody.get(), altitudeAccuracyKeyWK.get(), altitudeAccuracyWK.get());

    WKRetainPtr<WKStringRef> providesHeadingKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesHeading"));
    WKRetainPtr<WKBooleanRef> providesHeadingWK(AdoptWK, WKBooleanCreate(providesHeading));
    WKDictionarySetItem(messageBody.get(), providesHeadingKeyWK.get(), providesHeadingWK.get());

    WKRetainPtr<WKStringRef> headingKeyWK(AdoptWK, WKStringCreateWithUTF8CString("heading"));
    WKRetainPtr<WKDoubleRef> headingWK(AdoptWK, WKDoubleCreate(heading));
    WKDictionarySetItem(messageBody.get(), headingKeyWK.get(), headingWK.get());

    WKRetainPtr<WKStringRef> providesSpeedKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesSpeed"));
    WKRetainPtr<WKBooleanRef> providesSpeedWK(AdoptWK, WKBooleanCreate(providesSpeed));
    WKDictionarySetItem(messageBody.get(), providesSpeedKeyWK.get(), providesSpeedWK.get());

    WKRetainPtr<WKStringRef> speedKeyWK(AdoptWK, WKStringCreateWithUTF8CString("speed"));
    WKRetainPtr<WKDoubleRef> speedWK(AdoptWK, WKDoubleCreate(speed));
    WKDictionarySetItem(messageBody.get(), speedKeyWK.get(), speedWK.get());

    WKRetainPtr<WKStringRef> providesFloorLevelKeyWK(AdoptWK, WKStringCreateWithUTF8CString("providesFloorLevel"));
    WKRetainPtr<WKBooleanRef> providesFloorLevelWK(AdoptWK, WKBooleanCreate(providesFloorLevel));
    WKDictionarySetItem(messageBody.get(), providesFloorLevelKeyWK.get(), providesFloorLevelWK.get());

    WKRetainPtr<WKStringRef> floorLevelKeyWK(AdoptWK, WKStringCreateWithUTF8CString("floorLevel"));
    WKRetainPtr<WKDoubleRef> floorLevelWK(AdoptWK, WKDoubleCreate(floorLevel));
    WKDictionarySetItem(messageBody.get(), floorLevelKeyWK.get(), floorLevelWK.get());

    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::setMockGeolocationPositionUnavailableError(WKStringRef errorMessage)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetMockGeolocationPositionUnavailableError"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), errorMessage);
}

bool InjectedBundle::isGeolocationProviderActive() const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsGeolocationClientActive"));
    WKTypeRef resultToPass = 0;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), messageName.get(), 0, &resultToPass);
    WKRetainPtr<WKBooleanRef> isActive(AdoptWK, static_cast<WKBooleanRef>(resultToPass));

    return WKBooleanGetValue(isActive.get());
}

unsigned InjectedBundle::imageCountInGeneralPasteboard() const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ImageCountInGeneralPasteboard"));
    WKTypeRef resultToPass = 0;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), messageName.get(), 0, &resultToPass);
    WKRetainPtr<WKUInt64Ref> imageCount(AdoptWK, static_cast<WKUInt64Ref>(resultToPass));
    
    return static_cast<unsigned>(WKUInt64GetValue(imageCount.get()));
}

void InjectedBundle::setUserMediaPermission(bool enabled)
{
    auto messageName = adoptWK(WKStringCreateWithUTF8CString("SetUserMediaPermission"));
    auto messageBody = adoptWK(WKBooleanCreate(enabled));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::resetUserMediaPermission()
{
    auto messageName = adoptWK(WKStringCreateWithUTF8CString("ResetUserMediaPermission"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), 0);
}

void InjectedBundle::setUserMediaPersistentPermissionForOrigin(bool permission, WKStringRef origin, WKStringRef parentOrigin)
{
    auto messageName = adoptWK(WKStringCreateWithUTF8CString("SetUserMediaPersistentPermissionForOrigin"));
    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> permissionKeyWK(AdoptWK, WKStringCreateWithUTF8CString("permission"));
    WKRetainPtr<WKBooleanRef> permissionWK(AdoptWK, WKBooleanCreate(permission));
    WKDictionarySetItem(messageBody.get(), permissionKeyWK.get(), permissionWK.get());

    WKRetainPtr<WKStringRef> originKeyWK(AdoptWK, WKStringCreateWithUTF8CString("origin"));
    WKDictionarySetItem(messageBody.get(), originKeyWK.get(), origin);

    WKRetainPtr<WKStringRef> parentOriginKeyWK(AdoptWK, WKStringCreateWithUTF8CString("parentOrigin"));
    WKDictionarySetItem(messageBody.get(), parentOriginKeyWK.get(), parentOrigin);

    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

unsigned InjectedBundle::userMediaPermissionRequestCountForOrigin(WKStringRef origin, WKStringRef parentOrigin) const
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("UserMediaPermissionRequestCountForOrigin"));
    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> originKeyWK(AdoptWK, WKStringCreateWithUTF8CString("origin"));
    WKDictionarySetItem(messageBody.get(), originKeyWK.get(), origin);

    WKRetainPtr<WKStringRef> parentOriginKeyWK(AdoptWK, WKStringCreateWithUTF8CString("parentOrigin"));
    WKDictionarySetItem(messageBody.get(), parentOriginKeyWK.get(), parentOrigin);

    WKTypeRef resultToPass = 0;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), messageName.get(), messageBody.get(), &resultToPass);
    WKRetainPtr<WKUInt64Ref> count(AdoptWK, static_cast<WKUInt64Ref>(resultToPass));

    return static_cast<unsigned>(WKUInt64GetValue(count.get()));
}

void InjectedBundle::resetUserMediaPermissionRequestCountForOrigin(WKStringRef origin, WKStringRef parentOrigin)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ResetUserMediaPermissionRequestCountForOrigin"));
    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> originKeyWK(AdoptWK, WKStringCreateWithUTF8CString("origin"));
    WKDictionarySetItem(messageBody.get(), originKeyWK.get(), origin);

    WKRetainPtr<WKStringRef> parentOriginKeyWK(AdoptWK, WKStringCreateWithUTF8CString("parentOrigin"));
    WKDictionarySetItem(messageBody.get(), parentOriginKeyWK.get(), parentOrigin);

    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetCustomPolicyDelegate"));

    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> enabledKeyWK(AdoptWK, WKStringCreateWithUTF8CString("enabled"));
    WKRetainPtr<WKBooleanRef> enabledWK(AdoptWK, WKBooleanCreate(enabled));
    WKDictionarySetItem(messageBody.get(), enabledKeyWK.get(), enabledWK.get());

    WKRetainPtr<WKStringRef> permissiveKeyWK(AdoptWK, WKStringCreateWithUTF8CString("permissive"));
    WKRetainPtr<WKBooleanRef> permissiveWK(AdoptWK, WKBooleanCreate(permissive));
    WKDictionarySetItem(messageBody.get(), permissiveKeyWK.get(), permissiveWK.get());

    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::setHidden(bool hidden)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetHidden"));
    WKRetainPtr<WKMutableDictionaryRef> messageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> isInitialKeyWK(AdoptWK, WKStringCreateWithUTF8CString("hidden"));
    WKRetainPtr<WKBooleanRef> isInitialWK(AdoptWK, WKBooleanCreate(hidden));
    WKDictionarySetItem(messageBody.get(), isInitialKeyWK.get(), isInitialWK.get());

    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::setCacheModel(int model)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetCacheModel"));
    WKRetainPtr<WKUInt64Ref> messageBody(AdoptWK, WKUInt64Create(model));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

bool InjectedBundle::shouldProcessWorkQueue() const
{
    if (!m_useWorkQueue)
        return false;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("IsWorkQueueEmpty"));
    WKTypeRef resultToPass = 0;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), messageName.get(), 0, &resultToPass);
    WKRetainPtr<WKBooleanRef> isEmpty(AdoptWK, static_cast<WKBooleanRef>(resultToPass));

    // The IPC failed. This happens when swapping processes on navigation because the WebPageProxy unregisters itself
    // as a MessageReceiver from the old WebProcessProxy and register itself with the new WebProcessProxy instead.
    if (!isEmpty)
        return false;

    return !WKBooleanGetValue(isEmpty.get());
}

void InjectedBundle::processWorkQueue()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("ProcessWorkQueue"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), 0);
}

void InjectedBundle::queueBackNavigation(unsigned howFarBackward)
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueBackNavigation"));
    WKRetainPtr<WKUInt64Ref> messageBody(AdoptWK, WKUInt64Create(howFarBackward));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::queueForwardNavigation(unsigned howFarForward)
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueForwardNavigation"));
    WKRetainPtr<WKUInt64Ref> messageBody(AdoptWK, WKUInt64Create(howFarForward));
    WKBundlePagePostMessage(page()->page(), messageName.get(), messageBody.get());
}

void InjectedBundle::queueLoad(WKStringRef url, WKStringRef target, bool shouldOpenExternalURLs)
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueLoad"));

    WKRetainPtr<WKMutableDictionaryRef> loadData(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> urlKey(AdoptWK, WKStringCreateWithUTF8CString("url"));
    WKDictionarySetItem(loadData.get(), urlKey.get(), url);

    WKRetainPtr<WKStringRef> targetKey(AdoptWK, WKStringCreateWithUTF8CString("target"));
    WKDictionarySetItem(loadData.get(), targetKey.get(), target);

    WKRetainPtr<WKStringRef> shouldOpenExternalURLsKey(AdoptWK, WKStringCreateWithUTF8CString("shouldOpenExternalURLs"));
    WKRetainPtr<WKBooleanRef> shouldOpenExternalURLsValue(AdoptWK, WKBooleanCreate(shouldOpenExternalURLs));
    WKDictionarySetItem(loadData.get(), shouldOpenExternalURLsKey.get(), shouldOpenExternalURLsValue.get());

    WKBundlePagePostMessage(page()->page(), messageName.get(), loadData.get());
}

void InjectedBundle::queueLoadHTMLString(WKStringRef content, WKStringRef baseURL, WKStringRef unreachableURL)
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueLoadHTMLString"));

    WKRetainPtr<WKMutableDictionaryRef> loadData(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> contentKey(AdoptWK, WKStringCreateWithUTF8CString("content"));
    WKDictionarySetItem(loadData.get(), contentKey.get(), content);

    if (baseURL) {
        WKRetainPtr<WKStringRef> baseURLKey(AdoptWK, WKStringCreateWithUTF8CString("baseURL"));
        WKDictionarySetItem(loadData.get(), baseURLKey.get(), baseURL);
    }

    if (unreachableURL) {
        WKRetainPtr<WKStringRef> unreachableURLKey(AdoptWK, WKStringCreateWithUTF8CString("unreachableURL"));
        WKDictionarySetItem(loadData.get(), unreachableURLKey.get(), unreachableURL);
    }

    WKBundlePagePostMessage(page()->page(), messageName.get(), loadData.get());
}

void InjectedBundle::queueReload()
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueReload"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), 0);
}

void InjectedBundle::queueLoadingScript(WKStringRef script)
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueLoadingScript"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), script);
}

void InjectedBundle::queueNonLoadingScript(WKStringRef script)
{
    m_useWorkQueue = true;

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("QueueNonLoadingScript"));
    WKBundlePagePostMessage(page()->page(), messageName.get(), script);
}

bool InjectedBundle::isAllowedHost(WKStringRef host)
{
    if (m_allowedHosts.isEmpty())
        return false;
    return m_allowedHosts.contains(toWTFString(host));
}

void InjectedBundle::setAllowsAnySSLCertificate(bool allowsAnySSLCertificate)
{
    WebCoreTestSupport::setAllowsAnySSLCertificate(allowsAnySSLCertificate);
}

void InjectedBundle::statisticsNotifyObserver()
{
    WKBundleResourceLoadStatisticsNotifyObserver(m_bundle);
}

void InjectedBundle::textDidChangeInTextField()
{
    m_testRunner->textDidChangeInTextFieldCallback();
}

void InjectedBundle::textFieldDidBeginEditing()
{
    m_testRunner->textFieldDidBeginEditingCallback();
}

void InjectedBundle::textFieldDidEndEditing()
{
    m_testRunner->textFieldDidEndEditingCallback();
}

} // namespace WTR
