/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "DictionaryFunctions.h"
#include "InjectedBundlePage.h"
#include "WebCoreTestSupport.h"
#include <JavaScriptCore/Options.h>
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WebKit2_C.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

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
        nullptr,
        didReceiveMessage,
        didReceiveMessageToPage
    };
    WKBundleSetClient(m_bundle.get(), &client.base);
    WKBundleSetServiceWorkerProxyCreationCallback(m_bundle.get(), WebCoreTestSupport::setupNewlyCreatedServiceWorker);
    platformInitialize(initializationUserData);
    WebCoreTestSupport::populateJITOperations();

    activateFonts();
}

void InjectedBundle::didCreatePage(WKBundlePageRef page)
{
    bool isMainPage = m_pages.isEmpty();
    m_pages.append(makeUnique<InjectedBundlePage>(page));

    setUpInjectedBundleClients(page);

    if (!isMainPage)
        return;

    WKTypeRef result = nullptr;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundlePostSynchronousMessage(m_bundle.get(), toWK("Initialization").get(), nullptr, &result);
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto initializationDictionary = adoptWK(dictionaryValue(result));

    if (booleanValue(initializationDictionary.get(), "ResumeTesting"))
        beginTesting(initializationDictionary.get(), BegingTestingMode::Resume);
}

void InjectedBundle::willDestroyPage(WKBundlePageRef page)
{
    m_pages.removeFirstMatching([page](auto& current) {
        return current->page() == page;
    });
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
    // page instead of just assuming it's the first non-suspended one.
    for (auto& page : m_pages) {
        if (!WKBundlePageIsSuspended(page->page()))
            return page.get();
    }
    return m_pages[0].get();
}

WKBundlePageRef InjectedBundle::pageRef() const
{
    auto page = this->page();
    return page ? page->page() : nullptr;
}

void InjectedBundle::didReceiveMessage(WKStringRef, WKTypeRef)
{
    WKBundlePostMessage(m_bundle.get(), toWK("Error").get(), toWK("Unknown").get());
}

static void postGCTask(void* context)
{
    auto page = reinterpret_cast<WKBundlePageRef>(context);
    InjectedBundle::singleton().reportLiveDocuments(page);
    WKRelease(page);
}

void InjectedBundle::reportLiveDocuments(WKBundlePageRef page)
{
    // Release memory again, after the GC and timer fire. This is necessary to clear entries from CachedResourceLoader's m_documentResources in some scenarios.
    WKBundleReleaseMemory(m_bundle.get());

    const bool excludeDocumentsInPageGroup = true;
    WKBundlePagePostMessage(page, toWK("LiveDocuments").get(), adoptWK(WKBundleGetLiveDocumentURLsForTesting(m_bundle.get(), excludeDocumentsInPageGroup)).get());
}

