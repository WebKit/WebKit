/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "APIArray.h"
#include "APIAttachment.h"
#include "APIContentWorld.h"
#include "APIContextMenuClient.h"
#include "APIDictionary.h"
#include "APIFindClient.h"
#include "APIFindMatchesClient.h"
#include "APIFormClient.h"
#include "APIFrameInfo.h"
#include "APIFullscreenClient.h"
#include "APIGeometry.h"
#include "APIHistoryClient.h"
#include "APIHitTestResult.h"
#include "APIIconLoadingClient.h"
#include "APILegacyContextHistoryClient.h"
#include "APILoaderClient.h"
#include "APINavigation.h"
#include "APINavigationAction.h"
#include "APINavigationClient.h"
#include "APINavigationResponse.h"
#include "APIOpenPanelParameters.h"
#include "APIPageConfiguration.h"
#include "APIPolicyClient.h"
#include "APIResourceLoadClient.h"
#include "APISecurityOrigin.h"
#include "APIUIClient.h"
#include "APIURLRequest.h"
#include "APIWebsitePolicies.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "AuthenticationManager.h"
#include "AuthenticatorManager.h"
#include "DownloadManager.h"
#include "DownloadProxy.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxy.h"
#include "EventDispatcherMessages.h"
#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "FrameTreeNodeData.h"
#include "LegacyGlobalSettings.h"
#include "LoadParameters.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "NativeWebGestureEvent.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "NavigationActionData.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "NotificationPermissionRequest.h"
#include "NotificationPermissionRequestManager.h"
#include "PageClient.h"
#include "PrintInfo.h"
#include "ProcessThrottler.h"
#include "ProvisionalPageProxy.h"
#include "SafeBrowsingWarning.h"
#include "SpeechRecognitionPermissionManager.h"
#include "SpeechRecognitionRemoteRealtimeMediaSource.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManager.h"
#include "SyntheticEditingCommandType.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "TextRecognitionUpdateResult.h"
#include "URLSchemeTaskParameters.h"
#include "UndoOrRedo.h"
#include "UserMediaPermissionRequestProxy.h"
#include "UserMediaProcessManager.h"
#include "WKContextPrivate.h"
#include "WebAutomationSession.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListCounts.h"
#include "WebBackForwardListItem.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebEditCommandProxy.h"
#include "WebEventConversion.h"
#include "WebFoundTextRange.h"
#include "WebFrame.h"
#include "WebFrameMessages.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFullScreenManagerProxy.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebImage.h"
#include "WebInspectorUIProxy.h"
#include "WebInspectorUtilities.h"
#include "WebKeyboardEvent.h"
#include "WebNavigationDataStore.h"
#include "WebNavigationState.h"
#include "WebNotificationManagerProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageDebuggable.h"
#include "WebPageGroup.h"
#include "WebPageGroupData.h"
#include "WebPageInspectorController.h"
#include "WebPageMessages.h"
#include "WebPageNetworkParameters.h"
#include "WebPageProxyMessages.h"
#include "WebPasteboardProxy.h"
#include "WebPaymentCoordinatorProxy.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include "WebPreferences.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebScreenOrientationManagerProxy.h"
#include "WebURLSchemeHandler.h"
#include "WebUserContentControllerProxy.h"
#include "WebViewDidMoveToWindowObserver.h"
#include "WebWheelEventCoalescer.h"
#include "WebsiteDataStore.h"
#include <WebCore/AlternativeTextClient.h>
#include <WebCore/AppHighlight.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/CompositionHighlight.h>
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/ElementContext.h>
#include <WebCore/EventNames.h>
#include <WebCore/ExceptionDetails.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/FontAttributeChanges.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GlobalFrameIdentifier.h>
#include <WebCore/GlobalWindowIdentifier.h>
#include <WebCore/LengthBox.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaDeviceHashSalts.h>
#include <WebCore/MediaStreamRequest.h>
#include <WebCore/ModalContainerTypes.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PerformanceLoggingClient.h>
#include <WebCore/PermissionState.h>
#include <WebCore/PlatformEvent.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/ShareData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/ValidationBubble.h>
#include <WebCore/WindowFeatures.h>
#include <WebCore/WritingDirection.h>
#include <stdio.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextStream.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "APIApplicationManifest.h"
#endif

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
#include "RemoteScrollingCoordinatorProxy.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if PLATFORM(COCOA)
#include "InsertTextOptions.h"
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteLayerTreeScrollingPerformanceData.h"
#include "UserMediaCaptureManagerProxy.h"
#include "VideoFullscreenManagerProxy.h"
#include "VideoFullscreenManagerProxyMessages.h"
#include <WebCore/AttributedString.h>
#include <WebCore/CoreAudioCaptureDeviceManager.h>
#include <WebCore/RunLoopObserver.h>
#include <WebCore/SystemBattery.h>
#include <wtf/MachSendRight.h>
#include <wtf/cocoa/Entitlements.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if PLATFORM(MAC)
#include "DisplayLink.h"
#include <WebCore/ImageUtilities.h>
#include <WebCore/UTIUtilities.h>
#endif

#if HAVE(TOUCH_BAR)
#include "TouchBarMenuData.h"
#include "TouchBarMenuItemData.h"
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK)
#include "ViewSnapshotStore.h"
#endif

#if PLATFORM(GTK)
#include "GtkSettingsManager.h"
#include <WebCore/SelectionData.h>
#endif

#if USE(CAIRO)
#include <WebCore/CairoUtilities.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#include <WebCore/MediaPlaybackTarget.h>
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

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
#include "WebDeviceOrientationUpdateProviderProxy.h"
#endif

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResult.h"
#endif

#if ENABLE(MEDIA_USAGE)
#include "MediaUsageManager.h"
#endif

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
#include "WebDateTimePicker.h"
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

#if HAVE(SCREEN_CAPTURE_KIT)
#import "DisplayCaptureSessionManager.h"
#endif

#if HAVE(SC_CONTENT_SHARING_SESSION)
#import <WebCore/ScreenCaptureKitSharingSessionManager.h>
#endif

#if USE(QUICK_LOOK)
#include <WebCore/PreviewConverter.h>
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionController.h"
#endif

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())
#define MESSAGE_CHECK_URL(process, url) MESSAGE_CHECK_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), process->connection())
#define MESSAGE_CHECK_COMPLETION(process, assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, process->connection(), completion)

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, this, m_identifier.toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), ##__VA_ARGS__)
#define WEBPAGEPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, this, m_identifier.toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), ##__VA_ARGS__)

// Represents the number of wheel events we can hold in the queue before we start pushing them preemptively.
static const unsigned wheelEventQueueSizeThreshold = 10;

static const Seconds resetRecentCrashCountDelay = 30_s;
static unsigned maximumWebProcessRelaunchAttempts = 1;
static const Seconds audibleActivityClearDelay = 10_s;
static const Seconds tryCloseTimeoutDelay = 50_ms;

namespace WebKit {
using namespace WebCore;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageProxyCounter, ("WebPageProxy"));

class StorageRequests {
    WTF_MAKE_NONCOPYABLE(StorageRequests); WTF_MAKE_FAST_ALLOCATED;
    friend NeverDestroyed<StorageRequests>;
public:
    static StorageRequests& singleton();

    void processOrAppend(CompletionHandler<void()>&& completionHandler)
    {
        if (m_requestsAreBeingProcessed) {
            m_requests.append(WTFMove(completionHandler));
            return;
        }
        m_requestsAreBeingProcessed = true;
        completionHandler();
    }

    void processNextIfAny()
    {
        if (m_requests.isEmpty()) {
            m_requestsAreBeingProcessed = false;
            return;
        }
        m_requests.takeFirst()();
    }

private:
    StorageRequests() { }
    ~StorageRequests() { }

    Deque<CompletionHandler<void()>> m_requests;
    bool m_requestsAreBeingProcessed { false };
};

StorageRequests& StorageRequests::singleton()
{
    static NeverDestroyed<StorageRequests> requests;
    return requests;
}

#if !LOG_DISABLED
static const char* webMouseEventTypeString(WebEvent::Type type)
{
    switch (type) {
    case WebEvent::MouseDown:
        return "MouseDown";
    case WebEvent::MouseUp:
        return "MouseUp";
    case WebEvent::MouseMove:
        return "MouseMove";
    case WebEvent::MouseForceChanged:
        return "MouseForceChanged";
    case WebEvent::MouseForceDown:
        return "MouseForceDown";
    case WebEvent::MouseForceUp:
        return "MouseForceUp";
    default:
        ASSERT_NOT_REACHED();
        return "<unknown>";
    }
}

static const char* webKeyboardEventTypeString(WebEvent::Type type)
{
    switch (type) {
    case WebEvent::KeyDown:
        return "KeyDown";
    case WebEvent::KeyUp:
        return "KeyUp";
    case WebEvent::RawKeyDown:
        return "RawKeyDown";
    case WebEvent::Char:
        return "Char";
    default:
        ASSERT_NOT_REACHED();
        return "<unknown>";
    }
}
#endif // !LOG_DISABLED

class PageClientProtector {
    WTF_MAKE_NONCOPYABLE(PageClientProtector);
public:
    PageClientProtector(PageClient& pageClient)
        : m_pageClient(pageClient)
    {
        m_pageClient->refView();
    }

    ~PageClientProtector()
    {
        ASSERT(m_pageClient);
        m_pageClient->derefView();
    }

private:
    WeakPtr<PageClient> m_pageClient;
};

void WebPageProxy::forMostVisibleWebPageIfAny(PAL::SessionID sessionID, const SecurityOriginData& origin, CompletionHandler<void(WebPageProxy*)>&& completionHandler)
{
    // FIXME: If not finding right away a visible page, we might want to try again for a given period of time when there is a change of visibility.
    RefPtr<WebPageProxy> selectedPage;
    WebProcessProxy::forWebPagesWithOrigin(sessionID, origin, [&](auto& page) {
        if (!page.mainFrame())
            return;
        if (page.isViewVisible() && (!selectedPage || !selectedPage->isViewVisible())) {
            selectedPage = &page;
            return;
        }
        if (page.isViewFocused() && (!selectedPage || !selectedPage->isViewFocused())) {
            selectedPage = &page;
            return;
        }
    });
    completionHandler(selectedPage.get());
}

Ref<WebPageProxy> WebPageProxy::create(PageClient& pageClient, WebProcessProxy& process, Ref<API::PageConfiguration>&& configuration)
{
    return adoptRef(*new WebPageProxy(pageClient, process, WTFMove(configuration)));
}

WebPageProxy::WebPageProxy(PageClient& pageClient, WebProcessProxy& process, Ref<API::PageConfiguration>&& configuration)
    : m_identifier(Identifier::generate())
    , m_webPageID(PageIdentifier::generate())
    , m_pageClient(pageClient)
    , m_configuration(WTFMove(configuration))
    , m_navigationClient(makeUniqueRef<API::NavigationClient>())
    , m_historyClient(makeUniqueRef<API::HistoryClient>())
    , m_iconLoadingClient(makeUnique<API::IconLoadingClient>())
    , m_formClient(makeUnique<API::FormClient>())
    , m_uiClient(makeUnique<API::UIClient>())
    , m_findClient(makeUnique<API::FindClient>())
    , m_findMatchesClient(makeUnique<API::FindMatchesClient>())
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuClient(makeUnique<API::ContextMenuClient>())
#endif
    , m_navigationState(makeUnique<WebNavigationState>())
    , m_process(process)
    , m_pageGroup(*m_configuration->pageGroup())
    , m_preferences(*m_configuration->preferences())
    , m_userContentController(*m_configuration->userContentController())
#if ENABLE(WK_WEB_EXTENSIONS)
    , m_webExtensionController(m_configuration->webExtensionController())
    , m_weakWebExtensionController(m_configuration->weakWebExtensionController())
#endif
    , m_visitedLinkStore(*m_configuration->visitedLinkStore())
    , m_websiteDataStore(*m_configuration->websiteDataStore())
    , m_userAgent(standardUserAgent())
    , m_overrideContentSecurityPolicy { m_configuration->overrideContentSecurityPolicy() }
#if ENABLE(FULLSCREEN_API)
    , m_fullscreenClient(makeUnique<API::FullscreenClient>())
#endif
    , m_geolocationPermissionRequestManager(*this)
#if USE(RUNNINGBOARD)
    , m_audibleActivityTimer(RunLoop::main(), this, &WebPageProxy::clearAudibleActivity)
#endif
    , m_initialCapitalizationEnabled(m_configuration->initialCapitalizationEnabled())
    , m_cpuLimit(m_configuration->cpuLimit())
    , m_backForwardList(WebBackForwardList::create(*this))
    , m_waitsForPaintAfterViewDidMoveToWindow(m_configuration->waitsForPaintAfterViewDidMoveToWindow())
    , m_hasRunningProcess(process.state() != WebProcessProxy::State::Terminated)
#if HAVE(CVDISPLAYLINK)
    , m_wheelEventActivityHysteresis([this](PAL::HysteresisState state) { wheelEventHysteresisUpdated(state); })
#endif
    , m_controlledByAutomation(m_configuration->isControlledByAutomation())
#if PLATFORM(COCOA)
    , m_isSmartInsertDeleteEnabled(TextChecker::isSmartInsertDeleteEnabled())
#endif
    , m_pageLoadState(*this)
    , m_updateReportedMediaCaptureStateTimer(RunLoop::main(), this, &WebPageProxy::updateReportedMediaCaptureState)
    , m_inspectorController(makeUnique<WebPageInspectorController>(*this))
#if ENABLE(REMOTE_INSPECTOR)
    , m_inspectorDebuggable(makeUnique<WebPageDebuggable>(*this))
#endif
    , m_resetRecentCrashCountTimer(RunLoop::main(), this, &WebPageProxy::resetRecentCrashCount)
    , m_tryCloseTimeoutTimer(RunLoop::main(), this, &WebPageProxy::tryCloseTimedOut)
    , m_corsDisablingPatterns(m_configuration->corsDisablingPatterns())
#if ENABLE(APP_BOUND_DOMAINS)
    , m_ignoresAppBoundDomains(m_configuration->ignoresAppBoundDomains())
    , m_limitsNavigationsToAppBoundDomains(m_configuration->limitsNavigationsToAppBoundDomains())
#endif
    , m_notificationManagerMessageHandler(*this)
#if ENABLE(VIDEO_PRESENTATION_MODE)
    , m_fullscreenVideoTextRecognitionTimer(RunLoop::main(), this, &WebPageProxy::fullscreenVideoTextRecognitionTimerFired)
#endif
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "constructor:");

    if (!m_configuration->drawsBackground())
        m_backgroundColor = Color(Color::transparentBlack);

    updateActivityState();
    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();
    
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    m_layerHostingMode = m_activityState & ActivityState::IsInWindow ? WebPageProxy::pageClient().viewLayerHostingMode() : LayerHostingMode::OutOfProcess;
#endif

    platformInitialize();

#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif

    WebProcessPool::statistics().wkPageCount++;

    m_preferences->addPage(*this);
    m_pageGroup->addPage(*this);

#if ENABLE(WK_WEB_EXTENSIONS)
    if (m_webExtensionController)
        m_webExtensionController->addPage(*this);
    if (m_weakWebExtensionController)
        m_weakWebExtensionController->addPage(*this);
#endif

    m_inspector = WebInspectorUIProxy::create(*this);

    if (hasRunningProcess())
        didAttachToRunningProcess();

    addAllMessageReceivers();

#if PLATFORM(IOS_FAMILY)
    DeprecatedGlobalSettings::setDisableScreenSizeOverride(m_preferences->disableScreenSizeOverride());
    
    if (m_configuration->preferences()->serviceWorkerEntitlementDisabledForTesting())
        disableServiceWorkerEntitlementInNetworkProcess();
#endif

#if PLATFORM(COCOA)
    m_activityStateChangeDispatcher = makeUnique<RunLoopObserver>(static_cast<CFIndex>(RunLoopObserver::WellKnownRunLoopOrders::ActivityStateChange), [this] {
        this->dispatchActivityStateChange();
    });
#endif

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->setInspectable(JSRemoteInspectorGetInspectionEnabledByDefault());
    m_inspectorDebuggable->init();
#endif
    m_inspectorController->init();

#if ENABLE(IPC_TESTING_API)
    if (m_preferences->ipcTestingAPIEnabled())
        process.setIgnoreInvalidMessageForTesting();
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (m_preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::sharedNotifier().addWebPage(*this);
#endif
}

WebPageProxy::~WebPageProxy()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "destructor:");

    ASSERT(m_process->webPage(m_identifier) != this);
#if ASSERT_ENABLED
    for (auto& page : m_process->pages())
        ASSERT(page.get() != this);
#endif

    setPageLoadStateObserver(nullptr);

    if (!m_isClosed)
        close();

    WebProcessPool::statistics().wkPageCount--;

    if (m_spellDocumentTag)
        TextChecker::closeSpellDocumentWithTag(m_spellDocumentTag.value());

    m_preferences->removePage(*this);
    m_pageGroup->removePage(*this);

#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif

#if PLATFORM(MACCATALYST)
    EndowmentStateTracker::singleton().removeClient(*this);
#endif
    
    for (auto& callback : m_nextActivityStateChangeCallbacks)
        callback();

    if (auto* networkProcess = websiteDataStore().networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::RemoveWebPageNetworkParameters(sessionID(), m_identifier), 0);

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (m_preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::sharedNotifier().removeWebPage(*this);
#endif
}

void WebPageProxy::addAllMessageReceivers()
{
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID, *this);
    m_process->addMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_webPageID, m_notificationManagerMessageHandler);
}

void WebPageProxy::removeAllMessageReceivers()
{
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);
    m_process->removeMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_webPageID);
}

// FIXME: Should return a const PageClient& and add a separate non-const
// version of this function, but several PageClient methods will need to become
// const for this to be possible.
PageClient& WebPageProxy::pageClient() const
{
    ASSERT(m_pageClient);
    return *m_pageClient;
}

PAL::SessionID WebPageProxy::sessionID() const
{
    return m_websiteDataStore->sessionID();
}

DrawingAreaProxy* WebPageProxy::provisionalDrawingArea() const
{
    if (m_provisionalPage && m_provisionalPage->drawingArea())
        return m_provisionalPage->drawingArea();
    return drawingArea();
}

const API::PageConfiguration& WebPageProxy::configuration() const
{
    return m_configuration.get();
}

ProcessID WebPageProxy::gpuProcessIdentifier() const
{
    if (m_isClosed)
        return 0;

#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcess = process().processPool().gpuProcess())
        return gpuProcess->processIdentifier();
#endif

    return 0;
}

ProcessID WebPageProxy::processIdentifier() const
{
    if (m_isClosed)
        return 0;

    return m_process->processIdentifier();
}

bool WebPageProxy::hasRunningProcess() const
{
    // A page that has been explicitly closed is never valid.
    if (m_isClosed)
        return false;

    return m_hasRunningProcess;
}

void WebPageProxy::notifyProcessPoolToPrewarm()
{
    m_process->processPool().didReachGoodTimeToPrewarm();
}

void WebPageProxy::setPreferences(WebPreferences& preferences)
{
    if (&preferences == m_preferences.ptr())
        return;

    m_preferences->removePage(*this);
    m_preferences = preferences;
    m_preferences->addPage(*this);

    preferencesDidChange();
}
    
void WebPageProxy::setHistoryClient(UniqueRef<API::HistoryClient>&& historyClient)
{
    m_historyClient = WTFMove(historyClient);
}

void WebPageProxy::setNavigationClient(UniqueRef<API::NavigationClient>&& navigationClient)
{
    m_navigationClient = WTFMove(navigationClient);
}

void WebPageProxy::setLoaderClient(std::unique_ptr<API::LoaderClient>&& loaderClient)
{
    m_loaderClient = WTFMove(loaderClient);
}

void WebPageProxy::setPolicyClient(std::unique_ptr<API::PolicyClient>&& policyClient)
{
    m_policyClient = WTFMove(policyClient);
}

void WebPageProxy::setFormClient(std::unique_ptr<API::FormClient>&& formClient)
{
    if (!formClient) {
        m_formClient = makeUnique<API::FormClient>();
        return;
    }

    m_formClient = WTFMove(formClient);
}

void WebPageProxy::setUIClient(std::unique_ptr<API::UIClient>&& uiClient)
{
    if (!uiClient) {
        m_uiClient = makeUnique<API::UIClient>();
        return;
    }

    m_uiClient = WTFMove(uiClient);

    if (hasRunningProcess())
        send(Messages::WebPage::SetCanRunBeforeUnloadConfirmPanel(m_uiClient->canRunBeforeUnloadConfirmPanel()));

    setCanRunModal(m_uiClient->canRunModal());
    setNeedsFontAttributes(m_uiClient->needsFontAttributes());
}

void WebPageProxy::setIconLoadingClient(std::unique_ptr<API::IconLoadingClient>&& iconLoadingClient)
{
    bool hasClient = iconLoadingClient.get();
    if (!iconLoadingClient)
        m_iconLoadingClient = makeUnique<API::IconLoadingClient>();
    else
        m_iconLoadingClient = WTFMove(iconLoadingClient);

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetUseIconLoadingClient(hasClient));
}

void WebPageProxy::setPageLoadStateObserver(std::unique_ptr<PageLoadState::Observer>&& observer)
{
    if (m_pageLoadStateObserver)
        pageLoadState().removeObserver(*m_pageLoadStateObserver);
    m_pageLoadStateObserver = WTFMove(observer);
    if (m_pageLoadStateObserver)
        pageLoadState().addObserver(*m_pageLoadStateObserver);
}

void WebPageProxy::setFindClient(std::unique_ptr<API::FindClient>&& findClient)
{
    if (!findClient) {
        m_findClient = makeUnique<API::FindClient>();
        return;
    }
    
    m_findClient = WTFMove(findClient);
}

void WebPageProxy::setFindMatchesClient(std::unique_ptr<API::FindMatchesClient>&& findMatchesClient)
{
    if (!findMatchesClient) {
        m_findMatchesClient = makeUnique<API::FindMatchesClient>();
        return;
    }

    m_findMatchesClient = WTFMove(findMatchesClient);
}

void WebPageProxy::setDiagnosticLoggingClient(std::unique_ptr<API::DiagnosticLoggingClient>&& diagnosticLoggingClient)
{
    m_diagnosticLoggingClient = WTFMove(diagnosticLoggingClient);
}

#if ENABLE(CONTEXT_MENUS)
void WebPageProxy::setContextMenuClient(std::unique_ptr<API::ContextMenuClient>&& contextMenuClient)
{
    if (!contextMenuClient) {
        m_contextMenuClient = makeUnique<API::ContextMenuClient>();
        return;
    }

    m_contextMenuClient = WTFMove(contextMenuClient);
}
#endif

void WebPageProxy::setInjectedBundleClient(const WKPageInjectedBundleClientBase* client)
{
    if (!client) {
        m_injectedBundleClient = nullptr;
        return;
    }

    m_injectedBundleClient = makeUnique<WebPageInjectedBundleClient>();
    m_injectedBundleClient->initialize(client);
}

void WebPageProxy::setResourceLoadClient(std::unique_ptr<API::ResourceLoadClient>&& client)
{
    bool hadResourceLoadClient = !!m_resourceLoadClient;
    m_resourceLoadClient = WTFMove(client);
    bool hasResourceLoadClient = !!m_resourceLoadClient;
    if (hadResourceLoadClient != hasResourceLoadClient)
        send(Messages::WebPage::SetHasResourceLoadClient(hasResourceLoadClient));
}

void WebPageProxy::handleMessage(IPC::Connection& connection, const String& messageName, const WebKit::UserData& messageBody)
{
    ASSERT(m_process->connection() == &connection);

    if (!m_injectedBundleClient)
        return;

    m_injectedBundleClient->didReceiveMessageFromInjectedBundle(this, messageName, m_process->transformHandlesToObjects(messageBody.object()).get());
}

void WebPageProxy::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&& completionHandler)
{
    ASSERT(m_process->connection() == &connection);

    if (!m_injectedBundleClient)
        return completionHandler({ });

    RefPtr<API::Object> returnData;
    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(this, messageName, m_process->transformHandlesToObjects(messageBody.object()).get(), [completionHandler = WTFMove(completionHandler), process = m_process] (RefPtr<API::Object>&& returnData) mutable {
        completionHandler(UserData(process->transformObjectsToHandles(returnData.get())));
    });
}

void WebPageProxy::launchProcess(const RegistrableDomain& registrableDomain, ProcessLaunchReason reason)
{
    ASSERT(!m_isClosed);
    ASSERT(!hasRunningProcess());

    WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcess:");

    // In case we are currently connected to the dummy process, we need to make sure the inspector proxy
    // disconnects from the dummy process first.
    m_inspector->reset();

    m_process->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();

    auto& processPool = m_process->processPool();

    auto* relatedPage = m_configuration->relatedPage();
    if (relatedPage && !relatedPage->isClosed() && reason == ProcessLaunchReason::InitialProcess) {
        m_process = relatedPage->ensureRunningProcess();
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcess: Using process (process=%p, PID=%i) from related page", m_process.ptr(), m_process->processIdentifier());
    } else
        m_process = processPool.processForRegistrableDomain(m_websiteDataStore.get(), registrableDomain, shouldEnableLockdownMode() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled);

    m_hasRunningProcess = true;
    m_shouldReloadDueToCrashWhenVisible = false;
    m_isLockdownModeExplicitlySet = m_configuration->isLockdownModeExplicitlySet();

    m_process->addExistingWebPage(*this, WebProcessProxy::BeginsUsingDataStore::Yes);
    addAllMessageReceivers();

#if ENABLE(IPC_TESTING_API)
    if (m_preferences->store().getBoolValueForKey(WebPreferencesKey::ipcTestingAPIEnabledKey()))
        m_process->setIgnoreInvalidMessageForTesting();
#endif

    finishAttachingToWebProcess(reason);

    auto pendingInjectedBundleMessage = WTFMove(m_pendingInjectedBundleMessages);
    for (auto& message : pendingInjectedBundleMessage)
        send(Messages::WebPage::PostInjectedBundleMessage(message.messageName, UserData(process().transformObjectsToHandles(message.messageBody.get()).get())));
}

bool WebPageProxy::suspendCurrentPageIfPossible(API::Navigation& navigation, RefPtr<WebFrameProxy>&& mainFrame, ProcessSwapRequestedByClient processSwapRequestedByClient, ShouldDelayClosingUntilFirstLayerFlush shouldDelayClosingUntilFirstLayerFlush)
{
    m_suspendedPageKeptToPreventFlashing = nullptr;
    m_lastSuspendedPage = nullptr;

    if (!mainFrame)
        return false;

    if (!hasCommittedAnyProvisionalLoads()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because has not committed any load yet", m_process->processIdentifier());
        return false;
    }

    if (isPageOpenedByDOMShowingInitialEmptyDocument()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because it is showing the initial empty document", m_process->processIdentifier());
        return false;
    }

    auto* fromItem = navigation.fromItem();

    // If the source and the destination back / forward list items are the same, then this is a client-side redirect. In this case,
    // there is no need to suspend the previous page as there will be no way to get back to it.
    if (fromItem && fromItem == m_backForwardList->currentItem()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because this is a client-side redirect", m_process->processIdentifier());
        return false;
    }

    if (fromItem && fromItem->url() != pageLoadState().url()) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because fromItem's URL does not match the page URL.", m_process->processIdentifier());
        return false;
    }

    bool needsSuspendedPageToPreventFlashing = shouldDelayClosingUntilFirstLayerFlush == ShouldDelayClosingUntilFirstLayerFlush::Yes;
    if (!needsSuspendedPageToPreventFlashing && (!fromItem || !shouldUseBackForwardCache())) {
        if (!fromItem)
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i there is no associated WebBackForwardListItem", m_process->processIdentifier());
        else
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i the back / forward cache is disabled", m_process->processIdentifier());
        return false;
    }

    WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Suspending current page for process pid %i", m_process->processIdentifier());
    mainFrame->frameLoadState().didSuspend();
    auto suspendedPage = makeUnique<SuspendedPageProxy>(*this, m_process.copyRef(), mainFrame.releaseNonNull(), shouldDelayClosingUntilFirstLayerFlush);

    LOG(ProcessSwapping, "WebPageProxy %" PRIu64 " created suspended page %s for process pid %i, back/forward item %s" PRIu64, identifier().toUInt64(), suspendedPage->loggingString(), m_process->processIdentifier(), fromItem ? fromItem->itemID().toString().utf8().data() : "0"_s);

    m_lastSuspendedPage = *suspendedPage;

    if (fromItem && shouldUseBackForwardCache())
        backForwardCache().addEntry(*fromItem, WTFMove(suspendedPage));
    else {
        ASSERT(needsSuspendedPageToPreventFlashing);
        m_suspendedPageKeptToPreventFlashing = WTFMove(suspendedPage);
    }

    return true;
}

WebBackForwardCache& WebPageProxy::backForwardCache() const
{
    return process().processPool().backForwardCache();
}

bool WebPageProxy::shouldUseBackForwardCache() const
{
    return m_preferences->usesBackForwardCache() && backForwardCache().capacity() > 0;
}

void WebPageProxy::swapToProvisionalPage(std::unique_ptr<ProvisionalPageProxy> provisionalPage)
{
    ASSERT(!m_isClosed);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "swapToProvisionalPage: newWebPageID=%" PRIu64, provisionalPage->webPageID().toUInt64());

    m_process = provisionalPage->process();
    m_webPageID = provisionalPage->webPageID();
    pageClient().didChangeWebPageID();
    ASSERT(m_process->websiteDataStore());
    m_websiteDataStore = *m_process->websiteDataStore();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInWebProcess = provisionalPage->contextIDForVisibilityPropagationInWebProcess();
#if ENABLE(GPU_PROCESS)
    m_contextIDForVisibilityPropagationInGPUProcess = provisionalPage->contextIDForVisibilityPropagationInGPUProcess();
#endif
#endif

    // FIXME: Do we really need to disable this logging in ephemeral sessions?
    if (m_logger)
        m_logger->setEnabled(this, !sessionID().isEphemeral());

    m_hasRunningProcess = true;

    ASSERT(!m_drawingArea);
    setDrawingArea(provisionalPage->takeDrawingArea());
    ASSERT(!m_mainFrame);
    m_mainFrame = provisionalPage->mainFrame();

    m_process->addExistingWebPage(*this, WebProcessProxy::BeginsUsingDataStore::No);
    addAllMessageReceivers();

    finishAttachingToWebProcess(ProcessLaunchReason::ProcessSwap);

#if PLATFORM(IOS_FAMILY)
    // On iOS, the displayID is derived from the webPageID.
    m_displayID = generateDisplayIDFromPageID();
    // FIXME: We may want to send WindowScreenDidChange on non-iOS platforms too.
    send(Messages::WebPage::WindowScreenDidChange(*m_displayID, std::nullopt));
#endif

#if PLATFORM(COCOA)
    auto accessibilityToken = provisionalPage->takeAccessibilityToken();
    if (!accessibilityToken.isEmpty())
        registerWebProcessAccessibilityToken({ accessibilityToken.data(), accessibilityToken.size() });
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    auto accessibilityPlugID = provisionalPage->accessibilityPlugID();
    if (!accessibilityPlugID.isEmpty())
        bindAccessibilityTree(accessibilityPlugID);
#endif
}

void WebPageProxy::finishAttachingToWebProcess(ProcessLaunchReason reason)
{
    ASSERT(m_process->state() != AuxiliaryProcessProxy::State::Terminated);

    updateActivityState();
    updateThrottleState();

    didAttachToRunningProcess();

    // In the process-swap case, the ProvisionalPageProxy already took care of initializing the WebPage in the WebProcess.
    if (reason != ProcessLaunchReason::ProcessSwap)
        initializeWebPage();

    m_inspector->updateForNewPageProcess(*this);

#if ENABLE(REMOTE_INSPECTOR)
    remoteInspectorInformationDidChange();
#endif

    updateWheelEventActivityAfterProcessSwap();

    pageClient().didRelaunchProcess();
    m_pageLoadState.didSwapWebProcesses();
    if (reason != ProcessLaunchReason::InitialProcess)
        m_drawingArea->waitForBackingStoreUpdateOnNextPaint();
}

void WebPageProxy::didAttachToRunningProcess()
{
    ASSERT(hasRunningProcess());

#if ENABLE(FULLSCREEN_API)
    ASSERT(!m_fullScreenManager);
    m_fullScreenManager = makeUnique<WebFullScreenManagerProxy>(*this, pageClient().fullScreenManagerProxyClient());
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    ASSERT(!m_playbackSessionManager);
    m_playbackSessionManager = PlaybackSessionManagerProxy::create(*this);
    ASSERT(!m_videoFullscreenManager);
    m_videoFullscreenManager = VideoFullscreenManagerProxy::create(*this, *m_playbackSessionManager);
    if (m_videoFullscreenManager)
        m_videoFullscreenManager->setMockVideoPresentationModeEnabled(m_mockVideoPresentationModeEnabled);
#endif

#if ENABLE(APPLE_PAY)
    ASSERT(!m_paymentCoordinator);
    m_paymentCoordinator = makeUnique<WebPaymentCoordinatorProxy>(*this);
#endif

#if USE(SYSTEM_PREVIEW)
    ASSERT(!m_systemPreviewController);
    m_systemPreviewController = makeUnique<SystemPreviewController>(*this);
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
    if (m_preferences->modelElementEnabled()) {
        ASSERT(!m_modelElementController);
        m_modelElementController = makeUnique<ModelElementController>(*this);
    }
#endif

#if ENABLE(WEB_AUTHN)
    ASSERT(!m_credentialsMessenger);
    m_credentialsMessenger = makeUnique<WebAuthenticatorCoordinatorProxy>(*this);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    ASSERT(!m_webDeviceOrientationUpdateProviderProxy);
    m_webDeviceOrientationUpdateProviderProxy = makeUnique<WebDeviceOrientationUpdateProviderProxy>(*this);
#endif

    m_screenOrientationManager = makeUnique<WebScreenOrientationManagerProxy>(*this);

#if ENABLE(WEBXR) && !USE(OPENXR)
    ASSERT(!m_xrSystem);
    m_xrSystem = makeUnique<PlatformXRSystem>(*this);
#endif
}

RefPtr<API::Navigation> WebPageProxy::launchProcessForReload()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload:");

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload: page is closed");
        return nullptr;
    }
    
    ASSERT(!hasRunningProcess());
    auto registrableDomain = m_backForwardList->currentItem() ? RegistrableDomain { URL { m_backForwardList->currentItem()->url() } } : RegistrableDomain { };
    launchProcess(registrableDomain, ProcessLaunchReason::Crash);

    if (!m_backForwardList->currentItem()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload: no current item to reload");
        return nullptr;
    }

    auto navigation = m_navigationState->createReloadNavigation(m_backForwardList->currentItem());

    String url = currentURL();
    if (!url.isEmpty()) {
        auto transaction = m_pageLoadState.transaction();
        m_pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), url });
    }

    std::optional<String> topPrivatelyControlledDomain;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(m_backForwardList->currentItem()->url());
#endif

    // We allow stale content when reloading a WebProcess that's been killed or crashed.
    send(Messages::WebPage::GoToBackForwardItem(navigation->navigationID(), m_backForwardList->currentItem()->itemID(), FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, std::nullopt, topPrivatelyControlledDomain));
    m_process->startResponsivenessTimer();

    return navigation;
}

void WebPageProxy::setDrawingArea(std::unique_ptr<DrawingAreaProxy>&& drawingArea)
{
    m_drawingArea = WTFMove(drawingArea);
    if (!m_drawingArea)
        return;

    m_drawingArea->startReceivingMessages();
    m_drawingArea->setSize(viewSize());

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (is<RemoteLayerTreeDrawingAreaProxy>(m_drawingArea))
        m_scrollingCoordinatorProxy = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea).createScrollingCoordinatorProxy();
#endif
}

void WebPageProxy::initializeWebPage()
{
    if (!hasRunningProcess())
        return;

    setDrawingArea(pageClient().createDrawingAreaProxy(m_process));
    ASSERT(m_drawingArea);

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton();
#endif

    if (auto& attributedBundleIdentifier = m_configuration->attributedBundleIdentifier(); !!attributedBundleIdentifier) {
        WebPageNetworkParameters parameters { attributedBundleIdentifier };
        websiteDataStore().networkProcess().send(Messages::NetworkProcess::AddWebPageNetworkParameters(sessionID(), m_identifier, WTFMove(parameters)), 0);
    }

    send(Messages::WebProcess::CreateWebPage(m_webPageID, creationParameters(m_process, *m_drawingArea)), 0);

    m_process->addVisitedLinkStoreUser(visitedLinkStore(), m_identifier);
}

void WebPageProxy::close()
{
    if (m_isClosed)
        return;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "close:");

    m_isClosed = true;

    reportPageLoadResult(ResourceError { ResourceError::Type::Cancellation });

    if (m_activePopupMenu)
        m_activePopupMenu->cancelTracking();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willClosePage(*this);
    }

#if ENABLE(WK_WEB_EXTENSIONS)
    if (m_webExtensionController)
        m_webExtensionController->removePage(*this);
    if (m_weakWebExtensionController)
        m_weakWebExtensionController->removePage(*this);
#endif

#if ENABLE(CONTEXT_MENUS)
    m_activeContextMenu = nullptr;
#endif

    m_provisionalPage = nullptr;

    m_inspector->invalidate();

    m_backForwardList->pageClosed();
    m_inspectorController->pageClosed();
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable = nullptr;
#endif
    pageClient().pageClosed();

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

    m_process->processPool().backForwardCache().removeEntriesForPage(*this);

    RunLoop::current().dispatch([destinationID = messageSenderDestinationID(), protectedProcess = m_process.copyRef(), preventProcessShutdownScope = m_process->shutdownPreventingScope()] {
        protectedProcess->send(Messages::WebPage::Close(), destinationID);
    });
    m_process->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();
    m_process->processPool().supplement<WebNotificationManagerProxy>()->clearNotifications(this);

    // Null out related WebPageProxy to avoid leaks.
    m_configuration->setRelatedPage(nullptr);

#if PLATFORM(IOS_FAMILY)
    // Make sure we don't hold a process assertion after getting closed.
    m_isVisibleActivity = nullptr;
    m_isAudibleActivity = nullptr;
    m_isCapturingActivity = nullptr;
    m_openingAppLinkActivity = nullptr;
    m_audibleActivityTimer.stop();
#endif

    stopAllURLSchemeTasks();
    updatePlayingMediaDidChange(MediaProducer::IsNotPlaying);
}

bool WebPageProxy::tryClose()
{
    if (!hasRunningProcess())
        return true;

    WEBPAGEPROXY_RELEASE_LOG(Process, "tryClose:");

    // Close without delay if the process allows it. Our goal is to terminate
    // the process, so we check a per-process status bit.
    if (m_process->isSuddenTerminationEnabled())
        return true;

    m_tryCloseTimeoutTimer.startOneShot(tryCloseTimeoutDelay);
    sendWithAsyncReply(Messages::WebPage::TryClose(), [this, weakThis = WeakPtr { *this }](bool shouldClose) {
        if (!weakThis)
            return;

        // If we timed out, don't ask the client to close again.
        if (!m_tryCloseTimeoutTimer.isActive())
            return;

        m_tryCloseTimeoutTimer.stop();
        if (shouldClose)
            closePage();
    });
    return false;
}

void WebPageProxy::tryCloseTimedOut()
{
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "tryCloseTimedOut: Timed out waiting for the process to respond to the WebPage::TryClose IPC, closing the page now");
    closePage();
}

void WebPageProxy::maybeInitializeSandboxExtensionHandle(WebProcessProxy& process, const URL& url, const URL& resourceDirectoryURL, SandboxExtension::Handle& sandboxExtensionHandle, bool checkAssumedReadAccessToResourceURL)
{
    if (!url.isLocalFile())
        return;

#if HAVE(AUDIT_TOKEN)
    // If the process is still launching then it does not have a PID yet. We will take care of creating the sandbox extension
    // once the process has finished launching.
    if (process.isLaunching() || process.wasTerminated())
        return;
#endif

    if (!resourceDirectoryURL.isEmpty()) {
        if (checkAssumedReadAccessToResourceURL && process.hasAssumedReadAccessToURL(resourceDirectoryURL))
            return;

        bool createdExtension = false;
#if HAVE(AUDIT_TOKEN)
        ASSERT(process.connection() && process.connection()->getAuditToken());
        if (process.connection() && process.connection()->getAuditToken()) {
            if (auto handle = SandboxExtension::createHandleForReadByAuditToken(resourceDirectoryURL.fileSystemPath(), *(process.connection()->getAuditToken()))) {
                sandboxExtensionHandle = WTFMove(*handle);
                createdExtension = true;
            }
        } else
#endif
        {
            if (auto handle = SandboxExtension::createHandle(resourceDirectoryURL.fileSystemPath(), SandboxExtension::Type::ReadOnly)) {
                sandboxExtensionHandle = WTFMove(*handle);
                createdExtension = true;
            }
        }

        if (createdExtension) {
            process.assumeReadAccessToBaseURL(*this, resourceDirectoryURL.string());
            return;
        }
    }

    if (process.hasAssumedReadAccessToURL(url))
        return;

    // Inspector resources are in a directory with assumed access.
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!WebKit::isInspectorPage(*this));

    bool createdExtension = false;
#if HAVE(AUDIT_TOKEN)
    ASSERT(process.connection() && process.connection()->getAuditToken());
    if (process.connection() && process.connection()->getAuditToken()) {
        if (auto handle = SandboxExtension::createHandleForReadByAuditToken("/"_s, *(process.connection()->getAuditToken()))) {
            createdExtension = true;
            sandboxExtensionHandle = WTFMove(*handle);
        }
    } else
#endif
    {
        if (auto handle = SandboxExtension::createHandle("/"_s, SandboxExtension::Type::ReadOnly)) {
            createdExtension = true;
            sandboxExtensionHandle = WTFMove(*handle);
        }
    }

    if (createdExtension) {
        willAcquireUniversalFileReadSandboxExtension(process);
        return;
    }

#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoUnconditionalUniversalSandboxExtension))
        willAcquireUniversalFileReadSandboxExtension(process);
#endif

    // We failed to issue an universal file read access sandbox, fall back to issuing one for the base URL instead.
    auto baseURL = url.truncatedForUseAsBase();
    auto basePath = baseURL.fileSystemPath();
    if (basePath.isNull())
        return;
#if HAVE(AUDIT_TOKEN)
    if (process.connection() && process.connection()->getAuditToken()) {
        if (auto handle = SandboxExtension::createHandleForReadByAuditToken(basePath, *(process.connection()->getAuditToken()))) {
            sandboxExtensionHandle = WTFMove(*handle);
            createdExtension = true;
        }
    } else
#endif
    {
        if (auto handle = SandboxExtension::createHandle(basePath, SandboxExtension::Type::ReadOnly)) {
            sandboxExtensionHandle = WTFMove(*handle);
            createdExtension = true;
        }
    }

    if (createdExtension)
        process.assumeReadAccessToBaseURL(*this, baseURL.string());
}

#if !PLATFORM(COCOA)

void WebPageProxy::addPlatformLoadParameters(WebProcessProxy&, LoadParameters&)
{
}

#endif

WebProcessProxy& WebPageProxy::ensureRunningProcess()
{
    if (!hasRunningProcess())
        launchProcess({ }, ProcessLaunchReason::InitialProcess);

    return m_process;
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, API::Object* userData)
{
    if (m_isClosed)
        return nullptr;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadRequest:");

    if (!hasRunningProcess())
        launchProcess(RegistrableDomain { request.url() }, ProcessLaunchReason::InitialProcess);

    auto navigation = m_navigationState->createLoadRequestNavigation(ResourceRequest(request), m_backForwardList->currentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

#if PLATFORM(COCOA)
    setLastNavigationWasAppInitiated(request);
#endif

    loadRequestWithNavigationShared(m_process.copyRef(), m_webPageID, navigation.get(), WTFMove(request), shouldOpenExternalURLsPolicy, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain());
    return navigation;
}

void WebPageProxy::loadRequestWithNavigationShared(Ref<WebProcessProxy>&& process, WebCore::PageIdentifier webPageID, API::Navigation& navigation, ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, API::Object* userData, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    ASSERT(!m_isClosed);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadRequestWithNavigationShared:");

    auto transaction = m_pageLoadState.transaction();

    auto url = request.url();
    if (shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::No)
        m_pageLoadState.setPendingAPIRequest(transaction, { navigation.navigationID(), url.string() });

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation.navigationID();
    loadParameters.request = WTFMove(request);
    loadParameters.shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    loadParameters.userData = UserData(process->transformObjectsToHandles(userData).get());
    loadParameters.shouldTreatAsContinuingLoad = shouldTreatAsContinuingLoad;
    loadParameters.websitePolicies = WTFMove(websitePolicies);
    loadParameters.lockHistory = navigation.lockHistory();
    loadParameters.lockBackForwardList = navigation.lockBackForwardList();
    loadParameters.clientRedirectSourceForHistory = navigation.clientRedirectSourceForHistory();
    loadParameters.effectiveSandboxFlags = navigation.effectiveSandboxFlags();
    loadParameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    loadParameters.existingNetworkResourceLoadIdentifierToResume = existingNetworkResourceLoadIdentifierToResume;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    loadParameters.topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(loadParameters.request.url().host().toString());
#endif
    maybeInitializeSandboxExtensionHandle(process, url, m_pageLoadState.resourceDirectoryURL(), loadParameters.sandboxExtensionHandle);

    addPlatformLoadParameters(process, loadParameters);

    if (shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::No)
        preconnectTo(url, predictedUserAgentForRequest(loadParameters.request));

    navigation.setIsLoadedWithNavigationShared(true);

    process->markProcessAsRecentlyUsed();

    if (!process->isLaunching() || !url.isLocalFile())
        process->send(Messages::WebPage::LoadRequest(loadParameters), webPageID);
    else
        process->send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(loadParameters, m_pageLoadState.resourceDirectoryURL(), m_identifier, true), webPageID);
    process->startResponsivenessTimer();
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
        launchProcess({ }, ProcessLaunchReason::InitialProcess);

    URL fileURL { fileURLString };
    if (!fileURL.isLocalFile()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile: file is not local");
        return nullptr;
    }

    URL resourceDirectoryURL;
    if (resourceDirectoryURLString.isNull())
        resourceDirectoryURL = URL({ }, "file:///"_s);
    else {
        resourceDirectoryURL = URL { resourceDirectoryURLString };
        if (!resourceDirectoryURL.isLocalFile()) {
            WEBPAGEPROXY_RELEASE_LOG(Loading, "loadFile: resource URL is not local");
            return nullptr;
        }
    }

    auto navigation = m_navigationState->createLoadRequestNavigation(ResourceRequest(fileURL), m_backForwardList->currentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), fileURLString }, resourceDirectoryURL);

    auto request = ResourceRequest(fileURL);
    request.setIsAppInitiated(isAppInitiated);
    m_lastNavigationWasAppInitiated = isAppInitiated;

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTFMove(request);
    loadParameters.shouldOpenExternalURLsPolicy = ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
#if ENABLE(PUBLIC_SUFFIX_LIST)
    loadParameters.topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(loadParameters.request.url().host().toString());
#endif
    const bool checkAssumedReadAccessToResourceURL = false;
    maybeInitializeSandboxExtensionHandle(m_process, fileURL, resourceDirectoryURL, loadParameters.sandboxExtensionHandle, checkAssumedReadAccessToResourceURL);
    addPlatformLoadParameters(m_process, loadParameters);

    m_process->markProcessAsRecentlyUsed();
    if (m_process->isLaunching())
        send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(loadParameters, resourceDirectoryURL, m_identifier, checkAssumedReadAccessToResourceURL));
    else
        send(Messages::WebPage::LoadRequest(loadParameters));
    m_process->startResponsivenessTimer();

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadData(const IPC::DataReference& data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadData:");

#if ENABLE(APP_BOUND_DOMAINS)
    if (MIMEType == "text/html"_s && !isFullWebBrowser())
        m_limitsNavigationsToAppBoundDomains = true;
#endif

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadData: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess())
        launchProcess({ }, ProcessLaunchReason::InitialProcess);

    auto navigation = m_navigationState->createLoadDataNavigation(makeUnique<API::SubstituteData>(Vector(data), MIMEType, encoding, baseURL, userData));

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

    loadDataWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, data, MIMEType, encoding, baseURL, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain(), std::nullopt, shouldOpenExternalURLsPolicy, SubstituteData::SessionHistoryVisibility::Hidden);
    return navigation;
}

void WebPageProxy::loadDataWithNavigationShared(Ref<WebProcessProxy>&& process, WebCore::PageIdentifier webPageID, API::Navigation& navigation, const IPC::DataReference& data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadDataWithNavigation");

    ASSERT(!m_isClosed);

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequest(transaction, { navigation.navigationID(), !baseURL.isEmpty() ? baseURL : aboutBlankURL().string() });

    LoadParameters loadParameters;
    loadParameters.sessionHistoryVisibility = sessionHistoryVisibility;
    loadParameters.navigationID = navigation.navigationID();
    loadParameters.data = data;
    loadParameters.MIMEType = MIMEType;
    loadParameters.encodingName = encoding;
    loadParameters.baseURLString = baseURL;
    loadParameters.shouldTreatAsContinuingLoad = shouldTreatAsContinuingLoad;
    loadParameters.userData = UserData(process->transformObjectsToHandles(userData).get());
    loadParameters.websitePolicies = WTFMove(websitePolicies);
    loadParameters.shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    loadParameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    loadParameters.isServiceWorkerLoad = isServiceWorkerPage();
    addPlatformLoadParameters(process, loadParameters);

    process->markProcessAsRecentlyUsed();
    process->assumeReadAccessToBaseURL(*this, baseURL);
    process->send(Messages::WebPage::LoadData(loadParameters), webPageID);
    process->startResponsivenessTimer();
}

RefPtr<API::Navigation> WebPageProxy::loadSimulatedRequest(WebCore::ResourceRequest&& simulatedRequest, WebCore::ResourceResponse&& simulatedResponse, const IPC::DataReference& data)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadSimulatedRequest:");

#if PLATFORM(COCOA)
    setLastNavigationWasAppInitiated(simulatedRequest);
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    if (simulatedResponse.mimeType() == "text/html"_s && !isFullWebBrowser())
        m_limitsNavigationsToAppBoundDomains = true;
#endif

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadSimulatedRequest: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess())
        launchProcess(RegistrableDomain { simulatedRequest.url() }, ProcessLaunchReason::InitialProcess);

    auto navigation = m_navigationState->createSimulatedLoadWithDataNavigation(ResourceRequest(simulatedRequest), makeUnique<API::SubstituteData>(Vector(data), ResourceResponse(simulatedResponse), WebCore::SubstituteData::SessionHistoryVisibility::Visible), m_backForwardList->currentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

    auto transaction = m_pageLoadState.transaction();

    auto baseURL = simulatedRequest.url().string();
    simulatedResponse.setURL(simulatedRequest.url()); // These should always match for simulated load

    m_pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), !baseURL.isEmpty() ? baseURL : aboutBlankURL().string() });

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTFMove(simulatedRequest);
    loadParameters.data = data;
    loadParameters.MIMEType = simulatedResponse.mimeType();
    loadParameters.encodingName = simulatedResponse.textEncodingName();
    loadParameters.baseURLString = baseURL;
    loadParameters.shouldOpenExternalURLsPolicy = WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    loadParameters.shouldTreatAsContinuingLoad = ShouldTreatAsContinuingLoad::No;
    loadParameters.lockHistory = navigation->lockHistory();
    loadParameters.lockBackForwardList = navigation->lockBackForwardList();
    loadParameters.clientRedirectSourceForHistory = navigation->clientRedirectSourceForHistory();
    loadParameters.effectiveSandboxFlags = navigation->effectiveSandboxFlags();
    loadParameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain();

    simulatedResponse.setExpectedContentLength(data.size());
    simulatedResponse.includeCertificateInfo();

    addPlatformLoadParameters(m_process, loadParameters);

    m_process->markProcessAsRecentlyUsed();
    m_process->assumeReadAccessToBaseURL(*this, baseURL);
    m_process->send(Messages::WebPage::LoadSimulatedRequestAndResponse(loadParameters, simulatedResponse), m_webPageID);
    m_process->startResponsivenessTimer();
    return navigation;
}

void WebPageProxy::loadAlternateHTML(Ref<WebCore::DataSegment>&& htmlData, const String& encoding, const URL& baseURL, const URL& unreachableURL, API::Object* userData)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadAlternateHTML");

    // When the UIProcess is in the process of handling a failing provisional load, do not attempt to
    // start a second alternative HTML load as this will prevent the page load state from being
    // handled properly.
    if (m_isClosed || m_isLoadingAlternateHTMLStringForFailingProvisionalLoad) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadAlternateHTML: page is closed (or other)");
        return;
    }

    if (!m_failingProvisionalLoadURL.isEmpty())
        m_isLoadingAlternateHTMLStringForFailingProvisionalLoad = true;

    if (!hasRunningProcess())
        launchProcess(RegistrableDomain { baseURL }, ProcessLaunchReason::InitialProcess);

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setPendingAPIRequest(transaction, { 0, unreachableURL.string() });
    m_pageLoadState.setUnreachableURL(transaction, unreachableURL.string());

    if (m_mainFrame)
        m_mainFrame->setUnreachableURL(unreachableURL);

    LoadParameters loadParameters;
    loadParameters.navigationID = 0;
    loadParameters.MIMEType = "text/html"_s;
    loadParameters.encodingName = encoding;
    loadParameters.baseURLString = baseURL.string();
    loadParameters.unreachableURLString = unreachableURL.string();
    loadParameters.provisionalLoadErrorURLString = m_failingProvisionalLoadURL;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(process(), loadParameters);

    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(m_process->coreProcessIdentifier(), RegistrableDomain(baseURL)), [this, protectedThis = Ref { *this }, process = m_process, loadParameters = WTFMove(loadParameters), baseURL, unreachableURL, htmlData = WTFMove(htmlData)] () mutable {
        loadParameters.data = { htmlData->data(), htmlData->size() };
        process->markProcessAsRecentlyUsed();
        process->assumeReadAccessToBaseURL(*this, baseURL.string());
        process->assumeReadAccessToBaseURL(*this, unreachableURL.string());
        if (baseURL.isLocalFile())
            process->addPreviouslyApprovedFileURL(baseURL);
        if (unreachableURL.isLocalFile())
            process->addPreviouslyApprovedFileURL(unreachableURL);
        send(Messages::WebPage::LoadAlternateHTML(loadParameters));
        process->startResponsivenessTimer();
    });
}

void WebPageProxy::loadWebArchiveData(API::Data* webArchiveData, API::Object* userData)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadWebArchiveData:");

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadWebArchiveData: page is closed");
        return;
    }

    if (!hasRunningProcess())
        launchProcess({ }, ProcessLaunchReason::InitialProcess);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setPendingAPIRequest(transaction, { 0, aboutBlankURL().string() });

    LoadParameters loadParameters;
    loadParameters.navigationID = 0;
    loadParameters.data = webArchiveData->dataReference();
    loadParameters.MIMEType = "application/x-webarchive"_s;
    loadParameters.encodingName = "utf-16"_s;
    loadParameters.userData = UserData(process().transformObjectsToHandles(userData).get());
    addPlatformLoadParameters(process(), loadParameters);

    m_process->markProcessAsRecentlyUsed();
    send(Messages::WebPage::LoadData(loadParameters));
    m_process->startResponsivenessTimer();
}

void WebPageProxy::navigateToPDFLinkWithSimulatedClick(const String& urlString, IntPoint documentPoint, IntPoint screenPoint)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "navigateToPDFLinkWithSimulatedClick:");

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "navigateToPDFLinkWithSimulatedClick: page is closed:");
        return;
    }

    if (WTF::protocolIsJavaScript(urlString))
        return;

    if (!hasRunningProcess())
        launchProcess(RegistrableDomain { URL { urlString } }, ProcessLaunchReason::InitialProcess);

    send(Messages::WebPage::NavigateToPDFLinkWithSimulatedClick(urlString, documentPoint, screenPoint));
    m_process->startResponsivenessTimer();
}

void WebPageProxy::stopLoading()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "stopLoading:");

    if (!hasRunningProcess()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "navigateToPDFLinkWithSimulatedClick: page is not valid");
        return;
    }

    send(Messages::WebPage::StopLoading());
    if (m_provisionalPage) {
        m_provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }
    m_process->startResponsivenessTimer();
}

RefPtr<API::Navigation> WebPageProxy::reload(OptionSet<WebCore::ReloadOption> options)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "reload:");

    // Make sure the Network & GPU processes are still responsive. This is so that reload() gets us out of the bad state if one of these
    // processes is hung.
    websiteDataStore().networkProcess().checkForResponsiveness();
#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcess = process().processPool().gpuProcess())
        gpuProcess->checkForResponsiveness();
#endif

    SandboxExtension::Handle sandboxExtensionHandle;

    String url = currentURL();
    if (!url.isEmpty()) {
        // We may not have an extension yet if back/forward list was reinstated after a WebProcess crash or a browser relaunch
        maybeInitializeSandboxExtensionHandle(m_process, URL { url }, currentResourceDirectoryURL(), sandboxExtensionHandle);
    }

    if (!hasRunningProcess())
        return launchProcessForReload();
    
    auto navigation = m_navigationState->createReloadNavigation(m_backForwardList->currentItem());

    if (!url.isEmpty()) {
        auto transaction = m_pageLoadState.transaction();
        m_pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), url });
    }

    // Store decision to reload without content blockers on the navigation so that we can later set the corresponding
    // WebsitePolicies flag in WebPageProxy::receivedNavigationPolicyDecision().
    if (options.contains(WebCore::ReloadOption::DisableContentBlockers))
        navigation->setUserContentExtensionsEnabled(false);

    m_process->markProcessAsRecentlyUsed();
    send(Messages::WebPage::Reload(navigation->navigationID(), options, sandboxExtensionHandle));
    m_process->startResponsivenessTimer();

#if ENABLE(SPEECH_SYNTHESIS)
    resetSpeechSynthesizer();
#endif

    return navigation;
}

void WebPageProxy::recordAutomaticNavigationSnapshot()
{
    if (m_shouldSuppressNextAutomaticNavigationSnapshot)
        return;

    if (WebBackForwardListItem* item = m_backForwardList->currentItem())
        recordNavigationSnapshot(*item);
}

void WebPageProxy::recordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (!m_shouldRecordNavigationSnapshots)
        return;

#if PLATFORM(COCOA) || PLATFORM(GTK)
    ViewSnapshotStore::singleton().recordSnapshot(*this, item);
#else
    UNUSED_PARAM(item);
#endif
}

RefPtr<API::Navigation> WebPageProxy::goForward()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goForward:");
    auto* forwardItem = m_backForwardList->goForwardItemSkippingItemsWithoutUserGesture();
    if (!forwardItem)
        return nullptr;

    return goToBackForwardItem(*forwardItem, FrameLoadType::Forward);
}

RefPtr<API::Navigation> WebPageProxy::goBack()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goBack:");
    auto* backItem = m_backForwardList->goBackItemSkippingItemsWithoutUserGesture();
    if (!backItem)
        return nullptr;

    return goToBackForwardItem(*backItem, FrameLoadType::Back);
}

RefPtr<API::Navigation> WebPageProxy::goToBackForwardItem(WebBackForwardListItem& item)
{
    return goToBackForwardItem(item, FrameLoadType::IndexedBackForward);
}

RefPtr<API::Navigation> WebPageProxy::goToBackForwardItem(WebBackForwardListItem& item, FrameLoadType frameLoadType)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goToBackForwardItem:");
    LOG(Loading, "WebPageProxy %p goToBackForwardItem to item URL %s", this, item.url().utf8().data());

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "goToBackForwardItem: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess()) {
        launchProcess(RegistrableDomain { URL { item.url() } }, ProcessLaunchReason::InitialProcess);

        if (&item != m_backForwardList->currentItem())
            m_backForwardList->goToItem(item);
    }

    RefPtr<API::Navigation> navigation;
    if (!m_backForwardList->currentItem()->itemIsInSameDocument(item))
        navigation = m_navigationState->createBackForwardNavigation(item, m_backForwardList->currentItem(), frameLoadType);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setPendingAPIRequest(transaction, { navigation ? navigation->navigationID() : 0, item.url() });

    m_process->markProcessAsRecentlyUsed();

    std::optional<String> topPrivatelyControlledDomain;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(item.url());
#endif
    
    send(Messages::WebPage::GoToBackForwardItem(navigation ? navigation->navigationID() : 0, item.itemID(), frameLoadType, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, std::nullopt, topPrivatelyControlledDomain));
    m_process->startResponsivenessTimer();

    return navigation;
}

void WebPageProxy::tryRestoreScrollPosition()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "tryRestoreScrollPosition:");

    if (!hasRunningProcess()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "tryRestoreScrollPosition: page is not valid");
        return;
    }

    send(Messages::WebPage::TryRestoreScrollPosition());
}

void WebPageProxy::didChangeBackForwardList(WebBackForwardListItem* added, Vector<Ref<WebBackForwardListItem>>&& removed)
{
    PageClientProtector protector(pageClient());

    if (!m_navigationClient->didChangeBackForwardList(*this, added, removed) && m_loaderClient)
        m_loaderClient->didChangeBackForwardList(*this, added, WTFMove(removed));

    auto transaction = m_pageLoadState.transaction();

    m_pageLoadState.setCanGoBack(transaction, m_backForwardList->backItem());
    m_pageLoadState.setCanGoForward(transaction, m_backForwardList->forwardItem());
}

void WebPageProxy::willGoToBackForwardListItem(const BackForwardItemIdentifier& itemID, bool inBackForwardCache)
{
    PageClientProtector protector(pageClient());

    if (auto* item = m_backForwardList->itemForID(itemID))
        m_navigationClient->willGoToBackForwardListItem(*this, *item, inBackForwardCache);
}

bool WebPageProxy::shouldKeepCurrentBackForwardListItemInList(WebBackForwardListItem& item)
{
    PageClientProtector protector(pageClient());

    return !m_loaderClient || m_loaderClient->shouldKeepCurrentBackForwardListItemInList(*this, item);
}

bool WebPageProxy::canShowMIMEType(const String& mimeType)
{
    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

    if (m_preferences->pdfJSViewerEnabled() && MIMETypeRegistry::isPDFMIMEType(mimeType))
        return true;

#if PLATFORM(COCOA)
    // On Mac, we can show PDFs.
    if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(mimeType) && !WebProcessPool::omitPDFSupport())
        return true;
#endif // PLATFORM(COCOA)

    return false;
}

void WebPageProxy::setControlledByAutomation(bool controlled)
{
    if (m_controlledByAutomation == controlled)
        return;

    m_controlledByAutomation = controlled;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetControlledByAutomation(controlled));
    websiteDataStore().networkProcess().send(Messages::NetworkProcess::SetSessionIsControlledByAutomation(m_websiteDataStore->sessionID(), m_controlledByAutomation), 0);
}

void WebPageProxy::createInspectorTarget(const String& targetId, Inspector::InspectorTargetType type)
{
    MESSAGE_CHECK(m_process, !targetId.isEmpty());
    m_inspectorController->createInspectorTarget(targetId, type);
}

void WebPageProxy::destroyInspectorTarget(const String& targetId)
{
    MESSAGE_CHECK(m_process, !targetId.isEmpty());
    m_inspectorController->destroyInspectorTarget(targetId);
}

void WebPageProxy::sendMessageToInspectorFrontend(const String& targetId, const String& message)
{
    m_inspectorController->sendMessageToInspectorFrontend(targetId, message);
}

#if ENABLE(REMOTE_INSPECTOR)
void WebPageProxy::setIndicating(bool indicating)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetIndicating(indicating));
}

bool WebPageProxy::inspectable() const
{
    return m_inspectorDebuggable->inspectable();
}

void WebPageProxy::setInspectable(bool inspectable)
{
    m_inspectorDebuggable->setInspectable(inspectable);
}

String WebPageProxy::remoteInspectionNameOverride() const
{
    return m_inspectorDebuggable->nameOverride();
}

void WebPageProxy::setRemoteInspectionNameOverride(const String& name)
{
    m_inspectorDebuggable->setNameOverride(name);
}

void WebPageProxy::remoteInspectorInformationDidChange()
{
    m_inspectorDebuggable->update();
}
#endif

void WebPageProxy::setBackgroundColor(const std::optional<Color>& color)
{
    if (m_backgroundColor == color)
        return;

    m_backgroundColor = color;
    if (hasRunningProcess())
        send(Messages::WebPage::SetBackgroundColor(color));
}

void WebPageProxy::setTopContentInset(float contentInset)
{
    if (m_topContentInset == contentInset)
        return;

    m_topContentInset = contentInset;

    pageClient().topContentInsetDidChange();

    if (!hasRunningProcess())
        return;
#if PLATFORM(COCOA)
    send(Messages::WebPage::SetTopContentInsetFenced(contentInset, m_drawingArea->createFence()));
#else
    send(Messages::WebPage::SetTopContentInset(contentInset));
#endif
}

void WebPageProxy::setUnderlayColor(const Color& color)
{
    if (m_underlayColor == color)
        return;

    m_underlayColor = color;

    if (hasRunningProcess())
        send(Messages::WebPage::SetUnderlayColor(color));
}

Color WebPageProxy::underPageBackgroundColor() const
{
    if (m_underPageBackgroundColorOverride.isValid())
        return m_underPageBackgroundColorOverride;

    if (m_pageExtendedBackgroundColor.isValid())
        return m_pageExtendedBackgroundColor;

    return platformUnderPageBackgroundColor();
}

void WebPageProxy::setUnderPageBackgroundColorOverride(Color&& newUnderPageBackgroundColorOverride)
{
    if (newUnderPageBackgroundColorOverride == m_underPageBackgroundColorOverride)
        return;

    auto oldUnderPageBackgroundColor = underPageBackgroundColor();
    auto oldUnderPageBackgroundColorOverride = std::exchange(m_underPageBackgroundColorOverride, newUnderPageBackgroundColorOverride);
    bool changesUnderPageBackgroundColor = !equalIgnoringSemanticColor(oldUnderPageBackgroundColor, underPageBackgroundColor());
    m_underPageBackgroundColorOverride = WTFMove(oldUnderPageBackgroundColorOverride);

    if (changesUnderPageBackgroundColor)
        pageClient().underPageBackgroundColorWillChange();

    m_underPageBackgroundColorOverride = WTFMove(newUnderPageBackgroundColorOverride);

    if (changesUnderPageBackgroundColor)
        pageClient().underPageBackgroundColorDidChange();

    if (m_hasPendingUnderPageBackgroundColorOverrideToDispatch)
        return;

    m_hasPendingUnderPageBackgroundColorOverrideToDispatch = true;

    RunLoop::main().dispatch([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        if (!m_hasPendingUnderPageBackgroundColorOverrideToDispatch)
            return;

        m_hasPendingUnderPageBackgroundColorOverrideToDispatch = false;

        if (m_pageClient)
            m_pageClient->didChangeBackgroundColor();

        if (hasRunningProcess())
            send(Messages::WebPage::SetUnderPageBackgroundColorOverride(m_underPageBackgroundColorOverride));
    });
}

void WebPageProxy::viewWillStartLiveResize()
{
    if (!hasRunningProcess())
        return;

    closeOverlayedViews();
    send(Messages::WebPage::ViewWillStartLiveResize());
}

void WebPageProxy::viewWillEndLiveResize()
{
    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::ViewWillEndLiveResize());
}

void WebPageProxy::setViewNeedsDisplay(const Region& region)
{
    pageClient().setViewNeedsDisplay(region);
}

void WebPageProxy::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, ScrollIsAnimated animated)
{
    pageClient().requestScroll(scrollPosition, scrollOrigin, animated);
}

WebCore::FloatPoint WebPageProxy::viewScrollPosition() const
{
    return pageClient().viewScrollPosition();
}

void WebPageProxy::setSuppressVisibilityUpdates(bool flag)
{
    if (m_suppressVisibilityUpdates == flag)
        return;

    WEBPAGEPROXY_RELEASE_LOG(ViewState, "setSuppressVisibilityUpdates: %d", flag);
    m_suppressVisibilityUpdates = flag;

    if (!m_suppressVisibilityUpdates) {
#if PLATFORM(COCOA)
        scheduleActivityStateUpdate();
#else
        dispatchActivityStateChange();
#endif
    }
}

void WebPageProxy::updateActivityState(OptionSet<ActivityState::Flag> flagsToUpdate)
{
    bool wasVisible = isViewVisible();
    m_activityState.remove(flagsToUpdate);
    if (flagsToUpdate & ActivityState::IsFocused && pageClient().isViewFocused())
        m_activityState.add(ActivityState::IsFocused);
    if (flagsToUpdate & ActivityState::WindowIsActive && pageClient().isViewWindowActive())
        m_activityState.add(ActivityState::WindowIsActive);
    if (flagsToUpdate & ActivityState::IsVisible) {
        bool isNowVisible = pageClient().isViewVisible();
        if (isNowVisible)
            m_activityState.add(ActivityState::IsVisible);
        if (wasVisible != isNowVisible)
            WEBPAGEPROXY_RELEASE_LOG(ViewState, "updateActivityState: view visibility state changed %d -> %d", wasVisible, isNowVisible);
    }
    if (flagsToUpdate & ActivityState::IsVisibleOrOccluded && pageClient().isViewVisibleOrOccluded())
        m_activityState.add(ActivityState::IsVisibleOrOccluded);
    if (flagsToUpdate & ActivityState::IsInWindow && pageClient().isViewInWindow())
        m_activityState.add(ActivityState::IsInWindow);
    bool isVisuallyIdle = pageClient().isVisuallyIdle();
#if PLATFORM(COCOA) && !HAVE(CGS_FIX_FOR_RADAR_97530095) && ENABLE(MEDIA_USAGE)
    if (pageClient().isViewVisible() && m_mediaUsageManager && m_mediaUsageManager->isPlayingVideoInViewport())
        isVisuallyIdle = false;
#endif
    if (flagsToUpdate & ActivityState::IsVisuallyIdle && isVisuallyIdle)
        m_activityState.add(ActivityState::IsVisuallyIdle);
    if (flagsToUpdate & ActivityState::IsAudible && m_mediaState.contains(MediaProducerMediaState::IsPlayingAudio) && !(m_mutedState.contains(MediaProducerMutedState::AudioIsMuted)))
        m_activityState.add(ActivityState::IsAudible);
    if (flagsToUpdate & ActivityState::IsLoading && m_pageLoadState.isLoading())
        m_activityState.add(ActivityState::IsLoading);
    if (flagsToUpdate & ActivityState::IsCapturingMedia && m_mediaState.containsAny(MediaProducer::ActiveCaptureMask))
        m_activityState.add(ActivityState::IsCapturingMedia);
}

void WebPageProxy::activityStateDidChange(OptionSet<ActivityState::Flag> mayHaveChanged, ActivityStateChangeDispatchMode dispatchMode, ActivityStateChangeReplyMode replyMode)
{
    LOG_WITH_STREAM(ActivityState, stream << "WebPageProxy " << identifier() << " activityStateDidChange - mayHaveChanged " << mayHaveChanged);

    m_potentiallyChangedActivityStateFlags.add(mayHaveChanged);
    m_activityStateChangeWantsSynchronousReply = m_activityStateChangeWantsSynchronousReply || replyMode == ActivityStateChangeReplyMode::Synchronous;

    // We need to do this here instead of inside dispatchActivityStateChange() or viewIsBecomingVisible() because these don't run when the view doesn't
    // have a running WebProcess. For the same reason, we need to rely on PageClient::isViewVisible() instead of WebPageProxy::isViewVisible().
    if (m_potentiallyChangedActivityStateFlags & ActivityState::IsVisible && m_shouldReloadDueToCrashWhenVisible && pageClient().isViewVisible()) {
        RunLoop::main().dispatch([this, weakThis = WeakPtr { *this }] {
            if (weakThis && std::exchange(m_shouldReloadDueToCrashWhenVisible, false)) {
                WEBPAGEPROXY_RELEASE_LOG(ViewState, "activityStateDidChange: view is becoming visible after a crash, attempt a reload");
                tryReloadAfterProcessTermination();
            }
        });
    }

    if (m_suppressVisibilityUpdates && dispatchMode != ActivityStateChangeDispatchMode::Immediate) {
        WEBPAGEPROXY_RELEASE_LOG(ViewState, "activityStateDidChange: Returning early due to m_suppressVisibilityUpdates");
        return;
    }

#if PLATFORM(COCOA)
    bool isNewlyInWindow = !isInWindow() && (mayHaveChanged & ActivityState::IsInWindow) && pageClient().isViewInWindow();
    if (dispatchMode == ActivityStateChangeDispatchMode::Immediate || isNewlyInWindow) {
        dispatchActivityStateChange();
        return;
    }
    scheduleActivityStateUpdate();
#else
    UNUSED_PARAM(dispatchMode);
    dispatchActivityStateChange();
#endif
}

void WebPageProxy::viewDidLeaveWindow()
{
    closeOverlayedViews();
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // When leaving the current page, close the video fullscreen.
    if (m_videoFullscreenManager && m_videoFullscreenManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard))
        m_videoFullscreenManager->requestHideAndExitFullscreen();
#endif
}

void WebPageProxy::viewDidEnterWindow()
{
    LayerHostingMode layerHostingMode = pageClient().viewLayerHostingMode();
    if (m_layerHostingMode != layerHostingMode) {
        m_layerHostingMode = layerHostingMode;
        send(Messages::WebPage::SetLayerHostingMode(layerHostingMode));
    }
}

void WebPageProxy::dispatchActivityStateChange()
{
#if PLATFORM(COCOA)
    if (m_activityStateChangeDispatcher->isScheduled())
        m_activityStateChangeDispatcher->invalidate();
    m_hasScheduledActivityStateUpdate = false;
#endif

    if (!hasRunningProcess())
        return;

    LOG_WITH_STREAM(ActivityState, stream << "WebPageProxy " << identifier() << " dispatchActivityStateChange - potentiallyChangedActivityStateFlags " << m_potentiallyChangedActivityStateFlags);

    // If the visibility state may have changed, then so may the visually idle & occluded agnostic state.
    if (m_potentiallyChangedActivityStateFlags & ActivityState::IsVisible)
        m_potentiallyChangedActivityStateFlags.add({ ActivityState::IsVisibleOrOccluded, ActivityState::IsVisuallyIdle });

    // Record the prior view state, update the flags that may have changed,
    // and check which flags have actually changed.
    auto previousActivityState = m_activityState;
    updateActivityState(m_potentiallyChangedActivityStateFlags);
    auto changed = m_activityState ^ previousActivityState;

    if (changed)
        LOG_WITH_STREAM(ActivityState, stream << "WebPageProxy " << identifier() << " dispatchActivityStateChange: state changed from " << previousActivityState << " to " << m_activityState);

    if ((changed & ActivityState::WindowIsActive) && isViewWindowActive())
        updateCurrentModifierState();

    if ((m_potentiallyChangedActivityStateFlags & ActivityState::IsVisible)) {
        if (isViewVisible())
            viewIsBecomingVisible();
        else
            m_process->pageIsBecomingInvisible(m_webPageID);
    }

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (m_potentiallyChangedActivityStateFlags & ActivityState::IsConnectedToHardwareConsole)
        isConnectedToHardwareConsoleDidChange();
#endif

    bool isNowInWindow = (changed & ActivityState::IsInWindow) && isInWindow();
    // We always want to wait for the Web process to reply if we've been in-window before and are coming back in-window.
    if (m_viewWasEverInWindow && isNowInWindow) {
        if (m_drawingArea->hasVisibleContent() && m_waitsForPaintAfterViewDidMoveToWindow && !m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow)
            m_activityStateChangeWantsSynchronousReply = true;
        m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow = false;
    }

    // Don't wait synchronously if the view state is not visible. (This matters in particular on iOS, where a hidden page may be suspended.)
    if (!(m_activityState & ActivityState::IsVisible))
        m_activityStateChangeWantsSynchronousReply = false;

    auto activityStateChangeID = m_activityStateChangeWantsSynchronousReply ? takeNextActivityStateChangeID() : static_cast<ActivityStateChangeID>(ActivityStateChangeAsynchronous);

    if (changed || activityStateChangeID != ActivityStateChangeAsynchronous || !m_nextActivityStateChangeCallbacks.isEmpty()) {
        sendWithAsyncReply(Messages::WebPage::SetActivityState(m_activityState, activityStateChangeID), [callbacks = std::exchange(m_nextActivityStateChangeCallbacks, { })] () mutable {
            for (auto& callback : callbacks)
                callback();
        });
    }

    // This must happen after the SetActivityState message is sent, to ensure the page visibility event can fire.
    updateThrottleState();

#if ENABLE(POINTER_LOCK)
    if (((changed & ActivityState::IsVisible) && !isViewVisible()) || ((changed & ActivityState::WindowIsActive) && !pageClient().isViewWindowActive())
        || ((changed & ActivityState::IsFocused) && !(m_activityState & ActivityState::IsFocused)))
        requestPointerUnlock();
#endif

    if (changed & ActivityState::IsVisible) {
        if (isViewVisible())
            m_visiblePageToken = m_process->visiblePageToken();
        else {
            m_visiblePageToken = nullptr;

            // If we've started the responsiveness timer as part of telling the web process to update the backing store
            // state, it might not send back a reply (since it won't paint anything if the web page is hidden) so we
            // stop the unresponsiveness timer here.
            m_process->stopResponsivenessTimer();
        }
    }

    if (changed & ActivityState::IsInWindow) {
        if (isInWindow())
            viewDidEnterWindow();
        else
            viewDidLeaveWindow();
    }

    updateBackingStoreDiscardableState();

    if (activityStateChangeID != ActivityStateChangeAsynchronous)
        waitForDidUpdateActivityState(activityStateChangeID);

    m_potentiallyChangedActivityStateFlags = { };
    m_activityStateChangeWantsSynchronousReply = false;
    m_viewWasEverInWindow |= isNowInWindow;

#if PLATFORM(COCOA)
    for (auto& callback : m_activityStateUpdateCallbacks)
        callback();
    m_activityStateUpdateCallbacks.clear();
#endif
}

void WebPageProxy::updateThrottleState()
{
    bool processSuppressionEnabled = m_preferences->pageVisibilityBasedProcessSuppressionEnabled();

    // If process suppression is not enabled take a token on the process pool to disable suppression of support processes.
    if (!processSuppressionEnabled)
        m_preventProcessSuppressionCount = m_process->processPool().processSuppressionDisabledForPageCount();
    else if (!m_preventProcessSuppressionCount)
        m_preventProcessSuppressionCount = nullptr;

    if (m_activityState & ActivityState::IsVisuallyIdle)
        m_pageIsUserObservableCount = nullptr;
    else if (!m_pageIsUserObservableCount)
        m_pageIsUserObservableCount = m_process->processPool().userObservablePageCount();

#if USE(RUNNINGBOARD)
    if (isViewVisible()) {
        if (!m_isVisibleActivity || !m_isVisibleActivity->isValid()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because the view is visible");
            m_isVisibleActivity = m_process->throttler().foregroundActivity("View is visible"_s).moveToUniquePtr();
        }
    } else if (m_isVisibleActivity) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because the view is no longer visible");
        m_isVisibleActivity = nullptr;
    }

    bool isAudible = m_activityState.contains(ActivityState::IsAudible);
    if (isAudible) {
        if (!m_isAudibleActivity || !m_isAudibleActivity->isValid()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because we are playing audio");
            m_isAudibleActivity = m_process->throttler().foregroundActivity("View is playing audio"_s).moveToUniquePtr();
        }
        m_audibleActivityTimer.stop();
    } else if (m_isAudibleActivity) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess will release a foreground assertion in %g seconds because we are no longer playing audio", audibleActivityClearDelay.seconds());
        if (!m_audibleActivityTimer.isActive())
            m_audibleActivityTimer.startOneShot(audibleActivityClearDelay);
    }

    bool isCapturingMedia = m_activityState.contains(ActivityState::IsCapturingMedia);
    if (isCapturingMedia) {
        if (!m_isCapturingActivity || !m_isCapturingActivity->isValid()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because media capture is active");
            m_isCapturingActivity = m_process->throttler().foregroundActivity("View is capturing media"_s).moveToUniquePtr();
        }
    } else if (m_isCapturingActivity) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because media capture is no longer active");
        m_isCapturingActivity = nullptr;
    }
#endif
}

#if USE(RUNNINGBOARD)
void WebPageProxy::clearAudibleActivity()
{
    WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because we are no longer playing audio");
    m_isAudibleActivity = nullptr;
}
#endif

void WebPageProxy::updateHiddenPageThrottlingAutoIncreases()
{
    if (!m_preferences->hiddenPageDOMTimerThrottlingAutoIncreases())
        m_hiddenPageDOMTimerThrottlingAutoIncreasesCount = nullptr;
    else if (!m_hiddenPageDOMTimerThrottlingAutoIncreasesCount)
        m_hiddenPageDOMTimerThrottlingAutoIncreasesCount = m_process->processPool().hiddenPageThrottlingAutoIncreasesCount();
}

void WebPageProxy::layerHostingModeDidChange()
{
    LayerHostingMode layerHostingMode = pageClient().viewLayerHostingMode();
    if (m_layerHostingMode == layerHostingMode)
        return;

    m_layerHostingMode = layerHostingMode;

    if (hasRunningProcess())
        send(Messages::WebPage::SetLayerHostingMode(layerHostingMode));
}

void WebPageProxy::waitForDidUpdateActivityState(ActivityStateChangeID activityStateChangeID)
{
    if (!hasRunningProcess())
        return;

    if (m_process->state() != WebProcessProxy::State::Running)
        return;

    // If we have previously timed out with no response from the WebProcess, don't block the UIProcess again until it starts responding.
    if (m_waitingForDidUpdateActivityState)
        return;

#if USE(RUNNINGBOARD)
    // Hail Mary check. Should not be possible (dispatchActivityStateChange should force async if not visible,
    // and if visible we should be holding an assertion) - but we should never block on a suspended process.
    if (!m_isVisibleActivity) {
        ASSERT_NOT_REACHED();
        return;
    }
#endif

    m_waitingForDidUpdateActivityState = true;

    m_drawingArea->waitForDidUpdateActivityState(activityStateChangeID);
}

IntSize WebPageProxy::viewSize() const
{
    return pageClient().viewSize();
}

void WebPageProxy::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& keyboardEvent, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::SetInitialFocus(forward, isKeyboardEventValid, keyboardEvent), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::setInitialFocus"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::clearSelection()
{
    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::ClearSelection());
}

void WebPageProxy::restoreSelectionInFocusedEditableElement()
{
    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::RestoreSelectionInFocusedEditableElement());
}

void WebPageProxy::validateCommand(const String& commandName, CompletionHandler<void(bool, int32_t)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction(false, 0);

    sendWithAsyncReply(Messages::WebPage::ValidateCommand(commandName), WTFMove(callbackFunction));
}

void WebPageProxy::increaseListLevel()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::IncreaseListLevel());
}

void WebPageProxy::decreaseListLevel()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DecreaseListLevel());
}

void WebPageProxy::changeListType()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ChangeListType());
}

void WebPageProxy::setBaseWritingDirection(WritingDirection direction)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetBaseWritingDirection(direction));
}

void WebPageProxy::updateFontAttributesAfterEditorStateChange()
{
    m_cachedFontAttributesAtSelectionStart.reset();

    if (!m_editorState.hasPostLayoutData())
        return;

    if (auto fontAttributes = m_editorState.postLayoutData->fontAttributes) {
        m_uiClient->didChangeFontAttributes(*fontAttributes);
        m_cachedFontAttributesAtSelectionStart = WTFMove(fontAttributes);
    }
}

void WebPageProxy::setNeedsFontAttributes(bool needsFontAttributes)
{
    if (m_needsFontAttributes == needsFontAttributes)
        return;

    m_needsFontAttributes = needsFontAttributes;

    if (hasRunningProcess())
        send(Messages::WebPage::SetNeedsFontAttributes(needsFontAttributes));
}

bool WebPageProxy::maintainsInactiveSelection() const
{
    // Regardless of what the client wants to do, keep selections if a local Inspector is open.
    // Otherwise, there is no way to use the console to inspect the state of a selection.
    if (inspector() && inspector()->isVisible())
        return true;

    return m_maintainsInactiveSelection;
}

void WebPageProxy::setMaintainsInactiveSelection(bool newValue)
{
    m_maintainsInactiveSelection = newValue;
}

void WebPageProxy::scheduleFullEditorStateUpdate()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ScheduleFullEditorStateUpdate());
}

void WebPageProxy::selectAll()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SelectAll());
}

static std::optional<DOMPasteAccessCategory> pasteAccessCategoryForCommand(const String& commandName)
{
    static NeverDestroyed<HashMap<String, DOMPasteAccessCategory, ASCIICaseInsensitiveHash>> pasteCommandNames = HashMap<String, DOMPasteAccessCategory, ASCIICaseInsensitiveHash> {
        { "Paste"_s, DOMPasteAccessCategory::General },
        { "PasteAndMatchStyle"_s, DOMPasteAccessCategory::General },
        { "PasteAsQuotation"_s, DOMPasteAccessCategory::General },
        { "PasteAsPlainText"_s, DOMPasteAccessCategory::General },
        { "PasteFont"_s, DOMPasteAccessCategory::Fonts },
    };

    auto it = pasteCommandNames->find(commandName);
    if (it != pasteCommandNames->end())
        return it->value;

    return std::nullopt;
}

void WebPageProxy::executeEditCommand(const String& commandName, const String& argument, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    if (auto pasteAccessCategory = pasteAccessCategoryForCommand(commandName))
        willPerformPasteCommand(*pasteAccessCategory);

    sendWithAsyncReply(Messages::WebPage::ExecuteEditCommandWithCallback(commandName, argument), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::executeEditCommand"_s)] () mutable {
        callbackFunction();
    });
}
    
void WebPageProxy::executeEditCommand(const String& commandName, const String& argument)
{
    static NeverDestroyed<String> ignoreSpellingCommandName(MAKE_STATIC_STRING_IMPL("ignoreSpelling"));

    if (!hasRunningProcess())
        return;

    if (auto pasteAccessCategory = pasteAccessCategoryForCommand(commandName))
        willPerformPasteCommand(*pasteAccessCategory);

    if (commandName == ignoreSpellingCommandName)
        ++m_pendingLearnOrIgnoreWordMessageCount;

    send(Messages::WebPage::ExecuteEditCommand(commandName, argument));
}

void WebPageProxy::requestFontAttributesAtSelectionStart(CompletionHandler<void(const WebCore::FontAttributes&)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ });

    if (auto attributes = m_cachedFontAttributesAtSelectionStart) {
        callback(*attributes);
        return;
    }

    sendWithAsyncReply(Messages::WebPage::RequestFontAttributesAtSelectionStart(), [this, protectedThis = Ref { *this }, callback = WTFMove(callback)] (const WebCore::FontAttributes& attributes) mutable {
        m_cachedFontAttributesAtSelectionStart = attributes;
        callback(attributes);
    });
}

void WebPageProxy::setEditable(bool editable)
{
    if (editable == m_isEditable)
        return;

    m_isEditable = editable;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetEditable(editable));
}
    
void WebPageProxy::setMediaStreamCaptureMuted(bool muted)
{
    auto state = m_mutedState;
    if (muted)
        state.add(WebCore::MediaProducer::MediaStreamCaptureIsMuted);
    else
        state.remove(WebCore::MediaProducer::MediaStreamCaptureIsMuted);
    setMuted(state);
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void WebPageProxy::isConnectedToHardwareConsoleDidChange()
{
    SetForScope<bool> isProcessing(m_isProcessingIsConnectedToHardwareConsoleDidChangeNotification, true);
    if (m_process->isConnectedToHardwareConsole()) {
        if (m_captureWasMutedDueToDisconnectedHardwareConsole)
            setMediaStreamCaptureMuted(false);

        m_captureWasMutedDueToDisconnectedHardwareConsole = false;
        return;
    }

    if (m_mutedState.containsAny(WebCore::MediaProducer::MediaStreamCaptureIsMuted))
        return;

    m_captureWasMutedDueToDisconnectedHardwareConsole = true;
    setMediaStreamCaptureMuted(true);
}
#endif

bool WebPageProxy::isAllowedToChangeMuteState() const
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    return m_isProcessingIsConnectedToHardwareConsoleDidChangeNotification || m_process->isConnectedToHardwareConsole();
#else
    return true;
#endif
}

void WebPageProxy::activateMediaStreamCaptureInPage()
{
#if ENABLE(MEDIA_STREAM)
    WebProcessProxy::muteCaptureInPagesExcept(m_webPageID);
#endif
    setMediaStreamCaptureMuted(false);
}

#if !PLATFORM(IOS_FAMILY)
void WebPageProxy::didCommitLayerTree(const RemoteLayerTreeTransaction&)
{
}

void WebPageProxy::layerTreeCommitComplete()
{
}
#endif

void WebPageProxy::didUpdateRenderingAfterCommittingLoad()
{
    if (m_hasUpdatedRenderingAfterDidCommitLoad)
        return;

    m_hasUpdatedRenderingAfterDidCommitLoad = true;
    stopMakingViewBlankDueToLackOfRenderingUpdateIfNecessary();
}

void WebPageProxy::stopMakingViewBlankDueToLackOfRenderingUpdateIfNecessary()
{
    if (!m_madeViewBlankDueToLackOfRenderingUpdate)
        return;

    ASSERT(m_hasUpdatedRenderingAfterDidCommitLoad);
    WEBPAGEPROXY_RELEASE_LOG(Process, "stopMakingViewBlankDueToLackOfRenderingUpdateIfNecessary:");
    pageClient().makeViewBlank(false);
    m_madeViewBlankDueToLackOfRenderingUpdate = false;
}

// If we have not painted yet since the last load commit, then we are likely still displaying the previous page.
// Displaying a JS prompt for the new page with the old page behind would be confusing so we make the view blank
// until the next paint in such case.
void WebPageProxy::makeViewBlankIfUnpaintedSinceLastLoadCommit()
{
    if (!m_hasUpdatedRenderingAfterDidCommitLoad) {
#if PLATFORM(COCOA)
        static bool shouldMakeViewBlank = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::BlanksViewOnJSPrompt);
#else
        static bool shouldMakeViewBlank = true;
#endif
        if (shouldMakeViewBlank) {
            WEBPAGEPROXY_RELEASE_LOG(Process, "makeViewBlankIfUnpaintedSinceLastLoadCommit: Making the view blank because of a JS prompt before the first paint for its page");
            pageClient().makeViewBlank(true);
            m_madeViewBlankDueToLackOfRenderingUpdate = true;
        }
    }
}

void WebPageProxy::discardQueuedMouseEvents()
{
    while (m_mouseEventQueue.size() > 1)
        m_mouseEventQueue.removeLast();
}

#if ENABLE(DRAG_SUPPORT)
void WebPageProxy::dragEntered(DragData& dragData, const String& dragStorageName)
{
#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().grantAccessToCurrentTypes(m_process.get(), dragStorageName);
#endif
    launchInitialProcessIfNecessary();
    performDragControllerAction(DragControllerAction::Entered, dragData, dragStorageName, { }, { });
}

void WebPageProxy::dragUpdated(DragData& dragData, const String& dragStorageName)
{
#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().grantAccessToCurrentTypes(m_process.get(), dragStorageName);
#endif
    performDragControllerAction(DragControllerAction::Updated, dragData, dragStorageName, { }, { });
}

void WebPageProxy::dragExited(DragData& dragData, const String& dragStorageName)
{
    performDragControllerAction(DragControllerAction::Exited, dragData, dragStorageName, { }, { });
}

void WebPageProxy::performDragOperation(DragData& dragData, const String& dragStorageName, SandboxExtension::Handle&& sandboxExtensionHandle, Vector<SandboxExtension::Handle>&& sandboxExtensionsForUpload)
{
#if PLATFORM(COCOA)
    grantAccessToCurrentPasteboardData(dragStorageName);
#endif
    performDragControllerAction(DragControllerAction::PerformDragOperation, dragData, dragStorageName, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionsForUpload));
}

void WebPageProxy::performDragControllerAction(DragControllerAction action, DragData& dragData, const String& dragStorageName, SandboxExtension::Handle&& sandboxExtensionHandle, Vector<SandboxExtension::Handle>&& sandboxExtensionsForUpload)
{
    if (!hasRunningProcess())
        return;
#if PLATFORM(GTK)
    UNUSED_PARAM(dragStorageName);
    UNUSED_PARAM(sandboxExtensionHandle);
    UNUSED_PARAM(sandboxExtensionsForUpload);

    String url = dragData.asURL();
    if (!url.isEmpty())
        m_process->assumeReadAccessToBaseURL(*this, url);

    ASSERT(dragData.platformData());
    send(Messages::WebPage::PerformDragControllerAction(action, dragData.clientPosition(), dragData.globalPosition(), dragData.draggingSourceOperationMask(), *dragData.platformData(), dragData.flags()));
#else
    send(Messages::WebPage::PerformDragControllerAction(action, dragData, sandboxExtensionHandle, sandboxExtensionsForUpload));
#endif
}

void WebPageProxy::didPerformDragControllerAction(std::optional<WebCore::DragOperation> dragOperation, WebCore::DragHandlingMethod dragHandlingMethod, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted, const IntRect& insertionRect, const IntRect& editableElementRect)
{
    m_currentDragOperation = dragOperation;
    m_currentDragHandlingMethod = dragHandlingMethod;
    m_currentDragIsOverFileInput = mouseIsOverFileInput;
    m_currentDragNumberOfFilesToBeAccepted = numberOfItemsToBeAccepted;
    m_currentDragCaretEditableElementRect = editableElementRect;
    setDragCaretRect(insertionRect);
    pageClient().didPerformDragControllerAction();
}

#if PLATFORM(GTK)
void WebPageProxy::startDrag(SelectionData&& selectionData, OptionSet<WebCore::DragOperation> dragOperationMask, const ShareableBitmapHandle& dragImageHandle, IntPoint&& dragImageHotspot)
{
    RefPtr<ShareableBitmap> dragImage = !dragImageHandle.isNull() ? ShareableBitmap::create(dragImageHandle) : nullptr;
    pageClient().startDrag(WTFMove(selectionData), dragOperationMask, WTFMove(dragImage), WTFMove(dragImageHotspot));

    didStartDrag();
}
#endif

void WebPageProxy::dragEnded(const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<WebCore::DragOperation> dragOperationMask)
{
    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::DragEnded(clientPosition, globalPosition, dragOperationMask));
    setDragCaretRect({ });
}

void WebPageProxy::didPerformDragOperation(bool handled)
{
    pageClient().didPerformDragOperation(handled);
}

void WebPageProxy::didStartDrag()
{
    if (!hasRunningProcess())
        return;

    discardQueuedMouseEvents();
    send(Messages::WebPage::DidStartDrag());
}
    
void WebPageProxy::dragCancelled()
{
    if (hasRunningProcess())
        send(Messages::WebPage::DragCancelled());
}

void WebPageProxy::didEndDragging()
{
    resetCurrentDragInformation();
}

void WebPageProxy::resetCurrentDragInformation()
{
    m_currentDragOperation = std::nullopt;
    m_currentDragHandlingMethod = DragHandlingMethod::None;
    m_currentDragIsOverFileInput = false;
    m_currentDragNumberOfFilesToBeAccepted = 0;
    setDragCaretRect({ });
}

#if !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)

void WebPageProxy::setDragCaretRect(const IntRect& dragCaretRect)
{
    m_currentDragCaretRect = dragCaretRect;
}

#endif

#endif // ENABLE(DRAG_SUPPORT)

static bool removeOldRedundantEvent(Deque<NativeWebMouseEvent>& queue, WebEvent::Type incomingEventType)
{
    if (incomingEventType != WebEvent::MouseMove && incomingEventType != WebEvent::MouseForceChanged)
        return false;

    auto it = queue.rbegin();
    auto end = queue.rend();

    // Must not remove the first event in the deque, since it is already being dispatched.
    if (it != end)
        --end;

    for (; it != end; ++it) {
        auto type = it->type();
        if (type == incomingEventType) {
            queue.remove(--it.base());
            return true;
        }
        if (type != WebEvent::MouseMove && type != WebEvent::MouseForceChanged)
            break;
    }
    return false;
}

void WebPageProxy::handleMouseEvent(const NativeWebMouseEvent& event)
{
    if (event.type() == WebEvent::MouseDown)
        launchInitialProcessIfNecessary();

    if (!hasRunningProcess())
        return;

#if ENABLE(CONTEXT_MENU_EVENT)
    if (event.button() == WebMouseEventButton::RightButton && event.type() == WebEvent::MouseDown) {
        ASSERT(m_contextMenuPreventionState != EventPreventionState::Waiting);
        m_contextMenuPreventionState = EventPreventionState::Waiting;
    }
#endif

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (m_scrollingCoordinatorProxy)
        m_scrollingCoordinatorProxy->handleMouseEvent(platform(event));
#endif

    // If we receive multiple mousemove or mouseforcechanged events and the most recent mousemove or mouseforcechanged event
    // (respectively) has not yet been sent to WebProcess for processing, remove the pending mouse event and insert the new
    // event in the queue.
    bool didRemoveEvent = removeOldRedundantEvent(m_mouseEventQueue, event.type());
    m_mouseEventQueue.append(event);

#if LOG_DISABLED
    UNUSED_PARAM(didRemoveEvent);
#else
    LOG(MouseHandling, "UIProcess: %s mouse event %s (queue size %zu)", didRemoveEvent ? "replaced" : "enqueued", webMouseEventTypeString(event.type()), m_mouseEventQueue.size());
#endif

    if (m_mouseEventQueue.size() == 1) // Otherwise, called from DidReceiveEvent message handler.
        processNextQueuedMouseEvent();
}
    
void WebPageProxy::processNextQueuedMouseEvent()
{
    if (!hasRunningProcess())
        return;

    ASSERT(!m_mouseEventQueue.isEmpty());

    const NativeWebMouseEvent& event = m_mouseEventQueue.first();

    if (pageClient().windowIsFrontWindowUnderMouse(event))
        setToolTip(String());

    WebEvent::Type eventType = event.type();
    if (eventType == WebEvent::MouseDown || eventType == WebEvent::MouseForceChanged || eventType == WebEvent::MouseForceDown)
        m_process->startResponsivenessTimer(WebProcessProxy::UseLazyStop::Yes);
    else if (eventType != WebEvent::MouseMove) {
        // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
        m_process->startResponsivenessTimer();
    }

    std::optional<Vector<SandboxExtension::Handle>> sandboxExtensions;

#if PLATFORM(MAC)
    bool eventMayStartDrag = !m_currentDragOperation && eventType == WebEvent::MouseMove && event.button() != WebMouseEventButton::NoButton;
    if (eventMayStartDrag)
        sandboxExtensions = SandboxExtension::createHandlesForMachLookup({ "com.apple.iconservices"_s, "com.apple.iconservices.store"_s }, process().auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
#endif

    LOG(MouseHandling, "UIProcess: sent mouse event %s (queue size %zu)", webMouseEventTypeString(eventType), m_mouseEventQueue.size());
    send(Messages::WebPage::MouseEvent(event, sandboxExtensions));
}

void WebPageProxy::doAfterProcessingAllPendingMouseEvents(WTF::Function<void ()>&& action)
{
    if (!isProcessingMouseEvents()) {
        action();
        return;
    }

    m_callbackHandlersAfterProcessingPendingMouseEvents.append(WTFMove(action));
}

void WebPageProxy::didFinishProcessingAllPendingMouseEvents()
{
    flushPendingMouseEventCallbacks();
}

void WebPageProxy::flushPendingMouseEventCallbacks()
{
    for (auto& callback : m_callbackHandlersAfterProcessingPendingMouseEvents)
        callback();

    m_callbackHandlersAfterProcessingPendingMouseEvents.clear();
}

void WebPageProxy::dispatchWheelEventWithoutScrolling(const WebWheelEvent& event, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::DispatchWheelEventWithoutScrolling(event), WTFMove(completionHandler));
}

void WebPageProxy::handleWheelEvent(const NativeWebWheelEvent& event)
{
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (m_scrollingCoordinatorProxy && m_scrollingCoordinatorProxy->handleWheelEvent(platform(event)))
        return;
#endif

    if (!hasRunningProcess())
        return;

    closeOverlayedViews();

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    // FIXME: We should not have to look this up repeatedly, but it can also change occasionally.
    if (event.momentumPhase() == WebWheelEvent::PhaseBegan && preferences().momentumScrollingAnimatorEnabled())
        m_scrollingAccelerationCurve = ScrollingAccelerationCurve::fromNativeWheelEvent(event);
#endif

    if (wheelEventCoalescer().shouldDispatchEvent(event)) {
        auto event = wheelEventCoalescer().nextEventToDispatch();
        sendWheelEvent(*event);
    }
}

#if HAVE(CVDISPLAYLINK)
void WebPageProxy::wheelEventHysteresisUpdated(PAL::HysteresisState)
{
    updateDisplayLinkFrequency();
}

void WebPageProxy::updateDisplayLinkFrequency()
{
    if (!m_process->hasConnection() || !m_displayID)
        return;

    bool wantsFullSpeedUpdates = m_hasActiveAnimatedScroll || m_wheelEventActivityHysteresis.state() == PAL::HysteresisState::Started;
    if (wantsFullSpeedUpdates != m_registeredForFullSpeedUpdates) {
        process().processPool().setDisplayLinkForDisplayWantsFullSpeedUpdates(process(), *m_displayID, wantsFullSpeedUpdates);
        m_registeredForFullSpeedUpdates = wantsFullSpeedUpdates;
    }
}
#endif

void WebPageProxy::updateWheelEventActivityAfterProcessSwap()
{
#if HAVE(CVDISPLAYLINK)
    updateDisplayLinkFrequency();
#endif
}

void WebPageProxy::sendWheelEvent(const WebWheelEvent& event)
{
#if HAVE(CVDISPLAYLINK)
    m_wheelEventActivityHysteresis.impulse();
#endif

    auto* connection = messageSenderConnection();
    if (!connection)
        return;

    auto rubberBandableEdges = this->rubberBandableEdges();
    if (shouldUseImplicitRubberBandControl()) {
        rubberBandableEdges.setLeft(!m_backForwardList->backItem());
        rubberBandableEdges.setRight(!m_backForwardList->forwardItem());
    }

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (event.momentumPhase() == WebWheelEvent::PhaseBegan && m_scrollingAccelerationCurve != m_lastSentScrollingAccelerationCurve) {
        connection->send(Messages::EventDispatcher::SetScrollingAccelerationCurve(m_webPageID, m_scrollingAccelerationCurve), 0, { }, Thread::QOS::UserInteractive);
        m_lastSentScrollingAccelerationCurve = m_scrollingAccelerationCurve;
    }
#endif

    connection->send(Messages::EventDispatcher::WheelEvent(m_webPageID, event, rubberBandableEdges), 0, { }, Thread::QOS::UserInteractive);

    // Manually ping the web process to check for responsiveness since our wheel
    // event will dispatch to a non-main thread, which always responds.
    m_process->isResponsiveWithLazyStop();
}

WebWheelEventCoalescer& WebPageProxy::wheelEventCoalescer()
{
    if (!m_wheelEventCoalescer)
        m_wheelEventCoalescer = makeUnique<WebWheelEventCoalescer>();

    return *m_wheelEventCoalescer;
}

bool WebPageProxy::hasQueuedKeyEvent() const
{
    return !m_keyEventQueue.isEmpty();
}

const NativeWebKeyboardEvent& WebPageProxy::firstQueuedKeyEvent() const
{
    return m_keyEventQueue.first();
}

bool WebPageProxy::handleKeyboardEvent(const NativeWebKeyboardEvent& event)
{
    if (!hasRunningProcess())
        return false;
    
    LOG(KeyHandling, "WebPageProxy::handleKeyboardEvent: %s", webKeyboardEventTypeString(event.type()));

    m_keyEventQueue.append(event);

    m_process->startResponsivenessTimer(event.type() == WebEvent::KeyDown ? WebProcessProxy::UseLazyStop::Yes : WebProcessProxy::UseLazyStop::No);

    if (m_keyEventQueue.size() == 1) { // Otherwise, sent from DidReceiveEvent message handler.
        LOG(KeyHandling, " UI process: sent keyEvent from handleKeyboardEvent");
        send(Messages::WebPage::KeyEvent(event));
    }

    return true;
}

WebPreferencesStore WebPageProxy::preferencesStore() const
{
    return m_preferences->store();
}

#if ENABLE(TOUCH_EVENTS)

static TrackingType mergeTrackingTypes(TrackingType a, TrackingType b)
{
    if (static_cast<uintptr_t>(b) > static_cast<uintptr_t>(a))
        return b;
    return a;
}

void WebPageProxy::updateTouchEventTracking(const WebTouchEvent& touchStartEvent)
{
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        IntPoint location = touchPoint.location();
        auto updateTrackingType = [this, location](TrackingType& trackingType, EventTrackingRegions::EventType eventType) {
            if (trackingType == TrackingType::Synchronous)
                return;

            TrackingType trackingTypeForLocation = m_scrollingCoordinatorProxy->eventTrackingTypeForPoint(eventType, location);

            trackingType = mergeTrackingTypes(trackingType, trackingTypeForLocation);
        };
        updateTrackingType(m_touchAndPointerEventTracking.touchForceChangedTracking, EventTrackingRegions::EventType::Touchforcechange);
        updateTrackingType(m_touchAndPointerEventTracking.touchStartTracking, EventTrackingRegions::EventType::Touchstart);
        updateTrackingType(m_touchAndPointerEventTracking.touchMoveTracking, EventTrackingRegions::EventType::Touchmove);
        updateTrackingType(m_touchAndPointerEventTracking.touchEndTracking, EventTrackingRegions::EventType::Touchend);
        updateTrackingType(m_touchAndPointerEventTracking.touchStartTracking, EventTrackingRegions::EventType::Pointerover);
        updateTrackingType(m_touchAndPointerEventTracking.touchStartTracking, EventTrackingRegions::EventType::Pointerenter);
        updateTrackingType(m_touchAndPointerEventTracking.touchStartTracking, EventTrackingRegions::EventType::Pointerdown);
        updateTrackingType(m_touchAndPointerEventTracking.touchMoveTracking, EventTrackingRegions::EventType::Pointermove);
        updateTrackingType(m_touchAndPointerEventTracking.touchEndTracking, EventTrackingRegions::EventType::Pointerup);
        updateTrackingType(m_touchAndPointerEventTracking.touchEndTracking, EventTrackingRegions::EventType::Pointerout);
        updateTrackingType(m_touchAndPointerEventTracking.touchEndTracking, EventTrackingRegions::EventType::Pointerleave);
        updateTrackingType(m_touchAndPointerEventTracking.touchStartTracking, EventTrackingRegions::EventType::Mousedown);
        updateTrackingType(m_touchAndPointerEventTracking.touchMoveTracking, EventTrackingRegions::EventType::Mousemove);
        updateTrackingType(m_touchAndPointerEventTracking.touchEndTracking, EventTrackingRegions::EventType::Mouseup);
    }
#else
    UNUSED_PARAM(touchStartEvent);
    m_touchAndPointerEventTracking.touchForceChangedTracking = TrackingType::Synchronous;
    m_touchAndPointerEventTracking.touchStartTracking = TrackingType::Synchronous;
    m_touchAndPointerEventTracking.touchMoveTracking = TrackingType::Synchronous;
    m_touchAndPointerEventTracking.touchEndTracking = TrackingType::Synchronous;
#endif // ENABLE(ASYNC_SCROLLING)
}

TrackingType WebPageProxy::touchEventTrackingType(const WebTouchEvent& touchStartEvent) const
{
    // We send all events if any type is needed, we just do it asynchronously for the types that are not tracked.
    //
    // Touch events define a sequence with strong dependencies. For example, we can expect
    // a TouchMove to only appear after a TouchStart, and the ids of the touch points is consistent between
    // the two.
    //
    // WebCore should not have to set up its state correctly after some events were dismissed.
    // For example, we don't want to send a TouchMoved without a TouchPressed.
    // We send everything, WebCore updates its internal state and dispatch what is needed to the page.
    TrackingType globalTrackingType = m_touchAndPointerEventTracking.isTrackingAnything() ? TrackingType::Asynchronous : TrackingType::NotTracking;

    globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchAndPointerEventTracking.touchForceChangedTracking);
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        switch (touchPoint.state()) {
        case WebPlatformTouchPoint::TouchReleased:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchAndPointerEventTracking.touchEndTracking);
            break;
        case WebPlatformTouchPoint::TouchPressed:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchAndPointerEventTracking.touchStartTracking);
            break;
        case WebPlatformTouchPoint::TouchMoved:
        case WebPlatformTouchPoint::TouchStationary:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, m_touchAndPointerEventTracking.touchMoveTracking);
            break;
        case WebPlatformTouchPoint::TouchCancelled:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, TrackingType::Asynchronous);
            break;
        }
    }

    return globalTrackingType;
}

#endif

#if ENABLE(MAC_GESTURE_EVENTS)
void WebPageProxy::handleGestureEvent(const NativeWebGestureEvent& event)
{
    if (!hasRunningProcess())
        return;

    m_gestureEventQueue.append(event);
    // FIXME: Consider doing some coalescing here.

    m_process->startResponsivenessTimer((event.type() == WebEvent::GestureStart || event.type() == WebEvent::GestureChange) ? WebProcessProxy::UseLazyStop::Yes : WebProcessProxy::UseLazyStop::No);

    send(Messages::EventDispatcher::GestureEvent(m_webPageID, event), 0);
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPageProxy::handlePreventableTouchEvent(NativeWebTouchEvent& event)
{
    if (!hasRunningProcess())
        return;

    TraceScope scope(SyncTouchEventStart, SyncTouchEventEnd);

    updateTouchEventTracking(event);

    auto handleAllTouchPointsReleased = WTF::makeScopeExit([&] {
        if (!event.allTouchPointsAreReleased())
            return;

        m_touchAndPointerEventTracking.reset();
        didReleaseAllTouchPoints();
    });

    bool isTouchStart = event.type() == WebEvent::TouchStart;
    bool isTouchMove = event.type() == WebEvent::TouchMove;
    bool isTouchEnd = event.type() == WebEvent::TouchEnd;

    if (isTouchStart)
        m_touchMovePreventionState = EventPreventionState::None;

    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking) {
        if (isTouchStart)
            pageClient().doneDeferringTouchStart(false);
        if (isTouchMove)
            pageClient().doneDeferringTouchMove(false);
        if (isTouchEnd)
            pageClient().doneDeferringTouchEnd(false);
        return;
    }

    if (touchEventsTrackingType == TrackingType::Asynchronous) {
        // We can end up here if a native gesture has not started but the event handlers are passive.
        //
        // The client of WebPageProxy asks the event to be sent synchronously since the touch event
        // can prevent a native gesture.
        // But, here we know that all events handlers that can handle this events are passive.
        // We can use asynchronous dispatch and pretend to the client that the page does nothing with the events.
        event.setCanPreventNativeGestures(false);
        handleUnpreventableTouchEvent(event);
        didReceiveEvent(event.type(), false);
        if (isTouchStart)
            pageClient().doneDeferringTouchStart(false);
        if (isTouchMove)
            pageClient().doneDeferringTouchMove(false);
        if (isTouchEnd)
            pageClient().doneDeferringTouchEnd(false);
        return;
    }

    if (isTouchStart)
        ++m_handlingPreventableTouchStartCount;

    if (isTouchMove && m_touchMovePreventionState == EventPreventionState::None)
        m_touchMovePreventionState = EventPreventionState::Waiting;

    if (isTouchEnd)
        ++m_handlingPreventableTouchEndCount;

    sendWithAsyncReply(Messages::EventDispatcher::TouchEvent(m_webPageID, event), [this, weakThis = WeakPtr { *this }, event] (bool handled) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        bool didFinishDeferringTouchStart = false;
        ASSERT_IMPLIES(event.type() == WebEvent::TouchStart, m_handlingPreventableTouchStartCount);
        if (event.type() == WebEvent::TouchStart && m_handlingPreventableTouchStartCount)
            didFinishDeferringTouchStart = !--m_handlingPreventableTouchStartCount;

        bool didFinishDeferringTouchMove = false;
        if (event.type() == WebEvent::TouchMove && m_touchMovePreventionState == EventPreventionState::Waiting) {
            m_touchMovePreventionState = handled ? EventPreventionState::Prevented : EventPreventionState::Allowed;
            didFinishDeferringTouchMove = true;
        }

        bool didFinishDeferringTouchEnd = false;
        ASSERT_IMPLIES(event.type() == WebEvent::TouchEnd, m_handlingPreventableTouchEndCount);
        if (event.type() == WebEvent::TouchEnd && m_handlingPreventableTouchEndCount)
            didFinishDeferringTouchEnd = !--m_handlingPreventableTouchEndCount;

        didReceiveEvent(event.type(), handled);
        if (!m_pageClient)
            return;

        pageClient().doneWithTouchEvent(event, handled);

        if (didFinishDeferringTouchStart)
            pageClient().doneDeferringTouchStart(handled);

        if (didFinishDeferringTouchMove)
            pageClient().doneDeferringTouchMove(handled);

        if (didFinishDeferringTouchEnd)
            pageClient().doneDeferringTouchEnd(handled);
    });
}

void WebPageProxy::resetPotentialTapSecurityOrigin()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ResetPotentialTapSecurityOrigin());
}

void WebPageProxy::handleUnpreventableTouchEvent(const NativeWebTouchEvent& event)
{
    if (!hasRunningProcess())
        return;

    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking)
        return;

    send(Messages::EventDispatcher::TouchEventWithoutCallback(m_webPageID, event), 0);

    if (event.allTouchPointsAreReleased()) {
        m_touchAndPointerEventTracking.reset();
        didReleaseAllTouchPoints();
    }
}

#elif ENABLE(TOUCH_EVENTS)
void WebPageProxy::handleTouchEvent(const NativeWebTouchEvent& event)
{
    if (!hasRunningProcess())
        return;

    updateTouchEventTracking(event);

    if (touchEventTrackingType(event) == TrackingType::NotTracking)
        return;

    // If the page is suspended, which should be the case during panning, pinching
    // and animation on the page itself (kinetic scrolling, tap to zoom) etc, then
    // we do not send any of the events to the page even if is has listeners.
    if (!m_areActiveDOMObjectsAndAnimationsSuspended) {
        m_touchEventQueue.append(event);
        m_process->startResponsivenessTimer();
        send(Messages::WebPage::TouchEvent(event));
    } else {
        if (m_touchEventQueue.isEmpty()) {
            bool isEventHandled = false;
            pageClient().doneWithTouchEvent(event, isEventHandled);
        } else {
            // We attach the incoming events to the newest queued event so that all
            // the events are delivered in the correct order when the event is dequed.
            QueuedTouchEvents& lastEvent = m_touchEventQueue.last();
            lastEvent.deferredTouchEvents.append(event);
        }
    }

    if (event.allTouchPointsAreReleased()) {
        m_touchAndPointerEventTracking.reset();
        didReleaseAllTouchPoints();
    }
}
#endif // ENABLE(TOUCH_EVENTS)

void WebPageProxy::cancelPointer(WebCore::PointerID pointerId, const WebCore::IntPoint& documentPoint)
{
    send(Messages::WebPage::CancelPointer(pointerId, documentPoint));
}

void WebPageProxy::touchWithIdentifierWasRemoved(WebCore::PointerID pointerId)
{
    send(Messages::WebPage::TouchWithIdentifierWasRemoved(pointerId));
}

void WebPageProxy::scrollBy(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ScrollBy(direction, granularity));
}

void WebPageProxy::centerSelectionInVisibleArea()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::CenterSelectionInVisibleArea());
}

class WebPageProxy::PolicyDecisionSender : public RefCounted<PolicyDecisionSender> {
public:
    using SendFunction = CompletionHandler<void(PolicyDecision&&)>;

    static Ref<PolicyDecisionSender> create(PolicyCheckIdentifier identifier, SendFunction&& sendFunction)
    {
        return adoptRef(*new PolicyDecisionSender(identifier, WTFMove(sendFunction)));
    }

    void send(PolicyDecision&& policyDecision)
    {
        if (m_sendFunction)
            m_sendFunction(WTFMove(policyDecision));
    }

    PolicyCheckIdentifier identifier() { return m_identifier; }
    
private:
    PolicyDecisionSender(PolicyCheckIdentifier identifier, SendFunction sendFunction)
        : m_sendFunction(WTFMove(sendFunction))
        , m_identifier(identifier)
        { }

    SendFunction m_sendFunction;
    PolicyCheckIdentifier m_identifier;
};

#if ENABLE(APP_BOUND_DOMAINS)
static bool shouldTreatURLProtocolAsAppBound(const URL& requestURL, bool isRunningTest)
{
    return !isRunningTest
        && (SecurityOrigin::isLocalHostOrLoopbackIPAddress(requestURL.host())
            || requestURL.protocolIsAbout()
            || requestURL.protocolIsData()
            || requestURL.protocolIsBlob()
            || requestURL.isLocalFile()
            || requestURL.protocolIsJavaScript());
}

bool WebPageProxy::setIsNavigatingToAppBoundDomainAndCheckIfPermitted(bool isMainFrame, const URL& requestURL, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    if (isFullWebBrowser()) {
        if (hasProhibitedUsageStrings())
            m_isNavigatingToAppBoundDomain = NavigatingToAppBoundDomain::No;
        return true;
    }

    if (!isNavigatingToAppBoundDomain) {
        m_isNavigatingToAppBoundDomain = std::nullopt;
        return true;
    }
    if (m_ignoresAppBoundDomains)
        return true;

    if (isMainFrame && shouldTreatURLProtocolAsAppBound(requestURL, websiteDataStore().configuration().enableInAppBrowserPrivacyForTesting())) {
        isNavigatingToAppBoundDomain = NavigatingToAppBoundDomain::Yes;
        m_limitsNavigationsToAppBoundDomains = true;
    }
    if (m_limitsNavigationsToAppBoundDomains) {
        if (*isNavigatingToAppBoundDomain == NavigatingToAppBoundDomain::No) {
            if (isMainFrame)
                return false;
            m_isNavigatingToAppBoundDomain = NavigatingToAppBoundDomain::No;
            return true;
        }
        m_isNavigatingToAppBoundDomain = NavigatingToAppBoundDomain::Yes;
    } else {
        if (m_hasExecutedAppBoundBehaviorBeforeNavigation)
            return false;
        m_isNavigatingToAppBoundDomain = NavigatingToAppBoundDomain::No;
    }
    return true;
}

void WebPageProxy::isNavigatingToAppBoundDomainTesting(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_isNavigatingToAppBoundDomain && (*m_isNavigatingToAppBoundDomain == NavigatingToAppBoundDomain::Yes));
}

void WebPageProxy::isForcedIntoAppBoundModeTesting(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_limitsNavigationsToAppBoundDomains);
}
#endif

void WebPageProxy::disableServiceWorkerEntitlementInNetworkProcess()
{
#if ENABLE(APP_BOUND_DOMAINS) && !PLATFORM(MACCATALYST)
    websiteDataStore().networkProcess().send(Messages::NetworkProcess::DisableServiceWorkerEntitlement(), 0);
#endif
}

void WebPageProxy::clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS) && !PLATFORM(MACCATALYST)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    sendWithAsyncReply(Messages::WebPage::ClearServiceWorkerEntitlementOverride(), [callbackAggregator] { });
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::ClearServiceWorkerEntitlementOverride(), [callbackAggregator] { });
#else
    completionHandler();
#endif
}

void WebPageProxy::receivedNavigationPolicyDecision(PolicyAction policyAction, API::Navigation* navigation, Ref<API::NavigationAction>&& navigationAction, ProcessSwapRequestedByClient processSwapRequestedByClient, WebFrameProxy& frame, const FrameInfoData& frameInfo, Ref<PolicyDecisionSender>&& sender)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "receivedNavigationPolicyDecision: frameID=%llu, isMainFrame=%d, navigationID=%llu, policyAction=%u", frame.frameID().object().toUInt64(), frame.isMainFrame(), navigation ? navigation->navigationID() : 0, (unsigned)policyAction);

    Ref<WebsiteDataStore> websiteDataStore = m_websiteDataStore.copyRef();
    if (auto* policies = navigation->websitePolicies()) {
        if (policies->websiteDataStore() && policies->websiteDataStore() != websiteDataStore.ptr()) {
            websiteDataStore = *policies->websiteDataStore();
            processSwapRequestedByClient = ProcessSwapRequestedByClient::Yes;
        }
        if (policies->userContentController() && policies->userContentController() != m_userContentController.ptr())
            processSwapRequestedByClient = ProcessSwapRequestedByClient::Yes;
    }

    if (navigation && !navigation->userContentExtensionsEnabled()) {
        if (!navigation->websitePolicies())
            navigation->setWebsitePolicies(API::WebsitePolicies::create());
        navigation->websitePolicies()->setContentBlockersEnabled(false);
    }

#if ENABLE(DEVICE_ORIENTATION)
    if (navigation && (!navigation->websitePolicies() || navigation->websitePolicies()->deviceOrientationAndMotionAccessState() == WebCore::DeviceOrientationOrMotionPermissionState::Prompt)) {
        auto deviceOrientationPermission = websiteDataStore->deviceOrientationAndMotionAccessController().cachedDeviceOrientationPermission(SecurityOriginData::fromURL(navigation->currentRequest().url()));
        if (deviceOrientationPermission != WebCore::DeviceOrientationOrMotionPermissionState::Prompt) {
            if (!navigation->websitePolicies())
                navigation->setWebsitePolicies(API::WebsitePolicies::create());
            navigation->websitePolicies()->setDeviceOrientationAndMotionAccessState(deviceOrientationPermission);
        }
    }
#endif

#if PLATFORM(COCOA)
    static const bool forceDownloadFromDownloadAttribute = false;
#else
    static const bool forceDownloadFromDownloadAttribute = true;
#endif
    if (policyAction == PolicyAction::Use && navigation && (navigation->isSystemPreview() || (forceDownloadFromDownloadAttribute && navigation->shouldPerformDownload())))
        policyAction = PolicyAction::Download;

    if (policyAction != PolicyAction::Use
        || (!preferences().siteIsolationEnabled() && !frame.isMainFrame())
        || !navigation) {
        receivedPolicyDecision(policyAction, navigation, navigation->websitePolicies(), WTFMove(navigationAction), WTFMove(sender));
        return;
    }

    Ref<WebProcessProxy> sourceProcess = process();
    URL sourceURL { pageLoadState().url() };
    if (auto* provisionalPage = provisionalPageProxy()) {
        if (provisionalPage->navigationID() == navigation->navigationID()) {
            sourceProcess = provisionalPage->process();
            sourceURL = provisionalPage->provisionalURL();
        }
    }

    m_isLockdownModeExplicitlySet = (navigation->websitePolicies() && navigation->websitePolicies()->isLockdownModeExplicitlySet()) || m_configuration->isLockdownModeExplicitlySet();
    auto lockdownMode = (navigation->websitePolicies() ? navigation->websitePolicies()->lockdownModeEnabled() : shouldEnableLockdownMode()) ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled;
    process().processPool().processForNavigation(*this, *navigation, sourceProcess.copyRef(), sourceURL, processSwapRequestedByClient, lockdownMode, frameInfo, WTFMove(websiteDataStore), [this, protectedThis = Ref { *this }, policyAction, navigation = Ref { *navigation }, navigationAction = WTFMove(navigationAction), sourceProcess = sourceProcess.copyRef(), sender = WTFMove(sender), processSwapRequestedByClient, frame = Ref { frame }] (Ref<WebProcessProxy>&& processForNavigation, SuspendedPageProxy* destinationSuspendedPage, const String& reason) mutable {
        // If the navigation has been destroyed, then no need to proceed.
        if (isClosed() || !navigationState().hasNavigation(navigation->navigationID())) {
            receivedPolicyDecision(policyAction, navigation.ptr(), navigation->websitePolicies(), WTFMove(navigationAction), WTFMove(sender));
            return;
        }

        bool shouldProcessSwap = processForNavigation.ptr() != sourceProcess.ptr();
        if (shouldProcessSwap) {
            policyAction = PolicyAction::StopAllLoads;
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "decidePolicyForNavigationAction, swapping process %i with process %i for navigation, reason=%" PUBLIC_LOG_STRING, processIdentifier(), processForNavigation->processIdentifier(), reason.utf8().data());
            LOG(ProcessSwapping, "(ProcessSwapping) Switching from process %i to new process (%i) for navigation %" PRIu64 " '%s'", processIdentifier(), processForNavigation->processIdentifier(), navigation->navigationID(), navigation->loggingString());
        } else
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "decidePolicyForNavigationAction: keep using process %i for navigation, reason=%" PUBLIC_LOG_STRING, processIdentifier(), reason.utf8().data());

        if (shouldProcessSwap) {
            // Make sure the process to be used for the navigation does not get shutDown now due to destroying SuspendedPageProxy or ProvisionalPageProxy objects.
            auto preventNavigationProcessShutdown = processForNavigation->shutdownPreventingScope();

            ASSERT(!destinationSuspendedPage || navigation->targetItem());
            auto suspendedPage = destinationSuspendedPage ? backForwardCache().takeSuspendedPage(*navigation->targetItem()) : nullptr;

            // It is difficult to get history right if we have several WebPage objects inside a single WebProcess for the same WebPageProxy. As a result, if we make sure to
            // clear any SuspendedPageProxy for the current page that are backed by the destination process before we proceed with the navigation. This makes sure the WebPage
            // we are about to create in the destination process will be the only one associated with this WebPageProxy.
            if (!destinationSuspendedPage)
                backForwardCache().removeEntriesForPageAndProcess(*this, processForNavigation);

            ASSERT(suspendedPage.get() == destinationSuspendedPage);
            if (suspendedPage && suspendedPage->pageIsClosedOrClosing())
                suspendedPage = nullptr;

            continueNavigationInNewProcess(navigation, frame.get(), WTFMove(suspendedPage), WTFMove(processForNavigation), processSwapRequestedByClient, ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision);

            receivedPolicyDecision(policyAction, navigation.ptr(), nullptr, WTFMove(navigationAction), WTFMove(sender), WillContinueLoadInNewProcess::Yes);
            return;
        }

        auto item = navigation->reloadItem() ? navigation->reloadItem() : navigation->targetItem();
        std::optional<SandboxExtension::Handle> optionalHandle;
        if (policyAction == PolicyAction::Use && item) {
            URL fullURL { item->url() };
            if (fullURL.protocolIs("file"_s)) {
                SandboxExtension::Handle sandboxExtensionHandle;
                maybeInitializeSandboxExtensionHandle(processForNavigation.get(), fullURL, item->resourceDirectoryURL(), sandboxExtensionHandle);
                optionalHandle = WTFMove(sandboxExtensionHandle);
            }
        }

        receivedPolicyDecision(policyAction, navigation.ptr(), navigation->websitePolicies(), WTFMove(navigationAction), WTFMove(sender), WillContinueLoadInNewProcess::No, WTFMove(optionalHandle));
    });
}

void WebPageProxy::receivedPolicyDecision(PolicyAction action, API::Navigation* navigation, RefPtr<API::WebsitePolicies>&& websitePolicies, std::variant<Ref<API::NavigationResponse>, Ref<API::NavigationAction>>&& navigationActionOrResponse, Ref<PolicyDecisionSender>&& sender, WillContinueLoadInNewProcess willContinueLoadInNewProcess, std::optional<SandboxExtension::Handle> sandboxExtensionHandle)
{
    if (!hasRunningProcess()) {
        sender->send(PolicyDecision { sender->identifier(), isNavigatingToAppBoundDomain(), PolicyAction::Ignore, 0, std::nullopt, std::nullopt });
        return;
    }

    auto transaction = m_pageLoadState.transaction();

    if (action == PolicyAction::Ignore && willContinueLoadInNewProcess == WillContinueLoadInNewProcess::No && navigation && navigation->navigationID() == m_pageLoadState.pendingAPIRequest().navigationID)
        m_pageLoadState.clearPendingAPIRequest(transaction);

    std::optional<DownloadID> downloadID;
    if (action == PolicyAction::Download) {
        // Create a download proxy.
        auto& download = m_process->processPool().createDownloadProxy(m_websiteDataStore, m_decidePolicyForResponseRequest, this, navigation ? navigation->originatingFrameInfo() : FrameInfoData { });
        download.setDidStartCallback([this, weakThis = WeakPtr { *this }, navigationActionOrResponse = WTFMove(navigationActionOrResponse)] (auto* downloadProxy) {
            if (!weakThis || !downloadProxy)
                return;
            WTF::switchOn(navigationActionOrResponse,
                [&] (const Ref<API::NavigationResponse>& response) {
                    if (!response->downloadAttribute().isNull())
                        downloadProxy->setSuggestedFilename(response->downloadAttribute());
                    m_navigationClient->navigationResponseDidBecomeDownload(*this, response.get(), *downloadProxy);
                }, [&] (const Ref<API::NavigationAction>& action) {
                    m_navigationClient->navigationActionDidBecomeDownload(*this, action.get(), *downloadProxy);
                }
            );
        });
        if (navigation) {
            download.setWasUserInitiated(navigation->wasUserInitiated());
            download.setRedirectChain(navigation->takeRedirectChain());
        }

        downloadID = download.downloadID();
        handleDownloadRequest(download);
        m_decidePolicyForResponseRequest = { };
    }
    
    std::optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePolicies)
        websitePoliciesData = websitePolicies->data();

    sender->send(PolicyDecision { sender->identifier(), isNavigatingToAppBoundDomain(), action, navigation ? navigation->navigationID() : 0, downloadID, WTFMove(websitePoliciesData), WTFMove(sandboxExtensionHandle) });
}

void WebPageProxy::commitProvisionalPage(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, std::optional<WebCore::HasInsecureContent> forcedHasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    ASSERT(m_provisionalPage);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "commitProvisionalPage: newPID=%i", m_provisionalPage->process().processIdentifier());

    RefPtr<WebFrameProxy> mainFrameInPreviousProcess = m_mainFrame;

    ASSERT(m_process.ptr() != &m_provisionalPage->process());

    auto shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::No;
#if PLATFORM(MAC)
    // On macOS, when not using UI-side compositing, we need to make sure we do not close the page in the previous process until we've
    // entered accelerated compositing for the new page or we will flash on navigation.
    if (drawingArea()->type() == DrawingAreaType::TiledCoreAnimation)
        shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::Yes;
#endif

    if (m_isLayerTreeFrozenDueToSwipeAnimation)
        send(Messages::WebPage::UnfreezeLayerTreeDueToSwipeAnimation());

    resetStateAfterProcessTermination(ProcessTerminationReason::NavigationSwap);

    removeAllMessageReceivers();
    auto* navigation = navigationState().navigation(m_provisionalPage->navigationID());
    bool didSuspendPreviousPage = navigation && !m_provisionalPage->isProcessSwappingOnNavigationResponse() ? suspendCurrentPageIfPossible(*navigation, WTFMove(mainFrameInPreviousProcess), m_provisionalPage->processSwapRequestedByClient(), shouldDelayClosingUntilFirstLayerFlush) : false;
    m_process->removeWebPage(*this, m_websiteDataStore.ptr() == m_provisionalPage->process().websiteDataStore() ? WebProcessProxy::EndsUsingDataStore::No : WebProcessProxy::EndsUsingDataStore::Yes);

    // There is no way we'll be able to return to the page in the previous page so close it.
    if (!didSuspendPreviousPage)
        send(Messages::WebPage::Close());

    const auto oldWebPageID = m_webPageID;
    swapToProvisionalPage(std::exchange(m_provisionalPage, nullptr));

    didCommitLoadForFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, forcedHasInsecureContent, mouseEventPolicy, userData);

    m_inspectorController->didCommitProvisionalPage(oldWebPageID, m_webPageID);
}

void WebPageProxy::destroyProvisionalPage()
{
    m_provisionalPage = nullptr;
}

void WebPageProxy::continueNavigationInNewProcess(API::Navigation& navigation, WebFrameProxy& frame, std::unique_ptr<SuspendedPageProxy>&& suspendedPage, Ref<WebProcessProxy>&& newProcess, ProcessSwapRequestedByClient processSwapRequestedByClient, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "continueNavigationInNewProcess: newProcessPID=%i, hasSuspendedPage=%i", newProcess->processIdentifier(), !!suspendedPage);
    LOG(Loading, "Continuing navigation %" PRIu64 " '%s' in a new web process", navigation.navigationID(), navigation.loggingString());
    RELEASE_ASSERT(!newProcess->isInProcessCache());
    ASSERT(shouldTreatAsContinuingLoad != ShouldTreatAsContinuingLoad::No);

    if (navigation.currentRequest().url().isLocalFile())
        newProcess->addPreviouslyApprovedFileURL(navigation.currentRequest().url());

    if (m_provisionalPage) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "continueNavigationInNewProcess: There is already a pending provisional load, cancelling it (provisonalNavigationID=%llu, navigationID=%llu)", m_provisionalPage->navigationID(), navigation.navigationID());
        if (m_provisionalPage->navigationID() != navigation.navigationID())
            m_provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }

    RefPtr websitePolicies = navigation.websitePolicies();
    bool isServerSideRedirect = shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision && navigation.currentRequestIsRedirect();
    bool isProcessSwappingOnNavigationResponse = shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted;
    if (!frame.isMainFrame() && preferences().siteIsolationEnabled()) {
        frame.swapToProcess(WTFMove(newProcess), navigation.currentRequest());
        return;
    }

    m_provisionalPage = makeUnique<ProvisionalPageProxy>(*this, WTFMove(newProcess), WTFMove(suspendedPage), navigation.navigationID(), isServerSideRedirect, navigation.currentRequest(), processSwapRequestedByClient, isProcessSwappingOnNavigationResponse, websitePolicies.get());

    auto continuation = [this, protectedThis = Ref { *this }, navigation = Ref { navigation }, shouldTreatAsContinuingLoad, websitePolicies = WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume]() mutable {
        if (auto* item = navigation->targetItem()) {
            LOG(Loading, "WebPageProxy %p continueNavigationInNewProcess to back item URL %s", this, item->url().utf8().data());

            auto transaction = m_pageLoadState.transaction();
            m_pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), item->url() });

            m_provisionalPage->goToBackForwardItem(navigation, *item, WTFMove(websitePolicies), shouldTreatAsContinuingLoad, existingNetworkResourceLoadIdentifierToResume);
            return;
        }

        if (m_backForwardList->currentItem() && (navigation->lockBackForwardList() == LockBackForwardList::Yes || navigation->lockHistory() == LockHistory::Yes)) {
            // If WebCore is supposed to lock the history for this load, then the new process needs to know about the current history item so it can update
            // it instead of creating a new one.
            m_provisionalPage->send(Messages::WebPage::SetCurrentHistoryItemForReattach(m_backForwardList->currentItem()->itemState()));
        }

        std::optional<WebsitePoliciesData> websitePoliciesData;
        if (websitePolicies)
            websitePoliciesData = websitePolicies->data();

        // FIXME: Work out timing of responding with the last policy delegate, etc
        ASSERT(!navigation->currentRequest().isEmpty());
        ASSERT(!existingNetworkResourceLoadIdentifierToResume || !navigation->substituteData());
        if (auto& substituteData = navigation->substituteData())
            m_provisionalPage->loadData(navigation, { substituteData->content.data(), substituteData->content.size() }, substituteData->MIMEType, substituteData->encoding, substituteData->baseURL, substituteData->userData.get(), shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain(), WTFMove(websitePoliciesData), substituteData->sessionHistoryVisibility);
        else
            m_provisionalPage->loadRequest(navigation, ResourceRequest { navigation->currentRequest() }, nullptr, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain(), WTFMove(websitePoliciesData), existingNetworkResourceLoadIdentifierToResume);
    };
    if (m_inspectorController->shouldPauseLoading(*m_provisionalPage))
        m_inspectorController->setContinueLoadingCallback(*m_provisionalPage, WTFMove(continuation));
    else
        continuation();
}

bool WebPageProxy::isPageOpenedByDOMShowingInitialEmptyDocument() const
{
    return openedByDOM() && !hasCommittedAnyProvisionalLoads();
}

// MSVC gives a redeclaration error if noreturn is used on the definition and not the declaration, while
// Cocoa tests segfault if it is moved to the declaration site (even if we move the definition with it!).
#if !COMPILER(MSVC)
NO_RETURN_DUE_TO_ASSERT
#endif
void WebPageProxy::didFailToSuspendAfterProcessSwap()
{
    // Only the SuspendedPageProxy should be getting this call.
    ASSERT_NOT_REACHED();
}

#if !COMPILER(MSVC)
NO_RETURN_DUE_TO_ASSERT
#endif
void WebPageProxy::didSuspendAfterProcessSwap()
{
    // Only the SuspendedPageProxy should be getting this call.
    ASSERT_NOT_REACHED();
}

void WebPageProxy::setUserAgent(String&& userAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = WTFMove(userAgent);

    // We update the service worker there at the moment to be sure we use values used by actual web pages.
    // FIXME: Refactor this when we have a better User-Agent story.
    process().processPool().updateRemoteWorkerUserAgent(m_userAgent);

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetUserAgent(m_userAgent));
}

void WebPageProxy::setApplicationNameForUserAgent(const String& applicationName)
{
    if (m_applicationNameForUserAgent == applicationName)
        return;

    m_applicationNameForUserAgent = applicationName;
    if (!m_customUserAgent.isEmpty())
        return;

    setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
}

void WebPageProxy::setCustomUserAgent(const String& customUserAgent)
{
    if (m_customUserAgent == customUserAgent)
        return;

    m_customUserAgent = customUserAgent;

    if (m_customUserAgent.isEmpty()) {
        setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
        return;
    }

    setUserAgent(String { m_customUserAgent });
}

void WebPageProxy::resumeActiveDOMObjectsAndAnimations()
{
    if (!hasRunningProcess() || !m_areActiveDOMObjectsAndAnimationsSuspended)
        return;

    m_areActiveDOMObjectsAndAnimationsSuspended = false;

    send(Messages::WebPage::ResumeActiveDOMObjectsAndAnimations());
}

void WebPageProxy::suspendActiveDOMObjectsAndAnimations()
{
    if (!hasRunningProcess() || m_areActiveDOMObjectsAndAnimationsSuspended)
        return;

    m_areActiveDOMObjectsAndAnimationsSuspended = true;

    send(Messages::WebPage::SuspendActiveDOMObjectsAndAnimations());
}

void WebPageProxy::suspend(CompletionHandler<void(bool)>&& completionHandler)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "suspend:");
    if (!hasRunningProcess() || m_isSuspended)
        return completionHandler(false);

    m_isSuspended = true;
    sendWithAsyncReply(Messages::WebPage::Suspend(), WTFMove(completionHandler));
}

void WebPageProxy::resume(CompletionHandler<void(bool)>&& completionHandler)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "resume:");

    if (!hasRunningProcess() || !m_isSuspended)
        return completionHandler(false);

    m_isSuspended = false;
    sendWithAsyncReply(Messages::WebPage::Resume(), WTFMove(completionHandler));
}

bool WebPageProxy::supportsTextEncoding() const
{
    // FIXME (118840): We should probably only support this for text documents, not all non-image documents.
    return m_mainFrame && !m_mainFrame->isDisplayingStandaloneImageDocument();
}

void WebPageProxy::setCustomTextEncodingName(const String& encodingName)
{
    if (m_customTextEncodingName == encodingName)
        return;
    m_customTextEncodingName = encodingName;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetCustomTextEncodingName(encodingName));
}

SessionState WebPageProxy::sessionState(WTF::Function<bool (WebBackForwardListItem&)>&& filter) const
{
    RELEASE_ASSERT(RunLoop::isMain());
    SessionState sessionState;

    sessionState.backForwardListState = m_backForwardList->backForwardListState(WTFMove(filter));

    String provisionalURLString = m_pageLoadState.pendingAPIRequestURL();
    if (provisionalURLString.isEmpty())
        provisionalURLString = m_pageLoadState.provisionalURL();

    if (!provisionalURLString.isEmpty())
        sessionState.provisionalURL = URL { provisionalURLString };

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

        m_backForwardList->restoreFromState(WTFMove(sessionState.backForwardListState));
        // If the process is not launched yet, the session will be restored when sending the WebPageCreationParameters;
        if (hasRunningProcess())
            send(Messages::WebPage::RestoreSession(m_backForwardList->itemStates()));

        auto transaction = m_pageLoadState.transaction();
        m_pageLoadState.setCanGoBack(transaction, m_backForwardList->backItem());
        m_pageLoadState.setCanGoForward(transaction, m_backForwardList->forwardItem());

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
            return loadRequest(sessionState.provisionalURL);

        if (hasBackForwardList) {
            if (WebBackForwardListItem* item = m_backForwardList->currentItem())
                return goToBackForwardItem(*item);
        }
    }

    return nullptr;
}

bool WebPageProxy::supportsTextZoom() const
{
    // FIXME (118840): This should also return false for standalone media and plug-in documents.
    if (!m_mainFrame || m_mainFrame->isDisplayingStandaloneImageDocument())
        return false;

    return true;
}
 
void WebPageProxy::setTextZoomFactor(double zoomFactor)
{
    if (m_textZoomFactor == zoomFactor)
        return;

    m_textZoomFactor = zoomFactor;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetTextZoomFactor(m_textZoomFactor));
}

void WebPageProxy::setPageZoomFactor(double zoomFactor)
{
    if (m_pageZoomFactor == zoomFactor)
        return;

    closeOverlayedViews();

    m_pageZoomFactor = zoomFactor;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetPageZoomFactor(m_pageZoomFactor));
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    closeOverlayedViews();

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor));
}

double WebPageProxy::pageZoomFactor() const
{
    // Zoom factor for non-PDF pages persists across page loads. We maintain a separate member variable for PDF
    // zoom which ensures that we don't use the PDF zoom for a normal page.
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginZoomFactor;
    return m_pageZoomFactor;
}

double WebPageProxy::pageScaleFactor() const
{
    // PDF documents use zoom and scale factors to size themselves appropriately in the window. We store them
    // separately but decide which to return based on the main frame.
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginScaleFactor;
    return m_pageScaleFactor;
}

void WebPageProxy::scalePage(double scale, const IntPoint& origin)
{
    ASSERT(scale > 0);

    m_pageScaleFactor = scale;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ScalePage(scale, origin));
}

void WebPageProxy::scalePageInViewCoordinates(double scale, const IntPoint& centerInViewCoordinates)
{
    ASSERT(scale > 0);

    m_pageScaleFactor = scale;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ScalePageInViewCoordinates(scale, centerInViewCoordinates));
}

void WebPageProxy::scaleView(double scale)
{
    ASSERT(scale > 0);

    m_viewScaleFactor = scale;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ScaleView(scale));
}

void WebPageProxy::setIntrinsicDeviceScaleFactor(float scaleFactor)
{
    if (m_intrinsicDeviceScaleFactor == scaleFactor)
        return;

    m_intrinsicDeviceScaleFactor = scaleFactor;

    if (m_drawingArea)
        m_drawingArea->deviceScaleFactorDidChange();
}

void WebPageProxy::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
#if HAVE(CVDISPLAYLINK)
    if (hasRunningProcess() && m_displayID && m_registeredForFullSpeedUpdates)
        process().processPool().setDisplayLinkForDisplayWantsFullSpeedUpdates(process(), *m_displayID, false);

    m_registeredForFullSpeedUpdates = false;
#endif

    m_displayID = displayID;
    if (m_drawingArea)
        m_drawingArea->windowScreenDidChange(displayID, nominalFramesPerSecond);

    if (!hasRunningProcess())
        return;

    send(Messages::EventDispatcher::PageScreenDidChange(m_webPageID, displayID, nominalFramesPerSecond));
    send(Messages::WebPage::WindowScreenDidChange(displayID, nominalFramesPerSecond));
#if HAVE(CVDISPLAYLINK)
    updateDisplayLinkFrequency();
#endif
}

float WebPageProxy::deviceScaleFactor() const
{
    return m_customDeviceScaleFactor.value_or(m_intrinsicDeviceScaleFactor);
}

void WebPageProxy::setCustomDeviceScaleFactor(float customScaleFactor)
{
    if (m_customDeviceScaleFactor && m_customDeviceScaleFactor.value() == customScaleFactor)
        return;

    float oldScaleFactor = deviceScaleFactor();

    // A value of 0 clears the customScaleFactor.
    if (customScaleFactor)
        m_customDeviceScaleFactor = customScaleFactor;
    else
        m_customDeviceScaleFactor = std::nullopt;

    if (!hasRunningProcess())
        return;

    if (deviceScaleFactor() != oldScaleFactor)
        m_drawingArea->deviceScaleFactorDidChange();
}

void WebPageProxy::accessibilitySettingsDidChange()
{
    if (!hasRunningProcess())
        return;

    // Also update screen properties which encodes invert colors.
    process().processPool().screenPropertiesStateChanged();
    send(Messages::WebPage::AccessibilitySettingsDidChange());
}

void WebPageProxy::setUseFixedLayout(bool fixed)
{
    // This check is fine as the value is initialized in the web
    // process as part of the creation parameters.
    if (fixed == m_useFixedLayout)
        return;

    m_useFixedLayout = fixed;
    if (!fixed)
        m_fixedLayoutSize = IntSize();

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetUseFixedLayout(fixed));
}

void WebPageProxy::setFixedLayoutSize(const IntSize& size)
{
    if (size == m_fixedLayoutSize)
        return;

    m_fixedLayoutSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetFixedLayoutSize(size));
}

void WebPageProxy::setDefaultUnobscuredSize(const FloatSize& size)
{
    if (size == m_defaultUnobscuredSize)
        return;

    m_defaultUnobscuredSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetDefaultUnobscuredSize(m_defaultUnobscuredSize));
}

void WebPageProxy::setMinimumUnobscuredSize(const FloatSize& size)
{
    if (size == m_minimumUnobscuredSize)
        return;

    m_minimumUnobscuredSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMinimumUnobscuredSize(m_minimumUnobscuredSize));
}

void WebPageProxy::setMaximumUnobscuredSize(const FloatSize& size)
{
    if (size == m_maximumUnobscuredSize)
        return;

    m_maximumUnobscuredSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMaximumUnobscuredSize(m_maximumUnobscuredSize));
}

void WebPageProxy::setViewExposedRect(std::optional<WebCore::FloatRect> viewExposedRect)
{
    if (m_viewExposedRect == viewExposedRect)
        return;

    m_viewExposedRect = viewExposedRect;

#if PLATFORM(MAC)
    if (m_drawingArea)
        m_drawingArea->didChangeViewExposedRect();
#endif
}

void WebPageProxy::setAlwaysShowsHorizontalScroller(bool alwaysShowsHorizontalScroller)
{
    if (alwaysShowsHorizontalScroller == m_alwaysShowsHorizontalScroller)
        return;

    m_alwaysShowsHorizontalScroller = alwaysShowsHorizontalScroller;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetAlwaysShowsHorizontalScroller(alwaysShowsHorizontalScroller));
}

void WebPageProxy::setAlwaysShowsVerticalScroller(bool alwaysShowsVerticalScroller)
{
    if (alwaysShowsVerticalScroller == m_alwaysShowsVerticalScroller)
        return;

    m_alwaysShowsVerticalScroller = alwaysShowsVerticalScroller;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetAlwaysShowsVerticalScroller(alwaysShowsVerticalScroller));
}

void WebPageProxy::listenForLayoutMilestones(OptionSet<WebCore::LayoutMilestone> milestones)
{    
    if (milestones == m_observedLayoutMilestones)
        return;

    m_observedLayoutMilestones = milestones;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ListenForLayoutMilestones(milestones));
}

void WebPageProxy::setSuppressScrollbarAnimations(bool suppressAnimations)
{
    if (suppressAnimations == m_suppressScrollbarAnimations)
        return;

    m_suppressScrollbarAnimations = suppressAnimations;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetSuppressScrollbarAnimations(suppressAnimations));
}

void WebPageProxy::setRubberBandsAtLeft(bool rubberBandsAtLeft)
{
    m_rubberBandableEdges.setLeft(rubberBandsAtLeft);
}

void WebPageProxy::setRubberBandsAtRight(bool rubberBandsAtRight)
{
    m_rubberBandableEdges.setRight(rubberBandsAtRight);
}

void WebPageProxy::setRubberBandsAtTop(bool rubberBandsAtTop)
{
    m_rubberBandableEdges.setTop(rubberBandsAtTop);
}

void WebPageProxy::setRubberBandsAtBottom(bool rubberBandsAtBottom)
{
    m_rubberBandableEdges.setBottom(rubberBandsAtBottom);
}
    
void WebPageProxy::setEnableVerticalRubberBanding(bool enableVerticalRubberBanding)
{
    if (enableVerticalRubberBanding == m_enableVerticalRubberBanding)
        return;

    m_enableVerticalRubberBanding = enableVerticalRubberBanding;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetEnableVerticalRubberBanding(enableVerticalRubberBanding));
}
    
bool WebPageProxy::verticalRubberBandingIsEnabled() const
{
    return m_enableVerticalRubberBanding;
}
    
void WebPageProxy::setEnableHorizontalRubberBanding(bool enableHorizontalRubberBanding)
{
    if (enableHorizontalRubberBanding == m_enableHorizontalRubberBanding)
        return;

    m_enableHorizontalRubberBanding = enableHorizontalRubberBanding;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetEnableHorizontalRubberBanding(enableHorizontalRubberBanding));
}
    
bool WebPageProxy::horizontalRubberBandingIsEnabled() const
{
    return m_enableHorizontalRubberBanding;
}

void WebPageProxy::setBackgroundExtendsBeyondPage(bool backgroundExtendsBeyondPage)
{
    if (backgroundExtendsBeyondPage == m_backgroundExtendsBeyondPage)
        return;

    m_backgroundExtendsBeyondPage = backgroundExtendsBeyondPage;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetBackgroundExtendsBeyondPage(backgroundExtendsBeyondPage));
}

bool WebPageProxy::backgroundExtendsBeyondPage() const
{
    return m_backgroundExtendsBeyondPage;
}

void WebPageProxy::setPaginationMode(WebCore::Pagination::Mode mode)
{
    if (mode == m_paginationMode)
        return;

    m_paginationMode = mode;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetPaginationMode(mode));
}

void WebPageProxy::setPaginationBehavesLikeColumns(bool behavesLikeColumns)
{
    if (behavesLikeColumns == m_paginationBehavesLikeColumns)
        return;

    m_paginationBehavesLikeColumns = behavesLikeColumns;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetPaginationBehavesLikeColumns(behavesLikeColumns));
}

void WebPageProxy::setPageLength(double pageLength)
{
    if (pageLength == m_pageLength)
        return;

    m_pageLength = pageLength;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetPageLength(pageLength));
}

void WebPageProxy::setGapBetweenPages(double gap)
{
    if (gap == m_gapBetweenPages)
        return;

    m_gapBetweenPages = gap;

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::SetGapBetweenPages(gap));
}

void WebPageProxy::pageScaleFactorDidChange(double scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
}

void WebPageProxy::pluginScaleFactorDidChange(double pluginScaleFactor)
{
    m_pluginScaleFactor = pluginScaleFactor;
}

void WebPageProxy::pluginZoomFactorDidChange(double pluginZoomFactor)
{
    m_pluginZoomFactor = pluginZoomFactor;
}

void WebPageProxy::findStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    if (string.isEmpty()) {
        didFindStringMatches(string, Vector<Vector<WebCore::IntRect>> (), 0);
        return;
    }

    send(Messages::WebPage::FindStringMatches(string, options, maxMatchCount));
}

void WebPageProxy::findString(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(bool)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::FindString(string, options, maxMatchCount), WTFMove(callbackFunction));
}

void WebPageProxy::findRectsForStringMatches(const String& string, OptionSet<WebKit::FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(Vector<WebCore::FloatRect>&&)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::FindRectsForStringMatches(string, options, maxMatchCount), WTFMove(callbackFunction));
}

void WebPageProxy::findTextRangesForStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::FindTextRangesForStringMatches(string, options, maxMatchCount), WTFMove(callbackFunction));
}

void WebPageProxy::replaceFoundTextRangeWithString(const WebFoundTextRange& range, const String& string)
{
    send(Messages::WebPage::ReplaceFoundTextRangeWithString(range, string));
}

void WebPageProxy::decorateTextRangeWithStyle(const WebFoundTextRange& range, FindDecorationStyle style)
{
    send(Messages::WebPage::DecorateTextRangeWithStyle(range, style));
}

void WebPageProxy::scrollTextRangeToVisible(const WebFoundTextRange& range)
{
    send(Messages::WebPage::ScrollTextRangeToVisible(range));
}

void WebPageProxy::clearAllDecoratedFoundText()
{
    send(Messages::WebPage::ClearAllDecoratedFoundText());
}

void WebPageProxy::didBeginTextSearchOperation()
{
    send(Messages::WebPage::DidBeginTextSearchOperation());
}

void WebPageProxy::didEndTextSearchOperation()
{
    send(Messages::WebPage::DidEndTextSearchOperation());
}

void WebPageProxy::requestRectForFoundTextRange(const WebFoundTextRange& range, CompletionHandler<void(WebCore::FloatRect)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::RequestRectForFoundTextRange(range), WTFMove(callbackFunction));
}

void WebPageProxy::addLayerForFindOverlay(CompletionHandler<void(WebCore::GraphicsLayer::PlatformLayerID)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::AddLayerForFindOverlay(), WTFMove(callbackFunction));
}

void WebPageProxy::removeLayerForFindOverlay(CompletionHandler<void()>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::RemoveLayerForFindOverlay(), WTFMove(callbackFunction));
}

void WebPageProxy::getImageForFindMatch(int32_t matchIndex)
{
    send(Messages::WebPage::GetImageForFindMatch(matchIndex));
}

void WebPageProxy::selectFindMatch(int32_t matchIndex)
{
    send(Messages::WebPage::SelectFindMatch(matchIndex));
}

void WebPageProxy::indicateFindMatch(int32_t matchIndex)
{
    send(Messages::WebPage::IndicateFindMatch(matchIndex));
}

void WebPageProxy::hideFindUI()
{
    send(Messages::WebPage::HideFindUI());
}

void WebPageProxy::hideFindIndicator()
{
    send(Messages::WebPage::HideFindIndicator());
}

void WebPageProxy::countStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::CountStringMatches(string, options, maxMatchCount));
}

void WebPageProxy::replaceMatches(Vector<uint32_t>&& matchIndices, const String& replacementText, bool selectionOnly, CompletionHandler<void(uint64_t)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::ReplaceMatches(WTFMove(matchIndices), replacementText, selectionOnly), WTFMove(callback));
}

void WebPageProxy::launchInitialProcessIfNecessary()
{
    if (process().isDummyProcessProxy())
        launchProcess({ }, ProcessLaunchReason::InitialProcess);
}

void WebPageProxy::runJavaScriptInMainFrame(RunJavaScriptParameters&& parameters, CompletionHandler<void(Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails>&&)>&& callbackFunction)
{
    runJavaScriptInFrameInScriptWorld(WTFMove(parameters), std::nullopt, API::ContentWorld::pageContentWorld(), WTFMove(callbackFunction));
}

void WebPageProxy::runJavaScriptInFrameInScriptWorld(RunJavaScriptParameters&& parameters, std::optional<WebCore::FrameIdentifier> frameID, API::ContentWorld& world, CompletionHandler<void(Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails>&&)>&& callbackFunction)
{
    // For backward-compatibility support running script in a WebView which has not done any loads yets.
    launchInitialProcessIfNecessary();

    if (!hasRunningProcess())
        return callbackFunction({ nullptr });

    ProcessThrottler::ActivityVariant activity;
#if USE(RUNNINGBOARD)
    if (pageClient().canTakeForegroundAssertions())
        activity = m_process->throttler().foregroundActivity("WebPageProxy::runJavaScriptInFrameInScriptWorld"_s);
#endif

    sendWithAsyncReply(Messages::WebPage::RunJavaScriptInFrameInScriptWorld(parameters, frameID, world.worldData()), [activity = WTFMove(activity), callbackFunction = WTFMove(callbackFunction)] (const IPC::DataReference& dataReference, std::optional<ExceptionDetails>&& details) mutable {
        if (details)
            return callbackFunction(makeUnexpected(WTFMove(*details)));
        if (dataReference.empty())
            return callbackFunction({ nullptr });
        callbackFunction({ API::SerializedScriptValue::createFromWireBytes(Vector(dataReference)).ptr() });
    });
}

void WebPageProxy::getRenderTreeExternalRepresentation(CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetRenderTreeExternalRepresentation(), WTFMove(callback));
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetSourceForFrame(frame->frameID()), WTFMove(callback));
}

void WebPageProxy::getContentsAsString(ContentAsStringIncludesChildFrames includesChildFrames, CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetContentsAsString(includesChildFrames), WTFMove(callback));
}

#if PLATFORM(COCOA)
void WebPageProxy::getContentsAsAttributedString(CompletionHandler<void(const WebCore::AttributedString&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::GetContentsAsAttributedString(), WTFMove(completionHandler));
}
#endif

void WebPageProxy::getAllFrames(CompletionHandler<void(FrameTreeNodeData&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetAllFrames(), WTFMove(completionHandler));
}

void WebPageProxy::getBytecodeProfile(CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetBytecodeProfile(), WTFMove(callback));
}

void WebPageProxy::getSamplingProfilerOutput(CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetSamplingProfilerOutput(), WTFMove(callback));
}

template <typename T>
static CompletionHandler<void(T data)> toAPIDataCallbackT(CompletionHandler<void(API::Data*)>&& callback)
{
    return [callback = WTFMove(callback)] (T data) mutable {
        if (!data) {
            callback(nullptr);
            return;
        }
        callback(API::Data::create(data->data(), data->size()).ptr());
    };
}

auto* toAPIDataCallback = toAPIDataCallbackT<const std::optional<IPC::SharedBufferReference>&>;
auto* toAPIDataSharedBufferCallback = toAPIDataCallbackT<RefPtr<WebCore::SharedBuffer>&&>;

#if ENABLE(MHTML)
void WebPageProxy::getContentsAsMHTMLData(CompletionHandler<void(API::Data*)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetContentsAsMHTMLData(), toAPIDataCallback(WTFMove(callback)));
}
#endif

void WebPageProxy::getSelectionOrContentsAsString(CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetSelectionOrContentsAsString(), callback);
}

void WebPageProxy::getSelectionAsWebArchiveData(CompletionHandler<void(API::Data*)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetSelectionAsWebArchiveData(), toAPIDataCallback(WTFMove(callback)));
}

void WebPageProxy::getMainResourceDataOfFrame(WebFrameProxy* frame, CompletionHandler<void(API::Data*)>&& callback)
{
    if (!frame)
        return callback(nullptr);
    sendWithAsyncReply(Messages::WebPage::GetMainResourceDataOfFrame(frame->frameID()), toAPIDataCallback(WTFMove(callback)));
}

void WebPageProxy::getResourceDataFromFrame(WebFrameProxy& frame, API::URL* resourceURL, CompletionHandler<void(API::Data*)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetResourceDataFromFrame(frame.frameID(), resourceURL->string()), toAPIDataCallback(WTFMove(callback)));
}

void WebPageProxy::getWebArchiveOfFrame(WebFrameProxy* frame, CompletionHandler<void(API::Data*)>&& callback)
{
    if (!frame)
        return callback(nullptr);
    sendWithAsyncReply(Messages::WebPage::GetWebArchiveOfFrame(frame->frameID()), toAPIDataCallback(WTFMove(callback)));
}

void WebPageProxy::getAccessibilityTreeData(CompletionHandler<void(API::Data*)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetAccessibilityTreeData(), toAPIDataCallback(WTFMove(callback)));
}

void WebPageProxy::forceRepaint(CompletionHandler<void()>&& callback)
{
    if (!hasRunningProcess())
        return callback();

    m_drawingArea->waitForBackingStoreUpdateOnNextPaint();

    sendWithAsyncReply(Messages::WebPage::ForceRepaint(), [weakThis = WeakPtr { *this }, callback = WTFMove(callback)] () mutable {
        if (weakThis) {
            weakThis->callAfterNextPresentationUpdate([callback = WTFMove(callback)] (auto) mutable {
                callback();
            });
        } else
            callback();
    });
}

void WebPageProxy::preferencesDidChange()
{
    if (!hasRunningProcess())
        return;

    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();

    pageClient().preferencesDidChange();

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification
    // even if nothing changed in UI process, so that overrides get removed.

    // Preferences need to be updated during synchronous printing to make "print backgrounds" preference work when toggled from a print dialog checkbox.
    if (m_isPerformingDOMPrintOperation)
        send(Messages::WebPage::PreferencesDidChangeDuringDOMPrintOperation(preferencesStore()), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        send(Messages::WebPage::PreferencesDidChange(preferencesStore()));
}

void WebPageProxy::didCreateMainFrame(FrameIdentifier frameID)
{
    // The DecidePolicyForNavigationActionSync IPC is synchronous and may therefore get processed before the DidCreateMainFrame one.
    // When this happens, decidePolicyForNavigationActionSync() calls didCreateMainFrame() and we need to ignore the DidCreateMainFrame
    // IPC when it later gets processed.
    if (m_mainFrame && m_mainFrame->frameID() == frameID)
        return;

    PageClientProtector protector(pageClient());

    MESSAGE_CHECK(m_process, !m_mainFrame);
    MESSAGE_CHECK(m_process, WebFrameProxy::canCreateFrame(frameID));

    m_mainFrame = WebFrameProxy::create(*this, m_process, frameID);

#if ENABLE(SERVICE_WORKER)
    if (m_serviceWorkerOpenWindowCompletionCallback) {
        m_mainFrame->setNavigationCallback([callback = WTFMove(m_serviceWorkerOpenWindowCompletionCallback)](auto pageID, auto) mutable {
            callback(pageID);
        });
    }
#endif
}

void WebPageProxy::didDestroyFrame(FrameIdentifier frameID)
{
    // If the page is closed before it has had the chance to send the DidCreateMainFrame message
    // back to the UIProcess, then the frameDestroyed message will still be received because it
    // gets sent directly to the WebProcessProxy.
    ASSERT(WebFrameProxyMap::isValidKey(frameID));
    auto* frame = WebFrameProxy::webFrame(frameID);
#if ENABLE(WEB_AUTHN)
    if (auto* page = frame ? frame->page() : nullptr)
        page->websiteDataStore().authenticatorManager().cancelRequest(page->webPageID(), frameID);
#endif
    if (auto* automationSession = process().processPool().automationSession())
        automationSession->didDestroyFrame(frameID);
    if (frame)
        frame->disconnect();
}

void WebPageProxy::disconnectFramesFromPage()
{
    if (auto mainFrame = std::exchange(m_mainFrame, nullptr))
        mainFrame->webProcessWillShutDown();
}

double WebPageProxy::estimatedProgress() const
{
    return m_pageLoadState.estimatedProgress();
}

void WebPageProxy::didStartProgress()
{
    ASSERT(!m_isClosed);

    PageClientProtector protector(pageClient());

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didStartProgress(transaction);

    m_pageLoadState.commitChanges();
}

void WebPageProxy::didChangeProgress(double value)
{
    PageClientProtector protector(pageClient());

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didChangeProgress(transaction, value);

    m_pageLoadState.commitChanges();
}

void WebPageProxy::didFinishProgress()
{
    PageClientProtector protector(pageClient());

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didFinishProgress(transaction);

    m_pageLoadState.commitChanges();
}

void WebPageProxy::setNetworkRequestsInProgress(bool networkRequestsInProgress)
{
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.setNetworkRequestsInProgress(transaction, networkRequestsInProgress);
}

void WebPageProxy::preconnectTo(const URL& url, const String& userAgent)
{
    if (!m_websiteDataStore->configuration().allowsServerPreconnect())
        return;

    auto storedCredentialsPolicy = m_canUseCredentialStorage ? WebCore::StoredCredentialsPolicy::Use : WebCore::StoredCredentialsPolicy::DoNotUse;

    websiteDataStore().networkProcess().preconnectTo(sessionID(), identifier(), webPageID(), url, userAgent, storedCredentialsPolicy, isNavigatingToAppBoundDomain(), m_lastNavigationWasAppInitiated ? LastNavigationWasAppInitiated::Yes : LastNavigationWasAppInitiated::No);
}

void WebPageProxy::setCanUseCredentialStorage(bool canUseCredentialStorage)
{
    m_canUseCredentialStorage = canUseCredentialStorage;
    send(Messages::WebPage::SetCanUseCredentialStorage(canUseCredentialStorage));
}

void WebPageProxy::didDestroyNavigation(uint64_t navigationID)
{
    MESSAGE_CHECK(m_process, WebNavigationState::NavigationMap::isValidKey(navigationID));

    PageClientProtector protector(pageClient());

    // On process-swap, the previous process tries to destroy the navigation but the provisional process is actually taking over the navigation.
    if (m_provisionalPage && m_provisionalPage->navigationID() == navigationID)
        return;

    m_navigationState->didDestroyNavigation(navigationID);
}

void WebPageProxy::didStartProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    didStartProvisionalLoadForFrameShared(m_process.copyRef(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData);
}

void WebPageProxy::didStartProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK_URL(process, url);

    // If the page starts a new main frame provisional load, then cancel any pending one in a provisional process.
    if (frame->isMainFrame() && m_provisionalPage && m_provisionalPage->mainFrame() != frame) {
        m_provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    LOG(Loading, "WebPageProxy %" PRIu64 " in process pid %i didStartProvisionalLoadForFrame to frameID %" PRIu64 ", navigationID %" PRIu64 ", url %s", m_identifier.toUInt64(), process->processIdentifier(), frameID.object().toUInt64(), navigationID, url.string().utf8().data());
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didStartProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.clearPendingAPIRequest(transaction);

    if (frame->isMainFrame()) {
        process->didStartProvisionalLoadForMainFrame(url);
        reportPageLoadResult(ResourceError { ResourceError::Type::Cancellation });
        m_pageLoadStart = MonotonicTime::now();
        m_pageLoadState.didStartProvisionalLoad(transaction, url.string(), unreachableURL.string());
        pageClient().didStartProvisionalLoadForMainFrame();
        closeOverlayedViews();
    }

    frame->setUnreachableURL(unreachableURL);
    frame->didStartProvisionalLoad(url);

    m_pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didStartProvisionalLoadForFrame(*this, *frame, navigation.get(), process->transformHandlesToObjects(userData.object()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didStartProvisionalNavigation(*this, request, navigation.get(), process->transformHandlesToObjects(userData.object()).get());
        m_navigationClient->didStartProvisionalLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
    }

#if ENABLE(WEB_AUTHN)
    m_websiteDataStore->authenticatorManager().cancelRequest(m_webPageID, frameID);
#endif
}

void WebPageProxy::didExplicitOpenForFrame(FrameIdentifier frameID, URL&& url, String&& mimeType)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    if (!checkURLReceivedFromCurrentOrPreviousWebProcess(m_process, url)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring WebPageProxy::DidExplicitOpenForFrame() IPC from the WebContent process because the file URL is outside the sandbox");
        return;
    }

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didExplicitOpen(transaction, url.string());

    frame->didExplicitOpen(WTFMove(url), WTFMove(mimeType));

    m_hasCommittedAnyProvisionalLoads = true;
    m_process->didCommitProvisionalLoad();

    m_pageLoadState.commitChanges();
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(FrameIdentifier frameID, uint64_t navigationID, ResourceRequest&& request, const UserData& userData)
{
    didReceiveServerRedirectForProvisionalLoadForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(request), userData);
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, uint64_t navigationID, ResourceRequest&& request, const UserData& userData)
{
    LOG(Loading, "WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame to frameID %" PRIu64 ", navigationID %" PRIu64 ", url %s", frameID.object().toUInt64(), navigationID, request.url().string().utf8().data());

    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK_URL(process, request.url());

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didReceiveServerRedirectForProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation = navigationID ? navigationState().navigation(navigationID) : nullptr;
    if (navigation)
        navigation->appendRedirectionURL(request.url());

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, request.url().string());

    frame->didReceiveServerRedirectForProvisionalLoad(request.url());

    m_pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didReceiveServerRedirectForProvisionalLoadForFrame(*this, *frame, frame->isMainFrame() ? navigation.get() : nullptr, process->transformHandlesToObjects(userData.object()).get());
    else if (frame->isMainFrame())
        m_navigationClient->didReceiveServerRedirectForProvisionalNavigation(*this, navigation.get(), process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::willPerformClientRedirectForFrame(FrameIdentifier frameID, const String& url, double delay, WebCore::LockBackForwardList)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "willPerformClientRedirectForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame())
        m_navigationClient->willPerformClientRedirect(*this, url, delay);
}

void WebPageProxy::didCancelClientRedirectForFrame(FrameIdentifier frameID)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didCancelClientRedirectForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame())
        m_navigationClient->didCancelClientRedirect(*this);
}

void WebPageProxy::didChangeProvisionalURLForFrame(FrameIdentifier frameID, uint64_t navigationID, URL&& url)
{
    didChangeProvisionalURLForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(url));
}

void WebPageProxy::didChangeProvisionalURLForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, uint64_t, URL&& url)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->frameLoadState().state() == FrameLoadState::State::Provisional);
    MESSAGE_CHECK_URL(process, url);

    auto transaction = m_pageLoadState.transaction();

    // Internally, we handle this the same way we handle a server redirect. There are no client callbacks
    // for this, but if this is the main frame, clients may observe a change to the page's URL.
    if (frame->isMainFrame())
        m_pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, url.string());

    frame->didReceiveServerRedirectForProvisionalLoad(url);
}

void WebPageProxy::didFailProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const ResourceError& error, WillContinueLoading willContinueLoading, const UserData& userData)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    if (m_provisionalPage && frame->isMainFrame()) {
        // The load did not fail, it is merely happening in a new provisional process.
        return;
    }

    didFailProvisionalLoadForFrameShared(m_process.copyRef(), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, provisionalURL, error, willContinueLoading, userData);
}

void WebPageProxy::didFailProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, WebFrameProxy& frame, FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const ResourceError& error, WillContinueLoading willContinueLoading, const UserData& userData)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " in web process pid %i didFailProvisionalLoadForFrame to provisionalURL %s", m_identifier.toUInt64(), process->processIdentifier(), provisionalURL.utf8().data());
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "didFailProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d, domain=%s, code=%d, isMainFrame=%d", frame.frameID().object().toUInt64(), frame.isMainFrame(), error.domain().utf8().data(), error.errorCode(), frame.isMainFrame());

    PageClientProtector protector(pageClient());

    if (m_controlledByAutomation) {
        if (auto* automationSession = process->processPool().automationSession())
            automationSession->navigationOccurredForFrame(frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame.isMainFrame() && navigationID)
        navigation = navigationState().takeNavigation(navigationID);

    auto transaction = m_pageLoadState.transaction();

    if (frame.isMainFrame()) {
        reportPageLoadResult(error);
        m_pageLoadState.didFailProvisionalLoad(transaction);
        pageClient().didFailProvisionalLoadForMainFrame();
        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        callLoadCompletionHandlersIfNecessary(false);
    }

    frame.didFailProvisionalLoad();

    m_pageLoadState.commitChanges();

    ASSERT(!m_failingProvisionalLoadURL);
    m_failingProvisionalLoadURL = provisionalURL;

    if (m_loaderClient)
        m_loaderClient->didFailProvisionalLoadWithErrorForFrame(*this, frame, navigation.get(), error, process->transformHandlesToObjects(userData.object()).get());
    else {
        m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), error, process->transformHandlesToObjects(userData.object()).get());
        m_navigationClient->didFailProvisionalLoadWithErrorForFrame(*this, WTFMove(request), error, WTFMove(frameInfo));
    }

    m_failingProvisionalLoadURL = { };

    // If the provisional page's load fails then we destroy the provisional page.
    if (m_provisionalPage && m_provisionalPage->mainFrame() == &frame && willContinueLoading == WillContinueLoading::No)
        m_provisionalPage = nullptr;
}

#if ENABLE(SERVICE_WORKER)
void WebPageProxy::didFinishServiceWorkerPageRegistration(bool success)
{
    ASSERT(m_isServiceWorkerPage);
    ASSERT(m_serviceWorkerLaunchCompletionHandler);

    if (m_serviceWorkerLaunchCompletionHandler)
        m_serviceWorkerLaunchCompletionHandler(success);
}
#endif

void WebPageProxy::callLoadCompletionHandlersIfNecessary(bool success)
{
#if ENABLE(SERVICE_WORKER)
    if (m_isServiceWorkerPage && m_serviceWorkerLaunchCompletionHandler && !success)
        m_serviceWorkerLaunchCompletionHandler(false);

    if (m_serviceWorkerOpenWindowCompletionCallback)
        m_serviceWorkerOpenWindowCompletionCallback(success ? webPageID() : std::optional<WebCore::PageIdentifier> { });
#endif
}

#if ENABLE(TRACKING_PREVENTION)
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
#endif

void WebPageProxy::didCommitLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool wasPrivateRelayed, bool containsPluginDocument, std::optional<HasInsecureContent> hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " didCommitLoadForFrame in navigation %" PRIu64, m_identifier.toUInt64(), navigationID);
    LOG(BackForward, "(Back/Forward) After load commit, back/forward list is now:%s", m_backForwardList->loggingString());

    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didCommitLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID && (navigation = navigationState().navigation(navigationID))) {
#if ENABLE(TRACKING_PREVENTION)
        auto requesterOrigin = navigation->lastNavigationAction().requesterOrigin;
        auto currentRequest = navigation->currentRequest();
        auto navigationDataTransfer = checkIfNavigationContainsDataTransfer(requesterOrigin, currentRequest);
        if (!navigationDataTransfer.isEmpty()) {
            RegistrableDomain currentDomain { currentRequest.url() };
            URL requesterURL { requesterOrigin.toString() };
            if (!currentDomain.matches(requesterURL))
                m_websiteDataStore->networkProcess().didCommitCrossSiteLoadWithDataTransfer(m_websiteDataStore->sessionID(), RegistrableDomain { requesterURL }, currentDomain, navigationDataTransfer, m_identifier, m_webPageID);
        }
#endif
    }

    m_hasCommittedAnyProvisionalLoads = true;
    m_process->didCommitProvisionalLoad();

    if (frame->isMainFrame()) {
        m_hasUpdatedRenderingAfterDidCommitLoad = false;
#if PLATFORM(IOS_FAMILY)
        m_firstLayerTreeTransactionIdAfterDidCommitLoad = downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea()).nextLayerTreeTransactionID();
#endif
    }

    auto transaction = m_pageLoadState.transaction();
    bool markPageInsecure = hasInsecureContent ? hasInsecureContent.value() == HasInsecureContent::Yes : certificateInfo.containsNonRootSHA1SignedCertificate();

    if (frame->isMainFrame()) {
        m_pageLoadState.didCommitLoad(transaction, certificateInfo, markPageInsecure, usedLegacyTLS, wasPrivateRelayed);
        m_shouldSuppressNextAutomaticNavigationSnapshot = false;
    } else if (markPageInsecure)
        m_pageLoadState.didDisplayOrRunInsecureContent(transaction);

#if USE(APPKIT)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME: A load going on in one frame shouldn't affect text editing in other frames on the page.
    pageClient().resetSecureInputState();
#endif

    frame->didCommitLoad(mimeType, certificateInfo, containsPluginDocument);

    if (frame->isMainFrame()) {
        std::optional<WebCore::PrivateClickMeasurement> privateClickMeasurement;
        if (m_privateClickMeasurement)
            privateClickMeasurement = m_privateClickMeasurement->pcm;
        else if (navigation && navigation->privateClickMeasurement())
            privateClickMeasurement = navigation->privateClickMeasurement();
        if (privateClickMeasurement) {
            if (privateClickMeasurement->destinationSite().matches(frame->url()) || privateClickMeasurement->isSKAdNetworkAttribution())
                websiteDataStore().storePrivateClickMeasurement(*privateClickMeasurement);
        }
        if (m_screenOrientationManager)
            m_screenOrientationManager->unlockIfNecessary();
    }
    m_privateClickMeasurement.reset();

    if (frame->isMainFrame()) {
        m_mainFrameHasCustomContentProvider = frameHasCustomContentProvider;

        if (m_mainFrameHasCustomContentProvider) {
            // Always assume that the main frame is pinned here, since the custom representation view will handle
            // any wheel events and dispatch them to the WKView when necessary.
            m_mainFramePinnedState = { true, true, true, true };
            m_uiClient->pinnedStateDidChange(*this);
        }
        pageClient().didCommitLoadForMainFrame(mimeType, frameHasCustomContentProvider);
    }

    // Even if WebPage has the default pageScaleFactor (and therefore doesn't reset it),
    // WebPageProxy's cache of the value can get out of sync (e.g. in the case where a
    // plugin is handling page scaling itself) so we should reset it to the default
    // for standard main frame loads.
    if (frame->isMainFrame()) {
        m_pageScaleFactor = 1;
        m_pluginScaleFactor = 1;
        m_mainFramePluginHandlesPageScaleGesture = false;
#if ENABLE(POINTER_LOCK)
        requestPointerUnlock();
#endif
        pageClient().setMouseEventPolicy(mouseEventPolicy);
#if ENABLE(UI_PROCESS_PDF_HUD)
        pageClient().removeAllPDFHUDs();
#endif
    }

    m_pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didCommitLoadForFrame(*this, *frame, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didCommitNavigation(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
        m_navigationClient->didCommitLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
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
    }

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (frame->isMainFrame() && m_preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::sharedNotifier().webPageURLChanged(*this);
#endif

#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager->didCommitLoadForFrame(frameID);
#endif
}

void WebPageProxy::didFinishDocumentLoadForFrame(FrameIdentifier frameID, uint64_t navigationID, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didFinishDocumentLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->documentLoadedForFrame(*frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    if (frame->isMainFrame()) {
        m_navigationClient->didFinishDocumentLoad(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
#if ENABLE(TRACKING_PREVENTION)
        m_didFinishDocumentLoadForMainFrameTimestamp = MonotonicTime::now();
#endif
    }
}

void WebPageProxy::didFinishLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const UserData& userData)
{
    LOG(Loading, "WebPageProxy::didFinishLoadForFrame - WebPageProxy %p with navigationID %" PRIu64 " didFinishLoad", this, navigationID);

    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didFinishLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID && navigationState().hasNavigation(navigationID))
        navigation = navigationState().navigation(navigationID);

    bool isMainFrame = frame->isMainFrame();
    if (!isMainFrame || !navigationID || navigation) {
        auto transaction = m_pageLoadState.transaction();

        if (isMainFrame)
            m_pageLoadState.didFinishLoad(transaction);

        if (m_controlledByAutomation) {
            if (auto* automationSession = process().processPool().automationSession())
                automationSession->navigationOccurredForFrame(*frame);
        }

        frame->didFinishLoad();

        m_pageLoadState.commitChanges();
    }

    if (m_loaderClient)
        m_loaderClient->didFinishLoadForFrame(*this, *frame, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didFinishNavigation(*this, navigation.get(), m_process->transformHandlesToObjects(userData.object()).get());
        m_navigationClient->didFinishLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
    }

    if (isMainFrame) {
        reportPageLoadResult();
        pageClient().didFinishNavigation(navigation.get());

        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        resetRecentCrashCountSoon();

        notifyProcessPoolToPrewarm();

        callLoadCompletionHandlersIfNecessary(true);
    }

    m_isLoadingAlternateHTMLStringForFailingProvisionalLoad = false;
}

void WebPageProxy::didFailLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const ResourceError& error, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "didFailLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d, domain=%s, code=%d", frameID.object().toUInt64(), frame->isMainFrame(), error.domain().utf8().data(), error.errorCode());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();

    if (isMainFrame)
        m_pageLoadState.didFailLoad(transaction);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    frame->didFailLoad();

    m_pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didFailLoadWithErrorForFrame(*this, *frame, navigation.get(), error, m_process->transformHandlesToObjects(userData.object()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didFailNavigationWithError(*this, frameInfo, navigation.get(), error, m_process->transformHandlesToObjects(userData.object()).get());
        m_navigationClient->didFailLoadWithErrorForFrame(*this, WTFMove(request), error, WTFMove(frameInfo));
    }

    if (isMainFrame) {
        reportPageLoadResult(error);
        pageClient().didFailNavigation(navigation.get());
        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        callLoadCompletionHandlersIfNecessary(false);
    }
}

void WebPageProxy::didSameDocumentNavigationForFrame(FrameIdentifier frameID, uint64_t navigationID, uint32_t opaqueSameDocumentNavigationType, URL&& url, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK_URL(m_process, url);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        m_pageLoadState.didSameDocumentNavigation(transaction, url.string());

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    m_pageLoadState.clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(url);

    m_pageLoadState.commitChanges();

    SameDocumentNavigationType navigationType = static_cast<SameDocumentNavigationType>(opaqueSameDocumentNavigationType);
    if (isMainFrame)
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, m_process->transformHandlesToObjects(userData.object()).get());

    if (isMainFrame)
        pageClient().didSameDocumentNavigationForMainFrame(navigationType);
}

void WebPageProxy::didSameDocumentNavigationForFrameViaJSHistoryAPI(WebCore::FrameIdentifier frameID, uint32_t opaqueSameDocumentNavigationType, URL url, NavigationActionData&& navigationActionData, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK_URL(m_process, url);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrameViaJSHistoryAPI: frameID=%" PRIu64 ", isMainFrame=%d, type=%u", frameID.object().toUInt64(), frame->isMainFrame(), opaqueSameDocumentNavigationType);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame()) {
        navigation = navigationState().createLoadRequestNavigation(ResourceRequest(url), m_backForwardList->currentItem());
        navigation->setLastNavigationAction(WTFMove(navigationActionData));
    }

    auto transaction = m_pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        m_pageLoadState.didSameDocumentNavigation(transaction, url.string());

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    m_pageLoadState.clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(url);

    m_pageLoadState.commitChanges();

    SameDocumentNavigationType navigationType = static_cast<SameDocumentNavigationType>(opaqueSameDocumentNavigationType);
    if (isMainFrame)
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, m_process->transformHandlesToObjects(userData.object()).get());

    if (isMainFrame)
        pageClient().didSameDocumentNavigationForMainFrame(navigationType);

    if (navigation)
        navigationState().didDestroyNavigation(navigation->navigationID());
}

void WebPageProxy::didChangeMainDocument(FrameIdentifier frameID)
{
#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager) {
        m_userMediaPermissionRequestManager->resetAccess(frameID);

#if ENABLE(GPU_PROCESS)
        if (auto* gpuProcess = m_process->processPool().gpuProcess()) {
            if (auto* frame = WebFrameProxy::webFrame(frameID))
                gpuProcess->updateCaptureOrigin(SecurityOriginData::fromURL(frame->url()), m_process->coreProcessIdentifier());
        }
#endif
    }

#else
    UNUSED_PARAM(frameID);
#endif
    
    m_isQuotaIncreaseDenied = false;

    m_speechRecognitionPermissionManager = nullptr;
}

void WebPageProxy::viewIsBecomingVisible()
{
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "viewIsBecomingVisible:");
    m_process->markProcessAsRecentlyUsed();
#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager->viewIsBecomingVisible();
#endif
}

void WebPageProxy::didReceiveTitleForFrame(FrameIdentifier frameID, const String& title, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto transaction = m_pageLoadState.transaction();

    if (frame->isMainFrame())
        m_pageLoadState.setTitle(transaction, title);

    frame->didChangeTitle(title);
    
    m_pageLoadState.commitChanges();

#if ENABLE(REMOTE_INSPECTOR)
    if (frame->isMainFrame())
        remoteInspectorInformationDidChange();
#endif
}

void WebPageProxy::didFirstLayoutForFrame(FrameIdentifier, const UserData& userData)
{
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(FrameIdentifier frameID, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    if (m_loaderClient)
        m_loaderClient->didFirstVisuallyNonEmptyLayoutForFrame(*this, *frame, m_process->transformHandlesToObjects(userData.object()).get());

    if (frame->isMainFrame())
        pageClient().didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void WebPageProxy::didLayoutForCustomContentProvider()
{
    didReachLayoutMilestone({ DidFirstLayout, DidFirstVisuallyNonEmptyLayout, DidHitRelevantRepaintedObjectsAreaThreshold });
}

void WebPageProxy::didReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> layoutMilestones)
{
    PageClientProtector protector(pageClient());

    if (layoutMilestones.contains(DidFirstVisuallyNonEmptyLayout))
        pageClient().clearSafeBrowsingWarningIfForMainFrameNavigation();
    
    if (m_loaderClient)
        m_loaderClient->didReachLayoutMilestone(*this, layoutMilestones);
    m_navigationClient->renderingProgressDidChange(*this, layoutMilestones);
}

void WebPageProxy::didDisplayInsecureContentForFrame(FrameIdentifier frameID, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didDisplayOrRunInsecureContent(transaction);
    m_pageLoadState.commitChanges();

    m_navigationClient->didDisplayInsecureContent(*this, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didRunInsecureContentForFrame(FrameIdentifier frameID, const UserData& userData)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.didDisplayOrRunInsecureContent(transaction);
    m_pageLoadState.commitChanges();

    m_navigationClient->didRunInsecureContent(*this, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::mainFramePluginHandlesPageScaleGestureDidChange(bool mainFramePluginHandlesPageScaleGesture)
{
    m_mainFramePluginHandlesPageScaleGesture = mainFramePluginHandlesPageScaleGesture;
}

#if !PLATFORM(COCOA)
void WebPageProxy::beginSafeBrowsingCheck(const URL&, bool, WebFramePolicyListenerProxy& listener)
{
    listener.didReceiveSafeBrowsingResults({ });
}
#endif

void WebPageProxy::decidePolicyForNavigationActionAsync(FrameIdentifier frameID, FrameInfoData&& frameInfo, PolicyCheckIdentifier identifier, uint64_t navigationID,
    NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request,
    IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse, uint64_t listenerID)
{
    decidePolicyForNavigationActionAsyncShared(m_process.copyRef(), m_webPageID, frameID, WTFMove(frameInfo), identifier, navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), listenerID);
}

void WebPageProxy::decidePolicyForNavigationActionAsyncShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, FrameIdentifier frameID, FrameInfoData&& frameInfo,
    WebCore::PolicyCheckIdentifier identifier, uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID,
    const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse,
    uint64_t listenerID)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);

    auto sender = PolicyDecisionSender::create(identifier, [webPageID, frameID, listenerID, process, url = request.url()] (const auto& policyDecision) {
        if (policyDecision.policyAction == PolicyAction::Use && url.isLocalFile())
            process->addPreviouslyApprovedFileURL(url);

        process->send(Messages::WebPage::DidReceivePolicyDecision(frameID, listenerID, policyDecision
#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
            , createNetworkExtensionsSandboxExtensions(process)
#endif
        ), webPageID);
    });

    decidePolicyForNavigationAction(process.copyRef(), webPageID, *frame, WTFMove(frameInfo), navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID,
        originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), WTFMove(sender));
}

#if PLATFORM(COCOA)
// https://html.spec.whatwg.org/#hand-off-to-external-software
static bool frameSandboxAllowsOpeningExternalCustomProtocols(SandboxFlags sandboxFlags, bool hasUserGesture)
{
    if (!(sandboxFlags & SandboxPopups) || !(sandboxFlags & SandboxTopNavigation) || !(sandboxFlags & SandboxTopNavigationToCustomProtocols))
        return true;

    return !(sandboxFlags & SandboxTopNavigationByUserActivation) && hasUserGesture;
}
#endif

void WebPageProxy::decidePolicyForNavigationAction(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, WebFrameProxy& frame, FrameInfoData&& frameInfo, uint64_t navigationID,
    NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfoData, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request,
    IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse, Ref<PolicyDecisionSender>&& sender)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: frameID=%llu, isMainFrame=%d, navigationID=%llu", frame.frameID().object().toUInt64(), frame.isMainFrame(), navigationID);

    LOG(Loading, "WebPageProxy::decidePolicyForNavigationAction - Original URL %s, current target URL %s", originalRequest.url().string().utf8().data(), request.url().string().utf8().data());

    PageClientProtector protector(pageClient());

    // Make the request whole again as we do not normally encode the request's body when sending it over IPC, for performance reasons.
    request.setHTTPBody(requestBody.takeData());

    auto transaction = m_pageLoadState.transaction();

    bool fromAPI = request.url() == m_pageLoadState.pendingAPIRequestURL();
    if (navigationID && !fromAPI)
        m_pageLoadState.clearPendingAPIRequest(transaction);

    if (!checkURLReceivedFromCurrentOrPreviousWebProcess(process, request.url())) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it is outside the sandbox");
        sender->send(PolicyDecision { sender->identifier(), isNavigatingToAppBoundDomain(), PolicyAction::Ignore, 0, std::nullopt, std::nullopt });
        return;
    }

    MESSAGE_CHECK_URL(process, originalRequest.url());

    RefPtr<API::Navigation> navigation;
    if (navigationID)
        navigation = m_navigationState->navigation(navigationID);

    // When process-swapping on a redirect, the navigationActionData / originatingFrameInfoData provided by the fresh new WebProcess are inaccurate since
    // the new process does not have sufficient information. To address the issue, we restore the information we stored on the NavigationAction during the original request
    // policy decision.
    if (navigationActionData.isRedirect && navigation) {
        navigationActionData = navigation->lastNavigationAction();
        navigationActionData.isRedirect = true;
        frameInfo.securityOrigin = navigation->destinationFrameSecurityOrigin();
    }

    if (!navigation) {
        if (auto targetBackForwardItemIdentifier = navigationActionData.targetBackForwardItemIdentifier) {
            if (auto* item = m_backForwardList->itemForID(*targetBackForwardItemIdentifier)) {
                auto* fromItem = navigationActionData.sourceBackForwardItemIdentifier ? m_backForwardList->itemForID(*navigationActionData.sourceBackForwardItemIdentifier) : nullptr;
                if (!fromItem)
                    fromItem = m_backForwardList->currentItem();
                navigation = m_navigationState->createBackForwardNavigation(*item, fromItem, FrameLoadType::IndexedBackForward);
            }
        }
        if (!navigation)
            navigation = m_navigationState->createLoadRequestNavigation(ResourceRequest(request), m_backForwardList->currentItem());
    }

    navigationID = navigation->navigationID();

    // Make sure the provisional page always has the latest navigationID.
    if (m_provisionalPage && &m_provisionalPage->process() == process.ptr())
        m_provisionalPage->setNavigationID(navigationID);

    navigation->setCurrentRequest(ResourceRequest(request), process->coreProcessIdentifier());
    navigation->setLastNavigationAction(navigationActionData);
    navigation->setOriginatingFrameInfo(originatingFrameInfoData);
    navigation->setDestinationFrameSecurityOrigin(frameInfo.securityOrigin);

    API::Navigation* mainFrameNavigation = frame.isMainFrame() ? navigation.get() : nullptr;
    auto* originatingFrame = originatingFrameInfoData.frameID ? WebFrameProxy::webFrame(*originatingFrameInfoData.frameID) : nullptr;
    auto destinationFrameInfo = API::FrameInfo::create(FrameInfoData { frameInfo }, this);
    RefPtr<API::FrameInfo> sourceFrameInfo;
    if (!fromAPI && originatingFrame == &frame)
        sourceFrameInfo = destinationFrameInfo.copyRef();
    else if (!fromAPI) {
        auto originatingPage = originatingPageID ? process->webPage(*originatingPageID) : nullptr;
        sourceFrameInfo = API::FrameInfo::create(WTFMove(originatingFrameInfoData), WTFMove(originatingPage));
    }

    bool shouldOpenAppLinks = !m_shouldSuppressAppLinksInNextNavigationPolicyDecision
        && destinationFrameInfo->isMainFrame()
        && (m_mainFrame && m_mainFrame->url().host() != request.url().host())
        && navigationActionData.navigationType != WebCore::NavigationType::BackForward;

    auto userInitiatedActivity = process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), destinationFrameInfo.ptr(), std::nullopt, ResourceRequest(request), originalRequest.url(), shouldOpenAppLinks, WTFMove(userInitiatedActivity), mainFrameNavigation);

#if ENABLE(CONTENT_FILTERING)
    if (frame.didHandleContentFilterUnblockNavigation(request)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it was handled by content filter");
        return receivedPolicyDecision(PolicyAction::Ignore, m_navigationState->navigation(navigationID), nullptr, WTFMove(navigationAction), WTFMove(sender));
    }
#endif

    // Other ports do not implement WebPage::platformCanHandleRequest().
#if PLATFORM(COCOA)
    // Sandboxed iframes should be allowed to open external apps via custom protocols unless explicitely allowed (https://html.spec.whatwg.org/#hand-off-to-external-software).
    bool canHandleRequest = navigationActionData.canHandleRequest || m_urlSchemeHandlersByScheme.contains<StringViewHashTranslator>(request.url().protocol());
    if (!canHandleRequest && !destinationFrameInfo->isMainFrame() && !frameSandboxAllowsOpeningExternalCustomProtocols(navigationActionData.effectiveSandboxFlags, !!navigationActionData.userGestureTokenIdentifier)) {
        if (!sourceFrameInfo || !m_preferences->needsSiteSpecificQuirks() || !Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(request.url().protocol(), sourceFrameInfo->securityOrigin())) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe");
            process->send(Messages::WebPage::AddConsoleMessage(frame.frameID(), MessageSource::Security, MessageLevel::Error, "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe"_s, std::nullopt), webPageID);
            return receivedPolicyDecision(PolicyAction::Ignore, m_navigationState->navigation(navigationID), nullptr, WTFMove(navigationAction), WTFMove(sender));
        }
        process->send(Messages::WebPage::AddConsoleMessage(frame.frameID(), MessageSource::Security, MessageLevel::Warning, "In the future, requests to navigate to a URL with custom protocol from a sandboxed iframe will be ignored"_s, std::nullopt), webPageID);
    }
#endif

    ShouldExpectSafeBrowsingResult shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::Yes;
    if (!m_preferences->safeBrowsingEnabled())
        shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::No;

    ShouldExpectAppBoundDomainResult shouldExpectAppBoundDomainResult = ShouldExpectAppBoundDomainResult::No;
#if ENABLE(APP_BOUND_DOMAINS)
    shouldExpectAppBoundDomainResult = ShouldExpectAppBoundDomainResult::Yes;
#endif

    Ref listener = frame.setUpPolicyListenerProxy([this, protectedThis = Ref { *this }, frame = Ref { frame }, sender = WTFMove(sender), navigation, navigationAction, frameInfo] (PolicyAction policyAction, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isAppBoundDomain) mutable {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: listener called: frameID=%llu, isMainFrame=%d, navigationID=%llu, policyAction=%u, safeBrowsingWarning=%d, isAppBoundDomain=%d", frame->frameID().object().toUInt64(), frame->isMainFrame(), navigation ? navigation->navigationID() : 0, (unsigned)policyAction, !!safeBrowsingWarning, !!isAppBoundDomain);

        navigation->setWebsitePolicies(WTFMove(policies));
        auto completionHandler = [this, protectedThis, frame, frameInfo, sender = WTFMove(sender), navigation, navigationAction = WTFMove(navigationAction), processSwapRequestedByClient] (PolicyAction policyAction) mutable {
            if (frame->isMainFrame()) {
                if (!navigation->websitePolicies()) {
                    if (auto* defaultPolicies = m_configuration->defaultWebsitePolicies())
                        navigation->setWebsitePolicies(defaultPolicies->copy());
                }
                if (auto* policies = navigation->websitePolicies())
                    navigation->setEffectiveContentMode(effectiveContentModeAfterAdjustingPolicies(*policies, navigation->currentRequest()));
            }
            receivedNavigationPolicyDecision(policyAction, navigation.get(), WTFMove(navigationAction), processSwapRequestedByClient, frame, frameInfo, WTFMove(sender));
        };

#if ENABLE(APP_BOUND_DOMAINS)
        if (policyAction != PolicyAction::Ignore) {
            if (!setIsNavigatingToAppBoundDomainAndCheckIfPermitted(frame->isMainFrame(), navigation->currentRequest().url(), isAppBoundDomain)) {
                auto error = errorForUnpermittedAppBoundDomainNavigation(navigation->currentRequest().url());
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), error, nullptr);
                WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "Ignoring request to load this main resource because it is attempting to navigate away from an app-bound domain or navigate after using restricted APIs");
                completionHandler(PolicyAction::Ignore);
                return;
            }
            if (frame->isMainFrame())
                m_isTopFrameNavigatingToAppBoundDomain = m_isNavigatingToAppBoundDomain;
        }
#endif

        if (!m_pageClient)
            return completionHandler(policyAction);

        m_pageClient->clearSafeBrowsingWarning();

        if (safeBrowsingWarning) {
            if (frame->isMainFrame() && safeBrowsingWarning->url().isValid()) {
                auto transaction = m_pageLoadState.transaction();
                m_pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), safeBrowsingWarning->url().string() });
                m_pageLoadState.commitChanges();
            }

            auto transaction = m_pageLoadState.transaction();
            m_pageLoadState.setTitleFromSafeBrowsingWarning(transaction, safeBrowsingWarning->title());

            m_pageClient->showSafeBrowsingWarning(*safeBrowsingWarning, [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), policyAction] (auto&& result) mutable {

                auto transaction = m_pageLoadState.transaction();
                m_pageLoadState.setTitleFromSafeBrowsingWarning(transaction, { });

                switchOn(result, [&] (const URL& url) {
                    completionHandler(PolicyAction::Ignore);
                    loadRequest({ url });
                }, [&] (ContinueUnsafeLoad continueUnsafeLoad) {
                    switch (continueUnsafeLoad) {
                    case ContinueUnsafeLoad::No:
                        if (!hasCommittedAnyProvisionalLoads())
                            m_uiClient->close(protectedThis.ptr());
                        completionHandler(PolicyAction::Ignore);
                        break;
                    case ContinueUnsafeLoad::Yes:
                        completionHandler(policyAction);
                        break;
                    }
                });
            });
            m_uiClient->didShowSafeBrowsingWarning();
            return;
        }
        completionHandler(policyAction);

    }, shouldExpectSafeBrowsingResult, shouldExpectAppBoundDomainResult);
    if (shouldExpectSafeBrowsingResult == ShouldExpectSafeBrowsingResult::Yes)
        beginSafeBrowsingCheck(request.url(), frame.isMainFrame(), listener);
#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldSendSecurityOriginData = !frame.isMainFrame() && shouldTreatURLProtocolAsAppBound(request.url(), websiteDataStore().configuration().enableInAppBrowserPrivacyForTesting());
    auto host = shouldSendSecurityOriginData ? frameInfo.securityOrigin.host : request.url().host();
    auto protocol = shouldSendSecurityOriginData ? frameInfo.securityOrigin.protocol : request.url().protocol();
    m_websiteDataStore->beginAppBoundDomainCheck(host.toString(), protocol.toString(), listener);
#endif

#if ENABLE(TRACKING_PREVENTION)
    auto wasPotentiallyInitiatedByUser = navigation->isLoadedWithNavigationShared() || userInitiatedActivity;
    if (!sessionID().isEphemeral())
        logFrameNavigation(frame, URL { m_pageLoadState.url() }, request, redirectResponse.url(), wasPotentiallyInitiatedByUser);
#endif

    if (m_policyClient)
        m_policyClient->decidePolicyForNavigationAction(*this, &frame, WTFMove(navigationAction), originatingFrame, originalRequest, WTFMove(request), WTFMove(listener));
    else {
#if HAVE(APP_SSO)
        if (m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision || !m_preferences->isExtensibleSSOEnabled())
            navigationAction->unsetShouldPerformSOAuthorization();
#endif

        m_navigationClient->decidePolicyForNavigationAction(*this, WTFMove(navigationAction), WTFMove(listener));
    }

    m_shouldSuppressAppLinksInNextNavigationPolicyDecision = false;

#if HAVE(APP_SSO)
    m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = false;
#endif
}

RefPtr<WebPageProxy> WebPageProxy::nonEphemeralWebPageProxy()
{
    auto processPools = WebProcessPool::allProcessPools();
    if (processPools.isEmpty())
        return nullptr;
    
    for (auto& webProcess : processPools[0]->processes()) {
        for (auto& page : webProcess->pages()) {
            if (!page || page->sessionID().isEphemeral())
                continue;
            return page;
        }
    }
    return nullptr;
}

#if ENABLE(TRACKING_PREVENTION)
void WebPageProxy::logFrameNavigation(const WebFrameProxy& frame, const URL& pageURL, const WebCore::ResourceRequest& request, const URL& redirectURL, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(RunLoop::isMain());
    
    auto sourceURL = redirectURL;
    bool isRedirect = !redirectURL.isNull();
    if (!isRedirect) {
        sourceURL = frame.url();
        if (sourceURL.isNull())
            sourceURL = pageURL;
    }
    
    auto& targetURL = request.url();
    
    if (!targetURL.isValid() || !pageURL.isValid())
        return;
    
    auto targetHost = targetURL.host();
    auto mainFrameHost = pageURL.host();
    
    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == sourceURL.host())
        return;

    websiteDataStore().networkProcess().send(Messages::NetworkProcess::LogFrameNavigation(m_websiteDataStore->sessionID(), RegistrableDomain { targetURL }, RegistrableDomain { pageURL }, RegistrableDomain { sourceURL }, isRedirect, frame.isMainFrame(), MonotonicTime::now() - m_didFinishDocumentLoadForMainFrameTimestamp, wasPotentiallyInitiatedByUser), 0);
}
#endif

void WebPageProxy::decidePolicyForNavigationActionSync(FrameIdentifier frameID, bool isMainFrame, FrameInfoData&& frameInfo, PolicyCheckIdentifier identifier,
    uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID,
    const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse,
    CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame) {
        // This synchronous IPC message was processed before the asynchronous DidCreateMainFrame / DidCreateSubframe one so we do not know about this frameID yet.
        if (isMainFrame)
            didCreateMainFrame(frameID);
        else {
            MESSAGE_CHECK(m_process, frameInfo.parentFrameID);
            RefPtr parentFrame = WebFrameProxy::webFrame(*frameInfo.parentFrameID);
            MESSAGE_CHECK(m_process, parentFrame);
            parentFrame->didCreateSubframe(frameID);
        }
    }

    decidePolicyForNavigationActionSyncShared(m_process.copyRef(), m_webPageID, frameID, isMainFrame, WTFMove(frameInfo), identifier, navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), WTFMove(reply));
}

void WebPageProxy::decidePolicyForNavigationActionSyncShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, FrameIdentifier frameID, bool isMainFrame, FrameInfoData&& frameInfo, PolicyCheckIdentifier identifier, uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse, CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    auto sender = PolicyDecisionSender::create(identifier, WTFMove(reply));

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);

    decidePolicyForNavigationAction(WTFMove(process), webPageID, *frame, WTFMove(frameInfo), navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), sender.copyRef());

    // If the client did not respond synchronously, proceed with the load.
    sender->send(PolicyDecision { sender->identifier(), isNavigatingToAppBoundDomain(), PolicyAction::Use, navigationID, std::nullopt, std::nullopt });
}

void WebPageProxy::decidePolicyForNewWindowAction(FrameIdentifier frameID, FrameInfoData&& frameInfo, PolicyCheckIdentifier identifier, NavigationActionData&& navigationActionData, ResourceRequest&& request, const String& frameName, uint64_t listenerID)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK_URL(m_process, request.url());

    RefPtr<API::FrameInfo> sourceFrameInfo;
    if (frame)
        sourceFrameInfo = API::FrameInfo::create(WTFMove(frameInfo), this);

    auto userInitiatedActivity = m_process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    bool shouldOpenAppLinks = m_mainFrame && m_mainFrame->url().host() != request.url().host();
    auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), nullptr, frameName, ResourceRequest(request), URL { }, shouldOpenAppLinks, WTFMove(userInitiatedActivity));
    
    Ref listener = frame->setUpPolicyListenerProxy([this, protectedThis = Ref { *this }, identifier, listenerID, frameID, navigationAction] (PolicyAction policyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain) mutable {
        // FIXME: Assert the API::WebsitePolicies* is nullptr here once clients of WKFramePolicyListenerUseWithPolicies go away.
        RELEASE_ASSERT(processSwapRequestedByClient == ProcessSwapRequestedByClient::No);
        ASSERT_UNUSED(safeBrowsingWarning, !safeBrowsingWarning);

        auto sender = PolicyDecisionSender::create(identifier, [this, protectedThis = WTFMove(protectedThis), frameID, listenerID] (const auto& policyDecision) {
            send(Messages::WebPage::DidReceivePolicyDecision(frameID, listenerID, policyDecision
#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
                , createNetworkExtensionsSandboxExtensions(m_process)
#endif
            ));
        });

        receivedPolicyDecision(policyAction, nullptr, nullptr, WTFMove(navigationAction), WTFMove(sender));
    }, ShouldExpectSafeBrowsingResult::No, ShouldExpectAppBoundDomainResult::No);

    if (m_policyClient)
        m_policyClient->decidePolicyForNewWindowAction(*this, *frame, navigationAction.get(), request, frameName, WTFMove(listener));
    else
        m_navigationClient->decidePolicyForNavigationAction(*this, navigationAction.get(), WTFMove(listener));
}

void WebPageProxy::decidePolicyForResponse(FrameIdentifier frameID, FrameInfoData&& frameInfo, PolicyCheckIdentifier identifier,
    uint64_t navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, uint64_t listenerID)
{
    decidePolicyForResponseShared(m_process.copyRef(), m_webPageID, frameID, WTFMove(frameInfo), identifier, navigationID, response, request, canShowMIMEType, downloadAttribute, listenerID);
}

void WebPageProxy::decidePolicyForResponseShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, FrameIdentifier frameID, FrameInfoData&& frameInfo, PolicyCheckIdentifier identifier, uint64_t navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, uint64_t listenerID)
{
    PageClientProtector protector(pageClient());

    m_decidePolicyForResponseRequest = request;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK_URL(process, request.url());
    MESSAGE_CHECK_URL(process, response.url());
    RefPtr<API::Navigation> navigation = navigationID ? m_navigationState->navigation(navigationID) : nullptr;
    auto navigationResponse = API::NavigationResponse::create(API::FrameInfo::create(WTFMove(frameInfo), this).get(), request, response, canShowMIMEType, downloadAttribute);

    Ref listener = frame->setUpPolicyListenerProxy([this, protectedThis = Ref { *this }, webPageID, frameID, identifier, listenerID, navigation = WTFMove(navigation),
        process, navigationResponse] (PolicyAction policyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain) mutable {
        // FIXME: Assert the API::WebsitePolicies* is nullptr here once clients of WKFramePolicyListenerUseWithPolicies go away.
        RELEASE_ASSERT(processSwapRequestedByClient == ProcessSwapRequestedByClient::No);
        ASSERT_UNUSED(safeBrowsingWarning, !safeBrowsingWarning);

        auto sender = PolicyDecisionSender::create(identifier, [webPageID, frameID, listenerID, process] (const auto& policyDecision) {
            process->send(Messages::WebPage::DidReceivePolicyDecision(frameID, listenerID, policyDecision
#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
            , createNetworkExtensionsSandboxExtensions(process)
#endif
        ), webPageID);
        });
#if USE(QUICK_LOOK)
        if (policyAction == PolicyAction::Use && process->lockdownMode() == WebProcessProxy::LockdownMode::Enabled && (MIMETypeRegistry::isPDFOrPostScriptMIMEType(navigationResponse->response().mimeType()) || MIMETypeRegistry::isSupportedModelMIMEType(navigationResponse->response().mimeType()) || PreviewConverter::supportsMIMEType(navigationResponse->response().mimeType())))
            policyAction = PolicyAction::Download;
#endif
        receivedPolicyDecision(policyAction, navigation.get(), nullptr, WTFMove(navigationResponse), WTFMove(sender));
    }, ShouldExpectSafeBrowsingResult::No, ShouldExpectAppBoundDomainResult::No);

    if (m_policyClient)
        m_policyClient->decidePolicyForResponse(*this, *frame, response, request, canShowMIMEType, WTFMove(listener));
    else
        m_navigationClient->decidePolicyForNavigationResponse(*this, WTFMove(navigationResponse), WTFMove(listener));
}

void WebPageProxy::triggerBrowsingContextGroupSwitchForNavigation(uint64_t navigationID, BrowsingContextGroupSwitchDecision browsingContextGroupSwitchDecision, const RegistrableDomain& responseDomain, NetworkResourceLoadIdentifier existingNetworkResourceLoadIdentifierToResume, CompletionHandler<void(bool success)>&& completionHandler)
{
    ASSERT(browsingContextGroupSwitchDecision != BrowsingContextGroupSwitchDecision::StayInGroup);
    RefPtr<API::Navigation> navigation = navigationID ? m_navigationState->navigation(navigationID) : nullptr;
    WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "triggerBrowsingContextGroupSwitchForNavigation: Process-swapping due to Cross-Origin-Opener-Policy, newProcessIsCrossOriginIsolated=%d, navigation=%p existingNetworkResourceLoadIdentifierToResume=%" PRIu64, browsingContextGroupSwitchDecision == BrowsingContextGroupSwitchDecision::NewIsolatedGroup, navigation.get(), existingNetworkResourceLoadIdentifierToResume.toUInt64());
    if (!navigation)
        return completionHandler(false);

    RefPtr<WebProcessProxy> processForNavigation;
    if (browsingContextGroupSwitchDecision == BrowsingContextGroupSwitchDecision::NewIsolatedGroup)
        processForNavigation = m_process->processPool().createNewWebProcess(&websiteDataStore(), m_process->lockdownMode(), WebProcessProxy::IsPrewarmed::No, CrossOriginMode::Isolated);
    else
        processForNavigation = m_process->processPool().processForRegistrableDomain(websiteDataStore(), responseDomain, m_process->lockdownMode());

    auto processIdentifier = processForNavigation->coreProcessIdentifier();
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(processIdentifier, RegistrableDomain(navigation->currentRequest().url())), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), processForNavigation = WTFMove(processForNavigation), existingNetworkResourceLoadIdentifierToResume, navigation] () mutable {
        // Tell committed process to stop loading since we're going to do the provisional load in a provisional page now.
        if (!m_provisionalPage)
            send(Messages::WebPage::StopLoadingDueToProcessSwap());
        continueNavigationInNewProcess(*navigation, *m_mainFrame, nullptr, processForNavigation.releaseNonNull(), ProcessSwapRequestedByClient::No, ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted, existingNetworkResourceLoadIdentifierToResume);
        completionHandler(true);
    });
}

// FormClient

void WebPageProxy::willSubmitForm(FrameIdentifier frameID, FrameIdentifier sourceFrameID, const Vector<std::pair<String, String>>& textFieldValues, FormSubmitListenerIdentifier listenerID, const UserData& userData)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto* sourceFrame = WebFrameProxy::webFrame(sourceFrameID);
    MESSAGE_CHECK(m_process, sourceFrame);

    for (auto& pair : textFieldValues)
        MESSAGE_CHECK(m_process, API::Dictionary::MapType::isValidKey(pair.first));

    m_formClient->willSubmitForm(*this, *frame, *sourceFrame, textFieldValues, m_process->transformHandlesToObjects(userData.object()).get(), [protectedThis = Ref { *this }, frame, listenerID]() {
        frame->send(Messages::WebFrame::ContinueWillSubmitForm(listenerID));
    });
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebPageProxy::contentRuleListNotification(URL&& url, ContentRuleListResults&& results)
{
    m_navigationClient->contentRuleListNotification(*this, WTFMove(url), WTFMove(results));
}
#endif
    
void WebPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    didNavigateWithNavigationDataShared(m_process.copyRef(), store, frameID);
}

void WebPageProxy::didNavigateWithNavigationDataShared(Ref<WebProcessProxy>&& process, const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didNavigateWithNavigationDataShared:");

    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->page() == this);

    if (frame->isMainFrame())
        m_historyClient->didNavigateWithNavigationData(*this, store);
    process->processPool().historyClient().didNavigateWithNavigationData(process->processPool(), *this, store, *frame);
}

void WebPageProxy::didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    didPerformClientRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void WebPageProxy::didPerformClientRedirectShared(Ref<WebProcessProxy>&& process, const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    PageClientProtector protector(pageClient());

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->page() == this);
    MESSAGE_CHECK_URL(process, sourceURLString);
    MESSAGE_CHECK_URL(process, destinationURLString);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didPerformClientRedirectShared: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame()) {
        m_historyClient->didPerformClientRedirect(*this, sourceURLString, destinationURLString);
        m_navigationClient->didPerformClientRedirect(*this, sourceURLString, destinationURLString);
    }
    process->processPool().historyClient().didPerformClientRedirect(process->processPool(), *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    didPerformServerRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void WebPageProxy::didPerformServerRedirectShared(Ref<WebProcessProxy>&& process, const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didPerformServerRedirect:");

    PageClientProtector protector(pageClient());

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->page() == this);

    MESSAGE_CHECK_URL(process, sourceURLString);
    MESSAGE_CHECK_URL(process, destinationURLString);

    if (frame->isMainFrame())
        m_historyClient->didPerformServerRedirect(*this, sourceURLString, destinationURLString);
    process->processPool().historyClient().didPerformServerRedirect(process->processPool(), *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didUpdateHistoryTitle(const String& title, const String& url, FrameIdentifier frameID)
{
    PageClientProtector protector(pageClient());

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK(m_process, frame->page() == this);

    MESSAGE_CHECK_URL(m_process, url);

    if (frame->isMainFrame())
        m_historyClient->didUpdateHistoryTitle(*this, title, url);
    process().processPool().historyClient().didUpdateHistoryTitle(process().processPool(), *this, title, url, *frame);
}

// UIClient

using NewPageCallback = CompletionHandler<void(RefPtr<WebPageProxy>&&)>;
using UIClientCallback = Function<void(Ref<API::NavigationAction>&&, NewPageCallback&&)>;
static void trySOAuthorization(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
#if HAVE(APP_SSO)
    if (page.preferences().isExtensibleSSOEnabled()) {
        page.websiteDataStore().soAuthorizationCoordinator(page).tryAuthorize(WTFMove(navigationAction), page, WTFMove(newPageCallback), WTFMove(uiClientCallback));
        return;
    }
#endif
    uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
}

void WebPageProxy::createNewPage(FrameInfoData&& originatingFrameInfoData, WebPageProxyIdentifier originatingPageID, ResourceRequest&& request, WindowFeatures&& windowFeatures, NavigationActionData&& navigationActionData, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebKit::WebPageCreationParameters>)>&& reply)
{
    MESSAGE_CHECK(m_process, originatingFrameInfoData.frameID);
    MESSAGE_CHECK(m_process, WebFrameProxy::webFrame(*originatingFrameInfoData.frameID));

    auto originatingPage = m_process->webPage(originatingPageID);
    auto originatingFrameInfo = API::FrameInfo::create(WTFMove(originatingFrameInfoData), WTFMove(originatingPage));
    auto mainFrameURL = m_mainFrame ? m_mainFrame->url() : URL();

    std::optional<bool> openerAppInitiatedState;
    if (auto* page = originatingFrameInfo->page())
        openerAppInitiatedState = page->lastNavigationWasAppInitiated();

    auto completionHandler = [this, protectedThis = Ref { *this }, mainFrameURL, request, reply = WTFMove(reply), privateClickMeasurement = navigationActionData.privateClickMeasurement, openerAppInitiatedState = WTFMove(openerAppInitiatedState)] (RefPtr<WebPageProxy> newPage) mutable {
        if (!newPage) {
            reply(std::nullopt, std::nullopt);
            return;
        }

        newPage->setOpenedByDOM();

        if (openerAppInitiatedState)
            newPage->m_lastNavigationWasAppInitiated = *openerAppInitiatedState;

        reply(newPage->webPageID(), newPage->creationParameters(m_process, *newPage->drawingArea()));

        newPage->m_shouldSuppressAppLinksInNextNavigationPolicyDecision = mainFrameURL.host() == request.url().host();

        if (privateClickMeasurement)
            newPage->m_privateClickMeasurement = {{ WTFMove(*privateClickMeasurement), { }, { }}};
#if HAVE(APP_SSO)
        newPage->m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = true;
#endif
    };

    RefPtr<API::UserInitiatedAction> userInitiatedActivity;
    
#if ENABLE(TRACKING_PREVENTION)
    // WebKit cancels the original gesture to open the BBC radio player so
    // we can call the Storage Access API first. When we re-initiate the open,
    // we should make sure the client knows that this was user initiated so it
    // does not block the popup.
    if (request.url().string() == Quirks::staticRadioPlayerURLString())
        userInitiatedActivity = API::UserInitiatedAction::create();
    else
#endif
        userInitiatedActivity = m_process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);

    bool shouldOpenAppLinks = originatingFrameInfo->request().url().host() != request.url().host();
    auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), originatingFrameInfo.ptr(), nullptr, std::nullopt, WTFMove(request), URL(), shouldOpenAppLinks, WTFMove(userInitiatedActivity));

    trySOAuthorization(WTFMove(navigationAction), *this, WTFMove(completionHandler), [this, protectedThis = Ref { *this }, windowFeatures = WTFMove(windowFeatures)] (Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler) mutable {
        m_uiClient->createNewPage(*this, WTFMove(windowFeatures), WTFMove(navigationAction), WTFMove(completionHandler));
    });
}
    
void WebPageProxy::showPage()
{
    m_uiClient->showPage(this);
}

void WebPageProxy::exitFullscreenImmediately()
{
#if ENABLE(FULLSCREEN_API)
    if (fullScreenManager())
        fullScreenManager()->close();
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (videoFullscreenManager())
        videoFullscreenManager()->requestHideAndExitFullscreen();
#endif
}

void WebPageProxy::fullscreenMayReturnToInline()
{
    m_uiClient->fullscreenMayReturnToInline(this);
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebPageProxy::didEnterFullscreen(PlaybackSessionContextIdentifier identifier)
{
    m_uiClient->didEnterFullscreen(this);

    m_currentFullscreenVideoSessionIdentifier = identifier;
    updateFullscreenVideoTextRecognition();
}

void WebPageProxy::didExitFullscreen(PlaybackSessionContextIdentifier identifier)
{
    if (m_screenOrientationManager)
        m_screenOrientationManager->unlockIfNecessary();

    m_uiClient->didExitFullscreen(this);

    if (m_currentFullscreenVideoSessionIdentifier == identifier) {
        m_currentFullscreenVideoSessionIdentifier = std::nullopt;
        updateFullscreenVideoTextRecognition();
    }
}

void WebPageProxy::failedToEnterFullscreen(PlaybackSessionContextIdentifier identifier)
{
}

#else

void WebPageProxy::didEnterFullscreen()
{
    m_uiClient->didEnterFullscreen(this);
}

void WebPageProxy::didExitFullscreen()
{
    if (m_screenOrientationManager)
        m_screenOrientationManager->unlockIfNecessary();

    m_uiClient->didExitFullscreen(this);
}

#endif

void WebPageProxy::closePage()
{
    if (isClosed())
        return;

    WEBPAGEPROXY_RELEASE_LOG(Process, "closePage:");
    pageClient().clearAllEditCommands();
    m_uiClient->close(this);
}

void WebPageProxy::runModalJavaScriptDialog(RefPtr<WebFrameProxy>&& frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void(WebPageProxy&, WebFrameProxy* frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void()>&&)>&& runDialogCallback)
{
    pageClient().runModalJavaScriptDialog([weakThis = WeakPtr { *this }, frameInfo = WTFMove(frameInfo), frame = WTFMove(frame), message, runDialogCallback = WTFMove(runDialogCallback)]() mutable {
        RefPtr protectedThis { weakThis.get() };
        if (!protectedThis)
            return;

        protectedThis->m_isRunningModalJavaScriptDialog = true;
        runDialogCallback(*protectedThis, frame.get(), WTFMove(frameInfo), message, [weakThis = WTFMove(weakThis)]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->m_isRunningModalJavaScriptDialog = false;
        });
    });
}

void WebPageProxy::runJavaScriptAlert(FrameIdentifier frameID, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void()>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    exitFullscreenImmediately();

    // Since runJavaScriptAlert() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }

    runModalJavaScriptDialog(WTFMove(frame), WTFMove(frameInfo), message, [reply = WTFMove(reply)](WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void()>&& completion) mutable {
        page.m_uiClient->runJavaScriptAlert(page, message, frame, WTFMove(frameInfo), [reply = WTFMove(reply), completion = WTFMove(completion)]() mutable {
            reply();
            completion();
        });
    });
}

void WebPageProxy::runJavaScriptConfirm(FrameIdentifier frameID, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void(bool)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    exitFullscreenImmediately();

    // Since runJavaScriptConfirm() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }

    runModalJavaScriptDialog(WTFMove(frame), WTFMove(frameInfo), message, [reply = WTFMove(reply)](WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void()>&& completion) mutable {
        page.m_uiClient->runJavaScriptConfirm(page, message, frame, WTFMove(frameInfo), [reply = WTFMove(reply), completion = WTFMove(completion)](bool result) mutable {
            reply(result);
            completion();
        });
    });
}

void WebPageProxy::runJavaScriptPrompt(FrameIdentifier frameID, FrameInfoData&& frameInfo, const String& message, const String& defaultValue, CompletionHandler<void(const String&)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    exitFullscreenImmediately();

    // Since runJavaScriptPrompt() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this);
    }

    runModalJavaScriptDialog(WTFMove(frame), WTFMove(frameInfo), message, [reply = WTFMove(reply), defaultValue](WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void()>&& completion) mutable {
        page.m_uiClient->runJavaScriptPrompt(page, message, defaultValue, frame, WTFMove(frameInfo), [reply = WTFMove(reply), completion = WTFMove(completion)](auto& result) mutable {
            reply(result);
            completion();
        });
    });
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient->setStatusText(this, text);
}

void WebPageProxy::mouseDidMoveOverElement(WebHitTestResultData&& hitTestResultData, uint32_t opaqueModifiers, UserData&& userData)
{
    m_lastMouseMoveHitTestResult = API::HitTestResult::create(hitTestResultData);
    auto modifiers = OptionSet<WebEventModifier>::fromRaw(opaqueModifiers);
    m_uiClient->mouseDidMoveOverElement(*this, hitTestResultData, modifiers, m_process->transformHandlesToObjects(userData.object()).get());
    setToolTip(hitTestResultData.toolTipText);
}

void WebPageProxy::setToolbarsAreVisible(bool toolbarsAreVisible)
{
    m_uiClient->setToolbarsAreVisible(*this, toolbarsAreVisible);
}

void WebPageProxy::getToolbarsAreVisible(CompletionHandler<void(bool)>&& reply)
{
    m_uiClient->toolbarsAreVisible(*this, WTFMove(reply));
}

void WebPageProxy::setMenuBarIsVisible(bool menuBarIsVisible)
{
    m_uiClient->setMenuBarIsVisible(*this, menuBarIsVisible);
}

void WebPageProxy::getMenuBarIsVisible(CompletionHandler<void(bool)>&& reply)
{
    m_uiClient->menuBarIsVisible(*this, WTFMove(reply));
}

void WebPageProxy::setStatusBarIsVisible(bool statusBarIsVisible)
{
    m_uiClient->setStatusBarIsVisible(*this, statusBarIsVisible);
}

void WebPageProxy::getStatusBarIsVisible(CompletionHandler<void(bool)>&& reply)
{
    m_uiClient->statusBarIsVisible(*this, WTFMove(reply));
}

void WebPageProxy::setIsResizable(bool isResizable)
{
    m_uiClient->setIsResizable(*this, isResizable);
}

void WebPageProxy::setWindowFrame(const FloatRect& newWindowFrame)
{
    m_uiClient->setWindowFrame(*this, pageClient().convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(CompletionHandler<void(const FloatRect&)>&& reply)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, reply = WTFMove(reply)] (FloatRect frame) mutable {
        reply(pageClient().convertToUserSpace(frame));
    });
}

void WebPageProxy::getWindowFrameWithCallback(Function<void(FloatRect)>&& completionHandler)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (FloatRect frame) {
        completionHandler(pageClient().convertToUserSpace(frame));
    });
}

void WebPageProxy::screenToRootView(const IntPoint& screenPoint, CompletionHandler<void(const IntPoint&)>&& reply)
{
    reply(pageClient().screenToRootView(screenPoint));
}
    
void WebPageProxy::rootViewToScreen(const IntRect& viewRect, CompletionHandler<void(const IntRect&)>&& reply)
{
    reply(pageClient().rootViewToScreen(viewRect));
}

IntRect WebPageProxy::syncRootViewToScreen(const IntRect& viewRect)
{
    return pageClient().rootViewToScreen(viewRect);
}

void WebPageProxy::accessibilityScreenToRootView(const IntPoint& screenPoint, CompletionHandler<void(IntPoint)>&& completionHandler)
{
    completionHandler(pageClient().accessibilityScreenToRootView(screenPoint));
}

void WebPageProxy::rootViewToAccessibilityScreen(const IntRect& viewRect, CompletionHandler<void(IntRect)>&& completionHandler)
{
    completionHandler(pageClient().rootViewToAccessibilityScreen(viewRect));
}

void WebPageProxy::runBeforeUnloadConfirmPanel(FrameIdentifier frameID, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void(bool)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    // Per §18 User Prompts in the WebDriver spec, "User prompts that are spawned from beforeunload
    // event handlers, are dismissed implicitly upon navigation or close window, regardless of the
    // defined user prompt handler." So, always allow the unload to proceed if the page is being automated.
    if (m_controlledByAutomation) {
        if (!!process().processPool().automationSession()) {
            reply(true);
            return;
        }
    }

    // Since runBeforeUnloadConfirmPanel() can spin a nested run loop we need to turn off the responsiveness timer and the tryClose timer.
    m_process->stopResponsivenessTimer();
    bool shouldResumeTimerAfterPrompt = m_tryCloseTimeoutTimer.isActive();
    m_tryCloseTimeoutTimer.stop();
    m_uiClient->runBeforeUnloadConfirmPanel(*this, message, frame.get(), WTFMove(frameInfo),
        [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(reply), shouldResumeTimerAfterPrompt](bool shouldClose) mutable {
            if (weakThis && shouldResumeTimerAfterPrompt)
                m_tryCloseTimeoutTimer.startOneShot(tryCloseTimeoutDelay);
            completionHandler(shouldClose);
    });
}

void WebPageProxy::didChangeViewportProperties(const ViewportAttributes& attr)
{
    pageClient().didChangeViewportProperties(attr);
}

void WebPageProxy::pageDidScroll(const WebCore::IntPoint& scrollPosition)
{
    m_uiClient->pageDidScroll(this);

    pageClient().pageDidScroll(scrollPosition);

#if PLATFORM(IOS_FAMILY)
    // Do not hide the validation message if the scrolling was caused by the keyboard showing up.
    if (m_isKeyboardAnimatingIn)
        return;
#endif

#if !PLATFORM(IOS_FAMILY)
    closeOverlayedViews();
#endif
}

void WebPageProxy::setHasActiveAnimatedScrolls(bool isRunning)
{
    m_hasActiveAnimatedScroll = isRunning;
#if HAVE(CVDISPLAYLINK)
    updateDisplayLinkFrequency();
#endif
}

void WebPageProxy::runOpenPanel(FrameIdentifier frameID, FrameInfoData&& frameInfo, const FileChooserSettings& settings)
{
    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = nullptr;
    }

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    Ref<API::OpenPanelParameters> parameters = API::OpenPanelParameters::create(settings);
    m_openPanelResultListener = WebOpenPanelResultListenerProxy::create(this);

    if (m_controlledByAutomation) {
        if (auto* automationSession = process().processPool().automationSession())
            automationSession->handleRunOpenPanel(*this, *frame, parameters.get(), *m_openPanelResultListener);

        // Don't show a file chooser, since automation will be unable to interact with it.
        return;
    }

    // Since runOpenPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    const auto frameInfoForPageClient = frameInfo;

    if (!m_uiClient->runOpenPanel(*this, frame.get(), WTFMove(frameInfo), parameters.ptr(), m_openPanelResultListener.get())) {
        if (!pageClient().handleRunOpenPanel(this, frame.get(), frameInfoForPageClient, parameters.ptr(), m_openPanelResultListener.get()))
            didCancelForOpenPanel();
    }
}

void WebPageProxy::showShareSheet(const ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(m_process, !shareData.url || shareData.url->protocolIsInHTTPFamily() || shareData.url->protocolIsData());
    MESSAGE_CHECK(m_process, shareData.files.isEmpty() || m_preferences->webShareFileAPIEnabled());
    MESSAGE_CHECK(m_process, shareData.originator == ShareDataOriginator::Web);
    pageClient().showShareSheet(shareData, WTFMove(completionHandler));
}

void WebPageProxy::showContactPicker(const WebCore::ContactsRequestData& requestData, CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& completionHandler)
{
    MESSAGE_CHECK(m_process, m_preferences->contactPickerAPIEnabled());
    pageClient().showContactPicker(requestData, WTFMove(completionHandler));
}
    
void WebPageProxy::printFrame(FrameIdentifier frameID, const String& title, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!m_isPerformingDOMPrintOperation);
    m_isPerformingDOMPrintOperation = true;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    frame->didChangeTitle(title);

    m_uiClient->printFrame(*this, *frame, pdfFirstPageSize, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        endPrinting(); // Send a message synchronously while m_isPerformingDOMPrintOperation is still true.
        m_isPerformingDOMPrintOperation = false;
        completionHandler();
    });
}

void WebPageProxy::setMediaVolume(float volume)
{
    if (volume == m_mediaVolume)
        return;
    
    m_mediaVolume = volume;
    
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::SetMediaVolume(volume));
}

void WebPageProxy::setMuted(WebCore::MediaProducerMutedStateFlags state, CompletionHandler<void()>&& completionHandler)
{
    if (!isAllowedToChangeMuteState())
        state.add(WebCore::MediaProducer::MediaStreamCaptureIsMuted);

    m_mutedState = state;

    if (!hasRunningProcess())
        return completionHandler();

#if ENABLE(MEDIA_STREAM)
    bool hasMutedCaptureStreams = m_mediaState.containsAny(WebCore::MediaProducer::MutedCaptureMask);
    if (hasMutedCaptureStreams && !(state.containsAny(WebCore::MediaProducer::MediaStreamCaptureIsMuted)))
        WebProcessProxy::muteCaptureInPagesExcept(m_webPageID);
#endif

    m_process->pageMutedStateChanged(m_webPageID, state);

    sendWithAsyncReply(Messages::WebPage::SetMuted(state), WTFMove(completionHandler));
    activityStateDidChange({ ActivityState::IsAudible, ActivityState::IsCapturingMedia });
}

void WebPageProxy::setMediaCaptureEnabled(bool enabled)
{
    m_mediaCaptureEnabled = enabled;

    if (!hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().setCaptureEnabled(enabled);
#endif
}

void WebPageProxy::stopMediaCapture(MediaProducerMediaCaptureKind kind, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess())
        return completionHandler();

#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager->resetAccess();
    sendWithAsyncReply(Messages::WebPage::StopMediaCapture(kind), WTFMove(completionHandler));
#endif
}

void WebPageProxy::requestMediaPlaybackState(CompletionHandler<void(MediaPlaybackState)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::RequestMediaPlaybackState(), WTFMove(completionHandler));
}

void WebPageProxy::pauseAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::PauseAllMediaPlayback(), WTFMove(completionHandler));
}

void WebPageProxy::suspendAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    m_suspendMediaPlaybackCounter++;
    if (m_mediaPlaybackIsSuspended) {
        completionHandler();
        return;
    }
    m_mediaPlaybackIsSuspended = true;

    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::SuspendAllMediaPlayback(), WTFMove(completionHandler));
}

void WebPageProxy::resumeAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    if (m_suspendMediaPlaybackCounter > 0)
        m_suspendMediaPlaybackCounter--;

    if (!m_mediaPlaybackIsSuspended || m_suspendMediaPlaybackCounter) {
        completionHandler();
        return;
    }
    m_mediaPlaybackIsSuspended = false;

    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::ResumeAllMediaPlayback(), WTFMove(completionHandler));
}

void WebPageProxy::setMayStartMediaWhenInWindow(bool mayStartMedia)
{
    if (mayStartMedia == m_mayStartMediaWhenInWindow)
        return;

    m_mayStartMediaWhenInWindow = mayStartMedia;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMayStartMediaWhenInWindow(mayStartMedia));
}

void WebPageProxy::handleDownloadRequest(DownloadProxy& download)
{
    pageClient().handleDownloadRequest(download);
}

void WebPageProxy::resumeDownload(const API::Data& resumeData, const String& path, CompletionHandler<void(DownloadProxy*)>&& completionHandler)
{
    auto& download = process().processPool().resumeDownload(websiteDataStore(), this, resumeData, path, CallDownloadDidStart::Yes);
    download.setDestinationFilename(path);
    download.setDidStartCallback(WTFMove(completionHandler));
}

void WebPageProxy::downloadRequest(WebCore::ResourceRequest&& request, CompletionHandler<void(DownloadProxy*)>&& completionHandler)
{
    auto& download = process().processPool().download(websiteDataStore(), this, request, { });
    download.setDidStartCallback(WTFMove(completionHandler));
}

void WebPageProxy::dataTaskWithRequest(WebCore::ResourceRequest&& request, CompletionHandler<void(API::DataTask&)>&& completionHandler)
{
    websiteDataStore().networkProcess().dataTaskWithRequest(*this, sessionID(), WTFMove(request), WTFMove(completionHandler));
}

void WebPageProxy::didChangeContentSize(const IntSize& size)
{
    pageClient().didChangeContentSize(size);
}

void WebPageProxy::didChangeIntrinsicContentSize(const IntSize& intrinsicContentSize)
{
#if USE(APPKIT)
    pageClient().intrinsicContentSizeDidChange(intrinsicContentSize);
#endif
}

#if ENABLE(INPUT_TYPE_COLOR)
void WebPageProxy::showColorPicker(const WebCore::Color& initialColor, const IntRect& elementRect, Vector<WebCore::Color>&& suggestions)
{
    m_colorPicker = pageClient().createColorPicker(this, initialColor, elementRect, WTFMove(suggestions));
    m_colorPicker->showColorPicker(initialColor);
}

void WebPageProxy::setColorPickerColor(const WebCore::Color& color)
{
    if (m_colorPicker)
        m_colorPicker->setSelectedColor(color);
}

void WebPageProxy::endColorPicker()
{
    if (auto colorPicker = std::exchange(m_colorPicker, nullptr))
        colorPicker->endPicker();
}

void WebPageProxy::didChooseColor(const WebCore::Color& color)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidChooseColor(color));
}

void WebPageProxy::didEndColorPicker()
{
    if (std::exchange(m_colorPicker, nullptr)) {
        if (!hasRunningProcess())
            return;

        send(Messages::WebPage::DidEndColorPicker());
    }

}
#endif

#if ENABLE(DATALIST_ELEMENT)

void WebPageProxy::showDataListSuggestions(WebCore::DataListSuggestionInformation&& info)
{
    if (!m_dataListSuggestionsDropdown)
        m_dataListSuggestionsDropdown = pageClient().createDataListSuggestionsDropdown(*this);

    m_dataListSuggestionsDropdown->show(WTFMove(info));
}

void WebPageProxy::handleKeydownInDataList(const String& key)
{
    if (!m_dataListSuggestionsDropdown)
        return;

    m_dataListSuggestionsDropdown->handleKeydownWithIdentifier(key);
}

void WebPageProxy::endDataListSuggestions()
{
    if (m_dataListSuggestionsDropdown)
        m_dataListSuggestionsDropdown->close();
}

void WebPageProxy::didCloseSuggestions()
{
    if (!m_dataListSuggestionsDropdown)
        return;

    m_dataListSuggestionsDropdown = nullptr;
    send(Messages::WebPage::DidCloseSuggestions());
}

void WebPageProxy::didSelectOption(const String& selectedOption)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidSelectDataListOption(selectedOption));
}

#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

void WebPageProxy::showDateTimePicker(WebCore::DateTimeChooserParameters&& params)
{
    if (!m_dateTimePicker)
        m_dateTimePicker = pageClient().createDateTimePicker(*this);

    m_dateTimePicker->showDateTimePicker(WTFMove(params));
}

void WebPageProxy::endDateTimePicker()
{
    if (!m_dateTimePicker)
        return;

    m_dateTimePicker->endPicker();
}

void WebPageProxy::didChooseDate(StringView date)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidChooseDate(date.toString()));
}

void WebPageProxy::didEndDateTimePicker()
{
    m_dateTimePicker = nullptr;
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidEndDateTimePicker());
}

#endif

WebInspectorUIProxy* WebPageProxy::inspector() const
{
    if (isClosed())
        return nullptr;
    return m_inspector.get();
}

void WebPageProxy::resourceLoadDidSendRequest(ResourceLoadInfo&& loadInfo, WebCore::ResourceRequest&& request)
{
    if (m_resourceLoadClient)
        m_resourceLoadClient->didSendRequest(WTFMove(loadInfo), WTFMove(request));
}

void WebPageProxy::resourceLoadDidPerformHTTPRedirection(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request)
{
    if (m_resourceLoadClient)
        m_resourceLoadClient->didPerformHTTPRedirection(WTFMove(loadInfo), WTFMove(response), WTFMove(request));
}

void WebPageProxy::resourceLoadDidReceiveChallenge(ResourceLoadInfo&& loadInfo, WebCore::AuthenticationChallenge&& challenge)
{
    if (m_resourceLoadClient)
        m_resourceLoadClient->didReceiveChallenge(WTFMove(loadInfo), WTFMove(challenge));
}

void WebPageProxy::resourceLoadDidReceiveResponse(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response)
{
    if (m_resourceLoadClient)
        m_resourceLoadClient->didReceiveResponse(WTFMove(loadInfo), WTFMove(response));
}

void WebPageProxy::resourceLoadDidCompleteWithError(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceError&& error)
{
    if (m_resourceLoadClient)
        m_resourceLoadClient->didCompleteWithError(WTFMove(loadInfo), WTFMove(response), WTFMove(error));
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxy* WebPageProxy::fullScreenManager()
{
    return m_fullScreenManager.get();
}

void WebPageProxy::setFullscreenClient(std::unique_ptr<API::FullscreenClient>&& client)
{
    if (!client) {
        m_fullscreenClient = makeUnique<API::FullscreenClient>();
        return;
    }

    m_fullscreenClient = WTFMove(client);
}
#endif
    
#if ENABLE(VIDEO_PRESENTATION_MODE)
PlaybackSessionManagerProxy* WebPageProxy::playbackSessionManager()
{
    return m_playbackSessionManager.get();
}

VideoFullscreenManagerProxy* WebPageProxy::videoFullscreenManager()
{
    return m_videoFullscreenManager.get();
}

void WebPageProxy::setMockVideoPresentationModeEnabled(bool enabled)
{
    m_mockVideoPresentationModeEnabled = enabled;
    if (m_videoFullscreenManager)
        m_videoFullscreenManager->setMockVideoPresentationModeEnabled(enabled);
}
#endif

#if PLATFORM(IOS_FAMILY)
bool WebPageProxy::allowsMediaDocumentInlinePlayback() const
{
    return m_allowsMediaDocumentInlinePlayback;
}

void WebPageProxy::setAllowsMediaDocumentInlinePlayback(bool allows)
{
    if (m_allowsMediaDocumentInlinePlayback == allows)
        return;
    m_allowsMediaDocumentInlinePlayback = allows;

    send(Messages::WebPage::SetAllowsMediaDocumentInlinePlayback(allows));
}
#endif

void WebPageProxy::setHasHadSelectionChangesFromUserInteraction(bool hasHadUserSelectionChanges)
{
    m_hasHadSelectionChangesFromUserInteraction = hasHadUserSelectionChanges;
}

#if HAVE(TOUCH_BAR)

void WebPageProxy::setIsTouchBarUpdateSupressedForHiddenContentEditable(bool ignoreTouchBarUpdate)
{
    m_isTouchBarUpdateSupressedForHiddenContentEditable = ignoreTouchBarUpdate;
}

void WebPageProxy::setIsNeverRichlyEditableForTouchBar(bool isNeverRichlyEditable)
{
    m_isNeverRichlyEditableForTouchBar = isNeverRichlyEditable;
}

#endif

void WebPageProxy::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(m_process, !originIdentifier.isEmpty(), completionHandler(DOMPasteAccessResponse::DeniedForGesture));

    m_pageClient->requestDOMPasteAccess(pasteAccessCategory, elementRect, originIdentifier, WTFMove(completionHandler));
}

// BackForwardList

void WebPageProxy::backForwardAddItem(BackForwardListItemState&& itemState)
{
    backForwardAddItemShared(m_process.copyRef(), WTFMove(itemState));
}

void WebPageProxy::backForwardAddItemShared(Ref<WebProcessProxy>&& process, BackForwardListItemState&& itemState)
{
    URL itemURL { itemState.pageState.mainFrameState.urlString };
#if PLATFORM(COCOA)
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PushStateFilePathRestriction)) {
#endif
    ASSERT(!itemURL.isLocalFile() || process->wasPreviouslyApprovedFileURL(itemURL));
    MESSAGE_CHECK(process, !itemURL.isLocalFile() || process->wasPreviouslyApprovedFileURL(itemURL));
#if PLATFORM(COCOA)
    }
#endif

    auto item = WebBackForwardListItem::create(WTFMove(itemState), identifier());
    item->setResourceDirectoryURL(currentResourceDirectoryURL());
    m_backForwardList->addItem(WTFMove(item));
}

void WebPageProxy::backForwardGoToItem(const BackForwardItemIdentifier& itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    // On process swap, we tell the previous process to ignore the load, which causes it so restore its current back forward item to its previous
    // value. Since the load is really going on in a new provisional process, we want to ignore such requests from the committed process.
    // Any real new load in the committed process would have cleared m_provisionalPage.
    if (m_provisionalPage)
        return completionHandler(m_backForwardList->counts());

    backForwardGoToItemShared(m_process.copyRef(), itemID, WTFMove(completionHandler));
}

void WebPageProxy::backForwardListContainsItem(const WebCore::BackForwardItemIdentifier& itemID, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_backForwardList->itemForID(itemID));
}

void WebPageProxy::backForwardGoToItemShared(Ref<WebProcessProxy>&& process, const BackForwardItemIdentifier& itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(m_process, !WebKit::isInspectorPage(*this), completionHandler(m_backForwardList->counts()));

    auto* item = m_backForwardList->itemForID(itemID);
    if (!item)
        return completionHandler(m_backForwardList->counts());

    m_backForwardList->goToItem(*item);
    completionHandler(m_backForwardList->counts());
}

void WebPageProxy::backForwardItemAtIndex(int32_t index, CompletionHandler<void(std::optional<BackForwardItemIdentifier>&&)>&& completionHandler)
{
    if (auto* item = m_backForwardList->itemAtIndex(index))
        completionHandler(item->itemID());
    else
        completionHandler(std::nullopt);
}

void WebPageProxy::backForwardListCounts(CompletionHandler<void(WebBackForwardListCounts&&)>&& completionHandler)
{
    completionHandler(m_backForwardList->counts());
}

void WebPageProxy::compositionWasCanceled()
{
#if PLATFORM(COCOA)
    pageClient().notifyInputContextAboutDiscardedComposition();
#endif
}

// Undo management

void WebPageProxy::registerEditCommandForUndo(WebUndoStepID commandID, const String& label)
{
    registerEditCommand(WebEditCommandProxy::create(commandID, label, *this), UndoOrRedo::Undo);
}
    
void WebPageProxy::registerInsertionUndoGrouping()
{
#if USE(INSERTION_UNDO_GROUPING)
    pageClient().registerInsertionUndoGrouping();
#endif
}

void WebPageProxy::canUndoRedo(UndoOrRedo action, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(pageClient().canUndoRedo(action));
}

void WebPageProxy::executeUndoRedo(UndoOrRedo action, CompletionHandler<void()>&& completionHandler)
{
    pageClient().executeUndoRedo(action);
    completionHandler();
}

void WebPageProxy::clearAllEditCommands()
{
    pageClient().clearAllEditCommands();
}

void WebPageProxy::didCountStringMatches(const String& string, uint32_t matchCount)
{
    m_findClient->didCountStringMatches(this, string, matchCount);
}

void WebPageProxy::didGetImageForFindMatch(const ImageBufferBackend::Parameters& parameters, ShareableBitmapHandle contentImageHandle, uint32_t matchIndex)
{
    auto image = WebImage::create(parameters, WTFMove(contentImageHandle));
    if (!image) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_findMatchesClient->didGetImageForMatchResult(this, image.get(), matchIndex);
}

void WebPageProxy::setTextIndicator(const TextIndicatorData& indicatorData, uint64_t lifetime)
{
    // FIXME: Make TextIndicatorWindow a platform-independent presentational thing ("TextIndicatorPresentation"?).
#if PLATFORM(COCOA)
    pageClient().setTextIndicator(TextIndicator::create(indicatorData), static_cast<WebCore::TextIndicatorLifetime>(lifetime));
#else
    notImplemented();
#endif
}

void WebPageProxy::clearTextIndicator()
{
#if PLATFORM(COCOA)
    pageClient().clearTextIndicator(WebCore::TextIndicatorDismissalAnimation::FadeOut);
#else
    notImplemented();
#endif
}

void WebPageProxy::setTextIndicatorAnimationProgress(float progress)
{
#if PLATFORM(COCOA)
    pageClient().setTextIndicatorAnimationProgress(progress);
#else
    notImplemented();
#endif
}

void WebPageProxy::didFindString(const String& string, const Vector<WebCore::IntRect>& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrapAround)
{
    m_findClient->didFindString(this, string, matchRects, matchCount, matchIndex, didWrapAround);
}

void WebPageProxy::didFindStringMatches(const String& string, const Vector<Vector<WebCore::IntRect>>& matchRects, int32_t firstIndexAfterSelection)
{
    m_findMatchesClient->didFindStringMatches(this, string, matchRects, firstIndexAfterSelection);
}

void WebPageProxy::didFailToFindString(const String& string)
{
    m_findClient->didFailToFindString(this, string);
}

bool WebPageProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    return m_process->sendMessage(WTFMove(encoder), sendOptions);
}

bool WebPageProxy::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return m_process->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

IPC::Connection* WebPageProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t WebPageProxy::messageSenderDestinationID() const
{
    return m_webPageID.toUInt64();
}

void WebPageProxy::valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex)
{
    send(Messages::WebPage::DidChangeSelectedIndexForActivePopupMenu(newSelectedIndex));
}

void WebPageProxy::setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index)
{
    send(Messages::WebPage::SetTextForActivePopupMenu(index));
}

bool WebPageProxy::isProcessingKeyboardEvents() const
{
    return !m_keyEventQueue.isEmpty();
}

bool WebPageProxy::isProcessingMouseEvents() const
{
    return !m_mouseEventQueue.isEmpty();
}

bool WebPageProxy::isProcessingWheelEvents() const
{
    return m_wheelEventCoalescer && m_wheelEventCoalescer->hasEventsBeingProcessed();
}

NativeWebMouseEvent* WebPageProxy::currentlyProcessedMouseDownEvent()
{
    // <https://bugs.webkit.org/show_bug.cgi?id=57904> We need to keep track of the mouse down event in the case where we
    // display a popup menu for select elements. When the user changes the selected item, we fake a mouseup event by
    // using this stored mousedown event and changing the event type. This trickery happens when WebProcess handles
    // a mousedown event that runs the default handler for HTMLSelectElement, so the triggering mousedown must be the first event.

    if (m_mouseEventQueue.isEmpty())
        return nullptr;
    
    auto& event = m_mouseEventQueue.first();
    if (event.type() != WebEvent::Type::MouseDown)
        return nullptr;

    return &event;
}

void WebPageProxy::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    if (!hasRunningProcess()) {
        m_pendingInjectedBundleMessages.append(InjectedBundleMessage { messageName, messageBody });
        return;
    }

    send(Messages::WebPage::PostInjectedBundleMessage(messageName, UserData(process().transformObjectsToHandles(messageBody).get())));
}

#if PLATFORM(GTK)
void WebPageProxy::failedToShowPopupMenu()
{
    send(Messages::WebPage::FailedToShowPopupMenu());
}
#endif

void WebPageProxy::showPopupMenu(const IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    MESSAGE_CHECK(m_process, selectedIndex == -1 || static_cast<uint32_t>(selectedIndex) < items.size());

    if (m_activePopupMenu) {
        m_activePopupMenu->hidePopupMenu();
        m_activePopupMenu->invalidate();
        m_activePopupMenu = nullptr;
    }

    // If the page is controlled by automation, entering a nested run loop while the menu is open
    // can hang the page / WebDriver test. Since <option> elements are selected via a different
    // code path anyway, just don't show the native popup menu.
    if (auto* automationSession = process().processPool().automationSession()) {
        if (m_controlledByAutomation && automationSession->isSimulatingUserInteraction())
            return;
    }

    m_activePopupMenu = pageClient().createPopupMenuProxy(*this);

    if (!m_activePopupMenu)
        return;

    // Since showPopupMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    // Showing a popup menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref<WebPageProxy> protect(*this);
    m_activePopupMenu->showPopupMenu(rect, static_cast<TextDirection>(textDirection), m_pageScaleFactor, items, data, selectedIndex);
}

void WebPageProxy::hidePopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->hidePopupMenu();
    m_activePopupMenu->invalidate();
    m_activePopupMenu = nullptr;
}

#if ENABLE(CONTEXT_MENUS)
void WebPageProxy::showContextMenu(ContextMenuContextData&& contextMenuContextData, const UserData& userData)
{
    // Showing a context menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref<WebPageProxy> protect(*this);

    // If the page is controlled by automation, entering a nested run loop while the menu is open
    // can hang the page / WebDriver test. Pretend to show and immediately dismiss the context menu.
    if (auto* automationSession = process().processPool().automationSession()) {
        if (m_controlledByAutomation && automationSession->isSimulatingUserInteraction()) {
            send(Messages::WebPage::DidShowContextMenu());
            return;
        }
    }

    // Discard any enqueued mouse events that have been delivered to the UIProcess whilst the WebProcess is still processing the
    // MouseDown event that triggered this ShowContextMenu message. This can happen if we take too long to enter the nested runloop.
    discardQueuedMouseEvents();

    m_activeContextMenuContextData = contextMenuContextData;

    m_activeContextMenu = pageClient().createContextMenuProxy(*this, WTFMove(contextMenuContextData), userData);

    m_activeContextMenu->show();
}

void WebPageProxy::didShowContextMenu()
{
    // Don't send `Messages::WebPage::DidShowContextMenu` as that should've already been eagerly
    // sent when requesting the context menu to show, regardless of the result of that request.

    pageClient().didShowContextMenu();
}

void WebPageProxy::didDismissContextMenu()
{
    send(Messages::WebPage::DidDismissContextMenu());

    pageClient().didDismissContextMenu();
}

void WebPageProxy::contextMenuItemSelected(const WebContextMenuItemData& item)
{
    // Application custom items don't need to round-trip through to WebCore in the WebProcess.
    if (item.action() >= ContextMenuItemBaseApplicationTag) {
        m_contextMenuClient->customContextMenuItemSelected(*this, item);
        return;
    }

    struct DownloadInfo {
        String url;
        String suggestedFilename;
    };
    std::optional<DownloadInfo> downloadInfo;
    
    ASSERT(m_activeContextMenuContextData.webHitTestResultData());
    
    auto hitTestData = m_activeContextMenuContextData.webHitTestResultData().value();

    switch (item.action()) {
#if PLATFORM(COCOA)
    case ContextMenuItemTagSmartCopyPaste:
        setSmartInsertDeleteEnabled(!isSmartInsertDeleteEnabled());
        return;

    case ContextMenuItemTagSmartQuotes:
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
        m_process->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartDashes:
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
        m_process->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartLinks:
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
        m_process->updateTextCheckerState();
        return;

    case ContextMenuItemTagTextReplacement:
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
        m_process->updateTextCheckerState();
        return;

    case ContextMenuItemTagCorrectSpellingAutomatically:
        TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);
        m_process->updateTextCheckerState();
        return;

    case ContextMenuItemTagShowSubstitutions:
        TextChecker::toggleSubstitutionsPanelIsShowing();
        return;
#endif

    case ContextMenuItemTagDownloadImageToDisk:
        downloadInfo = { { hitTestData.absoluteImageURL, { } } };
        break;

    case ContextMenuItemTagDownloadLinkToDisk: {
        downloadInfo = { { hitTestData.absoluteLinkURL, hitTestData.linkSuggestedFilename } };
        break;
    }

    case ContextMenuItemTagDownloadMediaToDisk:
        downloadInfo = { { hitTestData.absoluteMediaURL, { } } };
        break;

    case ContextMenuItemTagCheckSpellingWhileTyping:
        TextChecker::setContinuousSpellCheckingEnabled(!TextChecker::state().isContinuousSpellCheckingEnabled);
        m_process->updateTextCheckerState();
        return;

    case ContextMenuItemTagCheckGrammarWithSpelling:
        TextChecker::setGrammarCheckingEnabled(!TextChecker::state().isGrammarCheckingEnabled);
        m_process->updateTextCheckerState();
        return;

#if PLATFORM(MAC)
    case ContextMenuItemTagShowFonts:
        showFontPanel();
        return;
    case ContextMenuItemTagStyles:
        showStylesPanel();
        return;
    case ContextMenuItemTagShowColors:
        showColorPanel();
        return;
#endif // PLATFORM(MAC)

    case ContextMenuItemTagShowSpellingPanel:
        if (!TextChecker::spellingUIIsShowing())
            advanceToNextMisspelling(true);
        TextChecker::toggleSpellingUIIsShowing();
        return;

    case ContextMenuItemTagAddHighlightToNewQuickNote:
#if ENABLE(APP_HIGHLIGHTS)
        createAppHighlightInSelectedRange(CreateNewGroupForHighlight::Yes, HighlightRequestOriginatedInApp::No);
#endif
        return;

    case ContextMenuItemTagAddHighlightToCurrentQuickNote:
#if ENABLE(APP_HIGHLIGHTS)
        createAppHighlightInSelectedRange(CreateNewGroupForHighlight::No, HighlightRequestOriginatedInApp::No);
#endif
        return;

    case ContextMenuItemTagLearnSpelling:
    case ContextMenuItemTagIgnoreSpelling:
        ++m_pendingLearnOrIgnoreWordMessageCount;
        break;

    case ContextMenuItemTagLookUpImage:
#if ENABLE(IMAGE_ANALYSIS)
        handleContextMenuLookUpImage();
#endif
        return;

    case ContextMenuItemTagCopySubject:
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        handleContextMenuCopySubject(hitTestData.sourceImageMIMEType);
#endif
        return;

    default:
        break;
    }

    if (downloadInfo) {
        auto& download = m_process->processPool().download(m_websiteDataStore, this, URL { downloadInfo->url }, downloadInfo->suggestedFilename);
        download.setDidStartCallback([this, weakThis = WeakPtr { *this }] (auto* download) {
            if (!weakThis || !download)
                return;
            m_navigationClient->contextMenuDidCreateDownload(*this, *download);
        });
    }

    platformDidSelectItemFromActiveContextMenu(item);

    send(Messages::WebPage::DidSelectItemFromActiveContextMenu(item));
}

void WebPageProxy::handleContextMenuKeyEvent()
{
    send(Messages::WebPage::ContextMenuForKeyEvent());
}
#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(CONTEXT_MENU_EVENT)

void WebPageProxy::dispatchAfterCurrentContextMenuEvent(CompletionHandler<void(bool)>&& completionHandler)
{
    m_contextMenuCallbacks.append(WTFMove(completionHandler));

    processContextMenuCallbacks();
}

void WebPageProxy::processContextMenuCallbacks()
{
    if (m_contextMenuPreventionState == EventPreventionState::Waiting)
        return;

    bool handled = m_contextMenuPreventionState == EventPreventionState::Prevented;

    for (auto&& callback : std::exchange(m_contextMenuCallbacks, { }))
        callback(handled);
}

#endif // ENABLE(CONTEXT_MENU_EVENT)

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>& fileURLs, const String& displayString, const API::Data* iconData)
{
    if (!hasRunningProcess())
        return;

#if ENABLE(SANDBOX_EXTENSIONS)
    auto sandboxExtensionHandles = SandboxExtension::createReadOnlyHandlesForFiles("WebPageProxy::didChooseFilesForOpenPanelWithDisplayStringAndIcon"_s, fileURLs);
    send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(WTFMove(sandboxExtensionHandles)));
#endif

    SandboxExtension::Handle frontboardServicesSandboxExtension, iconServicesSandboxExtension;
    auto auditToken = m_process->auditToken();
#if HAVE(FRONTBOARD_SYSTEM_APP_SERVICES)
    if (auto handle = SandboxExtension::createHandleForMachLookup("com.apple.frontboard.systemappservices"_s, auditToken, SandboxExtension::MachBootstrapOptions::EnableMachBootstrap))
        frontboardServicesSandboxExtension = WTFMove(*handle);
#endif
    if (auto handle = SandboxExtension::createHandleForMachLookup("com.apple.iconservices"_s, auditToken, SandboxExtension::MachBootstrapOptions::EnableMachBootstrap))
        iconServicesSandboxExtension = WTFMove(*handle);

    send(Messages::WebPage::DidChooseFilesForOpenPanelWithDisplayStringAndIcon(fileURLs, displayString, iconData ? iconData->dataReference() : IPC::DataReference(), frontboardServicesSandboxExtension, iconServicesSandboxExtension));

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = nullptr;
}
#endif

bool WebPageProxy::didChooseFilesForOpenPanelWithImageTranscoding(const Vector<String>& fileURLs, const Vector<String>& allowedMIMETypes)
{
#if PLATFORM(MAC)
    auto transcodingMIMEType = WebCore::MIMETypeRegistry::preferredImageMIMETypeForEncoding(allowedMIMETypes, { });
    if (transcodingMIMEType.isNull())
        return false;

    auto transcodingURLs = findImagesForTranscoding(fileURLs, allowedMIMETypes);
    if (transcodingURLs.isEmpty())
        return false;

    auto transcodingUTI = WebCore::UTIFromMIMEType(transcodingMIMEType);
    auto transcodingExtension = WebCore::MIMETypeRegistry::preferredExtensionForMIMEType(transcodingMIMEType);

    sharedImageTranscodingQueue().dispatch([this, protectedThis = Ref { *this }, fileURLs = crossThreadCopy(fileURLs), transcodingURLs = crossThreadCopy(WTFMove(transcodingURLs)), transcodingUTI = WTFMove(transcodingUTI).isolatedCopy(), transcodingExtension = WTFMove(transcodingExtension).isolatedCopy()]() mutable {
        ASSERT(!RunLoop::isMain());

        auto transcodedURLs = transcodeImages(transcodingURLs, transcodingUTI, transcodingExtension);
        ASSERT(transcodingURLs.size() == transcodedURLs.size());

        RunLoop::main().dispatch([this, protectedThis = WTFMove(protectedThis), fileURLs = crossThreadCopy(WTFMove(fileURLs)), transcodedURLs = crossThreadCopy(WTFMove(transcodedURLs))]() {
#if ENABLE(SANDBOX_EXTENSIONS)
            Vector<String> sandboxExtensionFiles;
            for (size_t i = 0, size = fileURLs.size(); i < size; ++i)
                sandboxExtensionFiles.append(!transcodedURLs[i].isNull() ? transcodedURLs[i] : fileURLs[i]);
            auto sandboxExtensionHandles = SandboxExtension::createReadOnlyHandlesForFiles("WebPageProxy::didChooseFilesForOpenPanel"_s, sandboxExtensionFiles);
            send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(WTFMove(sandboxExtensionHandles)));
#endif
            send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs, transcodedURLs));
        });
    });

    return true;
#else
    UNUSED_PARAM(fileURLs);
    UNUSED_PARAM(allowedMIMETypes);
    return false;
#endif
}

void WebPageProxy::didChooseFilesForOpenPanel(const Vector<String>& fileURLs, const Vector<String>& allowedMIMETypes)
{
    if (!hasRunningProcess())
        return;

    if (!didChooseFilesForOpenPanelWithImageTranscoding(fileURLs, allowedMIMETypes)) {
#if ENABLE(SANDBOX_EXTENSIONS)
        auto sandboxExtensionHandles = SandboxExtension::createReadOnlyHandlesForFiles("WebPageProxy::didChooseFilesForOpenPanel"_s, fileURLs);
        send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(WTFMove(sandboxExtensionHandles)));
#endif
        send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs, { }));
    }

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = nullptr;
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidCancelForOpenPanel());
    
    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = nullptr;
}

void WebPageProxy::advanceToNextMisspelling(bool startBeforeSelection)
{
    send(Messages::WebPage::AdvanceToNextMisspelling(startBeforeSelection));
}

void WebPageProxy::changeSpellingToWord(const String& word)
{
    if (word.isEmpty())
        return;

    send(Messages::WebPage::ChangeSpellingToWord(word));
}

void WebPageProxy::registerEditCommand(Ref<WebEditCommandProxy>&& commandProxy, UndoOrRedo undoOrRedo)
{
    MESSAGE_CHECK(m_process, commandProxy->commandID());
    pageClient().registerEditCommand(WTFMove(commandProxy), undoOrRedo);
}

void WebPageProxy::addEditCommand(WebEditCommandProxy& command)
{
    m_editCommandSet.add(&command);
}

void WebPageProxy::removeEditCommand(WebEditCommandProxy& command)
{
    m_editCommandSet.remove(&command);

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::DidRemoveEditCommand(command.commandID()));
}

bool WebPageProxy::canUndo()
{
    return pageClient().canUndoRedo(UndoOrRedo::Undo);
}

bool WebPageProxy::canRedo()
{
    return pageClient().canUndoRedo(UndoOrRedo::Redo);
}

SpellDocumentTag WebPageProxy::spellDocumentTag()
{
    if (!m_spellDocumentTag)
        m_spellDocumentTag = TextChecker::uniqueSpellDocumentTag(this);
    return m_spellDocumentTag.value();
}

#if USE(UNIFIED_TEXT_CHECKING)
void WebPageProxy::checkTextOfParagraph(const String& text, OptionSet<TextCheckingType> checkingTypes, int32_t insertionPoint, CompletionHandler<void(Vector<WebCore::TextCheckingResult>&&)>&& completionHandler)
{
    completionHandler(TextChecker::checkTextOfParagraph(spellDocumentTag(), text, insertionPoint, checkingTypes, m_initialCapitalizationEnabled));
}
#endif

void WebPageProxy::checkSpellingOfString(const String& text, CompletionHandler<void(int32_t misspellingLocation, int32_t misspellingLength)>&& completionHandler)
{
    int32_t misspellingLocation = 0;
    int32_t misspellingLength = 0;
    TextChecker::checkSpellingOfString(spellDocumentTag(), text, misspellingLocation, misspellingLength);
    completionHandler(misspellingLocation, misspellingLength);
}

void WebPageProxy::checkGrammarOfString(const String& text, CompletionHandler<void(Vector<WebCore::GrammarDetail>&&, int32_t badGrammarLocation, int32_t badGrammarLength)>&& completionHandler)
{
    Vector<GrammarDetail> grammarDetails;
    int32_t badGrammarLocation = 0;
    int32_t badGrammarLength = 0;
    TextChecker::checkGrammarOfString(spellDocumentTag(), text, grammarDetails, badGrammarLocation, badGrammarLength);
    completionHandler(WTFMove(grammarDetails), badGrammarLocation, badGrammarLength);
}

void WebPageProxy::spellingUIIsShowing(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(TextChecker::spellingUIIsShowing());
}

void WebPageProxy::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    TextChecker::updateSpellingUIWithMisspelledWord(spellDocumentTag(), misspelledWord);
}

void WebPageProxy::updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const GrammarDetail& grammarDetail)
{
    TextChecker::updateSpellingUIWithGrammarString(spellDocumentTag(), badGrammarPhrase, grammarDetail);
}

void WebPageProxy::getGuessesForWord(const String& word, const String& context, int32_t insertionPoint, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<String> guesses;
    TextChecker::getGuessesForWord(spellDocumentTag(), word, context, insertionPoint, guesses, m_initialCapitalizationEnabled);
    completionHandler(WTFMove(guesses));
}

void WebPageProxy::learnWord(const String& word)
{
    MESSAGE_CHECK(m_process, m_pendingLearnOrIgnoreWordMessageCount);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::learnWord(spellDocumentTag(), word);
}

void WebPageProxy::ignoreWord(const String& word)
{
    MESSAGE_CHECK(m_process, m_pendingLearnOrIgnoreWordMessageCount);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::ignoreWord(spellDocumentTag(), word);
}

void WebPageProxy::requestCheckingOfString(TextCheckerRequestID requestID, const TextCheckingRequestData& request, int32_t insertionPoint)
{
    TextChecker::requestCheckingOfString(TextCheckerCompletion::create(requestID, request, this), insertionPoint);
}

void WebPageProxy::didFinishCheckingText(TextCheckerRequestID requestID, const Vector<WebCore::TextCheckingResult>& result)
{
    send(Messages::WebPage::DidFinishCheckingText(requestID, result));
}

void WebPageProxy::didCancelCheckingText(TextCheckerRequestID requestID)
{
    send(Messages::WebPage::DidCancelCheckingText(requestID));
}

void WebPageProxy::focusFromServiceWorker(CompletionHandler<void()>&& callback)
{
    if (!m_uiClient->focusFromServiceWorker(*this)) {
        callback();
        return;
    }

#if PLATFORM(COCOA)
    makeFirstResponder();
#endif

    if (m_activityState.contains(ActivityState::IsVisible)) {
        callback();
        return;
    }
    installActivityStateChangeCompletionHandler(WTFMove(callback));
}

// Other

void WebPageProxy::setFocus(bool focused)
{
    if (focused)
        m_uiClient->focus(this);
    else
        m_uiClient->unfocus(this);
}

void WebPageProxy::takeFocus(uint8_t direction)
{
    if (m_uiClient->takeFocus(this, (static_cast<FocusDirection>(direction) == FocusDirection::Forward) ? kWKFocusDirectionForward : kWKFocusDirectionBackward))
        return;

    pageClient().takeFocus(static_cast<FocusDirection>(direction));
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    if (m_toolTip == toolTip)
        return;

    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    pageClient().toolTipChanged(oldToolTip, m_toolTip);
}

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    pageClient().setCursor(cursor);
}

void WebPageProxy::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    pageClient().setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
}

void WebPageProxy::didReceiveEvent(uint32_t opaqueType, bool handled)
{
    WebEvent::Type type = static_cast<WebEvent::Type>(opaqueType);

    switch (type) {
    case WebEvent::NoType:
    case WebEvent::MouseMove:
    case WebEvent::Wheel:
        break;

    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
    case WebEvent::MouseForceChanged:
    case WebEvent::MouseForceDown:
    case WebEvent::MouseForceUp:
    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char:
#if ENABLE(TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel:
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEvent::GestureStart:
    case WebEvent::GestureChange:
    case WebEvent::GestureEnd:
#endif
        m_process->stopResponsivenessTimer();
        break;
    }

    switch (type) {
    case WebEvent::NoType:
        break;
    case WebEvent::MouseForceChanged:
    case WebEvent::MouseForceDown:
    case WebEvent::MouseForceUp:
    case WebEvent::MouseMove:
    case WebEvent::MouseDown:
    case WebEvent::MouseUp: {
        LOG(MouseHandling, "WebPageProxy::didReceiveEvent: %s (queue size %zu)", webMouseEventTypeString(type), m_mouseEventQueue.size());

        // Retire the last sent event now that WebProcess is done handling it.
        MESSAGE_CHECK(m_process, !m_mouseEventQueue.isEmpty());
        auto event = m_mouseEventQueue.takeFirst();
        MESSAGE_CHECK(m_process, type == event.type());

#if ENABLE(CONTEXT_MENU_EVENT)
        if (event.button() == WebMouseEventButton::RightButton) {
            if (event.type() == WebEvent::MouseDown) {
                ASSERT(m_contextMenuPreventionState == EventPreventionState::Waiting);
                m_contextMenuPreventionState = handled ? EventPreventionState::Prevented : EventPreventionState::Allowed;
            } else if (m_contextMenuPreventionState != EventPreventionState::Waiting)
                m_contextMenuPreventionState = EventPreventionState::None;

            processContextMenuCallbacks();
        }
#endif

        if (!m_mouseEventQueue.isEmpty()) {
            LOG(MouseHandling, " UIProcess: handling a queued mouse event from didReceiveEvent");
            processNextQueuedMouseEvent();
        } else {
            if (auto* automationSession = process().processPool().automationSession())
                automationSession->mouseEventsFlushedForPage(*this);
            didFinishProcessingAllPendingMouseEvents();
        }
        break;
    }

    case WebEvent::Wheel: {
        MESSAGE_CHECK(m_process, wheelEventCoalescer().hasEventsBeingProcessed());
        auto oldestProcessedEvent = wheelEventCoalescer().takeOldestEventBeingProcessed();

        // FIXME: Dispatch additional events to the didNotHandleWheelEvent client function.
        if (!handled) {
            m_uiClient->didNotHandleWheelEvent(this, oldestProcessedEvent);
            pageClient().wheelEventWasNotHandledByWebCore(oldestProcessedEvent);
        }

        if (auto eventToSend = wheelEventCoalescer().nextEventToDispatch())
            sendWheelEvent(*eventToSend);
        else if (auto* automationSession = process().processPool().automationSession())
            automationSession->wheelEventsFlushedForPage(*this);
        break;
    }

    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char: {
        LOG(KeyHandling, "WebPageProxy::didReceiveEvent: %s (queue empty %d)", webKeyboardEventTypeString(type), m_keyEventQueue.isEmpty());

        MESSAGE_CHECK(m_process, !m_keyEventQueue.isEmpty());
        auto event = m_keyEventQueue.takeFirst();
        MESSAGE_CHECK(m_process, type == event.type());

#if PLATFORM(WIN)
        if (!handled && type == WebEvent::RawKeyDown)
            dispatchPendingCharEvents(event);
#endif

        bool canProcessMoreKeyEvents = !m_keyEventQueue.isEmpty();
        if (canProcessMoreKeyEvents) {
            LOG(KeyHandling, " UI process: sent keyEvent from didReceiveEvent");
            send(Messages::WebPage::KeyEvent(m_keyEventQueue.first()));
        }

        // The call to doneWithKeyEvent may close this WebPage.
        // Protect against this being destroyed.
        Ref<WebPageProxy> protect(*this);

        pageClient().doneWithKeyEvent(event, handled);
        if (!handled)
            m_uiClient->didNotHandleKeyEvent(this, event);

        // Notify the session after -[NSApp sendEvent:] has a crack at turning the event into an action.
        if (!canProcessMoreKeyEvents) {
            if (auto* automationSession = process().processPool().automationSession())
                automationSession->keyboardEventsFlushedForPage(*this);
        }
        break;
    }
#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEvent::GestureStart:
    case WebEvent::GestureChange:
    case WebEvent::GestureEnd: {
        MESSAGE_CHECK(m_process, !m_gestureEventQueue.isEmpty());
        auto event = m_gestureEventQueue.takeFirst();
        MESSAGE_CHECK(m_process, type == event.type());

        if (!handled)
            pageClient().gestureEventWasNotHandledByWebCore(event);
        break;
    }
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel:
        break;
#elif ENABLE(TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel: {
        MESSAGE_CHECK(m_process, !m_touchEventQueue.isEmpty());
        auto queuedEvents = m_touchEventQueue.takeFirst();
        MESSAGE_CHECK(m_process, type == queuedEvents.forwardedEvent.type());

        pageClient().doneWithTouchEvent(queuedEvents.forwardedEvent, handled);
        for (size_t i = 0; i < queuedEvents.deferredTouchEvents.size(); ++i) {
            bool isEventHandled = false;
            pageClient().doneWithTouchEvent(queuedEvents.deferredTouchEvents.at(i), isEventHandled);
        }
        break;
    }
#endif
    }
}

void WebPageProxy::editorStateChanged(const EditorState& editorState)
{
    // FIXME: This should not merge VisualData; they should only be merged
    // if the drawing area says to.
    if (updateEditorState(editorState, ShouldMergeVisualEditorState::Yes))
        dispatchDidUpdateEditorState();
}

bool WebPageProxy::updateEditorState(const EditorState& newEditorState, ShouldMergeVisualEditorState shouldMergeVisualEditorState)
{
    if (shouldMergeVisualEditorState == ShouldMergeVisualEditorState::Default)
        shouldMergeVisualEditorState = (!m_drawingArea || !m_drawingArea->shouldCoalesceVisualEditorStateUpdates()) ? ShouldMergeVisualEditorState::Yes : ShouldMergeVisualEditorState::No;

    bool isStaleEditorState = newEditorState.identifier < m_editorState.identifier;
    bool shouldKeepExistingVisualEditorState = shouldMergeVisualEditorState == ShouldMergeVisualEditorState::No && m_editorState.hasVisualData();
    bool shouldMergeNewVisualEditorState = shouldMergeVisualEditorState == ShouldMergeVisualEditorState::Yes && newEditorState.hasVisualData();
    
    std::optional<EditorState> oldEditorState;
    if (!isStaleEditorState) {
        oldEditorState = std::exchange(m_editorState, newEditorState);
        if (shouldKeepExistingVisualEditorState)
            m_editorState.visualData = oldEditorState->visualData;
    } else if (shouldMergeNewVisualEditorState) {
        oldEditorState = m_editorState;
        m_editorState.visualData = newEditorState.visualData;
    }

    if (oldEditorState) {
        didUpdateEditorState(*oldEditorState, newEditorState);
        return true;
    }

    return false;
}

#if !PLATFORM(IOS_FAMILY)

void WebPageProxy::dispatchDidUpdateEditorState()
{
}

#endif

inline API::DiagnosticLoggingClient* WebPageProxy::effectiveDiagnosticLoggingClient(ShouldSample shouldSample)
{
    // Diagnostic logging is disabled for ephemeral sessions for privacy reasons.
    if (sessionID().isEphemeral())
        return nullptr;

    return DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample) ? diagnosticLoggingClient() : nullptr;
}

void WebPageProxy::logDiagnosticMessage(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessage(this, message, description);
}

void WebPageProxy::logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    logDiagnosticMessage(message, description, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithResult(const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithResult(this, message, description, static_cast<WebCore::DiagnosticLoggingResultType>(result));
}

void WebPageProxy::logDiagnosticMessageWithResultFromWebProcess(const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    logDiagnosticMessageWithResult(message, description, result, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithValue(const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithValue(this, message, description, String::numberToStringFixedPrecision(value, significantFigures));
}

void WebPageProxy::logDiagnosticMessageWithValueFromWebProcess(const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    logDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithEnhancedPrivacy(const String& message, const String& description, ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithEnhancedPrivacy(this, message, description);
}

void WebPageProxy::logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    logDiagnosticMessageWithEnhancedPrivacy(message, description, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithValueDictionary(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    auto apiDictionary = API::Dictionary::create();

    for (auto& keyValuePair : valueDictionary) {
        apiDictionary->add(keyValuePair.key, WTF::switchOn(keyValuePair.value,
            [](const String& value) -> Ref<Object> { return API::String::create(value); },
            [](uint64_t value) -> Ref<Object> { return API::UInt64::create(value); },
            [](int64_t value) -> Ref<Object> { return API::Int64::create(value); },
            [](bool value) -> Ref<Object> { return API::Boolean::create(value); },
            [](double value) -> Ref<Object> { return API::Double::create(value); }
        ));
    }

    effectiveClient->logDiagnosticMessageWithValueDictionary(this, message, description, WTFMove(apiDictionary));
}

void WebPageProxy::logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    logDiagnosticMessageWithValueDictionary(message, description, valueDictionary, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithDomain(const String& message, WebCore::DiagnosticLoggingDomain domain)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(ShouldSample::No);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithDomain(this, message, domain);
}

void WebPageProxy::logDiagnosticMessageWithDomainFromWebProcess(const String& message, WebCore::DiagnosticLoggingDomain domain)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    logDiagnosticMessageWithDomain(message, domain);
}

void WebPageProxy::logScrollingEvent(uint32_t eventType, MonotonicTime timestamp, uint64_t data)
{
    PerformanceLoggingClient::ScrollingEvent event = static_cast<PerformanceLoggingClient::ScrollingEvent>(eventType);

    switch (event) {
    case PerformanceLoggingClient::ScrollingEvent::LoggingEnabled:
        WTFLogAlways("SCROLLING: ScrollingPerformanceTestingEnabled\n");
        break;
    case PerformanceLoggingClient::ScrollingEvent::ExposedTilelessArea:
        WTFLogAlways("SCROLLING: Exposed tileless area. Time: %f Unfilled Pixels: %llu\n", timestamp.secondsSinceEpoch().value(), (unsigned long long)data);
        break;
    case PerformanceLoggingClient::ScrollingEvent::FilledTile:
        WTFLogAlways("SCROLLING: Filled visible fresh tile. Time: %f Unfilled Pixels: %llu\n", timestamp.secondsSinceEpoch().value(), (unsigned long long)data);
        break;
    case PerformanceLoggingClient::ScrollingEvent::SwitchedScrollingMode:
        if (data)
            WTFLogAlways("SCROLLING: Switching to main-thread scrolling mode. Time: %f Reason(s): %s\n", timestamp.secondsSinceEpoch().value(), PerformanceLoggingClient::synchronousScrollingReasonsAsString(OptionSet<SynchronousScrollingReason>::fromRaw(data)).utf8().data());
        else
            WTFLogAlways("SCROLLING: Switching to threaded scrolling mode. Time: %f\n", timestamp.secondsSinceEpoch().value());
        break;
    }
}

void WebPageProxy::focusedFrameChanged(const std::optional<FrameIdentifier>& frameID)
{
    if (!frameID) {
        m_focusedFrame = nullptr;
        return;
    }

    auto* frame = WebFrameProxy::webFrame(*frameID);
    MESSAGE_CHECK(m_process, frame);

    m_focusedFrame = frame;
}

void WebPageProxy::processDidBecomeUnresponsive()
{
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "processDidBecomeUnresponsive:");

    if (!hasRunningProcess())
        return;

    updateBackingStoreDiscardableState();

    m_navigationClient->processDidBecomeUnresponsive(*this);
}

void WebPageProxy::processDidBecomeResponsive()
{
    WEBPAGEPROXY_RELEASE_LOG(Process, "processDidBecomeResponsive:");

    if (!hasRunningProcess())
        return;
    
    updateBackingStoreDiscardableState();

    m_navigationClient->processDidBecomeResponsive(*this);
}

void WebPageProxy::willChangeProcessIsResponsive()
{
    m_pageLoadState.willChangeProcessIsResponsive();
}

void WebPageProxy::didChangeProcessIsResponsive()
{
    m_pageLoadState.didChangeProcessIsResponsive();
}

String WebPageProxy::currentURL() const
{
    String url = m_pageLoadState.activeURL();
    if (url.isEmpty() && m_backForwardList->currentItem())
        url = m_backForwardList->currentItem()->url();
    return url;
}

URL WebPageProxy::currentResourceDirectoryURL() const
{
    auto resourceDirectoryURL = m_pageLoadState.resourceDirectoryURL();
    if (!resourceDirectoryURL.isEmpty())
        return resourceDirectoryURL;
    if (auto* item = m_backForwardList->currentItem())
        return item->resourceDirectoryURL();
    return { };
}

void WebPageProxy::resetStateAfterProcessTermination(ProcessTerminationReason reason)
{
    if (reason != ProcessTerminationReason::NavigationSwap)
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "processDidTerminate: (pid %d), reason=%" PUBLIC_LOG_STRING, processIdentifier(), processTerminationReasonToString(reason));

    ASSERT(m_hasRunningProcess);

#if PLATFORM(IOS_FAMILY)
    if (m_process->isUnderMemoryPressure()) {
        String domain = WebCore::topPrivatelyControlledDomain(URL({ }, currentURL()).host().toString());
        if (!domain.isEmpty())
            logDiagnosticMessageWithEnhancedPrivacy(WebCore::DiagnosticLoggingKeys::domainCausingJetsamKey(), domain, WebCore::ShouldSample::No);
    }
#endif

    resetStateAfterProcessExited(reason);
    stopAllURLSchemeTasks(m_process.ptr());
#if ENABLE(UI_PROCESS_PDF_HUD)
    pageClient().removeAllPDFHUDs();
#endif

    if (reason != ProcessTerminationReason::NavigationSwap) {
        // For bringup of process swapping, NavigationSwap termination will not go out to clients.
        // If it does *during* process swapping, and the client triggers a reload, that causes bizarre WebKit re-entry.
        // FIXME: This might have to change
        navigationState().clearAllNavigations();

        if (m_controlledByAutomation) {
            if (auto* automationSession = process().processPool().automationSession())
                automationSession->terminate();
        }
    }
}

void WebPageProxy::provisionalProcessDidTerminate()
{
    ASSERT(m_provisionalPage);
    m_provisionalPage = nullptr;
}

static bool shouldReloadAfterProcessTermination(ProcessTerminationReason reason)
{
    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
    case ProcessTerminationReason::ExceededCPULimit:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
    case ProcessTerminationReason::Crash:
    case ProcessTerminationReason::Unresponsive:
        return true;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::IdleExit:
    case ProcessTerminationReason::RequestedByClient:
        break;
    }
    return false;
}

void WebPageProxy::dispatchProcessDidTerminate(ProcessTerminationReason reason)
{
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "dispatchProcessDidTerminate: reason=%" PUBLIC_LOG_STRING, processTerminationReasonToString(reason));

    bool handledByClient = false;
    if (m_loaderClient)
        handledByClient = reason != ProcessTerminationReason::RequestedByClient && m_loaderClient->processDidCrash(*this);
    else
        handledByClient = m_navigationClient->processDidTerminate(*this, reason);

    if (!handledByClient && shouldReloadAfterProcessTermination(reason)) {
        // We delay the view reload until it becomes visible.
        if (isViewVisible())
            tryReloadAfterProcessTermination();
        else {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "dispatchProcessDidTerminate: Not eagerly reloading the view because it is not currently visible");
            m_shouldReloadDueToCrashWhenVisible = true;
        }
    }
}

void WebPageProxy::tryReloadAfterProcessTermination()
{
    m_resetRecentCrashCountTimer.stop();

    if (++m_recentCrashCount > maximumWebProcessRelaunchAttempts) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "tryReloadAfterProcessTermination: process crashed and the client did not handle it, not reloading the page because we reached the maximum number of attempts");
        m_recentCrashCount = 0;
        return;
    }
    URL pendingAPIRequestURL { m_pageLoadState.pendingAPIRequestURL() };
    if (pendingAPIRequestURL.isValid()) {
        WEBPAGEPROXY_RELEASE_LOG(Process, "tryReloadAfterProcessTermination: process crashed and the client did not handle it, loading the pending API request URL again");
        loadRequest(ResourceRequest { WTFMove(pendingAPIRequestURL) });
    } else {
        WEBPAGEPROXY_RELEASE_LOG(Process, "tryReloadAfterProcessTermination: process crashed and the client did not handle it, reloading the page");
        reload(ReloadOption::ExpiredOnly);
    }
}

void WebPageProxy::resetRecentCrashCountSoon()
{
    m_resetRecentCrashCountTimer.startOneShot(resetRecentCrashCountDelay);
}

void WebPageProxy::resetRecentCrashCount()
{
    m_recentCrashCount = 0;
}

void WebPageProxy::stopAllURLSchemeTasks(WebProcessProxy* process)
{
    for (auto& handler : copyToVectorOf<Ref<WebURLSchemeHandler>>(m_urlSchemeHandlersByScheme.values()))
        handler->stopAllTasksForPage(*this, process);
}

void WebPageProxy::resetState(ResetStateReason resetStateReason)
{
    m_mainFrame = nullptr;
    m_focusedFrame = nullptr;
    m_suspendedPageKeptToPreventFlashing = nullptr;
    m_lastSuspendedPage = nullptr;

#if PLATFORM(COCOA)
    m_scrollingPerformanceData = nullptr;
#endif

    if (m_drawingArea) {
#if PLATFORM(COCOA)
        if (resetStateReason == ResetStateReason::NavigationSwap && is<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea)) {
            // Keep layers around in frozen state to avoid flashing during process swaps.
            m_frozenRemoteLayerTreeHost = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea).detachRemoteLayerTreeHost();
        }
#endif
        m_drawingArea = nullptr;
    }
    closeOverlayedViews();

    m_inspector->reset();

#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenManager) {
        m_fullScreenManager->close();
        m_fullScreenManager = nullptr;
    }
#endif

    m_screenOrientationManager = nullptr;

#if ENABLE(MEDIA_USAGE)
    if (m_mediaUsageManager)
        m_mediaUsageManager->reset();
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (resetStateReason != ResetStateReason::NavigationSwap)
        m_contextIDForVisibilityPropagationInWebProcess = 0;
#endif

    if (resetStateReason != ResetStateReason::NavigationSwap)
        callLoadCompletionHandlersIfNecessary(false);

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = nullptr;
    }

#if ENABLE(TOUCH_EVENTS)
    m_touchAndPointerEventTracking.reset();
#endif

#if ENABLE(GEOLOCATION)
    m_geolocationPermissionRequestManager.invalidateRequests();
#endif

    setToolTip({ });

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    m_mainFramePinnedState = { true, true, true, true };

    m_visibleScrollerThumbRect = IntRect();

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_playbackSessionManager) {
        m_playbackSessionManager->invalidate();
        m_playbackSessionManager = nullptr;
    }
    if (m_videoFullscreenManager) {
        m_videoFullscreenManager->invalidate();
        m_videoFullscreenManager = nullptr;
    }
#endif

#if PLATFORM(IOS_FAMILY)
    m_firstLayerTreeTransactionIdAfterDidCommitLoad = { };
    m_lastVisibleContentRectUpdate = { };
    m_hasNetworkRequestsOnSuspended = false;
    m_isKeyboardAnimatingIn = false;
    m_isScrollingOrZooming = false;
    m_lastObservedStateWasBackground = false;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    pageClient().mediaSessionManager().removeAllPlaybackTargetPickerClients(*this);
#endif

#if ENABLE(APPLE_PAY)
    m_paymentCoordinator = nullptr;
#endif

#if USE(SYSTEM_PREVIEW)
    m_systemPreviewController = nullptr;
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
    m_modelElementController = nullptr;
#endif

#if ENABLE(WEB_AUTHN)
    m_credentialsMessenger = nullptr;
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    m_webDeviceOrientationUpdateProviderProxy = nullptr;
#endif

    for (auto& editCommand : std::exchange(m_editCommandSet, { }))
        editCommand->invalidate();

    m_activePopupMenu = nullptr;

    updatePlayingMediaDidChange(MediaProducer::IsNotPlaying);
#if ENABLE(MEDIA_STREAM)
    m_userMediaPermissionRequestManager = nullptr;
#endif

#if ENABLE(POINTER_LOCK)
    requestPointerUnlock();
#endif
    
#if ENABLE(SPEECH_SYNTHESIS)
    resetSpeechSynthesizer();
#endif

#if ENABLE(WEB_AUTHN)
    m_websiteDataStore->authenticatorManager().cancelRequest(m_webPageID, std::nullopt);
#endif

    m_speechRecognitionPermissionManager = nullptr;

#if ENABLE(WEBXR) && !USE(OPENXR)
    if (m_xrSystem) {
        m_xrSystem->invalidate();
        m_xrSystem = nullptr;
    }
#endif

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_lastSentScrollingAccelerationCurve = std::nullopt;
#endif
}

void WebPageProxy::resetStateAfterProcessExited(ProcessTerminationReason terminationReason)
{
    if (!hasRunningProcess())
        return;

    PageClientProtector protector(pageClient());

#if ASSERT_ENABLED
    // FIXME: It's weird that resetStateAfterProcessExited() is called even though the process is launching.
    if (terminationReason != ProcessTerminationReason::NavigationSwap)
        ASSERT(m_process->state() == WebProcessProxy::State::Launching || m_process->state() == WebProcessProxy::State::Terminated);
#endif

#if PLATFORM(IOS_FAMILY)
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = false;
    m_isVisibleActivity = nullptr;
    m_isAudibleActivity = nullptr;
    m_isCapturingActivity = nullptr;
    m_openingAppLinkActivity = nullptr;
#endif

    m_pageIsUserObservableCount = nullptr;
    m_visiblePageToken = nullptr;

    m_hasRunningProcess = false;
    m_areActiveDOMObjectsAndAnimationsSuspended = false;
#if ENABLE(SERVICE_WORKER)
    m_isServiceWorkerPage = false;
#endif

    m_userScriptsNotified = false;
    m_hasActiveAnimatedScroll = false;
    m_registeredForFullSpeedUpdates = false;

    m_editorState = EditorState();
    m_cachedFontAttributesAtSelectionStart.reset();

    if (terminationReason != ProcessTerminationReason::NavigationSwap)
        m_provisionalPage = nullptr;

    if (terminationReason == ProcessTerminationReason::NavigationSwap)
        pageClient().processWillSwap();
    else
        pageClient().processDidExit();

    pageClient().clearAllEditCommands();

#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().revokeAccess(m_process.get());
#endif

    auto resetStateReason = terminationReason == ProcessTerminationReason::NavigationSwap ? ResetStateReason::NavigationSwap : ResetStateReason::WebProcessExited;
    resetState(resetStateReason);

    m_pendingLearnOrIgnoreWordMessageCount = 0;

    // Can't expect DidReceiveEvent notifications from a crashed web process.
    m_mouseEventQueue.clear();
    m_keyEventQueue.clear();
    if (m_wheelEventCoalescer)
        m_wheelEventCoalescer->clear();

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    m_touchEventQueue.clear();
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    invalidateAllAttachments();
#endif

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (m_scrollingCoordinatorProxy)
        m_scrollingCoordinatorProxy->resetStateAfterProcessExited();
#endif

    if (terminationReason != ProcessTerminationReason::NavigationSwap) {
        PageLoadState::Transaction transaction = m_pageLoadState.transaction();
        m_pageLoadState.reset(transaction);
    }

    updatePlayingMediaDidChange(MediaProducer::IsNotPlaying);

#if ENABLE(VIDEO_PRESENTATION_MODE)
    m_fullscreenVideoTextRecognitionTimer.stop();
    m_currentFullscreenVideoSessionIdentifier = std::nullopt;
#endif

    // FIXME: <rdar://problem/38676604> In case of process swaps, the old process should gracefully suspend instead of terminating.
    m_process->processTerminated();
}

#if PLATFORM(COCOA) && !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)

static Span<const ASCIILiteral> gpuIOKitClasses()
{
    static constexpr std::array services {
#if PLATFORM(IOS_FAMILY)
        "AGXDeviceUserClient"_s,
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

static Span<const ASCIILiteral> gpuMachServices()
{
    static constexpr std::array services {
        "com.apple.MTLCompilerService"_s,
    };
    return services;
}

#endif // PLATFORM(COCOA)

WebPageCreationParameters WebPageProxy::creationParameters(WebProcessProxy& process, DrawingAreaProxy& drawingArea, RefPtr<API::WebsitePolicies>&& websitePolicies)
{
    WebPageCreationParameters parameters;

    parameters.processDisplayName = configuration().processDisplayName();

    parameters.viewSize = pageClient().viewSize();
    parameters.activityState = m_activityState;
    parameters.drawingAreaType = drawingArea.type();
    parameters.drawingAreaIdentifier = drawingArea.identifier();
    parameters.webPageProxyIdentifier = m_identifier;
    parameters.store = preferencesStore();
    parameters.pageGroupData = m_pageGroup->data();
    parameters.isEditable = m_isEditable;
    parameters.underlayColor = m_underlayColor;
    parameters.useFixedLayout = m_useFixedLayout;
    parameters.fixedLayoutSize = m_fixedLayoutSize;
    parameters.defaultUnobscuredSize = m_defaultUnobscuredSize;
    parameters.minimumUnobscuredSize = m_minimumUnobscuredSize;
    parameters.maximumUnobscuredSize = m_maximumUnobscuredSize;
    parameters.viewExposedRect = m_viewExposedRect;
    parameters.alwaysShowsHorizontalScroller = m_alwaysShowsHorizontalScroller;
    parameters.alwaysShowsVerticalScroller = m_alwaysShowsVerticalScroller;
    parameters.suppressScrollbarAnimations = m_suppressScrollbarAnimations;
    parameters.paginationMode = m_paginationMode;
    parameters.paginationBehavesLikeColumns = m_paginationBehavesLikeColumns;
    parameters.pageLength = m_pageLength;
    parameters.gapBetweenPages = m_gapBetweenPages;
    parameters.userAgent = userAgent();
    parameters.itemStatesWereRestoredByAPIRequest = m_sessionStateWasRestoredByAPIRequest;
    parameters.itemStates = m_backForwardList->itemStates();
    parameters.visitedLinkTableID = m_visitedLinkStore->identifier();
    parameters.canRunBeforeUnloadConfirmPanel = m_uiClient->canRunBeforeUnloadConfirmPanel();
    parameters.canRunModal = m_canRunModal;
    parameters.deviceScaleFactor = deviceScaleFactor();
    parameters.viewScaleFactor = m_viewScaleFactor;
    parameters.textZoomFactor = m_textZoomFactor;
    parameters.pageZoomFactor = m_pageZoomFactor;
    parameters.topContentInset = m_topContentInset;
    parameters.mediaVolume = m_mediaVolume;
    parameters.muted = m_mutedState;
    parameters.openedByDOM = m_openedByDOM;
    parameters.mayStartMediaWhenInWindow = m_mayStartMediaWhenInWindow;
    parameters.mediaPlaybackIsSuspended = m_mediaPlaybackIsSuspended;
    parameters.minimumSizeForAutoLayout = m_minimumSizeForAutoLayout;
    parameters.sizeToContentAutoSizeMaximumSize = m_sizeToContentAutoSizeMaximumSize;
    parameters.autoSizingShouldExpandToViewHeight = m_autoSizingShouldExpandToViewHeight;
    parameters.viewportSizeForCSSViewportUnits = m_viewportSizeForCSSViewportUnits;
    parameters.scrollPinningBehavior = m_scrollPinningBehavior;
    if (m_scrollbarOverlayStyle)
        parameters.scrollbarOverlayStyle = m_scrollbarOverlayStyle.value();
    else
        parameters.scrollbarOverlayStyle = std::nullopt;
    parameters.backgroundExtendsBeyondPage = m_backgroundExtendsBeyondPage;
    parameters.layerHostingMode = m_layerHostingMode;
    parameters.controlledByAutomation = m_controlledByAutomation;
    parameters.useDarkAppearance = useDarkAppearance();
    parameters.useElevatedUserInterfaceLevel = useElevatedUserInterfaceLevel();
#if PLATFORM(MAC)
    parameters.colorSpace = pageClient().colorSpace();
    parameters.useSystemAppearance = m_useSystemAppearance;
#endif

#if ENABLE(META_VIEWPORT)
    parameters.ignoresViewportScaleLimits = m_forceAlwaysUserScalable;
    parameters.viewportConfigurationViewLayoutSize = m_viewportConfigurationViewLayoutSize;
    parameters.viewportConfigurationLayoutSizeScaleFactor = m_viewportConfigurationLayoutSizeScaleFactor;
    parameters.viewportConfigurationMinimumEffectiveDeviceWidth = m_viewportConfigurationMinimumEffectiveDeviceWidth;
    parameters.overrideViewportArguments = m_overrideViewportArguments;
#endif

#if PLATFORM(IOS_FAMILY)
    parameters.screenSize = screenSize();
    parameters.availableScreenSize = availableScreenSize();
    parameters.overrideScreenSize = overrideScreenSize();
    parameters.textAutosizingWidth = textAutosizingWidth();
    parameters.mimeTypesWithCustomContentProviders = pageClient().mimeTypesWithCustomContentProviders();
    parameters.deviceOrientation = m_deviceOrientation;
    parameters.keyboardIsAttached = isInHardwareKeyboardMode();
    parameters.canShowWhileLocked = m_configuration->canShowWhileLocked();
#endif

#if PLATFORM(COCOA)
    parameters.smartInsertDeleteEnabled = m_isSmartInsertDeleteEnabled;
    parameters.additionalSupportedImageTypes = m_configuration->additionalSupportedImageTypes();

#if !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
    if (!preferences().useGPUProcessForMediaEnabled()
        || (!preferences().captureVideoInGPUProcessEnabled() && !preferences().captureVideoInUIProcessEnabled())
        || (!preferences().captureAudioInGPUProcessEnabled() && !preferences().captureAudioInUIProcessEnabled())
        || !preferences().webRTCPlatformCodecsInGPUProcessEnabled()
        || !preferences().useGPUProcessForCanvasRenderingEnabled()
        || !preferences().useGPUProcessForWebGLEnabled()) {
        parameters.gpuIOKitExtensionHandles = SandboxExtension::createHandlesForIOKitClassExtensions(gpuIOKitClasses(), std::nullopt);
        parameters.gpuMachExtensionHandles = SandboxExtension::createHandlesForMachLookup(gpuMachServices(), std::nullopt);
    }
#endif // !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
#endif // PLATFORM(COCOA)

#if HAVE(STATIC_FONT_REGISTRY)
    if (preferences().shouldAllowUserInstalledFonts())
        parameters.fontMachExtensionHandle = process.fontdMachExtensionHandle(SandboxExtension::MachBootstrapOptions::DoNotEnableMachBootstrap);
#endif
#if HAVE(APP_ACCENT_COLORS)
    parameters.accentColor = pageClient().accentColor();
#endif
    parameters.shouldScaleViewToFitDocument = m_shouldScaleViewToFitDocument;
    parameters.userInterfaceLayoutDirection = pageClient().userInterfaceLayoutDirection();
    parameters.observedLayoutMilestones = m_observedLayoutMilestones;
    parameters.overrideContentSecurityPolicy = m_overrideContentSecurityPolicy;
    parameters.contentSecurityPolicyModeForExtension = m_configuration->contentSecurityPolicyModeForExtension();
    parameters.cpuLimit = m_cpuLimit;

#if USE(WPE_RENDERER)
    parameters.hostFileDescriptor = pageClient().hostFileDescriptor();
#endif

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    parameters.nativeWindowHandle = reinterpret_cast<uint64_t>(viewWidget());
#endif
#if USE(GRAPHICS_LAYER_WC)
    parameters.usesOffscreenRendering = pageClient().usesOffscreenRendering();
#endif

    for (auto& iterator : m_urlSchemeHandlersByScheme)
        parameters.urlSchemeHandlers.set(iterator.key, iterator.value->identifier());
    parameters.urlSchemesWithLegacyCustomProtocolHandlers = WebProcessPool::urlSchemesWithCustomProtocolHandlers();

#if ENABLE(WEB_RTC)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.iceCandidateFilteringEnabled = m_preferences->iceCandidateFilteringEnabled();
#if USE(LIBWEBRTC)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.enumeratingAllNetworkInterfacesEnabled = m_preferences->enumeratingAllNetworkInterfacesEnabled();
#endif
#endif

#if ENABLE(APPLICATION_MANIFEST)
    parameters.applicationManifest = m_configuration->applicationManifest() ? std::optional<WebCore::ApplicationManifest>(m_configuration->applicationManifest()->applicationManifest()) : std::nullopt;
#endif

    parameters.needsFontAttributes = m_needsFontAttributes;
    parameters.backgroundColor = m_backgroundColor;

    parameters.overriddenMediaType = m_overriddenMediaType;
    parameters.corsDisablingPatterns = corsDisablingPatterns();
    parameters.maskedURLSchemes = m_configuration->maskedURLSchemes();
    parameters.userScriptsShouldWaitUntilNotification = m_configuration->userScriptsShouldWaitUntilNotification();
    parameters.allowedNetworkHosts = m_configuration->allowedNetworkHosts();
    parameters.loadsSubresources = m_configuration->loadsSubresources();
    parameters.crossOriginAccessControlCheckEnabled = m_configuration->crossOriginAccessControlCheckEnabled();
    parameters.hasResourceLoadClient = !!m_resourceLoadClient;

    std::reference_wrapper<WebUserContentControllerProxy> userContentController(m_userContentController.get());
    if (auto* userContentControllerFromWebsitePolicies = websitePolicies ? websitePolicies->userContentController() : nullptr)
        userContentController = *userContentControllerFromWebsitePolicies;
    process.addWebUserContentControllerProxy(userContentController);
    parameters.userContentControllerParameters = userContentController.get().parameters();

#if ENABLE(WK_WEB_EXTENSIONS)
    if (m_webExtensionController)
        parameters.webExtensionControllerParameters = m_webExtensionController->parameters();
    if (m_weakWebExtensionController)
        parameters.webExtensionControllerParameters = m_weakWebExtensionController->parameters();
#endif

    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldCaptureAudioInUIProcess = preferences().captureAudioInUIProcessEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldCaptureAudioInGPUProcess = preferences().captureAudioInGPUProcessEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldCaptureVideoInUIProcess = preferences().captureVideoInUIProcessEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldCaptureVideoInGPUProcess = preferences().captureVideoInGPUProcessEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldRenderCanvasInGPUProcess = preferences().useGPUProcessForCanvasRenderingEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldRenderDOMInGPUProcess = preferences().useGPUProcessForDOMRenderingEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldPlayMediaInGPUProcess = preferences().useGPUProcessForMediaEnabled();
#if ENABLE(WEBGL)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldRenderWebGLInGPUProcess = preferences().useGPUProcessForWebGLEnabled();
#endif

    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldEnableVP9Decoder = preferences().vp9DecoderEnabled();
#if ENABLE(VP9) && PLATFORM(COCOA)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldEnableVP8Decoder = preferences().vp8DecoderEnabled();
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.shouldEnableVP9SWDecoder = preferences().vp9DecoderEnabled() && (!WebCore::systemHasBattery() || preferences().vp9SWDecoderEnabledOnBattery());
#endif
    parameters.shouldCaptureDisplayInUIProcess = m_process->processPool().configuration().shouldCaptureDisplayInUIProcess();
    parameters.shouldCaptureDisplayInGPUProcess = preferences().useGPUProcessForDisplayCapture();
#if ENABLE(APP_BOUND_DOMAINS)
    parameters.limitsNavigationsToAppBoundDomains = m_limitsNavigationsToAppBoundDomains;
#endif
    parameters.lastNavigationWasAppInitiated = m_lastNavigationWasAppInitiated;
    parameters.shouldRelaxThirdPartyCookieBlocking = m_configuration->shouldRelaxThirdPartyCookieBlocking();
    parameters.canUseCredentialStorage = m_canUseCredentialStorage;

    parameters.httpsUpgradeEnabled = preferences().upgradeKnownHostsToHTTPSEnabled() ? m_configuration->httpsUpgradeEnabled() : false;

#if PLATFORM(IOS)
    // FIXME: This is also being passed over the to WebProcess via the PreferencesStore.
    parameters.allowsDeprecatedSynchronousXMLHttpRequestDuringUnload = allowsDeprecatedSynchronousXMLHttpRequestDuringUnload();
#endif
    
#if ENABLE(APP_HIGHLIGHTS)
    parameters.appHighlightsVisible = appHighlightsVisibility() ? HighlightVisibility::Visible : HighlightVisibility::Hidden;
#endif

#if HAVE(TOUCH_BAR)
    parameters.requiresUserActionForEditingControlsManager = m_configuration->requiresUserActionForEditingControlsManager();
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    parameters.hasResizableWindows = pageClient().hasResizableWindows();
#endif

    return parameters;
}

void WebPageProxy::isJITEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    launchInitialProcessIfNecessary();
    sendWithAsyncReply(Messages::WebProcess::IsJITEnabled(), WTFMove(completionHandler), 0);
}

void WebPageProxy::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
#if PLATFORM(MAC)
    ASSERT(m_drawingArea->type() == DrawingAreaType::TiledCoreAnimation);
#endif
    pageClient().enterAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::didFirstLayerFlush(const LayerTreeContext& layerTreeContext)
{
#if PLATFORM(MAC)
    ASSERT(m_drawingArea->type() == DrawingAreaType::TiledCoreAnimation);
#endif
    pageClient().didFirstLayerFlush(layerTreeContext);

    if (m_lastSuspendedPage)
        m_lastSuspendedPage->pageDidFirstLayerFlush();
    m_suspendedPageKeptToPreventFlashing = nullptr;
}

void WebPageProxy::exitAcceleratedCompositingMode()
{
    pageClient().exitAcceleratedCompositingMode();
}

void WebPageProxy::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    pageClient().updateAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::backForwardClear()
{
    m_backForwardList->clear();
}

#if ENABLE(GAMEPAD)

void WebPageProxy::gamepadActivity(const Vector<GamepadData>& gamepadDatas, EventMakesGamepadsVisible eventVisibility)
{
    send(Messages::WebPage::GamepadActivity(gamepadDatas, eventVisibility));
}

#endif

void WebPageProxy::didReceiveAuthenticationChallengeProxy(Ref<AuthenticationChallengeProxy>&& authenticationChallenge, NegotiatedLegacyTLS negotiatedLegacyTLS)
{
    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes) {
        m_navigationClient->shouldAllowLegacyTLS(*this, authenticationChallenge.get(), [this, protectedThis = Ref { *this }, authenticationChallenge] (bool shouldAllowLegacyTLS) {
            if (shouldAllowLegacyTLS)
                m_navigationClient->didReceiveAuthenticationChallenge(*this, authenticationChallenge.get());
            else
                authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::Cancel);
        });
        return;
    }
    m_navigationClient->didReceiveAuthenticationChallenge(*this, authenticationChallenge.get());
}

void WebPageProxy::negotiatedLegacyTLS()
{
    auto transaction = m_pageLoadState.transaction();
    m_pageLoadState.negotiatedLegacyTLS(transaction);
}

void WebPageProxy::didNegotiateModernTLS(const URL& url)
{
    m_navigationClient->didNegotiateModernTLS(url);
}

void WebPageProxy::exceededDatabaseQuota(FrameIdentifier frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, CompletionHandler<void(uint64_t)>&& reply)
{
    requestStorageSpace(frameID, originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, [reply = WTFMove(reply)](auto quota) mutable {
        reply(quota);
    });
}

void WebPageProxy::requestStorageSpace(FrameIdentifier frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    WEBPAGEPROXY_RELEASE_LOG(Storage, "requestStorageSpace for frame %" PRIu64 ", current quota %" PRIu64 " current usage %" PRIu64 " expected usage %" PRIu64, frameID.object().toUInt64(), currentQuota, currentDatabaseUsage, expectedUsage);

    StorageRequests::singleton().processOrAppend([this, protectedThis = Ref { *this }, pageURL = currentURL(), frameID, originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, completionHandler = WTFMove(completionHandler)]() mutable {
        this->makeStorageSpaceRequest(frameID, originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, [this, protectedThis = WTFMove(protectedThis), frameID, pageURL = WTFMove(pageURL), completionHandler = WTFMove(completionHandler), currentQuota](auto quota) mutable {

            WEBPAGEPROXY_RELEASE_LOG(Storage, "requestStorageSpace response for frame %" PRIu64 ", quota %" PRIu64, frameID.object().toUInt64(), quota);
            UNUSED_VARIABLE(frameID);

            if (quota <= currentQuota && this->currentURL() == pageURL) {
                WEBPAGEPROXY_RELEASE_LOG(Storage, "storage space increase denied");
                m_isQuotaIncreaseDenied =  true;
            }
            completionHandler(quota);
            StorageRequests::singleton().processNextIfAny();
        });
    });
}

void WebPageProxy::makeStorageSpaceRequest(FrameIdentifier frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    if (m_isQuotaIncreaseDenied) {
        completionHandler(currentQuota);
        return;
    }

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto originData = SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
    if (originData != SecurityOriginData::fromURL(URL { currentURL() })) {
        completionHandler(currentQuota);
        return;
    }

    auto origin = API::SecurityOrigin::create(originData->securityOrigin());
    m_uiClient->exceededDatabaseQuota(this, frame.get(), origin.ptr(), databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, WTFMove(completionHandler));
}

void WebPageProxy::reachedApplicationCacheOriginQuota(const String& originIdentifier, uint64_t currentQuota, uint64_t totalBytesNeeded, CompletionHandler<void(uint64_t)>&& reply)
{
    auto securityOriginData = SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
    MESSAGE_CHECK(m_process, securityOriginData);

    Ref<SecurityOrigin> securityOrigin = securityOriginData->securityOrigin();
    m_uiClient->reachedApplicationCacheOriginQuota(this, securityOrigin.get(), currentQuota, totalBytesNeeded, WTFMove(reply));
}

void WebPageProxy::requestGeolocationPermissionForFrame(GeolocationIdentifier geolocationID, FrameInfoData&& frameInfo)
{
    MESSAGE_CHECK(m_process, frameInfo.frameID);
    auto* frame = WebFrameProxy::webFrame(*frameInfo.frameID);
    MESSAGE_CHECK(m_process, frame);

    auto request = m_geolocationPermissionRequestManager.createRequest(geolocationID);
    Function<void(bool)> completionHandler = [request = WTFMove(request)](bool allowed) {
        if (allowed)
            request->allow();
        else
            request->deny();
    };

    // FIXME: Once iOS migrates to the new WKUIDelegate SPI, clean this up
    // and make it one UIClient call that calls the completionHandler with false
    // if there is no delegate instead of returning the completionHandler
    // for other code paths to try.
    m_uiClient->decidePolicyForGeolocationPermissionRequest(*this, *frame, frameInfo, completionHandler);
#if PLATFORM(IOS_FAMILY)
    if (completionHandler)
        pageClient().decidePolicyForGeolocationPermissionRequest(*frame, frameInfo, completionHandler);
#endif
    if (completionHandler)
        completionHandler(false);
}

void WebPageProxy::revokeGeolocationAuthorizationToken(const String& authorizationToken)
{
    m_geolocationPermissionRequestManager.revokeAuthorizationToken(authorizationToken);
}

void WebPageProxy::queryPermission(const ClientOrigin& clientOrigin, const PermissionDescriptor& descriptor, CompletionHandler<void(std::optional<PermissionState>)>&& completionHandler)
{
    bool canAPISucceed = true;
    bool shouldChangeDeniedToPrompt = true;
    bool shouldChangePromptToGrant = false;
    String name;
    if (descriptor.name == PermissionName::Camera) {
#if ENABLE(MEDIA_STREAM)
        name = "camera"_s;
        canAPISucceed = userMediaPermissionRequestManager().canVideoCaptureSucceed();
        shouldChangeDeniedToPrompt = userMediaPermissionRequestManager().shouldChangeDeniedToPromptForCamera(clientOrigin);
        shouldChangePromptToGrant = userMediaPermissionRequestManager().shouldChangePromptToGrantForCamera(clientOrigin);
#endif
    } else if (descriptor.name == PermissionName::Microphone) {
#if ENABLE(MEDIA_STREAM)
        name = "microphone"_s;
        canAPISucceed = userMediaPermissionRequestManager().canAudioCaptureSucceed();
        shouldChangeDeniedToPrompt = userMediaPermissionRequestManager().shouldChangeDeniedToPromptForMicrophone(clientOrigin);
        shouldChangePromptToGrant = userMediaPermissionRequestManager().shouldChangePromptToGrantForMicrophone(clientOrigin);
#endif
    } else if (descriptor.name == PermissionName::Geolocation) {
#if ENABLE(GEOLOCATION)
        name = "geolocation"_s;
        // FIXME: We should set shouldChangeDeniedToPrompt after the first
        // permission request like we do for notifications.
#endif
    } else if (descriptor.name == PermissionName::Notifications) {
#if ENABLE(NOTIFICATIONS)
        name = "notifications"_s;

        // Ensure that the true permission state of the Notifications API is returned if
        // this topOrigin has requested permission to use the Notifications API previously.
        if (m_notificationPermissionRequesters.contains(clientOrigin.topOrigin))
            shouldChangeDeniedToPrompt = false;
#endif
    } else if (descriptor.name == PermissionName::ScreenWakeLock) {
        name = "screen-wake-lock"_s;
        shouldChangeDeniedToPrompt = false;
    }

    if (name.isNull()) {
        completionHandler({ });
        return;
    }

    if (!canAPISucceed) {
        completionHandler(shouldChangeDeniedToPrompt ? PermissionState::Prompt : PermissionState::Denied);
        return;
    }

    CompletionHandler<void(std::optional<WebCore::PermissionState>)> callback = [clientOrigin, shouldChangeDeniedToPrompt, shouldChangePromptToGrant, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result) {
            completionHandler({ });
            return;
        }
        if (*result == PermissionState::Denied && shouldChangeDeniedToPrompt)
            result = PermissionState::Prompt;
        else if (*result == PermissionState::Prompt && shouldChangePromptToGrant)
            result = PermissionState::Granted;
        completionHandler(*result);
    };

    if (clientOrigin.topOrigin.isOpaque()) {
        callback(PermissionState::Prompt);
        return;
    }

    auto origin = API::SecurityOrigin::create(clientOrigin.topOrigin);
    m_uiClient->queryPermission(name, origin, WTFMove(callback));
}

#if ENABLE(MEDIA_STREAM)
UserMediaPermissionRequestManagerProxy& WebPageProxy::userMediaPermissionRequestManager()
{
    if (m_userMediaPermissionRequestManager)
        return *m_userMediaPermissionRequestManager;

    m_userMediaPermissionRequestManager = makeUnique<UserMediaPermissionRequestManagerProxy>(*this);

    return *m_userMediaPermissionRequestManager;
}

void WebPageProxy::setMockCaptureDevicesEnabledOverride(std::optional<bool> enabled)
{
    userMediaPermissionRequestManager().setMockCaptureDevicesEnabledOverride(enabled);
}

void WebPageProxy::willStartCapture(const UserMediaPermissionRequestProxy& request, CompletionHandler<void()>&& callback)
{
#if ENABLE(GPU_PROCESS)
    if (!preferences().captureVideoInGPUProcessEnabled() && !preferences().captureAudioInGPUProcessEnabled())
        return callback();

    auto& gpuProcess = process().processPool().ensureGPUProcess();
    gpuProcess.updateCaptureAccess(request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture(), m_process->coreProcessIdentifier(), WTFMove(callback));
    gpuProcess.updateCaptureOrigin(request.topLevelDocumentSecurityOrigin().data(), m_process->coreProcessIdentifier());
#if PLATFORM(IOS_FAMILY)
    gpuProcess.setOrientationForMediaCapture(m_deviceOrientation);
#endif
#else
    callback();
#endif
}

#endif

void WebPageProxy::requestUserMediaPermissionForFrame(UserMediaRequestIdentifier userMediaID, FrameIdentifier frameID, const WebCore::SecurityOriginData& userMediaDocumentOriginData, const WebCore::SecurityOriginData& topLevelDocumentOriginData, WebCore::MediaStreamRequest&& request)
{
#if ENABLE(MEDIA_STREAM)
    MESSAGE_CHECK(m_process, WebFrameProxy::webFrame(frameID));
#if PLATFORM(MAC)
    CoreAudioCaptureDeviceManager::singleton().setFilterTapEnabledDevices(!preferences().captureAudioInGPUProcessEnabled());
#endif
    userMediaPermissionRequestManager().requestUserMediaPermissionForFrame(userMediaID, frameID, userMediaDocumentOriginData.securityOrigin(), topLevelDocumentOriginData.securityOrigin(), WTFMove(request));
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOriginData);
    UNUSED_PARAM(topLevelDocumentOriginData);
    UNUSED_PARAM(request);
#endif
}

void WebPageProxy::enumerateMediaDevicesForFrame(FrameIdentifier frameID, const WebCore::SecurityOriginData& userMediaDocumentOriginData, const WebCore::SecurityOriginData& topLevelDocumentOriginData, CompletionHandler<void(const Vector<WebCore::CaptureDevice>&, WebCore::MediaDeviceHashSalts&&)>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    userMediaPermissionRequestManager().enumerateMediaDevicesForFrame(frameID, userMediaDocumentOriginData.securityOrigin(), topLevelDocumentOriginData.securityOrigin(), WTFMove(completionHandler));
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOriginData);
    UNUSED_PARAM(topLevelDocumentOriginData);
    UNUSED_PARAM(completionHandler);
#endif
}

void WebPageProxy::syncIfMockDevicesEnabledChanged()
{
#if ENABLE(MEDIA_STREAM)
    userMediaPermissionRequestManager().syncWithWebCorePrefs();
#endif
}

void WebPageProxy::beginMonitoringCaptureDevices()
{
#if ENABLE(MEDIA_STREAM)
    userMediaPermissionRequestManager().syncWithWebCorePrefs();
    UserMediaProcessManager::singleton().beginMonitoringCaptureDevices();
#endif
}

void WebPageProxy::clearUserMediaState()
{
#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager->clearCachedState();
#endif
}

void WebPageProxy::requestMediaKeySystemPermissionForFrame(MediaKeySystemRequestIdentifier mediaKeySystemID, FrameIdentifier frameID, const WebCore::SecurityOriginData& topLevelDocumentOriginData, const String& keySystem)
{
#if ENABLE(ENCRYPTED_MEDIA)
    MESSAGE_CHECK(m_process, WebFrameProxy::webFrame(frameID));

    auto origin = API::SecurityOrigin::create(topLevelDocumentOriginData.securityOrigin());
    auto request = mediaKeySystemPermissionRequestManager().createRequestForFrame(mediaKeySystemID, frameID, topLevelDocumentOriginData.securityOrigin(), keySystem);
    m_uiClient->decidePolicyForMediaKeySystemPermissionRequest(*this, origin, keySystem, [request = WTFMove(request)](bool allowed) {
        if (allowed)
            request->allow();
        else
            request->deny();
    });
#else
    UNUSED_PARAM(mediaKeySystemID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(topLevelDocumentOriginData);
    UNUSED_PARAM(keySystem);
#endif
}

#if ENABLE(DEVICE_ORIENTATION)
void WebPageProxy::shouldAllowDeviceOrientationAndMotionAccess(FrameIdentifier frameID, FrameInfoData&& frameInfo, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    websiteDataStore().deviceOrientationAndMotionAccessController().shouldAllowAccess(*this, *frame, WTFMove(frameInfo), mayPrompt, WTFMove(completionHandler));
}
#endif


#if ENABLE(IMAGE_ANALYSIS)

void WebPageProxy::requestTextRecognition(const URL& imageURL, const ShareableBitmapHandle& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completionHandler)
{
    pageClient().requestTextRecognition(imageURL, imageData, sourceLanguageIdentifier, targetLanguageIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&& completion)
{
    pageClient().computeHasVisualSearchResults(imageURL, imageBitmap, WTFMove(completion));
}

void WebPageProxy::updateWithTextRecognitionResult(TextRecognitionResult&& results, const ElementContext& context, const FloatPoint& location, CompletionHandler<void(TextRecognitionUpdateResult)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler(TextRecognitionUpdateResult::NoText);
        return;
    }

    sendWithAsyncReply(Messages::WebPage::UpdateWithTextRecognitionResult(WTFMove(results), context, location), WTFMove(completionHandler));
}

void WebPageProxy::startVisualTranslation(const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier)
{
    if (hasRunningProcess())
        send(Messages::WebPage::StartVisualTranslation(sourceLanguageIdentifier, targetLanguageIdentifier));
}

#endif // ENABLE(IMAGE_ANALYSIS)

void WebPageProxy::requestImageBitmap(const ElementContext& elementContext, CompletionHandler<void(const ShareableBitmapHandle&, const String&)>&& completion)
{
    if (!hasRunningProcess()) {
        completion({ }, { });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::RequestImageBitmap(elementContext), WTFMove(completion));
}

#if ENABLE(ENCRYPTED_MEDIA)
MediaKeySystemPermissionRequestManagerProxy& WebPageProxy::mediaKeySystemPermissionRequestManager()
{
    if (m_mediaKeySystemPermissionRequestManager)
        return *m_mediaKeySystemPermissionRequestManager;

    m_mediaKeySystemPermissionRequestManager = makeUnique<MediaKeySystemPermissionRequestManagerProxy>(*this);
    return *m_mediaKeySystemPermissionRequestManager;
}
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

void WebPageProxy::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    pageClient().showMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), WTFMove(completionHandler));
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(NOTIFICATIONS)
void WebPageProxy::clearNotificationPermissionState()
{
    m_notificationPermissionRequesters.clear();
    send(Messages::WebPage::ClearNotificationPermissionState());
}
#endif

void WebPageProxy::requestNotificationPermission(const String& originString, CompletionHandler<void(bool allowed)>&& completionHandler)
{
    auto origin = API::SecurityOrigin::createFromString(originString);

#if ENABLE(NOTIFICATIONS)
    // Add origin to list of origins that have requested permission to use the Notifications API.
    m_notificationPermissionRequesters.add(origin->securityOrigin());
#endif

    m_uiClient->decidePolicyForNotificationPermissionRequest(*this, origin.get(), WTFMove(completionHandler));
}

void WebPageProxy::showNotification(IPC::Connection& connection, const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources>&& notificationResources)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->show(this, connection, notificationData, WTFMove(notificationResources));
}

void WebPageProxy::cancelNotification(const UUID& notificationID)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->cancel(this, notificationID);
}

void WebPageProxy::clearNotifications(const Vector<UUID>& notificationIDs)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->clearNotifications(this, notificationIDs);
}

void WebPageProxy::didDestroyNotification(const UUID& notificationID)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->didDestroyNotification(this, notificationID);
}

float WebPageProxy::headerHeightForPrinting(WebFrameProxy& frame)
{
    if (frame.isDisplayingPDFDocument())
        return 0;
    return m_uiClient->headerHeight(*this, frame);
}

float WebPageProxy::footerHeightForPrinting(WebFrameProxy& frame)
{
    if (frame.isDisplayingPDFDocument())
        return 0;
    return m_uiClient->footerHeight(*this, frame);
}

void WebPageProxy::drawHeaderForPrinting(WebFrameProxy& frame, FloatRect&& rect)
{
    if (frame.isDisplayingPDFDocument())
        return;
    m_uiClient->drawHeader(*this, frame, WTFMove(rect));
}

void WebPageProxy::drawFooterForPrinting(WebFrameProxy& frame, FloatRect&& rect)
{
    if (frame.isDisplayingPDFDocument())
        return;
    m_uiClient->drawFooter(*this, frame, WTFMove(rect));
}

void WebPageProxy::drawPageBorderForPrinting(WebFrameProxy& frame, WebCore::FloatSize&& size)
{
    if (frame.isDisplayingPDFDocument())
        return;
    pageClient().drawPageBorderForPrinting(WTFMove(size));
}

void WebPageProxy::runModal()
{
    // Since runModal() can (and probably will) spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    // Our Connection's run loop might have more messages waiting to be handled after this RunModal message.
    // To make sure they are handled inside of the nested modal run loop we must first signal the Connection's
    // run loop so we're guaranteed that it has a chance to wake up.
    // See http://webkit.org/b/89590 for more discussion.
    m_process->connection()->wakeUpRunLoop();

    m_uiClient->runModal(*this);
}

void WebPageProxy::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    m_visibleScrollerThumbRect = scrollerThumb;
}

void WebPageProxy::recommendedScrollbarStyleDidChange(int32_t newStyle)
{
#if USE(APPKIT)
    pageClient().recommendedScrollbarStyleDidChange(static_cast<WebCore::ScrollbarStyle>(newStyle));
#else
    UNUSED_PARAM(newStyle);
#endif
}

void WebPageProxy::didChangeScrollbarsForMainFrame(bool hasHorizontalScrollbar, bool hasVerticalScrollbar)
{
    m_mainFrameHasHorizontalScrollbar = hasHorizontalScrollbar;
    m_mainFrameHasVerticalScrollbar = hasVerticalScrollbar;
}

void WebPageProxy::didChangeScrollOffsetPinningForMainFrame(RectEdges<bool> pinnedState)
{
    pageClient().pinnedStateWillChange();
    m_mainFramePinnedState = pinnedState;
    pageClient().pinnedStateDidChange();

    m_uiClient->pinnedStateDidChange(*this);
}

void WebPageProxy::didChangePageCount(unsigned pageCount)
{
    m_pageCount = pageCount;
}

void WebPageProxy::themeColorChanged(const Color& themeColor)
{
    if (m_themeColor == themeColor)
        return;

    pageClient().themeColorWillChange();
    m_themeColor = themeColor;
    pageClient().themeColorDidChange();
}

void WebPageProxy::pageExtendedBackgroundColorDidChange(const Color& newPageExtendedBackgroundColor)
{
    if (m_pageExtendedBackgroundColor == newPageExtendedBackgroundColor)
        return;

    auto oldUnderPageBackgroundColor = underPageBackgroundColor();
    auto oldPageExtendedBackgroundColor = std::exchange(m_pageExtendedBackgroundColor, newPageExtendedBackgroundColor);
    bool changesUnderPageBackgroundColor = !equalIgnoringSemanticColor(oldUnderPageBackgroundColor, underPageBackgroundColor());
    m_pageExtendedBackgroundColor = WTFMove(oldPageExtendedBackgroundColor);

    if (changesUnderPageBackgroundColor)
        pageClient().underPageBackgroundColorWillChange();
    pageClient().pageExtendedBackgroundColorWillChange();

    m_pageExtendedBackgroundColor = newPageExtendedBackgroundColor;

    if (changesUnderPageBackgroundColor)
        pageClient().underPageBackgroundColorDidChange();
    pageClient().pageExtendedBackgroundColorDidChange();
}

void WebPageProxy::sampledPageTopColorChanged(const Color& sampledPageTopColor)
{
    if (m_sampledPageTopColor == sampledPageTopColor)
        return;

    pageClient().sampledPageTopColorWillChange();
    m_sampledPageTopColor = sampledPageTopColor;
    pageClient().sampledPageTopColorDidChange();
}

#if !PLATFORM(COCOA)

Color WebPageProxy::platformUnderPageBackgroundColor() const
{
    return Color::transparentBlack;
}

#endif // !PLATFORM(COCOA)

bool WebPageProxy::willHandleHorizontalScrollEvents() const
{
    return !m_canShortCircuitHorizontalWheelEvents;
}

void WebPageProxy::updateWebsitePolicies(WebsitePoliciesData&& websitePolicies)
{
    send(Messages::WebPage::UpdateWebsitePolicies(websitePolicies));
}

void WebPageProxy::notifyUserScripts()
{
    m_userScriptsNotified = true;
    send(Messages::WebPage::NotifyUserScripts());
}

bool WebPageProxy::userScriptsNeedNotification() const
{
    if (!m_configuration->userScriptsShouldWaitUntilNotification())
        return false;
    return !m_userScriptsNotified;
}

void WebPageProxy::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
    pageClient().didFinishLoadingDataForCustomContentProvider(ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename), dataReference);
}

void WebPageProxy::backForwardRemovedItem(const BackForwardItemIdentifier& itemID)
{
    send(Messages::WebPage::DidRemoveBackForwardItem(itemID));
}

void WebPageProxy::setCanRunModal(bool canRunModal)
{
    // It's only possible to change the state for a WebPage which
    // already qualifies for running modal child web pages, otherwise
    // there's no other possibility than not allowing it.
    m_canRunModal = m_uiClient->canRunModal() && canRunModal;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetCanRunModal(m_canRunModal));
}

bool WebPageProxy::canRunModal()
{
    return hasRunningProcess() ? m_canRunModal : false;
}

void WebPageProxy::beginPrinting(WebFrameProxy* frame, const PrintInfo& printInfo)
{
    if (m_isInPrintingMode)
        return;

    m_isInPrintingMode = true;
    if (m_isPerformingDOMPrintOperation)
        send(Messages::WebPage::BeginPrintingDuringDOMPrintOperation(frame->frameID(), printInfo), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        send(Messages::WebPage::BeginPrinting(frame->frameID(), printInfo));
}

void WebPageProxy::endPrinting()
{
    if (!m_isInPrintingMode)
        return;

    m_isInPrintingMode = false;

    if (m_isPerformingDOMPrintOperation)
        send(Messages::WebPage::EndPrintingDuringDOMPrintOperation(), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        send(Messages::WebPage::EndPrinting());
}

IPC::Connection::AsyncReplyID WebPageProxy::computePagesForPrinting(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&& callback)
{
    m_isInPrintingMode = true;
    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReply(Messages::WebPage::ComputePagesForPrintingDuringDOMPrintOperation(frameID, printInfo), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReply(Messages::WebPage::ComputePagesForPrinting(frameID, printInfo), WTFMove(callback));
}

#if PLATFORM(COCOA)
IPC::Connection::AsyncReplyID WebPageProxy::drawRectToImage(WebFrameProxy* frame, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(const WebKit::ShareableBitmapHandle&)>&& callback)
{
    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReply(Messages::WebPage::DrawRectToImageDuringDOMPrintOperation(frame->frameID(), printInfo, rect, imageSize), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReply(Messages::WebPage::DrawRectToImage(frame->frameID(), printInfo, rect, imageSize), WTFMove(callback));
}

IPC::Connection::AsyncReplyID WebPageProxy::drawPagesToPDF(WebFrameProxy* frame, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(API::Data*)>&& callback)
{
    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReply(Messages::WebPage::DrawPagesToPDFDuringDOMPrintOperation(frame->frameID(), printInfo, first, count), toAPIDataSharedBufferCallback(WTFMove(callback)), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReply(Messages::WebPage::DrawPagesToPDF(frame->frameID(), printInfo, first, count), toAPIDataSharedBufferCallback(WTFMove(callback)));
}
#elif PLATFORM(GTK)
void WebPageProxy::drawPagesForPrinting(WebFrameProxy* frame, const PrintInfo& printInfo, CompletionHandler<void(std::optional<SharedMemory::Handle>&&, ResourceError&&)>&& callback)
{
    m_isInPrintingMode = true;
    if (m_isPerformingDOMPrintOperation)
        sendWithAsyncReply(Messages::WebPage::DrawPagesForPrintingDuringDOMPrintOperation(frame->frameID(), printInfo), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        sendWithAsyncReply(Messages::WebPage::DrawPagesForPrinting(frame->frameID(), printInfo), WTFMove(callback));
}
#endif

#if PLATFORM(COCOA)
void WebPageProxy::drawToPDF(FrameIdentifier frameID, const std::optional<FloatRect>& rect, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::DrawToPDF(frameID, rect), WTFMove(callback));
}
#endif // PLATFORM(COCOA)

void WebPageProxy::getPDFFirstPageSize(WebCore::FrameIdentifier frameID, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetPDFFirstPageSize(frameID), WTFMove(completionHandler));
}

void WebPageProxy::updateBackingStoreDiscardableState()
{
    ASSERT(hasRunningProcess());

    if (!m_drawingArea)
        return;

    bool isDiscardable;

    if (!m_process->isResponsive())
        isDiscardable = false;
    else
        isDiscardable = !pageClient().isViewWindowActive() || !isViewVisible();

    m_drawingArea->setBackingStoreIsDiscardable(isDiscardable);
}

void WebPageProxy::saveDataToFileInDownloadsFolder(String&& suggestedFilename, String&& mimeType, URL&& originatingURLString, API::Data& data)
{
    m_uiClient->saveDataToFileInDownloadsFolder(this, ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename), mimeType, originatingURLString, data);
}

void WebPageProxy::savePDFToFileInDownloadsFolder(String&& suggestedFilename, URL&& originatingURL, const IPC::DataReference& dataReference)
{
    String sanitizedFilename = ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename);
    if (!sanitizedFilename.endsWithIgnoringASCIICase(".pdf"_s))
        return;

    saveDataToFileInDownloadsFolder(WTFMove(sanitizedFilename), "application/pdf"_s, WTFMove(originatingURL),
        API::Data::create(dataReference.data(), dataReference.size()).get());
}

void WebPageProxy::setMinimumSizeForAutoLayout(const IntSize& size)
{
    if (m_minimumSizeForAutoLayout == size)
        return;

    m_minimumSizeForAutoLayout = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMinimumSizeForAutoLayout(size));
    m_drawingArea->minimumSizeForAutoLayoutDidChange();

#if USE(APPKIT)
    if (m_minimumSizeForAutoLayout.width() <= 0)
        didChangeIntrinsicContentSize(IntSize(-1, -1));
#endif
}

void WebPageProxy::setSizeToContentAutoSizeMaximumSize(const IntSize& size)
{
    if (m_sizeToContentAutoSizeMaximumSize == size)
        return;

    m_sizeToContentAutoSizeMaximumSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetSizeToContentAutoSizeMaximumSize(size));
    m_drawingArea->sizeToContentAutoSizeMaximumSizeDidChange();

#if USE(APPKIT)
    if (m_sizeToContentAutoSizeMaximumSize.width() <= 0)
        didChangeIntrinsicContentSize(IntSize(-1, -1));
#endif
}

void WebPageProxy::setAutoSizingShouldExpandToViewHeight(bool shouldExpand)
{
    if (m_autoSizingShouldExpandToViewHeight == shouldExpand)
        return;

    m_autoSizingShouldExpandToViewHeight = shouldExpand;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetAutoSizingShouldExpandToViewHeight(shouldExpand));
}

void WebPageProxy::setViewportSizeForCSSViewportUnits(const FloatSize& viewportSize)
{
    if (m_viewportSizeForCSSViewportUnits && *m_viewportSizeForCSSViewportUnits == viewportSize)
        return;

    m_viewportSizeForCSSViewportUnits = viewportSize;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetViewportSizeForCSSViewportUnits(viewportSize));
}

#if USE(AUTOMATIC_TEXT_REPLACEMENT)

void WebPageProxy::toggleSmartInsertDelete()
{
    if (TextChecker::isTestingMode())
        TextChecker::setSmartInsertDeleteEnabled(!TextChecker::isSmartInsertDeleteEnabled());
}

void WebPageProxy::toggleAutomaticQuoteSubstitution()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
}

void WebPageProxy::toggleAutomaticLinkDetection()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
}

void WebPageProxy::toggleAutomaticDashSubstitution()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
}

void WebPageProxy::toggleAutomaticTextReplacement()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
}

#endif

#if USE(DICTATION_ALTERNATIVES)

void WebPageProxy::showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext dictationContext)
{
    pageClient().showDictationAlternativeUI(boundingBoxOfDictatedText, dictationContext);
}

void WebPageProxy::removeDictationAlternatives(WebCore::DictationContext dictationContext)
{
    pageClient().removeDictationAlternatives(dictationContext);
}

void WebPageProxy::dictationAlternatives(WebCore::DictationContext dictationContext, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(pageClient().dictationAlternatives(dictationContext));
}

#endif

#if PLATFORM(MAC)

void WebPageProxy::substitutionsPanelIsShowing(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(TextChecker::substitutionsPanelIsShowing());
}

void WebPageProxy::showCorrectionPanel(AlternativeTextType panelType, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    pageClient().showCorrectionPanel(panelType, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
}

void WebPageProxy::dismissCorrectionPanel(int32_t reason)
{
    // FIXME: Make ReasonForDismissingAlternativeText an enum class with EnumTraits and serialize it instead of casting to/from an int32_t.
    pageClient().dismissCorrectionPanel((ReasonForDismissingAlternativeText)reason);
}

void WebPageProxy::dismissCorrectionPanelSoon(int32_t reason, CompletionHandler<void(String)>&& completionHandler)
{
    // FIXME: Make ReasonForDismissingAlternativeText an enum class with EnumTraits and serialize it instead of casting to/from an int32_t.
    completionHandler(pageClient().dismissCorrectionPanelSoon((ReasonForDismissingAlternativeText)reason));
}

void WebPageProxy::recordAutocorrectionResponse(int32_t response, const String& replacedString, const String& replacementString)
{
    // FIXME: Make AutocorrectionResponse an enum class with EnumTraits and serialize it instead of casting to/from an int32_t.
    pageClient().recordAutocorrectionResponse(static_cast<AutocorrectionResponse>(response), replacedString, replacementString);
}

void WebPageProxy::handleAlternativeTextUIResult(const String& result)
{
    if (!isClosed())
        send(Messages::WebPage::HandleAlternativeTextUIResult(result));
}

void WebPageProxy::setEditableElementIsFocused(bool editableElementIsFocused)
{
    pageClient().setEditableElementIsFocused(editableElementIsFocused);
}

#endif // PLATFORM(MAC)

#if PLATFORM(COCOA) || PLATFORM(GTK)
RefPtr<ViewSnapshot> WebPageProxy::takeViewSnapshot(std::optional<WebCore::IntRect>&& clipRect)
{
    return pageClient().takeViewSnapshot(WTFMove(clipRect));
}
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
void WebPageProxy::cancelComposition(const String& compositionString)
{
    if (!hasRunningProcess())
        return;

    // Remove any pending composition key event.
    if (m_keyEventQueue.size() > 1) {
        auto event = m_keyEventQueue.takeFirst();
        m_keyEventQueue.removeAllMatching([](const auto& event) {
            return event.handledByInputMethod();
        });
        m_keyEventQueue.prepend(WTFMove(event));
    }
    send(Messages::WebPage::CancelComposition(compositionString));
}

void WebPageProxy::deleteSurrounding(int64_t offset, unsigned characterCount)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DeleteSurrounding(offset, characterCount));
}
#endif // PLATFORM(GTK) || PLATFORM(WPE)

void WebPageProxy::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    if (m_scrollPinningBehavior == pinning)
        return;
    
    m_scrollPinningBehavior = pinning;

    if (hasRunningProcess())
        send(Messages::WebPage::SetScrollPinningBehavior(pinning));
}

void WebPageProxy::setOverlayScrollbarStyle(std::optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle)
{
    if (!m_scrollbarOverlayStyle && !scrollbarStyle)
        return;

    if ((m_scrollbarOverlayStyle && scrollbarStyle) && m_scrollbarOverlayStyle.value() == scrollbarStyle.value())
        return;

    m_scrollbarOverlayStyle = scrollbarStyle;

    std::optional<uint32_t> scrollbarStyleForMessage;
    if (scrollbarStyle)
        scrollbarStyleForMessage = static_cast<ScrollbarOverlayStyle>(scrollbarStyle.value());

    if (hasRunningProcess())
        send(Messages::WebPage::SetScrollbarOverlayStyle(scrollbarStyleForMessage), m_webPageID);
}

#if ENABLE(WEB_CRYPTO)
void WebPageProxy::wrapCryptoKey(const Vector<uint8_t>& key, CompletionHandler<void(bool, Vector<uint8_t>&&)>&& completionHandler)
{
    PageClientProtector protector(pageClient());

    Vector<uint8_t> masterKey;

    if (auto keyData = m_navigationClient->webCryptoMasterKey(*this))
        masterKey = Vector(keyData->dataReference());

    Vector<uint8_t> wrappedKey;
    bool succeeded = wrapSerializedCryptoKey(masterKey, key, wrappedKey);
    completionHandler(succeeded, WTFMove(wrappedKey));
}

void WebPageProxy::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, CompletionHandler<void(bool, Vector<uint8_t>&&)>&& completionHandler)
{
    PageClientProtector protector(pageClient());

    Vector<uint8_t> masterKey;

    if (auto keyData = m_navigationClient->webCryptoMasterKey(*this))
        masterKey = Vector(keyData->dataReference());

    Vector<uint8_t> key;
    bool succeeded = unwrapSerializedCryptoKey(masterKey, wrappedKey, key);
    completionHandler(succeeded, WTFMove(key));
}
#endif

void WebPageProxy::addMIMETypeWithCustomContentProvider(const String& mimeType)
{
    send(Messages::WebPage::AddMIMETypeWithCustomContentProvider(mimeType));
}

void WebPageProxy::changeFontAttributes(WebCore::FontAttributeChanges&& changes)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ChangeFontAttributes(WTFMove(changes)));
}

void WebPageProxy::changeFont(WebCore::FontChanges&& changes)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ChangeFont(WTFMove(changes)));
}

// FIXME: Move these functions to WebPageProxyCocoa.mm.
#if PLATFORM(COCOA)

void WebPageProxy::setTextAsync(const String& text)
{
    if (hasRunningProcess())
        send(Messages::WebPage::SetTextAsync(text));
}

void WebPageProxy::insertTextAsync(const String& text, const EditingRange& replacementRange, InsertTextOptions&& options)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::InsertTextAsync(text, replacementRange, WTFMove(options)));
}

void WebPageProxy::hasMarkedText(CompletionHandler<void(bool)>&& callback)
{
    if (!hasRunningProcess()) {
        callback(false);
        return;
    }
    sendWithAsyncReply(Messages::WebPage::HasMarkedText(), WTFMove(callback));
}

void WebPageProxy::getMarkedRangeAsync(CompletionHandler<void(const EditingRange&)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(EditingRange());
        return;
    }

    sendWithAsyncReply(Messages::WebPage::GetMarkedRangeAsync(), WTFMove(callbackFunction));
}

void WebPageProxy::getSelectedRangeAsync(CompletionHandler<void(const EditingRange&)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(EditingRange());
        return;
    }

    sendWithAsyncReply(Messages::WebPage::GetSelectedRangeAsync(), WTFMove(callbackFunction));
}

void WebPageProxy::characterIndexForPointAsync(const WebCore::IntPoint& point, CompletionHandler<void(uint64_t)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::CharacterIndexForPointAsync(point), WTFMove(callbackFunction));
}

void WebPageProxy::firstRectForCharacterRangeAsync(const EditingRange& range, CompletionHandler<void(const WebCore::IntRect&, const EditingRange&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction({ }, { });

    sendWithAsyncReply(Messages::WebPage::FirstRectForCharacterRangeAsync(range), WTFMove(callbackFunction));
}

void WebPageProxy::setCompositionAsync(const String& text, const Vector<CompositionUnderline>& underlines, const Vector<CompositionHighlight>& highlights, const EditingRange& selectionRange, const EditingRange& replacementRange)
{
    if (!hasRunningProcess()) {
        // If this fails, we should call -discardMarkedText on input context to notify the input method.
        // This will happen naturally later, as part of reloading the page.
        return;
    }

    send(Messages::WebPage::SetCompositionAsync(text, underlines, highlights, selectionRange, replacementRange));
}

void WebPageProxy::confirmCompositionAsync()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ConfirmCompositionAsync());
}

void WebPageProxy::setScrollPerformanceDataCollectionEnabled(bool enabled)
{
    if (enabled == m_scrollPerformanceDataCollectionEnabled)
        return;

    m_scrollPerformanceDataCollectionEnabled = enabled;

    if (m_scrollPerformanceDataCollectionEnabled && !m_scrollingPerformanceData)
        m_scrollingPerformanceData = makeUnique<RemoteLayerTreeScrollingPerformanceData>(downcast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea));
    else if (!m_scrollPerformanceDataCollectionEnabled)
        m_scrollingPerformanceData = nullptr;
}
#endif

void WebPageProxy::takeSnapshot(IntRect rect, IntSize bitmapSize, SnapshotOptions options, CompletionHandler<void(const ShareableBitmapHandle&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::TakeSnapshot(rect, bitmapSize, options), WTFMove(callback));
}

void WebPageProxy::navigationGestureDidBegin()
{
    PageClientProtector protector(pageClient());

    m_isShowingNavigationGestureSnapshot = true;
    pageClient().navigationGestureDidBegin();

    m_navigationClient->didBeginNavigationGesture(*this);
}

void WebPageProxy::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    PageClientProtector protector(pageClient());
    if (willNavigate) {
        m_isLayerTreeFrozenDueToSwipeAnimation = true;
        send(Messages::WebPage::FreezeLayerTreeDueToSwipeAnimation());
    }

    pageClient().navigationGestureWillEnd(willNavigate, item);

    m_navigationClient->willEndNavigationGesture(*this, willNavigate, item);
}

void WebPageProxy::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    PageClientProtector protector(pageClient());

    pageClient().navigationGestureDidEnd(willNavigate, item);

    m_navigationClient->didEndNavigationGesture(*this, willNavigate, item);

    if (m_isLayerTreeFrozenDueToSwipeAnimation) {
        m_isLayerTreeFrozenDueToSwipeAnimation = false;
        send(Messages::WebPage::UnfreezeLayerTreeDueToSwipeAnimation());

        if (m_provisionalPage)
            m_provisionalPage->unfreezeLayerTreeDueToSwipeAnimation();
    }
}

void WebPageProxy::navigationGestureDidEnd()
{
    PageClientProtector protector(pageClient());

    pageClient().navigationGestureDidEnd();
}

void WebPageProxy::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    PageClientProtector protector(pageClient());

    pageClient().willRecordNavigationSnapshot(item);
}

void WebPageProxy::navigationGestureSnapshotWasRemoved()
{
    m_isShowingNavigationGestureSnapshot = false;

    // The ViewGestureController may call this method on a WebPageProxy whose view has been destroyed. In such case,
    // we need to return early as the pageClient will not be valid below.
    if (m_isClosed)
        return;

    pageClient().didRemoveNavigationGestureSnapshot();

    m_navigationClient->didRemoveNavigationGestureSnapshot(*this);
}

void WebPageProxy::isPlayingMediaDidChange(MediaProducerMediaStateFlags newState)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_process->throttler().shouldBeRunnable())
        return;
#endif

    if (!m_isClosed)
        updatePlayingMediaDidChange(newState, CanDelayNotification::Yes);
}

void WebPageProxy::updatePlayingMediaDidChange(MediaProducerMediaStateFlags newState, CanDelayNotification canDelayNotification)
{
#if ENABLE(MEDIA_STREAM)
    auto updateMediaCaptureStateImmediatelyIfNeeded = [&] {
        if (canDelayNotification == CanDelayNotification::No && m_updateReportedMediaCaptureStateTimer.isActive()) {
            m_updateReportedMediaCaptureStateTimer.stop();
            updateReportedMediaCaptureState();
        }
    };
#endif

    if (newState == m_mediaState) {
#if ENABLE(MEDIA_STREAM)
        updateMediaCaptureStateImmediatelyIfNeeded();
#endif
        return;
    }

#if PLATFORM(MACCATALYST)
    // When the page starts playing media for the first time, make sure we register with
    // the EndowmentStateTracker to get notifications when the application is no longer
    // user-facing, so that we can appropriately suspend all media playback.
    if (!m_isListeningForUserFacingStateChangeNotification) {
        EndowmentStateTracker::singleton().addClient(*this);
        m_isListeningForUserFacingStateChangeNotification = true;
    }
#endif

#if ENABLE(MEDIA_STREAM)
    WebCore::MediaProducerMediaStateFlags oldMediaCaptureState = m_mediaState & WebCore::MediaProducer::MediaCaptureMask;
    WebCore::MediaProducerMediaStateFlags newMediaCaptureState = newState & WebCore::MediaProducer::MediaCaptureMask;
#endif

    MediaProducerMediaStateFlags playingMediaMask { MediaProducerMediaState::IsPlayingAudio, MediaProducerMediaState::IsPlayingVideo };
    MediaProducerMediaStateFlags oldState = m_mediaState;

    bool playingAudioChanges = (oldState.contains(MediaProducerMediaState::IsPlayingAudio)) != (newState.contains(MediaProducerMediaState::IsPlayingAudio));
    if (playingAudioChanges)
        pageClient().isPlayingAudioWillChange();
    m_mediaState = newState;
    if (playingAudioChanges)
        pageClient().isPlayingAudioDidChange();

#if ENABLE(MEDIA_STREAM)
    if (oldMediaCaptureState != newMediaCaptureState) {
        updateReportedMediaCaptureState();

        ASSERT(m_userMediaPermissionRequestManager);
        if (m_userMediaPermissionRequestManager)
            m_userMediaPermissionRequestManager->captureStateChanged(oldMediaCaptureState, newMediaCaptureState);
    }
    updateMediaCaptureStateImmediatelyIfNeeded();
#endif

    activityStateDidChange({ ActivityState::IsAudible, ActivityState::IsCapturingMedia });

    playingMediaMask.add(WebCore::MediaProducer::MediaCaptureMask);
    if ((oldState & playingMediaMask) != (m_mediaState & playingMediaMask))
        m_uiClient->isPlayingMediaDidChange(*this);

    if ((oldState.containsAny(MediaProducerMediaState::HasAudioOrVideo)) != (m_mediaState.containsAny(MediaProducerMediaState::HasAudioOrVideo)))
        videoControlsManagerDidChange();

    m_process->updateAudibleMediaAssertions();
}

void WebPageProxy::updateReportedMediaCaptureState()
{
    auto activeCaptureState = m_mediaState & MediaProducer::MediaCaptureMask;
    if (m_reportedMediaCaptureState == activeCaptureState)
        return;

    bool haveReportedCapture = m_reportedMediaCaptureState.containsAny(MediaProducer::MediaCaptureMask);
    bool willReportCapture = !activeCaptureState.isEmpty();

    if (haveReportedCapture && !willReportCapture && m_updateReportedMediaCaptureStateTimer.isActive())
        return;

    if (!haveReportedCapture && willReportCapture)
        m_updateReportedMediaCaptureStateTimer.startOneShot(m_mediaCaptureReportingDelay);

    WEBPAGEPROXY_RELEASE_LOG(WebRTC, "updateReportedMediaCaptureState: from %d to %d", m_reportedMediaCaptureState.toRaw(), activeCaptureState.toRaw());

    bool microphoneCaptureChanged = (m_reportedMediaCaptureState & MediaProducer::MicrophoneCaptureMask) != (activeCaptureState & MediaProducer::MicrophoneCaptureMask);
    bool cameraCaptureChanged = (m_reportedMediaCaptureState & MediaProducer::VideoCaptureMask) != (activeCaptureState & MediaProducer::VideoCaptureMask);
    bool displayCaptureChanged = (m_reportedMediaCaptureState & MediaProducer::DisplayCaptureMask) != (activeCaptureState & MediaProducer::DisplayCaptureMask);
    bool systemAudioCaptureChanged = (m_reportedMediaCaptureState & MediaProducer::SystemAudioCaptureMask) != (activeCaptureState & MediaProducer::SystemAudioCaptureMask);

    auto reportedDisplayCaptureSurfaces = m_reportedMediaCaptureState & (MediaProducer::ScreenCaptureMask | MediaProducer::WindowCaptureMask);
    auto activeDisplayCaptureSurfaces = activeCaptureState & (MediaProducer::ScreenCaptureMask | MediaProducer::WindowCaptureMask);
    auto displayCaptureSurfacesChanged = reportedDisplayCaptureSurfaces != activeDisplayCaptureSurfaces;

    if (microphoneCaptureChanged)
        pageClient().microphoneCaptureWillChange();
    if (cameraCaptureChanged)
        pageClient().cameraCaptureWillChange();
    if (displayCaptureChanged)
        pageClient().displayCaptureWillChange();
    if (displayCaptureSurfacesChanged)
        pageClient().displayCaptureSurfacesWillChange();
    if (systemAudioCaptureChanged)
        pageClient().systemAudioCaptureWillChange();

    m_reportedMediaCaptureState = activeCaptureState;
    m_uiClient->mediaCaptureStateDidChange(m_mediaState);

    if (microphoneCaptureChanged)
        pageClient().microphoneCaptureChanged();
    if (cameraCaptureChanged)
        pageClient().cameraCaptureChanged();
    if (displayCaptureChanged)
        pageClient().displayCaptureChanged();
    if (displayCaptureSurfacesChanged)
        pageClient().displayCaptureSurfacesChanged();
    if (systemAudioCaptureChanged)
        pageClient().systemAudioCaptureChanged();
}

void WebPageProxy::videoControlsManagerDidChange()
{
    pageClient().videoControlsManagerDidChange();
}

bool WebPageProxy::hasActiveVideoForControlsManager() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return m_playbackSessionManager && m_playbackSessionManager->controlsManagerInterface();
#else
    return false;
#endif
}

void WebPageProxy::requestControlledElementID() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_playbackSessionManager)
        m_playbackSessionManager->requestControlledElementID();
#endif
}

void WebPageProxy::handleControlledElementIDResponse(const String& identifier) const
{
#if PLATFORM(MAC)
    pageClient().handleControlledElementIDResponse(identifier);
#endif
}

bool WebPageProxy::isPlayingVideoInEnhancedFullscreen() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoFullscreenManager && m_videoFullscreenManager->isPlayingVideoInEnhancedFullscreen();
#else
    return false;
#endif
}

void WebPageProxy::handleAutoplayEvent(WebCore::AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    m_uiClient->handleAutoplayEvent(*this, event, flags);
}

#if PLATFORM(MAC)
void WebPageProxy::performImmediateActionHitTestAtLocation(FloatPoint point)
{
    send(Messages::WebPage::PerformImmediateActionHitTestAtLocation(point));
}

void WebPageProxy::immediateActionDidUpdate()
{
    send(Messages::WebPage::ImmediateActionDidUpdate());
}

void WebPageProxy::immediateActionDidCancel()
{
    send(Messages::WebPage::ImmediateActionDidCancel());
}

void WebPageProxy::immediateActionDidComplete()
{
    send(Messages::WebPage::ImmediateActionDidComplete());
}

void WebPageProxy::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, const UserData& userData)
{
    pageClient().didPerformImmediateActionHitTest(result, contentPreventsDefault, m_process->transformHandlesToObjects(userData.object()).get());
}

NSObject *WebPageProxy::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    return pageClient().immediateActionAnimationControllerForHitTestResult(hitTestResult, type, userData);
}

void WebPageProxy::handleAcceptedCandidate(WebCore::TextCheckingResult acceptedCandidate)
{
    send(Messages::WebPage::HandleAcceptedCandidate(acceptedCandidate));
}

void WebPageProxy::didHandleAcceptedCandidate()
{
    pageClient().didHandleAcceptedCandidate();
}
    
void WebPageProxy::setUseSystemAppearance(bool useSystemAppearance)
{    
    if (useSystemAppearance == m_useSystemAppearance)
        return;
    
    m_useSystemAppearance = useSystemAppearance;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetUseSystemAppearance(useSystemAppearance));
}
    
void WebPageProxy::setHeaderBannerHeightForTesting(int height)
{
    send(Messages::WebPage::SetHeaderBannerHeightForTesting(height));
}

void WebPageProxy::setFooterBannerHeightForTesting(int height)
{
    send(Messages::WebPage::SetFooterBannerHeightForTesting(height));
}

void WebPageProxy::didEndMagnificationGesture()
{
    send(Messages::WebPage::DidEndMagnificationGesture());
}

#endif

void WebPageProxy::installActivityStateChangeCompletionHandler(CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    m_nextActivityStateChangeCallbacks.append(WTFMove(completionHandler));
}

void WebPageProxy::imageOrMediaDocumentSizeChanged(const WebCore::IntSize& newSize)
{
    m_uiClient->imageOrMediaDocumentSizeChanged(newSize);
}

void WebPageProxy::setShouldDispatchFakeMouseMoveEvents(bool shouldDispatchFakeMouseMoveEvents)
{
    send(Messages::WebPage::SetShouldDispatchFakeMouseMoveEvents(shouldDispatchFakeMouseMoveEvents));
}

void WebPageProxy::handleAutoFillButtonClick(const UserData& userData)
{
    m_uiClient->didClickAutoFillButton(*this, m_process->transformHandlesToObjects(userData.object()).get());
}

void WebPageProxy::didResignInputElementStrongPasswordAppearance(const UserData& userData)
{
    m_uiClient->didResignInputElementStrongPasswordAppearance(*this, m_process->transformHandlesToObjects(userData.object()).get());
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
void WebPageProxy::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    pageClient().mediaSessionManager().addPlaybackTargetPickerClient(*this, contextId);
}

void WebPageProxy::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    pageClient().mediaSessionManager().removePlaybackTargetPickerClient(*this, contextId);
}

void WebPageProxy::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const WebCore::FloatRect& rect, bool hasVideo)
{
    pageClient().mediaSessionManager().showPlaybackTargetPicker(*this, contextId, pageClient().rootViewToScreen(IntRect(rect)), hasVideo, useDarkAppearance());
}

void WebPageProxy::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, WebCore::MediaProducerMediaStateFlags state)
{
    pageClient().mediaSessionManager().clientStateDidChange(*this, contextId, state);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    pageClient().mediaSessionManager().setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerState(const String& name, WebCore::MediaPlaybackTargetContext::MockState state)
{
    pageClient().mediaSessionManager().setMockMediaPlaybackTargetPickerState(name, state);
}

void WebPageProxy::mockMediaPlaybackTargetPickerDismissPopup()
{
    pageClient().mediaSessionManager().mockMediaPlaybackTargetPickerDismissPopup();
}

void WebPageProxy::setPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, Ref<MediaPlaybackTarget>&& target)
{
    if (!hasRunningProcess())
        return;

    auto context = target->targetContext();
    ASSERT(context.type() != MediaPlaybackTargetContext::Type::SerializedAVOutputContext);
    if (preferences().useGPUProcessForMediaEnabled())
        context.serializeOutputContext();

    send(Messages::WebPage::PlaybackTargetSelected(contextId, context));
}

void WebPageProxy::externalOutputDeviceAvailableDidChange(PlaybackTargetClientContextIdentifier contextId, bool available)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::PlaybackTargetAvailabilityDidChange(contextId, available));
}

void WebPageProxy::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetShouldPlayToPlaybackTarget(contextId, shouldPlay));
}

void WebPageProxy::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::PlaybackTargetPickerWasDismissed(contextId));
}
#endif

void WebPageProxy::didChangeBackgroundColor()
{
    pageClient().didChangeBackgroundColor();
}

void WebPageProxy::clearWheelEventTestMonitor()
{
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::ClearWheelEventTestMonitor());
}

void WebPageProxy::callAfterNextPresentationUpdate(WTF::Function<void (CallbackBase::Error)>&& callback)
{
    if (!hasRunningProcess() || !m_drawingArea) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    m_drawingArea->dispatchAfterEnsuringDrawing(WTFMove(callback));
}

void WebPageProxy::setShouldScaleViewToFitDocument(bool shouldScaleViewToFitDocument)
{
    if (m_shouldScaleViewToFitDocument == shouldScaleViewToFitDocument)
        return;

    m_shouldScaleViewToFitDocument = shouldScaleViewToFitDocument;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetShouldScaleViewToFitDocument(shouldScaleViewToFitDocument));
}

void WebPageProxy::didRestoreScrollPosition()
{
    pageClient().didRestoreScrollPosition();
}

void WebPageProxy::getLoadDecisionForIcon(const WebCore::LinkIcon& icon, CallbackID loadIdentifier)
{
    m_iconLoadingClient->getLoadDecisionForIcon(icon, [this, protectedThis = Ref { *this }, loadIdentifier] (CompletionHandler<void(API::Data*)>&& callback) {
        if (!hasRunningProcess()) {
            if (callback)
                callback(nullptr);
            return;
        }

        if (!callback) {
            sendWithAsyncReply(Messages::WebPage::DidGetLoadDecisionForIcon(false, loadIdentifier), [](auto) { });
            return;
        }
        sendWithAsyncReply(Messages::WebPage::DidGetLoadDecisionForIcon(true, loadIdentifier), [callback = WTFMove(callback)](const IPC::SharedBufferReference& iconData) mutable {
            callback(API::Data::create(iconData.data(), iconData.size()).ptr());
        });
    });
}

WebCore::UserInterfaceLayoutDirection WebPageProxy::userInterfaceLayoutDirection()
{
    return pageClient().userInterfaceLayoutDirection();
}

void WebPageProxy::setUserInterfaceLayoutDirection(WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetUserInterfaceLayoutDirection(static_cast<uint32_t>(userInterfaceLayoutDirection)));
}

void WebPageProxy::hideValidationMessage()
{
#if PLATFORM(COCOA) || PLATFORM(GTK)
    m_validationBubble = nullptr;
#endif
}

// FIXME: Consolidate with dismissContentRelativeChildWindows
void WebPageProxy::closeOverlayedViews()
{
    hideValidationMessage();

#if ENABLE(DATALIST_ELEMENT)
    endDataListSuggestions();
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    endColorPicker();
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    endDateTimePicker();
#endif
}

#if ENABLE(POINTER_LOCK)
void WebPageProxy::requestPointerLock()
{
    ASSERT(!m_isPointerLockPending);
    ASSERT(!m_isPointerLocked);
    m_isPointerLockPending = true;

    if (!isViewVisible() || !(m_activityState & ActivityState::IsFocused)) {
        didDenyPointerLock();
        return;
    }
    m_uiClient->requestPointerLock(this);
}
    
void WebPageProxy::didAllowPointerLock()
{
    ASSERT(m_isPointerLockPending && !m_isPointerLocked);
    m_isPointerLocked = true;
    m_isPointerLockPending = false;
#if PLATFORM(MAC)
    CGDisplayHideCursor(CGMainDisplayID());
    CGAssociateMouseAndMouseCursorPosition(false);
#endif
    send(Messages::WebPage::DidAcquirePointerLock());
}
    
void WebPageProxy::didDenyPointerLock()
{
    ASSERT(m_isPointerLockPending && !m_isPointerLocked);
    m_isPointerLockPending = false;
    send(Messages::WebPage::DidNotAcquirePointerLock());
}

void WebPageProxy::requestPointerUnlock()
{
    if (m_isPointerLocked) {
#if PLATFORM(MAC)
        CGAssociateMouseAndMouseCursorPosition(true);
        CGDisplayShowCursor(CGMainDisplayID());
#endif
        m_uiClient->didLosePointerLock(this);
        send(Messages::WebPage::DidLosePointerLock());
    }

    if (m_isPointerLockPending) {
        m_uiClient->didLosePointerLock(this);
        send(Messages::WebPage::DidNotAcquirePointerLock());
    }

    m_isPointerLocked = false;
    m_isPointerLockPending = false;
}
#endif

void WebPageProxy::setURLSchemeHandlerForScheme(Ref<WebURLSchemeHandler>&& handler, const String& scheme)
{
    auto canonicalizedScheme = WTF::URLParser::maybeCanonicalizeScheme(scheme);
    ASSERT(canonicalizedScheme);
    ASSERT(!WTF::URLParser::isSpecialScheme(canonicalizedScheme.value()));

    auto schemeResult = m_urlSchemeHandlersByScheme.add(canonicalizedScheme.value(), handler.get());
    ASSERT_UNUSED(schemeResult, schemeResult.isNewEntry);

    auto handlerIdentifier = handler->identifier();
    auto handlerIdentifierResult = m_urlSchemeHandlersByIdentifier.add(handlerIdentifier, WTFMove(handler));
    ASSERT_UNUSED(handlerIdentifierResult, handlerIdentifierResult.isNewEntry);

    send(Messages::WebPage::RegisterURLSchemeHandler(handlerIdentifier, canonicalizedScheme.value()));
}

WebURLSchemeHandler* WebPageProxy::urlSchemeHandlerForScheme(const String& scheme)
{
    return scheme.isNull() ? nullptr : m_urlSchemeHandlersByScheme.get(scheme);
}

void WebPageProxy::startURLSchemeTask(URLSchemeTaskParameters&& parameters)
{
    startURLSchemeTaskShared(m_process.copyRef(), m_webPageID, WTFMove(parameters));
}

void WebPageProxy::startURLSchemeTaskShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, URLSchemeTaskParameters&& parameters)
{
    MESSAGE_CHECK(m_process, decltype(m_urlSchemeHandlersByIdentifier)::isValidKey(parameters.handlerIdentifier));
    auto iterator = m_urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK(process, iterator != m_urlSchemeHandlersByIdentifier.end());

    iterator->value->startTask(*this, process, webPageID, WTFMove(parameters), nullptr);
}

void WebPageProxy::stopURLSchemeTask(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier)
{
    MESSAGE_CHECK(m_process, decltype(m_urlSchemeHandlersByIdentifier)::isValidKey(handlerIdentifier));
    auto iterator = m_urlSchemeHandlersByIdentifier.find(handlerIdentifier);
    MESSAGE_CHECK(m_process, iterator != m_urlSchemeHandlersByIdentifier.end());

    iterator->value->stopTask(*this, taskIdentifier);
}

void WebPageProxy::loadSynchronousURLSchemeTask(URLSchemeTaskParameters&& parameters, CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, Vector<uint8_t>&&)>&& reply)
{
    MESSAGE_CHECK(m_process, decltype(m_urlSchemeHandlersByIdentifier)::isValidKey(parameters.handlerIdentifier));
    auto iterator = m_urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK(m_process, iterator != m_urlSchemeHandlersByIdentifier.end());

    iterator->value->startTask(*this, m_process, m_webPageID, WTFMove(parameters), WTFMove(reply));
}

#if ENABLE(TRACKING_PREVENTION)
void WebPageProxy::requestStorageAccessConfirm(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, CompletionHandler<void(bool)>&& completionHandler)
{
    m_uiClient->requestStorageAccessConfirm(*this, WebFrameProxy::webFrame(frameID), subFrameDomain, topFrameDomain, WTFMove(completionHandler));
}

void WebPageProxy::didCommitCrossSiteLoadWithDataTransferFromPrevalentResource()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::WasLoadedWithDataTransferFromPrevalentResource());
}
#endif

bool WebPageProxy::useDarkAppearance() const
{
    return pageClient().effectiveAppearanceIsDark();
}

bool WebPageProxy::useElevatedUserInterfaceLevel() const
{
    return pageClient().effectiveUserInterfaceLevelIsElevated();
}

void WebPageProxy::effectiveAppearanceDidChange()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::EffectiveAppearanceDidChange(useDarkAppearance(), useElevatedUserInterfaceLevel()));
}

#if HAVE(TOUCH_BAR)
void WebPageProxy::touchBarMenuDataChanged(const TouchBarMenuData& touchBarMenuData)
{
    m_touchBarMenuData = touchBarMenuData;
}

void WebPageProxy::touchBarMenuItemDataAdded(const TouchBarMenuItemData& touchBarMenuItemData)
{
    m_touchBarMenuData.addMenuItem(touchBarMenuItemData);
}

void WebPageProxy::touchBarMenuItemDataRemoved(const TouchBarMenuItemData& touchBarMenuItemData)
{
    m_touchBarMenuData.removeMenuItem(touchBarMenuItemData);
}
#endif

DataOwnerType WebPageProxy::dataOwnerForPasteboard(PasteboardAccessIntent intent) const
{
    return pageClient().dataOwnerForPasteboard(intent);
}

#if ENABLE(ATTACHMENT_ELEMENT)

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&& info, const String& authorizationToken)
{
    MESSAGE_CHECK(m_process, isValidPerformActionOnElementAuthorizationToken(authorizationToken));

    pageClient().writePromisedAttachmentToPasteboard(WTFMove(info));
}
#endif

void WebPageProxy::requestAttachmentIcon(const String& identifier, const String& contentType, const String& fileName, const String& title, const FloatSize& requestedSize)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());

    auto updateAttachmentIcon = [&] {
        FloatSize size = requestedSize;
        ShareableBitmapHandle handle;

#if PLATFORM(COCOA)
        if (auto icon = iconForAttachment(fileName, contentType, title, size))
            if (auto iconHandle = icon->createHandle())
                handle = WTFMove(*iconHandle);
#endif

        send(Messages::WebPage::UpdateAttachmentIcon(identifier, handle, size));
    };

#if PLATFORM(MAC)
    if (auto attachment = attachmentForIdentifier(identifier); attachment && attachment->contentType() == "public.directory"_s) {
        attachment->doWithFileWrapper([&, updateAttachmentIcon = WTFMove(updateAttachmentIcon)] (NSFileWrapper *fileWrapper) {
            if (updateIconForDirectory(fileWrapper, attachment->identifier()))
                return;

            updateAttachmentIcon();
        });
        return;
    }
#endif

    updateAttachmentIcon();
}

RefPtr<API::Attachment> WebPageProxy::attachmentForIdentifier(const String& identifier) const
{
    if (identifier.isEmpty())
        return nullptr;

    return m_attachmentIdentifierToAttachmentMap.get(identifier);
}

void WebPageProxy::insertAttachment(Ref<API::Attachment>&& attachment, CompletionHandler<void()>&& callback)
{
    auto attachmentIdentifier = attachment->identifier();
    sendWithAsyncReply(Messages::WebPage::InsertAttachment(attachmentIdentifier, attachment->fileSizeForDisplay(), attachment->fileName(), attachment->contentType()), WTFMove(callback));
    m_attachmentIdentifierToAttachmentMap.set(attachmentIdentifier, WTFMove(attachment));
}

void WebPageProxy::updateAttachmentAttributes(const API::Attachment& attachment, CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::UpdateAttachmentAttributes(attachment.identifier(), attachment.fileSizeForDisplay(), attachment.contentType(), attachment.fileName(), IPC::SharedBufferReference(attachment.enclosingImageData())), WTFMove(callback));

#if HAVE(QUICKLOOK_THUMBNAILING)
    requestThumbnail(attachment, attachment.identifier());
#endif
}

#if HAVE(QUICKLOOK_THUMBNAILING)
void WebPageProxy::updateAttachmentThumbnail(const String& identifier, const RefPtr<ShareableBitmap>& bitmap)
{
    if (!hasRunningProcess())
        return;
    
    ShareableBitmapHandle handle;
    if (bitmap) {
        if (auto bitmapHandle = bitmap->createHandle())
            handle = WTFMove(*bitmapHandle);
    }

    send(Messages::WebPage::UpdateAttachmentThumbnail(identifier, handle));
}
#endif

void WebPageProxy::registerAttachmentIdentifierFromData(const String& identifier, const String& contentType, const String& preferredFileName, const IPC::SharedBufferReference& data)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    if (attachmentForIdentifier(identifier))
        return;

    auto attachment = ensureAttachment(identifier);
    attachment->setContentType(contentType);
    m_attachmentIdentifierToAttachmentMap.set(identifier, attachment.copyRef());

    platformRegisterAttachment(WTFMove(attachment), preferredFileName, data);
}

void WebPageProxy::registerAttachmentIdentifierFromFilePath(const String& identifier, const String& contentType, const String& filePath)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    if (attachmentForIdentifier(identifier))
        return;

    auto attachment = ensureAttachment(identifier);
    attachment->setContentType(contentType);
    attachment->setFilePath(filePath);
    m_attachmentIdentifierToAttachmentMap.set(identifier, attachment.copyRef());
    platformRegisterAttachment(WTFMove(attachment), filePath);
#if HAVE(QUICKLOOK_THUMBNAILING)
    requestThumbnailWithPath(filePath, identifier);
#endif
}

void WebPageProxy::registerAttachmentIdentifier(const String& identifier)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    if (!attachmentForIdentifier(identifier))
        m_attachmentIdentifierToAttachmentMap.set(identifier, ensureAttachment(identifier));
}

void WebPageProxy::registerAttachmentsFromSerializedData(Vector<WebCore::SerializedAttachmentData>&& data)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());

    for (auto& serializedData : data) {
        auto identifier = WTFMove(serializedData.identifier);
        if (!attachmentForIdentifier(identifier)) {
            auto attachment = ensureAttachment(identifier);
            attachment->updateFromSerializedRepresentation(WTFMove(serializedData.data), WTFMove(serializedData.mimeType));
#if HAVE(QUICKLOOK_THUMBNAILING)
            requestThumbnail(attachment, identifier);
#endif
        }
    }
}

void WebPageProxy::cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(fromIdentifier));
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(toIdentifier));

    auto newAttachment = ensureAttachment(toIdentifier);
    auto existingAttachment = attachmentForIdentifier(fromIdentifier);
    if (!existingAttachment) {
        ASSERT_NOT_REACHED();
        return;
    }

    newAttachment->setContentType(existingAttachment->contentType());
    newAttachment->setFilePath(existingAttachment->filePath());

    platformCloneAttachment(existingAttachment.releaseNonNull(), WTFMove(newAttachment));
}

void WebPageProxy::invalidateAllAttachments()
{
    for (auto& attachment : m_attachmentIdentifierToAttachmentMap.values()) {
        if (attachment->insertionState() == API::Attachment::InsertionState::Inserted)
            didRemoveAttachment(attachment.get());
        attachment->invalidate();
    }
    m_attachmentIdentifierToAttachmentMap.clear();
}

void WebPageProxy::serializedAttachmentDataForIdentifiers(const Vector<String>& identifiers, CompletionHandler<void(Vector<WebCore::SerializedAttachmentData>&&)>&& completionHandler)
{
    Vector<WebCore::SerializedAttachmentData> serializedData;

    MESSAGE_CHECK_COMPLETION(m_process, m_preferences->attachmentElementEnabled(), completionHandler(WTFMove(serializedData)));

    for (const auto& identifier : identifiers) {
        auto attachment = attachmentForIdentifier(identifier);
        if (!attachment)
            continue;

        auto data = attachment->createSerializedRepresentation();
        if (!data)
            continue;

        serializedData.append({ identifier, attachment->mimeType(), data.releaseNonNull() });
    }
    completionHandler(WTFMove(serializedData));
}

void WebPageProxy::didInvalidateDataForAttachment(API::Attachment& attachment)
{
    pageClient().didInvalidateDataForAttachment(attachment);
}

WebPageProxy::ShouldUpdateAttachmentAttributes WebPageProxy::willUpdateAttachmentAttributes(const API::Attachment& attachment)
{
    return ShouldUpdateAttachmentAttributes::Yes;
}

#if !PLATFORM(COCOA)

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&&, const String&, const IPC::SharedBufferReference&)
{
}

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&&, const String&)
{
}

void WebPageProxy::platformCloneAttachment(Ref<API::Attachment>&&, Ref<API::Attachment>&&)
{
}

#endif

void WebPageProxy::didInsertAttachmentWithIdentifier(const String& identifier, const String& source, bool hasEnclosingImage)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    auto attachment = ensureAttachment(identifier);
    attachment->setHasEnclosingImage(hasEnclosingImage);
    attachment->setInsertionState(API::Attachment::InsertionState::Inserted);
    pageClient().didInsertAttachment(attachment.get(), source);

    if (!attachment->isEmpty() && hasEnclosingImage)
        updateAttachmentAttributes(attachment.get(), [] { });
}

void WebPageProxy::didRemoveAttachmentWithIdentifier(const String& identifier)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    if (auto attachment = attachmentForIdentifier(identifier))
        didRemoveAttachment(*attachment);
}

void WebPageProxy::didRemoveAttachment(API::Attachment& attachment)
{
    attachment.setInsertionState(API::Attachment::InsertionState::NotInserted);
    pageClient().didRemoveAttachment(attachment);
}

Ref<API::Attachment> WebPageProxy::ensureAttachment(const String& identifier)
{
    if (auto existingAttachment = attachmentForIdentifier(identifier))
        return *existingAttachment;

    auto attachment = API::Attachment::create(identifier, *this);
    m_attachmentIdentifierToAttachmentMap.set(identifier, attachment.copyRef());
    return attachment;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(APPLICATION_MANIFEST)
void WebPageProxy::getApplicationManifest(CompletionHandler<void(const std::optional<WebCore::ApplicationManifest>&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetApplicationManifest(), WTFMove(callback));
}
#endif

void WebPageProxy::getTextFragmentMatch(CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetTextFragmentMatch(), WTFMove(callback));
}

#if ENABLE(APP_HIGHLIGHTS)
void WebPageProxy::storeAppHighlight(const WebCore::AppHighlight& highlight)
{
    MESSAGE_CHECK(m_process, !highlight.highlight->isEmpty());

    pageClient().storeAppHighlight(highlight);

}
#endif

namespace {
enum class CompletionCondition {
    Cancellation,
    Error,
    Success,
    Timeout,
};
struct MessageType {
    CompletionCondition condition;
    Seconds seconds;
    String message;
};
}

void WebPageProxy::reportPageLoadResult(const ResourceError& error)
{
    static const NeverDestroyed<Vector<MessageType>> messages(std::initializer_list<MessageType> {
        { CompletionCondition::Cancellation, 2_s, DiagnosticLoggingKeys::canceledLessThan2SecondsKey() },
        { CompletionCondition::Cancellation, 5_s, DiagnosticLoggingKeys::canceledLessThan5SecondsKey() },
        { CompletionCondition::Cancellation, 20_s, DiagnosticLoggingKeys::canceledLessThan20SecondsKey() },
        { CompletionCondition::Cancellation, Seconds::infinity(), DiagnosticLoggingKeys::canceledMoreThan20SecondsKey() },

        { CompletionCondition::Error, 2_s, DiagnosticLoggingKeys::failedLessThan2SecondsKey() },
        { CompletionCondition::Error, 5_s, DiagnosticLoggingKeys::failedLessThan5SecondsKey() },
        { CompletionCondition::Error, 20_s, DiagnosticLoggingKeys::failedLessThan20SecondsKey() },
        { CompletionCondition::Error, Seconds::infinity(), DiagnosticLoggingKeys::failedMoreThan20SecondsKey() },

        { CompletionCondition::Success, 2_s, DiagnosticLoggingKeys::succeededLessThan2SecondsKey() },
        { CompletionCondition::Success, 5_s, DiagnosticLoggingKeys::succeededLessThan5SecondsKey() },
        { CompletionCondition::Success, 20_s, DiagnosticLoggingKeys::succeededLessThan20SecondsKey() },
        { CompletionCondition::Success, Seconds::infinity(), DiagnosticLoggingKeys::succeededMoreThan20SecondsKey() },

        { CompletionCondition::Timeout, Seconds::infinity(), DiagnosticLoggingKeys::timedOutKey() }
        });

    if (!m_pageLoadStart)
        return;

    auto pageLoadTime = MonotonicTime::now() - *m_pageLoadStart;
    m_pageLoadStart = std::nullopt;

    CompletionCondition condition { CompletionCondition::Success };
    if (error.isCancellation())
        condition = CompletionCondition::Cancellation;
    else if (error.isTimeout())
        condition = CompletionCondition::Timeout;
    else if (!error.isNull() || error.errorCode())
        condition = CompletionCondition::Error;

    for (auto& messageItem : messages.get()) {
        if (condition == messageItem.condition && pageLoadTime < messageItem.seconds) {
            logDiagnosticMessage(DiagnosticLoggingKeys::telemetryPageLoadKey(), messageItem.message, ShouldSample::No);
            logDiagnosticMessage(DiagnosticLoggingKeys::telemetryPageLoadKey(), DiagnosticLoggingKeys::occurredKey(), ShouldSample::No);
            break;
        }
    }
}

void WebPageProxy::setDefersLoadingForTesting(bool defersLoading)
{
    send(Messages::WebPage::SetDefersLoading(defersLoading));
}

void WebPageProxy::getIsViewVisible(bool& result)
{
    result = isViewVisible();
}

void WebPageProxy::updateCurrentModifierState()
{
#if PLATFORM(COCOA)
    auto modifiers = PlatformKeyboardEvent::currentStateOfModifierKeys();
    send(Messages::WebPage::UpdateCurrentModifierState(modifiers));
#endif
}

bool WebPageProxy::checkURLReceivedFromCurrentOrPreviousWebProcess(WebProcessProxy& process, const String& urlString)
{
    return checkURLReceivedFromCurrentOrPreviousWebProcess(process, URL { urlString });
}

bool WebPageProxy::checkURLReceivedFromCurrentOrPreviousWebProcess(WebProcessProxy& process, const URL& url)
{
    if (!url.isLocalFile())
        return true;

    if (m_mayHaveUniversalFileReadSandboxExtension)
        return true;

    String path = url.fileSystemPath();
    auto startsWithURLPath = [&path](const String& visitedPath) {
        return path.startsWith(visitedPath);
    };

    auto localPathsEnd = m_previouslyVisitedPaths.end();
    if (std::find_if(m_previouslyVisitedPaths.begin(), localPathsEnd, startsWithURLPath) != localPathsEnd)
        return true;

    return process.checkURLReceivedFromWebProcess(url);
}

void WebPageProxy::addPreviouslyVisitedPath(const String& path)
{
    m_previouslyVisitedPaths.add(path);
}

void WebPageProxy::willAcquireUniversalFileReadSandboxExtension(WebProcessProxy& process)
{
    m_mayHaveUniversalFileReadSandboxExtension = true;
    process.willAcquireUniversalFileReadSandboxExtension();
}

void WebPageProxy::simulateDeviceOrientationChange(double alpha, double beta, double gamma)
{
    send(Messages::WebPage::SimulateDeviceOrientationChange(alpha, beta, gamma));
}

#if ENABLE(DATA_DETECTION)

void WebPageProxy::detectDataInAllFrames(OptionSet<WebCore::DataDetectorType> types, CompletionHandler<void(const DataDetectionResult&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::DetectDataInAllFrames(types), WTFMove(completionHandler));
}

void WebPageProxy::removeDataDetectedLinks(CompletionHandler<void(const DataDetectionResult&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::RemoveDataDetectedLinks(), WTFMove(completionHandler));
}

#endif

#if USE(SYSTEM_PREVIEW)
void WebPageProxy::systemPreviewActionTriggered(const WebCore::SystemPreviewInfo& previewInfo, const String& message)
{
    send(Messages::WebPage::SystemPreviewActionTriggered(previewInfo, message));
}
#endif

void WebPageProxy::dumpPrivateClickMeasurement(CompletionHandler<void(const String&)>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::DumpPrivateClickMeasurement(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::ClearPrivateClickMeasurement(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementOverrideTimerForTesting(bool value, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementOverrideTimerForTesting(m_websiteDataStore->sessionID(), value), WTFMove(completionHandler));
}

void WebPageProxy::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementEphemeralMeasurementForTesting(m_websiteDataStore->sessionID(), value), WTFMove(completionHandler));
}

void WebPageProxy::simulatePrivateClickMeasurementSessionRestart(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SimulatePrivateClickMeasurementSessionRestart(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementTokenPublicKeyURLForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenPublicKeyURLForTesting(m_websiteDataStore->sessionID(), url), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementTokenSignatureURLForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenSignatureURLForTesting(m_websiteDataStore->sessionID(), url), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementAttributionReportURLsForTesting(const URL& sourceURL, const URL& destinationURL, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAttributionReportURLsForTesting(m_websiteDataStore->sessionID(), sourceURL, destinationURL), WTFMove(completionHandler));
}

void WebPageProxy::markPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::MarkPrivateClickMeasurementsAsExpiredForTesting(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPCMFraudPreventionValuesForTesting(const String& unlinkableToken, const String& secretToken, const String& signature, const String& keyID, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPCMFraudPreventionValuesForTesting(m_websiteDataStore->sessionID(), unlinkableToken, secretToken, signature, keyID), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementAppBundleIDForTesting(const String& appBundleIDForTesting, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAppBundleIDForTesting(m_websiteDataStore->sessionID(), appBundleIDForTesting), WTFMove(completionHandler));
}

#if ENABLE(SPEECH_SYNTHESIS)

void WebPageProxy::resetSpeechSynthesizer()
{
    if (!m_speechSynthesisData)
        return;
    
    auto& synthesisData = speechSynthesisData();
    synthesisData.speakingFinishedCompletionHandler = nullptr;
    synthesisData.speakingStartedCompletionHandler = nullptr;
    synthesisData.speakingPausedCompletionHandler = nullptr;
    synthesisData.speakingResumedCompletionHandler = nullptr;
    synthesisData.synthesizer->resetState();
}

WebPageProxy::SpeechSynthesisData& WebPageProxy::speechSynthesisData()
{
    if (!m_speechSynthesisData)
        m_speechSynthesisData = SpeechSynthesisData { PlatformSpeechSynthesizer::create(*this), nullptr, nullptr, nullptr, nullptr, nullptr };
    return *m_speechSynthesisData;
}

void WebPageProxy::speechSynthesisVoiceList(CompletionHandler<void(Vector<WebSpeechSynthesisVoice>&&)>&& completionHandler)
{
    auto result = speechSynthesisData().synthesizer->voiceList().map([](auto& voice) {
        return WebSpeechSynthesisVoice { voice->voiceURI(), voice->name(), voice->lang(), voice->localService(), voice->isDefault() };
    });
    completionHandler(WTFMove(result));
}

void WebPageProxy::speechSynthesisSetFinishedCallback(CompletionHandler<void()>&& completionHandler)
{
    speechSynthesisData().speakingFinishedCompletionHandler = WTFMove(completionHandler);
}

void WebPageProxy::speechSynthesisSpeak(const String& text, const String& lang, float volume, float rate, float pitch, MonotonicTime, const String& voiceURI, const String& voiceName, const String& voiceLang, bool localService, bool defaultVoice, CompletionHandler<void()>&& completionHandler)
{
    auto voice = WebCore::PlatformSpeechSynthesisVoice::create(voiceURI, voiceName, voiceLang, localService, defaultVoice);
    auto utterance = WebCore::PlatformSpeechSynthesisUtterance::create(*this);
    utterance->setText(text);
    utterance->setLang(lang);
    utterance->setVolume(volume);
    utterance->setRate(rate);
    utterance->setPitch(pitch);
    utterance->setVoice(&voice.get());

    speechSynthesisData().speakingStartedCompletionHandler = WTFMove(completionHandler);
    speechSynthesisData().utterance = WTFMove(utterance);
    speechSynthesisData().synthesizer->speak(m_speechSynthesisData->utterance.get());
}

void WebPageProxy::speechSynthesisCancel()
{
    speechSynthesisData().synthesizer->cancel();
}

void WebPageProxy::speechSynthesisResetState()
{
    speechSynthesisData().synthesizer->resetState();
}

void WebPageProxy::speechSynthesisPause(CompletionHandler<void()>&& completionHandler)
{
    speechSynthesisData().speakingPausedCompletionHandler = WTFMove(completionHandler);
    speechSynthesisData().synthesizer->pause();
}

void WebPageProxy::speechSynthesisResume(CompletionHandler<void()>&& completionHandler)
{
    speechSynthesisData().speakingResumedCompletionHandler = WTFMove(completionHandler);
    speechSynthesisData().synthesizer->resume();
}
#endif // ENABLE(SPEECH_SYNTHESIS)

#if !PLATFORM(IOS_FAMILY)

WebContentMode WebPageProxy::effectiveContentModeAfterAdjustingPolicies(API::WebsitePolicies&, const WebCore::ResourceRequest&)
{
    return WebContentMode::Recommended;
}

#endif // !PLATFORM(IOS_FAMILY)

void WebPageProxy::addDidMoveToWindowObserver(WebViewDidMoveToWindowObserver& observer)
{
    auto result = m_webViewDidMoveToWindowObservers.add(&observer, observer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void WebPageProxy::removeDidMoveToWindowObserver(WebViewDidMoveToWindowObserver& observer)
{
    auto result = m_webViewDidMoveToWindowObservers.remove(&observer);
    ASSERT_UNUSED(result, result);
}

void WebPageProxy::webViewDidMoveToWindow()
{
    auto observersCopy = m_webViewDidMoveToWindowObservers;
    for (const auto& observer : observersCopy) {
        if (!observer.value)
            continue;
        observer.value->webViewDidMoveToWindow();
    }

    auto newWindowKind = pageClient().windowKind();
    if (m_windowKind != newWindowKind) {
        m_windowKind = newWindowKind;
        if (m_drawingArea)
            m_drawingArea->windowKindDidChange();    
    }
}

void WebPageProxy::setCanShowPlaceholder(const WebCore::ElementContext& context, bool canShowPlaceholder)
{
    if (hasRunningProcess())
        send(Messages::WebPage::SetCanShowPlaceholder(context, canShowPlaceholder));
}

Logger& WebPageProxy::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        // FIXME: Does this really need to be disabled in ephemeral sessions?
        m_logger->setEnabled(this, !sessionID().isEphemeral());
    }

    return *m_logger;
}

void WebPageProxy::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
#if !RELEASE_LOG_DISABLED
    auto* channel = getLogChannel(channelName);
    if  (!channel)
        return;

    channel->state = state;
    channel->level = level;
#else
    UNUSED_PARAM(channelName);
    UNUSED_PARAM(state);
    UNUSED_PARAM(level);
#endif
}

#if HAVE(APP_SSO)
void WebPageProxy::decidePolicyForSOAuthorizationLoad(const String& extension, CompletionHandler<void(SOAuthorizationLoadPolicy)>&& completionHandler)
{
    m_navigationClient->decidePolicyForSOAuthorizationLoad(*this, SOAuthorizationLoadPolicy::Allow, extension, WTFMove(completionHandler));
}
#endif

#if ENABLE(WEB_AUTHN)
void WebPageProxy::setMockWebAuthenticationConfiguration(MockWebAuthenticationConfiguration&& configuration)
{
    m_websiteDataStore->setMockWebAuthenticationConfiguration(WTFMove(configuration));
}
#endif

void WebPageProxy::startTextManipulations(const Vector<WebCore::TextManipulationController::ExclusionRule>& exclusionRules,
    TextManipulationItemCallback&& callback, WTF::CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }
    m_textManipulationItemCallback = WTFMove(callback);
    sendWithAsyncReply(Messages::WebPage::StartTextManipulations(exclusionRules), WTFMove(completionHandler));
}

void WebPageProxy::didFindTextManipulationItems(const Vector<WebCore::TextManipulationItem>& items)
{
    if (!m_textManipulationItemCallback)
        return;
    m_textManipulationItemCallback(items);
}

void WebPageProxy::completeTextManipulation(const Vector<WebCore::TextManipulationItem>& items,
    WTF::Function<void(bool allFailed, const Vector<WebCore::TextManipulationController::ManipulationFailure>&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler(true, { });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::CompleteTextManipulation(items), WTFMove(completionHandler));
}

void WebPageProxy::setCORSDisablingPatterns(Vector<String>&& patterns)
{
    m_corsDisablingPatterns = WTFMove(patterns);
    send(Messages::WebPage::UpdateCORSDisablingPatterns(m_corsDisablingPatterns));
}

void WebPageProxy::setOverriddenMediaType(const String& mediaType)
{
    m_overriddenMediaType = mediaType;
    send(Messages::WebPage::SetOverriddenMediaType(mediaType));
}

void WebPageProxy::setIsTakingSnapshotsForApplicationSuspension(bool isTakingSnapshotsForApplicationSuspension)
{
    send(Messages::WebPage::SetIsTakingSnapshotsForApplicationSuspension(isTakingSnapshotsForApplicationSuspension));
}

void WebPageProxy::setNeedsDOMWindowResizeEvent()
{
    send(Messages::WebPage::SetNeedsDOMWindowResizeEvent());
}

void WebPageProxy::loadServiceWorker(const URL& url, bool usingModules, CompletionHandler<void(bool success)>&& completionHandler)
{
#if ENABLE(SERVICE_WORKER)
    if (m_isClosed)
        return completionHandler(false);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadServiceWorker:");

    if (m_serviceWorkerLaunchCompletionHandler)
        return completionHandler(false);

    m_isServiceWorkerPage = true;
    m_serviceWorkerLaunchCompletionHandler = WTFMove(completionHandler);

    CString html;
    if (usingModules)
        html = makeString("<script>navigator.serviceWorker.register('", url.string().utf8().data(), "', { type: 'module' });</script>").utf8();
    else
        html = makeString("<script>navigator.serviceWorker.register('", url.string().utf8().data(), "');</script>").utf8();

    loadData({ reinterpret_cast<const uint8_t*>(html.data()), html.length() }, "text/html"_s, "UTF-8"_s, url.protocolHostAndPort());
#else
    UNUSED_PARAM(url);
    completionHandler(false);
#endif
}

#if !PLATFORM(IOS_FAMILY)
bool WebPageProxy::shouldForceForegroundPriorityForClientNavigation() const
{
    return false;
}
#endif

void WebPageProxy::getProcessDisplayName(CompletionHandler<void(String&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetProcessDisplayName(), WTFMove(completionHandler));
}

void WebPageProxy::setOrientationForMediaCapture(uint64_t orientation)
{
#if ENABLE(MEDIA_STREAM)
#if PLATFORM(COCOA)
    if (auto* proxy = m_process->userMediaCaptureManagerProxy())
        proxy->setOrientation(orientation);

    auto* gpuProcess = m_process->processPool().gpuProcess();
    if (gpuProcess && preferences().captureVideoInGPUProcessEnabled())
        gpuProcess->setOrientationForMediaCapture(orientation);
#elif USE(GSTREAMER)
    send(Messages::WebPage::SetOrientationForMediaCapture(orientation));
#endif
#endif
}

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
void WebPageProxy::setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted)
{
    send(Messages::WebPage::SetMockCaptureDevicesInterrupted(isCameraInterrupted, isMicrophoneInterrupted));
}
#endif

#if ENABLE(TRACKING_PREVENTION)
void WebPageProxy::getLoadedSubresourceDomains(CompletionHandler<void(Vector<RegistrableDomain>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetLoadedSubresourceDomains(), WTFMove(completionHandler));
}

void WebPageProxy::clearLoadedSubresourceDomains()
{
    send(Messages::WebPage::ClearLoadedSubresourceDomains());
}
#endif

#if ENABLE(GPU_PROCESS)
void WebPageProxy::gpuProcessDidFinishLaunching()
{
    pageClient().gpuProcessDidFinishLaunching();
}

void WebPageProxy::gpuProcessExited(ProcessTerminationReason)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInGPUProcess = 0;
#endif

    pageClient().gpuProcessDidExit();

#if ENABLE(MEDIA_STREAM)
    bool activeAudioCapture = isCapturingAudio() && preferences().captureAudioInGPUProcessEnabled();
    bool activeVideoCapture = isCapturingVideo() && preferences().captureVideoInGPUProcessEnabled();
    bool activeDisplayCapture = false;
    if (activeAudioCapture || activeVideoCapture) {
        auto& gpuProcess = process().processPool().ensureGPUProcess();
        gpuProcess.updateCaptureAccess(activeAudioCapture, activeVideoCapture, activeDisplayCapture, m_process->coreProcessIdentifier(), [] { });
#if PLATFORM(IOS_FAMILY)
        gpuProcess.setOrientationForMediaCapture(m_deviceOrientation);
#endif
    }
#endif
}
#endif

#if ENABLE(CONTEXT_MENUS) && !PLATFORM(MAC)

void WebPageProxy::platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData&)
{
}

#endif

#if !PLATFORM(COCOA)

void WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory)
{
}

#endif

void WebPageProxy::dispatchActivityStateUpdateForTesting()
{
    RunLoop::current().dispatch([protectedThis = Ref { *this }] {
        protectedThis->dispatchActivityStateChange();
    });
}

void WebPageProxy::isLayerTreeFrozen(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::IsLayerTreeFrozen(), WTFMove(completionHandler));
}

void WebPageProxy::requestSpeechRecognitionPermission(WebCore::SpeechRecognitionRequest& request, CompletionHandler<void(std::optional<SpeechRecognitionError>&&)>&& completionHandler)
{
    if (!m_speechRecognitionPermissionManager)
        m_speechRecognitionPermissionManager = makeUnique<SpeechRecognitionPermissionManager>(*this);

    m_speechRecognitionPermissionManager->request(request, WTFMove(completionHandler));
}

void WebPageProxy::requestSpeechRecognitionPermissionByDefaultAction(const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_speechRecognitionPermissionManager) {
        completionHandler(false);
        return;
    }

    m_speechRecognitionPermissionManager->decideByDefaultAction(origin, WTFMove(completionHandler));
}

void WebPageProxy::requestUserMediaPermissionForSpeechRecognition(FrameIdentifier frameIdentifier, const WebCore::SecurityOrigin& requestingOrigin, const WebCore::SecurityOrigin& topOrigin, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    auto captureDevice = SpeechRecognitionCaptureSource::findCaptureDevice();
    if (!captureDevice) {
        completionHandler(false);
        return;
    }

    userMediaPermissionRequestManager().checkUserMediaPermissionForSpeechRecognition(frameIdentifier, requestingOrigin, topOrigin, *captureDevice, WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

void WebPageProxy::requestMediaKeySystemPermissionByDefaultAction(const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(true);
}

#if ENABLE(MEDIA_STREAM)

WebCore::CaptureSourceOrError WebPageProxy::createRealtimeMediaSourceForSpeechRecognition()
{
    auto captureDevice = SpeechRecognitionCaptureSource::findCaptureDevice();
    if (!captureDevice)
        return CaptureSourceOrError { "No device is available for capture"_s };

    if (preferences().captureAudioInGPUProcessEnabled())
        return CaptureSourceOrError { SpeechRecognitionRemoteRealtimeMediaSource::create(m_process->ensureSpeechRecognitionRemoteRealtimeMediaSourceManager(), *captureDevice, m_webPageID) };

#if PLATFORM(IOS_FAMILY)
    return CaptureSourceOrError { SpeechRecognitionRemoteRealtimeMediaSource::create(m_process->ensureSpeechRecognitionRemoteRealtimeMediaSourceManager(), *captureDevice, m_webPageID) };
#else
    return SpeechRecognitionCaptureSource::createRealtimeMediaSource(*captureDevice, m_webPageID);
#endif
}

#endif

#if HAVE(SCREEN_CAPTURE_KIT)
void WebPageProxy::setIndexOfGetDisplayMediaDeviceSelectedForTesting(std::optional<unsigned> index)
{
    DisplayCaptureSessionManager::singleton().setIndexOfDeviceSelectedForTesting(index);
}
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
void WebPageProxy::modelElementGetCamera(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<WebCore::HTMLModelElementCamera, ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->getCameraForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetCamera(ModelIdentifier modelIdentifier, WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->setCameraForModelElement(modelIdentifier, camera, WTFMove(completionHandler));
}

void WebPageProxy::modelElementIsPlayingAnimation(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->isPlayingAnimationForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetAnimationIsPlaying(ModelIdentifier modelIdentifier, bool isPlaying, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->setAnimationIsPlayingForModelElement(modelIdentifier, isPlaying, WTFMove(completionHandler));
}

void WebPageProxy::modelElementIsLoopingAnimation(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->isLoopingAnimationForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetIsLoopingAnimation(ModelIdentifier modelIdentifier, bool isLooping, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->setIsLoopingAnimationForModelElement(modelIdentifier, isLooping, WTFMove(completionHandler));
}

void WebPageProxy::modelElementAnimationDuration(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->animationDurationForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementAnimationCurrentTime(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->animationCurrentTimeForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetAnimationCurrentTime(ModelIdentifier modelIdentifier, Seconds currentTime, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->setAnimationCurrentTimeForModelElement(modelIdentifier, currentTime, WTFMove(completionHandler));
}

void WebPageProxy::modelElementHasAudio(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->hasAudioForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementIsMuted(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->isMutedForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetIsMuted(ModelIdentifier modelIdentifier, bool isMuted, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->setIsMutedForModelElement(modelIdentifier, isMuted, WTFMove(completionHandler));
}
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
void WebPageProxy::takeModelElementFullscreen(ModelIdentifier modelIdentifier)
{
    if (m_modelElementController)
        m_modelElementController->takeModelElementFullscreen(modelIdentifier, URL { currentURL() });
}

void WebPageProxy::modelElementSetInteractionEnabled(ModelIdentifier modelIdentifier, bool isInteractionEnabled)
{
    if (m_modelElementController)
        m_modelElementController->setInteractionEnabledForModelElement(modelIdentifier, isInteractionEnabled);
}

void WebPageProxy::modelInlinePreviewDidLoad(WebCore::GraphicsLayer::PlatformLayerID layerID)
{
    send(Messages::WebPage::ModelInlinePreviewDidLoad(layerID));
}

void WebPageProxy::modelInlinePreviewDidFailToLoad(WebCore::GraphicsLayer::PlatformLayerID layerID, const WebCore::ResourceError& error)
{
    send(Messages::WebPage::ModelInlinePreviewDidFailToLoad(layerID, error));
}

#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
void WebPageProxy::modelElementCreateRemotePreview(const String& uuid, const FloatSize& size, CompletionHandler<void(Expected<std::pair<String, uint32_t>, ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->modelElementCreateRemotePreview(uuid, size, WTFMove(completionHandler));
}

void WebPageProxy::modelElementLoadRemotePreview(const String& uuid, const URL& url, CompletionHandler<void(std::optional<WebCore::ResourceError>&&)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->modelElementLoadRemotePreview(uuid, url, WTFMove(completionHandler));
}

void WebPageProxy::modelElementDestroyRemotePreview(const String& uuid)
{
    if (m_modelElementController)
        m_modelElementController->modelElementDestroyRemotePreview(uuid);
}

void WebPageProxy::modelElementSizeDidChange(const String& uuid, WebCore::FloatSize size, CompletionHandler<void(Expected<MachSendRight, WebCore::ResourceError>)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->modelElementSizeDidChange(uuid, size, WTFMove(completionHandler));
}

void WebPageProxy::handleMouseDownForModelElement(const String& uuid, const WebCore::LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (m_modelElementController)
        m_modelElementController->handleMouseDownForModelElement(uuid, flippedLocationInElement, timestamp);
}

void WebPageProxy::handleMouseMoveForModelElement(const String& uuid, const WebCore::LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (m_modelElementController)
        m_modelElementController->handleMouseMoveForModelElement(uuid, flippedLocationInElement, timestamp);
}

void WebPageProxy::handleMouseUpForModelElement(const String& uuid, const WebCore::LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (m_modelElementController)
        m_modelElementController->handleMouseUpForModelElement(uuid, flippedLocationInElement, timestamp);
}

void WebPageProxy::modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    if (m_modelElementController)
        m_modelElementController->inlinePreviewUUIDs(WTFMove(completionHandler));
}
#endif

#if !PLATFORM(COCOA) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
Vector<SandboxExtension::Handle> WebPageProxy::createNetworkExtensionsSandboxExtensions(WebProcessProxy& process)
{
    return { };
}

void WebPageProxy::classifyModalContainerControls(Vector<String>&&, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&& completion)
{
    completion({ });
}
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void WebPageProxy::createMediaSessionCoordinator(Ref<MediaSessionCoordinatorProxyPrivate>&& privateCoordinator, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::CreateMediaSessionCoordinator(privateCoordinator->identifier()), [weakThis = WeakPtr { *this }, privateCoordinator = WTFMove(privateCoordinator), completionHandler = WTFMove(completionHandler)](bool success) mutable {

        if (!weakThis || !success) {
            completionHandler(false);
            return;
        }

        weakThis->m_mediaSessionCoordinatorProxy = RemoteMediaSessionCoordinatorProxy::create(*weakThis, WTFMove(privateCoordinator));
        completionHandler(true);
    });
}
#endif

void WebPageProxy::requestScrollToRect(const FloatRect& targetRect, const FloatPoint& origin)
{
    pageClient().requestScrollToRect(targetRect, origin);
}

void WebPageProxy::scrollToRect(const FloatRect& targetRect, const FloatPoint& origin)
{
    send(Messages::WebPage::ScrollToRect(targetRect, origin));
}

bool WebPageProxy::shouldEnableLockdownMode() const
{
    return m_configuration->lockdownModeEnabled();
}

#if PLATFORM(COCOA)
void WebPageProxy::appPrivacyReportTestingData(CompletionHandler<void(const AppPrivacyReportTestingData&)>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::AppPrivacyReportTestingData(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::clearAppPrivacyReportTestingData(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::ClearAppPrivacyReportTestingData(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}
#endif

void WebPageProxy::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    m_uiClient->requestCookieConsent(WTFMove(completion));
}

void WebPageProxy::decidePolicyForModalContainer(OptionSet<ModalContainerControlType> types, CompletionHandler<void(ModalContainerDecision)>&& completion)
{
    m_uiClient->decidePolicyForModalContainer(types, WTFMove(completion));
}

void WebPageProxy::beginTextRecognitionForVideoInElementFullScreen(MediaPlayerIdentifier identifier, FloatRect bounds)
{
    if (!pageClient().isTextRecognitionInFullscreenVideoEnabled())
        return;

#if ENABLE(GPU_PROCESS)
    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess)
        return;

    m_isPerformingTextRecognitionInElementFullScreen = true;
    gpuProcess->requestBitmapImageForCurrentTime(m_process->coreProcessIdentifier(), identifier, [weakThis = WeakPtr { *this }, bounds](auto& bitmapHandle) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_isPerformingTextRecognitionInElementFullScreen)
            return;

        protectedThis->pageClient().beginTextRecognitionForVideoInElementFullscreen(bitmapHandle, bounds);
        protectedThis->m_isPerformingTextRecognitionInElementFullScreen = false;
    });
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(bounds);
#endif
}

void WebPageProxy::cancelTextRecognitionForVideoInElementFullScreen()
{
    if (!pageClient().isTextRecognitionInFullscreenVideoEnabled())
        return;

    m_isPerformingTextRecognitionInElementFullScreen = false;
    pageClient().cancelTextRecognitionForVideoInElementFullscreen();
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPageProxy::shouldAllowRemoveBackground(const ElementContext& context, CompletionHandler<void(bool)>&& completion)
{
    sendWithAsyncReply(Messages::WebPage::ShouldAllowRemoveBackground(context), WTFMove(completion));
}

#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

void WebPageProxy::setIsWindowResizingEnabled(bool hasResizableWindows)
{
    send(Messages::WebPage::SetIsWindowResizingEnabled(hasResizableWindows));
}

#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

void WebPageProxy::setInteractionRegionsEnabled(bool enable)
{
    send(Messages::WebPage::SetInteractionRegionsEnabled(enable));
}

#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

bool WebPageProxy::shouldAvoidSynchronouslyWaitingToPreventDeadlock() const
{
    if (m_isRunningModalJavaScriptDialog)
        return true;

#if ENABLE(GPU_PROCESS)
    if (m_preferences->useGPUProcessForDOMRenderingEnabled()) {
        auto* gpuProcess = GPUProcessProxy::singletonIfCreated();
        if (!gpuProcess || !gpuProcess->hasConnection()) {
            // It's possible that the GPU process hasn't been initialized yet; in this case, we might end up in a deadlock
            // if a message comes in from the web process to initialize the GPU process while we're synchronously waiting.
            return true;
        }
    }
#endif // ENABLE(GPU_PROCESS)

    return false;
}

void WebPageProxy::generateTestReport(const String& message, const String& group)
{
    send(Messages::WebPage::GenerateTestReport(message, group));
}

} // namespace WebKit

#undef WEBPAGEPROXY_RELEASE_LOG
#undef WEBPAGEPROXY_RELEASE_LOG_ERROR
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
