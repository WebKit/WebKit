/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "APIDiagnosticLoggingClient.h"
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
#include "APINumber.h"
#include "APIOpenPanelParameters.h"
#include "APIPageConfiguration.h"
#include "APIPolicyClient.h"
#include "APIResourceLoadClient.h"
#include "APISecurityOrigin.h"
#include "APISerializedScriptValue.h"
#include "APIUIClient.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "APIWebsitePolicies.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "AuthenticationManager.h"
#include "AuthenticatorManager.h"
#include "BrowsingContextGroup.h"
#include "Connection.h"
#include "DownloadManager.h"
#include "DownloadProxy.h"
#include "DragControllerAction.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxy.h"
#include "DrawingAreaProxyMessages.h"
#include "EventDispatcherMessages.h"
#include "FindStringCallbackAggregator.h"
#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "FrameTreeCreationParameters.h"
#include "FrameTreeNodeData.h"
#include "GoToBackForwardItemParameters.h"
#include "LegacyGlobalSettings.h"
#include "LoadParameters.h"
#include "LoadedWebArchive.h"
#include "LocalFrameCreationParameters.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "MediaKeySystemPermissionRequestManagerProxy.h"
#include "MessageSenderInlines.h"
#include "ModelElementController.h"
#include "ModelProcessProxy.h"
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
#include "PlatformXRSystem.h"
#include "PolicyDecision.h"
#include "PrintInfo.h"
#include "ProcessThrottler.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "RestrictedOpenerType.h"
#include "SafeBrowsingWarning.h"
#include "SpeechRecognitionPermissionManager.h"
#include "SpeechRecognitionRemoteRealtimeMediaSource.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManager.h"
#include "SuspendedPageProxy.h"
#include "SyntheticEditingCommandType.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "TextRecognitionUpdateResult.h"
#include "URLSchemeTaskParameters.h"
#include "UndoOrRedo.h"
#include "UserMediaPermissionRequestProxy.h"
#include "UserMediaProcessManager.h"
#include "ViewGestureController.h"
#include "ViewWindowCoordinates.h"
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
#include "WebErrors.h"
#include "WebEventConversion.h"
#include "WebEventType.h"
#include "WebFoundTextRange.h"
#include "WebFrame.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFrameProxy.h"
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
#include "WebPageInjectedBundleClient.h"
#include "WebPageInspectorController.h"
#include "WebPageMessages.h"
#include "WebPageNetworkParameters.h"
#include "WebPageProxyInternals.h"
#include "WebPageProxyMessages.h"
#include "WebPasteboardProxy.h"
#include "WebPopupItem.h"
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
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/AlternativeTextClient.h>
#include <WebCore/AppHighlight.h>
#include <WebCore/ArchiveError.h>
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
#include <WebCore/LinkDecorationFilteringData.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaDeviceHashSalts.h>
#include <WebCore/MediaStreamRequest.h>
#include <WebCore/ModalContainerTypes.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/OrganizationStorageAccessPromptQuirk.h>
#include <WebCore/PerformanceLoggingClient.h>
#include <WebCore/PermissionDescriptor.h>
#include <WebCore/PermissionState.h>
#include <WebCore/PlatformEvent.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/Quirks.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/RunJavaScriptParameters.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/ShareData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <WebCore/SleepDisabler.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextExtractionTypes.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/ValidationBubble.h>
#include <WebCore/WindowFeatures.h>
#include <WebCore/WritingDirection.h>
#include <stdio.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/EnumTraits.h>
#include <wtf/ListHashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/NumberOfCores.h>
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
#include "NetworkIssueReporter.h"
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteLayerTreeScrollingPerformanceData.h"
#include "UserMediaCaptureManagerProxy.h"
#include "VideoPresentationManagerProxy.h"
#include "VideoPresentationManagerProxyMessages.h"
#include "WebPrivacyHelpers.h"
#include <WebCore/AttributedString.h>
#include <WebCore/CoreAudioCaptureDeviceManager.h>
#include <WebCore/LegacyWebArchive.h>
#include <WebCore/RunLoopObserver.h>
#include <WebCore/SystemBattery.h>
#include <objc/runtime.h>
#include <wtf/MachSendRight.h>
#include <wtf/cocoa/Entitlements.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if PLATFORM(MAC)
#include "DisplayLink.h"
#include <WebCore/ImageUtilities.h>
#include <WebCore/UTIUtilities.h>
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK)
#include "ViewSnapshotStore.h"
#endif

#if PLATFORM(GTK)
#if USE(GBM)
#include "AcceleratedBackingStoreDMABuf.h"
#endif
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

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
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

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionController.h"
#endif

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())
#define MESSAGE_CHECK_URL(process, url) MESSAGE_CHECK_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), process->connection())
#define MESSAGE_CHECK_COMPLETION(process, assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, process->connection(), completion)

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, this, internals().identifier.toUInt64(), internals().webPageID.toUInt64(), m_process->processID(), ##__VA_ARGS__)
#define WEBPAGEPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] WebPageProxy::" fmt, this, internals().identifier.toUInt64(), internals().webPageID.toUInt64(), m_process->processID(), ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

static constexpr Seconds resetRecentCrashCountDelay = 30_s;
static constexpr unsigned maximumWebProcessRelaunchAttempts = 1;
static constexpr Seconds tryCloseTimeoutDelay = 50_ms;

#if USE(RUNNINGBOARD)
static constexpr Seconds audibleActivityClearDelay = 10_s;
#endif

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageProxyCounter, ("WebPageProxy"));

#if PLATFORM(COCOA)
static WorkQueue& sharedFileQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("com.apple.WebKit.WebPageSharedFileQueue"));
    return queue.get();
}
#endif

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

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

class WebPageProxyFrameLoadStateObserver final : public FrameLoadState::Observer {
    WTF_MAKE_NONCOPYABLE(WebPageProxyFrameLoadStateObserver);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr size_t maxVisitedDomainsSize = 6;

    WebPageProxyFrameLoadStateObserver() = default;
    virtual ~WebPageProxyFrameLoadStateObserver() = default;

    void didReceiveProvisionalURL(const URL& url) override
    {
        m_provisionalURLs.append(url);
    }

    void didCancelProvisionalLoad() override
    {
        m_provisionalURLs.clear();
    }

    void didCommitProvisionalLoad() override
    {
        for (auto& url : m_provisionalURLs)
            didVisitDomain(RegistrableDomain(url));
    }

    const ListHashSet<RegistrableDomain>& visitedDomains() const
    {
        return m_visitedDomains;
    }

private:
    void didVisitDomain(RegistrableDomain&& domain)
    {
        if (domain.isEmpty())
            return;

        m_visitedDomains.prependOrMoveToFirst(WTFMove(domain));

        if (m_visitedDomains.size() > maxVisitedDomainsSize)
            m_visitedDomains.removeLast();
    }

    Vector<URL> m_provisionalURLs;
    ListHashSet<RegistrableDomain> m_visitedDomains;
};

#endif // #if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

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

WebPageProxy::ProcessActivityState::ProcessActivityState(WebPageProxy& page)
    : m_page(page)
#if PLATFORM(MAC)
    , m_wasRecentlyVisibleActivity(makeUniqueRef<ProcessThrottlerTimedActivity>(8_min))
#endif
{
}

void WebPageProxy::ProcessActivityState::takeVisibleActivity()
{
    m_isVisibleActivity = m_page.process().throttler().foregroundActivity("View is visible"_s).moveToUniquePtr();
#if PLATFORM(MAC)
    *m_wasRecentlyVisibleActivity = nullptr;
#endif
}
void WebPageProxy::ProcessActivityState::takeAudibleActivity()
{
    m_isAudibleActivity = m_page.process().throttler().foregroundActivity("View is playing audio"_s).moveToUniquePtr();
}
void WebPageProxy::ProcessActivityState::takeCapturingActivity()
{
    m_isCapturingActivity = m_page.process().throttler().foregroundActivity("View is capturing media"_s).moveToUniquePtr();
}

void WebPageProxy::ProcessActivityState::reset()
{
    m_isVisibleActivity = nullptr;
#if PLATFORM(MAC)
    *m_wasRecentlyVisibleActivity = nullptr;
#endif
    m_isAudibleActivity = nullptr;
    m_isCapturingActivity = nullptr;
#if PLATFORM(IOS_FAMILY)
    m_openingAppLinkActivity = nullptr;
#endif
}

void WebPageProxy::ProcessActivityState::dropVisibleActivity()
{
#if PLATFORM(MAC)
    if (WTF::numberOfProcessorCores() > 4)
        *m_wasRecentlyVisibleActivity = m_page.process().throttler().backgroundActivity("View was recently visible"_s);
    else
        *m_wasRecentlyVisibleActivity = m_page.process().throttler().foregroundActivity("View was recently visible"_s);
#endif
    m_isVisibleActivity = nullptr;
}

void WebPageProxy::ProcessActivityState::dropAudibleActivity()
{
    m_isAudibleActivity = nullptr;
}

void WebPageProxy::ProcessActivityState::dropCapturingActivity()
{
    m_isCapturingActivity = nullptr;
}

bool WebPageProxy::ProcessActivityState::hasValidVisibleActivity() const
{
    return m_isVisibleActivity && m_isVisibleActivity->isValid();
}

bool WebPageProxy::ProcessActivityState::hasValidAudibleActivity() const
{
    return m_isAudibleActivity && m_isAudibleActivity->isValid();
}

bool WebPageProxy::ProcessActivityState::hasValidCapturingActivity() const
{
    return m_isCapturingActivity && m_isCapturingActivity->isValid();
}

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::ProcessActivityState::takeOpeningAppLinkActivity()
{
    m_openingAppLinkActivity = m_page.process().throttler().backgroundActivity("Opening AppLink"_s).moveToUniquePtr();
}

void WebPageProxy::ProcessActivityState::dropOpeningAppLinkActivity()
{
    m_openingAppLinkActivity = nullptr;
}

bool WebPageProxy::ProcessActivityState::hasValidOpeningAppLinkActivity() const
{
    return m_openingAppLinkActivity && m_openingAppLinkActivity->isValid();
}
#endif

WebPageProxy::Internals::Internals(WebPageProxy& page)
    : page(page)
    , audibleActivityTimer(RunLoop::main(), &page, &WebPageProxy::clearAudibleActivity)
    , geolocationPermissionRequestManager(page)
    , identifier(Identifier::generate())
    , notificationManagerMessageHandler(page)
    , pageLoadState(page)
    , resetRecentCrashCountTimer(RunLoop::main(), &page, &WebPageProxy::resetRecentCrashCount)
    , tryCloseTimeoutTimer(RunLoop::main(), &page, &WebPageProxy::tryCloseTimedOut)
    , updateReportedMediaCaptureStateTimer(RunLoop::main(), &page, &WebPageProxy::updateReportedMediaCaptureState)
    , webPageID(PageIdentifier::generate())
#if HAVE(DISPLAY_LINK)
    , wheelEventActivityHysteresis([&page](PAL::HysteresisState state) { page.wheelEventHysteresisUpdated(state); })
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    , fullscreenVideoTextRecognitionTimer(RunLoop::main(), &page, &WebPageProxy::fullscreenVideoTextRecognitionTimerFired)
#endif
{
}

WebPageProxy::WebPageProxy(PageClient& pageClient, WebProcessProxy& process, Ref<API::PageConfiguration>&& configuration)
    : m_internals(makeUniqueRef<Internals>(*this))
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
    , m_processActivityState(*this)
    , m_initialCapitalizationEnabled(m_configuration->initialCapitalizationEnabled())
    , m_cpuLimit(m_configuration->cpuLimit())
    , m_backForwardList(WebBackForwardList::create(*this))
    , m_waitsForPaintAfterViewDidMoveToWindow(m_configuration->waitsForPaintAfterViewDidMoveToWindow())
    , m_pluginMinZoomFactor(ViewGestureController::defaultMinMagnification)
    , m_pluginMaxZoomFactor(ViewGestureController::defaultMaxMagnification)
    , m_hasRunningProcess(process.state() != WebProcessProxy::State::Terminated)
    , m_controlledByAutomation(m_configuration->isControlledByAutomation())
#if PLATFORM(COCOA)
    , m_isSmartInsertDeleteEnabled(TextChecker::isSmartInsertDeleteEnabled())
#endif
    , m_inspectorController(makeUnique<WebPageInspectorController>(*this))
#if ENABLE(REMOTE_INSPECTOR)
    , m_inspectorDebuggable(makeUnique<WebPageDebuggable>(*this))
#endif
    , m_corsDisablingPatterns(m_configuration->corsDisablingPatterns())
#if ENABLE(APP_BOUND_DOMAINS)
    , m_ignoresAppBoundDomains(m_configuration->ignoresAppBoundDomains())
    , m_limitsNavigationsToAppBoundDomains(m_configuration->limitsNavigationsToAppBoundDomains())
#endif
    , m_browsingContextGroup(m_configuration->browsingContextGroup())
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "constructor:");

    if (!m_configuration->drawsBackground())
        internals().backgroundColor = Color(Color::transparentBlack);

    updateActivityState();
    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();
    
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    internals().layerHostingMode = isInWindow() ? WebPageProxy::protectedPageClient()->viewLayerHostingMode() : LayerHostingMode::OutOfProcess;
#endif

    platformInitialize();

#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif

    WebProcessPool::statistics().wkPageCount++;

    protectedPreferences()->addPage(*this);
    protectedPageGroup()->addPage(*this);

#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->addPage(*this);
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
    m_activityStateChangeDispatcher = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::ActivityStateChange, [this] {
        Ref { *this }->dispatchActivityStateChange();
    });
#endif

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->setInspectable(JSRemoteInspectorGetInspectionEnabledByDefault());
    m_inspectorDebuggable->setPresentingApplicationPID(m_process->processPool().configuration().presentingApplicationPID());
    m_inspectorDebuggable->init();
#endif
    m_inspectorController->init();

#if ENABLE(IPC_TESTING_API)
    if (m_preferences->ipcTestingAPIEnabled() && m_preferences->ignoreInvalidMessageWhenIPCTestingAPIEnabled())
        process.setIgnoreInvalidMessageForTesting();
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (m_preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::sharedNotifier().addWebPage(*this);
#endif

    m_pageToCloneSessionStorageFrom = m_configuration->pageToCloneSessionStorageFrom();

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    m_linkDecorationFilteringDataUpdateObserver = LinkDecorationFilteringController::shared().observeUpdates([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendCachedLinkDecorationFilteringData();
    });
#endif
}

WebPageProxy::~WebPageProxy()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "destructor:");

    ASSERT(m_process->webPage(internals().identifier) != this);
#if ASSERT_ENABLED
    for (Ref page : m_process->pages())
        ASSERT(page.ptr() != this);
#endif

    setPageLoadStateObserver(nullptr);

    if (!m_isClosed)
        close();

    WebProcessPool::statistics().wkPageCount--;

    if (m_spellDocumentTag)
        TextChecker::closeSpellDocumentWithTag(m_spellDocumentTag.value());

    Ref preferences = this->preferences();
    preferences->removePage(*this);
    protectedPageGroup()->removePage(*this);

#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif

#if PLATFORM(MACCATALYST)
    EndowmentStateTracker::singleton().removeClient(internals());
#endif
    
    for (auto& callback : m_nextActivityStateChangeCallbacks)
        callback();

    if (RefPtr networkProcess = websiteDataStore().networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::RemoveWebPageNetworkParameters(sessionID(), internals().identifier), 0);

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::sharedNotifier().removeWebPage(*this);
#endif
}

void WebPageProxy::addAllMessageReceivers()
{
    Ref process = m_process;
    internals().messageReceiverRegistration.startReceivingMessages(process, internals().webPageID, *this);
    process->addMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), internals().webPageID, internals().notificationManagerMessageHandler);
}

void WebPageProxy::removeAllMessageReceivers()
{
    internals().messageReceiverRegistration.stopReceivingMessages();
    protectedProcess()->removeMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), internals().webPageID);
}

WebPageProxyMessageReceiverRegistration& WebPageProxy::messageReceiverRegistration()
{
    return internals().messageReceiverRegistration;
}

bool WebPageProxy::attachmentElementEnabled()
{
    return preferences().attachmentElementEnabled();
}

bool WebPageProxy::modelElementEnabled()
{
    return preferences().modelElementEnabled();
}

#if ENABLE(WK_WEB_EXTENSIONS)
WebExtensionController* WebPageProxy::webExtensionController()
{
    return m_webExtensionController.get() ?: m_weakWebExtensionController.get();
}
#endif

// FIXME: Should return a const PageClient& and add a separate non-const
// version of this function, but several PageClient methods will need to become
// const for this to be possible.
PageClient& WebPageProxy::pageClient() const
{
    ASSERT(m_pageClient);
    return *m_pageClient;
}

Ref<PageClient> WebPageProxy::protectedPageClient() const
{
    return pageClient();
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

Ref<API::PageConfiguration> WebPageProxy::protectedConfiguration() const
{
    return m_configuration;
}


ProcessID WebPageProxy::gpuProcessID() const
{
    if (m_isClosed)
        return 0;

#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcess = process().processPool().gpuProcess())
        return gpuProcess->processID();
#endif

    return 0;
}

Ref<WebProcessProxy> WebPageProxy::protectedProcess() const
{
    return m_process;
}

ProcessID WebPageProxy::modelProcessID() const
{
    if (m_isClosed)
        return 0;

#if ENABLE(MODEL_PROCESS)
    if (auto* modelProcess = process().processPool().modelProcess())
        return modelProcess->processID();
#endif

    return 0;
}

ProcessID WebPageProxy::processID() const
{
    if (m_isClosed)
        return 0;

    return m_process->processID();
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
    Ref processPool = m_process->processPool();
    if (processPool->hasPrewarmedProcess())
        return;
    processPool->didReachGoodTimeToPrewarm();
}

void WebPageProxy::setPreferences(WebPreferences& preferences)
{
    if (&preferences == m_preferences.ptr())
        return;

    protectedPreferences()->removePage(*this);
    m_preferences = preferences;
    protectedPreferences()->addPage(*this);

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
    handleMessageShared(m_process, messageName, messageBody);
}

void WebPageProxy::handleMessageShared(const Ref<WebProcessProxy>& process, const String& messageName, const WebKit::UserData& messageBody)
{
    if (!m_injectedBundleClient)
        return;

    m_injectedBundleClient->didReceiveMessageFromInjectedBundle(this, messageName, process->transformHandlesToObjects(messageBody.protectedObject().get()).get());
}

void WebPageProxy::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&& completionHandler)
{
    ASSERT(m_process->connection() == &connection);

    if (!m_injectedBundleClient)
        return completionHandler({ });

    RefPtr<API::Object> returnData;
    Ref process = m_process;
    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(this, messageName, process->transformHandlesToObjects(messageBody.protectedObject().get()).get(), [completionHandler = WTFMove(completionHandler), process] (RefPtr<API::Object>&& returnData) mutable {
        completionHandler(UserData(process->transformObjectsToHandles(returnData.get())));
    });
}

bool WebPageProxy::hasSameGPUProcessPreferencesAs(const API::PageConfiguration& configuration) const
{
#if ENABLE(GPU_PROCESS)
    return preferencesForGPUProcess() == configuration.preferencesForGPUProcess();
#else
    UNUSED_PARAM(configuration);
    return true;
#endif
}

void WebPageProxy::launchProcess(const RegistrableDomain& registrableDomain, ProcessLaunchReason reason)
{
    ASSERT(!m_isClosed);
    ASSERT(!hasRunningProcess());

    WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcess:");

    // In case we are currently connected to the dummy process, we need to make sure the inspector proxy
    // disconnects from the dummy process first.
    protectedInspector()->reset();

    protectedProcess()->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();

    Ref processPool = m_process->processPool();

    RefPtr relatedPage = m_configuration->relatedPage();
    if (relatedPage && !relatedPage->isClosed() && reason == ProcessLaunchReason::InitialProcess && hasSameGPUProcessPreferencesAs(*relatedPage)) {
        m_process = relatedPage->ensureRunningProcess();
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcess: Using process (process=%p, PID=%i) from related page", m_process.ptr(), m_process->processID());
    } else
        m_process = processPool->processForRegistrableDomain(protectedWebsiteDataStore(), registrableDomain, shouldEnableLockdownMode() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled, protectedConfiguration());

    m_hasRunningProcess = true;
    m_shouldReloadDueToCrashWhenVisible = false;
    m_isLockdownModeExplicitlySet = m_configuration->isLockdownModeExplicitlySet();

    Ref process = m_process;
    process->addExistingWebPage(*this, WebProcessProxy::BeginsUsingDataStore::Yes);
    addAllMessageReceivers();

#if ENABLE(IPC_TESTING_API)
    if (m_preferences->store().getBoolValueForKey(WebPreferencesKey::ipcTestingAPIEnabledKey()) && m_preferences->store().getBoolValueForKey(WebPreferencesKey::ignoreInvalidMessageWhenIPCTestingAPIEnabledKey()))
        process->setIgnoreInvalidMessageForTesting();
#endif
    
    if (m_configuration->allowTestOnlyIPC())
        process->setAllowTestOnlyIPC(true);

    finishAttachingToWebProcess(reason);

    auto pendingInjectedBundleMessage = WTFMove(m_pendingInjectedBundleMessages);
    for (auto& message : pendingInjectedBundleMessage)
        send(Messages::WebPage::PostInjectedBundleMessage(message.messageName, UserData(process->transformObjectsToHandles(message.messageBody.get()).get())));
}

bool WebPageProxy::suspendCurrentPageIfPossible(API::Navigation& navigation, RefPtr<WebFrameProxy>&& mainFrame, ShouldDelayClosingUntilFirstLayerFlush shouldDelayClosingUntilFirstLayerFlush)
{
    m_suspendedPageKeptToPreventFlashing = nullptr;
    m_lastSuspendedPage = nullptr;

    if (!mainFrame)
        return false;

    if (openerFrame() && (preferences().processSwapOnCrossSiteWindowOpenEnabled() || preferences().siteIsolationEnabled())) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because it has an opener.", m_process->processID());
        return false;
    }

    if (!hasCommittedAnyProvisionalLoads()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because has not committed any load yet", m_process->processID());
        return false;
    }

    if (isPageOpenedByDOMShowingInitialEmptyDocument()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because it is showing the initial empty document", m_process->processID());
        return false;
    }

    RefPtr fromItem = navigation.fromItem();

    // If the source and the destination back / forward list items are the same, then this is a client-side redirect. In this case,
    // there is no need to suspend the previous page as there will be no way to get back to it.
    if (fromItem && fromItem == m_backForwardList->currentItem()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because this is a client-side redirect", m_process->processID());
        return false;
    }

    if (fromItem && fromItem->url() != pageLoadState().url()) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because fromItem's URL does not match the page URL.", m_process->processID());
        return false;
    }

    bool needsSuspendedPageToPreventFlashing = shouldDelayClosingUntilFirstLayerFlush == ShouldDelayClosingUntilFirstLayerFlush::Yes;
    if (!needsSuspendedPageToPreventFlashing && (!fromItem || !shouldUseBackForwardCache())) {
        if (!fromItem)
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i there is no associated WebBackForwardListItem", m_process->processID());
        else
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i the back / forward cache is disabled", m_process->processID());
        return false;
    }

    WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Suspending current page for process pid %i", m_process->processID());
    mainFrame->frameLoadState().didSuspend();
    auto suspendedPage = makeUnique<SuspendedPageProxy>(*this, protectedProcess(), mainFrame.releaseNonNull(), std::exchange(m_browsingContextGroup, BrowsingContextGroup::create()), std::exchange(internals().remotePageProxyState, { }), shouldDelayClosingUntilFirstLayerFlush);

    LOG(ProcessSwapping, "WebPageProxy %" PRIu64 " created suspended page %s for process pid %i, back/forward item %s" PRIu64, identifier().toUInt64(), suspendedPage->loggingString(), m_process->processID(), fromItem ? fromItem->itemID().toString().utf8().data() : "0"_s);

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
    return m_preferences->usesBackForwardCache()
        && backForwardCache().capacity() > 0
        && !m_preferences->siteIsolationEnabled();
}

void WebPageProxy::swapToProvisionalPage(std::unique_ptr<ProvisionalPageProxy> provisionalPage)
{
    ASSERT(!m_isClosed);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "swapToProvisionalPage: newWebPageID=%" PRIu64, provisionalPage->webPageID().toUInt64());

    for (auto& openedPage : internals().m_openedPages)
        openedPage.internals().remotePageProxyState.remotePageProxyInOpenerProcess = nullptr;

    m_process = provisionalPage->process();
    internals().webPageID = provisionalPage->webPageID();
    protectedPageClient()->didChangeWebPageID();
    ASSERT(m_process->websiteDataStore());
    m_websiteDataStore = *m_process->websiteDataStore();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInWebProcess = provisionalPage->contextIDForVisibilityPropagationInWebProcess();
#if ENABLE(GPU_PROCESS)
    m_contextIDForVisibilityPropagationInGPUProcess = provisionalPage->contextIDForVisibilityPropagationInGPUProcess();
#endif
#endif

    // FIXME: Do we really need to disable this logging in ephemeral sessions?
    if (RefPtr logger = m_logger)
        logger->setEnabled(this, !sessionID().isEphemeral());

    m_hasRunningProcess = true;

    ASSERT(!m_drawingArea);
    setDrawingArea(provisionalPage->takeDrawingArea());
    ASSERT(!m_mainFrame);
    m_mainFrame = provisionalPage->mainFrame();
    // FIXME: Think about what to do if the provisional page didn't get its browsing context group from the SuspendedPageProxy.
    // We do need to clear it at some point for navigations that aren't from back/forward navigations. Probably in the same place as PSON?
    if (RefPtr browsingContextGroup = provisionalPage->browsingContextGroup()) {
        m_browsingContextGroup = browsingContextGroup.releaseNonNull();
        internals().remotePageProxyState = provisionalPage->takeRemotePageProxyState();
    }

    protectedProcess()->addExistingWebPage(*this, WebProcessProxy::BeginsUsingDataStore::No);
    addAllMessageReceivers();

    finishAttachingToWebProcess(ProcessLaunchReason::ProcessSwap);

#if PLATFORM(IOS_FAMILY)
    // On iOS, the displayID is derived from the webPageID.
    m_displayID = generateDisplayIDFromPageID();

    std::optional<FramesPerSecond> nominalFramesPerSecond;
    if (m_drawingArea)
        nominalFramesPerSecond = m_drawingArea->displayNominalFramesPerSecond();
    // FIXME: We may want to send WindowScreenDidChange on non-iOS platforms too.
    send(Messages::WebPage::WindowScreenDidChange(*m_displayID, nominalFramesPerSecond));
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

    protectedInspector()->updateForNewPageProcess(*this);

#if ENABLE(REMOTE_INSPECTOR)
    remoteInspectorInformationDidChange();
#endif

    updateWheelEventActivityAfterProcessSwap();

    protectedPageClient()->didRelaunchProcess();
    internals().pageLoadState.didSwapWebProcesses();
}

void WebPageProxy::didAttachToRunningProcess()
{
    ASSERT(hasRunningProcess());

#if ENABLE(FULLSCREEN_API)
    ASSERT(!m_fullScreenManager);
    m_fullScreenManager = makeUnique<WebFullScreenManagerProxy>(*this, protectedPageClient()->fullScreenManagerProxyClient());
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    ASSERT(!m_playbackSessionManager);
    m_playbackSessionManager = PlaybackSessionManagerProxy::create(*this);
    ASSERT(!m_videoPresentationManager);
    m_videoPresentationManager = VideoPresentationManagerProxy::create(*this, *m_playbackSessionManager.copyRef());
    if (RefPtr videoPresentationManager = m_videoPresentationManager)
        videoPresentationManager->setMockVideoPresentationModeEnabled(m_mockVideoPresentationModeEnabled);
#endif

#if ENABLE(APPLE_PAY)
    ASSERT(!internals().paymentCoordinator);
    internals().paymentCoordinator = makeUnique<WebPaymentCoordinatorProxy>(internals());
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

#if !PLATFORM(IOS_FAMILY)
    auto currentOrientation = WebCore::naturalScreenOrientationType();
#else
    auto currentOrientation = toScreenOrientationType(m_deviceOrientation);
#endif
    m_screenOrientationManager = makeUnique<WebScreenOrientationManagerProxy>(*this, currentOrientation);

#if ENABLE(WEBXR) && !USE(OPENXR)
    ASSERT(!internals().xrSystem);
    internals().xrSystem = makeUnique<PlatformXRSystem>(*this);
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

    Ref navigation = m_navigationState->createReloadNavigation(process().coreProcessIdentifier(), m_backForwardList->protectedCurrentItem());

    String url = currentURL();
    if (!url.isEmpty()) {
        auto transaction = internals().pageLoadState.transaction();
        internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), url });
    }

    std::optional<String> topPrivatelyControlledDomain;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(URL(m_backForwardList->currentItem()->url()).host().toString());
#endif

    // We allow stale content when reloading a WebProcess that's been killed or crashed.
    send(Messages::WebPage::GoToBackForwardItem({ navigation->navigationID(), m_backForwardList->currentItem()->itemID(), FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, std::nullopt, topPrivatelyControlledDomain, { } }));
    m_process->startResponsivenessTimer();

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client reload"_s));

    return navigation;
}

void WebPageProxy::setDrawingArea(std::unique_ptr<DrawingAreaProxy>&& drawingArea)
{
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    // The scrolling coordinator needs to do cleanup before the drawing area goes away.
    m_scrollingCoordinatorProxy = nullptr;
#endif

    if (m_drawingArea)
        m_drawingArea->stopReceivingMessages(process());

    m_drawingArea = WTFMove(drawingArea);
    if (!m_drawingArea)
        return;

    m_drawingArea->startReceivingMessages(process());
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

    setDrawingArea(protectedPageClient()->createDrawingAreaProxy(m_process.copyRef()));
    ASSERT(m_drawingArea);

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton();
#endif

    if (auto& attributedBundleIdentifier = m_configuration->attributedBundleIdentifier(); !!attributedBundleIdentifier) {
        WebPageNetworkParameters parameters { attributedBundleIdentifier };
        websiteDataStore().protectedNetworkProcess()->send(Messages::NetworkProcess::AddWebPageNetworkParameters(sessionID(), internals().identifier, WTFMove(parameters)), 0);
    }

    if (RefPtr networkProcess = websiteDataStore().networkProcessIfExists()) {
        if (m_pageToCloneSessionStorageFrom)
            networkProcess->send(Messages::NetworkProcess::CloneSessionStorageForWebPage(sessionID(), m_pageToCloneSessionStorageFrom->identifier(), identifier()), 0);
    }
    m_pageToCloneSessionStorageFrom = nullptr;

    Ref process = m_process;
    send(Messages::WebProcess::CreateWebPage(internals().webPageID, creationParameters(process, *m_drawingArea)), 0);

    process->addVisitedLinkStoreUser(visitedLinkStore(), internals().identifier);

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    m_needsInitialLinkDecorationFilteringData = LinkDecorationFilteringController::shared().cachedStrings().isEmpty();
    m_shouldUpdateAllowedQueryParametersForAdvancedPrivacyProtections = cachedAllowedQueryParametersForAdvancedPrivacyProtections().isEmpty();
#endif
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
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->willClosePage(*this);
    }

#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = m_webExtensionController)
        webExtensionController->removePage(*this);
    if (RefPtr webExtensionController = m_weakWebExtensionController.get())
        webExtensionController->removePage(*this);
#endif

#if ENABLE(CONTEXT_MENUS)
    m_activeContextMenu = nullptr;
#endif

    m_provisionalPage = nullptr;

    // Do not call inspector() / protectedInspector() since they return
    // null after the page has closed.
    RefPtr { m_inspector }->invalidate();

    protectedBackForwardList()->pageClosed();
    m_inspectorController->pageClosed();
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable = nullptr;
#endif
    protectedPageClient()->pageClosed();

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

    Ref process = m_process;
    Ref processPool = process->processPool();
    processPool->backForwardCache().removeEntriesForPage(*this);

    RunLoop::current().dispatch([destinationID = messageSenderDestinationID(), process, preventProcessShutdownScope = process->shutdownPreventingScope()] {
        process->send(Messages::WebPage::Close(), destinationID);
    });
    process->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();
    processPool->supplement<WebNotificationManagerProxy>()->clearNotifications(this);

    // Null out related WebPageProxy to avoid leaks.
    protectedConfiguration()->setRelatedPage(nullptr);

    // Make sure we don't hold a process assertion after getting closed.
    m_processActivityState.reset();
    internals().audibleActivityTimer.stop();

    internals().remotePageProxyState = { };

    stopAllURLSchemeTasks();
    updatePlayingMediaDidChange(MediaProducer::IsNotPlaying);

    if (RefPtr openerPage = m_openerFrame ? m_openerFrame->page() : nullptr)
        openerPage->removeOpenedRemotePageProxy(identifier());
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

    internals().tryCloseTimeoutTimer.startOneShot(tryCloseTimeoutDelay);
    sendWithAsyncReply(Messages::WebPage::TryClose(), [this, weakThis = WeakPtr { *this }](bool shouldClose) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        // If we timed out, don't ask the client to close again.
        if (!internals().tryCloseTimeoutTimer.isActive())
            return;

        internals().tryCloseTimeoutTimer.stop();
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
    if (!url.protocolIsFile())
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

void WebPageProxy::prepareToLoadWebPage(WebProcessProxy& process, LoadParameters& parameters)
{
    addPlatformLoadParameters(process, parameters);
#if ENABLE(NETWORK_ISSUE_REPORTING)
    if (NetworkIssueReporter::isEnabled())
        m_networkIssueReporter = makeUnique<NetworkIssueReporter>();
#endif
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

    Ref navigation = m_navigationState->createLoadRequestNavigation(process().coreProcessIdentifier(), ResourceRequest(request), m_backForwardList->protectedCurrentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

#if PLATFORM(COCOA)
    setLastNavigationWasAppInitiated(request);
#endif

    loadRequestWithNavigationShared(protectedProcess(), internals().webPageID, navigation, WTFMove(request), shouldOpenExternalURLsPolicy, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain(), std::nullopt, std::nullopt);
    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(ResourceRequest&& request)
{
    return loadRequest(WTFMove(request), ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks);
}

void WebPageProxy::loadRequestWithNavigationShared(Ref<WebProcessProxy>&& process, WebCore::PageIdentifier webPageID, API::Navigation& navigation, ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, API::Object* userData, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    ASSERT(!m_isClosed);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadRequestWithNavigationShared:");

    auto transaction = internals().pageLoadState.transaction();

    auto url = request.url();
    if (shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::No)
        internals().pageLoadState.setPendingAPIRequest(transaction, { navigation.navigationID(), url.string() });

    LoadParameters loadParameters;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    auto host = url.host().toString();
    loadParameters.topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(host);
    loadParameters.host = host;
#endif
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
    loadParameters.advancedPrivacyProtections = navigation.originatorAdvancedPrivacyProtections();
    maybeInitializeSandboxExtensionHandle(process, url, internals().pageLoadState.resourceDirectoryURL(), loadParameters.sandboxExtensionHandle);

    prepareToLoadWebPage(process, loadParameters);

    if (shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::No)
        preconnectTo(ResourceRequest { loadParameters.request });

    navigation.setIsLoadedWithNavigationShared(true);

    process->markProcessAsRecentlyUsed();

    if (!process->isLaunching() || !url.protocolIsFile())
        process->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)), webPageID);
    else
        process->send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(WTFMove(loadParameters), internals().pageLoadState.resourceDirectoryURL(), internals().identifier, true), webPageID);
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

    Ref navigation = m_navigationState->createLoadRequestNavigation(process().coreProcessIdentifier(), ResourceRequest(fileURL), m_backForwardList->protectedCurrentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

    auto transaction = internals().pageLoadState.transaction();

    internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), fileURLString }, resourceDirectoryURL);

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
    Ref process = m_process;
    maybeInitializeSandboxExtensionHandle(process, fileURL, resourceDirectoryURL, loadParameters.sandboxExtensionHandle, checkAssumedReadAccessToResourceURL);
    prepareToLoadWebPage(process, loadParameters);

    process->markProcessAsRecentlyUsed();
    if (process->isLaunching())
        send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(WTFMove(loadParameters), resourceDirectoryURL, internals().identifier, checkAssumedReadAccessToResourceURL));
    else
        send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)));
    process->startResponsivenessTimer();

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadData(const IPC::DataReference& data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadData:");

#if ENABLE(APP_BOUND_DOMAINS)
    if (MIMEType == "text/html"_s && !isFullWebBrowserOrRunningTest())
        m_limitsNavigationsToAppBoundDomains = true;
#endif

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadData: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess())
        launchProcess({ }, ProcessLaunchReason::InitialProcess);

    Ref navigation = m_navigationState->createLoadDataNavigation(process().coreProcessIdentifier(), makeUnique<API::SubstituteData>(Vector(data), MIMEType, encoding, baseURL, userData));

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

    loadDataWithNavigationShared(protectedProcess(), internals().webPageID, navigation, data, MIMEType, encoding, baseURL, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain(), std::nullopt, shouldOpenExternalURLsPolicy, SubstituteData::SessionHistoryVisibility::Hidden);
    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadData(const IPC::DataReference& data, const String& type, const String& encoding, const String& baseURL, API::Object* userData)
{
    return loadData(data, type, encoding, baseURL, userData, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
}

void WebPageProxy::loadDataWithNavigationShared(Ref<WebProcessProxy>&& process, WebCore::PageIdentifier webPageID, API::Navigation& navigation, const IPC::DataReference& data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadDataWithNavigation");

    ASSERT(!m_isClosed);

    auto transaction = internals().pageLoadState.transaction();

    internals().pageLoadState.setPendingAPIRequest(transaction, { navigation.navigationID(), !baseURL.isEmpty() ? baseURL : aboutBlankURL().string() });

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
    prepareToLoadWebPage(process, loadParameters);

    process->markProcessAsRecentlyUsed();
    process->assumeReadAccessToBaseURL(*this, baseURL);
    process->send(Messages::WebPage::LoadData(WTFMove(loadParameters)), webPageID);
    process->startResponsivenessTimer();
}

RefPtr<API::Navigation> WebPageProxy::loadSimulatedRequest(WebCore::ResourceRequest&& simulatedRequest, WebCore::ResourceResponse&& simulatedResponse, const IPC::DataReference& data)
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
        launchProcess(RegistrableDomain { simulatedRequest.url() }, ProcessLaunchReason::InitialProcess);

    Ref navigation = m_navigationState->createSimulatedLoadWithDataNavigation(process().coreProcessIdentifier(), ResourceRequest(simulatedRequest), makeUnique<API::SubstituteData>(Vector(data), ResourceResponse(simulatedResponse), WebCore::SubstituteData::SessionHistoryVisibility::Visible), m_backForwardList->protectedCurrentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process().throttler().foregroundActivity("Client navigation"_s));

    auto transaction = internals().pageLoadState.transaction();

    auto baseURL = simulatedRequest.url().string();
    simulatedResponse.setURL(simulatedRequest.url()); // These should always match for simulated load

    internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), !baseURL.isEmpty() ? baseURL : aboutBlankURL().string() });

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

    Ref process = m_process;
    prepareToLoadWebPage(process, loadParameters);

    process->markProcessAsRecentlyUsed();
    process->assumeReadAccessToBaseURL(*this, baseURL);
    process->send(Messages::WebPage::LoadSimulatedRequestAndResponse(WTFMove(loadParameters), simulatedResponse), internals().webPageID);
    process->startResponsivenessTimer();

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

    auto transaction = internals().pageLoadState.transaction();

    internals().pageLoadState.setPendingAPIRequest(transaction, { 0, unreachableURL.string() });
    internals().pageLoadState.setUnreachableURL(transaction, unreachableURL.string());

    if (RefPtr mainFrame = m_mainFrame)
        mainFrame->setUnreachableURL(unreachableURL);

    LoadParameters loadParameters;
    loadParameters.navigationID = 0;
    loadParameters.MIMEType = "text/html"_s;
    loadParameters.encodingName = encoding;
    loadParameters.baseURLString = baseURL.string();
    loadParameters.unreachableURLString = unreachableURL.string();
    loadParameters.provisionalLoadErrorURLString = m_failingProvisionalLoadURL;
    Ref process = m_process;
    loadParameters.userData = UserData(process->transformObjectsToHandles(userData).get());
    prepareToLoadWebPage(process, loadParameters);

    auto continueLoad = [
        this,
        protectedThis = Ref { *this },
        process,
        loadParameters = WTFMove(loadParameters),
        baseURL,
        unreachableURL,
        htmlData = WTFMove(htmlData),
        preventProcessShutdownScope = process->shutdownPreventingScope()
    ] () mutable {
        loadParameters.data = { htmlData->data(), htmlData->size() };
        process->markProcessAsRecentlyUsed();
        process->assumeReadAccessToBaseURL(*this, baseURL.string());
        process->assumeReadAccessToBaseURL(*this, unreachableURL.string());
        if (baseURL.protocolIsFile())
            process->addPreviouslyApprovedFileURL(baseURL);
        if (unreachableURL.protocolIsFile())
            process->addPreviouslyApprovedFileURL(unreachableURL);
        send(Messages::WebPage::LoadAlternateHTML(WTFMove(loadParameters)));
        process->startResponsivenessTimer();
    };

    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(process->coreProcessIdentifier(), RegistrableDomain(baseURL), LoadedWebArchive::No), WTFMove(continueLoad));
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
    protectedProcess()->startResponsivenessTimer();
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
    protectedProcess()->startResponsivenessTimer();
}

RefPtr<API::Navigation> WebPageProxy::reload(OptionSet<WebCore::ReloadOption> options)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "reload:");

    // Make sure the Network & GPU processes are still responsive. This is so that reload() gets us out of the bad state if one of these
    // processes is hung.
    websiteDataStore().protectedNetworkProcess()->checkForResponsiveness();
#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = process().processPool().gpuProcess())
        gpuProcess->checkForResponsiveness();
#endif

    SandboxExtension::Handle sandboxExtensionHandle;

    String url = currentURL();
    if (!url.isEmpty()) {
        // We may not have an extension yet if back/forward list was reinstated after a WebProcess crash or a browser relaunch
        maybeInitializeSandboxExtensionHandle(protectedProcess(), URL { url }, currentResourceDirectoryURL(), sandboxExtensionHandle);
    }

    if (!hasRunningProcess())
        return launchProcessForReload();
    
    Ref navigation = m_navigationState->createReloadNavigation(process().coreProcessIdentifier(), m_backForwardList->protectedCurrentItem());

    if (!url.isEmpty()) {
        auto transaction = internals().pageLoadState.transaction();
        internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), url });
    }

    // Store decision to reload without content blockers on the navigation so that we can later set the corresponding
    // WebsitePolicies flag in WebPageProxy::receivedNavigationActionPolicyDecision().
    if (options.contains(WebCore::ReloadOption::DisableContentBlockers))
        navigation->setUserContentExtensionsEnabled(false);

    Ref process = m_process;
    process->markProcessAsRecentlyUsed();
    send(Messages::WebPage::Reload(navigation->navigationID(), options, WTFMove(sandboxExtensionHandle)));
    process->startResponsivenessTimer();

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(process->throttler().foregroundActivity("Client reload"_s));

#if ENABLE(SPEECH_SYNTHESIS)
    resetSpeechSynthesizer();
#endif

    return navigation;
}

void WebPageProxy::recordAutomaticNavigationSnapshot()
{
    if (m_shouldSuppressNextAutomaticNavigationSnapshot)
        return;

    if (RefPtr item = m_backForwardList->currentItem())
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
    RefPtr forwardItem = protectedBackForwardList()->goForwardItemSkippingItemsWithoutUserGesture();
    if (!forwardItem)
        return nullptr;

    return goToBackForwardItem(*forwardItem, FrameLoadType::Forward);
}

RefPtr<API::Navigation> WebPageProxy::goBack()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goBack:");
    RefPtr backItem = protectedBackForwardList()->goBackItemSkippingItemsWithoutUserGesture();
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
            protectedBackForwardList()->goToItem(item);
    }

    Ref navigation = m_navigationState->createBackForwardNavigation(process().coreProcessIdentifier(), item, m_backForwardList->protectedCurrentItem(), frameLoadType);
    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), item.url() });

    Ref process = m_process;
    process->markProcessAsRecentlyUsed();

    std::optional<String> topPrivatelyControlledDomain;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(URL(item.url()).host().toString());
#endif
    
    send(Messages::WebPage::GoToBackForwardItem({ navigation->navigationID(), item.itemID(), frameLoadType, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, std::nullopt, topPrivatelyControlledDomain, { } }));
    process->startResponsivenessTimer();

    return RefPtr<API::Navigation> { WTFMove(navigation) };
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
    Ref protectedPageClient { pageClient() };

    if (!m_navigationClient->didChangeBackForwardList(*this, added, removed) && m_loaderClient)
        m_loaderClient->didChangeBackForwardList(*this, added, WTFMove(removed));

    auto transaction = internals().pageLoadState.transaction();

    internals().pageLoadState.setCanGoBack(transaction, m_backForwardList->backItem());
    internals().pageLoadState.setCanGoForward(transaction, m_backForwardList->forwardItem());
}

void WebPageProxy::willGoToBackForwardListItem(const BackForwardItemIdentifier& itemID, bool inBackForwardCache)
{
    Ref protectedPageClient { pageClient() };

    if (RefPtr item = m_backForwardList->itemForID(itemID))
        m_navigationClient->willGoToBackForwardListItem(*this, *item, inBackForwardCache);
}

bool WebPageProxy::shouldKeepCurrentBackForwardListItemInList(WebBackForwardListItem& item)
{
    Ref protectedPageClient { pageClient() };

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
    websiteDataStore().protectedNetworkProcess()->send(Messages::NetworkProcess::SetSessionIsControlledByAutomation(m_websiteDataStore->sessionID(), m_controlledByAutomation), 0);
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
    return m_inspectorDebuggable && m_inspectorDebuggable->inspectable();
}

void WebPageProxy::setInspectable(bool inspectable)
{
    if (!m_inspectorDebuggable || m_inspectorDebuggable->inspectable() == inspectable)
        return;

    m_inspectorDebuggable->setInspectable(inspectable);

    protectedWebsiteDataStore()->updateServiceWorkerInspectability();
}

String WebPageProxy::remoteInspectionNameOverride() const
{
    return m_inspectorDebuggable ? m_inspectorDebuggable->nameOverride() : nullString();
}

void WebPageProxy::setRemoteInspectionNameOverride(const String& name)
{
    if (m_inspectorDebuggable)
        m_inspectorDebuggable->setNameOverride(name);
}

void WebPageProxy::remoteInspectorInformationDidChange()
{
    if (m_inspectorDebuggable)
        m_inspectorDebuggable->update();
}
#endif

const std::optional<Color>& WebPageProxy::backgroundColor() const
{
    return internals().backgroundColor;
}

void WebPageProxy::setBackgroundColor(const std::optional<Color>& color)
{
    if (internals().backgroundColor == color)
        return;

    internals().backgroundColor = color;
    if (hasRunningProcess())
        send(Messages::WebPage::SetBackgroundColor(color));
}

void WebPageProxy::setTopContentInset(float contentInset)
{
    if (m_topContentInset == contentInset)
        return;

    m_topContentInset = contentInset;

    protectedPageClient()->topContentInsetDidChange();

    if (!hasRunningProcess())
        return;
#if PLATFORM(COCOA)
    send(Messages::WebPage::SetTopContentInsetFenced(contentInset, m_drawingArea->createFence()));
#else
    send(Messages::WebPage::SetTopContentInset(contentInset));
#endif
}

Color WebPageProxy::underlayColor() const
{
    return internals().underlayColor;
}

void WebPageProxy::setUnderlayColor(const Color& color)
{
    if (internals().underlayColor == color)
        return;

    internals().underlayColor = color;

    if (hasRunningProcess())
        send(Messages::WebPage::SetUnderlayColor(color));
}

Color WebPageProxy::underPageBackgroundColor() const
{
    if (internals().underPageBackgroundColorOverride.isValid())
        return internals().underPageBackgroundColorOverride;
    if (internals().pageExtendedBackgroundColor.isValid())
        return internals().pageExtendedBackgroundColor;
    return platformUnderPageBackgroundColor();
}

Color WebPageProxy::underPageBackgroundColorOverride() const
{
    return internals().underPageBackgroundColorOverride;
}

void WebPageProxy::setUnderPageBackgroundColorOverride(Color&& newUnderPageBackgroundColorOverride)
{
    if (newUnderPageBackgroundColorOverride == internals().underPageBackgroundColorOverride)
        return;

    auto oldUnderPageBackgroundColor = underPageBackgroundColor();
    auto oldUnderPageBackgroundColorOverride = std::exchange(internals().underPageBackgroundColorOverride, newUnderPageBackgroundColorOverride);
    bool changesUnderPageBackgroundColor = !equalIgnoringSemanticColor(oldUnderPageBackgroundColor, underPageBackgroundColor());
    internals().underPageBackgroundColorOverride = WTFMove(oldUnderPageBackgroundColorOverride);

    if (changesUnderPageBackgroundColor)
        protectedPageClient()->underPageBackgroundColorWillChange();

    internals().underPageBackgroundColorOverride = WTFMove(newUnderPageBackgroundColorOverride);

    if (changesUnderPageBackgroundColor)
        protectedPageClient()->underPageBackgroundColorDidChange();

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
            send(Messages::WebPage::SetUnderPageBackgroundColorOverride(internals().underPageBackgroundColorOverride));
    });
}

void WebPageProxy::viewWillStartLiveResize()
{
    if (!hasRunningProcess())
        return;

    closeOverlayedViews();
    
    m_drawingArea->viewWillStartLiveResize();
    
    send(Messages::WebPage::ViewWillStartLiveResize());
}

void WebPageProxy::viewWillEndLiveResize()
{
    if (!hasRunningProcess())
        return;

    m_drawingArea->viewWillEndLiveResize();

    send(Messages::WebPage::ViewWillEndLiveResize());
}

void WebPageProxy::setViewNeedsDisplay(const Region& region)
{
    protectedPageClient()->setViewNeedsDisplay(region);
}

void WebPageProxy::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, ScrollIsAnimated animated)
{
    protectedPageClient()->requestScroll(scrollPosition, scrollOrigin, animated);
}

WebCore::FloatPoint WebPageProxy::viewScrollPosition() const
{
    return protectedPageClient()->viewScrollPosition();
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

bool WebPageProxy::isInWindow() const
{
    return internals().activityState.contains(ActivityState::IsInWindow);
}

bool WebPageProxy::isViewVisible() const
{
    return internals().activityState.contains(ActivityState::IsVisible);
}

bool WebPageProxy::isViewFocused() const
{
    return internals().activityState.contains(ActivityState::IsFocused);
}

bool WebPageProxy::isViewWindowActive() const
{
    return internals().activityState.contains(ActivityState::WindowIsActive);
}

void WebPageProxy::updateActivityState(OptionSet<ActivityState> flagsToUpdate)
{
    bool wasVisible = isViewVisible();
    Ref pageClient = this->pageClient();
    internals().activityState.remove(flagsToUpdate);
    if (flagsToUpdate & ActivityState::IsFocused && pageClient->isViewFocused())
        internals().activityState.add(ActivityState::IsFocused);
    if (flagsToUpdate & ActivityState::WindowIsActive && pageClient->isViewWindowActive())
        internals().activityState.add(ActivityState::WindowIsActive);
    if (flagsToUpdate & ActivityState::IsVisible) {
        bool isNowVisible = pageClient->isViewVisible();
        if (isNowVisible)
            internals().activityState.add(ActivityState::IsVisible);
        if (wasVisible != isNowVisible)
            WEBPAGEPROXY_RELEASE_LOG(ViewState, "updateActivityState: view visibility state changed %d -> %d", wasVisible, isNowVisible);
    }
    if (flagsToUpdate & ActivityState::IsVisibleOrOccluded && pageClient->isViewVisibleOrOccluded())
        internals().activityState.add(ActivityState::IsVisibleOrOccluded);
    if (flagsToUpdate & ActivityState::IsInWindow && pageClient->isViewInWindow())
        internals().activityState.add(ActivityState::IsInWindow);
    bool isVisuallyIdle = pageClient->isVisuallyIdle();
#if PLATFORM(COCOA) && !HAVE(CGS_FIX_FOR_RADAR_97530095) && ENABLE(MEDIA_USAGE)
    if (pageClient->isViewVisible() && m_mediaUsageManager && m_mediaUsageManager->isPlayingVideoInViewport())
        isVisuallyIdle = false;
#endif
    if (flagsToUpdate & ActivityState::IsVisuallyIdle && isVisuallyIdle)
        internals().activityState.add(ActivityState::IsVisuallyIdle);
    if (flagsToUpdate & ActivityState::IsAudible && isPlayingAudio() && !(internals().mutedState.contains(MediaProducerMutedState::AudioIsMuted)))
        internals().activityState.add(ActivityState::IsAudible);
    if (flagsToUpdate & ActivityState::IsLoading && internals().pageLoadState.isLoading())
        internals().activityState.add(ActivityState::IsLoading);
    if (flagsToUpdate & ActivityState::IsCapturingMedia && internals().mediaState.containsAny(MediaProducer::ActiveCaptureMask))
        internals().activityState.add(ActivityState::IsCapturingMedia);
}

void WebPageProxy::updateActivityState()
{
    updateActivityState(allActivityStates());
}

void WebPageProxy::activityStateDidChange(OptionSet<ActivityState> mayHaveChanged, ActivityStateChangeDispatchMode dispatchMode, ActivityStateChangeReplyMode replyMode)
{
    LOG_WITH_STREAM(ActivityState, stream << "WebPageProxy " << identifier() << " activityStateDidChange - mayHaveChanged " << mayHaveChanged);

    Ref pageClient = this->pageClient();

    internals().potentiallyChangedActivityStateFlags.add(mayHaveChanged);
    m_activityStateChangeWantsSynchronousReply = m_activityStateChangeWantsSynchronousReply || replyMode == ActivityStateChangeReplyMode::Synchronous;

    // We need to do this here instead of inside dispatchActivityStateChange() or viewIsBecomingVisible() because these don't run when the view doesn't
    // have a running WebProcess. For the same reason, we need to rely on PageClient::isViewVisible() instead of WebPageProxy::isViewVisible().
    if (internals().potentiallyChangedActivityStateFlags & ActivityState::IsVisible && m_shouldReloadDueToCrashWhenVisible && pageClient->isViewVisible()) {
        RunLoop::main().dispatch([this, weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis && std::exchange(m_shouldReloadDueToCrashWhenVisible, false)) {
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
    bool isNewlyInWindow = !isInWindow() && (mayHaveChanged & ActivityState::IsInWindow) && pageClient->isViewInWindow();
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
#if ENABLE(VIDEO_PRESENTATION_MODE) && !PLATFORM(APPLETV)
    // When leaving the current page, close the video fullscreen.
    // FIXME: On tvOS, modally presenting the AVPlayerViewController when entering fullscreen causes
    // the web view to become invisible, resulting in us exiting fullscreen as soon as we entered it.
    // Find a way to track web view visibility on tvOS that accounts for this behavior.
    if (m_videoPresentationManager && m_videoPresentationManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard))
        m_videoPresentationManager->requestHideAndExitFullscreen();
#endif
}

void WebPageProxy::viewDidEnterWindow()
{
    LayerHostingMode layerHostingMode = protectedPageClient()->viewLayerHostingMode();
    if (internals().layerHostingMode != layerHostingMode) {
        internals().layerHostingMode = layerHostingMode;
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

    LOG_WITH_STREAM(ActivityState, stream << "WebPageProxy " << identifier() << " dispatchActivityStateChange - potentiallyChangedActivityStateFlags " << internals().potentiallyChangedActivityStateFlags);

    // If the visibility state may have changed, then so may the visually idle & occluded agnostic state.
    if (internals().potentiallyChangedActivityStateFlags & ActivityState::IsVisible)
        internals().potentiallyChangedActivityStateFlags.add({ ActivityState::IsVisibleOrOccluded, ActivityState::IsVisuallyIdle });

    // Record the prior view state, update the flags that may have changed,
    // and check which flags have actually changed.
    auto previousActivityState = internals().activityState;
    updateActivityState(internals().potentiallyChangedActivityStateFlags);
    auto changed = internals().activityState ^ previousActivityState;

    if (changed)
        LOG_WITH_STREAM(ActivityState, stream << "WebPageProxy " << identifier() << " dispatchActivityStateChange: state changed from " << previousActivityState << " to " << internals().activityState);

    if ((changed & ActivityState::WindowIsActive) && isViewWindowActive())
        updateCurrentModifierState();

    if ((internals().potentiallyChangedActivityStateFlags & ActivityState::IsVisible)) {
        if (isViewVisible())
            viewIsBecomingVisible();
        else
            protectedProcess()->pageIsBecomingInvisible(internals().webPageID);
    }

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (internals().potentiallyChangedActivityStateFlags & ActivityState::IsConnectedToHardwareConsole)
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
    if (!isViewVisible())
        m_activityStateChangeWantsSynchronousReply = false;

    auto activityStateChangeID = m_activityStateChangeWantsSynchronousReply ? takeNextActivityStateChangeID() : static_cast<ActivityStateChangeID>(ActivityStateChangeAsynchronous);

    if (changed || activityStateChangeID != ActivityStateChangeAsynchronous || !m_nextActivityStateChangeCallbacks.isEmpty()) {
        auto callbackAggregator = CallbackAggregator::create([callbacks = std::exchange(m_nextActivityStateChangeCallbacks, { })] () mutable {
            for (auto& callback : callbacks)
                callback();
        });
        forEachWebContentProcess([&](auto& webProcess, auto pageID) {
            webProcess.sendWithAsyncReply(Messages::WebPage::SetActivityState(internals().activityState, activityStateChangeID), [callbackAggregator] { }, pageID);
        });
    }

    // This must happen after the SetActivityState message is sent, to ensure the page visibility event can fire.
    updateThrottleState();

#if ENABLE(POINTER_LOCK)
    if (((changed & ActivityState::IsVisible) && !isViewVisible()) || ((changed & ActivityState::WindowIsActive) && !protectedPageClient()->isViewWindowActive())
        || ((changed & ActivityState::IsFocused) && !isViewFocused()))
        requestPointerUnlock();
#endif

    if (changed & ActivityState::IsVisible) {
        if (isViewVisible())
            internals().visiblePageToken = m_process->visiblePageToken();
        else {
            internals().visiblePageToken = nullptr;

            // If we've started the responsiveness timer as part of telling the web process to update the backing store
            // state, it might not send back a reply (since it won't paint anything if the web page is hidden) so we
            // stop the unresponsiveness timer here.
            protectedProcess()->stopResponsivenessTimer();
        }
    }

    if (changed & ActivityState::IsInWindow) {
        if (isInWindow())
            viewDidEnterWindow();
        else
            viewDidLeaveWindow();
    }

#if ENABLE(WEB_AUTHN) && HAVE(WEB_AUTHN_AS_MODERN)
    if ((changed & ActivityState::WindowIsActive) && m_credentialsMessenger) {
        if (protectedPageClient()->isViewWindowActive())
            m_credentialsMessenger->unpauseConditionalAssertion();
        else
            m_credentialsMessenger->pauseConditionalAssertion();
    }
#endif

    updateBackingStoreDiscardableState();

    if (activityStateChangeID != ActivityStateChangeAsynchronous)
        waitForDidUpdateActivityState(activityStateChangeID);

    internals().potentiallyChangedActivityStateFlags = { };
    m_activityStateChangeWantsSynchronousReply = false;
    m_viewWasEverInWindow |= isNowInWindow;

#if ENABLE(EXTENSION_CAPABILITIES)
    updateMediaCapability();
#endif

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
        internals().preventProcessSuppressionCount = m_process->processPool().processSuppressionDisabledForPageCount();
    else if (!internals().preventProcessSuppressionCount)
        internals().preventProcessSuppressionCount = nullptr;

    if (internals().activityState & ActivityState::IsVisuallyIdle)
        internals().pageIsUserObservableCount = nullptr;
    else if (!internals().pageIsUserObservableCount)
        internals().pageIsUserObservableCount = m_process->processPool().userObservablePageCount();

#if USE(RUNNINGBOARD)
    if (isViewVisible()) {
        if (!m_processActivityState.hasValidVisibleActivity()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because the view is visible");
            m_processActivityState.takeVisibleActivity();
        }
    } else if (m_processActivityState.hasValidVisibleActivity()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because the view is no longer visible");
        m_processActivityState.dropVisibleActivity();
    }

    bool isAudible = internals().activityState.contains(ActivityState::IsAudible);
    if (isAudible) {
        if (!m_processActivityState.hasValidAudibleActivity()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because we are playing audio");
            m_processActivityState.takeAudibleActivity();
        }
        internals().audibleActivityTimer.stop();
    } else if (m_processActivityState.hasValidAudibleActivity()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess will release a foreground assertion in %g seconds because we are no longer playing audio", audibleActivityClearDelay.seconds());
        if (!internals().audibleActivityTimer.isActive())
            internals().audibleActivityTimer.startOneShot(audibleActivityClearDelay);
    }

    bool isCapturingMedia = internals().activityState.contains(ActivityState::IsCapturingMedia);
    if (isCapturingMedia) {
        if (!m_processActivityState.hasValidCapturingActivity()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because media capture is active");
            m_processActivityState.takeCapturingActivity();
        }
    } else if (m_processActivityState.hasValidCapturingActivity()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because media capture is no longer active");
        m_processActivityState.dropCapturingActivity();
    }
#endif
}

void WebPageProxy::clearAudibleActivity()
{
    WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because we are no longer playing audio");
    m_processActivityState.dropAudibleActivity();
}

void WebPageProxy::updateHiddenPageThrottlingAutoIncreases()
{
    if (!m_preferences->hiddenPageDOMTimerThrottlingAutoIncreases())
        internals().hiddenPageDOMTimerThrottlingAutoIncreasesCount = nullptr;
    else if (!internals().hiddenPageDOMTimerThrottlingAutoIncreasesCount)
        internals().hiddenPageDOMTimerThrottlingAutoIncreasesCount = m_process->processPool().hiddenPageThrottlingAutoIncreasesCount();
}

void WebPageProxy::layerHostingModeDidChange()
{
    LayerHostingMode layerHostingMode = protectedPageClient()->viewLayerHostingMode();
    if (internals().layerHostingMode == layerHostingMode)
        return;
    internals().layerHostingMode = layerHostingMode;
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
    if (!m_processActivityState.hasValidVisibleActivity()) {
        ASSERT_NOT_REACHED();
        return;
    }
#endif

    m_waitingForDidUpdateActivityState = true;

    m_drawingArea->waitForDidUpdateActivityState(activityStateChangeID, protectedProcess());
}

IntSize WebPageProxy::viewSize() const
{
    return protectedPageClient()->viewSize();
}

void WebPageProxy::setInitialFocus(bool forward, bool isKeyboardEventValid, const std::optional<WebKeyboardEvent>& keyboardEvent, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::SetInitialFocus(forward, isKeyboardEventValid, keyboardEvent), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::setInitialFocus"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::clearSelection(std::optional<FrameIdentifier> frameID)
{
    if (!hasRunningProcess())
        return;
    sendToProcessContainingFrame(frameID, Messages::WebPage::ClearSelection());
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

const EditorState& WebPageProxy::editorState() const
{
    return internals().editorState;
}

bool WebPageProxy::hasSelectedRange() const
{
    return internals().editorState.selectionIsRange;
}

bool WebPageProxy::isContentEditable() const
{
    return internals().editorState.isContentEditable;
}

void WebPageProxy::updateFontAttributesAfterEditorStateChange()
{
    internals().cachedFontAttributesAtSelectionStart.reset();

    if (!internals().editorState.hasPostLayoutData())
        return;

    if (auto fontAttributes = internals().editorState.postLayoutData->fontAttributes) {
        m_uiClient->didChangeFontAttributes(*fontAttributes);
        internals().cachedFontAttributesAtSelectionStart = WTFMove(fontAttributes);
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

    if (auto attributes = internals().cachedFontAttributesAtSelectionStart) {
        callback(*attributes);
        return;
    }

    sendWithAsyncReply(Messages::WebPage::RequestFontAttributesAtSelectionStart(), [this, protectedThis = Ref { *this }, callback = WTFMove(callback)] (const WebCore::FontAttributes& attributes) mutable {
        internals().cachedFontAttributesAtSelectionStart = attributes;
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

MediaProducerMutedStateFlags WebPageProxy::mutedStateFlags() const
{
    return internals().mutedState;
}

bool WebPageProxy::isAudioMuted() const
{
    return internals().mutedState.contains(MediaProducerMutedState::AudioIsMuted);
}

bool WebPageProxy::isMediaStreamCaptureMuted() const
{
    return internals().mutedState.containsAny(MediaProducer::MediaStreamCaptureIsMuted);
}

void WebPageProxy::setMediaStreamCaptureMuted(bool muted)
{
    auto state = internals().mutedState;
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

    if (internals().mutedState.containsAny(WebCore::MediaProducer::MediaStreamCaptureIsMuted))
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
    WebProcessProxy::muteCaptureInPagesExcept(internals().webPageID);
#endif
    setMediaStreamCaptureMuted(false);
}

#if !PLATFORM(COCOA)
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
    protectedPageClient()->makeViewBlank(false);
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
            protectedPageClient()->makeViewBlank(true);
            m_madeViewBlankDueToLackOfRenderingUpdate = true;
        }
    }
}

void WebPageProxy::discardQueuedMouseEvents()
{
    while (internals().mouseEventQueue.size() > 1)
        internals().mouseEventQueue.removeLast();
}

#if ENABLE(DRAG_SUPPORT)

DragHandlingMethod WebPageProxy::currentDragHandlingMethod() const
{
    return internals().currentDragHandlingMethod;
}

IntRect WebPageProxy::currentDragCaretRect() const
{
    return internals().currentDragCaretRect;
}

IntRect WebPageProxy::currentDragCaretEditableElementRect() const
{
    return internals().currentDragCaretEditableElementRect;
}

void WebPageProxy::dragEntered(DragData& dragData, const String& dragStorageName)
{
#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().grantAccessToCurrentTypes(m_process.get(), dragStorageName);
#endif
    launchInitialProcessIfNecessary();
    performDragControllerAction(DragControllerAction::Entered, dragData);
}

void WebPageProxy::dragUpdated(DragData& dragData, const String& dragStorageName)
{
#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().grantAccessToCurrentTypes(m_process.get(), dragStorageName);
#endif
    performDragControllerAction(DragControllerAction::Updated, dragData);
}

void WebPageProxy::dragExited(DragData& dragData)
{
    performDragControllerAction(DragControllerAction::Exited, dragData);
}

void WebPageProxy::performDragOperation(DragData& dragData, const String& dragStorageName, SandboxExtension::Handle&& sandboxExtensionHandle, Vector<SandboxExtension::Handle>&& sandboxExtensionsForUpload)
{
#if PLATFORM(COCOA)
    grantAccessToCurrentPasteboardData(dragStorageName);
#endif

#if PLATFORM(GTK)
    performDragControllerAction(DragControllerAction::PerformDragOperation, dragData);
#else
    if (!hasRunningProcess())
        return;

    sendWithAsyncReply(Messages::WebPage::PerformDragOperation(dragData, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionsForUpload)), [this, protectedThis = Ref { *this }] (bool handled) {
        protectedPageClient()->didPerformDragOperation(handled);
    });
#endif
}

void WebPageProxy::performDragControllerAction(DragControllerAction action, DragData& dragData, const std::optional<WebCore::FrameIdentifier>& frameID)
{
    if (!hasRunningProcess())
        return;

    auto completionHandler = [this, protectedThis = Ref { *this }, action, dragData] (std::optional<WebCore::DragOperation> dragOperation, WebCore::DragHandlingMethod dragHandlingMethod, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted, const IntRect& insertionRect, const IntRect& editableElementRect, std::optional<WebCore::RemoteUserInputEventData> remoteUserInputEventData) mutable {
        if (!remoteUserInputEventData) {
            didPerformDragControllerAction(dragOperation, dragHandlingMethod, mouseIsOverFileInput, numberOfItemsToBeAccepted, insertionRect, editableElementRect);
            return;
        }
        dragData.setClientPosition(remoteUserInputEventData->transformedPoint);
        performDragControllerAction(action, dragData, remoteUserInputEventData->targetFrameID);
    };

#if PLATFORM(GTK)
    UNUSED_PARAM(frameID);
    String url = dragData.asURL();
    if (!url.isEmpty())
        protectedProcess()->assumeReadAccessToBaseURL(*this, url);

    ASSERT(dragData.platformData());
    sendWithAsyncReply(Messages::WebPage::PerformDragControllerAction(action, dragData.clientPosition(), dragData.globalPosition(), dragData.draggingSourceOperationMask(), *dragData.platformData(), dragData.flags()), WTFMove(completionHandler));
#else
    sendToProcessContainingFrame(frameID, Messages::WebPage::PerformDragControllerAction(frameID, action, dragData), WTFMove(completionHandler));
#endif
}

void WebPageProxy::didPerformDragControllerAction(std::optional<WebCore::DragOperation> dragOperation, WebCore::DragHandlingMethod dragHandlingMethod, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted, const IntRect& insertionRect, const IntRect& editableElementRect)
{
    m_currentDragOperation = dragOperation;
    internals().currentDragHandlingMethod = dragHandlingMethod;
    m_currentDragIsOverFileInput = mouseIsOverFileInput;
    m_currentDragNumberOfFilesToBeAccepted = numberOfItemsToBeAccepted;
    internals().currentDragCaretEditableElementRect = editableElementRect;
    setDragCaretRect(insertionRect);
    protectedPageClient()->didPerformDragControllerAction();
}

#if PLATFORM(GTK)
void WebPageProxy::startDrag(SelectionData&& selectionData, OptionSet<WebCore::DragOperation> dragOperationMask, std::optional<ShareableBitmap::Handle>&& dragImageHandle, IntPoint&& dragImageHotspot)
{
    RefPtr dragImage = dragImageHandle ? ShareableBitmap::create(WTFMove(*dragImageHandle)) : nullptr;
    protectedPageClient()->startDrag(WTFMove(selectionData), dragOperationMask, WTFMove(dragImage), WTFMove(dragImageHotspot));

    didStartDrag();
}
#endif

void WebPageProxy::dragEnded(const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<WebCore::DragOperation> dragOperationMask, const std::optional<WebCore::FrameIdentifier>& frameID)
{
    if (!hasRunningProcess())
        return;
    auto completionHandler = [this, protectedThis = Ref { *this }, globalPosition, dragOperationMask] (std::optional<WebCore::RemoteUserInputEventData> remoteUserInputEventData) {
        if (!remoteUserInputEventData) {
            resetCurrentDragInformation();
            return;
        }
        dragEnded(remoteUserInputEventData->transformedPoint, globalPosition, dragOperationMask, remoteUserInputEventData->targetFrameID);
    };

    sendToProcessContainingFrame(frameID, Messages::WebPage::DragEnded(frameID, clientPosition, globalPosition, dragOperationMask), WTFMove(completionHandler));
    setDragCaretRect({ });
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

void WebPageProxy::resetCurrentDragInformation()
{
    m_currentDragOperation = std::nullopt;
    internals().currentDragHandlingMethod = DragHandlingMethod::None;
    m_currentDragIsOverFileInput = false;
    m_currentDragNumberOfFilesToBeAccepted = 0;
    setDragCaretRect({ });
}

void WebPageProxy::setDragCaretRect(const IntRect& dragCaretRect)
{
    auto& currentDragRect = internals().currentDragCaretRect;
    if (currentDragRect == dragCaretRect)
        return;

    auto previousRect = std::exchange(currentDragRect, dragCaretRect);
    protectedPageClient()->didChangeDragCaretRect(previousRect, dragCaretRect);
}

#endif

static bool removeOldRedundantEvent(Deque<NativeWebMouseEvent>& queue, WebEventType incomingEventType)
{
    if (incomingEventType != WebEventType::MouseMove && incomingEventType != WebEventType::MouseForceChanged)
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
        if (type != WebEventType::MouseMove && type != WebEventType::MouseForceChanged)
            break;
    }
    return false;
}

void WebPageProxy::handleMouseEventReply(WebEventType eventType, bool handled, const std::optional<WebCore::RemoteUserInputEventData>& remoteUserInputEventData, std::optional<Vector<SandboxExtensionHandle>>&& sandboxExtensions)
{
    if (remoteUserInputEventData) {
        auto& event = internals().mouseEventQueue.first();
        event.setPosition(remoteUserInputEventData->transformedPoint);
        sendMouseEvent(remoteUserInputEventData->targetFrameID, event, WTFMove(sandboxExtensions));
        return;
    }
    didReceiveEvent(eventType, handled);
}

void WebPageProxy::sendMouseEvent(const WebCore::FrameIdentifier& frameID, const NativeWebMouseEvent& event, std::optional<Vector<SandboxExtensionHandle>>&& sandboxExtensions)
{
    protectedProcess()->recordUserGestureAuthorizationToken(webPageID(), event.authorizationToken());
    sendToProcessContainingFrame(frameID, Messages::WebPage::MouseEvent(frameID, event, WTFMove(sandboxExtensions)), [this, protectedThis = Ref { *this }] (std::optional<WebEventType> eventType, bool handled, std::optional<WebCore::RemoteUserInputEventData> remoteUserInputEventData) {
        if (!m_pageClient)
            return;
        if (!eventType) {
            mouseEventHandlingCompleted(eventType, handled);
            return;
        }
        // FIXME: If these sandbox extensions are important, find a way to get them to the iframe process.
        handleMouseEventReply(*eventType, handled, remoteUserInputEventData, { });
    });
}

void WebPageProxy::handleMouseEvent(const NativeWebMouseEvent& event)
{
    if (event.type() == WebEventType::MouseDown)
        launchInitialProcessIfNecessary();

    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

#if ENABLE(CONTEXT_MENU_EVENT)
    if (event.button() == WebMouseEventButton::Right && event.type() == WebEventType::MouseDown) {
        ASSERT(m_contextMenuPreventionState != EventPreventionState::Waiting);
        m_contextMenuPreventionState = EventPreventionState::Waiting;
    }
#endif

    // If we receive multiple mousemove or mouseforcechanged events and the most recent mousemove or mouseforcechanged event
    // (respectively) has not yet been sent to WebProcess for processing, remove the pending mouse event and insert the new
    // event in the queue.
    bool didRemoveEvent = removeOldRedundantEvent(internals().mouseEventQueue, event.type());
    internals().mouseEventQueue.append(event);

    UNUSED_PARAM(didRemoveEvent);
    LOG_WITH_STREAM(MouseHandling, stream << "UIProcess: " << (didRemoveEvent ? "replaced" : "enqueued") << " mouse event " << event.type() << " (queue size " << internals().mouseEventQueue.size() << ")");

    if (event.type() != WebEventType::MouseMove)
        send(Messages::WebPage::FlushDeferredDidReceiveMouseEvent());

    if (internals().mouseEventQueue.size() == 1) // Otherwise, called from DidReceiveEvent message handler.
        processNextQueuedMouseEvent();
}

void WebPageProxy::dispatchMouseDidMoveOverElementAsynchronously(const NativeWebMouseEvent& event)
{
    sendWithAsyncReply(Messages::WebPage::PerformHitTestForMouseEvent { event }, [this, protectedThis = Ref { *this }](WebHitTestResultData&& hitTestResult, OptionSet<WebEventModifier> modifiers, UserData&& userData) {
        if (!isClosed())
            mouseDidMoveOverElement(WTFMove(hitTestResult), modifiers, WTFMove(userData));
    });
}

void WebPageProxy::processNextQueuedMouseEvent()
{
    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

    ASSERT(!internals().mouseEventQueue.isEmpty());

    const NativeWebMouseEvent& event = internals().mouseEventQueue.first();

    if (protectedPageClient()->windowIsFrontWindowUnderMouse(event))
        setToolTip(String());

    Ref process = m_process;
    auto eventType = event.type();
    if (eventType == WebEventType::MouseDown || eventType == WebEventType::MouseForceChanged || eventType == WebEventType::MouseForceDown)
        process->startResponsivenessTimer(WebProcessProxy::UseLazyStop::Yes);
    else if (eventType != WebEventType::MouseMove) {
        // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
        process->startResponsivenessTimer();
    }

    std::optional<Vector<SandboxExtension::Handle>> sandboxExtensions;

#if PLATFORM(MAC)
    bool eventMayStartDrag = !m_currentDragOperation && eventType == WebEventType::MouseMove && event.button() != WebMouseEventButton::None;
    if (eventMayStartDrag)
        sandboxExtensions = SandboxExtension::createHandlesForMachLookup({ "com.apple.iconservices"_s, "com.apple.iconservices.store"_s }, process->auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
#endif

    LOG_WITH_STREAM(MouseHandling, stream << "UIProcess: sent mouse event " << eventType << " (queue size " << internals().mouseEventQueue.size() << ")");
    sendMouseEvent(m_mainFrame->frameID(), event, WTFMove(sandboxExtensions));
}

void WebPageProxy::doAfterProcessingAllPendingMouseEvents(WTF::Function<void ()>&& action)
{
    if (!isProcessingMouseEvents()) {
        action();
        return;
    }

    internals().callbackHandlersAfterProcessingPendingMouseEvents.append(WTFMove(action));
}

void WebPageProxy::didFinishProcessingAllPendingMouseEvents()
{
    flushPendingMouseEventCallbacks();
}

void WebPageProxy::flushPendingMouseEventCallbacks()
{
    for (auto& callback : internals().callbackHandlersAfterProcessingPendingMouseEvents)
        callback();

    internals().callbackHandlersAfterProcessingPendingMouseEvents.clear();
}

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::dispatchWheelEventWithoutScrolling(const WebWheelEvent& event, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_mainFrame) {
        completionHandler(false);
        return;
    }
    sendWithAsyncReply(Messages::WebPage::DispatchWheelEventWithoutScrolling(m_mainFrame->frameID(), event), WTFMove(completionHandler));
}
#endif

void WebPageProxy::handleNativeWheelEvent(const NativeWebWheelEvent& nativeWheelEvent)
{
    if (!hasRunningProcess())
        return;

    closeOverlayedViews();

    cacheWheelEventScrollingAccelerationCurve(nativeWheelEvent);

    if (!wheelEventCoalescer().shouldDispatchEvent(nativeWheelEvent))
        return;

    auto eventToDispatch = *wheelEventCoalescer().nextEventToDispatch();
    handleWheelEvent(eventToDispatch);
}

void WebPageProxy::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    if (!hasRunningProcess())
        return;

    if (drawingArea()->shouldSendWheelEventsToEventDispatcher()) {
        continueWheelEventHandling(wheelEvent, { WheelEventProcessingSteps::SynchronousScrolling, false }, { });
        return;
    }

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
    if (m_scrollingCoordinatorProxy) {
        auto rubberBandableEdges = rubberBandableEdgesRespectingHistorySwipe();
        m_scrollingCoordinatorProxy->handleWheelEvent(wheelEvent, rubberBandableEdges);
        // continueWheelEventHandling() will get called after the event has been handled by the scrolling thread.
    }
#endif
}

void WebPageProxy::continueWheelEventHandling(const WebWheelEvent& wheelEvent, const WheelEventHandlingResult& result, std::optional<bool> willStartSwipe)
{
    LOG_WITH_STREAM(WheelEvents, stream << "WebPageProxy::continueWheelEventHandling - " << result);

    if (!result.needsMainThreadProcessing()) {
        wheelEventHandlingCompleted(result.wasHandled);
        return;
    }

    if (!m_mainFrame)
        return;

    auto rubberBandableEdges = rubberBandableEdgesRespectingHistorySwipe();
    sendWheelEvent(m_mainFrame->frameID(), wheelEvent, result.steps, rubberBandableEdges, willStartSwipe, result.wasHandled);
}

void WebPageProxy::sendWheelEvent(WebCore::FrameIdentifier frameID, const WebWheelEvent& event, OptionSet<WheelEventProcessingSteps> processingSteps, RectEdges<bool> rubberBandableEdges, std::optional<bool> willStartSwipe, bool wasHandledForScrolling)
{
#if HAVE(DISPLAY_LINK)
    internals().wheelEventActivityHysteresis.impulse();
#endif

    RefPtr connection = messageSenderConnection();
    if (!connection)
        return;

    if (drawingArea()->shouldSendWheelEventsToEventDispatcher()) {
        sendWheelEventScrollingAccelerationCurveIfNecessary(event);
        connection->send(Messages::EventDispatcher::WheelEvent(webPageID(), event, rubberBandableEdges), 0, { }, Thread::QOS::UserInteractive);
    } else {
        sendToProcessContainingFrame(frameID, Messages::WebPage::HandleWheelEvent(frameID, event, processingSteps, willStartSwipe), [weakThis = WeakPtr { *this }, wheelEvent = event, processingSteps, rubberBandableEdges, willStartSwipe, wasHandledForScrolling](ScrollingNodeID nodeID, std::optional<WheelScrollGestureState> gestureState, bool handled, std::optional<RemoteUserInputEventData> remoteWheelEventData) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (protectedThis->isClosed())
                return;

            if (remoteWheelEventData) {
                wheelEvent.setPosition(remoteWheelEventData->transformedPoint);
                protectedThis->sendWheelEvent(remoteWheelEventData->targetFrameID, wheelEvent, processingSteps, rubberBandableEdges, willStartSwipe, wasHandledForScrolling);
                return;
            }

            protectedThis->handleWheelEventReply(wheelEvent, nodeID, gestureState, wasHandledForScrolling, handled);
        });
    }

    // Manually ping the web process to check for responsiveness since our wheel
    // event will dispatch to a non-main thread, which always responds.
    protectedProcess()->isResponsiveWithLazyStop();
}

void WebPageProxy::handleWheelEventReply(const WebWheelEvent& event, ScrollingNodeID nodeID, std::optional<WheelScrollGestureState> gestureState, bool wasHandledForScrolling, bool wasHandledByWebProcess)
{
    LOG_WITH_STREAM(WheelEvents, stream << "WebPageProxy::handleWheelEventReply " << platform(event) << " - handled for scrolling " << wasHandledForScrolling << " handled by web process " << wasHandledByWebProcess << " nodeID " << nodeID << " gesture state " << gestureState);

    MESSAGE_CHECK(m_process, wheelEventCoalescer().hasEventsBeingProcessed());

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
    if (auto* scrollingCoordinatorProxy = this->scrollingCoordinatorProxy()) {
        scrollingCoordinatorProxy->wheelEventHandlingCompleted(platform(event), nodeID, gestureState, wasHandledForScrolling || wasHandledByWebProcess);
        return;
    }
#else
    UNUSED_PARAM(event);
    UNUSED_PARAM(nodeID);
    UNUSED_PARAM(gestureState);
#endif
    wheelEventHandlingCompleted(wasHandledForScrolling || wasHandledByWebProcess);
}

void WebPageProxy::wheelEventHandlingCompleted(bool wasHandled)
{
    auto oldestProcessedEvent = wheelEventCoalescer().takeOldestEventBeingProcessed();

    if (oldestProcessedEvent)
        LOG_WITH_STREAM(WheelEvents, stream << "WebPageProxy::wheelEventHandlingCompleted - finished handling " << platform(*oldestProcessedEvent) << " handled " << wasHandled);
    else
        LOG_WITH_STREAM(WheelEvents, stream << "WebPageProxy::wheelEventHandlingCompleted - no event, handled " << wasHandled);

    if (oldestProcessedEvent && !wasHandled) {
        m_uiClient->didNotHandleWheelEvent(this, *oldestProcessedEvent);
        if (m_pageClient)
            m_pageClient->wheelEventWasNotHandledByWebCore(*oldestProcessedEvent);
    }

    if (auto eventToSend = wheelEventCoalescer().nextEventToDispatch()) {
        handleWheelEvent(*eventToSend);
        return;
    }
    
    if (RefPtr automationSession = process().processPool().automationSession())
        automationSession->wheelEventsFlushedForPage(*this);
}

void WebPageProxy::cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent& nativeWheelEvent)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (m_scrollingCoordinatorProxy) {
        m_scrollingCoordinatorProxy->cacheWheelEventScrollingAccelerationCurve(nativeWheelEvent);
        return;
    }

    ASSERT(drawingArea()->shouldSendWheelEventsToEventDispatcher());

    if (nativeWheelEvent.momentumPhase() != WebWheelEvent::PhaseBegan)
        return;

    if (!preferences().momentumScrollingAnimatorEnabled())
        return;

    // FIXME: We should not have to fetch the curve repeatedly, but it can also change occasionally.
    internals().scrollingAccelerationCurve = ScrollingAccelerationCurve::fromNativeWheelEvent(nativeWheelEvent);
#endif
}

void WebPageProxy::sendWheelEventScrollingAccelerationCurveIfNecessary(const WebWheelEvent& event)
{
    ASSERT(drawingArea()->shouldSendWheelEventsToEventDispatcher());
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (event.momentumPhase() != WebWheelEvent::PhaseBegan)
        return;

    if (internals().scrollingAccelerationCurve == internals().lastSentScrollingAccelerationCurve)
        return;

    RefPtr connection = messageSenderConnection();
    if (!connection)
        return;

    connection->send(Messages::EventDispatcher::SetScrollingAccelerationCurve(webPageID(), internals().scrollingAccelerationCurve), 0, { }, Thread::QOS::UserInteractive);
    internals().lastSentScrollingAccelerationCurve = internals().scrollingAccelerationCurve;
#endif
}

#if HAVE(DISPLAY_LINK)
void WebPageProxy::wheelEventHysteresisUpdated(PAL::HysteresisState)
{
    updateDisplayLinkFrequency();
}

void WebPageProxy::updateDisplayLinkFrequency()
{
    if (!m_process->hasConnection() || !m_displayID)
        return;

    bool wantsFullSpeedUpdates = m_hasActiveAnimatedScroll || internals().wheelEventActivityHysteresis.state() == PAL::HysteresisState::Started;
    if (wantsFullSpeedUpdates != m_registeredForFullSpeedUpdates) {
        protectedProcess()->setDisplayLinkForDisplayWantsFullSpeedUpdates(*m_displayID, wantsFullSpeedUpdates);
        m_registeredForFullSpeedUpdates = wantsFullSpeedUpdates;
    }
}
#endif

void WebPageProxy::updateWheelEventActivityAfterProcessSwap()
{
#if HAVE(DISPLAY_LINK)
    updateDisplayLinkFrequency();
#endif
}

WebWheelEventCoalescer& WebPageProxy::wheelEventCoalescer()
{
    if (!m_wheelEventCoalescer)
        m_wheelEventCoalescer = makeUnique<WebWheelEventCoalescer>();

    return *m_wheelEventCoalescer;
}

bool WebPageProxy::hasQueuedKeyEvent() const
{
    return !internals().keyEventQueue.isEmpty();
}

const NativeWebKeyboardEvent& WebPageProxy::firstQueuedKeyEvent() const
{
    return internals().keyEventQueue.first();
}

void WebPageProxy::sendKeyEvent(const NativeWebKeyboardEvent& event)
{
    protectedProcess()->recordUserGestureAuthorizationToken(webPageID(), event.authorizationToken());

    auto handleKeyEventReply = [this, protectedThis = Ref { *this }] (std::optional<WebEventType> eventType, bool handled) {
        if (!m_pageClient)
            return;
        if (!eventType) {
            keyEventHandlingCompleted(eventType, handled);
            return;
        }
        didReceiveEvent(*eventType, handled);
    };

    auto targetFrameID = m_focusedFrame ? m_focusedFrame->frameID() : m_mainFrame->frameID();
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::KeyEvent(targetFrameID, event), WTFMove(handleKeyEventReply));
}

bool WebPageProxy::handleKeyboardEvent(const NativeWebKeyboardEvent& event)
{
    if (!hasRunningProcess())
        return false;

    if (!m_mainFrame) {
        m_uiClient->didNotHandleKeyEvent(this, event);
        return false;
    }

    LOG_WITH_STREAM(KeyHandling, stream << "WebPageProxy::handleKeyboardEvent: " << event.type());

    internals().keyEventQueue.append(event);

    Ref process = m_process;
    process->startResponsivenessTimer(event.type() == WebEventType::KeyDown ? WebProcessProxy::UseLazyStop::Yes : WebProcessProxy::UseLazyStop::No);

    // Otherwise, sent from DidReceiveEvent message handler.
    if (internals().keyEventQueue.size() == 1) {
        LOG(KeyHandling, " UI process: sent keyEvent from handleKeyboardEvent");
        sendKeyEvent(event);
    }

    return true;
}

const WebPreferencesStore& WebPageProxy::preferencesStore() const
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
        auto location = touchPoint.location();
        auto update = [this, location](TrackingType& trackingType, EventTrackingRegions::EventType eventType) {
            if (trackingType == TrackingType::Synchronous)
                return;
            auto trackingTypeForLocation = m_scrollingCoordinatorProxy->eventTrackingTypeForPoint(eventType, location);
            trackingType = mergeTrackingTypes(trackingType, trackingTypeForLocation);
        };
        auto& tracking = internals().touchEventTracking;
        using Type = EventTrackingRegions::EventType;
        update(tracking.touchForceChangedTracking, Type::Touchforcechange);
        update(tracking.touchStartTracking, Type::Touchstart);
        update(tracking.touchMoveTracking, Type::Touchmove);
        update(tracking.touchEndTracking, Type::Touchend);
        update(tracking.touchStartTracking, Type::Pointerover);
        update(tracking.touchStartTracking, Type::Pointerenter);
        update(tracking.touchStartTracking, Type::Pointerdown);
        update(tracking.touchMoveTracking, Type::Pointermove);
        update(tracking.touchEndTracking, Type::Pointerup);
        update(tracking.touchEndTracking, Type::Pointerout);
        update(tracking.touchEndTracking, Type::Pointerleave);
        update(tracking.touchStartTracking, Type::Mousedown);
        update(tracking.touchMoveTracking, Type::Mousemove);
        update(tracking.touchEndTracking, Type::Mouseup);
    }
#else
    UNUSED_PARAM(touchStartEvent);
    internals().touchEventTracking.touchForceChangedTracking = TrackingType::Synchronous;
    internals().touchEventTracking.touchStartTracking = TrackingType::Synchronous;
    internals().touchEventTracking.touchMoveTracking = TrackingType::Synchronous;
    internals().touchEventTracking.touchEndTracking = TrackingType::Synchronous;
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
    auto& tracking = internals().touchEventTracking;
    auto globalTrackingType = tracking.isTrackingAnything() ? TrackingType::Asynchronous : TrackingType::NotTracking;
    globalTrackingType = mergeTrackingTypes(globalTrackingType, tracking.touchForceChangedTracking);
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        switch (touchPoint.state()) {
        case WebPlatformTouchPoint::State::Released:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, tracking.touchEndTracking);
            break;
        case WebPlatformTouchPoint::State::Pressed:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, tracking.touchStartTracking);
            break;
        case WebPlatformTouchPoint::State::Moved:
        case WebPlatformTouchPoint::State::Stationary:
            globalTrackingType = mergeTrackingTypes(globalTrackingType, tracking.touchMoveTracking);
            break;
        case WebPlatformTouchPoint::State::Cancelled:
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

    internals().gestureEventQueue.append(event);
    // FIXME: Consider doing some coalescing here.

    protectedProcess()->startResponsivenessTimer((event.type() == WebEventType::GestureStart || event.type() == WebEventType::GestureChange) ? WebProcessProxy::UseLazyStop::Yes : WebProcessProxy::UseLazyStop::No);

    sendWithAsyncReply(Messages::EventDispatcher::GestureEvent(internals().webPageID, event), [this, protectedThis = Ref { *this }] (std::optional<WebEventType> eventType, bool handled) {
        if (!m_pageClient)
            return;
        if (!eventType)
            return;
        didReceiveEvent(*eventType, handled);
    }, 0);
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPageProxy::sendPreventableTouchEvent(WebCore::FrameIdentifier frameID, const NativeWebTouchEvent& event)
{
    sendToProcessContainingFrame(frameID, Messages::EventDispatcher::TouchEvent(internals().webPageID, frameID, event), [this, weakThis = WeakPtr { *this }, event = event] (bool handled, std::optional<RemoteUserInputEventData> remoteTouchEventData) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (remoteTouchEventData) {
            event.setPosition(remoteTouchEventData->transformedPoint);
            sendPreventableTouchEvent(remoteTouchEventData->targetFrameID, event);
            return;
        }

        bool didFinishDeferringTouchStart = false;
        ASSERT_IMPLIES(event.type() == WebEventType::TouchStart, m_handlingPreventableTouchStartCount);
        if (event.type() == WebEventType::TouchStart && m_handlingPreventableTouchStartCount)
            didFinishDeferringTouchStart = !--m_handlingPreventableTouchStartCount;

        bool didFinishDeferringTouchMove = false;
        if (event.type() == WebEventType::TouchMove && m_touchMovePreventionState == EventPreventionState::Waiting) {
            m_touchMovePreventionState = handled ? EventPreventionState::Prevented : EventPreventionState::Allowed;
            didFinishDeferringTouchMove = true;
        }

        bool didFinishDeferringTouchEnd = false;
        ASSERT_IMPLIES(event.type() == WebEventType::TouchEnd, m_handlingPreventableTouchEndCount);
        if (event.type() == WebEventType::TouchEnd && m_handlingPreventableTouchEndCount)
            didFinishDeferringTouchEnd = !--m_handlingPreventableTouchEndCount;

        didReceiveEvent(event.type(), handled);
        if (!m_pageClient)
            return;

        protectedPageClient()->doneWithTouchEvent(event, handled);

        if (didFinishDeferringTouchStart)
            protectedPageClient()->doneDeferringTouchStart(handled);

        if (didFinishDeferringTouchMove)
            protectedPageClient()->doneDeferringTouchMove(handled);

        if (didFinishDeferringTouchEnd)
            protectedPageClient()->doneDeferringTouchEnd(handled);
    });
}

void WebPageProxy::handlePreventableTouchEvent(NativeWebTouchEvent& event)
{
    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

    TraceScope scope(SyncTouchEventStart, SyncTouchEventEnd);

    updateTouchEventTracking(event);

    auto handleAllTouchPointsReleased = WTF::makeScopeExit([&] {
        if (!event.allTouchPointsAreReleased())
            return;

        internals().touchEventTracking.reset();
        didReleaseAllTouchPoints();
    });

    bool isTouchStart = event.type() == WebEventType::TouchStart;
    bool isTouchMove = event.type() == WebEventType::TouchMove;
    bool isTouchEnd = event.type() == WebEventType::TouchEnd;

    if (isTouchStart)
        m_touchMovePreventionState = EventPreventionState::None;

    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking) {
        if (isTouchStart)
            protectedPageClient()->doneDeferringTouchStart(false);
        if (isTouchMove)
            protectedPageClient()->doneDeferringTouchMove(false);
        if (isTouchEnd)
            protectedPageClient()->doneDeferringTouchEnd(false);
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
            protectedPageClient()->doneDeferringTouchStart(false);
        if (isTouchMove)
            protectedPageClient()->doneDeferringTouchMove(false);
        if (isTouchEnd)
            protectedPageClient()->doneDeferringTouchEnd(false);
        return;
    }

    if (isTouchStart)
        ++m_handlingPreventableTouchStartCount;

    if (isTouchMove && m_touchMovePreventionState == EventPreventionState::None)
        m_touchMovePreventionState = EventPreventionState::Waiting;

    if (isTouchEnd)
        ++m_handlingPreventableTouchEndCount;

    sendPreventableTouchEvent(m_mainFrame->frameID(), event);
}

void WebPageProxy::resetPotentialTapSecurityOrigin()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ResetPotentialTapSecurityOrigin());
}

void WebPageProxy::sendUnpreventableTouchEvent(WebCore::FrameIdentifier frameID, const NativeWebTouchEvent& event)
{
    sendToProcessContainingFrame(frameID, Messages::EventDispatcher::TouchEvent(internals().webPageID, frameID, event), [this, protectedThis = Ref { *this }, event = event] (bool, std::optional<RemoteUserInputEventData> remoteTouchEventData) mutable {
        if (!remoteTouchEventData)
            return;
        event.setPosition(remoteTouchEventData->transformedPoint);
        sendUnpreventableTouchEvent(remoteTouchEventData->targetFrameID, event);
    });
}

void WebPageProxy::handleUnpreventableTouchEvent(const NativeWebTouchEvent& event)
{
    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking)
        return;

    sendUnpreventableTouchEvent(m_mainFrame->frameID(), event);

    if (event.allTouchPointsAreReleased()) {
        internals().touchEventTracking.reset();
        didReleaseAllTouchPoints();
    }
}

#elif ENABLE(TOUCH_EVENTS)
void WebPageProxy::touchEventHandlingCompleted(std::optional<WebEventType> eventType, bool handled)
{
    MESSAGE_CHECK(m_process, !internals().touchEventQueue.isEmpty());
    auto queuedEvents = internals().touchEventQueue.takeFirst();
    if (eventType)
        MESSAGE_CHECK(m_process, *eventType == queuedEvents.forwardedEvent.type());

    protectedPageClient()->doneWithTouchEvent(queuedEvents.forwardedEvent, handled);
    for (size_t i = 0; i < queuedEvents.deferredTouchEvents.size(); ++i) {
        bool isEventHandled = false;
        protectedPageClient()->doneWithTouchEvent(queuedEvents.deferredTouchEvents.at(i), isEventHandled);
    }
}

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
        internals().touchEventQueue.append(event);
        protectedProcess()->startResponsivenessTimer();
        sendWithAsyncReply(Messages::WebPage::TouchEvent(event), [this, protectedThis = Ref { *this }] (std::optional<WebEventType> eventType, bool handled) {
            if (!m_pageClient)
                return;
            if (!eventType) {
                touchEventHandlingCompleted(eventType, handled);
                return;
            }
            didReceiveEvent(*eventType, handled);
        });
    } else {
        if (internals().touchEventQueue.isEmpty()) {
            bool isEventHandled = false;
            protectedPageClient()->doneWithTouchEvent(event, isEventHandled);
        } else {
            // We attach the incoming events to the newest queued event so that all
            // the events are delivered in the correct order when the event is dequed.
            QueuedTouchEvents& lastEvent = internals().touchEventQueue.last();
            lastEvent.deferredTouchEvents.append(event);
        }
    }

    if (event.allTouchPointsAreReleased()) {
        internals().touchEventTracking.reset();
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

bool WebPageProxy::setIsNavigatingToAppBoundDomainAndCheckIfPermitted(bool isMainFrame, const URL& requestURL, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    if (isFullWebBrowserOrRunningTest()) {
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
    websiteDataStore().protectedNetworkProcess()->send(Messages::NetworkProcess::DisableServiceWorkerEntitlement(), 0);
#endif
}

void WebPageProxy::clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS) && !PLATFORM(MACCATALYST)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    sendWithAsyncReply(Messages::WebPage::ClearServiceWorkerEntitlementOverride(), [callbackAggregator] { });
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearServiceWorkerEntitlementOverride(), [callbackAggregator] { });
#else
    completionHandler();
#endif
}

void WebPageProxy::receivedNavigationActionPolicyDecision(WebProcessProxy& processNavigatingFrom, WebProcessProxy& processInitiatingNavigation, PolicyAction policyAction, API::Navigation* navigation, Ref<API::NavigationAction>&& navigationAction, ProcessSwapRequestedByClient processSwapRequestedByClient, WebFrameProxy& frame, const FrameInfoData& frameInfo, WasNavigationIntercepted wasNavigationIntercepted, std::optional<PolicyDecisionConsoleMessage>&& message, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "receivedNavigationActionPolicyDecision: frameID=%llu, isMainFrame=%d, navigationID=%llu, policyAction=%u", frame.frameID().object().toUInt64(), frame.isMainFrame(), navigation ? navigation->navigationID() : 0, (unsigned)policyAction);

    Ref websiteDataStore = m_websiteDataStore;
    if (RefPtr policies = navigation->websitePolicies()) {
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
        navigation->websitePolicies()->setContentExtensionEnablement({ ContentExtensionDefaultEnablement::Disabled, { } });
    }

#if ENABLE(DEVICE_ORIENTATION)
    if (navigation && (!navigation->websitePolicies() || navigation->websitePolicies()->deviceOrientationAndMotionAccessState() == WebCore::DeviceOrientationOrMotionPermissionState::Prompt)) {
        auto deviceOrientationPermission = websiteDataStore->deviceOrientationAndMotionAccessController().cachedDeviceOrientationPermission(SecurityOriginData::fromURLWithoutStrictOpaqueness(navigation->currentRequest().url()));
        if (deviceOrientationPermission != WebCore::DeviceOrientationOrMotionPermissionState::Prompt) {
            if (!navigation->websitePolicies())
                navigation->setWebsitePolicies(API::WebsitePolicies::create());
            navigation->protectedWebsitePolicies()->setDeviceOrientationAndMotionAccessState(deviceOrientationPermission);
        }
    }
#endif

#if PLATFORM(COCOA)
    static const bool forceDownloadFromDownloadAttribute = false;
#else
    static const bool forceDownloadFromDownloadAttribute = true;
#endif
    if (policyAction == PolicyAction::Use && navigation && (forceDownloadFromDownloadAttribute && navigation->shouldPerformDownload()))
        policyAction = PolicyAction::Download;

    if (policyAction != PolicyAction::Use
        || (!preferences().siteIsolationEnabled() && !frame.isMainFrame())
        || !navigation) {
        auto previousPendingNavigationID = internals().pageLoadState.pendingAPIRequest().navigationID;
        receivedPolicyDecision(policyAction, navigation, navigation->protectedWebsitePolicies().get(), WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(message), WTFMove(completionHandler));
#if HAVE(APP_SSO)
        if (policyAction == PolicyAction::Ignore && navigation && navigation->navigationID() == previousPendingNavigationID && wasNavigationIntercepted == WasNavigationIntercepted::Yes) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "receivedNavigationActionPolicyDecision: Failing navigation because decision was intercepted and policy action is Ignore.");
            auto error = WebKit::cancelledError(navigation->currentRequest().url());
            error.setType(WebCore::ResourceError::Type::Cancellation);
            m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation, error, nullptr);
            return;
        }
#else
    UNUSED_PARAM(wasNavigationIntercepted);
    UNUSED_VARIABLE(previousPendingNavigationID);
#endif

        return;
    }

    URL sourceURL { pageLoadState().url() };
    if (auto* provisionalPage = provisionalPageProxy()) {
        if (provisionalPage->navigationID() == navigation->navigationID())
            sourceURL = provisionalPage->provisionalURL();
    }

    m_isLockdownModeExplicitlySet = (navigation->websitePolicies() && navigation->websitePolicies()->isLockdownModeExplicitlySet()) || m_configuration->isLockdownModeExplicitlySet();
    auto lockdownMode = (navigation->websitePolicies() ? navigation->websitePolicies()->lockdownModeEnabled() : shouldEnableLockdownMode()) ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled;

    auto continueWithProcessForNavigation = [
        this,
        protectedThis = Ref { *this },
        policyAction,
        navigation = Ref { *navigation },
        navigationAction = WTFMove(navigationAction),
        processNavigatingFrom = Ref { processNavigatingFrom },
        completionHandler = WTFMove(completionHandler),
        processSwapRequestedByClient,
        frame = Ref { frame },
        processInitiatingNavigation = Ref { processInitiatingNavigation },
        message = WTFMove(message)
    ] (Ref<WebProcessProxy>&& processNavigatingTo, SuspendedPageProxy* destinationSuspendedPage, ASCIILiteral reason) mutable {
        // If the navigation has been destroyed, then no need to proceed.
        if (isClosed() || !navigationState().hasNavigation(navigation->navigationID())) {
            receivedPolicyDecision(policyAction, navigation.ptr(), navigation->protectedWebsitePolicies().get(), WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(message), WTFMove(completionHandler));
            return;
        }

        Ref pageClientProtector = pageClient();

        const bool navigationChangesFrameProcess = processNavigatingTo.ptr() != processNavigatingFrom.ptr();
        const bool loadContinuingInNonInitiatingProcess = processInitiatingNavigation.ptr() != processNavigatingTo.ptr();
        if (navigationChangesFrameProcess) {
            policyAction = PolicyAction::LoadWillContinueInAnotherProcess;
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "decidePolicyForNavigationAction, swapping process %i with process %i for navigation, reason=%" PUBLIC_LOG_STRING, processID(), processNavigatingTo->processID(), reason.characters());
            LOG(ProcessSwapping, "(ProcessSwapping) Switching from process %i to new process (%i) for navigation %" PRIu64 " '%s'", processID(), processNavigatingTo->processID(), navigation->navigationID(), navigation->loggingString());
        } else
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "decidePolicyForNavigationAction: keep using process %i for navigation, reason=%" PUBLIC_LOG_STRING, processID(), reason.characters());

        if (navigationChangesFrameProcess) {
            // Make sure the process to be used for the navigation does not get shutDown now due to destroying SuspendedPageProxy or ProvisionalPageProxy objects.
            auto preventNavigationProcessShutdown = processNavigatingTo->shutdownPreventingScope();

            ASSERT(!destinationSuspendedPage || navigation->targetItem());
            auto suspendedPage = destinationSuspendedPage ? backForwardCache().takeSuspendedPage(*navigation->targetItem()) : nullptr;

            // It is difficult to get history right if we have several WebPage objects inside a single WebProcess for the same WebPageProxy. As a result, if we make sure to
            // clear any SuspendedPageProxy for the current page that are backed by the destination process before we proceed with the navigation. This makes sure the WebPage
            // we are about to create in the destination process will be the only one associated with this WebPageProxy.
            if (!destinationSuspendedPage)
                backForwardCache().removeEntriesForPageAndProcess(*this, processNavigatingTo);

            ASSERT(suspendedPage.get() == destinationSuspendedPage);
            if (suspendedPage && suspendedPage->pageIsClosedOrClosing())
                suspendedPage = nullptr;

            continueNavigationInNewProcess(navigation, frame.get(), WTFMove(suspendedPage), WTFMove(processNavigatingTo), processSwapRequestedByClient, ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision, std::nullopt);

            receivedPolicyDecision(policyAction, navigation.ptr(), nullptr, WTFMove(navigationAction), WillContinueLoadInNewProcess::Yes, std::nullopt, WTFMove(message), WTFMove(completionHandler));
            return;
        }

        if (loadContinuingInNonInitiatingProcess) {
            // FIXME: Add more parameters as appropriate. <rdar://116200985>
            LoadParameters loadParameters;
            loadParameters.request = navigation->currentRequest();
            loadParameters.shouldTreatAsContinuingLoad = WebCore::ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;
            loadParameters.frameIdentifier = frame->frameID();
            processNavigatingTo->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)), webPageID());
        }

        RefPtr item = navigation->reloadItem() ? navigation->reloadItem() : navigation->targetItem();
        std::optional<SandboxExtension::Handle> optionalHandle;
        if (policyAction == PolicyAction::Use && item) {
            URL fullURL { item->url() };
            if (fullURL.protocolIsFile()) {
                SandboxExtension::Handle sandboxExtensionHandle;
                maybeInitializeSandboxExtensionHandle(processNavigatingTo.get(), fullURL, item->resourceDirectoryURL(), sandboxExtensionHandle);
                optionalHandle = WTFMove(sandboxExtensionHandle);
            }
        }

        receivedPolicyDecision(policyAction, navigation.ptr(), navigation->websitePolicies(), WTFMove(navigationAction), WillContinueLoadInNewProcess::No, WTFMove(optionalHandle), WTFMove(message), WTFMove(completionHandler));
    };

    process().processPool().processForNavigation(*this, frame, *navigation, processNavigatingFrom, sourceURL, processSwapRequestedByClient, lockdownMode, frameInfo, WTFMove(websiteDataStore), WTFMove(continueWithProcessForNavigation));
}

void WebPageProxy::receivedPolicyDecision(PolicyAction action, API::Navigation* navigation, RefPtr<API::WebsitePolicies>&& websitePolicies, Ref<API::NavigationAction>&& navigationAction, WillContinueLoadInNewProcess willContinueLoadInNewProcess, std::optional<SandboxExtension::Handle> sandboxExtensionHandle, std::optional<PolicyDecisionConsoleMessage>&& consoleMessage, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!hasRunningProcess())
        return completionHandler(PolicyDecision { });

    auto transaction = internals().pageLoadState.transaction();

    if (action == PolicyAction::Ignore && willContinueLoadInNewProcess == WillContinueLoadInNewProcess::No && navigation && navigation->navigationID() == internals().pageLoadState.pendingAPIRequest().navigationID)
        internals().pageLoadState.clearPendingAPIRequest(transaction);

    std::optional<DownloadID> downloadID;
    if (action == PolicyAction::Download) {
        // Create a download proxy.
        auto download = m_process->processPool().createDownloadProxy(m_websiteDataStore, navigationAction->request(), this, navigation ? navigation->originatingFrameInfo() : FrameInfoData { });
        download->setDidStartCallback([this, weakThis = WeakPtr { *this }, navigationAction = WTFMove(navigationAction)] (auto* downloadProxy) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !downloadProxy)
                return;
            m_navigationClient->navigationActionDidBecomeDownload(*this, navigationAction, *downloadProxy);
        });
        if (navigation) {
            download->setWasUserInitiated(navigation->wasUserInitiated());
            download->setRedirectChain(navigation->takeRedirectChain());
        }

        downloadID = download->downloadID();
    }
    
    std::optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePolicies)
        websitePoliciesData = websitePolicies->data();

    completionHandler(PolicyDecision { isNavigatingToAppBoundDomain(), action, navigation ? navigation->navigationID() : 0, downloadID, WTFMove(websitePoliciesData), WTFMove(sandboxExtensionHandle), WTFMove(consoleMessage) });
}

void WebPageProxy::receivedNavigationResponsePolicyDecision(WebCore::PolicyAction action, API::Navigation* navigation, const WebCore::ResourceRequest& request, Ref<API::NavigationResponse>&& navigationResponse, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!hasRunningProcess())
        return completionHandler(PolicyDecision { });

    auto transaction = internals().pageLoadState.transaction();

    if (action == PolicyAction::Ignore
        && navigation
        && navigation->navigationID() == internals().pageLoadState.pendingAPIRequest().navigationID)
        internals().pageLoadState.clearPendingAPIRequest(transaction);

    std::optional<DownloadID> downloadID;
    if (action == PolicyAction::Download) {
        auto download = m_process->processPool().createDownloadProxy(m_websiteDataStore, request, this, navigation ? navigation->originatingFrameInfo() : FrameInfoData { });
        download->setDidStartCallback([this, weakThis = WeakPtr { *this }, navigationResponse = WTFMove(navigationResponse)] (auto* downloadProxy) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !downloadProxy)
                return;
            if (!navigationResponse->downloadAttribute().isNull())
                downloadProxy->setSuggestedFilename(navigationResponse->downloadAttribute());
            m_navigationClient->navigationResponseDidBecomeDownload(*this, navigationResponse, *downloadProxy);
        });
        if (navigation) {
            download->setWasUserInitiated(navigation->wasUserInitiated());
            download->setRedirectChain(navigation->takeRedirectChain());
        }

        downloadID = download->downloadID();
    }

    completionHandler(PolicyDecision { isNavigatingToAppBoundDomain(), action, navigation ? navigation->navigationID() : 0, downloadID, { }, { } });
}

void WebPageProxy::commitProvisionalPage(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    ASSERT(m_provisionalPage);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "commitProvisionalPage: newPID=%i", m_provisionalPage->process().processID());

    RefPtr mainFrameInPreviousProcess = m_mainFrame;

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
    RefPtr navigation = navigationState().navigation(m_provisionalPage->navigationID());
    bool didSuspendPreviousPage = navigation && !m_provisionalPage->isProcessSwappingOnNavigationResponse() ? suspendCurrentPageIfPossible(*navigation, WTFMove(mainFrameInPreviousProcess), shouldDelayClosingUntilFirstLayerFlush) : false;
    protectedProcess()->removeWebPage(*this, m_websiteDataStore.ptr() == m_provisionalPage->process().websiteDataStore() ? WebProcessProxy::EndsUsingDataStore::No : WebProcessProxy::EndsUsingDataStore::Yes);

    // There is no way we'll be able to return to the page in the previous page so close it.
    if (!didSuspendPreviousPage && shouldClosePreviousPage())
        send(Messages::WebPage::Close());

    const auto oldWebPageID = internals().webPageID;
    swapToProvisionalPage(std::exchange(m_provisionalPage, nullptr));

    didCommitLoadForFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData);

    m_inspectorController->didCommitProvisionalPage(oldWebPageID, internals().webPageID);
}

bool WebPageProxy::shouldClosePreviousPage()
{
    auto* opener = openerFrame();
    return !(preferences().processSwapOnCrossSiteWindowOpenEnabled() || preferences().siteIsolationEnabled())
        || !opener
        || opener->process() != process();
}

void WebPageProxy::destroyProvisionalPage()
{
    m_provisionalPage = nullptr;
}

void WebPageProxy::continueNavigationInNewProcess(API::Navigation& navigation, WebFrameProxy& frame, std::unique_ptr<SuspendedPageProxy>&& suspendedPage, Ref<WebProcessProxy>&& newProcess, ProcessSwapRequestedByClient processSwapRequestedByClient, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "continueNavigationInNewProcess: newProcessPID=%i, hasSuspendedPage=%i", newProcess->processID(), !!suspendedPage);
    LOG(Loading, "Continuing navigation %" PRIu64 " '%s' in a new web process", navigation.navigationID(), navigation.loggingString());
    RELEASE_ASSERT(!newProcess->isInProcessCache());
    ASSERT(shouldTreatAsContinuingLoad != ShouldTreatAsContinuingLoad::No);
    navigation.setProcessID(newProcess->coreProcessIdentifier());

    if (navigation.currentRequest().url().protocolIsFile())
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

        // FIXME: Add more parameters as appropriate. <rdar://116200985>
        LoadParameters loadParameters;
        loadParameters.request = navigation.currentRequest();
        loadParameters.shouldTreatAsContinuingLoad = navigation.currentRequestIsRedirect() ? WebCore::ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted : WebCore::ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;
        loadParameters.frameIdentifier = frame.frameID();

        auto webPageID = webPageIDInProcessForDomain(RegistrableDomain(navigation.currentRequest().url()));
        frame.prepareForProvisionalNavigationInProcess(newProcess, navigation, [loadParameters = WTFMove(loadParameters), newProcess = newProcess.copyRef(), webPageID, preventProcessShutdownScope = newProcess->shutdownPreventingScope()] () mutable {
            newProcess->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)), webPageID);
        });
        return;
    }

    m_provisionalPage = makeUnique<ProvisionalPageProxy>(*this, WTFMove(newProcess), WTFMove(suspendedPage), navigation, isServerSideRedirect, navigation.currentRequest(), processSwapRequestedByClient, isProcessSwappingOnNavigationResponse, websitePolicies.get());

    // FIXME: This should be a CompletionHandler, but http/tests/inspector/target/provisional-load-cancels-previous-load.html doesn't call it.
    Function<void()> continuation = [this, protectedThis = Ref { *this }, navigation = Ref { navigation }, shouldTreatAsContinuingLoad, websitePolicies = WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume]() mutable {
        if (auto* item = navigation->targetItem()) {
            LOG(Loading, "WebPageProxy %p continueNavigationInNewProcess to back item URL %s", this, item->url().utf8().data());

            auto transaction = internals().pageLoadState.transaction();
            internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), item->url() });

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

    if (m_provisionalPage->needsCookieAccessAddedInNetworkProcess()) {
        continuation = [
            networkProcess = Ref { websiteDataStore().networkProcess() },
            continuation = WTFMove(continuation),
            navigationDomain = RegistrableDomain(navigation.currentRequest().url()),
            processIdentifier = m_provisionalPage->process().coreProcessIdentifier(),
            preventProcessShutdownScope = m_provisionalPage->process().shutdownPreventingScope()
        ] () mutable {
            networkProcess->sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(processIdentifier, navigationDomain, LoadedWebArchive::No), WTFMove(continuation));
        };
    }

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

void WebPageProxy::setUserAgent(String&& userAgent, IsCustomUserAgent isCustomUserAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = WTFMove(userAgent);

    // We update the service worker there at the moment to be sure we use values used by actual web pages.
    // FIXME: Refactor this when we have a better User-Agent story.
    process().protectedProcessPool()->updateRemoteWorkerUserAgent(m_userAgent);

    if (!hasRunningProcess())
        return;
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::SetUserAgent(m_userAgent), pageID);
        webProcess.send(Messages::WebPage::SetHasCustomUserAgent(isCustomUserAgent == IsCustomUserAgent::Yes), pageID);
    });
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

    setUserAgent(String { m_customUserAgent }, IsCustomUserAgent::Yes);
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

    String provisionalURLString = internals().pageLoadState.pendingAPIRequestURL();
    if (provisionalURLString.isEmpty())
        provisionalURLString = internals().pageLoadState.provisionalURL();

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

        Ref backForwardList = m_backForwardList;
        backForwardList->restoreFromState(WTFMove(sessionState.backForwardListState));
        // If the process is not launched yet, the session will be restored when sending the WebPageCreationParameters;
        if (hasRunningProcess())
            send(Messages::WebPage::RestoreSession(backForwardList->itemStates()));

        auto transaction = internals().pageLoadState.transaction();
        internals().pageLoadState.setCanGoBack(transaction, backForwardList->backItem());
        internals().pageLoadState.setCanGoForward(transaction, backForwardList->forwardItem());

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
            if (RefPtr item = m_backForwardList->currentItem())
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

double WebPageProxy::minPageZoomFactor() const
{
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginMinZoomFactor;

    return ViewGestureController::defaultMinMagnification;
}

double WebPageProxy::maxPageZoomFactor() const
{
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginMaxZoomFactor;

    return ViewGestureController::defaultMaxMagnification;
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

void WebPageProxy::windowScreenDidChange(PlatformDisplayID displayID)
{
#if HAVE(DISPLAY_LINK)
    if (hasRunningProcess() && m_displayID && m_registeredForFullSpeedUpdates)
        protectedProcess()->setDisplayLinkForDisplayWantsFullSpeedUpdates(*m_displayID, false);

    m_registeredForFullSpeedUpdates = false;
#endif

    m_displayID = displayID;
    if (m_drawingArea)
        m_drawingArea->windowScreenDidChange(displayID);

    if (!hasRunningProcess())
        return;

    std::optional<FramesPerSecond> nominalFramesPerSecond;
    if (m_drawingArea)
        nominalFramesPerSecond = m_drawingArea->displayNominalFramesPerSecond();

    send(Messages::EventDispatcher::PageScreenDidChange(internals().webPageID, displayID, nominalFramesPerSecond));
    send(Messages::WebPage::WindowScreenDidChange(displayID, nominalFramesPerSecond));
#if HAVE(DISPLAY_LINK)
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

#if PLATFORM(COCOA)
    // Also update screen properties which encodes invert colors.
    process().protectedProcessPool()->screenPropertiesChanged();
#endif
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
        internals().fixedLayoutSize = IntSize();

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetUseFixedLayout(fixed));
}

const IntSize& WebPageProxy::fixedLayoutSize() const
{
    return internals().fixedLayoutSize;
}

void WebPageProxy::fixedLayoutSizeDidChange(IntSize size)
{
    internals().fixedLayoutSize = size;
}

void WebPageProxy::setFixedLayoutSize(const IntSize& size)
{
    if (size == internals().fixedLayoutSize)
        return;

    internals().fixedLayoutSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetFixedLayoutSize(size));
}

FloatSize WebPageProxy::defaultUnobscuredSize() const
{
    return internals().defaultUnobscuredSize;
}

void WebPageProxy::setDefaultUnobscuredSize(const FloatSize& size)
{
    if (size == internals().defaultUnobscuredSize)
        return;

    internals().defaultUnobscuredSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetDefaultUnobscuredSize(internals().defaultUnobscuredSize));
}

FloatSize WebPageProxy::minimumUnobscuredSize() const
{
    return internals().minimumUnobscuredSize;
}

void WebPageProxy::setMinimumUnobscuredSize(const FloatSize& size)
{
    if (size == internals().minimumUnobscuredSize)
        return;

    internals().minimumUnobscuredSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMinimumUnobscuredSize(internals().minimumUnobscuredSize));
}

FloatSize WebPageProxy::maximumUnobscuredSize() const
{
    return internals().maximumUnobscuredSize;
}

void WebPageProxy::setMaximumUnobscuredSize(const FloatSize& size)
{
    if (size == internals().maximumUnobscuredSize)
        return;

    internals().maximumUnobscuredSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMaximumUnobscuredSize(internals().maximumUnobscuredSize));
}

std::optional<FloatRect> WebPageProxy::viewExposedRect() const
{
    return internals().viewExposedRect;
}

void WebPageProxy::setViewExposedRect(std::optional<WebCore::FloatRect> viewExposedRect)
{
    if (internals().viewExposedRect == viewExposedRect)
        return;

    internals().viewExposedRect = viewExposedRect;

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
    if (milestones == internals().observedLayoutMilestones)
        return;

    internals().observedLayoutMilestones = milestones;

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

RectEdges<bool> WebPageProxy::rubberBandableEdges() const
{
    return internals().rubberBandableEdges;
}

void WebPageProxy::setRubberBandableEdges(RectEdges<bool> edges)
{
    internals().rubberBandableEdges = edges;
}

RectEdges<bool> WebPageProxy::rubberBandableEdgesRespectingHistorySwipe() const
{
    auto rubberBandableEdges = this->rubberBandableEdges();
    if (shouldUseImplicitRubberBandControl()) {
        rubberBandableEdges.setLeft(!m_backForwardList->backItem());
        rubberBandableEdges.setRight(!m_backForwardList->forwardItem());
    }

    return rubberBandableEdges;
}

void WebPageProxy::setRubberBandsAtLeft(bool rubberBandsAtLeft)
{
    internals().rubberBandableEdges.setLeft(rubberBandsAtLeft);
}

void WebPageProxy::setRubberBandsAtRight(bool rubberBandsAtRight)
{
    internals().rubberBandableEdges.setRight(rubberBandsAtRight);
}

void WebPageProxy::setRubberBandsAtTop(bool rubberBandsAtTop)
{
    internals().rubberBandableEdges.setTop(rubberBandsAtTop);
}

void WebPageProxy::setRubberBandsAtBottom(bool rubberBandsAtBottom)
{
    internals().rubberBandableEdges.setBottom(rubberBandsAtBottom);
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

static bool scaleFactorIsValid(double scaleFactor)
{
    return scaleFactor > 0 && scaleFactor <= 100;
}

void WebPageProxy::pageScaleFactorDidChange(double scaleFactor)
{
    MESSAGE_CHECK(m_process, scaleFactorIsValid(scaleFactor));
    m_pageScaleFactor = scaleFactor;
}

void WebPageProxy::pluginScaleFactorDidChange(double pluginScaleFactor)
{
    MESSAGE_CHECK(m_process, scaleFactorIsValid(pluginScaleFactor));
    m_pluginScaleFactor = pluginScaleFactor;
}

void WebPageProxy::pluginZoomFactorDidChange(double pluginZoomFactor)
{
    MESSAGE_CHECK(m_process, scaleFactorIsValid(pluginZoomFactor));
    m_pluginZoomFactor = pluginZoomFactor;
}

void WebPageProxy::findStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    if (string.isEmpty()) {
        m_findMatchesClient->didFindStringMatches(this, string, Vector<Vector<WebCore::IntRect>> (), 0);
        return;
    }

    sendWithAsyncReply(Messages::WebPage::FindStringMatches(string, options, maxMatchCount), [this, protectedThis = Ref { *this }, string](Vector<Vector<WebCore::IntRect>> matches, int32_t firstIndexAfterSelection) {
        m_findMatchesClient->didFindStringMatches(this, string, matches, firstIndexAfterSelection);
    });
}

void WebPageProxy::findString(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(bool)>&& callbackFunction)
{
    Ref callbackAggregator = FindStringCallbackAggregator::create(*this, string, options, maxMatchCount, WTFMove(callbackFunction));
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::FindString(string, options | FindOptions::DoNotSetSelection, maxMatchCount), [callbackAggregator] (std::optional<FrameIdentifier> frameID, bool didWrap) {
            callbackAggregator->foundString(frameID, didWrap);
        }, pageID);
    });
}

void WebPageProxy::findString(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    findString(string, options, maxMatchCount, [](bool) { });
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

void WebPageProxy::requestRectForFoundTextRange(const WebFoundTextRange& range, CompletionHandler<void(WebCore::FloatRect)>&& callbackFunction)
{
    sendWithAsyncReply(Messages::WebPage::RequestRectForFoundTextRange(range), WTFMove(callbackFunction));
}

void WebPageProxy::addLayerForFindOverlay(CompletionHandler<void(WebCore::PlatformLayerIdentifier)>&& callbackFunction)
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

    sendWithAsyncReply(Messages::WebPage::CountStringMatches(string, options, maxMatchCount), [this, protectedThis = Ref { *this }, string](uint32_t matchCount) {
        m_findClient->didCountStringMatches(this, string, matchCount);
    });
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
    if (protectedPageClient()->canTakeForegroundAssertions())
        activity = m_process->throttler().foregroundActivity("WebPageProxy::runJavaScriptInFrameInScriptWorld"_s);
#endif

    auto completionHandler = [activity = WTFMove(activity), callbackFunction = WTFMove(callbackFunction)] (const IPC::DataReference& dataReference, std::optional<ExceptionDetails>&& details) mutable {
        if (details)
            return callbackFunction(makeUnexpected(WTFMove(*details)));
        if (dataReference.empty())
            return callbackFunction({ nullptr });
        callbackFunction({ API::SerializedScriptValue::createFromWireBytes(Vector(dataReference)).ptr() });
    };

    sendToProcessContainingFrame(frameID, Messages::WebPage::RunJavaScriptInFrameInScriptWorld(parameters, frameID, world.worldData()), WTFMove(completionHandler));
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

    sendWithAsyncReply(Messages::WebPage::GetContentsAsAttributedString(), [completionHandler = WTFMove(completionHandler), activity = process().throttler().foregroundActivity("getContentsAsAttributedString"_s)](const WebCore::AttributedString& string) mutable {
        completionHandler(string);
    });
}
#endif

void WebPageProxy::getAllFrames(CompletionHandler<void(FrameTreeNodeData&&)>&& completionHandler)
{
    RefPtr mainFrame = m_mainFrame;
    if (!mainFrame)
        return completionHandler({ });
    mainFrame->getFrameInfo(WTFMove(completionHandler));
}

void WebPageProxy::getAllFrameTrees(CompletionHandler<void(Vector<FrameTreeNodeData>&&)>&& completionHandler)
{
    class FrameTreeCallbackAggregator : public RefCounted<FrameTreeCallbackAggregator> {
    public:
        static Ref<FrameTreeCallbackAggregator> create(CompletionHandler<void(Vector<FrameTreeNodeData>&&)>&& completionHandler) { return adoptRef(*new FrameTreeCallbackAggregator(WTFMove(completionHandler))); }
        void addFrameTree(FrameTreeNodeData&& data) { m_data.append(WTFMove(data)); }
        ~FrameTreeCallbackAggregator()
        {
            m_completionHandler(WTFMove(m_data));
        }
    private:
        FrameTreeCallbackAggregator(CompletionHandler<void(Vector<FrameTreeNodeData>&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler)) { }
        CompletionHandler<void(Vector<FrameTreeNodeData>&&)> m_completionHandler;
        Vector<FrameTreeNodeData> m_data;
    };

    if (!m_mainFrame)
        return completionHandler({ });

    auto& internals = this->internals();
    Ref aggregator = FrameTreeCallbackAggregator::create(WTFMove(completionHandler));
    sendWithAsyncReply(Messages::WebPage::GetFrameTree(), [aggregator] (FrameTreeNodeData&& data) {
        aggregator->addFrameTree(WTFMove(data));
    });
    for (auto& remotePageProxy : internals.remotePageProxyState.domainToRemotePageProxyMap.values()) {
        Ref { *remotePageProxy }->sendWithAsyncReply(Messages::WebPage::GetFrameTree(), [aggregator] (FrameTreeNodeData&& data) {
            aggregator->addFrameTree(WTFMove(data));
        });
    }
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

void WebPageProxy::saveResources(WebFrameProxy* frame, const Vector<WebCore::MarkupExclusionRule>& markupExclusionRules, const String& directory, const String& suggestedMainResourceName, CompletionHandler<void(Expected<void, WebCore::ArchiveError>)>&& completionHandler)
{
    if (!frame)
        return completionHandler(makeUnexpected(WebCore::ArchiveError::InvalidFrame));

    if (directory.isEmpty())
        return completionHandler(makeUnexpected(WebCore::ArchiveError::InvalidFilePath));

    String mainResourceName = suggestedMainResourceName;
    if (mainResourceName.isEmpty()) {
        auto host = frame->url().host();
        mainResourceName = host.isEmpty() ? "main"_s : host.toString();
    }

    sendWithAsyncReply(Messages::WebPage::GetWebArchiveOfFrameWithFileName(frame->frameID(), markupExclusionRules, mainResourceName), [directory = directory, completionHandler = WTFMove(completionHandler)](const std::optional<IPC::SharedBufferReference>& data) mutable {
#if PLATFORM(COCOA)
        if (!data)
            return completionHandler(makeUnexpected(WebCore::ArchiveError::SerializationFailure));

        auto buffer = data->unsafeBuffer();
        if (!buffer)
            return completionHandler(makeUnexpected(WebCore::ArchiveError::SerializationFailure));

        sharedFileQueue().dispatch([directory = directory.isolatedCopy(), buffer, completionHandler = WTFMove(completionHandler)]() mutable {
            RefPtr archive = LegacyWebArchive::create(*buffer);
            auto result = archive->saveResourcesToDisk(directory);
            std::optional<WebCore::ArchiveError> error;
            if (!result)
                error = result.error();
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), error]() mutable {
                if (error)
                    return completionHandler(makeUnexpected(*error));

                completionHandler({ });
            });
        });
#else
        ASSERT_UNUSED(data, !data);
        completionHandler(makeUnexpected(WebCore::ArchiveError::NotImplemented));
#endif
    });
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

    auto aggregator = CallbackAggregator::create([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callback();
        protectedThis->callAfterNextPresentationUpdate(WTFMove(callback));
    });
    sendWithAsyncReply(Messages::WebPage::ForceRepaint(), [aggregator] { });
    for (auto& remotePageProxy : internals().remotePageProxyState.domainToRemotePageProxyMap.values())
        Ref { *remotePageProxy }->sendWithAsyncReply(Messages::WebPage::ForceRepaint(), [aggregator] { });
}

void WebPageProxy::preferencesDidChange()
{
    if (!hasRunningProcess())
        return;

    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();

    protectedPageClient()->preferencesDidChange();

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

    Ref protectedPageClient { pageClient() };

    MESSAGE_CHECK(m_process, !m_mainFrame);
    MESSAGE_CHECK(m_process, WebFrameProxy::canCreateFrame(frameID));

    Ref mainFrame = WebFrameProxy::create(*this, protectedProcess(), frameID);
    m_mainFrame = mainFrame.copyRef();

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
    internals().frameLoadStateObserver = WTF::makeUnique<WebPageProxyFrameLoadStateObserver>();
    m_mainFrame->frameLoadState().addObserver(*internals().frameLoadStateObserver);
#endif

    if (internals().serviceWorkerOpenWindowCompletionCallback) {
        mainFrame->setNavigationCallback([callback = WTFMove(internals().serviceWorkerOpenWindowCompletionCallback)](auto pageID, auto) mutable {
            callback(pageID);
        });
    }
}

void WebPageProxy::didCreateSubframe(WebCore::FrameIdentifier parentID, WebCore::FrameIdentifier newFrameID, const String& frameName)
{
    RefPtr parent = WebFrameProxy::webFrame(parentID);
    MESSAGE_CHECK(m_process, parent);
    parent->didCreateSubframe(newFrameID, frameName);
}

void WebPageProxy::didDestroyFrame(FrameIdentifier frameID)
{
#if ENABLE(WEB_AUTHN)
    protectedWebsiteDataStore()->authenticatorManager().cancelRequest(webPageID(), frameID);
#endif
    if (RefPtr automationSession = process().processPool().automationSession())
        automationSession->didDestroyFrame(frameID);
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        frame->disconnect();
}

void WebPageProxy::disconnectFramesFromPage()
{
    if (RefPtr mainFrame = std::exchange(m_mainFrame, nullptr))
        mainFrame->webProcessWillShutDown();
}

double WebPageProxy::estimatedProgress() const
{
    return internals().pageLoadState.estimatedProgress();
}

void WebPageProxy::didStartProgress()
{
    ASSERT(!m_isClosed);

    Ref protectedPageClient { pageClient() };

    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.didStartProgress(transaction);

    internals().pageLoadState.commitChanges();
}

void WebPageProxy::didChangeProgress(double value)
{
    Ref protectedPageClient { pageClient() };

    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.didChangeProgress(transaction, value);

    internals().pageLoadState.commitChanges();
}

void WebPageProxy::didFinishProgress()
{
    Ref protectedPageClient { pageClient() };

    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.didFinishProgress(transaction);

    internals().pageLoadState.commitChanges();
}

void WebPageProxy::setNetworkRequestsInProgress(bool networkRequestsInProgress)
{
    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.setNetworkRequestsInProgress(transaction, networkRequestsInProgress);
}

void WebPageProxy::updateRemoteFrameSize(WebCore::FrameIdentifier frameID, WebCore::IntSize size)
{
    if (!drawingArea())
        return;
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (RefPtr remotePageProxy = internals().remotePageProxyState.domainToRemotePageProxyMap.get(RegistrableDomain(frame->url())).get())
        remotePageProxy->send(Messages::WebPage::UpdateFrameSize(frameID, size));
}

void WebPageProxy::preconnectTo(ResourceRequest&& request)
{
    if (!m_websiteDataStore->configuration().allowsServerPreconnect())
        return;

    auto storedCredentialsPolicy = m_canUseCredentialStorage ? WebCore::StoredCredentialsPolicy::Use : WebCore::StoredCredentialsPolicy::DoNotUse;
    request.setIsAppInitiated(m_lastNavigationWasAppInitiated);
    if (request.httpUserAgent().isEmpty()) {
        if (auto userAgent = predictedUserAgentForRequest(request); !userAgent.isEmpty()) {
            // FIXME: we add user-agent to the preconnect request because otherwise the preconnect
            // gets thrown away by CFNetwork when using an HTTPS proxy (<rdar://problem/59434166>).
            request.setHTTPUserAgent(WTFMove(userAgent));
        }
    }
    request.setFirstPartyForCookies(request.url());
    request.setPriority(ResourceLoadPriority::VeryHigh);
    websiteDataStore().networkProcess().preconnectTo(sessionID(), identifier(), webPageID(), WTFMove(request), storedCredentialsPolicy, isNavigatingToAppBoundDomain());
}

void WebPageProxy::setCanUseCredentialStorage(bool canUseCredentialStorage)
{
    m_canUseCredentialStorage = canUseCredentialStorage;
    send(Messages::WebPage::SetCanUseCredentialStorage(canUseCredentialStorage));
}

void WebPageProxy::didDestroyNavigation(uint64_t navigationID)
{
    didDestroyNavigationShared(protectedProcess(), navigationID);
}

void WebPageProxy::didDestroyNavigationShared(Ref<WebProcessProxy>&& process, uint64_t navigationID)
{
    MESSAGE_CHECK(process, WebNavigationState::NavigationMap::isValidKey(navigationID));

    Ref protectedPageClient { pageClient() };

    m_navigationState->didDestroyNavigation(process->coreProcessIdentifier(), navigationID);
}

void WebPageProxy::didStartProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    didStartProvisionalLoadForFrameShared(protectedProcess(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData);
}

static bool shouldPrewarmWebProcessOnProvisionalLoad()
{
#if ENABLE(PREWARM_WEBPROCESS_ON_PROVISIONAL_LOAD)
    // With sufficient number of cores, page load times improve when prewarming a Web process when the provisional load starts.
    // Otherwise, a Web process will be prewarmed when the main frame load is finished.
    return WTF::numberOfProcessorCores() > 4;
#else
    return false;
#endif
}

void WebPageProxy::didStartProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK_URL(process, url);
    MESSAGE_CHECK_URL(process, unreachableURL);

    // If the page starts a new main frame provisional load, then cancel any pending one in a provisional process.
    if (frame->isMainFrame() && m_provisionalPage && m_provisionalPage->mainFrame() != frame) {
        m_provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    if (navigation && frame->isMainFrame() && navigation->currentRequest().url().isValid())
        MESSAGE_CHECK(process, navigation->currentRequest().url() == url);

    LOG(Loading, "WebPageProxy %" PRIu64 " in process pid %i didStartProvisionalLoadForFrame to frameID %" PRIu64 ", navigationID %" PRIu64 ", url %s", internals().identifier.toUInt64(), process->processID(), frameID.object().toUInt64(), navigationID, url.string().utf8().data());
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didStartProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.clearPendingAPIRequest(transaction);

    if (frame->isMainFrame()) {
        if (shouldPrewarmWebProcessOnProvisionalLoad())
            notifyProcessPoolToPrewarm();
        process->didStartProvisionalLoadForMainFrame(url);
        reportPageLoadResult(ResourceError { ResourceError::Type::Cancellation });
        internals().pageLoadStart = MonotonicTime::now();
        internals().pageLoadState.didStartProvisionalLoad(transaction, url.string(), unreachableURL.string());
        protectedPageClient->didStartProvisionalLoadForMainFrame();
        closeOverlayedViews();
    }

    frame->setUnreachableURL(unreachableURL);
    frame->didStartProvisionalLoad(url);

    internals().pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didStartProvisionalLoadForFrame(*this, *frame, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didStartProvisionalNavigation(*this, request, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
        m_navigationClient->didStartProvisionalLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
    }

#if ENABLE(WEB_AUTHN)
    protectedWebsiteDataStore()->authenticatorManager().cancelRequest(internals().webPageID, frameID);
#endif
}

void WebPageProxy::didExplicitOpenForFrame(FrameIdentifier frameID, URL&& url, String&& mimeType)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    Ref process = m_process;
    if (!checkURLReceivedFromCurrentOrPreviousWebProcess(process, url)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring WebPageProxy::DidExplicitOpenForFrame() IPC from the WebContent process because the file URL is outside the sandbox");
        return;
    }

    auto transaction = internals().pageLoadState.transaction();

    if (frame->isMainFrame())
        internals().pageLoadState.didExplicitOpen(transaction, url.string());

    frame->didExplicitOpen(WTFMove(url), WTFMove(mimeType));

    m_hasCommittedAnyProvisionalLoads = true;
    process->didCommitProvisionalLoad();
    if (!url.protocolIsAbout())
        process->didCommitMeaningfulProvisionalLoad();

    internals().pageLoadState.commitChanges();
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(FrameIdentifier frameID, uint64_t navigationID, ResourceRequest&& request, const UserData& userData)
{
    didReceiveServerRedirectForProvisionalLoadForFrameShared(protectedProcess(), frameID, navigationID, WTFMove(request), userData);
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, uint64_t navigationID, ResourceRequest&& request, const UserData& userData)
{
    LOG(Loading, "WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame to frameID %" PRIu64 ", navigationID %" PRIu64 ", url %s", frameID.object().toUInt64(), navigationID, request.url().string().utf8().data());

    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK_URL(process, request.url());

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didReceiveServerRedirectForProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr navigation = navigationID ? navigationState().navigation(navigationID) : nullptr;
    if (navigation)
        navigation->appendRedirectionURL(request.url());

    auto transaction = internals().pageLoadState.transaction();

    if (frame->isMainFrame()) {
        internals().pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, request.url().string());
        // If the main frame in a provisional page is getting a server-side redirect, make sure the
        // committed page's provisional URL is kept up-to-date too.
        if (frame != m_mainFrame && !m_mainFrame->frameLoadState().provisionalURL().isEmpty())
            m_mainFrame->didReceiveServerRedirectForProvisionalLoad(request.url());
    }
    frame->didReceiveServerRedirectForProvisionalLoad(request.url());

    internals().pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didReceiveServerRedirectForProvisionalLoadForFrame(*this, *frame, frame->isMainFrame() ? navigation.get() : nullptr, process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else if (frame->isMainFrame())
        m_navigationClient->didReceiveServerRedirectForProvisionalNavigation(*this, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::willPerformClientRedirectForFrame(FrameIdentifier frameID, const String& url, double delay, WebCore::LockBackForwardList)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "willPerformClientRedirectForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame())
        m_navigationClient->willPerformClientRedirect(*this, url, delay);
}

void WebPageProxy::didCancelClientRedirectForFrame(FrameIdentifier frameID)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didCancelClientRedirectForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame())
        m_navigationClient->didCancelClientRedirect(*this);
}

void WebPageProxy::didChangeProvisionalURLForFrame(FrameIdentifier frameID, uint64_t navigationID, URL&& url)
{
    didChangeProvisionalURLForFrameShared(protectedProcess(), frameID, navigationID, WTFMove(url));
}

void WebPageProxy::didChangeProvisionalURLForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, uint64_t, URL&& url)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->frameLoadState().state() == FrameLoadState::State::Provisional);
    MESSAGE_CHECK_URL(process, url);

    auto transaction = internals().pageLoadState.transaction();

    // Internally, we handle this the same way we handle a server redirect. There are no client callbacks
    // for this, but if this is the main frame, clients may observe a change to the page's URL.
    if (frame->isMainFrame())
        internals().pageLoadState.didReceiveServerRedirectForProvisionalLoad(transaction, url.string());

    frame->didReceiveServerRedirectForProvisionalLoad(url);
}

void WebPageProxy::didFailProvisionalLoadForFrame(FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const ResourceError& error, WillContinueLoading willContinueLoading, const UserData& userData, WillInternallyHandleFailure willInternallyHandleFailure)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK(m_process, frame);

    if (m_provisionalPage && frame->isMainFrame()) {
        // The load did not fail, it is merely happening in a new provisional process.
        return;
    }

    didFailProvisionalLoadForFrameShared(protectedProcess(), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, provisionalURL, error, willContinueLoading, userData, willInternallyHandleFailure);
}

void WebPageProxy::didFailProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, WebFrameProxy& frame, FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const ResourceError& error, WillContinueLoading willContinueLoading, const UserData& userData, WillInternallyHandleFailure willInternallyHandleFailure)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " in web process pid %i didFailProvisionalLoadForFrame to provisionalURL %s", internals().identifier.toUInt64(), process->processID(), provisionalURL.utf8().data());
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "didFailProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d, domain=%s, code=%d, isMainFrame=%d, willInternallyHandleFailure=%d", frame.frameID().object().toUInt64(), frame.isMainFrame(), error.domain().utf8().data(), error.errorCode(), frame.isMainFrame(), willInternallyHandleFailure == WillInternallyHandleFailure::Yes);

    MESSAGE_CHECK_URL(process, provisionalURL);
    MESSAGE_CHECK_URL(process, error.failingURL());

    Ref protectedPageClient { pageClient() };

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process->processPool().automationSession())
            automationSession->navigationOccurredForFrame(frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame.isMainFrame() && navigationID)
        navigation = navigationState().takeNavigation(navigationID);

    auto transaction = internals().pageLoadState.transaction();

    if (frame.isMainFrame()) {
        reportPageLoadResult(error);
        internals().pageLoadState.didFailProvisionalLoad(transaction);
        protectedPageClient->didFailProvisionalLoadForMainFrame();
        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        callLoadCompletionHandlersIfNecessary(false);
    }

    frame.didFailProvisionalLoad();

    internals().pageLoadState.commitChanges();

    ASSERT(!m_failingProvisionalLoadURL);
    m_failingProvisionalLoadURL = provisionalURL;

    if (willInternallyHandleFailure == WillInternallyHandleFailure::No) {
        if (m_loaderClient)
            m_loaderClient->didFailProvisionalLoadWithErrorForFrame(*this, frame, navigation.get(), error, process->transformHandlesToObjects(userData.protectedObject().get()).get());
        else {
            m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), error, process->transformHandlesToObjects(userData.protectedObject().get()).get());
            m_navigationClient->didFailProvisionalLoadWithErrorForFrame(*this, WTFMove(request), error, WTFMove(frameInfo));
        }
    }

    m_failingProvisionalLoadURL = { };

    // If the provisional page's load fails then we destroy the provisional page.
    if (m_provisionalPage && m_provisionalPage->mainFrame() == &frame && willContinueLoading == WillContinueLoading::No)
        m_provisionalPage = nullptr;

    if (frame.takeProvisionalFrame())
        frame.notifyParentOfLoadCompletion(process);
}

void WebPageProxy::didFinishServiceWorkerPageRegistration(bool success)
{
    ASSERT(m_isServiceWorkerPage);
    ASSERT(internals().serviceWorkerLaunchCompletionHandler);

    if (internals().serviceWorkerLaunchCompletionHandler)
        internals().serviceWorkerLaunchCompletionHandler(success);
}

void WebPageProxy::setServiceWorkerOpenWindowCompletionCallback(CompletionHandler<void(std::optional<PageIdentifier>)>&& completionCallback)
{
    ASSERT(!internals().serviceWorkerOpenWindowCompletionCallback);
    internals().serviceWorkerOpenWindowCompletionCallback = WTFMove(completionCallback);
}

void WebPageProxy::callLoadCompletionHandlersIfNecessary(bool success)
{
    if (m_isServiceWorkerPage && internals().serviceWorkerLaunchCompletionHandler && !success)
        internals().serviceWorkerLaunchCompletionHandler(false);

    if (internals().serviceWorkerOpenWindowCompletionCallback)
        internals().serviceWorkerOpenWindowCompletionCallback(success ? webPageID() : std::optional<WebCore::PageIdentifier> { });
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

void WebPageProxy::didCommitLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool wasPrivateRelayed, bool containsPluginDocument, HasInsecureContent hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " didCommitLoadForFrame in navigation %" PRIu64, internals().identifier.toUInt64(), navigationID);
    LOG(BackForward, "(Back/Forward) After load commit, back/forward list is now:%s", m_backForwardList->loggingString());

    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    if (frame->provisionalFrame()) {
        frame->commitProvisionalFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, wasPrivateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData);
        return;
    }

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didCommitLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID && (navigation = navigationState().navigation(navigationID))) {
        auto requesterOrigin = navigation->lastNavigationAction().requesterOrigin;
        auto currentRequest = navigation->currentRequest();
        auto navigationDataTransfer = checkIfNavigationContainsDataTransfer(requesterOrigin, currentRequest);
        if (!navigationDataTransfer.isEmpty()) {
            RegistrableDomain currentDomain { currentRequest.url() };
            URL requesterURL { requesterOrigin.toString() };
            if (!currentDomain.matches(requesterURL))
                m_websiteDataStore->protectedNetworkProcess()->didCommitCrossSiteLoadWithDataTransfer(m_websiteDataStore->sessionID(), RegistrableDomain { requesterURL }, currentDomain, navigationDataTransfer, internals().identifier, internals().webPageID);
        }
    }

    m_hasCommittedAnyProvisionalLoads = true;
    Ref process = m_process;
    process->didCommitProvisionalLoad();
    if (!request.url().protocolIsAbout())
        process->didCommitMeaningfulProvisionalLoad();

    if (frame->isMainFrame()) {
        m_hasUpdatedRenderingAfterDidCommitLoad = false;
#if PLATFORM(COCOA)
        if (is<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea))
            internals().firstLayerTreeTransactionIdAfterDidCommitLoad = downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea()).nextLayerTreeTransactionID();
#endif
        internals().pageAllowedToRunInTheBackgroundToken = nullptr;
    }

    auto transaction = internals().pageLoadState.transaction();
    bool markPageInsecure = hasInsecureContent == HasInsecureContent::Yes;

    if (frame->isMainFrame()) {
        internals().pageLoadState.didCommitLoad(transaction, certificateInfo, markPageInsecure, usedLegacyTLS, wasPrivateRelayed, frameInfo.securityOrigin);
        m_shouldSuppressNextAutomaticNavigationSnapshot = false;
    } else if (markPageInsecure)
        internals().pageLoadState.didDisplayOrRunInsecureContent(transaction);

#if USE(APPKIT)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME: A load going on in one frame shouldn't affect text editing in other frames on the page.
    protectedPageClient->resetSecureInputState();
#endif

    frame->didCommitLoad(mimeType, certificateInfo, containsPluginDocument);

    if (frame->isMainFrame()) {
        std::optional<WebCore::PrivateClickMeasurement> privateClickMeasurement;
        if (internals().privateClickMeasurement)
            privateClickMeasurement = internals().privateClickMeasurement->pcm;
        else if (navigation && navigation->privateClickMeasurement())
            privateClickMeasurement = navigation->privateClickMeasurement();
        if (privateClickMeasurement) {
            if (privateClickMeasurement->destinationSite().matches(frame->url()) || privateClickMeasurement->isSKAdNetworkAttribution())
                protectedWebsiteDataStore()->storePrivateClickMeasurement(*privateClickMeasurement);
        }
        if (m_screenOrientationManager)
            m_screenOrientationManager->unlockIfNecessary();
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
        protectedPageClient->didCommitLoadForMainFrame(mimeType, frameHasCustomContentProvider);
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
        protectedPageClient->setMouseEventPolicy(mouseEventPolicy);
#if ENABLE(PDF_HUD)
        protectedPageClient->removeAllPDFHUDs();
#endif
    }

    internals().pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didCommitLoadForFrame(*this, *frame, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didCommitNavigation(*this, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
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
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
        updateAllowedQueryParametersForAdvancedPrivacyProtectionsIfNeeded();
        if (navigation && navigation->websitePolicies())
            m_advancedPrivacyProtectionsPolicies = navigation->websitePolicies()->advancedPrivacyProtections();
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

#if ENABLE(EXTENSION_CAPABILITIES)
    if (frame->isMainFrame())
        updateMediaCapability();
#endif
}

void WebPageProxy::didFinishDocumentLoadForFrame(FrameIdentifier frameID, uint64_t navigationID, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didFinishDocumentLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->documentLoadedForFrame(*frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    if (frame->isMainFrame()) {
        m_navigationClient->didFinishDocumentLoad(*this, navigation.get(), protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
        internals().didFinishDocumentLoadForMainFrameTimestamp = MonotonicTime::now();
    }
}

void WebPageProxy::forEachWebContentProcess(Function<void(WebProcessProxy&, WebCore::PageIdentifier)>&& function)
{
    for (auto& pair : internals().remotePageProxyState.domainToRemotePageProxyMap) {
        auto& remotePageProxy = pair.value;
        if (!remotePageProxy) {
            ASSERT_NOT_REACHED();
            continue;
        }
        function(remotePageProxy->protectedProcess(), remotePageProxy->pageID());
    }
    function(protectedProcess(), webPageID());
}

void WebPageProxy::createRemoteSubframesInOtherProcesses(WebFrameProxy& newFrame, const String& frameName)
{
    if (!m_preferences->siteIsolationEnabled())
        return;

    RefPtr parent = newFrame.parentFrame();
    if (!parent) {
        ASSERT_NOT_REACHED();
        return;
    }

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.processID() == newFrame.process().processID())
            return;
        webProcess.send(Messages::WebPage::CreateRemoteSubframe(parent->frameID(), newFrame.frameID(), frameName), pageID);
    });
}

void WebPageProxy::broadcastFrameRemovalToOtherProcesses(IPC::Connection& connection, WebCore::FrameIdentifier frameID)
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::FrameWasRemovedInAnotherProcess(frameID), pageID);
    });
}

void WebPageProxy::broadcastMainFrameURLChangeToOtherProcesses(IPC::Connection& connection, const URL& newURL)
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::MainFrameURLChangedInAnotherProcess(newURL), pageID);
    });
}

void WebPageProxy::didFinishLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const UserData& userData)
{
    LOG(Loading, "WebPageProxy::didFinishLoadForFrame - WebPageProxy %p with navigationID %" PRIu64 " didFinishLoad", this, navigationID);

    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didFinishLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.object().toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID && navigationState().hasNavigation(navigationID))
        navigation = navigationState().navigation(navigationID);

    bool isMainFrame = frame->isMainFrame();
    if (!isMainFrame || !navigationID || navigation) {
        auto transaction = internals().pageLoadState.transaction();

        if (isMainFrame)
            internals().pageLoadState.didFinishLoad(transaction);

        if (m_controlledByAutomation) {
            if (RefPtr automationSession = process().processPool().automationSession())
                automationSession->navigationOccurredForFrame(*frame);
        }

        frame->didFinishLoad();

        frame->notifyParentOfLoadCompletion(frame->process());

        internals().pageLoadState.commitChanges();
    }

    if (m_loaderClient)
        m_loaderClient->didFinishLoadForFrame(*this, *frame, navigation.get(), protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didFinishNavigation(*this, navigation.get(), protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
        m_navigationClient->didFinishLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
    }

    if (isMainFrame) {
        reportPageLoadResult({ });
        protectedPageClient->didFinishNavigation(navigation.get());

        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        resetRecentCrashCountSoon();

        if (!shouldPrewarmWebProcessOnProvisionalLoad())
            notifyProcessPoolToPrewarm();

        callLoadCompletionHandlersIfNecessary(true);
    }

    m_isLoadingAlternateHTMLStringForFailingProvisionalLoad = false;
}

void WebPageProxy::didFailLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const ResourceError& error, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "didFailLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d, domain=%s, code=%d", frameID.object().toUInt64(), frame->isMainFrame(), error.domain().utf8().data(), error.errorCode());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    auto transaction = internals().pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();

    if (isMainFrame) {
        internals().pageLoadState.didFailLoad(transaction);
        internals().pageAllowedToRunInTheBackgroundToken = nullptr;
    }

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    frame->didFailLoad();

    internals().pageLoadState.commitChanges();
    if (m_loaderClient)
        m_loaderClient->didFailLoadWithErrorForFrame(*this, *frame, navigation.get(), error, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didFailNavigationWithError(*this, frameInfo, navigation.get(), error, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
        m_navigationClient->didFailLoadWithErrorForFrame(*this, WTFMove(request), error, WTFMove(frameInfo));
    }

    if (isMainFrame) {
        reportPageLoadResult(error);
        protectedPageClient->didFailNavigation(navigation.get());
        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        callLoadCompletionHandlersIfNecessary(false);
    }
}

void WebPageProxy::didSameDocumentNavigationForFrame(FrameIdentifier frameID, uint64_t navigationID, SameDocumentNavigationType navigationType, URL&& url, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK_URL(m_process, url);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrame: frameID=%" PRIu64 ", isMainFrame=%d, type=%u", frameID.object().toUInt64(), frame->isMainFrame(), enumToUnderlyingType(navigationType));

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = navigationState().navigation(navigationID);

    auto transaction = internals().pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        internals().pageLoadState.didSameDocumentNavigation(transaction, url.string());

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    internals().pageLoadState.clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(url);

    internals().pageLoadState.commitChanges();

    if (isMainFrame)
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());

    if (isMainFrame)
        protectedPageClient->didSameDocumentNavigationForMainFrame(navigationType);
}

void WebPageProxy::didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType navigationType, URL url, NavigationActionData&& navigationActionData, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    auto frameID = navigationActionData.frameInfo.frameID;
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK_URL(m_process, url);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrameViaJSHistoryAPI: frameID=%" PRIu64 ", isMainFrame=%d, type=%u", frameID.object().toUInt64(), frame->isMainFrame(), enumToUnderlyingType(navigationType));

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame()) {
        navigation = navigationState().createLoadRequestNavigation(process().coreProcessIdentifier(), ResourceRequest(url), m_backForwardList->currentItem());
        navigation->setLastNavigationAction(WTFMove(navigationActionData));
    }

    auto transaction = internals().pageLoadState.transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        internals().pageLoadState.didSameDocumentNavigation(transaction, url.string());

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    internals().pageLoadState.clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(url);

    internals().pageLoadState.commitChanges();

    if (isMainFrame)
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());

    if (isMainFrame)
        protectedPageClient->didSameDocumentNavigationForMainFrame(navigationType);

    if (navigation)
        navigationState().didDestroyNavigation(navigation->processID(), navigation->navigationID());
}

void WebPageProxy::didChangeMainDocument(FrameIdentifier frameID)
{
#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager) {
        m_userMediaPermissionRequestManager->resetAccess(frameID);

#if ENABLE(GPU_PROCESS)
        if (RefPtr gpuProcess = m_process->processPool().gpuProcess()) {
            if (RefPtr frame = WebFrameProxy::webFrame(frameID))
                gpuProcess->updateCaptureOrigin(SecurityOriginData::fromURLWithoutStrictOpaqueness(frame->url()), m_process->coreProcessIdentifier());
        }
#endif
    }

#else
    UNUSED_PARAM(frameID);
#endif
    
    m_isQuotaIncreaseDenied = false;

    m_speechRecognitionPermissionManager = nullptr;
}

#if ENABLE(GPU_PROCESS)
GPUProcessPreferencesForWebProcess WebPageProxy::preferencesForGPUProcess() const
{
    return configuration().preferencesForGPUProcess();
}
#endif

void WebPageProxy::viewIsBecomingVisible()
{
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "viewIsBecomingVisible:");
    protectedProcess()->markProcessAsRecentlyUsed();
#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager->viewIsBecomingVisible();
#endif
}

void WebPageProxy::processIsNoLongerAssociatedWithPage(WebProcessProxy& process)
{
    m_navigationState->clearNavigationsFromProcess(process.coreProcessIdentifier());
}

void WebPageProxy::didReceiveTitleForFrame(FrameIdentifier frameID, const String& title, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto transaction = internals().pageLoadState.transaction();

    if (frame->isMainFrame()) {
        internals().pageLoadState.setTitle(transaction, title);
        if (!isViewVisible() && !frame->title().isNull() && frame->title() != title) {
            WEBPAGEPROXY_RELEASE_LOG(ViewState, "didReceiveTitleForFrame: This page changes its title in the background and is allowed to run in the background");
            // This page updates its title in the background and is thus able to communicate with
            // the user while in the background. Allow it to run in the background.
            if (!internals().pageAllowedToRunInTheBackgroundToken)
                internals().pageAllowedToRunInTheBackgroundToken = process().throttler().pageAllowedToRunInTheBackgroundToken();
        }
    }

    frame->didChangeTitle(title);
    
    internals().pageLoadState.commitChanges();

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
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    if (m_loaderClient)
        m_loaderClient->didFirstVisuallyNonEmptyLayoutForFrame(*this, *frame, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());

    if (frame->isMainFrame())
        protectedPageClient->didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void WebPageProxy::didLayoutForCustomContentProvider()
{
    didReachLayoutMilestone({ WebCore::LayoutMilestone::DidFirstLayout, WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout, WebCore::LayoutMilestone::DidHitRelevantRepaintedObjectsAreaThreshold });
}

void WebPageProxy::didReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> layoutMilestones)
{
    Ref protectedPageClient { pageClient() };

    if (layoutMilestones.contains(WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout))
        protectedPageClient->clearSafeBrowsingWarningIfForMainFrameNavigation();
    
    if (m_loaderClient)
        m_loaderClient->didReachLayoutMilestone(*this, layoutMilestones);
    m_navigationClient->renderingProgressDidChange(*this, layoutMilestones);
}

void WebPageProxy::didDisplayInsecureContentForFrame(FrameIdentifier frameID, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.didDisplayOrRunInsecureContent(transaction);
    internals().pageLoadState.commitChanges();

    m_navigationClient->didDisplayInsecureContent(*this, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::didRunInsecureContentForFrame(FrameIdentifier frameID, const UserData& userData)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.didDisplayOrRunInsecureContent(transaction);
    internals().pageLoadState.commitChanges();

    m_navigationClient->didRunInsecureContent(*this, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::mainFramePluginHandlesPageScaleGestureDidChange(bool mainFramePluginHandlesPageScaleGesture, double minScale, double maxScale)
{
    m_mainFramePluginHandlesPageScaleGesture = mainFramePluginHandlesPageScaleGesture;
    if (m_mainFramePluginHandlesPageScaleGesture) {
        m_pluginMinZoomFactor = minScale;
        m_pluginMaxZoomFactor = maxScale;
    }
}

#if !PLATFORM(COCOA)
void WebPageProxy::beginSafeBrowsingCheck(const URL&, bool, WebFramePolicyListenerProxy& listener)
{
    listener.didReceiveSafeBrowsingResults({ });
}
#endif

void WebPageProxy::decidePolicyForNavigationActionAsync(NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    decidePolicyForNavigationActionAsyncShared(protectedProcess(), WTFMove(data), WTFMove(completionHandler));
}

void WebPageProxy::decidePolicyForNavigationActionAsyncShared(Ref<WebProcessProxy>&& process, NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(data.frameInfo.frameID);
    MESSAGE_CHECK(process, frame);

    auto url = data.request.url();
    decidePolicyForNavigationAction(process.copyRef(), *frame, WTFMove(data), [completionHandler = WTFMove(completionHandler), process, url = WTFMove(url)] (PolicyDecision&& policyDecision) mutable {
        if (policyDecision.policyAction == PolicyAction::Use && url.protocolIsFile())
            process->addPreviouslyApprovedFileURL(url);

        completionHandler(WTFMove(policyDecision));
    });
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

void WebPageProxy::decidePolicyForNavigationAction(Ref<WebProcessProxy>&& process, WebFrameProxy& frame, NavigationActionData&& navigationActionData, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    auto frameInfo = navigationActionData.frameInfo;
    auto navigationID = navigationActionData.navigationID;
    auto originatingFrameInfoData = navigationActionData.originatingFrameInfoData;
    auto originatingPageID = navigationActionData.originatingPageID;
    auto originalRequest = navigationActionData.originalRequest;
    auto request = navigationActionData.request;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: frameID=%llu, isMainFrame=%d, navigationID=%llu", frame.frameID().object().toUInt64(), frame.isMainFrame(), navigationID);

    LOG(Loading, "WebPageProxy::decidePolicyForNavigationAction - Original URL %s, current target URL %s", originalRequest.url().string().utf8().data(), request.url().string().utf8().data());

    Ref protectedPageClient { pageClient() };

    auto transaction = internals().pageLoadState.transaction();

    bool fromAPI = request.url() == internals().pageLoadState.pendingAPIRequestURL();
    if (navigationID && !fromAPI)
        internals().pageLoadState.clearPendingAPIRequest(transaction);

    if (!checkURLReceivedFromCurrentOrPreviousWebProcess(process, request.url())) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it is outside the sandbox");
        completionHandler(PolicyDecision { isNavigatingToAppBoundDomain() });
        return;
    }

    MESSAGE_CHECK_URL(process, originalRequest.url());

    RefPtr<API::Navigation> navigation;
    if (navigationID)
        navigation = m_navigationState->navigation(navigationID);

    // When process-swapping on a redirect, the navigationActionData / originatingFrameInfoData provided by the fresh new WebProcess are inaccurate since
    // the new process does not have sufficient information. To address the issue, we restore the information we stored on the NavigationAction during the original request
    // policy decision.
    if (!navigationActionData.redirectResponse.isNull() && navigation) {
        bool canHandleRequest = navigationActionData.canHandleRequest;
        auto redirectResponse = WTFMove(navigationActionData.redirectResponse);
        navigationActionData = navigation->lastNavigationAction();
        navigationActionData.redirectResponse = WTFMove(redirectResponse);
        navigationActionData.canHandleRequest = canHandleRequest;
        frameInfo.securityOrigin = navigation->destinationFrameSecurityOrigin();
    }

    if (!navigation) {
        if (auto targetBackForwardItemIdentifier = navigationActionData.targetBackForwardItemIdentifier) {
            if (RefPtr item = m_backForwardList->itemForID(*targetBackForwardItemIdentifier)) {
                RefPtr fromItem = navigationActionData.sourceBackForwardItemIdentifier ? m_backForwardList->itemForID(*navigationActionData.sourceBackForwardItemIdentifier) : nullptr;
                if (!fromItem)
                    fromItem = m_backForwardList->currentItem();
                navigation = m_navigationState->createBackForwardNavigation(process->coreProcessIdentifier(), item.releaseNonNull(), WTFMove(fromItem), FrameLoadType::IndexedBackForward);
            }
        }
        if (!navigation)
            navigation = m_navigationState->createLoadRequestNavigation(process->coreProcessIdentifier(), ResourceRequest(request), m_backForwardList->protectedCurrentItem());
    }

    navigationID = navigation->navigationID();

    // Make sure the provisional page always has the latest navigationID.
    if (m_provisionalPage && &m_provisionalPage->process() == process.ptr())
        m_provisionalPage->setNavigation(*navigation);

    navigation->setCurrentRequest(ResourceRequest(request), process->coreProcessIdentifier());
    navigation->setLastNavigationAction(navigationActionData);
    navigation->setOriginatingFrameInfo(originatingFrameInfoData);
    navigation->setDestinationFrameSecurityOrigin(frameInfo.securityOrigin);
    navigation->setOriginatorAdvancedPrivacyProtections(navigation->wasUserInitiated() ? navigationActionData.advancedPrivacyProtections : navigationActionData.originatorAdvancedPrivacyProtections);

    RefPtr mainFrameNavigation = frame.isMainFrame() ? navigation.get() : nullptr;
    RefPtr originatingFrame = originatingFrameInfoData.frameID ? WebFrameProxy::webFrame(originatingFrameInfoData.frameID) : nullptr;
    Ref destinationFrameInfo = API::FrameInfo::create(FrameInfoData { frameInfo }, this);
    RefPtr<API::FrameInfo> sourceFrameInfo;
    if (!fromAPI && originatingFrame == &frame)
        sourceFrameInfo = destinationFrameInfo.copyRef();
    else if (!fromAPI) {
        RefPtr originatingPage = originatingPageID ? process->webPage(*originatingPageID) : nullptr;
        sourceFrameInfo = API::FrameInfo::create(WTFMove(originatingFrameInfoData), WTFMove(originatingPage));
    }

    bool shouldOpenAppLinks = !m_shouldSuppressAppLinksInNextNavigationPolicyDecision
    && destinationFrameInfo->isMainFrame()
    && (m_mainFrame && m_mainFrame->url().host() != request.url().host())
    && navigationActionData.navigationType != WebCore::NavigationType::BackForward;

    RefPtr userInitiatedActivity = process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    Ref navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), destinationFrameInfo.ptr(), String(), ResourceRequest(request), originalRequest.url(), shouldOpenAppLinks, WTFMove(userInitiatedActivity), mainFrameNavigation.get());

#if ENABLE(CONTENT_FILTERING)
    if (frame.didHandleContentFilterUnblockNavigation(request)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it was handled by content filter");
        return receivedPolicyDecision(PolicyAction::Ignore, RefPtr { m_navigationState->navigation(navigationID) }.get(), nullptr, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, std::nullopt, WTFMove(completionHandler));
    }
#endif

    std::optional<PolicyDecisionConsoleMessage> message;

    // Other ports do not implement WebPage::platformCanHandleRequest().
#if PLATFORM(COCOA)
    // Sandboxed iframes should be allowed to open external apps via custom protocols unless explicitely allowed (https://html.spec.whatwg.org/#hand-off-to-external-software).
    bool canHandleRequest = navigationAction->data().canHandleRequest || m_urlSchemeHandlersByScheme.contains<StringViewHashTranslator>(request.url().protocol());
    if (!canHandleRequest && !destinationFrameInfo->isMainFrame() && !frameSandboxAllowsOpeningExternalCustomProtocols(navigationAction->data().effectiveSandboxFlags, !!navigationAction->data().userGestureTokenIdentifier)) {
        if (!sourceFrameInfo || !m_preferences->needsSiteSpecificQuirks() || !Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(request.url().protocol(), sourceFrameInfo->securityOrigin())) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe");
            PolicyDecisionConsoleMessage errorMessage {
                MessageLevel::Error,
                MessageSource::Security,
                "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe"_s
            };
            return receivedPolicyDecision(PolicyAction::Ignore, RefPtr { m_navigationState->navigation(navigationID) }.get(), nullptr, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(errorMessage), WTFMove(completionHandler));
        }
        message = PolicyDecisionConsoleMessage {
            MessageLevel::Warning,
            MessageSource::Security,
            "In the future, requests to navigate to a URL with custom protocol from a sandboxed iframe will be ignored"_s
        };
    }
#endif

    ShouldExpectSafeBrowsingResult shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::Yes;
    if (!m_preferences->safeBrowsingEnabled())
        shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::No;

    ShouldExpectAppBoundDomainResult shouldExpectAppBoundDomainResult = ShouldExpectAppBoundDomainResult::No;
#if ENABLE(APP_BOUND_DOMAINS)
    shouldExpectAppBoundDomainResult = ShouldExpectAppBoundDomainResult::Yes;
#endif

    auto shouldWaitForInitialLinkDecorationFilteringData = ShouldWaitForInitialLinkDecorationFilteringData::No;
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (LinkDecorationFilteringController::shared().cachedStrings().isEmpty())
        shouldWaitForInitialLinkDecorationFilteringData = ShouldWaitForInitialLinkDecorationFilteringData::Yes;
    else if (m_needsInitialLinkDecorationFilteringData)
        sendCachedLinkDecorationFilteringData();
#endif

    // FIXME: preferences().siteIsolationEnabled() here indicates there's an issue.
    // See rdar://118574473 and rdar://118575766 for the kinds of things that might
    // be broken with site isolation enabled.
    Ref frameProcessBeforeNavigation { frame.provisionalFrame() ? frame.provisionalFrame()->process() : preferences().siteIsolationEnabled()  && frame.isMainFrame() && m_provisionalPage ? m_provisionalPage->process() : frame.process() };

    Ref listener = frame.setUpPolicyListenerProxy([
        this,
        protectedThis = Ref { *this },
        frameProcessBeforeNavigation = WTFMove(frameProcessBeforeNavigation),
        processInitiatingNavigation = process,
        frame = Ref { frame },
        completionHandler = WTFMove(completionHandler),
        navigation,
        navigationAction,
        message = WTFMove(message),
        frameInfo,
        protectedPageClient
    ] (PolicyAction policyAction, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isAppBoundDomain, WasNavigationIntercepted wasNavigationIntercepted) mutable {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: listener called: frameID=%llu, isMainFrame=%d, navigationID=%llu, policyAction=%u, safeBrowsingWarning=%d, isAppBoundDomain=%d, wasNavigationIntercepted=%d", frame->frameID().object().toUInt64(), frame->isMainFrame(), navigation ? navigation->navigationID() : 0, (unsigned)policyAction, !!safeBrowsingWarning, !!isAppBoundDomain, wasNavigationIntercepted == WasNavigationIntercepted::Yes);

        navigation->setWebsitePolicies(WTFMove(policies));
        auto completionHandlerWrapper = [
            this,
            protectedThis,
            frameProcessBeforeNavigation = WTFMove(frameProcessBeforeNavigation),
            processInitiatingNavigation = WTFMove(processInitiatingNavigation),
            frame,
            frameInfo,
            completionHandler = WTFMove(completionHandler),
            navigation,
            navigationAction = WTFMove(navigationAction),
            wasNavigationIntercepted,
            processSwapRequestedByClient,
            message = WTFMove(message)
        ] (PolicyAction policyAction) mutable {
            if (frame->isMainFrame()) {
                if (!navigation->websitePolicies()) {
                    if (RefPtr defaultPolicies = m_configuration->defaultWebsitePolicies())
                        navigation->setWebsitePolicies(defaultPolicies->copy());
                }
                if (RefPtr policies = navigation->websitePolicies())
                    navigation->setEffectiveContentMode(effectiveContentModeAfterAdjustingPolicies(*policies, navigation->currentRequest()));
            }
            receivedNavigationActionPolicyDecision(frameProcessBeforeNavigation, processInitiatingNavigation, policyAction, navigation.get(), WTFMove(navigationAction), processSwapRequestedByClient, frame, frameInfo, wasNavigationIntercepted, WTFMove(message), WTFMove(completionHandler));
        };

#if ENABLE(APP_BOUND_DOMAINS)
        if (policyAction != PolicyAction::Ignore) {
            if (!setIsNavigatingToAppBoundDomainAndCheckIfPermitted(frame->isMainFrame(), navigation->currentRequest().url(), isAppBoundDomain)) {
                auto error = errorForUnpermittedAppBoundDomainNavigation(navigation->currentRequest().url());
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), error, nullptr);
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

        protectedPageClient->clearSafeBrowsingWarning();

        if (safeBrowsingWarning) {
            if (frame->isMainFrame() && safeBrowsingWarning->url().isValid()) {
                auto transaction = internals().pageLoadState.transaction();
                internals().pageLoadState.setPendingAPIRequest(transaction, { navigation->navigationID(), safeBrowsingWarning->url().string() });
                internals().pageLoadState.commitChanges();
            }

            auto transaction = internals().pageLoadState.transaction();
            internals().pageLoadState.setTitleFromSafeBrowsingWarning(transaction, safeBrowsingWarning->title());

            protectedPageClient->showSafeBrowsingWarning(*safeBrowsingWarning, [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandlerWrapper), policyAction, protectedPageClient] (auto&& result) mutable {

                auto transaction = internals().pageLoadState.transaction();
                internals().pageLoadState.setTitleFromSafeBrowsingWarning(transaction, { });

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
        completionHandlerWrapper(policyAction);

    }, shouldExpectSafeBrowsingResult, shouldExpectAppBoundDomainResult, shouldWaitForInitialLinkDecorationFilteringData);
    if (shouldExpectSafeBrowsingResult == ShouldExpectSafeBrowsingResult::Yes)
        beginSafeBrowsingCheck(request.url(), frame.isMainFrame(), listener);
    if (shouldWaitForInitialLinkDecorationFilteringData == ShouldWaitForInitialLinkDecorationFilteringData::Yes)
        waitForInitialLinkDecorationFilteringData(listener);
#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldSendSecurityOriginData = !frame.isMainFrame() && shouldTreatURLProtocolAsAppBound(request.url(), websiteDataStore().configuration().enableInAppBrowserPrivacyForTesting());
    auto host = shouldSendSecurityOriginData ? frameInfo.securityOrigin.host() : request.url().host();
    auto protocol = shouldSendSecurityOriginData ? frameInfo.securityOrigin.protocol() : request.url().protocol();
    protectedWebsiteDataStore()->beginAppBoundDomainCheck(host.toString(), protocol.toString(), listener);
#endif

    auto wasPotentiallyInitiatedByUser = navigation->isLoadedWithNavigationShared() || navigation->wasUserInitiated();
    if (!sessionID().isEphemeral())
        logFrameNavigation(frame, URL { internals().pageLoadState.url() }, request, navigationAction->data().redirectResponse.url(), wasPotentiallyInitiatedByUser);

    if (m_policyClient)
        m_policyClient->decidePolicyForNavigationAction(*this, &frame, WTFMove(navigationAction), originatingFrame.get(), originalRequest, WTFMove(request), WTFMove(listener));
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
    
    for (Ref webProcess : processPools[0]->processes()) {
        for (Ref page : webProcess->pages()) {
            if (page->sessionID().isEphemeral())
                continue;
            return page;
        }
    }
    return nullptr;
}

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

    websiteDataStore().protectedNetworkProcess()->send(Messages::NetworkProcess::LogFrameNavigation(m_websiteDataStore->sessionID(), RegistrableDomain { targetURL }, RegistrableDomain { pageURL }, RegistrableDomain { sourceURL }, isRedirect, frame.isMainFrame(), MonotonicTime::now() - internals().didFinishDocumentLoadForMainFrameTimestamp, wasPotentiallyInitiatedByUser), 0);
}

void WebPageProxy::decidePolicyForNavigationActionSync(NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    auto& frameInfo = data.frameInfo;
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame) {
        // This synchronous IPC message was processed before the asynchronous DidCreateMainFrame / DidCreateSubframe one so we do not know about this frameID yet.
        if (frameInfo.isMainFrame)
            didCreateMainFrame(frameInfo.frameID);
        else {
            MESSAGE_CHECK(m_process, frameInfo.parentFrameID);
            RefPtr parentFrame = WebFrameProxy::webFrame(*frameInfo.parentFrameID);
            MESSAGE_CHECK(m_process, parentFrame);
            parentFrame->didCreateSubframe(frameInfo.frameID, frameInfo.frameName);
        }
    }

    decidePolicyForNavigationActionSyncShared(frame->protectedProcess(), WTFMove(data), WTFMove(reply));
}

void WebPageProxy::decidePolicyForNavigationActionSyncShared(Ref<WebProcessProxy>&& process, NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(data.frameInfo.frameID);
    MESSAGE_CHECK(process, frame);

    class PolicyDecisionSender : public RefCounted<PolicyDecisionSender> {
    public:
        using SendFunction = CompletionHandler<void(PolicyDecision&&)>;
        static Ref<PolicyDecisionSender> create(SendFunction&& sendFunction) { return adoptRef(*new PolicyDecisionSender(WTFMove(sendFunction))); }

        void send(PolicyDecision&& policyDecision)
        {
            if (m_sendFunction)
                m_sendFunction(WTFMove(policyDecision));
        }
    private:
        PolicyDecisionSender(SendFunction&& sendFunction)
            : m_sendFunction(WTFMove(sendFunction)) { }
        SendFunction m_sendFunction;
    };
    auto sender = PolicyDecisionSender::create(WTFMove(reply));

    auto navigationID = data.navigationID;
    decidePolicyForNavigationAction(WTFMove(process), *frame, WTFMove(data), [sender] (auto&& policyDecision) {
        sender->send(WTFMove(policyDecision));
    });

    // If the client did not respond synchronously, proceed with the load.
    sender->send(PolicyDecision { isNavigatingToAppBoundDomain(), PolicyAction::Use, navigationID });
}

void WebPageProxy::decidePolicyForNewWindowAction(NavigationActionData&& navigationActionData, const String& frameName, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    Ref protectedPageClient { pageClient() };
    auto frameInfo = navigationActionData.frameInfo;
    auto request = navigationActionData.request;

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK_URL(m_process, request.url());

    RefPtr<API::FrameInfo> sourceFrameInfo;
    if (frame)
        sourceFrameInfo = API::FrameInfo::create(WTFMove(frameInfo), this);

    auto userInitiatedActivity = m_process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    bool shouldOpenAppLinks = m_mainFrame && m_mainFrame->url().host() != request.url().host();
    auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), nullptr, frameName, ResourceRequest(request), URL { }, shouldOpenAppLinks, WTFMove(userInitiatedActivity));
    
    Ref listener = frame->setUpPolicyListenerProxy([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), navigationAction] (PolicyAction policyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted) mutable {
        // FIXME: Assert the API::WebsitePolicies* is nullptr here once clients of WKFramePolicyListenerUseWithPolicies go away.
        RELEASE_ASSERT(processSwapRequestedByClient == ProcessSwapRequestedByClient::No);
        ASSERT_UNUSED(safeBrowsingWarning, !safeBrowsingWarning);

        receivedPolicyDecision(policyAction, nullptr, nullptr, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, std::nullopt, WTFMove(completionHandler));
    }, ShouldExpectSafeBrowsingResult::No, ShouldExpectAppBoundDomainResult::No, ShouldWaitForInitialLinkDecorationFilteringData::No);

    if (m_policyClient)
        m_policyClient->decidePolicyForNewWindowAction(*this, *frame, navigationAction.get(), request, frameName, WTFMove(listener));
    else
        m_navigationClient->decidePolicyForNavigationAction(*this, navigationAction.get(), WTFMove(listener));
}

void WebPageProxy::decidePolicyForResponse(IPC::Connection& connection, FrameInfoData&& frameInfo, uint64_t navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK_BASE(frame, &connection);
    auto* provisionalFrame = frame->provisionalFrame();
    decidePolicyForResponseShared(provisionalFrame ? provisionalFrame->protectedProcess() : frame->protectedProcess(), internals().webPageID, WTFMove(frameInfo), navigationID, response, request, canShowMIMEType, downloadAttribute, WTFMove(completionHandler));
}

void WebPageProxy::decidePolicyForResponseShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, FrameInfoData&& frameInfo, uint64_t navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK_URL(process, request.url());
    MESSAGE_CHECK_URL(process, response.url());
    RefPtr navigation = navigationID ? m_navigationState->navigation(navigationID) : nullptr;
    Ref navigationResponse = API::NavigationResponse::create(API::FrameInfo::create(WTFMove(frameInfo), this).get(), request, response, canShowMIMEType, downloadAttribute);

    Ref listener = frame->setUpPolicyListenerProxy([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), navigation = WTFMove(navigation), process, navigationResponse, request] (PolicyAction policyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted) mutable {
        // FIXME: Assert the API::WebsitePolicies* is nullptr here once clients of WKFramePolicyListenerUseWithPolicies go away.
        RELEASE_ASSERT(processSwapRequestedByClient == ProcessSwapRequestedByClient::No);
        ASSERT_UNUSED(safeBrowsingWarning, !safeBrowsingWarning);

        bool shouldForceDownload = [&] {
            if (policyAction != PolicyAction::Use || process->lockdownMode() != WebProcessProxy::LockdownMode::Enabled)
                return false;
            if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(navigationResponse->response().mimeType()))
                return true;
            if (MIMETypeRegistry::isSupportedModelMIMEType(navigationResponse->response().mimeType()))
                return true;
#if USE(QUICK_LOOK)
            if (PreviewConverter::supportsMIMEType(navigationResponse->response().mimeType()))
                return true;
#endif
            return false;
        }();
        if (shouldForceDownload)
            policyAction = PolicyAction::Download;

        receivedNavigationResponsePolicyDecision(policyAction, navigation.get(), request, WTFMove(navigationResponse), WTFMove(completionHandler));
    }, ShouldExpectSafeBrowsingResult::No, ShouldExpectAppBoundDomainResult::No, ShouldWaitForInitialLinkDecorationFilteringData::No);

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
    auto lockdownMode = m_provisionalPage ? m_provisionalPage->process().lockdownMode() : m_process->lockdownMode();
    if (browsingContextGroupSwitchDecision == BrowsingContextGroupSwitchDecision::NewIsolatedGroup)
        processForNavigation = m_process->protectedProcessPool()->createNewWebProcess(protectedWebsiteDataStore().ptr(), lockdownMode, WebProcessProxy::IsPrewarmed::No, CrossOriginMode::Isolated);
    else
        processForNavigation = m_process->protectedProcessPool()->processForRegistrableDomain(protectedWebsiteDataStore(), responseDomain, lockdownMode, protectedConfiguration());

    auto processIdentifier = processForNavigation->coreProcessIdentifier();
    auto preventProcessShutdownScope = processForNavigation->shutdownPreventingScope();

    auto domain = RegistrableDomain { navigation->currentRequest().url() };
    processForNavigation->addAllowedFirstPartyForCookies(domain);
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(processIdentifier, domain, LoadedWebArchive::No), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), processForNavigation = WTFMove(processForNavigation), preventProcessShutdownScope = WTFMove(preventProcessShutdownScope), existingNetworkResourceLoadIdentifierToResume, navigationID] () mutable {
        RefPtr navigation = m_navigationState->navigation(navigationID);
        if (!navigation || !m_mainFrame)
            return completionHandler(false);

        // Tell committed process to stop loading since we're going to do the provisional load in a provisional page now.
        if (!m_provisionalPage)
            send(Messages::WebPage::StopLoadingDueToProcessSwap());
        continueNavigationInNewProcess(*navigation, *m_mainFrame, nullptr, processForNavigation.releaseNonNull(), ProcessSwapRequestedByClient::No, ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted, existingNetworkResourceLoadIdentifierToResume);
        completionHandler(true);
    });
}

// FormClient

void WebPageProxy::willSubmitForm(FrameIdentifier frameID, FrameIdentifier sourceFrameID, const Vector<std::pair<String, String>>& textFieldValues, const UserData& userData, CompletionHandler<void()>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    RefPtr sourceFrame = WebFrameProxy::webFrame(sourceFrameID);
    MESSAGE_CHECK(m_process, sourceFrame);

    for (auto& pair : textFieldValues)
        MESSAGE_CHECK(m_process, API::Dictionary::MapType::isValidKey(pair.first));

    m_formClient->willSubmitForm(*this, *frame, *sourceFrame, textFieldValues, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get(), WTFMove(completionHandler));
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebPageProxy::contentRuleListNotification(URL&& url, ContentRuleListResults&& results)
{
    m_navigationClient->contentRuleListNotification(*this, WTFMove(url), WTFMove(results));
}
#endif

void WebPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    didNavigateWithNavigationDataShared(protectedProcess(), store, frameID);
}

void WebPageProxy::didNavigateWithNavigationDataShared(Ref<WebProcessProxy>&& process, const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didNavigateWithNavigationDataShared:");

    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->page() == this);

    if (frame->isMainFrame())
        m_historyClient->didNavigateWithNavigationData(*this, store);
    process->processPool().historyClient().didNavigateWithNavigationData(process->protectedProcessPool(), *this, store, *frame);
}

void WebPageProxy::didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    didPerformClientRedirectShared(protectedProcess(), sourceURLString, destinationURLString, frameID);
}

void WebPageProxy::didPerformClientRedirectShared(Ref<WebProcessProxy>&& process, const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    Ref protectedPageClient { pageClient() };

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
    Ref processPool = process->processPool();
    processPool->historyClient().didPerformClientRedirect(processPool, *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    didPerformServerRedirectShared(protectedProcess(), sourceURLString, destinationURLString, frameID);
}

void WebPageProxy::didPerformServerRedirectShared(Ref<WebProcessProxy>&& process, const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didPerformServerRedirect:");

    Ref protectedPageClient { pageClient() };

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;
    
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(process, frame);
    MESSAGE_CHECK(process, frame->page() == this);

    MESSAGE_CHECK_URL(process, sourceURLString);
    MESSAGE_CHECK_URL(process, destinationURLString);

    if (frame->isMainFrame())
        m_historyClient->didPerformServerRedirect(*this, sourceURLString, destinationURLString);
    process->processPool().historyClient().didPerformServerRedirect(process->protectedProcessPool(), *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didUpdateHistoryTitle(const String& title, const String& url, FrameIdentifier frameID)
{
    Ref protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    MESSAGE_CHECK(m_process, frame->page() == this);

    MESSAGE_CHECK_URL(m_process, url);

    if (frame->isMainFrame())
        m_historyClient->didUpdateHistoryTitle(*this, title, url);
    Ref processPool = process().processPool();
    processPool->historyClient().didUpdateHistoryTitle(processPool, *this, title, url, *frame);
}

// UIClient

using NewPageCallback = CompletionHandler<void(RefPtr<WebPageProxy>&&)>;
using UIClientCallback = Function<void(Ref<API::NavigationAction>&&, NewPageCallback&&)>;
static void trySOAuthorization(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
#if HAVE(APP_SSO)
    if (page.preferences().isExtensibleSSOEnabled()) {
        page.protectedWebsiteDataStore()->soAuthorizationCoordinator(page).tryAuthorize(WTFMove(navigationAction), page, WTFMove(newPageCallback), WTFMove(uiClientCallback));
        return;
    }
#endif
    uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
}

void WebPageProxy::createNewPage(WindowFeatures&& windowFeatures, NavigationActionData&& navigationActionData, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebKit::WebPageCreationParameters>)>&& reply)
{
    auto& originatingFrameInfoData = navigationActionData.originatingFrameInfoData;
    auto originatingPageID = navigationActionData.originatingPageID;
    auto& request = navigationActionData.request;
    MESSAGE_CHECK(m_process, originatingPageID);
    MESSAGE_CHECK(m_process, originatingFrameInfoData.frameID);
    MESSAGE_CHECK(m_process, WebFrameProxy::webFrame(originatingFrameInfoData.frameID));

    auto originatingPage = protectedProcess()->webPage(*originatingPageID);
    auto originatingFrameInfo = API::FrameInfo::create(WTFMove(originatingFrameInfoData), WTFMove(originatingPage));
    auto mainFrameURL = m_mainFrame ? m_mainFrame->url() : URL();

    std::optional<bool> openerAppInitiatedState;
    if (RefPtr page = originatingFrameInfo->page())
        openerAppInitiatedState = page->lastNavigationWasAppInitiated();

    auto completionHandler = [this, protectedThis = Ref { *this }, mainFrameURL, request, reply = WTFMove(reply), privateClickMeasurement = navigationActionData.privateClickMeasurement, openerAppInitiatedState = WTFMove(openerAppInitiatedState), openerFrameID = originatingFrameInfoData.frameID] (RefPtr<WebPageProxy> newPage) mutable {
        if (!newPage) {
            reply(std::nullopt, std::nullopt);
            return;
        }

        newPage->setOpenedByDOM();

        if (openerAppInitiatedState)
            newPage->m_lastNavigationWasAppInitiated = *openerAppInitiatedState;
        if (RefPtr openerFrame = WebFrameProxy::webFrame(openerFrameID)) {
            newPage->m_openerFrame = openerFrame;
            if (RefPtr page = openerFrame->page())
                page->addOpenedPage(*newPage);
        }

        reply(newPage->webPageID(), newPage->creationParameters(protectedProcess(), *newPage->drawingArea()));

        newPage->m_shouldSuppressAppLinksInNextNavigationPolicyDecision = mainFrameURL.host() == request.url().host();

        if (privateClickMeasurement)
            newPage->internals().privateClickMeasurement = { { WTFMove(*privateClickMeasurement), { }, { } } };
#if HAVE(APP_SSO)
        newPage->m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = true;
#endif
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
        newPage->m_needsInitialLinkDecorationFilteringData = LinkDecorationFilteringController::shared().cachedStrings().isEmpty();
        newPage->m_shouldUpdateAllowedQueryParametersForAdvancedPrivacyProtections = cachedAllowedQueryParametersForAdvancedPrivacyProtections().isEmpty();
#endif
    };

    Ref process = m_process;
    RefPtr userInitiatedActivity = process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);

    if (userInitiatedActivity && protectedPreferences()->verifyWindowOpenUserGestureFromUIProcess())
        process->consumeIfNotVerifiablyFromUIProcess(webPageID(), *userInitiatedActivity, navigationActionData.userGestureAuthorizationToken);

    bool shouldOpenAppLinks = originatingFrameInfo->request().url().host() != request.url().host();
    Ref navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), originatingFrameInfo.ptr(), nullptr, String(), WTFMove(request), URL(), shouldOpenAppLinks, WTFMove(userInitiatedActivity));

    trySOAuthorization(WTFMove(navigationAction), *this, WTFMove(completionHandler), [this, protectedThis = Ref { *this }, windowFeatures = WTFMove(windowFeatures)] (Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler) mutable {
        m_uiClient->createNewPage(*this, WTFMove(windowFeatures), WTFMove(navigationAction), WTFMove(completionHandler));
    });
}

void WebPageProxy::showPage()
{
    m_uiClient->showPage(this);
}

bool WebPageProxy::hasOpenedPage() const
{
    return !internals().m_openedPages.isEmptyIgnoringNullReferences();
}

void WebPageProxy::addOpenedPage(WebPageProxy& page)
{
    internals().m_openedPages.add(page);
}

void WebPageProxy::exitFullscreenImmediately()
{
#if ENABLE(FULLSCREEN_API)
    if (fullScreenManager())
        fullScreenManager()->close();
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr manager = videoPresentationManager())
        manager->requestHideAndExitFullscreen();
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

    internals().currentFullscreenVideoSessionIdentifier = identifier;
    updateFullscreenVideoTextRecognition();
}

void WebPageProxy::didExitFullscreen(PlaybackSessionContextIdentifier identifier)
{
    if (m_screenOrientationManager)
        m_screenOrientationManager->unlockIfNecessary();

    m_uiClient->didExitFullscreen(this);

    if (internals().currentFullscreenVideoSessionIdentifier == identifier) {
        internals().currentFullscreenVideoSessionIdentifier = std::nullopt;
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
    protectedPageClient()->clearAllEditCommands();
    m_uiClient->close(this);
}

void WebPageProxy::runModalJavaScriptDialog(RefPtr<WebFrameProxy>&& frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void(WebPageProxy&, WebFrameProxy* frame, FrameInfoData&& frameInfo, const String& message, CompletionHandler<void()>&&)>&& runDialogCallback)
{
    protectedPageClient()->runModalJavaScriptDialog([weakThis = WeakPtr { *this }, frameInfo = WTFMove(frameInfo), frame = WTFMove(frame), message, runDialogCallback = WTFMove(runDialogCallback)]() mutable {
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
    protectedProcess()->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
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
    protectedProcess()->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
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
    protectedProcess()->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
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

void WebPageProxy::mouseDidMoveOverElement(WebHitTestResultData&& hitTestResultData, OptionSet<WebEventModifier> modifiers, UserData&& userData)
{
#if PLATFORM(MAC)
    m_lastMouseMoveHitTestResult = API::HitTestResult::create(hitTestResultData, this);
#endif
    m_uiClient->mouseDidMoveOverElement(*this, hitTestResultData, modifiers, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
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
    m_uiClient->setWindowFrame(*this, protectedPageClient()->convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(CompletionHandler<void(const FloatRect&)>&& reply)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, reply = WTFMove(reply)] (FloatRect frame) mutable {
        reply(protectedPageClient()->convertToUserSpace(frame));
    });
}

void WebPageProxy::getWindowFrameWithCallback(Function<void(FloatRect)>&& completionHandler)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (FloatRect frame) {
        completionHandler(protectedPageClient()->convertToUserSpace(frame));
    });
}

void WebPageProxy::screenToRootView(const IntPoint& screenPoint, CompletionHandler<void(const IntPoint&)>&& reply)
{
    reply(protectedPageClient()->screenToRootView(screenPoint));
}

void WebPageProxy::rootViewToScreen(const IntRect& viewRect, CompletionHandler<void(const IntRect&)>&& reply)
{
    reply(protectedPageClient()->rootViewToScreen(viewRect));
}

IntRect WebPageProxy::syncRootViewToScreen(const IntRect& viewRect)
{
    return protectedPageClient()->rootViewToScreen(viewRect);
}

void WebPageProxy::accessibilityScreenToRootView(const IntPoint& screenPoint, CompletionHandler<void(IntPoint)>&& completionHandler)
{
    completionHandler(protectedPageClient()->accessibilityScreenToRootView(screenPoint));
}

void WebPageProxy::rootViewToAccessibilityScreen(const IntRect& viewRect, CompletionHandler<void(IntRect)>&& completionHandler)
{
    completionHandler(protectedPageClient()->rootViewToAccessibilityScreen(viewRect));
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
    protectedProcess()->stopResponsivenessTimer();
    bool shouldResumeTimerAfterPrompt = internals().tryCloseTimeoutTimer.isActive();
    internals().tryCloseTimeoutTimer.stop();
    m_uiClient->runBeforeUnloadConfirmPanel(*this, message, frame.get(), WTFMove(frameInfo),
        [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(reply), shouldResumeTimerAfterPrompt](bool shouldClose) mutable {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis && shouldResumeTimerAfterPrompt)
                internals().tryCloseTimeoutTimer.startOneShot(tryCloseTimeoutDelay);
            completionHandler(shouldClose);
    });
}

void WebPageProxy::didChangeViewportProperties(const ViewportAttributes& attr)
{
    protectedPageClient()->didChangeViewportProperties(attr);
}

void WebPageProxy::pageDidScroll(const WebCore::IntPoint& scrollPosition)
{
    m_uiClient->pageDidScroll(this);

    protectedPageClient()->pageDidScroll(scrollPosition);

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
#if HAVE(DISPLAY_LINK)
    updateDisplayLinkFrequency();
#endif
}

void WebPageProxy::runOpenPanel(FrameIdentifier frameID, FrameInfoData&& frameInfo, const FileChooserSettings& settings)
{
    if (RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr))
        openPanelResultListener->invalidate();

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    Ref parameters = API::OpenPanelParameters::create(settings);
    Ref openPanelResultListener = WebOpenPanelResultListenerProxy::create(this);
    m_openPanelResultListener = openPanelResultListener.copyRef();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->handleRunOpenPanel(*this, *frame, parameters.get(), openPanelResultListener);

        // Don't show a file chooser, since automation will be unable to interact with it.
        return;
    }

    // Since runOpenPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    m_process->stopResponsivenessTimer();

    const auto frameInfoForPageClient = frameInfo;

    if (!m_uiClient->runOpenPanel(*this, frame.get(), WTFMove(frameInfo), parameters.ptr(), openPanelResultListener.ptr())) {
        if (!protectedPageClient()->handleRunOpenPanel(this, frame.get(), frameInfoForPageClient, parameters.ptr(), openPanelResultListener.ptr()))
            didCancelForOpenPanel();
    }
}

void WebPageProxy::showShareSheet(const ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(m_process, !shareData.url || shareData.url->protocolIsInHTTPFamily() || shareData.url->protocolIsData());
    MESSAGE_CHECK(m_process, shareData.files.isEmpty() || m_preferences->webShareFileAPIEnabled());
    MESSAGE_CHECK(m_process, shareData.originator == ShareDataOriginator::Web);
    protectedPageClient()->showShareSheet(shareData, WTFMove(completionHandler));
}

void WebPageProxy::showContactPicker(const WebCore::ContactsRequestData& requestData, CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& completionHandler)
{
    MESSAGE_CHECK(m_process, m_preferences->contactPickerAPIEnabled());
    protectedPageClient()->showContactPicker(requestData, WTFMove(completionHandler));
}

void WebPageProxy::printFrame(FrameIdentifier frameID, const String& title, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!m_isPerformingDOMPrintOperation);
    m_isPerformingDOMPrintOperation = true;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    frame->didChangeTitle(title);

    m_uiClient->printFrame(*this, *frame, pdfFirstPageSize, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        endPrinting(WTFMove(completionHandler)); // Send a message synchronously while m_isPerformingDOMPrintOperation is still true.
        m_isPerformingDOMPrintOperation = false;
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

    internals().mutedState = state;

    if (!hasRunningProcess())
        return completionHandler();

#if ENABLE(MEDIA_STREAM)
    bool hasMutedCaptureStreams = internals().mediaState.containsAny(WebCore::MediaProducer::MutedCaptureMask);
    if (hasMutedCaptureStreams && !(state.containsAny(WebCore::MediaProducer::MediaStreamCaptureIsMuted)))
        WebProcessProxy::muteCaptureInPagesExcept(internals().webPageID);
#endif

    protectedProcess()->pageMutedStateChanged(internals().webPageID, state);

    WEBPAGEPROXY_RELEASE_LOG(Media, "setMuted: %d", state.toRaw());

    sendWithAsyncReply(Messages::WebPage::SetMuted(state), WTFMove(completionHandler));
    activityStateDidChange({ ActivityState::IsAudible, ActivityState::IsCapturingMedia });
}

void WebPageProxy::setMuted(WebCore::MediaProducerMutedStateFlags state)
{
    setMuted(state, [] { });
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

void WebPageProxy::stopMediaCapture(MediaProducerMediaCaptureKind kind)
{
    stopMediaCapture(kind, [] { });
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

void WebPageProxy::resumeDownload(const API::Data& resumeData, const String& path, CompletionHandler<void(DownloadProxy*)>&& completionHandler)
{
    Ref download = process().processPool().resumeDownload(websiteDataStore(), this, resumeData, path, CallDownloadDidStart::Yes);
    download->setDestinationFilename(path);
    download->setDidStartCallback(WTFMove(completionHandler));
}

void WebPageProxy::downloadRequest(WebCore::ResourceRequest&& request, CompletionHandler<void(DownloadProxy*)>&& completionHandler)
{
    Ref download = process().processPool().download(websiteDataStore(), this, request, { });
    download->setDidStartCallback(WTFMove(completionHandler));
}

void WebPageProxy::dataTaskWithRequest(WebCore::ResourceRequest&& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, CompletionHandler<void(API::DataTask&)>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->dataTaskWithRequest(*this, sessionID(), WTFMove(request), topOrigin, WTFMove(completionHandler));
}

void WebPageProxy::didChangeContentSize(const IntSize& size)
{
    protectedPageClient()->didChangeContentSize(size);
}

void WebPageProxy::didChangeIntrinsicContentSize(const IntSize& intrinsicContentSize)
{
#if USE(APPKIT)
    protectedPageClient()->intrinsicContentSizeDidChange(intrinsicContentSize);
#endif
}

#if ENABLE(WEBXR) && !USE(OPENXR)
PlatformXRSystem* WebPageProxy::xrSystem() const
{
    return internals().xrSystem.get();
}

void WebPageProxy::restartXRSessionActivityOnProcessResumeIfNeeded()
{
    if (xrSystem() && xrSystem()->hasActiveSession())
        xrSystem()->ensureImmersiveSessionActivity();
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)

void WebPageProxy::showColorPicker(const WebCore::Color& initialColor, const IntRect& elementRect, Vector<WebCore::Color>&& suggestions)
{
    internals().colorPicker = protectedPageClient()->createColorPicker(this, initialColor, elementRect, WTFMove(suggestions));
    internals().colorPicker->showColorPicker(initialColor);
}

void WebPageProxy::setColorPickerColor(const WebCore::Color& color)
{
    if (RefPtr colorPicker = internals().colorPicker)
        colorPicker->setSelectedColor(color);
}

void WebPageProxy::endColorPicker()
{
    if (RefPtr colorPicker = std::exchange(internals().colorPicker, nullptr))
        colorPicker->endPicker();
}

WebColorPickerClient& WebPageProxy::colorPickerClient()
{
    return internals();
}

void WebPageProxy::Internals::didChooseColor(const WebCore::Color& color)
{
    if (!page.hasRunningProcess())
        return;

    page.send(Messages::WebPage::DidChooseColor(color));
}

void WebPageProxy::Internals::didEndColorPicker()
{
    if (!std::exchange(colorPicker, nullptr))
        return;

    if (!page.hasRunningProcess())
        return;

    page.send(Messages::WebPage::DidEndColorPicker());
}

#endif

#if ENABLE(DATALIST_ELEMENT)

void WebPageProxy::showDataListSuggestions(WebCore::DataListSuggestionInformation&& info)
{
    if (!internals().dataListSuggestionsDropdown)
        internals().dataListSuggestionsDropdown = protectedPageClient()->createDataListSuggestionsDropdown(*this);

    Ref { *internals().dataListSuggestionsDropdown }->show(WTFMove(info));
}

void WebPageProxy::handleKeydownInDataList(const String& key)
{
    RefPtr dataListSuggestionsDropdown = internals().dataListSuggestionsDropdown;
    if (!dataListSuggestionsDropdown)
        return;

    dataListSuggestionsDropdown->handleKeydownWithIdentifier(key);
}

void WebPageProxy::endDataListSuggestions()
{
    if (RefPtr dataListSuggestionsDropdown = internals().dataListSuggestionsDropdown)
        dataListSuggestionsDropdown->close();
}

void WebPageProxy::didCloseSuggestions()
{
    if (!internals().dataListSuggestionsDropdown)
        return;

    internals().dataListSuggestionsDropdown = nullptr;
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
        m_dateTimePicker = protectedPageClient()->createDateTimePicker(*this);

    Ref { *m_dateTimePicker }->showDateTimePicker(WTFMove(params));
}

void WebPageProxy::endDateTimePicker()
{
    if (!m_dateTimePicker)
        return;

    Ref { *m_dateTimePicker }->endPicker();
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

RefPtr<WebInspectorUIProxy> WebPageProxy::protectedInspector() const
{
    return inspector();
}

void WebPageProxy::resourceLoadDidSendRequest(ResourceLoadInfo&& loadInfo, WebCore::ResourceRequest&& request)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidSendRequest(identifier(), loadInfo, request);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didSendRequest(WTFMove(loadInfo), WTFMove(request));
}

void WebPageProxy::resourceLoadDidPerformHTTPRedirection(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidPerformHTTPRedirection(identifier(), loadInfo, response, request);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didPerformHTTPRedirection(WTFMove(loadInfo), WTFMove(response), WTFMove(request));
}

void WebPageProxy::resourceLoadDidReceiveChallenge(ResourceLoadInfo&& loadInfo, WebCore::AuthenticationChallenge&& challenge)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidReceiveChallenge(identifier(), loadInfo, challenge);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didReceiveChallenge(WTFMove(loadInfo), WTFMove(challenge));
}

void WebPageProxy::resourceLoadDidReceiveResponse(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidReceiveResponse(identifier(), loadInfo, response);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didReceiveResponse(WTFMove(loadInfo), WTFMove(response));
}

void WebPageProxy::resourceLoadDidCompleteWithError(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceError&& error)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidCompleteWithError(identifier(), loadInfo, response, error);
#endif

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

VideoPresentationManagerProxy* WebPageProxy::videoPresentationManager()
{
    return m_videoPresentationManager.get();
}

void WebPageProxy::setMockVideoPresentationModeEnabled(bool enabled)
{
    m_mockVideoPresentationModeEnabled = enabled;
    if (RefPtr videoPresentationManager = m_videoPresentationManager)
        videoPresentationManager->setMockVideoPresentationModeEnabled(enabled);
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

void WebPageProxy::requestDOMPasteAccess(DOMPasteAccessCategory pasteAccessCategory, FrameIdentifier frameID, const IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(DOMPasteAccessResponse)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(m_process, !originIdentifier.isEmpty(), completionHandler(DOMPasteAccessResponse::DeniedForGesture));

    if (auto origin = SecurityOrigin::createFromString(originIdentifier); !origin->isOpaque()) {
        RefPtr frame = WebFrameProxy::webFrame(frameID);
        MESSAGE_CHECK_COMPLETION(m_process, frame && frame->page() == this, completionHandler(DOMPasteAccessResponse::DeniedForGesture));

        auto originFromFrame = SecurityOrigin::create(frame->url());
        MESSAGE_CHECK_COMPLETION(m_process, origin->isSameOriginDomain(originFromFrame), completionHandler(DOMPasteAccessResponse::DeniedForGesture));

        static constexpr auto recentlyRequestedDOMPasteOriginLimit = 10;

        auto currentTime = ApproximateTime::now();
        m_recentlyRequestedDOMPasteOrigins.removeAllMatching([&](auto& identifierAndTimestamp) {
            static constexpr auto recentlyRequestedDOMPasteOriginDelay = 1_s;

            auto& [identifier, lastRequestTime] = identifierAndTimestamp;
            return identifier == originIdentifier || currentTime - lastRequestTime > recentlyRequestedDOMPasteOriginDelay;
        });
        m_recentlyRequestedDOMPasteOrigins.append({ originIdentifier, currentTime });

        if (m_recentlyRequestedDOMPasteOrigins.size() > recentlyRequestedDOMPasteOriginLimit) {
            completionHandler(DOMPasteAccessResponse::DeniedForGesture);
            return;
        }
    }

    m_pageClient->requestDOMPasteAccess(pasteAccessCategory, elementRect, originIdentifier, WTFMove(completionHandler));
}

// BackForwardList

void WebPageProxy::backForwardAddItem(BackForwardListItemState&& itemState)
{
    backForwardAddItemShared(protectedProcess(), WTFMove(itemState));
}

void WebPageProxy::backForwardAddItemShared(Ref<WebProcessProxy>&& process, BackForwardListItemState&& itemState)
{
    URL itemURL { itemState.pageState.mainFrameState.urlString };
    URL itemOriginalURL { itemState.pageState.mainFrameState.originalURLString };
#if PLATFORM(COCOA)
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PushStateFilePathRestriction)
#if PLATFORM(MAC)
        && !MacApplication::isMimeoPhotoProject() // rdar://112445672.
#endif // PLATFORM(MAC)
    ) {
#endif // PLATFORM(COCOA)
        ASSERT(!itemURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemURL));
        MESSAGE_CHECK(process, !itemURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemURL));
        MESSAGE_CHECK(process, !itemOriginalURL.protocolIsFile() || process->wasPreviouslyApprovedFileURL(itemOriginalURL));
#if PLATFORM(COCOA)
    }
#endif

    Ref item = WebBackForwardListItem::create(WTFMove(itemState), identifier());
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

    backForwardGoToItemShared(itemID, WTFMove(completionHandler));
}

void WebPageProxy::backForwardListContainsItem(const WebCore::BackForwardItemIdentifier& itemID, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(protectedBackForwardList()->itemForID(itemID));
}

void WebPageProxy::backForwardGoToItemShared(const BackForwardItemIdentifier& itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(m_process, !WebKit::isInspectorPage(*this), completionHandler(m_backForwardList->counts()));

    Ref backForwardList = m_backForwardList;
    RefPtr item = backForwardList->itemForID(itemID);
    if (!item)
        return completionHandler(backForwardList->counts());

    backForwardList->goToItem(*item);
    completionHandler(backForwardList->counts());
}

void WebPageProxy::backForwardItemAtIndex(int32_t index, CompletionHandler<void(std::optional<BackForwardItemIdentifier>&&)>&& completionHandler)
{
    if (RefPtr item = m_backForwardList->itemAtIndex(index))
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
    protectedPageClient()->notifyInputContextAboutDiscardedComposition();
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
    protectedPageClient()->registerInsertionUndoGrouping();
#endif
}

void WebPageProxy::canUndoRedo(UndoOrRedo action, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(protectedPageClient()->canUndoRedo(action));
}

void WebPageProxy::executeUndoRedo(UndoOrRedo action, CompletionHandler<void()>&& completionHandler)
{
    protectedPageClient()->executeUndoRedo(action);
    completionHandler();
}

void WebPageProxy::clearAllEditCommands()
{
    protectedPageClient()->clearAllEditCommands();
}

void WebPageProxy::didGetImageForFindMatch(ImageBufferParameters&& parameters, ShareableBitmap::Handle&& contentImageHandle, uint32_t matchIndex)
{
    Ref image = WebImage::create({ { WTFMove(parameters), WTFMove(contentImageHandle) } });
    if (image->isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_findMatchesClient->didGetImageForMatchResult(this, image.ptr(), matchIndex);
}

void WebPageProxy::setTextIndicatorFromFrame(FrameIdentifier frameID, TextIndicatorData&& indicatorData, uint64_t lifetime)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    RefPtr rootFrameParent = frame->rootFrame()->parentFrame();
    if (!rootFrameParent) {
        setTextIndicator(WTFMove(indicatorData), lifetime);
        return;
    }

    ASSERT(m_preferences->siteIsolationEnabled());

    auto parentFrameID = rootFrameParent->frameID();
    auto textBoundingRect = indicatorData.textBoundingRectInRootViewCoordinates;
    sendToProcessContainingFrame(parentFrameID, Messages::WebPage::RemoteViewCoordinatesToRootView(frameID, textBoundingRect), [protectedThis = Ref { *this }, indicatorData = WTFMove(indicatorData), rootFrameParent = WTFMove(rootFrameParent), lifetime](FloatRect rect) mutable {
        indicatorData.textBoundingRectInRootViewCoordinates = rect;
        protectedThis->setTextIndicatorFromFrame(rootFrameParent->rootFrame()->frameID(), WTFMove(indicatorData), lifetime);
    });
}

void WebPageProxy::setTextIndicator(const TextIndicatorData& indicatorData, uint64_t lifetime)
{
    // FIXME: Make TextIndicatorWindow a platform-independent presentational thing ("TextIndicatorPresentation"?).
#if PLATFORM(COCOA)
    protectedPageClient()->setTextIndicator(TextIndicator::create(indicatorData), static_cast<WebCore::TextIndicatorLifetime>(lifetime));
#else
    notImplemented();
#endif
}

void WebPageProxy::clearTextIndicator()
{
#if PLATFORM(COCOA)
    protectedPageClient()->clearTextIndicator(WebCore::TextIndicatorDismissalAnimation::FadeOut);
#else
    notImplemented();
#endif
}

void WebPageProxy::setTextIndicatorAnimationProgress(float progress)
{
#if PLATFORM(COCOA)
    protectedPageClient()->setTextIndicatorAnimationProgress(progress);
#else
    notImplemented();
#endif
}

void WebPageProxy::didFindString(const String& string, const Vector<WebCore::IntRect>& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrapAround)
{
    m_findClient->didFindString(this, string, matchRects, matchCount, matchIndex, didWrapAround);
}

void WebPageProxy::didFailToFindString(const String& string)
{
    m_findClient->didFailToFindString(this, string);
}

bool WebPageProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    return protectedProcess()->sendMessage(WTFMove(encoder), sendOptions);
}

bool WebPageProxy::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return protectedProcess()->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

IPC::Connection* WebPageProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t WebPageProxy::messageSenderDestinationID() const
{
    return internals().webPageID.toUInt64();
}

void WebPageProxy::Internals::valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex)
{
    Ref { page }->send(Messages::WebPage::DidChangeSelectedIndexForActivePopupMenu(newSelectedIndex));
}

void WebPageProxy::Internals::setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index)
{
    Ref { page }->send(Messages::WebPage::SetTextForActivePopupMenu(index));
}

bool WebPageProxy::isProcessingKeyboardEvents() const
{
    return !internals().keyEventQueue.isEmpty();
}

bool WebPageProxy::isProcessingMouseEvents() const
{
    return !internals().mouseEventQueue.isEmpty();
}

bool WebPageProxy::isProcessingWheelEvents() const
{
    return m_wheelEventCoalescer && m_wheelEventCoalescer->hasEventsBeingProcessed();
}

NativeWebMouseEvent* WebPageProxy::Internals::currentlyProcessedMouseDownEvent()
{
    // <https://bugs.webkit.org/show_bug.cgi?id=57904> We need to keep track of the mouse down event in the case where we
    // display a popup menu for select elements. When the user changes the selected item, we fake a mouseup event by
    // using this stored mousedown event and changing the event type. This trickery happens when WebProcess handles
    // a mousedown event that runs the default handler for HTMLSelectElement, so the triggering mousedown must be the first event.

    if (mouseEventQueue.isEmpty())
        return nullptr;
    
    auto& event = mouseEventQueue.first();
    if (event.type() != WebEventType::MouseDown)
        return nullptr;

    return &event;
}

void WebPageProxy::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    if (!hasRunningProcess()) {
        m_pendingInjectedBundleMessages.append(InjectedBundleMessage { messageName, messageBody });
        return;
    }

    send(Messages::WebPage::PostInjectedBundleMessage(messageName, UserData(protectedProcess()->transformObjectsToHandles(messageBody).get())));
}

#if PLATFORM(GTK)
void WebPageProxy::Internals::failedToShowPopupMenu()
{
    Ref { page }->send(Messages::WebPage::FailedToShowPopupMenu());
}
#endif

void WebPageProxy::showPopupMenu(const IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    MESSAGE_CHECK(m_process, selectedIndex == -1 || static_cast<uint32_t>(selectedIndex) < items.size());

    if (RefPtr activePopupMenu = std::exchange(m_activePopupMenu, nullptr)) {
        activePopupMenu->hidePopupMenu();
        activePopupMenu->invalidate();
    }

    // If the page is controlled by automation, entering a nested run loop while the menu is open
    // can hang the page / WebDriver test. Since <option> elements are selected via a different
    // code path anyway, just don't show the native popup menu.
    if (RefPtr automationSession = process().processPool().automationSession()) {
        if (m_controlledByAutomation && automationSession->isSimulatingUserInteraction())
            return;
    }

    RefPtr activePopupMenu = protectedPageClient()->createPopupMenuProxy(*this);
    m_activePopupMenu = activePopupMenu;

    if (!activePopupMenu)
        return;

    // Since showPopupMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    protectedProcess()->stopResponsivenessTimer();

    // Showing a popup menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref protectedThis { *this };
    activePopupMenu->showPopupMenu(rect, static_cast<TextDirection>(textDirection), m_pageScaleFactor, items, data, selectedIndex);
}

void WebPageProxy::hidePopupMenu()
{
    if (RefPtr activePopupMenu = std::exchange(m_activePopupMenu, nullptr)) {
        activePopupMenu->hidePopupMenu();
        activePopupMenu->invalidate();
    }
}

#if ENABLE(CONTEXT_MENUS)

void WebPageProxy::showContextMenu(ContextMenuContextData&& contextMenuContextData, const UserData& userData)
{
    // Showing a context menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref protectedThis { *this };

    // If the page is controlled by automation, entering a nested run loop while the menu is open
    // can hang the page / WebDriver test. Pretend to show and immediately dismiss the context menu.
    if (RefPtr automationSession = process().processPool().automationSession()) {
        if (m_controlledByAutomation && automationSession->isSimulatingUserInteraction()) {
            send(Messages::WebPage::DidShowContextMenu());
            return;
        }
    }

    // Discard any enqueued mouse events that have been delivered to the UIProcess whilst the WebProcess is still processing the
    // MouseDown event that triggered this ShowContextMenu message. This can happen if we take too long to enter the nested runloop.
    discardQueuedMouseEvents();

    internals().activeContextMenuContextData = contextMenuContextData;

    Ref activeContextMenu = protectedPageClient()->createContextMenuProxy(*this, WTFMove(contextMenuContextData), userData);
    m_activeContextMenu = activeContextMenu.copyRef();

    activeContextMenu->show();
}

void WebPageProxy::didShowContextMenu()
{
    // Don't send `Messages::WebPage::DidShowContextMenu` as that should've already been eagerly
    // sent when requesting the context menu to show, regardless of the result of that request.

    protectedPageClient()->didShowContextMenu();
}

void WebPageProxy::didDismissContextMenu()
{
    send(Messages::WebPage::DidDismissContextMenu());

    protectedPageClient()->didDismissContextMenu();
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
    
    auto hitTestData = internals().activeContextMenuContextData.webHitTestResultData().value();

    switch (item.action()) {
#if PLATFORM(COCOA)
    case ContextMenuItemTagSmartCopyPaste:
        setSmartInsertDeleteEnabled(!isSmartInsertDeleteEnabled());
        return;

    case ContextMenuItemTagSmartQuotes:
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
        protectedProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartDashes:
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
        protectedProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartLinks:
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
        protectedProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagTextReplacement:
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
        protectedProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagCorrectSpellingAutomatically:
        TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);
        protectedProcess()->updateTextCheckerState();
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
        protectedProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagCheckGrammarWithSpelling:
        TextChecker::setGrammarCheckingEnabled(!TextChecker::state().isGrammarCheckingEnabled);
        protectedProcess()->updateTextCheckerState();
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
        Ref download = m_process->processPool().download(m_websiteDataStore, this, URL { downloadInfo->url }, downloadInfo->suggestedFilename);
        download->setDidStartCallback([this, weakThis = WeakPtr { *this }] (auto* download) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !download)
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
    auto machBootstrapHandle = SandboxExtension::createHandleForMachBootstrapExtension();
    if (auto handle = SandboxExtension::createHandleForMachLookup("com.apple.iconservices"_s, auditToken))
        iconServicesSandboxExtension = WTFMove(*handle);

    send(Messages::WebPage::DidChooseFilesForOpenPanelWithDisplayStringAndIcon(fileURLs, displayString, iconData ? iconData->dataReference() : IPC::DataReference(), WTFMove(machBootstrapHandle), WTFMove(iconServicesSandboxExtension)));

    RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr);
    openPanelResultListener->invalidate();
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

    RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr);
    openPanelResultListener->invalidate();
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidCancelForOpenPanel());
    
    RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr);
    openPanelResultListener->invalidate();
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
    protectedPageClient()->registerEditCommand(WTFMove(commandProxy), undoOrRedo);
}

void WebPageProxy::addEditCommand(WebEditCommandProxy& command)
{
    m_editCommandSet.add(command);
}

void WebPageProxy::removeEditCommand(WebEditCommandProxy& command)
{
    m_editCommandSet.remove(command);

    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::DidRemoveEditCommand(command.commandID()));
}

bool WebPageProxy::canUndo()
{
    return protectedPageClient()->canUndoRedo(UndoOrRedo::Undo);
}

bool WebPageProxy::canRedo()
{
    return protectedPageClient()->canUndoRedo(UndoOrRedo::Redo);
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

    if (isViewVisible()) {
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

void WebPageProxy::takeFocus(WebCore::FocusDirection direction)
{
    if (m_uiClient->takeFocus(this, (direction == WebCore::FocusDirection::Forward) ? kWKFocusDirectionForward : kWKFocusDirectionBackward))
        return;

    protectedPageClient()->takeFocus(direction);
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    if (m_toolTip == toolTip)
        return;

    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    protectedPageClient()->toolTipChanged(oldToolTip, m_toolTip);
}

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    protectedPageClient()->setCursor(cursor);
}

void WebPageProxy::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    protectedPageClient()->setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
}

void WebPageProxy::mouseEventHandlingCompleted(std::optional<WebEventType> eventType, bool handled)
{
    // Retire the last sent event now that WebProcess is done handling it.
    MESSAGE_CHECK(m_process, !internals().mouseEventQueue.isEmpty());
    auto event = internals().mouseEventQueue.takeFirst();
    if (eventType) {
        MESSAGE_CHECK(m_process, *eventType == event.type());
#if ENABLE(CONTEXT_MENU_EVENT)
        if (event.button() == WebMouseEventButton::Right) {
            if (event.type() == WebEventType::MouseDown) {
                ASSERT(m_contextMenuPreventionState == EventPreventionState::Waiting);
                m_contextMenuPreventionState = handled ? EventPreventionState::Prevented : EventPreventionState::Allowed;
            } else if (m_contextMenuPreventionState != EventPreventionState::Waiting)
                m_contextMenuPreventionState = EventPreventionState::None;

            processContextMenuCallbacks();
        }
#endif
    }

    if (!internals().mouseEventQueue.isEmpty()) {
        LOG(MouseHandling, " UIProcess: handling a queued mouse event from mouseEventHandlingCompleted");
        processNextQueuedMouseEvent();
    } else {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->mouseEventsFlushedForPage(*this);
        didFinishProcessingAllPendingMouseEvents();
    }
}

void WebPageProxy::keyEventHandlingCompleted(std::optional<WebEventType> eventType, bool handled)
{
    MESSAGE_CHECK(m_process, !internals().keyEventQueue.isEmpty());
    auto event = internals().keyEventQueue.takeFirst();
    if (eventType)
        MESSAGE_CHECK(m_process, *eventType == event.type());

#if PLATFORM(WIN)
    if (!handled && eventType && *eventType == WebEventType::RawKeyDown)
        dispatchPendingCharEvents(event);
#endif

    bool canProcessMoreKeyEvents = !internals().keyEventQueue.isEmpty();
    if (canProcessMoreKeyEvents && m_mainFrame) {
        auto nextEvent = internals().keyEventQueue.first();
        LOG(KeyHandling, " UI process: sent keyEvent from keyEventHandlingCompleted");
        sendKeyEvent(nextEvent);
    }

    // The call to doneWithKeyEvent may close this WebPage.
    // Protect against this being destroyed.
    Ref protectedThis { *this };

    protectedPageClient()->doneWithKeyEvent(event, handled);
    if (!handled)
        m_uiClient->didNotHandleKeyEvent(this, event);

    // Notify the session after -[NSApp sendEvent:] has a crack at turning the event into an action.
    if (!canProcessMoreKeyEvents) {
        if (RefPtr automationSession = process().processPool().automationSession())
            automationSession->keyboardEventsFlushedForPage(*this);
    }
}

void WebPageProxy::didReceiveEvent(WebEventType eventType, bool handled)
{
    switch (eventType) {
    case WebEventType::MouseMove:
    case WebEventType::Wheel:
        break;

    case WebEventType::MouseDown:
    case WebEventType::MouseUp:
    case WebEventType::MouseForceChanged:
    case WebEventType::MouseForceDown:
    case WebEventType::MouseForceUp:
    case WebEventType::KeyDown:
    case WebEventType::KeyUp:
    case WebEventType::RawKeyDown:
    case WebEventType::Char:
#if ENABLE(TOUCH_EVENTS)
    case WebEventType::TouchStart:
    case WebEventType::TouchMove:
    case WebEventType::TouchEnd:
    case WebEventType::TouchCancel:
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEventType::GestureStart:
    case WebEventType::GestureChange:
    case WebEventType::GestureEnd:
#endif
        protectedProcess()->stopResponsivenessTimer();
        break;
    }

    switch (eventType) {
    case WebEventType::MouseForceChanged:
    case WebEventType::MouseForceDown:
    case WebEventType::MouseForceUp:
    case WebEventType::MouseMove:
    case WebEventType::MouseDown:
    case WebEventType::MouseUp: {
        LOG_WITH_STREAM(MouseHandling, stream << "WebPageProxy::didReceiveEvent: " << eventType << " (queue size " << internals().mouseEventQueue.size() << ")");
        mouseEventHandlingCompleted(eventType, handled);
        break;
    }

    case WebEventType::Wheel:
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
        ASSERT(!scrollingCoordinatorProxy());
#endif
        MESSAGE_CHECK(m_process, wheelEventCoalescer().hasEventsBeingProcessed());
        wheelEventHandlingCompleted(handled);
        break;

    case WebEventType::KeyDown:
    case WebEventType::KeyUp:
    case WebEventType::RawKeyDown:
    case WebEventType::Char: {
        LOG_WITH_STREAM(KeyHandling, stream << "WebPageProxy::didReceiveEvent: " << eventType << " (queue empty " << internals().keyEventQueue.isEmpty() << ")");
        keyEventHandlingCompleted(eventType, handled);
        break;
    }
#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEventType::GestureStart:
    case WebEventType::GestureChange:
    case WebEventType::GestureEnd: {
        MESSAGE_CHECK(m_process, !internals().gestureEventQueue.isEmpty());
        auto event = internals().gestureEventQueue.takeFirst();
        MESSAGE_CHECK(m_process, eventType == event.type());

        if (!handled)
            protectedPageClient()->gestureEventWasNotHandledByWebCore(event);
        break;
    }
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    case WebEventType::TouchStart:
    case WebEventType::TouchMove:
    case WebEventType::TouchEnd:
    case WebEventType::TouchCancel:
        break;
#elif ENABLE(TOUCH_EVENTS)
    case WebEventType::TouchStart:
    case WebEventType::TouchMove:
    case WebEventType::TouchEnd:
    case WebEventType::TouchCancel: {
        touchEventHandlingCompleted(eventType, handled);
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

    bool isStaleEditorState = newEditorState.identifier < internals().editorState.identifier;
    bool shouldKeepExistingVisualEditorState = shouldMergeVisualEditorState == ShouldMergeVisualEditorState::No && internals().editorState.hasVisualData();
    bool shouldMergeNewVisualEditorState = shouldMergeVisualEditorState == ShouldMergeVisualEditorState::Yes && newEditorState.hasVisualData();
    
    std::optional<EditorState> oldEditorState;
    if (!isStaleEditorState) {
        oldEditorState = std::exchange(internals().editorState, newEditorState);
        if (shouldKeepExistingVisualEditorState)
            internals().editorState.visualData = oldEditorState->visualData;
    } else if (shouldMergeNewVisualEditorState) {
        oldEditorState = internals().editorState;
        internals().editorState.visualData = newEditorState.visualData;
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
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

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
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

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
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

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
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

    logDiagnosticMessageWithEnhancedPrivacy(message, description, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithValueDictionary(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    Ref apiDictionary = API::Dictionary::create();

    for (auto& keyValuePair : valueDictionary.dictionary) {
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
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

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
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

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

void WebPageProxy::focusedFrameChanged(IPC::Connection& connection, const std::optional<FrameIdentifier>& frameID)
{
    if (!frameID) {
        m_focusedFrame = nullptr;
        return;
    }

    RefPtr frame = WebFrameProxy::webFrame(*frameID);
    MESSAGE_CHECK(m_process, frame);

    m_focusedFrame = WTFMove(frame);
    broadcastFocusedFrameToOtherProcesses(connection, *frameID);
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
    internals().pageLoadState.willChangeProcessIsResponsive();
}

void WebPageProxy::didChangeProcessIsResponsive()
{
    internals().pageLoadState.didChangeProcessIsResponsive();
}

String WebPageProxy::currentURL() const
{
    String url = internals().pageLoadState.activeURL();
    if (url.isEmpty() && m_backForwardList->currentItem())
        url = m_backForwardList->currentItem()->url();
    return url;
}

URL WebPageProxy::currentResourceDirectoryURL() const
{
    auto resourceDirectoryURL = internals().pageLoadState.resourceDirectoryURL();
    if (!resourceDirectoryURL.isEmpty())
        return resourceDirectoryURL;
    if (RefPtr item = m_backForwardList->currentItem())
        return item->resourceDirectoryURL();
    return { };
}

void WebPageProxy::resetStateAfterProcessTermination(ProcessTerminationReason reason)
{
    if (reason != ProcessTerminationReason::NavigationSwap)
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "processDidTerminate: (pid %d), reason=%" PUBLIC_LOG_STRING, processID(), processTerminationReasonToString(reason));

    ASSERT(m_hasRunningProcess);

#if PLATFORM(IOS_FAMILY)
    if (m_process->isUnderMemoryPressure()) {
        String domain = WebCore::topPrivatelyControlledDomain(URL({ }, currentURL()).host().toString());
        if (!domain.isEmpty())
            logDiagnosticMessageWithEnhancedPrivacy(WebCore::DiagnosticLoggingKeys::domainCausingJetsamKey(), domain, WebCore::ShouldSample::No);
    }
#endif

    resetStateAfterProcessExited(reason);
    stopAllURLSchemeTasks(protectedProcess().ptr());
#if ENABLE(PDF_HUD)
    protectedPageClient()->removeAllPDFHUDs();
#endif

    if (reason != ProcessTerminationReason::NavigationSwap) {
        // For bringup of process swapping, NavigationSwap termination will not go out to clients.
        // If it does *during* process swapping, and the client triggers a reload, that causes bizarre WebKit re-entry.
        // FIXME: This might have to change
        navigationState().clearAllNavigations();

        if (m_controlledByAutomation) {
            if (RefPtr automationSession = process().processPool().automationSession())
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
    internals().resetRecentCrashCountTimer.stop();

    if (++m_recentCrashCount > maximumWebProcessRelaunchAttempts) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "tryReloadAfterProcessTermination: process crashed and the client did not handle it, not reloading the page because we reached the maximum number of attempts");
        m_recentCrashCount = 0;
        return;
    }
    URL pendingAPIRequestURL { internals().pageLoadState.pendingAPIRequestURL() };
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
    internals().resetRecentCrashCountTimer.startOneShot(resetRecentCrashCountDelay);
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
#if PLATFORM(MAC)
    m_scrollPerformanceDataCollectionEnabled = false;
#endif
    internals().firstLayerTreeTransactionIdAfterDidCommitLoad = { };
#endif

    m_recentlyRequestedDOMPasteOrigins = { };

    if (m_drawingArea) {
#if PLATFORM(COCOA)
        if (resetStateReason == ResetStateReason::NavigationSwap && is<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea)) {
            // Keep layers around in frozen state to avoid flashing during process swaps.
            m_frozenRemoteLayerTreeHost = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea).detachRemoteLayerTreeHost();
        }
#endif
        setDrawingArea(nullptr);
    }
    closeOverlayedViews();

    // Do not call inspector() / protectedInspector() since they return
    // null after the page has closed.
    RefPtr { m_inspector }->reset();

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

    if (RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr))
        openPanelResultListener->invalidate();

#if ENABLE(TOUCH_EVENTS)
    internals().touchEventTracking.reset();
#endif

#if ENABLE(GEOLOCATION)
    internals().geolocationPermissionRequestManager.invalidateRequests();
#endif

    setToolTip({ });

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    internals().mainFramePinnedState = { true, true, true, true };

    internals().visibleScrollerThumbRect = IntRect();

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr playbackSessionManager = std::exchange(m_playbackSessionManager, nullptr))
        playbackSessionManager->invalidate();

    if (RefPtr videoPresentationManager = std::exchange(m_videoPresentationManager, nullptr))
        videoPresentationManager->invalidate();
#endif

#if ENABLE(UI_SIDE_COMPOSITING)
    internals().lastVisibleContentRectUpdate = { };
#endif

#if PLATFORM(IOS_FAMILY)
    m_hasNetworkRequestsOnSuspended = false;
    m_isKeyboardAnimatingIn = false;
    m_isScrollingOrZooming = false;
    m_lastObservedStateWasBackground = false;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    protectedPageClient()->mediaSessionManager().removeAllPlaybackTargetPickerClients(internals());
#endif

#if ENABLE(APPLE_PAY)
    resetPaymentCoordinator(resetStateReason);
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

    for (Ref editCommand : std::exchange(m_editCommandSet, { }))
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
    protectedWebsiteDataStore()->authenticatorManager().cancelRequest(internals().webPageID, std::nullopt);
#endif

    m_speechRecognitionPermissionManager = nullptr;

#if ENABLE(WEBXR) && !USE(OPENXR)
    if (internals().xrSystem) {
        internals().xrSystem->invalidate();
        internals().xrSystem = nullptr;
    }
#endif

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    internals().lastSentScrollingAccelerationCurve = std::nullopt;
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    m_advancedPrivacyProtectionsPolicies = { };
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    setMediaCapability(std::nullopt);
#endif
}

void WebPageProxy::resetStateAfterProcessExited(ProcessTerminationReason terminationReason)
{
    if (!hasRunningProcess())
        return;

    Ref protectedPageClient { pageClient() };

#if ASSERT_ENABLED
    // FIXME: It's weird that resetStateAfterProcessExited() is called even though the process is launching.
    if (terminationReason != ProcessTerminationReason::NavigationSwap)
        ASSERT(m_process->state() == WebProcessProxy::State::Launching || m_process->state() == WebProcessProxy::State::Terminated);
#endif

#if PLATFORM(IOS_FAMILY)
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = false;
#endif

    m_processActivityState.reset();

    internals().pageIsUserObservableCount = nullptr;
    internals().visiblePageToken = nullptr;
    internals().pageAllowedToRunInTheBackgroundToken = nullptr;

    m_hasRunningProcess = false;
    m_areActiveDOMObjectsAndAnimationsSuspended = false;
    m_isServiceWorkerPage = false;

    m_userScriptsNotified = false;
    m_hasActiveAnimatedScroll = false;
    m_registeredForFullSpeedUpdates = false;
    internals().sleepDisablers.clear();

    internals().editorState = EditorState();
    internals().cachedFontAttributesAtSelectionStart.reset();

    if (terminationReason != ProcessTerminationReason::NavigationSwap)
        m_provisionalPage = nullptr;

    if (terminationReason == ProcessTerminationReason::NavigationSwap)
        protectedPageClient->processWillSwap();
    else
        protectedPageClient->processDidExit();

    protectedPageClient->clearAllEditCommands();

#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().revokeAccess(m_process.get());
#endif

    auto resetStateReason = terminationReason == ProcessTerminationReason::NavigationSwap ? ResetStateReason::NavigationSwap : ResetStateReason::WebProcessExited;
    resetState(resetStateReason);

    m_pendingLearnOrIgnoreWordMessageCount = 0;

    if (m_wheelEventCoalescer)
        m_wheelEventCoalescer->clear();

#if ENABLE(ATTACHMENT_ELEMENT)
    invalidateAllAttachments();
#endif

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (m_scrollingCoordinatorProxy)
        m_scrollingCoordinatorProxy->resetStateAfterProcessExited();
#endif

    if (terminationReason != ProcessTerminationReason::NavigationSwap) {
        auto transaction = internals().pageLoadState.transaction();
        internals().pageLoadState.reset(transaction);
    }

    updatePlayingMediaDidChange(MediaProducer::IsNotPlaying);

#if ENABLE(VIDEO_PRESENTATION_MODE)
    internals().fullscreenVideoTextRecognitionTimer.stop();
    internals().currentFullscreenVideoSessionIdentifier = std::nullopt;
#endif

    // FIXME: <rdar://problem/38676604> In case of process swaps, the old process should gracefully suspend instead of terminating.
    protectedProcess()->processTerminated();
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
static bool shouldBlockIOKit(const WebPreferences& preferences, DrawingAreaType drawingAreaType)
{
    if (!preferences.useGPUProcessForMediaEnabled()
        || (!preferences.captureVideoInGPUProcessEnabled() && !preferences.captureVideoInUIProcessEnabled())
        || (!preferences.captureAudioInGPUProcessEnabled() && !preferences.captureAudioInUIProcessEnabled())
        || !preferences.webRTCPlatformCodecsInGPUProcessEnabled()
        || !preferences.useGPUProcessForCanvasRenderingEnabled()
        || !preferences.useGPUProcessForDOMRenderingEnabled()
        || !preferences.useGPUProcessForWebGLEnabled()
        || drawingAreaType != DrawingAreaType::RemoteLayerTree)
        return false;
    return true;
}
#endif

#if !PLATFORM(COCOA)
bool WebPageProxy::useGPUProcessForDOMRenderingEnabled() const
{
    return preferences().useGPUProcessForDOMRenderingEnabled();
}
#endif

WebPageCreationParameters WebPageProxy::creationParameters(WebProcessProxy& process, DrawingAreaProxy& drawingArea, RefPtr<API::WebsitePolicies>&& websitePolicies)
{
    WebPageCreationParameters parameters;

    parameters.processDisplayName = configuration().processDisplayName();

    parameters.viewSize = protectedPageClient()->viewSize();
    parameters.activityState = internals().activityState;
    parameters.drawingAreaType = drawingArea.type();
    parameters.drawingAreaIdentifier = drawingArea.identifier();
    parameters.webPageProxyIdentifier = internals().identifier;
    parameters.store = preferencesStore();
    parameters.pageGroupData = m_pageGroup->data();
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
    parameters.layerHostingMode = internals().layerHostingMode;
    parameters.controlledByAutomation = m_controlledByAutomation;
    parameters.useDarkAppearance = useDarkAppearance();
    parameters.useElevatedUserInterfaceLevel = useElevatedUserInterfaceLevel();
#if PLATFORM(MAC)
    parameters.colorSpace = protectedPageClient()->colorSpace();
    parameters.useSystemAppearance = m_useSystemAppearance;
    parameters.useFormSemanticContext = useFormSemanticContext();
    parameters.headerBannerHeight = headerBannerHeight();
    parameters.footerBannerHeight = footerBannerHeight();
    if (m_viewWindowCoordinates)
        parameters.viewWindowCoordinates = *m_viewWindowCoordinates;
#endif

#if ENABLE(META_VIEWPORT)
    parameters.ignoresViewportScaleLimits = m_forceAlwaysUserScalable;
    parameters.viewportConfigurationViewLayoutSize = internals().viewportConfigurationViewLayoutSize;
    parameters.viewportConfigurationLayoutSizeScaleFactor = m_viewportConfigurationLayoutSizeScaleFactor;
    parameters.viewportConfigurationMinimumEffectiveDeviceWidth = m_viewportConfigurationMinimumEffectiveDeviceWidth;
    parameters.overrideViewportArguments = internals().overrideViewportArguments;
#endif

#if PLATFORM(IOS_FAMILY)
    parameters.screenSize = screenSize();
    parameters.availableScreenSize = availableScreenSize();
    parameters.overrideScreenSize = overrideScreenSize();
    parameters.textAutosizingWidth = textAutosizingWidth();
    parameters.mimeTypesWithCustomContentProviders = protectedPageClient()->mimeTypesWithCustomContentProviders();
    parameters.deviceOrientation = m_deviceOrientation;
    parameters.keyboardIsAttached = isInHardwareKeyboardMode();
    parameters.canShowWhileLocked = m_configuration->canShowWhileLocked();
    parameters.insertionPointColor = protectedPageClient()->insertionPointColor();
#endif

#if PLATFORM(COCOA)
    parameters.smartInsertDeleteEnabled = m_isSmartInsertDeleteEnabled;
    parameters.additionalSupportedImageTypes = m_configuration->additionalSupportedImageTypes();

#if !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
    if (!shouldBlockIOKit(preferences(), drawingArea.type())) {
        parameters.gpuIOKitExtensionHandles = SandboxExtension::createHandlesForIOKitClassExtensions(gpuIOKitClasses(), std::nullopt);
        parameters.gpuMachExtensionHandles = SandboxExtension::createHandlesForMachLookup(gpuMachServices(), std::nullopt);
    }
#endif // !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
#endif // PLATFORM(COCOA)

#if HAVE(STATIC_FONT_REGISTRY)
    if (preferences().shouldAllowUserInstalledFonts())
        parameters.fontMachExtensionHandles = process.fontdMachExtensionHandles(SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
#endif
#if HAVE(APP_ACCENT_COLORS)
    parameters.accentColor = protectedPageClient()->accentColor();
#if PLATFORM(MAC)
    parameters.appUsesCustomAccentColor = protectedPageClient()->appUsesCustomAccentColor();
#endif
#endif
    parameters.shouldScaleViewToFitDocument = m_shouldScaleViewToFitDocument;
    parameters.userInterfaceLayoutDirection = protectedPageClient()->userInterfaceLayoutDirection();
    parameters.observedLayoutMilestones = internals().observedLayoutMilestones;
    parameters.overrideContentSecurityPolicy = m_overrideContentSecurityPolicy;
    parameters.contentSecurityPolicyModeForExtension = m_configuration->contentSecurityPolicyModeForExtension();
    parameters.cpuLimit = m_cpuLimit;

#if USE(WPE_RENDERER)
    parameters.hostFileDescriptor = protectedPageClient()->hostFileDescriptor();
#endif

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    parameters.nativeWindowHandle = viewWidget();
#endif
#if USE(GRAPHICS_LAYER_WC)
    parameters.usesOffscreenRendering = protectedPageClient()->usesOffscreenRendering();
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
    parameters.backgroundColor = internals().backgroundColor;

    parameters.overriddenMediaType = m_overriddenMediaType;
    parameters.corsDisablingPatterns = corsDisablingPatterns();
    parameters.maskedURLSchemes = m_configuration->maskedURLSchemes();
    parameters.userScriptsShouldWaitUntilNotification = m_configuration->userScriptsShouldWaitUntilNotification();
    parameters.allowedNetworkHosts = m_configuration->allowedNetworkHosts();
    parameters.loadsSubresources = m_configuration->loadsSubresources();
    parameters.crossOriginAccessControlCheckEnabled = m_configuration->crossOriginAccessControlCheckEnabled();
    parameters.hasResourceLoadClient = !!m_resourceLoadClient;
    parameters.portsForUpgradingInsecureSchemeForTesting = m_configuration->portsForUpgradingInsecureSchemeForTesting();

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
    parameters.shouldRenderDOMInGPUProcess = useGPUProcessForDOMRenderingEnabled();
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

#if PLATFORM(IOS) || PLATFORM(VISION)
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
    parameters.hasResizableWindows = protectedPageClient()->hasResizableWindows();
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    parameters.linkDecorationFilteringData = LinkDecorationFilteringController::shared().cachedStrings();
    parameters.allowedQueryParametersForAdvancedPrivacyProtections = cachedAllowedQueryParametersForAdvancedPrivacyProtections();
#endif

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
#if ENABLE(LAUNCHD_BLOCKING_IN_WEBCONTENT)
    bool createBootstrapExtension = false;
#else
    bool createBootstrapExtension = !parameters.store.getBoolValueForKey(WebPreferencesKey::experimentalSandboxEnabledKey());
#endif
    if (!shouldBlockIOKit(preferences(), drawingArea.type()) || createBootstrapExtension)
        parameters.machBootstrapHandle = SandboxExtension::createHandleForMachBootstrapExtension();
#endif

#if USE(GBM)
#if PLATFORM(GTK)
    parameters.preferredBufferFormats = AcceleratedBackingStoreDMABuf::preferredBufferFormats();
#endif
#if PLATFORM(WPE)
    parameters.preferredBufferFormats = preferredBufferFormats();
#endif
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
    protectedPageClient()->enterAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::didFirstLayerFlush(const LayerTreeContext& layerTreeContext)
{
#if PLATFORM(MAC)
    ASSERT(m_drawingArea->type() == DrawingAreaType::TiledCoreAnimation);
#endif
    protectedPageClient()->didFirstLayerFlush(layerTreeContext);

    if (m_lastSuspendedPage)
        m_lastSuspendedPage->pageDidFirstLayerFlush();
    m_suspendedPageKeptToPreventFlashing = nullptr;
}

void WebPageProxy::exitAcceleratedCompositingMode()
{
    protectedPageClient()->exitAcceleratedCompositingMode();
}

void WebPageProxy::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    protectedPageClient()->updateAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::backForwardClear()
{
    protectedBackForwardList()->clear();
}

#if ENABLE(GAMEPAD)

void WebPageProxy::gamepadActivity(const Vector<std::optional<GamepadData>>& gamepadDatas, EventMakesGamepadsVisible eventVisibility)
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
    auto transaction = internals().pageLoadState.transaction();
    internals().pageLoadState.negotiatedLegacyTLS(transaction);
}

void WebPageProxy::didNegotiateModernTLS(const URL& url)
{
    m_navigationClient->didNegotiateModernTLS(url);
}

void WebPageProxy::didBlockLoadToKnownTracker(const URL& url)
{
    m_navigationClient->didBlockLoadToKnownTracker(*this, url);
}

void WebPageProxy::didApplyLinkDecorationFiltering(const URL& originalURL, const URL& adjustedURL)
{
    m_navigationClient->didApplyLinkDecorationFiltering(*this, originalURL, adjustedURL);
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
    if (originData != SecurityOriginData::fromURLWithoutStrictOpaqueness(URL { currentURL() })) {
        completionHandler(currentQuota);
        return;
    }

    Ref origin = API::SecurityOrigin::create(originData->securityOrigin());
    m_uiClient->exceededDatabaseQuota(this, frame.get(), origin.ptr(), databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, WTFMove(completionHandler));
}

void WebPageProxy::reachedApplicationCacheOriginQuota(const String& originIdentifier, uint64_t currentQuota, uint64_t totalBytesNeeded, CompletionHandler<void(uint64_t)>&& reply)
{
    auto securityOriginData = SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
    MESSAGE_CHECK(m_process, securityOriginData);

    Ref securityOrigin = securityOriginData->securityOrigin();
    m_uiClient->reachedApplicationCacheOriginQuota(this, securityOrigin.get(), currentQuota, totalBytesNeeded, WTFMove(reply));
}

void WebPageProxy::requestGeolocationPermissionForFrame(GeolocationIdentifier geolocationID, FrameInfoData&& frameInfo)
{
    MESSAGE_CHECK(m_process, frameInfo.frameID);
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK(m_process, frame);

    auto request = internals().geolocationPermissionRequestManager.createRequest(geolocationID);
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
        protectedPageClient()->decidePolicyForGeolocationPermissionRequest(*frame, frameInfo, completionHandler);
#endif
    if (completionHandler)
        completionHandler(false);
}

void WebPageProxy::revokeGeolocationAuthorizationToken(const String& authorizationToken)
{
    internals().geolocationPermissionRequestManager.revokeAuthorizationToken(authorizationToken);
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
    } else if (descriptor.name == PermissionName::Notifications || descriptor.name == PermissionName::Push) {
#if ENABLE(NOTIFICATIONS)
        name = "notifications"_s;

        // Ensure that the true permission state of the Notifications API is returned if
        // this topOrigin has requested permission to use the Notifications API previously.
        if (internals().notificationPermissionRequesters.contains(clientOrigin.topOrigin))
            shouldChangeDeniedToPrompt = false;

        if (sessionID().isEphemeral()) {
            completionHandler(shouldChangeDeniedToPrompt ? PermissionState::Prompt : PermissionState::Denied);
            return;
        }
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

    bool isNotificationPermission = descriptor.name == PermissionName::Notifications;
    CompletionHandler<void(std::optional<WebCore::PermissionState>)> callback = [clientOrigin, shouldChangeDeniedToPrompt, shouldChangePromptToGrant, isNotificationPermission, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result) {
            completionHandler({ });
            return;
        }
        if (*result == PermissionState::Denied && shouldChangeDeniedToPrompt)
            result = PermissionState::Prompt;
        else if (*result == PermissionState::Prompt && shouldChangePromptToGrant)
            result = PermissionState::Granted;
        if (result == PermissionState::Granted && isNotificationPermission && weakThis)
            weakThis->pageWillLikelyUseNotifications();
        completionHandler(*result);
    };

    if (clientOrigin.topOrigin.isOpaque()) {
        callback(PermissionState::Prompt);
        return;
    }

    Ref origin = API::SecurityOrigin::create(clientOrigin.topOrigin);
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

void WebPageProxy::clearUserMediaPermissionRequestHistory(WebCore::PermissionName name)
{
    if (m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager->clearUserMediaPermissionRequestHistory(name);
}

void WebPageProxy::setMockCaptureDevicesEnabledOverride(std::optional<bool> enabled)
{
    userMediaPermissionRequestManager().setMockCaptureDevicesEnabledOverride(enabled);
}

void WebPageProxy::willStartCapture(const UserMediaPermissionRequestProxy& request, CompletionHandler<void()>&& callback)
{
    activateMediaStreamCaptureInPage();

#if ENABLE(GPU_PROCESS)
    if (!preferences().captureVideoInGPUProcessEnabled() && !preferences().captureAudioInGPUProcessEnabled())
        return callback();

    Ref gpuProcess = process().processPool().ensureGPUProcess();
    gpuProcess->updateCaptureAccess(request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture(), m_process->coreProcessIdentifier(), WTFMove(callback));
    gpuProcess->updateCaptureOrigin(request.topLevelDocumentSecurityOrigin().data(), m_process->coreProcessIdentifier());
#if PLATFORM(IOS_FAMILY)
    gpuProcess->setOrientationForMediaCapture(m_orientationForMediaCapture);
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

void WebPageProxy::enumerateMediaDevicesForFrame(FrameIdentifier frameID, const WebCore::SecurityOriginData& userMediaDocumentOriginData, const WebCore::SecurityOriginData& topLevelDocumentOriginData, CompletionHandler<void(const Vector<WebCore::CaptureDeviceWithCapabilities>&, WebCore::MediaDeviceHashSalts&&)>&& completionHandler)
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

    Ref origin = API::SecurityOrigin::create(topLevelDocumentOriginData.securityOrigin());
    Ref request = mediaKeySystemPermissionRequestManager().createRequestForFrame(mediaKeySystemID, frameID, topLevelDocumentOriginData.securityOrigin(), keySystem);
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

    protectedWebsiteDataStore()->deviceOrientationAndMotionAccessController().shouldAllowAccess(*this, *frame, WTFMove(frameInfo), mayPrompt, WTFMove(completionHandler));
}
#endif


#if ENABLE(IMAGE_ANALYSIS)

void WebPageProxy::requestTextRecognition(const URL& imageURL, ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completionHandler)
{
    protectedPageClient()->requestTextRecognition(imageURL, WTFMove(imageData), sourceLanguageIdentifier, targetLanguageIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&& completion)
{
    protectedPageClient()->computeHasVisualSearchResults(imageURL, imageBitmap, WTFMove(completion));
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

void WebPageProxy::requestImageBitmap(const ElementContext& elementContext, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&, const String&)>&& completion)
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
    protectedPageClient()->showMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), WTFMove(completionHandler));
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(NOTIFICATIONS)
void WebPageProxy::clearNotificationPermissionState()
{
    internals().notificationPermissionRequesters.clear();
    send(Messages::WebPage::ClearNotificationPermissionState());
}
#endif

void WebPageProxy::requestNotificationPermission(const String& originString, CompletionHandler<void(bool allowed)>&& completionHandler)
{
    Ref origin = API::SecurityOrigin::createFromString(originString);

#if ENABLE(NOTIFICATIONS)
    // Add origin to list of origins that have requested permission to use the Notifications API.
    internals().notificationPermissionRequesters.add(origin->securityOrigin());
#endif

    m_uiClient->decidePolicyForNotificationPermissionRequest(*this, origin.get(), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](bool allowed) mutable {
        RefPtr protectedThis = weakThis.get();
        if (allowed && protectedThis)
            protectedThis->pageWillLikelyUseNotifications();
        completionHandler(allowed);
    });
}

void WebPageProxy::pageWillLikelyUseNotifications()
{
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "pageWillLikelyUseNotifications: This page is likely to use notifications and is allowed to run in the background");
    if (!internals().pageAllowedToRunInTheBackgroundToken)
        internals().pageAllowedToRunInTheBackgroundToken = process().throttler().pageAllowedToRunInTheBackgroundToken();
}

void WebPageProxy::showNotification(IPC::Connection& connection, const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources>&& notificationResources)
{
    m_process->processPool().supplement<WebNotificationManagerProxy>()->show(this, connection, notificationData, WTFMove(notificationResources));
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "showNotification: This page shows notifications and is allowed to run in the background");
    if (!internals().pageAllowedToRunInTheBackgroundToken)
        internals().pageAllowedToRunInTheBackgroundToken = process().throttler().pageAllowedToRunInTheBackgroundToken();
}

void WebPageProxy::cancelNotification(const WTF::UUID& notificationID)
{
    m_process->protectedProcessPool()->supplement<WebNotificationManagerProxy>()->cancel(this, notificationID);
}

void WebPageProxy::clearNotifications(const Vector<WTF::UUID>& notificationIDs)
{
    m_process->protectedProcessPool()->supplement<WebNotificationManagerProxy>()->clearNotifications(this, notificationIDs);
}

void WebPageProxy::didDestroyNotification(const WTF::UUID& notificationID)
{
    m_process->protectedProcessPool()->supplement<WebNotificationManagerProxy>()->didDestroyNotification(this, notificationID);
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
    protectedPageClient()->drawPageBorderForPrinting(WTFMove(size));
}

void WebPageProxy::runModal()
{
    Ref process = m_process;
    // Since runModal() can (and probably will) spin a nested run loop we need to turn off the responsiveness timer.
    process->stopResponsivenessTimer();

    // Our Connection's run loop might have more messages waiting to be handled after this RunModal message.
    // To make sure they are handled inside of the nested modal run loop we must first signal the Connection's
    // run loop so we're guaranteed that it has a chance to wake up.
    // See http://webkit.org/b/89590 for more discussion.
    process->protectedConnection()->wakeUpRunLoop();

    m_uiClient->runModal(*this);
}

void WebPageProxy::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    internals().visibleScrollerThumbRect = scrollerThumb;
}

void WebPageProxy::recommendedScrollbarStyleDidChange(int32_t newStyle)
{
#if USE(APPKIT)
    protectedPageClient()->recommendedScrollbarStyleDidChange(static_cast<WebCore::ScrollbarStyle>(newStyle));
#else
    UNUSED_PARAM(newStyle);
#endif
}

void WebPageProxy::didChangeScrollbarsForMainFrame(bool hasHorizontalScrollbar, bool hasVerticalScrollbar)
{
    m_mainFrameHasHorizontalScrollbar = hasHorizontalScrollbar;
    m_mainFrameHasVerticalScrollbar = hasVerticalScrollbar;
}

RectEdges<bool> WebPageProxy::pinnedState() const
{
    return internals().mainFramePinnedState;
}

void WebPageProxy::didChangeScrollOffsetPinningForMainFrame(RectEdges<bool> pinnedState)
{
    protectedPageClient()->pinnedStateWillChange();
    internals().mainFramePinnedState = pinnedState;
    protectedPageClient()->pinnedStateDidChange();

    m_uiClient->pinnedStateDidChange(*this);
}

void WebPageProxy::didChangePageCount(unsigned pageCount)
{
    m_pageCount = pageCount;
}

Color WebPageProxy::themeColor() const
{
    return internals().themeColor;
}

void WebPageProxy::themeColorChanged(const Color& themeColor)
{
    if (internals().themeColor == themeColor)
        return;

    protectedPageClient()->themeColorWillChange();
    internals().themeColor = themeColor;
    protectedPageClient()->themeColorDidChange();
}

Color WebPageProxy::pageExtendedBackgroundColor() const
{
    return internals().pageExtendedBackgroundColor;
}

void WebPageProxy::pageExtendedBackgroundColorDidChange(const Color& newPageExtendedBackgroundColor)
{
    if (internals().pageExtendedBackgroundColor == newPageExtendedBackgroundColor)
        return;

    auto oldUnderPageBackgroundColor = underPageBackgroundColor();
    auto oldPageExtendedBackgroundColor = std::exchange(internals().pageExtendedBackgroundColor, newPageExtendedBackgroundColor);
    bool changesUnderPageBackgroundColor = !equalIgnoringSemanticColor(oldUnderPageBackgroundColor, underPageBackgroundColor());
    internals().pageExtendedBackgroundColor = WTFMove(oldPageExtendedBackgroundColor);

    if (changesUnderPageBackgroundColor)
        protectedPageClient()->underPageBackgroundColorWillChange();
    protectedPageClient()->pageExtendedBackgroundColorWillChange();

    internals().pageExtendedBackgroundColor = newPageExtendedBackgroundColor;

    if (changesUnderPageBackgroundColor)
        protectedPageClient()->underPageBackgroundColorDidChange();
    protectedPageClient()->pageExtendedBackgroundColorDidChange();
}

Color WebPageProxy::sampledPageTopColor() const
{
    return internals().sampledPageTopColor;
}

void WebPageProxy::sampledPageTopColorChanged(const Color& sampledPageTopColor)
{
    if (internals().sampledPageTopColor == sampledPageTopColor)
        return;

    protectedPageClient()->sampledPageTopColorWillChange();
    internals().sampledPageTopColor = sampledPageTopColor;
    protectedPageClient()->sampledPageTopColorDidChange();
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
    protectedPageClient()->didFinishLoadingDataForCustomContentProvider(ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename), dataReference);
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

void WebPageProxy::endPrinting(CompletionHandler<void()>&& callback)
{
    if (!m_isInPrintingMode) {
        callback();
        return;
    }

    m_isInPrintingMode = false;

    if (m_isPerformingDOMPrintOperation)
        sendWithAsyncReply(Messages::WebPage::EndPrintingDuringDOMPrintOperation(), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        sendWithAsyncReply(Messages::WebPage::EndPrinting(), WTFMove(callback));
}

IPC::Connection::AsyncReplyID WebPageProxy::computePagesForPrinting(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&& callback)
{
    m_isInPrintingMode = true;
    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReply(Messages::WebPage::ComputePagesForPrintingDuringDOMPrintOperation(frameID, printInfo), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReply(Messages::WebPage::ComputePagesForPrinting(frameID, printInfo), WTFMove(callback));
}

#if PLATFORM(COCOA)
IPC::Connection::AsyncReplyID WebPageProxy::drawRectToImage(WebFrameProxy* frame, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& callback)
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
void WebPageProxy::drawToPDF(FrameIdentifier frameID, const std::optional<FloatRect>& rect, bool allowTransparentBackground, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::DrawToPDF(frameID, rect, allowTransparentBackground), WTFMove(callback));
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
        isDiscardable = !protectedPageClient()->isViewWindowActive() || !isViewVisible();

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

    saveDataToFileInDownloadsFolder(WTFMove(sanitizedFilename), "application/pdf"_s, WTFMove(originatingURL), API::Data::create(dataReference.data(), dataReference.size()).get());
}

void WebPageProxy::setMinimumSizeForAutoLayout(const IntSize& size)
{
    if (internals().minimumSizeForAutoLayout == size)
        return;

    internals().minimumSizeForAutoLayout = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMinimumSizeForAutoLayout(size));
    m_drawingArea->minimumSizeForAutoLayoutDidChange();

#if USE(APPKIT)
    if (internals().minimumSizeForAutoLayout.width() <= 0)
        didChangeIntrinsicContentSize(IntSize(-1, -1));
#endif
}

void WebPageProxy::setSizeToContentAutoSizeMaximumSize(const IntSize& size)
{
    if (internals().sizeToContentAutoSizeMaximumSize == size)
        return;

    internals().sizeToContentAutoSizeMaximumSize = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetSizeToContentAutoSizeMaximumSize(size));
    m_drawingArea->sizeToContentAutoSizeMaximumSizeDidChange();

#if USE(APPKIT)
    if (internals().sizeToContentAutoSizeMaximumSize.width() <= 0)
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
    if (internals().viewportSizeForCSSViewportUnits && *internals().viewportSizeForCSSViewportUnits == viewportSize)
        return;

    internals().viewportSizeForCSSViewportUnits = viewportSize;

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
    protectedPageClient()->showDictationAlternativeUI(boundingBoxOfDictatedText, dictationContext);
}

void WebPageProxy::removeDictationAlternatives(WebCore::DictationContext dictationContext)
{
    protectedPageClient()->removeDictationAlternatives(dictationContext);
}

void WebPageProxy::dictationAlternatives(WebCore::DictationContext dictationContext, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(protectedPageClient()->dictationAlternatives(dictationContext));
}

#endif

#if PLATFORM(MAC)

void WebPageProxy::substitutionsPanelIsShowing(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(TextChecker::substitutionsPanelIsShowing());
}

void WebPageProxy::showCorrectionPanel(AlternativeTextType panelType, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    protectedPageClient()->showCorrectionPanel(panelType, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
}

void WebPageProxy::dismissCorrectionPanel(ReasonForDismissingAlternativeText reason)
{
    protectedPageClient()->dismissCorrectionPanel(reason);
}

void WebPageProxy::dismissCorrectionPanelSoon(ReasonForDismissingAlternativeText reason, CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(protectedPageClient()->dismissCorrectionPanelSoon(reason));
}

void WebPageProxy::recordAutocorrectionResponse(AutocorrectionResponse response, const String& replacedString, const String& replacementString)
{
    protectedPageClient()->recordAutocorrectionResponse(response, replacedString, replacementString);
}

void WebPageProxy::handleAlternativeTextUIResult(const String& result)
{
    if (!isClosed())
        send(Messages::WebPage::HandleAlternativeTextUIResult(result));
}

void WebPageProxy::setEditableElementIsFocused(bool editableElementIsFocused)
{
    protectedPageClient()->setEditableElementIsFocused(editableElementIsFocused);
}

#endif // PLATFORM(MAC)

#if PLATFORM(COCOA) || PLATFORM(GTK)

RefPtr<ViewSnapshot> WebPageProxy::takeViewSnapshot(std::optional<WebCore::IntRect>&& clipRect)
{
    return protectedPageClient()->takeViewSnapshot(WTFMove(clipRect));
}

#endif

#if PLATFORM(GTK) || PLATFORM(WPE)

void WebPageProxy::cancelComposition(const String& compositionString)
{
    if (!hasRunningProcess())
        return;

    // Remove any pending composition key event.
    if (internals().keyEventQueue.size() > 1) {
        auto event = internals().keyEventQueue.takeFirst();
        internals().keyEventQueue.removeAllMatching([](const auto& event) {
            return event.handledByInputMethod();
        });
        internals().keyEventQueue.prepend(WTFMove(event));
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
    if (internals().scrollPinningBehavior == pinning)
        return;
    
    internals().scrollPinningBehavior = pinning;

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
        send(Messages::WebPage::SetScrollbarOverlayStyle(scrollbarStyleForMessage), internals().webPageID);
}

void WebPageProxy::wrapCryptoKey(const Vector<uint8_t>& key, CompletionHandler<void(bool, Vector<uint8_t>&&)>&& completionHandler)
{
    Ref protectedPageClient { pageClient() };

    Vector<uint8_t> masterKey;

    if (auto keyData = m_navigationClient->webCryptoMasterKey(*this))
        masterKey = Vector(keyData->dataReference());

    Vector<uint8_t> wrappedKey;
    bool succeeded = wrapSerializedCryptoKey(masterKey, key, wrappedKey);
    completionHandler(succeeded, WTFMove(wrappedKey));
}

void WebPageProxy::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, CompletionHandler<void(bool, Vector<uint8_t>&&)>&& completionHandler)
{
    Ref protectedPageClient { pageClient() };

    Vector<uint8_t> masterKey;

    if (auto keyData = m_navigationClient->webCryptoMasterKey(*this))
        masterKey = Vector(keyData->dataReference());

    Vector<uint8_t> key;
    bool succeeded = unwrapSerializedCryptoKey(masterKey, wrappedKey, key);
    completionHandler(succeeded, WTFMove(key));
}

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

void WebPageProxy::setCompositionAsync(const String& text, const Vector<CompositionUnderline>& underlines, const Vector<CompositionHighlight>& highlights, const HashMap<String, Vector<CharacterRange>>& annotations, const EditingRange& selectionRange, const EditingRange& replacementRange)
{
    if (!hasRunningProcess()) {
        // If this fails, we should call -discardMarkedText on input context to notify the input method.
        // This will happen naturally later, as part of reloading the page.
        return;
    }

    send(Messages::WebPage::SetCompositionAsync(text, underlines, highlights, annotations, selectionRange, replacementRange));
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

void WebPageProxy::takeSnapshot(IntRect rect, IntSize bitmapSize, SnapshotOptions options, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::TakeSnapshot(rect, bitmapSize, options), WTFMove(callback));
}

void WebPageProxy::navigationGestureDidBegin()
{
    Ref protectedPageClient { pageClient() };

    m_isShowingNavigationGestureSnapshot = true;
    protectedPageClient->navigationGestureDidBegin();

    m_navigationClient->didBeginNavigationGesture(*this);
}

void WebPageProxy::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    Ref protectedPageClient { pageClient() };
    if (willNavigate) {
        m_isLayerTreeFrozenDueToSwipeAnimation = true;
        send(Messages::WebPage::FreezeLayerTreeDueToSwipeAnimation());
    }

    protectedPageClient->navigationGestureWillEnd(willNavigate, item);

    m_navigationClient->willEndNavigationGesture(*this, willNavigate, item);
}

void WebPageProxy::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    Ref protectedPageClient { pageClient() };

    protectedPageClient->navigationGestureDidEnd(willNavigate, item);

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
    Ref protectedPageClient { pageClient() };

    protectedPageClient->navigationGestureDidEnd();
}

void WebPageProxy::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    Ref protectedPageClient { pageClient() };

    protectedPageClient->willRecordNavigationSnapshot(item);
}

void WebPageProxy::navigationGestureSnapshotWasRemoved()
{
    m_isShowingNavigationGestureSnapshot = false;

    // The ViewGestureController may call this method on a WebPageProxy whose view has been destroyed. In such case,
    // we need to return early as the pageClient will not be valid below.
    if (m_isClosed)
        return;

    protectedPageClient()->didRemoveNavigationGestureSnapshot();

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

bool WebPageProxy::isPlayingAudio() const
{
    return internals().mediaState.contains(MediaProducerMediaState::IsPlayingAudio);
}

bool WebPageProxy::hasMediaStreaming() const
{
    return internals().mediaState.contains(MediaProducerMediaState::HasStreamingActivity);
}

bool WebPageProxy::isCapturingAudio() const
{
    return internals().mediaState.containsAny(MediaProducer::IsCapturingAudioMask);
}

bool WebPageProxy::isCapturingVideo() const
{
    return internals().mediaState.containsAny(MediaProducer::IsCapturingVideoMask);
}

bool WebPageProxy::hasActiveAudioStream() const
{
    return internals().mediaState.contains(MediaProducerMediaState::HasActiveAudioCaptureDevice);
}

bool WebPageProxy::hasActiveVideoStream() const
{
    return internals().mediaState.contains(MediaProducerMediaState::HasActiveVideoCaptureDevice);
}

MediaProducerMediaStateFlags WebPageProxy::reportedMediaState() const
{
    return internals().reportedMediaCaptureState | (internals().mediaState - MediaProducer::MediaCaptureMask);
}

void WebPageProxy::updatePlayingMediaDidChange(MediaProducerMediaStateFlags newState, CanDelayNotification canDelayNotification)
{
#if ENABLE(MEDIA_STREAM)
    auto updateMediaCaptureStateImmediatelyIfNeeded = [&] {
        if (canDelayNotification == CanDelayNotification::No && internals().updateReportedMediaCaptureStateTimer.isActive()) {
            internals().updateReportedMediaCaptureStateTimer.stop();
            updateReportedMediaCaptureState();
        }
    };
#endif

    if (newState == internals().mediaState) {
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
        EndowmentStateTracker::singleton().addClient(internals());
        m_isListeningForUserFacingStateChangeNotification = true;
    }
#endif

#if ENABLE(MEDIA_STREAM)
    WebCore::MediaProducerMediaStateFlags oldMediaCaptureState = internals().mediaState & WebCore::MediaProducer::MediaCaptureMask;
    WebCore::MediaProducerMediaStateFlags newMediaCaptureState = newState & WebCore::MediaProducer::MediaCaptureMask;
#endif

    MediaProducerMediaStateFlags playingMediaMask { MediaProducerMediaState::IsPlayingAudio, MediaProducerMediaState::IsPlayingVideo };
    MediaProducerMediaStateFlags oldState = internals().mediaState;

    bool playingAudioChanges = oldState.contains(MediaProducerMediaState::IsPlayingAudio) != newState.contains(MediaProducerMediaState::IsPlayingAudio);
    if (playingAudioChanges)
        protectedPageClient()->isPlayingAudioWillChange();
    internals().mediaState = newState;
    if (playingAudioChanges)
        protectedPageClient()->isPlayingAudioDidChange();

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
    if ((oldState & playingMediaMask) != (internals().mediaState & playingMediaMask))
        m_uiClient->isPlayingMediaDidChange(*this);

    if ((oldState.containsAny(MediaProducerMediaState::HasAudioOrVideo)) != (internals().mediaState.containsAny(MediaProducerMediaState::HasAudioOrVideo)))
        videoControlsManagerDidChange();

    Ref process = m_process;
    process->updateAudibleMediaAssertions();
    bool mediaStreamingChanges = oldState.contains(MediaProducerMediaState::HasStreamingActivity) != newState.contains(MediaProducerMediaState::HasStreamingActivity);
    if (mediaStreamingChanges)
        process->updateMediaStreamingActivity();

#if ENABLE(EXTENSION_CAPABILITIES)
    updateMediaCapability();
#endif
}

void WebPageProxy::updateReportedMediaCaptureState()
{
    auto activeCaptureState = internals().mediaState & MediaProducer::MediaCaptureMask;
    if (internals().reportedMediaCaptureState == activeCaptureState)
        return;

    bool haveReportedCapture = internals().reportedMediaCaptureState.containsAny(MediaProducer::MediaCaptureMask);
    bool willReportCapture = !activeCaptureState.isEmpty();

    if (haveReportedCapture && !willReportCapture && internals().updateReportedMediaCaptureStateTimer.isActive())
        return;

    if (!haveReportedCapture && willReportCapture)
        internals().updateReportedMediaCaptureStateTimer.startOneShot(m_mediaCaptureReportingDelay);

    WEBPAGEPROXY_RELEASE_LOG(WebRTC, "updateReportedMediaCaptureState: from %d to %d", internals().reportedMediaCaptureState.toRaw(), activeCaptureState.toRaw());

    bool microphoneCaptureChanged = (internals().reportedMediaCaptureState & MediaProducer::MicrophoneCaptureMask) != (activeCaptureState & MediaProducer::MicrophoneCaptureMask);
    bool cameraCaptureChanged = (internals().reportedMediaCaptureState & MediaProducer::VideoCaptureMask) != (activeCaptureState & MediaProducer::VideoCaptureMask);
    bool displayCaptureChanged = (internals().reportedMediaCaptureState & MediaProducer::DisplayCaptureMask) != (activeCaptureState & MediaProducer::DisplayCaptureMask);
    bool systemAudioCaptureChanged = (internals().reportedMediaCaptureState & MediaProducer::SystemAudioCaptureMask) != (activeCaptureState & MediaProducer::SystemAudioCaptureMask);

    auto reportedDisplayCaptureSurfaces = internals().reportedMediaCaptureState & (MediaProducer::ScreenCaptureMask | MediaProducer::WindowCaptureMask);
    auto activeDisplayCaptureSurfaces = activeCaptureState & (MediaProducer::ScreenCaptureMask | MediaProducer::WindowCaptureMask);
    auto displayCaptureSurfacesChanged = reportedDisplayCaptureSurfaces != activeDisplayCaptureSurfaces;

    if (microphoneCaptureChanged)
        protectedPageClient()->microphoneCaptureWillChange();
    if (cameraCaptureChanged)
        protectedPageClient()->cameraCaptureWillChange();
    if (displayCaptureChanged)
        protectedPageClient()->displayCaptureWillChange();
    if (displayCaptureSurfacesChanged)
        protectedPageClient()->displayCaptureSurfacesWillChange();
    if (systemAudioCaptureChanged)
        protectedPageClient()->systemAudioCaptureWillChange();

    internals().reportedMediaCaptureState = activeCaptureState;
    m_uiClient->mediaCaptureStateDidChange(internals().mediaState);

    if (microphoneCaptureChanged)
        protectedPageClient()->microphoneCaptureChanged();
    if (cameraCaptureChanged)
        protectedPageClient()->cameraCaptureChanged();
    if (displayCaptureChanged)
        protectedPageClient()->displayCaptureChanged();
    if (displayCaptureSurfacesChanged)
        protectedPageClient()->displayCaptureSurfacesChanged();
    if (systemAudioCaptureChanged)
        protectedPageClient()->systemAudioCaptureChanged();
}

void WebPageProxy::videoControlsManagerDidChange()
{
    protectedPageClient()->videoControlsManagerDidChange();
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
    if (RefPtr playbackSessionManager = m_playbackSessionManager)
        playbackSessionManager->requestControlledElementID();
#endif
}

void WebPageProxy::handleControlledElementIDResponse(const String& identifier) const
{
#if PLATFORM(MAC)
    protectedPageClient()->handleControlledElementIDResponse(identifier);
#endif
}

bool WebPageProxy::isPlayingVideoInEnhancedFullscreen() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoPresentationManager && m_videoPresentationManager->isPlayingVideoInEnhancedFullscreen();
#else
    return false;
#endif
}

void WebPageProxy::handleAutoplayEvent(WebCore::AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    m_uiClient->handleAutoplayEvent(*this, event, flags);
}

#if PLATFORM(MAC)
void WebPageProxy::setCaretAnimatorType(WebCore::CaretAnimatorType caretType)
{
    send(Messages::WebPage::SetCaretAnimatorType(caretType));
}

void WebPageProxy::setCaretBlinkingSuspended(bool suspended)
{
    send(Messages::WebPage::SetCaretBlinkingSuspended(suspended));
}

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
    protectedPageClient()->didPerformImmediateActionHitTest(result, contentPreventsDefault, m_process->transformHandlesToObjects(userData.protectedObject().get()).get());
}

NSObject *WebPageProxy::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    return protectedPageClient()->immediateActionAnimationControllerForHitTestResult(hitTestResult, type, userData);
}

void WebPageProxy::handleAcceptedCandidate(WebCore::TextCheckingResult acceptedCandidate)
{
    send(Messages::WebPage::HandleAcceptedCandidate(acceptedCandidate));
}

void WebPageProxy::didHandleAcceptedCandidate()
{
    protectedPageClient()->didHandleAcceptedCandidate();
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

void WebPageProxy::setHeaderBannerHeight(int height)
{
    send(Messages::WebPage::SetHeaderBannerHeight(height));
}

void WebPageProxy::setFooterBannerHeight(int height)
{
    send(Messages::WebPage::SetFooterBannerHeight(height));
}

void WebPageProxy::didBeginMagnificationGesture()
{
    if (!hasRunningProcess())
        return;
    send(Messages::WebPage::DidBeginMagnificationGesture());
}

void WebPageProxy::didEndMagnificationGesture()
{
    if (!hasRunningProcess())
        return;
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
    m_uiClient->didClickAutoFillButton(*this, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::didResignInputElementStrongPasswordAppearance(const UserData& userData)
{
    m_uiClient->didResignInputElementStrongPasswordAppearance(*this, protectedProcess()->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::performSwitchHapticFeedback()
{
    protectedPageClient()->performSwitchHapticFeedback();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

void WebPageProxy::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    protectedPageClient()->mediaSessionManager().addPlaybackTargetPickerClient(internals(), contextId);
}

void WebPageProxy::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    protectedPageClient()->mediaSessionManager().removePlaybackTargetPickerClient(internals(), contextId);
}

void WebPageProxy::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const WebCore::FloatRect& rect, bool hasVideo)
{
    protectedPageClient()->mediaSessionManager().showPlaybackTargetPicker(internals(), contextId, protectedPageClient()->rootViewToScreen(IntRect(rect)), hasVideo, useDarkAppearance());
}

void WebPageProxy::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, WebCore::MediaProducerMediaStateFlags state)
{
    protectedPageClient()->mediaSessionManager().clientStateDidChange(internals(), contextId, state);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    protectedPageClient()->mediaSessionManager().setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerState(const String& name, WebCore::MediaPlaybackTargetContext::MockState state)
{
    protectedPageClient()->mediaSessionManager().setMockMediaPlaybackTargetPickerState(name, state);
}

void WebPageProxy::mockMediaPlaybackTargetPickerDismissPopup()
{
    protectedPageClient()->mediaSessionManager().mockMediaPlaybackTargetPickerDismissPopup();
}

void WebPageProxy::Internals::setPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, Ref<MediaPlaybackTarget>&& target)
{
    if (!page.hasRunningProcess())
        return;

    auto context = target->targetContext();
    page.send(Messages::WebPage::PlaybackTargetSelected(contextId, context));
}

void WebPageProxy::Internals::externalOutputDeviceAvailableDidChange(PlaybackTargetClientContextIdentifier contextId, bool available)
{
    if (!page.hasRunningProcess())
        return;

    page.send(Messages::WebPage::PlaybackTargetAvailabilityDidChange(contextId, available));
}

void WebPageProxy::Internals::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    if (!page.hasRunningProcess())
        return;

    page.send(Messages::WebPage::SetShouldPlayToPlaybackTarget(contextId, shouldPlay));
}

void WebPageProxy::Internals::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    if (!page.hasRunningProcess())
        return;

    page.send(Messages::WebPage::PlaybackTargetPickerWasDismissed(contextId));
}

#endif

void WebPageProxy::didChangeBackgroundColor()
{
    protectedPageClient()->didChangeBackgroundColor();
}

void WebPageProxy::clearWheelEventTestMonitor()
{
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::ClearWheelEventTestMonitor());
}

#if !PLATFORM(GTK) && !PLATFORM(WPE)
void WebPageProxy::callAfterNextPresentationUpdate(CompletionHandler<void()>&& callback)
{
    if (!hasRunningProcess() || !m_drawingArea)
        return callback();

#if PLATFORM(COCOA)
    Ref aggregator = CallbackAggregator::create(WTFMove(callback));
    auto drawingAreaIdentifier = m_drawingArea->identifier();
    sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [aggregator] { }, drawingAreaIdentifier);
    for (auto& remotePageProxy : internals().remotePageProxyState.domainToRemotePageProxyMap.values())
        Ref { *remotePageProxy }->sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [aggregator] { }, drawingAreaIdentifier);
#elif USE(COORDINATED_GRAPHICS)
    downcast<DrawingAreaProxyCoordinatedGraphics>(*m_drawingArea).dispatchAfterEnsuringDrawing(WTFMove(callback));
#else
    callback();
#endif
}
#endif

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
    protectedPageClient()->didRestoreScrollPosition();
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
    return protectedPageClient()->userInterfaceLayoutDirection();
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

    if (!isViewVisible() || !isViewFocused()) {
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
#endif // ENABLE(POINTER_LOCK)

void WebPageProxy::setURLSchemeHandlerForScheme(Ref<WebURLSchemeHandler>&& handler, const String& scheme)
{
    auto canonicalizedScheme = WTF::URLParser::maybeCanonicalizeScheme(scheme);
    ASSERT(canonicalizedScheme);
    ASSERT(!WTF::URLParser::isSpecialScheme(canonicalizedScheme.value()));

    auto schemeResult = m_urlSchemeHandlersByScheme.add(canonicalizedScheme.value(), handler.get());
    ASSERT_UNUSED(schemeResult, schemeResult.isNewEntry);

    auto handlerIdentifier = handler->identifier();
    auto handlerIdentifierResult = internals().urlSchemeHandlersByIdentifier.add(handlerIdentifier, WTFMove(handler));
    ASSERT_UNUSED(handlerIdentifierResult, handlerIdentifierResult.isNewEntry);

    send(Messages::WebPage::RegisterURLSchemeHandler(handlerIdentifier, canonicalizedScheme.value()));
}

WebURLSchemeHandler* WebPageProxy::urlSchemeHandlerForScheme(const String& scheme)
{
    return scheme.isNull() ? nullptr : m_urlSchemeHandlersByScheme.get(scheme);
}

void WebPageProxy::startURLSchemeTask(URLSchemeTaskParameters&& parameters)
{
    startURLSchemeTaskShared(protectedProcess(), internals().webPageID, WTFMove(parameters));
}

void WebPageProxy::startURLSchemeTaskShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, URLSchemeTaskParameters&& parameters)
{
    MESSAGE_CHECK(m_process, decltype(Internals::urlSchemeHandlersByIdentifier)::isValidKey(parameters.handlerIdentifier));
    auto iterator = internals().urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK(process, iterator != internals().urlSchemeHandlersByIdentifier.end());

    Ref { iterator->value }->startTask(*this, process, webPageID, WTFMove(parameters), nullptr);
}

void WebPageProxy::stopURLSchemeTask(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier)
{
    MESSAGE_CHECK(m_process, decltype(Internals::urlSchemeHandlersByIdentifier)::isValidKey(handlerIdentifier));
    auto iterator = internals().urlSchemeHandlersByIdentifier.find(handlerIdentifier);
    MESSAGE_CHECK(m_process, iterator != internals().urlSchemeHandlersByIdentifier.end());

    Ref { iterator->value }->stopTask(*this, taskIdentifier);
}

void WebPageProxy::loadSynchronousURLSchemeTask(URLSchemeTaskParameters&& parameters, CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, Vector<uint8_t>&&)>&& reply)
{
    MESSAGE_CHECK(m_process, decltype(Internals::urlSchemeHandlersByIdentifier)::isValidKey(parameters.handlerIdentifier));
    auto iterator = internals().urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK(m_process, iterator != internals().urlSchemeHandlersByIdentifier.end());

    Ref { iterator->value }->startTask(*this, m_process, internals().webPageID, WTFMove(parameters), WTFMove(reply));
}

void WebPageProxy::requestStorageAccessConfirm(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, std::optional<OrganizationStorageAccessPromptQuirk>&& organizationStorageAccessPromptQuirk, CompletionHandler<void(bool)>&& completionHandler)
{
    m_uiClient->requestStorageAccessConfirm(*this, WebFrameProxy::webFrame(frameID), subFrameDomain, topFrameDomain, WTFMove(organizationStorageAccessPromptQuirk), WTFMove(completionHandler));
    m_navigationClient->didPromptForStorageAccess(*this, topFrameDomain.string(), subFrameDomain.string(), !!organizationStorageAccessPromptQuirk);
}

void WebPageProxy::didCommitCrossSiteLoadWithDataTransferFromPrevalentResource()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::WasLoadedWithDataTransferFromPrevalentResource());
}

bool WebPageProxy::useDarkAppearance() const
{
    return protectedPageClient()->effectiveAppearanceIsDark();
}

bool WebPageProxy::useElevatedUserInterfaceLevel() const
{
    return protectedPageClient()->effectiveUserInterfaceLevelIsElevated();
}

void WebPageProxy::effectiveAppearanceDidChange()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::EffectiveAppearanceDidChange(useDarkAppearance(), useElevatedUserInterfaceLevel()));
}

DataOwnerType WebPageProxy::dataOwnerForPasteboard(PasteboardAccessIntent intent) const
{
    return protectedPageClient()->dataOwnerForPasteboard(intent);
}

#if ENABLE(ATTACHMENT_ELEMENT)

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&& info, const String& authorizationToken)
{
    MESSAGE_CHECK(m_process, isValidPerformActionOnElementAuthorizationToken(authorizationToken));

    protectedPageClient()->writePromisedAttachmentToPasteboard(WTFMove(info));
}
#endif

void WebPageProxy::requestAttachmentIcon(const String& identifier, const String& contentType, const String& fileName, const String& title, const FloatSize& requestedSize)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());

    auto updateAttachmentIcon = [&] {
        FloatSize size = requestedSize;
        std::optional<ShareableBitmap::Handle> handle;

#if PLATFORM(COCOA)
        if (RefPtr icon = iconForAttachment(fileName, contentType, title, size)) {
            if (auto iconHandle = icon->createHandle())
                handle = WTFMove(*iconHandle);
        }
#endif

        send(Messages::WebPage::UpdateAttachmentIcon(identifier, WTFMove(handle), size));
    };

#if PLATFORM(MAC)
    if (RefPtr attachment = attachmentForIdentifier(identifier); attachment && attachment->contentType() == "public.directory"_s) {
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
    sendWithAsyncReply(Messages::WebPage::UpdateAttachmentAttributes(attachment.identifier(), attachment.fileSizeForDisplay(), attachment.contentType(), attachment.fileName(), IPC::SharedBufferReference(attachment.associatedElementData())), WTFMove(callback));

#if HAVE(QUICKLOOK_THUMBNAILING)
    requestThumbnail(attachment, attachment.identifier());
#endif
}

#if HAVE(QUICKLOOK_THUMBNAILING)
void WebPageProxy::updateAttachmentThumbnail(const String& identifier, const RefPtr<ShareableBitmap>& bitmap)
{
    if (!hasRunningProcess())
        return;
    
    std::optional<ShareableBitmap::Handle> handle;
    if (bitmap)
        handle = bitmap->createHandle();

    send(Messages::WebPage::UpdateAttachmentThumbnail(identifier, handle));
}
#endif

void WebPageProxy::registerAttachmentIdentifierFromData(const String& identifier, const String& contentType, const String& preferredFileName, const IPC::SharedBufferReference& data)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    if (attachmentForIdentifier(identifier))
        return;

    Ref attachment = ensureAttachment(identifier);
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

    Ref attachment = ensureAttachment(identifier);
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
            Ref attachment = ensureAttachment(identifier);
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

    Ref newAttachment = ensureAttachment(toIdentifier);
    RefPtr existingAttachment = attachmentForIdentifier(fromIdentifier);
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
        Ref { attachment }->invalidate();
    }
    m_attachmentIdentifierToAttachmentMap.clear();
}

void WebPageProxy::serializedAttachmentDataForIdentifiers(const Vector<String>& identifiers, CompletionHandler<void(Vector<WebCore::SerializedAttachmentData>&&)>&& completionHandler)
{
    Vector<WebCore::SerializedAttachmentData> serializedData;

    MESSAGE_CHECK_COMPLETION(m_process, m_preferences->attachmentElementEnabled(), completionHandler(WTFMove(serializedData)));

    for (const auto& identifier : identifiers) {
        RefPtr attachment = attachmentForIdentifier(identifier);
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
    protectedPageClient()->didInvalidateDataForAttachment(attachment);
}

WebPageProxy::ShouldUpdateAttachmentAttributes WebPageProxy::willUpdateAttachmentAttributes(const API::Attachment&)
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

void WebPageProxy::didInsertAttachmentWithIdentifier(const String& identifier, const String& source, WebCore::AttachmentAssociatedElementType associatedElementType)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    Ref attachment = ensureAttachment(identifier);
    attachment->setAssociatedElementType(associatedElementType);
    attachment->setInsertionState(API::Attachment::InsertionState::Inserted);
    protectedPageClient()->didInsertAttachment(attachment, source);

    if (!attachment->isEmpty() && associatedElementType != WebCore::AttachmentAssociatedElementType::None)
        updateAttachmentAttributes(attachment, [] { });
}

void WebPageProxy::didRemoveAttachmentWithIdentifier(const String& identifier)
{
    MESSAGE_CHECK(m_process, m_preferences->attachmentElementEnabled());
    MESSAGE_CHECK(m_process, IdentifierToAttachmentMap::isValidKey(identifier));

    if (RefPtr attachment = attachmentForIdentifier(identifier))
        didRemoveAttachment(*attachment);
}

void WebPageProxy::didRemoveAttachment(API::Attachment& attachment)
{
    attachment.setInsertionState(API::Attachment::InsertionState::NotInserted);
    protectedPageClient()->didRemoveAttachment(attachment);
}

Ref<API::Attachment> WebPageProxy::ensureAttachment(const String& identifier)
{
    if (RefPtr existingAttachment = attachmentForIdentifier(identifier))
        return existingAttachment.releaseNonNull();

    Ref attachment = API::Attachment::create(identifier, *this);
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

    protectedPageClient()->storeAppHighlight(highlight);

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

    if (!internals().pageLoadStart)
        return;

    auto pageLoadTime = MonotonicTime::now() - *internals().pageLoadStart;
    internals().pageLoadStart = std::nullopt;

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
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
#if PLATFORM(COCOA)
    auto modifiers = PlatformKeyboardEvent::currentStateOfModifierKeys();
#elif PLATFORM(GTK) || PLATFORM(WPE)
    auto modifiers = currentStateOfModifierKeys();
#endif
    send(Messages::WebPage::UpdateCurrentModifierState(modifiers));
#endif
}

bool WebPageProxy::checkURLReceivedFromCurrentOrPreviousWebProcess(WebProcessProxy& process, const String& urlString)
{
    return checkURLReceivedFromCurrentOrPreviousWebProcess(process, URL { urlString });
}

bool WebPageProxy::checkURLReceivedFromCurrentOrPreviousWebProcess(WebProcessProxy& process, const URL& url)
{
    if (!url.protocolIsFile())
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

void WebPageProxy::setPrivateClickMeasurement(PrivateClickMeasurement&& measurement, String sourceDescription, String purchaser)
{
    internals().privateClickMeasurement = { WTFMove(measurement), WTFMove(sourceDescription), WTFMove(purchaser) };
}

void WebPageProxy::setPrivateClickMeasurement(PrivateClickMeasurement&& measurement)
{
    setPrivateClickMeasurement(WTFMove(measurement), { }, { });
}

void WebPageProxy::setPrivateClickMeasurement(std::nullopt_t)
{
    internals().privateClickMeasurement = std::nullopt;
}

auto WebPageProxy::privateClickMeasurementEventAttribution() const -> std::optional<EventAttribution>
{
    auto& pcm = internals().privateClickMeasurement;
    if (!pcm)
        return std::nullopt;
    return { { pcm->pcm.sourceID(), pcm->pcm.destinationSite().registrableDomain.string(), pcm->sourceDescription, pcm->purchaser } };
}

void WebPageProxy::dumpPrivateClickMeasurement(CompletionHandler<void(const String&)>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::DumpPrivateClickMeasurement(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearPrivateClickMeasurement(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementOverrideTimerForTesting(bool value, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementOverrideTimerForTesting(m_websiteDataStore->sessionID(), value), WTFMove(completionHandler));
}

void WebPageProxy::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementEphemeralMeasurementForTesting(m_websiteDataStore->sessionID(), value), WTFMove(completionHandler));
}

void WebPageProxy::simulatePrivateClickMeasurementSessionRestart(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SimulatePrivateClickMeasurementSessionRestart(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementTokenPublicKeyURLForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenPublicKeyURLForTesting(m_websiteDataStore->sessionID(), url), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementTokenSignatureURLForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenSignatureURLForTesting(m_websiteDataStore->sessionID(), url), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementAttributionReportURLsForTesting(const URL& sourceURL, const URL& destinationURL, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAttributionReportURLsForTesting(m_websiteDataStore->sessionID(), sourceURL, destinationURL), WTFMove(completionHandler));
}

void WebPageProxy::markPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::MarkPrivateClickMeasurementsAsExpiredForTesting(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::setPCMFraudPreventionValuesForTesting(const String& unlinkableToken, const String& secretToken, const String& signature, const String& keyID, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPCMFraudPreventionValuesForTesting(m_websiteDataStore->sessionID(), unlinkableToken, secretToken, signature, keyID), WTFMove(completionHandler));
}

void WebPageProxy::setPrivateClickMeasurementAppBundleIDForTesting(const String& appBundleIDForTesting, CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAppBundleIDForTesting(m_websiteDataStore->sessionID(), appBundleIDForTesting), WTFMove(completionHandler));
}

#if ENABLE(APPLE_PAY)

void WebPageProxy::resetPaymentCoordinator(ResetStateReason resetStateReason)
{
    if (!internals().paymentCoordinator)
        return;

    if (resetStateReason == ResetStateReason::WebProcessExited)
        internals().paymentCoordinator->webProcessExited();

    internals().paymentCoordinator = nullptr;
}

#endif // ENABLE(APPLE_PAY)

#if ENABLE(SPEECH_SYNTHESIS)

void WebPageProxy::resetSpeechSynthesizer()
{
    if (!internals().optionalSpeechSynthesisData)
        return;
    
    auto& synthesisData = *internals().optionalSpeechSynthesisData;
    synthesisData.speakingFinishedCompletionHandler = nullptr;
    synthesisData.speakingStartedCompletionHandler = nullptr;
    synthesisData.speakingPausedCompletionHandler = nullptr;
    synthesisData.speakingResumedCompletionHandler = nullptr;
    synthesisData.synthesizer->resetState();
}

SpeechSynthesisData& WebPageProxy::Internals::speechSynthesisData()
{
    if (!optionalSpeechSynthesisData)
        optionalSpeechSynthesisData = { PlatformSpeechSynthesizer::create(*this), nullptr, nullptr, nullptr, nullptr, nullptr };
    return *optionalSpeechSynthesisData;
}

void WebPageProxy::speechSynthesisVoiceList(CompletionHandler<void(Vector<WebSpeechSynthesisVoice>&&)>&& completionHandler)
{
    auto result = internals().speechSynthesisData().synthesizer->voiceList().map([](auto& voice) {
        return WebSpeechSynthesisVoice { voice->voiceURI(), voice->name(), voice->lang(), voice->localService(), voice->isDefault() };
    });
    completionHandler(WTFMove(result));
}

void WebPageProxy::speechSynthesisSetFinishedCallback(CompletionHandler<void()>&& completionHandler)
{
    internals().speechSynthesisData().speakingFinishedCompletionHandler = WTFMove(completionHandler);
}

void WebPageProxy::speechSynthesisSpeak(const String& text, const String& lang, float volume, float rate, float pitch, MonotonicTime, const String& voiceURI, const String& voiceName, const String& voiceLang, bool localService, bool defaultVoice, CompletionHandler<void()>&& completionHandler)
{
    auto voice = WebCore::PlatformSpeechSynthesisVoice::create(voiceURI, voiceName, voiceLang, localService, defaultVoice);
    auto utterance = WebCore::PlatformSpeechSynthesisUtterance::create(internals());
    utterance->setText(text);
    utterance->setLang(lang);
    utterance->setVolume(volume);
    utterance->setRate(rate);
    utterance->setPitch(pitch);
    utterance->setVoice(&voice.get());

    internals().speechSynthesisData().speakingStartedCompletionHandler = WTFMove(completionHandler);
    internals().speechSynthesisData().utterance = WTFMove(utterance);
    internals().speechSynthesisData().synthesizer->speak(internals().speechSynthesisData().utterance.get());
}

void WebPageProxy::speechSynthesisCancel()
{
    internals().speechSynthesisData().synthesizer->cancel();
}

void WebPageProxy::speechSynthesisResetState()
{
    internals().speechSynthesisData().synthesizer->resetState();
}

void WebPageProxy::speechSynthesisPause(CompletionHandler<void()>&& completionHandler)
{
    internals().speechSynthesisData().speakingPausedCompletionHandler = WTFMove(completionHandler);
    internals().speechSynthesisData().synthesizer->pause();
}

void WebPageProxy::speechSynthesisResume(CompletionHandler<void()>&& completionHandler)
{
    internals().speechSynthesisData().speakingResumedCompletionHandler = WTFMove(completionHandler);
    internals().speechSynthesisData().synthesizer->resume();
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
    auto result = m_webViewDidMoveToWindowObservers.add(observer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void WebPageProxy::removeDidMoveToWindowObserver(WebViewDidMoveToWindowObserver& observer)
{
    auto result = m_webViewDidMoveToWindowObservers.remove(observer);
    ASSERT_UNUSED(result, result);
}

WindowKind WebPageProxy::windowKind() const
{
    return internals().windowKind;
}

void WebPageProxy::webViewDidMoveToWindow()
{
    m_webViewDidMoveToWindowObservers.forEach([](auto& observer) {
        observer.webViewDidMoveToWindow();
    });

    auto newWindowKind = protectedPageClient()->windowKind();
    if (internals().windowKind != newWindowKind) {
        internals().windowKind = newWindowKind;
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
        Ref logger = Logger::create(this);
        m_logger = logger.copyRef();
        // FIXME: Does this really need to be disabled in ephemeral sessions?
        logger->setEnabled(this, !sessionID().isEphemeral());
    }

    return *m_logger;
}

const void* WebPageProxy::logIdentifier() const
{
    return reinterpret_cast<const void*>(intHash(identifier().toUInt64()));
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
    protectedWebsiteDataStore()->setMockWebAuthenticationConfiguration(WTFMove(configuration));
}
#endif

void WebPageProxy::startTextManipulations(const Vector<TextManipulationController::ExclusionRule>& exclusionRules, bool includeSubframes, TextManipulationItemCallback&& callback, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }
    m_textManipulationItemCallback = WTFMove(callback);
    sendWithAsyncReply(Messages::WebPage::StartTextManipulations(exclusionRules, includeSubframes), WTFMove(completionHandler));
}

void WebPageProxy::didFindTextManipulationItems(const Vector<WebCore::TextManipulationItem>& items)
{
    if (!m_textManipulationItemCallback)
        return;
    m_textManipulationItemCallback(items);
}

void WebPageProxy::completeTextManipulation(const Vector<TextManipulationItem>& items, Function<void(bool allFailed, const Vector<TextManipulationControllerManipulationFailure>&)>&& completionHandler)
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
    if (m_isClosed)
        return completionHandler(false);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadServiceWorker:");

    if (internals().serviceWorkerLaunchCompletionHandler)
        return completionHandler(false);

    m_isServiceWorkerPage = true;
    internals().serviceWorkerLaunchCompletionHandler = WTFMove(completionHandler);

    CString html;
    if (usingModules)
        html = makeString("<script>navigator.serviceWorker.register('", url.string(), "', { type: 'module' });</script>").utf8();
    else
        html = makeString("<script>navigator.serviceWorker.register('", url.string(), "');</script>").utf8();

    loadData({ reinterpret_cast<const uint8_t*>(html.data()), html.length() }, "text/html"_s, "UTF-8"_s, url.protocolHostAndPort());
}

#if !PLATFORM(COCOA)
bool WebPageProxy::shouldForceForegroundPriorityForClientNavigation() const
{
    return false;
}
#endif

void WebPageProxy::getProcessDisplayName(CompletionHandler<void(String&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetProcessDisplayName(), WTFMove(completionHandler));
}

void WebPageProxy::setOrientationForMediaCapture(WebCore::IntDegrees orientation)
{
    m_orientationForMediaCapture = orientation;
    if (!hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
#if PLATFORM(COCOA)
    if (auto* proxy = m_process->userMediaCaptureManagerProxy())
        proxy->setOrientation(orientation);

    RefPtr gpuProcess = m_process->processPool().gpuProcess();
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

void WebPageProxy::getLoadedSubresourceDomains(CompletionHandler<void(Vector<RegistrableDomain>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetLoadedSubresourceDomains(), WTFMove(completionHandler));
}

void WebPageProxy::clearLoadedSubresourceDomains()
{
    send(Messages::WebPage::ClearLoadedSubresourceDomains());
}

#if ENABLE(GPU_PROCESS)
void WebPageProxy::gpuProcessDidFinishLaunching()
{
    protectedPageClient()->gpuProcessDidFinishLaunching();
#if ENABLE(EXTENSION_CAPABILITIES)
    if (auto& mediaCapability = this->mediaCapability()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessCapabilities, "gpuProcessDidFinishLaunching[envID=%{public}s]: updating media capability", mediaCapability->environmentIdentifier().utf8().data());
        updateMediaCapability();
    }
#endif
}

void WebPageProxy::gpuProcessExited(ProcessTerminationReason)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInGPUProcess = 0;
#endif

    protectedPageClient()->gpuProcessDidExit();

#if ENABLE(MEDIA_STREAM)
    bool activeAudioCapture = isCapturingAudio() && preferences().captureAudioInGPUProcessEnabled();
    bool activeVideoCapture = isCapturingVideo() && preferences().captureVideoInGPUProcessEnabled();
    bool activeDisplayCapture = false;
    if (activeAudioCapture || activeVideoCapture) {
        auto& gpuProcess = process().processPool().ensureGPUProcess();
        gpuProcess.updateCaptureAccess(activeAudioCapture, activeVideoCapture, activeDisplayCapture, m_process->coreProcessIdentifier(), [] { });
#if PLATFORM(IOS_FAMILY)
        gpuProcess.setOrientationForMediaCapture(m_orientationForMediaCapture);
#endif
    }
#endif
}
#endif

#if ENABLE(MODEL_PROCESS)
void WebPageProxy::modelProcessDidFinishLaunching()
{
    protectedPageClient()->modelProcessDidFinishLaunching();
}

void WebPageProxy::modelProcessExited(ProcessTerminationReason)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInModelProcess = 0;
#endif

    protectedPageClient()->modelProcessDidExit();
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
        protectedThis->updateActivityState();
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

void WebPageProxy::requestMediaKeySystemPermissionByDefaultAction(const WebCore::SecurityOriginData&, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(true);
}

#if ENABLE(MEDIA_STREAM)

WebCore::CaptureSourceOrError WebPageProxy::createRealtimeMediaSourceForSpeechRecognition()
{
    auto captureDevice = SpeechRecognitionCaptureSource::findCaptureDevice();
    if (!captureDevice)
        return CaptureSourceOrError { { "No device is available for capture"_s, WebCore::MediaAccessDenialReason::PermissionDenied } };

    if (preferences().captureAudioInGPUProcessEnabled())
        return CaptureSourceOrError { SpeechRecognitionRemoteRealtimeMediaSource::create(m_process->ensureSpeechRecognitionRemoteRealtimeMediaSourceManager(), *captureDevice, internals().webPageID) };

#if PLATFORM(IOS_FAMILY)
    return CaptureSourceOrError { SpeechRecognitionRemoteRealtimeMediaSource::create(m_process->ensureSpeechRecognitionRemoteRealtimeMediaSourceManager(), *captureDevice, internals().webPageID) };
#else
    return SpeechRecognitionCaptureSource::createRealtimeMediaSource(*captureDevice, internals().webPageID);
#endif
}

#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
void WebPageProxy::setIndexOfGetDisplayMediaDeviceSelectedForTesting(std::optional<unsigned> index)
{
    DisplayCaptureSessionManager::singleton().setIndexOfDeviceSelectedForTesting(index);
}

void WebPageProxy::setSystemCanPromptForGetDisplayMediaForTesting(bool canPrompt)
{
    DisplayCaptureSessionManager::singleton().setSystemCanPromptForTesting(canPrompt);
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

void WebPageProxy::modelInlinePreviewDidLoad(WebCore::PlatformLayerIdentifier layerID)
{
    send(Messages::WebPage::ModelInlinePreviewDidLoad(layerID));
}

void WebPageProxy::modelInlinePreviewDidFailToLoad(WebCore::PlatformLayerIdentifier layerID, const WebCore::ResourceError& error)
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

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void WebPageProxy::createMediaSessionCoordinator(Ref<MediaSessionCoordinatorProxyPrivate>&& privateCoordinator, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::CreateMediaSessionCoordinator(privateCoordinator->identifier()), [weakThis = WeakPtr { *this }, privateCoordinator = WTFMove(privateCoordinator), completionHandler = WTFMove(completionHandler)](bool success) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !success) {
            completionHandler(false);
            return;
        }

        protectedThis->m_mediaSessionCoordinatorProxy = RemoteMediaSessionCoordinatorProxy::create(*protectedThis, WTFMove(privateCoordinator));
        completionHandler(true);
    });
}
#endif

void WebPageProxy::requestScrollToRect(const FloatRect& targetRect, const FloatPoint& origin)
{
    protectedPageClient()->requestScrollToRect(targetRect, origin);
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
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AppPrivacyReportTestingData(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::clearAppPrivacyReportTestingData(CompletionHandler<void()>&& completionHandler)
{
    websiteDataStore().protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearAppPrivacyReportTestingData(m_websiteDataStore->sessionID()), WTFMove(completionHandler));
}
#endif

void WebPageProxy::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    m_uiClient->requestCookieConsent(WTFMove(completion));
}

#if ENABLE(VIDEO)
void WebPageProxy::beginTextRecognitionForVideoInElementFullScreen(MediaPlayerIdentifier identifier, FloatRect bounds)
{
    if (!protectedPageClient()->isTextRecognitionInFullscreenVideoEnabled())
        return;

#if ENABLE(GPU_PROCESS)
    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess)
        return;

    m_isPerformingTextRecognitionInElementFullScreen = true;
    gpuProcess->requestBitmapImageForCurrentTime(m_process->coreProcessIdentifier(), identifier, [weakThis = WeakPtr { *this }, bounds](std::optional<ShareableBitmap::Handle>&& bitmapHandle) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_isPerformingTextRecognitionInElementFullScreen)
            return;

        if (!bitmapHandle)
            return;

        protectedThis->protectedPageClient()->beginTextRecognitionForVideoInElementFullscreen(WTFMove(*bitmapHandle), bounds);
        protectedThis->m_isPerformingTextRecognitionInElementFullScreen = false;
    });
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(bounds);
#endif
}

void WebPageProxy::cancelTextRecognitionForVideoInElementFullScreen()
{
    if (!protectedPageClient()->isTextRecognitionInFullscreenVideoEnabled())
        return;

    m_isPerformingTextRecognitionInElementFullScreen = false;
    protectedPageClient()->cancelTextRecognitionForVideoInElementFullscreen();
}
#endif

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

#if ENABLE(NETWORK_ISSUE_REPORTING)

void WebPageProxy::reportNetworkIssue(const URL& requestURL)
{
    if (m_networkIssueReporter)
        m_networkIssueReporter->report(requestURL);
}

#endif // ENABLE(NETWORK_ISSUE_REPORTING)

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
    if (useGPUProcessForDOMRenderingEnabled()) {
        RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
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

void WebPageProxy::addRemotePageProxy(const WebCore::RegistrableDomain& domain, RemotePageProxy& remotePageProxy)
{
    auto addResult = internals().remotePageProxyState.domainToRemotePageProxyMap.add(domain, remotePageProxy);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    m_browsingContextGroup->addProcessForDomain(domain, remotePageProxy.process());
}

void WebPageProxy::removeRemotePageProxy(const WebCore::RegistrableDomain& domain)
{
    internals().remotePageProxyState.domainToRemotePageProxyMap.remove(domain);
}

WebProcessProxy* WebPageProxy::processForRegistrableDomain(const WebCore::RegistrableDomain& domain)
{
    return m_browsingContextGroup->processForDomain(domain);
}

RemotePageProxy* WebPageProxy::remotePageProxyForRegistrableDomain(const WebCore::RegistrableDomain& domain) const
{
    return internals().remotePageProxyState.domainToRemotePageProxyMap.get(domain).get();
}

void WebPageProxy::setRemotePageProxyInOpenerProcess(Ref<RemotePageProxy>&& page)
{
    internals().remotePageProxyState.remotePageProxyInOpenerProcess = WTFMove(page);
}

RefPtr<RemotePageProxy> WebPageProxy::takeRemotePageProxyInOpenerProcessIfDomainEquals(const WebCore::RegistrableDomain& domain)
{
    auto& state = internals().remotePageProxyState;
    if (!state.remotePageProxyInOpenerProcess)
        return nullptr;
    if (state.remotePageProxyInOpenerProcess->domain() != domain)
        return nullptr;
    return std::exchange(state.remotePageProxyInOpenerProcess, nullptr);
}

void WebPageProxy::removeOpenedRemotePageProxy(WebPageProxyIdentifier pageID)
{
    internals().remotePageProxyState.openedRemotePageProxies.remove(pageID);
}

RefPtr<RemotePageProxy> WebPageProxy::takeOpenedRemotePageProxyIfDomainEquals(const WebCore::RegistrableDomain& domain)
{
    auto& map = internals().remotePageProxyState.openedRemotePageProxies;
    // FIXME: <rdar://121240859> Use better data structures so we don't need to iterate the whole map.
    for (auto it = map.begin(); it != map.end(); ++it) {
        Ref remotePageProxy = it->value;
        if (remotePageProxy->domain() == domain) {
            map.remove(it);
            return { WTFMove(remotePageProxy) };
        }
    }
    return nullptr;
}

void WebPageProxy::addOpenedRemotePageProxy(WebPageProxyIdentifier pageID, Ref<RemotePageProxy>&& page)
{
    // FIXME: When <rdar://116203552> is fixed we should be able to assert that the add result is a
    // new entry because we won't make an opened RemotePageProxy until we know what process
    // the opened page's main frame will end up in after all redirects.
    internals().remotePageProxyState.openedRemotePageProxies.add(pageID, WTFMove(page));
}

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

Vector<LinkDecorationFilteringData>& WebPageProxy::cachedAllowedQueryParametersForAdvancedPrivacyProtections()
{
    static NeverDestroyed cachedParameters = [] {
        return Vector<LinkDecorationFilteringData> { };
    }();
    return cachedParameters.get();
}

void WebPageProxy::updateAllowedQueryParametersForAdvancedPrivacyProtectionsIfNeeded()
{
    if (!m_shouldUpdateAllowedQueryParametersForAdvancedPrivacyProtections)
        return;

    m_shouldUpdateAllowedQueryParametersForAdvancedPrivacyProtections = false;

    if (!cachedAllowedQueryParametersForAdvancedPrivacyProtections().isEmpty())
        return;

    requestLinkDecorationFilteringData([weakPage = WeakPtr { *this }](auto&& data) mutable {
        if (cachedAllowedQueryParametersForAdvancedPrivacyProtections().isEmpty()) {
            cachedAllowedQueryParametersForAdvancedPrivacyProtections() = WTFMove(data);
            cachedAllowedQueryParametersForAdvancedPrivacyProtections().shrinkToFit();
        }

        if (RefPtr page = weakPage.get(); page && page->hasRunningProcess())
            page->send(Messages::WebPage::SetAllowedQueryParametersForAdvancedPrivacyProtections(cachedAllowedQueryParametersForAdvancedPrivacyProtections()));
    });
}

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

void WebPageProxy::sendCachedLinkDecorationFilteringData()
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (!hasRunningProcess())
        return;

    if (LinkDecorationFilteringController::shared().cachedStrings().isEmpty())
        return;

    m_needsInitialLinkDecorationFilteringData = false;
    send(Messages::WebPage::SetLinkDecorationFilteringData(LinkDecorationFilteringController::shared().cachedStrings()));
#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
}

void WebPageProxy::waitForInitialLinkDecorationFilteringData(WebFramePolicyListenerProxy& listener)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    LinkDecorationFilteringController::shared().updateStrings([listener = Ref { listener }] {
        listener->didReceiveInitialLinkDecorationFilteringData();
    });
#else
    listener.didReceiveInitialLinkDecorationFilteringData();
#endif
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebPageProxy::pauseAllAnimations(CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::PauseAllAnimations(), WTFMove(completionHandler));
}

void WebPageProxy::playAllAnimations(CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::PlayAllAnimations(), WTFMove(completionHandler));
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

void WebPageProxy::adjustLayersForLayoutViewport(const FloatPoint& scrollPosition, const WebCore::FloatRect& layoutViewport, double scale)
{
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (!m_scrollingCoordinatorProxy)
        return;

    m_scrollingCoordinatorProxy->viewportChangedViaDelegatedScrolling(scrollPosition, layoutViewport, scale);
#endif
}

String WebPageProxy::scrollbarStateForScrollingNodeID(int scrollingNodeID, bool isVertical)
{
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    if (!m_scrollingCoordinatorProxy)
        return ""_s;
    return m_scrollingCoordinatorProxy->scrollbarStateForScrollingNodeID(scrollingNodeID, isVertical);
#else
    return ""_s;
#endif
}

PageIdentifier WebPageProxy::webPageID() const
{
    return internals().webPageID;
}

WebCore::PageIdentifier WebPageProxy::webPageIDInProcessForDomain(const WebCore::RegistrableDomain& domain) const
{
    if (auto* remotePageProxy = remotePageProxyForRegistrableDomain(domain))
        return remotePageProxy->pageID();
    return internals().webPageID;
}

WebPopupMenuProxyClient& WebPageProxy::popupMenuClient()
{
    return internals();
}

const PageLoadState& WebPageProxy::pageLoadState() const
{
    return internals().pageLoadState;
}

PageLoadState& WebPageProxy::pageLoadState()
{
    return internals().pageLoadState;
}

void WebPageProxy::isLoadingChanged()
{
    activityStateDidChange(ActivityState::IsLoading);
}

GeolocationPermissionRequestManagerProxy& WebPageProxy::geolocationPermissionRequestManager()
{
    return internals().geolocationPermissionRequestManager;
}

auto WebPageProxy::identifier() const -> Identifier
{
    return internals().identifier;
}

ScrollPinningBehavior WebPageProxy::scrollPinningBehavior() const
{
    return internals().scrollPinningBehavior;
}

IntRect WebPageProxy::visibleScrollerThumbRect() const
{
    return internals().visibleScrollerThumbRect;
}

IntSize WebPageProxy::minimumSizeForAutoLayout() const
{
    return internals().minimumSizeForAutoLayout;
}

IntSize WebPageProxy::sizeToContentAutoSizeMaximumSize() const
{
    return internals().sizeToContentAutoSizeMaximumSize;
}

FloatSize WebPageProxy::viewportSizeForCSSViewportUnits() const
{
    return valueOrDefault(internals().viewportSizeForCSSViewportUnits);
}

void WebPageProxy::didCreateSleepDisabler(SleepDisablerIdentifier identifier, const String& reason, bool display)
{
    MESSAGE_CHECK(m_process, !reason.isNull());
    auto sleepDisabler = makeUnique<WebCore::SleepDisabler>(reason, display ? PAL::SleepDisabler::Type::Display : PAL::SleepDisabler::Type::System, webPageID());
    internals().sleepDisablers.add(identifier, WTFMove(sleepDisabler));
}

void WebPageProxy::didDestroySleepDisabler(SleepDisablerIdentifier identifier)
{
    internals().sleepDisablers.remove(identifier);
}

bool WebPageProxy::hasSleepDisabler() const
{
    return !internals().sleepDisablers.isEmpty();
}

#if USE(SYSTEM_PREVIEW)
void WebPageProxy::beginSystemPreview(const URL& url, const SecurityOriginData& topOrigin, const SystemPreviewInfo& systemPreviewInfo, CompletionHandler<void()>&& completionHandler)
{
    if (!m_systemPreviewController)
        return completionHandler();
    m_systemPreviewController->begin(url, topOrigin, systemPreviewInfo, WTFMove(completionHandler));
}

void WebPageProxy::setSystemPreviewCompletionHandlerForLoadTesting(CompletionHandler<void(bool)>&& handler)
{
    if (m_systemPreviewController)
        m_systemPreviewController->setCompletionHandlerForLoadTesting(WTFMove(handler));
}
#endif

void WebPageProxy::useRedirectionForCurrentNavigation(const ResourceResponse& response)
{
    ASSERT(response.isRedirection());
    send(Messages::WebPage::UseRedirectionForCurrentNavigation(response));
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

void WebPageProxy::didAccessWindowProxyPropertyViaOpenerForFrame(FrameIdentifier frameID, const SecurityOriginData& parentOrigin, WindowProxyProperty property)
{
    if (!internals().frameLoadStateObserver)
        return;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);

    RegistrableDomain parentDomain { parentOrigin };

    bool isMostRecentDomain = true;
    for (auto& childDomain : internals().frameLoadStateObserver->visitedDomains()) {
        // If we already told the embedder about this domain/property pair before, don't tell them again.
        auto result = internals().windowOpenerAccessedProperties.add(childDomain, OptionSet<WindowProxyProperty> { });
        if (result.iterator->value.contains(property))
            continue;
        result.iterator->value.add(property);

        websiteDataStore().client().didAccessWindowProxyProperty(parentDomain, childDomain, property, isMostRecentDomain);
        isMostRecentDomain = false;
    }
}

#endif

void WebPageProxy::dispatchLoadEventToFrameOwnerElement(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    RefPtr parentFrame = frame->parentFrame();
    if (!parentFrame)
        return;

    sendToProcessContainingFrame(parentFrame->frameID(), Messages::WebPage::DispatchLoadEventToFrameOwnerElement(frameID));
}

Ref<VisitedLinkStore> WebPageProxy::protectedVisitedLinkStore()
{
    return m_visitedLinkStore;
}

void WebPageProxy::broadcastFocusedFrameToOtherProcesses(IPC::Connection& connection, const WebCore::FrameIdentifier& frameID)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::FrameWasFocusedInAnotherProcess(frameID), pageID);
    });
}

Ref<WebPreferences> WebPageProxy::protectedPreferences() const
{
    return m_preferences;
}

Ref<WebPageGroup> WebPageProxy::protectedPageGroup() const
{
    return m_pageGroup;
}

Ref<WebsiteDataStore> WebPageProxy::protectedWebsiteDataStore() const
{
    return m_websiteDataStore;
}

Ref<WebBackForwardList> WebPageProxy::protectedBackForwardList() const
{
    return m_backForwardList;
}

template<typename F>
decltype(auto) WebPageProxy::sendToWebPage(std::optional<FrameIdentifier> frameID, F&& sendFunction)
{
    if (frameID) {
        if (RefPtr frame = WebFrameProxy::webFrame(*frameID)) {
            if (RefPtr remotePageProxy = frame->remotePageProxy())
                return sendFunction(*remotePageProxy);
        }
    }
    return sendFunction(*this);
}

template<typename M, typename C>
void WebPageProxy::sendToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message, C&& completionHandler)
{
    sendToWebPage(frameID,
        [&message, &completionHandler] (auto& targetPage) {
            targetPage.sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler));
        }
    );
}

template<typename M>
void WebPageProxy::sendToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message)
{
    sendToWebPage(frameID,
        [&message] (auto& targetPage) {
            targetPage.send(std::forward<M>(message));
        }
    );
}

template<typename M>
IPC::ConnectionSendSyncResult<M> WebPageProxy::sendSyncToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message)
{
    return sendToWebPage(frameID,
        [&message] (auto& targetPage) {
            return targetPage.sendSync(std::forward<M>(message));
        }
    );
}

void WebPageProxy::focusRemoteFrame(IPC::Connection& connection, WebCore::FrameIdentifier frameID)
{
    RefPtr destinationFrame = WebFrameProxy::webFrame(frameID);
    if (!destinationFrame || !destinationFrame->isMainFrame())
        return;

    ASSERT(destinationFrame->page() == this);

    broadcastFocusedFrameToOtherProcesses(connection, frameID);
    setFocus(true);
}

void WebPageProxy::postMessageToRemote(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData> targetOrigin, const WebCore::MessageWithMessagePorts& message)
{
    sendToProcessContainingFrame(target, Messages::WebPage::RemotePostMessage(source, sourceOrigin, target, targetOrigin, message));
}

void WebPageProxy::renderTreeAsText(WebCore::FrameIdentifier frameID, size_t baseIndent, OptionSet<WebCore::RenderAsTextFlag> behavior, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto sendResult = sendSyncToProcessContainingFrame(frameID, Messages::WebPage::RenderTreeAsText(frameID, baseIndent, behavior));
    if (!sendResult.succeeded())
        return completionHandler("Test Error - sending WebPage::RenderTreeAsText failed"_s);

    auto [result] = sendResult.takeReply();
    completionHandler(WTFMove(result));
}

void WebPageProxy::requestTextExtraction(std::optional<FloatRect>&& collectionRectInRootView, CompletionHandler<void(WebCore::TextExtraction::Item&&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });
    sendWithAsyncReply(Messages::WebPage::RequestTextExtraction(WTFMove(collectionRectInRootView)), WTFMove(completion));
}

void WebPageProxy::addConsoleMessage(FrameIdentifier frameID, MessageSource messageSource, MessageLevel messageLevel, const String& message, std::optional<ResourceLoaderIdentifier> coreIdentifier)
{
    send(Messages::WebPage::AddConsoleMessage { frameID, messageSource, messageLevel, message, coreIdentifier });
}

} // namespace WebKit

#undef WEBPAGEPROXY_RELEASE_LOG
#undef WEBPAGEPROXY_RELEASE_LOG_ERROR
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