void InjectedBundle::didReceiveMessageToPage(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "BeginTest")) {
        ASSERT(messageBody);
        auto messageBodyDictionary = dictionaryValue(messageBody);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        m_accessibilityIsolatedTreeMode = booleanValue(messageBodyDictionary, "IsAccessibilityIsolatedTreeEnabled");
#endif
        WKBundlePagePostMessage(page, toWK("Ack").get(), toWK("BeginTest").get());
        beginTesting(messageBodyDictionary, BegingTestingMode::New);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Reset")) {
        ASSERT(messageBody);
        auto messageBodyDictionary = dictionaryValue(messageBody);

        bool beforeTest = false;
        if (auto options = stringValue(messageBodyDictionary, "ResetStage"))
            beforeTest = toWTFString(options) == "BeforeTest"_s;

        if (auto options = stringValue(messageBodyDictionary, "JSCOptions"))
            JSC::Options::setOptions(toWTFString(options).utf8().data());

        if (booleanValue(messageBodyDictionary, "ShouldGC"))
            WKBundleGarbageCollectJavaScriptObjects(m_bundle.get());

        if (!beforeTest) {
            setAllowedHosts(messageBodyDictionary);

            m_dumpPixels = false;
            m_pixelResultIsPending = false;

            setlocale(LC_ALL, "");
            if (m_testRunner)
                m_testRunner->removeAllWebNotificationPermissions();
            m_testRunner = nullptr;

            InjectedBundle::page()->resetAfterTest();
        }
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetLiveDocuments")) {
        const bool excludeDocumentsInPageGroup = false;
        postPageMessage("LiveDocuments", adoptWK(WKBundleGetLiveDocumentURLsForTesting(m_bundle.get(), excludeDocumentsInPageGroup)));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CheckForWorldLeaks")) {
        WKBundleReleaseMemory(m_bundle.get());

        WKRetain(page); // Balanced by the release in postGCTask.
        WKBundlePageCallAfterTasksAndTimers(page, postGCTask, (void*)page);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidBeginSwipeCallback")) {
        if (m_testRunner)
            m_testRunner->callDidBeginSwipeCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallWillEndSwipeCallback")) {
        if (m_testRunner)
            m_testRunner->callWillEndSwipeCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidEndSwipeCallback")) {
        if (m_testRunner)
            m_testRunner->callDidEndSwipeCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallDidRemoveSwipeSnapshotCallback")) {
        if (m_testRunner)
            m_testRunner->callDidRemoveSwipeSnapshotCallback();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "NotifyDownloadDone")) {
        if (m_testRunner && m_testRunner->shouldFinishAfterDownload())
            m_testRunner->notifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "NotifyDone")) {
        if (m_testRunner && InjectedBundle::page())
            InjectedBundle::page()->dump(m_testRunner->shouldForceRepaint());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CallUISideScriptCallback")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto callbackID = uint64Value(messageBodyDictionary, "CallbackID");
        auto resultString = stringValue(messageBodyDictionary, "Result");
        if (m_testRunner)
            m_testRunner->runUIScriptCallback(callbackID, toJS(resultString).get());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "WorkQueueProcessedCallback")) {
        if (!topLoadingFrame() && m_testRunner && !m_testRunner->shouldWaitUntilDone())
            InjectedBundle::page()->dump(m_testRunner->shouldForceRepaint());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ForceImmediateCompletion")) {
        if (m_testRunner && InjectedBundle::page())
            InjectedBundle::page()->dump(m_testRunner->shouldForceRepaint());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "WheelEventMarker")) {
        ASSERT(messageBody);
        ASSERT(WKGetTypeID(messageBody) == WKStringGetTypeID());

        auto bodyString = toWTFString(static_cast<WKStringRef>(messageBody));
        // These match the strings in EventSenderProxy::sendWheelEvent().
        if (bodyString == "SentWheelPhaseEndOrCancel"_s)
            m_eventSendingController->sentWheelPhaseEndOrCancel();
        else if (bodyString == "SentWheelMomentumPhaseEnd"_s)
            m_eventSendingController->sentWheelMomentumPhaseEnd();
        return;
    }

    postPageMessage("Error", "Unknown");
}

void InjectedBundle::setAllowedHosts(WKDictionaryRef settings)
{
    auto allowedHostsValue = value(settings, "AllowedHosts");
    if (allowedHostsValue && WKGetTypeID(allowedHostsValue) == WKArrayGetTypeID()) {
        m_allowedHosts.clear();
        auto array = static_cast<WKArrayRef>(allowedHostsValue);
        for (size_t i = 0, size = WKArrayGetSize(array); i < size; ++i)
            m_allowedHosts.append(toWTFString(WKArrayGetItemAtIndex(array, i)));
    }
}

