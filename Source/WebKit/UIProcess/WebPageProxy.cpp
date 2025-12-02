/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "APITargetedElementInfo.h"
#include "APITargetedElementRequest.h"
#include "APITextRun.h"
#include "APIUIClient.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "APIWebsitePolicies.h"
#include "AboutSchemeHandler.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "AuthenticationManager.h"
#include "AuthenticatorManager.h"
#include "BrowsingContextGroup.h"
#include "BrowsingWarning.h"
#include "CallbackID.h"
#include "ColorControlSupportsAlpha.h"
#include "Connection.h"
#include "DidFilterKnownLinkDecoration.h"
#include "DigitalCredentialsCoordinatorMessages.h"
#include "DownloadManager.h"
#include "DownloadProxy.h"
#include "DragControllerAction.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxy.h"
#include "DrawingAreaProxyMessages.h"
#include "EventDispatcherMessages.h"
#include "FindStringCallbackAggregator.h"
#include "FindTextMatchesCallbackAggregator.h"
#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "FrameProcess.h"
#include "FrameTreeCreationParameters.h"
#include "FrameTreeNodeData.h"
#include "GamepadData.h"
#include "GoToBackForwardItemParameters.h"
#include "ImageOptions.h"
#include "JavaScriptEvaluationResult.h"
#include "LegacyGlobalSettings.h"
#include "LoadParameters.h"
#include "LoadedWebArchive.h"
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
#include "NodeHitTestResult.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "NotificationPermissionRequest.h"
#include "NotificationPermissionRequestManager.h"
#include "PageClient.h"
#include "PlatformPopupMenuData.h"
#include "PlatformXRSystem.h"
#include "PolicyDecision.h"
#include "PrintInfo.h"
#include "ProcessAssertion.h"
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "RemoteWebTouchEvent.h"
#include "RestrictedOpenerType.h"
#include "RunJavaScriptParameters.h"
#include "SharedBufferReference.h"
#include "SpeechRecognitionPermissionManager.h"
#include "SpeechRecognitionRemoteRealtimeMediaSource.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManager.h"
#include "SuspendedPageProxy.h"
#include "SwiftDemoLogoConfirmation.h"
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
#include "WebAutomationSessionProxyMessages.h"
#include "WebBackForwardCache.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListCounts.h"
#include "WebBackForwardListFrameItem.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListMessages.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuProxy.h"
#include "WebDateTimePicker.h"
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
#include "WebInspectorBackendMessages.h"
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
#include "WebPageLoadTiming.h"
#include "WebPageMessages.h"
#include "WebPageNetworkParameters.h"
#include "WebPageProxyInternals.h"
#include "WebPageProxyMessages.h"
#include "WebPageProxyTesting.h"
#include "WebPageTestingMessages.h"
#include "WebPasteboardProxy.h"
#include "WebPopupItem.h"
#include "WebPreferences.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include "WebProcessActivityState.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebScreenOrientationManagerProxy.h"
#include "WebSpeechSynthesisVoice.h"
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
#include <WebCore/BoxExtents.h>
#include <WebCore/CaptureDeviceManager.h>
#include <WebCore/CaptureDeviceWithCapabilities.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/CompositionHighlight.h>
#include <WebCore/CrossSiteNavigationDataTransfer.h>
#include <WebCore/CryptoKey.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DigitalCredentialRequest.h>
#include <WebCore/DigitalCredentialRequestOptions.h>
#include <WebCore/DigitalCredentialsProtocols.h>
#include <WebCore/DigitalCredentialsRequestData.h>
#include <WebCore/DigitalCredentialsResponseData.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/ElementContext.h>
#include <WebCore/EventNames.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/ExceptionDetails.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/FocusOptions.h>
#include <WebCore/FontAttributeChanges.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/GlobalFrameIdentifier.h>
#include <WebCore/GlobalWindowIdentifier.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LegacySchemeRegistry.h>
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
#include <WebCore/ProcessSwapDisposition.h>
#include <WebCore/PublicSuffixStore.h>
#include <WebCore/Quirks.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/RunJavaScriptParameters.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ShareData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <WebCore/Site.h>
#include <WebCore/SleepDisabler.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/SystemPreviewInfo.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextExtractionTypes.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextManipulationController.h>
#include <WebCore/TextManipulationItem.h>
#include <WebCore/ValidationBubble.h>
#include <WebCore/WindowFeatures.h>
#include <WebCore/WrappedCryptoKey.h>
#include <WebCore/WritingDirection.h>
#include <optional>
#include <ranges>
#include <stdio.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CoroutineUtilities.h>
#include <wtf/EnumTraits.h>
#include <wtf/FileSystem.h>
#include <wtf/ListHashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/NumberOfCores.h>
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/URLParser.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextStream.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "APIApplicationManifest.h"
#endif

#if PLATFORM(COCOA)
#include "RemoteScrollingCoordinatorMessages.h"
#include "RemoteScrollingCoordinatorProxy.h"
#endif

#if PLATFORM(COCOA)
#include "InsertTextOptions.h"
#include "NetworkIssueReporter.h"
#include "PlaybackSessionInterfaceLMK.h"
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteLayerTreeScrollingPerformanceData.h"
#include "VideoPresentationManagerProxy.h"
#include "VideoPresentationManagerProxyMessages.h"
#include "WKTextExtractionUtilities.h"
#include "WebPrivacyHelpers.h"
#include <WebCore/AttributedString.h>
#include <WebCore/CoreAudioCaptureDeviceManager.h>
#include <WebCore/LegacyWebArchive.h>
#include <WebCore/NullPlaybackSessionInterface.h>
#include <WebCore/PlaybackSessionInterfaceAVKitLegacy.h>
#include <WebCore/PlaybackSessionInterfaceMac.h>
#include <WebCore/PlaybackSessionInterfaceTVOS.h>
#include <WebCore/RunLoopObserver.h>
#include <WebCore/SystemBattery.h>
#include <objc/runtime.h>
#include <wtf/MachSendRight.h>
#include <wtf/cocoa/Entitlements.h>
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

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && USE(SKIA))
#include "ViewSnapshotStore.h"
#endif

#if PLATFORM(GTK)
#include <WebCore/SelectionData.h>
#endif

#if USE(CAIRO)
#include <WebCore/CairoUtilities.h>
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

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResult.h"
#endif

#if ENABLE(MEDIA_USAGE)
#include "MediaUsageManager.h"
#endif

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
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

#if PLATFORM(COCOA)
#include <wtf/spi/darwin/SandboxSPI.h>
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

#if ENABLE(SWIFT_DEMO_URI_SCHEME)
#include "WebKit-Swift.h"
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
#include "RemoteAudioSessionConfiguration.h"
#include "RemoteMediaSessionManagerProxy.h"
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

static constexpr Seconds resetRecentCrashCountDelay = 30_s;
static constexpr unsigned maximumWebProcessRelaunchAttempts = 1;
static constexpr Seconds tryCloseTimeoutDelay = 50_ms;

#if USE(RUNNINGBOARD)
static constexpr Seconds audibleActivityClearDelay = 10_s;
#endif

#if PLATFORM(COCOA)
static WorkQueue& sharedFileQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("com.apple.WebKit.WebPageSharedFileQueue"_s));
    return queue.get();
}
#endif

class StorageRequests {
    WTF_MAKE_TZONE_ALLOCATED(StorageRequests);
    WTF_MAKE_NONCOPYABLE(StorageRequests);
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(StorageRequests);

StorageRequests& StorageRequests::singleton()
{
    static NeverDestroyed<StorageRequests> requests;
    return requests;
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
Ref<WebPageProxyFrameLoadStateObserver> WebPageProxyFrameLoadStateObserver::create()
{
    return adoptRef(*new WebPageProxyFrameLoadStateObserver);
}

WebPageProxyFrameLoadStateObserver::WebPageProxyFrameLoadStateObserver() = default;

WebPageProxyFrameLoadStateObserver::~WebPageProxyFrameLoadStateObserver() = default;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageProxyFrameLoadStateObserver);

#endif // #if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

void WebPageProxy::forMostVisibleWebPageIfAny(PAL::SessionID sessionID, const SecurityOriginData& origin, CompletionHandler<void(WebPageProxy*)>&& completionHandler)
{
    // FIXME: If not finding right away a visible page, we might want to try again for a given period of time when there is a change of visibility.
    RefPtr<WebPageProxy> selectedPage;
    WebProcessProxy::forWebPagesWithOrigin(sessionID, origin, [&](auto& page) {
        if (!page.mainFrame())
            return;
        if (page.isViewVisible() && (!selectedPage || !selectedPage->isViewVisible())) {
            selectedPage = page;
            return;
        }
        if (page.isViewFocused() && (!selectedPage || !selectedPage->isViewFocused())) {
            selectedPage = page;
            return;
        }
    });
    completionHandler(selectedPage.get());
}

Ref<WebPageProxy> WebPageProxy::create(PageClient& pageClient, WebProcessProxy& process, Ref<API::PageConfiguration>&& configuration)
{
    return adoptRef(*new WebPageProxy(pageClient, process, WTFMove(configuration)));
}

void WebPageProxy::takeVisibleActivity()
{
    m_mainFrameProcessActivityState->takeVisibleActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().takeVisibleActivity();
    });
}

void WebPageProxy::takeAudibleActivity()
{
    m_mainFrameProcessActivityState->takeAudibleActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().takeAudibleActivity();
    });
}

void WebPageProxy::takeCapturingActivity()
{
    m_mainFrameProcessActivityState->takeCapturingActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().takeCapturingActivity();
    });
}

void WebPageProxy::takeMutedCaptureAssertion()
{
    m_mainFrameProcessActivityState->takeMutedCaptureAssertion();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().takeMutedCaptureAssertion();
    });
}

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
void WebPageProxy::takeAccessibilityActivityWhenInWindow()
{
    m_mainFrameProcessActivityState->takeAccessibilityActivityWhenInWindow();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().takeAccessibilityActivityWhenInWindow();
    });
}

bool WebPageProxy::hasAccessibilityActivityForTesting()
{
    if (!m_mainFrameProcessActivityState->hasAccessibilityActivityForTesting())
        return false;

    bool result = true;
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&result](auto& remotePageProxy) {
        result = result || remotePageProxy.processActivityState().hasAccessibilityActivityForTesting();
    });

    return result;
}
#endif

void WebPageProxy::resetActivityState()
{
    m_mainFrameProcessActivityState->reset();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().reset();
    });
}

void WebPageProxy::dropVisibleActivity()
{
    m_mainFrameProcessActivityState->dropVisibleActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().dropVisibleActivity();
    });
}

void WebPageProxy::dropAudibleActivity()
{
    m_mainFrameProcessActivityState->dropAudibleActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().dropAudibleActivity();
    });
}

void WebPageProxy::dropCapturingActivity()
{
    m_mainFrameProcessActivityState->dropCapturingActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().dropCapturingActivity();
    });
}

void WebPageProxy::dropMutedCaptureAssertion()
{
    m_mainFrameProcessActivityState->dropMutedCaptureAssertion();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().dropMutedCaptureAssertion();
    });
}

bool WebPageProxy::hasValidVisibleActivity() const
{
    bool hasValidVisibleActivity = m_mainFrameProcessActivityState->hasValidVisibleActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&](auto& remotePageProxy) {
        hasValidVisibleActivity &= remotePageProxy.processActivityState().hasValidVisibleActivity();
    });
    return hasValidVisibleActivity;
}

bool WebPageProxy::hasValidAudibleActivity() const
{
    bool hasValidAudibleActivity = m_mainFrameProcessActivityState->hasValidAudibleActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&](auto& remotePageProxy) {
        hasValidAudibleActivity &= remotePageProxy.processActivityState().hasValidAudibleActivity();
    });
    return hasValidAudibleActivity;
}

bool WebPageProxy::hasValidCapturingActivity() const
{
    bool hasValidCapturingActivity = m_mainFrameProcessActivityState->hasValidCapturingActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&](auto& remotePageProxy) {
        hasValidCapturingActivity &= remotePageProxy.processActivityState().hasValidCapturingActivity();
    });
    return hasValidCapturingActivity;
}

bool WebPageProxy::hasValidMutedCaptureAssertion() const
{
    bool hasValidMutedCaptureAssertion = m_mainFrameProcessActivityState->hasValidMutedCaptureAssertion();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&](auto& remotePageProxy) {
        hasValidMutedCaptureAssertion &= remotePageProxy.processActivityState().hasValidMutedCaptureAssertion();
    });
    return hasValidMutedCaptureAssertion;
}

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::takeOpeningAppLinkActivity()
{
    m_mainFrameProcessActivityState->takeOpeningAppLinkActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().takeOpeningAppLinkActivity();
    });
}

void WebPageProxy::dropOpeningAppLinkActivity()
{
    m_mainFrameProcessActivityState->dropOpeningAppLinkActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().dropOpeningAppLinkActivity();
    });
}

bool WebPageProxy::hasValidOpeningAppLinkActivity() const
{
    bool hasValidOpeningAppLinkActivity = m_mainFrameProcessActivityState->hasValidOpeningAppLinkActivity();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&](auto& remotePageProxy) {
        hasValidOpeningAppLinkActivity &= remotePageProxy.processActivityState().hasValidOpeningAppLinkActivity();
    });
    return hasValidOpeningAppLinkActivity;
}
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)

void WebPageProxy::updateWebProcessSuspensionDelay()
{
    m_mainFrameProcessActivityState->updateWebProcessSuspensionDelay();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().updateWebProcessSuspensionDelay();
    });
}

#endif

WebPageProxy::Internals::Internals(WebPageProxy& page)
    : page(page)
    , audibleActivityTimer(RunLoop::mainSingleton(), "WebPageProxy::Internals::AudibleActivityTimer"_s, &page, &WebPageProxy::clearAudibleActivity)
    , geolocationPermissionRequestManager(page)
    , updatePlayingMediaDidChangeTimer(RunLoop::mainSingleton(), "WebPageProxy::Internals::UpdatePlayingMediaDidChangeTimer"_s, &page, &WebPageProxy::updatePlayingMediaDidChangeTimerFired)
    , notificationManagerMessageHandler(page)
    , pageLoadState(page)
    , resetRecentCrashCountTimer(RunLoop::mainSingleton(), "WebPageProxy::Internals::ResetRecentCrashCountTimer"_s, &page, &WebPageProxy::resetRecentCrashCount)
    , tryCloseTimeoutTimer(RunLoop::mainSingleton(), "WebPageProxy::Internals::TryCloseTimeoutTimer"_s, &page, &WebPageProxy::tryCloseTimedOut)
    , updateReportedMediaCaptureStateTimer(RunLoop::mainSingleton(), "updateReportedMediaCaptureStateTimer"_s, &page, &WebPageProxy::updateReportedMediaCaptureState)
#if ENABLE(GAMEPAD)
    , recentGamepadAccessHysteresis([weakPage = WeakPtr { page }](PAL::HysteresisState state) {
        if (RefPtr page = weakPage.get())
            page->recentGamepadAccessStateChanged(state);
    }, gamepadsRecentlyAccessedThreshold)
#endif

#if HAVE(DISPLAY_LINK)
    , wheelEventActivityHysteresis([weakPage = WeakPtr { page }](PAL::HysteresisState state) {
        if (RefPtr page = weakPage.get())
            page->wheelEventHysteresisUpdated(state);
    })
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    , fullscreenVideoTextRecognitionTimer(RunLoop::mainSingleton(), "WebPageProxy::Internals::FullscreenVideoTextRecognitionTimer"_s, &page, &WebPageProxy::fullscreenVideoTextRecognitionTimerFired)
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    , activityStateChangeTimer(RunLoop::mainSingleton(), "WebPageProxy::Internals::activityStateChangeTimer"_s, &page, &WebPageProxy::dispatchActivityStateChange)
#endif
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    // Give the events causing activity state changes more priority than the change timer.
    activityStateChangeTimer.setPriority(RunLoopSourcePriority::RunLoopTimer + 1);
#endif
}

#if !PLATFORM(COCOA)
WebPageProxy::Internals::~Internals() = default;
#endif

#if PLATFORM(MAC)
// FIXME: Remove this once the cause of rdar://148942809 is found and fixed.
static std::optional<API::PageConfiguration::OpenerInfo>& openerInfoOfPageBeingOpened()
{
    static NeverDestroyed<std::optional<API::PageConfiguration::OpenerInfo>> info;
    return info.get();
}
#endif

static HashMap<WebPageProxyIdentifier, WeakPtr<WebPageProxy>>& webPageProxyMap()
{
    static MainRunLoopNeverDestroyed<HashMap<WebPageProxyIdentifier, WeakPtr<WebPageProxy>>> map;
    return map.get();
}

WebPageProxy* WebPageProxy::fromIdentifier(std::optional<WebPageProxyIdentifier> identifier)
{
    return identifier ? webPageProxyMap().get(*identifier) : nullptr;
}

static bool windowFeature(auto getter, const API::PageConfiguration& configuration)
{
    if (!configuration.windowFeatures())
        return true;
    auto optional = getter(*configuration.windowFeatures());
    if (!optional)
        return true;
    return *optional;
}

WebPageProxy::WebPageProxy(PageClient& pageClient, WebProcessProxy& process, Ref<API::PageConfiguration>&& configuration)
    : m_internals(makeUniqueRefWithoutRefCountedCheck<Internals>(*this))
    , m_identifier(Identifier::generate())
    , m_webPageID(PageIdentifier::generate())
    , m_pageClient(pageClient)
    , m_configuration(configuration)
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
    , m_navigationState(makeUniqueRefWithoutRefCountedCheck<WebNavigationState>(*this))
    , m_generatePageLoadTimingTimer(RunLoop::mainSingleton(), "WebPageProxy::GeneratePageLoadTimingTimer"_s, this, &WebPageProxy::didEndNetworkRequestsForPageLoadTimingTimerFired)
#if PLATFORM(COCOA)
    , m_textIndicatorFadeTimer(RunLoop::mainSingleton(), "WebPageProxy::TextIndicatorFadeTimer"_s, this, &WebPageProxy::startTextIndicatorFadeOut)
#endif
    , m_legacyMainFrameProcess(process)
    , m_pageGroup(*configuration->pageGroup())
    , m_preferences(configuration->preferences())
    , m_userContentController(configuration->userContentController())
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    , m_webExtensionController(configuration->webExtensionController())
    , m_weakWebExtensionController(configuration->weakWebExtensionController())
#endif
    , m_visitedLinkStore(configuration->visitedLinkStore())
    , m_websiteDataStore(configuration->websiteDataStore())
    , m_userAgent(standardUserAgent())
    , m_overrideContentSecurityPolicy { configuration->overrideContentSecurityPolicy() }
    , m_openedMainFrameName { configuration->openedMainFrameName() }
#if ENABLE(FULLSCREEN_API)
    , m_fullscreenClient(makeUnique<API::FullscreenClient>())
#endif
    , m_mainFrameProcessActivityState(makeUniqueRef<WebProcessActivityState>(*this))
    , m_initialCapitalizationEnabled(configuration->initialCapitalizationEnabled())
    , m_cpuLimit(configuration->cpuLimit())
    , m_backForwardList(WebBackForwardList::create(*this))
    , m_waitsForPaintAfterViewDidMoveToWindow(configuration->waitsForPaintAfterViewDidMoveToWindow())
    , m_controlledByAutomation(configuration->isControlledByAutomation())
#if PLATFORM(COCOA)
    , m_isSmartInsertDeleteEnabled(TextChecker::isSmartInsertDeleteEnabled())
#endif
    , m_inspectorController(makeUniqueRef<WebPageInspectorController>(*this))
#if ENABLE(REMOTE_INSPECTOR)
    , m_inspectorDebuggable(WebPageDebuggable::create(*this))
#endif
    , m_corsDisablingPatterns(configuration->corsDisablingPatterns())
#if ENABLE(APP_BOUND_DOMAINS)
    , m_ignoresAppBoundDomains(m_configuration->ignoresAppBoundDomains())
    , m_limitsNavigationsToAppBoundDomains(m_configuration->limitsNavigationsToAppBoundDomains())
#endif
    , m_browsingContextGroup(configuration->openerInfo() ? configuration->openerInfo()->browsingContextGroup : BrowsingContextGroup::create())
    , m_openerFrameIdentifier(configuration->openerInfo() ? std::optional(configuration->openerInfo()->frameID) : std::nullopt)
#if HAVE(AUDIT_TOKEN)
    , m_presentingApplicationAuditToken(process.processPool().configuration().presentingApplicationProcessToken())
#endif
    , m_aboutSchemeHandler(AboutSchemeHandler::create())
    , m_pageForTesting(WebPageProxyTesting::create(*this))
    , m_statusBarIsVisible(windowFeature([] (auto& features) { return features.statusBarVisible; }, configuration))
    , m_menuBarIsVisible(windowFeature([] (auto& features) { return features.menuBarVisible; }, configuration))
    , m_toolbarsAreVisible(windowFeature([] (auto& features) { return features.toolBarVisible; }, configuration)
        || windowFeature([] (auto& features) { return features.locationBarVisible; }, configuration))
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "constructor, site isolation enabled %d", protectedPreferences()->siteIsolationEnabled());

    ASSERT(!webPageProxyMap().contains(m_identifier));
    webPageProxyMap().set(m_identifier, this);

#if PLATFORM(MAC)
    if (openerInfoOfPageBeingOpened() && openerInfoOfPageBeingOpened() != m_configuration->openerInfo())
        RELEASE_LOG_FAULT(Process, "Created WebPageProxy with wrong configuration");
#endif
    m_configuration->consumeOpenerInfo();

    if (!configuration->drawsBackground())
        internals().backgroundColor = Color(Color::transparentBlack);

    updateActivityState();
    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();

    platformInitialize();

    WebProcessPool::statistics().wkPageCount++;

    protectedPreferences()->addPage(*this);

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->addPage(*this);
#endif

    m_inspector = WebInspectorUIProxy::create(*this);

    if (hasRunningProcess())
        didAttachToRunningProcess();

    addAllMessageReceivers();

#if PLATFORM(IOS_FAMILY)
    DeprecatedGlobalSettings::setDisableScreenSizeOverride(m_preferences->disableScreenSizeOverride());

    if (m_configuration->preferences().serviceWorkerEntitlementDisabledForTesting())
        disableServiceWorkerEntitlementInNetworkProcess();
#endif

#if PLATFORM(COCOA)
    m_activityStateChangeDispatcher = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::ActivityStateChange, [this] {
        Ref { *this }->dispatchActivityStateChange();
    });
#endif

#if ENABLE(REMOTE_INSPECTOR)
    RefPtr inspectorDebuggable = m_inspectorDebuggable;
    inspectorDebuggable->setInspectable(JSRemoteInspectorGetInspectionEnabledByDefault());
    inspectorDebuggable->setPresentingApplicationPID(process.processPool().configuration().presentingApplicationPID());
    inspectorDebuggable->init();
#endif
    m_inspectorController->init();

#if ENABLE(WEBDRIVER_BIDI)
    if (m_controlledByAutomation) {
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->didCreatePage(*this);
    }
#endif

#if ENABLE(IPC_TESTING_API)
    if (m_preferences->ipcTestingAPIEnabled() && m_preferences->ignoreInvalidMessageWhenIPCTestingAPIEnabled())
        process.setIgnoreInvalidMessageForTesting();
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (protectedPreferences()->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::singleton().addWebPage(*this);
#endif

    m_pageToCloneSessionStorageFrom = configuration->pageToCloneSessionStorageFrom();

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    m_linkDecorationFilteringDataUpdateObserver = LinkDecorationFilteringController::sharedSingleton().observeUpdates([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendCachedLinkDecorationFilteringData();
    });

    if (protectedPreferences()->scriptTrackingPrivacyProtectionsEnabled())
        process.protectedProcessPool()->observeScriptTrackingPrivacyUpdatesIfNeeded();
#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

#if HAVE(AUDIT_TOKEN)
    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->setPresentingApplicationAuditToken(process.coreProcessIdentifier(), m_webPageID, m_presentingApplicationAuditToken);
#endif
    if (protectedPreferences()->siteIsolationEnabled())
        IPC::Connection::setShouldCrashOnMessageCheckFailure(true);
}

WebPageProxy::~WebPageProxy()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "destructor:");

    ASSERT(m_legacyMainFrameProcess->webPage(identifier()) != this);
#if ASSERT_ENABLED
    for (Ref page : m_legacyMainFrameProcess->pages())
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

#if PLATFORM(MACCATALYST)
    EndowmentStateTracker::singleton().removeClient(internals());
#endif

#if ENABLE(REMOTE_INSPECTOR)
    ASSERT(!m_inspectorDebuggable);
#endif

    for (auto& callback : m_nextActivityStateChangeCallbacks)
        callback();

    if (RefPtr networkProcess = websiteDataStore().networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::RemoveWebPageNetworkParameters(sessionID(), identifier()), 0);

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
    if (preferences->mediaSessionCoordinatorEnabled())
        GroupActivitiesSessionNotifier::singleton().removeWebPage(*this);
#endif

#if HAVE(AUDIT_TOKEN)
    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->setPresentingApplicationAuditToken(m_legacyMainFrameProcess->coreProcessIdentifier(), m_webPageID, std::nullopt);
#endif

    internals().updatePlayingMediaDidChangeTimer.stop();

    ASSERT(webPageProxyMap().get(m_identifier) == this);
    webPageProxyMap().remove(m_identifier);
}

Ref<WebPageProxy> WebPageProxy::Internals::protectedPage() const
{
    return page.get();
}

void WebPageProxy::addAllMessageReceivers()
{
    Ref process = m_legacyMainFrameProcess;
    internals().messageReceiverRegistration.startReceivingMessages(process, m_webPageID, *this, backForwardList());
    process->addMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_webPageID, internals().protectedNotificationManagerMessageHandler());
}

void WebPageProxy::removeAllMessageReceivers()
{
    internals().messageReceiverRegistration.stopReceivingMessages();
    protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_webPageID);
}

WebPageProxyMessageReceiverRegistration& WebPageProxy::messageReceiverRegistration()
{
    return internals().messageReceiverRegistration;
}

std::optional<SharedPreferencesForWebProcess> WebPageProxy::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

bool WebPageProxy::attachmentElementEnabled()
{
    return protectedPreferences()->attachmentElementEnabled();
}

bool WebPageProxy::modelElementEnabled()
{
    return protectedPreferences()->modelElementEnabled();
}

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
WebExtensionController* WebPageProxy::webExtensionController()
{
    return m_webExtensionController.get() ?: m_weakWebExtensionController.get();
}
#endif

// FIXME: Should return a const PageClient& and add a separate non-const
// version of this function, but several PageClient methods will need to become
// const for this to be possible.
PageClient* WebPageProxy::pageClient() const
{
    return m_pageClient.get();
}

RefPtr<PageClient> WebPageProxy::protectedPageClient() const
{
    return pageClient();
}

PAL::SessionID WebPageProxy::sessionID() const
{
    return m_websiteDataStore->sessionID();
}

RefPtr<WebFrameProxy> WebPageProxy::protectedMainFrame() const
{
    return m_mainFrame;
}

RefPtr<WebFrameProxy> WebPageProxy::protectedFocusedFrame() const
{
    return m_focusedFrame;
}

RefPtr<DrawingAreaProxy> WebPageProxy::protectedDrawingArea() const
{
    return m_drawingArea;
}

DrawingAreaProxy* WebPageProxy::provisionalDrawingArea() const
{
    if (m_provisionalPage && m_provisionalPage->drawingArea())
        return m_provisionalPage->drawingArea();
    return drawingArea();
}

ProcessID WebPageProxy::gpuProcessID() const
{
    if (m_isClosed)
        return 0;

#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = m_configuration->processPool().gpuProcess())
        return gpuProcess->processID();
#endif

    return 0;
}

Ref<WebProcessProxy> WebPageProxy::protectedLegacyMainFrameProcess() const
{
    return m_legacyMainFrameProcess;
}

ProcessID WebPageProxy::modelProcessID() const
{
    if (m_isClosed)
        return 0;

#if ENABLE(MODEL_PROCESS)
    if (auto* modelProcess = configuration().processPool().modelProcess())
        return modelProcess->processID();
#endif

    return 0;
}

ProcessID WebPageProxy::legacyMainFrameProcessID() const
{
    if (m_isClosed)
        return 0;

    return m_legacyMainFrameProcess->processID();
}

bool WebPageProxy::hasRunningProcess() const
{
    // A page that has been explicitly closed is never valid.
    if (m_isClosed)
        return false;

    return m_legacyMainFrameProcess->state() != WebProcessProxy::State::Terminated;
}

void WebPageProxy::notifyProcessPoolToPrewarm()
{
    m_configuration->protectedProcessPool()->didReachGoodTimeToPrewarm();
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

template<typename Message>
void WebPageProxy::send(Message&& message)
{
    protectedLegacyMainFrameProcess()->send(WTFMove(message), webPageIDInMainFrameProcess());
}

template<typename Message, typename CH>
void WebPageProxy::sendWithAsyncReply(Message&& message, CH&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler), webPageIDInMainFrameProcess());
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
    if (!iconLoadingClient)
        m_iconLoadingClient = makeUnique<API::IconLoadingClient>();
    else
        m_iconLoadingClient = WTFMove(iconLoadingClient);
}

void WebPageProxy::setPageLoadStateObserver(RefPtr<PageLoadState::Observer>&& observer)
{
    Ref protectedPageLoadState = pageLoadState();
    if (RefPtr pageLoadStateObserver = m_pageLoadStateObserver)
        protectedPageLoadState->removeObserver(*pageLoadStateObserver);
    m_pageLoadStateObserver = WTFMove(observer);
    if (RefPtr pageLoadStateObserver = m_pageLoadStateObserver)
        protectedPageLoadState->addObserver(*pageLoadStateObserver);
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
    if (!m_injectedBundleClient)
        return;

    m_injectedBundleClient->didReceiveMessageFromInjectedBundle(this, messageName, WebProcessProxy::fromConnection(connection)->transformHandlesToObjects(messageBody.protectedObject().get()).get());
}

void WebPageProxy::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&& completionHandler)
{
    if (!m_injectedBundleClient)
        return completionHandler({ });

    RefPtr<API::Object> returnData;
    Ref process = WebProcessProxy::fromConnection(connection);
    m_injectedBundleClient->didReceiveSynchronousMessageFromInjectedBundle(this, messageName, process->transformHandlesToObjects(messageBody.protectedObject().get()).get(), [completionHandler = WTFMove(completionHandler), process] (RefPtr<API::Object>&& returnData) mutable {
        completionHandler(UserData(process->transformObjectsToHandles(returnData.get())));
    });
}

bool WebPageProxy::hasSameGPUAndNetworkProcessPreferencesAs(const API::PageConfiguration& configuration) const
{
    auto sharedPreferences = WebKit::sharedPreferencesForWebProcess(preferences().store(), shouldEnableLockdownMode());
    return !updateSharedPreferencesForWebProcess(sharedPreferences, configuration.preferences().store(), shouldEnableLockdownMode());
}

bool WebPageProxy::hasSameGPUAndNetworkProcessPreferencesAs(const WebPageProxy& page) const
{
    return hasSameGPUAndNetworkProcessPreferencesAs(Ref { page }->configuration());
}

void WebPageProxy::launchProcess(const Site& site, ProcessLaunchReason reason)
{
    ASSERT(!m_isClosed);
    ASSERT(!hasRunningProcess());

    WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcess:");

    // In case we are currently connected to the dummy process, we need to make sure the inspector proxy
    // disconnects from the dummy process first. Do not call inspector() / protectedInspector() since they return
    // null after the page has closed.
    RefPtr { m_inspector }->reset();

    protectedLegacyMainFrameProcess()->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();

    Ref processPool = m_configuration->processPool();
    RefPtr relatedPage = m_configuration->relatedPage();

    if (RefPtr frameProcess = protectedBrowsingContextGroup()->processForSite(site)) {
        ASSERT(protectedPreferences()->siteIsolationEnabled());
        m_legacyMainFrameProcess = frameProcess->process();
    } else if (relatedPage && !relatedPage->isClosed() && reason == ProcessLaunchReason::InitialProcess && hasSameGPUAndNetworkProcessPreferencesAs(*relatedPage)) {
        m_legacyMainFrameProcess = relatedPage->ensureRunningProcess();
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcess: Using process (process=%p, PID=%i) from related page", m_legacyMainFrameProcess.ptr(), m_legacyMainFrameProcess->processID());
    } else
        m_legacyMainFrameProcess = processPool->processForSite(protectedWebsiteDataStore(), WebProcessPool::IsSharedProcess::No, site, site, { }, shouldEnableLockdownMode() ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled, (shouldEnableEnhancedSecurity() || protectedPreferences()->forceEnhancedSecurity()) ? WebProcessProxy::EnhancedSecurity::Enabled : WebProcessProxy::EnhancedSecurity::Disabled, m_configuration, WebCore::ProcessSwapDisposition::None);

    m_shouldReloadDueToCrashWhenVisible = false;
    m_isLockdownModeExplicitlySet = m_configuration->isLockdownModeExplicitlySet();

    Ref process = m_legacyMainFrameProcess;
    process->addExistingWebPage(*this, WebProcessProxy::BeginsUsingDataStore::Yes);
    addAllMessageReceivers();

#if ENABLE(IPC_TESTING_API)
    if (m_preferences->store().getBoolValueForKey(WebPreferencesKey::ipcTestingAPIEnabledKey()) && m_preferences->store().getBoolValueForKey(WebPreferencesKey::ignoreInvalidMessageWhenIPCTestingAPIEnabledKey()))
        process->setIgnoreInvalidMessageForTesting();
#endif

    if (m_configuration->allowTestOnlyIPC())
        process->setAllowTestOnlyIPC(true);

    finishAttachingToWebProcess(site, reason);

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

    if (!hasCommittedAnyProvisionalLoads()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because has not committed any load yet", m_legacyMainFrameProcess->processID());
        return false;
    }

    if (isPageOpenedByDOMShowingInitialEmptyDocument()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "suspendCurrentPageIfPossible: Not suspending current page for process pid %i because it is showing the initial empty document", m_legacyMainFrameProcess->processID());
        return false;
    }

    RefPtr fromItem = navigation.fromItem();

    // If the source and the destination back / forward list items are the same, then this is a client-side redirect. In this case,
    // there is no need to suspend the previous page as there will be no way to get back to it.
    if (fromItem && fromItem == m_backForwardList->currentItem()) {
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

    Ref suspendedPage = SuspendedPageProxy::create(*this, protectedLegacyMainFrameProcess(), mainFrame.releaseNonNull(), std::exchange(m_browsingContextGroup, BrowsingContextGroup::create()), shouldDelayClosingUntilFirstLayerFlush);

    LOG(ProcessSwapping, "WebPageProxy %" PRIu64 " created suspended page %s for process pid %i, back/forward item %s" PRIu64, identifier().toUInt64(), suspendedPage->loggingString().utf8().data(), m_legacyMainFrameProcess->processID(), fromItem ? fromItem->identifier().toString().utf8().data() : "0"_s);

    m_lastSuspendedPage = suspendedPage.get();

    if (fromItem && shouldUseBackForwardCache())
        protectedBackForwardCache()->addEntry(*fromItem, WTFMove(suspendedPage));
    else {
        ASSERT(needsSuspendedPageToPreventFlashing);
        m_suspendedPageKeptToPreventFlashing = WTFMove(suspendedPage);
    }

    return true;
}

WebBackForwardCache& WebPageProxy::backForwardCache() const
{
    return m_configuration->processPool().backForwardCache();
}

Ref<WebBackForwardCache> WebPageProxy::protectedBackForwardCache() const
{
    return backForwardCache();
}

bool WebPageProxy::shouldUseBackForwardCache() const
{
    Ref preferences = m_preferences;
    return preferences->usesBackForwardCache()
        && backForwardCache().capacity() > 0
        && !preferences->siteIsolationEnabled();
}

void WebPageProxy::setBrowsingContextGroup(BrowsingContextGroup& browsingContextGroup)
{
    Ref protectedBrowsingContextGroup = m_browsingContextGroup;
    if (protectedBrowsingContextGroup.ptr() == &browsingContextGroup)
        return;

    if (protectedPreferences()->siteIsolationEnabled()) {
        protectedBrowsingContextGroup->removePage(*this);
        browsingContextGroup.addPage(*this);
    }

    m_browsingContextGroup = browsingContextGroup;
}

#if ENABLE(VIDEO)
void WebPageProxy::showCaptionDisplaySettings(WebCore::HTMLMediaElementIdentifier identifier, const WebCore::ResolvedCaptionDisplaySettingsOptions& options, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&& completionHandler)
{
    if (RefPtr pageClient = this->pageClient()) {
        pageClient->showCaptionDisplaySettings(identifier, options, WTFMove(completionHandler));
        return;
    }

    completionHandler(makeUnexpected<WebCore::ExceptionData>({ ExceptionCode::NotSupportedError, "Caption Display Settings are not supported."_s }));
}

void WebPageProxy::showCaptionDisplaySettingsPreview(const FrameInfoData& frameInfo, WebCore::HTMLMediaElementIdentifier identifier)
{
    sendToProcessContainingFrame(frameInfo.frameID, Messages::WebPage::ShowCaptionDisplaySettingsPreview(identifier));
}

void WebPageProxy::hideCaptionDisplaySettingsPreview(const FrameInfoData& frameInfo, WebCore::HTMLMediaElementIdentifier identifier)
{
    sendToProcessContainingFrame(frameInfo.frameID, Messages::WebPage::HideCaptionDisplaySettingsPreview(identifier));
}

#endif

void WebPageProxy::swapToProvisionalPage(Ref<ProvisionalPageProxy>&& provisionalPage)
{
    ASSERT(!m_isClosed);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "swapToProvisionalPage: newWebPageID=%" PRIu64, provisionalPage->webPageID().toUInt64());

    m_legacyMainFrameProcess = provisionalPage->process();
    m_webPageID = provisionalPage->webPageID();
    if (RefPtr pageClient = this->pageClient())
        pageClient->didChangeWebPageID();
    ASSERT(m_legacyMainFrameProcess->websiteDataStore());
    m_websiteDataStore = *m_legacyMainFrameProcess->websiteDataStore();
#if ENABLE(WEB_ARCHIVE)
    if (provisionalPage->replacedDataStoreForWebArchiveLoad())
        m_replacedDataStoreForWebArchiveLoad = provisionalPage->replacedDataStoreForWebArchiveLoad();
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInWebProcess = provisionalPage->contextIDForVisibilityPropagationInWebProcess();
#if ENABLE(GPU_PROCESS)
    m_contextIDForVisibilityPropagationInGPUProcess = provisionalPage->contextIDForVisibilityPropagationInGPUProcess();
#endif
#endif

    // FIXME: Do we really need to disable this logging in ephemeral sessions?
    if (RefPtr logger = m_logger)
        logger->setEnabled(this, isAlwaysOnLoggingAllowed());

    ASSERT(!m_mainFrame);
    m_mainFrame = provisionalPage->mainFrame();
    ASSERT(!m_drawingArea);
    setDrawingArea(provisionalPage->takeDrawingArea());

    // FIXME: Think about what to do if the provisional page didn't get its browsing context group from the SuspendedPageProxy.
    // We do need to clear it at some point for navigations that aren't from back/forward navigations. Probably in the same place as PSON?
    setBrowsingContextGroup(provisionalPage->browsingContextGroup());

    protectedLegacyMainFrameProcess()->addExistingWebPage(*this, WebProcessProxy::BeginsUsingDataStore::No);
    addAllMessageReceivers();

    Site unusedSite(aboutBlankURL());
    finishAttachingToWebProcess(unusedSite, ProcessLaunchReason::ProcessSwap);

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
    if (!accessibilityToken.isEmpty()) {
        registerWebProcessAccessibilityToken(accessibilityToken.span());
    }
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    auto accessibilityPlugID = provisionalPage->accessibilityPlugID();
    if (!accessibilityPlugID.isEmpty())
        bindAccessibilityTree(accessibilityPlugID);
#endif
}

void WebPageProxy::finishAttachingToWebProcess(const Site& site, ProcessLaunchReason reason)
{
    ASSERT(m_legacyMainFrameProcess->state() != AuxiliaryProcessProxy::State::Terminated);

    updateActivityState();
    updateThrottleState();

    didAttachToRunningProcess();

    // In the process-swap case, the ProvisionalPageProxy already took care of initializing the WebPage in the WebProcess.
    if (reason != ProcessLaunchReason::ProcessSwap)
        initializeWebPage(site, m_mainFrame ? m_mainFrame->effectiveSandboxFlags() : configuration().initialSandboxFlags(), m_mainFrame ? m_mainFrame->effectiveReferrerPolicy() : configuration().initialReferrerPolicy());

    if (RefPtr inspector = this->inspector())
        inspector->updateForNewPageProcess(*this);

#if ENABLE(REMOTE_INSPECTOR)
    remoteInspectorInformationDidChange();
#endif

    updateWheelEventActivityAfterProcessSwap();

    if (RefPtr pageClient = this->pageClient())
        pageClient->didRelaunchProcess();
    protectedPageLoadState()->didSwapWebProcesses();
}

void WebPageProxy::didAttachToRunningProcess()
{
    ASSERT(hasRunningProcess());

#if ENABLE(FULLSCREEN_API)
    ASSERT(!m_fullScreenManager);
    m_fullScreenManager = WebFullScreenManagerProxy::create(*this, protectedPageClient()->checkedFullScreenManagerProxyClient().get());
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    ASSERT(!m_playbackSessionManager);
    m_playbackSessionManager = PlaybackSessionManagerProxy::create(*this);
    ASSERT(!m_videoPresentationManager);
    m_videoPresentationManager = VideoPresentationManagerProxy::create(*this, *protectedPlaybackSessionManager());
    if (RefPtr videoPresentationManager = m_videoPresentationManager)
        videoPresentationManager->setMockVideoPresentationModeEnabled(m_mockVideoPresentationModeEnabled);
#endif

#if ENABLE(APPLE_PAY)
    ASSERT(!internals().paymentCoordinator);
    internals().paymentCoordinator = WebPaymentCoordinatorProxy::create(internals());
#endif

#if USE(SYSTEM_PREVIEW)
    ASSERT(!m_systemPreviewController);
    m_systemPreviewController = SystemPreviewController::create(*this);
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
    if (protectedPreferences()->modelElementEnabled()) {
        ASSERT(!m_modelElementController);
        m_modelElementController = ModelElementController::create(*this);
    }
#endif

#if ENABLE(WEB_AUTHN)
    ASSERT(!m_webAuthnCredentialsMessenger);
    m_webAuthnCredentialsMessenger = WebAuthenticatorCoordinatorProxy::create(*this);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    ASSERT(!m_webDeviceOrientationUpdateProviderProxy);
    m_webDeviceOrientationUpdateProviderProxy = WebDeviceOrientationUpdateProviderProxy::create(*this);
#endif

#if !PLATFORM(IOS_FAMILY)
    auto currentOrientation = WebCore::naturalScreenOrientationType();
#else
    auto currentOrientation = toScreenOrientationType(m_deviceOrientation);
#endif
    m_screenOrientationManager = WebScreenOrientationManagerProxy::create(*this, currentOrientation);

#if ENABLE(WEBXR)
    ASSERT(!internals().xrSystem);
    internals().xrSystem = PlatformXRSystem::create(*this);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
    internals().modelPresentationManagerProxy = ModelPresentationManagerProxy::create(*this);
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
    RefPtr currentItem = m_backForwardList->currentItem();
    auto site = currentItem ? Site { URL { currentItem->url() } } : Site(aboutBlankURL());
    launchProcess(site, ProcessLaunchReason::Crash);

    if (!currentItem) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "launchProcessForReload: no current item to reload");
        return nullptr;
    }

    Ref navigation = m_navigationState->createReloadNavigation(legacyMainFrameProcess().coreProcessIdentifier(), m_backForwardList->protectedCurrentItem());

    String url = currentURL();
    if (!url.isEmpty()) {
        Ref protectedPageLoadState = pageLoadState();
        auto transaction = protectedPageLoadState->transaction();
        protectedPageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), url });
    }

    auto publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(URL(currentItem->url()));

    // We allow stale content when reloading a WebProcess that's been killed or crashed.
    send(Messages::WebPage::GoToBackForwardItem({ navigation->navigationID(), currentItem->mainFrameState(), FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, std::nullopt, publicSuffix, { }, WebCore::ProcessSwapDisposition::None }));

    Ref legacyMainFrameProcess = m_legacyMainFrameProcess;
    legacyMainFrameProcess->startResponsivenessTimer();

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(legacyMainFrameProcess->protectedThrottler()->foregroundActivity("Client reload"_s));

    return navigation;
}

void WebPageProxy::setDrawingArea(RefPtr<DrawingAreaProxy>&& newDrawingArea)
{
    RELEASE_ASSERT(m_drawingArea != newDrawingArea);
#if PLATFORM(COCOA)
    // The scrolling coordinator needs to do cleanup before the drawing area goes away.
    m_scrollingCoordinatorProxy = nullptr;
#endif

    Ref legacyMainFrameProcess = m_legacyMainFrameProcess;
    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->stopReceivingMessages(legacyMainFrameProcess);

    m_drawingArea = WTFMove(newDrawingArea);
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [drawingArea = m_drawingArea](auto& remotePageProxy) {
        remotePageProxy.setDrawingArea(drawingArea.get());
    });
    RefPtr drawingArea = m_drawingArea;
    if (!drawingArea)
        return;

    drawingArea->startReceivingMessages(legacyMainFrameProcess);
    drawingArea->setSize(viewSize());

#if PLATFORM(COCOA)
    if (RefPtr drawingAreaProxy = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea))
        m_scrollingCoordinatorProxy = drawingAreaProxy->createScrollingCoordinatorProxy();
#endif
}

void WebPageProxy::initializeWebPage(const Site& site, WebCore::SandboxFlags effectiveSandboxFlags, WebCore::ReferrerPolicy effectiveReferrerPolicy)
{
    if (!hasRunningProcess())
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    setDrawingArea(pageClient->createDrawingAreaProxy(m_legacyMainFrameProcess.copyRef()));
    ASSERT(m_drawingArea);

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton();
#endif

    if (auto& attributedBundleIdentifier = m_configuration->attributedBundleIdentifier(); !!attributedBundleIdentifier) {
        WebPageNetworkParameters parameters { attributedBundleIdentifier };
        protectedWebsiteDataStore()->protectedNetworkProcess()->send(Messages::NetworkProcess::AddWebPageNetworkParameters(sessionID(), identifier(), WTFMove(parameters)), 0);
    }

    if (RefPtr networkProcess = websiteDataStore().networkProcessIfExists()) {
        if (m_pageToCloneSessionStorageFrom)
            networkProcess->send(Messages::NetworkProcess::CloneSessionStorageForWebPage(sessionID(), m_pageToCloneSessionStorageFrom->identifier(), identifier()), 0);
        if (m_configuration->shouldRelaxThirdPartyCookieBlocking() == ShouldRelaxThirdPartyCookieBlocking::Yes)
            networkProcess->send(Messages::NetworkProcess::SetShouldRelaxThirdPartyCookieBlockingForPage(identifier()), 0);
    }
    m_pageToCloneSessionStorageFrom = nullptr;

    Ref process = m_legacyMainFrameProcess;
    Ref browsingContextGroup = m_browsingContextGroup;
    Ref preferences = m_preferences;

    m_mainFrame = WebFrameProxy::create(*this, browsingContextGroup->ensureProcessForSite(site, site, process, preferences), generateFrameIdentifier(), effectiveSandboxFlags, effectiveReferrerPolicy, ScrollbarMode::Auto, WebFrameProxy::protectedWebFrame(m_openerFrameIdentifier).get(), IsMainFrame::Yes);
    if (preferences->siteIsolationEnabled())
        browsingContextGroup->addPage(*this);
    process->send(Messages::WebProcess::CreateWebPage(m_webPageID, creationParameters(process, *protectedDrawingArea(), m_mainFrame->frameID(), std::nullopt)), 0);

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
    internals().frameLoadStateObserver = WebPageProxyFrameLoadStateObserver::create();
    m_mainFrame->frameLoadState().addObserver(*internals().protectedFrameLoadStateObserver());
#endif

    process->addVisitedLinkStoreUser(m_visitedLinkStore, identifier());

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    m_needsInitialLinkDecorationFilteringData = LinkDecorationFilteringController::sharedSingleton().cachedListData().isEmpty();
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
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->willClosePage(*this);
    }

#if ENABLE(FULLSCREEN_API)
    if (RefPtr fullscreenManager = std::exchange(m_fullScreenManager, nullptr))
        fullscreenManager->detachFromClient();
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

    // Do not call inspector() / protectedInspector() since they return
    // null after the page has closed.
    RefPtr { m_inspector }->invalidate();

    m_backForwardList->pageClosed();
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
    RunLoop::currentSingleton().dispatch([processesToClose = WTFMove(processesToClose)] {
        for (auto [process, pageID, scope] : processesToClose)
            Ref { process }->send(Messages::WebPage::Close(), pageID);
    });

    process->removeWebPage(*this, WebProcessProxy::EndsUsingDataStore::Yes);
    removeAllMessageReceivers();
    processPool->protectedSupplement<WebNotificationManagerProxy>()->clearNotifications(this);

    // Null out related WebPageProxy to avoid leaks.
    m_configuration->setRelatedPage(nullptr);

    // Make sure we don't hold a process assertion after getting closed.
    resetActivityState();
    internals().audibleActivityTimer.stop();

    stopAllURLSchemeTasks();

#if ENABLE(GAMEPAD)
    m_internals->recentGamepadAccessHysteresis.cancel();
#endif

    if (protectedPreferences()->siteIsolationEnabled())
        protectedBrowsingContextGroup()->removePage(*this);
}

bool WebPageProxy::tryClose()
{
    if (!hasRunningProcess())
        return true;

    WEBPAGEPROXY_RELEASE_LOG(Process, "tryClose:");

    // Close without delay if the process allows it. Our goal is to terminate
    // the process, so we check a per-process status bit.
    if (m_legacyMainFrameProcess->isSuddenTerminationEnabled())
        return true;

    internals().tryCloseTimeoutTimer.startOneShot(tryCloseTimeoutDelay);
    sendWithAsyncReply(Messages::WebPage::TryClose(), [weakThis = WeakPtr { *this }](bool shouldClose) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        // If we timed out, don't ask the client to close again.
        if (!protectedThis->internals().tryCloseTimeoutTimer.isActive())
            return;

        protectedThis->internals().tryCloseTimeoutTimer.stop();
        if (shouldClose)
            protectedThis->closePage();
    });
    return false;
}

void WebPageProxy::tryCloseTimedOut()
{
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "tryCloseTimedOut: Timed out waiting for the process to respond to the WebPage::TryClose IPC, closing the page now");
    closePage();
}

void WebPageProxy::maybeInitializeSandboxExtensionHandle(WebProcessProxy& process, const URL& url, const URL& resourceDirectoryURL, bool checkAssumedReadAccessToResourceURL, CompletionHandler<void(std::optional<SandboxExtension::Handle>&&)>&& completionHandler)
{
    if (!url.protocolIsFile())
        return completionHandler(std::nullopt);

#if !RELEASE_LOG_DISABLED
    auto urlHash = url.isValid() ? WTF::URLHash::hash(url) : 0;
    auto resourceDirectoryURLHash = resourceDirectoryURL.isValid() ? WTF::URLHash::hash(resourceDirectoryURL) : 0;
#endif
    WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: url(%u) = %" PRIVATE_LOG_STRING ", resourceDirectoryURL(%u) = %" PRIVATE_LOG_STRING ", checkAssumedReadAccessToResourceURL = %d", urlHash, url.string().utf8().data(), resourceDirectoryURLHash, resourceDirectoryURL.string().utf8().data(), checkAssumedReadAccessToResourceURL);

#if HAVE(AUDIT_TOKEN)
    // If the process is still launching then it does not have a PID yet. We will take care of creating the sandbox extension
    // once the process has finished launching.
    if (process.isLaunching() || process.wasTerminated()) {
        if (process.isLaunching())
            WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: process is launching");
        else
            WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: process is terminated");
        return completionHandler(std::nullopt);
    }
#endif

    auto createSandboxExtension = [protectedProcess = Ref { process }] (const String& path) {
        if (auto handle = protectedProcess->sandboxExtensionForFile(path))
            return handle;
        std::optional<SandboxExtension::Handle> handle;
#if HAVE(AUDIT_TOKEN)
        if (auto token = protectedProcess->protectedConnection()->getAuditToken())
            handle = SandboxExtension::createHandleForReadByAuditToken(path, *token);
        else
#endif
        handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly);
        if (handle)
            protectedProcess->addSandboxExtensionForFile(path, *handle);
        return handle;
    };

    if (!resourceDirectoryURL.isEmpty()) {
        if (!url.string().startsWith(resourceDirectoryURL.string()))
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Sandbox, "maybeInitializeSandboxExtensionHandle: url is not inside resource directory url");

        if (checkAssumedReadAccessToResourceURL && process.hasAssumedReadAccessToURL(resourceDirectoryURL)) {
#if PLATFORM(COCOA)
            // Check the actual access to this directory in the WebContent process, since a sandbox extension created earlier could have been revoked in the WebContent process by now.
            if (!sandbox_check(process.processID(), "file-read-data", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT), FileSystem::fileSystemRepresentation(resourceDirectoryURL.fileSystemPath()).data())) {
                WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: has sandbox access to resource directory");
                return completionHandler(std::nullopt);
            }

#else
            return completionHandler(std::nullopt);
#endif
        }

        if (auto sandboxExtensionHandle = createSandboxExtension(resourceDirectoryURL.fileSystemPath())) {
            WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: created sandbox extension to resource directory");
            process.assumeReadAccessToBaseURL(*this, resourceDirectoryURL.string(), [sandboxExtensionHandle = WTFMove(*sandboxExtensionHandle), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(WTFMove(sandboxExtensionHandle));
            });
            return;
        }
    }

    if (process.hasAssumedReadAccessToURL(url))
        return completionHandler(std::nullopt);

    // Inspector resources are in a directory with assumed access.
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!WebKit::isInspectorPage(*this));

    if (auto sandboxExtensionHandle = createSandboxExtension("/"_s)) {
        WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: created sandbox extension to root of filesystem");
        willAcquireUniversalFileReadSandboxExtension(process);
        auto baseURL = url.truncatedForUseAsBase();
        auto basePath = baseURL.fileSystemPath();
        process.assumeReadAccessToBaseURL(*this, basePath, [sandboxExtensionHandle = WTFMove(*sandboxExtensionHandle), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(WTFMove(sandboxExtensionHandle));
        });
        return;
    }

#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoUnconditionalUniversalSandboxExtension))
        willAcquireUniversalFileReadSandboxExtension(process);
#endif

    // We failed to issue an universal file read access sandbox, fall back to issuing one for the base URL instead.
    auto baseURL = url.truncatedForUseAsBase();
    auto basePath = baseURL.fileSystemPath();
    if (basePath.isNull()) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Sandbox, "maybeInitializeSandboxExtensionHandle: base path is null");
        return completionHandler(std::nullopt);
    }

    if (auto sandboxExtensionHandle = createSandboxExtension(basePath)) {
        WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: created sandbox extension to base path");
        process.assumeReadAccessToBaseURL(*this, baseURL.string(), [sandboxExtensionHandle = WTFMove(*sandboxExtensionHandle), completionHandler = WTFMove(completionHandler)] mutable {
            completionHandler(WTFMove(sandboxExtensionHandle));
        });
        return;
    }

    // We failed to issue read access to the base path, fall back to issuing one for the full URL instead.
    auto fullPath = url.fileSystemPath();
    if (fullPath.isNull()) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Sandbox, "maybeInitializeSandboxExtensionHandle: full path is null");
        return completionHandler(std::nullopt);
    }

    if (auto sandboxExtensionHandle = createSandboxExtension(fullPath)) {
        WEBPAGEPROXY_RELEASE_LOG(Sandbox, "maybeInitializeSandboxExtensionHandle: created sandbox extension to full path");
        completionHandler(WTFMove(*sandboxExtensionHandle));
        return;
    }

    WEBPAGEPROXY_RELEASE_LOG_ERROR(Sandbox, "maybeInitializeSandboxExtensionHandle: unable to create sandbox extension");
    completionHandler(std::nullopt);
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
    // Callers should perform page close check.
    RELEASE_ASSERT(!m_isClosed);

    if (!hasRunningProcess())
        launchProcess(Site(aboutBlankURL()), ProcessLaunchReason::InitialProcess);

    return m_legacyMainFrameProcess;
}

Ref<WebProcessProxy> WebPageProxy::ensureProtectedRunningProcess()
{
    return ensureRunningProcess();
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(WebCore::ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, IsPerformingHTTPFallback isPerformingHTTPFallback, std::unique_ptr<NavigationActionData>&& lastNavigationAction, API::Object* userData, bool isRequestFromClientOrUserInput)
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

    Ref navigation = m_navigationState->createLoadRequestNavigation(legacyMainFrameProcess().coreProcessIdentifier(), ResourceRequest(request), m_backForwardList->protectedCurrentItem());

    if (lastNavigationAction)
        navigation->setLastNavigationAction(*lastNavigationAction);

    if (isRequestFromClientOrUserInput)
        navigation->markRequestAsFromClientInput();

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(legacyMainFrameProcess().protectedThrottler()->foregroundActivity("Client navigation"_s));

#if PLATFORM(COCOA)
    setLastNavigationWasAppInitiated(request);
#endif

    loadRequestWithNavigationShared(protectedLegacyMainFrameProcess(), m_webPageID, navigation, WTFMove(request), shouldOpenExternalURLsPolicy, isPerformingHTTPFallback, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain(), nullptr, std::nullopt);
    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(WebCore::ResourceRequest&& request, WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, WebCore::IsPerformingHTTPFallback isPerformingHTTPFallback)
{
    return loadRequest(WTFMove(request), shouldOpenExternalURLsPolicy, isPerformingHTTPFallback, nullptr);
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
    return loadRequest(WTFMove(request), shouldOpenExternalURLsPolicy, IsPerformingHTTPFallback::No);
}

RefPtr<API::Navigation> WebPageProxy::loadRequest(ResourceRequest&& request)
{
    return loadRequest(WTFMove(request), ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks);
}

void WebPageProxy::loadRequestWithNavigationShared(Ref<WebProcessProxy>&& process, WebCore::PageIdentifier webPageID, API::Navigation& navigation, ResourceRequest&& request, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, IsPerformingHTTPFallback isPerformingHTTPFallback, API::Object* userData, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, RefPtr<API::WebsitePolicies>&& websitePolicies, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    ASSERT(!m_isClosed);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadRequestWithNavigationShared:");

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    auto url = request.url();
#if PLATFORM(COCOA)
    bool urlIsInvalidButNotEmpty = !url.isValid() && !url.isEmpty();
    if (urlIsInvalidButNotEmpty && WTF::linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToNull)) {
        RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }, request, navigation = Ref { navigation }] mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            auto requestURL = request.url();
            auto error = cannotShowURLError(request);
            protectedThis->m_navigationClient->didFailProvisionalNavigationWithError(*protectedThis, legacyEmptyFrameInfo(WTFMove(request)), navigation.ptr(), requestURL, error, nullptr);
        });
        return;
    }
#endif

    if (shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::No)
        pageLoadState->setPendingAPIRequest(transaction, { navigation.navigationID(), url.string() });

    pageLoadState->setHTTPFallbackInProgress(transaction, isPerformingHTTPFallback == IsPerformingHTTPFallback::Yes);

    LoadParameters loadParameters;
    loadParameters.publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(url);
    loadParameters.navigationID = navigation.navigationID();
    loadParameters.request = WTFMove(request);
    loadParameters.shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    loadParameters.userData = UserData(process->transformObjectsToHandles(userData).get());
    loadParameters.shouldTreatAsContinuingLoad = shouldTreatAsContinuingLoad;
    loadParameters.websitePolicies = websitePolicies ? std::optional(websitePolicies->dataForProcess(process)) : std::nullopt;
    loadParameters.lockHistory = navigation.lockHistory();
    loadParameters.lockBackForwardList = navigation.lockBackForwardList();
    loadParameters.clientRedirectSourceForHistory = navigation.clientRedirectSourceForHistory();
    loadParameters.ownerPermissionsPolicy = navigation.ownerPermissionsPolicy();
    loadParameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    loadParameters.existingNetworkResourceLoadIdentifierToResume = existingNetworkResourceLoadIdentifierToResume;
    loadParameters.advancedPrivacyProtections = navigation.originatorAdvancedPrivacyProtections();
    loadParameters.isRequestFromClientOrUserInput = navigation.isRequestFromClientOrUserInput();
    loadParameters.isPerformingHTTPFallback = isPerformingHTTPFallback == IsPerformingHTTPFallback::Yes;
    loadParameters.isHandledByAboutSchemeHandler = m_aboutSchemeHandler->canHandleURL(url);
    loadParameters.requiredCookiesVersion = protectedWebsiteDataStore()->cookiesVersion();
    loadParameters.originatingFrame = navigation.lastNavigationAction() ? std::optional(navigation.lastNavigationAction()->originatingFrameInfoData) : std::nullopt;
    if (auto& action = navigation.lastNavigationAction())
        loadParameters.requester = action->requester;

#if ENABLE(CONTENT_EXTENSIONS)
    if (protectedPreferences()->iFrameResourceMonitoringEnabled())
        process->requestResourceMonitorRuleLists(protectedPreferences()->iFrameResourceMonitoringTestingSettingsEnabled());
#endif

    maybeInitializeSandboxExtensionHandle(process, url, pageLoadState->resourceDirectoryURL(), true, [weakThis = WeakPtr { *this }, weakProcess = WeakPtr { process }, loadParameters = WTFMove(loadParameters), url, navigation = Ref { navigation }, webPageID, shouldTreatAsContinuingLoad] (std::optional<SandboxExtension::Handle>&& sandboxExtensionHandle) mutable {
        RefPtr protectedProcess = weakProcess.get();
        RefPtr protectedThis = weakThis.get();
        if (!protectedProcess || !protectedThis)
            return;
        if (sandboxExtensionHandle)
            loadParameters.sandboxExtensionHandle = WTFMove(*sandboxExtensionHandle);
        protectedThis->prepareToLoadWebPage(*weakProcess, loadParameters);

        if (shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::No)
            protectedThis->preconnectTo(ResourceRequest { loadParameters.request });

        navigation->setIsLoadedWithNavigationShared(true);
        protectedProcess->markProcessAsRecentlyUsed();
        if (!protectedProcess->isLaunching() || !url.protocolIsFile())
            protectedProcess->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)), webPageID);
        else
            protectedProcess->send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(WTFMove(loadParameters), protectedThis->pageLoadState().resourceDirectoryURL(), protectedThis->identifier(), true), webPageID);
        protectedProcess->startResponsivenessTimer();
    });
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

    Ref navigation = m_navigationState->createLoadRequestNavigation(legacyMainFrameProcess().coreProcessIdentifier(), ResourceRequest(URL { fileURL }), m_backForwardList->protectedCurrentItem());

    navigation->markRequestAsFromClientInput();

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(legacyMainFrameProcess().protectedThrottler()->foregroundActivity("Client navigation"_s));

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), fileURLString }, resourceDirectoryURL);

    auto request = ResourceRequest(URL { fileURL });
    request.setIsAppInitiated(isAppInitiated);
    m_lastNavigationWasAppInitiated = isAppInitiated;

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTFMove(request);
    loadParameters.shouldOpenExternalURLsPolicy = ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    loadParameters.userData = UserData(legacyMainFrameProcess().transformObjectsToHandles(userData).get());
    loadParameters.publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(loadParameters.request.url());
    loadParameters.isRequestFromClientOrUserInput = isAppInitiated;
    Ref process = m_legacyMainFrameProcess;
    maybeInitializeSandboxExtensionHandle(process, fileURL, resourceDirectoryURL, true, [weakThis = WeakPtr { *this }, weakProcess = WeakPtr { process }, loadParameters = WTFMove(loadParameters), resourceDirectoryURL] (std::optional<SandboxExtension::Handle>&& sandboxExtension) mutable {
        const bool checkAssumedReadAccessToResourceURL = false;
        RefPtr protectedProcess = weakProcess.get();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedProcess)
            return;
        if (sandboxExtension)
            loadParameters.sandboxExtensionHandle = WTFMove(*sandboxExtension);

        protectedThis->prepareToLoadWebPage(*protectedProcess, loadParameters);

        protectedProcess->markProcessAsRecentlyUsed();
        if (protectedProcess->isLaunching())
            protectedThis->send(Messages::WebPage::LoadRequestWaitingForProcessLaunch(WTFMove(loadParameters), resourceDirectoryURL, protectedThis->identifier(), checkAssumedReadAccessToResourceURL));
        else
            protectedThis->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)));
        protectedProcess->startResponsivenessTimer();
    });

    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadData(Ref<WebCore::SharedBuffer>&& data, const String& type, const String& encoding, const String& baseURL, API::Object* userData, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadData:");

#if ENABLE(APP_BOUND_DOMAINS)
    if (type == "text/html"_s && !isFullWebBrowserOrRunningTest())
        m_limitsNavigationsToAppBoundDomains = true;
#endif

    if (m_isClosed) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "loadData: page is closed");
        return nullptr;
    }

    if (!hasRunningProcess())
        launchProcess(Site(URL(baseURL)), ProcessLaunchReason::InitialProcess);

    Ref navigation = m_navigationState->createLoadDataNavigation(legacyMainFrameProcess().coreProcessIdentifier(), makeUnique<API::SubstituteData>(Vector(data->span()), type, encoding, baseURL, userData));
    navigation->markAsFromLoadData();

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(legacyMainFrameProcess().protectedThrottler()->foregroundActivity("Client navigation"_s));

    loadDataWithNavigationShared(protectedLegacyMainFrameProcess(), m_webPageID, navigation, WTFMove(data), type, encoding, baseURL, userData, ShouldTreatAsContinuingLoad::No, isNavigatingToAppBoundDomain(), nullptr, shouldOpenExternalURLsPolicy, SubstituteData::SessionHistoryVisibility::Hidden);
    return navigation;
}

RefPtr<API::Navigation> WebPageProxy::loadData(Ref<WebCore::SharedBuffer>&& data, const String& type, const String& encoding, const String& baseURL, API::Object* userData)
{
    return loadData(WTFMove(data), type, encoding, baseURL, userData, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
}

void WebPageProxy::loadDataWithNavigationShared(Ref<WebProcessProxy>&& process, WebCore::PageIdentifier webPageID, API::Navigation& navigation, Ref<WebCore::SharedBuffer>&& data, const String& type, const String& encoding, const String& baseURL, API::Object* userData, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, RefPtr<API::WebsitePolicies>&& websitePolicies, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "loadDataWithNavigation");

    ASSERT(!m_isClosed);

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    pageLoadState->setPendingAPIRequest(transaction, { navigation.navigationID(), !baseURL.isEmpty() ? baseURL : aboutBlankURL().string() });

    LoadParameters loadParameters;
    loadParameters.sessionHistoryVisibility = sessionHistoryVisibility;
    loadParameters.navigationID = navigation.navigationID();
    loadParameters.data = WTFMove(data);
    loadParameters.MIMEType = type;
    loadParameters.encodingName = encoding;
    loadParameters.baseURLString = baseURL;
    loadParameters.shouldTreatAsContinuingLoad = shouldTreatAsContinuingLoad;
    loadParameters.userData = UserData(process->transformObjectsToHandles(userData).get());
    loadParameters.websitePolicies = websitePolicies ? std::optional(websitePolicies->dataForProcess(process)) : std::nullopt;
    loadParameters.shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    loadParameters.isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    loadParameters.isServiceWorkerLoad = isServiceWorkerPage();
    prepareToLoadWebPage(process, loadParameters);

    process->markProcessAsRecentlyUsed();
    process->assumeReadAccessToBaseURL(*this, baseURL, [weakProcess = WeakPtr { process }, webPageID, loadParameters = WTFMove(loadParameters)] () mutable {
        RefPtr protectedProcess = weakProcess.get();
        if (!protectedProcess)
            return;
        protectedProcess->send(Messages::WebPage::LoadData(WTFMove(loadParameters)), webPageID);
        protectedProcess->startResponsivenessTimer();
    }, true);
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

    Ref navigation = m_navigationState->createSimulatedLoadWithDataNavigation(legacyMainFrameProcess().coreProcessIdentifier(), ResourceRequest(simulatedRequest), makeUnique<API::SubstituteData>(Vector(data->span()), ResourceResponse(simulatedResponse), WebCore::SubstituteData::SessionHistoryVisibility::Visible), m_backForwardList->protectedCurrentItem());

    if (shouldForceForegroundPriorityForClientNavigation())
        navigation->setClientNavigationActivity(legacyMainFrameProcess().protectedThrottler()->foregroundActivity("Client navigation"_s));

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    auto baseURL = simulatedRequest.url().string();
    simulatedResponse.setURL(URL { simulatedRequest.url() }); // These should always match for simulated load

    pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), !baseURL.isEmpty() ? baseURL : aboutBlankURL().string() });

    LoadParameters loadParameters;
    loadParameters.navigationID = navigation->navigationID();
    loadParameters.request = WTFMove(simulatedRequest);
    loadParameters.data = WTFMove(data);
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
    process->assumeReadAccessToBaseURL(*this, baseURL, [weakProcess = WeakPtr { process }, loadParameters = WTFMove(loadParameters), simulatedResponse = WTFMove(simulatedResponse), webPageID = m_webPageID] () mutable {
        weakProcess->send(Messages::WebPage::LoadSimulatedRequestAndResponse(WTFMove(loadParameters), simulatedResponse), webPageID);
        weakProcess->startResponsivenessTimer();
    });

    return navigation;
}

void WebPageProxy::loadAlternateHTML(Ref<WebCore::DataSegment>&& htmlData, const String& encoding, const URL& baseURL, const URL& unreachableURL, RefPtr<API::WebsitePolicies> policies)
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
        launchProcess(Site { baseURL }, ProcessLaunchReason::InitialProcess);

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    pageLoadState->setPendingAPIRequest(transaction, { { }, unreachableURL.string() });
    pageLoadState->setUnreachableURL(transaction, unreachableURL.string());

    if (RefPtr mainFrame = m_mainFrame)
        mainFrame->setUnreachableURL(unreachableURL);

    LoadParameters loadParameters;
    loadParameters.navigationID = std::nullopt;
    loadParameters.MIMEType = "text/html"_s;
    loadParameters.encodingName = encoding;
    loadParameters.baseURLString = baseURL.string();
    loadParameters.unreachableURLString = unreachableURL.string();
    loadParameters.provisionalLoadErrorURLString = m_failingProvisionalLoadURL;
    // FIXME: This is an unnecessary copy.
    loadParameters.data = WebCore::SharedBuffer::create(htmlData->span());
    Ref process = m_legacyMainFrameProcess;
    loadParameters.websitePolicies = policies ? std::optional(policies->dataForProcess(process)) : std::nullopt;
    prepareToLoadWebPage(process, loadParameters);

    auto continueLoad = [
        this,
        protectedThis = Ref { *this },
        process,
        loadParameters = WTFMove(loadParameters),
        baseURL,
        unreachableURL,
        preventProcessShutdownScope = process->shutdownPreventingScope()
    ] () mutable {
        process->markProcessAsRecentlyUsed();
        process->assumeReadAccessToBaseURLs(*this, { baseURL.string(), unreachableURL.string() }, [weakThis = WeakPtr { *this }, weakProcess = WeakPtr { process }, baseURL, unreachableURL, loadParameters = WTFMove(loadParameters)] () mutable {
            if (!weakThis || !weakProcess)
                return;
            if (baseURL.protocolIsFile())
                weakProcess->addPreviouslyApprovedFileURL(baseURL);
            if (unreachableURL.protocolIsFile())
                weakProcess->addPreviouslyApprovedFileURL(unreachableURL);
            weakThis->send(Messages::WebPage::LoadAlternateHTML(WTFMove(loadParameters)));
            weakProcess->startResponsivenessTimer();
        });
    };

    protectedWebsiteDataStore()->protectedNetworkProcess()->addAllowedFirstPartyForCookies(process, RegistrableDomain(baseURL), LoadedWebArchive::No, WTFMove(continueLoad));
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
        launchProcess(Site { URL { urlString } }, ProcessLaunchReason::InitialProcess);

    send(Messages::WebPage::NavigateToPDFLinkWithSimulatedClick(urlString, documentPoint, screenPoint));
    protectedLegacyMainFrameProcess()->startResponsivenessTimer();
}

void WebPageProxy::stopLoading()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "stopLoading:");

    if (!hasRunningProcess()) {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "stopLoading: page is not valid");
        return;
    }

    send(Messages::WebPage::StopLoading());
    if (RefPtr provisionalPage = m_provisionalPage) {
        provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }
    protectedLegacyMainFrameProcess()->startResponsivenessTimer();
}

RefPtr<API::Navigation> WebPageProxy::reload(OptionSet<WebCore::ReloadOption> options)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "reload:");

    // Make sure the Network & GPU processes are still responsive. This is so that reload() gets us out of the bad state if one of these
    // processes is hung.
    protectedWebsiteDataStore()->protectedNetworkProcess()->checkForResponsiveness();
#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = m_configuration->processPool().gpuProcess())
        gpuProcess->checkForResponsiveness();
#endif

    SandboxExtension::Handle sandboxExtensionHandle;

    if (!hasRunningProcess())
        return launchProcessForReload();

    Ref navigation = m_navigationState->createReloadNavigation(legacyMainFrameProcess().coreProcessIdentifier(), m_backForwardList->protectedCurrentItem());

    String url = currentURL();
    if (!url.isEmpty()) {
        Ref pageLoadState = internals().pageLoadState;
        auto transaction = pageLoadState->transaction();
        pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), url });
    }

    // Store decision to reload without content blockers on the navigation so that we can later set the corresponding
    // WebsitePolicies flag in WebPageProxy::receivedNavigationActionPolicyDecision().
    if (options.contains(WebCore::ReloadOption::DisableContentBlockers))
        navigation->setUserContentExtensionsEnabled(false);

    Ref process = m_legacyMainFrameProcess;
    process->markProcessAsRecentlyUsed();
    if (!url.isEmpty()) {
        // We may not have an extension yet if back/forward list was reinstated after a WebProcess crash or a browser relaunch
        maybeInitializeSandboxExtensionHandle(protectedLegacyMainFrameProcess(), URL { url }, currentResourceDirectoryURL(), true, [weakThis = WeakPtr { *this }, process = WTFMove(process), options = WTFMove(options), sandboxExtensionHandle = WTFMove(sandboxExtensionHandle), navigation](std::optional<SandboxExtension::Handle>&& sandboxExtension) mutable {
            if (!weakThis)
                return;
            if (sandboxExtension)
                sandboxExtensionHandle = WTFMove(*sandboxExtension);
            weakThis->send(Messages::WebPage::Reload(navigation->navigationID(), options, WTFMove(sandboxExtensionHandle)));
            process->startResponsivenessTimer();

            if (weakThis->shouldForceForegroundPriorityForClientNavigation())
                navigation->setClientNavigationActivity(process->protectedThrottler()->foregroundActivity("Client reload"_s));

#if ENABLE(SPEECH_SYNTHESIS)
            weakThis->resetSpeechSynthesizer();
#endif
        });
    }

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
    RefPtr forwardItem = m_backForwardList->goForwardItemSkippingItemsWithoutUserGesture();
    if (!forwardItem)
        return nullptr;

    return goToBackForwardItem(forwardItem->protectedNavigatedFrameItem(), FrameLoadType::Forward);
}

RefPtr<API::Navigation> WebPageProxy::goBack()
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "goBack:");
    RefPtr backItem = m_backForwardList->goBackItemSkippingItemsWithoutUserGesture();
    if (!backItem)
        return nullptr;

    Ref frameItem = backItem->mainFrameItem();
    if (RefPtr currentItem = m_backForwardList->currentItem()) {
        if (RefPtr childItem = currentItem->navigatedFrameID() ? frameItem->childItemForFrameID(*currentItem->navigatedFrameID()) : nullptr)
            frameItem = childItem.releaseNonNull();
    }

    return goToBackForwardItem(frameItem, FrameLoadType::Back);
}

RefPtr<API::Navigation> WebPageProxy::goToBackForwardItem(WebBackForwardListItem& item)
{
    return goToBackForwardItem(item.protectedMainFrameItem(), FrameLoadType::IndexedBackForward);
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

        if (item != m_backForwardList->currentItem())
            m_backForwardList->goToItem(*item);
    }

    Ref process = m_legacyMainFrameProcess;
    Ref navigation = m_navigationState->createBackForwardNavigation(process->coreProcessIdentifier(), frameItem, m_backForwardList->protectedCurrentItem(), frameLoadType);
    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();
    pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), item->url() });

    process->markProcessAsRecentlyUsed();

    Ref frameState = item->mainFrameState();
    if (protectedPreferences()->siteIsolationEnabled()) {
        if (RefPtr frame = WebFrameProxy::webFrame(frameItem.frameID()); frame && frame->page() == this) {
            process = frame->process();
            frameState = frameItem.copyFrameStateWithChildren();
        }
    }
    auto publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(URL(item->url()));
    process->send(Messages::WebPage::GoToBackForwardItem({ navigation->navigationID(), WTFMove(frameState), frameLoadType, ShouldTreatAsContinuingLoad::No, std::nullopt, m_lastNavigationWasAppInitiated, std::nullopt, WTFMove(publicSuffix), { }, WebCore::ProcessSwapDisposition::None }), webPageIDInProcess(process));
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
    RefPtr protectedPageClient { pageClient() };

    if (!m_navigationClient->didChangeBackForwardList(*this, added, removed) && m_loaderClient)
        m_loaderClient->didChangeBackForwardList(*this, added, WTFMove(removed));

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    pageLoadState->setCanGoBack(transaction, m_backForwardList->backItem());
    pageLoadState->setCanGoForward(transaction, m_backForwardList->forwardItem());
}

void WebPageProxy::shouldGoToBackForwardListItem(BackForwardItemIdentifier itemID, bool inBackForwardCache, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&& completionHandler)
{
    RefPtr protectedPageClient { pageClient() };

    if (RefPtr item = m_backForwardList->itemForID(itemID)) {
        auto innerHandler = [protectedPageClient = WTFMove(protectedPageClient), completionHandler = WTFMove(completionHandler)] (bool result) mutable {
            completionHandler(result ? WebCore::ShouldGoToHistoryItem::Yes : WebCore::ShouldGoToHistoryItem::No);
        };

        m_navigationClient->shouldGoToBackForwardListItem(*this, *item, inBackForwardCache, WTFMove(innerHandler));
        return;
    }

    completionHandler(WebCore::ShouldGoToHistoryItem::ItemUnknown);
}

void WebPageProxy::shouldGoToBackForwardListItemSync(BackForwardItemIdentifier itemID, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&& completionHandler)
{
    shouldGoToBackForwardListItem(itemID, false, WTFMove(completionHandler));
}

bool WebPageProxy::shouldKeepCurrentBackForwardListItemInList(WebBackForwardListItem& item)
{
    RefPtr protectedPageClient { pageClient() };

    return !m_loaderClient || m_loaderClient->shouldKeepCurrentBackForwardListItemInList(*this, item);
}

bool WebPageProxy::canShowMIMEType(const String& mimeType)
{
    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

    if (protectedPreferences()->pdfJSViewerEnabled() && MIMETypeRegistry::isPDFMIMEType(mimeType))
        return true;

#if PLATFORM(COCOA)
    // On Mac, we can show PDFs.
    if (MIMETypeRegistry::isPDFMIMEType(mimeType) && !WebProcessPool::omitPDFSupport())
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
    protectedWebsiteDataStore()->protectedNetworkProcess()->send(Messages::NetworkProcess::SetSessionIsControlledByAutomation(m_websiteDataStore->sessionID(), m_controlledByAutomation), 0);
}

RefPtr<WebAutomationSession> WebPageProxy::activeAutomationSession() const
{
    if (!m_controlledByAutomation)
        return nullptr;
    return m_configuration->processPool().automationSession();
}

void WebPageProxy::createInspectorTarget(IPC::Connection& connection, const String& targetId, Inspector::InspectorTargetType type)
{
    MESSAGE_CHECK_BASE(!targetId.isEmpty(), connection);
    m_inspectorController->createWebPageInspectorTarget(targetId, type);
}

void WebPageProxy::destroyInspectorTarget(IPC::Connection& connection, const String& targetId)
{
    MESSAGE_CHECK_BASE(!targetId.isEmpty(), connection);
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
    RefPtr inspectorDebuggable = m_inspectorDebuggable;
    return inspectorDebuggable && inspectorDebuggable->inspectable();
}

void WebPageProxy::setInspectable(bool inspectable)
{
    RefPtr inspectorDebuggable = m_inspectorDebuggable;
    if (!inspectorDebuggable || inspectorDebuggable->inspectable() == inspectable)
        return;

    inspectorDebuggable->setInspectable(inspectable);

    protectedWebsiteDataStore()->updateServiceWorkerInspectability();
}

String WebPageProxy::remoteInspectionNameOverride() const
{
    return m_inspectorDebuggable ? m_inspectorDebuggable->nameOverride() : nullString();
}

void WebPageProxy::setRemoteInspectionNameOverride(const String& name)
{
    if (RefPtr inspectorDebuggable = m_inspectorDebuggable)
        inspectorDebuggable->setNameOverride(name);
}

void WebPageProxy::remoteInspectorInformationDidChange()
{
    if (RefPtr inspectorDebuggable = m_inspectorDebuggable)
        inspectorDebuggable->update();
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

void WebPageProxy::setObscuredContentInsets(const WebCore::FloatBoxExtent& obscuredContentInsets)
{
    if (m_internals->obscuredContentInsets == obscuredContentInsets)
        return;

    m_internals->obscuredContentInsets = obscuredContentInsets;

    if (RefPtr pageClient = this->pageClient())
        pageClient->obscuredContentInsetsDidChange();

    if (!hasRunningProcess())
        return;

    protectedDrawingArea()->updateDebugIndicator();

#if PLATFORM(COCOA)
    send(Messages::WebPage::SetObscuredContentInsetsFenced(m_internals->obscuredContentInsets, protectedDrawingArea()->createFence()));
#else
    send(Messages::WebPage::SetObscuredContentInsets(m_internals->obscuredContentInsets));
#endif
}

const WebCore::FloatBoxExtent& WebPageProxy::obscuredContentInsets() const
{
    return m_internals->obscuredContentInsets;
}

Color WebPageProxy::underlayColor() const
{
    return internals().underlayColor;
}

void WebPageProxy::setShouldSuppressHDR(bool shouldSuppressHDR)
{
#if PLATFORM(IOS_FAMILY)
    Ref processPool = m_configuration->processPool();
    processPool->suppressEDR(shouldSuppressHDR);
#endif
    if (hasRunningProcess())
        send(Messages::WebPage::SetShouldSuppressHDR(shouldSuppressHDR));
}

void WebPageProxy::setUnderlayColor(const Color& color)
{
    if (internals().underlayColor == color)
        return;

    internals().underlayColor = color;

    if (hasRunningProcess())
        send(Messages::WebPage::SetUnderlayColor(color));
}

Color WebPageProxy::underPageBackgroundColorIgnoringPlatformColor() const
{
    if (internals().underPageBackgroundColorOverride.isValid())
        return internals().underPageBackgroundColorOverride;

    if (internals().pageExtendedBackgroundColor.isValid())
        return internals().pageExtendedBackgroundColor;

    return { };
}

Color WebPageProxy::underPageBackgroundColor() const
{
    if (auto color = underPageBackgroundColorIgnoringPlatformColor(); color.isValid())
        return color;

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

    if (changesUnderPageBackgroundColor) {
        if (RefPtr pageClient = this->pageClient())
            pageClient->underPageBackgroundColorWillChange();
    }

    internals().underPageBackgroundColorOverride = WTFMove(newUnderPageBackgroundColorOverride);

    if (changesUnderPageBackgroundColor) {
        if (RefPtr pageClient = this->pageClient())
            pageClient->underPageBackgroundColorDidChange();
    }

    if (m_hasPendingUnderPageBackgroundColorOverrideToDispatch)
        return;

    m_hasPendingUnderPageBackgroundColorOverrideToDispatch = true;

    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!protectedThis->m_hasPendingUnderPageBackgroundColorOverrideToDispatch)
            return;

        protectedThis->m_hasPendingUnderPageBackgroundColorOverrideToDispatch = false;

        if (RefPtr pageClient = protectedThis->m_pageClient.get())
            pageClient->didChangeBackgroundColor();

        if (protectedThis->hasRunningProcess())
            protectedThis->send(Messages::WebPage::SetUnderPageBackgroundColorOverride(protectedThis->internals().underPageBackgroundColorOverride));
    });
}

void WebPageProxy::viewWillStartLiveResize()
{
    if (!hasRunningProcess())
        return;

    closeOverlayedViews();

    protectedDrawingArea()->viewWillStartLiveResize();

    send(Messages::WebPage::ViewWillStartLiveResize());
}

void WebPageProxy::viewWillEndLiveResize()
{
    if (!hasRunningProcess())
        return;

    protectedDrawingArea()->viewWillEndLiveResize();

    send(Messages::WebPage::ViewWillEndLiveResize());
}

void WebPageProxy::setViewNeedsDisplay(const Region& region)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->setViewNeedsDisplay(region);
}

void WebPageProxy::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, ScrollIsAnimated animated)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->requestScroll(scrollPosition, scrollOrigin, animated);
}

WebCore::FloatPoint WebPageProxy::viewScrollPosition() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->viewScrollPosition() : WebCore::FloatPoint { };
}

void WebPageProxy::setNeedsScrollGeometryUpdates(bool needsScrollGeometryUpdates)
{
    if (m_needsScrollGeometryUpdates == needsScrollGeometryUpdates)
        return;

    m_needsScrollGeometryUpdates = needsScrollGeometryUpdates;
    send(Messages::WebPage::SetNeedsScrollGeometryUpdates(m_needsScrollGeometryUpdates));
}

void WebPageProxy::setSuppressVisibilityUpdates(bool flag)
{
    if (m_suppressVisibilityUpdates == flag)
        return;

    WEBPAGEPROXY_RELEASE_LOG(ViewState, "setSuppressVisibilityUpdates: %d", flag);
    m_suppressVisibilityUpdates = flag;

    if (!m_suppressVisibilityUpdates) {
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
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
    RefPtr pageClient = this->pageClient();
    internals().activityState.remove(flagsToUpdate);
    if (flagsToUpdate & ActivityState::IsFocused && pageClient->isViewFocused())
        internals().activityState.add(ActivityState::IsFocused);
    if (flagsToUpdate & ActivityState::WindowIsActive && pageClient->isViewWindowActive())
        internals().activityState.add(ActivityState::WindowIsActive);
    if (flagsToUpdate & ActivityState::IsVisible) {
        bool isNowVisible = pageClient->isMainViewVisible();
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
    if (pageClient->isMainViewVisible() && m_mediaUsageManager && m_mediaUsageManager->isPlayingVideoInViewport())
        isVisuallyIdle = false;
#endif
    if (flagsToUpdate & ActivityState::IsVisuallyIdle && isVisuallyIdle)
        internals().activityState.add(ActivityState::IsVisuallyIdle);
    if (flagsToUpdate & ActivityState::IsAudible && isPlayingAudio() && !(internals().mutedState.contains(MediaProducerMutedState::AudioIsMuted)))
        internals().activityState.add(ActivityState::IsAudible);
    if (flagsToUpdate & ActivityState::IsLoading && protectedPageLoadState()->isLoading())
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

    RefPtr pageClient = this->pageClient();

    internals().potentiallyChangedActivityStateFlags.add(mayHaveChanged);
    m_activityStateChangeWantsSynchronousReply = m_activityStateChangeWantsSynchronousReply || replyMode == ActivityStateChangeReplyMode::Synchronous;

    // We need to do this here instead of inside dispatchActivityStateChange() or viewIsBecomingVisible() because these don't run when the view doesn't
    // have a running WebProcess. For the same reason, we need to rely on PageClient::isViewVisible() instead of WebPageProxy::isViewVisible().
    if (internals().potentiallyChangedActivityStateFlags & ActivityState::IsVisible && m_shouldReloadDueToCrashWhenVisible && pageClient->isMainViewVisible()) {
        RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis && std::exchange(protectedThis->m_shouldReloadDueToCrashWhenVisible, false)) {
                WEBPAGEPROXY_RELEASE_LOG_WITH_THIS(ViewState, protectedThis, "activityStateDidChange: view is becoming visible after a crash, attempt a reload");
                protectedThis->tryReloadAfterProcessTermination();
            }
        });
    }

    if (m_suppressVisibilityUpdates && dispatchMode != ActivityStateChangeDispatchMode::Immediate) {
        WEBPAGEPROXY_RELEASE_LOG(ViewState, "activityStateDidChange: Returning early due to m_suppressVisibilityUpdates");
        return;
    }

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    bool isNewlyInWindow = !isInWindow() && (mayHaveChanged & ActivityState::IsInWindow) && pageClient->isViewInWindow();
    if (dispatchMode == ActivityStateChangeDispatchMode::Immediate || isNewlyInWindow) {
        dispatchActivityStateChange();
        return;
    }
    scheduleActivityStateUpdate();
#elif PLATFORM(GTK) || PLATFORM(WPE)
    UNUSED_PARAM(dispatchMode);
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
    // FIXME: The tvOS behavior applies to visionOS as well when AVPlayerViewController is used for
    // iPad compatability apps. So the same fix for tvOS should be made for visionOS.
    if (RefPtr videoPresentationManager = m_videoPresentationManager; videoPresentationManager && videoPresentationManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard)
#if PLATFORM(VISION)
        && PAL::currentUserInterfaceIdiomIsVision()
#endif
        ) {
        videoPresentationManager->requestHideAndExitFullscreen();
    }
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateDefaultSpatialTrackingLabel();
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    m_mainFrameProcessActivityState->viewDidLeaveWindow();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().viewDidLeaveWindow();
    });
#endif
}

void WebPageProxy::viewDidEnterWindow()
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateDefaultSpatialTrackingLabel();
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    m_mainFrameProcessActivityState->viewDidEnterWindow();
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [](auto& remotePageProxy) {
        remotePageProxy.processActivityState().viewDidEnterWindow();
    });
#endif
}

void WebPageProxy::dispatchActivityStateChange()
{
#if PLATFORM(COCOA)
    if (m_activityStateChangeDispatcher->isScheduled())
        m_activityStateChangeDispatcher->invalidate();
    m_hasScheduledActivityStateUpdate = false;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    internals().activityStateChangeTimer.stop();
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
            viewIsBecomingInvisible();
    }

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (internals().potentiallyChangedActivityStateFlags & ActivityState::IsConnectedToHardwareConsole)
        isConnectedToHardwareConsoleDidChange();
#endif

    bool isNowInWindow = (changed & ActivityState::IsInWindow) && isInWindow();
    // We always want to wait for the Web process to reply if we've been in-window before and are coming back in-window.
    if (m_viewWasEverInWindow && isNowInWindow) {
        if (protectedDrawingArea()->hasVisibleContent() && m_waitsForPaintAfterViewDidMoveToWindow && !m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow)
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
        resetPointerLockState();
#endif

    if (changed & ActivityState::IsVisible) {
        if (isViewVisible())
            internals().visiblePageToken = protectedLegacyMainFrameProcess()->visiblePageToken();
        else {
            internals().visiblePageToken = nullptr;

            // If we've started the responsiveness timer as part of telling the web process to update the backing store
            // state, it might not send back a reply (since it won't paint anything if the web page is hidden) so we
            // stop the unresponsiveness timer here.
            protectedLegacyMainFrameProcess()->stopResponsivenessTimer();
        }
    }

    if (changed & ActivityState::IsInWindow) {
        if (isInWindow())
            viewDidEnterWindow();
        else
            viewDidLeaveWindow();
    }

#if ENABLE(WEB_AUTHN) && HAVE(WEB_AUTHN_AS_MODERN)
    if (RefPtr webAuthnCredentialsMessenger = m_webAuthnCredentialsMessenger; (changed & ActivityState::WindowIsActive) && webAuthnCredentialsMessenger) {
        RefPtr pageClient = this->pageClient();
        if (pageClient && pageClient->isViewWindowActive())
            webAuthnCredentialsMessenger->makeActiveConditionalAssertion();
    }
#endif

    if (isNowInWindow)
        protectedDrawingArea()->hideContentUntilPendingUpdate();

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
    bool processSuppressionEnabled = protectedPreferences()->pageVisibilityBasedProcessSuppressionEnabled();

    Ref processPool = m_configuration->processPool();

    // If process suppression is not enabled take a token on the process pool to disable suppression of support processes.
    if (!processSuppressionEnabled)
        internals().preventProcessSuppressionCount = processPool->processSuppressionDisabledForPageCount();
    else if (!internals().preventProcessSuppressionCount)
        internals().preventProcessSuppressionCount = nullptr;

    if (internals().activityState & ActivityState::IsVisuallyIdle)
        internals().pageIsUserObservableCount = nullptr;
    else if (!internals().pageIsUserObservableCount)
        internals().pageIsUserObservableCount = processPool->userObservablePageCount();

#if USE(RUNNINGBOARD)
    if (isViewVisible()) {
        if (!hasValidVisibleActivity()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because the view is visible");
            takeVisibleActivity();
        }
    } else if (hasValidVisibleActivity()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because the view is no longer visible");
        dropVisibleActivity();
    }

    bool isAudible = internals().activityState.contains(ActivityState::IsAudible);
    if (isAudible) {
        if (!hasValidAudibleActivity()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because we are playing audio");
            takeAudibleActivity();
        }
        if (internals().audibleActivityTimer.isActive()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: Cancelling timer to release foreground assertion");
            internals().audibleActivityTimer.stop();
        }
    } else if (hasValidAudibleActivity()) {
        if (!internals().audibleActivityTimer.isActive()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess starting timer to release a foreground assertion in %g seconds if audio doesn't start to play", audibleActivityClearDelay.seconds());
            internals().audibleActivityTimer.startOneShot(audibleActivityClearDelay);
        }
    }

    bool isCapturingMedia = internals().activityState.contains(ActivityState::IsCapturingMedia);
    bool hasMutedCapture = internals().mediaState.containsAny(MediaProducer::MutedCaptureMask);

    if (!isCapturingMedia && hasMutedCapture) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: taking a web process background assertion for muted media capture");
        takeMutedCaptureAssertion();
    } else if (hasValidMutedCaptureAssertion()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: releasing a web process background assertion for muted media capture");
        dropMutedCaptureAssertion();
    }

    if (isCapturingMedia) {
        if (!hasValidCapturingActivity()) {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is taking a foreground assertion because media capture is active");
            takeCapturingActivity();
        }
    } else if (hasValidCapturingActivity()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "updateThrottleState: UIProcess is releasing a foreground assertion because media capture is no longer active");
        dropCapturingActivity();
    }
#endif
}

void WebPageProxy::clearAudibleActivity()
{
    WEBPAGEPROXY_RELEASE_LOG(ProcessSuspension, "clearAudibleActivity: UIProcess is releasing a foreground assertion because we are no longer playing audio");
    dropAudibleActivity();
#if ENABLE(EXTENSION_CAPABILITIES)
    updateMediaCapability();
#endif
}

void WebPageProxy::updateHiddenPageThrottlingAutoIncreases()
{
    if (!protectedPreferences()->hiddenPageDOMTimerThrottlingAutoIncreases())
        internals().hiddenPageDOMTimerThrottlingAutoIncreasesCount = nullptr;
    else if (!internals().hiddenPageDOMTimerThrottlingAutoIncreasesCount)
        internals().hiddenPageDOMTimerThrottlingAutoIncreasesCount = m_configuration->protectedProcessPool()->hiddenPageThrottlingAutoIncreasesCount();
}

void WebPageProxy::waitForDidUpdateActivityState(ActivityStateChangeID activityStateChangeID)
{
    if (!hasRunningProcess())
        return;

    if (m_legacyMainFrameProcess->state() != WebProcessProxy::State::Running)
        return;

    // If we have previously timed out with no response from the WebProcess, don't block the UIProcess again until it starts responding.
    if (m_waitingForDidUpdateActivityState)
        return;

#if USE(RUNNINGBOARD)
    // Hail Mary check. Should not be possible (dispatchActivityStateChange should force async if not visible,
    // and if visible we should be holding an assertion) - but we should never block on a suspended process.
    if (!hasValidVisibleActivity()) {
        ASSERT_NOT_REACHED();
        return;
    }
#endif

    m_waitingForDidUpdateActivityState = true;

    protectedDrawingArea()->waitForDidUpdateActivityState(activityStateChangeID);
}

IntSize WebPageProxy::viewSize() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->viewSize() : IntSize { };
}

void WebPageProxy::setInitialFocus(bool forward, bool isKeyboardEventValid, const std::optional<WebKeyboardEvent>& keyboardEvent, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::SetInitialFocus(forward, isKeyboardEventValid, keyboardEvent), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->protectedThrottler()->backgroundActivity("WebPageProxy::setInitialFocus"_s)] () mutable {
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

    if (!m_uiClient->needsFontAttributes())
        return;

    requestFontAttributesAtSelectionStart([this, protectedThis = Ref { *this }](auto& attributes) {
        m_uiClient->didChangeFontAttributes(attributes);
    });
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

    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;

    auto completionHandler = [weakThis = WeakPtr { *this }, callbackFunction = WTFMove(callbackFunction), commandName, argument, targetFrameID] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callbackFunction();

        protectedThis->sendWithAsyncReplyToProcessContainingFrame(targetFrameID, Messages::WebPage::ExecuteEditCommandWithCallback(commandName, argument), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = weakThis->processContainingFrame(targetFrameID)->protectedThrottler()->backgroundActivity("WebPageProxy::executeEditCommand"_s)] () mutable {
            callbackFunction();
        });
    };

    if (auto pasteAccessCategory = pasteAccessCategoryForCommand(commandName))
        willPerformPasteCommand(*pasteAccessCategory, WTFMove(completionHandler), targetFrameID);
    else
        completionHandler();
}

void WebPageProxy::executeEditCommand(const String& commandName, const String& argument)
{
    if (!hasRunningProcess())
        return;

    RefPtr focusedFrame = focusedOrMainFrame();
    if (!focusedFrame)
        return;
    auto frameID = focusedFrame->frameID();

    auto completionHandler = [weakThis = WeakPtr { *this }, commandName, argument, frameID] () mutable {
        static NeverDestroyed<String> ignoreSpellingCommandName(MAKE_STATIC_STRING_IMPL("ignoreSpelling"));
        if (!weakThis)
            return;

        if (commandName == ignoreSpellingCommandName)
            ++weakThis->m_pendingLearnOrIgnoreWordMessageCount;

        weakThis->sendToProcessContainingFrame(frameID, Messages::WebPage::ExecuteEditCommand(commandName, argument));
    };

    if (auto pasteAccessCategory = pasteAccessCategoryForCommand(commandName)) {
        if (auto replyID = willPerformPasteCommand(*pasteAccessCategory, WTFMove(completionHandler), frameID))
            protectedWebsiteDataStore()->protectedNetworkProcess()->protectedConnection()->waitForAsyncReplyAndDispatchImmediately<Messages::NetworkProcess::AllowFilesAccessFromWebProcess>(*replyID, 100_ms);
    } else
        completionHandler();
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
    if (m_legacyMainFrameProcess->isConnectedToHardwareConsole()) {
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
    return m_isProcessingIsConnectedToHardwareConsoleDidChangeNotification || m_legacyMainFrameProcess->isConnectedToHardwareConsole();
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

#if !PLATFORM(COCOA)
void WebPageProxy::didCommitLayerTree(const RemoteLayerTreeTransaction&, const std::optional<MainFrameData>&, const PageData&, const TransactionID&)
{
}

void WebPageProxy::didCommitMainFrameData(const MainFrameData&, const TransactionID&)
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->makeViewBlank(false);
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
            if (RefPtr pageClient = this->pageClient())
                pageClient->makeViewBlank(true);
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
    WebPasteboardProxy::singleton().grantAccessToCurrentTypes(m_legacyMainFrameProcess.get(), dragStorageName);
#endif
    launchInitialProcessIfNecessary();
    performDragControllerAction(DragControllerAction::Entered, dragData);
}

void WebPageProxy::dragUpdated(DragData& dragData, const String& dragStorageName)
{
#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().grantAccessToCurrentTypes(m_legacyMainFrameProcess.get(), dragStorageName);
#endif
    performDragControllerAction(DragControllerAction::Updated, dragData);
}

void WebPageProxy::dragExited(DragData& dragData)
{
    performDragControllerAction(DragControllerAction::Exited, dragData);
}

void WebPageProxy::performDragOperation(DragData& dragData, const String& dragStorageName, SandboxExtension::Handle&& sandboxExtensionHandle, Vector<SandboxExtension::Handle>&& sandboxExtensionsForUpload)
{
    if (!hasRunningProcess())
        return;

#if PLATFORM(GTK)
    URL url { dragData.asURL() };
    if (url.protocolIsFile())
        protectedLegacyMainFrameProcess()->assumeReadAccessToBaseURL(*this, url.string(), [] { });
    else if (!dragData.fileNames().isEmpty())
        protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AllowFilesAccessFromWebProcess(siteIsolatedProcess().coreProcessIdentifier(), dragData.fileNames()), [] { });

    performDragControllerAction(DragControllerAction::PerformDragOperation, dragData);
#elif PLATFORM(COCOA)
    grantAccessToCurrentPasteboardData(dragStorageName, [this, protectedThis = Ref { *this }, dragStorageName, dragData = WTFMove(dragData), sandboxExtensionHandle = WTFMove(sandboxExtensionHandle), sandboxExtensionsForUpload = WTFMove(sandboxExtensionsForUpload)] () mutable {
        sendWithAsyncReply(Messages::WebPage::PerformDragOperation(dragData, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionsForUpload)), [this, protectedThis = Ref { *this }] (bool handled) {
            if (RefPtr pageClient = this->pageClient())
                pageClient->didPerformDragOperation(handled);
        });
    });
#else
    sendWithAsyncReply(Messages::WebPage::PerformDragOperation(dragData, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionsForUpload)), [this, protectedThis = Ref { *this }] (bool handled) {
        if (RefPtr pageClient = this->pageClient())
            pageClient->didPerformDragOperation(handled);
    });
#endif
}

void WebPageProxy::performDragControllerAction(DragControllerAction action, DragData& dragData, const std::optional<WebCore::FrameIdentifier>& frameID)
{
    if (!hasRunningProcess())
        return;

    auto completionHandler = [this, protectedThis = Ref { *this }, action, dragData] (std::optional<WebCore::DragOperation> dragOperation, WebCore::DragHandlingMethod dragHandlingMethod, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted, const IntRect& insertionRect, const IntRect& editableElementRect, std::optional<WebCore::RemoteUserInputEventData> remoteUserInputEventData) mutable {
        if (!m_pageClient)
            return;

        if (!remoteUserInputEventData) {
            didPerformDragControllerAction(dragOperation, dragHandlingMethod, mouseIsOverFileInput, numberOfItemsToBeAccepted, insertionRect, editableElementRect);
            return;
        }
        dragData.setClientPosition(roundedIntPoint(remoteUserInputEventData->transformedPoint));
        performDragControllerAction(action, dragData, remoteUserInputEventData->targetFrameID);
    };
#if PLATFORM(GTK)
    ASSERT(dragData.platformData());
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::PerformDragControllerAction(action, dragData.clientPosition(), dragData.globalPosition(), dragData.draggingSourceOperationMask(), *dragData.platformData(), dragData.flags()), WTFMove(completionHandler));
#else
    auto filenames = dragData.fileNames();

    auto afterAllowed = [weakThis = WeakPtr { *this }, frameID, action, dragData = WTFMove(dragData), completionHandler = WTFMove(completionHandler)] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::PerformDragControllerAction(frameID, action, dragData), WTFMove(completionHandler));
    };

    auto processID = siteIsolatedProcess().coreProcessIdentifier();
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        processID = frame->process().coreProcessIdentifier();

    if (!filenames.size())
        return afterAllowed();

    protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AllowFilesAccessFromWebProcess(processID, filenames), WTFMove(afterAllowed));
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->didPerformDragControllerAction();
}

#if PLATFORM(GTK) || PLATFORM(WPE)
void WebPageProxy::startDrag(SelectionData&& selectionData, OptionSet<WebCore::DragOperation> dragOperationMask, std::optional<ShareableBitmap::Handle>&& dragImageHandle, IntPoint&& dragImageHotspot)
{
#if PLATFORM(GTK)
    if (RefPtr pageClient = this->pageClient()) {
        RefPtr dragImage = dragImageHandle ? ShareableBitmap::create(WTFMove(*dragImageHandle)) : nullptr;
        pageClient->startDrag(WTFMove(selectionData), dragOperationMask, WTFMove(dragImage), WTFMove(dragImageHotspot));
    }
#endif
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
        dragEnded(roundedIntPoint(remoteUserInputEventData->transformedPoint), globalPosition, dragOperationMask, remoteUserInputEventData->targetFrameID);
    };

    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DragEnded(frameID, clientPosition, globalPosition, dragOperationMask), WTFMove(completionHandler));
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

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    auto previousRect = std::exchange(currentDragRect, dragCaretRect);
    pageClient->didChangeDragCaretRect(previousRect, dragCaretRect);
}

#if ENABLE(MODEL_PROCESS)
void WebPageProxy::modelDragEnded(const WebCore::NodeIdentifier nodeIdentifier)
{
    send(Messages::WebPage::ModelDragEnded(nodeIdentifier));
}
#endif
#endif

#if ENABLE(MODEL_PROCESS)
void WebPageProxy::requestInteractiveModelElementAtPoint(const IntPoint clientPosition)
{
    send(Messages::WebPage::RequestInteractiveModelElementAtPoint(clientPosition));
}

void WebPageProxy::stageModeSessionDidUpdate(std::optional<NodeIdentifier> nodeID, const TransformationMatrix& transform)
{
    send(Messages::WebPage::StageModeSessionDidUpdate(nodeID, transform));
}

void WebPageProxy::stageModeSessionDidEnd(std::optional<NodeIdentifier> nodeID)
{
    send(Messages::WebPage::StageModeSessionDidEnd(nodeID));
}
#endif

template <typename T>
requires std::derived_from<T, WebEvent>
static std::optional<T> removeOldRedundantEvent(Deque<T>& queue, WebEventType incomingEventType, OptionSet<WebEventType> eventFilter)
{
    if (!eventFilter.contains(incomingEventType))
        return std::nullopt;

    auto it = queue.rbegin();
    auto end = queue.rend();

    // Must not remove the first event in the deque, since it is already being dispatched.
    if (it != end)
        --end;

    for (; it != end; ++it) {
        auto type = it->type();
        if (type == incomingEventType) {
            auto event = *it;
            queue.remove(--it.base());
            return event;
        }
        if (!eventFilter.contains(type))
            break;
    }
    return std::nullopt;
}

void WebPageProxy::sendMouseEvent(FrameIdentifier frameID, const NativeWebMouseEvent& event, std::optional<Vector<SandboxExtensionHandle>>&& sandboxExtensions)
{
    if (event.type() == WebEventType::MouseDown || event.type() == WebEventType::MouseUp)
        processContainingFrame(frameID)->recordUserGestureAuthorizationToken(frameID, webPageIDInMainFrameProcess(), event.authorizationToken());
    if (event.isActivationTriggeringEvent())
        internals().lastActivationTimestamp = MonotonicTime::now();

    sendToProcessContainingFrame(frameID, Messages::WebPage::MouseEvent(frameID, event, WTFMove(sandboxExtensions)));
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
    auto removedEvent = removeOldRedundantEvent(internals().mouseEventQueue, event.type(), { WebEventType::MouseMove, WebEventType::MouseForceChanged });
    if (removedEvent && removedEvent->type() == WebEventType::MouseMove)
        internals().coalescedMouseEvents.append(CheckedRef { *removedEvent }.get());

    internals().mouseEventQueue.append(event);

    LOG_WITH_STREAM(MouseHandling, stream << "UIProcess: " << (removedEvent ? "replaced" : "enqueued") << " mouse event " << event.type() << " (queue size " << internals().mouseEventQueue.size() << ", coalesced events size " << internals().coalescedMouseEvents.size() << ")");

    if (event.type() != WebEventType::MouseMove)
        send(Messages::WebPage::FlushDeferredDidReceiveMouseEvent());

    if (internals().mouseEventQueue.size() == 1) // Otherwise, called from DidReceiveEvent message handler.
        processNextQueuedMouseEvent();
    else if (++m_deferredMouseEvents >= 20)
        WEBPAGEPROXY_RELEASE_LOG(MouseHandling, "handleMouseEvent: skipped called processNextQueuedMouseEvent 20 times, possibly stuck?");
}

void WebPageProxy::dispatchMouseDidMoveOverElementAsynchronously(const NativeWebMouseEvent& event)
{
    sendWithAsyncReply(Messages::WebPage::PerformHitTestForMouseEvent { event }, [this, protectedThis = Ref { *this }] (WebHitTestResultData&& hitTestResult, OptionSet<WebEventModifier> modifiers) {
        if (!isClosed())
            mouseDidMoveOverElement(WTFMove(hitTestResult), modifiers);
    });
}

void WebPageProxy::processNextQueuedMouseEvent()
{
    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

    ASSERT(!internals().mouseEventQueue.isEmpty());
    m_deferredMouseEvents = 0;

    const CheckedRef event = internals().mouseEventQueue.first();

#if ENABLE(CONTEXT_MENUS)
    if (m_waitingForContextMenuToShow) {
        WEBPAGEPROXY_RELEASE_LOG(MouseHandling, "processNextQueuedMouseEvent: Waiting for context menu to show.");
        mouseEventHandlingCompleted(event->type(), false, std::nullopt);
        return;
    }
#endif

    RefPtr pageClient = this->pageClient();
    if (pageClient && pageClient->windowIsFrontWindowUnderMouse(event))
        setToolTip(String());

    Ref process = m_legacyMainFrameProcess;
    auto eventType = event->type();
    if (eventType == WebEventType::MouseDown || eventType == WebEventType::MouseForceChanged || eventType == WebEventType::MouseForceDown)
        process->startResponsivenessTimer(WebProcessProxy::UseLazyStop::Yes);
    else if (eventType != WebEventType::MouseMove) {
        // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
        process->startResponsivenessTimer();
    }

    std::optional<Vector<SandboxExtension::Handle>> sandboxExtensions;

#if PLATFORM(MAC)
    bool eventMayStartDrag = !m_currentDragOperation && eventType == WebEventType::MouseMove && event->button() != WebMouseEventButton::None;
    if (eventMayStartDrag)
        sandboxExtensions = SandboxExtension::createHandlesForMachLookup({ "com.apple.iconservices"_s, "com.apple.iconservices.store"_s }, process->auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
#endif

    auto eventWithCoalescedEvents = event;

    if (event->type() == WebEventType::MouseMove) {
        internals().coalescedMouseEvents.append(event);
        eventWithCoalescedEvents->setCoalescedEvents(internals().coalescedMouseEvents);
    }

    LOG_WITH_STREAM(MouseHandling, stream << "UIProcess: sent mouse event " << eventType << " (queue size " << internals().mouseEventQueue.size() << ", coalesced events size " << internals().coalescedMouseEvents.size() << ")");

    sendMouseEvent(m_mainFrame->frameID(), eventWithCoalescedEvents, WTFMove(sandboxExtensions));

    internals().coalescedMouseEvents.clear();
}

#if ENABLE(MAC_GESTURE_EVENTS)
void WebPageProxy::processNextQueuedGestureEvent()
{
    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

    ASSERT(!internals().gestureEventQueue.isEmpty());
    m_deferredGestureEvents = 0;

    const CheckedRef event = internals().gestureEventQueue.first();
    const auto eventType = event->type();

    protectedLegacyMainFrameProcess()->startResponsivenessTimer((eventType == WebEventType::GestureStart || eventType == WebEventType::GestureChange) ? WebProcessProxy::UseLazyStop::Yes : WebProcessProxy::UseLazyStop::No);

    LOG_WITH_STREAM(GestureHandling, stream << "UIProcess: sent gesture event " << eventType << " (queue size " << internals().gestureEventQueue.size() << ", dropped gestures since last gesture event processed: " << internals().droppedGestureEventCount << ")");

    sendGestureEvent(m_mainFrame->frameID(), event);

    internals().droppedGestureEventCount = 0;
}
#endif

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

static RectEdges<WebCore::RubberBandingBehavior> resolvedRubberBandingBehaviorEdges(RectEdges<bool> rubberBandableEdges, bool alwaysBounceVertical, bool alwaysBounceHorizontal)
{
    auto rubberBandingBehaviorValue = [&](WebCore::BoxSide side) {
        if (!rubberBandableEdges[side])
            return WebCore::RubberBandingBehavior::Never;

        auto isVertical = side == WebCore::BoxSide::Top || side == WebCore::BoxSide::Bottom;
        auto isHorizontal = !isVertical;

        if (isVertical && alwaysBounceVertical)
            return WebCore::RubberBandingBehavior::Always;

        if (isHorizontal && alwaysBounceHorizontal)
            return WebCore::RubberBandingBehavior::Always;

        return WebCore::RubberBandingBehavior::BasedOnSize;
    };

    RectEdges<WebCore::RubberBandingBehavior> result;
    for (const auto side : WebCore::allBoxSides)
        result[side] = rubberBandingBehaviorValue(side);

    return result;
}

void WebPageProxy::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    if (!hasRunningProcess())
        return;

    if (protectedDrawingArea()->shouldSendWheelEventsToEventDispatcher()) {
        continueWheelEventHandling(wheelEvent, { WheelEventProcessingSteps::SynchronousScrolling, false }, { });
        return;
    }

#if PLATFORM(MAC)
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get()) {
        auto rubberBandableEdges = rubberBandableEdgesRespectingHistorySwipe();
        auto rubberBandingBehavior = resolvedRubberBandingBehaviorEdges(rubberBandableEdges, alwaysBounceVertical(), alwaysBounceHorizontal());

        scrollingCoordinatorProxy->handleWheelEvent(wheelEvent, rubberBandingBehavior);
        // continueWheelEventHandling() will get called after the event has been handled by the scrolling thread.
    }
#endif
}

void WebPageProxy::continueWheelEventHandling(const WebWheelEvent& wheelEvent, const WheelEventHandlingResult& result, std::optional<bool> willStartSwipe)
{
    LOG_WITH_STREAM(WheelEvents, stream << "WebPageProxy::continueWheelEventHandling - " << result);

    if (!result.needsMainThreadProcessing()) {
        if (m_mainFrame && wheelEvent.phase() == WebWheelEvent::Phase::Began) {
            // When wheel events are handled entirely in the UI process, we still need to tell the web process where the mouse is for cursor updates.
            sendToProcessContainingFrame(m_mainFrame->frameID(), Messages::WebPage::SetLastKnownMousePosition(m_mainFrame->frameID(), wheelEvent.position(), wheelEvent.globalPosition()));
        }

        wheelEventHandlingCompleted(result.wasHandled);
        return;
    }

    if (!m_mainFrame)
        return;

    auto rubberBandableEdges = rubberBandableEdgesRespectingHistorySwipe();
    auto rubberBandingBehavior = resolvedRubberBandingBehaviorEdges(rubberBandableEdges, alwaysBounceVertical(), alwaysBounceHorizontal());

    sendWheelEvent(m_mainFrame->frameID(), wheelEvent, result.steps, rubberBandingBehavior, willStartSwipe, result.wasHandled);
}

void WebPageProxy::sendWheelEvent(WebCore::FrameIdentifier frameID, const WebWheelEvent& event, OptionSet<WheelEventProcessingSteps> processingSteps, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges, std::optional<bool> willStartSwipe, bool wasHandledForScrolling)
{
#if HAVE(DISPLAY_LINK)
    internals().wheelEventActivityHysteresis.impulse();
#endif

    if (!hasRunningProcess())
        return;

    Ref process = processContainingFrame(frameID);
    if (protectedDrawingArea()->shouldSendWheelEventsToEventDispatcher()) {
        sendWheelEventScrollingAccelerationCurveIfNecessary(frameID, event);
        process->protectedConnection()->send(Messages::EventDispatcher::WheelEvent(webPageIDInProcess(process), event, rubberBandableEdges), 0, { }, Thread::QOS::UserInteractive);
    } else {
        sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::HandleWheelEvent(frameID, event, processingSteps, willStartSwipe), [weakThis = WeakPtr { *this }, wheelEvent = event, processingSteps, rubberBandableEdges, willStartSwipe, wasHandledForScrolling] (IPC::Connection* connection, std::optional<ScrollingNodeID> nodeID, std::optional<WheelScrollGestureState> gestureState, bool handled, std::optional<RemoteUserInputEventData> remoteWheelEventData) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!protectedThis->hasRunningProcess())
                return;

            if (remoteWheelEventData) {
                wheelEvent.setPosition(roundedIntPoint(remoteWheelEventData->transformedPoint));
                protectedThis->sendWheelEvent(remoteWheelEventData->targetFrameID, wheelEvent, processingSteps, rubberBandableEdges, willStartSwipe, wasHandledForScrolling);
                return;
            }

            protectedThis->handleWheelEventReply(connection, wheelEvent, nodeID, gestureState, wasHandledForScrolling, handled);
        });
    }

    // Manually ping the web process to check for responsiveness since our wheel
    // event will dispatch to a non-main thread, which always responds.
    process->isResponsiveWithLazyStop();
}

void WebPageProxy::handleWheelEventReply(IPC::Connection* connection, const WebWheelEvent& event, std::optional<ScrollingNodeID> nodeID, std::optional<WheelScrollGestureState> gestureState, bool wasHandledForScrolling, bool wasHandledByWebProcess)
{
    LOG_WITH_STREAM(WheelEvents, stream << "WebPageProxy::handleWheelEventReply " << platform(event) << " - handled for scrolling " << wasHandledForScrolling << " handled by web process " << wasHandledByWebProcess << " nodeID " << nodeID << " gesture state " << gestureState);

    MESSAGE_CHECK_BASE(wheelEventCoalescer().hasEventsBeingProcessed(), connection);

#if PLATFORM(MAC)
    if (CheckedPtr scrollingCoordinatorProxy = this->scrollingCoordinatorProxy()) {
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
        CheckedRef event = *oldestProcessedEvent;
        m_uiClient->didNotHandleWheelEvent(this, event.get());
        if (RefPtr pageClient = m_pageClient.get())
            pageClient->wheelEventWasNotHandledByWebCore(event.get());
    }

    if (auto eventToSend = wheelEventCoalescer().nextEventToDispatch()) {
        handleWheelEvent(CheckedRef { *eventToSend }.get());
        return;
    }

    if (RefPtr automationSession = m_configuration->processPool().automationSession())
        automationSession->wheelEventsFlushedForPage(*this);
}

void WebPageProxy::cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent& nativeWheelEvent)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get()) {
        scrollingCoordinatorProxy->cacheWheelEventScrollingAccelerationCurve(nativeWheelEvent);
        return;
    }

    ASSERT(drawingArea()->shouldSendWheelEventsToEventDispatcher());

    if (nativeWheelEvent.momentumPhase() != WebWheelEvent::Phase::Began)
        return;

    if (!protectedPreferences()->momentumScrollingAnimatorEnabled())
        return;

    // FIXME: We should not have to fetch the curve repeatedly, but it can also change occasionally.
    internals().scrollingAccelerationCurve = ScrollingAccelerationCurve::fromNativeWheelEvent(nativeWheelEvent).or_else([identifier = identifier()] {
        auto curve = ScrollingAccelerationCurve::fallbackCurve();
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&curve, identifier] {
            UNUSED_VARIABLE(identifier);
            if (curve)
                LOG_WITH_STREAM(ScrollAnimations, stream << "WebPageProxy::cacheWheelEventScrollingAccelerationCurve - using fallback acceleration curve " << *curve << " for page " << identifier);
        });
        return curve;
    });
#endif
}

void WebPageProxy::sendWheelEventScrollingAccelerationCurveIfNecessary(WebCore::FrameIdentifier frameID, const WebWheelEvent& event)
{
    ASSERT(drawingArea()->shouldSendWheelEventsToEventDispatcher());
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (event.momentumPhase() != WebWheelEvent::Phase::Began)
        return;

    if (internals().scrollingAccelerationCurve == internals().lastSentScrollingAccelerationCurve)
        return;

    Ref process = processContainingFrame(frameID);
    Ref connection = process->connection();
    connection->send(Messages::EventDispatcher::SetScrollingAccelerationCurve(webPageIDInProcess(process), internals().scrollingAccelerationCurve), 0, { }, Thread::QOS::UserInteractive);
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
    if (!m_legacyMainFrameProcess->hasConnection() || !m_displayID)
        return;

    bool wantsFullSpeedUpdates = m_hasActiveAnimatedScroll || internals().wheelEventActivityHysteresis.state() == PAL::HysteresisState::Started;
    if (wantsFullSpeedUpdates != m_registeredForFullSpeedUpdates) {
        protectedLegacyMainFrameProcess()->setDisplayLinkForDisplayWantsFullSpeedUpdates(*m_displayID, wantsFullSpeedUpdates);
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
    auto targetFrameID = m_focusedFrame ? m_focusedFrame->frameID() : m_mainFrame->frameID();
    protectedLegacyMainFrameProcess()->recordUserGestureAuthorizationToken(targetFrameID, webPageIDInMainFrameProcess(), event.authorizationToken());
    if (event.isActivationTriggeringEvent())
        internals().lastActivationTimestamp = MonotonicTime::now();
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::KeyEvent(targetFrameID, event));
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

    Ref process = m_legacyMainFrameProcess;
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
#if PLATFORM(COCOA)
    for (auto& touchPoint : touchStartEvent.touchPoints()) {
        auto location = touchPoint.locationInRootView();
        auto update = [this, location](TrackingType& trackingType, EventTrackingRegions::EventType eventType) {
            if (trackingType == TrackingType::Synchronous)
                return;
#if ENABLE(TOUCH_EVENT_REGIONS)
            if (RefPtr drawingAreaProxy = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea)) {
                auto trackingTypeForLocation = drawingAreaProxy->eventTrackingTypeForPoint(eventType, WebCore::IntPoint(location));
                trackingType = mergeTrackingTypes(trackingType, trackingTypeForLocation);
            }
#else
            auto trackingTypeForLocation = m_scrollingCoordinatorProxy->eventTrackingTypeForPoint(eventType, roundedIntPoint(location));
            trackingType = mergeTrackingTypes(trackingType, trackingTypeForLocation);
#endif
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
        update(tracking.touchEndTracking, Type::Gestureend);
        update(tracking.touchMoveTracking, Type::Gesturechange);
        update(tracking.touchStartTracking, Type::Gesturestart);
    }
#else
    UNUSED_PARAM(touchStartEvent);
    internals().touchEventTracking.touchForceChangedTracking = TrackingType::Synchronous;
    internals().touchEventTracking.touchStartTracking = TrackingType::Synchronous;
    internals().touchEventTracking.touchMoveTracking = TrackingType::Synchronous;
    internals().touchEventTracking.touchEndTracking = TrackingType::Synchronous;
#endif // PLATFORM(COCOA)
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
void WebPageProxy::sendGestureEvent(FrameIdentifier frameID, const NativeWebGestureEvent& event)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::EventDispatcher::GestureEvent(frameID, webPageIDInProcess(processContainingFrame(frameID)), event), [protectedThis = Ref { *this }, event] (IPC::Connection* connection, std::optional<WebEventType>&& eventType, bool handled, std::optional<WebCore::RemoteUserInputEventData>&& remoteUserInputEventData) {
        if (!protectedThis->m_pageClient)
            return;
        if (!eventType)
            return;
        protectedThis->didReceiveEvent(connection, *eventType, handled, WTFMove(remoteUserInputEventData));
    });
}

void WebPageProxy::handleGestureEvent(const NativeWebGestureEvent& event)
{
    if (!hasRunningProcess())
        return;

    if (!m_mainFrame)
        return;

    if (removeOldRedundantEvent(internals().gestureEventQueue, event.type(), { WebEventType::GestureChange }))
        internals().droppedGestureEventCount++;

    internals().gestureEventQueue.append(event);

    if (internals().gestureEventQueue.size() == 1) // Otherwise, called from DidReceiveEvent message handler.
        processNextQueuedGestureEvent();
    else if (++m_deferredGestureEvents >= 20)
        WEBPAGEPROXY_RELEASE_LOG(GestureHandling, "handleGestureEvent: skipped called processNextQueuedGestureEvent 20 times, possibly stuck?");
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPageProxy::sendPreventableTouchEvent(WebCore::FrameIdentifier frameID, const WebTouchEvent& event)
{
    if (event.type() == WebEventType::TouchEnd && protectedPreferences()->verifyWindowOpenUserGestureFromUIProcess())
        processContainingFrame(frameID)->recordUserGestureAuthorizationToken(frameID, webPageIDInMainFrameProcess(), event.authorizationToken());

    if (event.isActivationTriggeringEvent())
        internals().lastActivationTimestamp = MonotonicTime::now();

    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::EventDispatcher::TouchEvent(webPageIDInProcess(processContainingFrame(frameID)), frameID, event), [this, weakThis = WeakPtr { *this }, event] (IPC::Connection* connection, bool handled, std::optional<RemoteWebTouchEvent> remoteWebTouchEvent) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (remoteWebTouchEvent)
            return sendPreventableTouchEvent(remoteWebTouchEvent->targetFrameID, remoteWebTouchEvent->transformedEvent);

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

        didReceiveEvent(connection, event.type(), handled, std::nullopt);

        RefPtr pageClient = this->pageClient();
        if (!pageClient)
            return;

        pageClient->doneWithTouchEvent(event, handled);

        if (didFinishDeferringTouchStart)
            pageClient->doneDeferringTouchStart(handled);

        if (didFinishDeferringTouchMove)
            pageClient->doneDeferringTouchMove(handled);

        if (didFinishDeferringTouchEnd)
            pageClient->doneDeferringTouchEnd(handled);
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

    RefPtr pageClient = this->pageClient();
    TrackingType touchEventsTrackingType = touchEventTrackingType(event);
    if (touchEventsTrackingType == TrackingType::NotTracking && pageClient) {
        if (isTouchStart)
            pageClient->doneDeferringTouchStart(false);
        if (isTouchMove)
            pageClient->doneDeferringTouchMove(false);
        if (isTouchEnd)
            pageClient->doneDeferringTouchEnd(false);
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
        didReceiveEvent(nullptr, event.type(), false, std::nullopt);
        if (pageClient) {
            if (isTouchStart)
                pageClient->doneDeferringTouchStart(false);
            if (isTouchMove)
                pageClient->doneDeferringTouchMove(false);
            if (isTouchEnd)
                pageClient->doneDeferringTouchEnd(false);
        }
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

void WebPageProxy::didBeginTouchPoint(FloatPoint locationInRootView)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidBeginTouchPoint(locationInRootView));
}

void WebPageProxy::sendUnpreventableTouchEvent(WebCore::FrameIdentifier frameID, const WebTouchEvent& event)
{
    if (event.type() == WebEventType::TouchEnd && protectedPreferences()->verifyWindowOpenUserGestureFromUIProcess())
        processContainingFrame(frameID)->recordUserGestureAuthorizationToken(frameID, webPageIDInMainFrameProcess(), event.authorizationToken());

    if (event.isActivationTriggeringEvent())
        internals().lastActivationTimestamp = MonotonicTime::now();

    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::EventDispatcher::TouchEvent(webPageIDInProcess(processContainingFrame(frameID)), frameID, event), [protectedThis = Ref { *this }] (bool, std::optional<RemoteWebTouchEvent> remoteWebTouchEvent) mutable {
        if (!remoteWebTouchEvent)
            return;
        protectedThis->sendUnpreventableTouchEvent(remoteWebTouchEvent->targetFrameID, remoteWebTouchEvent->transformedEvent);
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
void WebPageProxy::touchEventHandlingCompleted(IPC::Connection* connection, std::optional<WebEventType> eventType, bool handled)
{
    MESSAGE_CHECK_BASE(!internals().touchEventQueue.isEmpty(), connection);
    auto queuedEvents = internals().touchEventQueue.takeFirst();
    if (eventType)
        MESSAGE_CHECK_BASE(*eventType == queuedEvents.forwardedEvent.type(), connection);

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    pageClient->doneWithTouchEvent(queuedEvents.forwardedEvent, handled);
    for (size_t i = 0; i < queuedEvents.deferredTouchEvents.size(); ++i) {
        bool isEventHandled = false;
        pageClient->doneWithTouchEvent(queuedEvents.deferredTouchEvents.at(i), isEventHandled);
    }
}

void WebPageProxy::handleTouchEvent(IPC::Connection* connection, const NativeWebTouchEvent& event)
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
        protectedLegacyMainFrameProcess()->startResponsivenessTimer();
        sendWithAsyncReply(Messages::WebPage::TouchEvent(event), [this, protectedThis = Ref { *this }] (IPC::Connection* connection, std::optional<WebEventType> eventType, bool handled) {
            if (!m_pageClient)
                return;
            if (!eventType) {
                touchEventHandlingCompleted(connection, eventType, handled);
                return;
            }
            didReceiveEvent(connection, *eventType, handled, std::nullopt);
        });
    } else {
        if (internals().touchEventQueue.isEmpty()) {
            bool isEventHandled = false;
            if (RefPtr pageClient = this->pageClient())
                pageClient->doneWithTouchEvent(event, isEventHandled);
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
    protectedWebsiteDataStore()->protectedNetworkProcess()->send(Messages::NetworkProcess::DisableServiceWorkerEntitlement(), 0);
#endif
}

void WebPageProxy::clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS) && !PLATFORM(MACCATALYST)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    sendWithAsyncReply(Messages::WebPage::ClearServiceWorkerEntitlementOverride(), [callbackAggregator] { });
    protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearServiceWorkerEntitlementOverride(), [callbackAggregator] { });
#else
    completionHandler();
#endif
}

static std::optional<std::pair<Ref<API::WebsitePolicies>, Ref<WebProcessProxy>>> websitePoliciesAndProcess(API::WebsitePolicies* policies, const Ref<WebProcessProxy>& process)
{
    if (!policies)
        return std::nullopt;
    return { { *policies, process } };
}

#if ENABLE(WEB_ARCHIVE)
Expected<WebPageProxy::DataStoreUpdateResult, WebCore::ResourceError> WebPageProxy::updateDataStoreForWebArchiveLoad(WebFrameProxy& frame, PolicyAction policyAction, NavigationType navigationType, API::Navigation& navigation)
{
    RefPtr<WebsiteDataStore> updatedWebsiteDataStore;
    LoadedWebArchive loadedWebArchive { LoadedWebArchive::No };
    if (!protectedPreferences()->loadWebArchiveWithEphemeralStorageEnabled())
        return DataStoreUpdateResult { updatedWebsiteDataStore, loadedWebArchive };

    if (policyAction != PolicyAction::Use || navigationType == NavigationType::Reload)
        return DataStoreUpdateResult { updatedWebsiteDataStore, loadedWebArchive };

    // Only update data store of the page in main frame navigation.
    if (!frame.isMainFrame())
        return DataStoreUpdateResult { updatedWebsiteDataStore, loadedWebArchive };

    bool isSubstituteDataWebArchive = navigation.substituteData() && MIMETypeRegistry::isWebArchiveMIMEType(navigation.substituteData()->MIMEType);
    auto requestURL = isSubstituteDataWebArchive ? URL { navigation.substituteData()->baseURL } : navigation.currentRequest().url();
    bool isLoadingWebArchive = isSubstituteDataWebArchive || (requestURL.protocolIsFile() && requestURL.fileSystemPath().endsWith(".webarchive"_s));
    loadedWebArchive = isLoadingWebArchive ? LoadedWebArchive::Yes : LoadedWebArchive::No;
    if (!isLoadingWebArchive) {
        // If data store is changed for archive load, switch back to original data store.
        if (m_replacedDataStoreForWebArchiveLoad) {
            m_configuration->protectedProcessPool()->pageEndUsingWebsiteDataStore(*this, protectedWebsiteDataStore());
            if (auto replacedDataStoreForWebArchiveLoad = std::exchange(m_replacedDataStoreForWebArchiveLoad, nullptr))
                m_websiteDataStore = *replacedDataStoreForWebArchiveLoad;
            updatedWebsiteDataStore = m_websiteDataStore.ptr();
        }
        return DataStoreUpdateResult { updatedWebsiteDataStore, loadedWebArchive };
    }

#if PLATFORM(MAC)
    bool clientDoesNotHaveAccessToArchiveFile = !isSubstituteDataWebArchive && isQuarantinedAndNotUserApproved(requestURL.fileSystemPath());
    if (clientDoesNotHaveAccessToArchiveFile) {
        auto error = WebKit::cancelledError(URL { requestURL });
        error.setType(WebCore::ResourceError::Type::Cancellation);
        return makeUnexpected(error);
    }
#endif

    if (navigation.targetItem() && navigation.targetItem()->dataStoreForWebArchive()) {
        updatedWebsiteDataStore = navigation.targetItem()->dataStoreForWebArchive();
        return DataStoreUpdateResult { updatedWebsiteDataStore, loadedWebArchive };
    }

    m_websiteDataStore = WebsiteDataStore::createNonPersistent();
    updatedWebsiteDataStore = m_websiteDataStore.ptr();
    m_configuration->protectedProcessPool()->pageBeginUsingWebsiteDataStore(*this, protectedWebsiteDataStore());
    return DataStoreUpdateResult { updatedWebsiteDataStore, loadedWebArchive };
}
#endif

void WebPageProxy::receivedNavigationActionPolicyDecision(WebProcessProxy& processInitiatingNavigation, PolicyAction policyAction, API::Navigation& navigation, Ref<API::NavigationAction>&& navigationAction, ProcessSwapRequestedByClient processSwapRequestedByClient, WebFrameProxy& frame, const FrameInfoData& frameInfo, WasNavigationIntercepted wasNavigationIntercepted, std::optional<PolicyDecisionConsoleMessage>&& message, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "receivedNavigationActionPolicyDecision: frameID=%" PRIu64 ", isMainFrame=%d, navigationID=%" PRIu64 ", policyAction=%" PUBLIC_LOG_STRING, frame.frameID().toUInt64(), frame.isMainFrame(), navigation.navigationID().toUInt64(), toString(policyAction).characters());

    Ref websiteDataStore = m_websiteDataStore;
    RefPtr policies = navigation.websitePolicies();
    if (policies) {
        if (policies->websiteDataStore() && policies->websiteDataStore() != websiteDataStore.ptr()) {
            websiteDataStore = *policies->websiteDataStore();
            processSwapRequestedByClient = ProcessSwapRequestedByClient::Yes;
        }
    }

    if (!navigation.userContentExtensionsEnabled()) {
        if (!navigation.websitePolicies())
            navigation.setWebsitePolicies(API::WebsitePolicies::create());
        navigation.protectedWebsitePolicies()->setContentExtensionEnablement({ ContentExtensionDefaultEnablement::Disabled, { } });
    }

    RefPtr websitePolicies = navigation.websitePolicies();
#if ENABLE(DEVICE_ORIENTATION)
    bool isPermissionSet = false;
    auto origin = SecurityOriginData::fromURL(navigation.currentRequest().url());
    Ref deviceOrientationAndMotionAccessController = websiteDataStore->deviceOrientationAndMotionAccessController();
    if (websitePolicies) {
        // Update cache with permission in navigation policy as this is the most recent decision.
        if (auto permission = websitePolicies->deviceOrientationAndMotionAccessState()) {
            deviceOrientationAndMotionAccessController->setCachedDeviceOrientationPermission(origin, *permission);
            isPermissionSet = true;
        }
    }
    if (!isPermissionSet) {
        if (!websitePolicies) {
            navigation.setWebsitePolicies(API::WebsitePolicies::create());
            websitePolicies = navigation.websitePolicies();
        }
        auto cachedPermission = deviceOrientationAndMotionAccessController->cachedDeviceOrientationPermission(origin);
        websitePolicies->setDeviceOrientationAndMotionAccessState(cachedPermission);
    }
#endif

    Ref preferences = m_preferences;

#if PLATFORM(COCOA)
    static const bool forceDownloadFromDownloadAttribute = false;
#else
    static const bool forceDownloadFromDownloadAttribute = true;
#endif
    if (policyAction == PolicyAction::Use && (forceDownloadFromDownloadAttribute && navigation.shouldPerformDownload()))
        policyAction = PolicyAction::Download;

    bool navigatingIFrameWithoutSiteIsolation = !frame.isMainFrame() && !preferences->siteIsolationEnabled();
    if (policyAction != PolicyAction::Use || navigatingIFrameWithoutSiteIsolation) {
        auto previousPendingNavigationID = pageLoadState().pendingAPIRequest().navigationID;
        receivedPolicyDecision(policyAction, &navigation, navigatingIFrameWithoutSiteIsolation ? websitePoliciesAndProcess(websitePolicies.get(), protectedLegacyMainFrameProcess()) : std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(message), WTFMove(completionHandler));
#if HAVE(APP_SSO)
        if (policyAction == PolicyAction::Ignore && navigation.navigationID() == previousPendingNavigationID && wasNavigationIntercepted == WasNavigationIntercepted::Yes) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "receivedNavigationActionPolicyDecision: Failing navigation because decision was intercepted and policy action is Ignore.");
            auto error = WebKit::cancelledError(URL { navigation.currentRequest().url() });
            error.setType(WebCore::ResourceError::Type::Cancellation);
            m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, &navigation, navigation.currentRequest().url(), error, nullptr);
            return;
        }
#else
    UNUSED_PARAM(wasNavigationIntercepted);
    UNUSED_VARIABLE(previousPendingNavigationID);
#endif

        return;
    }

    RefPtr<WebsiteDataStore> replacedDataStoreForWebArchiveLoad;
    LoadedWebArchive loadedWebArchive { LoadedWebArchive::No };
#if ENABLE(WEB_ARCHIVE)
    // If websiteDataStore is specified by website policies, we should not update it.
    if (m_websiteDataStore.ptr() == websiteDataStore.ptr()) {
        auto result = updateDataStoreForWebArchiveLoad(frame, policyAction, navigationAction->navigationType(), navigation);
        if (!result) {
            m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, &navigation, navigation.currentRequest().url(), result.error(), nullptr);
            receivedPolicyDecision(PolicyAction::Ignore, &navigation, std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(message), WTFMove(completionHandler));
            return;
        }
        auto [updatedWebsiteDataStore, updatedLoadedWebArchive] = result.value();
        if (updatedWebsiteDataStore && updatedWebsiteDataStore.get() != websiteDataStore.ptr()) {
            loadedWebArchive = updatedLoadedWebArchive;
            if (loadedWebArchive == LoadedWebArchive::Yes)
                replacedDataStoreForWebArchiveLoad = websiteDataStore.ptr();
            websiteDataStore = *updatedWebsiteDataStore;
            processSwapRequestedByClient = ProcessSwapRequestedByClient::Yes;
        }
    }
#endif

    URL sourceURL { pageLoadState().url() };
    if (auto* provisionalPage = provisionalPageProxy()) {
        if (provisionalPage->navigationID() == navigation.navigationID())
            sourceURL = provisionalPage->provisionalURL();
    }

    m_isLockdownModeExplicitlySet = (websitePolicies && websitePolicies->isLockdownModeExplicitlySet()) || m_configuration->isLockdownModeExplicitlySet();
    auto lockdownMode = (websitePolicies ? websitePolicies->lockdownModeEnabled() : shouldEnableLockdownMode()) ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled;
    auto enhancedSecurity = ((websitePolicies && websitePolicies->enhancedSecurityEnabled()) || protectedPreferences()->forceEnhancedSecurity()) ? WebProcessProxy::EnhancedSecurity::Enabled : WebProcessProxy::EnhancedSecurity::Disabled;

    Ref browsingContextGroup = m_browsingContextGroup;
    bool usesSameWebsiteDataStore = websiteDataStore.ptr() == &this->websiteDataStore();
    bool mainFrameSiteChanges = !m_mainFrame || Site { m_mainFrame->url() } != Site { navigation.currentRequest().url() };
    if (RefPtr targetBackForwardItem = navigation.targetItem(); targetBackForwardItem && targetBackForwardItem->browsingContextGroup() && usesSameWebsiteDataStore)
        browsingContextGroup = *targetBackForwardItem->browsingContextGroup();
    else if (processSwapRequestedByClient == ProcessSwapRequestedByClient::Yes || !usesSameWebsiteDataStore || (navigation.isRequestFromClientOrUserInput() && !navigation.isFromLoadData() && mainFrameSiteChanges))
        browsingContextGroup = BrowsingContextGroup::create();

    auto continueWithProcessForNavigation = [
        this,
        protectedThis = Ref { *this },
        policyAction,
        browsingContextGroup = browsingContextGroup.copyRef(),
        navigation = Ref { navigation },
        navigationAction = WTFMove(navigationAction),
        completionHandler = WTFMove(completionHandler),
        processSwapRequestedByClient,
        frame = Ref { frame },
        processInitiatingNavigation = Ref { processInitiatingNavigation },
        message = WTFMove(message),
        loadedWebArchive,
        replacedDataStoreForWebArchiveLoad
    ] (Ref<WebProcessProxy>&& processNavigatingTo, SuspendedPageProxy* destinationSuspendedPage, ASCIILiteral reason) mutable {
        ASSERT(!processNavigatingTo->isInProcessCache());
        // If the navigation has been destroyed or the frame has been replaced by PSON, then no need to proceed.
        auto currentMainFrameID = m_mainFrame ? std::optional<WebCore::FrameIdentifier> { m_mainFrame->frameID() } : std::nullopt;
        if (isClosed()
            || !m_navigationState->hasNavigation(navigation->navigationID())
            || (navigationAction->mainFrameIDBeforeNavigationActionDecision() && navigationAction->mainFrameIDBeforeNavigationActionDecision() != currentMainFrameID)) {
            receivedPolicyDecision(policyAction, navigation.ptr(), std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(message), WTFMove(completionHandler));
            return;
        }

        RefPtr pageClientProtector = pageClient();
        Ref processNavigatingFrom = [&] {
            RefPtr provisionalPage = m_provisionalPage;
            return Ref { protectedPreferences()->siteIsolationEnabled() && frame->isMainFrame() && provisionalPage ? provisionalPage->process() : frame->process() };
        }();

        const bool navigationChangesFrameProcess = processNavigatingTo->coreProcessIdentifier() != processNavigatingFrom->coreProcessIdentifier();
        const bool loadContinuingInNonInitiatingProcess = processInitiatingNavigation->coreProcessIdentifier() != processNavigatingTo->coreProcessIdentifier();
        if (navigationChangesFrameProcess) {
            policyAction = PolicyAction::LoadWillContinueInAnotherProcess;
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "decidePolicyForNavigationAction, swapping process %i with process %i for navigation, reason=%" PUBLIC_LOG_STRING, legacyMainFrameProcessID(), processNavigatingTo->processID(), reason.characters());
            LOG(ProcessSwapping, "(ProcessSwapping) Switching from process %i to new process (%i) for navigation %" PRIu64 " '%s'", legacyMainFrameProcessID(), processNavigatingTo->processID(), navigation->navigationID().toUInt64(), navigation->loggingString().utf8().data());
        } else {
            WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "decidePolicyForNavigationAction: keep using process %i for navigation, reason=%" PUBLIC_LOG_STRING, legacyMainFrameProcessID(), reason.characters());
            frame->takeProvisionalFrame();
        }

        if (navigationChangesFrameProcess) {
            // Make sure the process to be used for the navigation does not get shutDown now due to destroying SuspendedPageProxy or ProvisionalPageProxy objects.
            auto preventNavigationProcessShutdown = processNavigatingTo->shutdownPreventingScope();

            Ref protectedBackForwardCache = backForwardCache();

            ASSERT(!destinationSuspendedPage || navigation->targetItem());
            RefPtr suspendedPage = destinationSuspendedPage ? RefPtr { protectedBackForwardCache->takeSuspendedPage(*navigation->protectedTargetItem()) } : nullptr;

            // It is difficult to get history right if we have several WebPage objects inside a single WebProcess for the same WebPageProxy. As a result, if we make sure to
            // clear any SuspendedPageProxy for the current page that are backed by the destination process before we proceed with the navigation. This makes sure the WebPage
            // we are about to create in the destination process will be the only one associated with this WebPageProxy.
            if (!destinationSuspendedPage)
                protectedBackForwardCache->removeEntriesForPageAndProcess(*this, processNavigatingTo);

            ASSERT(suspendedPage.get() == destinationSuspendedPage);
            if (suspendedPage && suspendedPage->pageIsClosedOrClosing())
                suspendedPage = nullptr;

            auto isPerformingHTTPFallback = navigationAction->data().isPerformingHTTPFallback ? IsPerformingHTTPFallback::Yes : IsPerformingHTTPFallback::No;
            receivedPolicyDecision(policyAction, navigation.ptr(), std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::Yes, std::nullopt, WTFMove(message), WTFMove(completionHandler));
            continueNavigationInNewProcess(navigation, frame.get(), WTFMove(suspendedPage), browsingContextGroup, WTFMove(processNavigatingTo), processSwapRequestedByClient, ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision, std::nullopt, loadedWebArchive, isPerformingHTTPFallback, WebCore::ProcessSwapDisposition::None, replacedDataStoreForWebArchiveLoad.get());
            return;
        }

        if (loadContinuingInNonInitiatingProcess) {
            // FIXME: Add more parameters as appropriate. <rdar://116200985>
            LoadParameters loadParameters;
            loadParameters.effectiveSandboxFlags = frame->effectiveSandboxFlags();
            loadParameters.effectiveReferrerPolicy = frame->effectiveReferrerPolicy();
            loadParameters.request = navigation->currentRequest();
            loadParameters.shouldTreatAsContinuingLoad = navigation->currentRequestIsRedirect() ? ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted : ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;
            loadParameters.frameIdentifier = frame->frameID();
            loadParameters.isRequestFromClientOrUserInput = navigationAction->data().isRequestFromClientOrUserInput;
            loadParameters.navigationID = navigation->navigationID();
            loadParameters.ownerPermissionsPolicy = navigation->ownerPermissionsPolicy();
            loadParameters.isPerformingHTTPFallback = navigationAction->data().isPerformingHTTPFallback;
            loadParameters.isHandledByAboutSchemeHandler = m_aboutSchemeHandler->canHandleURL(loadParameters.request.url());
            if (auto& action = navigation->lastNavigationAction())
                loadParameters.requester = action->requester;

            processNavigatingTo->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)), webPageIDInProcess(processNavigatingTo));
        }

        RefPtr item = navigation->reloadItem() ? navigation->reloadItem() : navigation->targetItem();
        std::optional<SandboxExtension::Handle> optionalHandle;
        if (policyAction == PolicyAction::Use && item) {
            URL fullURL { item->url() };
            if (fullURL.protocolIsFile()) {
                maybeInitializeSandboxExtensionHandle(processNavigatingTo.get(), fullURL, item->resourceDirectoryURL(), true, [
                    weakThis = WeakPtr { *this },
                    navigation = WTFMove(navigation),
                    navigationAction = WTFMove(navigationAction),
                    message = WTFMove(message),
                    completionHandler = WTFMove(completionHandler),
                    processNavigatingTo,
                    policyAction
                ] (std::optional<SandboxExtension::Handle>&& sandboxExtension) mutable {
                    RefPtr protectedThis = weakThis.get();
                    if (!protectedThis)
                        return;
                    protectedThis->receivedPolicyDecision(policyAction, navigation.ptr(), websitePoliciesAndProcess(navigation->websitePolicies(), processNavigatingTo), WTFMove(navigationAction), WillContinueLoadInNewProcess::No, WTFMove(sandboxExtension), WTFMove(message), WTFMove(completionHandler));
                });
                return;
            }
        }

        receivedPolicyDecision(policyAction, navigation.ptr(), websitePoliciesAndProcess(navigation->websitePolicies(), processNavigatingTo), WTFMove(navigationAction), WillContinueLoadInNewProcess::No, WTFMove(optionalHandle), WTFMove(message), WTFMove(completionHandler));
    };

    Site site { navigation.currentRequest().url() };
    Site mainFrameSite = frame.isMainFrame() ? site : Site { URL(protectedPageLoadState()->activeURL()) };
    browsingContextGroup->sharedProcessForSite(websiteDataStore, policies.get(), preferences, site, mainFrameSite, lockdownMode, enhancedSecurity, Ref { m_configuration }, frame.isMainFrame() ? IsMainFrame::Yes : IsMainFrame::No, [
        this,
        protectedThis = Ref { *this },
        frame = Ref { frame },
        navigation = Ref { navigation },
        browsingContextGroup = browsingContextGroup.copyRef(),
        site,
        mainFrameSite,
        sourceURL,
        processSwapRequestedByClient,
        lockdownMode,
        enhancedSecurity,
        loadedWebArchive,
        frameInfo = FrameInfoData { frameInfo },
        websiteDataStore = websiteDataStore.copyRef(),
        continueWithProcessForNavigation = WTFMove(continueWithProcessForNavigation)
    ](FrameProcess* sharedProcess) mutable {
        if (sharedProcess) {
            navigation->setPendingSharedProcess(*sharedProcess);
            ASSERT(!sharedProcess->process().isInProcessCache());
            if (frame->isMainFrame()) {
                protectedWebsiteDataStore()->protectedNetworkProcess()->addAllowedFirstPartyForCookies(sharedProcess->process(), site.domain(), LoadedWebArchive::No, [process = Ref { sharedProcess->process() }, continueWithProcessForNavigation = WTFMove(continueWithProcessForNavigation)] mutable {
                    continueWithProcessForNavigation(WTFMove(process), nullptr, "Uses shared Web process"_s);
                });
            } else
                continueWithProcessForNavigation(sharedProcess->process(), nullptr, "Uses shared Web process"_s);
            return;
        }
        m_configuration->protectedProcessPool()->processForNavigation(*this, frame, navigation, sourceURL, browsingContextGroup, WebProcessPool::IsSharedProcess::No, mainFrameSite, processSwapRequestedByClient, lockdownMode, enhancedSecurity, loadedWebArchive, frameInfo, WTFMove(websiteDataStore), WTFMove(continueWithProcessForNavigation));
    });
}

Ref<WebPageProxy> WebPageProxy::downloadOriginatingPage(const API::Navigation* navigation)
{
    if (!navigation)
        return *this;
    auto& frameInfo = navigation->originatingFrameInfo();
    if (!frameInfo)
        return *this;
    return navigationOriginatingPage(*frameInfo);
}

Ref<WebPageProxy> WebPageProxy::navigationOriginatingPage(const FrameInfoData& frameInfo)
{
    RefPtr webFrame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!webFrame)
        return *this;
    RefPtr page = webFrame->page();
    if (!page)
        return *this;
    return page.releaseNonNull();
}

void WebPageProxy::receivedPolicyDecision(PolicyAction action, API::Navigation* navigation, std::optional<std::pair<Ref<API::WebsitePolicies>, Ref<WebProcessProxy>>>&& websitePoliciesAndProcess, Ref<API::NavigationAction>&& navigationAction, WillContinueLoadInNewProcess willContinueLoadInNewProcess, std::optional<SandboxExtension::Handle> sandboxExtensionHandle, std::optional<PolicyDecisionConsoleMessage>&& consoleMessage, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!hasRunningProcess())
        return completionHandler(PolicyDecision { });

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    if (action == PolicyAction::Ignore && willContinueLoadInNewProcess == WillContinueLoadInNewProcess::No && navigation && navigation->navigationID() == pageLoadState->pendingAPIRequest().navigationID)
        pageLoadState->clearPendingAPIRequest(transaction);

    std::optional<DownloadID> downloadID;
    if (action == PolicyAction::Download) {
        // Create a download proxy.
        RefPtr<DownloadProxy> download;

        if (navigation && (navigation->targetItem() || navigation->isRequestFromClientOrUserInput()))
            download = m_configuration->protectedProcessPool()->createDownloadProxy(m_websiteDataStore, navigationAction->request(), downloadOriginatingPage(navigation).ptr(), std::nullopt);
        else
            download = m_configuration->protectedProcessPool()->createDownloadProxy(m_websiteDataStore, navigationAction->request(), downloadOriginatingPage(navigation).ptr(), navigation ? navigation->originatingFrameInfo() : std::optional(navigationAction->data().originatingFrameInfoData));

        download->setDidStartCallback([weakThis = WeakPtr { *this }, navigationAction = WTFMove(navigationAction)] (auto* downloadProxy) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !downloadProxy)
                return;
            protectedThis->m_navigationClient->navigationActionDidBecomeDownload(*protectedThis, navigationAction, *downloadProxy);
        });
        if (navigation) {
            download->setWasUserInitiated(navigation->wasUserInitiated());
            download->setRedirectChain(navigation->takeRedirectChain());
        }

        downloadID = download->downloadID();
    }

    std::optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePoliciesAndProcess)
        websitePoliciesData = Ref { websitePoliciesAndProcess->first }->dataForProcess(websitePoliciesAndProcess->second);
    auto isSafeBrowsingCheckOngoing = SafeBrowsingCheckOngoing::No;
    if (navigation)
        isSafeBrowsingCheckOngoing = navigation->safeBrowsingCheckOngoing() ? SafeBrowsingCheckOngoing::Yes : SafeBrowsingCheckOngoing::No;

    completionHandler(PolicyDecision { isNavigatingToAppBoundDomain(), action, navigation ? std::optional { navigation->navigationID() } : std::nullopt, downloadID, WTFMove(websitePoliciesData), WTFMove(sandboxExtensionHandle), WTFMove(consoleMessage), isSafeBrowsingCheckOngoing });
}

void WebPageProxy::receivedNavigationResponsePolicyDecision(WebCore::PolicyAction action, API::Navigation* navigation, const WebCore::ResourceRequest& request, Ref<API::NavigationResponse>&& navigationResponse, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!hasRunningProcess())
        return completionHandler(PolicyDecision { });

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    if (action == PolicyAction::Ignore
        && navigation
        && navigation->navigationID() == pageLoadState->pendingAPIRequest().navigationID)
        pageLoadState->clearPendingAPIRequest(transaction);

    std::optional<DownloadID> downloadID;
    if (action == PolicyAction::Download) {
        RefPtr<DownloadProxy> download;

        if (navigation && (navigation->targetItem() || navigation->isRequestFromClientOrUserInput()))
            download = m_configuration->protectedProcessPool()->createDownloadProxy(m_websiteDataStore, request, downloadOriginatingPage(navigation).ptr(), std::nullopt);
        else
            download = m_configuration->protectedProcessPool()->createDownloadProxy(m_websiteDataStore, request, downloadOriginatingPage(navigation).ptr(), navigation ? navigation->originatingFrameInfo() : std::nullopt);

        download->setDidStartCallback([weakThis = WeakPtr { *this }, navigationResponse = WTFMove(navigationResponse)] (auto* downloadProxy) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !downloadProxy)
                return;
            if (!navigationResponse->downloadAttribute().isNull())
                downloadProxy->setSuggestedFilename(navigationResponse->downloadAttribute());
            protectedThis->m_navigationClient->navigationResponseDidBecomeDownload(*protectedThis, navigationResponse, *downloadProxy);
        });
        if (navigation) {
            download->setWasUserInitiated(navigation->wasUserInitiated());
            download->setRedirectChain(navigation->takeRedirectChain());
        }

        downloadID = download->downloadID();
    }

    completionHandler(PolicyDecision { isNavigatingToAppBoundDomain(), action, navigation ? std::optional { navigation->navigationID() } : std::nullopt, downloadID, { }, { } });
}

void WebPageProxy::commitProvisionalPage(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, String&& proxyName, WebCore::ResourceResponseSource source, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    ASSERT(m_provisionalPage);
    RefPtr provisionalPage = std::exchange(m_provisionalPage, nullptr);
    WEBPAGEPROXY_RELEASE_LOG(Loading, "commitProvisionalPage: newPID=%i", provisionalPage->process().processID());

    RefPtr mainFrameInPreviousProcess = m_mainFrame;
    Ref preferences = m_preferences;
    if (mainFrameInPreviousProcess && preferences->siteIsolationEnabled())
        mainFrameInPreviousProcess->removeChildFrames();

    ASSERT(m_legacyMainFrameProcess.ptr() != &provisionalPage->process() || preferences->siteIsolationEnabled());

    auto shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::No;
#if ENABLE(TILED_CA_DRAWING_AREA)
    // On macOS, when not using UI-side compositing, we need to make sure we do not close the page in the previous process until we've
    // entered accelerated compositing for the new page or we will flash on navigation.
    if (protectedDrawingArea()->type() == DrawingAreaType::TiledCoreAnimation)
        shouldDelayClosingUntilFirstLayerFlush = ShouldDelayClosingUntilFirstLayerFlush::Yes;
#endif

    if (m_isLayerTreeFrozenDueToSwipeAnimation)
        send(Messages::WebPage::SwipeAnimationDidEnd());

    resetStateAfterProcessTermination(ProcessTerminationReason::NavigationSwap);

    removeAllMessageReceivers();
    RefPtr navigation = m_navigationState->navigation(provisionalPage->navigationID());
    bool didSuspendPreviousPage = navigation ? suspendCurrentPageIfPossible(*navigation, WTFMove(mainFrameInPreviousProcess), shouldDelayClosingUntilFirstLayerFlush) : false;
    protectedLegacyMainFrameProcess()->removeWebPage(*this, m_websiteDataStore.ptr() == provisionalPage->process().websiteDataStore() ? WebProcessProxy::EndsUsingDataStore::No : WebProcessProxy::EndsUsingDataStore::Yes);

    if (RefPtr mainFrameWebsitePolicies = provisionalPage->mainFrameWebsitePolicies())
        m_mainFrameWebsitePolicies = mainFrameWebsitePolicies->copy();

    // There is no way we'll be able to return to the page in the previous page so close it.
    if (!didSuspendPreviousPage && shouldClosePreviousPage(*provisionalPage))
        send(Messages::WebPage::Close());

    const auto oldWebPageID = m_webPageID;
    swapToProvisionalPage(provisionalPage.releaseNonNull());

    didCommitLoadForFrame(connection, frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(mimeType), frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, WTFMove(proxyName), source, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData);

    // FIXME: <rdar://121240770> This is a hack. There seems to be a bug in our interaction with WebPageInspectorController.
    if (!preferences->siteIsolationEnabled())
        m_inspectorController->didCommitProvisionalPage(oldWebPageID, m_webPageID);
}

bool WebPageProxy::shouldClosePreviousPage(const ProvisionalPageProxy& provisionalPage)
{
    if (!protectedPreferences()->siteIsolationEnabled())
        return true;
    RefPtr mainFrame = provisionalPage.mainFrame();
    if (!mainFrame)
        return true;
    return !mainFrame->opener();
}

void WebPageProxy::destroyProvisionalPage()
{
    m_provisionalPage = nullptr;
}

void WebPageProxy::continueNavigationInNewProcess(API::Navigation& navigation, WebFrameProxy& frame, RefPtr<SuspendedPageProxy>&& suspendedPage, BrowsingContextGroup& browsingContextGroup, Ref<WebProcessProxy>&& newProcess, ProcessSwapRequestedByClient processSwapRequestedByClient, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume, LoadedWebArchive loadedWebArchive, IsPerformingHTTPFallback isPerformingHTTPFallback, WebCore::ProcessSwapDisposition processSwapDisposition, WebsiteDataStore* replacedDataStoreForWebArchiveLoad)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "continueNavigationInNewProcess: newProcessPID=%i, hasSuspendedPage=%i", newProcess->processID(), !!suspendedPage);
    LOG(Loading, "Continuing navigation %" PRIu64 " '%s' in a new web process", navigation.navigationID().toUInt64(), navigation.loggingString().utf8().data());
    RELEASE_ASSERT(!newProcess->isInProcessCache());
    ASSERT(shouldTreatAsContinuingLoad != ShouldTreatAsContinuingLoad::No);
    navigation.setProcessID(newProcess->coreProcessIdentifier());

    if (navigation.currentRequest().url().protocolIsFile())
        newProcess->addPreviouslyApprovedFileURL(navigation.currentRequest().url());

    if (RefPtr provisionalPage = m_provisionalPage; provisionalPage && frame.isMainFrame()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "continueNavigationInNewProcess: There is already a pending provisional load, cancelling it (provisonalNavigationID=%" PRIu64 ", navigationID=%" PRIu64 ")", m_provisionalPage->navigationID().toUInt64(), navigation.navigationID().toUInt64());
        if (provisionalPage->navigationID() != navigation.navigationID())
            provisionalPage->cancel();
        m_provisionalPage = nullptr;
    }

    RefPtr websitePolicies = navigation.websitePolicies();
    bool isServerSideRedirect = shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision && navigation.currentRequestIsRedirect();
    bool isProcessSwappingOnNavigationResponse = shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted;
    Site navigationSite { navigation.currentRequest().url() };

    Ref preferences = m_preferences;
    if (preferences->siteIsolationEnabled() && (!frame.isMainFrame() || newProcess->coreProcessIdentifier() == frame.process().coreProcessIdentifier())) {
        // FIXME: Add more parameters as appropriate. <rdar://116200985>
        LoadParameters loadParameters;
        loadParameters.request = navigation.currentRequest();
        loadParameters.shouldTreatAsContinuingLoad = navigation.currentRequestIsRedirect() ? WebCore::ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted : WebCore::ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;
        loadParameters.frameIdentifier = frame.frameID();
        loadParameters.isRequestFromClientOrUserInput = navigation.isRequestFromClientOrUserInput();
        loadParameters.navigationID = navigation.navigationID();
        loadParameters.effectiveSandboxFlags = frame.effectiveSandboxFlags();
        loadParameters.effectiveReferrerPolicy = frame.effectiveReferrerPolicy();
        loadParameters.lockBackForwardList = !!navigation.backForwardFrameLoadType() ? LockBackForwardList::Yes : LockBackForwardList::No;
        loadParameters.ownerPermissionsPolicy = navigation.ownerPermissionsPolicy();
        loadParameters.isPerformingHTTPFallback = isPerformingHTTPFallback == IsPerformingHTTPFallback::Yes;
        loadParameters.isHandledByAboutSchemeHandler = m_aboutSchemeHandler->canHandleURL(loadParameters.request.url());
        if (auto& action = navigation.lastNavigationAction())
            loadParameters.requester = action->requester;

        if (navigation.isInitialFrameSrcLoad())
            frame.setIsPendingInitialHistoryItem(true);

        // about:blank frames should be in the same process as the frame which originated navigation
        std::optional<SecurityOriginData> originator = navigation.currentRequest().url().isAboutBlank() && navigation.originatingFrameInfo() ? std::make_optional(navigation.originatingFrameInfo()->securityOrigin) : std::nullopt;

        frame.prepareForProvisionalLoadInProcess(newProcess, navigation, browsingContextGroup, originator, [
            loadParameters = WTFMove(loadParameters),
            newProcess = newProcess.copyRef(),
            preventProcessShutdownScope = newProcess->shutdownPreventingScope()
        ] (PageIdentifier pageID) mutable {
            newProcess->send(Messages::WebPage::LoadRequest(WTFMove(loadParameters)), pageID);
        });
        return;
    }

    // FIXME: Assert the equality of data stores regardless of whether site isolation is enabled or not.
    ASSERT(!protectedPreferences()->siteIsolationEnabled() || newProcess->websiteDataStore() == &websiteDataStore());
    Ref frameProcess = browsingContextGroup.ensureProcessForSite(navigationSite, Site { mainFrame()->url() }, newProcess, preferences, loadedWebArchive, InjectBrowsingContextIntoProcess::No);
    // Make sure we destroy any existing ProvisionalPageProxy object *before* we construct a new one.
    // It is important from the previous provisional page to unregister itself before we register a
    // new one to avoid confusion.
    m_provisionalPage = nullptr;
    Ref provisionalPage = ProvisionalPageProxy::create(*this, WTFMove(frameProcess), browsingContextGroup, WTFMove(suspendedPage), navigation, isServerSideRedirect, navigation.currentRequest(), processSwapRequestedByClient, isProcessSwappingOnNavigationResponse, websitePolicies.get(), replacedDataStoreForWebArchiveLoad);
    m_provisionalPage = provisionalPage.copyRef();

    // FIXME: This should be a CompletionHandler, but http/tests/inspector/target/provisional-load-cancels-previous-load.html doesn't call it.
    Function<void()> continuation = [this, protectedThis = Ref { *this }, navigation = Ref { navigation }, shouldTreatAsContinuingLoad, websitePolicies = WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume, isPerformingHTTPFallback, processSwapDisposition]() mutable {
        RefPtr provisionalPage = m_provisionalPage;
        if (RefPtr item = navigation->targetItem()) {
            LOG(Loading, "WebPageProxy %p continueNavigationInNewProcess to back item URL %s", this, item->url().utf8().data());

            Ref pageLoadState = internals().pageLoadState;
            auto transaction = pageLoadState->transaction();
            pageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), item->url() });

            provisionalPage->goToBackForwardItem(navigation, *item, WTFMove(websitePolicies), shouldTreatAsContinuingLoad, existingNetworkResourceLoadIdentifierToResume, processSwapDisposition);
            return;
        }

        RefPtr currentItem = m_backForwardList->currentItem();
        if (currentItem && (navigation->lockBackForwardList() == LockBackForwardList::Yes || navigation->lockHistory() == LockHistory::Yes)) {
            // If WebCore is supposed to lock the history for this load, then the new process needs to know about the current history item so it can update
            // it instead of creating a new one.
            provisionalPage->send(Messages::WebPage::SetCurrentHistoryItemForReattach(currentItem->mainFrameState()));
        }

        // FIXME: Work out timing of responding with the last policy delegate, etc
        ASSERT(!navigation->currentRequest().isEmpty());
        ASSERT(!existingNetworkResourceLoadIdentifierToResume || !navigation->substituteData());
        if (auto& substituteData = navigation->substituteData())
            provisionalPage->loadData(navigation, SharedBuffer::create(Vector(substituteData->content)), substituteData->MIMEType, substituteData->encoding, substituteData->baseURL, substituteData->userData.get(), shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain(), WTFMove(websitePolicies), substituteData->sessionHistoryVisibility);
        else
            provisionalPage->loadRequest(navigation, ResourceRequest { navigation->currentRequest() }, nullptr, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain(), WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume, isPerformingHTTPFallback);
    };

    Ref process = provisionalPage->process();

    if (provisionalPage->needsCookieAccessAddedInNetworkProcess()) {
        continuation = [
            networkProcess = Ref { protectedWebsiteDataStore()->networkProcess() },
            continuation = WTFMove(continuation),
            navigationDomain = RegistrableDomain(navigation.currentRequest().url()),
            process,
            preventProcessShutdownScope = process->shutdownPreventingScope(),
            loadedWebArchive
        ] () mutable {
            networkProcess->addAllowedFirstPartyForCookies(process, navigationDomain, loadedWebArchive, WTFMove(continuation));
        };
    }

    if (m_inspectorController->shouldPauseLoading(provisionalPage))
        m_inspectorController->setContinueLoadingCallback(provisionalPage, WTFMove(continuation));
    else
        continuation();
}

bool WebPageProxy::isPageOpenedByDOMShowingInitialEmptyDocument() const
{
    return openedByDOM() && !hasCommittedAnyProvisionalLoads();
}

void WebPageProxy::setUserAgent(String&& userAgent, IsCustomUserAgent isCustomUserAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = WTFMove(userAgent);

    // We update the service worker there at the moment to be sure we use values used by actual web pages.
    // FIXME: Refactor this when we have a better User-Agent story.
    m_configuration->protectedProcessPool()->updateRemoteWorkerUserAgent(m_userAgent);

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

void WebPageProxy::setCustomUserAgent(String&& customUserAgent)
{
    if (m_customUserAgent == customUserAgent)
        return;

    m_customUserAgent = WTFMove(customUserAgent);

    if (m_customUserAgent.isEmpty()) {
        setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
        return;
    }

    if (m_userAgent != m_customUserAgent)
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
    RefPtr mainFrame = m_mainFrame;
    return mainFrame && !mainFrame->isDisplayingStandaloneImageDocument();
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

        m_backForwardList->restoreFromState(WTFMove(sessionState.backForwardListState));
        // If the process is not launched yet, the session will be restored when sending the WebPageCreationParameters;
        if (hasRunningProcess())
            m_backForwardList->setItemsAsRestoredFromSession();

        Ref pageLoadState = internals().pageLoadState;
        auto transaction = pageLoadState->transaction();
        pageLoadState->setCanGoBack(transaction, m_backForwardList->backItem());
        pageLoadState->setCanGoForward(transaction, m_backForwardList->forwardItem());

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
            return loadRequest(WTFMove(sessionState.provisionalURL));

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
    RefPtr mainFrame = m_mainFrame;
    if (!mainFrame || mainFrame->isDisplayingStandaloneImageDocument())
        return false;

    return true;
}

void WebPageProxy::setTextZoomFactor(double zoomFactor)
{
    if (!m_mainFramePluginHandlesPageScaleGesture && m_textZoomFactor == zoomFactor)
        return;

    m_textZoomFactor = zoomFactor;

    if (!hasRunningProcess())
        return;

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::DidSetTextZoomFactor(m_textZoomFactor), pageID);
    });
}

void WebPageProxy::setPageZoomFactor(double zoomFactor)
{
    if (!m_mainFramePluginHandlesPageScaleGesture && m_pageZoomFactor == zoomFactor)
        return;

    closeOverlayedViews();

    m_pageZoomFactor = zoomFactor;

    if (!hasRunningProcess())
        return;

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::DidSetPageZoomFactor(m_pageZoomFactor), pageID);
    });
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    setPageZoomFactor(pageZoomFactor);
    setTextZoomFactor(textZoomFactor);
}

double WebPageProxy::pageZoomFactor() const
{
    // Zoom factor for non-PDF pages persists across page loads. We maintain a separate member variable for PDF
    // zoom which ensures that we don't use the PDF zoom for a normal page.
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginZoomFactor;
    return m_pageZoomFactor;
}

// // FIXME: <webkit.org/b/287508> Respect the plugin-specific min/max limits.
double WebPageProxy::minPageZoomFactor() const
{
    return m_pluginMinZoomFactor.value_or(ViewGestureController::defaultMinMagnification);
}

double WebPageProxy::maxPageZoomFactor() const
{
    return m_pluginMaxZoomFactor.value_or(ViewGestureController::defaultMaxMagnification);
}

double WebPageProxy::pageScaleFactor() const
{
    // PDF documents use zoom and scale factors to size themselves appropriately in the window. We store them
    // separately but decide which to return based on the main frame.
    if (m_mainFramePluginHandlesPageScaleGesture)
        return m_pluginScaleFactor;
    return m_pageScaleFactor;
}

void WebPageProxy::scalePage(double scale, const IntPoint& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(scale > 0);

    m_pageScaleFactor = scale;

    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    send(Messages::WebPage::DidScalePage(scale, origin));
    forEachWebContentProcess([&] (auto& process, auto pageID) {
        if (&process == &legacyMainFrameProcess())
            return;
        process.send(Messages::WebPage::DidScalePage(scale, IntPoint { }), pageID);
    });
    callAfterNextPresentationUpdate(WTFMove(completionHandler));
}

void WebPageProxy::scalePageInViewCoordinates(double scale, const IntPoint& centerInViewCoordinates)
{
    ASSERT(scale > 0);

    m_pageScaleFactor = scale;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidScalePageInViewCoordinates(scale, centerInViewCoordinates));
    forEachWebContentProcess([&] (auto& process, auto pageID) {
        if (&process == &legacyMainFrameProcess())
            return;
        process.send(Messages::WebPage::DidScalePage(scale, IntPoint { }), pageID);
    });
}

void WebPageProxy::scalePageRelativeToScrollPosition(double scale, const IntPoint& origin)
{
    ASSERT(scale > 0);

    m_pageScaleFactor = scale;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::DidScalePageRelativeToScrollPosition(scale, origin));
    forEachWebContentProcess([&] (auto& process, auto pageID) {
        if (&process == &legacyMainFrameProcess())
            return;
        process.send(Messages::WebPage::DidScalePage(scale, IntPoint { }), pageID);
    });
}

void WebPageProxy::scaleView(double scale)
{
    ASSERT(scale > 0);

    m_viewScaleFactor = scale;

    if (!hasRunningProcess())
        return;

    forEachWebContentProcess([&] (auto& process, auto pageID) {
        process.send(Messages::WebPage::DidScaleView(scale), pageID);
    });
}

void WebPageProxy::setIntrinsicDeviceScaleFactor(float scaleFactor)
{
    if (m_intrinsicDeviceScaleFactor == scaleFactor)
        return;

    m_intrinsicDeviceScaleFactor = scaleFactor;

    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->deviceScaleFactorDidChange([]() { });
}

void WebPageProxy::windowScreenDidChange(PlatformDisplayID displayID)
{
#if HAVE(DISPLAY_LINK)
    if (hasRunningProcess() && m_displayID && m_registeredForFullSpeedUpdates)
        protectedLegacyMainFrameProcess()->setDisplayLinkForDisplayWantsFullSpeedUpdates(*m_displayID, false);

    m_registeredForFullSpeedUpdates = false;
#endif

    m_displayID = displayID;
    RefPtr drawingArea = m_drawingArea;
    if (drawingArea)
        drawingArea->windowScreenDidChange(displayID);

    if (!hasRunningProcess())
        return;

    std::optional<FramesPerSecond> nominalFramesPerSecond;
    if (drawingArea)
        nominalFramesPerSecond = drawingArea->displayNominalFramesPerSecond();

    send(Messages::EventDispatcher::PageScreenDidChange(m_webPageID, displayID, nominalFramesPerSecond));
    send(Messages::WebPage::WindowScreenDidChange(displayID, nominalFramesPerSecond));
#if HAVE(DISPLAY_LINK)
    updateDisplayLinkFrequency();
#endif
}

float WebPageProxy::deviceScaleFactor() const
{
    return m_customDeviceScaleFactor.value_or(m_intrinsicDeviceScaleFactor);
}

void WebPageProxy::setCustomDeviceScaleFactor(float customScaleFactor, CompletionHandler<void()>&& completionHandler)
{
    if (m_customDeviceScaleFactor && m_customDeviceScaleFactor.value() == customScaleFactor) {
        completionHandler();
        return;
    }

    float oldScaleFactor = deviceScaleFactor();

    // A value of 0 clears the customScaleFactor.
    if (customScaleFactor)
        m_customDeviceScaleFactor = customScaleFactor;
    else
        m_customDeviceScaleFactor = std::nullopt;

    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    if (deviceScaleFactor() != oldScaleFactor)
        protectedDrawingArea()->deviceScaleFactorDidChange(WTFMove(completionHandler));
    else
        completionHandler();
}

void WebPageProxy::accessibilitySettingsDidChange()
{
    if (!hasRunningProcess())
        return;

#if PLATFORM(COCOA)
    // Also update screen properties which encodes invert colors.
    legacyMainFrameProcess().protectedProcessPool()->screenPropertiesChanged();
#endif
    send(Messages::WebPage::AccessibilitySettingsDidChange());
}

void WebPageProxy::enableAccessibilityForAllProcesses()
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::EnableAccessibility(), pageID);
    });
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
    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->didChangeViewExposedRect();
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

bool WebPageProxy::alwaysBounceVertical() const
{
    return internals().alwaysBounceVertical;
}

void WebPageProxy::setAlwaysBounceVertical(bool value)
{
    internals().alwaysBounceVertical = value;
}

bool WebPageProxy::alwaysBounceHorizontal() const
{
    return internals().alwaysBounceHorizontal;
}

void WebPageProxy::setAlwaysBounceHorizontal(bool value)
{
    internals().alwaysBounceHorizontal = value;
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

void WebPageProxy::pageScaleFactorDidChange(IPC::Connection& connection, double scaleFactor)
{
    MESSAGE_CHECK_BASE(scaleFactorIsValid(scaleFactor), connection);
    if (!legacyMainFrameProcess().hasConnection(connection))
        return;
    m_pageScaleFactor = scaleFactor;

    forEachWebContentProcess([&] (auto& process, auto pageID) {
        if (&process == &legacyMainFrameProcess())
            return;
        process.send(Messages::WebPage::DidScalePage(scaleFactor, { }), pageID);
    });
}

void WebPageProxy::viewScaleFactorDidChange(IPC::Connection& connection, double scaleFactor)
{
    MESSAGE_CHECK_BASE(scaleFactorIsValid(scaleFactor), connection);
    if (!legacyMainFrameProcess().hasConnection(connection))
        return;

    forEachWebContentProcess([&] (auto& process, auto pageID) {
        if (&process == &legacyMainFrameProcess())
            return;
        process.send(Messages::WebPage::DidScaleView(scaleFactor), pageID);
    });
}

void WebPageProxy::pluginScaleFactorDidChange(IPC::Connection& connection, double pluginScaleFactor)
{
    MESSAGE_CHECK_BASE(scaleFactorIsValid(pluginScaleFactor), connection);
    m_pluginScaleFactor = pluginScaleFactor;
}

void WebPageProxy::pluginZoomFactorDidChange(IPC::Connection& connection, double pluginZoomFactor)
{
    MESSAGE_CHECK_BASE(scaleFactorIsValid(pluginZoomFactor), connection);
    m_pluginZoomFactor = pluginZoomFactor;
}

void WebPageProxy::findStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    if (string.isEmpty()) {
        m_findMatchesClient->didFindStringMatches(this, string, Vector<Vector<WebCore::IntRect>> (), 0);
        return;
    }

    sendWithAsyncReply(Messages::WebPage::FindStringMatches(string, options, maxMatchCount), [this, protectedThis = Ref { *this }, string](Vector<Vector<WebCore::IntRect>> matches, int32_t firstIndexAfterSelection) {
        if (matches.isEmpty())
            m_findClient->didFailToFindString(this, string);
        else
            m_findMatchesClient->didFindStringMatches(this, string, matches, firstIndexAfterSelection);
    });
}

void WebPageProxy::findString(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(bool)>&& callbackFunction)
{
    auto sendAndAggregateFindStringMessage = [&]<typename M>(M&& message, CompletionHandler<void(bool)>&& completionHandler)
    {
        Ref callbackAggregator = FindStringCallbackAggregator::create(*this, string, options, maxMatchCount, WTFMove(completionHandler));
        forEachWebContentProcess([&](auto& webProcess, auto pageID) {
            webProcess.sendWithAsyncReply(std::forward<M>(message), [callbackAggregator](std::optional<FrameIdentifier> frameID, Vector<IntRect>&&, uint32_t matchCount, int32_t, bool didWrap) {
                callbackAggregator->foundString(frameID, matchCount, didWrap);
            }, pageID);
        });
    };

#if ENABLE(IMAGE_ANALYSIS)
    if (protectedPreferences()->imageAnalysisDuringFindInPageEnabled())
        sendAndAggregateFindStringMessage(Messages::WebPage::FindStringIncludingImages(string, options | FindOptions::DoNotSetSelection, maxMatchCount), [](bool) { });
#endif

    if (!protectedBrowsingContextGroup()->hasRemotePages(*this)) {
        auto completionHandler = [protectedThis = Ref { *this }, string, callbackFunction = WTFMove(callbackFunction)](std::optional<FrameIdentifier> frameID, Vector<IntRect>&& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrap) mutable {
            if (!frameID)
                protectedThis->findClient().didFailToFindString(protectedThis.ptr(), string);
            else
                protectedThis->findClient().didFindString(protectedThis.ptr(), string, matchRects, matchCount, matchIndex, didWrap);
            callbackFunction(frameID.has_value());
        };
        sendWithAsyncReply(Messages::WebPage::FindString(string, options, maxMatchCount), WTFMove(completionHandler));
        return;
    }

    sendAndAggregateFindStringMessage(Messages::WebPage::FindString(string, options | FindOptions::DoNotSetSelection, maxMatchCount), WTFMove(callbackFunction));
}

void WebPageProxy::findString(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    findString(string, options, maxMatchCount, [](bool) { });
}

void WebPageProxy::findTextRangesForStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction({ });
        return;
    }

    Ref aggregator = FindTextMatchCallbackAggregator::create(WTFMove(callbackFunction));

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::FindTextRangesForStringMatches(string, options, maxMatchCount), [aggregator](Vector<WebFoundTextRange>&& vector) {
            aggregator->foundMatches(WTFMove(vector));
        }, pageID);
    });
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

void WebPageProxy::addLayerForFindOverlay(CompletionHandler<void(std::optional<WebCore::PlatformLayerIdentifier>)>&& callbackFunction)
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

void WebPageProxy::countStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    if (!hasRunningProcess())
        return;

    class CountStringMatchesCallbackAggregator : public RefCounted<CountStringMatchesCallbackAggregator> {
    public:
        static Ref<CountStringMatchesCallbackAggregator> create(CompletionHandler<void(uint32_t)>&& completionHandler) { return adoptRef(*new CountStringMatchesCallbackAggregator(WTFMove(completionHandler))); }
        void didCountStringMatches(uint32_t matchCount) { m_matchCount += matchCount; }
        ~CountStringMatchesCallbackAggregator()
        {
            m_completionHandler(m_matchCount);
        }
    private:
        explicit CountStringMatchesCallbackAggregator(CompletionHandler<void(uint32_t)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }
        CompletionHandler<void(uint32_t)> m_completionHandler;
        uint32_t m_matchCount { 0 };
    };

    Ref callbackAggregator = CountStringMatchesCallbackAggregator::create([protectedThis = Ref { *this }, string](uint32_t matchCount) {
        protectedThis->m_findClient->didCountStringMatches(protectedThis.ptr(), string, matchCount);
    });

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::CountStringMatches(string, options, maxMatchCount), [callbackAggregator](uint32_t matchCount) {
            callbackAggregator->didCountStringMatches(matchCount);
        }, pageID);
    });
}

void WebPageProxy::replaceMatches(Vector<uint32_t>&& matchIndices, const String& replacementText, bool selectionOnly, CompletionHandler<void(uint64_t)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::ReplaceMatches(WTFMove(matchIndices), replacementText, selectionOnly), WTFMove(callback));
}

void WebPageProxy::launchInitialProcessIfNecessary()
{
    if (protectedLegacyMainFrameProcess()->isDummyProcessProxy())
        launchProcess(Site(aboutBlankURL()), ProcessLaunchReason::InitialProcess);
}

void WebPageProxy::runJavaScriptInMainFrame(RunJavaScriptParameters&& parameters, bool wantsResult, CompletionHandler<void(Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>)>&& callbackFunction)
{
    runJavaScriptInFrameInScriptWorld(WTFMove(parameters), std::nullopt, API::ContentWorld::pageContentWorldSingleton(), wantsResult, WTFMove(callbackFunction));
}

void WebPageProxy::runJavaScriptInFrameInScriptWorld(RunJavaScriptParameters&& parameters, std::optional<WebCore::FrameIdentifier> frameID, API::ContentWorld& world, bool wantsResult, CompletionHandler<void(Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>)>&& callbackFunction)
{
    static uintptr_t nextLoggingIdentifier;
    uintptr_t loggingIdentifier = nextLoggingIdentifier++;
#if PLATFORM(COCOA)
    WTFBeginSignpost(loggingIdentifier, EvaluateJavaScript, "evaluateJavaScript: frameID=%llu sourceURL=%" PRIVATE_LOG_STRING, frameID ? frameID->toUInt64() : 0, parameters.sourceURL.string().ascii().data());
#endif

    // For backward-compatibility support running script in a WebView which has not done any loads yets.
    launchInitialProcessIfNecessary();

    if (!hasRunningProcess())
        return callbackFunction(makeUnexpected(std::nullopt));

    RefPtr<ProcessThrottler::Activity> activity;
#if USE(RUNNINGBOARD)
    if (RefPtr pageClient = this->pageClient(); pageClient && pageClient->canTakeForegroundAssertions())
        activity = processContainingFrame(frameID)->protectedThrottler()->foregroundActivity("WebPageProxy::runJavaScriptInFrameInScriptWorld"_s);
#endif

    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::RunJavaScriptInFrameInScriptWorld(parameters, frameID, world.worldDataForProcess(processContainingFrame(frameID)), wantsResult), [activity = WTFMove(activity), loggingIdentifier, callbackFunction = WTFMove(callbackFunction)] (auto&& result) mutable {
#if PLATFORM(COCOA)
        WTFEndSignpost(loggingIdentifier, EvaluateJavaScript);
#else
        UNUSED_PARAM(loggingIdentifier);
#endif
        callbackFunction(WTFMove(result));
    });
}

void WebPageProxy::getRenderTreeExternalRepresentation(CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetRenderTreeExternalRepresentation(), WTFMove(callback));
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, CompletionHandler<void(const String&)>&& callback)
{
    if (!frame)
        return callback({ });
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

    sendWithAsyncReply(Messages::WebPage::GetContentsAsAttributedString(), [completionHandler = WTFMove(completionHandler), activity = legacyMainFrameProcess().protectedThrottler()->foregroundActivity("getContentsAsAttributedString"_s)](const WebCore::AttributedString& string) mutable {
        completionHandler(string);
    });
}
#endif

void WebPageProxy::getAllFrames(CompletionHandler<void(std::optional<FrameTreeNodeData>&&)>&& completionHandler)
{
    RefPtr mainFrame = m_mainFrame;
    if (!mainFrame)
        return completionHandler({ });
    mainFrame->getFrameTree(WTFMove(completionHandler));
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

    Ref aggregator = FrameTreeCallbackAggregator::create(WTFMove(completionHandler));
    forEachWebContentProcess([&] (auto& process, auto pageID) {
        process.sendWithAsyncReply(Messages::WebPage::GetFrameTree(), [aggregator] (std::optional<FrameTreeNodeData>&& data) {
            if (data)
                aggregator->addFrameTree(WTFMove(*data));
        }, pageID);
    });
}

#if RELEASE_LOG_DISABLED

void WebPageProxy::logFrameTree()
{
}

#else

static void logFrameTreeHelper(int indent, const FrameTreeNodeData& node)
{
    int spaces = (indent > 2) ? indent - 2 : 0;
    RELEASE_LOG(FrameTree, "%*s|- pid: %d | site: %" SENSITIVE_LOG_STRING " | url: %" SENSITIVE_LOG_STRING, spaces, "", node.info.processID, Site(node.info.securityOrigin).toString().ascii().data(), node.info.request.url().string().ascii().data());
    for (const auto& child : node.children)
        logFrameTreeHelper(indent + 2, child);
}

static void logFrameTreeRoot(uintptr_t pagePointer, const FrameTreeNodeData& root)
{
    RELEASE_LOG(FrameTree, "WebPageProxy %p | pid: %d | site: %" SENSITIVE_LOG_STRING " | url: %" SENSITIVE_LOG_STRING, reinterpret_cast<void*>(pagePointer), root.info.processID, Site(root.info.securityOrigin).toString().ascii().data(), root.info.request.url().string().ascii().data());
    for (const auto& child : root.children)
        logFrameTreeHelper(2, child);
}

void WebPageProxy::logFrameTree()
{
    getAllFrames([pagePointer = reinterpret_cast<uintptr_t>(this)](auto&& maybeFrameTree) {
        if (!maybeFrameTree)
            return;
        logFrameTreeRoot(pagePointer, *maybeFrameTree);
    });
}

#endif

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
        callback(API::Data::create(data->span()).ptr());
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

        sharedFileQueueSingleton().dispatch([directory = directory.isolatedCopy(), buffer, completionHandler = WTFMove(completionHandler)]() mutable {
            RefPtr archive = LegacyWebArchive::create(*buffer);
            auto result = archive->saveResourcesToDisk(directory);
            std::optional<WebCore::ArchiveError> error;
            if (!result)
                error = result.error();
            RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), error]() mutable {
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

void WebPageProxy::getAccessibilityTreeData(CompletionHandler<void(API::Data*)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::GetAccessibilityTreeData(), toAPIDataCallback(WTFMove(callback)));
}

void WebPageProxy::updateRenderingWithForcedRepaint(CompletionHandler<void()>&& callback)
{
    if (!hasRunningProcess())
        return callback();

    auto aggregator = CallbackAggregator::create([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callback();
        protectedThis->callAfterNextPresentationUpdate(WTFMove(callback));
    });
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::UpdateRenderingWithForcedRepaint(), [aggregator] { }, pageID);
    });
}

void WebPageProxy::preferencesDidChange()
{
    if (!hasRunningProcess())
        return;

    updateThrottleState();
    updateHiddenPageThrottlingAutoIncreases();

    if (RefPtr pageClient = this->pageClient())
        pageClient->preferencesDidChange();

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification
    // even if nothing changed in UI process, so that overrides get removed.

    // Preferences need to be updated during synchronous printing to make "print backgrounds" preference work when toggled from a print dialog checkbox.
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        std::optional<uint64_t> sharedPreferencesVersion;
        if (auto sharedPreferences = webProcess.updateSharedPreferences(preferencesStore()))
            sharedPreferencesVersion = sharedPreferences->version;

        if (m_isPerformingDOMPrintOperation) {
            ASSERT(!sharedPreferencesVersion);
            webProcess.send(Messages::WebPage::PreferencesDidChangeDuringDOMPrintOperation(preferencesStore(), std::nullopt), pageID,  IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
        } else
            webProcess.send(Messages::WebPage::PreferencesDidChange(preferencesStore(), sharedPreferencesVersion), pageID);
    });

    protectedWebsiteDataStore()->propagateSettingUpdates();
}

void WebPageProxy::didCreateSubframe(FrameIdentifier parentID, FrameIdentifier newFrameID, String&& frameName, SandboxFlags sandboxFlags, ReferrerPolicy referrerPolicy, ScrollbarMode scrollingMode)
{
    RefPtr parent = WebFrameProxy::webFrame(parentID);
    if (!parent)
        return;
    parent->didCreateSubframe(newFrameID, WTFMove(frameName), sandboxFlags, referrerPolicy, scrollingMode);
}

void WebPageProxy::didDestroyFrame(IPC::Connection& connection, FrameIdentifier frameID)
{
#if ENABLE(WEB_AUTHN)
    protectedWebsiteDataStore()->protectedAuthenticatorManager()->cancelRequest(webPageIDInMainFrameProcess(), frameID);
#endif
    if (RefPtr automationSession = m_configuration->processPool().automationSession())
        automationSession->didDestroyFrame(frameID);
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        frame->disconnect();

    bool didRemove = m_framesWithSubresourceLoadingForPageLoadTiming.remove(frameID);
#if PLATFORM(COCOA)
    if (didRemove)
        WTFEndSignpost(static_cast<uintptr_t>(frameID.toUInt64()), PLTSubresourceLoading, "didDestroyFrame(%llu)", frameID.toUInt64());
#endif

    if (didRemove && m_framesWithSubresourceLoadingForPageLoadTiming.isEmpty())
        generatePageLoadTimingSoon();

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || &webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::FrameWasRemovedInAnotherProcess(frameID), pageID);
    });
}

void WebPageProxy::disconnectFramesFromPage()
{
    if (RefPtr mainFrame = std::exchange(m_mainFrame, nullptr))
        mainFrame->webProcessWillShutDown();
}

double WebPageProxy::estimatedProgress() const
{
    return protectedPageLoadState()->estimatedProgress();
}

void WebPageProxy::didStartProgress()
{
    ASSERT(!m_isClosed);

    RefPtr protectedPageClient { pageClient() };
    Ref pageLoadState = internals().pageLoadState;

    auto transaction = pageLoadState->transaction();
    pageLoadState->didStartProgress(transaction);

    pageLoadState->commitChanges();
}

void WebPageProxy::didChangeProgress(double value)
{
    RefPtr protectedPageClient { pageClient() };
    Ref pageLoadState = internals().pageLoadState;

    auto transaction = pageLoadState->transaction();
    pageLoadState->didChangeProgress(transaction, value);

    pageLoadState->commitChanges();
}

void WebPageProxy::didFinishProgress()
{
    RefPtr protectedPageClient { pageClient() };
    Ref pageLoadState = internals().pageLoadState;

    auto transaction = pageLoadState->transaction();
    pageLoadState->didFinishProgress(transaction);

    pageLoadState->commitChanges();
}

void WebPageProxy::setNetworkRequestsInProgress(bool networkRequestsInProgress)
{
    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();
    pageLoadState->setNetworkRequestsInProgress(transaction, networkRequestsInProgress);
}

void WebPageProxy::startNetworkRequestsForPageLoadTiming(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame || (!frame->isMainFrame() && !frame->parentFrame()))
        return;

    m_generatePageLoadTimingTimer.stop();
    auto addResult = m_framesWithSubresourceLoadingForPageLoadTiming.add(frameID);
#if PLATFORM(COCOA)
    if (addResult.isNewEntry)
        WTFBeginSignpost(static_cast<uintptr_t>(frameID.toUInt64()), PLTSubresourceLoading, "startNetworkRequestsForPageLoadTiming(%llu)", frameID.toUInt64());
#endif
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void WebPageProxy::endNetworkRequestsForPageLoadTiming(WebCore::FrameIdentifier frameID, WallTime timestamp)
{
    bool didRemove = m_framesWithSubresourceLoadingForPageLoadTiming.remove(frameID);
#if PLATFORM(COCOA)
    if (didRemove)
        WTFEndSignpost(static_cast<uintptr_t>(frameID.toUInt64()), PLTSubresourceLoading, "endNetworkRequestsForPageLoadTiming(%llu)", frameID.toUInt64());
#else
    UNUSED_PARAM(didRemove);
#endif

    if (!m_pageLoadTiming)
        return;
    m_pageLoadTiming->updateEndOfNetworkRequests(timestamp);
    if (m_framesWithSubresourceLoadingForPageLoadTiming.isEmpty())
        generatePageLoadTimingSoon();
}

void WebPageProxy::generatePageLoadTimingSoon()
{
#if PLATFORM(COCOA)
    static bool shouldLog = CFPreferencesGetAppBooleanValue(CFSTR("WebKitDebugGeneratePageLoadTimingLogs"), kCFPreferencesCurrentApplication, nullptr);

    auto result = generatePageLoadTimingSoonImpl();
    if (shouldLog && result != GeneratePageLoadTimingResult::WaitForPageLoadTimingObject)
        WEBPAGEPROXY_RELEASE_LOG(Loading, "generatePageLoadTimingSoon: %d", static_cast<int>(result));
#else
    generatePageLoadTimingSoonImpl();
#endif
}

WebPageProxy::GeneratePageLoadTimingResult WebPageProxy::generatePageLoadTimingSoonImpl()
{
    using enum WebPageProxy::GeneratePageLoadTimingResult;

    m_generatePageLoadTimingTimer.stop();
    if (!m_pageLoadTiming)
        return WaitForPageLoadTimingObject;

    WallTime lastTimestamp;
    if (!m_pageLoadTiming->firstVisualLayout())
        return WaitForFirstVisualLayout;
    lastTimestamp = std::max(lastTimestamp, m_pageLoadTiming->firstVisualLayout());

    if (!m_pageLoadTiming->firstMeaningfulPaint())
        return WaitForFirstMeaningfulPaint;
    lastTimestamp = std::max(lastTimestamp, m_pageLoadTiming->firstMeaningfulPaint());

    if (!m_pageLoadTiming->documentFinishedLoading())
        return WaitForDOMContentLoaded;
    lastTimestamp = std::max(lastTimestamp, m_pageLoadTiming->documentFinishedLoading());

    if (!m_pageLoadTiming->finishedLoading())
        return WaitForLoadEvent;
    lastTimestamp = std::max(lastTimestamp, m_pageLoadTiming->finishedLoading());

    if (m_framesWithSubresourceLoadingForPageLoadTiming.size())
        return WaitForSubresourcesFinishedLoading;
    lastTimestamp = std::max(lastTimestamp, m_pageLoadTiming->allSubresourcesFinishedLoading());

    // Stop waiting for page load to end N ms after the last of the timestamps we care about, where
    // N is the quiescence interval passed in by the client.
    auto quiescenceInterval = m_configuration->processPool().pltResourceDelayInterval();
    Seconds interval = std::max(0_ms, lastTimestamp + quiescenceInterval - WallTime::now());
    m_generatePageLoadTimingTimer.startOneShot(interval);
    return WaitForQuiescence;
}

void WebPageProxy::didEndNetworkRequestsForPageLoadTimingTimerFired()
{
    if (!m_pageLoadTiming)
        return;

    // If the page had no subresources, just say that all subresources finished loading when the
    // load event fired.
    if (!m_pageLoadTiming->allSubresourcesFinishedLoading()) {
        auto loadEventEnd = m_pageLoadTiming->finishedLoading();
        m_pageLoadTiming->updateEndOfNetworkRequests(loadEventEnd);
    }

    didGeneratePageLoadTiming(*m_pageLoadTiming);
    m_pageLoadTiming = nullptr;
}

void WebPageProxy::updateScrollingMode(IPC::Connection& connection, WebCore::FrameIdentifier frameID, WebCore::ScrollbarMode scrollingMode)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID)) {
        Ref process = WebProcessProxy::fromConnection(connection);
        RefPtr parentFrame = frame->parentFrame();
        MESSAGE_CHECK(process, parentFrame && &parentFrame->process() == process.ptr());
        frame->updateScrollingMode(scrollingMode);
    }
}

void WebPageProxy::setFramePrinting(IPC::Connection& connection, WebCore::FrameIdentifier frameID, bool printing, const WebCore::FloatSize& pageSize, const WebCore::FloatSize& originalPageSize, float maximumShrinkRatio, WebCore::AdjustViewSize shouldAdjustViewSize)
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.hasConnection(connection))
            return;
        webProcess.send(Messages::WebPage::SetFramePrinting(frameID, printing, pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize), pageID);
    });
}

void WebPageProxy::resolveAccessibilityHitTestForTesting(WebCore::FrameIdentifier frameID, WebCore::IntPoint point, CompletionHandler<void(String)>&& callback)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::ResolveAccessibilityHitTestForTesting(frameID, point), WTFMove(callback));
}

void WebPageProxy::updateReferrerPolicy(IPC::Connection& connection, WebCore::FrameIdentifier frameID, WebCore::ReferrerPolicy referrerPolicy)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID)) {
        Ref process = WebProcessProxy::fromConnection(connection);
        RefPtr parentFrame = frame->parentFrame();
        MESSAGE_CHECK(process, parentFrame && &parentFrame->process() == process.ptr());
        frame->updateReferrerPolicy(referrerPolicy);
    }
}

void WebPageProxy::updateSandboxFlags(IPC::Connection& connection, WebCore::FrameIdentifier frameID, WebCore::SandboxFlags sandboxFlags)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID)) {
        Ref process = WebProcessProxy::fromConnection(connection);
        RefPtr parentFrame = frame->parentFrame();
        MESSAGE_CHECK(process, parentFrame && &parentFrame->process() == process.ptr());
        frame->updateSandboxFlags(sandboxFlags);
    }
}

void WebPageProxy::updateOpener(IPC::Connection& connection, WebCore::FrameIdentifier frameID, WebCore::FrameIdentifier newOpener)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        frame->updateOpener(newOpener);
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.hasConnection(connection))
            return;
        webProcess.send(Messages::WebPage::UpdateOpener(frameID, newOpener), pageID);
    });
}

void WebPageProxy::preconnectTo(ResourceRequest&& request)
{
    Ref websiteDataStore = m_websiteDataStore;
    if (!websiteDataStore->configuration().allowsServerPreconnect())
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
    websiteDataStore->protectedNetworkProcess()->preconnectTo(sessionID(), identifier(), webPageIDInMainFrameProcess(), WTFMove(request), storedCredentialsPolicy, isNavigatingToAppBoundDomain());
}

void WebPageProxy::setCanUseCredentialStorage(bool canUseCredentialStorage)
{
    m_canUseCredentialStorage = canUseCredentialStorage;
    send(Messages::WebPage::SetCanUseCredentialStorage(canUseCredentialStorage));
}

void WebPageProxy::didDestroyNavigation(IPC::Connection& connection, WebCore::NavigationIdentifier navigationID)
{
    didDestroyNavigationShared(WebProcessProxy::fromConnection(connection), navigationID);
}

void WebPageProxy::didDestroyNavigationShared(Ref<WebProcessProxy>&& process, WebCore::NavigationIdentifier navigationID)
{
    MESSAGE_CHECK(process, WebNavigationState::NavigationMap::isValidKey(navigationID));

    RefPtr protectedPageClient { pageClient() };

    m_navigationState->didDestroyNavigation(process->coreProcessIdentifier(), navigationID);
}

void WebPageProxy::didStartProvisionalLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, URL&& url, URL&& unreachableURL, const UserData& userData, WallTime timestamp)
{
    didStartProvisionalLoadForFrameShared(WebProcessProxy::fromConnection(connection), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData, timestamp);
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

RefPtr<ProvisionalPageProxy> WebPageProxy::protectedProvisionalPageProxy() const
{
    return m_provisionalPage;
}

void WebPageProxy::didStartProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, URL&& url, URL&& unreachableURL, const UserData& userData, WallTime timestamp)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    MESSAGE_CHECK_URL(process, url);
    MESSAGE_CHECK_URL(process, unreachableURL);

    if (frame->isMainFrame()) {
        m_pageLoadTiming = nullptr;
        m_pageLoadTimingPendingCommit = makeUnique<WebPageLoadTiming>(timestamp);
        m_generatePageLoadTimingTimer.stop();
    }

    // If a provisional load has since been started in another process, ignore this message.
    if (protectedPreferences()->siteIsolationEnabled()) {
        if (frame->provisionalLoadProcess().coreProcessIdentifier() != process->coreProcessIdentifier()) {
            // FIXME: The API test ProcessSwap.DoSameSiteNavigationAfterCrossSiteProvisionalLoadStarted
            // is probably not handled correctly with site isolation on.
            return;
        }
        if (frame->frameLoadState().state() == FrameLoadState::State::Provisional) {
            // FIXME: We need to actually notify m_navigationClient somehow.
            frame->frameLoadState().didFailProvisionalLoad();
        }
    }

    // If the page starts a new main frame provisional load, then cancel any pending one in a provisional process.
    if (frame->isMainFrame() && m_provisionalPage && m_provisionalPage->mainFrame() != frame) {
        protectedProvisionalPageProxy()->cancel();
        m_provisionalPage = nullptr;
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = m_navigationState->navigation(*navigationID);

    if (navigation && frame->isMainFrame() && navigation->currentRequest().url().isValid())
        MESSAGE_CHECK(process, navigation->currentRequest().url() == url);

    LOG(Loading, "WebPageProxy %" PRIu64 " in process pid %i didStartProvisionalLoadForFrame to frameID %" PRIu64 ", navigationID %" PRIu64 ", url %s", identifier().toUInt64(), process->processID(), frameID.toUInt64(), navigationID ? navigationID->toUInt64() : 0, url.string().utf8().data());
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didStartProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();
    pageLoadState->clearPendingAPIRequest(transaction);

    if (frame->isMainFrame()) {
        if (shouldPrewarmWebProcessOnProvisionalLoad())
            notifyProcessPoolToPrewarm();
        process->didStartProvisionalLoadForMainFrame(url);
        reportPageLoadResult(ResourceError { ResourceError::Type::Cancellation });
        internals().pageLoadStart = MonotonicTime::now();
        pageLoadState->didStartProvisionalLoad(transaction, url.string(), unreachableURL.string());
        protectedPageClient->didStartProvisionalLoadForMainFrame();
        closeOverlayedViews();
    }

    frame->setUnreachableURL(unreachableURL);
    frame->didStartProvisionalLoad(WTFMove(url));

#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->navigationStartedForFrame(*frame, navigationID);
#endif

    pageLoadState->commitChanges();
    if (m_loaderClient)
        m_loaderClient->didStartProvisionalLoadForFrame(*this, *frame, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didStartProvisionalNavigation(*this, request, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
        m_navigationClient->didStartProvisionalLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
    }

#if ENABLE(WEB_AUTHN)
    protectedWebsiteDataStore()->protectedAuthenticatorManager()->cancelRequest(m_webPageID, frameID);
#endif
}

void WebPageProxy::didExplicitOpenForFrame(IPC::Connection& connection, FrameIdentifier frameID, URL&& url, String&& mimeType)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    Ref process = WebProcessProxy::fromConnection(connection);
    if (!checkURLReceivedFromCurrentOrPreviousWebProcess(process, url)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring WebPageProxy::DidExplicitOpenForFrame() IPC from the WebContent process because the file URL is outside the sandbox");
        return;
    }

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    if (frame->isMainFrame())
        pageLoadState->didExplicitOpen(transaction, url.string());

    frame->didExplicitOpen(WTFMove(url), WTFMove(mimeType));

    m_hasCommittedAnyProvisionalLoads = true;
    process->didCommitProvisionalLoad();
    if (!url.protocolIsAbout())
        process->didCommitMeaningfulProvisionalLoad();

    pageLoadState->commitChanges();
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, ResourceRequest&& request, const UserData& userData)
{
    didReceiveServerRedirectForProvisionalLoadForFrameShared(WebProcessProxy::fromConnection(connection), frameID, navigationID, WTFMove(request), userData);
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, ResourceRequest&& request, const UserData& userData)
{
    LOG(Loading, "WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame to frameID %" PRIu64 ", navigationID %" PRIu64 ", url %s", frameID.toUInt64(), navigationID ? navigationID->toUInt64() : 0, request.url().string().utf8().data());

    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    MESSAGE_CHECK_URL(process, request.url());

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didReceiveServerRedirectForProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr navigation = navigationID ? m_navigationState->navigation(*navigationID) : nullptr;
    if (navigation) {
        navigation->appendRedirectionURL(request.url());
        navigation->resetRequestStart();
    }

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    URL requestURL = request.url();
    if (frame->isMainFrame()) {
        pageLoadState->didReceiveServerRedirectForProvisionalLoad(transaction, requestURL.string());
        // If the main frame in a provisional page is getting a server-side redirect, make sure the
        // committed page's provisional URL is kept up-to-date too.
        RefPtr mainFrame = m_mainFrame;
        if (frame != mainFrame && !mainFrame->frameLoadState().provisionalURL().isEmpty())
            mainFrame->didReceiveServerRedirectForProvisionalLoad(URL { requestURL });
    }
    frame->didReceiveServerRedirectForProvisionalLoad(WTFMove(requestURL));

    pageLoadState->commitChanges();
    if (m_loaderClient)
        m_loaderClient->didReceiveServerRedirectForProvisionalLoadForFrame(*this, *frame, frame->isMainFrame() ? navigation.get() : nullptr, process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else if (frame->isMainFrame())
        m_navigationClient->didReceiveServerRedirectForProvisionalNavigation(*this, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::willPerformClientRedirectForFrame(IPC::Connection& connection, FrameIdentifier frameID, String&& url, double delay, LockBackForwardList)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "willPerformClientRedirectForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame())
        m_navigationClient->willPerformClientRedirect(*this, WTFMove(url), delay);
}

void WebPageProxy::didCancelClientRedirectForFrame(IPC::Connection& connection, FrameIdentifier frameID)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didCancelClientRedirectForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->navigationAbortedForFrame(*frame, std::nullopt);
#endif

    if (frame->isMainFrame())
        m_navigationClient->didCancelClientRedirect(*this);
}

void WebPageProxy::didChangeProvisionalURLForFrame(IPC::Connection& connection, FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, URL&& url)
{
    didChangeProvisionalURLForFrameShared(WebProcessProxy::fromConnection(connection), frameID, navigationID, WTFMove(url));
}

void WebPageProxy::didChangeProvisionalURLForFrameShared(Ref<WebProcessProxy>&& process, FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier>, URL&& url)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    MESSAGE_CHECK(process, frame->frameLoadState().state() == FrameLoadState::State::Provisional);
    MESSAGE_CHECK_URL(process, url);

    Ref pageLoadState = internals().pageLoadState;
    auto transaction = pageLoadState->transaction();

    // Internally, we handle this the same way we handle a server redirect. There are no client callbacks
    // for this, but if this is the main frame, clients may observe a change to the page's URL.
    if (frame->isMainFrame())
        pageLoadState->didReceiveServerRedirectForProvisionalLoad(transaction, url.string());

    frame->didReceiveServerRedirectForProvisionalLoad(WTFMove(url));
}

void WebPageProxy::didFailProvisionalLoadForFrame(IPC::Connection& connection, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& provisionalURL, ResourceError&& error, WillContinueLoading willContinueLoading, const UserData& userData, WillInternallyHandleFailure willInternallyHandleFailure)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return;

    if (m_provisionalPage && frame->isMainFrame()) {
        // The load did not fail, it is merely happening in a new provisional process.
        return;
    }

    Ref process = WebProcessProxy::fromConnection(connection);
    if (protectedPreferences()->siteIsolationEnabled() && process != frame->process()) {
        RefPtr provisionalFrame = frame->provisionalFrame();
        if (!provisionalFrame || provisionalFrame->process() != process)
            return;
    }

    didFailProvisionalLoadForFrameShared(WTFMove(process), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(provisionalURL), WTFMove(error), willContinueLoading, userData, willInternallyHandleFailure);
}

void WebPageProxy::didFailProvisionalLoadForFrameShared(Ref<WebProcessProxy>&& process, WebFrameProxy& frame, FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& provisionalURL, ResourceError&& error, WillContinueLoading willContinueLoading, const UserData& userData, WillInternallyHandleFailure willInternallyHandleFailure)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " in web process pid %i didFailProvisionalLoadForFrame to provisionalURL %s", identifier().toUInt64(), process->processID(), provisionalURL.utf8().data());
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "didFailProvisionalLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d, domain=%s, code=%d, isMainFrame=%d, willInternallyHandleFailure=%d", frame.frameID().toUInt64(), frame.isMainFrame(), error.domain().utf8().data(), error.errorCode(), frame.isMainFrame(), willInternallyHandleFailure == WillInternallyHandleFailure::Yes);

    MESSAGE_CHECK_URL(process, provisionalURL);
    MESSAGE_CHECK_URL(process, error.failingURL());
#if PLATFORM(COCOA) && USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
    MESSAGE_CHECK(process, error.hasMatchingFailingURLKeys());
#endif

    RefPtr protectedPageClient { pageClient() };

    if (m_controlledByAutomation && willInternallyHandleFailure == WillInternallyHandleFailure::No) {
        if (RefPtr automationSession = process->processPool().automationSession())
            automationSession->navigationOccurredForFrame(frame);
    }

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame.isMainFrame() && navigationID)
        navigation = m_navigationState->takeNavigation(*navigationID);

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();

    if (frame.isMainFrame()) {
        reportPageLoadResult(error);
        protectedPageLoadState->didFailProvisionalLoad(transaction);
        protectedPageClient->didFailProvisionalLoadForMainFrame();
        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        callLoadCompletionHandlersIfNecessary(false);
    }

    frame.didFailProvisionalLoad();

    protectedPageLoadState->commitChanges();
#if ENABLE(WEBDRIVER_BIDI)
    if (willInternallyHandleFailure == WillInternallyHandleFailure::No) {
        if (RefPtr automationSession = activeAutomationSession())
            automationSession->navigationFailedForFrame(frame, navigationID);
    }
#endif

    ASSERT(!m_failingProvisionalLoadURL);
    m_failingProvisionalLoadURL = WTFMove(provisionalURL);

    if (willInternallyHandleFailure == WillInternallyHandleFailure::No) {
        auto callClientFunctions = [this, protectedThis = Ref { *this }, frame = Ref { frame }, navigation, error, process, request = WTFMove(request), frameInfo = WTFMove(frameInfo), protectedObject = userData.protectedObject()]() mutable {
            if (m_loaderClient)
                m_loaderClient->didFailProvisionalLoadWithErrorForFrame(*this, frame, navigation.get(), error, process->transformHandlesToObjects(protectedObject.get()).get());
            else {
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), request.url(), error, process->transformHandlesToObjects(protectedObject.get()).get());
                m_navigationClient->didFailProvisionalLoadWithErrorForFrame(*this, WTFMove(request), error, WTFMove(frameInfo));
            }
        };
#if HAVE(SAFE_BROWSING)
        URL failedURL { m_failingProvisionalLoadURL };
        bool canFallbackToHTTP = frame.isMainFrame() && error.errorRecoveryMethod() == ResourceError::ErrorRecoveryMethod::HTTPFallback && failedURL.protocolIs("https"_s);
        if (RefPtr websitePolicies = navigation ? navigation->websitePolicies() : nullptr; websitePolicies
            && websitePolicies->isUpgradeWithUserMediatedFallbackEnabled()
            && !websitePolicies->advancedPrivacyProtections().contains(AdvancedPrivacyProtections::HTTPSOnlyExplicitlyBypassedForDomain)
            && !protectedPageLoadState->httpFallbackInProgress()
            && canFallbackToHTTP) {
            protectedPageClient->clearBrowsingWarning();

            Ref httpFallbackBrowsingWarning = BrowsingWarning::create(failedURL, frame.isMainFrame(), BrowsingWarning::HTTPSNavigationFailureData { });
            protectedPageLoadState->setTitleFromBrowsingWarning(transaction, httpFallbackBrowsingWarning->title());
            protectedPageClient = pageClient();
            protectedPageClient->showBrowsingWarning(httpFallbackBrowsingWarning, [this, protectedThis = Ref { *this }, protectedPageClient, failedURL = WTFMove(failedURL), callClientFunctions] (auto&& result) mutable {
                Ref protectedPageLoadState = pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setTitleFromBrowsingWarning(transaction, { });

                switchOn(result, [&protectedThis] (const URL& url) {
                    protectedThis->loadRequest({ URL { url } });
                }, [&protectedThis, &failedURL, &callClientFunctions] (ContinueUnsafeLoad continueUnsafeLoad) {
                    switch (continueUnsafeLoad) {
                    case ContinueUnsafeLoad::No:
                        callClientFunctions();
                        break;
                    case ContinueUnsafeLoad::Yes:
                        failedURL.setProtocol("http"_s);
                        protectedThis->loadRequest({ WTFMove(failedURL) }, ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks, IsPerformingHTTPFallback::Yes);
                        break;
                    }
                });
            });
            // FIXME: We need a new delegate that uses a more generic name.
            m_uiClient->didShowSafeBrowsingWarning();
        } else
#endif
            callClientFunctions();
    } else {
        if (RefPtr websitePolicies = navigation ? navigation->websitePolicies() : nullptr; websitePolicies
            && (websitePolicies->isUpgradeWithAutomaticFallbackEnabled() || protectedPreferences()->httpSByDefaultEnabled())) {
            URL failedURL { m_failingProvisionalLoadURL };
            if (frame.isMainFrame() && error.errorRecoveryMethod() == ResourceError::ErrorRecoveryMethod::HTTPFallback && failedURL.protocolIs("https"_s)) {
                failedURL.setProtocol("http"_s);
                loadRequest(WTFMove(failedURL), ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks, IsPerformingHTTPFallback::Yes);
            }
        }
    }

    m_failingProvisionalLoadURL = { };

    // If the provisional page's load fails then we destroy the provisional page.
    if (m_provisionalPage && m_provisionalPage->mainFrame() == &frame && (willContinueLoading == WillContinueLoading::No || protectedPreferences()->siteIsolationEnabled()))
        m_provisionalPage = nullptr;

    if (auto provisionalFrame = frame.takeProvisionalFrame()) {
        ASSERT(m_preferences->siteIsolationEnabled());
        ASSERT(!frame.isMainFrame());
        ASSERT_UNUSED(provisionalFrame, provisionalFrame->process().coreProcessIdentifier() != frame.process().coreProcessIdentifier());
        frame.notifyParentOfLoadCompletion(process);
        frame.broadcastFrameTreeSyncData(FrameTreeSyncData::create());
    }
}

void WebPageProxy::didFinishServiceWorkerPageRegistration(bool success)
{
    ASSERT(m_isServiceWorkerPage);
    ASSERT(internals().serviceWorkerLaunchCompletionHandler);

    if (internals().serviceWorkerLaunchCompletionHandler)
        internals().serviceWorkerLaunchCompletionHandler(success);
}

void WebPageProxy::callLoadCompletionHandlersIfNecessary(bool success)
{
    if (m_isServiceWorkerPage && internals().serviceWorkerLaunchCompletionHandler && !success)
        internals().serviceWorkerLaunchCompletionHandler(false);
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

void WebPageProxy::didCommitLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool wasPrivateRelayed, String&& proxyName, const WebCore::ResourceResponseSource source, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    LOG(Loading, "(Loading) WebPageProxy %" PRIu64 " didCommitLoadForFrame in navigation %" PRIu64, identifier().toUInt64(), navigationID ? navigationID->toUInt64() : 0);
    LOG(BackForward, "(Back/Forward) After load commit, back/forward list is now:%s", m_backForwardList->loggingString().utf8().data());

    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    if (frame->provisionalFrame()) {
        frame->commitProvisionalFrame(connection, frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(mimeType), frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, wasPrivateRelayed, WTFMove(proxyName), source, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData);
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
                websiteDataStore->protectedNetworkProcess()->didCommitCrossSiteLoadWithDataTransfer(websiteDataStore->sessionID(), RegistrableDomain { requesterURL }, currentDomain, navigationDataTransfer, identifier(), m_webPageID, request.didFilterLinkDecoration() ? DidFilterKnownLinkDecoration::Yes : DidFilterKnownLinkDecoration::No);
            }
        }
        if (RefPtr websitePolicies = navigation->websitePolicies(); websitePolicies && !m_provisionalPage)
            m_mainFrameWebsitePolicies = websitePolicies->copy();
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
        protectedPageLoadState->didCommitLoad(transaction, certificateInfo, markPageInsecure, usedLegacyTLS, wasPrivateRelayed, WTFMove(proxyName), source, frameInfo.securityOrigin);
        m_shouldSuppressNextAutomaticNavigationSnapshot = false;

#if PLATFORM(COCOA)
        for (auto frameIDWithPendingLoad : m_framesWithSubresourceLoadingForPageLoadTiming)
            WTFEndSignpost(static_cast<uintptr_t>(frameID.toUInt64()), PLTSubresourceLoading, "didCommitLoadForFrame(%llu), ending pending resource loads for frame %llu", frameID.toUInt64(), frameIDWithPendingLoad.toUInt64());
#endif

        m_pageLoadTiming = std::exchange(m_pageLoadTimingPendingCommit, nullptr);
        m_framesWithSubresourceLoadingForPageLoadTiming.clear();
    }

#if USE(APPKIT)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME: A load going on in one frame shouldn't affect text editing in other frames on the page.
    protectedPageClient->resetSecureInputState();
#endif

    frame->didCommitLoad(mimeType, certificateInfo, containsPluginDocument);

    if (auto frameProcessSite = frame->frameProcess().site(); frameProcessSite && !frame->frameProcess().isArchiveProcess()) {
        auto frameSite = Site { frame->url() };
        if (frameProcessSite->isEmpty() && !frameSite.isEmpty())
            frame->setProcess(protectedBrowsingContextGroup()->ensureProcessForSite(frameSite, Site { protectedMainFrame()->url() }, frame->frameProcess().process(), preferences));
    }

    if (frame->isMainFrame()) {
        std::optional<WebCore::PrivateClickMeasurement> privateClickMeasurement;
        if (internals().privateClickMeasurement)
            privateClickMeasurement = internals().privateClickMeasurement->pcm;
        else if (navigation && navigation->privateClickMeasurement())
            privateClickMeasurement = *navigation->privateClickMeasurement();
        if (privateClickMeasurement) {
            if (privateClickMeasurement->destinationSite().matches(frame->url()) || privateClickMeasurement->isSKAdNetworkAttribution())
                protectedWebsiteDataStore()->storePrivateClickMeasurement(*privateClickMeasurement);
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
        protectedPageClient->didCommitLoadForMainFrame(WTFMove(mimeType), frameHasCustomContentProvider);
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

#if ENABLE(EXTENSION_CAPABILITIES)
    if (frame->isMainFrame())
        resetMediaCapability();
#endif

#if ENABLE(IMAGE_ANALYSIS)
    if (frame->isMainFrame())
        m_internals->imageTranslationLanguageIdentifiers = std::nullopt;
#endif

    if (frame->isMainFrame())
        m_internals->textManipulationParameters = std::nullopt;
}

void WebPageProxy::didFinishDocumentLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, const UserData& userData, WallTime timestamp)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    if (m_pageLoadTiming && frame->isMainFrame() && !frame->url().isAboutBlank()) {
        m_pageLoadTiming->setDocumentFinishedLoading(timestamp);
        generatePageLoadTimingSoon();
    }

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didFinishDocumentLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    if (RefPtr automationSession = activeAutomationSession())
        automationSession->documentLoadedForFrame(*frame, navigationID, timestamp);

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = m_navigationState->navigation(*navigationID);

    if (frame->isMainFrame()) {
        Ref process = WebProcessProxy::fromConnection(connection);
        m_navigationClient->didFinishDocumentLoad(*this, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
        internals().didFinishDocumentLoadForMainFrameTimestamp = MonotonicTime::now();
    }
}

HashSet<Ref<WebProcessProxy>> WebPageProxy::webContentProcessesWithFrame()
{
    HashSet<Ref<WebProcessProxy>> processes;
    for (RefPtr frame = m_mainFrame; frame; frame = frame->traverseNext().frame)
        processes.add(frame->process());
    return processes;
}

void WebPageProxy::forEachWebContentProcess(NOESCAPE Function<void(WebProcessProxy&, PageIdentifier)>&& function)
{
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&] (auto& remotePageProxy) {
        function(remotePageProxy.process(), remotePageProxy.pageID());
    });
    function(protectedLegacyMainFrameProcess(), webPageIDInMainFrameProcess());
}

void WebPageProxy::observeAndCreateRemoteSubframesInOtherProcesses(WebFrameProxy& newFrame, const String& frameName)
{
    if (!protectedPreferences()->siteIsolationEnabled())
        return;

    RefPtr parent = newFrame.parentFrame();
    if (!parent) {
        ASSERT_NOT_REACHED();
        return;
    }

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.processID() == newFrame.process().processID())
            return;
        webProcess.send(Messages::WebPage::CreateRemoteSubframe(parent->frameID(), newFrame.frameID(), frameName, newFrame.calculateFrameTreeSyncData()), pageID);
    });
}

void WebPageProxy::broadcastDocumentSyncData(IPC::Connection& connection, const WebCore::DocumentSyncSerializationData& data)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    // FIXME: Check that the sending process is allowed to write the specified property.
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess == process)
            return;
        webProcess.send(Messages::WebPage::TopDocumentSyncDataChangedInAnotherProcess(data), pageID);
    });
}

void WebPageProxy::broadcastAllDocumentSyncData(IPC::Connection& connection, Ref<WebCore::DocumentSyncData>&& data)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess == process)
            return;
        webProcess.send(Messages::WebPage::AllTopDocumentSyncDataChangedInAnotherProcess(data), pageID);
    });
}

void WebPageProxy::broadcastFrameTreeSyncData(IPC::Connection& connection, FrameIdentifier frameID, const WebCore::FrameTreeSyncSerializationData& data)
{
    // FIXME: This only allows changes from the process that the frame currently lives in.
    // We may want to also allow changes from the process that owns the parent frame, possibly
    // filtered on a per-property basis.
    Ref process = WebProcessProxy::fromConnection(connection);
    MESSAGE_CHECK(process, &siteIsolatedProcess() == process.ptr());
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess == process)
            return;
        webProcess.send(Messages::WebPage::FrameTreeSyncDataChangedInAnotherProcess(frameID, data), pageID);
    });
}

void WebPageProxy::broadcastAllFrameTreeSyncData(IPC::Connection& connection, FrameIdentifier frameID, Ref<WebCore::FrameTreeSyncData>&& data)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    MESSAGE_CHECK(process, &siteIsolatedProcess() == process.ptr());
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess == process)
            return;
        webProcess.send(Messages::WebPage::AllFrameTreeSyncDataChangedInAnotherProcess(frameID, data), pageID);
    });
}

void WebPageProxy::didFinishLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, const UserData& userData, WallTime timestamp)
{
    LOG(Loading, "WebPageProxy::didFinishLoadForFrame - WebPageProxy %p with navigationID %" PRIu64 " didFinishLoad", this, navigationID ? navigationID->toUInt64() : 0);

    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    // If a provisional load has since been started in another process, ignore this message.
    if (protectedPreferences()->siteIsolationEnabled() && !frame->provisionalLoadProcess().hasConnection(connection))
        return;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didFinishLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID && m_navigationState->hasNavigation(*navigationID))
        navigation = m_navigationState->navigation(*navigationID);

    bool isMainFrame = frame->isMainFrame();
    if (!isMainFrame || !navigationID || navigation) {
        Ref protectedPageLoadState = pageLoadState();
        auto transaction = protectedPageLoadState->transaction();

        if (isMainFrame)
            protectedPageLoadState->didFinishLoad(transaction);

        if (RefPtr automationSession = activeAutomationSession()) {
            automationSession->navigationOccurredForFrame(*frame);
            automationSession->loadCompletedForFrame(*frame, navigationID, WallTime::now());
        }

        frame->didFinishLoad();

        frame->notifyParentOfLoadCompletion(frame->protectedProcess());

        protectedPageLoadState->commitChanges();
    }

    if (protectedPreferences()->siteIsolationEnabled())
        frame->broadcastFrameTreeSyncData(frame->calculateFrameTreeSyncData());

    Ref process = WebProcessProxy::fromConnection(connection);
    if (m_loaderClient)
        m_loaderClient->didFinishLoadForFrame(*this, *frame, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didFinishNavigation(*this, navigation.get(), process->transformHandlesToObjects(userData.protectedObject().get()).get());
        m_navigationClient->didFinishLoadForFrame(*this, WTFMove(request), WTFMove(frameInfo));
    }

    if (isMainFrame) {
        reportPageLoadResult({ });
        protectedPageClient->didFinishNavigation(navigation.get());

        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        resetRecentCrashCountSoon();

        callLoadCompletionHandlersIfNecessary(true);

        if (m_pageLoadTiming && !frame->url().isAboutBlank()) {
            m_pageLoadTiming->setFinishedLoading(timestamp);
            generatePageLoadTimingSoon();
        }
    }

    if (isMainFrame || protectedPreferences()->siteIsolationEnabled())
        notifyProcessPoolToPrewarm();

    m_isLoadingAlternateHTMLStringForFailingProvisionalLoad = false;
}

void WebPageProxy::didFailLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, const ResourceError& error, const UserData& userData)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "didFailLoadForFrame: frameID=%" PRIu64 ", isMainFrame=%d, domain=%s, code=%d", frameID.toUInt64(), frame->isMainFrame(), error.domain().utf8().data(), error.errorCode());

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = m_navigationState->navigation(*navigationID);

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();

    bool isMainFrame = frame->isMainFrame();

    if (isMainFrame) {
        protectedPageLoadState->didFailLoad(transaction);
        internals().pageAllowedToRunInTheBackgroundActivityDueToTitleChanges = nullptr;
        internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications = nullptr;
    }

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    frame->didFailLoad();
    protectedPageLoadState->commitChanges();
#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->navigationFailedForFrame(*frame, navigationID);
#endif
    Ref process = WebProcessProxy::fromConnection(connection);
    if (m_loaderClient)
        m_loaderClient->didFailLoadWithErrorForFrame(*this, *frame, navigation.get(), error, process->transformHandlesToObjects(userData.protectedObject().get()).get());
    else {
        if (frameInfo.isMainFrame)
            m_navigationClient->didFailNavigationWithError(*this, frameInfo, navigation.get(), request.url(), error, process->transformHandlesToObjects(userData.protectedObject().get()).get());
        m_navigationClient->didFailLoadWithErrorForFrame(*this, WTFMove(request), error, WTFMove(frameInfo));
    }

    if (isMainFrame) {
        reportPageLoadResult(error);
        protectedPageClient->didFailNavigation(navigation.get());
        if (navigation)
            navigation->setClientNavigationActivity(nullptr);

        callLoadCompletionHandlersIfNecessary(false);
    }

    RefPtr parentFrame = frame->parentFrame();
    if (protectedPreferences()->siteIsolationEnabled()) {
        if (parentFrame && parentFrame->process().coreProcessIdentifier() != process->coreProcessIdentifier())
            frame->notifyParentOfLoadCompletion(process);

        frame->broadcastFrameTreeSyncData(FrameTreeSyncData::create());
    }
}

void WebPageProxy::didSameDocumentNavigationForFrame(IPC::Connection& connection, FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, SameDocumentNavigationType navigationType, URL&& url, const UserData& userData)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    MESSAGE_CHECK_URL(m_legacyMainFrameProcess, url);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrame: frameID=%" PRIu64 ", isMainFrame=%d, type=%u", frameID.toUInt64(), frame->isMainFrame(), enumToUnderlyingType(navigationType));

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame() && navigationID)
        navigation = m_navigationState->navigation(*navigationID);

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        protectedPageLoadState->didSameDocumentNavigation(transaction, url.string());

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    protectedPageLoadState->clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(WTFMove(url));

    protectedPageLoadState->commitChanges();
#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->fragmentNavigatedForFrame(*frame, navigationID);
#endif

    if (isMainFrame) {
        Ref process = WebProcessProxy::fromConnection(connection);
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, process->transformHandlesToObjects(userData.protectedObject().get()).get());
    }

    if (isMainFrame)
        protectedPageClient->didSameDocumentNavigationForMainFrame(navigationType);
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

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didSameDocumentNavigationForFrameViaJS: frameID=%" PRIu64 ", isMainFrame=%d, type=%u", frameID.toUInt64(), frame->isMainFrame(), enumToUnderlyingType(navigationType));

    // FIXME: We should message check that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.
    RefPtr<API::Navigation> navigation;
    if (frame->isMainFrame()) {
        navigation = m_navigationState->createLoadRequestNavigation(process->coreProcessIdentifier(), ResourceRequest(URL { url }), m_backForwardList->currentItem());
        navigation->setLastNavigationAction(WTFMove(navigationActionData));
    }

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();

    bool isMainFrame = frame->isMainFrame();
    if (isMainFrame)
        protectedPageLoadState->didSameDocumentNavigation(transaction, url.string());

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = m_configuration->processPool().automationSession())
            automationSession->navigationOccurredForFrame(*frame);
    }

    protectedPageLoadState->clearPendingAPIRequest(transaction);
    frame->didSameDocumentNavigation(WTFMove(url));

    protectedPageLoadState->commitChanges();
#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr automationSession = activeAutomationSession())
        automationSession->fragmentNavigatedForFrame(*frame, navigation ? std::optional(navigation->navigationID()) : std::nullopt);
#endif

    if (isMainFrame)
        m_navigationClient->didSameDocumentNavigation(*this, navigation.get(), navigationType, process->transformHandlesToObjects(userData.protectedObject().get()).get());

    if (isMainFrame)
        protectedPageClient->didSameDocumentNavigationForMainFrame(navigationType);

    if (navigation)
        m_navigationState->didDestroyNavigation(navigation->processID(), navigation->navigationID());
}

void WebPageProxy::didChangeMainDocument(IPC::Connection& connection, FrameIdentifier frameID, std::optional<NavigationIdentifier> navigationID)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);

#if ENABLE(MEDIA_STREAM)
    if (m_userMediaPermissionRequestManager) {
        auto shouldClearAllGrantedRequests = [&] {
            if (!frame)
                return true;
            if (!frame->isMainFrame())
                return false;
            if (!navigationID || !m_navigationState->hasNavigation(*navigationID))
                return true;
            RefPtr navigation = m_navigationState->navigation(*navigationID);
            if (!navigation)
                return true;
            return navigation->isRequestFromClientOrUserInput();
        };
        protectedUserMediaPermissionRequestManager()->resetAccess(shouldClearAllGrantedRequests() ? nullptr : frame.get());

#if ENABLE(GPU_PROCESS)
        if (RefPtr gpuProcess = m_configuration->processPool().gpuProcess()) {
            Ref process = WebProcessProxy::fromConnection(connection);
            if (frame)
                gpuProcess->updateCaptureOrigin(SecurityOriginData::fromURLWithoutStrictOpaqueness(frame->url()), process->coreProcessIdentifier());
        }
#endif
    }
#endif

    m_isQuotaIncreaseDenied = false;

    m_speechRecognitionPermissionManager = nullptr;
}

void WebPageProxy::viewIsBecomingVisible()
{
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "viewIsBecomingVisible:");
    protectedLegacyMainFrameProcess()->markProcessAsRecentlyUsed();
    if (RefPtr drawingAreaProxy = drawingArea())
        drawingAreaProxy->viewIsBecomingVisible();
#if ENABLE(MEDIA_STREAM)
    if (RefPtr userMediaPermissionRequestManager = m_userMediaPermissionRequestManager)
        userMediaPermissionRequestManager->viewIsBecomingVisible();
#endif

    RefPtr protectedPageClient { pageClient() };
    protectedPageClient->viewIsBecomingVisible();
}

void WebPageProxy::viewIsBecomingInvisible()
{
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "viewIsBecomingInvisible:");
    protectedLegacyMainFrameProcess()->pageIsBecomingInvisible(m_webPageID);
    if (RefPtr drawingAreaProxy = drawingArea())
        drawingAreaProxy->viewIsBecomingInvisible();

    RefPtr protectedPageClient { pageClient() };
    protectedPageClient->viewIsBecomingInvisible();
}

void WebPageProxy::processIsNoLongerAssociatedWithPage(WebProcessProxy& process)
{
    m_navigationState->clearNavigationsFromProcess(process.coreProcessIdentifier());
}

void WebPageProxy::isNoLongerAssociatedWithRemotePage(RemotePageProxy&)
{
    internals().updatePlayingMediaDidChangeTimer.startOneShot(0_s);
}

bool WebPageProxy::hasAllowedToRunInTheBackgroundActivity() const
{
    return internals().pageAllowedToRunInTheBackgroundActivityDueToTitleChanges || internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications;
}

void WebPageProxy::didReceiveTitleForFrame(IPC::Connection& connection, FrameIdentifier frameID, String&& title, const UserData& userData)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();

    if (frame->isMainFrame()) {
        protectedPageLoadState->setTitle(transaction, String { title });
        // FIXME: Ideally we'd enable this on iPhone as well but this currently regresses PLT.
        bool deviceClassIsSmallScreen = false;
#if PLATFORM(IOS_FAMILY)
        deviceClassIsSmallScreen = PAL::deviceClassIsSmallScreen();
#endif
        if (!deviceClassIsSmallScreen) {
            bool isTitleChangeLikelyDueToUserAction = false;

#if !PLATFORM(IOS_FAMILY)
            // Disable this on iPad for now as this regresses youtube.com on PLT5
            // (rdar://127015092).
            isTitleChangeLikelyDueToUserAction = [&] {
                bool hasRecentUserActivation = (MonotonicTime::now() - internals().lastActivationTimestamp) <= 5_s;
                bool hasRecentlyCommittedLoad = (MonotonicTime::now() - internals().didCommitLoadForMainFrameTimestamp) <= 5_s;
                return hasRecentUserActivation || hasRecentlyCommittedLoad;
            }();
#endif
            if (!isTitleChangeLikelyDueToUserAction && !internals().pageAllowedToRunInTheBackgroundActivityDueToTitleChanges && !frame->title().isNull() && frame->title() != title) {
                WEBPAGEPROXY_RELEASE_LOG(ViewState, "didReceiveTitleForFrame: This page updates its title without user interaction and is allowed to run in the background");
                internals().pageAllowedToRunInTheBackgroundActivityDueToTitleChanges = WebProcessProxy::fromConnection(connection)->protectedThrottler()->backgroundActivity("Page updates its title"_s);
            }
        }
    }

    frame->didChangeTitle(WTFMove(title));

    protectedPageLoadState->commitChanges();

#if ENABLE(REMOTE_INSPECTOR)
    if (frame->isMainFrame())
        remoteInspectorInformationDidChange();
#endif
}

void WebPageProxy::processDidUpdateThrottleState()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->processDidUpdateThrottleState();
}


void WebPageProxy::didFirstLayoutForFrame(FrameIdentifier, const UserData& userData)
{
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(IPC::Connection& connection, FrameIdentifier frameID, const UserData& userData, WallTime timestamp)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    Ref process = WebProcessProxy::fromConnection(connection);
    if (m_loaderClient)
        m_loaderClient->didFirstVisuallyNonEmptyLayoutForFrame(*this, *frame, process->transformHandlesToObjects(userData.protectedObject().get()).get());

    if (frame->isMainFrame())
        protectedPageClient->didFirstVisuallyNonEmptyLayoutForMainFrame();

    if (m_pageLoadTiming && !m_pageLoadTiming->firstVisualLayout()) {
        m_pageLoadTiming->setFirstVisualLayout(timestamp);
        generatePageLoadTimingSoon();
    }
}

void WebPageProxy::didLayoutForCustomContentProvider()
{
    didReachLayoutMilestone({ WebCore::LayoutMilestone::DidFirstLayout, WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout, WebCore::LayoutMilestone::DidHitRelevantRepaintedObjectsAreaThreshold }, WallTime::now());
}

void WebPageProxy::didReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> layoutMilestones, WallTime timestamp)
{
    RefPtr protectedPageClient { pageClient() };

    if (layoutMilestones.contains(WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout))
        protectedPageClient->clearBrowsingWarningIfForMainFrameNavigation();

    if (layoutMilestones.contains(WebCore::LayoutMilestone::DidFirstMeaningfulPaint)) {
        if (m_pageLoadTiming && !m_pageLoadTiming->firstMeaningfulPaint()) {
            m_pageLoadTiming->setFirstMeaningfulPaint(timestamp);
            generatePageLoadTimingSoon();
        }
    }

    if (m_loaderClient)
        m_loaderClient->didReachLayoutMilestone(*this, layoutMilestones);
    m_navigationClient->renderingProgressDidChange(*this, layoutMilestones);
}

void WebPageProxy::mainFramePluginHandlesPageScaleGestureDidChange(bool mainFramePluginHandlesPageScaleGesture, double minScale, double maxScale)
{
    m_mainFramePluginHandlesPageScaleGesture = mainFramePluginHandlesPageScaleGesture;
    m_pluginMinZoomFactor = minScale;
    m_pluginMaxZoomFactor = maxScale;
}

#if !PLATFORM(COCOA)
void WebPageProxy::beginSafeBrowsingCheck(const URL&, API::Navigation&, bool forMainFrameNavigation)
{
}
#endif

void WebPageProxy::decidePolicyForNavigationActionAsync(IPC::Connection& connection, NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(data.frameInfo.frameID);
    if (!frame)
        return completionHandler({ });

    auto url = data.request.url();
    Ref process = WebProcessProxy::fromConnection(connection);
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
    if (!sandboxFlags.contains(SandboxFlag::Popups) || !sandboxFlags.contains(SandboxFlag::TopNavigation) || !sandboxFlags.contains(SandboxFlag::TopNavigationToCustomProtocols))
        return true;

    return !sandboxFlags.contains(SandboxFlag::TopNavigationByUserActivation) && hasUserGesture;
}
#endif

void WebPageProxy::decidePolicyForNavigationAction(Ref<WebProcessProxy>&& process, WebFrameProxy& frame, NavigationActionData&& navigationActionData, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
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
        auto redirectResponse = WTFMove(navigationActionData.redirectResponse);
        navigationActionData = *navigation->lastNavigationAction();
        navigationActionData.redirectResponse = WTFMove(redirectResponse);
        navigationActionData.canHandleRequest = canHandleRequest;
        frameInfo.securityOrigin = navigation->destinationFrameSecurityOrigin();
    }

    if (!navigation) {
        Ref backForwardList = m_backForwardList;
        if (auto targetBackForwardItemIdentifier = navigationActionData.targetBackForwardItemIdentifier) {
            if (RefPtr item = backForwardList->itemForID(*targetBackForwardItemIdentifier)) {
                RefPtr fromItem = navigationActionData.sourceBackForwardItemIdentifier ? backForwardList->itemForID(*navigationActionData.sourceBackForwardItemIdentifier) : nullptr;
                if (!fromItem)
                    fromItem = backForwardList->currentItem();
                navigation = m_navigationState->createBackForwardNavigation(process->coreProcessIdentifier(), item->mainFrameItem(), WTFMove(fromItem), FrameLoadType::IndexedBackForward);
            }
        }
        if (!navigation)
            navigation = m_navigationState->createLoadRequestNavigation(process->coreProcessIdentifier(), ResourceRequest(request), backForwardList->protectedCurrentItem());
    }

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
    if (RefPtr provisionalPage = m_provisionalPage; provisionalPage && &provisionalPage->process() == process.ptr())
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
    Ref destinationFrameInfo = sourceAndDestinationEqual ? Ref { *sourceFrameInfo } : API::FrameInfo::create(FrameInfoData { frameInfo });

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
    Ref navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), destinationFrameInfo.ptr(), String(), ResourceRequest(request), originalRequest.url(), shouldOpenAppLinks, WTFMove(userInitiatedActivity), mainFrameNavigation.get(), currentMainFrameIdentifier);

#if ENABLE(CONTENT_FILTERING)
    if (frame.didHandleContentFilterUnblockNavigation(request)) {
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it was handled by content filter");
        return receivedPolicyDecision(PolicyAction::Ignore, RefPtr { m_navigationState->navigation(*navigationID) }.get(), std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, std::nullopt, WTFMove(completionHandler));
    }
#endif

    std::optional<PolicyDecisionConsoleMessage> message;

    // Other ports do not implement WebPage::platformCanHandleRequest().
#if PLATFORM(COCOA)
    // Sandboxed iframes should be allowed to open external apps via custom protocols unless explicitely allowed (https://html.spec.whatwg.org/#hand-off-to-external-software).
    bool canHandleRequest = navigationAction->data().canHandleRequest || m_urlSchemeHandlersByScheme.contains<StringViewHashTranslator>(request.url().protocol());
    if (!canHandleRequest && !destinationFrameInfo->isMainFrame() && !frameSandboxAllowsOpeningExternalCustomProtocols(navigationAction->data().effectiveSandboxFlags, !!navigationAction->data().userGestureTokenIdentifier)) {
        if (!sourceFrameInfo || !protectedPreferences()->needsSiteSpecificQuirks() || !Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(request.url().protocol(), sourceFrameInfo->securityOrigin())) {
            WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe");
            PolicyDecisionConsoleMessage errorMessage {
                MessageLevel::Error,
                MessageSource::Security,
                "Ignoring request to load this main resource because it has a custom protocol and comes from a sandboxed iframe"_s
            };
            return receivedPolicyDecision(PolicyAction::Ignore, RefPtr { m_navigationState->navigation(*navigationID) }.get(), std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, WTFMove(errorMessage), WTFMove(completionHandler));
        }
        message = PolicyDecisionConsoleMessage {
            MessageLevel::Warning,
            MessageSource::Security,
            "In the future, requests to navigate to a URL with custom protocol from a sandboxed iframe will be ignored"_s
        };
    }
#endif

    ShouldExpectSafeBrowsingResult shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::Yes;
    if (!protectedPreferences()->safeBrowsingEnabled())
        shouldExpectSafeBrowsingResult = ShouldExpectSafeBrowsingResult::No;

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
        frame = Ref { frame },
        completionHandler = WTFMove(completionHandler),
        navigation,
        navigationAction,
        message = WTFMove(message),
        frameInfo,
        protectedPageClient = RefPtr { pageClient() }
    ] (PolicyAction policyAction, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain> isAppBoundDomain, WasNavigationIntercepted wasNavigationIntercepted) mutable {
        WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: listener called: frameID=%" PRIu64 ", isMainFrame=%d, navigationID=%" PRIu64  ", policyAction=%" PUBLIC_LOG_STRING ", isAppBoundDomain=%d, wasNavigationIntercepted=%d", frame->frameID().toUInt64(), frame->isMainFrame(), navigation ? navigation->navigationID().toUInt64() : 0, toString(policyAction).characters(), !!isAppBoundDomain, wasNavigationIntercepted == WasNavigationIntercepted::Yes);

        if (policies && !policies->alternateRequest().isNull())
            navigation->setCurrentRequest(ResourceRequest(policies->alternateRequest()), std::nullopt);
        navigation->setWebsitePolicies(policies);

        auto completionHandlerWrapper = [
            this,
            protectedThis,
            processInitiatingNavigation = WTFMove(processInitiatingNavigation),
            frame,
            frameInfo,
            completionHandler = WTFMove(completionHandler),
            navigation = Ref { *navigation },
            navigationAction = WTFMove(navigationAction),
            wasNavigationIntercepted,
            processSwapRequestedByClient,
            message = WTFMove(message)
        ] (PolicyAction policyAction) mutable {
            if (frame->isMainFrame()) {
                if (!navigation->websitePolicies())
                    navigation->setWebsitePolicies(m_configuration->protectedDefaultWebsitePolicies()->copy());
                if (RefPtr policies = navigation->websitePolicies()) {
                    navigation->setEffectiveContentMode(effectiveContentModeAfterAdjustingPolicies(*policies, navigation->currentRequest()));
                    adjustAdvancedPrivacyProtectionsIfNeeded(*policies);
                }
            }
            receivedNavigationActionPolicyDecision(processInitiatingNavigation, policyAction, navigation.get(), WTFMove(navigationAction), processSwapRequestedByClient, frame, frameInfo, wasNavigationIntercepted, WTFMove(message), WTFMove(completionHandler));
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

        protectedPageClient->clearBrowsingWarning();

        if (RefPtr safeBrowsingWarning = navigation->safeBrowsingWarning()) {
            navigation->setSafeBrowsingWarning(nullptr);
            if (frame->isMainFrame() && safeBrowsingWarning->url().isValid()) {
                Ref protectedPageLoadState = pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), safeBrowsingWarning->url().string() });
                protectedPageLoadState->commitChanges();
            }

            if (!frame->isMainFrame()) {
                auto error = interruptedForPolicyChangeError(navigation->currentRequest());
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), navigation->currentRequest().url(), error, nullptr);
                WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForNavigationAction: Ignoring request to load subframe resource because Safe Browsing found a match.");
                completionHandlerWrapper(PolicyAction::Ignore);
                return;
            }

            Ref protectedPageLoadState = pageLoadState();
            auto transaction = protectedPageLoadState->transaction();
            protectedPageLoadState->setTitleFromBrowsingWarning(transaction, safeBrowsingWarning->title());

            protectedPageClient->showBrowsingWarning(*safeBrowsingWarning, [protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandlerWrapper), policyAction, protectedPageClient] (auto&& result) mutable {

                Ref protectedPageLoadState = protectedThis->pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setTitleFromBrowsingWarning(transaction, { });

                switchOn(result, [&] (const URL& url) {
                    completionHandler(PolicyAction::Ignore);
                    protectedThis->loadRequest({ URL { url } });
                }, [&protectedThis, &completionHandler, policyAction] (ContinueUnsafeLoad continueUnsafeLoad) {
                    switch (continueUnsafeLoad) {
                    case ContinueUnsafeLoad::No:
                        if (!protectedThis->hasCommittedAnyProvisionalLoads())
                            protectedThis->m_uiClient->close(protectedThis.ptr());
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

    }, ShouldExpectSafeBrowsingResult::No, shouldExpectAppBoundDomainResult, shouldWaitForInitialLinkDecorationFilteringData);
    if (shouldExpectSafeBrowsingResult == ShouldExpectSafeBrowsingResult::Yes)
        beginSafeBrowsingCheck(request.url(), *navigation, frame.isMainFrame());
    if (shouldWaitForInitialLinkDecorationFilteringData == ShouldWaitForInitialLinkDecorationFilteringData::Yes)
        waitForInitialLinkDecorationFilteringData(listener);
#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldSendSecurityOriginData = !frame.isMainFrame() && shouldTreatURLProtocolAsAppBound(request.url(), websiteDataStore().configuration().enableInAppBrowserPrivacyForTesting());
    auto host = shouldSendSecurityOriginData ? frameInfo.securityOrigin.host() : request.url().host();
    auto protocol = shouldSendSecurityOriginData ? frameInfo.securityOrigin.protocol() : request.url().protocol();
    protectedWebsiteDataStore()->beginAppBoundDomainCheck(host.toString(), protocol.toString(), listener);
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
        auto data2 = SharedBuffer::create(WTFMove(logo2));
        m_shouldSuppressSwiftDemoInNextNavigationPolicyDecision = true;
        loadData(WTFMove(data2), mimeType, charset, baseURL);
        listener->ignore(WasNavigationIntercepted::Yes);
        return;
    }
#endif

    auto wasPotentiallyInitiatedByUser = navigation->isLoadedWithNavigationShared() || navigation->wasUserInitiated();
    if (!sessionID().isEphemeral())
        logFrameNavigation(frame, URL { internals().pageLoadState.url() }, request, navigationAction->data().redirectResponse.url(), wasPotentiallyInitiatedByUser);

    if (m_policyClient)
        m_policyClient->decidePolicyForNavigationAction(*this, &frame, WTFMove(navigationAction), originatingFrame.get(), originalRequest, WTFMove(request), WTFMove(listener));
    else {
#if HAVE(APP_SSO)
        if (m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision || !protectedPreferences()->isExtensibleSSOEnabled())
            navigationAction->unsetShouldPerformSOAuthorization();
#endif

        m_navigationClient->decidePolicyForNavigationAction(*this, WTFMove(navigationAction), WTFMove(listener));
    }

    m_shouldSuppressAppLinksInNextNavigationPolicyDecision = false;

#if HAVE(APP_SSO)
    m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = false;
#endif
}

void WebPageProxy::adjustAdvancedPrivacyProtectionsIfNeeded(API::WebsitePolicies& policies)
{
    if (!protectedWebsiteDataStore()->trackingPreventionEnabled())
        return;

    if (!protectedPreferences()->scriptTrackingPrivacyProtectionsEnabled())
        return;

    policies.setAdvancedPrivacyProtections(policies.advancedPrivacyProtections() | AdvancedPrivacyProtections::ScriptTrackingPrivacy);
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

    protectedWebsiteDataStore()->protectedNetworkProcess()->send(Messages::NetworkProcess::LogFrameNavigation(m_websiteDataStore->sessionID(), RegistrableDomain { targetURL }, RegistrableDomain { pageURL }, RegistrableDomain { sourceURL }, isRedirect, frame.isMainFrame(), MonotonicTime::now() - internals().didFinishDocumentLoadForMainFrameTimestamp, wasPotentiallyInitiatedByUser), 0);
}

void WebPageProxy::decidePolicyForNavigationActionSync(IPC::Connection& connection, NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    auto frameID = data.frameInfo.frameID;
    Ref process = WebProcessProxy::fromConnection(connection);
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame) {
        // This message should always be queued at this point, so we can pull it out with a 0 timeout.
        connection.waitForAndDispatchImmediately<Messages::WebPageProxy::DidCreateSubframe>(webPageIDInProcess(process), 0_s);
        frame = WebFrameProxy::webFrame(frameID);
        MESSAGE_CHECK_COMPLETION_BASE(frame, connection, reply({ }));
    }

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

void WebPageProxy::decidePolicyForNewWindowAction(IPC::Connection& connection, NavigationActionData&& navigationActionData, const String& frameName, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    RefPtr protectedPageClient { pageClient() };
    auto frameInfo = navigationActionData.frameInfo;
    auto request = navigationActionData.request;

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return completionHandler({ });

    Ref process = WebProcessProxy::fromConnection(connection);
    MESSAGE_CHECK_URL_COMPLETION(process, request.url(), completionHandler({ }));

    RefPtr<API::FrameInfo> sourceFrameInfo;
    if (frame)
        sourceFrameInfo = API::FrameInfo::create(WTFMove(frameInfo));

    auto userInitiatedActivity = process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    bool shouldOpenAppLinks = m_mainFrame && m_mainFrame->url().host() != request.url().host();
    auto navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.get(), nullptr, frameName, ResourceRequest(request), URL { }, shouldOpenAppLinks, WTFMove(userInitiatedActivity));

    Ref listener = frame->setUpPolicyListenerProxy([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), navigationAction] (PolicyAction policyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient processSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted) mutable {
        // FIXME: Assert the API::WebsitePolicies* is nullptr here once clients of WKFramePolicyListenerUseWithPolicies go away.
        RELEASE_ASSERT(processSwapRequestedByClient == ProcessSwapRequestedByClient::No);

        receivedPolicyDecision(policyAction, nullptr, std::nullopt, WTFMove(navigationAction), WillContinueLoadInNewProcess::No, std::nullopt, std::nullopt, WTFMove(completionHandler));
    }, ShouldExpectSafeBrowsingResult::No, ShouldExpectAppBoundDomainResult::No, ShouldWaitForInitialLinkDecorationFilteringData::No);

    if (m_policyClient)
        m_policyClient->decidePolicyForNewWindowAction(*this, *frame, navigationAction.get(), request, frameName, WTFMove(listener));
    else
        m_navigationClient->decidePolicyForNavigationAction(*this, navigationAction.get(), WTFMove(listener));
}

void WebPageProxy::decidePolicyForResponse(IPC::Connection& connection, FrameInfoData&& frameInfo, std::optional<WebCore::NavigationIdentifier> navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, String&& downloadAttribute, bool isShowingInitialAboutBlank, WebCore::CrossOriginOpenerPolicyValue activeDocumentCOOPValue, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return completionHandler({ });
    decidePolicyForResponseShared(WebProcessProxy::fromConnection(connection), m_webPageID, WTFMove(frameInfo), navigationID, response, request, canShowMIMEType, WTFMove(downloadAttribute), isShowingInitialAboutBlank, activeDocumentCOOPValue, WTFMove(completionHandler));
}

void WebPageProxy::decidePolicyForResponseShared(Ref<WebProcessProxy>&& process, PageIdentifier webPageID, FrameInfoData&& frameInfo, std::optional<WebCore::NavigationIdentifier> navigationID, const ResourceResponse& response, const ResourceRequest& request, bool canShowMIMEType, String&& downloadAttribute, bool isShowingInitialAboutBlank, WebCore::CrossOriginOpenerPolicyValue activeDocumentCOOPValue, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return completionHandler({ });
    MESSAGE_CHECK_URL_COMPLETION(process, request.url(), completionHandler({ }));
    MESSAGE_CHECK_URL_COMPLETION(process, response.url(), completionHandler({ }));
    RefPtr navigation = navigationID ? m_navigationState->navigation(*navigationID) : nullptr;
    Ref navigationResponse = API::NavigationResponse::create(API::FrameInfo::create(WTFMove(frameInfo)).get(), request, response, canShowMIMEType, WTFMove(downloadAttribute), navigation.get());

    // COOP only applies to top-level browsing contexts.
    if (frameInfo.isMainFrame && coopValuesRequireBrowsingContextGroupSwitch(isShowingInitialAboutBlank, activeDocumentCOOPValue, frameInfo.securityOrigin.securityOrigin().get(), obtainCrossOriginOpenerPolicy(response).value, SecurityOrigin::create(response.url()).get())) {
        protectedMainFrame()->disownOpener();
        m_openedMainFrameName = { };
    }

    auto expectSafeBrowsing = ShouldExpectSafeBrowsingResult::No;
    MonotonicTime requestStart;

    if (navigation && navigation->safeBrowsingCheckOngoing()) {
        expectSafeBrowsing = ShouldExpectSafeBrowsingResult::Yes;
        requestStart = navigation->requestStart();
    }

    Ref listener = frame->setUpPolicyListenerProxy([
        this,
        protectedThis = Ref { *this },
        completionHandler = WTFMove(completionHandler),
        frameInfo = WTFMove(frameInfo),
        navigation,
        process,
        navigationResponse,
        request,
        frame
    ] (PolicyAction policyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient processSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted) mutable {
        // FIXME: Assert the API::WebsitePolicies* is nullptr here once clients of WKFramePolicyListenerUseWithPolicies go away.
        RELEASE_ASSERT(processSwapRequestedByClient == ProcessSwapRequestedByClient::No);

        bool shouldForceDownload = [&] {
            // Disallows loading model files as the main resource for child frames. If desired in the future, we can remove this line and add required support to enable this behavior.
            if (!frame->isMainFrame() && MIMETypeRegistry::isSupportedModelMIMEType(navigationResponse->response().mimeType()))
                return true;
            if (policyAction != PolicyAction::Use || process->lockdownMode() != WebProcessProxy::LockdownMode::Enabled)
                return false;
            if (MIMETypeRegistry::isPDFMIMEType(navigationResponse->response().mimeType()))
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
#if USE(QUICK_LOOK) && ENABLE(QUICKLOOK_SANDBOX_RESTRICTIONS)
        bool supportsMIMEType = PreviewConverter::supportsMIMEType(navigationResponse->response().mimeType());
#endif
        auto completionHandlerWrapper = [navigation, protectedThis, request, navigationResponse = WTFMove(navigationResponse), frameInfo = WTFMove(frameInfo), completionHandler = WTFMove(completionHandler)]  (PolicyAction policyAction) mutable {
            protectedThis->receivedNavigationResponsePolicyDecision(policyAction, navigation.get(), request, WTFMove(navigationResponse), WTFMove(completionHandler));
        };
        if (navigation && navigation->safeBrowsingWarning()) {
            RefPtr safeBrowsingWarning = navigation->safeBrowsingWarning();
            if (frame->isMainFrame() && safeBrowsingWarning->url().isValid()) {
                Ref protectedPageLoadState = pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setPendingAPIRequest(transaction, { navigation->navigationID(), safeBrowsingWarning->url().string() });
                protectedPageLoadState->commitChanges();
            }

            if (!frame->isMainFrame()) {
                auto error = interruptedForPolicyChangeError(navigation->currentRequest());
                m_navigationClient->didFailProvisionalNavigationWithError(*this, FrameInfoData { frameInfo }, navigation.get(), request.url(), error, nullptr);
                WEBPAGEPROXY_RELEASE_LOG(Loading, "decidePolicyForResponseShared: Ignoring request to load subframe resource because Safe Browsing found a match.");
                completionHandlerWrapper(PolicyAction::Ignore);
                return;
            }

            Ref protectedPageLoadState = pageLoadState();
            auto transaction = protectedPageLoadState->transaction();
            protectedPageLoadState->setTitleFromBrowsingWarning(transaction, safeBrowsingWarning->title());
            navigation->setSafeBrowsingWarning(nullptr);
            protectedThis->protectedPageClient()->showBrowsingWarning(*safeBrowsingWarning, [protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandlerWrapper), policyAction] (auto&& result) mutable {

                Ref protectedPageLoadState = protectedThis->pageLoadState();
                auto transaction = protectedPageLoadState->transaction();
                protectedPageLoadState->setTitleFromBrowsingWarning(transaction, { });

                switchOn(result, [&] (const URL& url) {
                    completionHandler(PolicyAction::Ignore);
                    protectedThis->loadRequest(URL { url });
                }, [&protectedThis, &completionHandler, policyAction] (ContinueUnsafeLoad continueUnsafeLoad) {
                    switch (continueUnsafeLoad) {
                    case ContinueUnsafeLoad::No:
                        if (!protectedThis->hasCommittedAnyProvisionalLoads())
                            protectedThis->m_uiClient->close(protectedThis.ptr());
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

#if USE(QUICK_LOOK) && ENABLE(QUICKLOOK_SANDBOX_RESTRICTIONS)
        if (policyAction == PolicyAction::Use && supportsMIMEType) {
            auto auditToken = process->connection().getAuditToken();
            bool status = sandbox_enable_state_flag("EnableQuickLookSandboxResources", *auditToken);
            WEBPAGEPROXY_RELEASE_LOG(Sandbox, "Enabling EnableQuickLookSandboxResources state flag, status = %d", status);
        }
#endif
        completionHandlerWrapper(policyAction);
    }, expectSafeBrowsing , ShouldExpectAppBoundDomainResult::No, ShouldWaitForInitialLinkDecorationFilteringData::No);
    if (expectSafeBrowsing == ShouldExpectSafeBrowsingResult::Yes && navigation) {
        Seconds timeout = (MonotonicTime::now() - requestStart) * 1.5 + 0.25_s;
        RunLoop::mainSingleton().dispatchAfter(timeout, [listener, navigation] mutable {
            listener->didReceiveSafeBrowsingResults({ });
            navigation->setSafeBrowsingCheckTimedOut();
        });
    }

    if (m_policyClient)
        m_policyClient->decidePolicyForResponse(*this, *frame, response, request, canShowMIMEType, WTFMove(listener));
    else
        m_navigationClient->decidePolicyForNavigationResponse(*this, WTFMove(navigationResponse), WTFMove(listener));
}

void WebPageProxy::showBrowsingWarning(RefPtr<WebKit::BrowsingWarning>&& safeBrowsingWarning)
{
    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();
    protectedPageLoadState->setTitleFromBrowsingWarning(transaction, safeBrowsingWarning->title());
    protectedPageClient()->showBrowsingWarning(*safeBrowsingWarning, [protectedThis = Ref { *this }] (auto&& result) mutable {
        Ref protectedPageLoadState = protectedThis->pageLoadState();
        auto transaction = protectedPageLoadState->transaction();
        protectedPageLoadState->setTitleFromBrowsingWarning(transaction, { });

        switchOn(result, [protectedThis] (const URL& url) {
            protectedThis->loadRequest(URL { url });
        }, [protectedThis] (ContinueUnsafeLoad continueUnsafeLoad) {
            if (continueUnsafeLoad == ContinueUnsafeLoad::No)
                protectedThis->goBack();
            else
                protectedThis->protectedPageClient()->clearBrowsingWarning();
        });
    });
    m_uiClient->didShowSafeBrowsingWarning();
}

void WebPageProxy::triggerBrowsingContextGroupSwitchForNavigation(WebCore::NavigationIdentifier navigationID, BrowsingContextGroupSwitchDecision browsingContextGroupSwitchDecision, const Site& responseSite, NetworkResourceLoadIdentifier existingNetworkResourceLoadIdentifierToResume, CompletionHandler<void(bool success)>&& completionHandler)
{
    // FIXME: When site isolation is enabled, this should probably switch the BrowsingContextGroup. <rdar://116203642>
    ASSERT(browsingContextGroupSwitchDecision != BrowsingContextGroupSwitchDecision::StayInGroup);
    RefPtr navigation = m_navigationState->navigation(navigationID);
    WEBPAGEPROXY_RELEASE_LOG(ProcessSwapping, "triggerBrowsingContextGroupSwitchForNavigation: Process-swapping due to Cross-Origin-Opener-Policy, newProcessIsCrossOriginIsolated=%d, navigation=%p existingNetworkResourceLoadIdentifierToResume=%" PRIu64, browsingContextGroupSwitchDecision == BrowsingContextGroupSwitchDecision::NewIsolatedGroup, navigation.get(), existingNetworkResourceLoadIdentifierToResume.toUInt64());
    if (!navigation)
        return completionHandler(false);

    m_openedMainFrameName = { };
    setBrowsingContextGroup(BrowsingContextGroup::create());

    RefPtr<WebProcessProxy> processForNavigation;
    RefPtr provisionalPage = m_provisionalPage;
    auto lockdownMode = provisionalPage ? provisionalPage->process().lockdownMode() : m_legacyMainFrameProcess->lockdownMode();
    auto enhancedSecurity = provisionalPage ? provisionalPage->process().enhancedSecurity() : m_legacyMainFrameProcess->enhancedSecurity();
    if (browsingContextGroupSwitchDecision == BrowsingContextGroupSwitchDecision::NewIsolatedGroup)
        processForNavigation = m_configuration->protectedProcessPool()->createNewWebProcess(protectedWebsiteDataStore().ptr(), lockdownMode, enhancedSecurity, WebProcessProxy::IsPrewarmed::No, CrossOriginMode::Isolated);
    else
        processForNavigation = m_configuration->protectedProcessPool()->processForSite(protectedWebsiteDataStore(), WebProcessPool::IsSharedProcess::No, responseSite, responseSite, { }, lockdownMode, enhancedSecurity, m_configuration, WebCore::ProcessSwapDisposition::COOP);

    ASSERT(processForNavigation);
    auto domain = RegistrableDomain { navigation->currentRequest().url() };
    protectedWebsiteDataStore()->protectedNetworkProcess()->addAllowedFirstPartyForCookies(*processForNavigation, domain, LoadedWebArchive::No, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), processForNavigation = processForNavigation, preventProcessShutdownScope = processForNavigation->shutdownPreventingScope(), existingNetworkResourceLoadIdentifierToResume, navigationID]() mutable {
        RefPtr navigation = m_navigationState->navigation(navigationID);
        RefPtr mainFrame = m_mainFrame;
        if (!navigation || !mainFrame)
            return completionHandler(false);

        // Tell committed process to stop loading since we're going to do the provisional load in a provisional page now.
        if (!m_provisionalPage)
            send(Messages::WebPage::StopLoadingDueToProcessSwap());
        continueNavigationInNewProcess(*navigation, *mainFrame, nullptr, m_browsingContextGroup, processForNavigation.releaseNonNull(), ProcessSwapRequestedByClient::No, ShouldTreatAsContinuingLoad::YesAfterProvisionalLoadStarted, existingNetworkResourceLoadIdentifierToResume, LoadedWebArchive::No, IsPerformingHTTPFallback::No, WebCore::ProcessSwapDisposition::COOP, nullptr);
        completionHandler(true);
    });
}

// FormClient

void WebPageProxy::willSubmitForm(IPC::Connection& connection, FrameInfoData&& frameInfoData, FrameInfoData&& sourceFrameInfoData, Vector<std::pair<String, String>>&& textFieldValues, const UserData& userData, const URL& requestURL, const String& method, CompletionHandler<void()>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfoData.frameID);
    if (!frame) {
        completionHandler();
        return;
    }

    RefPtr sourceFrame = WebFrameProxy::webFrame(sourceFrameInfoData.frameID);
    if (!sourceFrame) {
        completionHandler();
        return;
    }

    for (auto& pair : textFieldValues)
        MESSAGE_CHECK_COMPLETION_BASE(API::Dictionary::MapType::isValidKey(pair.first), connection, completionHandler());

    Ref process = WebProcessProxy::fromConnection(connection);
    m_formClient->willSubmitForm(*this, *frame, *sourceFrame, WTFMove(frameInfoData), WTFMove(sourceFrameInfoData), WTFMove(textFieldValues), process->transformHandlesToObjects(userData.protectedObject().get()).get(), requestURL, method, WTFMove(completionHandler));
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebPageProxy::contentRuleListNotification(URL&& url, ContentRuleListResults&& results)
{
    m_navigationClient->contentRuleListNotification(*this, WTFMove(url), WTFMove(results));
}

void WebPageProxy::contentRuleListMatchedRule(WebCore::ContentRuleListMatchedRule&& matchedRule)
{
    m_navigationClient->contentRuleListMatchedRule(*this, WTFMove(matchedRule));
}
#endif

void WebPageProxy::didNavigateWithNavigationData(IPC::Connection& connection, const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    didNavigateWithNavigationDataShared(WebProcessProxy::fromConnection(connection), store, frameID);
}

void WebPageProxy::didNavigateWithNavigationDataShared(Ref<WebProcessProxy>&& process, const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didNavigateWithNavigationDataShared:");

    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    MESSAGE_CHECK(process, frame->page() == this);

    if (frame->isMainFrame())
        m_historyClient->didNavigateWithNavigationData(*this, store);
    process->processPool().historyClient().didNavigateWithNavigationData(process->protectedProcessPool(), *this, store, *frame);
}

void WebPageProxy::didPerformClientRedirect(IPC::Connection& connection, String&& sourceURLString, String&& destinationURLString, FrameIdentifier frameID)
{
    didPerformClientRedirectShared(WebProcessProxy::fromConnection(connection), WTFMove(sourceURLString), WTFMove(destinationURLString), frameID);
}

void WebPageProxy::didPerformClientRedirectShared(Ref<WebProcessProxy>&& process, String&& sourceURLString, String&& destinationURLString, FrameIdentifier frameID)
{
    RefPtr protectedPageClient { pageClient() };

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    MESSAGE_CHECK(process, frame->page() == this);
    MESSAGE_CHECK_URL(process, sourceURLString);
    MESSAGE_CHECK_URL(process, destinationURLString);

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didPerformClientRedirectShared: frameID=%" PRIu64 ", isMainFrame=%d", frameID.toUInt64(), frame->isMainFrame());

    if (frame->isMainFrame()) {
        m_historyClient->didPerformClientRedirect(*this, sourceURLString, destinationURLString);
        m_navigationClient->didPerformClientRedirect(*this, sourceURLString, destinationURLString);
    }
    Ref processPool = process->processPool();
    processPool->historyClient().didPerformClientRedirect(processPool, *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didPerformServerRedirect(IPC::Connection& connection, String&& sourceURLString, String&& destinationURLString, FrameIdentifier frameID)
{
    didPerformServerRedirectShared(WebProcessProxy::fromConnection(connection), WTFMove(sourceURLString), WTFMove(destinationURLString), frameID);
}

void WebPageProxy::didPerformServerRedirectShared(Ref<WebProcessProxy>&& process, String&& sourceURLString, String&& destinationURLString, FrameIdentifier frameID)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "didPerformServerRedirect:");

    RefPtr protectedPageClient { pageClient() };

    if (sourceURLString.isEmpty() || destinationURLString.isEmpty())
        return;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;
    MESSAGE_CHECK(process, frame->page() == this);

    MESSAGE_CHECK_URL(process, sourceURLString);
    MESSAGE_CHECK_URL(process, destinationURLString);

    if (frame->isMainFrame())
        m_historyClient->didPerformServerRedirect(*this, sourceURLString, destinationURLString);
    process->processPool().historyClient().didPerformServerRedirect(process->protectedProcessPool(), *this, sourceURLString, destinationURLString, *frame);
}

void WebPageProxy::didUpdateHistoryTitle(IPC::Connection& connection, String&& title, String&& url, FrameIdentifier frameID)
{
    RefPtr protectedPageClient { pageClient() };

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    MESSAGE_CHECK_BASE(frame->page() == this, connection);
    MESSAGE_CHECK_URL(m_legacyMainFrameProcess, url);

    if (frame->isMainFrame())
        m_historyClient->didUpdateHistoryTitle(*this, title, url);
    Ref processPool = configuration().processPool();
    processPool->historyClient().didUpdateHistoryTitle(processPool, *this, title, url, *frame);
}

// UIClient

using NewPageCallback = CompletionHandler<void(RefPtr<WebPageProxy>&&)>;
using UIClientCallback = Function<void(Ref<API::NavigationAction>&&, NewPageCallback&&)>;
static void trySOAuthorization(Ref<API::PageConfiguration>&& configuration, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
#if HAVE(APP_SSO)
    if (page.protectedPreferences()->isExtensibleSSOEnabled()) {
        page.protectedWebsiteDataStore()->soAuthorizationCoordinator(page).tryAuthorize(WTFMove(configuration), WTFMove(navigationAction), page, WTFMove(newPageCallback), WTFMove(uiClientCallback));
        return;
    }
#endif
    uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
}

// FIXME: navigationActionData.hasOpener and windowFeatures.wantsNoOpener() are almost redundant bits that we are assuming are always equal,
// except noreferrer and noopener are similar and related but slightly different.
// Serialize WindowFeatures.noreferrer, distinguish between noopener and noreferrer in the UI process, and stop
// serializing redundant information that has to be just right.
void WebPageProxy::createNewPage(IPC::Connection& connection, WindowFeatures&& windowFeatures, NavigationActionData&& navigationActionData, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebKit::WebPageCreationParameters>)>&& reply)
{
    auto& originatingFrameInfoData = navigationActionData.originatingFrameInfoData;
    auto& request = navigationActionData.request;
    bool openedBlobURL = request.url().protocolIsBlob();
    MESSAGE_CHECK_COMPLETION_BASE(WebFrameProxy::webFrame(originatingFrameInfoData.frameID), connection, reply(std::nullopt, std::nullopt));

    Ref process = WebProcessProxy::fromConnection(connection);
    auto navigationDataForNewProcess = navigationActionData.hasOpener ? nullptr : makeUnique<NavigationActionData>(navigationActionData);

    auto originatingFrameInfo = API::FrameInfo::create(WTFMove(originatingFrameInfoData));
    auto mainFrameURL = m_mainFrame ? m_mainFrame->url() : URL();
    auto openedMainFrameName = navigationActionData.openedMainFrameName;

    auto effectiveSandboxFlags = navigationActionData.effectiveSandboxFlags;
    if (!effectiveSandboxFlags.contains(SandboxFlag::PropagatesToAuxiliaryBrowsingContexts))
        effectiveSandboxFlags = { };

    std::optional<bool> openerAppInitiatedState;
    if (RefPtr page = originatingFrameInfo->page())
        openerAppInitiatedState = page->lastNavigationWasAppInitiated();

    auto completionHandler = [
        this,
        protectedThis = Ref { *this },
        process,
        mainFrameURL,
        request,
        reply = WTFMove(reply),
        privateClickMeasurement = navigationActionData.privateClickMeasurement,
        openerAppInitiatedState = WTFMove(openerAppInitiatedState),
        navigationDataForNewProcess = WTFMove(navigationDataForNewProcess),
        shouldOpenExternalURLsPolicy = navigationActionData.shouldOpenExternalURLsPolicy,
        openedBlobURL,
        wantsNoOpener = windowFeatures.wantsNoOpener()
    ] (RefPtr<WebPageProxy> newPage) mutable {

#if PLATFORM(MAC)
        openerInfoOfPageBeingOpened() = std::nullopt;
#endif

        m_isCallingCreateNewPage = false;
        if (!newPage) {
            reply(std::nullopt, std::nullopt);
            return;
        }

        if (RefPtr pageClient = this->pageClient())
            pageClient->dismissAnyOpenPicker();

        newPage->setOpenedByDOM();

        if (openerAppInitiatedState)
            newPage->m_lastNavigationWasAppInitiated = *openerAppInitiatedState;
        RefPtr openedMainFrame = newPage->m_mainFrame ? newPage->m_mainFrame->opener() : nullptr;

        // FIXME: Move this to WebPageProxy constructor.
        if (RefPtr page = openedMainFrame ? openedMainFrame->page() : nullptr)
            page->addOpenedPage(*newPage);

        if (RefPtr networkProcess = websiteDataStore().networkProcessIfExists()) {
            if (!wantsNoOpener)
                networkProcess->send(Messages::NetworkProcess::CloneSessionStorageForWebPage(sessionID(), identifier(), newPage->identifier()), 0);
            if (m_configuration->shouldRelaxThirdPartyCookieBlocking() == ShouldRelaxThirdPartyCookieBlocking::Yes)
                networkProcess->send(Messages::NetworkProcess::SetShouldRelaxThirdPartyCookieBlockingForPage(newPage->identifier()), 0);
        }

        newPage->m_shouldSuppressAppLinksInNextNavigationPolicyDecision = mainFrameURL.host() == request.url().host();

        if (privateClickMeasurement)
            newPage->internals().privateClickMeasurement = { { WTFMove(*privateClickMeasurement), { }, { } } };

        // When site isolation is enabled, blobs get a dedicated process if its opener process is cross-site from the main process.
        if (navigationDataForNewProcess && (protectedPreferences()->siteIsolationEnabled() || !openedBlobURL))  {
            bool isRequestFromClientOrUserInput = navigationDataForNewProcess->isRequestFromClientOrUserInput;

            reply(std::nullopt, std::nullopt);
            newPage->loadRequest(WTFMove(request), shouldOpenExternalURLsPolicy, IsPerformingHTTPFallback::No, WTFMove(navigationDataForNewProcess), nullptr, isRequestFromClientOrUserInput);
            return;
        }

        ASSERT(newPage->m_mainFrame);
        reply(newPage->webPageIDInProcess(process), newPage->creationParameters(process, *newPage->protectedDrawingArea().get(), newPage->m_mainFrame->frameID(), std::nullopt));

#if HAVE(APP_SSO)
        newPage->m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = true;
#endif
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
        newPage->m_needsInitialLinkDecorationFilteringData = LinkDecorationFilteringController::sharedSingleton().cachedListData().isEmpty();
        newPage->m_shouldUpdateAllowedQueryParametersForAdvancedPrivacyProtections = cachedAllowedQueryParametersForAdvancedPrivacyProtections().isEmpty();
#endif
    };

    RefPtr userInitiatedActivity = process->userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);

    if (userInitiatedActivity && protectedPreferences()->verifyWindowOpenUserGestureFromUIProcess())
        process->consumeIfNotVerifiablyFromUIProcess(webPageIDInProcess(process), *userInitiatedActivity, navigationActionData.userGestureAuthorizationToken);

    bool shouldOpenAppLinks = originatingFrameInfo->request().url().host() != request.url().host();
    Ref navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), originatingFrameInfo.ptr(), nullptr, String(), WTFMove(request), URL(), shouldOpenAppLinks, WTFMove(userInitiatedActivity));

    Ref configuration = this->configuration().copy();
    configuration->setInitialSandboxFlags(effectiveSandboxFlags);
    auto effectiveReferrerPolicy = navigationActionData.effectiveReferrerPolicy;
    configuration->setInitialReferrerPolicy(effectiveReferrerPolicy);
    configuration->setWindowFeatures(WTFMove(windowFeatures));
    configuration->setOpenedMainFrameName(openedMainFrameName);

    if (RefPtr openerFrame = WebFrameProxy::webFrame(originatingFrameInfoData.frameID); navigationActionData.hasOpener && openerFrame) {
        configuration->setRelatedPage(*this);
        configuration->setOpenerInfo({ {
            openerFrame->frameProcess().process(),
            m_browsingContextGroup.copyRef(),
            originatingFrameInfoData.frameID
        } });
        WebCore::Site site { openerFrame->url() };
        ASSERT(!configuration->preferences().siteIsolationEnabled() || openerFrame->frameProcess().isSharedProcess() || site.isEmpty() || (openerFrame->frameProcess().site() && (*openerFrame->frameProcess().site() == site || openerFrame->frameProcess().site()->isEmpty())));
        configuration->setOpenedSite(site);
    } else {
        configuration->setOpenerInfo(std::nullopt);
        configuration->setOpenedSite(WebCore::Site(request.url()));
        if (openedBlobURL)
            configuration->setRelatedPage(*this);
    }

#if PLATFORM(MAC)
    if (WTF::MacApplication::isSafari())
        openerInfoOfPageBeingOpened() = configuration->openerInfo();
#endif

    trySOAuthorization(configuration.copyRef(), WTFMove(navigationAction), *this, WTFMove(completionHandler), [this, protectedThis = Ref { *this }, configuration] (Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler) mutable {
        m_isCallingCreateNewPage = true;
        m_uiClient->createNewPage(*this, WTFMove(configuration), WTFMove(navigationAction), WTFMove(completionHandler));
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

#if PLATFORM(COCOA)
CheckedPtr<RemoteScrollingCoordinatorProxy> WebPageProxy::checkedScrollingCoordinatorProxy() const
{
    return m_scrollingCoordinatorProxy.get();
}
#endif

void WebPageProxy::exitFullscreenImmediately()
{
#if ENABLE(FULLSCREEN_API)
    if (RefPtr manager = fullScreenManager())
        manager->close();
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

bool WebPageProxy::canEnterFullscreen()
{
    if (RefPtr playbackSessionManager = m_playbackSessionManager)
        return playbackSessionManager->canEnterVideoFullscreen();
    return false;
}

void WebPageProxy::enterFullscreen()
{
    RefPtr playbackSessionManager = m_playbackSessionManager;
    if (!playbackSessionManager)
        return;

    RefPtr controlsManagerInterface = playbackSessionManager->controlsManagerInterface();
    if (!controlsManagerInterface)
        return;

    CheckedPtr playbackSessionModel = controlsManagerInterface->playbackSessionModel();
    if (!playbackSessionModel)
        return;

    playbackSessionModel->enterFullscreen();
}

void WebPageProxy::willEnterFullscreen(PlaybackSessionContextIdentifier identifier)
{
    m_uiClient->willEnterFullscreen(this);
}

void WebPageProxy::didEnterFullscreen(PlaybackSessionContextIdentifier identifier)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didEnterFullscreen();
    m_uiClient->didEnterFullscreen(this);

    internals().currentFullscreenVideoSessionIdentifier = identifier;
    updateFullscreenVideoTextRecognition();
}

void WebPageProxy::didExitFullscreen(PlaybackSessionContextIdentifier identifier)
{
    if (RefPtr manager = m_screenOrientationManager)
        manager->unlockIfNecessary();

    if (RefPtr pageClient = this->pageClient())
        pageClient->didExitFullscreen();
    m_uiClient->didExitFullscreen(this);

    if (internals().currentFullscreenVideoSessionIdentifier == identifier) {
        internals().currentFullscreenVideoSessionIdentifier = std::nullopt;
        updateFullscreenVideoTextRecognition();
    }
}

void WebPageProxy::didCleanupFullscreen(PlaybackSessionContextIdentifier)
{
    WEBPAGEPROXY_RELEASE_LOG(Fullscreen, "didCleanupFullscreen");
    if (RefPtr pageClient = this->pageClient())
        pageClient->didCleanupFullscreen();
}

void WebPageProxy::failedToEnterFullscreen(PlaybackSessionContextIdentifier identifier)
{
}

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::didEnterStandby(PlaybackSessionContextIdentifier)
{
    m_uiClient->didEnterStandby(*this);
}

void WebPageProxy::didExitStandby(PlaybackSessionContextIdentifier)
{
    m_uiClient->didExitStandby(*this);
}
#endif

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
    if (RefPtr pageClient = this->pageClient())
        pageClient->clearAllEditCommands();
    m_uiClient->close(this);
}

void WebPageProxy::runModalJavaScriptDialog(RefPtr<WebFrameProxy>&& frame, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void(WebPageProxy&, WebFrameProxy* frame, FrameInfoData&& frameInfo, String&& message2, CompletionHandler<void()>&&)>&& runDialogCallback)
{
    protectedPageClient()->runModalJavaScriptDialog([weakThis = WeakPtr { *this }, frameInfo = WTFMove(frameInfo), frame = WTFMove(frame), message = WTFMove(message), runDialogCallback = WTFMove(runDialogCallback)]() mutable {
        RefPtr protectedThis { weakThis.get() };
        if (!protectedThis)
            return;

        protectedThis->m_isRunningModalJavaScriptDialog = true;
        runDialogCallback(*protectedThis, frame.get(), WTFMove(frameInfo), WTFMove(message), [weakThis = WTFMove(weakThis)]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->m_isRunningModalJavaScriptDialog = false;
        });
    });
}

void WebPageProxy::runJavaScriptAlert(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void()>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return reply();

    exitFullscreenImmediately();

    // Since runJavaScriptAlert() can spin a nested run loop we need to turn off the responsiveness timer.
    WebProcessProxy::fromConnection(connection)->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = configuration().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this, message, std::nullopt);
    }

    runModalJavaScriptDialog(WTFMove(frame), WTFMove(frameInfo), WTFMove(message), [reply = WTFMove(reply)](WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void()>&& completion) mutable {
        page.m_uiClient->runJavaScriptAlert(page, WTFMove(message), frame, WTFMove(frameInfo), [reply = WTFMove(reply), completion = WTFMove(completion)]() mutable {
            reply();
            completion();
        });
    });
}

void WebPageProxy::runJavaScriptConfirm(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void(bool)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return reply(false);

    exitFullscreenImmediately();

    // Since runJavaScriptConfirm() can spin a nested run loop we need to turn off the responsiveness timer.
    WebProcessProxy::fromConnection(connection)->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = configuration().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this, message, std::nullopt);
    }

    runModalJavaScriptDialog(WTFMove(frame), WTFMove(frameInfo), WTFMove(message), [reply = WTFMove(reply)](WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void()>&& completion) mutable {
        page.m_uiClient->runJavaScriptConfirm(page, WTFMove(message), frame, WTFMove(frameInfo), [reply = WTFMove(reply), completion = WTFMove(completion)](bool result) mutable {
            reply(result);
            completion();
        });
    });
}

void WebPageProxy::runJavaScriptPrompt(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, String&& message, String&& defaultValue, CompletionHandler<void(const String&)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return reply({ });

    exitFullscreenImmediately();

    // Since runJavaScriptPrompt() can spin a nested run loop we need to turn off the responsiveness timer.
    WebProcessProxy::fromConnection(connection)->stopResponsivenessTimer();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = configuration().processPool().automationSession())
            automationSession->willShowJavaScriptDialog(*this, message, defaultValue);
    }

    runModalJavaScriptDialog(WTFMove(frame), WTFMove(frameInfo), WTFMove(message), [reply = WTFMove(reply), defaultValue= WTFMove(defaultValue)](WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void()>&& completion) mutable {
        page.m_uiClient->runJavaScriptPrompt(page, WTFMove(message), WTFMove(defaultValue), frame, WTFMove(frameInfo), [reply = WTFMove(reply), completion = WTFMove(completion)](auto& result) mutable {
            reply(result);
            completion();
        });
    });
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient->setStatusText(this, text);
}

void WebPageProxy::mouseDidMoveOverElement(WebHitTestResultData&& hitTestResultData, OptionSet<WebEventModifier> modifiers)
{
#if PLATFORM(MAC)
    m_lastMouseMoveHitTestResult = API::HitTestResult::create(hitTestResultData, this);
#endif

    m_uiClient->mouseDidMoveOverElement(*this, hitTestResultData, modifiers);
    setToolTip(hitTestResultData.tooltipText);
}

void WebPageProxy::setToolbarsAreVisible(bool visible)
{
    m_toolbarsAreVisible = visible;
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::SetToolbarsAreVisible(visible), pageID);
    });
}

void WebPageProxy::setMenuBarIsVisible(bool visible)
{
    m_statusBarIsVisible = visible;
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::SetMenuBarIsVisible(visible), pageID);
    });
}

void WebPageProxy::setStatusBarIsVisible(bool visible)
{
    m_menuBarIsVisible = visible;
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::SetStatusBarIsVisible(visible), pageID);
    });
}

void WebPageProxy::setIsResizable(bool isResizable)
{
    m_uiClient->setIsResizable(*this, isResizable);
}

void WebPageProxy::setWindowFrame(const FloatRect& newWindowFrame)
{
    if (RefPtr pageClient = this->pageClient())
        m_uiClient->setWindowFrame(*this, pageClient->convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(CompletionHandler<void(const FloatRect&)>&& reply)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, reply = WTFMove(reply)] (FloatRect frame) mutable {
        RefPtr pageClient = this->pageClient();
        reply(pageClient ? pageClient->convertToUserSpace(frame) : FloatRect { });
    });
}

void WebPageProxy::getWindowFrameWithCallback(Function<void(FloatRect)>&& completionHandler)
{
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (FloatRect frame) {
        RefPtr pageClient = this->pageClient();
        completionHandler(pageClient ? pageClient->convertToUserSpace(frame) : FloatRect { });
    });
}

void WebPageProxy::screenToRootView(const IntPoint& screenPoint, CompletionHandler<void(const IntPoint&)>&& reply)
{
    RefPtr pageClient = this->pageClient();
    reply(pageClient ? pageClient->screenToRootView(screenPoint) : IntPoint { });
}

void WebPageProxy::rootViewPointToScreen(const IntPoint& viewPoint, CompletionHandler<void(const IntPoint&)>&& reply)
{
    RefPtr pageClient = this->pageClient();
    reply(pageClient ? pageClient->rootViewToScreen(viewPoint) : IntPoint { });
}

void WebPageProxy::rootViewRectToScreen(const IntRect& viewRect, CompletionHandler<void(const IntRect&)>&& reply)
{
    RefPtr pageClient = this->pageClient();
    reply(pageClient ? pageClient->rootViewToScreen(viewRect) : IntRect { });
}

IntRect WebPageProxy::syncRootViewToScreen(const IntRect& viewRect)
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->rootViewToScreen(viewRect) : IntRect { };
}

void WebPageProxy::accessibilityScreenToRootView(const IntPoint& screenPoint, CompletionHandler<void(IntPoint)>&& completionHandler)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler({ });
    completionHandler(pageClient->accessibilityScreenToRootView(screenPoint));
}

void WebPageProxy::rootViewToAccessibilityScreen(const IntRect& viewRect, CompletionHandler<void(IntRect)>&& completionHandler)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler({ });
    completionHandler(pageClient->rootViewToAccessibilityScreen(viewRect));
}

void WebPageProxy::runBeforeUnloadConfirmPanel(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, String&& message, CompletionHandler<void(bool)>&& reply)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return reply(false);

    Ref webProcess = WebProcessProxy::fromConnection(connection);
    if (&frame->frameProcess().process() != webProcess.ptr()) {
        reply(true);
        return;
    }

    // Per §18 User Prompts in the WebDriver spec, "User prompts that are spawned from beforeunload
    // event handlers, are dismissed implicitly upon navigation or close window, regardless of the
    // defined user prompt handler." So, always allow the unload to proceed if the page is being automated.
    if (m_controlledByAutomation) {
        if (!!configuration().processPool().automationSession()) {
            reply(true);
            return;
        }
    }

    // Since runBeforeUnloadConfirmPanel() can spin a nested run loop we need to turn off the responsiveness timer and the tryClose timer.
    webProcess->stopResponsivenessTimer();
    bool shouldResumeTimerAfterPrompt = internals().tryCloseTimeoutTimer.isActive();
    internals().tryCloseTimeoutTimer.stop();
    m_uiClient->runBeforeUnloadConfirmPanel(*this, WTFMove(message), frame.get(), WTFMove(frameInfo),
        [weakThis = WeakPtr { *this }, completionHandler = WTFMove(reply), shouldResumeTimerAfterPrompt](bool shouldClose) mutable {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis && shouldResumeTimerAfterPrompt)
                protectedThis->internals().tryCloseTimeoutTimer.startOneShot(tryCloseTimeoutDelay);
            completionHandler(shouldClose);
    });
}

void WebPageProxy::pageDidScroll(const WebCore::IntPoint& scrollPosition)
{
    m_uiClient->pageDidScroll(this);

    if (RefPtr pageClient = this->pageClient())
        pageClient->pageDidScroll(scrollPosition);

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

#if ENABLE(MODEL_PROCESS)
void WebPageProxy::setHasModelElement(bool hasModelElement)
{
    m_hasModelElement = hasModelElement;
}
#endif

void WebPageProxy::runOpenPanel(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, const FileChooserSettings& settings)
{
    if (RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr))
        openPanelResultListener->invalidate();

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    Ref parameters = API::OpenPanelParameters::create(settings);
    Ref openPanelResultListener = WebOpenPanelResultListenerProxy::create(this, frame->protectedProcess());
    m_openPanelResultListener = openPanelResultListener.copyRef();

    if (m_controlledByAutomation) {
        if (RefPtr automationSession = configuration().processPool().automationSession())
            automationSession->handleRunOpenPanel(*this, *frame, parameters.get(), openPanelResultListener);

        // Don't show a file chooser, since automation will be unable to interact with it.
        return;
    }

    // Since runOpenPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    WebProcessProxy::fromConnection(connection)->stopResponsivenessTimer();

    const auto frameInfoForPageClient = frameInfo;

    if (!m_uiClient->runOpenPanel(*this, frame.get(), WTFMove(frameInfo), parameters.ptr(), openPanelResultListener.ptr())) {
        RefPtr pageClient = this->pageClient();
        if (!pageClient || !pageClient->handleRunOpenPanel(*this, *frame, frameInfoForPageClient, parameters, openPanelResultListener))
            didCancelForOpenPanel();
    }
}

void WebPageProxy::showShareSheet(IPC::Connection& connection, ShareDataWithParsedURL&& shareData, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(!shareData.url || shareData.url->protocolIsInHTTPFamily() || shareData.url->protocolIsData(), connection, completionHandler(false));
    MESSAGE_CHECK_COMPLETION_BASE(shareData.files.isEmpty() || protectedPreferences()->webShareFileAPIEnabled(), connection, completionHandler(false));
    MESSAGE_CHECK_COMPLETION_BASE(shareData.originator == ShareDataOriginator::Web, connection, completionHandler(false));
    if (RefPtr pageClient = this->pageClient())
        pageClient->showShareSheet(WTFMove(shareData), WTFMove(completionHandler));
    else
        completionHandler(false);
}

void WebPageProxy::showContactPicker(IPC::Connection& connection, ContactsRequestData&& requestData, CompletionHandler<void(std::optional<Vector<ContactInfo>>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(protectedPreferences()->contactPickerAPIEnabled(), connection, completionHandler(std::nullopt));
    if (RefPtr pageClient = this->pageClient())
        pageClient->showContactPicker(WTFMove(requestData), WTFMove(completionHandler));
    else
        completionHandler(std::nullopt);
}

#if ENABLE(WEB_AUTHN)
void WebPageProxy::showDigitalCredentialsPicker(IPC::Connection& connection, const WebCore::DigitalCredentialsRequestData& requestData, CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(
        protectedPreferences()->digitalCredentialsEnabled(),
        connection,
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::SecurityError, "Digital credentials feature is disabled by preference."_s }))
    );

#if HAVE(DIGITAL_CREDENTIALS_UI)
    MESSAGE_CHECK_COMPLETION_BASE(
        requestData.topOrigin.securityOrigin()->isSameOriginDomain(SecurityOrigin::create(protectedMainFrame()->url())),
        connection,
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::SecurityError, "Digital credentials request is not same-origin with main frame."_s }))
    );

    protectedPageClient()->showDigitalCredentialsPicker(requestData, WTFMove(completionHandler));
#else
    completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotSupportedError, "Digital credentials UI is not supported."_s }));
#endif
}

void WebPageProxy::fetchRawDigitalCredentialRequests(CompletionHandler<void(Vector<Variant<WebCore::MobileDocumentRequest, WebCore::OpenID4VPRequest>>)>&& completionHandler)
{
#if HAVE(DIGITAL_CREDENTIALS_UI)
    sendWithAsyncReply(Messages::DigitalCredentialsCoordinator::ProvideRawDigitalCredentialRequests(), WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

void WebPageProxy::dismissDigitalCredentialsPicker(IPC::Connection& connection, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(
        protectedPreferences()->digitalCredentialsEnabled(),
        connection,
        completionHandler(false)
    );
#if HAVE(DIGITAL_CREDENTIALS_UI)
    protectedPageClient()->dismissDigitalCredentialsPicker(WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}
#endif // ENABLE(WEB_AUTHN)

void WebPageProxy::printFrame(IPC::Connection& connection, FrameIdentifier frameID, String&& title, const FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!m_isPerformingDOMPrintOperation);
    m_isPerformingDOMPrintOperation = true;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return completionHandler();

    frame->didChangeTitle(WTFMove(title));

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

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::SetMediaVolume(volume), pageID);
    });
}

#if ENABLE(MEDIA_STREAM)
static WebCore::MediaProducerMutedStateFlags applyWebAppDesiredMutedKinds(WebCore::MediaProducerMutedStateFlags state, OptionSet<WebCore::MediaProducerMediaCaptureKind> desiredMutedKinds)
{
    if (desiredMutedKinds.contains(WebCore::MediaProducerMediaCaptureKind::EveryKind))
        state.add(MediaProducer::MediaStreamCaptureIsMuted);
    else {
        if (desiredMutedKinds.contains(WebCore::MediaProducerMediaCaptureKind::Microphone))
            state.add(WebCore::MediaProducer::MutedState::AudioCaptureIsMuted);
        if (desiredMutedKinds.contains(WebCore::MediaProducerMediaCaptureKind::Camera))
            state.add(WebCore::MediaProducer::MutedState::VideoCaptureIsMuted);
        if (desiredMutedKinds.contains(WebCore::MediaProducerMediaCaptureKind::Display)) {
            state.add(WebCore::MediaProducer::MutedState::ScreenCaptureIsMuted);
            state.add(WebCore::MediaProducer::MutedState::WindowCaptureIsMuted);
        }
        if (desiredMutedKinds.contains(WebCore::MediaProducerMediaCaptureKind::SystemAudio))
            state.add(WebCore::MediaProducer::MutedState::SystemAudioCaptureIsMuted);
    }

    return state;
}

static void updateMutedCaptureKindsDesiredByWebApp(OptionSet<WebCore::MediaProducerMediaCaptureKind>& mutedCaptureKindsDesiredByWebApp, WebCore::MediaProducerMutedStateFlags newState)
{
    if (newState.contains(WebCore::MediaProducerMutedState::AudioCaptureIsMuted))
        mutedCaptureKindsDesiredByWebApp.add(WebCore::MediaProducerMediaCaptureKind::Microphone);
    else
        mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Microphone);

    if (newState.contains(WebCore::MediaProducerMutedState::VideoCaptureIsMuted))
        mutedCaptureKindsDesiredByWebApp.add(WebCore::MediaProducerMediaCaptureKind::Camera);
    else
        mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Camera);

    if (newState.contains(WebCore::MediaProducerMutedState::ScreenCaptureIsMuted)
        || newState.contains(WebCore::MediaProducerMutedState::WindowCaptureIsMuted))
        mutedCaptureKindsDesiredByWebApp.add(WebCore::MediaProducerMediaCaptureKind::Display);
    else
        mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Display);

    if (newState.contains(WebCore::MediaProducerMutedState::SystemAudioCaptureIsMuted))
        mutedCaptureKindsDesiredByWebApp.add(WebCore::MediaProducerMediaCaptureKind::SystemAudio);
    else
        mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::SystemAudio);
}
#endif

void WebPageProxy::setMuted(WebCore::MediaProducerMutedStateFlags state, FromApplication fromApplication, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    if (fromApplication == FromApplication::Yes)
        updateMutedCaptureKindsDesiredByWebApp(m_mutedCaptureKindsDesiredByWebApp, state);
#endif

    if (!isAllowedToChangeMuteState())
        state.add(WebCore::MediaProducer::MediaStreamCaptureIsMuted);

#if ENABLE(MEDIA_STREAM)
    auto newState = applyWebAppDesiredMutedKinds(state, m_mutedCaptureKindsDesiredByWebApp);
#else
    auto newState = state;
#endif
    WEBPAGEPROXY_RELEASE_LOG(Media, "setMuted, app state = %d, final state = %d", state.toRaw(), newState.toRaw());

    internals().mutedState = newState;

    if (!hasRunningProcess())
        return completionHandler();

#if ENABLE(MEDIA_STREAM)
    bool hasMutedCaptureStreams = internals().mediaState.containsAny(WebCore::MediaProducer::MutedCaptureMask);
    if (hasMutedCaptureStreams && !(state.containsAny(WebCore::MediaProducer::MediaStreamCaptureIsMuted)))
        WebProcessProxy::muteCaptureInPagesExcept(m_webPageID);
#endif // ENABLE(MEDIA_STREAM)

    forEachWebContentProcess([&] (auto& process, auto pageID) {
        process.pageMutedStateChanged(pageID, newState);
    });

    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::SetMuted(newState), [aggregator] { }, pageID);
    });

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
    if (RefPtr manager = m_userMediaPermissionRequestManager)
        manager->resetAccess();

    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::StopMediaCapture(kind), [aggregator] { }, pageID);
    });
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

void WebPageProxy::processWillSuspend()
{
    Ref { m_legacyMainFrameProcess }->send(Messages::WebPage::ProcessWillSuspend(), webPageIDInMainFrameProcess());
}

void WebPageProxy::processDidResume()
{
    Ref { m_legacyMainFrameProcess }->send(Messages::WebPage::ProcessDidResume(), webPageIDInMainFrameProcess());
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
    Ref download = configuration().protectedProcessPool()->resumeDownload(protectedWebsiteDataStore(), this, resumeData, path, CallDownloadDidStart::Yes);
    download->setDestinationFilename(path);
    download->setDidStartCallback(WTFMove(completionHandler));
}

void WebPageProxy::downloadRequest(WebCore::ResourceRequest&& request, CompletionHandler<void(DownloadProxy*)>&& completionHandler)
{
    Ref download = configuration().protectedProcessPool()->download(protectedWebsiteDataStore(), this, request, { });
    download->setDidStartCallback(WTFMove(completionHandler));
}

void WebPageProxy::dataTaskWithRequest(WebCore::ResourceRequest&& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, bool shouldRunAtForegroundPriority, CompletionHandler<void(API::DataTask&)>&& completionHandler)
{
    protectedWebsiteDataStore()->protectedNetworkProcess()->dataTaskWithRequest(*this, sessionID(), WTFMove(request), topOrigin, shouldRunAtForegroundPriority, WTFMove(completionHandler));
}

void WebPageProxy::loadAndDecodeImage(WebCore::ResourceRequest&& request, std::optional<WebCore::FloatSize> sizeConstraint, size_t maximumBytesFromNetwork, CompletionHandler<void(Expected<Ref<WebCore::ShareableBitmap>, WebCore::ResourceError>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(makeUnexpected(decodeError(request.url())));

    if (!hasRunningProcess())
        launchProcess(Site(aboutBlankURL()), ProcessLaunchReason::InitialProcess);
    sendWithAsyncReply(Messages::WebPage::LoadAndDecodeImage(request, sizeConstraint, maximumBytesFromNetwork), [preventProcessShutdownScope = protectedLegacyMainFrameProcess()->shutdownPreventingScope(), completionHandler = WTFMove(completionHandler)] (Expected<Ref<WebCore::ShareableBitmap>, WebCore::ResourceError>&& result) mutable {
        completionHandler(WTFMove(result));
    });
}

void WebPageProxy::didChangeContentSize(const IntSize& size)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didChangeContentSize(size);
}

void WebPageProxy::didChangeIntrinsicContentSize(const IntSize& intrinsicContentSize)
{
#if USE(APPKIT)
    if (RefPtr pageClient = this->pageClient())
        pageClient->intrinsicContentSizeDidChange(intrinsicContentSize);
#endif
}

#if ENABLE(WEBXR)
PlatformXRSystem* WebPageProxy::xrSystem() const
{
    return internals().xrSystem.get();
}

void WebPageProxy::restartXRSessionActivityOnProcessResumeIfNeeded()
{
    if (RefPtr xrSystem = internals().xrSystem; xrSystem && xrSystem->hasActiveSession())
        xrSystem->ensureImmersiveSessionActivity();
}
#endif

void WebPageProxy::showColorPicker(IPC::Connection& connection, const WebCore::Color& initialColor, const IntRect& elementRect, ColorControlSupportsAlpha supportsAlpha, Vector<WebCore::Color>&& suggestions, std::optional<WebCore::FrameIdentifier>&& rootFrameID)
{
    MESSAGE_CHECK_BASE(supportsAlpha == ColorControlSupportsAlpha::No || protectedPreferences()->inputTypeColorEnhancementsEnabled(), connection);

    convertRectToMainFrameCoordinates(elementRect, rootFrameID, [weakThis = WeakPtr { *this }, initialColor, supportsAlpha, suggestions = WTFMove(suggestions)](std::optional<FloatRect> convertedRect) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !convertedRect)
            return;

        RefPtr pageClient = protectedThis->pageClient();
        if (!pageClient)
            return;

        protectedThis->internals().colorPicker = pageClient->createColorPicker(*protectedThis, initialColor, IntRect(*convertedRect), supportsAlpha, WTFMove(suggestions));
        // FIXME: Remove this conditional once all ports have a functional PageClientImpl::createColorPicker.
        if (RefPtr colorPicker = protectedThis->internals().colorPicker)
            colorPicker->showColorPicker(initialColor);
    });
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

CheckedRef<WebColorPickerClient> WebPageProxy::checkedColorPickerClient()
{
    return internals();
}

void WebPageProxy::hasVideoInPictureInPictureDidChange(bool value)
{
    uiClient().hasVideoInPictureInPictureDidChange(this, value);
#if ENABLE(SCREEN_TIME)
    protectedPageClient()->setURLIsPictureInPictureForScreenTime(value);
#endif
}


void WebPageProxy::Internals::didChooseColor(const WebCore::Color& color)
{
    Ref protectedPage = page.get();
    if (!protectedPage->hasRunningProcess())
        return;

    protectedPage->send(Messages::WebPage::DidChooseColor(color));
}

void WebPageProxy::Internals::didEndColorPicker()
{
    if (!std::exchange(colorPicker, nullptr))
        return;

    Ref protectedPage = page.get();
    if (!protectedPage->hasRunningProcess())
        return;

    protectedPage->send(Messages::WebPage::DidEndColorPicker());
}

void WebPageProxy::showDataListSuggestions(WebCore::DataListSuggestionInformation&& info)
{
    if (!internals().dataListSuggestionsDropdown) {
        RefPtr pageClient = this->pageClient();
        if (!pageClient)
            return;
        internals().dataListSuggestionsDropdown = pageClient->createDataListSuggestionsDropdown(*this);
    }
    if (!internals().dataListSuggestionsDropdown)
        return;

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

void WebPageProxy::showDateTimePicker(WebCore::DateTimeChooserParameters&& params)
{
    if (!m_dateTimePicker) {
        if (RefPtr pageClient = this->pageClient())
            m_dateTimePicker = pageClient->createDateTimePicker(*this);
    }
    if (!m_dateTimePicker)
        return;

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

    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::DidChooseDate(date.toString()));
}

void WebPageProxy::didEndDateTimePicker()
{
    m_dateTimePicker = nullptr;
    if (!hasRunningProcess())
        return;

    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::DidEndDateTimePicker());
}

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
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidSendRequest(identifier(), loadInfo, request);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didSendRequest(WTFMove(loadInfo), WTFMove(request));
}

void WebPageProxy::resourceLoadDidPerformHTTPRedirection(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request)
{
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidPerformHTTPRedirection(identifier(), loadInfo, response, request);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didPerformHTTPRedirection(WTFMove(loadInfo), WTFMove(response), WTFMove(request));
}

void WebPageProxy::resourceLoadDidReceiveChallenge(ResourceLoadInfo&& loadInfo, WebCore::AuthenticationChallenge&& challenge)
{
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidReceiveChallenge(identifier(), loadInfo, challenge);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didReceiveChallenge(WTFMove(loadInfo), WTFMove(challenge));
}

void WebPageProxy::resourceLoadDidReceiveResponse(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response)
{
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (RefPtr webExtensionController = this->webExtensionController())
        webExtensionController->resourceLoadDidReceiveResponse(identifier(), loadInfo, response);
#endif

    if (m_resourceLoadClient)
        m_resourceLoadClient->didReceiveResponse(WTFMove(loadInfo), WTFMove(response));
}

void WebPageProxy::resourceLoadDidCompleteWithError(ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceError&& error)
{
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
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

RefPtr<WebFullScreenManagerProxy> WebPageProxy::protectedFullScreenManager()
{
    return fullScreenManager();
}

void WebPageProxy::setFullscreenClient(std::unique_ptr<API::FullscreenClient>&& client)
{
    if (!client) {
        m_fullscreenClient = makeUnique<API::FullscreenClient>();
        return;
    }

    m_fullscreenClient = WTFMove(client);
}

void WebPageProxy::setFullScreenClientForTesting(std::unique_ptr<WebFullScreenManagerProxyClient>&& client)
{
    if (RefPtr fullScreenManager = m_fullScreenManager)
        fullScreenManager->detachFromClient();

    RefPtr pageClient = m_pageClient.get();
    pageClient->setFullScreenClientForTesting(WTFMove(client));

    if (RefPtr fullScreenManager = m_fullScreenManager)
        fullScreenManager->attachToNewClient(pageClient->checkedFullScreenManagerProxyClient().get());
}
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)

RefPtr<PlaybackSessionManagerProxy> WebPageProxy::protectedPlaybackSessionManager()
{
    return m_playbackSessionManager.get();
}

VideoPresentationManagerProxy* WebPageProxy::videoPresentationManager()
{
    return m_videoPresentationManager.get();
}

RefPtr<VideoPresentationManagerProxy> WebPageProxy::protectedVideoPresentationManager()
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

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
void WebPageProxy::ensureRemoteMediaSessionManagerProxy()
{
    if (!m_mediaSessionManagerProxy)
        m_mediaSessionManagerProxy = RemoteMediaSessionManagerProxy::create(webPageIDInMainFrameProcess(), Ref { siteIsolatedProcess() });
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

void WebPageProxy::setHasFocusedElementWithUserInteraction(bool value)
{
    m_hasFocusedElementWithUserInteraction = value;
}

#if HAVE(TOUCH_BAR)

void WebPageProxy::setIsTouchBarUpdateSuppressedForHiddenContentEditable(bool ignoreTouchBarUpdate)
{
    m_isTouchBarUpdateSuppressedForHiddenContentEditable = ignoreTouchBarUpdate;
}

void WebPageProxy::setIsNeverRichlyEditableForTouchBar(bool isNeverRichlyEditable)
{
    m_isNeverRichlyEditableForTouchBar = isNeverRichlyEditable;
}

#endif

void WebPageProxy::requestDOMPasteAccess(IPC::Connection& connection, DOMPasteAccessCategory pasteAccessCategory, FrameIdentifier frameID, const IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(DOMPasteAccessResponse)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(!originIdentifier.isEmpty(), connection, completionHandler(DOMPasteAccessResponse::DeniedForGesture));

    auto requiresInteraction = DOMPasteRequiresInteraction::Yes;
    if (auto origin = SecurityOrigin::createFromString(originIdentifier); !origin->isOpaque()) {
        RefPtr frame = WebFrameProxy::webFrame(frameID);
        MESSAGE_CHECK_COMPLETION_BASE(frame && frame->page() == this, connection, completionHandler(DOMPasteAccessResponse::DeniedForGesture));

        for (RefPtr currentFrame = frame; currentFrame; currentFrame = currentFrame->parentFrame()) {
            if (origin->isSameOriginDomain(SecurityOrigin::create(currentFrame->url()))) {
                requiresInteraction = DOMPasteRequiresInteraction::No;
                break;
            }
        }

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

    protectedPageClient()->requestDOMPasteAccess(pasteAccessCategory, requiresInteraction, elementRect, originIdentifier, WTFMove(completionHandler));
}

// BackForwardList

void WebPageProxy::backForwardAddItemShared(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState, LoadedWebArchive loadedWebArchive)
{
    m_backForwardList->backForwardAddItemShared(connection, WTFMove(navigatedFrameState), loadedWebArchive);
}

void WebPageProxy::backForwardGoToItemShared(BackForwardItemIdentifier itemID, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    m_backForwardList->backForwardGoToItemShared(itemID, WTFMove(completionHandler));
}

void WebPageProxy::compositionWasCanceled()
{
#if PLATFORM(COCOA)
    if (RefPtr pageClient = this->pageClient())
        pageClient->notifyInputContextAboutDiscardedComposition();
#endif
}

// Undo management

void WebPageProxy::registerEditCommandForUndo(IPC::Connection& connection, WebUndoStepID commandID, String&& label)
{
    Ref commandProxy = WebEditCommandProxy::create(commandID, WTFMove(label), *this);
    MESSAGE_CHECK_BASE(commandProxy->commandID(), connection);
    registerEditCommand(WTFMove(commandProxy), UndoOrRedo::Undo);
}

void WebPageProxy::registerInsertionUndoGrouping()
{
#if USE(INSERTION_UNDO_GROUPING)
    if (RefPtr pageClient = this->pageClient())
        pageClient->registerInsertionUndoGrouping();
#endif
}

void WebPageProxy::canUndoRedo(UndoOrRedo action, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr pageClient = this->pageClient();
    completionHandler(pageClient && pageClient->canUndoRedo(action));
}

void WebPageProxy::executeUndoRedo(UndoOrRedo action, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->executeUndoRedo(action);
    completionHandler();
}

void WebPageProxy::clearAllEditCommands()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->clearAllEditCommands();
}

#if USE(APPKIT)
void WebPageProxy::uppercaseWord()
{
    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;
    if (!targetFrameID)
        return;
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::UppercaseWord(*targetFrameID));
}

void WebPageProxy::lowercaseWord()
{
    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;
    if (!targetFrameID)
        return;
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::LowercaseWord(*targetFrameID));
}

void WebPageProxy::capitalizeWord()
{
    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;
    if (!targetFrameID)
        return;
    sendToProcessContainingFrame(targetFrameID, Messages::WebPage::CapitalizeWord(*targetFrameID));
}
#endif

void WebPageProxy::didGetImageForFindMatch(ImageBufferParameters&& parameters, ShareableBitmap::Handle&& contentImageHandle, uint32_t matchIndex)
{
    Ref image = WebImage::create({ { WTFMove(parameters), WTFMove(contentImageHandle) } });
    if (image->isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_findMatchesClient->didGetImageForMatchResult(this, image.ptr(), matchIndex);
}

#if !PLATFORM(COCOA)
void WebPageProxy::setTextIndicatorFromFrame(FrameIdentifier frameID, const RefPtr<WebCore::TextIndicator>&& textIndicator, WebCore::TextIndicatorLifetime lifetime)
{
    notImplemented();
}

void WebPageProxy::setTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator, WebCore::TextIndicatorLifetime lifetime)
{
    notImplemented();
}

void WebPageProxy::updateTextIndicatorFromFrame(FrameIdentifier frameID, RefPtr<WebCore::TextIndicator>&& textIndicator)
{
    notImplemented();
}

void WebPageProxy::updateTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator)
{
    notImplemented();
}

void WebPageProxy::clearTextIndicator()
{
    notImplemented();
}

void WebPageProxy::setTextIndicatorAnimationProgress(float animationProgress)
{
    notImplemented();
}

void WebPageProxy::teardownTextIndicatorLayer()
{
    notImplemented();
}

void WebPageProxy::startTextIndicatorFadeOut()
{
    notImplemented();
}
#endif // !PLATFORM(COCOA)

void WebPageProxy::Internals::valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex)
{
    Ref protectedPage = page.get();
    RefPtr frame = protectedPage->focusedOrMainFrame();
    if (!frame)
        return;
    protectedPage->sendToProcessContainingFrame(frame->frameID(), Messages::WebPage::DidChangeSelectedIndexForActivePopupMenu(newSelectedIndex));
}

void WebPageProxy::Internals::setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index)
{
    protectedPage()->send(Messages::WebPage::SetTextForActivePopupMenu(index));
}

void WebPageProxy::startDeferringResizeEvents()
{
    internals().protectedPage()->send(Messages::WebPage::StartDeferringResizeEvents());
}

void WebPageProxy::flushDeferredResizeEvents()
{
    internals().protectedPage()->send(Messages::WebPage::FlushDeferredResizeEvents());
}

void WebPageProxy::startDeferringScrollEvents()
{
    internals().protectedPage()->send(Messages::WebPage::StartDeferringScrollEvents());
}

void WebPageProxy::flushDeferredScrollEvents()
{
    internals().protectedPage()->send(Messages::WebPage::FlushDeferredScrollEvents());
}

void WebPageProxy::startDeferringIntersectionObservations()
{
    internals().protectedPage()->send(Messages::WebPage::StartDeferringIntersectionObservations());
}

void WebPageProxy::flushDeferredIntersectionObservations()
{
    internals().protectedPage()->send(Messages::WebPage::FlushDeferredIntersectionObservations());
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

    send(Messages::WebPage::PostInjectedBundleMessage(messageName, UserData(protectedLegacyMainFrameProcess()->transformObjectsToHandles(messageBody).get())));
}

#if PLATFORM(GTK)
void WebPageProxy::Internals::failedToShowPopupMenu()
{
    protectedPage()->send(Messages::WebPage::FailedToShowPopupMenu());
}
#endif

void WebPageProxy::showPopupMenuFromFrame(IPC::Connection& connection, FrameIdentifier frameID, const IntRect& rect, uint64_t textDirection, Vector<WebPopupItem>&& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    convertRectToMainFrameCoordinates(rect, frame->rootFrame().frameID(), [weakThis = WeakPtr { *this }, textDirection, selectedIndex, data, items = WTFMove(items), connection = Ref { connection }] (std::optional<FloatRect> convertedRect) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !convertedRect)
            return;
        protectedThis->showPopupMenu(connection, IntRect(*convertedRect), textDirection, items, selectedIndex, data);
    });
}

void WebPageProxy::showPopupMenu(IPC::Connection& connection, const IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    // FIXME: Move all IPC callers of this to WebPageProxy::showPopupMenuFromFrame and move the message check to there before converting coordinates.
    MESSAGE_CHECK_BASE(selectedIndex == -1 || static_cast<uint32_t>(selectedIndex) < items.size(), connection);

    if (RefPtr activePopupMenu = std::exchange(m_activePopupMenu, nullptr)) {
        activePopupMenu->hidePopupMenu();
        activePopupMenu->invalidate();
    }

    // If the page is controlled by automation, entering a nested run loop while the menu is open
    // can hang the page / WebDriver test. Since <option> elements are selected via a different
    // code path anyway, just don't show the native popup menu.
    if (RefPtr automationSession = configuration().processPool().automationSession()) {
        if (m_controlledByAutomation && automationSession->isSimulatingUserInteraction())
            return;
    }

    RefPtr pageClient = this->pageClient();
    RefPtr activePopupMenu = pageClient ? pageClient->createPopupMenuProxy(*this) : nullptr;
    m_activePopupMenu = activePopupMenu;

    if (!activePopupMenu)
        return;

    // Since showPopupMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    WebProcessProxy::fromConnection(connection)->stopResponsivenessTimer();

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

void WebPageProxy::showContextMenuFromFrame(FrameInfoData&& frameInfo, ContextMenuContextData&& contextMenuContextData, UserData&& userData)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return;

    auto menuLocation = contextMenuContextData.menuLocation();
    convertPointToMainFrameCoordinates(menuLocation, frame->rootFrame().frameID(), [weakThis = WeakPtr { *this }, contextMenuContextData = WTFMove(contextMenuContextData), userData = WTFMove(userData), frameInfo = WTFMove(frameInfo)] (std::optional<FloatPoint> result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!result)
            return;
        contextMenuContextData.setMenuLocation(IntPoint(*result));
        protectedThis->showContextMenu(WTFMove(frameInfo), WTFMove(contextMenuContextData), userData);
    });
}

void WebPageProxy::showContextMenu(FrameInfoData&& frameInfo, ContextMenuContextData&& contextMenuContextData, const UserData& userData)
{
    // Showing a context menu runs a nested runloop, which can handle messages that cause |this| to get closed.
    Ref protectedThis { *this };

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    // If the page is controlled by automation, entering a nested run loop while the menu is open
    // can hang the page / WebDriver test. Pretend to show and immediately dismiss the context menu.
    if (RefPtr automationSession = configuration().processPool().automationSession()) {
        if (m_controlledByAutomation && automationSession->isSimulatingUserInteraction())
            return;
    }

#if ENABLE(CONTEXT_MENUS)
    m_waitingForContextMenuToShow = true;
#endif

    // Discard any enqueued mouse events that have been delivered to the UIProcess whilst the WebProcess is still processing the
    // MouseDown event that triggered this ShowContextMenu message. This can happen if we take too long to enter the nested runloop.
    discardQueuedMouseEvents();

    internals().activeContextMenuContextData = contextMenuContextData;

    Ref activeContextMenu = pageClient->createContextMenuProxy(*this, WTFMove(frameInfo), WTFMove(contextMenuContextData), userData);
    m_activeContextMenu = activeContextMenu.copyRef();

    activeContextMenu->show();
}

void WebPageProxy::didShowContextMenu()
{
    // Don't send `Messages::WebPage::DidShowContextMenu` as that should've already been eagerly
    // sent when requesting the context menu to show, regardless of the result of that request.

    if (RefPtr pageClient = this->pageClient())
        pageClient->didShowContextMenu();
}

void WebPageProxy::didDismissContextMenu()
{
    send(Messages::WebPage::DidDismissContextMenu());

    if (RefPtr pageClient = this->pageClient())
        pageClient->didDismissContextMenu();
}

void WebPageProxy::contextMenuItemSelected(const WebContextMenuItemData& item, const FrameInfoData& frameInfo)
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
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartDashes:
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartLinks:
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticLinkDetectionEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagSmartLists:
        TextChecker::setSmartListsEnabled(!TextChecker::state().contains(TextCheckerState::SmartListsEnabled));
        protectedLegacyMainFrameProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagTextReplacement:
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticTextReplacementEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagCorrectSpellingAutomatically:
        TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticSpellingCorrectionEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
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
        TextChecker::setContinuousSpellCheckingEnabled(!TextChecker::state().contains(TextCheckerState::ContinuousSpellCheckingEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
        return;

    case ContextMenuItemTagCheckGrammarWithSpelling:
        TextChecker::setGrammarCheckingEnabled(!TextChecker::state().contains(TextCheckerState::GrammarCheckingEnabled));
            protectedLegacyMainFrameProcess()->updateTextCheckerState();
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

    case ContextMenuItemTagInspectElement:
        // The web process can no longer demand Web Inspector to show, so handle that part here.
        if (RefPtr inspector = this->inspector())
            inspector->show();
        // The actual element-selection is still handled in the web process, so we break instead of return.
        break;

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

#if PLATFORM(COCOA)
    case ContextMenuItemTagStartSpeaking:
        getSelectionOrContentsAsString([weakThis = WeakPtr { *this }](const String& selectedText) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->speak(selectedText);
        });
        break;
    case ContextMenuItemTagStopSpeaking:
        stopSpeaking();
        break;
#endif

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

#if ENABLE(TOP_LEVEL_WRITING_TOOLS_CONTEXT_MENU_ITEMS)
    case ContextMenuItemTagWritingTools:
    case ContextMenuItemTagProofread:
    case ContextMenuItemTagRewrite:
    case ContextMenuItemTagSummarize:
        handleContextMenuWritingTools(item);
        return;
#endif

    default:
        break;
    }

    if (downloadInfo) {
        Ref download = m_configuration->protectedProcessPool()->download(m_websiteDataStore, this, URL { downloadInfo->url }, frameInfo, downloadInfo->suggestedFilename);
        download->setDidStartCallback([weakThis = WeakPtr { *this }] (auto* download) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !download)
                return;
            protectedThis->m_navigationClient->contextMenuDidCreateDownload(*protectedThis, *download);
        });
    }
    auto targetFrameID = focusedOrMainFrame() ? std::optional(focusedOrMainFrame()->frameID()) : std::nullopt;
    platformDidSelectItemFromActiveContextMenu(item, [weakThis = WeakPtr { *this }, item, targetFrameID] () mutable {
        if (!weakThis)
            return;
        weakThis->sendToProcessContainingFrame(targetFrameID, Messages::WebPage::DidSelectItemFromActiveContextMenu(item));
    });
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

    auto completionHandler = [weakThis = WeakPtr { *this }, fileURLs, displayString, iconData = RefPtr { iconData }] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        RefPtr openPanelResultListener = std::exchange(protectedThis->m_openPanelResultListener, nullptr);
        if (!openPanelResultListener)
            return;
        if (RefPtr process = openPanelResultListener->process()) {
#if ENABLE(SANDBOX_EXTENSIONS)
            auto sandboxExtensionHandles = SandboxExtension::createReadOnlyHandlesForFiles("WebPageProxy::didChooseFilesForOpenPanelWithDisplayStringAndIcon"_s, fileURLs);
            process->send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(WTFMove(sandboxExtensionHandles)), protectedThis->webPageIDInMainFrameProcess());
#endif
            process->send(Messages::WebPage::DidChooseFilesForOpenPanelWithDisplayStringAndIcon(fileURLs, displayString, iconData ? iconData->span() : std::span<const uint8_t>()), protectedThis->webPageIDInMainFrameProcess());
        }

        openPanelResultListener->invalidate();
    };
    protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AllowFilesAccessFromWebProcess(m_legacyMainFrameProcess->coreProcessIdentifier(), fileURLs), WTFMove(completionHandler));
}
#endif

bool WebPageProxy::didChooseFilesForOpenPanelWithImageTranscoding(const Vector<String>& fileURLs, const Vector<String>& allowedMIMETypes)
{
#if PLATFORM(MAC)
    auto transcodingMIMEType = WebCore::MIMETypeRegistry::preferredImageMIMETypeForEncoding(allowedMIMETypes, { });

    if (transcodingMIMEType.isNull()) {
        // For designated sites which are sending "image/*", we need to force the mimetype
        // to be able to transcode from HEIC to JPEG.
        if (protectedPreferences()->needsSiteSpecificQuirks() && Quirks::shouldTranscodeHeicImagesForURL(URL { currentURL() }))
            transcodingMIMEType = "image/jpeg"_s;
        else
            return false;
    }

    auto transcodingURLs = findImagesForTranscoding(fileURLs, allowedMIMETypes);
    if (transcodingURLs.isEmpty())
        return false;

    auto transcodingUTI = WebCore::UTIFromMIMEType(transcodingMIMEType);
    auto transcodingExtension = WebCore::MIMETypeRegistry::preferredExtensionForMIMEType(transcodingMIMEType);

    sharedImageTranscodingQueueSingleton().dispatch([this, protectedThis = Ref { *this }, fileURLs = crossThreadCopy(fileURLs), transcodingURLs = crossThreadCopy(WTFMove(transcodingURLs)), transcodingUTI = WTFMove(transcodingUTI).isolatedCopy(), transcodingExtension = WTFMove(transcodingExtension).isolatedCopy()]() mutable {
        ASSERT(!RunLoop::isMain());

        auto transcodedURLs = transcodeImages(transcodingURLs, transcodingUTI, transcodingExtension);
        ASSERT(transcodingURLs.size() == transcodedURLs.size());

        RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, fileURLs = crossThreadCopy(WTFMove(fileURLs)), transcodedURLs = crossThreadCopy(WTFMove(transcodedURLs))]() {
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

    RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr);
    if (!openPanelResultListener)
        return;
    RefPtr process = openPanelResultListener->process();
    if (!process)
        return;

    auto completionHandler = [weakThis = WeakPtr { *this }, openPanelResultListener = WTFMove(openPanelResultListener), fileURLs, allowedMIMETypes] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr process = openPanelResultListener->process()) {
            if (!protectedThis->didChooseFilesForOpenPanelWithImageTranscoding(fileURLs, allowedMIMETypes)) {
#if ENABLE(SANDBOX_EXTENSIONS)
                auto sandboxExtensionHandles = SandboxExtension::createReadOnlyHandlesForFiles("WebPageProxy::didChooseFilesForOpenPanel"_s, fileURLs);
                process->send(Messages::WebPage::ExtendSandboxForFilesFromOpenPanel(WTFMove(sandboxExtensionHandles)), protectedThis->webPageIDInProcess(*process));
#endif
                process->send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs, { }), protectedThis->webPageIDInProcess(*process));
            }
        }

        openPanelResultListener->invalidate();
    };
    protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AllowFilesAccessFromWebProcess(process->coreProcessIdentifier(), fileURLs), WTFMove(completionHandler));
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!hasRunningProcess())
        return;

    RefPtr openPanelResultListener = std::exchange(m_openPanelResultListener, nullptr);
    if (!openPanelResultListener)
        return;

    if (RefPtr process = openPanelResultListener->process())
        process->send(Messages::WebPage::DidCancelForOpenPanel(), webPageIDInProcess(*process));

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
    if (RefPtr pageClient = this->pageClient())
        pageClient->registerEditCommand(WTFMove(commandProxy), undoOrRedo);
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
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->canUndoRedo(UndoOrRedo::Undo);
}

bool WebPageProxy::canRedo()
{
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->canUndoRedo(UndoOrRedo::Redo);
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

void WebPageProxy::learnWord(IPC::Connection& connection, const String& word)
{
    MESSAGE_CHECK_BASE(m_pendingLearnOrIgnoreWordMessageCount, connection);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::learnWord(spellDocumentTag(), word);
}

void WebPageProxy::ignoreWord(IPC::Connection& connection, const String& word)
{
    MESSAGE_CHECK_BASE(m_pendingLearnOrIgnoreWordMessageCount, connection);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::ignoreWord(spellDocumentTag(), word);
}

void WebPageProxy::requestCheckingOfString(TextCheckerRequestID requestID, const TextCheckingRequestData& request, int32_t insertionPoint)
{
    TextChecker::requestCheckingOfString(TextCheckerCompletion::create(requestID, request, *this), insertionPoint);
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

    if (RefPtr pageClient = this->pageClient())
        pageClient->takeFocus(direction);
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    if (m_toolTip == toolTip)
        return;

    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    if (RefPtr pageClient = this->pageClient())
        pageClient->toolTipChanged(oldToolTip, m_toolTip);
    m_uiClient->tooltipDidChange(*this, m_toolTip);
}

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->setCursor(cursor);
}

void WebPageProxy::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
}

void WebPageProxy::mouseEventHandlingCompleted(std::optional<WebEventType> eventType, bool handled, std::optional<RemoteUserInputEventData> remoteUserInputEventData)
{
    if (remoteUserInputEventData) {
        CheckedRef event = internals().mouseEventQueue.first();
        const auto originalPosition = event->position();
        const auto transformedPosition = remoteUserInputEventData->transformedPoint;
        event->setPosition(transformedPosition);

        const auto offset = originalPosition - transformedPosition;
        auto coalescedEvents = event->coalescedEvents();
        for (CheckedRef coalescedEvent : coalescedEvents) {
            const auto adjustedPosition = coalescedEvent->position() - offset;
            coalescedEvent->setPosition(adjustedPosition);
        }
        event->setCoalescedEvents(coalescedEvents);

        // FIXME: If these sandbox extensions are important, find a way to get them to the iframe process.
        sendMouseEvent(remoteUserInputEventData->targetFrameID, event, { });
        return;
    }

    // Retire the last sent event now that WebProcess is done handling it.
    MESSAGE_CHECK(m_legacyMainFrameProcess, !internals().mouseEventQueue.isEmpty());
    auto event = internals().mouseEventQueue.takeFirst();
    if (eventType) {
        MESSAGE_CHECK(m_legacyMainFrameProcess, *eventType == event.type());
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
        if (RefPtr automationSession = configuration().processPool().automationSession())
            automationSession->mouseEventsFlushedForPage(*this);
        didFinishProcessingAllPendingMouseEvents();
    }
}

#if ENABLE(MAC_GESTURE_EVENTS)
void WebPageProxy::gestureEventHandlingCompleted(std::optional<WebEventType> eventType, bool handled, std::optional<RemoteUserInputEventData> remoteUserInputEventData)
{
    if (remoteUserInputEventData) {
        sendGestureEvent(remoteUserInputEventData->targetFrameID, internals().gestureEventQueue.first());
        return;
    }

    // Retire the last sent event now that WebProcess is done handling it.
    MESSAGE_CHECK(m_legacyMainFrameProcess, !internals().gestureEventQueue.isEmpty());
    auto event = internals().gestureEventQueue.takeFirst();
    if (eventType)
        MESSAGE_CHECK(m_legacyMainFrameProcess, *eventType == event.type());

    if (RefPtr pageClient = this->pageClient(); !handled && pageClient)
        pageClient->gestureEventWasNotHandledByWebCore(event);

    if (!internals().gestureEventQueue.isEmpty())
        processNextQueuedGestureEvent();
}
#endif

void WebPageProxy::keyEventHandlingCompleted(std::optional<WebEventType> eventType, bool handled)
{
    MESSAGE_CHECK(m_legacyMainFrameProcess, !internals().keyEventQueue.isEmpty());
    auto event = internals().keyEventQueue.takeFirst();
    if (eventType)
        MESSAGE_CHECK(m_legacyMainFrameProcess, *eventType == event.type());

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

    if (RefPtr pageClient = this->pageClient())
        pageClient->doneWithKeyEvent(event, handled);
    if (!handled)
        m_uiClient->didNotHandleKeyEvent(this, event);

    // Notify the session after -[NSApp sendEvent:] has a crack at turning the event into an action.
    if (!canProcessMoreKeyEvents) {
        if (RefPtr automationSession = configuration().processPool().automationSession())
            automationSession->keyboardEventsFlushedForPage(*this);
    }
}

void WebPageProxy::didReceiveEventIPC(IPC::Connection& connection, WebEventType eventType, bool handled, std::optional<WebCore::RemoteUserInputEventData>&& remoteUserInputEventData)
{
    didReceiveEvent(&connection, eventType, handled, WTFMove(remoteUserInputEventData));
}

void WebPageProxy::didReceiveEvent(IPC::Connection* connection, WebEventType eventType, bool handled, std::optional<RemoteUserInputEventData>&& remoteUserInputEventData)
{
    MESSAGE_CHECK_BASE(!remoteUserInputEventData || protectedPreferences()->siteIsolationEnabled(), connection);
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
        protectedLegacyMainFrameProcess()->stopResponsivenessTimer();
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
        mouseEventHandlingCompleted(eventType, handled, remoteUserInputEventData);
        break;
    }

    case WebEventType::Wheel:
#if PLATFORM(COCOA)
        ASSERT(!scrollingCoordinatorProxy());
#endif
        MESSAGE_CHECK_BASE(wheelEventCoalescer().hasEventsBeingProcessed(), connection);
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
        LOG_WITH_STREAM(GestureHandling, stream << "WebPageProxy::didReceiveEvent: " << eventType << " (queue empty " << internals().gestureEventQueue.isEmpty() << ")");
        gestureEventHandlingCompleted(eventType, handled, remoteUserInputEventData);
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
        touchEventHandlingCompleted(connection, eventType, handled);
        break;
    }
#endif
    }
}

void WebPageProxy::editorStateChanged(EditorState&& editorState)
{
    // FIXME: This should not merge VisualData; they should only be merged
    // if the drawing area says to.
    if (updateEditorState(WTFMove(editorState), ShouldMergeVisualEditorState::Yes))
        dispatchDidUpdateEditorState();
}

bool WebPageProxy::updateEditorState(EditorState&& newEditorState, ShouldMergeVisualEditorState shouldMergeVisualEditorState)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->reconcileEnclosingScrollViewContentOffset(newEditorState);

    if (shouldMergeVisualEditorState == ShouldMergeVisualEditorState::Default) {
        RefPtr drawingArea = m_drawingArea;
        shouldMergeVisualEditorState = (!drawingArea || !drawingArea->shouldCoalesceVisualEditorStateUpdates()) ? ShouldMergeVisualEditorState::Yes : ShouldMergeVisualEditorState::No;
    }

    bool isStaleEditorState = newEditorState.identifier < internals().editorState.identifier;
    bool shouldKeepExistingVisualEditorState = shouldMergeVisualEditorState == ShouldMergeVisualEditorState::No && internals().editorState.hasVisualData();
    bool shouldMergeNewVisualEditorState = shouldMergeVisualEditorState == ShouldMergeVisualEditorState::Yes && newEditorState.hasVisualData();

#if PLATFORM(MAC)
    internals().scrollPositionDuringLastEditorStateUpdate = mainFrameScrollPosition();
#endif

    std::optional<EditorState> oldEditorState;
    if (!isStaleEditorState) {
        oldEditorState = std::exchange(internals().editorState, WTFMove(newEditorState));
        if (shouldKeepExistingVisualEditorState)
            internals().editorState.visualData = oldEditorState->visualData;
    } else if (shouldMergeNewVisualEditorState) {
        oldEditorState = internals().editorState;
        internals().editorState.visualData = WTFMove(newEditorState.visualData);
    }

    if (oldEditorState) {
        didUpdateEditorState(*oldEditorState, internals().editorState);
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
    if (!isAlwaysOnLoggingAllowed())
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

void WebPageProxy::logDiagnosticMessageFromWebProcess(IPC::Connection& connection, const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK_BASE(message.containsOnlyASCII(), connection);

    logDiagnosticMessage(message, description, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithResult(const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithResult(this, message, description, static_cast<WebCore::DiagnosticLoggingResultType>(result));
}

void WebPageProxy::logDiagnosticMessageWithResultFromWebProcess(IPC::Connection& connection, const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK_BASE(message.containsOnlyASCII(), connection);

    logDiagnosticMessageWithResult(message, description, result, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithValue(const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithValue(this, message, description, String::numberToStringFixedPrecision(value, significantFigures));
}

void WebPageProxy::logDiagnosticMessageWithValueFromWebProcess(IPC::Connection& connection, const String& message, const String& description, double value, unsigned significantFigures, ShouldSample shouldSample)
{
    MESSAGE_CHECK_BASE(message.containsOnlyASCII(), connection);

    logDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithEnhancedPrivacy(const String& message, const String& description, ShouldSample shouldSample)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(shouldSample);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithEnhancedPrivacy(this, message, description);
}

void WebPageProxy::logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(IPC::Connection& connection, const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK_BASE(message.containsOnlyASCII(), connection);

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

void WebPageProxy::logDiagnosticMessageWithValueDictionaryFromWebProcess(IPC::Connection& connection, const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK_BASE(message.containsOnlyASCII(), connection);

    logDiagnosticMessageWithValueDictionary(message, description, valueDictionary, shouldSample);
}

void WebPageProxy::logDiagnosticMessageWithDomain(const String& message, WebCore::DiagnosticLoggingDomain domain)
{
    auto* effectiveClient = effectiveDiagnosticLoggingClient(ShouldSample::No);
    if (!effectiveClient)
        return;

    effectiveClient->logDiagnosticMessageWithDomain(this, message, domain);
}

void WebPageProxy::logDiagnosticMessageWithDomainFromWebProcess(IPC::Connection& connection, const String& message, WebCore::DiagnosticLoggingDomain domain)
{
    MESSAGE_CHECK_BASE(message.containsOnlyASCII(), connection);

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
    case PerformanceLoggingClient::ScrollingEvent::StartedRubberbanding:
        WTFLogAlways("SCROLLING: Started Rubberbanding\n");
        break;
    }
}

void WebPageProxy::focusedElementChanged(IPC::Connection& connection, const std::optional<FrameIdentifier>& frameID, FocusOptions options)
{
    if (!frameID)
        return;

    RefPtr frame = WebFrameProxy::webFrame(*frameID);
    if (!frame)
        return;

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || &webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::ElementWasFocusedInAnotherProcess(*frameID, options), pageID);
    });
}

void WebPageProxy::focusedFrameChanged(IPC::Connection& connection, std::optional<FrameIdentifier>&& frameID)
{
    RefPtr frame = frameID ? WebFrameProxy::webFrame(*frameID) : nullptr;
    m_focusedFrame = WTFMove(frame);
    broadcastFocusedFrameToOtherProcesses(connection, WTFMove(frameID));
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
    protectedPageLoadState()->willChangeProcessIsResponsive();
}

void WebPageProxy::didChangeProcessIsResponsive()
{
    protectedPageLoadState()->didChangeProcessIsResponsive();
}

String WebPageProxy::currentURL() const
{
    String url = protectedPageLoadState()->activeURL();
    RefPtr currentItem = m_backForwardList->currentItem();
    if (url.isEmpty() && currentItem)
        url = currentItem->url();
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
        WEBPAGEPROXY_RELEASE_LOG_ERROR(Process, "processDidTerminate: (pid %d), reason=%" PUBLIC_LOG_STRING, legacyMainFrameProcessID(), processTerminationReasonToString(reason).characters());

    resetStateAfterProcessExited(reason);
    stopAllURLSchemeTasks(protectedLegacyMainFrameProcess().ptr());
#if ENABLE(PDF_HUD)
    if (RefPtr pageClient = this->pageClient())
        pageClient->removeAllPDFHUDs();
#endif
#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    if (RefPtr pageClient = this->pageClient())
        pageClient->removeAnyPDFPageNumberIndicator();
#endif

    if (reason != ProcessTerminationReason::NavigationSwap) {
        // For bringup of process swapping, NavigationSwap termination will not go out to clients.
        // If it does *during* process swapping, and the client triggers a reload, that causes bizarre WebKit re-entry.
        // FIXME: This might have to change
        m_navigationState->clearAllNavigations();

        if (m_controlledByAutomation) {
            if (RefPtr automationSession = configuration().processPool().automationSession())
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
    case ProcessTerminationReason::RequestedByModelProcess:
    case ProcessTerminationReason::Crash:
    case ProcessTerminationReason::Unresponsive:
        return true;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::IdleExit:
    case ProcessTerminationReason::RequestedByClient:
    case ProcessTerminationReason::GPUProcessCrashedTooManyTimes:
    case ProcessTerminationReason::ModelProcessCrashedTooManyTimes:
    case ProcessTerminationReason::NonMainFrameWebContentProcessCrash:
        break;
    }
    return false;
}

void WebPageProxy::dispatchProcessDidTerminate(WebProcessProxy& process, ProcessTerminationReason reason)
{
    WEBPAGEPROXY_RELEASE_LOG_ERROR(Loading, "dispatchProcessDidTerminate: reason=%" PUBLIC_LOG_STRING, processTerminationReasonToString(reason).characters());

    if (protectedPreferences()->siteIsolationEnabled())
        protectedBrowsingContextGroup()->processDidTerminate(*this, process);

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
        if (resetStateReason == ResetStateReason::NavigationSwap) {
            // Keep layers around in frozen state to avoid flashing during process swaps.
            if (RefPtr drawingAreaProxy = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea))
                m_frozenRemoteLayerTreeHost = drawingAreaProxy->detachRemoteLayerTreeHost();
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
        RefPtr { m_fullScreenManager }->detachFromClient();
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
    internals().protectedGeolocationPermissionRequestManager()->invalidateRequests();
#endif

    setToolTip({ });

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    internals().mainFramePinnedState = { true, true, true, true };

    internals().visibleScrollerThumbRect = IntRect();

    internals().needsFixedContainerEdgesUpdateAfterNextCommit = false;

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

    internals().allowsLayoutViewportHeightExpansion = true;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->removeAllPlaybackTargetPickerClients(internals());
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
    m_webAuthnCredentialsMessenger = nullptr;
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    m_webDeviceOrientationUpdateProviderProxy = nullptr;
#endif

    for (Ref editCommand : std::exchange(m_editCommandSet, { }))
        editCommand->invalidate();

    m_activePopupMenu = nullptr;

    internals().mainFrameMediaState = MediaProducer::IsNotPlaying;
    updatePlayingMediaDidChange();
#if ENABLE(MEDIA_STREAM)
    if (RefPtr userMediaPermissionRequestManager = std::exchange(m_userMediaPermissionRequestManager, nullptr))
        userMediaPermissionRequestManager->disconnectFromPage();
    m_shouldListenToVoiceActivity = false;
    m_mutedCaptureKindsDesiredByWebApp = { };
#endif

#if ENABLE(POINTER_LOCK)
    resetPointerLockState();
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    resetSpeechSynthesizer();
#endif

#if ENABLE(WEB_AUTHN)
    protectedWebsiteDataStore()->protectedAuthenticatorManager()->cancelRequest(m_webPageID, std::nullopt);
#endif

    m_speechRecognitionPermissionManager = nullptr;

#if ENABLE(WEBXR)
    if (RefPtr xrSystem = internals().xrSystem) {
        xrSystem->invalidate();
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
    setMediaCapability(nullptr);
#endif

#if ENABLE(WRITING_TOOLS)
    auto& completionHandlers = internals().completionHandlerForAnimationID;
    for (auto& completionHandler : completionHandlers.values())
        completionHandler(WebCore::TextAnimationRunMode::DoNotRun);
    completionHandlers.clear();
#endif

    m_nowPlayingMetadataObservers.clear();
    m_nowPlayingMetadataObserverForTesting = nullptr;

    if (RefPtr pageClient = this->pageClient())
        pageClient->hasActiveNowPlayingSessionChanged(false);

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
    if (auto modelPresentationManager = modelPresentationManagerProxy())
        modelPresentationManager->invalidateAllModels();
#endif
}

void WebPageProxy::resetStateAfterProcessExited(ProcessTerminationReason terminationReason)
{
    RefPtr protectedPageClient { pageClient() };

#if ASSERT_ENABLED
    // FIXME: It's weird that resetStateAfterProcessExited() is called even though the process is launching.
    if (terminationReason != ProcessTerminationReason::NavigationSwap)
        ASSERT(m_legacyMainFrameProcess->state() == WebProcessProxy::State::Launching || m_legacyMainFrameProcess->state() == WebProcessProxy::State::Terminated);
#endif

#if PLATFORM(IOS_FAMILY)
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = false;
#endif

    resetActivityState();

    internals().pageIsUserObservableCount = nullptr;
    internals().visiblePageToken = nullptr;
    internals().pageAllowedToRunInTheBackgroundActivityDueToTitleChanges = nullptr;
    internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications = nullptr;

    m_areActiveDOMObjectsAndAnimationsSuspended = false;
    m_isServiceWorkerPage = false;

    m_userScriptsNotified = false;
    m_hasActiveAnimatedScroll = false;
    m_registeredForFullSpeedUpdates = false;
    internals().sleepDisablers.clear();

    internals().editorState = EditorState();
    internals().cachedFontAttributesAtSelectionStart.reset();
#if PLATFORM(MAC)
    internals().scrollPositionDuringLastEditorStateUpdate = { };
#endif

    if (terminationReason != ProcessTerminationReason::NavigationSwap)
        m_provisionalPage = nullptr;

    if (terminationReason == ProcessTerminationReason::NavigationSwap)
        protectedPageClient->processWillSwap();
    else
        protectedPageClient->processDidExit();

    protectedPageClient->clearAllEditCommands();

#if PLATFORM(COCOA)
    WebPasteboardProxy::singleton().revokeAccess(m_legacyMainFrameProcess.get());
#endif

    auto resetStateReason = terminationReason == ProcessTerminationReason::NavigationSwap ? ResetStateReason::NavigationSwap : ResetStateReason::WebProcessExited;
    resetState(resetStateReason);

    m_pendingLearnOrIgnoreWordMessageCount = 0;

#if ENABLE(MAC_GESTURE_EVENTS)
    internals().droppedGestureEventCount = 0;
    internals().gestureEventQueue.clear();
#endif

    internals().mouseEventQueue.clear();
    internals().coalescedMouseEvents.clear();
    internals().keyEventQueue.clear();
    if (m_wheelEventCoalescer)
        m_wheelEventCoalescer->clear();

#if ENABLE(ATTACHMENT_ELEMENT)
    invalidateAllAttachments();
#endif

#if PLATFORM(COCOA)
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->resetStateAfterProcessExited();
#endif

    if (terminationReason != ProcessTerminationReason::NavigationSwap) {
        Ref protectedPageLoadState = pageLoadState();
        auto transaction = protectedPageLoadState->transaction();
        protectedPageLoadState->reset(transaction);
    }

#if ENABLE(VIDEO_PRESENTATION_MODE)
    internals().fullscreenVideoTextRecognitionTimer.stop();
    internals().currentFullscreenVideoSessionIdentifier = std::nullopt;
#endif

#if ENABLE(GAMEPAD)
    resetRecentGamepadAccessState();
#endif

    // FIXME: <rdar://problem/38676604> In case of process swaps, the old process should gracefully suspend instead of terminating.
    protectedLegacyMainFrameProcess()->processTerminated();

#if ENABLE(MODEL_PROCESS)
    m_hasModelElement = false;
#endif
}

WebPageProxyTesting* WebPageProxy::pageForTesting() const
{
    return m_pageForTesting.get();
}

RefPtr<WebPageProxyTesting> WebPageProxy::protectedPageForTesting() const
{
    return m_pageForTesting;
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

#if !PLATFORM(COCOA)
bool WebPageProxy::useGPUProcessForDOMRenderingEnabled() const
{
    return protectedPreferences()->useGPUProcessForDOMRenderingEnabled();
}
#endif

WebPageCreationParameters WebPageProxy::creationParameters(WebProcessProxy& process, DrawingAreaProxy& drawingArea, WebCore::FrameIdentifier mainFrameIdentifier, std::optional<RemotePageParameters>&& remotePageParameters, bool isProcessSwap)
{
    if (m_sessionStateWasRestoredByAPIRequest)
        m_backForwardList->setItemsAsRestoredFromSession();

    RefPtr pageClient = this->pageClient();

    WebPageCreationParameters parameters {
        .drawingAreaIdentifier = drawingArea.identifier(),
        .webPageProxyIdentifier = identifier(),
        .pageGroupData = m_pageGroup->data(),
        .visitedLinkTableID = m_visitedLinkStore->identifier(),
        .userContentControllerParameters = m_userContentController->parametersForProcess(process),
        .mainFrameIdentifier = mainFrameIdentifier,
        .openedMainFrameName = m_openedMainFrameName,
        .initialSandboxFlags = m_mainFrame ? m_mainFrame->effectiveSandboxFlags() : SandboxFlags { },
        .initialReferrerPolicy = m_mainFrame ? m_mainFrame->effectiveReferrerPolicy() : ReferrerPolicy::EmptyString,
        .statusBarIsVisible = m_statusBarIsVisible,
        .menuBarIsVisible = m_menuBarIsVisible,
        .toolbarsAreVisible = m_toolbarsAreVisible,
        .shouldSendConsoleLogsToUIProcessForTesting = m_configuration->shouldSendConsoleLogsToUIProcessForTesting(),
    };

    parameters.processDisplayName = m_configuration->processDisplayName();

    parameters.remotePageParameters = WTFMove(remotePageParameters);
    parameters.mainFrameOpenerIdentifier = m_mainFrame && m_mainFrame->opener() ? std::optional(m_mainFrame->opener()->frameID()) : std::nullopt;
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
    parameters.hardwareKeyboardState = m_configuration->processPool().cachedHardwareKeyboardState();
    parameters.canShowWhileLocked = m_configuration->canShowWhileLocked();
    parameters.insertionPointColor = pageClient ? pageClient->insertionPointColor() : WebCore::Color { };
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    parameters.gamepadAccessRequiresExplicitConsent = m_configuration->gamepadAccessRequiresExplicitConsent();
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
            parameters.renderServerMachExtensionHandle = WTFMove(*handle);
    }
#endif // ENABLE(TILED_CA_DRAWING_AREA)

#if HAVE(STATIC_FONT_REGISTRY)
    if (preferences->shouldAllowUserInstalledFonts()) {
#if ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)
        process.protectedProcessPool()->registerUserInstalledFonts(process);
#else
        if (auto handles = process.fontdMachExtensionHandles())
            parameters.fontMachExtensionHandles = WTFMove(*handles);
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

    parameters.httpsUpgradeEnabled = preferences->upgradeKnownHostsToHTTPSEnabled() ? m_configuration->httpsUpgradeEnabled() : false;
    parameters.allowPostingLegacySynchronousMessages = m_configuration->allowPostingLegacySynchronousMessages();

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

#if USE(GBM) && (PLATFORM(GTK) || PLATFORM(WPE))
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

    return parameters;
}

WebPageCreationParameters WebPageProxy::creationParametersForProvisionalPage(WebProcessProxy& process, DrawingAreaProxy& drawingArea, WebCore::FrameIdentifier mainFrameIdentifier)
{
    constexpr bool isProcessSwap = true;
    return creationParameters(process, drawingArea, mainFrameIdentifier, std::nullopt, isProcessSwap);
}

WebPageCreationParameters WebPageProxy::creationParametersForRemotePage(WebProcessProxy& process, DrawingAreaProxy& drawingArea, RemotePageParameters&& remotePageParameters)
{
    constexpr bool isProcessSwap = true;
    return creationParameters(process, drawingArea, m_mainFrame->frameID(), WTFMove(remotePageParameters), isProcessSwap);
}

void WebPageProxy::isJITEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    launchInitialProcessIfNecessary();
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebProcess::IsJITEnabled(), WTFMove(completionHandler), 0);
}

void WebPageProxy::isEnhancedSecurityEnabled(CompletionHandler<void(bool)>&& completionHandler)
{
    if (protectedLegacyMainFrameProcess()->isDummyProcessProxy())
        return completionHandler(false);

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebProcess::IsEnhancedSecurityEnabled(), WTFMove(completionHandler), 0);
}

void WebPageProxy::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    ASSERT(m_drawingArea->type() == DrawingAreaType::TiledCoreAnimation);
#endif
    if (RefPtr pageClient = this->pageClient())
        pageClient->enterAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::didFirstLayerFlush(const LayerTreeContext& layerTreeContext)
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    ASSERT(m_drawingArea->type() == DrawingAreaType::TiledCoreAnimation);
#endif
    if (RefPtr pageClient = this->pageClient())
        pageClient->didFirstLayerFlush(layerTreeContext);

    if (RefPtr lastSuspendedPage = m_lastSuspendedPage.get())
        lastSuspendedPage->pageDidFirstLayerFlush();
    m_suspendedPageKeptToPreventFlashing = nullptr;
}

void WebPageProxy::exitAcceleratedCompositingMode()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->exitAcceleratedCompositingMode();
}

void WebPageProxy::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->updateAcceleratedCompositingMode(layerTreeContext);
}

#if ENABLE(GAMEPAD)

void WebPageProxy::gamepadActivity(const Vector<std::optional<GamepadData>>& gamepadDatas, EventMakesGamepadsVisible eventVisibility)
{
    send(Messages::WebPage::GamepadActivity(gamepadDatas, eventVisibility));
}

void WebPageProxy::recentGamepadAccessStateChanged(PAL::HysteresisState state)
{
    RefPtr pageClient = this->pageClient();
    switch (state) {
    case PAL::HysteresisState::Started:
        if (pageClient)
            pageClient->setGamepadsRecentlyAccessed(PageClient::GamepadsRecentlyAccessed::Yes);
        m_uiClient->recentlyAccessedGamepadsForTesting(*this);
        break;
    case PAL::HysteresisState::Stopped:
        if (pageClient)
            pageClient->setGamepadsRecentlyAccessed(PageClient::GamepadsRecentlyAccessed::No);
        m_uiClient->stoppedAccessingGamepadsForTesting(*this);
    }
}

void WebPageProxy::gamepadsRecentlyAccessed()
{
    // FIXME: We'd like to message_check here to validate the process should be allowed
    // to refresh the "recently using gamepads" state.
    // We could check our "set of processes using gamepads" but it is already driven
    // by web process messages, therefore a compromised WebProcess can add itself.
    // Is there something meaningful we can do here?

    m_internals->recentGamepadAccessHysteresis.impulse();
}

void WebPageProxy::resetRecentGamepadAccessState()
{
    if (m_internals->recentGamepadAccessHysteresis.state() == PAL::HysteresisState::Started)
        recentGamepadAccessStateChanged(PAL::HysteresisState::Stopped);

    m_internals->recentGamepadAccessHysteresis.cancel();
}

#if PLATFORM(VISION)

void WebPageProxy::setGamepadsConnected(bool gamepadsConnected)
{
    if (m_gamepadsConnected == gamepadsConnected)
        return;

    m_gamepadsConnected = gamepadsConnected;
    if (auto pageClient = this->pageClient())
        pageClient->gamepadsConnectedStateChanged();
}

void WebPageProxy::allowGamepadAccess()
{
    send(Messages::WebPage::AllowGamepadAccess());
}

#endif // PLATFORM(VISION)

#endif // ENABLE(GAMEPAD)

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
    Ref protectedPageLoadState = pageLoadState();
    auto transaction = protectedPageLoadState->transaction();
    protectedPageLoadState->negotiatedLegacyTLS(transaction);
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
    WEBPAGEPROXY_RELEASE_LOG(Storage, "requestStorageSpace for frame %" PRIu64 ", current quota %" PRIu64 " current usage %" PRIu64 " expected usage %" PRIu64, frameID.toUInt64(), currentQuota, currentDatabaseUsage, expectedUsage);

    StorageRequests::singleton().processOrAppend([this, protectedThis = Ref { *this }, pageURL = currentURL(), frameID, originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, completionHandler = WTFMove(completionHandler)] mutable {
        this->makeStorageSpaceRequest(frameID, originIdentifier, databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, [this, protectedThis = Ref { *this }, frameID, pageURL = WTFMove(pageURL), completionHandler = WTFMove(completionHandler), currentQuota](auto quota) mutable {

            WEBPAGEPROXY_RELEASE_LOG(Storage, "requestStorageSpace response for frame %" PRIu64 ", quota %" PRIu64, frameID.toUInt64(), quota);
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
    MESSAGE_CHECK_COMPLETION(m_legacyMainFrameProcess, frame, completionHandler(0));

    auto originData = SecurityOriginData::fromDatabaseIdentifier(originIdentifier);
    if (originData != SecurityOriginData::fromURLWithoutStrictOpaqueness(URL { currentURL() })) {
        completionHandler(currentQuota);
        return;
    }

    Ref origin = API::SecurityOrigin::create(originData->securityOrigin());
    m_uiClient->exceededDatabaseQuota(this, frame.get(), origin.ptr(), databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, WTFMove(completionHandler));
}

void WebPageProxy::requestGeolocationPermissionForFrame(IPC::Connection& connection, GeolocationIdentifier geolocationID, FrameInfoData&& frameInfo)
{
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return;

    auto request = internals().protectedGeolocationPermissionRequestManager()->createRequest(geolocationID, frame->protectedProcess());
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
    if (RefPtr pageClient = this->pageClient(); completionHandler && pageClient)
        pageClient->decidePolicyForGeolocationPermissionRequest(*frame, frameInfo, completionHandler);
#endif
    if (completionHandler)
        completionHandler(false);
}

void WebPageProxy::revokeGeolocationAuthorizationToken(const String& authorizationToken)
{
    internals().protectedGeolocationPermissionRequestManager()->revokeAuthorizationToken(authorizationToken);
}

void WebPageProxy::queryPermission(const ClientOrigin& clientOrigin, const PermissionDescriptor& descriptor, CompletionHandler<void(std::optional<PermissionState>)>&& completionHandler)
{
    bool canAPISucceed = true;
    bool shouldChangeDeniedToPrompt = true;
    bool shouldChangePromptToGrant = false;
    String name;

#if ENABLE(WEB_ARCHIVE)
    if (didLoadWebArchive()) {
        completionHandler(PermissionState::Denied);
        return;
    }
#endif

    if (descriptor.name == PermissionName::Camera) {
#if ENABLE(MEDIA_STREAM)
        Ref protectedUserMediaPermissionRequestManager = userMediaPermissionRequestManager();
        name = "camera"_s;
        canAPISucceed = protectedUserMediaPermissionRequestManager->canVideoCaptureSucceed();
        shouldChangeDeniedToPrompt = protectedUserMediaPermissionRequestManager->shouldChangeDeniedToPromptForCamera(clientOrigin);
        shouldChangePromptToGrant = protectedUserMediaPermissionRequestManager->shouldChangePromptToGrantForCamera(clientOrigin);
#endif
    } else if (descriptor.name == PermissionName::Microphone) {
#if ENABLE(MEDIA_STREAM)
        Ref protectedUserMediaPermissionRequestManager = userMediaPermissionRequestManager();
        name = "microphone"_s;
        canAPISucceed = protectedUserMediaPermissionRequestManager->canAudioCaptureSucceed();
        shouldChangeDeniedToPrompt = protectedUserMediaPermissionRequestManager->shouldChangeDeniedToPromptForMicrophone(clientOrigin);
        shouldChangePromptToGrant = protectedUserMediaPermissionRequestManager->shouldChangePromptToGrantForMicrophone(clientOrigin);
#endif
    } else if (descriptor.name == PermissionName::Geolocation) {
#if ENABLE(GEOLOCATION)
        name = "geolocation"_s;

        // The decision to change denied to prompt is made directly in the WebProcess.
        // (See the Permissions API code).
        shouldChangeDeniedToPrompt = false;
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
UserMediaPermissionRequestManagerProxy* WebPageProxy::userMediaPermissionRequestManagerIfExists()
{
    return m_userMediaPermissionRequestManager.get();
}

UserMediaPermissionRequestManagerProxy& WebPageProxy::userMediaPermissionRequestManager()
{
    if (!m_userMediaPermissionRequestManager)
        m_userMediaPermissionRequestManager = UserMediaPermissionRequestManagerProxy::create(*this);
    return *m_userMediaPermissionRequestManager;
}

Ref<UserMediaPermissionRequestManagerProxy> WebPageProxy::protectedUserMediaPermissionRequestManager()
{
    return userMediaPermissionRequestManager();
}

void WebPageProxy::clearUserMediaPermissionRequestHistory(WebCore::PermissionName name)
{
    if (RefPtr protectedUserMediaPermissionRequestManager = m_userMediaPermissionRequestManager)
        protectedUserMediaPermissionRequestManager->clearUserMediaPermissionRequestHistory(name);
}

void WebPageProxy::setMockCaptureDevicesEnabledOverride(std::optional<bool> enabled)
{
    protectedUserMediaPermissionRequestManager()->setMockCaptureDevicesEnabledOverride(enabled);
}

void WebPageProxy::willStartCapture(UserMediaPermissionRequestProxy& request, CompletionHandler<void()>&& callback)
{
    if (auto beforeStartingCaptureCallback = request.beforeStartingCaptureCallback())
        beforeStartingCaptureCallback();

    switch (request.requestType()) {
    case WebCore::MediaStreamRequest::Type::UserMedia:
        if (request.userRequest().audioConstraints.isValid)
            m_mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Microphone);
        if (request.userRequest().videoConstraints.isValid)
            m_mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Camera);
        break;
    case WebCore::MediaStreamRequest::Type::DisplayMediaWithAudio:
        m_mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::SystemAudio);
        [[fallthrough]];
    case WebCore::MediaStreamRequest::Type::DisplayMedia:
        m_mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Display);
        break;
    }

    activateMediaStreamCaptureInPage();

#if ENABLE(GPU_PROCESS)
    Ref preferences = m_preferences;
    if (!preferences->captureVideoInGPUProcessEnabled() && !preferences->captureAudioInGPUProcessEnabled())
        return callback();

    Ref gpuProcess = configuration().protectedProcessPool()->ensureGPUProcess();
#if PLATFORM(IOS_FAMILY)
    gpuProcess->setOrientationForMediaCapture(m_orientationForMediaCapture);
#endif

    if (RefPtr frame = WebFrameProxy::webFrame(request.frameID())) {
        auto webProcessIdentifier = frame->process().coreProcessIdentifier();
        gpuProcess->updateCaptureAccess(request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture(), webProcessIdentifier, identifier(), WTFMove(callback));
        gpuProcess->updateCaptureOrigin(request.topLevelDocumentSecurityOrigin().data(), webProcessIdentifier);
    } else
        return callback();
#else
    callback();
#endif
}

void WebPageProxy::microphoneMuteStatusChanged(bool isMuting)
{
    // We are updating both the internal and web app muting states so that only microphone changes, and not camera or screenshare.
    auto mutedState = internals().mutedState;
    if (isMuting) {
        mutedState.add(WebCore::MediaProducerMutedState::AudioCaptureIsMuted);
        m_mutedCaptureKindsDesiredByWebApp.add(WebCore::MediaProducerMediaCaptureKind::Microphone);
    } else {
        WebProcessProxy::muteCaptureInPagesExcept(m_webPageID);

        mutedState.remove(WebCore::MediaProducerMutedState::AudioCaptureIsMuted);
        m_mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Microphone);
    }

    setMuted(mutedState, FromApplication::No);
}

void WebPageProxy::requestUserMediaPermissionForFrame(IPC::Connection& connection, UserMediaRequestIdentifier userMediaID, FrameInfoData&& frameInfo, const SecurityOriginData& userMediaDocumentOriginData, const SecurityOriginData& topLevelDocumentOriginData, MediaStreamRequest&& request)
{
    MESSAGE_CHECK_BASE(WebFrameProxy::webFrame(frameInfo.frameID), connection);
#if PLATFORM(MAC)
    CoreAudioCaptureDeviceManager::singleton().setFilterTapEnabledDevices(!protectedPreferences()->captureAudioInGPUProcessEnabled());
#endif
    protectedUserMediaPermissionRequestManager()->requestUserMediaPermissionForFrame(userMediaID, WTFMove(frameInfo), userMediaDocumentOriginData.securityOrigin(), topLevelDocumentOriginData.securityOrigin(), WTFMove(request));
}

void WebPageProxy::enumerateMediaDevicesForFrame(IPC::Connection& connection, FrameIdentifier frameID, const SecurityOriginData& userMediaDocumentOriginData, const SecurityOriginData& topLevelDocumentOriginData, CompletionHandler<void(const Vector<CaptureDeviceWithCapabilities>&, MediaDeviceHashSalts&&)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return completionHandler({ }, { });

    protectedUserMediaPermissionRequestManager()->enumerateMediaDevicesForFrame(frameID, userMediaDocumentOriginData.securityOrigin(), topLevelDocumentOriginData.securityOrigin(), WTFMove(completionHandler));
}

void WebPageProxy::beginMonitoringCaptureDevices()
{
    protectedUserMediaPermissionRequestManager()->syncWithWebCorePrefs();
    UserMediaProcessManager::singleton().beginMonitoringCaptureDevices();
}

static WebCore::MediaStreamRequest toUserMediaRequest(WebCore::MediaProducerMediaCaptureKind kind, WebCore::PageIdentifier pageIdentifier)
{
    switch (kind) {
    case WebCore::MediaProducerMediaCaptureKind::Microphone:
        return { MediaStreamRequest::Type::UserMedia, { { }, { }, true }, { }, true, pageIdentifier };
    case WebCore::MediaProducerMediaCaptureKind::Camera:
        return { MediaStreamRequest::Type::UserMedia, { }, { { }, { }, true }, true, pageIdentifier };
    case WebCore::MediaProducerMediaCaptureKind::Display:
    case WebCore::MediaProducerMediaCaptureKind::SystemAudio:
    case WebCore::MediaProducerMediaCaptureKind::EveryKind:
        ASSERT_NOT_REACHED();
    }
    return { };
}

class ValidateCaptureStateUpdateCallbackHandler final : public RefCounted<ValidateCaptureStateUpdateCallbackHandler> {
public:
    using Callback = Function<void(bool)>;
    static Ref<ValidateCaptureStateUpdateCallbackHandler> create(Callback&& callback) { return adoptRef(*new ValidateCaptureStateUpdateCallbackHandler(WTFMove(callback))); }

    ~ValidateCaptureStateUpdateCallbackHandler()
    {
        handle(false);
    }

    void handle(bool value)
    {
        if (auto callback = std::exchange(m_callback, { }))
            callback(value);
    }

private:
    explicit ValidateCaptureStateUpdateCallbackHandler(Callback&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    Callback m_callback;
};

void WebPageProxy::validateCaptureStateUpdate(WebCore::UserMediaRequestIdentifier requestIdentifier, WebCore::ClientOrigin&& clientOrigin, FrameInfoData&& frameInfo, bool isActive, WebCore::MediaProducerMediaCaptureKind kind, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    WEBPAGEPROXY_RELEASE_LOG(WebRTC, "validateCaptureStateUpdate: isActive=%d kind=%hhu", isActive, kind);
    RefPtr webFrame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!webFrame) {
        completionHandler(WebCore::Exception { ExceptionCode::InvalidStateError, "no frame available"_s });
        return;
    }

    if (!isActive) {
        m_mutedCaptureKindsDesiredByWebApp.add(kind);
        completionHandler({ });
        return;
    }

    auto requestPermission = [&] (auto kind, auto completionHandler) {
        auto responseHandler = ValidateCaptureStateUpdateCallbackHandler::create([completionHandler = WTFMove(completionHandler)] (bool result) mutable {
            if (!result) {
                completionHandler(Exception { ExceptionCode::NotAllowedError, "Capture access is denied"_s });
                return;
            }
            completionHandler({ });
        });

        Vector<WebCore::CaptureDevice> audioDevices, videoDevices;
        if (kind == WebCore::MediaProducerMediaCaptureKind::Camera)
            videoDevices = RealtimeMediaSourceCenter::singleton().videoCaptureFactory().videoCaptureDeviceManager().captureDevices();
        else if (kind == WebCore::MediaProducerMediaCaptureKind::Microphone)
            audioDevices = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().audioCaptureDeviceManager().captureDevices();
        Ref<UserMediaPermissionRequestProxy> request = UserMediaPermissionRequestProxy::create(protectedUserMediaPermissionRequestManager(), requestIdentifier, mainFrame()->frameID(), WTFMove(frameInfo), clientOrigin.clientOrigin.securityOrigin(), clientOrigin.topOrigin.securityOrigin(), WTFMove(audioDevices), WTFMove(videoDevices), toUserMediaRequest(kind, webPageIDInMainFrameProcess()), [responseHandler] (bool result) mutable {
            responseHandler->handle(result);
        });
        request->setBeforeStartingCaptureCallback([responseHandler] () mutable {
            responseHandler->handle(true);
        });

        Ref userMediaOrigin = API::SecurityOrigin::create(request->userMediaDocumentSecurityOrigin());
        Ref topLevelOrigin = API::SecurityOrigin::create(request->topLevelDocumentSecurityOrigin());
        // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once rdar://144557500 is resolved.
        uiClient().decidePolicyForUserMediaPermissionRequest(*this, *webFrame, userMediaOrigin, topLevelOrigin, request.get());
    };


    WebCore::MediaProducerMutedStateFlags mutedState = internals().mutedState;
    switch (kind) {
    case WebCore::MediaProducerMediaCaptureKind::Microphone:
        if (mutedState.contains(WebCore::MediaProducerMutedState::AudioCaptureIsMuted)) {
            requestPermission(kind, WTFMove(completionHandler));
            return;
        }
        break;
    case WebCore::MediaProducerMediaCaptureKind::Camera:
        if (mutedState.contains(WebCore::MediaProducerMutedState::VideoCaptureIsMuted)) {
            requestPermission(kind, WTFMove(completionHandler));
            return;
        }
        break;
    case WebCore::MediaProducerMediaCaptureKind::Display:
        if (mutedState.containsAny({ WebCore::MediaProducerMutedState::ScreenCaptureIsMuted, WebCore::MediaProducerMutedState::WindowCaptureIsMuted })) {
            Ref userMediaOrigin = API::SecurityOrigin::create(clientOrigin.clientOrigin.securityOrigin().get());
            Ref topLevelOrigin = API::SecurityOrigin::create(clientOrigin.topOrigin.securityOrigin().get());

            uiClient().decidePolicyForScreenCaptureUnmuting(*this, *webFrame, WTFMove(frameInfo), WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (bool isAllowed) mutable {
                if (!isAllowed) {
                    completionHandler(Exception { ExceptionCode::NotAllowedError, "Screen capture access is denied"_s });
                    return;
                }

                completionHandler({ });
                RefPtr page = weakThis.get();
                if (!page)
                    return;
                page->m_mutedCaptureKindsDesiredByWebApp.remove(WebCore::MediaProducerMediaCaptureKind::Display);
                page->setMediaStreamCaptureMuted(false);
            });
            return;
        }
        break;
    case WebCore::MediaProducerMediaCaptureKind::SystemAudio:
    case WebCore::MediaProducerMediaCaptureKind::EveryKind:
        ASSERT_NOT_REACHED();
    }

    m_mutedCaptureKindsDesiredByWebApp.remove(kind);
    completionHandler({ });
}

void WebPageProxy::setShouldListenToVoiceActivity(bool value)
{
    m_shouldListenToVoiceActivity = value;
#if ENABLE(GPU_PROCESS)
    RefPtr gpuProcess = m_configuration->processPool().gpuProcess();
    if (gpuProcess && protectedPreferences()->captureAudioInGPUProcessEnabled())
        gpuProcess->setShouldListenToVoiceActivity(*this, m_shouldListenToVoiceActivity);
#endif
}

void WebPageProxy::voiceActivityDetected()
{
    send(Messages::WebPage::VoiceActivityDetected { });
}

void WebPageProxy::startMonitoringCaptureDeviceRotation(const String& persistentId)
{
#if HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
    if (!m_preferences->useAVCaptureDeviceRotationCoordinatorAPI())
        return;

    userMediaPermissionRequestManager().startMonitoringCaptureDeviceRotation(persistentId);
#endif // HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
}

void WebPageProxy::stopMonitoringCaptureDeviceRotation(const String& persistentId)
{
#if HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
    if (!m_preferences->useAVCaptureDeviceRotationCoordinatorAPI())
        return;

    userMediaPermissionRequestManager().stopMonitoringCaptureDeviceRotation(persistentId);
#endif // HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
}

void WebPageProxy::rotationAngleForCaptureDeviceChanged(const String& persistentId, WebCore::VideoFrameRotation rotation)
{
#if HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
    if (!preferences().useAVCaptureDeviceRotationCoordinatorAPI())
        return;

#if ENABLE(GPU_PROCESS)
    if (preferences().captureVideoInGPUProcessEnabled()) {
        if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
            gpuProcess->rotationAngleForCaptureDeviceChanged(persistentId, rotation);
        return;
    }
#endif // ENABLE(GPU_PROCESS)
#endif // HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
}
#endif // ENABLE(MEDIA_STREAM)

void WebPageProxy::syncIfMockDevicesEnabledChanged()
{
#if ENABLE(MEDIA_STREAM)
    protectedUserMediaPermissionRequestManager()->syncWithWebCorePrefs();
#endif
}

void WebPageProxy::clearUserMediaState()
{
#if ENABLE(MEDIA_STREAM)
    if (RefPtr userMediaPermissionRequestManager = m_userMediaPermissionRequestManager)
        userMediaPermissionRequestManager->clearCachedState();
#endif
}

void WebPageProxy::requestMediaKeySystemPermissionForFrame(IPC::Connection& connection, MediaKeySystemRequestIdentifier mediaKeySystemID, FrameIdentifier frameID, WebCore::ClientOrigin&& clientOrigin, const String& keySystem)
{
#if ENABLE(ENCRYPTED_MEDIA)
    MESSAGE_CHECK_BASE(WebFrameProxy::webFrame(frameID), connection);

    Ref origin = API::SecurityOrigin::create(clientOrigin.topOrigin.securityOrigin());
    protectedMediaKeySystemPermissionRequestManager()->createRequestForFrame(mediaKeySystemID, frameID, clientOrigin.clientOrigin.securityOrigin(), clientOrigin.topOrigin.securityOrigin(), keySystem, [weakThis = WeakPtr { *this }, origin = WTFMove(origin), keySystem = keySystem] (auto request) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_uiClient->decidePolicyForMediaKeySystemPermissionRequest(*protectedThis, origin, keySystem, [request = WTFMove(request)](bool allowed) {
            if (allowed)
                request->allow();
            else
                request->deny();
        });
    });
#else
    UNUSED_PARAM(mediaKeySystemID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(clientOrigin);
    UNUSED_PARAM(keySystem);
#endif
}

#if ENABLE(DEVICE_ORIENTATION)

void WebPageProxy::shouldAllowDeviceOrientationAndMotionAccess(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return completionHandler(DeviceOrientationOrMotionPermissionState::Denied);

    protectedWebsiteDataStore()->protectedDeviceOrientationAndMotionAccessController()->shouldAllowAccess(*this, *frame, WTFMove(frameInfo), mayPrompt, WTFMove(completionHandler));
}

#endif


#if ENABLE(IMAGE_ANALYSIS)

void WebPageProxy::requestTextRecognition(const URL& imageURL, ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completionHandler)
{
    protectedPageClient()->requestTextRecognition(imageURL, WTFMove(imageData), sourceLanguageIdentifier, targetLanguageIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&& completion)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completion(false);
    pageClient->computeHasVisualSearchResults(imageURL, imageBitmap, WTFMove(completion));
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
    m_internals->imageTranslationLanguageIdentifiers = { sourceLanguageIdentifier, targetLanguageIdentifier };

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (hasRunningProcess())
            webProcess.send(Messages::WebPage::StartVisualTranslation(sourceLanguageIdentifier, targetLanguageIdentifier), pageID);
    });
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

    lazyInitialize(m_mediaKeySystemPermissionRequestManager, makeUniqueWithoutRefCountedCheck<MediaKeySystemPermissionRequestManagerProxy>(*this));
    return *m_mediaKeySystemPermissionRequestManager;
}

Ref<MediaKeySystemPermissionRequestManagerProxy> WebPageProxy::protectedMediaKeySystemPermissionRequestManager()
{
    return mediaKeySystemPermissionRequestManager();
}
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

void WebPageProxy::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, const FrameInfoData& frameInfo, HTMLMediaElementIdentifier identifier, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->showMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), frameInfo, identifier, WTFMove(completionHandler));
}
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(NOTIFICATIONS)
void WebPageProxy::clearNotificationPermissionState()
{
    internals().notificationPermissionRequesters.clear();
    if (RefPtr pageForTesting = m_pageForTesting)
        pageForTesting->clearNotificationPermissionState();
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
    if (!internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications)
        internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications = legacyMainFrameProcess().protectedThrottler()->backgroundActivity("Page is likely to show notifications"_s);
}

void WebPageProxy::showNotification(IPC::Connection& connection, const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources>&& notificationResources)
{
    m_configuration->protectedProcessPool()->protectedSupplement<WebNotificationManagerProxy>()->show(this, connection, notificationData, WTFMove(notificationResources));
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "showNotification: This page shows notifications and is allowed to run in the background");
    if (!internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications)
        internals().pageAllowedToRunInTheBackgroundActivityDueToNotifications = WebProcessProxy::fromConnection(connection)->protectedThrottler()->backgroundActivity("Page has shown notification"_s);
}

void WebPageProxy::cancelNotification(const WTF::UUID& notificationID)
{
    m_configuration->protectedProcessPool()->protectedSupplement<WebNotificationManagerProxy>()->cancel(this, notificationID);
}

void WebPageProxy::clearNotifications(const Vector<WTF::UUID>& notificationIDs)
{
    m_configuration->protectedProcessPool()->protectedSupplement<WebNotificationManagerProxy>()->clearNotifications(this, notificationIDs);
}

void WebPageProxy::didDestroyNotification(const WTF::UUID& notificationID)
{
    m_configuration->protectedProcessPool()->protectedSupplement<WebNotificationManagerProxy>()->didDestroyNotification(this, notificationID);
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->drawPageBorderForPrinting(WTFMove(size));
}

void WebPageProxy::runModal()
{
    Ref process = m_legacyMainFrameProcess;
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->recommendedScrollbarStyleDidChange(static_cast<WebCore::ScrollbarStyle>(newStyle));
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
    RefPtr pageClient = this->pageClient();
    if (pageClient)
        pageClient->pinnedStateWillChange();
    internals().mainFramePinnedState = pinnedState;
    if (pageClient)
        pageClient->pinnedStateDidChange();

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

    RefPtr pageClient = this->pageClient();
    if (pageClient)
        pageClient->themeColorWillChange();
    internals().themeColor = themeColor;
    if (pageClient)
        pageClient->themeColorDidChange();
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

    RefPtr pageClient = this->pageClient();
    if (pageClient) {
        if (changesUnderPageBackgroundColor)
            pageClient->underPageBackgroundColorWillChange();
    }

    internals().pageExtendedBackgroundColor = newPageExtendedBackgroundColor;

    if (RefPtr pageClient = this->pageClient()) {
        if (changesUnderPageBackgroundColor)
            pageClient->underPageBackgroundColorDidChange();
    }
}

Color WebPageProxy::sampledPageTopColor() const
{
    return internals().sampledPageTopColor;
}

void WebPageProxy::sampledPageTopColorChanged(const Color& sampledPageTopColor)
{
    if (internals().sampledPageTopColor == sampledPageTopColor)
        return;

    RefPtr pageClient = this->pageClient();
    if (pageClient)
        pageClient->sampledPageTopColorWillChange();
    internals().sampledPageTopColor = sampledPageTopColor;
    if (pageClient)
        pageClient->sampledPageTopColorDidChange();
}

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
std::optional<WebCore::SpatialBackdropSource> WebPageProxy::spatialBackdropSource() const
{
    return internals().spatialBackdropSource;
}

void WebPageProxy::spatialBackdropSourceChanged(std::optional<WebCore::SpatialBackdropSource>&& spatialBackdropSource)
{
    if (internals().spatialBackdropSource == spatialBackdropSource)
        return;

    if (RefPtr pageClient = this->pageClient())
        pageClient->spatialBackdropSourceWillChange();

    internals().spatialBackdropSource = WTFMove(spatialBackdropSource);

    if (RefPtr pageClient = this->pageClient())
        pageClient->spatialBackdropSourceDidChange();
}
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
void WebPageProxy::canEnterImmersiveElementFromURL(const URL& url, CompletionHandler<void(bool)>&& completion)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->canEnterImmersiveElementFromURL(url, WTFMove(completion));
    else
        completion(false);
}
#endif

void WebPageProxy::copyLinkWithHighlight()
{
    send(Messages::WebPage::CopyLinkWithHighlight());
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

void WebPageProxy::updateWebsitePolicies(const API::WebsitePolicies& policies)
{
    forEachWebContentProcess([&] (auto& process, auto pageID) {
        process.send(Messages::WebPage::UpdateWebsitePolicies(policies.dataForProcess(process)), pageID);
    });
}

void WebPageProxy::convertPointToMainFrameCoordinates(WebCore::FloatPoint point, std::optional<WebCore::FrameIdentifier> frameID, CompletionHandler<void(std::optional<WebCore::FloatPoint>)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return completionHandler(std::nullopt);

    RefPtr parent = frame->parentFrame();
    if (!parent)
        return completionHandler(point);

    sendWithAsyncReplyToProcessContainingFrame(parent->frameID(), Messages::WebPage::ContentsToRootViewPoint(frame->frameID(), point), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler), nextFrameID = parent->rootFrame().frameID()](FloatPoint convertedPoint) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(std::nullopt);
        protectedThis->convertPointToMainFrameCoordinates(convertedPoint, nextFrameID, WTFMove(completionHandler));
    });
}

void WebPageProxy::convertRectToMainFrameCoordinates(WebCore::FloatRect rect, std::optional<WebCore::FrameIdentifier> frameID, CompletionHandler<void(std::optional<WebCore::FloatRect>)>&& completionHandler)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return completionHandler(std::nullopt);

    RefPtr parent = frame->parentFrame();
    if (!parent)
        return completionHandler(rect);

    sendWithAsyncReplyToProcessContainingFrame(parent->frameID(), Messages::WebPage::ContentsToRootViewRect(frame->frameID(), rect), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler), nextFrameID = parent->rootFrame().frameID()](FloatRect convertedRect) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(std::nullopt);
        protectedThis->convertRectToMainFrameCoordinates(convertedRect, nextFrameID, WTFMove(completionHandler));
    });
}

Awaitable<std::optional<WebCore::FloatRect>> WebPageProxy::convertRectToMainFrameCoordinates(WebCore::FloatRect rect, std::optional<WebCore::FrameIdentifier> frameID)
{
    co_return co_await AwaitableFromCompletionHandler<std::optional<WebCore::FloatRect>> { [protectedThis = Ref { *this }, rect, frameID] (auto completionHandler) {
        protectedThis->convertRectToMainFrameCoordinates(rect, frameID, WTFMove(completionHandler));
    } };
}

void WebPageProxy::hitTestAtPoint(WebCore::FrameIdentifier frameID, WebCore::FloatPoint point, CompletionHandler<void(std::optional<JSHandleInfo>&&)>&& completionHandler)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::HitTestAtPoint(frameID, point), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (auto&& result) mutable {
        WTF::switchOn(WTFMove(result.variant), [&] (std::monostate) {
            completionHandler(std::nullopt);
        }, [&] (WebKit::NodeHitTestResult::RemoteFrameInfo&& info) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return completionHandler(std::nullopt);
            protectedThis->hitTestAtPoint(info.remoteFrameIdentifier, info.transformedPoint, WTFMove(completionHandler));
        }, [&] (JSHandleInfo&& nodeAndFrame) {
            completionHandler(WTFMove(nodeAndFrame));
        });
    });
}

void WebPageProxy::didFinishLoadingDataForCustomContentProvider(String&& suggestedFilename, std::span<const uint8_t> dataReference)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didFinishLoadingDataForCustomContentProvider(ResourceResponseBase::sanitizeSuggestedFilename(WTFMove(suggestedFilename)), dataReference);
}

void WebPageProxy::backForwardRemovedItem(BackForwardItemIdentifier itemID)
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
    auto frameID = frame->frameID();
    if (m_isPerformingDOMPrintOperation)
        sendToProcessContainingFrame(frameID, Messages::WebPage::BeginPrintingDuringDOMPrintOperation(frameID, printInfo), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        sendToProcessContainingFrame(frameID, Messages::WebPage::BeginPrinting(frameID, printInfo));
}

void WebPageProxy::endPrinting(CompletionHandler<void()>&& callback)
{
    if (!m_isInPrintingMode) {
        callback();
        return;
    }

    m_isInPrintingMode = false;

    if (m_isPerformingDOMPrintOperation)
        protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::EndPrintingDuringDOMPrintOperation(), WTFMove(callback), webPageIDInMainFrameProcess(), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        sendWithAsyncReply(Messages::WebPage::EndPrinting(), WTFMove(callback));
}

std::optional<IPC::Connection::AsyncReplyID> WebPageProxy::computePagesForPrinting(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&& callback)
{
    m_isInPrintingMode = true;
    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::ComputePagesForPrintingDuringDOMPrintOperation(frameID, printInfo), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::ComputePagesForPrinting(frameID, printInfo), WTFMove(callback));
}

#if PLATFORM(COCOA)
std::optional<IPC::Connection::AsyncReplyID> WebPageProxy::drawRectToImage(WebFrameProxy& frame, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& callback)
{
    auto frameID = frame.frameID();

    Ref preferences = this->preferences();
    if (!(preferences->remoteSnapshottingEnabled() && preferences->useGPUProcessForDOMRenderingEnabled())) {
        if (m_isPerformingDOMPrintOperation)
            return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawRectToImageDuringDOMPrintOperation(frameID, printInfo, rect, imageSize), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
        return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawRectToImage(frameID, printInfo, rect, imageSize), WTFMove(callback));
    }

    auto snapshotIdentifier = RemoteSnapshotIdentifier::generate();
    Ref gpuProcess = GPUProcessProxy::getOrCreate();
    auto snapshotCallback = [weakGPUProcess = WeakPtr { gpuProcess }, snapshotIdentifier, callback = WTFMove(callback), rootFrameIdentifier = frameID, imageSize](bool success) mutable {
        RefPtr gpuProcess = weakGPUProcess.get();
        if (!gpuProcess || !gpuProcess->hasConnection()) {
            callback({ });
            return;
        }
        if (!success) {
            gpuProcess->releaseSnapshot(snapshotIdentifier);
            callback({ });
            return;
        }
        gpuProcess->sinkCompletedSnapshotToBitmap(snapshotIdentifier, imageSize, rootFrameIdentifier, WTFMove(callback));
    };

    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPrintingRectToSnapshotDuringDOMPrintOperation(snapshotIdentifier, frameID, printInfo, rect, imageSize), WTFMove(snapshotCallback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPrintingRectToSnapshot(snapshotIdentifier, frameID, printInfo, rect, imageSize), WTFMove(snapshotCallback));
}

std::optional<IPC::Connection::AsyncReplyID> WebPageProxy::drawPagesToPDF(WebFrameProxy& frame, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(API::Data*)>&& callback)
{
    auto frameID = frame.frameID();

    Ref preferences = this->preferences();
    if (!(preferences->remoteSnapshottingEnabled() && preferences->useGPUProcessForDOMRenderingEnabled())) {
        if (m_isPerformingDOMPrintOperation)
            return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPagesToPDFDuringDOMPrintOperation(frameID, printInfo, first, count), toAPIDataSharedBufferCallback(WTFMove(callback)), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
        return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPagesToPDF(frameID, printInfo, first, count),  toAPIDataSharedBufferCallback(WTFMove(callback)));
    }

    auto snapshotIdentifier = RemoteSnapshotIdentifier::generate();
    Ref gpuProcess = GPUProcessProxy::getOrCreate();
    auto snapshotCallback = [weakGPUProcess = WeakPtr { gpuProcess }, snapshotIdentifier, callback = WTFMove(callback), rootFrameIdentifier = frameID](std::optional<FloatSize> result) mutable {
        RefPtr gpuProcess = weakGPUProcess.get();
        if (!gpuProcess || !gpuProcess->hasConnection()) {
            callback(nullptr);
            return;
        }
        if (!result) {
            gpuProcess->releaseSnapshot(snapshotIdentifier);
            callback(nullptr);
            return;
        }
        gpuProcess->sinkCompletedSnapshotToPDF(snapshotIdentifier, *result, rootFrameIdentifier, toAPIDataSharedBufferCallback(WTFMove(callback)));
    };

    if (m_isPerformingDOMPrintOperation)
        return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPrintingPagesToSnapshotDuringDOMPrintOperation(snapshotIdentifier, frameID, printInfo, first, count), WTFMove(snapshotCallback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPrintingPagesToSnapshot(snapshotIdentifier, frameID, printInfo, first, count), WTFMove(snapshotCallback));
}
#elif PLATFORM(GTK)
void WebPageProxy::drawPagesForPrinting(WebFrameProxy& frame, const PrintInfo& printInfo, CompletionHandler<void(std::optional<SharedMemory::Handle>&&, ResourceError&&)>&& callback)
{
    m_isInPrintingMode = true;
    auto frameID = frame.frameID();
    if (m_isPerformingDOMPrintOperation)
        sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPagesForPrintingDuringDOMPrintOperation(frameID, printInfo), WTFMove(callback), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    else
        sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawPagesForPrinting(frameID, printInfo), WTFMove(callback));
}
#endif

#if PLATFORM(COCOA)

void WebPageProxy::drawToPDF(const std::optional<FloatRect>& rect, bool allowTransparentBackground, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }
    Ref preferences = this->preferences();
    if (!(preferences->remoteSnapshottingEnabled() && preferences->useGPUProcessForDOMRenderingEnabled())) {
        sendWithAsyncReply(Messages::WebPage::DrawToPDF(rect, allowTransparentBackground), WTFMove(callback));
        return;
    }

    auto snapshotIdentifier = RemoteSnapshotIdentifier::generate();
    Ref gpuProcess = GPUProcessProxy::getOrCreate();
    sendWithAsyncReply(Messages::WebPage::DrawToSnapshot(rect, allowTransparentBackground, snapshotIdentifier),
        [weakGPUProcess = WeakPtr { gpuProcess }, snapshotIdentifier, callback = WTFMove(callback), rootFrameIdentifier = m_mainFrame->frameID()](std::optional<IntSize> result) mutable {
        RefPtr gpuProcess = weakGPUProcess.get();
        if (!gpuProcess || !gpuProcess->hasConnection()) {
            callback({ });
            return;
        }
        if (!result) {
            gpuProcess->releaseSnapshot(snapshotIdentifier);
            callback({ });
            return;
        }
        gpuProcess->sinkCompletedSnapshotToPDF(snapshotIdentifier, *result, rootFrameIdentifier, WTFMove(callback));
    });
}

#endif // PLATFORM(COCOA)

void WebPageProxy::getPDFFirstPageSize(WebCore::FrameIdentifier frameID, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::GetPDFFirstPageSize(frameID), WTFMove(completionHandler));
}

void WebPageProxy::updateBackingStoreDiscardableState()
{
    ASSERT(hasRunningProcess());

    RefPtr drawingArea = m_drawingArea;
    if (!drawingArea)
        return;

    bool isDiscardable;

    if (!protectedLegacyMainFrameProcess()->isResponsive())
        isDiscardable = false;
    else
        isDiscardable = !protectedPageClient()->isViewWindowActive() || !isViewVisible();

    drawingArea->setBackingStoreIsDiscardable(isDiscardable);
}

void WebPageProxy::saveDataToFileInDownloadsFolder(String&& suggestedFilename, String&& mimeType, URL&& originatingURLString, API::Data& data)
{
    m_uiClient->saveDataToFileInDownloadsFolder(this, ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename), mimeType, originatingURLString, data);
}

void WebPageProxy::savePDFToFileInDownloadsFolder(String&& suggestedFilename, URL&& originatingURL, std::span<const uint8_t> dataReference)
{
    String sanitizedFilename = ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename);
    if (!sanitizedFilename.endsWithIgnoringASCIICase(".pdf"_s))
        return;

    saveDataToFileInDownloadsFolder(WTFMove(sanitizedFilename), "application/pdf"_s, WTFMove(originatingURL), API::Data::create(dataReference).get());
}

void WebPageProxy::setMinimumSizeForAutoLayout(const IntSize& size)
{
    if (internals().minimumSizeForAutoLayout == size)
        return;

    internals().minimumSizeForAutoLayout = size;

    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMinimumSizeForAutoLayout(size));
    protectedDrawingArea()->minimumSizeForAutoLayoutDidChange();

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
    protectedDrawingArea()->sizeToContentAutoSizeMaximumSizeDidChange();

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
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled));
}

void WebPageProxy::toggleAutomaticLinkDetection()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticLinkDetectionEnabled));
}

void WebPageProxy::toggleAutomaticDashSubstitution()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled));
}

void WebPageProxy::toggleSmartLists()
{
    if (TextChecker::isTestingMode())
        TextChecker::setSmartListsEnabled(!TextChecker::state().contains(TextCheckerState::SmartListsEnabled));
}

void WebPageProxy::toggleAutomaticTextReplacement()
{
    if (TextChecker::isTestingMode())
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().contains(TextCheckerState::AutomaticTextReplacementEnabled));
}

#endif

#if USE(DICTATION_ALTERNATIVES)

void WebPageProxy::showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext dictationContext)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->showDictationAlternativeUI(boundingBoxOfDictatedText, dictationContext);
}

void WebPageProxy::removeDictationAlternatives(WebCore::DictationContext dictationContext)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->removeDictationAlternatives(dictationContext);
}

void WebPageProxy::dictationAlternatives(WebCore::DictationContext dictationContext, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler({ });
    completionHandler(protectedPageClient()->dictationAlternatives(dictationContext));
}

#endif

#if PLATFORM(MAC)

void WebPageProxy::substitutionsPanelIsShowing(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(TextChecker::substitutionsPanelIsShowing());
}

Awaitable<void> WebPageProxy::showCorrectionPanel(AlternativeTextType panelType, FloatRect boundingBoxOfReplacedString, String replacedString, String replacementString, Vector<String> alternativeReplacementStrings, FrameIdentifier rootFrameID)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        co_return;

    auto convertedBoundingBox = co_await convertRectToMainFrameCoordinates(boundingBoxOfReplacedString, rootFrameID);
    if (!convertedBoundingBox)
        co_return;

    pageClient->showCorrectionPanel(panelType, *convertedBoundingBox, replacedString, replacementString, alternativeReplacementStrings);
}

void WebPageProxy::dismissCorrectionPanel(ReasonForDismissingAlternativeText reason)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->dismissCorrectionPanel(reason);
}

void WebPageProxy::dismissCorrectionPanelSoon(ReasonForDismissingAlternativeText reason, CompletionHandler<void(String)>&& completionHandler)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler({ });
    completionHandler(protectedPageClient()->dismissCorrectionPanelSoon(reason));
}

void WebPageProxy::recordAutocorrectionResponse(AutocorrectionResponse response, const String& replacedString, const String& replacementString)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->recordAutocorrectionResponse(response, replacedString, replacementString);
}

void WebPageProxy::handleAlternativeTextUIResult(const String& result)
{
    if (!isClosed())
        send(Messages::WebPage::HandleAlternativeTextUIResult(result));
}

void WebPageProxy::setEditableElementIsFocused(bool editableElementIsFocused)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->setEditableElementIsFocused(editableElementIsFocused);
}

#endif // PLATFORM(MAC)

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && USE(SKIA))

RefPtr<ViewSnapshot> WebPageProxy::takeViewSnapshot(std::optional<WebCore::IntRect>&& clipRect)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return nullptr;
    return protectedPageClient()->takeViewSnapshot(WTFMove(clipRect));
}

RefPtr<ViewSnapshot> WebPageProxy::takeViewSnapshot(std::optional<WebCore::IntRect>&& clipRect, ForceSoftwareCapturingViewportSnapshot forceSoftwareCapturing)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return nullptr;
#if PLATFORM(MAC)
    return pageClient->takeViewSnapshot(WTFMove(clipRect), forceSoftwareCapturing);
#else
    return pageClient->takeViewSnapshot(WTFMove(clipRect));
#endif
}

#endif // PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && USE(SKIA))

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

    if (hasRunningProcess())
        protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetScrollbarOverlayStyle(scrollbarStyle), m_webPageID);
}

void WebPageProxy::getWebCryptoMasterKey(CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    m_websiteDataStore->client().webCryptoMasterKey([completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }](std::optional<Vector<uint8_t>>&& key) mutable {
        if (key)
            return completionHandler(WTFMove(key));
        protectedThis->m_navigationClient->legacyWebCryptoMasterKey(protectedThis, WTFMove(completionHandler));
    });

}

void WebPageProxy::wrapCryptoKey(Vector<uint8_t>&& key, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    getWebCryptoMasterKey([key = WTFMove(key), completionHandler = WTFMove(completionHandler)](std::optional<Vector<uint8_t>> && masterKey) mutable {
#if PLATFORM(COCOA)
        if (!masterKey)
            return completionHandler(std::nullopt);
#endif
        Vector<uint8_t> wrappedKey;
        const Vector<uint8_t> blankMasterKey;
        if (wrapSerializedCryptoKey(masterKey.value_or(blankMasterKey), key, wrappedKey))
            return completionHandler(WTFMove(wrappedKey));
        completionHandler(std::nullopt);
    });
}

void WebPageProxy::serializeAndWrapCryptoKey(IPC::Connection& connection, WebCore::CryptoKeyData&& keyData, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    auto key = WebCore::CryptoKey::create(WTFMove(keyData));
    MESSAGE_CHECK_COMPLETION_BASE(key, connection, completionHandler(std::nullopt));
    MESSAGE_CHECK_COMPLETION_BASE(key->isValid(), connection, completionHandler(std::nullopt));
    MESSAGE_CHECK_COMPLETION_BASE(key->algorithmIdentifier() != CryptoAlgorithmIdentifier::DEPRECATED_SHA_224, connection, completionHandler(std::nullopt));

    auto serializedKey = WebCore::SerializedScriptValue::serializeCryptoKey(*key);
    wrapCryptoKey(WTFMove(serializedKey), WTFMove(completionHandler));
}

void WebPageProxy::unwrapCryptoKey(WrappedCryptoKey&& wrappedKey, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    getWebCryptoMasterKey([wrappedKey = WTFMove(wrappedKey), completionHandler = WTFMove(completionHandler)](std::optional<Vector<uint8_t>> && masterKey) mutable {
#if PLATFORM(COCOA)
        if (!masterKey)
            return completionHandler(std::nullopt);
#endif
        const Vector<uint8_t> blankMasterKey;
        if (auto key = WebCore::unwrapCryptoKey(masterKey.value_or(blankMasterKey), wrappedKey))
            return completionHandler(WTFMove(key));
        completionHandler(std::nullopt);
    });

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

void WebPageProxy::getSelectedRangeAsync(CompletionHandler<void(const EditingRange& selectedRange, const EditingRange& compositionRange)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction({ }, { });
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

void WebPageProxy::setWritingSuggestion(const String& text, const EditingRange& selectionRange)
{
    if (!hasRunningProcess()) {
        // If this fails, we should call -discardMarkedText on input context to notify the input method.
        // This will happen naturally later, as part of reloading the page.
        return;
    }

    send(Messages::WebPage::SetWritingSuggestion(text, selectionRange));
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
        m_scrollingPerformanceData = makeUnique<RemoteLayerTreeScrollingPerformanceData>(Ref { downcast<RemoteLayerTreeDrawingAreaProxy>(*m_drawingArea) });
    else if (!m_scrollPerformanceDataCollectionEnabled)
        m_scrollingPerformanceData = nullptr;
}
#endif

void WebPageProxy::takeSnapshotLegacy(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& callback)
{
    options.remove(SnapshotOption::Accelerated);
    options.remove(SnapshotOption::AllowHDR);
    sendWithAsyncReply(Messages::WebPage::TakeSnapshot(rect, bitmapSize, options), [callback = WTFMove(callback)] (std::optional<ImageBufferBackendHandle>&& imageHandle, Headroom) mutable {
        RELEASE_ASSERT(!imageHandle || std::holds_alternative<ShareableBitmap::Handle>(*imageHandle));
        callback(imageHandle ? std::make_optional(std::get<ShareableBitmap::Handle>(*imageHandle)) : std::nullopt);
    });
}

#if PLATFORM(COCOA)
void WebPageProxy::takeSnapshot(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options, CompletionHandler<void(CGImageRef)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::TakeSnapshot(rect, bitmapSize, options), [callback = WTFMove(callback)] (std::optional<ImageBufferBackendHandle>&& imageHandle, Headroom headroom) mutable {
        if (!imageHandle) {
            callback(nullptr);
            return;
        }

        RetainPtr<CGImageRef> image;
        WTF::switchOn(*imageHandle,
            [&image] (WebCore::ShareableBitmap::Handle& handle) {
                if (RefPtr bitmap = WebCore::ShareableBitmap::create(WTFMove(handle), WebCore::SharedMemory::Protection::ReadOnly))
                    image = bitmap->createPlatformImage(DontCopyBackingStore);
            }
            , [&image] (MachSendRight& machSendRight) {
                if (auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight)))
                    image = WebCore::IOSurface::sinkIntoImage(WTFMove(surface));
            }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
            , [] (WebCore::DynamicContentScalingDisplayList&) {
                ASSERT_NOT_REACHED();
                return;
            }
#endif
        );

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
        if (image && headroom > Headroom::None)
            image = adoptCF(CGImageCreateCopyWithContentHeadroom(headroom.headroom, image.get()));
#endif

        callback(image.get());
    });
}
#endif

void WebPageProxy::navigationGestureDidBegin()
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    m_isShowingNavigationGestureSnapshot = true;
    pageClient->navigationGestureDidBegin();

    m_navigationClient->didBeginNavigationGesture(*this);
}

void WebPageProxy::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    if (willNavigate) {
        m_isLayerTreeFrozenDueToSwipeAnimation = true;
        send(Messages::WebPage::SwipeAnimationDidStart());
    }

    pageClient->navigationGestureWillEnd(willNavigate, item);

    m_navigationClient->willEndNavigationGesture(*this, willNavigate, item);
}

void WebPageProxy::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    pageClient->navigationGestureDidEnd(willNavigate, item);

    m_navigationClient->didEndNavigationGesture(*this, willNavigate, item);

    if (m_isLayerTreeFrozenDueToSwipeAnimation) {
        m_isLayerTreeFrozenDueToSwipeAnimation = false;
        send(Messages::WebPage::SwipeAnimationDidEnd());

        if (RefPtr provisionalPage = m_provisionalPage)
            provisionalPage->swipeAnimationDidEnd();
    }
}

void WebPageProxy::navigationGestureDidEnd()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->navigationGestureDidEnd();
}

void WebPageProxy::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->willRecordNavigationSnapshot(item);
}

void WebPageProxy::navigationGestureSnapshotWasRemoved()
{
    m_isShowingNavigationGestureSnapshot = false;

    // The ViewGestureController may call this method on a WebPageProxy whose view has been destroyed. In such case,
    // we need to return early as the pageClient will not be valid below.
    if (m_isClosed)
        return;

    if (RefPtr pageClient = this->pageClient())
        pageClient->didRemoveNavigationGestureSnapshot();

    m_navigationClient->didRemoveNavigationGestureSnapshot(*this);
}

void WebPageProxy::willBeginViewGesture()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->willBeginViewGesture();
}

void WebPageProxy::didEndViewGesture()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didEndViewGesture();
}

void WebPageProxy::isPlayingMediaDidChange(MediaProducerMediaStateFlags newState)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_legacyMainFrameProcess->throttler().shouldBeRunnable())
        return;
#endif

    if (internals().mainFrameMediaState == newState)
        return;
    internals().mainFrameMediaState = newState;

    if (!m_isClosed)
        updatePlayingMediaDidChange(CanDelayNotification::Yes);
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

void WebPageProxy::updatePlayingMediaDidChange(CanDelayNotification canDelayNotification)
{
    MediaProducerMediaStateFlags newState = internals().mainFrameMediaState;
    protectedBrowsingContextGroup()->forEachRemotePage(*this, [&newState](auto& remotePage) {
        newState.add(remotePage.mediaState());
    });

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
    RefPtr pageClient = this->pageClient();
    if (playingAudioChanges && pageClient)
        pageClient->isPlayingAudioWillChange();

    internals().mediaState = newState;

    if (playingAudioChanges && pageClient)
        pageClient->isPlayingAudioDidChange();

#if ENABLE(MEDIA_STREAM)
    if (oldMediaCaptureState != newMediaCaptureState) {
        updateReportedMediaCaptureState();

        RefPtr userMediaPermissionRequestManager = m_userMediaPermissionRequestManager;
        ASSERT(userMediaPermissionRequestManager);
        if (userMediaPermissionRequestManager)
            userMediaPermissionRequestManager->captureStateChanged(oldMediaCaptureState, newMediaCaptureState);

#if ENABLE(MEDIA_STREAM) && ENABLE(GPU_PROCESS)
        if (protectedPreferences()->captureAudioInGPUProcessEnabled() && newMediaCaptureState & WebCore::MediaProducerMediaState::HasActiveAudioCaptureDevice)
            configuration().protectedProcessPool()->ensureProtectedGPUProcess()->setPageUsingMicrophone(identifier());
#endif
    }
    updateMediaCaptureStateImmediatelyIfNeeded();
#endif

    activityStateDidChange({ ActivityState::IsAudible, ActivityState::IsCapturingMedia });

    playingMediaMask.add(WebCore::MediaProducer::MediaCaptureMask);
    if ((oldState & playingMediaMask) != (internals().mediaState & playingMediaMask))
        m_uiClient->isPlayingMediaDidChange(*this);

    if ((oldState.containsAny(MediaProducerMediaState::HasAudioOrVideo)) != (internals().mediaState.containsAny(MediaProducerMediaState::HasAudioOrVideo)))
        videoControlsManagerDidChange();

    forEachWebContentProcess([&] (auto& process, auto) {
        process.updateAudibleMediaAssertions();
    });

    bool mediaStreamingChanges = oldState.contains(MediaProducerMediaState::HasStreamingActivity) != newState.contains(MediaProducerMediaState::HasStreamingActivity);
    if (mediaStreamingChanges) {
        forEachWebContentProcess([&] (auto& process, auto) {
            process.updateMediaStreamingActivity();
        });
    }

#if ENABLE(EXTENSION_CAPABILITIES)
    updateMediaCapability();
#endif

#if ENABLE(SCREEN_TIME)
    if (oldState.contains(MediaProducerMediaState::IsPlayingVideo) != newState.contains(MediaProducerMediaState::IsPlayingVideo))
        protectedPageClient()->setURLIsPlayingVideoForScreenTime(newState.contains(MediaProducerMediaState::IsPlayingVideo));
#endif
}

void WebPageProxy::updatePlayingMediaDidChangeTimerFired()
{
    updatePlayingMediaDidChange(CanDelayNotification::Yes);
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

    RefPtr pageClient = this->pageClient();
    if (pageClient) {
        if (microphoneCaptureChanged)
            pageClient->microphoneCaptureWillChange();
        if (cameraCaptureChanged)
            pageClient->cameraCaptureWillChange();
        if (displayCaptureChanged)
            pageClient->displayCaptureWillChange();
        if (displayCaptureSurfacesChanged)
            pageClient->displayCaptureSurfacesWillChange();
        if (systemAudioCaptureChanged)
            pageClient->systemAudioCaptureWillChange();
    }

    internals().reportedMediaCaptureState = activeCaptureState;
    m_uiClient->mediaCaptureStateDidChange(internals().mediaState);

    if (pageClient) {
        if (microphoneCaptureChanged)
            pageClient->microphoneCaptureChanged();
        if (cameraCaptureChanged)
            pageClient->cameraCaptureChanged();
        if (displayCaptureChanged)
            pageClient->displayCaptureChanged();
        if (displayCaptureSurfacesChanged)
            pageClient->displayCaptureSurfacesChanged();
        if (systemAudioCaptureChanged)
            pageClient->systemAudioCaptureChanged();
    }
}

void WebPageProxy::videoControlsManagerDidChange()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->videoControlsManagerDidChange();
}

void WebPageProxy::videosInElementFullscreenChanged()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->videosInElementFullscreenChanged();
}

bool WebPageProxy::hasActiveVideoForControlsManager() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr playbackSessionManager = m_playbackSessionManager;
    return playbackSessionManager && playbackSessionManager->controlsManagerInterface();
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->handleControlledElementIDResponse(identifier);
#endif
}

bool WebPageProxy::isPlayingVideoInEnhancedFullscreen() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr videoPresentationManager = m_videoPresentationManager;
    return videoPresentationManager && videoPresentationManager->isPlayingVideoInEnhancedFullscreen();
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

void WebPageProxy::performImmediateActionHitTestAtLocation(WebCore::FrameIdentifier frameID, FloatPoint point)
{
    sendToProcessContainingFrame(frameID, Messages::WebPage::PerformImmediateActionHitTestAtLocation(frameID, point));
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

void WebPageProxy::didPerformImmediateActionHitTest(IPC::Connection& connection, WebHitTestResultData&& result, bool contentPreventsDefault, const UserData& userData)
{
    if (protectedPreferences()->siteIsolationEnabled()) {
        if (result.remoteUserInputEventData) {
            performImmediateActionHitTestAtLocation(result.remoteUserInputEventData->targetFrameID, FloatPoint(result.remoteUserInputEventData->transformedPoint));
            return;
        }
        if (auto parentFrameID = result.frameInfo->parentFrameID) {
            sendWithAsyncReplyToProcessContainingFrame(parentFrameID, Messages::WebPage::RemoteDictionaryPopupInfoToRootView(result.frameInfo->frameID, result.dictionaryPopupInfo), [protectedThis = Ref { *this }, userData, result = WTFMove(result), contentPreventsDefault] (IPC::Connection* connection, WebCore::DictionaryPopupInfo popupInfo) mutable {
                result.dictionaryPopupInfo = popupInfo;
                if (!connection)
                    return;
                if (RefPtr pageClient = protectedThis->pageClient())
                    pageClient->didPerformImmediateActionHitTest(result, contentPreventsDefault, WebProcessProxy::fromConnection(*connection)->transformHandlesToObjects(userData.protectedObject().get()).get());
            });
            return;
        }
    }
    if (RefPtr pageClient = this->pageClient())
        pageClient->didPerformImmediateActionHitTest(result, contentPreventsDefault, WebProcessProxy::fromConnection(connection)->transformHandlesToObjects(userData.protectedObject().get()).get());
}

NSObject *WebPageProxy::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return nullptr;
    return protectedPageClient()->immediateActionAnimationControllerForHitTestResult(hitTestResult, type, userData);
}

void WebPageProxy::handleAcceptedCandidate(WebCore::TextCheckingResult acceptedCandidate)
{
    send(Messages::WebPage::HandleAcceptedCandidate(acceptedCandidate));
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

void WebPageProxy::handleAutoFillButtonClick(IPC::Connection& connection, const UserData& userData)
{
    m_uiClient->didClickAutoFillButton(*this, WebProcessProxy::fromConnection(connection)->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::didResignInputElementStrongPasswordAppearance(IPC::Connection& connection, const UserData& userData)
{
    m_uiClient->didResignInputElementStrongPasswordAppearance(*this, WebProcessProxy::fromConnection(connection)->transformHandlesToObjects(userData.protectedObject().get()).get());
}

void WebPageProxy::performSwitchHapticFeedback()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->performSwitchHapticFeedback();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

void WebPageProxy::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->addPlaybackTargetPickerClient(internals(), contextId);
}

void WebPageProxy::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->removePlaybackTargetPickerClient(internals(), contextId);
}

void WebPageProxy::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const WebCore::FloatRect& rect, bool hasVideo)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->showPlaybackTargetPicker(internals(), contextId, protectedPageClient()->rootViewToScreen(IntRect(rect)), hasVideo, useDarkAppearance());
}

void WebPageProxy::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, WebCore::MediaProducerMediaStateFlags state)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->clientStateDidChange(internals(), contextId, state);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void WebPageProxy::setMockMediaPlaybackTargetPickerState(const String& name, WebCore::MediaPlaybackTargetMockState state)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->setMockMediaPlaybackTargetPickerState(name, state);
}

void WebPageProxy::mockMediaPlaybackTargetPickerDismissPopup()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->checkedMediaSessionManager()->mockMediaPlaybackTargetPickerDismissPopup();
}

void WebPageProxy::Internals::setPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, Ref<MediaPlaybackTarget>&& target)
{
    Ref protectedPage = page.get();
    if (!protectedPage->hasRunningProcess())
        return;

    protectedPage->send(Messages::WebPage::PlaybackTargetSelected(contextId, MediaPlaybackTargetContextSerialized { target.get() }));
}

void WebPageProxy::Internals::externalOutputDeviceAvailableDidChange(PlaybackTargetClientContextIdentifier contextId, bool available)
{
    Ref protectedPage = page.get();
    if (!protectedPage->hasRunningProcess())
        return;

    protectedPage->send(Messages::WebPage::PlaybackTargetAvailabilityDidChange(contextId, available));
}

void WebPageProxy::Internals::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    Ref protectedPage = page.get();
    if (!protectedPage->hasRunningProcess())
        return;

    protectedPage->send(Messages::WebPage::SetShouldPlayToPlaybackTarget(contextId, shouldPlay));
}

void WebPageProxy::Internals::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    Ref protectedPage = page.get();
    if (!protectedPage->hasRunningProcess())
        return;

    protectedPage->send(Messages::WebPage::PlaybackTargetPickerWasDismissed(contextId));
}

#endif

void WebPageProxy::didChangeBackgroundColor()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didChangeBackgroundColor();
}

Awaitable<void> WebPageProxy::nextPresentationUpdate()
{
    co_return co_await AwaitableFromCompletionHandler<void> { [this, protectedThis = Ref { *this }] (auto completionHandler) {
        callAfterNextPresentationUpdate(WTFMove(completionHandler));
    } };
}

#if !PLATFORM(GTK) && !PLATFORM(WPE)
void WebPageProxy::callAfterNextPresentationUpdate(CompletionHandler<void()>&& callback)
{
    if (!hasRunningProcess() || !m_drawingArea)
        return callback();

#if PLATFORM(COCOA)
    Ref aggregator = CallbackAggregator::create(WTFMove(callback));
    auto drawingAreaIdentifier = m_drawingArea->identifier();
    for (Ref process : webContentProcessesWithFrame()) {
        auto callbackID = process->sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [aggregator] { }, drawingAreaIdentifier);
        if (callbackID && process->hasConnection())
            protectedDrawingArea()->addOutstandingPresentationUpdateCallback(process->protectedConnection(), *callbackID);
    }
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->didRestoreScrollPosition();
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
            if (!iconData.isNull())
                callback(API::Data::create(iconData.span()).ptr());
            else
                callback(nullptr);
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

    endDataListSuggestions();

    endColorPicker();

    endDateTimePicker();
}

#if ENABLE(POINTER_LOCK)
void WebPageProxy::requestPointerLock(IPC::Connection& connection, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!m_isPointerLockPending);
    ASSERT(!m_isPointerLocked);
    m_isPointerLockPending = true;

    if (!isViewVisible() || !isViewFocused()) {
        didDenyPointerLock(WTFMove(completionHandler));
        return;
    }

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    if (!hasMouseDevice()) {
        didDenyPointerLock(WTFMove(completionHandler));
        return;
    }
#endif

    Ref webContentProcess = WebProcessProxy::fromConnection(connection);

    m_uiClient->requestPointerLock(this, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), webContentProcess = WTFMove(webContentProcess)] (bool result) mutable {
        if (result) {
            didAllowPointerLock(WTFMove(completionHandler));
            m_webContentPointerLockProcess = webContentProcess.get();
        } else
            didDenyPointerLock(WTFMove(completionHandler));
    });
}

void WebPageProxy::didAllowPointerLock(CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_isPointerLockPending)
        return completionHandler(false);

    ASSERT(!m_isPointerLocked);
    m_isPointerLocked = true;
    m_isPointerLockPending = false;

    platformLockPointer();

    completionHandler(true);
}

void WebPageProxy::didDenyPointerLock(CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_isPointerLockPending)
        return completionHandler(false);

    ASSERT(!m_isPointerLocked);
    m_isPointerLockPending = false;

    completionHandler(false);
}

void WebPageProxy::requestPointerUnlock(CompletionHandler<void(bool)>&& completionHandler)
{
    bool wasPointerLocked = std::exchange(m_isPointerLocked, false);
    bool wasPointerLockPending = std::exchange(m_isPointerLockPending, false);

    if (wasPointerLocked)
        platformUnlockPointer();

    if (wasPointerLocked || wasPointerLockPending)
        m_uiClient->didLosePointerLock(this);

    completionHandler(wasPointerLocked);
}

RefPtr<WebProcessProxy> WebPageProxy::webContentPointerLockProcess()
{
    return m_webContentPointerLockProcess;
}

void WebPageProxy::clearWebContentPointerLockProcess()
{
    m_webContentPointerLockProcess = nullptr;
}

void WebPageProxy::resetPointerLockState()
{
    requestPointerUnlock([this, protectedThis = RefPtr { *this }](bool result) {
        if (result) {
            RefPtr<WebProcessProxy> webContentPointerLock = webContentPointerLockProcess();
            webContentPointerLock->send(Messages::WebPage::DidLosePointerLock(), webPageIDInProcess(*webContentPointerLock));
            clearWebContentPointerLockProcess();
        }
    });
}

#if !PLATFORM(COCOA)

void WebPageProxy::platformLockPointer()
{
}

void WebPageProxy::platformUnlockPointer()
{
}

#endif

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

    WebCore::LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(scheme);
    if (hasRunningProcess())
        send(Messages::WebPage::RegisterURLSchemeHandler(handlerIdentifier, canonicalizedScheme.value()));
}

WebURLSchemeHandler* WebPageProxy::urlSchemeHandlerForScheme(const String& scheme)
{
    return scheme.isNull() ? nullptr : m_urlSchemeHandlersByScheme.get(scheme);
}

void WebPageProxy::startURLSchemeTask(IPC::Connection& connection, URLSchemeTaskParameters&& parameters)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    auto webPageID = webPageIDInProcess(process);
    startURLSchemeTaskShared(connection, WTFMove(process), webPageID, WTFMove(parameters));
}

void WebPageProxy::startURLSchemeTaskShared(IPC::Connection& connection, Ref<WebProcessProxy>&& process, PageIdentifier webPageID, URLSchemeTaskParameters&& parameters)
{
    MESSAGE_CHECK_BASE(decltype(Internals::urlSchemeHandlersByIdentifier)::isValidKey(parameters.handlerIdentifier), connection);
    auto iterator = internals().urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK(process, iterator != internals().urlSchemeHandlersByIdentifier.end());

    Ref { iterator->value }->startTask(*this, process, webPageID, WTFMove(parameters), nullptr);
}

void WebPageProxy::stopURLSchemeTask(IPC::Connection& connection, WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier)
{
    MESSAGE_CHECK_BASE(decltype(Internals::urlSchemeHandlersByIdentifier)::isValidKey(handlerIdentifier), connection);
    auto iterator = internals().urlSchemeHandlersByIdentifier.find(handlerIdentifier);
    MESSAGE_CHECK_BASE(iterator != internals().urlSchemeHandlersByIdentifier.end(), connection);

    Ref { iterator->value }->stopTask(*this, taskIdentifier);
}

void WebPageProxy::loadSynchronousURLSchemeTask(IPC::Connection& connection, URLSchemeTaskParameters&& parameters, CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, Vector<uint8_t>&&)>&& reply)
{
    MESSAGE_CHECK_COMPLETION_BASE(decltype(Internals::urlSchemeHandlersByIdentifier)::isValidKey(parameters.handlerIdentifier), connection, reply({ }, { }, { }));
    auto iterator = internals().urlSchemeHandlersByIdentifier.find(parameters.handlerIdentifier);
    MESSAGE_CHECK_COMPLETION_BASE(iterator != internals().urlSchemeHandlersByIdentifier.end(), connection, reply({ }, { }, { }));

    Ref { iterator->value }->startTask(*this, m_legacyMainFrameProcess, m_webPageID, WTFMove(parameters), WTFMove(reply));
}

void WebPageProxy::requestStorageAccessConfirm(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, std::optional<OrganizationStorageAccessPromptQuirk>&& organizationStorageAccessPromptQuirk, CompletionHandler<void(bool)>&& completionHandler)
{
    m_uiClient->requestStorageAccessConfirm(*this, WebFrameProxy::protectedWebFrame(frameID).get(), subFrameDomain, topFrameDomain, WTFMove(organizationStorageAccessPromptQuirk), WTFMove(completionHandler));
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
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->effectiveAppearanceIsDark();
}

bool WebPageProxy::useElevatedUserInterfaceLevel() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->effectiveUserInterfaceLevelIsElevated();
}

void WebPageProxy::setUseColorAppearance(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
    if (!hasRunningProcess())
        return;

    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::SetUseColorAppearance(useDarkAppearance, useElevatedUserInterfaceLevel), pageID);
    });
}

void WebPageProxy::setUseDarkAppearanceForTesting(bool useDarkAppearance)
{
    setUseColorAppearance(useDarkAppearance, useElevatedUserInterfaceLevel());
}

void WebPageProxy::effectiveAppearanceDidChange()
{
    setUseColorAppearance(useDarkAppearance(), useElevatedUserInterfaceLevel());
}

DataOwnerType WebPageProxy::dataOwnerForPasteboard(PasteboardAccessIntent intent) const
{
    return protectedPageClient()->dataOwnerForPasteboard(intent);
}

#if ENABLE(ATTACHMENT_ELEMENT)

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::writePromisedAttachmentToPasteboard(IPC::Connection& connection, PromisedAttachmentInfo&& info, const String& authorizationToken)
{
    MESSAGE_CHECK_BASE(isValidPerformActionOnElementAuthorizationToken(authorizationToken), connection);

    if (RefPtr pageClient = this->pageClient())
        pageClient->writePromisedAttachmentToPasteboard(WTFMove(info));
}
#endif

void WebPageProxy::requestAttachmentIcon(IPC::Connection& connection, const String& identifier, const String& contentType, const String& fileName, const String& title, const FloatSize& requestedSize)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);

    auto updateAttachmentIcon = [&, protectedThis = Ref { *this }] {
        FloatSize size = requestedSize;
        std::optional<ShareableBitmap::Handle> handle;

#if PLATFORM(COCOA)
        if (RefPtr icon = iconForAttachment(fileName, contentType, title, size)) {
            if (auto iconHandle = icon->createHandle())
                handle = WTFMove(*iconHandle);
        }
#endif

        protectedLegacyMainFrameProcess()->send(Messages::WebPage::UpdateAttachmentIcon(identifier, WTFMove(handle), size), webPageIDInMainFrameProcess());
    };

#if PLATFORM(MAC)
    if (RefPtr attachment = attachmentForIdentifier(identifier); attachment && attachment->shouldUseFileWrapperIconForDirectory()) {
        attachment->doWithFileWrapper([&, updateAttachmentIcon = WTFMove(updateAttachmentIcon)] (NSFileWrapper *fileWrapper) {
            if (updateIconForDirectory(fileWrapper, attachment->identifier()))
                return;

            updateAttachmentIcon();
        });
        return;
    }
#endif // PLATFORM(MAC)

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
}

void WebPageProxy::registerAttachmentIdentifierFromData(IPC::Connection& connection, const String& identifier, const String& contentType, const String& preferredFileName, const IPC::SharedBufferReference& data)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(identifier), connection);

    if (attachmentForIdentifier(identifier))
        return;

    Ref attachment = ensureAttachment(identifier);
    attachment->setContentType(contentType);
    m_attachmentIdentifierToAttachmentMap.set(identifier, attachment.copyRef());

    platformRegisterAttachment(WTFMove(attachment), preferredFileName, data);
}

void WebPageProxy::registerAttachmentIdentifierFromFilePath(IPC::Connection& connection, const String& identifier, const String& contentType, const String& filePath)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(identifier), connection);

    if (attachmentForIdentifier(identifier))
        return;

    Ref attachment = ensureAttachment(identifier);
    attachment->setContentType(contentType);
    attachment->setFilePath(filePath);
    m_attachmentIdentifierToAttachmentMap.set(identifier, attachment.copyRef());
    platformRegisterAttachment(WTFMove(attachment), filePath);
}

void WebPageProxy::registerAttachmentIdentifier(IPC::Connection& connection, const String& identifier)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(identifier), connection);

    if (!attachmentForIdentifier(identifier))
        m_attachmentIdentifierToAttachmentMap.set(identifier, ensureAttachment(identifier));
}

void WebPageProxy::registerAttachmentsFromSerializedData(IPC::Connection& connection, Vector<SerializedAttachmentData>&& data)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);

    for (auto& serializedData : data) {
        auto identifier = WTFMove(serializedData.identifier);
        if (!attachmentForIdentifier(identifier)) {
            Ref attachment = ensureAttachment(identifier);
            attachment->updateFromSerializedRepresentation(WTFMove(serializedData.data), WTFMove(serializedData.mimeType));
        }
    }
}

void WebPageProxy::cloneAttachmentData(IPC::Connection& connection, const String& fromIdentifier, const String& toIdentifier)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(fromIdentifier), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(toIdentifier), connection);

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

    MESSAGE_CHECK_COMPLETION(protectedLegacyMainFrameProcess(), protectedPreferences()->attachmentElementEnabled(), completionHandler(WTFMove(serializedData)));

    for (const auto& identifier : identifiers)
        MESSAGE_CHECK_COMPLETION(m_legacyMainFrameProcess, IdentifierToAttachmentMap::isValidKey(identifier), completionHandler(WTFMove(serializedData)));

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
    if (RefPtr pageClient = this->pageClient())
        pageClient->didInvalidateDataForAttachment(attachment);
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

void WebPageProxy::didInsertAttachmentWithIdentifier(IPC::Connection& connection, const String& identifier, const String& source, WebCore::AttachmentAssociatedElementType associatedElementType)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(identifier), connection);

    Ref attachment = ensureAttachment(identifier);
    attachment->setAssociatedElementType(associatedElementType);
    attachment->setInsertionState(API::Attachment::InsertionState::Inserted);
    if (RefPtr pageClient = this->pageClient())
        pageClient->didInsertAttachment(attachment, source);

    if (!attachment->isEmpty() && associatedElementType != WebCore::AttachmentAssociatedElementType::None)
        updateAttachmentAttributes(attachment, [] { });
}

void WebPageProxy::didRemoveAttachmentWithIdentifier(IPC::Connection& connection, const String& identifier)
{
    MESSAGE_CHECK_BASE(protectedPreferences()->attachmentElementEnabled(), connection);
    MESSAGE_CHECK_BASE(IdentifierToAttachmentMap::isValidKey(identifier), connection);

    if (RefPtr attachment = attachmentForIdentifier(identifier))
        didRemoveAttachment(*attachment);
}

void WebPageProxy::didRemoveAttachment(API::Attachment& attachment)
{
    attachment.setInsertionState(API::Attachment::InsertionState::NotInserted);
    if (RefPtr pageClient = this->pageClient())
        pageClient->didRemoveAttachment(attachment);
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

#if PLATFORM(COCOA)
void WebPageProxy::insertTextPlaceholder(const IntSize& size, CompletionHandler<void(const std::optional<ElementContext>&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::InsertTextPlaceholder { size }, WTFMove(completionHandler));
}

void WebPageProxy::removeTextPlaceholder(const ElementContext& placeholder, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }
    sendWithAsyncReply(Messages::WebPage::RemoveTextPlaceholder { placeholder }, WTFMove(completionHandler));
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

    if (std::ranges::find_if(m_previouslyVisitedPaths, startsWithURLPath) != m_previouslyVisitedPaths.end())
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

void WebPageProxy::detectDataInAllFrames(OptionSet<WebCore::DataDetectorType> types, CompletionHandler<void(DataDetectionResult&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::DetectDataInAllFrames(types), WTFMove(completionHandler));
}

void WebPageProxy::removeDataDetectedLinks(CompletionHandler<void(DataDetectionResult&&)>&& completionHandler)
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

void WebPageProxy::setPrivateClickMeasurementImmediately(PrivateClickMeasurement&& measurement)
{
    protectedWebsiteDataStore()->storePrivateClickMeasurement(WTFMove(measurement));
}

auto WebPageProxy::privateClickMeasurementEventAttribution() const -> std::optional<EventAttribution>
{
    auto& pcm = internals().privateClickMeasurement;
    if (!pcm)
        return std::nullopt;
    return { { pcm->pcm.sourceID(), pcm->pcm.destinationSite().registrableDomain.string(), pcm->sourceDescription, pcm->purchaser } };
}

void WebPageProxy::simulatePrivateClickMeasurementConversion(int priority, int triggerData, const URL& sourceURL, const URL& destinationURL)
{
    protectedWebsiteDataStore()->simulatePrivateClickMeasurementConversion(priority, triggerData, sourceURL, destinationURL);
}

#if ENABLE(APPLE_PAY)

void WebPageProxy::resetPaymentCoordinator(ResetStateReason resetStateReason)
{
    RefPtr paymentCoordinator = internals().paymentCoordinator;
    if (!paymentCoordinator)
        return;

    if (resetStateReason == ResetStateReason::WebProcessExited)
        paymentCoordinator->webProcessExited();

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
    synthesisData.protectedSynthesizer()->resetState();
}

SpeechSynthesisData& WebPageProxy::Internals::speechSynthesisData()
{
    if (!optionalSpeechSynthesisData)
        optionalSpeechSynthesisData = { PlatformSpeechSynthesizer::create(*this), nullptr, nullptr, nullptr, nullptr, nullptr };
    return *optionalSpeechSynthesisData;
}

void WebPageProxy::speechSynthesisVoiceList(CompletionHandler<void(Vector<WebSpeechSynthesisVoice>&&)>&& completionHandler)
{
    auto result = internals().speechSynthesisData().protectedSynthesizer()->voiceList().map([](auto& voice) {
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
    internals().speechSynthesisData().protectedSynthesizer()->speak(internals().speechSynthesisData().utterance.get());
}

void WebPageProxy::speechSynthesisCancel()
{
    internals().speechSynthesisData().protectedSynthesizer()->cancel();
}

void WebPageProxy::speechSynthesisResetState()
{
    internals().speechSynthesisData().protectedSynthesizer()->resetState();
}

void WebPageProxy::speechSynthesisPause(CompletionHandler<void()>&& completionHandler)
{
    internals().speechSynthesisData().speakingPausedCompletionHandler = WTFMove(completionHandler);
    internals().speechSynthesisData().protectedSynthesizer()->pause();
}

void WebPageProxy::speechSynthesisResume(CompletionHandler<void()>&& completionHandler)
{
    internals().speechSynthesisData().speakingResumedCompletionHandler = WTFMove(completionHandler);
    internals().speechSynthesisData().protectedSynthesizer()->resume();
}

#endif // ENABLE(SPEECH_SYNTHESIS)

#if !PLATFORM(COCOA)

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
    m_webViewDidMoveToWindowObservers.forEach([](Ref<WebViewDidMoveToWindowObserver> observer) {
        observer->webViewDidMoveToWindow();
    });

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    auto newWindowKind = pageClient->windowKind();
    if (internals().windowKind != newWindowKind)
        internals().windowKind = newWindowKind;
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
        logger->setEnabled(this, isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}

uint64_t WebPageProxy::logIdentifier() const
{
    return intHash(identifier().toUInt64());
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
    m_textManipulationItemCallback = WTFMove(callback);
    m_internals->textManipulationParameters = { includeSubframes, exclusionRules };

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::StartTextManipulations(exclusionRules, includeSubframes), [callbackAggregator] { }, pageID);
    });
}

void WebPageProxy::didFindTextManipulationItems(const Vector<WebCore::TextManipulationItem>& items)
{
    if (!m_textManipulationItemCallback)
        return;
    m_textManipulationItemCallback(items);
}

void WebPageProxy::completeTextManipulation(const Vector<TextManipulationItem>& items, CompletionHandler<void(Vector<TextManipulationControllerManipulationFailure>&&)>&& completionHandler)
{
    class TextManipulationCallbackAggregator final : public ThreadSafeRefCounted<TextManipulationCallbackAggregator, WTF::DestructionThread::MainRunLoop> {
    public:
        struct ItemInfo {
            Markable<FrameIdentifier> frameID;
            Markable<TextManipulationItemIdentifier> identifier;
        };

        using Callback = CompletionHandler<void(Vector<TextManipulationControllerManipulationFailure>&&)>;

        static Ref<TextManipulationCallbackAggregator> create(Vector<ItemInfo>&& items, Callback&& callback)
        {
            return adoptRef(*new TextManipulationCallbackAggregator(WTFMove(items), WTFMove(callback)));
        }

        ~TextManipulationCallbackAggregator()
        {
            ASSERT(RunLoop::isMain());
            BitVector resultIndexes;
            for (auto& failure : m_result.failures)
                resultIndexes.set(failure.index);
            for (auto& index : m_result.succeededIndexes)
                resultIndexes.add(index);
            for (unsigned index = 0; index < m_items.size(); ++index) {
                if (resultIndexes.get(index))
                    continue;

                WebCore::TextManipulationControllerManipulationFailure failure { *m_items[index].frameID, m_items[index].identifier, index, WebCore::TextManipulationControllerManipulationFailure::Type::NotAvailable };
                m_result.failures.append(WTFMove(failure));
            }

            m_callback(WTFMove(m_result.failures));
        }

        void addResult(TextManipulationControllerManipulationResult&& result)
        {
            m_result.failures.appendVector(WTFMove(result.failures));
            m_result.succeededIndexes.appendVector(WTFMove(result.succeededIndexes));
        }

    private:
        TextManipulationCallbackAggregator(Vector<ItemInfo>&& items, Callback&& callback)
            : m_items(WTFMove(items))
            , m_callback(WTFMove(callback))
        {
            ASSERT(RunLoop::isMain());
        }

        Vector<ItemInfo> m_items;
        Callback m_callback;
        TextManipulationControllerManipulationResult m_result;
    };

    Vector<TextManipulationCallbackAggregator::ItemInfo> itemInfos;
    itemInfos.reserveCapacity(items.size());
    for (auto& item : items)
        itemInfos.append({ item.frameID, item.identifier });
    auto callbackAggregator = TextManipulationCallbackAggregator::create(WTFMove(itemInfos), WTFMove(completionHandler));
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPage::CompleteTextManipulation(items), [callbackAggregator](auto&& result) {
            callbackAggregator->addResult(WTFMove(result));
        }, pageID);
    });
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
    m_isTakingSnapshotsForApplicationSuspension = isTakingSnapshotsForApplicationSuspension;
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
        html = makeString("<script>navigator.serviceWorker.register('"_s, url.string(), "', { type: 'module' });</script>"_s).utf8();
    else
        html = makeString("<script>navigator.serviceWorker.register('"_s, url.string(), "');</script>"_s).utf8();

    loadData(SharedBuffer::create(html.span()), "text/html"_s, "UTF-8"_s, url.protocolHostAndPort());
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

void WebPageProxy::setMediaCaptureRotationForTesting(WebCore::IntDegrees rotation, const String& persistentId)
{
#if ENABLE(MEDIA_STREAM) && HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
    if (preferences().useAVCaptureDeviceRotationCoordinatorAPI() && userMediaPermissionRequestManager().isMonitoringCaptureDeviceRotation(persistentId)) {
        rotationAngleForCaptureDeviceChanged(persistentId, static_cast<VideoFrameRotation>(rotation));
        return;
    }
#endif

    setOrientationForMediaCapture(rotation);
}

void WebPageProxy::setOrientationForMediaCapture(WebCore::IntDegrees orientation)
{
    m_orientationForMediaCapture = orientation;
    if (!hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
#if PLATFORM(COCOA)
    RefPtr gpuProcess = m_configuration->processPool().gpuProcess();
    if (gpuProcess && protectedPreferences()->captureVideoInGPUProcessEnabled())
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

void WebPageProxy::triggerMockCaptureConfigurationChange(bool forCamera, bool forMicrophone, bool forDisplay)
{
    send(Messages::WebPage::TriggerMockCaptureConfigurationChange(forCamera, forMicrophone, forDisplay));
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->gpuProcessDidFinishLaunching();
#if ENABLE(EXTENSION_CAPABILITIES)
    if (RefPtr mediaCapability = this->mediaCapability()) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessCapabilities, "gpuProcessDidFinishLaunching[envID=%" PUBLIC_LOG_STRING "]: updating media capability", mediaCapability->environmentIdentifier().utf8().data());
        updateMediaCapability();
    }
#endif
}

void WebPageProxy::gpuProcessExited(ProcessTerminationReason)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInGPUProcess = 0;
#endif

    if (RefPtr pageClient = this->pageClient())
        pageClient->gpuProcessDidExit();

#if ENABLE(MEDIA_STREAM)
    Ref preferences = m_preferences;
    bool activeAudioCapture = isCapturingAudio() && preferences->captureAudioInGPUProcessEnabled();
    bool activeVideoCapture = isCapturingVideo() && preferences->captureVideoInGPUProcessEnabled();
    bool activeDisplayCapture = false;
    if (activeAudioCapture || activeVideoCapture) {
        Ref gpuProcess = configuration().protectedProcessPool()->ensureGPUProcess();
        forEachWebContentProcess([&](auto& webProcess, auto pageID) {
            gpuProcess->updateCaptureAccess(activeAudioCapture, activeVideoCapture, activeDisplayCapture, webProcess.coreProcessIdentifier(), identifier(), [] { });
        });
#if PLATFORM(IOS_FAMILY)
        gpuProcess->setOrientationForMediaCapture(m_orientationForMediaCapture);
#endif
        if (m_shouldListenToVoiceActivity)
            gpuProcess->setShouldListenToVoiceActivity(*this, m_shouldListenToVoiceActivity);
    }
#endif
}
#endif

#if ENABLE(MODEL_PROCESS)
void WebPageProxy::modelProcessDidFinishLaunching()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->modelProcessDidFinishLaunching();
}

void WebPageProxy::modelProcessExited(ProcessTerminationReason)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextIDForVisibilityPropagationInModelProcess = 0;
#endif

    if (RefPtr pageClient = this->pageClient())
        pageClient->modelProcessDidExit();
}
#endif

#if ENABLE(CONTEXT_MENUS) && !PLATFORM(MAC)

void WebPageProxy::platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

#endif

#if !PLATFORM(COCOA)

std::optional<IPC::AsyncReplyID> WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory, CompletionHandler<void()>&& completionHandler, std::optional<FrameIdentifier>)
{
    completionHandler();
    return std::nullopt;
}

#endif

RefPtr<SpeechRecognitionPermissionManager> WebPageProxy::protectedSpeechRecognitionPermissionManager()
{
    return m_speechRecognitionPermissionManager;
}

void WebPageProxy::requestSpeechRecognitionPermission(WebCore::SpeechRecognitionRequest& request, FrameInfoData&& frameInfo, CompletionHandler<void(std::optional<SpeechRecognitionError>&&)>&& completionHandler)
{
    if (!m_speechRecognitionPermissionManager)
        m_speechRecognitionPermissionManager = SpeechRecognitionPermissionManager::create(*this);

    protectedSpeechRecognitionPermissionManager()->request(request, WTFMove(frameInfo), WTFMove(completionHandler));
}

void WebPageProxy::requestSpeechRecognitionPermissionByDefaultAction(const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr speechRecognitionPermissionManager = m_speechRecognitionPermissionManager.get();
    if (!speechRecognitionPermissionManager) {
        completionHandler(false);
        return;
    }

    speechRecognitionPermissionManager->decideByDefaultAction(origin, WTFMove(completionHandler));
}

void WebPageProxy::requestUserMediaPermissionForSpeechRecognition(FrameIdentifier mainFrameIdentifier, FrameInfoData&& frameInfo, const WebCore::SecurityOrigin& requestingOrigin, const WebCore::SecurityOrigin& topOrigin, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    auto captureDevice = SpeechRecognitionCaptureSource::findCaptureDevice();
    if (!captureDevice) {
        completionHandler(false);
        return;
    }

    protectedUserMediaPermissionRequestManager()->checkUserMediaPermissionForSpeechRecognition(mainFrameIdentifier, WTFMove(frameInfo), requestingOrigin, topOrigin, *captureDevice, WTFMove(completionHandler));
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

    Ref speechRecognitionRemoteRealtimeMediaSourceManager = protectedLegacyMainFrameProcess()->ensureSpeechRecognitionRemoteRealtimeMediaSourceManager();
    if (protectedPreferences()->captureAudioInGPUProcessEnabled())
        return CaptureSourceOrError { SpeechRecognitionRemoteRealtimeMediaSource::create(speechRecognitionRemoteRealtimeMediaSourceManager, *captureDevice, m_webPageID) };

#if PLATFORM(IOS_FAMILY)
    return CaptureSourceOrError { SpeechRecognitionRemoteRealtimeMediaSource::create(speechRecognitionRemoteRealtimeMediaSourceManager, *captureDevice, m_webPageID) };
#else
    return SpeechRecognitionCaptureSource::createRealtimeMediaSource(*captureDevice, m_webPageID);
#endif
}

#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
void WebPageProxy::modelElementGetCamera(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<WebCore::HTMLModelElementCamera, ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->getCameraForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetCamera(ModelIdentifier modelIdentifier, WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->setCameraForModelElement(modelIdentifier, camera, WTFMove(completionHandler));
}

void WebPageProxy::modelElementIsPlayingAnimation(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->isPlayingAnimationForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetAnimationIsPlaying(ModelIdentifier modelIdentifier, bool isPlaying, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->setAnimationIsPlayingForModelElement(modelIdentifier, isPlaying, WTFMove(completionHandler));
}

void WebPageProxy::modelElementIsLoopingAnimation(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->isLoopingAnimationForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetIsLoopingAnimation(ModelIdentifier modelIdentifier, bool isLooping, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->setIsLoopingAnimationForModelElement(modelIdentifier, isLooping, WTFMove(completionHandler));
}

void WebPageProxy::modelElementAnimationDuration(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->animationDurationForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementAnimationCurrentTime(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->animationCurrentTimeForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetAnimationCurrentTime(ModelIdentifier modelIdentifier, Seconds currentTime, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->setAnimationCurrentTimeForModelElement(modelIdentifier, currentTime, WTFMove(completionHandler));
}

void WebPageProxy::modelElementHasAudio(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->hasAudioForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementIsMuted(ModelIdentifier modelIdentifier, CompletionHandler<void(Expected<bool, ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->isMutedForModelElement(modelIdentifier, WTFMove(completionHandler));
}

void WebPageProxy::modelElementSetIsMuted(ModelIdentifier modelIdentifier, bool isMuted, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->setIsMutedForModelElement(modelIdentifier, isMuted, WTFMove(completionHandler));
}
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
void WebPageProxy::takeModelElementFullscreen(ModelIdentifier modelIdentifier)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->takeModelElementFullscreen(modelIdentifier, URL { currentURL() });
}

void WebPageProxy::modelElementSetInteractionEnabled(ModelIdentifier modelIdentifier, bool isInteractionEnabled)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->setInteractionEnabledForModelElement(modelIdentifier, isInteractionEnabled);
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
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->modelElementCreateRemotePreview(uuid, size, WTFMove(completionHandler));
}

void WebPageProxy::modelElementLoadRemotePreview(const String& uuid, const URL& url, CompletionHandler<void(std::optional<WebCore::ResourceError>&&)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->modelElementLoadRemotePreview(uuid, url, WTFMove(completionHandler));
}

void WebPageProxy::modelElementDestroyRemotePreview(const String& uuid)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->modelElementDestroyRemotePreview(uuid);
}

void WebPageProxy::modelElementSizeDidChange(const String& uuid, WebCore::FloatSize size, CompletionHandler<void(Expected<MachSendRight, WebCore::ResourceError>)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->modelElementSizeDidChange(uuid, size, WTFMove(completionHandler));
}

void WebPageProxy::handleMouseDownForModelElement(const String& uuid, const WebCore::LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->handleMouseDownForModelElement(uuid, flippedLocationInElement, timestamp);
}

void WebPageProxy::handleMouseMoveForModelElement(const String& uuid, const WebCore::LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->handleMouseMoveForModelElement(uuid, flippedLocationInElement, timestamp);
}

void WebPageProxy::handleMouseUpForModelElement(const String& uuid, const WebCore::LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->handleMouseUpForModelElement(uuid, flippedLocationInElement, timestamp);
}

void WebPageProxy::modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    if (RefPtr modelElementController = m_modelElementController)
        modelElementController->inlinePreviewUUIDs(WTFMove(completionHandler));
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->requestScrollToRect(targetRect, origin);
}

void WebPageProxy::scrollToRect(const FloatRect& targetRect, const FloatPoint& origin)
{
    send(Messages::WebPage::ScrollToRect(targetRect, origin));
}

void WebPageProxy::setContentOffset(std::optional<int> x, std::optional<int> y, WebCore::ScrollIsAnimated animated)
{
    send(Messages::WebPage::SetContentOffset(x, y, animated));
}

void WebPageProxy::scrollToEdge(WebCore::RectEdges<bool> edges, WebCore::ScrollIsAnimated animated)
{
    send(Messages::WebPage::ScrollToEdge(edges, animated));
}

bool WebPageProxy::shouldEnableLockdownMode() const
{
    return m_configuration->lockdownModeEnabled();
}

bool WebPageProxy::shouldEnableEnhancedSecurity() const
{
    return m_configuration->enhancedSecurityEnabled();
}

#if PLATFORM(COCOA)
void WebPageProxy::appPrivacyReportTestingData(CompletionHandler<void(const AppPrivacyReportTestingData&)>&& completionHandler)
{
    Ref websiteDataStore = m_websiteDataStore;
    websiteDataStore->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::AppPrivacyReportTestingData(websiteDataStore->sessionID()), WTFMove(completionHandler));
}

void WebPageProxy::clearAppPrivacyReportTestingData(CompletionHandler<void()>&& completionHandler)
{
    Ref websiteDataStore = m_websiteDataStore;
    websiteDataStore->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearAppPrivacyReportTestingData(websiteDataStore->sessionID()), WTFMove(completionHandler));
}
#endif

void WebPageProxy::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    m_uiClient->requestCookieConsent(WTFMove(completion));
}

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)
void WebPageProxy::beginTextRecognitionForVideoInElementFullScreen(MediaPlayerIdentifier identifier, FloatRect bounds)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient || !pageClient->isTextRecognitionInFullscreenVideoEnabled())
        return;

#if ENABLE(GPU_PROCESS)
    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess)
        return;

    m_isPerformingTextRecognitionInElementFullScreen = true;
    gpuProcess->requestBitmapImageForCurrentTime(m_legacyMainFrameProcess->coreProcessIdentifier(), identifier, [weakThis = WeakPtr { *this }, bounds](std::optional<ShareableBitmap::Handle>&& bitmapHandle) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_isPerformingTextRecognitionInElementFullScreen)
            return;

        if (!bitmapHandle)
            return;

        if (RefPtr pageClient = protectedThis->pageClient())
            pageClient->beginTextRecognitionForVideoInElementFullscreen(WTFMove(*bitmapHandle), bounds);
        protectedThis->m_isPerformingTextRecognitionInElementFullScreen = false;
    });
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(bounds);
#endif
}

void WebPageProxy::cancelTextRecognitionForVideoInElementFullScreen()
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient || !pageClient->isTextRecognitionInFullscreenVideoEnabled())
        return;

    m_isPerformingTextRecognitionInElementFullScreen = false;
    pageClient->cancelTextRecognitionForVideoInElementFullscreen();
}
#endif // #if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)

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

WebProcessProxy* WebPageProxy::processForSite(const Site& site)
{
    if (RefPtr process = protectedBrowsingContextGroup()->processForSite(site))
        return &process->process();

    return nullptr;
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

    if (LinkDecorationFilteringController::sharedSingleton().cachedListData().isEmpty())
        return;

    m_needsInitialLinkDecorationFilteringData = false;
    send(Messages::WebPage::SetLinkDecorationFilteringData(LinkDecorationFilteringController::sharedSingleton().cachedListData()));
#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
}

void WebPageProxy::waitForInitialLinkDecorationFilteringData(WebFramePolicyListenerProxy& listener)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    LinkDecorationFilteringController::sharedSingleton().updateList([listener = Ref { listener }] {
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

void WebPageProxy::stickyScrollingTreeNodeBeganSticking()
{
    if (!protectedPreferences()->contentInsetBackgroundFillEnabled())
        return;

    internals().needsFixedContainerEdgesUpdateAfterNextCommit = true;
}

void WebPageProxy::adjustLayersForLayoutViewport(const FloatPoint& scrollPosition, const WebCore::FloatRect& layoutViewport, double scale)
{
#if PLATFORM(COCOA)
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->viewportChangedViaDelegatedScrolling(scrollPosition, layoutViewport, scale);
#endif
}

String WebPageProxy::scrollbarStateForScrollingNodeID(std::optional<ScrollingNodeID> nodeID, bool isVertical)
{
#if PLATFORM(COCOA)
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        return scrollingCoordinatorProxy->scrollbarStateForScrollingNodeID(nodeID, isVertical);
#endif
    return ""_s;
}

WebCore::PageIdentifier WebPageProxy::webPageIDInProcess(const WebProcessProxy& process) const
{
    if (RefPtr remotePage = protectedBrowsingContextGroup()->remotePageInProcess(*this, process))
        return remotePage->pageID();
    return m_webPageID;
}

WebPopupMenuProxyClient& WebPageProxy::popupMenuClient()
{
    return internals();
}

CheckedRef<WebPopupMenuProxyClient> WebPageProxy::checkedPopupMenuClient()
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

Ref<PageLoadState> WebPageProxy::protectedPageLoadState()
{
    return internals().pageLoadState;
}

Ref<const PageLoadState> WebPageProxy::protectedPageLoadState() const
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

Ref<GeolocationPermissionRequestManagerProxy> WebPageProxy::protectedGeolocationPermissionRequestManager()
{
    return geolocationPermissionRequestManager();
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

void WebPageProxy::didCreateSleepDisabler(IPC::Connection& connection, SleepDisablerIdentifier identifier, const String& reason, bool display)
{
    MESSAGE_CHECK_BASE(!reason.isNull(), connection);
    auto sleepDisabler = makeUnique<WebCore::SleepDisabler>(reason, display ? PAL::SleepDisabler::Type::Display : PAL::SleepDisabler::Type::System, webPageIDInMainFrameProcess());
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
    RefPtr systemPreviewController = m_systemPreviewController;
    if (!systemPreviewController)
        return completionHandler();
    systemPreviewController->begin(url, topOrigin, systemPreviewInfo, WTFMove(completionHandler));
}

void WebPageProxy::setSystemPreviewCompletionHandlerForLoadTesting(CompletionHandler<void(bool)>&& handler)
{
    if (RefPtr systemPreviewController = m_systemPreviewController)
        systemPreviewController->setCompletionHandlerForLoadTesting(WTFMove(handler));
}
#endif

void WebPageProxy::useRedirectionForCurrentNavigation(const ResourceResponse& response)
{
    ASSERT(response.isRedirection());
    send(Messages::WebPage::UseRedirectionForCurrentNavigation(response));
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

void WebPageProxy::didAccessWindowProxyPropertyViaOpenerForFrame(IPC::Connection& connection, FrameIdentifier frameID, const SecurityOriginData& parentOrigin, WindowProxyProperty property)
{
    if (!internals().frameLoadStateObserver)
        return;

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

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

void WebPageProxy::broadcastFocusedFrameToOtherProcesses(IPC::Connection& connection, std::optional<WebCore::FrameIdentifier>&& frameID)
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || &webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::FrameWasFocusedInAnotherProcess(WTFMove(frameID)), pageID);
    });
}

Ref<WebPreferences> WebPageProxy::protectedPreferences() const
{
    return m_preferences;
}

Ref<WebsiteDataStore> WebPageProxy::protectedWebsiteDataStore() const
{
    return m_websiteDataStore;
}

Ref<WebProcessProxy> WebPageProxy::processContainingFrame(std::optional<WebCore::FrameIdentifier> frameID)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        return frame->process();
    return siteIsolatedProcess();
}

template<typename F>
decltype(auto) WebPageProxy::sendToWebPage(std::optional<FrameIdentifier> frameID, F&& sendFunction)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID)) {
        if (RefPtr remotePage = protectedBrowsingContextGroup()->remotePageInProcess(*this, frame->protectedProcess()))
            return sendFunction(*remotePage);
    }
    return sendFunction(*this);
}

template<typename M, typename C>
std::optional<IPC::AsyncReplyID> WebPageProxy::sendWithAsyncReplyToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message, C&& completionHandler, OptionSet<IPC::SendOption> options)
{
    return sendToWebPage(frameID,
        [&message, &completionHandler, options] (auto& targetPage) {
            return targetPage.siteIsolatedProcess().sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler), targetPage.identifierInSiteIsolatedProcess(), options);
        }
    );
}

template<typename M, typename C> void WebPageProxy::sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(std::optional<WebCore::FrameIdentifier> frameID, M&& message, C&& completionHandler, OptionSet<IPC::SendOption> options)
{
    sendToWebPage(frameID,
        [&message, &completionHandler, options] (auto& targetPage) {
        return targetPage.siteIsolatedProcess().sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler), { }, options);
        }
    );
}

template<typename M>
void WebPageProxy::sendToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message, OptionSet<IPC::SendOption> options)
{
    sendToWebPage(frameID,
        [&message, options] (auto& targetPage) {
            targetPage.siteIsolatedProcess().send(std::forward<M>(message), targetPage.identifierInSiteIsolatedProcess(), options);
        }
    );
}

template<typename M>
IPC::ConnectionSendSyncResult<M> WebPageProxy::sendSyncToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message, const IPC::Timeout& timeout, OptionSet<IPC::SendSyncOption> options)
{
    return sendToWebPage(frameID,
        [&message, &timeout, options] (auto& targetPage) {
            return targetPage.siteIsolatedProcess().sendSync(std::forward<M>(message), targetPage.identifierInSiteIsolatedProcess(), timeout, options);
        }
    );
}

template<typename M>
IPC::ConnectionSendSyncResult<M> WebPageProxy::sendSyncToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message)
{
    return sendSyncToProcessContainingFrame(frameID, std::forward<M>(message), 1_s, { });
}

template<typename M>
IPC::ConnectionSendSyncResult<M> WebPageProxy::sendSyncToProcessContainingFrame(std::optional<FrameIdentifier> frameID, M&& message, const IPC::Timeout& timeout)
{
    return sendSyncToProcessContainingFrame(frameID, std::forward<M>(message), timeout, { });
}

#define INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(message) \
    template void WebPageProxy::sendToProcessContainingFrame<Messages::message>(std::optional<WebCore::FrameIdentifier>, Messages::message&&, OptionSet<IPC::SendOption>)
#if PLATFORM(COCOA)
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(RemoteScrollingCoordinator::ScrollingTreeNodeScrollbarVisibilityDidChange);
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(RemoteScrollingCoordinator::ScrollingTreeNodeScrollbarMinimumThumbLengthDidChange);
#endif
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebInspectorBackend::ShowMainResourceForFrame);
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::LoadURLInFrame);
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::LoadDataInFrame);
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebProcess::BindAccessibilityFrameWithData);
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::UpdateFrameScrollingMode);
#if PLATFORM(MAC)
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::ZoomPDFOut);
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::ZoomPDFIn);
#endif
#if ENABLE(MEDIA_STREAM)
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::UserMediaAccessWasDenied);
#endif
#if PLATFORM(GTK)
INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME(WebPage::CollapseSelectionInFrame);
#endif
#undef INSTANTIATE_SEND_TO_PROCESS_CONTAINING_FRAME

#define INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(message) \
    template void WebPageProxy::sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier<Messages::message, Messages::message::Reply>(std::optional<WebCore::FrameIdentifier>, Messages::message&&, Messages::message::Reply&&, OptionSet<IPC::SendOption>)
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::TakeScreenshot);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::EvaluateJavaScriptFunction);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::ComputeElementLayout);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::FocusFrame);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::GetComputedRole);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::GetComputedLabel);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::ResolveParentFrame);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::SelectOptionElement);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::ResolveChildFrameWithName);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::SnapshotRectForScreenshot);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::SetFilesForInputFileUpload);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::ResolveChildFrameWithOrdinal);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID(WebAutomationSessionProxy::ResolveChildFrameWithNodeHandle);
#undef INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME_WITHOUT_DESTINATION_ID

#define INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(message) \
    template std::optional<IPC::AsyncReplyID> WebPageProxy::sendWithAsyncReplyToProcessContainingFrame<Messages::message, Messages::message::Reply>(std::optional<WebCore::FrameIdentifier>, Messages::message&&, Messages::message::Reply&&, OptionSet<IPC::SendOption>)
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::NavigateServiceWorkerClient);
#if PLATFORM(IOS_FAMILY)
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::CommitPotentialTap);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::PotentialTapAtPosition);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::DrawToImage);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::DrawToPDFiOS);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::DrawPrintingPagesToSnapshotiOS);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::DrawPrintingToSnapshotiOS);
#if ENABLE(DRAG_SUPPORT)
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::RequestDragStart);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::RequestAdditionalItemsForDragSession);
#endif
#endif
#if PLATFORM(MAC)
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::SavePDF);
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::OpenPDFWithPreview);
#endif
#if ENABLE(MEDIA_STREAM)
INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME(WebPage::UserMediaAccessWasGranted);
#endif
#undef INSTANTIATE_SEND_WITH_ASYNC_REPLY_TO_PROCESS_CONTAINING_FRAME

#define INSTANTIATE_SEND_SYNC_TO_PROCESS_CONTAINING_FRAME(message) \
    template IPC::ConnectionSendSyncResult<Messages::message> WebPageProxy::sendSyncToProcessContainingFrame<Messages::message>(std::optional<WebCore::FrameIdentifier>, Messages::message&&, const IPC::Timeout&)
INSTANTIATE_SEND_SYNC_TO_PROCESS_CONTAINING_FRAME(WebPageTesting::IsEditingCommandEnabled);
#if PLATFORM(IOS_FAMILY)
INSTANTIATE_SEND_SYNC_TO_PROCESS_CONTAINING_FRAME(WebPage::ComputePagesForPrintingiOS);
#endif
#undef INSTANTIATE_SEND_SYNC_TO_PROCESS_CONTAINING_FRAME

void WebPageProxy::focusRemoteFrame(IPC::Connection& connection, WebCore::FrameIdentifier frameID)
{
    RefPtr destinationFrame = WebFrameProxy::webFrame(frameID);
    if (!destinationFrame || !destinationFrame->isMainFrame())
        return;

    ASSERT(destinationFrame->page() == this);

    broadcastFocusedFrameToOtherProcesses(connection, std::make_optional(frameID));
    setFocus(true);
}

void WebPageProxy::postMessageToRemote(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData> targetOrigin, const WebCore::MessageWithMessagePorts& message)
{
    sendToProcessContainingFrame(target, Messages::WebPage::RemotePostMessage(source, sourceOrigin, target, targetOrigin, message));
}

void WebPageProxy::renderTreeAsTextForTesting(WebCore::FrameIdentifier frameID, uint64_t baseIndent, OptionSet<WebCore::RenderAsTextFlag> behavior, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto sendResult = sendSyncToProcessContainingFrame(frameID, Messages::WebPage::RenderTreeAsTextForTesting(frameID, baseIndent, behavior), 1_s, IPC::SendSyncOption::MaintainOrderingWithAsyncMessages);
    if (!sendResult.succeeded())
        return completionHandler("Test Error - sending WebPage::RenderTreeAsTextForTesting failed"_s);

    auto [result] = sendResult.takeReply();
    completionHandler(WTFMove(result));
}

void WebPageProxy::layerTreeAsTextForTesting(FrameIdentifier frameID, uint64_t baseIndent, OptionSet<LayerTreeAsTextOptions> options, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto sendResult = sendSyncToProcessContainingFrame(frameID, Messages::WebPage::LayerTreeAsTextForTesting(frameID, baseIndent, options));
    if (!sendResult.succeeded())
        return completionHandler("Test Error - sending WebPage::RenderTreeAsTextForTesting failed"_s);

    auto [result] = sendResult.takeReply();
    completionHandler(WTFMove(result));
}

void WebPageProxy::addMessageToConsoleForTesting(String&& message)
{
    m_uiClient->addMessageToConsoleForTesting(*this, WTFMove(message));
}

void WebPageProxy::frameTextForTesting(WebCore::FrameIdentifier frameID, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto sendResult = sendSyncToProcessContainingFrame(frameID, Messages::WebPage::FrameTextForTesting(frameID));
    if (!sendResult.succeeded())
        return completionHandler("Test Error - sending WebPage::FrameTextForTesting failed"_s);

    auto [result] = sendResult.takeReply();
    completionHandler(WTFMove(result));
}

void WebPageProxy::requestAllTextAndRects(CompletionHandler<void(Vector<Ref<API::TextRun>>&&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebPage::RequestAllTextAndRects(), [completion = WTFMove(completion), weakThis = WeakPtr { *this }](auto&& textAndRects) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completion({ });

        completion(WTF::map(WTFMove(textAndRects), [protectedThis](auto&& textAndRect) {
            return API::TextRun::create(*protectedThis, WTFMove(textAndRect.first), WTFMove(textAndRect.second));
        }));
    });
}

void WebPageProxy::requestTargetedElement(const API::TargetedElementRequest& request, CompletionHandler<void(const Vector<Ref<API::TargetedElementInfo>>&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebPage::RequestTargetedElement(request.makeRequest(*this)), [completion = WTFMove(completion), weakThis = WeakPtr { *this }](Vector<WebCore::TargetedElementInfo>&& elements) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completion({ });

        completion(WTF::map(WTFMove(elements), [protectedThis](auto&& element) {
            return API::TargetedElementInfo::create(*protectedThis, WTFMove(element));
        }));
    });
}

void WebPageProxy::requestAllTargetableElements(float hitTestingInterval, CompletionHandler<void(Vector<Vector<Ref<API::TargetedElementInfo>>>&&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebPage::RequestAllTargetableElements(hitTestingInterval), [completion = WTFMove(completion), weakThis = WeakPtr { *this }](Vector<Vector<WebCore::TargetedElementInfo>>&& elements) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completion({ });

        completion(WTF::map(WTFMove(elements), [protectedThis](auto&& subelements) {
            return WTF::map(WTFMove(subelements), [protectedThis](auto&& subelement) {
                return API::TargetedElementInfo::create(*protectedThis, WTFMove(subelement));
            });
        }));
    });
}

void WebPageProxy::takeSnapshotForTargetedElement(const API::TargetedElementInfo& info, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebPage::TakeSnapshotForTargetedElement(info.nodeIdentifier(), info.documentIdentifier()), WTFMove(completion));
}

void WebPageProxy::requestTextExtraction(WebCore::TextExtraction::Request&& request, CompletionHandler<void(WebCore::TextExtraction::Item&&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });
    sendWithAsyncReply(Messages::WebPage::RequestTextExtraction(WTFMove(request)), WTFMove(completion));
}

void WebPageProxy::handleTextExtractionInteraction(TextExtraction::Interaction&& interaction, CompletionHandler<void(bool, String&&)>&& completion)
{
    if (!hasRunningProcess()) {
        ASSERT_NOT_REACHED();
        return completion(false, "Internal inconsistency / unexpected state. Please file a bug"_s);
    }

    sendWithAsyncReply(Messages::WebPage::HandleTextExtractionInteraction(WTFMove(interaction)), WTFMove(completion));
}

void WebPageProxy::takeSnapshotOfExtractedText(TextExtraction::ExtractedText&& extractedText, CompletionHandler<void(RefPtr<TextIndicator>&&)>&& completion)
{
    if (!hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebPage::TakeSnapshotOfExtractedText(WTFMove(extractedText)), WTFMove(completion));
}

void WebPageProxy::describeTextExtractionInteraction(TextExtraction::Interaction&& interaction, CompletionHandler<void(TextExtraction::InteractionDescription&&)>&& completion)
{
    if (!hasRunningProcess()) {
        ASSERT_NOT_REACHED();
        return completion({ "Internal inconsistency / unexpected state. Please file a bug"_s, { } });
    }

    sendWithAsyncReply(Messages::WebPage::DescribeTextExtractionInteraction(WTFMove(interaction)), WTFMove(completion));
}

void WebPageProxy::updateTextExtractionFilterRules(Vector<WebCore::TextExtraction::FilterRuleData>&& rules)
{
    send(Messages::WebPage::UpdateTextExtractionFilterRules(WTFMove(rules)));
}

void WebPageProxy::applyTextExtractionFilter(const String& input, std::optional<NodeIdentifier>&& containerNodeID, CompletionHandler<void(String&&)>&& completion)
{
    sendWithAsyncReply(Messages::WebPage::ApplyTextExtractionFilter(input, WTFMove(containerNodeID)), WTFMove(completion));
}

void WebPageProxy::addConsoleMessage(FrameIdentifier frameID, MessageSource messageSource, MessageLevel messageLevel, const String& message, std::optional<ResourceLoaderIdentifier> coreIdentifier)
{
    sendToProcessContainingFrame(frameID, Messages::WebPage::AddConsoleMessage { frameID, messageSource, messageLevel, message, coreIdentifier });
}

#if PLATFORM(COCOA)
void WebPageProxy::sendScrollUpdateForNode(std::optional<WebCore::FrameIdentifier> frameID, WebCore::ScrollUpdate update, bool isLastUpdate)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::RemoteScrollingCoordinator::ScrollUpdateForNode(update), [weakThis = WeakPtr { *m_scrollingCoordinatorProxy }, isLastUpdate] {
        if (!weakThis)
            return;

        if (isLastUpdate)
            weakThis->receivedLastScrollingTreeNodeUpdateReply();
    });
}
#endif

void WebPageProxy::bindRemoteAccessibilityFrames(int processIdentifier, WebCore::FrameIdentifier frameID, Vector<uint8_t>&& dataToken, CompletionHandler<void(Vector<uint8_t>, int)>&& completionHandler)
{
    auto sendResult = sendSyncToProcessContainingFrame(frameID, Messages::WebPage::BindRemoteAccessibilityFrames(processIdentifier, frameID, WTFMove(dataToken)));
    if (!sendResult.succeeded())
        return completionHandler({ }, 0);

    auto [frameDataToken, frameProcessIdentifier] = sendResult.takeReply();
    completionHandler(frameDataToken, frameProcessIdentifier);
}

void WebPageProxy::updateRemoteFrameAccessibilityOffset(WebCore::FrameIdentifier frameID, WebCore::IntPoint offset)
{
    sendToProcessContainingFrame(frameID, Messages::WebPage::UpdateRemotePageAccessibilityOffset(frameID, offset));
}

void WebPageProxy::documentURLForConsoleLog(WebCore::FrameIdentifier frameID, CompletionHandler<void(const URL&)>&& completionHandler)
{
    // FIXME: <rdar://125885582> Respond with an empty string if there's no inspector and no test runner.
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        return completionHandler(frame->url());
    completionHandler({ });
}

void WebPageProxy::reportMixedContentViolation(FrameIdentifier frameID, bool blocked, const URL& target)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    auto isUpgradingLocalhostDisabled = !protectedPreferences()->iPAddressAndLocalhostMixedContentUpgradeTestingEnabled() && shouldTreatAsPotentiallyTrustworthy(target);
    ASCIILiteral errorString = [&] {
        if (blocked)
            return "blocked and must"_s;
        if (isUpgradingLocalhostDisabled)
            return "not upgraded to HTTPS and must be served from the local host."_s;
        return "automatically upgraded and should"_s;
    }();

    auto message = makeString((!blocked ? ""_s : "[blocked] "_s), "The page at "_s, frame->url().stringCenterEllipsizedToLength(), " requested insecure content from "_s, target.stringCenterEllipsizedToLength(), ". This content was "_s, errorString, !isUpgradingLocalhostDisabled ? " be served over HTTPS.\n"_s : "\n"_s);
    addConsoleMessage(frameID, MessageSource::Security, MessageLevel::Warning, message);
}

void WebPageProxy::drawFrameToSnapshot(FrameIdentifier frameID, const IntRect& rect, RemoteSnapshotIdentifier snapshotIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawFrameToSnapshot(frameID, rect, snapshotIdentifier), WTFMove(completionHandler));
}

void WebPageProxy::resetVisibilityAdjustmentsForTargetedElements(const Vector<Ref<API::TargetedElementInfo>>& elements, CompletionHandler<void(bool)>&& completion)
{
    if (!hasRunningProcess())
        return completion(false);

    sendWithAsyncReply(Messages::WebPage::ResetVisibilityAdjustmentsForTargetedElements(elements.map([](auto& info) -> TargetedElementIdentifiers {
        return { info->nodeIdentifier(), info->documentIdentifier() };
    })), WTFMove(completion));
}

void WebPageProxy::adjustVisibilityForTargetedElements(const Vector<Ref<API::TargetedElementInfo>>& elements, CompletionHandler<void(bool)>&& completion)
{
    if (!hasRunningProcess())
        return completion(false);

    sendWithAsyncReply(Messages::WebPage::AdjustVisibilityForTargetedElements(elements.map([](auto& info) -> TargetedElementAdjustment {
        return {
            { info->nodeIdentifier(), info->documentIdentifier() },
            info->selectors().map([](auto& selectors) {
                return HashSet<String>(selectors);
            })
        };
    })), WTFMove(completion));
}

void WebPageProxy::numberOfVisibilityAdjustmentRects(CompletionHandler<void(uint64_t)>&& completion)
{
    if (!hasRunningProcess())
        return completion(0);

    sendWithAsyncReply(Messages::WebPage::NumberOfVisibilityAdjustmentRects(), WTFMove(completion));
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void WebPageProxy::setDefaultSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (spatialTrackingLabel == m_defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = spatialTrackingLabel;
    updateDefaultSpatialTrackingLabel();
}

String WebPageProxy::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void WebPageProxy::updateDefaultSpatialTrackingLabel()
{
    auto effectiveSpatialTrackingLabel = [&] {
        // Use the explicitly set tracking label, if set.
        if (!m_defaultSpatialTrackingLabel.isNull())
            return m_defaultSpatialTrackingLabel;

        // Otherwise, use the pageClient's tracking label, if the page is parented.
        RefPtr pageClient = this->pageClient();
        if (pageClient && pageClient->isViewInWindow())
            return pageClient->spatialTrackingLabel();

        // If there is no explicit tracking label, and the view is unparented, use a null tracking label.
        return nullString();
    };
    send(Messages::WebPage::SetDefaultSpatialTrackingLabel(effectiveSpatialTrackingLabel()));
}
#endif

void WebPageProxy::addNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(!m_nowPlayingMetadataObservers.contains(observer));
    if (m_nowPlayingMetadataObservers.isEmptyIgnoringNullReferences())
        send(Messages::WebPage::StartObservingNowPlayingMetadata());
    m_nowPlayingMetadataObservers.add(observer);
}

void WebPageProxy::removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.remove(observer);
    if (m_nowPlayingMetadataObservers.isEmptyIgnoringNullReferences())
        send(Messages::WebPage::StopObservingNowPlayingMetadata());
}

void WebPageProxy::setNowPlayingMetadataObserverForTesting(RefPtr<WebCore::NowPlayingMetadataObserver>&& observer)
{
    if (RefPtr previousObserver = std::exchange(m_nowPlayingMetadataObserverForTesting, nullptr))
        removeNowPlayingMetadataObserver(*previousObserver);

    m_nowPlayingMetadataObserverForTesting = WTFMove(observer);

    if (RefPtr observer = m_nowPlayingMetadataObserverForTesting)
        addNowPlayingMetadataObserver(*observer);
}

void WebPageProxy::nowPlayingMetadataChanged(const WebCore::NowPlayingMetadata& metadata)
{
    m_nowPlayingMetadataObservers.forEach([&](auto& observer) {
        observer(metadata);
    });
}

void WebPageProxy::simulateClickOverFirstMatchingTextInViewportWithUserInteraction(String&& targetText, CompletionHandler<void(bool)>&& completion)
{
    sendWithAsyncReply(Messages::WebPage::SimulateClickOverFirstMatchingTextInViewportWithUserInteraction(WTFMove(targetText)), WTFMove(completion));
}

void WebPageProxy::didAdjustVisibilityWithSelectors(Vector<String>&& selectors)
{
    m_uiClient->didAdjustVisibilityWithSelectors(*this, WTFMove(selectors));
}

void WebPageProxy::frameNameChanged(IPC::Connection& connection, WebCore::FrameIdentifier frameID, const String& frameName)
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (!webProcess.hasConnection() || &webProcess.connection() == &connection)
            return;
        webProcess.send(Messages::WebPage::FrameNameWasChangedInAnotherProcess(frameID, frameName), pageID);
    });
}

void WebPageProxy::hasActiveNowPlayingSessionChanged(bool hasActiveNowPlayingSession)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->hasActiveNowPlayingSessionChanged(hasActiveNowPlayingSession);
}

void WebPageProxy::setAllowsLayoutViewportHeightExpansion(bool value)
{
    if (internals().allowsLayoutViewportHeightExpansion == value)
        return;

    internals().allowsLayoutViewportHeightExpansion = value;
    if (RefPtr pageClient = this->pageClient())
        pageClient->scheduleVisibleContentRectUpdate();
}

void WebPageProxy::closeCurrentTypingCommand()
{
    if (hasRunningProcess())
        send(Messages::WebPage::CloseCurrentTypingCommand());
}

Ref<BrowsingContextGroup> WebPageProxy::protectedBrowsingContextGroup() const
{
    return m_browsingContextGroup;
}

bool WebPageProxy::isAlwaysOnLoggingAllowed() const
{
    return sessionID().isAlwaysOnLoggingAllowed() || protectedPreferences()->allowPrivacySensitiveOperationsInNonPersistentDataStores();
}

#if PLATFORM(COCOA)

FloatPoint WebPageProxy::mainFrameScrollPosition() const
{
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        return scrollingCoordinatorProxy->currentMainFrameScrollPosition();

    return { };
}

#endif // PLATFORM(COCOA)

void WebPageProxy::fetchSessionStorage(CompletionHandler<void(std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>>&&)>&& completionHandler)
{
    if (RefPtr networkProcess = websiteDataStore().networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::NetworkProcess::FetchSessionStorage(sessionID(), identifier()), WTFMove(completionHandler));
    else
        completionHandler(HashMap<WebCore::ClientOrigin, HashMap<String, String>> { });
}

void WebPageProxy::restoreSessionStorage(HashMap<WebCore::ClientOrigin, HashMap<String, String>>&& sessionStorage, CompletionHandler<void(bool)>&& completionHandler)
{
    protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::RestoreSessionStorage(sessionID(), identifier(), WTFMove(sessionStorage)), WTFMove(completionHandler));
}

#if HAVE(AUDIT_TOKEN)
const std::optional<audit_token_t>& WebPageProxy::presentingApplicationAuditToken() const
{
    return m_presentingApplicationAuditToken;
}

void WebPageProxy::setPresentingApplicationAuditToken(const audit_token_t& presentingApplicationAuditToken)
{
    m_presentingApplicationAuditToken = presentingApplicationAuditToken;

    if (hasRunningProcess())
        send(Messages::WebPage::SetPresentingApplicationAuditTokenAndBundleIdentifier(presentingApplicationAuditToken, presentingApplicationBundleIdentifier()));

    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->setPresentingApplicationAuditToken(protectedLegacyMainFrameProcess()->coreProcessIdentifier(), m_webPageID, presentingApplicationAuditToken);
}
#endif

bool WebPageProxy::canStartNavigationSwipeAtLastInteractionLocation() const
{
    RefPtr client = pageClient();
    return !client || client->canStartNavigationSwipeAtLastInteractionLocation();
}

Ref<AboutSchemeHandler> WebPageProxy::protectedAboutSchemeHandler()
{
    return m_aboutSchemeHandler;
}

bool WebPageProxy::isRemoteFrameNavigation(Ref<WebProcessProxy> process)
{
    RefPtr provisionalPage = m_provisionalPage;
    return m_legacyMainFrameProcess != process && (!provisionalPage || provisionalPage->process() != process);
}

// See SwiftDemoLogo.swift for the rationale here
bool shouldShowSwiftDemoLogo()
{
#if ENABLE(SWIFT_DEMO_URI_SCHEME)
    return true;
#else
    // This shouldn't even be called if ENABLE_SWIFT_DEMO_URI_SCHEME
    // isn't enabled
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

} // namespace WebKit

#undef WEBPAGEPROXY_RELEASE_LOG
#undef WEBPAGEPROXY_RELEASE_LOG_ERROR
#undef MESSAGE_CHECK_URL_COMPLETION
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
