/*
 * Copyright (C) 2010-2026 Apple Inc. All rights reserved.
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
#include "WebPageProxy.h"

#include "APIContextMenuClient.h"
#include "APIDiagnosticLoggingClient.h"
#include "APIFindClient.h"
#include "APIFindMatchesClient.h"
#include "APIFormClient.h"
#include "APIFullscreenClient.h"
#include "APIIconLoadingClient.h"
#include "APILoaderClient.h"
#include "APINavigationAction.h"
#include "APINavigationClient.h"
#include "APIPolicyClient.h"
#include "APIUIClient.h"
#include "AboutSchemeHandler.h"
#include "BrowsingWarning.h"
#include "DidFilterKnownLinkDecoration.h"
#include "FrameProcess.h"
#include "MessageSenderInlines.h"
#include "RemotePageProxy.h"
#include "SuspendedPageProxy.h"
#include "UserMediaProcessManager.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardCacheEntry.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListFrameItem.h"
#include "WebContextMenuProxy.h"
#include "WebErrors.h"
#include "WebFullScreenManagerProxy.h"
#include "WebKit-Swift.h"
#include "WebNavigationState.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageDebuggable.h"
#include "WebPageGroup.h"
#include "WebPageInspectorController.h"
#include "WebPageLoadTiming.h"
#include "WebPageMessages.h"
#include "WebPageProxyInlines.h"
#include "WebPageProxyInternals.h"
#include "WebPageProxyTesting.h"
#include "WebProcessMessages.h"
#include "WebScreenOrientationManagerProxy.h"
#include <WebCore/DocumentSyncData.h>
#include <WebCore/Quirks.h>
#include <WebCore/SleepDisabler.h>
#include <wtf/CallbackAggregator.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "APIApplicationManifest.h"
#endif

#if ENABLE(BACK_FORWARD_LIST_SWIFT)
#include "WebBackForwardListSwiftUtilities.h"
#endif

#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "WebPrivacyHelpers.h"
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "DragInitiationResult.h"
#endif

#if PLATFORM(MAC)
#include "DisplayLink.h"
#include <WebCore/ImageUtilities.h>
#include <WebCore/UTIUtilities.h>
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
#include "ViewSnapshotStore.h"
#endif

#if PLATFORM(GTK)
#include <WebCore/SelectionData.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#include "MediaPlaybackTargetContextSerialized.h"
#include <WebCore/WebMediaSessionManager.h>
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManagerProxy.h"
#endif

#if ENABLE(WEB_AUTHN)
#include "WebAuthenticatorCoordinatorProxy.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/JSRemoteInspector.h>
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if HAVE(SEC_KEY_PROXY)
#include "SecKeyProxyStore.h"
#endif

#if HAVE(APP_SSO)
#include "SOAuthorizationCoordinator.h"
#endif

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
#include "WebDeviceOrientationUpdateProviderProxy.h"
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
#include "RemoteMediaSessionManagerProxy.h"
#endif

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResult.h"
#endif

#if ENABLE(MEDIA_USAGE)
#include "MediaUsageManager.h"
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#include "MediaSessionCoordinatorProxyPrivate.h"
#include "RemoteMediaSessionCoordinatorProxy.h"
#endif

#if HAVE(GROUP_ACTIVITIES)
#include "GroupActivitiesSessionNotifier.h"
#endif

#if ENABLE(APP_HIGHLIGHTS)
#include <WebCore/HighlightVisibility.h>
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#import "DisplayCaptureSessionManager.h"
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
#import <WebCore/ScreenCaptureKitSharingSessionManager.h>
#endif

#if USE(QUICK_LOOK)
#include <WebCore/PreviewConverter.h>
#endif

#if USE(SYSTEM_PREVIEW)
#include "SystemPreviewController.h"
#endif

#if USE(COORDINATED_GRAPHICS)
#include "DrawingAreaProxyCoordinatedGraphics.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
#include "WebExtensionController.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/Device.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
#include "ModelPresentationManagerProxy.h"
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
#include "RemoteAudioSessionConfiguration.h"
#endif

#if HAVE(ENHANCED_SECURITY_LINKS)
#include "EnhancedSecurityLinkUtilities.h"
#endif

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())
#define MESSAGE_CHECK_URL(process, url) MESSAGE_CHECK_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), process->connection())
#define MESSAGE_CHECK_URL_COROUTINE(process, url) MESSAGE_CHECK_BASE_COROUTINE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), process->connection())
#define MESSAGE_CHECK_COMPLETION(process, assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, process->connection(), completion)
#define MESSAGE_CHECK_URL_COMPLETION(process, url, completion) MESSAGE_CHECK_COMPLETION_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), process->connection(), completion)

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, this, identifier().toUInt64(), m_webPageID.toUInt64(), m_legacyMainFrameProcess->processID(), ##__VA_ARGS__)
#define WEBPAGEPROXY_RELEASE_LOG_WITH_THIS(channel, thisPtr, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, WTF::getPtr(thisPtr), thisPtr->identifier().toUInt64(), thisPtr->m_webPageID.toUInt64(), thisPtr->m_legacyMainFrameProcess->processID(), ##__VA_ARGS__)

#define WEBPAGEPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, this, identifier().toUInt64(), m_webPageID.toUInt64(), m_legacyMainFrameProcess->processID(), ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

// The definitions below were split out of WebPageProxy.cpp: they depend on the
// Swift C++ interop header (WebKit-Swift.h) via the BACK_FORWARD_LIST_SWIFT type
// and the SWIFT_DEMO_URI_SCHEME logo. Keeping them in this separate translation
// unit — compiled in the WebKit_SwiftInterop subtarget — lets the rest of
// WebPageProxy.cpp compile before the generated Swift header is available.

// Duplicated from WebPageProxy.cpp: small pure predicate shared by both TUs.
// Only the APP_BOUND_DOMAINS path in decidePolicyForNavigationAction uses it here.
#if ENABLE(APP_BOUND_DOMAINS)
static bool shouldTreatURLProtocolAsAppBound(const URL& requestURL, bool isRunningTest)
{
    return !isRunningTest
        && (SecurityOrigin::isLocalHostOrLoopbackIPAddress(requestURL.host())
            || requestURL.protocolIsAbout()
            || requestURL.protocolIsData()
            || requestURL.protocolIsBlob()
            || requestURL.protocolIsFile()
            || requestURL.protocolIsJavaScript());
}
#endif


bool WebPageProxy::suspendCurrentPageIfPossible(API::Navigation& navigation, RefPtr<WebFrameProxy>&& mainFrame, ShouldDelayClosingUntilFirstLayerFlush shouldDelayClosingUntilFirstLayerFlush)
{
    m_suspendedPageKeptToPreventFlashing = nullptr;
    m_lastSuspendedPage = nullptr;

    if (!mainFrame)
        return false;

    if (!hasCommittedAnyProvisionalLoads()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because has not committed any load yet", m_legacyMainFrameProcess->processID());
        return false;
    }

    if (isPageOpenedByDOMShowingInitialEmptyDocument()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because it is showing the initial empty document", m_legacyMainFrameProcess->processID());
        return false;
    }

    if (protect(m_browsingContextGroup)->hasMultiplePages()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because BrowsingContextGroup has multiple pages", m_legacyMainFrameProcess->processID());
        return false;
    }

    RefPtr fromItem = navigation.fromItem();

    // If the source and the destination back / forward list items are the same, then this is a client-side redirect. In this case,
    // there is no need to suspend the previous page as there will be no way to get back to it.
    if (fromItem && fromItem == backForwardList().currentItem()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because this is a client-side redirect", m_legacyMainFrameProcess->processID());
        return false;
    }

    if (fromItem && fromItem->url() != pageLoadState().url()) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because fromItem's URL does not match the page URL.", m_legacyMainFrameProcess->processID());
        return false;
    }

    bool needsSuspendedPageToPreventFlashing = shouldDelayClosingUntilFirstLayerFlush == ShouldDelayClosingUntilFirstLayerFlush::Yes;
    if (!needsSuspendedPageToPreventFlashing && (!fromItem || !shouldUseBackForwardCache())) {
        if (!fromItem)
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i there is no associated WebBackForwardListItem", m_legacyMainFrameProcess->processID());
        else
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i the back / forward cache is disabled", m_legacyMainFrameProcess->processID());
        return false;
    }

    WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Suspending current page for process pid %i", m_legacyMainFrameProcess->processID());
    mainFrame->frameLoadState().didSuspend();

    Ref suspendedPage = SuspendedPageProxy::create(*this, protect(legacyMainFrameProcess()), mainFrame.releaseNonNull(), std::exchange(m_browsingContextGroup, BrowsingContextGroup::create()), shouldDelayClosingUntilFirstLayerFlush);
    std::optional<BackForwardFrameItemIdentifier> mainFrameItemID;
    Ref preferences = this->preferences();
    if (fromItem && preferences->siteIsolationEnabled() && preferences->multiProcessBackForwardCacheEnabled())
        mainFrameItemID = protect(fromItem)->mainFrameItem().identifier();
    suspendedPage->startSuspension(mainFrameItemID);
    // startSuspension() sends async IPCs to subframe processes. Failure is
    // handled by the CallbackAggregator which removes the BFCache entry,
    // destroying this SuspendedPageProxy and triggering teardown().

    LOG(ProcessSwapping, "WebPageProxy %" PRIu64 " created suspended page %s for process pid %i, back/forward item %s" PRIu64, identifier().toUInt64(), suspendedPage->loggingString().utf8().data(), m_legacyMainFrameProcess->processID(), fromItem ? fromItem->identifier().toString().utf8().data() : "0"_s);

    m_lastSuspendedPage = suspendedPage.get();

    if (fromItem && shouldUseBackForwardCache())
        protect(backForwardCache())->addEntry(*fromItem, WTF::move(suspendedPage));
    else {
        ASSERT(needsSuspendedPageToPreventFlashing);
        m_suspendedPageKeptToPreventFlashing = WTF::move(suspendedPage);
    }

    return true;
}

RefPtr<API::Navigation> WebPageProxy::launchProcessForReload()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload:");

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload: page is closed");
        return nullptr;
    }

    ASSERT(!hasRunningProcess());
    RefPtr currentItem = backForwardList().currentItem();
    auto site = currentItem ? Site { URL { currentItem->url() } } : Site(aboutBlankURL());
    launchProcess(site, ProcessLaunchReason::Crash);

    if (!currentItem) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload: no current item to reload");
        return nullptr;
    }

    Ref navigation = m_navigationState->createReloadNavigation(legacyMainFrameProcess().coreProcessIdentifier(), protect(backForwardList().currentItem()));

    String url = currentURL();
    if (!url.isEmpty()) {
        Ref protectedPageLoadState = pageLoadState();
        auto transaction = protectedPageLoadState->transaction();
        protectedPageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), URL { url } });
    }

    auto publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(URL(currentItem->url()));

    // We allow stale content when reloading a WebProcess that's been killed or crashed.
    send(Messages::WebPage::GoToBackForwardItem({ navigation->navigationID(), copyFrameStateForBackForwardNavigation(protect(currentItem->mainFrameItem())), FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, ShouldRestoreFromBackForwardCache::Unspecified, std::nullopt, publicSuffix, { }, WebCore::ProcessSwapDisposition::None }));

    Ref legacyMainFrameProcess = m_legacyMainFrameProcess;
    legacyMainFrameProcess->startResponsivenessTimer();

    if (shouldForceForegroundPriorityForClientNavigation())
        setClientNavigationActivity(navigation);

    return navigation;
}

void WebPageProxy::close()
{
    if (m_isClosed)
        return;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "close:");

    m_isClosed = true;

    // Make sure we do this before we clear the UIClient so that we can ask the UIClient
    // to release the wake locks.
    internals().sleepDisablers.clear();

    reportPageLoadResult(ResourceError { ResourceError::Type::Cancellation });

    if (RefPtr activePopupMenu = m_activePopupMenu)
        activePopupMenu->cancelTracking();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->willClosePage(*this);
    }

#if ENABLE(FULLSCREEN_API)
    if (RefPtr fullscreenManager = std::exchange(m_fullScreenManager, nullptr))
        fullscreenManager->detachFromClient();
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (m_immersive)
        dismissImmersiveElement([] { });
#endif

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = m_webExtensionController)
        webExtensionController->removePage(*this);
    if (RefPtr webExtensionController = m_weakWebExtensionController.get())
        webExtensionController->removePage(*this);
#endif

#if ENABLE(CONTEXT_MENUS)
    m_activeContextMenu = nullptr;
#endif

    m_provisionalPage = nullptr;

    m_pageForTesting = nullptr;

    // Do not call inspector() since it returns null after the page has closed.
    protect(m_inspector)->invalidate();

    backForwardList().pageClosed();
    m_inspectorController->pageClosed();
#if ENABLE(REMOTE_INSPECTOR)
    if (RefPtr inspectorDebuggable = std::exchange(m_inspectorDebuggable, nullptr))
        inspectorDebuggable->detachFromPage();
#endif

    if (RefPtr pageClient = this->pageClient())
        pageClient->pageClosed();

    disconnectFramesFromPage();

    m_loaderClient = nullptr;
    m_navigationClient = makeUniqueRef<API::NavigationClient>();
    m_policyClient = nullptr;
    m_iconLoadingClient = makeUnique<API::IconLoadingClient>();
    m_formClient = makeUnique<API::FormClient>();
    m_uiClient = makeUnique<API::UIClient>();
    m_findClient = makeUnique<API::FindClient>();
    m_findMatchesClient = makeUnique<API::FindMatchesClient>();
    m_diagnosticLoggingClient = nullptr;
#if ENABLE(CONTEXT_MENUS)
    m_contextMenuClient = makeUnique<API::ContextMenuClient>();
#endif
#if ENABLE(FULLSCREEN_API)
    m_fullscreenClient = makeUnique<API::FullscreenClient>();
#endif

    resetState(ResetStateReason::PageInvalidated);

    Ref process = m_legacyMainFrameProcess;
    Ref processPool = m_configuration->processPool();
    processPool->backForwardCache().removeEntriesForPage(*this);

    struct ProcessToClose {
        const Ref<WebProcessProxy> process;
        WebCore::PageIdentifier pageID;
        WebProcessProxy::ShutdownPreventingScopeCounter::Token shutdownPreventingScope;
    };
    Vector<ProcessToClose> processesToClose;
    forEachWebContentProcess([&](auto& process, auto pageID) {
        processesToClose.append({
            process,
            pageID,
            process.shutdownPreventingScope()
        });
    });
    // Delay sending close message to next runloop cycle to avoid white flash.
    RunLoop::currentSingleton().dispatch([processesToClose = WTF::move(processesToClose), pageProxyID = identifier()] mutable {
        for (auto& [process, pageID, scope] : processesToClose)
            protect(process)->sendPageCloseMessage(pageProxyID, pageID, [scope = WTF::move(scope)] { });
    });

    process->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();
    protect(processPool->supplement<WebNotificationManagerProxy>())->clearNotifications(this);

    // Null out related WebPageProxy to avoid leaks.
    m_configuration->setRelatedPage(nullptr);

    // Make sure we don't hold a process assertion after getting closed.
    resetActivityState();
    internals().audibleActivityTimer.stop();

    stopAllURLSchemeTasks();

#if ENABLE(GAMEPAD)
    m_internals->recentGamepadAccessHysteresis.cancel();
#endif

    if (protect(preferences())->siteIsolationEnabled())
        protect(browsingContextGroup())->removePage(*this);
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(WebCore::ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, NavigationUpgradeToHTTPSBehavior navigationUpgradeToHTTPSBehavior, std::unique_ptr<NavigationActionData>&& lastNavigationAction, API::Object* userData, bool isRequestFromClientOrUserInput)
{
    if (m_isClosed)
        return nullptr;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadRequest:");

    if (m_isCallingCreateNewPage && request.url().protocolIsJavaScript()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadRequest: Not loading javascript URL during createNewPage.");
        return nullptr;
    }

    if (!hasRunningProcess())
        launchProcess(Site { request.url() }, ProcessLaunchReason::InitialProcess);

    Ref navigation = m_navigationState->createLoadRequestNavigation(legacyMainFrameProcess().coreProcessIdentifier(), ResourceRequest(request), protect(backForwardList().currentItem()));

    if (lastNavigationAction)
        navigation->setLastNavigationAction(*lastNavigationAction);

    if (isRequestFromClientOrUserInput)
        navigation->markRequestAsFromClientInput();

    if (shouldForceForegroundPriorityForClientNavigation())
        setClientNavigationActivity(navigation);

#if PLATFORM(COCOA)
    setLastNavigationWasAppInitiated(request);
#endif

    loadRequestWithNavigationShared(protect(legacyMainFrameProcess()), m_webPageID, navigation, WTF::move(request), shouldOpenExternalURLsPolicy, navigationUpgradeToHTTPSBehavior, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain(), nullptr, std::nullopt);
    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadFile(const String& fileURLString, const String& resourceDirectoryURLString, bool isAppInitiated, API::Object* userData)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile:");

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile: page is closed");
        return nullptr;
    }

#if PLATFORM(MAC)
    if (isQuarantinedAndNotUserApproved(fileURLString)) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile: file cannot be opened because it is from an unidentified developer.");
        return nullptr;
    }
#endif

    if (!hasRunningProcess())
        launchProcess(Site(aboutBlankURL()), ProcessLaunchReason::InitialProcess);

    URL fileURL { fileURLString };
    if (!fileURL.protocolIsFile()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile: file is not local");
        return nullptr;
    }

    URL resourceDirectoryURL;
    if (resourceDirectoryURLString.isNull())
        resourceDirectoryURL = URL({ }, "file:///"_s);
    else {
        resourceDirectoryURL = URL { resourceDirectoryURLString };
        if (!resourceDirectoryURL.protocolIsFile()) {
            WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile: resource URL is not local");
            return nullptr;
        }
    }

    Ref navigation = m_navigationState->createLoadRequestNavigation(legacyMainFrameProcess().coreProcessIdentifier(), ResourceRequest(URL { fileURL }), protect(backForwardList().currentItem()));

    navigation->markRequestAsFromClientInput();

    if (shouldForceForegroundPriorityForClientNavigation())
        setClientNavigationActivity(navigation);


    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), URL { fileURLString } }, resourceDirectoryURL);

    auto request = ResourceRequest(URL { fileURL });
    request.setIsAppInitiated(isAppInitiated);
    m_lastNavigationWasAppInitiated = isAppInitiated;

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTF::move(request);
    loadParameters.shouldOpenExternalURLsPolicy = ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    loadParameters.userData = UserData(legacyMainFrameProcess().transformObjectsToHandles(userData).get());
    loadParameters.publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(loadParameters.request.url());
    loadParameters.isRequestFromClientOrUserInput = isAppInitiated;
    Ref process = m_legacyMainFrameProcess;
    maybeInitializeSandboxExtensionHandle(process, fileURL, resourceDirectoryURL, true, [weakThis = WeakPtr { *this }, weakProcess = WeakPtr { process }, loadParameters = WTF::move(loadParameters), resourceDirectoryURL] (std::optional<SandboxExtension::Handle>&& sandboxExtension) mutable {
        const bool checkAssumedReadAccessToResourceURL = false;
        RefPtr protectedProcess = weakProcess.get();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedProcess)
            return;
        if (sandboxExtension)
            loadParameters.sandboxExtensionHandle = WTF::move(*sandboxExtension);

        protectedThis->prepareToLoadWebPage(*protectedProcess, loadParameters);

        protectedProcess->markProcessAsRecentlyUsed();
        if (protectedProcess->isLaunching())
            protectedThis->send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(WTF::move(loadParameters), resourceDirectoryURL, protectedThis->identifier(), checkAssumedReadAccessToResourceURL));
        else
            protectedThis->send(Messages::WebPage::LoadRequest(WTF::move(loadParameters)));
        protectedProcess->startResponsivenessTimer();
    });

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadSimulatedRequest(WebCore::ResourceRequest&& simulatedRequest, WebCore::ResourceResponse&& simulatedResponse, Ref<WebCore::SharedBuffer>&& data)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadSimulatedRequest:");

#if PLATFORM(COCOA)
    setLastNavigationWasAppInitiated(simulatedRequest);
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    if (simulatedResponse.mimeType() == "text/html"_s && !isFullWebBrowserOrRunningTest())
        m_limitsNavigationsToAppBoundDomains = true;
#endif

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadSimulatedRequest: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess())
        launchProcess(Site { simulatedRequest.url() }, ProcessLaunchReason::InitialProcess);

    Ref navigation = m_navigationState->createSimulatedLoadWithDataNavigation(legacyMainFrameProcess().coreProcessIdentifier(), ResourceRequest(simulatedRequest), makeUnique<API::SubstituteData>(Vector(data->span()), ResourceResponse(simulatedResponse), WebCore::SubstituteData::SessionHistoryVisibility::Visible), protect(backForwardList().currentItem()));

    if (shouldForceForegroundPriorityForClientNavigation())
        setClientNavigationActivity(navigation);


    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    auto baseURL = simulatedRequest.url().string();
    simulatedResponse.setURL(URL { simulatedRequest.url() }); // These should always match for simulated load

    pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), !baseURL.isEmpty() ? URL { baseURL } : aboutBlankURL() });

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTF::move(simulatedRequest);
    loadParameters.data = WTF::move(data);
    loadParameters.MIMEType = simulatedResponse.mimeType();
    loadParameters.encodingName = simulatedResponse.textEncodingName();
    loadParameters.baseURLString = baseURL;
    loadParameters.shouldOpenExternalURLsPolicy = WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    loadParameters.shouldTreatAsContinuingLoad = ShouldTreatAsContinuingLoad::No;
    loadParameters.lockHistory = navigation->lockHistory();
    loadParameters.lockBackForwardList = navigation->lockBackForwardList();
    loadParameters.clientRedirectSourceForHistory = navigation->clientRedirectSourceForHistory();
    loadParameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain();
    loadParameters.isHandledByAboutSchemeHandler = m_aboutSchemeHandler->canHandleURL(loadParameters.request.url());

    simulatedResponse.setExpectedContentLength(loadParameters.data->size());
    simulatedResponse.includeCertificateInfo();

    Ref process = m_legacyMainFrameProcess;
    prepareToLoadWebPage(process, loadParameters);

    process->markProcessAsRecentlyUsed();
    process->assumeReadAccessToBaseURL(*this, baseURL, [weakProcess = WeakPtr { process }, loadParameters = WTF::move(loadParameters), simulatedResponse = WTF::move(simulatedResponse), webPageID = m_webPageID] () mutable {
        RefPtr protectedProcess = weakProcess.get();
        if (!protectedProcess)
            return;
        protectedProcess->send(Messages::WebPage::LoadSimulatedRequestAndResponse(WTF::move(loadParameters), simulatedResponse), webPageID);
        protectedProcess->startResponsivenessTimer();
    });

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::reload(OptionSet<WebCore::ReloadOption> options)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "reload:");

    // Make sure the Network & GPU processes are still responsive. This is so that reload() gets us out of the bad state if one of these
    // processes is hung.
    protect(protect(websiteDataStore())->networkProcess())->checkForResponsiveness();
#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = m_configuration->processPool().gpuProcess())
        gpuProcess->checkForResponsiveness();
#endif

    SandboxExtension::Handle sandboxExtensionHandle;

    if (!hasRunningProcess())
        return launchProcessForReload();

    Ref navigation = m_navigationState->createReloadNavigation(legacyMainFrameProcess().coreProcessIdentifier(), protect(backForwardList().currentItem()));

    String url = currentURL();
    if (!url.isEmpty()) {
        Ref pageLoadState = internals().pageLoadState;
        auto transaction = pageLoadState->transaction();
        pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), URL { url } });
    }

    // Store decision to reload without content blockers on the navigation so that we can later set the corresponding
    // WebsitePolicies flag in WebPageProxy::receivedNavigationActionPolicyDecision().
    if (options.contains(WebCore::ReloadOption::DisableContentBlockers))
        navigation->setUserContentExtensionsEnabled(false);

    Ref process = m_legacyMainFrameProcess;
    process->markProcessAsRecentlyUsed();
    if (!url.isEmpty()) {
        // We may not have an extension yet if back/forward list was reinstated after a WebProcess crash or a browser relaunch
        maybeInitializeSandboxExtensionHandle(protect(legacyMainFrameProcess()), URL { url }, currentResourceDirectoryURL(), true, [weakThis = WeakPtr { *this }, process = WTF::move(process), options = WTF::move(options), sandboxExtensionHandle = WTF::move(sandboxExtensionHandle), navigation](std::optional<SandboxExtension::Handle>&& sandboxExtension) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (sandboxExtension)
                sandboxExtensionHandle = WTF::move(*sandboxExtension);
            protectedThis->send(Messages::WebPage::Reload(navigation->navigationID(), options, WTF::move(sandboxExtensionHandle)));
            process->startResponsivenessTimer();

            if (protectedThis->shouldForceForegroundPriorityForClientNavigation())
                protectedThis->setClientNavigationActivity(navigation);


#if ENABLE(SPEECH_SYNTHESIS)
            protectedThis->resetSpeechSynthesizer();
#endif
        });
    }

    return navigation;
}

void WebPageProxy::recordAutomaticNavigationSnapshot()
{
    if (m_shouldSuppressNextAutomaticNavigationSnapshot)
        return;

    if (RefPtr item = backForwardList().currentItem())
        recordNavigationSnapshot(*item);
}

RefPtr<API::Navigation> WebPageProxy::goForward()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goForward:");
    RefPtr forwardItem = backForwardList().goForwardItemSkippingItemsWithoutUserGesture();
    if (!forwardItem)
        return nullptr;

    return goToBackForwardItem(protect(forwardItem->navigatedFrameItem()), FrameLoadType::Forward);
}

RefPtr<API::Navigation> WebPageProxy::goBack()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goBack:");
    RefPtr backItem = backForwardList().goBackItemSkippingItemsWithoutUserGesture();
    if (!backItem)
        return nullptr;

    Ref frameItem = backItem->mainFrameItem();
    if (RefPtr currentItem = backForwardList().currentItem()) {
        if (RefPtr childItem = currentItem->navigatedFrameID() ? frameItem->childItemForFrameID(*currentItem->navigatedFrameID()) : nullptr) {
            if (childItem.get() != frameItem.ptr())
                WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "goBack: redirecting from mainFrameItem to child frameItem for navigatedFrameID=%" PRIu64, currentItem->navigatedFrameID()->toUInt64());
            frameItem = childItem.releaseNonNull();
        }
    }

    return goToBackForwardItem(frameItem, FrameLoadType::Back);
}

RefPtr<API::Navigation> WebPageProxy::goToBackForwardItem(WebBackForwardListFrameItem& frameItem, FrameLoadType frameLoadType)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goToBackForwardItem:");

    RefPtr item = frameItem.backForwardListItem();
    ASSERT(item);
    if (!item)
        return nullptr;

    LOG(Loading, "WebPageProxy %p goToBackForwardItem to item URL %s", this, item->url().utf8().data());

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "goToBackForwardItem: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess()) {
        launchProcess(Site { URL { item->url() } }, ProcessLaunchReason::InitialProcess);

        if (item != backForwardList().currentItem())
#if ENABLE(BACK_FORWARD_LIST_SWIFT)
            backForwardList().goToItem(&*item);
#else
            backForwardList().goToItem(*item);
#endif
    }

    Ref process = processForTheFrameItem(frameItem);
    if (process.ptr() != m_legacyMainFrameProcess.ptr())
        WEBPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "goToBackForwardItem: processForTheFrameItem selected a different process pid=%d (main frame process pid=%d), frameID=%" PRIu64, process->processID(), m_legacyMainFrameProcess->processID(), frameItem.frameID() ? frameItem.frameID()->toUInt64() : 0);

    // Cross-site SuspendedPageProxy entries follow the unsuspend() path; skip them here.
    auto shouldRestoreFromBackForwardCache = ShouldRestoreFromBackForwardCache::Unspecified;
    Ref preferences = this->preferences();
    if (preferences->siteIsolationEnabled() && preferences->multiProcessBackForwardCacheEnabled()) {
        RefPtr entry = item->backForwardCacheEntry();
        shouldRestoreFromBackForwardCache = entry ? ShouldRestoreFromBackForwardCache::Yes : ShouldRestoreFromBackForwardCache::No;

        if (entry && !item->suspendedPage()) {
            auto [cachedChildren, iframeProcesses] = entry->takeForRestoration();
            auto itemID = item->identifier();

            protect(backForwardCache())->removeEntry(*item);

            if (!cachedChildren.isEmpty()) {
                auto mainFrameItemID = item->mainFrameItem().identifier();

                internals().pendingBackForwardCachedChildren.set(itemID, WTF::move(cachedChildren));

                auto aggregator = MainRunLoopSuccessCallbackAggregator::create(
                    [weakThis = WeakPtr { *this }, itemID](bool success) {
                        if (success)
                            return;
                        RefPtr page = weakThis.get();
                        if (!page)
                            return;
                        page->internals().pendingBackForwardCachedChildren.remove(itemID);
                        RELEASE_LOG_ERROR(ProcessSwapping, "WebPageProxy::goToBackForwardItem: iframe restore failed, reloading");
                        page->reload(ReloadOption::ExpiredOnly);
                    });

                for (Ref iframeProcess : iframeProcesses) {
                    if (!protect(browsingContextGroup())->remotePageInProcess(*this, iframeProcess)) {
                        RELEASE_LOG_ERROR(ProcessSwapping, "WebPageProxy::goToBackForwardItem: no RemotePageProxy for pid %i, signaling failure", iframeProcess->processID());
                        aggregator->failed();
                        continue;
                    }
                    RELEASE_LOG(ProcessSwapping, "WebPageProxy::goToBackForwardItem: dispatching RestoreWithFrameItem to pid %i", iframeProcess->processID());
                    iframeProcess->sendWithAsyncReply(Messages::WebPage::RestoreWithFrameItem(mainFrameItemID, std::nullopt), aggregator->chain(), webPageIDInProcess(iframeProcess));
                }
            }
        }
    }

    Ref navigation = m_navigationState->createBackForwardNavigation(process->coreProcessIdentifier(), frameItem, protect(backForwardList().currentItem()), frameLoadType);
    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();
    pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), URL { item->url() } });

    process->markProcessAsRecentlyUsed();

    auto publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(URL(item->url()));
    process->send(Messages::WebPage::GoToBackForwardItem({ navigation->navigationID(), copyFrameStateForBackForwardNavigation(frameItem), frameLoadType, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, shouldRestoreFromBackForwardCache, std::nullopt, WTF::move(publicSuffix), { }, WebCore::ProcessSwapDisposition::None }), webPageIDInProcess(process));
    process->startResponsivenessTimer();

    return RefPtr<API::Navigation> { WTF::move(navigation) };
}

void WebPageProxy::updateCanGoBackAndForward()
{
    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    pageLoadState->setCanGoBack(transaction, backForwardList().backItem());
    pageLoadState->setCanGoForward(transaction, backForwardList().forwardItem());
}

void WebPageProxy::shouldGoToBackForwardListItem(BackForwardItemIdentifier itemID, bool inBackForwardCache, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&& completionHandler)
{
    RefPtr protectedPageClient { pageClient() };

    if (RefPtr item = backForwardList().itemForID(itemID)) {
        auto innerHandler = [protectedPageClient = WTF::move(protectedPageClient), completionHandler = WTF::move(completionHandler)] (bool result) mutable {
            completionHandler(result ? WebCore::ShouldGoToHistoryItem::Yes : WebCore::ShouldGoToHistoryItem::No);
        };

        m_navigationClient->shouldGoToBackForwardListItem(*this, *item, inBackForwardCache, WTF::move(innerHandler));
        return;
    }

    completionHandler(WebCore::ShouldGoToHistoryItem::ItemUnknown);
}

void WebPageProxy::goToBackForwardItemAtIndex(int32_t steps, FrameLoadType frameLoadType)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goToBackForwardItemAtIndex: steps=%d", steps);

    RefPtr item = backForwardListWrapper().itemAtDeltaFromCurrentIndex(steps, AllowSkippingBackForwardItems::No);
    if (!item)
        return;

    Ref frameItem = item->mainFrameItem();
    if (RefPtr currentItem = backForwardList().currentItem()) {
        if (RefPtr childItem = currentItem->navigatedFrameID() ? frameItem->childItemForFrameID(*currentItem->navigatedFrameID()) : nullptr) {
            if (childItem.get() != frameItem.ptr())
                WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "goToBackForwardItemAtIndex: redirecting from mainFrameItem to child frameItem for navigatedFrameID=%" PRIu64, currentItem->navigatedFrameID()->toUInt64());
            frameItem = childItem.releaseNonNull();
        }
    }

    goToBackForwardItem(frameItem, frameLoadType);
}

void WebPageProxy::commitProvisionalPage(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, String&& proxyName, WebCore::ResourceResponseSource source, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, DocumentSecurityPolicy&& documentSecurityPolicy, HashSet<WebCore::SecurityOriginData>&& cspOriginsThatUpgradeInsecureNavigations, const UserData& userData, RestoredFromBackForwardCache restoredFromBackForwardCache)
{
    ASSERT(m_provisionalPage);
    RefPtr provisionalPage = std::exchange(m_provisionalPage, nullptr);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "commitProvisionalPage: newPID=%i", provisionalPage->process().processID());

    RefPtr mainFrameInPreviousProcess = m_mainFrame;
    Ref preferences = m_preferences;
    std::optional<WebCore::FrameIdentifier> oldMainFrameID;
    if (mainFrameInPreviousProcess && preferences->siteIsolationEnabled()) {
        oldMainFrameID = mainFrameInPreviousProcess->frameID();

        // Update the back/forward list so existing entries use the new main frame's FrameIdentifier.
        // This is needed for back/forward navigations that trigger a process swap, since no new
        // back/forward list item is added (unlike forward navigations where backForwardAddItemShared
        // handles the update).
        if (*oldMainFrameID != frameID)
            backForwardList().updateFrameIdentifier(*oldMainFrameID, frameID);
    }

    ASSERT(m_legacyMainFrameProcess.ptr() != &provisionalPage->process() || preferences->siteIsolationEnabled());

    auto shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::No;
#if ENABLE(TILED_CA_DRAWING_AREA)
    // On macOS, when not using UI-side compositing, we need to make sure we do not close the page in the previous process until we've
    // entered accelerated compositing for the new page or we will flash on navigation.
    if (protect(drawingArea())->type() == DrawingAreaType::TiledCoreAnimation)
        shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::Yes;
#endif

    if (m_isLayerTreeFrozenDueToSwipeAnimation)
        send(Messages::WebPage::SwipeAnimationDidEnd());

    resetStateAfterProcessTermination(ProcessTerminationReason::NavigationSwap);

    removeAllMessageReceivers();
    RefPtr navigation = m_navigationState->navigation(provisionalPage->navigationID());
    bool didSuspendPreviousPage = navigation ? suspendCurrentPageIfPossible(*navigation, WTF::move(mainFrameInPreviousProcess), shouldDelayClosingUntilFirstLayerFlush) : false;

    // Deferred from ProvisionalPageProxy::didCommitLoadForFrame(): if the
    // previous main-frame process still has local frames in this BCG,
    // transition the WebPageProxy in that process to a RemotePageProxy.
    // Skipped when the previous page was BFCache-suspended — the page is
    // frozen in the cache, not transitioning to "remote."
    if (!didSuspendPreviousPage && provisionalPage->deferredRemoteTransitionSite()) {
        ASSERT(oldMainFrameID);
        auto topDocumentSyncData = DocumentSyncData::create();
        topDocumentSyncData->documentURL = request.url();
        topDocumentSyncData->documentSecurityOrigin = SecurityOrigin::create(request.url());
        setTopDocumentSyncData(topDocumentSyncData.copyRef());
        protect(legacyMainFrameProcess())->send(Messages::WebPage::LoadDidCommitInAnotherProcess(*oldMainFrameID, std::nullopt, WTF::move(topDocumentSyncData)), webPageIDInMainFrameProcess());
        protect(m_browsingContextGroup)->transitionPageToRemotePage(*this, *provisionalPage->deferredRemoteTransitionSite());
    }

    if (!didSuspendPreviousPage && mainFrameInPreviousProcess && preferences->siteIsolationEnabled())
        mainFrameInPreviousProcess->removeChildFrames();

    // Defer shutting down old process as it might lead WebPageProxy to be closed and removeWebPage to be invoked again.
    auto preventProcessShutdownScope = protect(legacyMainFrameProcess())->shutdownPreventingScope();
    protect(legacyMainFrameProcess())->removeWebPage(*this, m_websiteDataStore.ptr() == provisionalPage->process().websiteDataStore() ? WebProcessProxy::EndsUsingDataStore::No : WebProcessProxy::EndsUsingDataStore::Yes);

    if (RefPtr mainFrameWebsitePolicies = provisionalPage->mainFrameWebsitePolicies())
        m_mainFrameWebsitePolicies = mainFrameWebsitePolicies->copy();

    // There is no way we'll be able to return to the page in the previous page so close it.
    if (!didSuspendPreviousPage && shouldClosePreviousPage(*provisionalPage))
        protect(legacyMainFrameProcess())->sendPageCloseMessage(identifier(), webPageIDInMainFrameProcess());

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (m_immersive)
        dismissImmersiveElement([] { });
#endif

    const auto oldProcessID = siteIsolatedProcess().coreProcessIdentifier();
    const auto oldWebPageID = m_webPageID;
    swapToProvisionalPage(provisionalPage.releaseNonNull());

    didCommitLoadForFrame(connection, frameID, WTF::move(frameInfo), WTF::move(request), navigationID, WTF::move(mimeType), frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, WTF::move(proxyName), source, containsPluginDocument, hasInsecureContent, mouseEventPolicy, WTF::move(documentSecurityPolicy), WTF::move(cspOriginsThatUpgradeInsecureNavigations), userData, restoredFromBackForwardCache);

    m_inspectorController->didCommitProvisionalPage(oldMainFrameID, oldProcessID, oldWebPageID, m_webPageID);
}

void WebPageProxy::continueNavigationInNewProcess(API::Navigation& navigation, WebFrameProxy& frame, RefPtr<SuspendedPageProxy>&& suspendedPage, BrowsingContextGroup& browsingContextGroup, Ref<WebProcessProxy>&& newProcess, ProcessSwapRequestedByClient processSwapRequestedByClient, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume, LoadedWebArchive loadedWebArchive, NavigationUpgradeToHTTPSBehavior navigationUpgradeToHTTPSBehavior, WebCore::ProcessSwapDisposition processSwapDisposition, WebsiteDataStore* replacedDataStoreForWebArchiveLoad)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "continueNavigationInNewProcess: newProcessPID=%i, hasSuspendedPage=%i", newProcess->processID(), !!suspendedPage);
    LOG(Loading, "Continuing navigation %" PRIu64 " '%s' in a new web process", navigation.navigationID().toUInt64(), navigation.loggingString().utf8().data());
    RELEASE_ASSERT(!newProcess->isInProcessCache());
    ASSERT(shouldTreatAsContinuingLoad != ShouldTreatAsContinuingLoad::No);
    navigation.setProcessID(newProcess->coreProcessIdentifier());

    if (navigation.currentRequest().url().protocolIsFile())
        newProcess->addPreviouslyApprovedFileURL(navigation.currentRequest().url());

    // Approve file URLs from the target BF item now; BackForwardUpdateItem IPC can surface
    // iframe file:// URLs before the new process is otherwise seeded with them.
    if (RefPtr targetItem = navigation.targetItem()) {
        Ref targetFrameState = targetItem->copyMainFrameStateWithChildren();
        newProcess->addPreviouslyApprovedFileURLsFromFrameStateTree(targetFrameState.get());
    }

    if (RefPtr provisionalPage = m_provisionalPage; provisionalPage && frame.isMainFrame()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "continueNavigationInNewProcess: There is already a pending provisional load, cancelling it (provisonalNavigationID=%" PRIu64 ", navigationID=%" PRIu64 ")", m_provisionalPage->navigationID().toUInt64(), navigation.navigationID().toUInt64());
        if (provisionalPage->navigationID() != navigation.navigationID())
            provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }

    RefPtr websitePolicies = navigation.websitePolicies();
    bool isServerSideRedirect = shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision && navigation.currentRequestIsRedirect();
    bool isProcessSwappingOnNavigationResponse = shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted;
    bool shouldInheritOriginFromInitiator = navigation.currentRequest().url().isAboutBlank() && navigation.originatingFrameInfo();
    Site navigationSite { shouldInheritOriginFromInitiator ? Site { navigation.originatingFrameInfo()->securityOrigin } : Site { navigation.currentRequest().url() } };

    Ref preferences = m_preferences;
    if (preferences->siteIsolationEnabled() && (!frame.isMainFrame() || newProcess->coreProcessIdentifier() == frame.process().coreProcessIdentifier())) {
        // about:blank frames should inherit the origin of the which originated navigation.
        // If the two frames share origins, they should share the same process.
        //
        // From HTML Spec: browsing the Web, section 7.4.2.2, Item 23, sub-item 5:
        // https://html.spec.whatwg.org/multipage/browsing-the-web.html#beginning-navigation
        //
        // If url matches about:blank or is about:srcdoc, then:
        //     Set documentState's origin to initiatorOriginSnapshot.
        //     Set documentState's about base URL to initiatorBaseURLSnapshot.
        std::optional<SecurityOriginData> originator = navigation.currentRequest().url().isAboutBlank() && navigation.originatingFrameInfo() ? std::make_optional(navigation.originatingFrameInfo()->securityOrigin) : std::nullopt;

        auto shouldTreatAsContinuingLoad = navigation.currentRequestIsRedirect() ? WebCore::ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted : WebCore::ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;

        // When a child frame's Back/Forward navigation triggers a process swap (Site Isolation),
        // send GoToBackForwardItem so the new process performs a proper history navigation using
        // the FrameState stored on the Navigation object.
        if (RefPtr frameState = navigation.backForwardFrameState()) {
            WEBPAGEPROXY_RELEASE_LOG(Loading, "continueNavigationInNewProcess: Sending GoToBackForwardItem for child frame to new process, URL=%" SENSITIVE_LOG_STRING, frameState->urlString.utf8().data());
            auto publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(navigation.currentRequest().url());
            frame.prepareForProvisionalLoadInProcess(newProcess, navigation, browsingContextGroup, originator, [
                navigationID = navigation.navigationID(),
                frameState = WTF::move(frameState),
                shouldTreatAsContinuingLoad,
                lastNavigationWasAppInitiated = m_lastNavigationWasAppInitiated,
                publicSuffix = WTF::move(publicSuffix),
                newProcess = newProcess.copyRef(),
                preventProcessShutdownScope = newProcess->shutdownPreventingScope()
            ] (std::optional<PageIdentifier> pageID) mutable {
                if (pageID)
                    newProcess->send(Messages::WebPage::GoToBackForwardItem({ navigationID, frameState.releaseNonNull(), FrameLoadType::IndexedBackForward, shouldTreatAsContinuingLoad, std::nullopt, lastNavigationWasAppInitiated, ShouldRestoreFromBackForwardCache::Unspecified, std::nullopt, WTF::move(publicSuffix), { }, WebCore::ProcessSwapDisposition::None }), *pageID);
            });
            return;
        }

        // FIXME: Add more parameters as appropriate. <rdar://116200985>
        LoadParameters loadParameters;
        loadParameters.request = navigation.currentRequest();
        loadParameters.shouldTreatAsContinuingLoad = shouldTreatAsContinuingLoad;
        loadParameters.frameIdentifier = frame.frameID();
        loadParameters.isRequestFromClientOrUserInput = navigation.isRequestFromClientOrUserInput();
        loadParameters.navigationID = navigation.navigationID();
        loadParameters.effectiveSandboxFlags = frame.effectiveSandboxFlags();
        loadParameters.effectiveReferrerPolicy = frame.effectiveReferrerPolicy();
        bool isPendingInitialHistoryItem = navigation.isInitialFrameSrcLoad() || frame.isShowingInitialAboutBlank();
        loadParameters.lockBackForwardList = isPendingInitialHistoryItem ? LockBackForwardList::No : navigation.lockBackForwardList();
        loadParameters.ownerPermissionsPolicy = navigation.ownerPermissionsPolicy();
        loadParameters.navigationUpgradeToHTTPSBehavior = navigationUpgradeToHTTPSBehavior;
        loadParameters.isHandledByAboutSchemeHandler = m_aboutSchemeHandler->canHandleURL(loadParameters.request.url());
        loadParameters.isHistoryItemNavigation = navigation.lastNavigationAction()->navigationType == NavigationType::BackForward;
        if (auto& action = navigation.lastNavigationAction()) {
            loadParameters.requester = action->requester;
            loadParameters.hadUserGesture = action->userGestureTokenIdentifier.has_value();
        }
        if (navigation.currentRequestIsRedirect())
            loadParameters.originalRequest = navigation.originalRequest();

        if (isPendingInitialHistoryItem)
            frame.setIsPendingInitialHistoryItem(true);

        frame.prepareForProvisionalLoadInProcess(newProcess, navigation, browsingContextGroup, originator, [
            loadParameters = WTF::move(loadParameters),
            newProcess = newProcess.copyRef(),
#if ENABLE(CONTENT_EXTENSIONS)
            iFrameResourceMonitoringEnabled = preferences->iFrameResourceMonitoringEnabled(),
            iFrameResourceMonitoringTestingSettingsEnabled = preferences->iFrameResourceMonitoringTestingSettingsEnabled(),
#endif
            preventProcessShutdownScope = newProcess->shutdownPreventingScope()
        ](std::optional<PageIdentifier> pageID) mutable {
            if (!pageID)
                return;
#if ENABLE(CONTENT_EXTENSIONS)
            if (iFrameResourceMonitoringEnabled)
                newProcess->requestResourceMonitorRuleLists(iFrameResourceMonitoringTestingSettingsEnabled);
#endif
            newProcess->send(Messages::WebPage::LoadRequest(WTF::move(loadParameters)), *pageID);
        });
        return;
    }

    // FIXME: Assert the equality of data stores regardless of whether site isolation is enabled or not.
    ASSERT(!preferences->siteIsolationEnabled() || newProcess->websiteDataStore() == &websiteDataStore());
    Ref frameProcess = browsingContextGroup.ensureProcessForSite(navigationSite, Site { mainFrame()->url() }, newProcess, preferences, loadedWebArchive, BrowsingContextGroupUpdate::None);
    // Make sure we destroy any existing ProvisionalPageProxy object *before* we construct a new one.
    // It is important from the previous provisional page to unregister itself before we register a
    // new one to avoid confusion.
    m_provisionalPage = nullptr;
    Ref provisionalPage = ProvisionalPageProxy::create(*this, WTF::move(frameProcess), browsingContextGroup, WTF::move(suspendedPage), navigation, isServerSideRedirect, navigation.currentRequest(), processSwapRequestedByClient, isProcessSwappingOnNavigationResponse, websitePolicies.get(), replacedDataStoreForWebArchiveLoad);
    m_provisionalPage = provisionalPage.copyRef();

    // FIXME: This should be a CompletionHandler, but http/tests/inspector/target/provisional-load-cancels-previous-load.html doesn't call it.
    Function<void()> continuation = [this, protectedThis = Ref { *this }, navigation = protect(navigation), shouldTreatAsContinuingLoad, websitePolicies = WTF::move(websitePolicies), existingNetworkResourceLoadIdentifierToResume, navigationUpgradeToHTTPSBehavior, processSwapDisposition]() mutable {
        RefPtr provisionalPage = m_provisionalPage;
        if (RefPtr item = navigation->targetItem()) {
            LOG(Loading, "WebPageProxy %p continueNavigationInNewProcess to back item URL %s", this, item->url().utf8().data());

            Ref pageLoadState = internals().pageLoadState;
            auto transaction = pageLoadState->transaction();
            pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), URL { item->url() } });

            provisionalPage->goToBackForwardItem(navigation, *item, WTF::move(websitePolicies), shouldTreatAsContinuingLoad, existingNetworkResourceLoadIdentifierToResume, processSwapDisposition);
            return;
        }

        RefPtr currentItem = backForwardList().currentItem();
        if (currentItem && (navigation->lockBackForwardList() == LockBackForwardList::Yes || navigation->lockHistory() == LockHistory::Yes)) {
            // If WebCore is supposed to lock the history for this load, then the new process needs to know about the current history item so it can update
            // it instead of creating a new one.
            provisionalPage->send(Messages::WebPage::SetCurrentHistoryItemForReattach(currentItem->copyMainFrameStateWithChildren()));
        }

        // FIXME: Work out timing of responding with the last policy delegate, etc
        ASSERT(!existingNetworkResourceLoadIdentifierToResume || !navigation->substituteData());
        if (auto& substituteData = navigation->substituteData())
            provisionalPage->loadData(navigation, SharedBuffer::create(Vector(substituteData->content)), substituteData->MIMEType, substituteData->encoding, substituteData->baseURL, substituteData->userData.get(), shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain(), WTF::move(websitePolicies), substituteData->sessionHistoryVisibility);
        else if (navigation->currentRequest().isEmpty()) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "continueNavigationInNewProcess: Tearing down provisional load because the navigation request URL is empty");
            m_provisionalPage = nullptr;
        } else
            provisionalPage->loadRequest(navigation, ResourceRequest { navigation->currentRequest() }, nullptr, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain(), WTF::move(websitePolicies), existingNetworkResourceLoadIdentifierToResume, navigationUpgradeToHTTPSBehavior);
    };

    Ref process = provisionalPage->process();

    if (provisionalPage->needsCookieAccessAddedInNetworkProcess()) {
        continuation = [
            networkProcess = protect(Ref { websiteDataStore() }->networkProcess()),
            continuation = WTF::move(continuation),
            navigationDomain = RegistrableDomain(navigation.currentRequest().url()),
            process,
            preventProcessShutdownScope = process->shutdownPreventingScope(),
            loadedWebArchive
        ] () mutable {
            networkProcess->addAllowedFirstPartyForCookies(process, navigationDomain, loadedWebArchive, WTF::move(continuation));
        };
    }

    if (m_inspectorController->shouldPauseLoadingForPage(provisionalPage))
        m_inspectorController->setContinueLoadingCallbackForPage(provisionalPage, WTF::move(continuation));
    else
        continuation();
}

SessionState WebPageProxy::sessionState(WTF::Function<bool (WebBackForwardListItem&)>&& filter) const
{
    RELEASE_ASSERT(RunLoop::isMain());
    SessionState sessionState;

#if ENABLE(BACK_FORWARD_LIST_SWIFT)
    sessionState.backForwardListState = backForwardList().backForwardListState(WebBackForwardListItemFilter::create(WTF::move(filter)).ptr());
#else
    sessionState.backForwardListState = backForwardList().backForwardListState(WTF::move(filter));
#endif

    auto& pendingURL = internals().pageLoadState.pendingAPIRequestURL();
    auto& provisionalURL = !pendingURL.isEmpty() ? pendingURL : internals().pageLoadState.provisionalURL();

    if (!provisionalURL.isEmpty())
        sessionState.provisionalURL = provisionalURL;

    sessionState.renderTreeSize = renderTreeSize();
    sessionState.isAppInitiated = m_lastNavigationWasAppInitiated;
    return sessionState;
}

RefPtr<API::Navigation> WebPageProxy::restoreFromSessionState(SessionState sessionState, bool navigate)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "restoreFromSessionState:");

    m_lastNavigationWasAppInitiated = sessionState.isAppInitiated;
    m_sessionRestorationRenderTreeSize = 0;
    m_hitRenderTreeSizeThreshold = false;

    bool hasBackForwardList = !!sessionState.backForwardListState.currentIndex;

    if (hasBackForwardList) {
        m_sessionStateWasRestoredByAPIRequest = true;

        backForwardList().restoreFromState(WTF::move(sessionState.backForwardListState));
        // If the process is not launched yet, the session will be restored when sending the WebPageCreationParameters;
        if (hasRunningProcess())
            backForwardList().setItemsAsRestoredFromSession();

        Ref pageLoadState = internals().pageLoadState;
        auto transaction = pageLoadState->transaction();
        pageLoadState->setCanGoBack(transaction, backForwardList().backItem());
        pageLoadState->setCanGoForward(transaction, backForwardList().forwardItem());

        // The back / forward list was restored from a sessionState so we don't want to snapshot the current
        // page when navigating away. Suppress navigation snapshotting until the next load has committed
        suppressNextAutomaticNavigationSnapshot();
    }

    // FIXME: Navigating should be separate from state restoration.
    if (navigate) {
        m_sessionRestorationRenderTreeSize = sessionState.renderTreeSize;
        if (!m_sessionRestorationRenderTreeSize)
            m_hitRenderTreeSizeThreshold = true; // If we didn't get data on renderTreeSize, just don't fire the milestone.

        if (!sessionState.provisionalURL.isNull())
            return loadRequest(WTF::move(sessionState.provisionalURL));

        if (hasBackForwardList) {
            if (RefPtr item = backForwardList().currentItem())
                return goToBackForwardItem(*item);
        }
    }

    return nullptr;
}

RectEdges<bool> WebPageProxy::rubberBandableEdgesRespectingHistorySwipe() const
{
    auto rubberBandableEdges = this->rubberBandableEdges();
    if (shouldUseImplicitRubberBandControl()) {
        rubberBandableEdges.setLeft(!backForwardList().backItem());
        rubberBandableEdges.setRight(!backForwardList().forwardItem());
    }

    return rubberBandableEdges;
}

static OptionSet<CrossSiteNavigationDataTransfer::Flag> checkIfNavigationContainsDataTransfer(const SecurityOriginData requesterOrigin, const ResourceRequest& currentRequest)
{
    OptionSet<CrossSiteNavigationDataTransfer::Flag> navigationDataTransfer;
    if (requesterOrigin.securityOrigin()->isOpaque())
        return navigationDataTransfer;

    auto currentURL = currentRequest.url();
    if (!currentURL.query().isEmpty() || !currentURL.fragmentIdentifier().isEmpty())
        navigationDataTransfer.add(CrossSiteNavigationDataTransfer::Flag::DestinationLinkDecoration);

    URL referrerURL { currentRequest.httpReferrer() };
    if (!referrerURL.query().isEmpty() || !referrerURL.fragmentIdentifier().isEmpty())
        navigationDataTransfer.add(CrossSiteNavigationDataTransfer::Flag::ReferrerLinkDecoration);

    return navigationDataTransfer;
}

void WebPageProxy::didCommitLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool wasPrivateRelayed, String&& proxyName, const WebCore::ResourceResponseSource source, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, DocumentSecurityPolicy&& documentSecurityPolicy, HashSet<WebCore::SecurityOriginData>&& cspOriginsThatUpgradeInsecureNavigations, const UserData& userData, RestoredFromBackForwardCache restoredFromBackForwardCache)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " didCommitLoadForFrame in navigation %" PRIu64, identifier().toUInt64(), navigationID ? navigationID->toUInt64() : 0);
#if ENABLE(BACK_FORWARD_LIST_SWIFT)
    LOG(BackForward, "(Back/Forward) After load commit, back/forward list is now:%s", std::string(backForwardList().loggingString()).data());
#else
    LOG(BackForward, "(Back/Forward) After load commit, back/forward list is now:%s", backForwardList().loggingString().utf8().data());
#endif

    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    if (frame->provisionalFrame()) {
        frame->commitProvisionalFrame(connection, frameID, WTF::move(frameInfo), WTF::move(request), navigationID, WTF::move(mimeType), frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, wasPrivateRelayed, WTF::move(proxyName), source, containsPluginDocument, hasInsecureContent, mouseEventPolicy, WTF::move(documentSecurityPolicy), WTF::move(cspOriginsThatUpgradeInsecureNavigations), userData, restoredFromBackForwardCache);
        return;
    }

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didCommitLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID && (navigation = m_navigationState->navigation(*navigationID))) {
        auto requesterOrigin = navigation->requesterOrigin();
        auto currentRequest = navigation->currentRequest();
        auto navigationDataTransfer = checkIfNavigationContainsDataTransfer(requesterOrigin, currentRequest);
        if (!navigationDataTransfer.isEmpty()) {
            RegistrableDomain currentDomain { currentRequest.url() };
            URL requesterURL { requesterOrigin.toString() };
            if (!currentDomain.matches(requesterURL)) {
                Ref websiteDataStore = m_websiteDataStore;
                protect(websiteDataStore->networkProcess())->didCommitCrossSiteLoadWithDataTransfer(websiteDataStore->sessionID(), RegistrableDomain { requesterURL }, currentDomain, navigationDataTransfer, identifier(), m_webPageID, request.didFilterLinkDecoration() ? DidFilterKnownLinkDecoration::Yes : DidFilterKnownLinkDecoration::No);
            }
        }
        if (RefPtr websitePolicies = navigation->websitePolicies(); websitePolicies && !m_provisionalPage)
            m_mainFrameWebsitePolicies = websitePolicies->copy();
    }

    // Reattach iframe subtree from BFCache restore. Always drain the pending entry to avoid
    // leaking iframe processes when the BFCache restore fails (RestoredFromBackForwardCache::No).
    if (frame->isMainFrame() && navigation) {
        RefPtr targetItem = navigation->targetItem();
        if (!targetItem) {
            if (restoredFromBackForwardCache == RestoredFromBackForwardCache::Yes)
                WEBPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "didCommitLoadForFrame: BFCache commit but navigation has no target item");
        } else if (auto pendingChildren = internals().pendingBackForwardCachedChildren.take(targetItem->identifier()); !pendingChildren.isEmpty()) {
            if (restoredFromBackForwardCache == RestoredFromBackForwardCache::Yes)
                frame->adoptChildFrames(WTF::move(pendingChildren));
        }
    }

    m_hasCommittedAnyProvisionalLoads = true;

    Ref process = WebProcessProxy::fromConnection(connection);
    process->didCommitProvisionalLoad();
    if (!request.url().protocolIsAbout())
        process->didCommitMeaningfulProvisionalLoad();

    if (frame->isMainFrame()) {
        m_hasUpdatedRenderingAfterDidCommitLoad = false;
#if PLATFORM(COCOA)
        if (RefPtr drawingAreaProxy = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea))
            internals().firstLayerTreeTransactionIdAfterDidCommitLoad = drawingAreaProxy->nextMainFrameLayerTreeTransactionID();
#endif
        internals().pageAllowedToRunInTheBackgroundActivityDueToTitleChanges = nullptr;
        internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications = nullptr;
        internals().didCommitLoadForMainFrameTimestamp = MonotonicTime::now();
    }

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();
    bool markPageInsecure = hasInsecureContent == HasInsecureContent::Yes;
    Ref preferences = m_preferences;

    if (frame->isMainFrame()) {
        protectedPageLoadState->didCommitLoad(transaction, certificateInfo, markPageInsecure, usedLegacyTLS, wasPrivateRelayed, WTF::move(proxyName), source, frameInfo.securityOrigin);
        m_shouldSuppressNextAutomaticNavigationSnapshot = false;

#if PLATFORM(COCOA)
        for (auto frameIDWithPendingLoad : m_framesWithSubresourceLoadingForPageLoadTiming)
            WTFEndSignpost(static_cast<uintptr_t>(frameID.toUInt64()), PLTSubresourceLoading, "didCommitLoadForFrame(%llu), ending pending resource loads for frame %llu", frameID.toUInt64(), frameIDWithPendingLoad.toUInt64());
#endif

        m_pageLoadTiming = std::exchange(m_pageLoadTimingPendingCommit, nullptr);
        m_framesWithSubresourceLoadingForPageLoadTiming.clear();

#if HAVE(SAFE_BROWSING)
        if (navigation && navigation->hadSafeBrowsingWarning())
            protectedPageLoadState->setHadSafeBrowsingWarning(transaction);
#endif
    }

#if USE(APPKIT)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME: A load going on in one frame shouldn't affect text editing in other frames on the page.
    protectedPageClient->resetSecureInputState();
#endif

    frame->didCommitLoad(mimeType, certificateInfo, containsPluginDocument, WTF::move(documentSecurityPolicy), WTF::move(cspOriginsThatUpgradeInsecureNavigations));

    if (frame->isMainFrame()) {
        std::optional<WebCore::PrivateClickMeasurement> privateClickMeasurement;
        if (internals().privateClickMeasurement)
            privateClickMeasurement = internals().privateClickMeasurement->pcm;
        else if (navigation && navigation->privateClickMeasurement())
            privateClickMeasurement = *navigation->privateClickMeasurement();
        if (privateClickMeasurement) {
            if (privateClickMeasurement->destinationSite().matches(frame->url()) || privateClickMeasurement->isSKAdNetworkAttribution())
                protect(websiteDataStore())->storePrivateClickMeasurement(*privateClickMeasurement);
        }
        if (RefPtr screenOrientationManager = m_screenOrientationManager)
            screenOrientationManager->unlockIfNecessary();
    }
    internals().privateClickMeasurement.reset();

    if (frame->isMainFrame()) {
        m_mainFrameHasCustomContentProvider = frameHasCustomContentProvider;

        if (m_mainFrameHasCustomContentProvider) {
            // Always assume that the main frame is pinned here, since the custom representation view will handle
            // any wheel events and dispatch them to the WKView when necessary.
            internals().mainFramePinnedState = { true, true, true, true };
            m_uiClient->pinnedStateDidChange(*this);
        }
        protectedPageClient->didCommitLoadForMainFrame(WTF::move(mimeType), frameHasCustomContentProvider);
    }

    // Even if WebPage has the default pageScaleFactor (and therefore doesn't reset it),
    // WebPageProxy's cache of the value can get out of sync (e.g. in the case where a
    // plugin is handling page scaling itself) so we should reset it to the default
    // for standard main frame loads.
    if (frame->isMainFrame()) {
        m_pageScaleFactor = 1;
        m_pluginScaleFactor = 1;
        m_mainFramePluginHandlesPageScaleGesture = false;
        m_pluginMinZoomFactor = { };
        m_pluginMaxZoomFactor = { };
#if ENABLE(POINTER_LOCK)
        resetPointerLockState();
#endif
        protectedPageClient->setMouseEventPolicy(mouseEventPolicy);
#if ENABLE(PDF_HUD)
        protectedPageClient->removeAllPDFHUDs();
#endif
#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
        protectedPageClient->removeAnyPDFPageNumberIndicator();
#endif
#if ENABLE(GAMEPAD)
        resetRecentGamepadAccessState();
#endif
    }

    protectedPageLoadState->commitChanges();
#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->navigationCommittedForFrame(*frame, navigationID);
#endif
    if (m_loaderClient)
        m_loaderClient->didCommitLoadForFrame(*this, *frame, navigation, process->transformHandlesToObjects(protect(userData.object()).get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didCommitNavigation(*this, navigation, process->transformHandlesToObjects(protect(userData.object()).get()).get());
        m_navigationClient->didCommitLoadForFrame(*this, WTF::move(request), WTF::move(frameInfo));
    }
    if (frame->isMainFrame()) {
#if ENABLE(ATTACHMENT_ELEMENT)
        invalidateAllAttachments();
#endif
#if ENABLE(REMOTE_INSPECTOR)
        remoteInspectorInformationDidChange();
#endif
#if USE(APPKIT)
        closeSharedPreviewPanelIfNecessary();
#endif
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
        updateAllowedQueryParametersForAdvancedPrivacyProtectionsIfNeeded();
        if (navigation && navigation->websitePolicies())
            m_advancedPrivacyProtectionsPolicies = navigation->websitePolicies()->advancedPrivacyProtections();
#endif
    }

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (frame->isMainFrame() && preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::singleton().webPageURLChanged(*this);
#endif

#if ENABLE(MEDIA_STREAM)
    if (RefPtr userMediaPermissionRequestManager = m_userMediaPermissionRequestManager)
        userMediaPermissionRequestManager->didCommitLoadForFrame(frameID);
    if (frame->isMainFrame()) {
        m_shouldListenToVoiceActivity = false;
        m_mutedCaptureKindsDesiredByWebApp = { };
    }
#endif

#if ENABLE(MEDIA_USAGE)
    if (frame->isMainFrame() && m_mediaUsageManager)
        m_mediaUsageManager->reset();
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    if (frame->isMainFrame()) {
        resetMediaCapability();
        resetDisplayCaptureCapability();
    }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    if (frame->isMainFrame())
        m_internals->imageTranslationLanguageIdentifiers = std::nullopt;
#endif

    if (frame->isMainFrame())
        m_internals->textManipulationParameters = std::nullopt;
}

void WebPageProxy::didSameDocumentNavigationForFrameViaJS(IPC::Connection& connection, SameDocumentNavigationType navigationType, URL&& url, NavigationActionData&& navigationActionData, const UserData& userData)
{
    RefPtr protectedPageClient { pageClient() };

    auto frameID = navigationActionData.frameInfo.frameID;
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    Ref process = WebProcessProxy::fromConnection(connection);
    MESSAGE_CHECK_URL(process, url);
    MESSAGE_CHECK(process, url.protocolIsFile() || frame->url().isEmpty() || protocolHostAndPortAreEqual(url, frame->url()));

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrameViaJS: frameID=%" PRIu64 ", isMainFrame=%d, type=%u", frameID.toUInt64(), frame->isMainFrame(), std::to_underlying(navigationType));

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame()) {
        navigation = m_navigationState->createLoadRequestNavigation(process->coreProcessIdentifier(), ResourceRequest(URL { url }), backForwardList().currentItem());
        navigation->setLastNavigationAction(WTF::move(navigationActionData));
    }

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        protectedPageLoadState->didSameDocumentNavigation(transaction, url);

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    protectedPageLoadState->clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(WTF::move(url));

    protectedPageLoadState->commitChanges();
#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->fragmentNavigatedForFrame(*frame, navigation ? std::optional(navigation->navigationID()) : std::nullopt);
#endif

    if (isMainFrame)
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, process->transformHandlesToObjects(protect(userData.object()).get()).get());

    if (isMainFrame)
        protectedPageClient->didSameDocumentNavigationForMainFrame(navigationType);

    if (navigation)
        m_navigationState->didDestroyNavigation(navigation->processID(), navigation->navigationID());
}

static bool NODELETE frameSandboxAllowsOpeningExternalCustomProtocols(SandboxFlags sandboxFlags, bool hasUserGesture)
{
    if (!sandboxFlags.contains(SandboxFlag::Popups) || !sandboxFlags.contains(SandboxFlag::TopNavigation) || !sandboxFlags.contains(SandboxFlag::TopNavigationToCustomProtocols))
        return true;

    return !sandboxFlags.contains(SandboxFlag::TopNavigationByUserActivation) && hasUserGesture;
}

RefPtr<FrameState> WebPageProxy::frameStateForBackForwardChildFrame(WebFrameProxy& frame, WebCore::BackForwardItemIdentifier targetBackForwardItemIdentifier)
{
    auto index = frame.indexInFrameTreeSiblings();
    if (!index)
        return nullptr;

    RefPtr frameState = backForwardList().findFrameStateInItem(targetBackForwardItemIdentifier, frame.parentFrame()->frameID(), *index);
    if (!frameState)
        return nullptr;

    if (auto currentFrameID = frameState->frameID; currentFrameID && *currentFrameID != frame.frameID())
        backForwardList().updateFrameIdentifier(*currentFrameID, frame.frameID());

    return frameState;
}

void WebPageProxy::decidePolicyForNavigationAction(Ref<WebProcessProxy>&& process, WebFrameProxy& frame, NavigationActionData&& navigationActionData, CompletionHandler<void(PolicyDecision&&)>&& originalCompletionHandler)
{
    RefPtr<FrameState> frameStateForBackForwardNavigation;
    if (protect(preferences())->useUIProcessForBackForwardItemLoading() && navigationActionData.navigationType == WebCore::NavigationType::BackForward && navigationActionData.targetBackForwardItemIdentifier) {
        if (RefPtr frameState = frameStateForBackForwardChildFrame(frame, *navigationActionData.targetBackForwardItemIdentifier)) {
            WEBPAGEPROXY_RELEASE_LOG(Loading, "frameStateForBackForwardChildFrame: Back/Forward child frame, rewriting URL to %" SENSITIVE_LOG_STRING, frameState->urlString.utf8().data());
            navigationActionData.request.setURL(URL { frameState->urlString });

            frameStateForBackForwardNavigation = WTF::move(frameState);
        }
    }

    auto completionHandler = [
        originalCompletionHandler = WTF::move(originalCompletionHandler),
        frameStateForBackForwardNavigation
    ](PolicyDecision&& policyDecision) mutable {
        if (frameStateForBackForwardNavigation && policyDecision.policyAction == PolicyAction::Use)
            policyDecision.backForwardFrameState = WTF::move(frameStateForBackForwardNavigation);
        originalCompletionHandler(WTF::move(policyDecision));
    };

    auto frameInfo = navigationActionData.frameInfo;
    auto navigationID = navigationActionData.navigationID;
    auto originatingFrameInfoData = navigationActionData.originatingFrameInfoData;
    auto originalRequest = navigationActionData.originalRequest;
    auto request = navigationActionData.request;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: frameID=%" PRIu64 ", isMainFrame=%d, navigationID=%" PRIu64, frame.frameID().toUInt64(), frame.isMainFrame(), navigationID ? navigationID->toUInt64() : 0);

    LOG(Loading, "WebPageProxy::decidePolicyForNavigationAction - Original URL %s, current target URL %s", originalRequest.url().string().utf8().data(), request.url().string().utf8().data());

    RefPtr protectedPageClient { pageClient() };

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = std::optional(protectedPageLoadState->transaction());

    bool fromAPI = request.url() == protectedPageLoadState->pendingAPIRequestURL();
    if (frame.isMainFrame() && protectedPageClient->hasBrowsingWarning() && !(fromAPI || (navigationID && navigationID == protectedPageLoadState->pendingAPIRequest().navigationID))) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: Ignoring navigation because a safe browsing warning is currently shown");
        return completionHandler(PolicyDecision { isNavigatingToAppBoundDomain() });
    }

    if (navigationID && !fromAPI)
        protectedPageLoadState->clearPendingAPIRequest(*transaction);

    RefPtr<API::Navigation> navigation;
    if (navigationID)
        navigation = m_navigationState->navigation(*navigationID);

    // When process-swapping on a redirect, the navigationActionData / originatingFrameInfoData provided by the fresh new WebProcess are inaccurate since
    // the new process does not have sufficient information. To address the issue, we restore the information we stored on the NavigationAction during the original request
    // policy decision.
    if (!navigationActionData.redirectResponse.isNull() && navigation && navigation->lastNavigationAction()) {
        bool canHandleRequest = navigationActionData.canHandleRequest;
        auto redirectResponse = WTF::move(navigationActionData.redirectResponse);
        navigationActionData = *navigation->lastNavigationAction();
        navigationActionData.redirectResponse = WTF::move(redirectResponse);
        navigationActionData.canHandleRequest = canHandleRequest;
        frameInfo.securityOrigin = navigation->destinationFrameSecurityOrigin();
    }

    if (!navigation) {
        if (auto targetBackForwardItemIdentifier = navigationActionData.targetBackForwardItemIdentifier) {
            if (RefPtr item = backForwardList().itemForID(*targetBackForwardItemIdentifier)) {
                RefPtr fromItem = navigationActionData.sourceBackForwardItemIdentifier ? backForwardList().itemForID(*navigationActionData.sourceBackForwardItemIdentifier) : nullptr;
                if (!fromItem)
                    fromItem = backForwardList().currentItem();
                navigation = m_navigationState->createBackForwardNavigation(process->coreProcessIdentifier(), item->mainFrameItem(), WTF::move(fromItem), FrameLoadType::IndexedBackForward);
            }
        }
        if (!navigation)
            navigation = m_navigationState->createLoadRequestNavigation(process->coreProcessIdentifier(), ResourceRequest(request), protect(backForwardList().currentItem()));
    }

    // Store frameState on navigation for Site Isolation process swap.
    if (frameStateForBackForwardNavigation && navigation)
        navigation->setBackForwardFrameState(WTF::move(frameStateForBackForwardNavigation));

    if (!checkURLReceivedFromCurrentOrPreviousWebProcess(process, request.url())) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it is outside the sandbox");
#if PLATFORM(COCOA)
        if (WTF::linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DidFailProvisionalNavigationWithErrorForFileURLNavigation)) {
            WebCore::ResourceError error { errorDomainWebKitInternal, 0, { }, "Ignoring request to load this main resource because it is outside the sandbox"_s };
            m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), request.url(), error, nullptr);
        }
#endif
        return completionHandler(PolicyDecision { isNavigatingToAppBoundDomain() });
    }

    MESSAGE_CHECK_URL(process, originalRequest.url());

    navigationID = navigation->navigationID();

    // Make sure the provisional page always has the latest navigationID.
    if (auto* provisionalPage = m_provisionalPage.get(); provisionalPage && &provisionalPage->process() == process.ptr())
        provisionalPage->setNavigation(*navigation);

    navigation->setCurrentRequest(ResourceRequest(request), process->coreProcessIdentifier());
    navigation->setLastNavigationAction(navigationActionData);
    if (!navigation->originatingFrameInfo())
        navigation->setOriginatingFrameInfo(originatingFrameInfoData);
    navigation->setDestinationFrameSecurityOrigin(frameInfo.securityOrigin);
    if (navigationActionData.originatorAdvancedPrivacyProtections)
        navigation->setOriginatorAdvancedPrivacyProtections(*navigationActionData.originatorAdvancedPrivacyProtections);

    RefPtr mainFrameNavigation = frame.isMainFrame() ? navigation.get() : nullptr;
    RefPtr originatingFrame = WebFrameProxy::webFrame(navigation->originatingFrameInfo()->frameID);
    RefPtr sourceFrameInfo = API::FrameInfo::create(FrameInfoData { *navigation->originatingFrameInfo() });

    bool sourceAndDestinationEqual = originatingFrame == &frame
        || (originatingFrame == mainFrame() && m_provisionalPage && m_provisionalPage->mainFrame() == &frame);
    Ref destinationFrameInfo = sourceAndDestinationEqual ? protect(*sourceFrameInfo) : API::FrameInfo::create(FrameInfoData { frameInfo });

#if PLATFORM(COCOA)
    if (fromAPI && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NavigationActionSourceFrameNonNull))
        sourceFrameInfo = nullptr;
#endif

    bool shouldOpenAppLinks = !m_shouldSuppressAppLinksInNextNavigationPolicyDecision
    && destinationFrameInfo->isMainFrame()
    && (m_mainFrame && (!m_mainFrame->url().isNull() || !m_hasCommittedAnyProvisionalLoads) && m_mainFrame->url().host() != request.url().host())
    && navigationActionData.navigationType != WebCore::NavigationType::BackForward;

    RefPtr userInitiatedActivity = process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    std::optional<WebCore::FrameIdentifier> currentMainFrameIdentifier;
    if (frame.isMainFrame() && m_mainFrame)
        currentMainFrameIdentifier = m_mainFrame->frameID();
    Ref navigationAction = API::NavigationAction::create(WTF::move(navigationActionData), sourceFrameInfo.get(), destinationFrameInfo.ptr(), String(), ResourceRequest(request), originalRequest.url(), shouldOpenAppLinks, WTF::move(userInitiatedActivity), mainFrameNavigation.get(), currentMainFrameIdentifier);

#if ENABLE(CONTENT_FILTERING)
    if (frame.didHandleContentFilterUnblockNavigation(request)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it was handled by content filter");
        return receivedPolicyDecision(PolicyAction::Ignore, protect(m_navigationState->navigation(*navigationID)).get(), std::nullopt, WTF::move(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, std::nullopt, WTF::move(completionHandler));
    }
#endif

    std::optional<PolicyDecisionConsoleMessage> message;

    // Other ports do not implement WebPage::platformCanHandleRequest().
#if PLATFORM(COCOA)
    // Sandboxed iframes should be allowed to open external apps via custom protocols unless explicitely allowed (https://html.spec.whatwg.org/#hand-off-to-external-software).
    bool canHandleRequest = navigationAction->data().canHandleRequest || m_urlSchemeHandlersByScheme.contains<StringViewHashTranslator>(request.url().protocol());
    if (!canHandleRequest && !destinationFrameInfo->isMainFrame() && !frameSandboxAllowsOpeningExternalCustomProtocols(navigationAction->data().effectiveSandboxFlags, !!navigationAction->data().userGestureTokenIdentifier)) {
        if (!sourceFrameInfo || !protect(preferences())->needsSiteSpecificQuirks() || !Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(request.url().protocol(), sourceFrameInfo->securityOrigin())) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe");
            PolicyDecisionConsoleMessage errorMessage {
                MessageLevel::Error,
                MessageSource::Security,
                "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe"_s
            };
            return receivedPolicyDecision(PolicyAction::Ignore, protect(m_navigationState->navigation(*navigationID)).get(), std::nullopt, WTF::move(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTF::move(errorMessage), WTF::move(completionHandler));
        }
        message = PolicyDecisionConsoleMessage {
            MessageLevel::Warning,
            MessageSource::Security,
            "In the future, requests to navigate to a URL with custom protocol from a sandboxed iframe will be ignored"_s
        };
    }
#endif

    ShouldExpectSafeBrowsingResult shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::Yes;
    if (!protect(preferences())->safeBrowsingEnabled())
        shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::No;

    ShouldWaitForSiteHasStorageCheck shouldWaitForSiteHasStorageCheck = ShouldWaitForSiteHasStorageCheck::Yes;
    if (!frame.isMainFrame() || !protect(preferences())->enhancedSecurityHeuristicsEnabled())
        shouldWaitForSiteHasStorageCheck = ShouldWaitForSiteHasStorageCheck::No;

    ShouldWaitForEnhancedSecurityLinkCheck shouldWaitForEnhancedSecurityLink = ShouldWaitForEnhancedSecurityLinkCheck::No;
#if HAVE(ENHANCED_SECURITY_LINKS)
    if (frame.isMainFrame() && protect(preferences())->enhancedSecurityHeuristicsEnabled() && protect(preferences())->enhancedSecurityLinksEnabled())
        shouldWaitForEnhancedSecurityLink = ShouldWaitForEnhancedSecurityLinkCheck::Yes;
#endif

    ShouldExpectAppBoundDomainResult shouldExpectAppBoundDomainResult = ShouldExpectAppBoundDomainResult::No;
#if ENABLE(APP_BOUND_DOMAINS)
    shouldExpectAppBoundDomainResult = ShouldExpectAppBoundDomainResult::Yes;
#endif

    auto shouldWaitForInitialLinkDecorationFilteringData = ShouldWaitForInitialLinkDecorationFilteringData::No;
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (LinkDecorationFilteringController::sharedSingleton().cachedListData().isEmpty())
        shouldWaitForInitialLinkDecorationFilteringData = ShouldWaitForInitialLinkDecorationFilteringData::Yes;
    else if (m_needsInitialLinkDecorationFilteringData)
        sendCachedLinkDecorationFilteringData();
#endif

    transaction = std::nullopt;

    Ref listener = frame.setUpPolicyListenerProxy([
        this,
        protectedThis = Ref { *this },
        processInitiatingNavigation = process,
        frame = protect(frame),
        completionHandler = WTF::move(completionHandler),
        navigation,
        navigationAction,
        message = WTF::move(message),
        frameInfo,
        protectedPageClient = protect(pageClient())
#if HAVE(SAFE_BROWSING)
        , shouldExpectSafeBrowsingResult
#endif
    ] (PolicyAction policyAction, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain> isAppBoundDomain, WasNavigationIntercepted wasNavigationIntercepted) mutable {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: listener called: frameID=%" PRIu64 ", isMainFrame=%d, navigationID=%" PRIu64  ", policyAction=%" PUBLIC_LOG_STRING ", isAppBoundDomain=%d, wasNavigationIntercepted=%d", frame->frameID().toUInt64(), frame->isMainFrame(), navigation ? navigation->navigationID().toUInt64() : 0, toString(policyAction).characters(), !!isAppBoundDomain, wasNavigationIntercepted == WasNavigationIntercepted::Yes);

        if (policies && !policies->alternateRequest().isNull())
            navigation->setCurrentRequest(ResourceRequest(policies->alternateRequest()), std::nullopt);
        navigation->setWebsitePolicies(policies);

        auto completionHandlerWrapper = [
            this,
            protectedThis,
            processInitiatingNavigation = WTF::move(processInitiatingNavigation),
            frame,
            frameInfo,
            completionHandler = WTF::move(completionHandler),
            navigation = protect(*navigation),
            navigationAction = WTF::move(navigationAction),
            wasNavigationIntercepted,
            processSwapRequestedByClient,
            message = WTF::move(message)
        ] (PolicyAction policyAction) mutable {
            if (frame->isMainFrame()) {
                if (!navigation->websitePolicies())
                    navigation->setWebsitePolicies(protect(m_configuration->defaultWebsitePolicies())->copy());
                if (RefPtr policies = navigation->websitePolicies()) {
                    navigation->setEffectiveContentMode(effectiveContentModeAfterAdjustingPolicies(*policies, navigation->currentRequest()));
                    adjustAdvancedPrivacyProtectionsIfNeeded(*policies);
                }
            }
            receivedNavigationActionPolicyDecision(processInitiatingNavigation, policyAction, navigation.get(), WTF::move(navigationAction), processSwapRequestedByClient, frame, frameInfo, wasNavigationIntercepted, WTF::move(message), WTF::move(completionHandler));
        };

#if ENABLE(APP_BOUND_DOMAINS)
        if (policyAction != PolicyAction::Ignore) {
            if (!setIsNavigatingToAppBoundDomainAndCheckIfPermitted(frame->isMainFrame(), navigation->currentRequest().url(), isAppBoundDomain)) {
                auto error = errorForUnpermittedAppBoundDomainNavigation(navigation->currentRequest().url());
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), navigation->currentRequest().url(), error, nullptr);
                WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "Ignoring request to load this main resource because it is attempting to navigate away from an app-bound domain or navigate after using restricted APIs");
                completionHandlerWrapper(PolicyAction::Ignore);
                return;
            }
            if (frame->isMainFrame())
                m_isTopFrameNavigatingToAppBoundDomain = m_isNavigatingToAppBoundDomain;
        }
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
        if (m_needsInitialLinkDecorationFilteringData)
            sendCachedLinkDecorationFilteringData();
#endif

#if HAVE(SAFE_BROWSING)
        bool safeBrowsingWarningAlreadyShown = navigation->hadSafeBrowsingWarning() && !navigation->safeBrowsingWarning();
        if (!safeBrowsingWarningAlreadyShown) {
            m_safeBrowsingWarningShownForNavigation = std::nullopt;
            protectedPageClient->clearBrowsingWarning();
        }
#else
        protectedPageClient->clearBrowsingWarning();
#endif

        if (policyAction == PolicyAction::Download && navigation->safeBrowsingCheckOngoing()) {
            navigation->whenSafeBrowsingCheckCompletes([
                this, protectedThis = WTF::move(protectedThis), navigation, completionHandlerWrapper = WTF::move(completionHandlerWrapper),
                frame, frameInfo = WTF::move(frameInfo), protectedPageClient = WTF::move(protectedPageClient)
            ] mutable {
                if (RefPtr safeBrowsingWarning = navigation->safeBrowsingWarning()) {
                    navigation->setSafeBrowsingWarning(nullptr);
                    if (!frame->isMainFrame()) {
                        auto error = interruptedForPolicyChangeError(navigation->currentRequest());
                        m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), navigation->currentRequest().url(), error, nullptr);
                        WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: Ignoring download because Safe Browsing found a match.");
                        completionHandlerWrapper(PolicyAction::Ignore);
                        return;
                    }

                    Ref protectedPageLoadState = pageLoadState();
                    auto transaction = protectedPageLoadState->transaction();
                    protectedPageLoadState->setTitleFromBrowsingWarning(transaction, safeBrowsingWarning->title());

                    protectedPageClient->showBrowsingWarning(*safeBrowsingWarning, [protectedThis = WTF::move(protectedThis), completionHandlerWrapper = WTF::move(completionHandlerWrapper), protectedPageClient](auto&& result) mutable {
                        Ref protectedPageLoadState = protectedThis->pageLoadState();
                        auto transaction = protectedPageLoadState->transaction();
                        protectedPageLoadState->setTitleFromBrowsingWarning(transaction, { });

                        switchOn(result, [&](const URL& url) {
                            completionHandlerWrapper(PolicyAction::Ignore);
                            protectedThis->loadRequest({ URL { url } });
                        }, [&protectedThis, &completionHandlerWrapper](ContinueUnsafeLoad continueUnsafeLoad) {
                            switch (continueUnsafeLoad) {
                            case ContinueUnsafeLoad::No:
                                if (!protectedThis->hasCommittedAnyProvisionalLoads())
                                    protectedThis->m_uiClient->close(protectedThis.ptr());
                                completionHandlerWrapper(PolicyAction::Ignore);
                                break;
                            case ContinueUnsafeLoad::Yes:
                                completionHandlerWrapper(PolicyAction::Download);
                                break;
                            }
                        });
                    });
                    m_uiClient->didShowSafeBrowsingWarning();
                    return;
                }
                completionHandlerWrapper(PolicyAction::Download);
            });
            return;
        }

        if (RefPtr safeBrowsingWarning = navigation->safeBrowsingWarning()) {
            navigation->setSafeBrowsingWarning(nullptr);
            if (frame->isMainFrame() && safeBrowsingWarning->url().isValid()) {
                Ref protectedPageLoadState = pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setHadSafeBrowsingWarning(transaction);
                protectedPageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), safeBrowsingWarning->url() });
                protectedPageLoadState->commitChanges();
            }

            auto failProvisionalNavigation = [&] {
                auto error = interruptedForPolicyChangeError(navigation->currentRequest());
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), navigation->currentRequest().url(), error, nullptr);
                completionHandlerWrapper(PolicyAction::Ignore);
            };

            if (!frame->isMainFrame()) {
                WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: Ignoring request to load subframe resource because Safe Browsing found a match.");
                return failProvisionalNavigation();
            }

            if (m_configuration->backgroundTextExtractionEnabled()) {
                WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: Ignoring main frame navigation because Safe Browsing found a match and background text extraction is enabled.");
                return failProvisionalNavigation();
            }

            Ref protectedPageLoadState = pageLoadState();
            auto transaction = protectedPageLoadState->transaction();
            protectedPageLoadState->setTitleFromBrowsingWarning(transaction, safeBrowsingWarning->title());

            WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: showing safe browsing warning, navigationID=%" PRIu64, navigation->navigationID().toUInt64());
            protectedPageClient->showBrowsingWarning(*safeBrowsingWarning, [protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandlerWrapper), policyAction, protectedPageClient] (auto&& result) mutable {

                Ref protectedPageLoadState = protectedThis->pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setTitleFromBrowsingWarning(transaction, { });

                switchOn(result, [&] (const URL& url) {
#if HAVE(SAFE_BROWSING)
                    protectedThis->completeSafeBrowsingCheckForModals(false);
#endif
                    completionHandler(PolicyAction::Ignore);
                    protectedThis->loadRequest({ URL { url } });
                }, [&protectedThis, &completionHandler, policyAction] (ContinueUnsafeLoad continueUnsafeLoad) {
                    switch (continueUnsafeLoad) {
                    case ContinueUnsafeLoad::No:
#if HAVE(SAFE_BROWSING)
                        protectedThis->completeSafeBrowsingCheckForModals(false);
#endif
                        if (!protectedThis->hasCommittedAnyProvisionalLoads())
                            protectedThis->m_uiClient->close(protectedThis.ptr());
                        completionHandler(PolicyAction::Ignore);
                        break;
                    case ContinueUnsafeLoad::Yes:
#if HAVE(SAFE_BROWSING)
                        protectedThis->completeSafeBrowsingCheckForModals(true);
#endif
                        completionHandler(policyAction);
                        break;
                    }
                });
            });
            m_uiClient->didShowSafeBrowsingWarning();
            return;
        }
#if HAVE(SAFE_BROWSING)
        if (shouldExpectSafeBrowsingResult == ShouldExpectSafeBrowsingResult::Yes)
            protectedThis->completeSafeBrowsingCheckForModals(true);
#endif
        completionHandlerWrapper(policyAction);

    }, ShouldExpectSafeBrowsingResult::No, shouldExpectAppBoundDomainResult, shouldWaitForInitialLinkDecorationFilteringData, shouldWaitForSiteHasStorageCheck, shouldWaitForEnhancedSecurityLink);
    if (shouldExpectSafeBrowsingResult == ShouldExpectSafeBrowsingResult::Yes)
        beginSafeBrowsingCheck(request.url(), *navigation, frame.isMainFrame());
    if (shouldWaitForInitialLinkDecorationFilteringData == ShouldWaitForInitialLinkDecorationFilteringData::Yes)
        waitForInitialLinkDecorationFilteringData(listener);
    if (shouldWaitForSiteHasStorageCheck == ShouldWaitForSiteHasStorageCheck::Yes)
        beginSiteHasStorageCheck(request.url(), *navigation, listener);
#if HAVE(ENHANCED_SECURITY_LINKS)
    if (shouldWaitForEnhancedSecurityLink == ShouldWaitForEnhancedSecurityLinkCheck::Yes)
        beginEnhancedSecurityLinkCheck(request.url(), *navigation, listener);
#endif
#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldSendSecurityOriginData = !frame.isMainFrame() && shouldTreatURLProtocolAsAppBound(request.url(), websiteDataStore().configuration().enableInAppBrowserPrivacyForTesting());
    auto host = shouldSendSecurityOriginData ? frameInfo.securityOrigin.host() : request.url().host();
    auto protocol = shouldSendSecurityOriginData ? frameInfo.securityOrigin.protocol() : request.url().protocol();
    protect(websiteDataStore())->beginAppBoundDomainCheck(host.toString(), protocol.toString(), listener);
#endif

#if ENABLE(SWIFT_DEMO_URI_SCHEME)
    if (navigationAction->request().url().protocolIs("x-swift-demo"_s) && !m_shouldSuppressSwiftDemoInNextNavigationPolicyDecision) {
        auto logo = getSwiftLogoData();
        WTF::Vector<uint8_t> logo2;
        logo2.reserveCapacity(logo.getCount());
        for (swift::Int i = 0; i < logo.getCount(); i++)
            logo2.append(logo[i]);
        auto mimeType = "image/png"_s;
        auto charset = "US-ASCII"_s;
        auto baseURL = "x-swift-demo://"_s;
        auto data2 = SharedBuffer::create(WTF::move(logo2));
        m_shouldSuppressSwiftDemoInNextNavigationPolicyDecision = true;
        loadData(WTF::move(data2), mimeType, charset, baseURL);
        listener->ignore(WasNavigationIntercepted::Yes);
        return;
    }
#endif

    auto wasPotentiallyInitiatedByUser = navigation->isLoadedWithNavigationShared() || navigation->wasUserInitiated();
    if (!sessionID().isEphemeral())
        logFrameNavigation(frame, internals().pageLoadState.url(), request, navigationAction->data().redirectResponse.url(), wasPotentiallyInitiatedByUser);

    if (m_policyClient)
        m_policyClient->decidePolicyForNavigationAction(*this, &frame, WTF::move(navigationAction), originatingFrame.get(), originalRequest, WTF::move(request), WTF::move(listener));
    else {
#if HAVE(APP_SSO)
        if (m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision || !protect(preferences())->isExtensibleSSOEnabled())
            navigationAction->unsetShouldPerformSOAuthorization();
#endif

        m_navigationClient->decidePolicyForNavigationAction(*this, WTF::move(navigationAction), WTF::move(listener));
    }

    m_shouldSuppressAppLinksInNextNavigationPolicyDecision = false;

#if HAVE(APP_SSO)
    m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = false;
#endif
}

void WebPageProxy::backForwardAddItemShared(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState, LoadedWebArchive loadedWebArchive)
{
#if ENABLE(BACK_FORWARD_LIST_SWIFT)
    backForwardList().backForwardAddItemShared(&connection, WTF::move(navigatedFrameState), loadedWebArchive);
#else
    backForwardList().backForwardAddItemShared(connection, WTF::move(navigatedFrameState), loadedWebArchive);
#endif
}

void WebPageProxy::backForwardGoToItemShared(BackForwardItemIdentifier itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
#if ENABLE(BACK_FORWARD_LIST_SWIFT)
    backForwardList().backForwardGoToItemShared(itemID, CompletionHandlers::WebBackForwardList::BackForwardGoToItemCompletionHandler::create(WTF::move(completionHandler)).ptr());
#else
    backForwardList().backForwardGoToItemShared(itemID, WTF::move(completionHandler));
#endif
}

String WebPageProxy::currentURL() const
{
    auto& url = pageLoadState().activeURL();
    RefPtr currentItem = backForwardList().currentItem();
    if (url.isEmpty() && currentItem)
        return currentItem->url();
    return url.string();
}

URL WebPageProxy::currentResourceDirectoryURL() const
{
    auto resourceDirectoryURL = internals().pageLoadState.resourceDirectoryURL();
    if (!resourceDirectoryURL.isEmpty())
        return resourceDirectoryURL;
    if (auto* item = backForwardList().currentItem())
        return item->resourceDirectoryURL();
    return { };
}

#if PLATFORM(COCOA) && !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)

static std::span<const ASCIILiteral> gpuIOKitClasses()
{
    static constexpr std::array services {
#if PLATFORM(IOS_FAMILY)
        "AGXDeviceUserClient"_s,
        "AppleParavirtDeviceUserClient"_s,
        "IOGPU"_s,
        "IOSurfaceRootUserClient"_s,
#endif
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        "AGPMClient"_s,
        "AppleGraphicsControlClient"_s,
        "AppleGraphicsPolicyClient"_s,
        "AppleIntelMEUserClient"_s,
        "AppleMGPUPowerControlClient"_s,
        "AppleSNBFBUserClient"_s,
        "AppleUpstreamUserClient"_s,
        "AudioAUUC"_s,
        "IOAccelerationUserClient"_s,
        "IOAccelerator"_s,
        "IOAudioControlUserClient"_s,
        "IOAudioEngineUserClient"_s,
        "IOSurfaceRootUserClient"_s,
#endif
        // FIXME: Is this also needed in PLATFORM(MACCATALYST)?
#if PLATFORM(MAC) && CPU(ARM64)
        "IOMobileFramebufferUserClient"_s,
#endif
#if (PLATFORM(MAC) && CPU(ARM64)) || PLATFORM(IOS_FAMILY)
        "IOSurfaceAcceleratorClient"_s,
#endif
    };
    return services;
}

static std::span<const ASCIILiteral> gpuMachServices()
{
    static constexpr std::array services {
        "com.apple.MTLCompilerService"_s,
    };
    return services;
}

#endif // PLATFORM(COCOA)

#if PLATFORM(COCOA) && !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING) || HAVE(MACH_BOOTSTRAP_EXTENSION)
static bool shouldBlockIOKit(const WebPreferences& preferences)
{
    if (!preferences.useGPUProcessForMediaEnabled()
        || !preferences.captureVideoInGPUProcessEnabled()
        || !preferences.captureAudioInGPUProcessEnabled()
        || !preferences.webRTCPlatformCodecsInGPUProcessEnabled()
        || !preferences.useGPUProcessForCanvasRenderingEnabled()
        || !preferences.useGPUProcessForDOMRenderingEnabled()
        || !preferences.useGPUProcessForWebGLEnabled())
        return false;
    return true;
}
#endif

WebPageCreationParameters WebPageProxy::creationParameters(WebProcessProxy& process, DrawingAreaProxy& drawingArea, WebCore::FrameIdentifier mainFrameIdentifier, std::optional<RemotePageParameters>&& remotePageParameters, bool isProcessSwap)
{
    if (m_sessionStateWasRestoredByAPIRequest)
        backForwardList().setItemsAsRestoredFromSession();

    RefPtr pageClient = this->pageClient();

    WebPageCreationParameters parameters {
        .drawingAreaIdentifier = drawingArea.identifier(),
        .webPageProxyIdentifier = identifier(),
        .pageGroupData = m_pageGroup->data(),
        .browsingContextGroupIdentifier = m_browsingContextGroup->identifier(),
        .visitedLinkTableID = m_visitedLinkStore->identifier(),
        .userContentControllerParameters = m_userContentController->parametersForProcess(process),
        .mainFrameIdentifier = mainFrameIdentifier,
        .openedMainFrameName = m_openedMainFrameName,
        .mainFrameOpenerIdentifier = m_mainFrame && m_mainFrame->opener() ? std::optional(m_mainFrame->opener()->frameID()) : std::nullopt,
        .mainFrameOpenerURL = m_mainFrame && m_mainFrame->opener() ? m_mainFrame->opener()->url() : URL { },
        .initialSandboxFlags = m_mainFrame ? m_mainFrame->effectiveSandboxFlags() : SandboxFlags { },
        .initialReferrerPolicy = m_mainFrame ? m_mainFrame->effectiveReferrerPolicy() : ReferrerPolicy::EmptyString,
        .shouldSendConsoleLogsToUIProcessForTesting = m_configuration->shouldSendConsoleLogsToUIProcessForTesting(),
    };

    parameters.processDisplayName = m_configuration->processDisplayName();

    parameters.remotePageParameters = WTF::move(remotePageParameters);
    parameters.windowFeatures = m_configuration->windowFeatures();
    parameters.viewSize = pageClient ? pageClient->viewSize() : WebCore::IntSize { };
    parameters.activityState = internals().activityState;
#if ENABLE(TILED_CA_DRAWING_AREA)
    parameters.drawingAreaType = drawingArea.type();
#endif
    parameters.store = preferencesStore();
    parameters.isEditable = m_isEditable;
    parameters.underlayColor = internals().underlayColor;
    parameters.useFixedLayout = m_useFixedLayout;
    parameters.fixedLayoutSize = internals().fixedLayoutSize;
    parameters.defaultUnobscuredSize = internals().defaultUnobscuredSize;
    parameters.minimumUnobscuredSize = internals().minimumUnobscuredSize;
    parameters.maximumUnobscuredSize = internals().maximumUnobscuredSize;
    parameters.viewExposedRect = internals().viewExposedRect;
    if (m_displayID) {
        parameters.displayID = m_displayID;
        parameters.nominalFramesPerSecond = drawingArea.displayNominalFramesPerSecond();
    }
    parameters.alwaysShowsHorizontalScroller = m_alwaysShowsHorizontalScroller;
    parameters.alwaysShowsVerticalScroller = m_alwaysShowsVerticalScroller;
    parameters.suppressScrollbarAnimations = m_suppressScrollbarAnimations;
    parameters.paginationMode = m_paginationMode;
    parameters.paginationBehavesLikeColumns = m_paginationBehavesLikeColumns;
    parameters.pageLength = m_pageLength;
    parameters.gapBetweenPages = m_gapBetweenPages;
    parameters.userAgent = userAgent();
    parameters.canRunBeforeUnloadConfirmPanel = m_uiClient->canRunBeforeUnloadConfirmPanel();
    parameters.canRunModal = m_canRunModal;
    parameters.deviceScaleFactor = deviceScaleFactor();
#if USE(GRAPHICS_LAYER_WC) || USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    parameters.intrinsicDeviceScaleFactor = intrinsicDeviceScaleFactor();
#endif
    parameters.viewScaleFactor = m_viewScaleFactor;
    parameters.textZoomFactor = m_textZoomFactor;
    parameters.pageZoomFactor = m_pageZoomFactor;
    parameters.obscuredContentInsets = m_internals->obscuredContentInsets;
#if ENABLE(TOP_BANNER_VIEW_OVERLAYS)
    parameters.hasBannerViewOverlay = m_internals->hasBannerViewOverlay;
#endif
    parameters.mediaVolume = m_mediaVolume;
    parameters.muted = internals().mutedState;
    parameters.openedByDOM = m_openedByDOM;
    parameters.mayStartMediaWhenInWindow = m_mayStartMediaWhenInWindow;
    parameters.mediaPlaybackIsSuspended = m_mediaPlaybackIsSuspended;
    parameters.minimumSizeForAutoLayout = internals().minimumSizeForAutoLayout;
    parameters.sizeToContentAutoSizeMaximumSize = internals().sizeToContentAutoSizeMaximumSize;
    parameters.autoSizingShouldExpandToViewHeight = m_autoSizingShouldExpandToViewHeight;
    parameters.viewportSizeForCSSViewportUnits = internals().viewportSizeForCSSViewportUnits;
    parameters.scrollPinningBehavior = internals().scrollPinningBehavior;
    if (m_scrollbarOverlayStyle)
        parameters.scrollbarOverlayStyle = m_scrollbarOverlayStyle.value();
    else
        parameters.scrollbarOverlayStyle = std::nullopt;
    parameters.backgroundExtendsBeyondPage = m_backgroundExtendsBeyondPage;
    parameters.controlledByAutomation = m_controlledByAutomation;
    parameters.isProcessSwap = isProcessSwap;
    parameters.useDarkAppearance = useDarkAppearance();
    parameters.useElevatedUserInterfaceLevel = useElevatedUserInterfaceLevel();
#if PLATFORM(MAC)
    parameters.colorSpace = pageClient ? std::optional { pageClient->colorSpace() } : std::nullopt;
    parameters.useFormSemanticContext = useFormSemanticContext();
    parameters.headerBannerHeight = headerBannerHeight();
    parameters.footerBannerHeight = footerBannerHeight();
    if (m_viewWindowCoordinates)
        parameters.viewWindowCoordinates = *m_viewWindowCoordinates;
    parameters.overflowHeightForTopScrollEdgeEffect = m_overflowHeightForTopScrollEdgeEffect;
#if HAVE(NSVIEW_CORNER_CONFIGURATION)
    parameters.scrollbarAvoidanceCornerRadii = internals().scrollbarAvoidanceCornerRadii;
#endif
#endif

#if ENABLE(META_VIEWPORT)
    parameters.ignoresViewportScaleLimits = m_forceAlwaysUserScalable;
    parameters.viewportConfigurationViewLayoutSize = internals().viewportConfigurationViewLayoutSize;
    parameters.viewportConfigurationLayoutSizeScaleFactorFromClient = m_viewportConfigurationLayoutSizeScaleFactorFromClient;
    parameters.viewportConfigurationMinimumEffectiveDeviceWidth = m_viewportConfigurationMinimumEffectiveDeviceWidth;
    parameters.overrideViewportArguments = internals().overrideViewportArguments;
#endif

#if PLATFORM(IOS_FAMILY)
    parameters.screenSize = screenSize();
    parameters.availableScreenSize = availableScreenSize();
    parameters.overrideScreenSize = overrideScreenSize();
    parameters.overrideAvailableScreenSize = overrideAvailableScreenSize();
    parameters.textAutosizingWidth = textAutosizingWidth();
    parameters.mimeTypesWithCustomContentProviders = pageClient ? pageClient->mimeTypesWithCustomContentProviders() : Vector<String> { };
    parameters.deviceOrientation = m_deviceOrientation;
    parameters.hardwareKeyboardState = protect(protect(m_configuration)->processPool())->cachedHardwareKeyboardState();
    parameters.canShowWhileLocked = m_configuration->canShowWhileLocked();
    parameters.insertionPointColor = pageClient ? pageClient->insertionPointColor() : WebCore::Color { };
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    parameters.gamepadAccessRequiresExplicitConsent = m_configuration->gamepadAccessRequiresExplicitConsent();
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    parameters.allowsImmersiveEnvironments = m_configuration->allowsImmersiveEnvironments();
#endif

    Ref preferences = m_preferences;
#if PLATFORM(COCOA)
    parameters.smartInsertDeleteEnabled = m_isSmartInsertDeleteEnabled;
    parameters.additionalSupportedImageTypes = m_configuration->additionalSupportedImageTypes().value_or(Vector<String>());

#if !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
#if ENABLE(TILED_CA_DRAWING_AREA)
    if (!shouldBlockIOKit(preferences) || drawingArea.type() == DrawingAreaType::TiledCoreAnimation)
#else
    if (!shouldBlockIOKit(preferences))
#endif
    {
        parameters.gpuIOKitExtensionHandles = SandboxExtension::createHandlesForIOKitClassExtensions(gpuIOKitClasses(), std::nullopt);
        parameters.gpuMachExtensionHandles = SandboxExtension::createHandlesForMachLookup(gpuMachServices(), std::nullopt);
    }
#endif // !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
#endif // PLATFORM(COCOA)

#if ENABLE(TILED_CA_DRAWING_AREA)
    if (!shouldBlockIOKit(preferences)
        || drawingArea.type() == DrawingAreaType::TiledCoreAnimation
        || !preferences->unifiedPDFEnabled()) {
        auto handle = SandboxExtension::createHandleForMachLookup("com.apple.CARenderServer"_s, std::nullopt);
        if (handle)
            parameters.renderServerMachExtensionHandle = WTF::move(*handle);
    }
#endif // ENABLE(TILED_CA_DRAWING_AREA)

#if HAVE(STATIC_FONT_REGISTRY)
    if (preferences->shouldAllowUserInstalledFonts()) {
#if ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)
        protect(process.processPool())->registerUserInstalledFonts(process);
#else
        if (auto handles = process.fontdMachExtensionHandles())
            parameters.fontMachExtensionHandles = WTF::move(*handles);
#endif
    }
#endif
#if HAVE(APP_ACCENT_COLORS)
    parameters.accentColor = pageClient ? pageClient->accentColor() : WebCore::Color { };
#if PLATFORM(MAC)
    parameters.appUsesCustomAccentColor = pageClient && pageClient->appUsesCustomAccentColor();
#endif
#endif
    parameters.shouldScaleViewToFitDocument = m_shouldScaleViewToFitDocument;
    if (pageClient)
        parameters.userInterfaceLayoutDirection = pageClient->userInterfaceLayoutDirection();
    parameters.observedLayoutMilestones = internals().observedLayoutMilestones;
    parameters.overrideContentSecurityPolicy = m_overrideContentSecurityPolicy;
    parameters.contentSecurityPolicyModeForExtension = m_configuration->contentSecurityPolicyModeForExtension();
    parameters.cpuLimit = m_cpuLimit;

#if USE(WPE_RENDERER)
    if (pageClient)
        parameters.hostFileDescriptor = pageClient->hostFileDescriptor();
#endif

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    parameters.nativeWindowHandle = viewWidget();
#endif
#if USE(GRAPHICS_LAYER_WC)
    parameters.usesOffscreenRendering = pageClient && pageClient->usesOffscreenRendering();
#endif

    for (auto& iterator : m_urlSchemeHandlersByScheme)
        parameters.urlSchemeHandlers.set(iterator.key, iterator.value->identifier());
    parameters.urlSchemesWithLegacyCustomProtocolHandlers = WebProcessPool::urlSchemesWithCustomProtocolHandlers();

#if ENABLE(WEB_RTC)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.iceCandidateFilteringEnabled = preferences->iceCandidateFilteringEnabled();
#if USE(LIBWEBRTC)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.enumeratingAllNetworkInterfacesEnabled = preferences->enumeratingAllNetworkInterfacesEnabled();
#endif
#endif

#if ENABLE(APPLICATION_MANIFEST)
    parameters.applicationManifest = m_configuration->applicationManifest() ? std::optional<WebCore::ApplicationManifest>(m_configuration->applicationManifest()->applicationManifest()) : std::nullopt;
#endif

    parameters.needsFontAttributes = m_needsFontAttributes;
    parameters.needsScrollGeometryUpdates = m_needsScrollGeometryUpdates;
    parameters.backgroundColor = internals().backgroundColor;

    parameters.overriddenMediaType = m_overriddenMediaType;
    parameters.corsDisablingPatterns = corsDisablingPatterns();
    parameters.maskedURLSchemes = m_configuration->maskedURLSchemes();
    parameters.allowedNetworkHosts = m_configuration->allowedNetworkHosts();
    parameters.loadsSubresources = m_configuration->loadsSubresources();
    parameters.crossOriginAccessControlCheckEnabled = m_configuration->crossOriginAccessControlCheckEnabled();
    parameters.hasResourceLoadClient = !!m_resourceLoadClient;
    parameters.portsForUpgradingInsecureSchemeForTesting = m_configuration->portsForUpgradingInsecureSchemeForTesting();

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = m_webExtensionController)
        parameters.webExtensionControllerParameters = webExtensionController->parameters(m_configuration);

    if (RefPtr weakWebExtensionController = m_weakWebExtensionController.get())
        parameters.webExtensionControllerParameters = weakWebExtensionController->parameters(m_configuration);
#endif

    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldCaptureAudioInGPUProcess = preferences->captureAudioInGPUProcessEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldCaptureVideoInGPUProcess = preferences->captureVideoInGPUProcessEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldRenderCanvasInGPUProcess = preferences->useGPUProcessForCanvasRenderingEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldRenderDOMInGPUProcess = useGPUProcessForDOMRenderingEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldPlayMediaInGPUProcess = preferences->useGPUProcessForMediaEnabled();
#if ENABLE(WEBGL)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldRenderWebGLInGPUProcess = preferences->useGPUProcessForWebGLEnabled();
#endif

    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldEnableVP9Decoder = preferences->vp9DecoderEnabled();
    parameters.shouldCaptureDisplayInUIProcess = m_configuration->processPool().configuration().shouldCaptureDisplayInUIProcess();
    parameters.shouldCaptureDisplayInGPUProcess = preferences->useGPUProcessForDisplayCapture();
#if ENABLE(APP_BOUND_DOMAINS)
    parameters.limitsNavigationsToAppBoundDomains = m_limitsNavigationsToAppBoundDomains;
#endif
    parameters.lastNavigationWasAppInitiated = m_lastNavigationWasAppInitiated;
    parameters.shouldRelaxThirdPartyCookieBlocking = m_configuration->shouldRelaxThirdPartyCookieBlocking();
    parameters.canUseCredentialStorage = m_canUseCredentialStorage;

    parameters.httpsUpgradeEnabled = preferences->upgradeKnownHostsToHTTPSEnabled() && m_configuration->httpsUpgradeEnabled();
    parameters.allowPostingLegacySynchronousMessages = m_configuration->allowPostingLegacySynchronousMessages();
    parameters.backgroundTextExtractionEnabled = m_configuration->backgroundTextExtractionEnabled();

#if ENABLE(APP_HIGHLIGHTS)
    parameters.appHighlightsVisible = appHighlightsVisibility() ? HighlightVisibility::Visible : HighlightVisibility::Hidden;
#endif

#if HAVE(TOUCH_BAR)
    parameters.requiresUserActionForEditingControlsManager = m_configuration->requiresUserActionForEditingControlsManager();
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    parameters.hasResizableWindows = pageClient && pageClient->hasResizableWindows();
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    parameters.linkDecorationFilteringData = LinkDecorationFilteringController::sharedSingleton().cachedListData();
    parameters.allowedQueryParametersForAdvancedPrivacyProtections = cachedAllowedQueryParametersForAdvancedPrivacyProtections();
#endif

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
#if ENABLE(LAUNCHD_BLOCKING_IN_WEBCONTENT)
    bool createBootstrapExtension = false;
#else
    bool createBootstrapExtension = !parameters.store.getBoolValueForKey(WebPreferencesKey::experimentalSandboxEnabledKey());
#endif
    if (!shouldBlockIOKit(preferences)
#if ENABLE(TILED_CA_DRAWING_AREA)
        || drawingArea.type() == DrawingAreaType::TiledCoreAnimation
#endif
        || createBootstrapExtension)
        parameters.machBootstrapHandle = SandboxExtension::createHandleForMachBootstrapExtension();
#endif

#if (PLATFORM(GTK) || PLATFORM(WPE)) && (USE(GBM) || OS(ANDROID))
    parameters.preferredBufferFormats = preferredBufferFormats();
#endif

#if HAVE(AUDIT_TOKEN)
    parameters.presentingApplicationAuditToken = presentingApplicationAuditToken();
#endif

#if PLATFORM(COCOA)
    parameters.presentingApplicationBundleIdentifier = presentingApplicationBundleIdentifier();
#endif

#if ENABLE(IMAGE_ANALYSIS)
    parameters.imageTranslationLanguageIdentifiers = m_internals->imageTranslationLanguageIdentifiers;
#endif

    parameters.textManipulationParameters = m_internals->textManipulationParameters;

    parameters.accessibilityMode = m_accessibilityMode;
    parameters.shouldForceSiteIsolationAlwaysOnForTesting = WebPreferences::forcedSiteIsolationAlwaysOnForTesting();
    parameters.shouldEnableNetworkInstrumentation = inspectorController().isNetworkInstrumentationEnabled();

    return parameters;
}

#if ENABLE(BACK_FORWARD_LIST_SWIFT)

WebBackForwardListMessageForwarder& WebPageProxy::backForwardListMessageReceiver() const
{
    // Returns a pointer to something owned by the BackForwardList
IGNORE_CLANG_WARNINGS_BEGIN("return-stack-address")
    return backForwardList().getMessageReceiver().get();
IGNORE_CLANG_WARNINGS_END
}

#endif

} // namespace WebKit

#undef WEBPAGEPROXY_RELEASE_LOG
#undef WEBPAGEPROXY_RELEASE_LOG_ERROR
#undef MESSAGE_CHECK_URL_COMPLETION
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