void InjectedBundle::beginTesting(WKDictionaryRef settings, BegingTestingMode testingMode)
{
    m_dumpPixels = booleanValue(settings, "DumpPixels");
    m_timeout = Seconds::fromMilliseconds(uint64Value(settings, "Timeout"));
    m_dumpJSConsoleLogInStdErr = booleanValue(settings, "DumpJSConsoleLogInStdErr");

    m_pixelResult.clear();
    m_repaintRects.clear();

    setAllowedHosts(settings);

    m_testRunner = TestRunner::create();
    m_gcController = GCController::create();
    m_eventSendingController = EventSendingController::create();
    m_textInputController = TextInputController::create();
    m_accessibilityController = AccessibilityController::create();
    m_accessibilityController->setForceDeferredSpellChecking(false);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    m_accessibilityController->setIsolatedTreeMode(m_accessibilityIsolatedTreeMode);
#endif
#if ENABLE(VIDEO)
    if (!m_captionUserPreferencesTestingModeToken)
        m_captionUserPreferencesTestingModeToken = WKBundlePageCreateCaptionUserPreferencesTestingModeToken(page()->page());
#endif

#if PLATFORM(IOS_FAMILY)
    WKBundlePageSetUseTestingViewportConfiguration(page()->page(), !booleanValue(settings, "UseFlexibleViewport"));
#endif

#if PLATFORM(COCOA)
    WebCoreTestSupport::setAdditionalSupportedImageTypesForTesting(toWTFString(stringValue(settings, "additionalSupportedImageTypes")));
#endif

    m_testRunner->setUserStyleSheetEnabled(false);

    m_testRunner->setAcceptsEditing(true);
    m_testRunner->setTabKeyCyclesThroughElements(true);
    m_testRunner->clearTestRunnerCallbacks();

    if (m_timeout > 0_s)
        m_testRunner->setCustomTimeout(m_timeout);

    if (testingMode != BegingTestingMode::New)
        return;

    WKBundleResetOriginAccessAllowLists(m_bundle.get());
    clearResourceLoadStatistics();

#if ENABLE(VIDEO)
    WKBundlePageSetCaptionDisplayMode(page()->page(), stringValue(settings, "CaptionDisplayMode"));
#endif
    // [WK2] REGRESSION(r128623): It made layout tests extremely slow
    // https://bugs.webkit.org/show_bug.cgi?id=96862
    // WKBundleSetDatabaseQuota(m_bundle.get(), 5 * 1024 * 1024);
}

void InjectedBundle::done(bool forceRepaint)
{
    m_useWorkQueue = false;

    setTopLoadingFrame(0);

    m_accessibilityController->resetToConsistentState();

    auto body = adoptWK(WKMutableDictionaryCreate());

    setValue(body, "PixelResultIsPending", m_pixelResultIsPending);
    if (!m_pixelResultIsPending)
        setValue(body, "PixelResult", m_pixelResult);
    setValue(body, "RepaintRects", m_repaintRects);
    setValue(body, "AudioResult", m_audioResult);
    setValue(body, "ForceRepaint", forceRepaint);

    WKBundlePagePostMessageIgnoringFullySynchronousMode(page()->page(), toWK("Done").get(), body.get());
    m_testRunner = nullptr;
}

void InjectedBundle::clearResourceLoadStatistics()
{
    WKBundleClearResourceLoadStatistics(m_bundle.get());
}

void InjectedBundle::reloadFromOrigin()
{
    m_useWorkQueue = true;
    postPageMessage("ReloadFromOrigin");
}

void InjectedBundle::dumpBackForwardListsForAllPages(StringBuilder& stringBuilder)
{
    size_t size = m_pages.size();
    for (size_t i = 0; i < size; ++i)
        stringBuilder.append(m_pages[i]->dumpHistory());
}

void InjectedBundle::dumpToStdErr(const String& output)
{
    if (!isTestRunning())
        return;
    if (output.isEmpty())
        return;
    // FIXME: Do we really have to convert to UTF-8 instead of using toWK?
    auto string = output.tryGetUTF8();
    postPageMessage("DumpToStdErr", string ? string->data() : "Out of memory\n");
}

void InjectedBundle::outputText(StringView output, IsFinalTestOutput isFinalTestOutput)
{
    if (!isTestRunning())
        return;
    if (output.isEmpty())
        return;
    // FIXME: Do we really have to convert to UTF-8 instead of using toWK?
    auto string = output.tryGetUTF8();
    // We use WKBundlePagePostMessageIgnoringFullySynchronousMode() instead of WKBundlePagePostMessage() to make sure that all text output
    // is done via asynchronous IPC, even if the connection is in fully synchronous mode due to a WKBundlePagePostSynchronousMessageForTesting()
    // call. Otherwise, messages logged via sync and async IPC may end up out of order and cause flakiness.
    auto messageName = isFinalTestOutput == IsFinalTestOutput::Yes ? toWK("FinalTextOutput") : toWK("TextOutput");
    WKBundlePagePostMessageIgnoringFullySynchronousMode(page()->page(), messageName.get(), toWK(string ? string->data() : "Out of memory\n").get());
}

void InjectedBundle::postNewBeforeUnloadReturnValue(bool value)
{
    postPageMessage("BeforeUnloadReturnValue", value);
}

void InjectedBundle::postSetWindowIsKey(bool isKey)
{
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("SetWindowIsKey").get(), adoptWK(WKBooleanCreate(isKey)).get(), 0);
}

void InjectedBundle::postSetViewSize(double width, double height)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "width", width);
    setValue(body, "height", height);
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("SetViewSize").get(), body.get(), 0);
}

void InjectedBundle::postSimulateWebNotificationClick(WKDataRef notificationID)
{
    postPageMessage("SimulateWebNotificationClick", notificationID);
}

void InjectedBundle::postSimulateWebNotificationClickForServiceWorkerNotifications()
{
    postPageMessage("SimulateWebNotificationClickForServiceWorkerNotifications");
}

void InjectedBundle::postSetAddsVisitedLinks(bool addsVisitedLinks)
{
    postPageMessage("SetAddsVisitedLinks", adoptWK(WKBooleanCreate(addsVisitedLinks)));
}

void InjectedBundle::setGeolocationPermission(bool enabled)
{
    postPageMessage("SetGeolocationPermission", adoptWK(WKBooleanCreate(enabled)));
}

void InjectedBundle::setScreenWakeLockPermission(bool enabled)
{
    postPageMessage("SetScreenWakeLockPermission", adoptWK(WKBooleanCreate(enabled)));
}

void InjectedBundle::setMockGeolocationPosition(double latitude, double longitude, double accuracy, std::optional<double> altitude, std::optional<double> altitudeAccuracy, std::optional<double> heading, std::optional<double> speed, std::optional<double> floorLevel)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "latitude", latitude);
    setValue(body, "longitude", longitude);
    setValue(body, "accuracy", accuracy);
    setValue(body, "altitude", altitude);
    setValue(body, "altitudeAccuracy", altitudeAccuracy);
    setValue(body, "heading", heading);
    setValue(body, "speed", speed);
    setValue(body, "floorLevel", floorLevel);
    postPageMessage("SetMockGeolocationPosition", body);
}

void InjectedBundle::setMockGeolocationPositionUnavailableError(WKStringRef errorMessage)
{
    postPageMessage("SetMockGeolocationPositionUnavailableError", errorMessage);
}

bool InjectedBundle::isGeolocationProviderActive() const
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("IsGeolocationClientActive").get(), 0, &result);
    return booleanValue(adoptWK(result).get());
}

WKRetainPtr<WKStringRef> InjectedBundle::getBackgroundFetchIdentifier()
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("GetBackgroundFetchIdentifier").get(), 0, &result);
    return static_cast<WKStringRef>(result);
}

unsigned InjectedBundle::imageCountInGeneralPasteboard() const
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("ImageCountInGeneralPasteboard").get(), 0, &result);
    return uint64Value(adoptWK(result).get());
}

void InjectedBundle::setCameraPermission(bool enabled)
{
    postPageMessage("SetCameraPermission", adoptWK(WKBooleanCreate(enabled)));
}

void InjectedBundle::setMicrophonePermission(bool enabled)
{
    postPageMessage("SetMicrophonePermission", adoptWK(WKBooleanCreate(enabled)));
}

void InjectedBundle::resetUserMediaPermission()
{
    postPageMessage("ResetUserMediaPermission");
}

void InjectedBundle::setUserMediaPersistentPermissionForOrigin(bool permission, WKStringRef origin, WKStringRef parentOrigin)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "permission", permission);
    setValue(body, "origin", origin);
    setValue(body, "parentOrigin", parentOrigin);
    postPageMessage("SetUserMediaPersistentPermissionForOrigin", body);
}

unsigned InjectedBundle::userMediaPermissionRequestCountForOrigin(WKStringRef origin, WKStringRef parentOrigin) const
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "origin", origin);
    setValue(body, "parentOrigin", parentOrigin);
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("UserMediaPermissionRequestCountForOrigin").get(), body.get(), &result);
    return uint64Value(adoptWK(result).get());
}

void InjectedBundle::resetUserMediaPermissionRequestCountForOrigin(WKStringRef origin, WKStringRef parentOrigin)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "origin", origin);
    setValue(body, "parentOrigin", parentOrigin);
    postPageMessage("ResetUserMediaPermissionRequestCountForOrigin", body);
}

void InjectedBundle::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "enabled", enabled);
    setValue(body, "permissive", permissive);
    postPageMessage("SetCustomPolicyDelegate", body);
}

void InjectedBundle::setHidden(bool hidden)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "hidden", hidden);
    postPageMessage("SetHidden", body);
}

void InjectedBundle::setCacheModel(int model)
{
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("SetCacheModel").get(), adoptWK(WKUInt64Create(model)).get(), nullptr);
}

bool InjectedBundle::shouldProcessWorkQueue() const
{
    if (!m_useWorkQueue)
        return false;

    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("IsWorkQueueEmpty").get(), 0, &result);

    // The IPC failed. This happens when swapping processes on navigation because the WebPageProxy unregisters itself
    // as a MessageReceiver from the old WebProcessProxy and register itself with the new WebProcessProxy instead.
    if (!result)
        return false;

    auto isEmpty = booleanValue(adoptWK(result).get());
    return !isEmpty;
}

void InjectedBundle::processWorkQueue()
{
    postPageMessage("ProcessWorkQueue");
}

void InjectedBundle::queueBackNavigation(unsigned howFarBackward)
{
    m_useWorkQueue = true;
    postPageMessage("QueueBackNavigation", adoptWK(WKUInt64Create(howFarBackward)));
}

void InjectedBundle::queueForwardNavigation(unsigned howFarForward)
{
    m_useWorkQueue = true;
    postPageMessage("QueueForwardNavigation", adoptWK(WKUInt64Create(howFarForward)));
}

void InjectedBundle::queueLoad(WKStringRef url, WKStringRef target, bool shouldOpenExternalURLs)
{
    m_useWorkQueue = true;
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "url", url);
    setValue(body, "target", target);
    setValue(body, "shouldOpenExternalURLs", shouldOpenExternalURLs);
    postPageMessage("QueueLoad", body);
}

void InjectedBundle::queueLoadHTMLString(WKStringRef content, WKStringRef baseURL, WKStringRef unreachableURL)
{
    m_useWorkQueue = true;
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "content", content);
    if (baseURL)
        setValue(body, "baseURL", baseURL);
    if (unreachableURL)
        setValue(body, "unreachableURL", unreachableURL);
    postPageMessage("QueueLoadHTMLString", body);
}

void InjectedBundle::queueReload()
{
    m_useWorkQueue = true;
    postPageMessage("QueueReload");
}

void InjectedBundle::queueLoadingScript(WKStringRef script)
{
    m_useWorkQueue = true;
    postPageMessage("QueueLoadingScript", script);
}

void InjectedBundle::queueNonLoadingScript(WKStringRef script)
{
    m_useWorkQueue = true;
    postPageMessage("QueueNonLoadingScript", script);
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

void InjectedBundle::statisticsNotifyObserver(CompletionHandler<void()>&& completionHandler)
{
    return WKBundleResourceLoadStatisticsNotifyObserver(m_bundle.get(), completionHandler.leak(), [] (void* context) {
        WTF::adopt(static_cast<CompletionHandler<void()>::Impl*>(context))();
    });
}

WKRetainPtr<WKStringRef> InjectedBundle::lastAddedBackgroundFetchIdentifier() const
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("LastAddedBackgroundFetchIdentifier").get(), 0, &result);
    return static_cast<WKStringRef>(result);
}

WKRetainPtr<WKStringRef> InjectedBundle::lastRemovedBackgroundFetchIdentifier() const
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("LastRemovedBackgroundFetchIdentifier").get(), 0, &result);
    return static_cast<WKStringRef>(result);
}

WKRetainPtr<WKStringRef> InjectedBundle::lastUpdatedBackgroundFetchIdentifier() const
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("LastUpdatedBackgroundFetchIdentifier").get(), 0, &result);
    return static_cast<WKStringRef>(result);
}

WKRetainPtr<WKStringRef> InjectedBundle::backgroundFetchState(WKStringRef identifier)
{
    WKTypeRef result = nullptr;
    WKBundlePagePostSynchronousMessageForTesting(page()->page(), toWK("BackgroundFetchState").get(), identifier, &result);
    return static_cast<WKStringRef>(result);
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

void postMessage(const char* name)
{
    postMessage(name, WKRetainPtr<WKTypeRef> { });
}

void postMessage(const char* name, bool value)
{
    postMessage(name, adoptWK(WKBooleanCreate(value)));
}

void postMessage(const char* name, unsigned value)
{
    postMessage(name, adoptWK(WKUInt64Create(value)));
}

void postMessage(const char* name, JSStringRef value)
{
    postMessage(name, toWK(value));
}

void postSynchronousMessage(const char* name)
{
    postSynchronousMessage(name, WKRetainPtr<WKTypeRef> { });
}

void postSynchronousMessage(const char* name, bool value)
{
    postSynchronousMessage(name, adoptWK(WKBooleanCreate(value)));
}

void postSynchronousMessage(const char* name, uint64_t value)
{
    postSynchronousMessage(name, adoptWK(WKUInt64Create(value)));
}

void postSynchronousMessage(const char* name, unsigned value)
{
    uint64_t promotedValue = value;
    postSynchronousMessage(name, promotedValue);
}

void postSynchronousMessage(const char* name, double value)
{
    postSynchronousMessage(name, adoptWK(WKDoubleCreate(value)));
}

void postPageMessage(const char* name)
{
    postPageMessage(name, WKRetainPtr<WKTypeRef> { });
}

void postPageMessage(const char* name, bool value)
{
    postPageMessage(name, adoptWK(WKBooleanCreate(value)));
}

void postPageMessage(const char* name, const char* value)
{
    postPageMessage(name, toWK(value));
}

void postPageMessage(const char* name, WKStringRef value)
{
    if (auto page = InjectedBundle::singleton().pageRef())
        WKBundlePagePostMessage(page, toWK(name).get(), value);
}

void postPageMessage(const char* name, WKDataRef value)
{
    if (auto page = InjectedBundle::singleton().pageRef())
        WKBundlePagePostMessage(page, toWK(name).get(), value);
}

void postSynchronousPageMessage(const char* name)
{
    postSynchronousPageMessage(name, WKRetainPtr<WKTypeRef> { });
}

void postSynchronousPageMessage(const char* name, bool value)
{
    postSynchronousPageMessage(name, adoptWK(WKBooleanCreate(value)));
}

static JSValueRef stringArrayToJS(JSContextRef context, WKArrayRef strings)
{
    ASSERT(WKGetTypeID(strings) == WKArrayGetTypeID());
    const size_t count = WKArrayGetSize(strings);
    auto array = JSObjectMakeArray(context, 0, 0, nullptr);
    for (size_t i = 0; i < count; ++i) {
        auto stringRef = static_cast<WKStringRef>(WKArrayGetItemAtIndex(strings, i));
        ASSERT(WKGetTypeID(stringRef) == WKStringGetTypeID());
        JSObjectSetPropertyAtIndex(context, array, i, JSValueMakeString(context, toJS(stringRef).get()), nullptr);
    }
    return array;
}

void postMessageWithAsyncReply(JSContextRef context, const char* messageName, WKRetainPtr<WKTypeRef> parameter, JSValueRef callback)
{
    auto globalContext = JSContextGetGlobalContext(context);
    JSValueProtect(globalContext, callback);

    Function<void(WKTypeRef)> completionHandler = [callback, globalContext = JSRetainPtr { globalContext }] (WKTypeRef result) mutable {
        JSContextRef context = globalContext.get();

        size_t argumentCount { 0 };
        JSValueRef* arguments { nullptr };
        JSValueRef resultJS { nullptr };

        if (result) {
            if (WKGetTypeID(result) == WKArrayGetTypeID())
                resultJS = stringArrayToJS(context, static_cast<WKArrayRef>(result));
            else if (WKGetTypeID(result) == WKStringGetTypeID())
                resultJS = JSValueMakeString(context, toJS(static_cast<WKStringRef>(result)).get());
            else
                RELEASE_ASSERT_NOT_REACHED();
            arguments = &resultJS;
            argumentCount = 1;
        }

        JSObjectCallAsFunction(context, JSValueToObject(context, callback, nullptr), JSContextGetGlobalObject(context), argumentCount, arguments, nullptr);
        JSValueUnprotect(context, callback);
    };

    if (auto page = InjectedBundle::singleton().pageRef()) {
        WKBundlePagePostMessageWithAsyncReply(page, toWK(messageName).get(), parameter.get(), [] (WKTypeRef result, void* context) {
            auto function = WTF::adopt(static_cast<Function<void(WKTypeRef)>::Impl*>(context));
            function(result);
        }, completionHandler.leak());
    } else
        completionHandler(nullptr);
}

void postMessageWithAsyncReply(JSContextRef context, const char* messageName, JSValueRef callback)
{
    postMessageWithAsyncReply(context, messageName, nullptr, callback);
}

} // namespace WTR
