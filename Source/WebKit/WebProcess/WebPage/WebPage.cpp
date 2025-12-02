/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "WebPage.h"

#include "APIArray.h"
#include "APIGeometry.h"
#include "APIInjectedBundleEditorClient.h"
#include "APIInjectedBundleFormClient.h"
#include "APIInjectedBundlePageContextMenuClient.h"
#include "APIInjectedBundlePageLoaderClient.h"
#include "APIInjectedBundlePageResourceLoadClient.h"
#include "APIInjectedBundlePageUIClient.h"
#include "Connection.h"
#include "ContentAsStringIncludesChildFrames.h"
#include "DragControllerAction.h"
#include "DrawingArea.h"
#include "DrawingAreaMessages.h"
#include "EditorState.h"
#include "EventDispatcher.h"
#include "FindController.h"
#include "FocusedElementInformation.h"
#include "FormDataReference.h"
#include "FrameTreeNodeData.h"
#include "GeolocationPermissionRequestManager.h"
#include "GoToBackForwardItemParameters.h"
#include "ImageOptions.h"
#include "InjectUserScriptImmediately.h"
#include "InjectedBundle.h"
#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleScriptWorld.h"
#include "JavaScriptEvaluationResult.h"
#include "LibWebRTCCodecs.h"
#include "LibWebRTCProvider.h"
#include "LoadParameters.h"
#include "Logging.h"
#include "MediaKeySystemPermissionRequestManager.h"
#include "MediaPlaybackState.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NodeHitTestResult.h"
#include "NotificationPermissionRequestManager.h"
#include "PageBanner.h"
#include "PluginView.h"
#include "PolicyDecision.h"
#include "PrintInfo.h"
#include "ProvisionalFrameCreationParameters.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteScrollingCoordinator.h"
#include "RemoteSnapshotRecorderProxy.h"
#include "RemoteWebInspectorUI.h"
#include "RemoteWebInspectorUIMessages.h"
#include "RunJavaScriptParameters.h"
#include "SessionState.h"
#include "SessionStateConversion.h"
#include "ShareableBitmapUtilities.h"
#include "SharedBufferReference.h"
#include "TextRecognitionUpdateResult.h"
#include "UserMediaPermissionRequestManager.h"
#include "ViewGestureGeometryCollector.h"
#include "VisitedLinkTableController.h"
#include "WKBundleAPICast.h"
#include "WKRetainPtr.h"
#include "WKSharedAPICast.h"
#include "WebAlternativeTextClient.h"
#include "WebAttachmentElementClient.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListProxy.h"
#include "WebBadgeClient.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCacheStorageProvider.h"
#include "WebChromeClient.h"
#include "WebColorChooser.h"
#include "WebContextMenu.h"
#include "WebContextMenuClient.h"
#include "WebCookieJar.h"
#include "WebCryptoClient.h"
#include "WebDataListSuggestionPicker.h"
#include "WebDatabaseProvider.h"
#include "WebDateTimeChooser.h"
#include "WebDiagnosticLoggingClient.h"
#include "WebDocumentSyncClient.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebErrors.h"
#include "WebEventConversion.h"
#include "WebEventFactory.h"
#include "WebFoundTextRange.h"
#include "WebFoundTextRangeController.h"
#include "WebFrame.h"
#include "WebFrameMetrics.h"
#include "WebFullScreenManager.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebGamepadProvider.h"
#include "WebGeolocationClient.h"
#include "WebHistoryItemClient.h"
#include "WebHitTestResultData.h"
#include "WebImage.h"
#include "WebInspectorBackend.h"
#include "WebInspectorBackendClient.h"
#include "WebInspectorBackendMessages.h"
#include "WebInspectorUI.h"
#include "WebInspectorUIMessages.h"
#include "WebKeyboardEvent.h"
#include "WebLoaderStrategy.h"
#include "WebLocalFrameLoaderClient.h"
#include "WebMediaKeyStorageManager.h"
#include "WebMediaKeySystemClient.h"
#include "WebMediaStrategy.h"
#include "WebModelPlayerProvider.h"
#include "WebMouseEvent.h"
#include "WebNotificationClient.h"
#include "WebOpenPanelResultListener.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroupProxy.h"
#include "WebPageInlines.h"
#include "WebPageInspectorTarget.h"
#include "WebPageInternals.h"
#include "WebPageMessages.h"
#include "WebPageOverlay.h"
#include "WebPageProxyMessages.h"
#include "WebPageTesting.h"
#include "WebPaymentCoordinator.h"
#include "WebPerformanceLoggingClient.h"
#include "WebPluginInfoProvider.h"
#include "WebPopupMenu.h"
#include "WebPreferencesDefinitions.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebProgressTrackerClient.h"
#include "WebRemoteFrameClient.h"
#include "WebScreenOrientationManager.h"
#include "WebServiceWorkerProvider.h"
#include "WebSocketProvider.h"
#include "WebSpeechRecognitionProvider.h"
#include "WebSpeechSynthesisClient.h"
#include "WebStorageNamespaceProvider.h"
#include "WebStorageProvider.h"
#include "WebTouchEvent.h"
#include "WebURLSchemeHandlerProxy.h"
#include "WebUndoStep.h"
#include "WebUserContentController.h"
#include "WebUserMediaClient.h"
#include "WebValidationMessageClient.h"
#include "WebWheelEvent.h"
#include "WebsiteDataStoreParameters.h"
#include "WebsitePoliciesData.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ProfilerDatabase.h>
#include <JavaScriptCore/SamplingProfiler.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/AnimationTimelinesController.h>
#include <WebCore/AppHighlight.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/BackForwardCache.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/CachedPage.h>
#include <WebCore/Chrome.h>
#include <WebCore/CommonVM.h>
#include <WebCore/ContactsRequestData.h>
#include <WebCore/ContainerNodeInlines.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/CrossOriginOpenerPolicy.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/DataTransfer.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DisplayListRecorderImpl.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DocumentFullscreen.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DocumentMarkers.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentQuirks.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/DocumentView.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/Editing.h>
#include <WebCore/Editor.h>
#include <WebCore/ElementAncestorIteratorInlines.h>
#include <WebCore/ElementTargetingController.h>
#include <WebCore/EventHandler.h>
#include <WebCore/EventNames.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/File.h>
#include <WebCore/FocusController.h>
#include <WebCore/FocusControllerTypes.h>
#include <WebCore/FocusOptions.h>
#include <WebCore/FontAttributeChanges.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/FormState.h>
#include <WebCore/FragmentDirectiveParser.h>
#include <WebCore/FragmentDirectiveRangeFinder.h>
#include <WebCore/FragmentDirectiveUtilities.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/HTMLAttachmentElement.h>
#include <WebCore/HTMLBodyElement.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLImageElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLModelElement.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/HTMLTextFormControlElement.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/HTTPStatusCodes.h>
#include <WebCore/HandleUserInputEventResult.h>
#include <WebCore/Highlight.h>
#include <WebCore/HighlightRegistry.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/ImageAnalysisQueue.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/JSNode.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/LoginStatus.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaDocument.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NavigationScheduler.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/NotificationController.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PageInspectorController.h>
#include <WebCore/PingLoader.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PointerCaptureController.h>
#include <WebCore/PopupMenuClient.h>
#include <WebCore/PrintContext.h>
#include <WebCore/ProcessCapabilities.h>
#include <WebCore/PromisedAttachmentInfo.h>
#include <WebCore/Quirks.h>
#include <WebCore/Range.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RemoteDOMWindow.h>
#include <WebCore/RemoteFrame.h>
#include <WebCore/RemoteFrameClient.h>
#include <WebCore/RemoteFrameGeometryTransformer.h>
#include <WebCore/RemoteFrameView.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <WebCore/RenderImage.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderVideoInlines.h>
#include <WebCore/RenderView.h>
#include <WebCore/Report.h>
#include <WebCore/ReportingScope.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RunJavaScriptParameters.h>
#include <WebCore/SWClientConnection.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScriptDisallowedScope.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/SelectionRestorationMode.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/Settings.h>
#include <WebCore/ShadowRoot.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/StaticRange.h>
#include <WebCore/StyleProperties.h>
#include <WebCore/SubframeLoader.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/SystemPreviewInfo.h>
#include <WebCore/TextExtraction.h>
#include <WebCore/TextIterator.h>
#include <WebCore/TextManipulationController.h>
#include <WebCore/TextRecognitionOptions.h>
#include <WebCore/TranslationContextMenuInfo.h>
#include <WebCore/UserContentURLPattern.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/UserTypingGestureIndicator.h>
#include <WebCore/ViolationReportType.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/VisibleUnits.h>
#include <WebCore/WebKitJSHandle.h>
#include <WebCore/WritingDirection.h>
#include <WebCore/markup.h>
#include <algorithm>
#include <pal/SessionID.h>
#include <ranges>
#include <wtf/CoroutineUtilities.h>
#include <wtf/ProcessID.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

#if ENABLE(APP_HIGHLIGHTS)
#include <WebCore/AppHighlightStorage.h>
#endif

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResult.h"
#endif

#if ENABLE(MHTML)
#include <WebCore/MHTMLArchive.h>
#endif

#if ENABLE(POINTER_LOCK)
#include <WebCore/PointerLockController.h>
#endif

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#include "InsertTextOptions.h"
#include "PlaybackSessionManager.h"
#include "RemoteLayerTreeDrawingArea.h"
#include "RemoteObjectRegistryMessages.h"
#include "TextAnimationController.h"
#include "TextCheckingControllerProxy.h"
#include "VideoPresentationManager.h"
#include "WKStringCF.h"
#include "WebRemoteObjectRegistry.h"
#include <WebCore/ImageUtilities.h>
#include <WebCore/LegacyWebArchive.h>
#include <WebCore/VP9UtilitiesCocoa.h>
#include <pal/spi/cg/ImageIOSPI.h>
#include <wtf/MachSendRight.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#include <wtf/spi/darwin/SandboxSPI.h>
#endif

#if PLATFORM(GTK)
#include "WebPrintOperationGtk.h"
#include <WebCore/SelectionData.h>
#include <gtk/gtk.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "InteractionInformationAtPosition.h"
#include "InteractionInformationRequest.h"
#include "WebAutocorrectionContext.h"
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/Icon.h>
#include <pal/spi/cf/CoreTextSPI.h>
#endif

#if PLATFORM(MAC)
#include <WebCore/LocalDefaultSystemAppearance.h>
#include <pal/spi/cf/CFUtilitiesSPI.h>
#endif

#if ENABLE(DATA_DETECTION)
#include <WebCore/DataDetection.h>
#include <WebCore/DataDetectionResultsStorage.h>
#endif

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#endif

#if ENABLE(WEB_AUTHN)
#include "WebAuthenticatorCoordinator.h"
#include <WebCore/AuthenticatorCoordinator.h>

#if HAVE(DIGITAL_CREDENTIALS_UI)
#include "DigitalCredentialsCoordinator.h"
#include <WebCore/DigitalCredentialsRequestData.h>
#include <WebCore/DigitalCredentialsResponseData.h>
#include <WebCore/ExceptionData.h>
#endif // HAVE(DIGITAL_CREDENTIALS_UI)
#endif // ENABLE(WEB_AUTHN)

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
#include "WebDeviceOrientationUpdateProvider.h"
#endif

#if ENABLE(GPU_PROCESS)
#include "RemoteMediaPlayerManager.h"
#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMFactory.h"
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "RemoteLegacyCDMFactory.h"
#endif
#if HAVE(AVASSETREADER)
#include "RemoteImageDecoderAVFManager.h"
#endif
#endif

#if ENABLE(IMAGE_ANALYSIS)
#include <WebCore/TextRecognitionResult.h>
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#include "RemoteMediaSessionCoordinator.h"
#include <WebCore/MediaSessionCoordinator.h>
#include <WebCore/NavigatorMediaSession.h>
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#include "ARKitInlinePreviewModelPlayerIOS.h"
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#include "WebPreferencesDefaultValuesIOS.h"
#endif

#if ENABLE(MODEL_PROCESS)
#include "ModelProcessConnection.h"
#endif

#if USE(SKIA)
#include <WebCore/FontRenderOptions.h>
#endif

#if USE(CG)
// FIXME: Move the CG-specific PDF painting code out of WebPage.cpp.
#include <WebCore/GraphicsContextCG.h>
#endif

#if ENABLE(LOCKDOWN_MODE_API)
#import <pal/spi/cg/CoreGraphicsSPI.h>
#endif

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
#include "WebExtensionControllerProxy.h"
#endif

#if ENABLE(WEBXR)
#include "PlatformXRSystemProxy.h"
#endif

#if ENABLE(PDF_HUD)
#include "PDFPluginBase.h"
#endif

#if HAVE(AUDIT_TOKEN)
#include "CoreIPCAuditToken.h"
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
#include "RemoteMediaSessionManager.h"
#endif

#if __has_include(<WebKitAdditions/WebPreferencesDefaultValuesAdditions.h>)
#include <WebKitAdditions/WebPreferencesDefaultValuesAdditions.h>
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

static const Seconds pageScrollHysteresisDuration { 300_ms };
static const Seconds initialLayerVolatilityTimerInterval { 20_ms };
static const Seconds maximumLayerVolatilityTimerInterval { 2_s };

#if PLATFORM(IOS_FAMILY)
static constexpr Seconds updateFocusedElementInformationDebounceInterval { 100_ms };
static constexpr Seconds updateLayoutViewportHeightExpansionTimerInterval { 200_ms };
#endif

#define WEBPAGE_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)
#define WEBPAGE_RELEASE_LOG_FORWARDABLE(channel, fmt, ...) RELEASE_LOG_FORWARDABLE(channel, fmt, m_identifier.toUInt64(), ##__VA_ARGS__)
#define WEBPAGE_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)

class SendStopResponsivenessTimer {
public:
    ~SendStopResponsivenessTimer()
    {
        WebProcess::singleton().protectedParentProcessConnection()->send(Messages::WebProcessProxy::StopResponsivenessTimer(), 0);
    }
};

Ref<WebPage> WebPage::create(PageIdentifier pageID, WebPageCreationParameters&& parameters)
{
    String openedMainFrameName = parameters.openedMainFrameName;
    auto page = adoptRef(*new WebPage(pageID, WTFMove(parameters)));

    if (RefPtr injectedBundle = WebProcess::singleton().injectedBundle())
        injectedBundle->didCreatePage(page);

    page->corePage()->mainFrame().tree().setSpecifiedName(AtomString(openedMainFrameName));

#if HAVE(SANDBOX_STATE_FLAGS)
    setHasLaunchedWebContentProcess();
#endif

    return page;
}

void WebPage::ref() const
{
    API::ObjectImpl<API::Object::Type::BundlePage>::ref();
}

void WebPage::deref() const
{
    API::ObjectImpl<API::Object::Type::BundlePage>::deref();
}

static Vector<UserContentURLPattern> parseAndAllowAccessToCORSDisablingPatterns(const Vector<String>& input)
{
    return WTF::compactMap(input, [](auto& pattern) -> std::optional<UserContentURLPattern> {
        UserContentURLPattern parsedPattern(pattern);
        if (!parsedPattern.isValid())
            return std::nullopt;
        WebCore::OriginAccessPatternsForWebProcess::singleton().allowAccessTo(parsedPattern);
        return parsedPattern;
    });
}

static PageConfiguration::MainFrameCreationParameters mainFrameCreationParameters(Ref<WebFrame>&& mainFrame, auto frameType, auto initialSandboxFlags, auto initialReferrerPolicy)
{
    auto invalidator = mainFrame->makeInvalidator();
    switch (frameType) {
    case Frame::FrameType::Local:
        return PageConfiguration::LocalMainFrameCreationParameters {
            { [mainFrame = WTFMove(mainFrame), invalidator = WTFMove(invalidator)] (auto& localFrame, auto& frameLoader) mutable {
                return makeUniqueRefWithoutRefCountedCheck<WebLocalFrameLoaderClient>(localFrame, frameLoader, WTFMove(mainFrame), WTFMove(invalidator));
            } },
            initialSandboxFlags,
            initialReferrerPolicy
        };
    case Frame::FrameType::Remote:
        return CompletionHandler<UniqueRef<RemoteFrameClient>(RemoteFrame&)> { [mainFrame = WTFMove(mainFrame), invalidator = WTFMove(invalidator)] (auto&) mutable {
            return makeUniqueRef<WebRemoteFrameClient>(WTFMove(mainFrame), WTFMove(invalidator));
        } };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static RefPtr<Frame> frameFromIdentifier(std::optional<FrameIdentifier> identifier)
{
    if (!identifier)
        return nullptr;
    RefPtr webFrame = WebProcess::singleton().webFrame(*identifier);
    if (!webFrame)
        return nullptr;
    return webFrame->coreFrame();
}

WebPage::WebPage(PageIdentifier pageID, WebPageCreationParameters&& parameters)
    : m_internals(makeUniqueRef<Internals>())
    , m_identifier(pageID)
    , m_viewSize(parameters.viewSize)
    , m_drawingArea(DrawingArea::create(*this, parameters))
    , m_webPageTesting(WebPageTesting::create(*this))
    , m_mainFrame(WebFrame::create(*this, parameters.mainFrameIdentifier))
#if ENABLE(TILED_CA_DRAWING_AREA)
    , m_drawingAreaType(parameters.drawingAreaType)
#endif
    , m_alwaysShowsHorizontalScroller { parameters.alwaysShowsHorizontalScroller }
    , m_alwaysShowsVerticalScroller { parameters.alwaysShowsVerticalScroller }
    , m_shouldRenderCanvasInGPUProcess { parameters.shouldRenderCanvasInGPUProcess }
    , m_shouldRenderDOMInGPUProcess { parameters.shouldRenderDOMInGPUProcess }
    , m_shouldPlayMediaInGPUProcess { parameters.shouldPlayMediaInGPUProcess }
#if ENABLE(WEBGL)
    , m_shouldRenderWebGLInGPUProcess { parameters.shouldRenderWebGLInGPUProcess }
#endif
    , m_shouldSendConsoleLogsToUIProcessForTesting(parameters.shouldSendConsoleLogsToUIProcessForTesting)
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    , m_textCheckingControllerProxy(makeUniqueRefWithoutRefCountedCheck<TextCheckingControllerProxy>(*this))
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK)
    , m_viewGestureGeometryCollector(ViewGestureGeometryCollector::create(*this))
#elif PLATFORM(GTK)
    , m_accessibilityObject(nullptr)
#endif
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    , m_nativeWindowHandle(parameters.nativeWindowHandle)
#endif
    , m_setCanStartMediaTimer(RunLoop::mainSingleton(), "WebPage::SetCanStartMediaTimer"_s, this, &WebPage::setCanStartMediaTimerFired)
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuClient(makeUnique<API::InjectedBundle::PageContextMenuClient>())
#endif
    , m_editorClient { makeUnique<API::InjectedBundle::EditorClient>() }
    , m_formClient(makeUnique<API::InjectedBundle::FormClient>())
    , m_loaderClient(makeUnique<API::InjectedBundle::PageLoaderClient>())
    , m_resourceLoadClient(makeUnique<API::InjectedBundle::ResourceLoadClient>())
    , m_uiClient(makeUnique<API::InjectedBundle::PageUIClient>())
    , m_findController(makeUniqueRef<FindController>(this))
    , m_foundTextRangeController(makeUniqueRef<WebFoundTextRangeController>(*this))
    , m_userContentController(WebUserContentController::getOrCreate(WTFMove(parameters.userContentControllerParameters)))
    , m_screenOrientationManager(makeUniqueRefWithoutRefCountedCheck<WebScreenOrientationManager>(*this))
#if ENABLE(GEOLOCATION)
    , m_geolocationPermissionRequestManager(makeUniqueRefWithoutRefCountedCheck<GeolocationPermissionRequestManager>(*this))
#endif
#if ENABLE(MEDIA_STREAM)
    , m_userMediaPermissionRequestManager { makeUniqueRefWithoutRefCountedCheck<UserMediaPermissionRequestManager>(*this) }
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    , m_mediaKeySystemPermissionRequestManager { makeUniqueRefWithoutRefCountedCheck<MediaKeySystemPermissionRequestManager>(*this) }
#endif
    , m_pageScrolledHysteresis([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) pageStoppedScrolling(); }, pageScrollHysteresisDuration)
    , m_canRunBeforeUnloadConfirmPanel(parameters.canRunBeforeUnloadConfirmPanel)
    , m_canRunModal(parameters.canRunModal)
#if HAVE(TOUCH_BAR)
    , m_requiresUserActionForEditingControlsManager(parameters.requiresUserActionForEditingControlsManager)
#endif
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    , m_isWindowResizingEnabled(parameters.hasResizableWindows)
#endif
#if PLATFORM(MAC)
    , m_overflowHeightForTopScrollEdgeEffect(parameters.overflowHeightForTopScrollEdgeEffect)
#endif
#if ENABLE(META_VIEWPORT)
    , m_forceAlwaysUserScalable(parameters.ignoresViewportScaleLimits)
#endif
#if PLATFORM(IOS_FAMILY)
    , m_screenIsBeingCaptured(parameters.isCapturingScreen)
    , m_screenSize(parameters.screenSize)
    , m_availableScreenSize(parameters.availableScreenSize)
    , m_overrideScreenSize(parameters.overrideScreenSize)
    , m_overrideAvailableScreenSize(parameters.overrideAvailableScreenSize)
    , m_deviceOrientation(parameters.deviceOrientation)
    , m_keyboardIsAttached(parameters.hardwareKeyboardState.isAttached)
    , m_updateFocusedElementInformationTimer(*this, &WebPage::updateFocusedElementInformation, updateFocusedElementInformationDebounceInterval)
#endif
    , m_layerVolatilityTimer(*this, &WebPage::layerVolatilityTimerFired)
    , m_activityState(parameters.activityState)
    , m_userInterfaceLayoutDirection(parameters.userInterfaceLayoutDirection)
    , m_overrideContentSecurityPolicy { WTFMove(parameters.overrideContentSecurityPolicy) }
    , m_cpuLimit(parameters.cpuLimit)
#if USE(WPE_RENDERER)
    , m_hostFileDescriptor(WTFMove(parameters.hostFileDescriptor))
#endif
    , m_webPageProxyIdentifier(parameters.webPageProxyIdentifier)
#if ENABLE(TEXT_AUTOSIZING)
    , m_textAutoSizingAdjustmentTimer(*this, &WebPage::textAutoSizingAdjustmentTimerFired)
#endif
    , m_overriddenMediaType { WTFMove(parameters.overriddenMediaType) }
    , m_processDisplayName { WTFMove(parameters.processDisplayName) }
#if PLATFORM(GTK) || PLATFORM(WPE)
#if USE(GBM)
    , m_preferredBufferFormats(WTFMove(parameters.preferredBufferFormats))
#endif
#endif
#if ENABLE(APP_BOUND_DOMAINS)
    , m_limitsNavigationsToAppBoundDomains(parameters.limitsNavigationsToAppBoundDomains)
#endif
    , m_lastNavigationWasAppInitiated(parameters.lastNavigationWasAppInitiated)
#if PLATFORM(IOS_FAMILY)
    , m_updateLayoutViewportHeightExpansionTimer(*this, &WebPage::updateLayoutViewportHeightExpansionTimerFired, updateLayoutViewportHeightExpansionTimerInterval)
#endif
#if ENABLE(IPC_TESTING_API)
    , m_visitedLinkTableID(parameters.visitedLinkTableID)
#endif
#if ENABLE(APP_HIGHLIGHTS)
    , m_appHighlightsVisible(parameters.appHighlightsVisible)
#endif
    , m_historyItemClient(WebHistoryItemClient::create(*this))
#if ENABLE(WRITING_TOOLS)
    , m_textAnimationController(makeUniqueRef<TextAnimationController>(*this))
#endif
    , m_statusBarIsVisible(parameters.statusBarIsVisible)
    , m_menuBarIsVisible(parameters.menuBarIsVisible)
    , m_toolbarsAreVisible(parameters.toolbarsAreVisible)
{
    WEBPAGE_RELEASE_LOG(Loading, "constructor:");

#if PLATFORM(COCOA)
#if HAVE(SANDBOX_STATE_FLAGS)
    auto auditToken = WebProcess::singleton().auditTokenForSelf();
    auto shouldBlockWebInspector = parameters.store.getBoolValueForKey(WebPreferencesKey::blockWebInspectorInWebContentSandboxKey());
    if (shouldBlockWebInspector)
        sandbox_enable_state_flag("BlockWebInspectorInWebContentSandbox", *auditToken);
#if PLATFORM(IOS)
    auto shouldBlockMobileGestalt = parameters.store.getBoolValueForKey(WebPreferencesKey::blockMobileGestaltInWebContentSandboxKey());
    if (shouldBlockMobileGestalt)
        sandbox_enable_state_flag("BlockMobileGestaltInWebContentSandbox", *auditToken);
#endif
    auto shouldBlockMobileAsset = parameters.store.getBoolValueForKey(WebPreferencesKey::blockMobileAssetInWebContentSandboxKey());
    if (shouldBlockMobileAsset)
        sandbox_enable_state_flag("BlockMobileAssetInWebContentSandbox", *auditToken);
    auto unifiedPDFEnabled = parameters.store.getBoolValueForKey(WebPreferencesKey::unifiedPDFEnabledKey());
    if (unifiedPDFEnabled)
        sandbox_enable_state_flag("UnifiedPDFEnabled", *auditToken);
#if PLATFORM(MAC)
    auto shouldAllowInstalledFonts = parameters.store.getBoolValueForKey(WebPreferencesKey::shouldAllowUserInstalledFontsKey());
    if (!shouldAllowInstalledFonts || !WTF::MacApplication::isAppleMail())
        sandbox_enable_state_flag("BlockUserInstalledFonts", *auditToken);
    auto shouldBlockFontService = parameters.store.getBoolValueForKey(WebPreferencesKey::blockFontServiceInWebContentSandboxKey());
    if (shouldBlockFontService)
        sandbox_enable_state_flag("BlockFontServiceInWebContentSandbox", *auditToken);
    auto shouldBlockIconServices = parameters.store.getBoolValueForKey(WebPreferencesKey::blockIconServicesInWebContentSandboxKey());
    if (shouldBlockIconServices)
        sandbox_enable_state_flag("BlockIconServicesInWebContentSandbox", *auditToken);
    auto shouldBlockOpenDirectory = parameters.store.getBoolValueForKey(WebPreferencesKey::blockOpenDirectoryInWebContentSandboxKey());
    if (shouldBlockOpenDirectory)
        sandbox_enable_state_flag("BlockOpenDirectoryInWebContentSandbox", *auditToken);
#endif // PLATFORM(MAC)
#endif // HAVE(SANDBOX_STATE_FLAGS)
    auto shouldBlockIOKit = parameters.store.getBoolValueForKey(WebPreferencesKey::blockIOKitInWebContentSandboxKey())
#if ENABLE(WEBGL)
        && m_shouldRenderWebGLInGPUProcess
#if ENABLE(TILED_CA_DRAWING_AREA)
        && m_drawingAreaType == DrawingAreaType::RemoteLayerTree
#endif
#endif
        && m_shouldRenderCanvasInGPUProcess
        && m_shouldRenderDOMInGPUProcess
        && m_shouldPlayMediaInGPUProcess;

    if (shouldBlockIOKit) {
#if HAVE(SANDBOX_STATE_FLAGS) && !ENABLE(WEBCONTENT_GPU_SANDBOX_EXTENSIONS_BLOCKING)
        sandbox_enable_state_flag("BlockIOKitInWebContentSandbox", *auditToken);
#endif
        ProcessCapabilities::setHardwareAcceleratedDecodingDisabled(true);
        ProcessCapabilities::setCanUseAcceleratedBuffers(false);
        static bool disabled { false };
        if (!std::exchange(disabled, true)) {
            OSStatus ok = CGImageSourceDisableHardwareDecoding();
            ASSERT_UNUSED(ok, ok == noErr);
        }
    }
#endif

    m_pageGroup = WebProcess::singleton().webPageGroup(WTFMove(parameters.pageGroupData));

    auto frameType = parameters.remotePageParameters ? Frame::FrameType::Remote : Frame::FrameType::Local;
    ASSERT(!parameters.remotePageParameters || parameters.remotePageParameters->frameTreeParameters.frameID == parameters.mainFrameIdentifier);

    PageConfiguration pageConfiguration(
        pageID,
        WebProcess::singleton().sessionID(),
        makeUniqueRef<WebEditorClient>(*this),
        WebSocketProvider::create(parameters.webPageProxyIdentifier),
        createLibWebRTCProvider(*this),
        WebProcess::singleton().cacheStorageProvider(),
        m_userContentController,
        WebBackForwardListProxy::create(*this),
        WebProcess::singleton().cookieJar(),
        makeUniqueRef<WebProgressTrackerClient>(*this),
        mainFrameCreationParameters(m_mainFrame.copyRef(), frameType, parameters.initialSandboxFlags, parameters.initialReferrerPolicy),
        m_mainFrame->frameID(),
        frameFromIdentifier(parameters.mainFrameOpenerIdentifier),
        makeUniqueRef<WebSpeechRecognitionProvider>(pageID),
        WebProcess::singleton().broadcastChannelRegistry(),
        makeUniqueRef<WebStorageProvider>(WebProcess::singleton().mediaKeysStorageDirectory(), WebProcess::singleton().mediaKeysStorageSalt()),
        WebModelPlayerProvider::create(*this),
        WebProcess::singleton().badgeClient(),
        m_historyItemClient.copyRef(),
#if ENABLE(CONTEXT_MENUS)
        makeUniqueRef<WebContextMenuClient>(this),
#endif
#if ENABLE(APPLE_PAY)
        WebPaymentCoordinator::create(*this),
#endif
        makeUniqueRef<WebChromeClient>(*this),
        makeUniqueRef<WebCryptoClient>(this->identifier()),
        makeUniqueRef<WebDocumentSyncClient>(*this)
#if HAVE(DIGITAL_CREDENTIALS_UI)
        , DigitalCredentialsCoordinator::create(*this)
#endif
    );

#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient = makeUnique<WebDragClient>(this);
#endif
    pageConfiguration.inspectorBackendClient = makeUnique<WebInspectorBackendClient>(this);
#if USE(AUTOCORRECTION_PANEL)
    pageConfiguration.alternativeTextClient = makeUnique<WebAlternativeTextClient>(this);
#endif

    pageConfiguration.diagnosticLoggingClient = makeUnique<WebDiagnosticLoggingClient>(*this);
    pageConfiguration.performanceLoggingClient = makeUnique<WebPerformanceLoggingClient>(*this);
    pageConfiguration.screenOrientationManager = m_screenOrientationManager.get();

#if ENABLE(SPEECH_SYNTHESIS) && !USE(GSTREAMER)
    pageConfiguration.speechSynthesisClient = WebSpeechSynthesisClient::create(*this);
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK)
    pageConfiguration.validationMessageClient = makeUnique<WebValidationMessageClient>(*this);
#endif

    pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroup->pageGroupID());
    pageConfiguration.pluginInfoProvider = WebPluginInfoProvider::singleton();
    pageConfiguration.storageNamespaceProvider = WebStorageNamespaceProvider::getOrCreate();
    pageConfiguration.visitedLinkStore = VisitedLinkTableController::getOrCreate(parameters.visitedLinkTableID);

#if ENABLE(WEB_AUTHN)
    pageConfiguration.authenticatorCoordinatorClient = makeUnique<WebAuthenticatorCoordinator>(*this);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    pageConfiguration.applicationManifest = WTFMove(parameters.applicationManifest);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    pageConfiguration.deviceOrientationUpdateProvider = WebDeviceOrientationUpdateProvider::create(*this);
#endif

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (parameters.webExtensionControllerParameters)
        m_webExtensionController = WebExtensionControllerProxy::getOrCreate(parameters.webExtensionControllerParameters.value(), this);
#endif

    m_corsDisablingPatterns = WTFMove(parameters.corsDisablingPatterns);
    if (!m_corsDisablingPatterns.isEmpty())
        synchronizeCORSDisablingPatternsWithNetworkProcess();
    pageConfiguration.corsDisablingPatterns = parseAndAllowAccessToCORSDisablingPatterns(m_corsDisablingPatterns);

    pageConfiguration.maskedURLSchemes = WTFMove(parameters.maskedURLSchemes);
    pageConfiguration.loadsSubresources = parameters.loadsSubresources;
    pageConfiguration.allowedNetworkHosts = WTFMove(parameters.allowedNetworkHosts);
    pageConfiguration.shouldRelaxThirdPartyCookieBlocking = parameters.shouldRelaxThirdPartyCookieBlocking;
    pageConfiguration.httpsUpgradeEnabled = parameters.httpsUpgradeEnabled;
    pageConfiguration.portsForUpgradingInsecureSchemeForTesting = parameters.portsForUpgradingInsecureSchemeForTesting;

    if (!parameters.crossOriginAccessControlCheckEnabled)
        CrossOriginAccessControlCheckDisabler::singleton().setCrossOriginAccessControlCheckEnabled(false);

#if ENABLE(ATTACHMENT_ELEMENT)
    pageConfiguration.attachmentElementClient = makeUnique<WebAttachmentElementClient>(*this);
#endif

    pageConfiguration.contentSecurityPolicyModeForExtension = parameters.contentSecurityPolicyModeForExtension;

#if PLATFORM(COCOA)
    static bool hasConsumedGPUExtensionHandles = false;
    if (!hasConsumedGPUExtensionHandles) {
        SandboxExtension::consumePermanently(parameters.gpuIOKitExtensionHandles);
        SandboxExtension::consumePermanently(parameters.gpuMachExtensionHandles);
        hasConsumedGPUExtensionHandles = true;
    }
#endif

#if HAVE(STATIC_FONT_REGISTRY)
    if (parameters.fontMachExtensionHandles.size())
        WebProcess::singleton().switchFromStaticFontRegistryToUserFontRegistry(WTFMove(parameters.fontMachExtensionHandles));
#endif

#if PLATFORM(IOS_FAMILY)
    pageConfiguration.canShowWhileLocked = parameters.canShowWhileLocked;
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    pageConfiguration.gamepadAccessRequiresExplicitConsent = parameters.gamepadAccessRequiresExplicitConsent;
#endif

#if HAVE(AUDIT_TOKEN)
    pageConfiguration.presentingApplicationAuditToken = parameters.presentingApplicationAuditToken ? std::optional(parameters.presentingApplicationAuditToken->auditToken()) : std::nullopt;
#endif

#if PLATFORM(COCOA)
    pageConfiguration.presentingApplicationBundleIdentifier = WTFMove(parameters.presentingApplicationBundleIdentifier);
#endif

#if ENABLE(IMAGE_ANALYSIS)
    pageConfiguration.imageTranslationLanguageIdentifiers = WTFMove(parameters.imageTranslationLanguageIdentifiers);
#endif

    if (parameters.textManipulationParameters) {
        m_textManipulationIncludesSubframes = parameters.textManipulationParameters->includeSubframes;
        m_internals->textManipulationExclusionRules = WTFMove(parameters.textManipulationParameters->exclusionRules);
    }

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (parameters.store.getBoolValueForKey(WebPreferencesKey::remoteMediaSessionManagerEnabledKey())) {
        pageConfiguration.mediaSessionManagerFactory = [weakThis = WeakPtr { *this }](PageIdentifier) -> RefPtr<MediaSessionManagerInterface> {

            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return nullptr;

            RefPtr topDocument = protectedThis->localTopDocument();
            if (!topDocument)
                return nullptr;

            RefPtr topCorePage = topDocument->page();
            if (!topCorePage)
                return nullptr;

            RefPtr topWebPage = WebPage::fromCorePage(*topCorePage);
            if (!topWebPage)
                return nullptr;

            RefPtr<PlatformMediaSessionManager> manager = RemoteMediaSessionManager::create(*topWebPage, *protectedThis);
            manager->resetRestrictions();

            return manager;
        };
    }
#endif

    Ref page = Page::create(WTFMove(pageConfiguration));
    m_page = page.copyRef();

    updateAfterDrawingAreaCreation(parameters);

    if (parameters.displayID)
        windowScreenDidChange(*parameters.displayID, parameters.nominalFramesPerSecond);

    WebStorageNamespaceProvider::incrementUseCount(sessionStorageNamespaceIdentifier());

    updatePreferences(parameters.store);

#if PLATFORM(IOS_FAMILY) || ENABLE(ROUTING_ARBITRATION)
    DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

    m_backgroundColor = parameters.backgroundColor;

    // We need to set the device scale factor before creating the drawing area
    // to ensure it's created with the right size.
    page->setDeviceScaleFactor(parameters.deviceScaleFactor);

#if USE(GRAPHICS_LAYER_WC) || USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    setIntrinsicDeviceScaleFactor(parameters.intrinsicDeviceScaleFactor);
#endif

#if USE(SKIA)
    FontRenderOptions::singleton().setUseSubpixelPositioning(parameters.deviceScaleFactor >= 2.);
#endif

    RefPtr drawingArea = m_drawingArea;
#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    if (drawingArea->enterAcceleratedCompositingModeIfNeeded() && !parameters.isProcessSwap)
        drawingArea->sendEnterAcceleratedCompositingModeIfNeeded();
#endif
    drawingArea->setShouldScaleViewToFitDocument(parameters.shouldScaleViewToFitDocument);

    if (parameters.isProcessSwap)
        freezeLayerTree(LayerTreeFreezeReason::ProcessSwap);

#if ENABLE(ASYNC_SCROLLING)
    m_useAsyncScrolling = parameters.store.getBoolValueForKey(WebPreferencesKey::threadedScrollingEnabledKey());
    if (!drawingArea->supportsAsyncScrolling())
        m_useAsyncScrolling = false;
    page->settings().setScrollingCoordinatorEnabled(m_useAsyncScrolling);
#endif

    // Disable Back/Forward cache expiration in the WebContent process since management happens in the UIProcess
    // in modern WebKit.
    page->settings().setBackForwardCacheExpirationInterval(Seconds::infinity());

    m_mainFrame->initWithCoreMainFrame(*this, page->protectedMainFrame());

    if (auto& remotePageParameters = parameters.remotePageParameters) {
        Ref frameTreeSyncData = remotePageParameters->frameTreeParameters.frameTreeSyncData;
        page->protectedMainFrame()->updateFrameTreeSyncData(WTFMove(frameTreeSyncData));
        for (auto& childParameters : remotePageParameters->frameTreeParameters.children)
            constructFrameTree(m_mainFrame.get(), childParameters);
        page->setMainFrameURLAndOrigin(remotePageParameters->initialMainDocumentURL, nullptr);
        if (auto websitePolicies = remotePageParameters->websitePoliciesData) {
            if (auto* remoteMainFrameClient = m_mainFrame->remoteFrameClient())
                remoteMainFrameClient->applyWebsitePolicies(WTFMove(*remotePageParameters->websitePoliciesData));
        }
    }
    if (auto&& provisionalFrameCreationParameters = parameters.provisionalFrameCreationParameters) {
        ASSERT(page->settings().siteIsolationEnabled());
        createProvisionalFrame(WTFMove(*provisionalFrameCreationParameters));
    }

    drawingArea->updatePreferences(parameters.store);

    setBackgroundExtendsBeyondPage(parameters.backgroundExtendsBeyondPage);
    didSetPageZoomFactor(parameters.pageZoomFactor);
    didSetTextZoomFactor(parameters.textZoomFactor);

#if ENABLE(GEOLOCATION)
    WebCore::provideGeolocationTo(page.ptr(), WebGeolocationClient::create(*this));
#endif
    // FIXME: These should use makeUnique and makeUniqueRef instead of new.
#if ENABLE(NOTIFICATIONS)
    WebCore::provideNotification(page.ptr(), new WebNotificationClient(this));
#endif
#if ENABLE(MEDIA_STREAM)
    WebCore::provideUserMediaTo(page.ptr(), WebUserMediaClient::create(*this));
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    WebCore::provideMediaKeySystemTo(page, WebMediaKeySystemClient::create(*this));
#endif

    page->setControlledByAutomation(parameters.controlledByAutomation);
    page->setHasResourceLoadClient(parameters.hasResourceLoadClient);

    page->setCanStartMedia(false);
    m_mayStartMediaWhenInWindow = parameters.mayStartMediaWhenInWindow;
    if (parameters.mediaPlaybackIsSuspended)
        page->suspendAllMediaPlayback();

    if (parameters.openedByDOM)
        page->setOpenedByDOM();

    page->setGroupName(m_pageGroup->identifier());
    page->setUserInterfaceLayoutDirection(m_userInterfaceLayoutDirection);
#if PLATFORM(IOS_FAMILY)
    // Set hardware keyboard attached status
    page->didUpdateHardwareKeyboardAttachment(m_keyboardIsAttached);

    page->setTextAutosizingWidth(parameters.textAutosizingWidth);
    setOverrideViewportArguments(parameters.overrideViewportArguments);
#endif

    platformInitialize(parameters);

    setUseFixedLayout(parameters.useFixedLayout);

    setDefaultUnobscuredSize(parameters.defaultUnobscuredSize);
    setMinimumUnobscuredSize(parameters.minimumUnobscuredSize);
    setMaximumUnobscuredSize(parameters.maximumUnobscuredSize);

    setUnderlayColor(parameters.underlayColor);

    setPaginationMode(parameters.paginationMode);
    setPaginationBehavesLikeColumns(parameters.paginationBehavesLikeColumns);
    setPageLength(parameters.pageLength);
    setGapBetweenPages(parameters.gapBetweenPages);

    setUseColorAppearance(parameters.useDarkAppearance, parameters.useElevatedUserInterfaceLevel);

    if (parameters.isEditable)
        setEditable(true);

#if PLATFORM(MAC)
    setUseFormSemanticContext(parameters.useFormSemanticContext);
    setHeaderBannerHeight(parameters.headerBannerHeight);
    setFooterBannerHeight(parameters.footerBannerHeight);
    if (parameters.viewWindowCoordinates)
        windowAndViewFramesChanged(*parameters.viewWindowCoordinates);
#endif

    // If the page is created off-screen, its visibilityState should be prerender.
    page->setActivityState(m_activityState);
    if (!isVisible())
        page->setIsPrerender();

    updateIsInWindow(true);

    setMinimumSizeForAutoLayout(parameters.minimumSizeForAutoLayout);
    setSizeToContentAutoSizeMaximumSize(parameters.sizeToContentAutoSizeMaximumSize);
    setAutoSizingShouldExpandToViewHeight(parameters.autoSizingShouldExpandToViewHeight);
    setViewportSizeForCSSViewportUnits(parameters.viewportSizeForCSSViewportUnits);

    setScrollPinningBehavior(parameters.scrollPinningBehavior);
    if (parameters.scrollbarOverlayStyle)
        m_scrollbarOverlayStyle = static_cast<ScrollbarOverlayStyle>(parameters.scrollbarOverlayStyle.value());
    else
        m_scrollbarOverlayStyle = std::optional<ScrollbarOverlayStyle>();

    setObscuredContentInsets(parameters.obscuredContentInsets);

    m_userAgent = WTFMove(parameters.userAgent);

    setMediaVolume(parameters.mediaVolume);

    setMuted(parameters.muted, [] { });

    // We use the DidFirstVisuallyNonEmptyLayout milestone to determine when to unfreeze the layer tree.
    // We use LayoutMilestone::DidFirstMeaningfulPaint to generte WKPageLoadTiming.
    page->addLayoutMilestones({ WebCore::LayoutMilestone::DidFirstLayout, WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout, LayoutMilestone::DidFirstMeaningfulPaint });

    auto& webProcess = WebProcess::singleton();
    webProcess.addMessageReceiver(Messages::WebPage::messageReceiverName(), m_identifier, *this);

    // FIXME: This should be done in the object constructors, and the objects themselves should be message receivers.
    webProcess.addMessageReceiver(Messages::WebInspectorBackend::messageReceiverName(), m_identifier, *this);
    webProcess.addMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_identifier, *this);
    webProcess.addMessageReceiver(Messages::RemoteWebInspectorUI::messageReceiverName(), m_identifier, *this);
#if ENABLE(FULLSCREEN_API)
    webProcess.addMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_identifier, *this);
#endif

#if ENABLE(SCROLLING_THREAD)
    if (m_useAsyncScrolling)
        drawingArea->registerScrollingTree();
#endif

    for (auto& mimeType : parameters.mimeTypesWithCustomContentProviders)
        m_mimeTypesWithCustomContentProviders.add(mimeType);

    if (parameters.viewScaleFactor != 1)
        scaleView(parameters.viewScaleFactor);

    page->addLayoutMilestones(parameters.observedLayoutMilestones);

#if PLATFORM(COCOA)
    setSmartInsertDeleteEnabled(parameters.smartInsertDeleteEnabled);
#endif

#if HAVE(APP_ACCENT_COLORS)
    setAccentColor(parameters.accentColor);
#if PLATFORM(MAC)
    setAppUsesCustomAccentColor(parameters.appUsesCustomAccentColor);
#endif
#endif

    m_needsFontAttributes = parameters.needsFontAttributes;

    setNeedsScrollGeometryUpdates(parameters.needsScrollGeometryUpdates);

#if ENABLE(WEB_RTC)
    if (!parameters.iceCandidateFilteringEnabled)
        page->disableICECandidateFiltering();
#if USE(LIBWEBRTC)
    if (parameters.enumeratingAllNetworkInterfacesEnabled)
        downcast<LibWebRTCProvider>(page->webRTCProvider()).enableEnumeratingAllNetworkInterfaces();
    if (parameters.store.getBoolValueForKey(WebPreferencesKey::enumeratingVisibleNetworkInterfacesEnabledKey()))
        downcast<LibWebRTCProvider>(page->webRTCProvider()).enableEnumeratingVisibleNetworkInterfaces();
#endif
#endif

    for (const auto& iterator : parameters.urlSchemeHandlers)
        registerURLSchemeHandler(iterator.value, iterator.key);
    for (auto& scheme : parameters.urlSchemesWithLegacyCustomProtocolHandlers)
        LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler({ scheme });

#if PLATFORM(IOS_FAMILY)
    setViewportConfigurationViewLayoutSize(parameters.viewportConfigurationViewLayoutSize, parameters.viewportConfigurationLayoutSizeScaleFactorFromClient, parameters.viewportConfigurationMinimumEffectiveDeviceWidth);
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW) && !HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
    m_contextForVisibilityPropagation = LayerHostingContext::create({
        canShowWhileLocked()
    });
    WEBPAGE_RELEASE_LOG(Process, "WebPage: Created context with ID %u for visibility propagation from UIProcess", m_contextForVisibilityPropagation->contextID());
    send(Messages::WebPageProxy::DidCreateContextInWebProcessForVisibilityPropagation(m_contextForVisibilityPropagation->cachedContextID()));
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW) && !HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setShouldEnableVP9Decoder(parameters.shouldEnableVP9Decoder);
#endif

    page->setCanUseCredentialStorage(parameters.canUseCredentialStorage);

#if HAVE(SANDBOX_STATE_FLAGS)
    auto experimentalSandbox = parameters.store.getBoolValueForKey(WebPreferencesKey::experimentalSandboxEnabledKey());
    if (experimentalSandbox)
        sandbox_enable_state_flag("EnableExperimentalSandbox", *auditToken);

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
    SandboxExtension::consumePermanently(parameters.machBootstrapHandle);
#endif
#endif // HAVE(SANDBOX_STATE_FLAGS)

    updateThrottleState();
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    updateImageAnimationEnabled();
#endif
#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    updatePrefersNonBlinkingCursor();
#endif
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    setLinkDecorationFilteringData(WTFMove(parameters.linkDecorationFilteringData));
    setAllowedQueryParametersForAdvancedPrivacyProtections(WTFMove(parameters.allowedQueryParametersForAdvancedPrivacyProtections));
#endif
    if (parameters.windowFeatures) {
        page->applyWindowFeatures(*parameters.windowFeatures);
        page->chrome().show();
        page->setOpenedByDOM();
    }

    if (parameters.allowPostingLegacySynchronousMessages)
        InjectedBundleScriptWorld::normalWorldSingleton().setAllowPostingLegacySynchronousMessages();
}

void WebPage::updateAfterDrawingAreaCreation(const WebPageCreationParameters& parameters)
{
#if PLATFORM(COCOA)
    m_page->settings().setForceCompositingMode(true);
#endif
#if ENABLE(TILED_CA_DRAWING_AREA)
    if (parameters.drawingAreaType == DrawingAreaType::TiledCoreAnimation) {
        if (auto viewExposedRect = parameters.viewExposedRect)
            protectedDrawingArea()->setViewExposedRect(viewExposedRect);
    }
#endif
#if USE(COORDINATED_GRAPHICS)
    protectedDrawingArea()->updatePreferences(parameters.store);
#endif
}

void WebPage::constructFrameTree(WebFrame& parent, const FrameTreeCreationParameters& treeCreationParameters)
{
    auto frame = WebFrame::createRemoteSubframe(*this, parent, treeCreationParameters.frameID, treeCreationParameters.frameName, treeCreationParameters.openerFrameID, Ref { treeCreationParameters.frameTreeSyncData });
    for (auto& parameters : treeCreationParameters.children)
        constructFrameTree(frame, parameters);
}

void WebPage::createRemoteSubframe(WebCore::FrameIdentifier parentID, WebCore::FrameIdentifier newChildID, const String& newChildFrameName, Ref<WebCore::FrameTreeSyncData>&& frameTreeSyncData)
{
    RefPtr parentFrame = WebProcess::singleton().webFrame(parentID);
    if (!parentFrame) {
        ASSERT_NOT_REACHED();
        return;
    }
    WebFrame::createRemoteSubframe(*this, *parentFrame, newChildID, newChildFrameName, std::nullopt, WTFMove(frameTreeSyncData));
}

Awaitable<std::optional<FrameTreeNodeData>> WebPage::getFrameTree()
{
    co_return m_mainFrame->frameTreeData();
}

void WebPage::didFinishLoadInAnotherProcess(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    ASSERT(frame->page() == this);
    frame->didFinishLoadInAnotherProcess();
}

void WebPage::frameWasRemovedInAnotherProcess(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT(frame->page() == this);
    frame->markAsRemovedInAnotherProcess();
    frame->removeFromTree();
}

void WebPage::topDocumentSyncDataChangedInAnotherProcess(const WebCore::DocumentSyncSerializationData& data)
{
    if (RefPtr page = corePage())
        page->updateTopDocumentSyncData(data);
}

void WebPage::allTopDocumentSyncDataChangedInAnotherProcess(Ref<WebCore::DocumentSyncData>&& data)
{
    if (RefPtr page = corePage())
        page->updateTopDocumentSyncData(WTFMove(data));
}

void WebPage::frameTreeSyncDataChangedInAnotherProcess(FrameIdentifier frameID, const WebCore::FrameTreeSyncSerializationData& data)
{
    ASSERT(m_page->settings().siteIsolationEnabled());

    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    ASSERT(frame->page() == this);

    RefPtr coreFrame = frame->coreFrame();
    if (coreFrame)
        coreFrame->updateFrameTreeSyncData(data);
}

void WebPage::allFrameTreeSyncDataChangedInAnotherProcess(FrameIdentifier frameID, Ref<WebCore::FrameTreeSyncData>&& data)
{
    ASSERT(m_page->settings().siteIsolationEnabled());

    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    ASSERT(frame->page() == this);

    RefPtr coreFrame = frame->coreFrame();
    if (coreFrame)
        coreFrame->updateFrameTreeSyncData(WTFMove(data));
}

#if ENABLE(GPU_PROCESS)
void WebPage::gpuProcessConnectionDidBecomeAvailable(GPUProcessConnection& gpuProcessConnection)
{
    UNUSED_PARAM(gpuProcessConnection);

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    gpuProcessConnection.createVisibilityPropagationContextForPage(*this);
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    if (!mediaEnvironment().isEmpty())
        gpuProcessConnection.setMediaEnvironment(identifier(), mediaEnvironment());
#endif
}

void WebPage::gpuProcessConnectionWasDestroyed()
{
#if PLATFORM(COCOA)
    if (RefPtr remoteLayerTreeDrawingArea = dynamicDowncast<RemoteLayerTreeDrawingArea>(protectedDrawingArea()))
        remoteLayerTreeDrawingArea->gpuProcessConnectionWasDestroyed();
#endif
}

#endif

#if ENABLE(MODEL_PROCESS)
void WebPage::modelProcessConnectionDidBecomeAvailable(ModelProcessConnection& modelProcessConnection)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    modelProcessConnection.createVisibilityPropagationContextForPage(*this);
#else
    UNUSED_PARAM(modelProcessConnection);
#endif
}
#endif

void WebPage::requestMediaPlaybackState(CompletionHandler<void(WebKit::MediaPlaybackState)>&& completionHandler)
{
    RefPtr page = m_page;
    if (!page->mediaPlaybackExists())
        return completionHandler(MediaPlaybackState::NoMediaPlayback);
    if (page->mediaPlaybackIsPaused())
        return completionHandler(MediaPlaybackState::MediaPlaybackPaused);
    if (page->mediaPlaybackIsSuspended())
        return completionHandler(MediaPlaybackState::MediaPlaybackSuspended);

    completionHandler(MediaPlaybackState::MediaPlaybackPlaying);
}

void WebPage::pauseAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    protectedCorePage()->pauseAllMediaPlayback();
    completionHandler();
}

void WebPage::suspendAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    protectedCorePage()->suspendAllMediaPlayback();
    completionHandler();
}

void WebPage::resumeAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    protectedCorePage()->resumeAllMediaPlayback();
    completionHandler();
}

void WebPage::suspendAllMediaBuffering()
{
    protectedCorePage()->suspendAllMediaBuffering();
}

void WebPage::resumeAllMediaBuffering()
{
    protectedCorePage()->resumeAllMediaBuffering();
}

static void addRootFramesToNewDrawingArea(WebFrame& frame, DrawingArea& drawingArea)
{
    if (frame.isRootFrame() || (frame.provisionalFrame() && frame.provisionalFrame()->isRootFrame()))
        drawingArea.addRootFrame(frame.frameID());
    if (!frame.coreFrame())
        return;
    for (RefPtr child = frame.coreFrame()->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr childWebFrame = WebFrame::fromCoreFrame(*child))
            addRootFramesToNewDrawingArea(*childWebFrame, drawingArea);
    }
}

void WebPage::reinitializeWebPage(WebPageCreationParameters&& parameters)
{
    ASSERT(m_drawingArea);

    setSize(parameters.viewSize);

    // If the UIProcess created a new DrawingArea, then we need to do the same.
    if (m_drawingArea->identifier() != parameters.drawingAreaIdentifier) {
        RefPtr oldDrawingArea = std::exchange(m_drawingArea, nullptr);
        oldDrawingArea->removeMessageReceiverIfNeeded();

        m_drawingArea = DrawingArea::create(*this, parameters);
        RefPtr drawingArea = m_drawingArea;
        updateAfterDrawingAreaCreation(parameters);
        addRootFramesToNewDrawingArea(m_mainFrame.get(), *drawingArea);

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
        if (drawingArea->enterAcceleratedCompositingModeIfNeeded() && !parameters.isProcessSwap)
            drawingArea->sendEnterAcceleratedCompositingModeIfNeeded();
#endif
        drawingArea->setShouldScaleViewToFitDocument(parameters.shouldScaleViewToFitDocument);
        drawingArea->updatePreferences(parameters.store);

        drawingArea->adoptLayersFromDrawingArea(*oldDrawingArea);
        drawingArea->adoptDisplayRefreshMonitorsFromDrawingArea(*oldDrawingArea);

        unfreezeLayerTree(LayerTreeFreezeReason::PageSuspended);
    }

    setMinimumSizeForAutoLayout(parameters.minimumSizeForAutoLayout);
    setSizeToContentAutoSizeMaximumSize(parameters.sizeToContentAutoSizeMaximumSize);

    if (m_activityState != parameters.activityState)
        setActivityState(parameters.activityState, ActivityStateChangeAsynchronous, [] { });

#if HAVE(APP_ACCENT_COLORS)
    setAccentColor(parameters.accentColor);
#if PLATFORM(MAC)
    setAppUsesCustomAccentColor(parameters.appUsesCustomAccentColor);
#endif
#endif

    setUseColorAppearance(parameters.useDarkAppearance, parameters.useElevatedUserInterfaceLevel);

    if (auto&& provisionalFrameCreationParameters = parameters.provisionalFrameCreationParameters) {
        ASSERT(m_page->settings().siteIsolationEnabled());
        createProvisionalFrame(WTFMove(*provisionalFrameCreationParameters));
    }

    platformReinitializeAccessibilityToken();
}

void WebPage::updateThrottleState()
{
    bool isThrottleable = this->isThrottleable();

    // The UserActivity prevents App Nap. So if we want to allow App Nap of the page, stop the activity.
    // If the page should not be app nap'd, start it.
    if (isThrottleable)
        m_internals->userActivity.stop();
    else
        m_internals->userActivity.start();

    if (m_page && m_page->settings().serviceWorkersEnabled()) {
        RunLoop::mainSingleton().dispatch([isThrottleable] {
            WebServiceWorkerProvider::singleton().updateThrottleState(isThrottleable);
        });
    }
}

bool WebPage::isThrottleable() const
{
    bool isActive = m_activityState.containsAny({ ActivityState::IsLoading, ActivityState::IsAudible, ActivityState::IsCapturingMedia, ActivityState::WindowIsActive });
    bool isVisuallyIdle = m_activityState.contains(ActivityState::IsVisuallyIdle);

    return m_isAppNapEnabled && !isActive && isVisuallyIdle;
}

WebPage::~WebPage()
{
    ASSERT(!m_page);
    WEBPAGE_RELEASE_LOG(Loading, "destructor:");

    if (!m_corsDisablingPatterns.isEmpty()) {
        m_corsDisablingPatterns.clear();
        synchronizeCORSDisablingPatternsWithNetworkProcess();
    }

    platformDetach();

    m_sandboxExtensionTracker.invalidate();

#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews)
        pluginView->webPageDestroyed();
#endif

#if !PLATFORM(IOS_FAMILY)
    if (RefPtr headerBanner = m_headerBanner)
        headerBanner->detachFromPage();
    if (RefPtr footerBanner = m_footerBanner)
        footerBanner->detachFromPage();
#endif

    WebStorageNamespaceProvider::decrementUseCount(sessionStorageNamespaceIdentifier());

#if ENABLE(GPU_PROCESS) && HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (auto* gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->destroyVisibilityPropagationContextForPage(*this);
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS) && HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (auto* modelProcessConnection = WebProcess::singleton().existingModelProcessConnection())
        modelProcessConnection->destroyVisibilityPropagationContextForPage(*this);
#endif // ENABLE(MODEL_PROCESS) && HAVE(VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr playbackSessionManager = m_playbackSessionManager)
        playbackSessionManager->invalidate();

    if (RefPtr videoPresentationManager = m_videoPresentationManager)
        videoPresentationManager->invalidate();
#endif

    for (auto& completionHandler : std::exchange(m_markLayersAsVolatileCompletionHandlers, { }))
        completionHandler(false);

#if ENABLE(EXTENSION_CAPABILITIES)
    setMediaEnvironment({ });
#endif
}

IPC::Connection* WebPage::messageSenderConnection() const
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t WebPage::messageSenderDestinationID() const
{
    return identifier().toUInt64();
}

#if ENABLE(CONTEXT_MENUS)
void WebPage::setInjectedBundleContextMenuClient(std::unique_ptr<API::InjectedBundle::PageContextMenuClient>&& contextMenuClient)
{
    if (!contextMenuClient) {
        m_contextMenuClient = makeUnique<API::InjectedBundle::PageContextMenuClient>();
        return;
    }

    m_contextMenuClient = WTFMove(contextMenuClient);
}
#endif

void WebPage::setInjectedBundleEditorClient(std::unique_ptr<API::InjectedBundle::EditorClient>&& editorClient)
{
    if (!editorClient) {
        m_editorClient = makeUnique<API::InjectedBundle::EditorClient>();
        return;
    }

    m_editorClient = WTFMove(editorClient);
}

void WebPage::setInjectedBundleFormClient(std::unique_ptr<API::InjectedBundle::FormClient>&& formClient)
{
    if (!formClient) {
        m_formClient = makeUnique<API::InjectedBundle::FormClient>();
        return;
    }

    m_formClient = WTFMove(formClient);
}

void WebPage::setInjectedBundlePageLoaderClient(std::unique_ptr<API::InjectedBundle::PageLoaderClient>&& loaderClient)
{
    if (!loaderClient) {
        m_loaderClient = makeUnique<API::InjectedBundle::PageLoaderClient>();
        return;
    }

    m_loaderClient = WTFMove(loaderClient);

    // It would be nice to get rid of this code and transition all clients to using didLayout instead of
    // didFirstLayoutInFrame and didFirstVisuallyNonEmptyLayoutInFrame. In the meantime, this is required
    // for backwards compatibility.
    if (auto milestones = m_loaderClient->layoutMilestones())
        listenForLayoutMilestones(milestones);
}

void WebPage::setInjectedBundleResourceLoadClient(std::unique_ptr<API::InjectedBundle::ResourceLoadClient>&& client)
{
    if (!m_resourceLoadClient)
        m_resourceLoadClient = makeUnique<API::InjectedBundle::ResourceLoadClient>();
    else
        m_resourceLoadClient = WTFMove(client);
}

void WebPage::setInjectedBundleUIClient(std::unique_ptr<API::InjectedBundle::PageUIClient>&& uiClient)
{
    if (!uiClient) {
        m_uiClient = makeUnique<API::InjectedBundle::PageUIClient>();
        return;
    }

    m_uiClient = WTFMove(uiClient);
}

bool WebPage::hasPendingEditorStateUpdate() const
{
    return m_pendingEditorStateUpdateStatus != PendingEditorStateUpdateStatus::NotScheduled;
}

EditorState WebPage::editorState(ShouldPerformLayout shouldPerformLayout) const
{
    // Always return an EditorState with a valid identifier or it will fail to decode and this process will be terminated.
    EditorState result;
    result.identifier = m_internals->lastEditorStateIdentifier.increment();

    // Ref the frame because this function may perform layout, which may cause frame destruction.
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return result;

    auto sanitizeEditorStateOnceCreated = makeScopeExit([&result] {
        result.clipOwnedRectExtentsToNumericLimits();
    });

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = focusedPluginViewForFrame(*frame); pluginView && pluginView->populateEditorStateIfNeeded(result))
        return result;
#endif

    const VisibleSelection& selection = frame->selection().selection();
    Ref editor = frame->editor();

    result.selectionIsNone = selection.isNone();
    result.selectionIsRange = selection.isRange();
    result.isContentEditable = selection.hasEditableStyle();
    result.isContentRichlyEditable = selection.isContentRichlyEditable();
    result.isInPasswordField = selection.isInPasswordField();
    result.hasComposition = editor->hasComposition();
    result.shouldIgnoreSelectionChanges = editor->ignoreSelectionChanges() || (editor->client() && !editor->checkedClient()->shouldRevealCurrentSelectionAfterInsertion());
    result.triggeredByAccessibilitySelectionChange = m_pendingEditorStateUpdateStatus == PendingEditorStateUpdateStatus::ScheduledDuringAccessibilitySelectionChange || m_isChangingSelectionForAccessibility;

    Ref<Document> document = *frame->document();

    if (result.selectionIsRange) {
        auto selectionRange = selection.range();
        result.selectionIsRangeInsideImageOverlay = selectionRange && ImageOverlay::isInsideOverlay(*selectionRange);
        result.selectionIsRangeInAutoFilledAndViewableField = selection.isInAutoFilledAndViewableField();
    }

    m_lastEditorStateWasContentEditable = result.isContentEditable ? EditorStateIsContentEditable::Yes : EditorStateIsContentEditable::No;

    if (shouldAvoidComputingPostLayoutDataForEditorState()) {
        getPlatformEditorState(*frame, result);
        return result;
    }

    if (shouldPerformLayout == ShouldPerformLayout::Yes || requiresPostLayoutDataForEditorState(*frame))
        document->updateLayout(); // May cause document destruction

    if (RefPtr frameView = document->view(); frameView && !frameView->needsLayout() && !document->hasNodesWithMissingStyle()) {
        if (!result.postLayoutData)
            result.postLayoutData = std::optional<EditorState::PostLayoutData> { EditorState::PostLayoutData { } };
        result.postLayoutData->canCut = editor->canCut();
        result.postLayoutData->canCopy = editor->canCopy();
        result.postLayoutData->canPaste = editor->canEdit();

        if (!result.visualData)
            result.visualData = std::optional<EditorState::VisualData> { EditorState::VisualData { } };
    }

    getPlatformEditorState(*frame, result);

    return result;
}

void WebPage::changeFontAttributes(WebCore::FontAttributeChanges&& changes)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable())
        frame->protectedEditor()->applyStyleToSelection(changes.createEditingStyle(), changes.editAction(), Editor::ColorFilterMode::InvertColor);
}

void WebPage::changeFont(WebCore::FontChanges&& changes)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable())
        frame->protectedEditor()->applyStyleToSelection(changes.createEditingStyle(), EditAction::SetFont, Editor::ColorFilterMode::InvertColor);
}

void WebPage::executeEditCommandWithCallback(const String& commandName, const String& argument, CompletionHandler<void()>&& completionHandler)
{
    executeEditCommand(commandName, argument);
    completionHandler();
}

void WebPage::selectAll()
{
    executeEditingCommand("SelectAll"_s, { });
    platformDidSelectAll();
}

bool WebPage::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    RefPtr localTopDocument = protectedCorePage()->localTopDocument();
    return localTopDocument && localTopDocument->quirks().shouldDispatchSyntheticMouseEventsWhenModifyingSelection();
}

#if !PLATFORM(IOS_FAMILY)

void WebPage::platformDidSelectAll()
{
}

#endif // !PLATFORM(IOS_FAMILY)

#if !PLATFORM(COCOA)
std::pair<URL, WebCore::DidFilterLinkDecoration> WebPage::applyLinkDecorationFilteringWithResult(const URL& url, WebCore::LinkDecorationFilteringTrigger)
{
    return { url, WebCore::DidFilterLinkDecoration::No };
}

void WebPage::bindRemoteAccessibilityFrames(int, WebCore::FrameIdentifier, Vector<uint8_t>, CompletionHandler<void(Vector<uint8_t>, int)>&& completionHandler)
{
    completionHandler({ }, { });
}

void WebPage::resolveAccessibilityHitTestForTesting(WebCore::FrameIdentifier, const WebCore::IntPoint&, CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler({ });
}

void WebPage::updateRemotePageAccessibilityOffset(WebCore::FrameIdentifier, WebCore::IntPoint)
{
}
#endif

void WebPage::updateEditorStateAfterLayoutIfEditabilityChanged()
{
    // FIXME: We should update EditorStateIsContentEditable to track whether the state is richly
    // editable or plainttext-only.
    if (m_lastEditorStateWasContentEditable == EditorStateIsContentEditable::Unset)
        return;

    if (hasPendingEditorStateUpdate())
        return;

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    auto isEditable = frame->selection().selection().hasEditableStyle() ? EditorStateIsContentEditable::Yes : EditorStateIsContentEditable::No;
    if (m_lastEditorStateWasContentEditable != isEditable)
        scheduleFullEditorStateUpdate();
}

static OptionSet<RenderAsTextFlag> toRenderAsTextFlags(unsigned options)
{
    OptionSet<RenderAsTextFlag> flags;

    if (options & RenderTreeShowAllLayers)
        flags.add(RenderAsTextFlag::ShowAllLayers);
    if (options & RenderTreeShowLayerNesting)
        flags.add(RenderAsTextFlag::ShowLayerNesting);
    if (options & RenderTreeShowCompositedLayers)
        flags.add(RenderAsTextFlag::ShowCompositedLayers);
    if (options & RenderTreeShowOverflow)
        flags.add(RenderAsTextFlag::ShowOverflow);
    if (options & RenderTreeShowSVGGeometry)
        flags.add(RenderAsTextFlag::ShowSVGGeometry);
    if (options & RenderTreeShowLayerFragments)
        flags.add(RenderAsTextFlag::ShowLayerFragments);

    return flags;
}

String WebPage::renderTreeExternalRepresentation(unsigned options) const
{
    return externalRepresentation(m_mainFrame->protectedCoreLocalFrame().get(), toRenderAsTextFlags(options));
}

String WebPage::renderTreeExternalRepresentationForPrinting() const
{
    return externalRepresentation(m_mainFrame->protectedCoreLocalFrame().get(), { RenderAsTextFlag::PrintingMode });
}

uint64_t WebPage::renderTreeSize() const
{
    if (RefPtr page = m_page)
        return page->renderTreeSize();
    return 0;
}

void WebPage::setHasResourceLoadClient(bool has)
{
    if (m_page)
        m_page->setHasResourceLoadClient(has);
}

void WebPage::setCanUseCredentialStorage(bool has)
{
    if (m_page)
        m_page->setCanUseCredentialStorage(has);
}

bool WebPage::isTrackingRepaints() const
{
    if (RefPtr view = localMainFrameView())
        return view->isTrackingRepaints();

    return false;
}

Ref<API::Array> WebPage::trackedRepaintRects()
{
    RefPtr view = localMainFrameView();
    if (!view)
        return API::Array::create();

    auto repaintRects = view->trackedRepaintRects().map([](auto& repaintRect) -> RefPtr<API::Object> {
        return API::Rect::create(toAPI(repaintRect));
    });
    return API::Array::create(WTFMove(repaintRects));
}

#if ENABLE(PDF_PLUGIN)

PluginView* WebPage::focusedPluginViewForFrame(LocalFrame& frame)
{
    RefPtr pluginDocument = dynamicDowncast<PluginDocument>(frame.document());
    if (!pluginDocument)
        return nullptr;

    if (pluginDocument->focusedElement() != pluginDocument->pluginElement())
        return nullptr;

    return pluginViewForFrame(&frame);
}

PluginView* WebPage::pluginViewForFrame(LocalFrame* frame)
{
    if (!frame)
        return nullptr;
    RefPtr document = dynamicDowncast<PluginDocument>(frame->document());
    if (!document)
        return nullptr;
    return downcast<PluginView>(document->pluginWidget());
}

PluginView* WebPage::mainFramePlugIn() const
{
    RefPtr localMainFrame = this->localMainFrame();
    return pluginViewForFrame(localMainFrame.get());
}

#endif

void WebPage::executeEditingCommand(const String& commandName, const String& argument)
{
    platformWillPerformEditingCommand();

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = focusedPluginViewForFrame(*frame)) {
        pluginView->handleEditingCommand(commandName, argument);
        return;
    }
#endif

    frame->protectedEditor()->command(commandName).execute(argument);
}

void WebPage::setEditable(bool editable)
{
    protectedCorePage()->setEditable(editable);
    protectedCorePage()->setTabKeyCyclesThroughElements(!editable);
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (editable) {
        frame->protectedEditor()->applyEditingStyleToBodyElement();
        // If the page is made editable and the selection is empty, set it to something.
        if (frame->selection().isNone())
            frame->checkedSelection()->setSelectionFromNone();
    }
}

void WebPage::increaseListLevel()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->protectedEditor()->increaseSelectionListLevel();
}

void WebPage::decreaseListLevel()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->protectedEditor()->decreaseSelectionListLevel();
}

void WebPage::changeListType()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->protectedEditor()->changeSelectionListType();
}

void WebPage::setBaseWritingDirection(WritingDirection direction)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->protectedEditor()->setBaseWritingDirection(direction);
}

void WebPage::enterAcceleratedCompositingMode(WebCore::Frame& frame, GraphicsLayer* layer)
{
    protectedDrawingArea()->setRootCompositingLayer(frame, layer);
}

void WebPage::exitAcceleratedCompositingMode(WebCore::Frame& frame)
{
    protectedDrawingArea()->setRootCompositingLayer(frame, nullptr);
}

void WebPage::close()
{
    if (m_isClosed)
        return;

    flushDeferredDidReceiveMouseEvent();

    WEBPAGE_RELEASE_LOG_FORWARDABLE(Loading, WEBPAGE_CLOSE);

    if (auto networkProcessConnection = WebProcess::singleton().protectedNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::ClearPageSpecificData(m_identifier), 0);

    m_isClosed = true;

    // If there is still no URL, then we never loaded anything in this page, so nothing to report.
    if (!m_mainFrame->url().isEmpty())
        reportUsedFeatures();

    if (WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->willDestroyPage(Ref { *this });

    if (RefPtr inspector = std::exchange(m_inspector, nullptr))
        inspector->disconnectFromPage();

    m_page->inspectorController().disconnectAllFrontends();

#if ENABLE(FULLSCREEN_API)
    if (auto manager = std::exchange(m_fullScreenManager, { }))
        manager->invalidate();
#endif

    if (RefPtr activePopupMenu = std::exchange(m_activePopupMenu, nullptr))
        activePopupMenu->disconnectFromPage();

    if (RefPtr activeOpenPanelResultListener = std::exchange(m_activeOpenPanelResultListener, nullptr))
        activeOpenPanelResultListener->disconnectFromPage();

    if (RefPtr activeColorChooser = m_activeColorChooser.get()) {
        activeColorChooser->disconnectFromPage();
        m_activeColorChooser = nullptr;
    }

#if PLATFORM(GTK)
    m_printOperation = nullptr;
#endif

    m_sandboxExtensionTracker.invalidate();

#if ENABLE(TEXT_AUTOSIZING)
    m_textAutoSizingAdjustmentTimer.stop();
#endif

#if PLATFORM(IOS_FAMILY)
    invokePendingSyntheticClickCallback(SyntheticClickResult::PageInvalid);
    m_updateFocusedElementInformationTimer.stop();
#endif

#if ENABLE(CONTEXT_MENUS)
    m_contextMenuClient = makeUnique<API::InjectedBundle::PageContextMenuClient>();
#endif
    m_editorClient = makeUnique<API::InjectedBundle::EditorClient>();
    m_formClient = makeUnique<API::InjectedBundle::FormClient>();
    m_loaderClient = makeUnique<API::InjectedBundle::PageLoaderClient>();
    m_resourceLoadClient = makeUnique<API::InjectedBundle::ResourceLoadClient>();
    m_uiClient = makeUnique<API::InjectedBundle::PageUIClient>();

    m_printContext = nullptr;
    if (RefPtr localFrame = m_mainFrame->coreLocalFrame())
        localFrame->loader().detachFromParent();

#if ENABLE(SCROLLING_THREAD)
    if (m_useAsyncScrolling)
        protectedDrawingArea()->unregisterScrollingTree();
#endif

    protectedCorePage()->destroyRenderTrees();

    m_drawingArea = nullptr;
    m_webPageTesting = nullptr;
    m_page = nullptr;

    bool isRunningModal = m_isRunningModal;
    m_isRunningModal = false;

#if PLATFORM(COCOA)
    if (RefPtr remoteObjectRegistry = m_remoteObjectRegistry.get())
        remoteObjectRegistry->close();
    ASSERT(!m_remoteObjectRegistry);
#endif

    auto& webProcess = WebProcess::singleton();
    webProcess.removeMessageReceiver(Messages::WebPage::messageReceiverName(), m_identifier);
    // FIXME: This should be done in the object destructors, and the objects themselves should be message receivers.
    webProcess.removeMessageReceiver(Messages::WebInspectorBackend::messageReceiverName(), m_identifier);
    webProcess.removeMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_identifier);
    webProcess.removeMessageReceiver(Messages::RemoteWebInspectorUI::messageReceiverName(), m_identifier);
#if ENABLE(FULLSCREEN_API)
    webProcess.removeMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_identifier);
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK)
    m_viewGestureGeometryCollector = nullptr;
#endif

    stopObservingNowPlayingMetadata();

    String processDisplayName = m_processDisplayName;

    // The WebPage can be destroyed by this call.
    WebProcess::singleton().removeWebPage(m_identifier);

    WebProcess::singleton().updateActivePages(processDisplayName);

    if (isRunningModal)
        RunLoop::mainSingleton().stop();
}

void WebPage::tryClose(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    if (!coreFrame) {
        completionHandler(false);
        return;
    }
    completionHandler(coreFrame->loader().shouldClose());
}

void WebPage::sendClose()
{
    send(Messages::WebPageProxy::ClosePage());
}

void WebPage::suspendForProcessSwap(CompletionHandler<void(std::optional<bool>)>&& completionHandler)
{
    flushDeferredDidReceiveMouseEvent();

    // FIXME: Make this work if the main frame is not a LocalFrame.
    RefPtr currentHistoryItem = m_mainFrame->coreLocalFrame()->loader().history().currentItem();
    if (!currentHistoryItem)
        return completionHandler(false);

    if (!BackForwardCache::singleton().addIfCacheable(*currentHistoryItem, protectedCorePage().get()))
        return completionHandler(false);

    // Back/forward cache does not break the opener link for the main frame (only does so for the subframes) because the
    // main frame is normally re-used for the navigation. However, in the case of process-swapping, the main frame
    // is now hosted in another process and the one in this process is in the cache.
    if (RefPtr frame = m_mainFrame->coreLocalFrame())
        frame->detachFromAllOpenedFrames();

    completionHandler(true);
}

void WebPage::loadURLInFrame(URL&& url, const String& referrer, FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreLocalFrame = frame->coreLocalFrame();
    coreLocalFrame->loader().load(FrameLoadRequest(*coreLocalFrame, ResourceRequest(URL { url }, referrer)));
}

void WebPage::loadDataInFrame(std::span<const uint8_t> data, String&& type, String&& encodingName, URL&& baseURL, FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    ASSERT(&mainWebFrame() != frame);

    Ref sharedBuffer = SharedBuffer::create(data);
    ResourceResponse response(URL { baseURL }, WTFMove(type), sharedBuffer->size(), WTFMove(encodingName));
    SubstituteData substituteData(WTFMove(sharedBuffer), URL { baseURL }, WTFMove(response), SubstituteData::SessionHistoryVisibility::Hidden);
    frame->coreLocalFrame()->loader().load(FrameLoadRequest(*frame->coreLocalFrame(), ResourceRequest(WTFMove(baseURL)), WTFMove(substituteData)));
}

#if !PLATFORM(COCOA)
void WebPage::platformDidReceiveLoadParameters(const LoadParameters& loadParameters)
{
}
#endif

void WebPage::createProvisionalFrame(ProvisionalFrameCreationParameters&& parameters)
{
    RefPtr frame = WebProcess::singleton().webFrame(parameters.frameID);
    if (!frame)
        return;
    ASSERT(frame->page() == this);
    frame->createProvisionalFrame(WTFMove(parameters));
}

void WebPage::loadDidCommitInAnotherProcess(WebCore::FrameIdentifier frameID, std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    ASSERT(frame->page() == this);
    frame->loadDidCommitInAnotherProcess(layerHostingContextIdentifier);
}

void WebPage::loadRequest(LoadParameters&& loadParameters)
{
    WEBPAGE_RELEASE_LOG_FORWARDABLE(Loading, WEBPAGE_LOADREQUEST, loadParameters.navigationID ? loadParameters.navigationID->toUInt64() : 0, static_cast<unsigned>(loadParameters.shouldTreatAsContinuingLoad), loadParameters.request.isAppInitiated(), loadParameters.existingNetworkResourceLoadIdentifierToResume ? loadParameters.existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);

    RefPtr frame = loadParameters.frameIdentifier ? WebProcess::singleton().webFrame(*loadParameters.frameIdentifier) : m_mainFrame.ptr();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr localFrame = frame->coreLocalFrame() ? frame->coreLocalFrame() : frame->provisionalFrame();
    if (!localFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    setLastNavigationWasAppInitiated(loadParameters.request.isAppInitiated());

#if ENABLE(APP_BOUND_DOMAINS)
    setIsNavigatingToAppBoundDomain(loadParameters.isNavigatingToAppBoundDomain, *frame);
#endif

    WebProcess::singleton().webLoaderStrategy().setExistingNetworkResourceLoadIdentifierToResume(loadParameters.existingNetworkResourceLoadIdentifierToResume);
    auto resumingLoadScope = makeScopeExit([] {
        WebProcess::singleton().webLoaderStrategy().setExistingNetworkResourceLoadIdentifierToResume(std::nullopt);
    });

    SendStopResponsivenessTimer stopper;

    m_pendingNavigationID = loadParameters.navigationID;
    m_internals->pendingWebsitePolicies = WTFMove(loadParameters.websitePolicies);

    m_sandboxExtensionTracker.beginLoad(WTFMove(loadParameters.sandboxExtensionHandle));

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient->willLoadURLRequest(*this, loadParameters.request, WebProcess::singleton().transformHandlesToObjects(loadParameters.userData.protectedObject().get()).get());

    platformDidReceiveLoadParameters(loadParameters);

    if (loadParameters.originatingFrame && !loadParameters.frameIdentifier)
        m_mainFrameNavigationInitiator = makeUnique<FrameInfoData>(*loadParameters.originatingFrame);

    // Initate the load in WebCore.
    ASSERT(localFrame->document());
    FrameLoadRequest frameLoadRequest { *localFrame, WTFMove(loadParameters.request) };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(loadParameters.shouldOpenExternalURLsPolicy);
    frameLoadRequest.setShouldTreatAsContinuingLoad(loadParameters.shouldTreatAsContinuingLoad);
    frameLoadRequest.setLockHistory(loadParameters.lockHistory);
    frameLoadRequest.setLockBackForwardList(loadParameters.lockBackForwardList);
    frameLoadRequest.setClientRedirectSourceForHistory(WTFMove(loadParameters.clientRedirectSourceForHistory));
    frameLoadRequest.setIsHandledByAboutSchemeHandler(loadParameters.isHandledByAboutSchemeHandler);
    if (loadParameters.isRequestFromClientOrUserInput)
        frameLoadRequest.setIsRequestFromClientOrUserInput();
    if (loadParameters.advancedPrivacyProtections)
        frameLoadRequest.setAdvancedPrivacyProtections(*loadParameters.advancedPrivacyProtections);

    if (loadParameters.effectiveSandboxFlags)
        localFrame->updateSandboxFlags(loadParameters.effectiveSandboxFlags, Frame::NotifyUIProcess::No);

    if (auto ownerPermissionsPolicy = std::exchange(loadParameters.ownerPermissionsPolicy, { }))
        localFrame->setOwnerPermissionsPolicy(WTFMove(*ownerPermissionsPolicy));

    localFrame->loader().setHTTPFallbackInProgress(loadParameters.isPerformingHTTPFallback);
    localFrame->loader().setRequiredCookiesVersion(loadParameters.requiredCookiesVersion);
    localFrame->loader().load(WTFMove(frameLoadRequest), WTFMove(loadParameters.requester));

    ASSERT(!m_pendingNavigationID);
    ASSERT(!m_internals->pendingWebsitePolicies);
}

// LoadRequestWaitingForProcessLaunch should never be sent to the WebProcess. It must always be converted to a LoadRequest message.
void WebPage::loadRequestWaitingForProcessLaunch(LoadParameters&&, URL&&, WebPageProxyIdentifier, bool)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void WebPage::loadDataImpl(std::optional<WebCore::NavigationIdentifier> navigationID, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<WebsitePoliciesData>&& websitePolicies, Ref<FragmentedSharedBuffer>&& sharedBuffer, ResourceRequest&& request, ResourceResponse&& response, URL&& unreachableURL, const UserData& userData, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
#if ENABLE(APP_BOUND_DOMAINS)
    Ref mainFrame = m_mainFrame.copyRef();
    setIsNavigatingToAppBoundDomain(isNavigatingToAppBoundDomain, mainFrame.get());
    mainFrame->setIsSafeBrowsingCheckOngoing(SafeBrowsingCheckOngoing::No);
#else
    UNUSED_PARAM(isNavigatingToAppBoundDomain);
#endif

    SendStopResponsivenessTimer stopper;

    m_pendingNavigationID = navigationID;
    m_internals->pendingWebsitePolicies = WTFMove(websitePolicies);

    SubstituteData substituteData(WTFMove(sharedBuffer), WTFMove(unreachableURL), WTFMove(response), sessionHistoryVisibility);

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient->willLoadDataRequest(*this, request, substituteData.content(), substituteData.mimeType(), substituteData.textEncoding(), substituteData.failingURL(), WebProcess::singleton().transformHandlesToObjects(userData.protectedObject().get()).get());

    RefPtr localFrame = m_mainFrame->coreLocalFrame() ? m_mainFrame->coreLocalFrame() : m_mainFrame->provisionalFrame();
    if (!localFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Initate the load in WebCore.
    FrameLoadRequest frameLoadRequest(*localFrame, WTFMove(request), WTFMove(substituteData));
    frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy);
    frameLoadRequest.setShouldTreatAsContinuingLoad(shouldTreatAsContinuingLoad);
    frameLoadRequest.setIsRequestFromClientOrUserInput();
    localFrame->loader().load(WTFMove(frameLoadRequest));
}

void WebPage::loadData(LoadParameters&& loadParameters)
{
    WEBPAGE_RELEASE_LOG(Loading, "loadData: navigationID=%" PRIu64 ", shouldTreatAsContinuingLoad=%u", loadParameters.navigationID ? loadParameters.navigationID->toUInt64() : 0, static_cast<unsigned>(loadParameters.shouldTreatAsContinuingLoad));

    platformDidReceiveLoadParameters(loadParameters);

    RefPtr sharedBuffer = loadParameters.data;
    if (!sharedBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    URL baseURL;
    if (loadParameters.baseURLString.isEmpty())
        baseURL = aboutBlankURL();
    else {
        baseURL = URL { WTFMove(loadParameters.baseURLString) };
        if (baseURL.isValid() && !baseURL.protocolIsInHTTPFamily())
            LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(baseURL.protocol().toString());
    }

    if (loadParameters.isServiceWorkerLoad && corePage())
        corePage()->markAsServiceWorkerPage();

    ResourceResponse response(URL(), WTFMove(loadParameters.MIMEType), sharedBuffer->size(), WTFMove(loadParameters.encodingName));
    loadDataImpl(loadParameters.navigationID, loadParameters.shouldTreatAsContinuingLoad, WTFMove(loadParameters.websitePolicies), sharedBuffer.releaseNonNull(), ResourceRequest(WTFMove(baseURL)), WTFMove(response), URL(), loadParameters.userData, loadParameters.isNavigatingToAppBoundDomain, loadParameters.sessionHistoryVisibility, loadParameters.shouldOpenExternalURLsPolicy);
}

void WebPage::loadAlternateHTML(LoadParameters&& loadParameters)
{
    platformDidReceiveLoadParameters(loadParameters);

    URL baseURL = loadParameters.baseURLString.isEmpty() ? aboutBlankURL() : URL { WTFMove(loadParameters.baseURLString) };
    URL unreachableURL = loadParameters.unreachableURLString.isEmpty() ? URL() : URL { WTFMove(loadParameters.unreachableURLString) };
    URL provisionalLoadErrorURL = loadParameters.provisionalLoadErrorURLString.isEmpty() ? URL() : URL { WTFMove(loadParameters.provisionalLoadErrorURLString) };
    RefPtr sharedBuffer = loadParameters.data;
    if (!sharedBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_mainFrame->coreLocalFrame()->loader().setProvisionalLoadErrorBeingHandledURL(provisionalLoadErrorURL);

    ResourceResponse response(URL(), WTFMove(loadParameters.MIMEType), sharedBuffer->size(), WTFMove(loadParameters.encodingName));
    loadDataImpl(loadParameters.navigationID, loadParameters.shouldTreatAsContinuingLoad, WTFMove(loadParameters.websitePolicies), sharedBuffer.releaseNonNull(), ResourceRequest(WTFMove(baseURL)), WTFMove(response), WTFMove(unreachableURL), loadParameters.userData, loadParameters.isNavigatingToAppBoundDomain, WebCore::SubstituteData::SessionHistoryVisibility::Hidden);
    m_mainFrame->coreLocalFrame()->loader().setProvisionalLoadErrorBeingHandledURL({ });
}

void WebPage::loadSimulatedRequestAndResponse(LoadParameters&& loadParameters, ResourceResponse&& simulatedResponse)
{
    setLastNavigationWasAppInitiated(loadParameters.request.isAppInitiated());
    RefPtr sharedBuffer = loadParameters.data;
    if (!sharedBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }
    loadDataImpl(loadParameters.navigationID, loadParameters.shouldTreatAsContinuingLoad, WTFMove(loadParameters.websitePolicies), sharedBuffer.releaseNonNull(), WTFMove(loadParameters.request), WTFMove(simulatedResponse), URL(), loadParameters.userData, loadParameters.isNavigatingToAppBoundDomain, SubstituteData::SessionHistoryVisibility::Visible);
}

void WebPage::navigateToPDFLinkWithSimulatedClick(const String& url, IntPoint documentPoint, IntPoint screenPoint)
{
    RefPtr mainFrame = m_mainFrame->coreLocalFrame();
    RefPtr mainFrameDocument = mainFrame->document();
    if (!mainFrameDocument)
        return;

    const int singleClick = 1;
    // FIXME: Set modifier keys.
    // FIXME: This should probably set IsSimulated::Yes.
    auto mouseEvent = MouseEvent::create(eventNames().clickEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, Event::IsComposed::Yes,
        MonotonicTime::now(), nullptr, singleClick, screenPoint, documentPoint, 0, 0, { }, MouseButton::Left, 0, nullptr, 0, WebCore::SyntheticClickType::NoTap, { }, { });

    mainFrame->loader().changeLocation(mainFrameDocument->completeURL(url), emptyAtom(), mouseEvent.ptr(), ReferrerPolicy::NoReferrer, ShouldOpenExternalURLsPolicy::ShouldAllow);
}

void WebPage::stopLoading()
{
    if (!m_page || !m_mainFrame->coreLocalFrame())
        return;

    SendStopResponsivenessTimer stopper;

    Ref coreFrame = *m_mainFrame->coreLocalFrame();
    coreFrame->loader().stopForUserCancel();
    coreFrame->loader().completePageTransitionIfNeeded();
}

void WebPage::stopLoadingDueToProcessSwap()
{
    SetForScope isStoppingLoadingDueToProcessSwap(m_isStoppingLoadingDueToProcessSwap, true);
    stopLoading();
}

bool WebPage::defersLoading() const
{
    return m_page->defersLoading();
}

void WebPage::reload(WebCore::NavigationIdentifier navigationID, OptionSet<WebCore::ReloadOption> reloadOptions, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper;

    ASSERT(!m_mainFrame->coreLocalFrame()->loader().frameHasLoaded() || !m_pendingNavigationID);
    m_pendingNavigationID = navigationID;

    Ref mainFrame = m_mainFrame;
    m_sandboxExtensionTracker.beginReload(mainFrame.ptr(), WTFMove(sandboxExtensionHandle));
    if (m_page && mainFrame->coreLocalFrame()) {
        bool isRequestFromClientOrUserInput = true;
        mainFrame->coreLocalFrame()->loader().reload(reloadOptions, isRequestFromClientOrUserInput);
    } else
        ASSERT_NOT_REACHED();

    if (m_pendingNavigationID) {
        // This can happen if FrameLoader::reload() returns early because the document URL is empty.
        // The reload does nothing so we need to reset the pending navigation. See webkit.org/b/153210.
        m_pendingNavigationID = std::nullopt;
    }
}

void WebPage::goToBackForwardItem(GoToBackForwardItemParameters&& parameters)
{
    WEBPAGE_RELEASE_LOG(Loading, "goToBackForwardItem: navigationID=%" PRIu64 ", backForwardItemID=%s, shouldTreatAsContinuingLoad=%u, lastNavigationWasAppInitiated=%d, existingNetworkResourceLoadIdentifierToResume=%" PRIu64, parameters.navigationID.toUInt64(), parameters.frameState->itemID->toString().utf8().data(), static_cast<unsigned>(parameters.shouldTreatAsContinuingLoad), parameters.lastNavigationWasAppInitiated, parameters.existingNetworkResourceLoadIdentifierToResume ? parameters.existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);
    SendStopResponsivenessTimer stopper;

    m_sandboxExtensionTracker.beginLoad(WTFMove(parameters.sandboxExtensionHandle));

    m_lastNavigationWasAppInitiated = parameters.lastNavigationWasAppInitiated;
    if (RefPtr localMainFrame = protectedCorePage()->localMainFrame()) {
        if (RefPtr documentLoader = localMainFrame->loader().documentLoader())
            documentLoader->setLastNavigationWasAppInitiated(parameters.lastNavigationWasAppInitiated);
    }

    WebProcess::singleton().webLoaderStrategy().setExistingNetworkResourceLoadIdentifierToResume(parameters.existingNetworkResourceLoadIdentifierToResume);
    auto resumingLoadScope = makeScopeExit([] {
        WebProcess::singleton().webLoaderStrategy().setExistingNetworkResourceLoadIdentifierToResume(std::nullopt);
    });

    ASSERT(isBackForwardLoadType(parameters.backForwardType));

    RefPtr<HistoryItem> item;
    {
        auto ignoreHistoryItemChangesForScope = m_historyItemClient->ignoreChangesForScope();
        item = toHistoryItem(m_historyItemClient, parameters.frameState);
    }

    LOG(Loading, "In WebProcess pid %i, WebPage %" PRIu64 " is navigating to back/forward URL %s", getCurrentProcessID(), m_identifier.toUInt64(), item->url().string().utf8().data());

#if PLATFORM(COCOA)
    WebCore::PublicSuffixStore::singleton().addPublicSuffix(parameters.publicSuffix);
#endif

    m_pendingNavigationID = parameters.navigationID;
    m_internals->pendingWebsitePolicies = WTFMove(parameters.websitePolicies);

    Ref targetFrame = m_mainFrame;
    if (RefPtr historyItemFrame = WebProcess::singleton().webFrame(item->frameID()); historyItemFrame && historyItemFrame->page() == this)
        targetFrame = historyItemFrame.releaseNonNull();

    if (RefPtr targetLocalFrame = targetFrame->provisionalFrame() ? targetFrame->provisionalFrame() : targetFrame->coreLocalFrame())
        protectedCorePage()->goToItem(*targetLocalFrame, *item, parameters.backForwardType, parameters.shouldTreatAsContinuingLoad, parameters.processSwapDisposition);
}

// GoToBackForwardItemWaitingForProcessLaunch should never be sent to the WebProcess. It must always be converted to a GoToBackForwardItem message.
void WebPage::goToBackForwardItemWaitingForProcessLaunch(GoToBackForwardItemParameters&&, WebKit::WebPageProxyIdentifier)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void WebPage::tryRestoreScrollPosition()
{
    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->loader().history().restoreScrollPositionAndViewState();
}

WebPage* WebPage::fromCorePage(Page& page)
{
    auto& client = page.chrome().client();
    return client.isEmptyChromeClient() ? nullptr : downcast<WebChromeClient>(client).page();
}

RefPtr<WebCore::Page> WebPage::protectedCorePage() const
{
    return corePage();
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    if (m_viewSize == viewSize)
        return;

    m_viewSize = viewSize;
    RefPtr view = protectedCorePage()->protectedMainFrame()->virtualView();
    if (!view) {
        ASSERT_NOT_REACHED();
        return;
    }

    view->resize(viewSize);
    protectedDrawingArea()->setNeedsDisplay();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    cacheAXSize(m_viewSize);
#endif
}

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
#if PLATFORM(MAC)
    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame)
        return;
    RefPtr mainFrameView = localMainFrame->view();
    LocalDefaultSystemAppearance localAppearance(mainFrameView ? mainFrameView->useDarkAppearance() : false);
#endif

    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);

    m_mainFrame->coreLocalFrame()->protectedView()->paint(graphicsContext, rect);

#if PLATFORM(GTK) || PLATFORM(WIN) || PLATFORM(PLAYSTATION)
    if (!m_page->settings().acceleratedCompositingEnabled() && m_page->inspectorController().enabled() && m_page->inspectorController().shouldShowOverlay()) {
        graphicsContext.beginTransparencyLayer(1);
        m_page->inspectorController().drawHighlight(graphicsContext);
        graphicsContext.endTransparencyLayer();
    }
#endif
}

double WebPage::textZoomFactor() const
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView && pluginView->pluginHandlesPageScaleFactor())
        return pluginView->pageScaleFactor();
#endif

    RefPtr frame = m_mainFrame->coreLocalFrame();
    if (!frame)
        return 1;
    return frame->textZoomFactor();
}

void WebPage::didSetTextZoomFactor(double zoomFactor)
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView && pluginView->pluginHandlesPageScaleFactor())
        return pluginView->setPageScaleFactor(zoomFactor, std::nullopt);
#endif

    if (!m_page)
        return;

    for (WeakRef frame : m_page->rootFrames())
        frame->setTextZoomFactor(static_cast<float>(zoomFactor));
}

double WebPage::pageZoomFactor() const
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView && pluginView->pluginHandlesPageScaleFactor()) {
        // Note that this maps page *scale* factor to page *zoom* factor.
        return pluginView->pageScaleFactor();
    }
#endif

    RefPtr frame = m_mainFrame->coreLocalFrame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void WebPage::didSetPageZoomFactor(double zoomFactor)
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView && pluginView->pluginHandlesPageScaleFactor()) {
        // Note that this maps page *zoom* factor to page *scale* factor.
        pluginView->setPageScaleFactor(zoomFactor, std::nullopt);
        return;
    }
#endif

    if (!m_page)
        return;

    for (WeakRef frame : m_page->rootFrames())
        frame->setPageZoomFactor(static_cast<float>(zoomFactor));
}

static void dumpHistoryItem(HistoryItem& item, size_t indent, bool isCurrentItem, StringBuilder& stringBuilder, const String& directoryName)
{
    if (isCurrentItem)
        stringBuilder.append("curr->  "_s);
    else {
        for (size_t i = 0; i < indent; ++i)
            stringBuilder.append(' ');
    }

    auto url = item.url();
    if (url.protocolIsFile()) {
        size_t start = url.string().find(directoryName);
        if (start == WTF::notFound)
            start = 0;
        else
            start += directoryName.length();
        stringBuilder.append("(file test):"_s, StringView { url.string() }.substring(start));
    } else
        stringBuilder.append(url.string());

    auto& target = item.target();
    if (target.length())
        stringBuilder.append(" (in frame \""_s, target, "\")"_s);

    stringBuilder.append('\n');

    auto children = item.children();
    std::ranges::stable_sort(children, [](auto& a, auto& b) {
        return codePointCompare(a->target(), b->target()) < 0;
    });
    for (auto& child : children)
        dumpHistoryItem(child, indent + 4, false, stringBuilder, directoryName);
}

String WebPage::dumpHistoryForTesting(const String& directory)
{
    if (!m_page)
        return { };

    CheckedRef list = m_page->backForward();

    StringBuilder builder;
    int begin = -list->backCount();
    if (list->itemAtIndex(begin)->url() == aboutBlankURL())
        ++begin;
    for (int i = begin; i <= static_cast<int>(list->forwardCount()); ++i)
        dumpHistoryItem(*list->itemAtIndex(i), 8, !i, builder, directory);
    return builder.toString();
}

String WebPage::frameTextForTestingIncludingSubframes(bool includeSubframes)
{
    return m_mainFrame->frameTextForTesting(includeSubframes);
}

void WebPage::windowScreenDidChange(PlatformDisplayID displayID, std::optional<unsigned> nominalFramesPerSecond)
{
    m_page->chrome().windowScreenDidChange(displayID, nominalFramesPerSecond);

#if PLATFORM(MAC)
    WebProcess::singleton().updatePageScreenProperties();
#endif
}

void WebPage::didScalePage(double scale, const IntPoint& origin)
{
    double totalScale = scale * viewScaleFactor();
    bool willChangeScaleFactor = totalScale != totalScaleFactor();
    auto platformDidScalePageIfNeeded = makeScopeExit([willChangeScaleFactor, this, protectedThis = Ref { *this }] {
        if (willChangeScaleFactor)
            platformDidScalePage();
    });

#if PLATFORM(IOS_FAMILY)
    if (willChangeScaleFactor) {
        if (!m_inDynamicSizeUpdate)
            m_internals->dynamicSizeUpdateHistory.clear();
        m_scaleWasSetByUIProcess = false;
    }
#endif

    RefPtr page = m_page;
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView && pluginView->pluginHandlesPageScaleFactor()) {
        // Whenever the PDF plug-in handles the page scale factor, make sure to reset WebCore's page scale.
        // Otherwise, we can end up with an immutable but non-1 page scale applied by WebCore on top of whatever the plugin does.
        if (page->pageScaleFactor() != 1)
            page->setPageScaleFactor(1, origin);
        pluginView->setPageScaleFactor(totalScale, { origin });
        return;
    }
#endif

    page->setPageScaleFactor(totalScale, origin);

    // We can't early return before setPageScaleFactor because the origin might be different.
    if (!willChangeScaleFactor)
        return;

#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews) {
        if (pluginView->pluginHandlesPageScaleFactor())
            pluginView->setPageScaleFactor(totalScale, { origin });
    }
#endif
}

void WebPage::didScalePageInViewCoordinates(double scale, const IntPoint& origin)
{
    RefPtr frameView = localMainFrameView();
    if (!frameView)
        return;
    auto adjustedOrigin = frameView->rootViewToContents(-origin);
    double scaleRatio = scale / pageScaleFactor();
    adjustedOrigin.scale(scaleRatio);

    didScalePage(scale, adjustedOrigin);
}

void WebPage::didScalePageRelativeToScrollPosition(double scale, const IntPoint& origin)
{
    RefPtr frameView = localMainFrameView();
    if (!frameView)
        return;
    auto unscrolledOrigin = origin;
    IntRect unobscuredContentRect = frameView->unobscuredContentRectIncludingScrollbars();
    unscrolledOrigin.moveBy(-unobscuredContentRect.location());

    didScalePage(scale, -unscrolledOrigin);
}

#if !PLATFORM(IOS_FAMILY)

void WebPage::platformDidScalePage()
{
}

#endif

void WebPage::scalePage(double scale, const IntPoint& origin)
{
    didScalePage(scale, origin);
    send(Messages::WebPageProxy::PageScaleFactorDidChange(scale));
}

double WebPage::totalScaleFactor() const
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView && pluginView->pluginHandlesPageScaleFactor())
        return pluginView->pageScaleFactor();
#endif
    return m_page->pageScaleFactor();
}

double WebPage::pageScaleFactor() const
{
    return totalScaleFactor() / viewScaleFactor();
}

double WebPage::viewScaleFactor() const
{
    return m_page->viewScaleFactor();
}

void WebPage::didScaleView(double scale)
{
    if (viewScaleFactor() == scale)
        return;

    float pageScale = pageScaleFactor();

    RefPtr page = m_page;
    IntPoint scrollPositionAtNewScale;
    if (RefPtr mainFrameView = page->protectedMainFrame()->virtualView()) {
        double scaleRatio = scale / viewScaleFactor();
        scrollPositionAtNewScale = mainFrameView->scrollPosition();
        scrollPositionAtNewScale.scale(scaleRatio);
    }

    page->setViewScaleFactor(scale);
    didScalePage(pageScale, scrollPositionAtNewScale);
}

void WebPage::scaleView(double scale)
{
    if (scale == viewScaleFactor())
        return;
    didScaleView(scale);
    send(Messages::WebPageProxy::ViewScaleFactorDidChange(scale));
}

void WebPage::setDeviceScaleFactor(float scaleFactor)
{
    RefPtr page = m_page;
    if (scaleFactor == page->deviceScaleFactor())
        return;

    page->setDeviceScaleFactor(scaleFactor);

    // Tell all our plug-in views that the device scale factor changed.
#if PLATFORM(MAC)
    for (Ref pluginView : m_pluginViews)
        pluginView->setDeviceScaleFactor(scaleFactor);

    updateHeaderAndFooterLayersForDeviceScaleChange(scaleFactor);
#endif

#if USE(SKIA)
    FontRenderOptions::singleton().setUseSubpixelPositioning(scaleFactor >= 2.);
#endif

    if (findController().isShowingOverlay()) {
        // We must have updated layout to get the selection rects right.
        layoutIfNeeded();
        findController().deviceScaleFactorDidChange();
    }
}

float WebPage::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

void WebPage::accessibilitySettingsDidChange()
{
    protectedCorePage()->accessibilitySettingsDidChange();
}

void WebPage::enableAccessibilityForAllProcesses()
{
    send(Messages::WebPageProxy::EnableAccessibilityForAllProcesses());
}

void WebPage::enableAccessibility()
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();
}

void WebPage::screenPropertiesDidChange(bool affectsStyle)
{
    protectedCorePage()->screenPropertiesDidChange(affectsStyle);
}

void WebPage::setUseFixedLayout(bool fixed)
{
    // Do not overwrite current settings if initially setting it to false.
    if (m_useFixedLayout == fixed)
        return;
    m_useFixedLayout = fixed;

#if !PLATFORM(IOS_FAMILY)
    m_page->settings().setFixedElementsLayoutRelativeToFrame(fixed);
#endif

    RefPtr view = localMainFrameView();
    if (!view)
        return;

    view->setUseFixedLayout(fixed);
    if (!fixed)
        setFixedLayoutSize(IntSize());

    send(Messages::WebPageProxy::UseFixedLayoutDidChange(fixed));
}

bool WebPage::setFixedLayoutSize(const IntSize& size)
{
    RefPtr view = localMainFrameView();
    if (!view || view->fixedLayoutSize() == size)
        return false;

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_identifier.toUInt64() << " setFixedLayoutSize " << size);
    view->setFixedLayoutSize(size);

    send(Messages::WebPageProxy::FixedLayoutSizeDidChange(size));
    return true;
}

IntSize WebPage::fixedLayoutSize() const
{
    RefPtr view = localMainFrameView();
    if (!view)
        return IntSize();
    return view->fixedLayoutSize();
}

void WebPage::setDefaultUnobscuredSize(const FloatSize& defaultUnobscuredSize)
{
    if (defaultUnobscuredSize == m_defaultUnobscuredSize)
        return;

    m_defaultUnobscuredSize = defaultUnobscuredSize;

    updateSizeForCSSDefaultViewportUnits();
}

void WebPage::updateSizeForCSSDefaultViewportUnits()
{
    RefPtr mainFrameView = this->localMainFrameView();
    if (!mainFrameView)
        return;

    auto defaultUnobscuredSize = m_defaultUnobscuredSize;
#if ENABLE(META_VIEWPORT)
    if (defaultUnobscuredSize.isEmpty())
        defaultUnobscuredSize = m_viewportConfiguration.viewLayoutSize();
    defaultUnobscuredSize.scale(1 / m_viewportConfiguration.initialScaleIgnoringContentSize());
#endif
    mainFrameView->setSizeForCSSDefaultViewportUnits(defaultUnobscuredSize);
}

void WebPage::setMinimumUnobscuredSize(const FloatSize& minimumUnobscuredSize)
{
    if (minimumUnobscuredSize == m_minimumUnobscuredSize)
        return;

    m_minimumUnobscuredSize = minimumUnobscuredSize;

    updateSizeForCSSSmallViewportUnits();
}

void WebPage::updateSizeForCSSSmallViewportUnits()
{
    RefPtr mainFrameView = this->localMainFrameView();
    if (!mainFrameView)
        return;

    auto minimumUnobscuredSize = m_minimumUnobscuredSize;
#if ENABLE(META_VIEWPORT)
    if (minimumUnobscuredSize.isEmpty())
        minimumUnobscuredSize = m_viewportConfiguration.viewLayoutSize();
    minimumUnobscuredSize.scale(1 / m_viewportConfiguration.initialScaleIgnoringContentSize());
#endif
    mainFrameView->setSizeForCSSSmallViewportUnits(minimumUnobscuredSize);
}

void WebPage::setMaximumUnobscuredSize(const FloatSize& maximumUnobscuredSize)
{
    if (maximumUnobscuredSize == m_maximumUnobscuredSize)
        return;

    m_maximumUnobscuredSize = maximumUnobscuredSize;

    updateSizeForCSSLargeViewportUnits();
}

void WebPage::updateSizeForCSSLargeViewportUnits()
{
    RefPtr mainFrameView = this->localMainFrameView();
    if (!mainFrameView)
        return;

    auto maximumUnobscuredSize = m_maximumUnobscuredSize;
#if ENABLE(META_VIEWPORT)
    if (maximumUnobscuredSize.isEmpty())
        maximumUnobscuredSize = m_viewportConfiguration.viewLayoutSize();
    maximumUnobscuredSize.scale(1 / m_viewportConfiguration.initialScaleIgnoringContentSize());
#endif
    mainFrameView->setSizeForCSSLargeViewportUnits(maximumUnobscuredSize);
}

void WebPage::disabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations)
{
#if PLATFORM(IOS_FAMILY)
    if (m_viewportConfiguration.setDisabledAdaptations(disabledAdaptations))
        viewportConfigurationChanged();
#else
    UNUSED_PARAM(disabledAdaptations);
#endif
}

void WebPage::viewportPropertiesDidChange(const ViewportArguments& viewportArguments)
{
#if PLATFORM(IOS_FAMILY)
    if (m_viewportConfiguration.setViewportArguments(viewportArguments))
        viewportConfigurationChanged();
#elif PLATFORM(GTK) || PLATFORM(WPE)
    // Adjust view dimensions when using fixed layout.
    RefPtr localMainFrame = this->localMainFrame();
    RefPtr view = localMainFrame ? localMainFrame->view() : nullptr;
    if (view && view->useFixedLayout() && !m_viewSize.isEmpty()) {
        Settings& settings = m_page->settings();
        int deviceWidth = (settings.deviceWidth() > 0) ? settings.deviceWidth() : m_viewSize.width();
        int deviceHeight = (settings.deviceHeight() > 0) ? settings.deviceHeight() : m_viewSize.height();
        int minimumLayoutFallbackWidth = std::max<int>(settings.layoutFallbackWidth(), m_viewSize.width());
        ViewportAttributes attr = computeViewportAttributes(viewportArguments, minimumLayoutFallbackWidth, deviceWidth, deviceHeight, 1, m_viewSize);
        setFixedLayoutSize(roundedIntSize(attr.layoutSize));
        scaleView(deviceWidth / attr.layoutSize.width());
    }
#else
    UNUSED_PARAM(viewportArguments);
#endif
}

#if !PLATFORM(IOS_FAMILY)

FloatSize WebPage::screenSizeForFingerprintingProtections(const LocalFrame& frame, FloatSize defaultSize) const
{
    return frame.view() ? FloatSize { frame.protectedView()->unobscuredContentRectIncludingScrollbars().size() } : defaultSize;
}

#endif // !PLATFORM(IOS_FAMILY)

void WebPage::listenForLayoutMilestones(OptionSet<WebCore::LayoutMilestone> milestones)
{
    if (RefPtr page = m_page)
        page->addLayoutMilestones(milestones);
}

void WebPage::setSuppressScrollbarAnimations(bool suppressAnimations)
{
    protectedCorePage()->setShouldSuppressScrollbarAnimations(suppressAnimations);
}

void WebPage::setEnableVerticalRubberBanding(bool enableVerticalRubberBanding)
{
    protectedCorePage()->setVerticalScrollElasticity(enableVerticalRubberBanding ? ScrollElasticity::Allowed : ScrollElasticity::None);
}

void WebPage::setEnableHorizontalRubberBanding(bool enableHorizontalRubberBanding)
{
    protectedCorePage()->setHorizontalScrollElasticity(enableHorizontalRubberBanding ? ScrollElasticity::Allowed : ScrollElasticity::None);
}

void WebPage::setBackgroundExtendsBeyondPage(bool backgroundExtendsBeyondPage)
{
    if (m_page->settings().backgroundShouldExtendBeyondPage() != backgroundExtendsBeyondPage)
        m_page->settings().setBackgroundShouldExtendBeyondPage(backgroundExtendsBeyondPage);
}

void WebPage::setPaginationMode(Pagination::Mode mode)
{
    RefPtr page = m_page;
    Pagination pagination = page->pagination();
    pagination.mode = static_cast<Pagination::Mode>(mode);
    page->setPagination(pagination);
}

void WebPage::setPaginationBehavesLikeColumns(bool behavesLikeColumns)
{
    RefPtr page = m_page;
    Pagination pagination = page->pagination();
    pagination.behavesLikeColumns = behavesLikeColumns;
    page->setPagination(pagination);
}

void WebPage::setPageLength(double pageLength)
{
    RefPtr page = m_page;
    Pagination pagination = page->pagination();
    pagination.pageLength = pageLength;
    page->setPagination(pagination);
}

void WebPage::setGapBetweenPages(double gap)
{
    RefPtr page = m_page;
    Pagination pagination = page->pagination();
    pagination.gap = gap;
    page->setPagination(pagination);
}

void WebPage::postInjectedBundleMessage(const String& messageName, const UserData& userData)
{
    auto& webProcess = WebProcess::singleton();
    RefPtr injectedBundle = webProcess.injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->didReceiveMessageToPage(Ref { *this }, messageName, webProcess.transformHandlesToObjects(userData.protectedObject().get()));
}

void WebPage::setUnderPageBackgroundColorOverride(WebCore::Color&& underPageBackgroundColorOverride)
{
    protectedCorePage()->setUnderPageBackgroundColorOverride(WTFMove(underPageBackgroundColorOverride));
}

void WebPage::setShouldSuppressHDR(bool shouldSuppressHDR)
{
    protectedCorePage()->setShouldSuppressHDR(shouldSuppressHDR);
}

#if !PLATFORM(IOS_FAMILY)

void WebPage::setHeaderPageBanner(PageBanner* pageBanner)
{
    if (RefPtr headerBanner = m_headerBanner)
        headerBanner->detachFromPage();

    m_headerBanner = pageBanner;

    if (RefPtr headerBanner = m_headerBanner)
        headerBanner->addToPage(PageBanner::Header, this);
}

PageBanner* WebPage::headerPageBanner()
{
    return m_headerBanner.get();
}

void WebPage::setFooterPageBanner(PageBanner* pageBanner)
{
    if (RefPtr footerBanner = m_footerBanner)
        footerBanner->detachFromPage();

    m_footerBanner = pageBanner;

    if (RefPtr footerBanner = m_footerBanner)
        footerBanner->addToPage(PageBanner::Footer, this);
}

PageBanner* WebPage::footerPageBanner()
{
    return m_footerBanner.get();
}

void WebPage::hidePageBanners()
{
    if (RefPtr headerBanner = m_headerBanner)
        headerBanner->hide();
    if (RefPtr footerBanner = m_footerBanner)
        footerBanner->hide();
}

void WebPage::showPageBanners()
{
    if (RefPtr headerBanner = m_headerBanner)
        headerBanner->showIfHidden();
    if (RefPtr footerBanner = m_footerBanner)
        footerBanner->showIfHidden();
}

#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
void WebPage::setHeaderBannerHeight(int height)
{
    protectedCorePage()->setHeaderHeight(height);
}

void WebPage::setFooterBannerHeight(int height)
{
    protectedCorePage()->setFooterHeight(height);
}
#endif

RefPtr<ShareableBitmap> WebPage::shareableBitmapSnapshotForNode(Node& node)
{
    // Ensure that the image contains at most 600K pixels, so that it is not too big.
    if (auto snapshot = snapshotNode(node, SnapshotOption::Shareable, 600 * 1024))
        return snapshot->bitmap();
    return nullptr;
}

void WebPage::takeSnapshot(IntRect snapshotRect, IntSize bitmapSize, SnapshotOptions snapshotOptions, CompletionHandler<void(std::optional<ImageBufferBackendHandle>&&, Headroom)>&& completionHandler)
{
    std::optional<ImageBufferBackendHandle> handle;
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    if (!coreFrame) {
        completionHandler(WTFMove(handle), Headroom::None);
        return;
    }

    RefPtr frameView = coreFrame->view();
    if (!frameView) {
        completionHandler(WTFMove(handle), Headroom::None);
        return;
    }

    snapshotOptions.add(SnapshotOption::Shareable);

    auto originalLayoutViewportOverrideRect = frameView->layoutViewportOverrideRect();
    auto originalPaintBehavior = frameView->paintBehavior();
    auto paintBehavior = originalPaintBehavior;

    if (snapshotOptions.contains(SnapshotOption::VisibleContentRect))
        snapshotRect = frameView->visibleContentRect();
    else if (snapshotOptions.contains(SnapshotOption::FullContentRect)) {
        snapshotRect = IntRect({ 0, 0 }, frameView->contentsSize());
        frameView->setLayoutViewportOverrideRect(LayoutRect(snapshotRect));
        paintBehavior.add(PaintBehavior::AnnotateLinks);
    }

#if HAVE(SUPPORT_HDR_DISPLAY)
    if (snapshotOptions.contains(SnapshotOption::AllowHDR) && protectedCorePage()->drawsHDRContent())
        paintBehavior.add(PaintBehavior::DrawsHDRContent);
#endif

    if (originalPaintBehavior != paintBehavior)
        frameView->setPaintBehavior(paintBehavior);

    if (bitmapSize.isEmpty()) {
        bitmapSize = snapshotRect.size();
        if (!snapshotOptions.contains(SnapshotOption::ExcludeDeviceScaleFactor))
            bitmapSize.scale(corePage()->deviceScaleFactor());
    }

    Headroom headroom = Headroom::None;
    if (auto image = snapshotAtSize(snapshotRect, bitmapSize, snapshotOptions, *coreFrame, *frameView)) {
        handle = image->createImageBufferBackendHandle(SharedMemory::Protection::ReadOnly);
#if HAVE(SUPPORT_HDR_DISPLAY)
        if (image->context())
            headroom = Headroom(image->context()->maxPaintedEDRHeadroom());
#endif
    }

    if (originalPaintBehavior != paintBehavior) {
        frameView->setLayoutViewportOverrideRect(originalLayoutViewportOverrideRect);
        frameView->setPaintBehavior(originalPaintBehavior);
    }

    completionHandler(WTFMove(handle), headroom);
}

RefPtr<WebImage> WebPage::scaledSnapshotWithOptions(const IntRect& rect, double additionalScaleFactor, SnapshotOptions options)
{
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    RefPtr frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    IntRect snapshotRect = rect;
    IntSize bitmapSize = snapshotRect.size();
    if (options.contains(SnapshotOption::Printing)) {
        ASSERT(additionalScaleFactor == 1);
        bitmapSize.setHeight(PrintContext::numberOfPages(*coreFrame, bitmapSize) * (bitmapSize.height() + 1) - 1);
    } else {
        double scaleFactor = additionalScaleFactor;
        if (!options.contains(SnapshotOption::ExcludeDeviceScaleFactor))
            scaleFactor *= corePage()->deviceScaleFactor();
        bitmapSize.scale(scaleFactor);
    }

    return snapshotAtSize(rect, bitmapSize, options, *coreFrame, *frameView);
}

void WebPage::paintSnapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options, LocalFrame& frame, LocalFrameView& frameView, GraphicsContext& graphicsContext)
{
    TraceScope snapshotScope(PaintSnapshotStart, PaintSnapshotEnd, options.toRaw());

    IntRect snapshotRect = rect;
    float horizontalScaleFactor = static_cast<float>(bitmapSize.width()) / rect.width();
    float verticalScaleFactor = static_cast<float>(bitmapSize.height()) / rect.height();
    float scaleFactor = std::max(horizontalScaleFactor, verticalScaleFactor);

    if (options.contains(SnapshotOption::Printing)) {
        PrintContext::spoolAllPagesWithBoundaries(frame, graphicsContext, snapshotRect.size());
        return;
    }

    Color backgroundColor;
    Color savedBackgroundColor;
    if (options.contains(SnapshotOption::TransparentBackground)) {
        backgroundColor = Color::transparentBlack;
        savedBackgroundColor = frameView.baseBackgroundColor();
        frameView.setBaseBackgroundColor(backgroundColor);
    } else {
        Color documentBackgroundColor = frameView.documentBackgroundColor();
        backgroundColor = (frame.settings().backgroundShouldExtendBeyondPage() && documentBackgroundColor.isValid()) ? documentBackgroundColor : frameView.baseBackgroundColor();
    }
    graphicsContext.fillRect(IntRect(IntPoint(), bitmapSize), backgroundColor);

    if (!options.contains(SnapshotOption::ExcludeDeviceScaleFactor)) {
        double deviceScaleFactor = frame.page()->deviceScaleFactor();
        graphicsContext.applyDeviceScaleFactor(deviceScaleFactor);
        scaleFactor /= deviceScaleFactor;
    }

    graphicsContext.scale(scaleFactor);
    graphicsContext.translate(-snapshotRect.location());

    LocalFrameView::SelectionInSnapshot shouldPaintSelection = LocalFrameView::IncludeSelection;
    if (options.contains(SnapshotOption::ExcludeSelectionHighlighting))
        shouldPaintSelection = LocalFrameView::ExcludeSelection;

    LocalFrameView::CoordinateSpaceForSnapshot coordinateSpace = LocalFrameView::DocumentCoordinates;
    if (options.contains(SnapshotOption::InViewCoordinates))
        coordinateSpace = LocalFrameView::ViewCoordinates;

    frameView.paintContentsForSnapshot(graphicsContext, snapshotRect, shouldPaintSelection, coordinateSpace);

    if (options.contains(SnapshotOption::PaintSelectionRectangle)) {
        FloatRect selectionRectangle = frame.checkedSelection()->selectionBounds();
        graphicsContext.setStrokeColor(Color::red);
        graphicsContext.strokeRect(selectionRectangle, 1);
    }

    if (options.contains(SnapshotOption::TransparentBackground))
        frameView.setBaseBackgroundColor(savedBackgroundColor);
}

static DestinationColorSpace snapshotColorSpace(SnapshotOptions options, WebPage& page)
{
#if USE(CG)
    if (options.contains(SnapshotOption::UseScreenColorSpace)) {
        auto screenColorSpace = WebCore::screenColorSpace(page.protectedCorePage()->protectedMainFrame()->protectedVirtualView().get());
#if HAVE(SUPPORT_HDR_DISPLAY)
        if (options.contains(SnapshotOption::AllowHDR) && page.protectedCorePage()->drawsHDRContent()) {
            if (auto extendedScreenColorSpace = screenColorSpace.asExtended())
                return *extendedScreenColorSpace;
        }
#endif
        return screenColorSpace;
    }
#endif

#if HAVE(SUPPORT_HDR_DISPLAY)
    if (options.contains(SnapshotOption::AllowHDR) && page.protectedCorePage()->drawsHDRContent())
        return DestinationColorSpace::ExtendedSRGB();
#endif

    return DestinationColorSpace::SRGB();
}

RefPtr<WebImage> WebPage::snapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options, LocalFrame& frame, LocalFrameView& frameView)
{
#if ENABLE(PDF_PLUGIN)
    ImageOptions imageOptions = m_pluginViews.computeSize() ? ImageOption::Local : ImageOption::Shareable;
#else
    ImageOptions imageOptions = ImageOption::Shareable;
#endif

    if (options.contains(SnapshotOption::Accelerated))
        imageOptions.add(ImageOption::Accelerated);
    if (options.contains(SnapshotOption::AllowHDR))
        imageOptions.add(ImageOption::AllowHDR);

    auto snapshot = WebImage::create(bitmapSize, imageOptions, snapshotColorSpace(options, *this), &m_page->chrome().client());
    if (!snapshot->context())
        return nullptr;

    auto& graphicsContext = *snapshot->context();
#if HAVE(SUPPORT_HDR_DISPLAY)
    graphicsContext.setMaxEDRHeadroom(maxEDRHeadroomForDisplay(m_page->displayID()));
#endif
    paintSnapshotAtSize(rect, bitmapSize, options, frame, frameView, graphicsContext);

    return snapshot;
}

RefPtr<WebImage> WebPage::snapshotNode(WebCore::Node& node, SnapshotOptions options, unsigned maximumPixelCount)
{
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    RefPtr frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    if (!node.renderer())
        return nullptr;

    LayoutRect topLevelRect;
    IntRect snapshotRect = snappedIntRect(node.checkedRenderer()->paintingRootRect(topLevelRect));
    if (snapshotRect.isEmpty())
        return nullptr;

    double scaleFactor = 1;
    IntSize snapshotSize = snapshotRect.size();
    unsigned maximumHeight = maximumPixelCount / snapshotSize.width();
    if (maximumHeight < static_cast<unsigned>(snapshotSize.height())) {
        scaleFactor = static_cast<double>(maximumHeight) / snapshotSize.height();
        snapshotSize = IntSize(snapshotSize.width() * scaleFactor, maximumHeight);
    }

    auto snapshot = WebImage::create(snapshotSize, snapshotOptionsToImageOptions(options), snapshotColorSpace(options, *this), &m_page->chrome().client());
    if (!snapshot->context())
        return nullptr;

    auto& graphicsContext = *snapshot->context();

    if (!options.contains(SnapshotOption::ExcludeDeviceScaleFactor)) {
        double deviceScaleFactor = corePage()->deviceScaleFactor();
        graphicsContext.applyDeviceScaleFactor(deviceScaleFactor);
        scaleFactor /= deviceScaleFactor;
    }

    graphicsContext.scale(scaleFactor);
    graphicsContext.translate(-snapshotRect.location());

    Color savedBackgroundColor = frameView->baseBackgroundColor();
    frameView->setBaseBackgroundColor(Color::transparentBlack);
    frameView->setNodeToDraw(&node);

    frameView->paintContentsForSnapshot(graphicsContext, snapshotRect, LocalFrameView::ExcludeSelection, LocalFrameView::DocumentCoordinates);

    frameView->setBaseBackgroundColor(savedBackgroundColor);
    frameView->setNodeToDraw(nullptr);

    return snapshot;
}

void WebPage::pageDidScroll()
{
#if PLATFORM(IOS_FAMILY)
    if (!m_inDynamicSizeUpdate)
        m_internals->dynamicSizeUpdateHistory.clear();
#endif
    m_pageScrolledHysteresis.impulse();

    if (RefPtr view = protectedCorePage()->protectedMainFrame()->virtualView())
        send(Messages::WebPageProxy::PageDidScroll(view->scrollPosition()));
}

void WebPage::pageStoppedScrolling()
{
    // Maintain the current history item's scroll position up-to-date.
    if (RefPtr frame = m_mainFrame->coreLocalFrame())
        frame->loader().history().saveScrollPositionAndViewStateToItem(frame->loader().history().protectedCurrentItem().get());
}

void WebPage::setHasActiveAnimatedScrolls(bool hasActiveAnimatedScrolls)
{
    send(Messages::WebPageProxy::SetHasActiveAnimatedScrolls(hasActiveAnimatedScrolls));
}

#if ENABLE(CONTEXT_MENUS)
WebContextMenu& WebPage::contextMenu()
{
    if (!m_contextMenu)
        m_contextMenu = WebContextMenu::create(*this);
    return *m_contextMenu;
}

Ref<WebContextMenu> WebPage::protectedContextMenu()
{
    return contextMenu();
}

RefPtr<WebContextMenu> WebPage::contextMenuAtPointInWindow(FrameIdentifier frameID, const DoublePoint& point)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return nullptr;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    corePage()->contextMenuController().clearContextMenu();

    // Simulate a mouse click to generate the correct menu.
    PlatformMouseEvent mousePressEvent(point, point, MouseButton::Right, PlatformEvent::Type::MousePressed, 1, { }, MonotonicTime::now(), WebCore::ForceAtClick, WebCore::SyntheticClickType::NoTap);
    coreFrame->eventHandler().handleMousePressEvent(mousePressEvent);
    bool handled = coreFrame->eventHandler().sendContextMenuEvent(mousePressEvent);
    RefPtr menu = handled ? &contextMenu() : nullptr;
    PlatformMouseEvent mouseReleaseEvent(point, point, MouseButton::Right, PlatformEvent::Type::MouseReleased, 1, { }, MonotonicTime::now(), WebCore::ForceAtClick, WebCore::SyntheticClickType::NoTap);
    coreFrame->eventHandler().handleMouseReleaseEvent(mouseReleaseEvent);

    return menu;
}
#endif

// Events

static const WebEvent* g_currentEvent = 0;

// FIXME: WebPage::currentEvent is used by the plug-in code to avoid having to convert from DOM events back to
// WebEvents. When we get the event handling sorted out, this should go away and the Widgets should get the correct
// platform events passed to the event handler code.
const WebEvent* WebPage::currentEvent()
{
    return g_currentEvent;
}

void WebPage::freezeLayerTree(LayerTreeFreezeReason reason)
{
    auto oldReasons = m_layerTreeFreezeReasons.toRaw();
    UNUSED_PARAM(oldReasons);
    m_layerTreeFreezeReasons.add(reason);
    WEBPAGE_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPAGE_FREEZE_LAYER_TREE, static_cast<unsigned>(reason), m_layerTreeFreezeReasons.toRaw(), oldReasons);
    updateDrawingAreaLayerTreeFreezeState();
}

void WebPage::unfreezeLayerTree(LayerTreeFreezeReason reason)
{
    auto oldReasons = m_layerTreeFreezeReasons.toRaw();
    UNUSED_PARAM(oldReasons);
    m_layerTreeFreezeReasons.remove(reason);
    WEBPAGE_RELEASE_LOG_FORWARDABLE(ProcessSuspension, WEBPAGE_UNFREEZE_LAYER_TREE, static_cast<unsigned>(reason), m_layerTreeFreezeReasons.toRaw(), oldReasons);
    updateDrawingAreaLayerTreeFreezeState();
}

void WebPage::updateDrawingAreaLayerTreeFreezeState()
{
    RefPtr drawingArea = m_drawingArea;
    if (!drawingArea)
        return;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    // When the browser is in the background, we should not freeze the layer tree
    // if the page has a video playing in picture-in-picture.
    RefPtr videoPresentationManager = m_videoPresentationManager;
    if (videoPresentationManager && videoPresentationManager->hasVideoPlayingInPictureInPicture() && m_layerTreeFreezeReasons.hasExactlyOneBitSet() && m_layerTreeFreezeReasons.contains(LayerTreeFreezeReason::BackgroundApplication)) {
        drawingArea->setLayerTreeStateIsFrozen(false);
        return;
    }
#endif

    drawingArea->setLayerTreeStateIsFrozen(!!m_layerTreeFreezeReasons);
}

void WebPage::updateFrameScrollingMode(FrameIdentifier frameID, ScrollbarMode scrollingMode)
{
    if (!m_page)
        return;

    ASSERT(m_page->settings().siteIsolationEnabled());
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return;

    RefPtr frame = webFrame->coreLocalFrame();
    if (!frame)
        return;

    frame->setScrollingMode(scrollingMode);
}

void WebPage::tryMarkLayersVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr drawingArea = m_drawingArea;
    if (!drawingArea) {
        completionHandler(false);
        return;
    }

    drawingArea->tryMarkLayersVolatile(WTFMove(completionHandler));
}

void WebPage::callVolatilityCompletionHandlers(bool succeeded)
{
    auto completionHandlers = std::exchange(m_markLayersAsVolatileCompletionHandlers, { });
    for (auto& completionHandler : completionHandlers)
        completionHandler(succeeded);
}

void WebPage::layerVolatilityTimerFired()
{
    m_layerVolatilityTimerInterval *= 2;
    markLayersVolatileOrRetry(m_layerVolatilityTimerInterval > maximumLayerVolatilityTimerInterval ? MarkLayersVolatileDontRetryReason::TimedOut : MarkLayersVolatileDontRetryReason::None);
}

void WebPage::markLayersVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    WEBPAGE_RELEASE_LOG_FORWARDABLE(Layers, WEBPAGE_MARK_LAYERS_VOLATILE);

    if (m_layerVolatilityTimer.isActive())
        m_layerVolatilityTimer.stop();

    if (completionHandler)
        m_markLayersAsVolatileCompletionHandlers.append(WTFMove(completionHandler));

    m_layerVolatilityTimerInterval = initialLayerVolatilityTimerInterval;
    markLayersVolatileOrRetry(m_isSuspendedUnderLock ? MarkLayersVolatileDontRetryReason::SuspendedUnderLock : MarkLayersVolatileDontRetryReason::None);
}

void WebPage::markLayersVolatileOrRetry(MarkLayersVolatileDontRetryReason dontRetryReason)
{
    tryMarkLayersVolatile([dontRetryReason, protectedThis = Ref { *this }](bool didSucceed) {
        protectedThis->tryMarkLayersVolatileCompletionHandler(dontRetryReason, didSucceed);
    });
}

void WebPage::tryMarkLayersVolatileCompletionHandler(MarkLayersVolatileDontRetryReason dontRetryReason, bool didSucceed)
{
    if (m_isClosed)
        return;

    if (didSucceed || dontRetryReason != MarkLayersVolatileDontRetryReason::None) {
        m_layerVolatilityTimer.stop();
        if (didSucceed)
            WEBPAGE_RELEASE_LOG(Layers, "markLayersVolatile: Succeeded in marking layers as volatile");
        else {
            switch (dontRetryReason) {
            case MarkLayersVolatileDontRetryReason::None:
                break;
            case MarkLayersVolatileDontRetryReason::SuspendedUnderLock:
                WEBPAGE_RELEASE_LOG(Layers, "markLayersVolatile: Did what we could to mark IOSurfaces as purgeable after locking the screen");
                break;
            case MarkLayersVolatileDontRetryReason::TimedOut:
                WEBPAGE_RELEASE_LOG(Layers, "markLayersVolatile: Failed to mark layers as volatile within %gms", maximumLayerVolatilityTimerInterval.milliseconds());
                break;
            }
        }
        callVolatilityCompletionHandlers(didSucceed);
        return;
    }

    if (m_markLayersAsVolatileCompletionHandlers.isEmpty()) {
        WEBPAGE_RELEASE_LOG(Layers, "markLayersVolatile: Failed to mark all layers as volatile, but will not retry because the operation was cancelled");
        return;
    }

    WEBPAGE_RELEASE_LOG_FORWARDABLE(Layers, WEBPAGE_FAILED_TO_MARK_ALL_LAYERS_VOLATILE, m_layerVolatilityTimerInterval.milliseconds());
    m_layerVolatilityTimer.startOneShot(m_layerVolatilityTimerInterval);
}

void WebPage::cancelMarkLayersVolatile()
{
    WEBPAGE_RELEASE_LOG(Layers, "cancelMarkLayersVolatile:");
    m_layerVolatilityTimer.stop();
    callVolatilityCompletionHandlers(false);
}

class CurrentEvent {
public:
    explicit CurrentEvent(const WebEvent& event)
        : m_previousCurrentEvent(g_currentEvent)
    {
        g_currentEvent = &event;
    }

    ~CurrentEvent()
    {
        g_currentEvent = m_previousCurrentEvent.get();
    }

private:
    CheckedPtr<const WebEvent> m_previousCurrentEvent;
};

#if ENABLE(CONTEXT_MENUS)

void WebPage::didDismissContextMenu()
{
    corePage()->contextMenuController().didDismissContextMenu();
}

void WebPage::showContextMenuFromFrame(const FrameInfoData& frameInfo, const ContextMenuContextData& contextMenuContextData, const UserData& userData)
{
    flushPendingEditorStateUpdate();
    send(Messages::WebPageProxy::ShowContextMenuFromFrame(frameInfo, contextMenuContextData, userData));
    m_hasEverDisplayedContextMenu = true;
    scheduleFullEditorStateUpdate();
}

#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(CONTEXT_MENU_EVENT)
void WebPage::contextMenuForKeyEvent()
{
#if ENABLE(CONTEXT_MENUS)
    corePage()->contextMenuController().clearContextMenu();
#endif

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    bool handled = frame->eventHandler().sendContextMenuEventForKey();
#if ENABLE(CONTEXT_MENUS)
    if (handled)
        protectedContextMenu()->show();
#else
    UNUSED_PARAM(handled);
#endif
}
#endif

void WebPage::mouseEvent(FrameIdentifier frameID, const WebMouseEvent& mouseEvent, std::optional<Vector<SandboxExtension::Handle>>&& sandboxExtensions)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    m_internals->userActivity.impulse();

    bool shouldHandleEvent = true;
#if ENABLE(DRAG_SUPPORT)
    if (m_isStartingDrag)
        shouldHandleEvent = false;
#endif

    if (!shouldHandleEvent) {
        send(Messages::WebPageProxy::DidReceiveEventIPC(mouseEvent.type(), false, std::nullopt));
        return;
    }

    Vector<Ref<SandboxExtension>> mouseEventSandboxExtensions;
    if (sandboxExtensions)
        mouseEventSandboxExtensions = consumeSandboxExtensions(WTFMove(*sandboxExtensions));

    bool handled = false;

#if !PLATFORM(IOS_FAMILY)
    if (!handled && m_headerBanner)
        handled = Ref { *m_headerBanner }->mouseEvent(mouseEvent);
    if (!handled && m_footerBanner)
        handled = Ref { *m_footerBanner }->mouseEvent(mouseEvent);
#endif // !PLATFORM(IOS_FAMILY)

    if (RefPtr frame = WebProcess::singleton().webFrame(frameID); !handled && frame) {
        CurrentEvent currentEvent(mouseEvent);
        auto mouseEventResult = frame->handleMouseEvent(mouseEvent);
        if (auto remoteMouseEventData = mouseEventResult.remoteUserInputEventData()) {
            revokeSandboxExtensions(mouseEventSandboxExtensions);
            send(Messages::WebPageProxy::DidReceiveEventIPC(mouseEvent.type(), false, *remoteMouseEventData));
            return;
        }
        handled = mouseEventResult.wasHandled();
    }

    revokeSandboxExtensions(mouseEventSandboxExtensions);

    RefPtr drawingArea = m_drawingArea;
    bool shouldDeferDidReceiveEvent = [&] {
        if (!drawingArea)
            return false;

        if (mouseEvent.type() != WebEventType::MouseMove)
            return false;

        if (mouseEvent.button() != WebMouseEventButton::None)
            return false;

        if (mouseEvent.force())
            return false;

        return true;
    }();

    flushDeferredDidReceiveMouseEvent();

    if (shouldDeferDidReceiveEvent && drawingArea->scheduleRenderingUpdate()) {
        // For mousemove events where the user is only hovering (not clicking and dragging),
        // we defer sending the DidReceiveEvent() IPC message until the end of the rendering
        // update to throttle the rate of these events to the rendering update frequency.
        // This logic works in tandem with the mouse event queue in the UI process, which
        // coalesces mousemove events until the DidReceiveEvent() message is received after
        // the rendering update.
        m_deferredDidReceiveMouseEvent = { { mouseEvent.type(), handled } };
        return;
    }

    send(Messages::WebPageProxy::DidReceiveEventIPC(mouseEvent.type(), handled, std::nullopt));

#if PLATFORM(IOS_FAMILY)
    if (mouseEvent.type() == WebEventType::MouseUp)
        removeTextInteractionSources(TextInteractionSource::Mouse);
#endif
}

void WebPage::setLastKnownMousePosition(WebCore::FrameIdentifier frameID, IntPoint eventPoint, IntPoint globalPoint)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame || !frame->coreLocalFrame() || !frame->coreLocalFrame()->view())
        return;

    frame->coreLocalFrame()->eventHandler().setLastKnownMousePosition(eventPoint, globalPoint);
}

void WebPage::startDeferringResizeEvents()
{
    protectedCorePage()->startDeferringResizeEvents();
}

void WebPage::flushDeferredResizeEvents()
{
    protectedCorePage()->flushDeferredResizeEvents();
}

void WebPage::startDeferringScrollEvents()
{
    protectedCorePage()->startDeferringScrollEvents();
}

void WebPage::flushDeferredScrollEvents()
{
    protectedCorePage()->flushDeferredScrollEvents();
}

void WebPage::startDeferringIntersectionObservations()
{
    protectedCorePage()->startDeferringIntersectionObservations();
}

void WebPage::flushDeferredIntersectionObservations()
{
    protectedCorePage()->flushDeferredIntersectionObservations();
}

void WebPage::flushDeferredDidReceiveMouseEvent()
{
    if (auto info = std::exchange(m_deferredDidReceiveMouseEvent, std::nullopt))
        send(Messages::WebPageProxy::DidReceiveEventIPC(*info->type, info->handled, std::nullopt));
}

void WebPage::performHitTestForMouseEvent(const WebMouseEvent& event, CompletionHandler<void(WebHitTestResultData&&, OptionSet<WebEventModifier>)>&& completionHandler)
{
    auto modifiers = event.modifiers();
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame || !localMainFrame->view())
        return completionHandler({ }, modifiers);

    auto hitTestResult = localMainFrame->eventHandler().getHitTestResultForMouseEvent(platform(event));

    String toolTip;
    TextDirection toolTipDirection;
    corePage()->chrome().getToolTip(hitTestResult, toolTip, toolTipDirection);

    WebHitTestResultData hitTestResultData { hitTestResult, toolTip };

    completionHandler(WTFMove(hitTestResultData), modifiers);
}

void WebPage::handleWheelEvent(FrameIdentifier frameID, const WebWheelEvent& event, const OptionSet<WheelEventProcessingSteps>& processingSteps, std::optional<bool> willStartSwipe, CompletionHandler<void(std::optional<WebCore::ScrollingNodeID>, std::optional<WebCore::WheelScrollGestureState>, bool, std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
#if ENABLE(ASYNC_SCROLLING)
    RefPtr remoteScrollingCoordinator = dynamicDowncast<RemoteScrollingCoordinator>(scrollingCoordinator());
    if (remoteScrollingCoordinator)
        remoteScrollingCoordinator->setCurrentWheelEventWillStartSwipe(willStartSwipe);
#else
    UNUSED_PARAM(willStartSwipe);
#endif

    auto [handleWheelEventResult, _] = wheelEvent(frameID, event, processingSteps);
#if ENABLE(ASYNC_SCROLLING)
    if (remoteScrollingCoordinator) {
        auto gestureInfo = remoteScrollingCoordinator->takeCurrentWheelGestureInfo();
        completionHandler(gestureInfo.wheelGestureNode, gestureInfo.wheelGestureState, handleWheelEventResult.wasHandled(), handleWheelEventResult.remoteUserInputEventData());
        return;
    }
#endif
    completionHandler({ }, { }, handleWheelEventResult.wasHandled(), handleWheelEventResult.remoteUserInputEventData());
}

std::pair<HandleUserInputEventResult, OptionSet<EventHandling>> WebPage::wheelEvent(const FrameIdentifier& frameID, const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    m_internals->userActivity.impulse();

    CurrentEvent currentEvent(wheelEvent);

    auto dispatchWheelEvent = [&](const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps) {
        RefPtr frame = WebProcess::singleton().webFrame(frameID);
        if (!frame || !frame->coreLocalFrame() || !frame->coreLocalFrame()->view())
            return std::pair { HandleUserInputEventResult { false }, OptionSet<EventHandling> { } };

        auto platformWheelEvent = platform(wheelEvent);
        return frame->coreLocalFrame()->eventHandler().handleWheelEvent(platformWheelEvent, processingSteps);
    };

    auto [result, handling] = dispatchWheelEvent(wheelEvent, processingSteps);
    LOG_WITH_STREAM(WheelEvents, stream << "WebPage::wheelEvent - processing steps " << processingSteps << " handled " << result.wasHandled());
    return { result, handling };
}

#if PLATFORM(IOS_FAMILY)
void WebPage::dispatchWheelEventWithoutScrolling(FrameIdentifier frameID, const WebWheelEvent& wheelEvent, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(KINETIC_SCROLLING)
    RefPtr localMainFrame = this->localMainFrame();
    auto gestureState =  localMainFrame ? localMainFrame->eventHandler().wheelScrollGestureState() : std::nullopt;
    bool isCancelable = !gestureState || gestureState == WheelScrollGestureState::Blocking || wheelEvent.phase() == WebWheelEvent::Phase::Began;
#else
    bool isCancelable = true;
#endif
    auto [result, handling] = this->wheelEvent(frameID, wheelEvent, { isCancelable ? WheelEventProcessingSteps::BlockingDOMEventDispatch : WheelEventProcessingSteps::NonBlockingDOMEventDispatch });
    // The caller of dispatchWheelEventWithoutScrolling never cares about DidReceiveEvent being sent back.
    completionHandler(result.wasHandled() && handling.contains(EventHandling::DefaultPrevented));
}
#endif

void WebPage::keyEvent(FrameIdentifier frameID, const WebKeyboardEvent& keyboardEvent)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    m_internals->userActivity.impulse();

    PlatformKeyboardEvent::setCurrentModifierState(platform(keyboardEvent).modifiers());

    CurrentEvent currentEvent(keyboardEvent);

    bool handled = false;
    if (RefPtr frame = WebProcess::singleton().webFrame(frameID))
        handled = frame->handleKeyEvent(keyboardEvent);

    send(Messages::WebPageProxy::DidReceiveEventIPC(keyboardEvent.type(), handled, std::nullopt));
}

bool WebPage::handleKeyEventByRelinquishingFocusToChrome(const KeyboardEvent& event)
{
    if (m_page->tabKeyCyclesThroughElements())
        return false;

    if (event.charCode() != '\t')
        return false;

    if (!event.shiftKey() || event.ctrlKey() || event.metaKey())
        return false;

    ASSERT(event.type() == eventNames().keypressEvent);
    // Allow a shift-tab keypress event to relinquish focus even if we don't allow tab to cycle between
    // elements inside the view. We can only do this for shift-tab, not tab itself because
    // tabKeyCyclesThroughElements is used to make tab character insertion work in editable web views.
    return protectedCorePage()->focusController().relinquishFocusToChrome(FocusDirection::Backward);
}

void WebPage::validateCommand(const String& commandName, CompletionHandler<void(bool, int32_t)>&& completionHandler)
{
    bool isEnabled = false;
    int32_t state = 0;
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ }, { });

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = focusedPluginViewForFrame(*frame))
        isEnabled = pluginView->isEditingCommandEnabled(commandName);
    else
#endif
    {
        auto command = frame->protectedEditor()->command(commandName);
        state = (command.state() != TriState::False);
        isEnabled = command.isSupported() && command.isEnabled();
    }

    completionHandler(isEnabled, state);
}

void WebPage::executeEditCommand(const String& commandName, const String& argument)
{
    executeEditingCommand(commandName, argument);
}

void WebPage::setNeedsFontAttributes(bool needsFontAttributes)
{
    if (m_needsFontAttributes == needsFontAttributes)
        return;

    m_needsFontAttributes = needsFontAttributes;

    if (m_needsFontAttributes)
        scheduleFullEditorStateUpdate();
}

void WebPage::setCurrentHistoryItemForReattach(Ref<FrameState>&& mainFrameState)
{
    if (RefPtr localMainFrame = m_mainFrame->provisionalFrame() ? m_mainFrame->provisionalFrame() : m_mainFrame->coreLocalFrame())
        localMainFrame->loader().history().setCurrentItem(toHistoryItem(m_historyItemClient, mainFrameState));
}

void WebPage::requestFontAttributesAtSelectionStart(CompletionHandler<void(const WebCore::FontAttributes&)>&& completionHandler)
{
    RefPtr focusedOrMainFrame = corePage()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return completionHandler({ });
    completionHandler(focusedOrMainFrame->protectedEditor()->fontAttributesAtSelectionStart());
}

void WebPage::cancelCurrentInteractionInformationRequest()
{
#if PLATFORM(IOS_FAMILY)
    if (auto reply = WTFMove(m_pendingSynchronousPositionInformationReply))
        reply(InteractionInformationAtPosition::invalidInformation());
#endif
}

#if ENABLE(TOUCH_EVENTS)
static Expected<bool, WebCore::RemoteFrameGeometryTransformer> handleTouchEvent(FrameIdentifier frameID, const WebTouchEvent& touchEvent, Page* page)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return false;

    RefPtr localFrame = frame->coreLocalFrame();
    if (!localFrame || !localFrame->view())
        return false;

    return localFrame->eventHandler().handleTouchEvent(platform(touchEvent));
}
#endif

RefPtr<WebCore::LocalFrame> WebPage::localRootFrame(std::optional<WebCore::FrameIdentifier> frameID)
{
    if (RefPtr webFrame = WebProcess::singleton().webFrame(frameID)) {
        ASSERT(webFrame->coreLocalFrame());
        ASSERT(webFrame->coreLocalFrame()->isRootFrame());
        return webFrame->coreLocalFrame();
    }
    ASSERT(m_page);
    ASSERT(m_page->localMainFrame());
    RefPtr page = m_page;
    return page ? page->localMainFrame() : nullptr;
}

#if ENABLE(IOS_TOUCH_EVENTS)
Expected<bool, WebCore::RemoteFrameGeometryTransformer> WebPage::dispatchTouchEvent(FrameIdentifier frameID, const WebTouchEvent& touchEvent)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };
    m_lastInteractionLocation = touchEvent.position();
    CurrentEvent currentEvent(touchEvent);
    auto handleTouchEventResult = handleTouchEvent(frameID, touchEvent, m_page.get());
    updatePotentialTapSecurityOrigin(touchEvent, handleTouchEventResult.value_or(false));
    return handleTouchEventResult;
}

void WebPage::didBeginTouchPoint(FloatPoint locationInRootView)
{
    m_hasAnyActiveTouchPoints = true;
    m_potentialTapSecurityOrigin = nullptr;
    m_lastTouchLocationBeforeTap = locationInRootView;
}

void WebPage::updatePotentialTapSecurityOrigin(const WebTouchEvent& touchEvent, bool wasHandled)
{
    if (wasHandled)
        return;

    if (!touchEvent.isPotentialTap())
        return;

    if (touchEvent.type() != WebEventType::TouchStart)
        return;

    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame)
        return;

    RefPtr document = localMainFrame->document();
    if (!document)
        return;

    if (!document->handlingTouchEvent())
        return;

    RefPtr touchEventTargetFrame = localMainFrame;
    while (RefPtr localSubframe = dynamicDowncast<LocalFrame>(touchEventTargetFrame->eventHandler().touchEventTargetSubframe()))
        touchEventTargetFrame = WTFMove(localSubframe);

    auto& touches = touchEventTargetFrame->eventHandler().touches();
    if (touches.isEmpty())
        return;

    ASSERT(touches.size() == 1);

    if (auto targetDocument = touchEventTargetFrame->document())
        m_potentialTapSecurityOrigin = targetDocument->securityOrigin();
}
#elif ENABLE(TOUCH_EVENTS)
void WebPage::touchEvent(const WebTouchEvent& touchEvent, CompletionHandler<void(std::optional<WebEventType>, bool)>&& completionHandler)
{
    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame)
        return;

    CurrentEvent currentEvent(touchEvent);

    bool handled = handleTouchEvent(localMainFrame->frameID(), touchEvent, m_page.get()).value_or(false);

    completionHandler(touchEvent.type(), handled);
}
#endif

void WebPage::cancelPointer(WebCore::PointerID pointerId, const WebCore::IntPoint& documentPoint)
{
    m_page->pointerCaptureController().cancelPointer(pointerId, documentPoint);
}

void WebPage::touchWithIdentifierWasRemoved(WebCore::PointerID pointerId)
{
    m_page->pointerCaptureController().touchWithIdentifierWasRemoved(pointerId);
}

#if ENABLE(MAC_GESTURE_EVENTS)
static HandleUserInputEventResult handleGestureEvent(FrameIdentifier frameID, const WebGestureEvent& event, Page* page)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return false;

    RefPtr coreLocalFrame = frame->coreLocalFrame();
    if (!coreLocalFrame)
        return false;
    return coreLocalFrame->eventHandler().handleGestureEvent(platform(event));
}

void WebPage::gestureEvent(FrameIdentifier frameID, const WebGestureEvent& gestureEvent, CompletionHandler<void(std::optional<WebEventType>, bool, std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
    CurrentEvent currentEvent(gestureEvent);
    auto result = handleGestureEvent(frameID, gestureEvent, m_page.get());
    completionHandler(gestureEvent.type(), result.wasHandled(), result.remoteUserInputEventData());
}
#endif

bool WebPage::scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    RefPtr focusedOrMainFrame = page->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return false;
    return focusedOrMainFrame->eventHandler().scrollRecursively(direction, granularity);
}

bool WebPage::logicalScroll(Page* page, ScrollLogicalDirection direction, ScrollGranularity granularity)
{
    RefPtr focusedOrMainFrame = page->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return false;
    return focusedOrMainFrame->eventHandler().logicalScrollRecursively(direction, granularity);
}

bool WebPage::scrollBy(WebCore::ScrollDirection scrollDirection, WebCore::ScrollGranularity scrollGranularity)
{
    return scroll(m_page.get(), static_cast<ScrollDirection>(scrollDirection), static_cast<ScrollGranularity>(scrollGranularity));
}

void WebPage::centerSelectionInVisibleArea()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;
    frame->checkedSelection()->revealSelection({ SelectionRevealMode::Reveal, ScrollAlignment::alignCenterAlways });
    findController().showFindIndicatorInSelection();
}

bool WebPage::isControlledByAutomation() const
{
    return m_page->isControlledByAutomation();
}

void WebPage::setControlledByAutomation(bool controlled)
{
    m_page->setControlledByAutomation(controlled);
}

WebPageInspectorTarget& WebPage::ensureInspectorTarget()
{
    if (!m_inspectorTarget)
        m_inspectorTarget = makeUnique<WebPageInspectorTarget>(*this);
    return *m_inspectorTarget;
}

void WebPage::connectInspector(Inspector::FrontendChannel::ConnectionType connectionType)
{
    ensureInspectorTarget().connect(connectionType);
}

void WebPage::disconnectInspector()
{
    ensureInspectorTarget().disconnect();
}

void WebPage::sendMessageToTargetBackend(const String& message)
{
    ensureInspectorTarget().sendMessageToTargetBackend(message);
}

void WebPage::insertNewlineInQuotedContent()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;
    if (frame->selection().isNone())
        return;
    frame->protectedEditor()->insertParagraphSeparatorInQuotedContent();
}

#if ENABLE(REMOTE_INSPECTOR)
void WebPage::setIndicating(bool indicating)
{
    m_page->inspectorController().setIndicating(indicating);
}
#endif

void WebPage::setBackgroundColor(const std::optional<WebCore::Color>& backgroundColor)
{
    if (m_backgroundColor == backgroundColor)
        return;

    m_backgroundColor = backgroundColor;

    if (RefPtr frameView = localMainFrameView())
        frameView->updateBackgroundRecursively(backgroundColor);

    RefPtr drawingArea = m_drawingArea;
#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    drawingArea->backgroundColorDidChange();
#endif
    drawingArea->setNeedsDisplay();
}

void WebPage::setObscuredContentInsets(const FloatBoxExtent& obscuredContentInsets)
{
    RefPtr page = m_page;
    if (obscuredContentInsets == page->obscuredContentInsets())
        return;

    page->setObscuredContentInsets(obscuredContentInsets);

#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews)
        pluginView->obscuredContentInsetsDidChange();
#endif
}

void WebPage::viewWillStartLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (RefPtr view = frame->view())
        view->willStartLiveResize();
}

void WebPage::viewWillEndLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (RefPtr view = frame->view())
        view->willEndLiveResize();
}

void WebPage::setInitialFocus(bool forward, bool isKeyboardEventValid, const std::optional<WebKeyboardEvent>& event, CompletionHandler<void()>&& completionHandler)
{
    if (!m_page)
        return completionHandler();

    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    CheckedRef focusController { m_page->focusController() };
    RefPtr frame = focusController->focusedOrMainFrame();
    if (!frame)
        return completionHandler();
    frame->protectedDocument()->setFocusedElement(nullptr);

    if (isKeyboardEventValid && event && event->type() == WebEventType::KeyDown) {
        PlatformKeyboardEvent platformEvent(platform(CheckedRef { *event }));
        platformEvent.disambiguateKeyDownEvent(PlatformEvent::Type::RawKeyDown);
        focusController->setInitialFocus(forward ? FocusDirection::Forward : FocusDirection::Backward, &KeyboardEvent::create(platformEvent, &frame->windowProxy()).get());
        completionHandler();
        return;
    }

    focusController->setInitialFocus(forward ? FocusDirection::Forward : FocusDirection::Backward, nullptr);
    completionHandler();
}

void WebPage::setCanStartMediaTimerFired()
{
    if (RefPtr page = m_page)
        page->setCanStartMedia(true);
}

void WebPage::updateIsInWindow(bool isInitialState)
{
    bool isInWindow = m_activityState.contains(WebCore::ActivityState::IsInWindow);

    if (!isInWindow) {
        m_setCanStartMediaTimer.stop();
        protectedCorePage()->setCanStartMedia(false);

        // The WebProcess does not yet know about this page; no need to tell it we're leaving the window.
        if (!isInitialState)
            WebProcess::singleton().pageWillLeaveWindow(m_identifier);
    } else {
        // Defer the call to Page::setCanStartMedia() since it ends up sending a synchronous message to the UI process
        // in order to get plug-in connections, and the UI process will be waiting for the Web process to update the backing
        // store after moving the view into a window, until it times out and paints white. See <rdar://problem/9242771>.
        if (m_mayStartMediaWhenInWindow)
            m_setCanStartMediaTimer.startOneShot(0_s);

        WebProcess::singleton().pageDidEnterWindow(m_identifier);
    }

    if (isInWindow)
        layoutIfNeeded();

#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews)
        pluginView->didChangeIsInWindow();
#endif
}

void WebPage::visibilityDidChange()
{
    bool isVisible = m_activityState.contains(ActivityState::IsVisible);
    if (!isVisible) {
        // We save the document / scroll state when backgrounding a tab so that we are able to restore it
        // if it gets terminated while in the background.
        if (RefPtr frame = m_mainFrame->coreLocalFrame())
            frame->loader().history().saveDocumentAndScrollState();
    }
}

void WebPage::windowActivityDidChange()
{
#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews)
        pluginView->windowActivityDidChange();
#endif
}

void WebPage::setActivityState(OptionSet<ActivityState> activityState, ActivityStateChangeID activityStateChangeID, CompletionHandler<void()>&& callback)
{
    LOG_WITH_STREAM(ActivityState, stream << "WebPage " << identifier().toUInt64() << " setActivityState to " << activityState);

    auto changed = m_activityState ^ activityState;
    m_activityState = activityState;

    if (changed)
        updateThrottleState();

    ASSERT_WITH_MESSAGE(m_page, "setActivityState called on %" PRIu64 " but WebCore page was null", identifier().toUInt64());
    if (RefPtr page = m_page) {
        SetForScope currentlyChangingActivityState { m_lastActivityStateChanges, changed };
        page->setActivityState(activityState);
    }

    protectedDrawingArea()->activityStateDidChange(changed, activityStateChangeID, WTFMove(callback));
    WebProcess::singleton().pageActivityStateDidChange(m_identifier, changed);

    if (changed & ActivityState::IsInWindow)
        updateIsInWindow();

    if (changed & ActivityState::IsVisible)
        visibilityDidChange();

    if (changed & ActivityState::WindowIsActive)
        windowActivityDidChange();
}

void WebPage::didStartPageTransition()
{
    freezeLayerTree(LayerTreeFreezeReason::PageTransition);

#if HAVE(TOUCH_BAR)
    bool hasPreviouslyFocusedDueToUserInteraction = m_userInteractionsSincePageTransition.contains(UserInteractionFlag::FocusedElement);
    m_userInteractionsSincePageTransition = { };
#endif
    m_lastEditorStateWasContentEditable = EditorStateIsContentEditable::Unset;

#if PLATFORM(MAC)
    if (hasPreviouslyFocusedDueToUserInteraction)
        send(Messages::WebPageProxy::SetHasFocusedElementWithUserInteraction(false));
#endif

#if HAVE(TOUCH_BAR)
    if (m_isTouchBarUpdateSuppressedForHiddenContentEditable) {
        m_isTouchBarUpdateSuppressedForHiddenContentEditable = false;
        send(Messages::WebPageProxy::SetIsTouchBarUpdateSuppressedForHiddenContentEditable(m_isTouchBarUpdateSuppressedForHiddenContentEditable));
    }

    if (m_isNeverRichlyEditableForTouchBar) {
        m_isNeverRichlyEditableForTouchBar = false;
        send(Messages::WebPageProxy::SetIsNeverRichlyEditableForTouchBar(m_isNeverRichlyEditableForTouchBar));
    }
#endif

#if PLATFORM(IOS_FAMILY)
    m_isShowingInputViewForFocusedElement = false;
    // This is used to enable a first-tap quirk.
    m_hasHandledSyntheticClick = false;
#endif
}

void WebPage::didCompletePageTransition()
{
    unfreezeLayerTree(LayerTreeFreezeReason::PageTransition);
}

void WebPage::setMainFrameDocumentVisualUpdatesAllowed(bool allowed)
{
    if (allowed)
        unfreezeLayerTree(LayerTreeFreezeReason::DocumentVisualUpdatesNotAllowed);
    else
        freezeLayerTree(LayerTreeFreezeReason::DocumentVisualUpdatesNotAllowed);
}

void WebPage::show()
{
    send(Messages::WebPageProxy::ShowPage());
}

void WebPage::setIsTakingSnapshotsForApplicationSuspension(bool isTakingSnapshotsForApplicationSuspension)
{
    WEBPAGE_RELEASE_LOG(Resize, "setIsTakingSnapshotsForApplicationSuspension(%d)", isTakingSnapshotsForApplicationSuspension);

    if (m_page)
        m_page->setIsTakingSnapshotsForApplicationSuspension(isTakingSnapshotsForApplicationSuspension);
}

void WebPage::setNeedsDOMWindowResizeEvent()
{
    RefPtr page = m_page;
    if (!page)
        return;

    if (RefPtr localTopDocument = page->localTopDocument())
        localTopDocument->setNeedsDOMWindowResizeEvent();
}

String WebPage::userAgent(const URL& webCoreURL) const
{
    String userAgent = platformUserAgent(webCoreURL);
    if (!userAgent.isEmpty())
        return userAgent;
    return m_userAgent;
}

void WebPage::setUserAgent(String&& userAgent)
{
    if (m_userAgent == userAgent)
        return;

    m_userAgent = WTFMove(userAgent);

    if (RefPtr page = m_page)
        page->userAgentChanged();
}

void WebPage::setHasCustomUserAgent(bool hasCustomUserAgent)
{
    m_hasCustomUserAgent = hasCustomUserAgent;
}

void WebPage::suspendActiveDOMObjectsAndAnimations()
{
    protectedCorePage()->suspendActiveDOMObjectsAndAnimations();
}

void WebPage::resumeActiveDOMObjectsAndAnimations()
{
    protectedCorePage()->resumeActiveDOMObjectsAndAnimations();
}

void WebPage::suspend(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr page = m_page;
    WEBPAGE_RELEASE_LOG(Loading, "suspend: m_page=%p", page.get());
    if (!page)
        return completionHandler(false);

    freezeLayerTree(LayerTreeFreezeReason::PageSuspended);

    m_cachedPage = BackForwardCache::singleton().suspendPage(*page);
    ASSERT(m_cachedPage);
    if (RefPtr mainFrame = m_mainFrame->coreLocalFrame())
        mainFrame->detachFromAllOpenedFrames();
    completionHandler(true);
}

void WebPage::resume(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr page = m_page;
    WEBPAGE_RELEASE_LOG(Loading, "resume: m_page=%p", page.get());
    if (!page)
        return completionHandler(false);

    auto cachedPage = std::exchange(m_cachedPage, nullptr);
    ASSERT(cachedPage);
    if (!cachedPage)
        return completionHandler(false);

    cachedPage->restore(*page);
    unfreezeLayerTree(LayerTreeFreezeReason::PageSuspended);
    completionHandler(true);
}

IntPoint WebPage::screenToRootView(const IntPoint& point)
{
    auto sendResult = sendSync(Messages::WebPageProxy::ScreenToRootView(point));
    auto [windowPoint] = sendResult.takeReplyOr(IntPoint { });
    return windowPoint;
}

IntPoint WebPage::rootViewToScreen(const IntPoint& point)
{
    auto sendResult = sendSync(Messages::WebPageProxy::RootViewPointToScreen(point));
    auto [screenPoint] = sendResult.takeReplyOr(IntPoint { });
    return screenPoint;
}

IntRect WebPage::rootViewToScreen(const IntRect& rect)
{
    auto sendResult = sendSync(Messages::WebPageProxy::RootViewRectToScreen(rect.toRectWithExtentsClippedToNumericLimits()));
    auto [screenRect] = sendResult.takeReplyOr(IntRect { });
    return screenRect;
}

IntPoint WebPage::accessibilityScreenToRootView(const IntPoint& point)
{
    auto sendResult = sendSync(Messages::WebPageProxy::AccessibilityScreenToRootView(point));
    auto [windowPoint] = sendResult.takeReplyOr(IntPoint { });
    return windowPoint;
}

IntRect WebPage::rootViewToAccessibilityScreen(const IntRect& rect)
{
    auto sendResult = sendSync(Messages::WebPageProxy::RootViewToAccessibilityScreen(rect));
    auto [screenRect] = sendResult.takeReplyOr(IntRect { });
    return screenRect;
}

KeyboardUIMode WebPage::keyboardUIMode()
{
    bool fullKeyboardAccessEnabled = WebProcess::singleton().fullKeyboardAccessEnabled();
    return static_cast<KeyboardUIMode>((fullKeyboardAccessEnabled ? KeyboardAccessFull : KeyboardAccessDefault) | (m_tabToLinks ? KeyboardAccessTabsToLinks : 0));
}

void WebPage::runJavaScript(WebFrame* frame, RunJavaScriptParameters&& parameters, ContentWorldIdentifier worldIdentifier, bool wantsResult, CompletionHandler<void(Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>)>&& completionHandler)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    if (!frame || !frame->coreLocalFrame()) {
        completionHandler(makeUnexpected(ExceptionDetails { "Unable to execute JavaScript: Target frame could not be found in the page"_s, 0, 0, ExceptionDetails::Type::InvalidTargetFrame }));
        return;
    }

    RefPtr world = m_userContentController->worldForIdentifier(worldIdentifier);
    if (!world) {
        completionHandler(makeUnexpected(ExceptionDetails { "Unable to execute JavaScript: Cannot find specified content world"_s }));
        return;
    }

#if ENABLE(APP_BOUND_DOMAINS)
    if (frame->shouldEnableInAppBrowserPrivacyProtections()) {
        completionHandler(makeUnexpected(ExceptionDetails { "Unable to execute JavaScript in a frame that is not in an app-bound domain"_s, 0, 0, ExceptionDetails::Type::AppBoundDomain }));
        if (RefPtr localTopDocument = protectedCorePage()->localTopDocument())
            localTopDocument->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user script injection for non-app bound domain."_s);
        WEBPAGE_RELEASE_LOG_ERROR(Loading, "runJavaScript: Ignoring user script injection for non app-bound domain");
        return;
    }
#endif

    bool shouldAllowUserInteraction = [&] {
        if (m_userIsInteracting)
            return true;

        if (parameters.forceUserGesture == ForceUserGesture::No)
            return false;

#if PLATFORM(COCOA)
        if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ProgrammaticFocusDuringUserScriptShowsInputViews))
            return true;
#endif

        return false;
    }();

    SetForScope userIsInteractingChange { m_userIsInteracting, shouldAllowUserInteraction };
    auto resolveFunction = [world = Ref { *world }, frame = Ref { *frame }, coreFrame = Ref { *frame->coreLocalFrame() }, wantsResult, completionHandler = WTFMove(completionHandler)] (ValueOrException result) mutable {
        if (!result)
            return completionHandler(makeUnexpected(result.error()));

        if (!wantsResult)
            return completionHandler(makeUnexpected(std::nullopt));

        JSGlobalContextRef context = frame->jsContextForWorld(world.ptr());
        JSValueRef jsValue = toRef(coreFrame->checkedScript()->globalObject(Ref { world->coreWorld() }), result.value());
        if (auto result = JavaScriptEvaluationResult::extract(context, jsValue))
            return completionHandler(WTFMove(*result));
        return completionHandler(makeUnexpected(std::nullopt));
    };

    auto mapArguments = [] (auto&& vector) -> std::optional<HashMap<String, Function<JSC::JSValue(JSC::JSGlobalObject&)>>> {
        if (!vector)
            return std::nullopt;
        HashMap<String, Function<JSC::JSValue(JSC::JSGlobalObject&)>> map;
        for (auto&& [key, result] : WTFMove(*vector)) {
            map.set(key, [result = WTFMove(result)] (JSC::JSGlobalObject& globalObject) mutable -> JSC::JSValue {
                return toJS(&globalObject, result.toJS(JSContextGetGlobalContext(toRef(&globalObject))).get());
            });
        }
        return { WTFMove(map) };
    };

    WebCore::RunJavaScriptParameters coreParameters {
        WTFMove(parameters.source),
        WTFMove(parameters.taintedness),
        WTFMove(parameters.sourceURL),
        parameters.runAsAsyncFunction == WebCore::RunAsAsyncFunction::Yes,
        mapArguments(WTFMove(parameters.arguments)),
        parameters.forceUserGesture == WebCore::ForceUserGesture::Yes,
        parameters.removeTransientActivation
    };

    JSLockHolder lock(commonVM());
    frame->protectedCoreLocalFrame()->checkedScript()->executeAsynchronousUserAgentScriptInWorld(world->protectedCoreWorld(), WTFMove(coreParameters), WTFMove(resolveFunction));
}

void WebPage::runJavaScriptInFrameInScriptWorld(RunJavaScriptParameters&& parameters, std::optional<WebCore::FrameIdentifier> frameID, const ContentWorldData& worldData, bool wantsResult, CompletionHandler<void(Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>)>&& completionHandler)
{
    WEBPAGE_RELEASE_LOG(Process, "runJavaScriptInFrameInScriptWorld: frameID=%" PRIu64, frameID ? frameID->toUInt64() : 0);
    RefPtr webFrame = frameID ? WebProcess::singleton().webFrame(*frameID) : &mainWebFrame();

    m_userContentController->addContentWorldIfNecessary(worldData);

    runJavaScript(webFrame.get(), WTFMove(parameters), worldData.identifier, wantsResult, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        if (!result && result.error())
            WEBPAGE_RELEASE_LOG_ERROR(Process, "runJavaScriptInFrameInScriptWorld: Request to run JavaScript failed with error %" PRIVATE_LOG_STRING, result.error()->message.utf8().data());
        else
            WEBPAGE_RELEASE_LOG(Process, "runJavaScriptInFrameInScriptWorld: Request to run JavaScript succeeded");
        completionHandler(WTFMove(result));
    });
}

void WebPage::getContentsAsString(ContentAsStringIncludesChildFrames includeChildFrames, CompletionHandler<void(const String&)>&& callback)
{
    switch (includeChildFrames) {
    case ContentAsStringIncludesChildFrames::No:
        callback(m_mainFrame->contentsAsString());
        break;
    case ContentAsStringIncludesChildFrames::Yes:
        StringBuilder builder;
        for (RefPtr<Frame> frame = m_mainFrame->coreLocalFrame(); frame; frame = frame->tree().traverseNextRendered()) {
            if (RefPtr webFrame = WebFrame::fromCoreFrame(*frame))
                builder.append(builder.isEmpty() ? ""_s : "\n\n"_s, webFrame->contentsAsString());
        }
        callback(builder.toString());
        break;
    }
}

#if ENABLE(MHTML)
void WebPage::getContentsAsMHTMLData(CompletionHandler<void(const IPC::SharedBufferReference&)>&& callback)
{
    callback(IPC::SharedBufferReference(MHTMLArchive::generateMHTMLData(m_page.get())));
}
#endif

void WebPage::getRenderTreeExternalRepresentation(CompletionHandler<void(const String&)>&& callback)
{
    callback(renderTreeExternalRepresentation());
}

static RefPtr<LocalFrame> frameWithSelection(Page* page)
{
    for (RefPtr frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(*frame);
        if (!localFrame)
            continue;
        if (localFrame->selection().isRange())
            return localFrame;
    }
    return nullptr;
}

void WebPage::copyLinkWithHighlight()
{
    RefPtr page = m_page;
    auto url = page->fragmentDirectiveURLForSelectedText();
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (url.isValid())
        frame->protectedEditor()->copyURL(url, { });
}

void WebPage::getSelectionOrContentsAsString(CompletionHandler<void(const String&)>&& callback)
{
    RefPtr focusedOrMainCoreFrame = corePage()->focusController().focusedOrMainFrame();
    RefPtr focusedOrMainFrame = focusedOrMainCoreFrame ? WebFrame::fromCoreFrame(*focusedOrMainCoreFrame) : nullptr;

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = pluginViewForFrame(focusedOrMainCoreFrame.get())) {
        auto result = pluginView->selectionString();
        if (result.isEmpty())
            result = pluginView->fullDocumentString();
        return callback(WTFMove(result));
    }
#endif

    String resultString = focusedOrMainFrame->selectionAsString();
    if (resultString.isEmpty())
        resultString = focusedOrMainFrame->contentsAsString();
    callback(resultString);
}

void WebPage::getSourceForFrame(FrameIdentifier frameID, CompletionHandler<void(const String&)>&& callback)
{
    String resultString;
    if (RefPtr frame = WebProcess::singleton().webFrame(frameID))
       resultString = frame->source();

    callback(resultString);
}

void WebPage::getMainResourceDataOfFrame(FrameIdentifier frameID, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& callback)
{
    RefPtr<FragmentedSharedBuffer> buffer;
    if (RefPtr frame = WebProcess::singleton().webFrame(frameID)) {
        RefPtr coreFrame = frame->coreLocalFrame();
#if ENABLE(PDF_PLUGIN)
        if (RefPtr pluginView = pluginViewForFrame(coreFrame.get()))
            buffer = pluginView->liveResourceData();
        if (!buffer)
#endif
        {
            if (RefPtr loader = coreFrame->loader().documentLoader())
                buffer = loader->mainResourceData();
        }
    }

    callback(IPC::SharedBufferReference(WTFMove(buffer)));
}

static RefPtr<FragmentedSharedBuffer> resourceDataForFrame(LocalFrame* frame, const URL& resourceURL)
{
    RefPtr loader = frame->loader().documentLoader();
    if (!loader)
        return nullptr;

    RefPtr<ArchiveResource> subresource = loader->subresource(resourceURL);
    if (!subresource)
        return nullptr;

    return &subresource->data();
}

void WebPage::getResourceDataFromFrame(FrameIdentifier frameID, const String& resourceURLString, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& callback)
{
    RefPtr<FragmentedSharedBuffer> buffer;
    if (RefPtr frame = WebProcess::singleton().webFrame(frameID)) {
        URL resourceURL { resourceURLString };
        buffer = resourceDataForFrame(frame->protectedCoreLocalFrame().get(), resourceURL);
    }

    callback(IPC::SharedBufferReference(WTFMove(buffer)));
}

void WebPage::getWebArchiveOfFrameWithFileName(WebCore::FrameIdentifier frameID, const Vector<WebCore::MarkupExclusionRule>& exclusionRules, const String& fileName, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& completionHandler)
{
    std::optional<IPC::SharedBufferReference> result;
#if PLATFORM(COCOA)
    if (RefPtr frame = WebProcess::singleton().webFrame(frameID)) {
        if (RetainPtr<CFDataRef> data = frame->webArchiveData(nullptr, nullptr, exclusionRules, fileName))
            result = IPC::SharedBufferReference(SharedBuffer::create(data.get()));
    }
#else
    UNUSED_PARAM(frameID);
#endif
    completionHandler(result);
}

void WebPage::getAccessibilityTreeData(CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& callback)
{
    IPC::SharedBufferReference dataBuffer;
#if PLATFORM(COCOA)
    if (auto treeData = protectedCorePage()->accessibilityTreeData(IncludeDOMInfo::Yes)) {
        auto stream = adoptCF(CFWriteStreamCreateWithAllocatedBuffers(0, 0));
        CFWriteStreamOpen(stream.get());

        auto writeTreeToStream = [&stream](auto& tree) {
            auto utf8 = tree.utf8();
            auto utf8Span = utf8.span();
            CFWriteStreamWrite(stream.get(), byteCast<UInt8>(utf8Span.data()), utf8Span.size());
        };
        writeTreeToStream(treeData->liveTree);
        writeTreeToStream(treeData->isolatedTree);

        auto data = adoptCF(checked_cf_cast<CFDataRef>(CFWriteStreamCopyProperty(stream.get(), kCFStreamPropertyDataWritten)));
        CFWriteStreamClose(stream.get());

        dataBuffer = IPC::SharedBufferReference(SharedBuffer::create(data.get()));
    }
#endif
    callback(dataBuffer);
}

void WebPage::updateRenderingWithForcedRepaintWithoutCallback()
{
    protectedDrawingArea()->updateRenderingWithForcedRepaint();
}

void WebPage::updateRenderingWithForcedRepaint(CompletionHandler<void()>&& completionHandler)
{
    protectedDrawingArea()->updateRenderingWithForcedRepaintAsync(*this, WTFMove(completionHandler));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store, std::optional<uint64_t> sharedPreferencesVersion)
{
#if ENABLE(GPU_PROCESS)
    if (sharedPreferencesVersion) {
        ASSERT(*sharedPreferencesVersion);
        auto sendResult = WebProcess::singleton().protectedParentProcessConnection()->sendSync(Messages::WebProcessProxy::WaitForSharedPreferencesForWebProcessToSync { *sharedPreferencesVersion }, 0);
        auto [success] = sendResult.takeReplyOr(false);
        if (!success)
            return; // Sync IPC has timed out or WebProcessProxy is getting destroyed
    }
#else
    UNUSED_PARAM(sharedPreferencesVersion);
#endif
    WebPreferencesStore::removeTestRunnerOverrides();
    updatePreferences(store);
}

bool WebPage::isParentProcessAWebBrowser() const
{
#if HAVE(AUDIT_TOKEN)
    return isParentProcessAFullWebBrowser(WebProcess::singleton());
#endif
    return false;
}

void WebPage::adjustSettingsForLockdownMode(Settings& settings, const WebPreferencesStore* store)
{
    // Disable unstable Experimental settings, even if the user enabled them for local use.
    settings.disableUnstableFeaturesForModernWebKit();
    Settings::disableGlobalUnstableFeaturesForModernWebKit();
    settings.disableFeaturesForLockdownMode();
#if PLATFORM(COCOA)
    if (settings.downloadableBinaryFontTrustedTypes() != DownloadableBinaryFontTrustedTypes::None) {
        auto downloadableBinaryFontTrustedTypes = DownloadableBinaryFontTrustedTypes::Restricted;
#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
        if (settings.lockdownFontParserEnabled())
            downloadableBinaryFontTrustedTypes = DownloadableBinaryFontTrustedTypes::SafeFontParser;
#endif
        settings.setDownloadableBinaryFontTrustedTypes(downloadableBinaryFontTrustedTypes);
    }
#endif

    // FIXME: This seems like an odd place to put logic for setting global state in CoreGraphics.
#if HAVE(LOCKDOWN_MODE_PDF_ADDITIONS)
    CGEnterLockdownModeForPDF();
#endif

    if (store) {
        settings.setAllowedMediaContainerTypes(store->getStringValueForKey(WebPreferencesKey::mediaContainerTypesAllowedInLockdownModeKey()));
        settings.setAllowedMediaCodecTypes(store->getStringValueForKey(WebPreferencesKey::mediaCodecTypesAllowedInLockdownModeKey()));
        settings.setAllowedMediaVideoCodecIDs(store->getStringValueForKey(WebPreferencesKey::mediaVideoCodecIDsAllowedInLockdownModeKey()));
        settings.setAllowedMediaAudioCodecIDs(store->getStringValueForKey(WebPreferencesKey::mediaAudioCodecIDsAllowedInLockdownModeKey()));
        settings.setAllowedMediaCaptionFormatTypes(store->getStringValueForKey(WebPreferencesKey::mediaCaptionFormatTypesAllowedInLockdownModeKey()));
    }
}

void WebPage::updatePreferences(const WebPreferencesStore& store)
{
    updatePreferencesGenerated(store);

    Settings& settings = m_page->settings();

    updateSettingsGenerated(store, settings);

#if !PLATFORM(GTK) && !PLATFORM(WIN) && !PLATFORM(PLAYSTATION)
    if (!settings.acceleratedCompositingEnabled()) {
        WEBPAGE_RELEASE_LOG(Layers, "updatePreferences: acceleratedCompositingEnabled setting was false. WebKit cannot function in this mode; changing setting to true");
        settings.setAcceleratedCompositingEnabled(true);
    }
#endif

    bool requiresUserGestureForMedia = store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForMediaPlaybackKey());
    settings.setRequiresUserGestureForVideoPlayback(requiresUserGestureForMedia || store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForVideoPlaybackKey()));
    settings.setRequiresUserGestureForAudioPlayback(requiresUserGestureForMedia || store.getBoolValueForKey(WebPreferencesKey::requiresUserGestureForAudioPlaybackKey()));
    settings.setUserInterfaceDirectionPolicy(static_cast<WebCore::UserInterfaceDirectionPolicy>(store.getUInt32ValueForKey(WebPreferencesKey::userInterfaceDirectionPolicyKey())));
    settings.setSystemLayoutDirection(static_cast<TextDirection>(store.getUInt32ValueForKey(WebPreferencesKey::systemLayoutDirectionKey())));
    settings.setJavaScriptRuntimeFlags(static_cast<RuntimeFlags>(store.getUInt32ValueForKey(WebPreferencesKey::javaScriptRuntimeFlagsKey())));
    settings.setStorageBlockingPolicy(static_cast<StorageBlockingPolicy>(store.getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey())));
    settings.setEditableLinkBehavior(static_cast<WebCore::EditableLinkBehavior>(store.getUInt32ValueForKey(WebPreferencesKey::editableLinkBehaviorKey())));
#if ENABLE(DATA_DETECTION)
    settings.setDataDetectorTypes(static_cast<DataDetectorType>(store.getUInt32ValueForKey(WebPreferencesKey::dataDetectorTypesKey())));
#endif
    settings.setPitchCorrectionAlgorithm(static_cast<MediaPlayerEnums::PitchCorrectionAlgorithm>(store.getUInt32ValueForKey(WebPreferencesKey::pitchCorrectionAlgorithmKey())));

    DatabaseManager::singleton().setIsAvailable(store.getBoolValueForKey(WebPreferencesKey::databasesEnabledKey()));

    m_tabToLinks = store.getBoolValueForKey(WebPreferencesKey::tabsToLinksKey());

    bool isAppNapEnabled = store.getBoolValueForKey(WebPreferencesKey::pageVisibilityBasedProcessSuppressionEnabledKey());
    if (m_isAppNapEnabled != isAppNapEnabled) {
        m_isAppNapEnabled = isAppNapEnabled;
        updateThrottleState();
    }

#if PLATFORM(COCOA)
    m_pdfPluginEnabled = store.getBoolValueForKey(WebPreferencesKey::pdfPluginEnabledKey());

    m_selectionFlippingEnabled = store.getBoolValueForKey(WebPreferencesKey::selectionFlippingEnabledKey());
#endif
#if ENABLE(PAYMENT_REQUEST)
    settings.setPaymentRequestEnabled(store.getBoolValueForKey(WebPreferencesKey::applePayEnabledKey()));
#endif

#if PLATFORM(IOS_FAMILY)
    setForceAlwaysUserScalable(m_forceAlwaysUserScalable || store.getBoolValueForKey(WebPreferencesKey::forceAlwaysUserScalableKey()));
#endif

    if (store.getBoolValueForKey(WebPreferencesKey::serviceWorkerEntitlementDisabledForTestingKey()))
        disableServiceWorkerEntitlement();
#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldAllowServiceWorkersForAppBoundViews = m_limitsNavigationsToAppBoundDomains;
#else
    bool shouldAllowServiceWorkersForAppBoundViews = false;
#endif

    if (store.getBoolValueForKey(WebPreferencesKey::serviceWorkersEnabledKey())) {
        ASSERT(parentProcessHasServiceWorkerEntitlement() || shouldAllowServiceWorkersForAppBoundViews);
        if (!parentProcessHasServiceWorkerEntitlement() && !shouldAllowServiceWorkersForAppBoundViews)
            settings.setServiceWorkersEnabled(false);
    }

#if ENABLE(APP_BOUND_DOMAINS)
    m_needsInAppBrowserPrivacyQuirks = store.getBoolValueForKey(WebPreferencesKey::needsInAppBrowserPrivacyQuirksKey());
#endif

    settings.setPrivateClickMeasurementEnabled(store.getBoolValueForKey(WebPreferencesKey::privateClickMeasurementEnabledKey()));

    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->updatePreferences(store);

    WebProcess::singleton().setChildProcessDebuggabilityEnabled(store.getBoolValueForKey(WebPreferencesKey::childProcessDebuggabilityEnabledKey()));

#if ENABLE(GPU_PROCESS)
    downcast<WebMediaStrategy>(platformStrategies()->mediaStrategy()).setUseGPUProcess(m_shouldPlayMediaInGPUProcess);
#if ENABLE(VIDEO)
    WebProcess::singleton().protectedRemoteMediaPlayerManager()->setUseGPUProcess(m_shouldPlayMediaInGPUProcess);
#if PLATFORM(COCOA)
    platformStrategies()->mediaStrategy()->enableRemoteRenderer(MediaPlayerMediaEngineIdentifier::CocoaWebM, settings.webMUseRemoteAudioVideoRenderer());
    platformStrategies()->mediaStrategy()->enableRemoteRenderer(MediaPlayerMediaEngineIdentifier::AVFoundationMSE, settings.mediaSourceUseRemoteAudioVideoRenderer());
#endif
#endif
#if HAVE(AVASSETREADER)
    WebProcess::singleton().protectedRemoteImageDecoderAVFManager()->setUseGPUProcess(m_shouldPlayMediaInGPUProcess);
#endif
    WebProcess::singleton().setUseGPUProcessForCanvasRendering(m_shouldRenderCanvasInGPUProcess);
    bool usingGPUProcessForDOMRendering = m_shouldRenderDOMInGPUProcess
#if ENABLE(TILED_CA_DRAWING_AREA)
        && DrawingArea::supportsGPUProcessRendering(m_drawingAreaType);
#else
        && DrawingArea::supportsGPUProcessRendering();
#endif
    WebProcess::singleton().setUseGPUProcessForDOMRendering(usingGPUProcessForDOMRendering);
    WebProcess::singleton().setUseGPUProcessForMedia(m_shouldPlayMediaInGPUProcess);
#if ENABLE(WEBGL)
    WebProcess::singleton().setUseGPUProcessForWebGL(m_shouldRenderWebGLInGPUProcess);
#endif
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(IPC_TESTING_API)
    m_ipcTestingAPIEnabled = store.getBoolValueForKey(WebPreferencesKey::ipcTestingAPIEnabledKey());

    WebProcess::singleton().protectedParentProcessConnection()->setIgnoreInvalidMessageForTesting();
    if (auto* gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->connection().setIgnoreInvalidMessageForTesting();
#if ENABLE(MODEL_PROCESS)
    if (auto* modelProcessConnection = WebProcess::singleton().existingModelProcessConnection())
        modelProcessConnection->connection().setIgnoreInvalidMessageForTesting();
#endif // ENABLE(MODEL_PROCESS)
#endif // ENABLE(IPC_TESTING_API)

#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setSWVPDecodersAlwaysEnabled(store.getBoolValueForKey(WebPreferencesKey::sWVPDecodersAlwaysEnabledKey()));
#endif

    // FIXME: This should be automated by adding a new field in WebPreferences*.yaml
    // that indicates override state for Lockdown mode. https://webkit.org/b/233100.
    if (WebProcess::singleton().isLockdownModeEnabled())
        adjustSettingsForLockdownMode(settings, &store);
    if (settings.forceLockdownFontParserEnabled())
        settings.setDownloadableBinaryFontTrustedTypes(DownloadableBinaryFontTrustedTypes::SafeFontParser);

#if ENABLE(ARKIT_INLINE_PREVIEW)
    m_useARKitForModel = store.getBoolValueForKey(WebPreferencesKey::useARKitForModelKey());
#endif
#if HAVE(SCENEKIT)
    m_useSceneKitForModel = store.getBoolValueForKey(WebPreferencesKey::useSceneKitForModelKey());
#endif

    if (settings.developerExtrasEnabled()) {
        settings.setShowMediaStatsContextMenuItemEnabled(true);
        settings.setTrackConfigurationEnabled(true);
    }

#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews)
        pluginView->didChangeSettings();
#endif

    WebProcess::singleton().updateSharedPreferencesForWebProcess(WebKit::sharedPreferencesForWebProcess(store, WebProcess::singleton().isLockdownModeEnabled()));

    protectedCorePage()->settingsDidChange();
}

#if ENABLE(DATA_DETECTION)

void WebPage::setDataDetectionResults(NSArray *detectionResults)
{
    DataDetectionResult dataDetectionResult;
    dataDetectionResult.setResults(detectionResults);
    send(Messages::WebPageProxy::SetDataDetectionResult(dataDetectionResult));
}

void WebPage::removeDataDetectedLinks(CompletionHandler<void(DataDetectionResult&&)>&& completionHandler)
{
    for (RefPtr frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        RefPtr document = localFrame->document();
        if (!document)
            continue;

        DataDetection::removeDataDetectedLinksInDocument(*document);

        if (auto* results = localFrame->dataDetectionResultsIfExists()) {
            // FIXME: It seems odd that we're clearing out all data detection results here,
            // instead of only data detectors that correspond to links.
            results->setDocumentLevelResults(nullptr);
        }
    }
    completionHandler({ });
}

static void detectDataInFrame(const Ref<Frame>& frame, OptionSet<WebCore::DataDetectorType> dataDetectorTypes, const std::optional<double>& dataDetectionReferenceDate, UniqueRef<DataDetectionResult>&& mainFrameResult, CompletionHandler<void(DataDetectionResult&&)>&& completionHandler)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
    if (!localFrame) {
        completionHandler(WTFMove(mainFrameResult.get()));
        return;
    }

    DataDetection::detectContentInFrame(localFrame.get(), dataDetectorTypes, dataDetectionReferenceDate, [localFrame, mainFrameResult = WTFMove(mainFrameResult), dataDetectionReferenceDate, completionHandler = WTFMove(completionHandler), dataDetectorTypes](NSArray *results) mutable {
        localFrame->dataDetectionResults().setDocumentLevelResults(results);
        if (localFrame->isMainFrame())
            mainFrameResult->setResults(results);

        RefPtr next = localFrame->tree().traverseNext();
        if (!next) {
            completionHandler(WTFMove(mainFrameResult.get()));
            return;
        }

        detectDataInFrame(Ref { *next }, dataDetectorTypes, dataDetectionReferenceDate, WTFMove(mainFrameResult), WTFMove(completionHandler));
    });
}

void WebPage::detectDataInAllFrames(OptionSet<WebCore::DataDetectorType> dataDetectorTypes, CompletionHandler<void(DataDetectionResult&&)>&& completionHandler)
{
    auto mainFrameResult = makeUniqueRef<DataDetectionResult>();
    detectDataInFrame(protectedCorePage()->protectedMainFrame().get(), dataDetectorTypes, m_dataDetectionReferenceDate, WTFMove(mainFrameResult), WTFMove(completionHandler));
}

#endif // ENABLE(DATA_DETECTION)

void WebPage::layoutIfNeeded()
{
    protectedCorePage()->layoutIfNeeded();
}

void WebPage::updateRendering()
{
    protectedCorePage()->updateRendering();

#if PLATFORM(IOS_FAMILY)
    findController().redraw();
    foundTextRangeController().redraw();
#endif
}

bool WebPage::hasRootFrames()
{
    bool result = m_page && !m_page->rootFrames().isEmpty();
#if ASSERT_ENABLED
    if (!result)
        ASSERT(m_page->settings().siteIsolationEnabled());
#endif
    return result;
}

String WebPage::rootFrameOriginString()
{
    auto rootFrameURL = [&] () -> URL {
        if (!m_page)
            return { };
        if (m_page->rootFrames().isEmpty())
            return { };
        RefPtr documentLoader = m_page->rootFrames().begin()->get().loader().documentLoader();
        if (!documentLoader)
            return { };
        return documentLoader->url();
    } ();

    Ref<SecurityOrigin> origin = SecurityOrigin::create(rootFrameURL);
    if (!origin->isOpaque())
        return origin->toRawString();

    // toRawString() is not supposed to work with opaque origins, and would just return "://".
    return makeString(rootFrameURL.protocol(), ':');
}

void WebPage::didUpdateRendering(OptionSet<DidUpdateRenderingFlags> flags)
{
    if (flags & DidUpdateRenderingFlags::PaintedLayers) {
#if ENABLE(GPU_PROCESS)
        if (RefPtr proxy = m_remoteRenderingBackendProxy)
            proxy->didPaintLayers();
#endif
    }

    if (flags & DidUpdateRenderingFlags::NotifyUIProcess) {
        if (m_didUpdateRenderingAfterCommittingLoad)
            return;

        m_didUpdateRenderingAfterCommittingLoad = true;
        send(Messages::WebPageProxy::DidUpdateRenderingAfterCommittingLoad());
    }

    protectedCorePage()->didUpdateRendering();
}

bool WebPage::shouldTriggerRenderingUpdate(unsigned rescheduledRenderingUpdateCount) const
{
#if ENABLE(GPU_PROCESS)
    static constexpr unsigned maxRescheduledRenderingUpdateCount = FullSpeedFramesPerSecond;
    if (rescheduledRenderingUpdateCount >= maxRescheduledRenderingUpdateCount)
        return true;

    static constexpr unsigned maxDelayedRenderingUpdateCount = 2;
    RefPtr proxy = m_remoteRenderingBackendProxy;
    if (proxy && proxy->delayedRenderingUpdateCount() > maxDelayedRenderingUpdateCount)
        return false;
#endif
    return true;
}

void WebPage::finalizeRenderingUpdate(OptionSet<FinalizeRenderingUpdateFlags> flags)
{
#if !PLATFORM(COCOA)
    WTFBeginSignpost(this, FinalizeRenderingUpdate);
#endif

    protectedCorePage()->finalizeRenderingUpdate(flags);
#if ENABLE(GPU_PROCESS)
    if (RefPtr proxy = m_remoteRenderingBackendProxy)
        proxy->finalizeRenderingUpdate();
#endif
    flushDeferredDidReceiveMouseEvent();

#if !PLATFORM(COCOA)
    WTFEndSignpost(this, FinalizeRenderingUpdate);
#endif
}

void WebPage::willStartRenderingUpdateDisplay()
{
    if (m_isClosed)
        return;
    protectedCorePage()->willStartRenderingUpdateDisplay();
}

void WebPage::didCompleteRenderingUpdateDisplay()
{
    if (m_isClosed)
        return;
    protectedCorePage()->didCompleteRenderingUpdateDisplay();
}

void WebPage::didCompleteRenderingFrame()
{
    if (m_isClosed)
        return;
    protectedCorePage()->didCompleteRenderingFrame();
}

void WebPage::releaseMemory(Critical)
{
#if ENABLE(GPU_PROCESS)
    if (RefPtr renderingBackend = m_remoteRenderingBackendProxy)
        renderingBackend->releaseMemory();
#endif

    m_foundTextRangeController->clearCachedRanges();
}

void WebPage::willDestroyDecodedDataForAllImages()
{
    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->setNextRenderingUpdateRequiresSynchronousImageDecoding();
}

unsigned WebPage::remoteImagesCountForTesting() const
{
#if ENABLE(GPU_PROCESS)
    if (RefPtr renderingBackend = m_remoteRenderingBackendProxy)
        return renderingBackend->nativeImageCountForTesting();
#endif
    return 0;
}

WebInspectorBackend* WebPage::inspector(LazyCreationPolicy behavior)
{
    if (m_isClosed)
        return nullptr;
    if (!m_inspector && behavior == LazyCreationPolicy::CreateIfNeeded)
        m_inspector = WebInspectorBackend::create(*this);
    return m_inspector.get();
}

RefPtr<WebInspectorBackend> WebPage::protectedInspector()
{
    return inspector();
}

WebInspectorUI* WebPage::inspectorUI()
{
    if (m_isClosed)
        return nullptr;
    if (!m_inspectorUI)
        m_inspectorUI = WebInspectorUI::create(*this);
    return m_inspectorUI.get();
}

RemoteWebInspectorUI* WebPage::remoteInspectorUI()
{
    if (m_isClosed)
        return nullptr;
    if (!m_remoteInspectorUI)
        m_remoteInspectorUI = RemoteWebInspectorUI::create(*this);
    return m_remoteInspectorUI.get();
}

void WebPage::inspectorFrontendCountChanged(unsigned count)
{
    send(Messages::WebPageProxy::DidChangeInspectorFrontendCount(count));
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
PlaybackSessionManager& WebPage::playbackSessionManager()
{
    if (!m_playbackSessionManager)
        m_playbackSessionManager = PlaybackSessionManager::create(*this);
    return *m_playbackSessionManager;
}

Ref<PlaybackSessionManager> WebPage::protectedPlaybackSessionManager()
{
    return playbackSessionManager();
}

VideoPresentationManager& WebPage::videoPresentationManager()
{
    if (!m_videoPresentationManager)
        m_videoPresentationManager = VideoPresentationManager::create(*this, protectedPlaybackSessionManager());
    return *m_videoPresentationManager;
}

Ref<VideoPresentationManager> WebPage::protectedVideoPresentationManager()
{
    return videoPresentationManager();
}

void WebPage::videoControlsManagerDidChange()
{
#if ENABLE(FULLSCREEN_API)
    protectedFullscreenManager()->videoControlsManagerDidChange();
#endif
}

void WebPage::startPlayingPredominantVideo(CompletionHandler<void(bool)>&& completion)
{
    RefPtr mainFrame = m_mainFrame->coreLocalFrame();
    if (!mainFrame) {
        completion(false);
        return;
    }

    RefPtr view = mainFrame->view();
    if (!view) {
        completion(false);
        return;
    }

    RefPtr document = mainFrame->document();
    if (!document) {
        completion(false);
        return;
    }

    Vector<Ref<HTMLMediaElement>> candidates;
    document->updateLayoutIgnorePendingStylesheets();
    document->forEachMediaElement([&candidates](auto& element) {
        if (!element.canPlay())
            return;

        if (!element.isVisibleInViewport())
            return;

        candidates.append(element);
    });

    RefPtr<HTMLMediaElement> largestElement;
    float largestArea = 0;
    auto unobscuredContentRect = view->unobscuredContentRect();
    auto unobscuredArea = unobscuredContentRect.area<RecordOverflow>();
    if (unobscuredArea.hasOverflowed()) {
        completion(false);
        return;
    }

    constexpr auto minimumViewportRatioForLargestMediaElement = 0.25;
    float minimumAreaForLargestElement = minimumViewportRatioForLargestMediaElement * unobscuredArea.value();
    for (auto& candidate : candidates) {
        auto intersectionRect = intersection(unobscuredContentRect, candidate->boundingBoxInRootViewCoordinates());
        if (intersectionRect.isEmpty())
            continue;

        auto area = intersectionRect.area<RecordOverflow>();
        if (area.hasOverflowed())
            continue;

        if (area <= largestArea)
            continue;

        if (area < minimumAreaForLargestElement)
            continue;

        largestArea = area;
        largestElement = candidate.ptr();
    }

    if (!largestElement) {
        completion(false);
        return;
    }

    UserGestureIndicator userGesture { IsProcessingUserGesture::Yes, document.get() };
    largestElement->play();
    completion(true);
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#if PLATFORM(IOS_FAMILY)
void WebPage::setSceneIdentifier(String&& sceneIdentifier)
{
    AudioSession::singleton().setSceneIdentifier(sceneIdentifier);
    m_page->setSceneIdentifier(WTFMove(sceneIdentifier));
}

void WebPage::setAllowsMediaDocumentInlinePlayback(bool allows)
{
    m_page->setAllowsMediaDocumentInlinePlayback(allows);
}
#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManager& WebPage::fullScreenManager()
{
    if (!m_fullScreenManager)
        m_fullScreenManager = WebFullScreenManager::create(*this);
    return *m_fullScreenManager;
}

Ref<WebFullScreenManager> WebPage::protectedFullscreenManager()
{
    return fullScreenManager();
}

void WebPage::isInFullscreenChanged(IsInFullscreenMode isInFullscreenMode)
{
    if (m_isInFullscreenMode == isInFullscreenMode)
        return;
    m_isInFullscreenMode = isInFullscreenMode;

#if ENABLE(META_VIEWPORT)
    resetViewportDefaultConfiguration(m_mainFrame.ptr(), m_isMobileDoctype);
#endif
}

void WebPage::closeFullScreen()
{
    removeReasonsToDisallowLayoutViewportHeightExpansion(DisallowLayoutViewportHeightExpansionReason::ElementFullScreen);

    send(Messages::WebFullScreenManagerProxy::Close());
}

void WebPage::prepareToEnterElementFullScreen()
{
    addReasonsToDisallowLayoutViewportHeightExpansion(DisallowLayoutViewportHeightExpansionReason::ElementFullScreen);
}

void WebPage::prepareToExitElementFullScreen()
{
    removeReasonsToDisallowLayoutViewportHeightExpansion(DisallowLayoutViewportHeightExpansionReason::ElementFullScreen);
}

#endif // ENABLE(FULLSCREEN_API)

void WebPage::addConsoleMessage(FrameIdentifier frameID, MessageSource messageSource, MessageLevel messageLevel, const String& message, std::optional<WebCore::ResourceLoaderIdentifier> requestID)
{
    if (RefPtr frame = WebProcess::singleton().webFrame(frameID))
        frame->addConsoleMessage(messageSource, messageLevel, message, requestID ? requestID->toUInt64() : 0);
}

void WebPage::enqueueSecurityPolicyViolationEvent(FrameIdentifier frameID, SecurityPolicyViolationEventInit&& eventInit)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;
    if (RefPtr document = coreFrame->document())
        document->enqueueSecurityPolicyViolationEvent(WTFMove(eventInit));
}

void WebPage::notifyReportObservers(FrameIdentifier frameID, Ref<WebCore::Report>&& report)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;
    if (RefPtr document = coreFrame->document())
        document->protectedReportingScope()->notifyReportObservers(WTFMove(report));
}

void WebPage::sendReportToEndpoints(FrameIdentifier frameID, URL&& baseURL, const Vector<String>& endpointURIs, const Vector<String>& endpointTokens, IPC::FormDataReference&& reportData, WebCore::ViolationReportType reportType)
{
    auto report = reportData.takeData();
    if (!report)
        return;

    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame || !frame->coreLocalFrame())
        return;

    for (auto& url : endpointURIs)
        PingLoader::sendViolationReport(*frame->protectedCoreLocalFrame(), URL { baseURL, url }, Ref { *report.get() }, reportType);

    RefPtr document = frame->coreLocalFrame()->document();
    if (!document)
        return;

    for (auto& token : endpointTokens) {
        if (auto url = document->endpointURIForToken(token); !url.isEmpty())
            PingLoader::sendViolationReport(*frame->protectedCoreLocalFrame(), URL { baseURL, url }, Ref { *report.get() }, reportType);
    }
}

NotificationPermissionRequestManager* WebPage::notificationPermissionRequestManager()
{
    if (m_notificationPermissionRequestManager)
        return m_notificationPermissionRequestManager.get();

    m_notificationPermissionRequestManager = NotificationPermissionRequestManager::create(this);
    return m_notificationPermissionRequestManager.get();
}

RefPtr<NotificationPermissionRequestManager> WebPage::protectedNotificationPermissionRequestManager()
{
    return notificationPermissionRequestManager();
}

#if ENABLE(DRAG_SUPPORT)

#if PLATFORM(GTK)
void WebPage::performDragControllerAction(DragControllerAction action, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation> draggingSourceOperationMask, SelectionData&& selectionData, OptionSet<DragApplicationFlags> flags, CompletionHandler<void(std::optional<DragOperation>, DragHandlingMethod, bool, unsigned, IntRect, IntRect, std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
    if (!m_page)
        return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::nullopt);

    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame)
        return;

    DragData dragData(&selectionData, clientPosition, globalPosition, draggingSourceOperationMask, flags, anyDragDestinationAction(), m_identifier);
    switch (action) {
    case DragControllerAction::Entered:
    case DragControllerAction::Updated: {
        auto resolvedDragAction = m_page->dragController().dragEnteredOrUpdated(*localMainFrame, WTFMove(dragData));
        if (std::holds_alternative<RemoteUserInputEventData>(resolvedDragAction))
            return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::get<RemoteUserInputEventData>(resolvedDragAction));
        auto dragOperation = std::get<std::optional<DragOperation>>(resolvedDragAction);
        return completionHandler(dragOperation, m_page->dragController().dragHandlingMethod(), m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted(), { }, { }, std::nullopt);
    }
    case DragControllerAction::Exited:
        m_page->dragController().dragExited(*localMainFrame, WTFMove(dragData));
        return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::nullopt);

    case DragControllerAction::PerformDragOperation: {
        m_page->dragController().performDragOperation(WTFMove(dragData));
        return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::nullopt);
    }
    }
    ASSERT_NOT_REACHED();
}
#else
void WebPage::performDragControllerAction(std::optional<FrameIdentifier> frameID, DragControllerAction action, DragData&& dragData, CompletionHandler<void(std::optional<DragOperation>, DragHandlingMethod, bool, unsigned, IntRect, IntRect, std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
    if (!m_page)
        return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::nullopt);

    RefPtr frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &mainWebFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr localFrame = frame->coreLocalFrame();
    if (!localFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (action) {
    case DragControllerAction::Entered:
    case DragControllerAction::Updated: {
        auto resolvedDragAction = m_page->dragController().dragEnteredOrUpdated(*localFrame, WTFMove(dragData));
        if (std::holds_alternative<RemoteUserInputEventData>(resolvedDragAction))
            return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::get<RemoteUserInputEventData>(resolvedDragAction));
        auto dragOperation = std::get<std::optional<DragOperation>>(resolvedDragAction);
        return completionHandler(dragOperation, m_page->dragController().dragHandlingMethod(), m_page->dragController().mouseIsOverFileInput(), m_page->dragController().numberOfItemsToBeAccepted(), m_page->dragCaretController().caretRectInRootViewCoordinates(), m_page->dragCaretController().editableElementRectInRootViewCoordinates(), std::nullopt);
    }
    case DragControllerAction::Exited:
        m_page->dragController().dragExited(*localFrame, WTFMove(dragData));
        return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::nullopt);
    case DragControllerAction::PerformDragOperation:
        break;
    }
    ASSERT_NOT_REACHED();
}

void WebPage::performDragOperation(WebCore::DragData&& dragData, SandboxExtension::Handle&& sandboxExtensionHandle, Vector<SandboxExtension::Handle>&& sandboxExtensionsHandleArray, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!m_pendingDropSandboxExtension);

    m_pendingDropSandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    for (size_t i = 0; i < sandboxExtensionsHandleArray.size(); i++) {
        if (auto extension = SandboxExtension::create(WTFMove(sandboxExtensionsHandleArray[i])))
            m_pendingDropExtensionsForFileUpload.append(extension);
    }

    bool handled = m_page->dragController().performDragOperation(WTFMove(dragData));

    // If we started loading a local file, the sandbox extension tracker would have adopted this
    // pending drop sandbox extension. If not, we'll play it safe and clear it.
    m_pendingDropSandboxExtension = nullptr;

    m_pendingDropExtensionsForFileUpload.clear();
    completionHandler(handled);
}
#endif

void WebPage::dragEnded(std::optional<FrameIdentifier> frameID, IntPoint clientPosition, IntPoint globalPosition, OptionSet<DragOperation> dragOperationMask, CompletionHandler<void(std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
    IntPoint adjustedClientPosition(clientPosition.x() + m_page->dragController().dragOffset().x(), clientPosition.y() + m_page->dragController().dragOffset().y());
    IntPoint adjustedGlobalPosition(globalPosition.x() + m_page->dragController().dragOffset().x(), globalPosition.y() + m_page->dragController().dragOffset().y());

    m_page->dragController().dragEnded();
    RefPtr frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &mainWebFrame();
    if (!frame)
        return completionHandler(std::nullopt);

    RefPtr localFrame = frame->coreLocalFrame();
    if (!localFrame)
        return completionHandler(std::nullopt);

    RefPtr view = localFrame->view();
    if (!view)
        return completionHandler(std::nullopt);

    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(adjustedClientPosition, adjustedGlobalPosition, MouseButton::Left, PlatformEvent::Type::MouseMoved, 0, { }, MonotonicTime::now(), 0, WebCore::SyntheticClickType::NoTap);
    auto remoteUserInputEventData = localFrame->eventHandler().dragSourceEndedAt(event, dragOperationMask);

    completionHandler(remoteUserInputEventData);

    m_isStartingDrag = false;
}

void WebPage::willPerformLoadDragDestinationAction()
{
    m_sandboxExtensionTracker.willPerformLoadDragDestinationAction(WTFMove(m_pendingDropSandboxExtension));
}

void WebPage::mayPerformUploadDragDestinationAction()
{
    for (RefPtr extension : m_pendingDropExtensionsForFileUpload)
        extension->consumePermanently();
    m_pendingDropExtensionsForFileUpload.clear();
}

void WebPage::didStartDrag()
{
    m_isStartingDrag = false;
    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->eventHandler().didStartDrag();
}

void WebPage::dragCancelled()
{
    m_isStartingDrag = false;
    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->eventHandler().dragCancelled();
}

#if ENABLE(MODEL_PROCESS)
void WebPage::modelDragEnded(NodeIdentifier nodeIdentifier)
{
    RefPtr node = Node::fromIdentifier(nodeIdentifier);
    if (!node)
        return;

    RefPtr modelElement = dynamicDowncast<HTMLModelElement>(node);
    if (!modelElement)
        return;

    modelElement->resetModelTransformAfterDrag();
}
#endif

#endif // ENABLE(DRAG_SUPPORT)

#if ENABLE(MODEL_PROCESS)
void WebPage::requestInteractiveModelElementAtPoint(IntPoint clientPosition)
{
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame())) {
        auto nodeID = localMainFrame->eventHandler().requestInteractiveModelElementAtPoint(clientPosition);
        send(Messages::WebPageProxy::DidReceiveInteractiveModelElement(nodeID));
    } else
        send(Messages::WebPageProxy::DidReceiveInteractiveModelElement(std::nullopt));
}

void WebPage::stageModeSessionDidUpdate(std::optional<NodeIdentifier> nodeID, const TransformationMatrix& transform)
{
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->eventHandler().stageModeSessionDidUpdate(nodeID, transform);
}

void WebPage::stageModeSessionDidEnd(std::optional<NodeIdentifier> nodeID)
{
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->eventHandler().stageModeSessionDidEnd(nodeID);
}
#endif

WebUndoStep* WebPage::webUndoStep(WebUndoStepID stepID)
{
    return m_undoStepMap.get(stepID);
}

void WebPage::addWebUndoStep(WebUndoStepID stepID, Ref<WebUndoStep>&& entry)
{
    auto addResult = m_undoStepMap.set(stepID, WTFMove(entry));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void WebPage::removeWebEditCommand(WebUndoStepID stepID)
{
    if (auto undoStep = m_undoStepMap.take(stepID))
        undoStep->didRemoveFromUndoManager();
}

void WebPage::unapplyEditCommand(WebUndoStepID stepID)
{
    RefPtr step = webUndoStep(stepID);
    if (!step)
        return;

    step->protectedStep()->unapply();
}

void WebPage::reapplyEditCommand(WebUndoStepID stepID)
{
    RefPtr step = webUndoStep(stepID);
    if (!step)
        return;

    setIsInRedo(true);
    step->protectedStep()->reapply();
    setIsInRedo(false);
}

void WebPage::didRemoveEditCommand(WebUndoStepID commandID)
{
    removeWebEditCommand(commandID);
}

void WebPage::closeCurrentTypingCommand()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (RefPtr document = frame->document())
        document->protectedEditor()->closeTyping();
}

void WebPage::setActivePopupMenu(WebPopupMenu* menu)
{
    m_activePopupMenu = menu;
}

WebColorChooser* WebPage::activeColorChooser() const
{
    return m_activeColorChooser.get();
}

void WebPage::setActiveColorChooser(WebColorChooser* colorChooser)
{
    m_activeColorChooser = colorChooser;
}

void WebPage::didEndColorPicker()
{
    if (RefPtr activeColorChooser = m_activeColorChooser.get())
        activeColorChooser->didEndChooser();
}

void WebPage::didChooseColor(const WebCore::Color& color)
{
    if (RefPtr activeColorChooser = m_activeColorChooser.get())
        activeColorChooser->didChooseColor(color);
}

void WebPage::setActiveDataListSuggestionPicker(WebDataListSuggestionPicker& dataListSuggestionPicker)
{
    m_activeDataListSuggestionPicker = dataListSuggestionPicker;
}

void WebPage::didSelectDataListOption(const String& selectedOption)
{
    if (RefPtr activeDataListSuggestionPicker = m_activeDataListSuggestionPicker.get())
        activeDataListSuggestionPicker->didSelectOption(selectedOption);
}

void WebPage::didCloseSuggestions()
{
    if (RefPtr picker = std::exchange(m_activeDataListSuggestionPicker, nullptr).get())
        picker->didCloseSuggestions();
}

void WebPage::setActiveDateTimeChooser(WebDateTimeChooser& dateTimeChooser)
{
    m_activeDateTimeChooser = dateTimeChooser;
}

void WebPage::didChooseDate(const String& date)
{
    if (RefPtr activeDateTimeChooser = m_activeDateTimeChooser.get())
        activeDateTimeChooser->didChooseDate(date);
}

void WebPage::didEndDateTimePicker()
{
    if (auto chooser = std::exchange(m_activeDateTimeChooser, nullptr))
        chooser->didEndChooser();
}

void WebPage::setActiveOpenPanelResultListener(Ref<WebOpenPanelResultListener>&& openPanelResultListener)
{
    m_activeOpenPanelResultListener = WTFMove(openPanelResultListener);
}

void WebPage::setTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator)
{
    send(Messages::WebPageProxy::SetTextIndicatorFromFrame(m_mainFrame->frameID(), WTFMove(textIndicator), WebCore::TextIndicatorLifetime::Temporary));
}

void WebPage::updateTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator)
{
    send(Messages::WebPageProxy::UpdateTextIndicatorFromFrame(m_mainFrame->frameID(), WTFMove(textIndicator)));
}

void WebPage::replaceStringMatchesFromInjectedBundle(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly)
{
    findController().replaceMatches(matchIndices, replacementText, selectionOnly);
}

void WebPage::findString(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(std::optional<FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
    findController().findString(string, options, maxMatchCount, WTFMove(completionHandler));
}

#if ENABLE(IMAGE_ANALYSIS)
void WebPage::findStringIncludingImages(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(std::optional<FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
    findController().findStringIncludingImages(string, options, maxMatchCount, WTFMove(completionHandler));
}
#endif

void WebPage::findStringMatches(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(Vector<Vector<WebCore::IntRect>>, int32_t)>&& completionHandler)
{
    findController().findStringMatches(string, options, maxMatchCount, WTFMove(completionHandler));
}

void WebPage::findTextRangesForStringMatches(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& completionHandler)
{
    foundTextRangeController().findTextRangesForStringMatches(string, options, maxMatchCount, WTFMove(completionHandler));
}

void WebPage::replaceFoundTextRangeWithString(const WebFoundTextRange& range, const String& string)
{
    foundTextRangeController().replaceFoundTextRangeWithString(range, string);
}

void WebPage::decorateTextRangeWithStyle(const WebFoundTextRange& range, WebKit::FindDecorationStyle style)
{
    foundTextRangeController().decorateTextRangeWithStyle(range, style);
}

void WebPage::scrollTextRangeToVisible(const WebFoundTextRange& range)
{
    foundTextRangeController().scrollTextRangeToVisible(range);
}

void WebPage::clearAllDecoratedFoundText()
{
    hideFindUI();
    foundTextRangeController().clearAllDecoratedFoundText();
}

void WebPage::didBeginTextSearchOperation()
{
    foundTextRangeController().didBeginTextSearchOperation();
}

void WebPage::requestRectForFoundTextRange(const WebFoundTextRange& range, CompletionHandler<void(WebCore::FloatRect)>&& completionHandler)
{
    foundTextRangeController().requestRectForFoundTextRange(range, WTFMove(completionHandler));
}

void WebPage::addLayerForFindOverlay(CompletionHandler<void(std::optional<WebCore::PlatformLayerIdentifier>)>&& completionHandler)
{
    foundTextRangeController().addLayerForFindOverlay(WTFMove(completionHandler));
}

void WebPage::removeLayerForFindOverlay(CompletionHandler<void()>&& completionHandler)
{
    foundTextRangeController().removeLayerForFindOverlay();
    completionHandler();
}

void WebPage::getImageForFindMatch(uint32_t matchIndex)
{
    findController().getImageForFindMatch(matchIndex);
}

void WebPage::selectFindMatch(uint32_t matchIndex)
{
    findController().selectFindMatch(matchIndex);
}

void WebPage::indicateFindMatch(uint32_t matchIndex)
{
    findController().indicateFindMatch(matchIndex);
}

void WebPage::hideFindUI()
{
    findController().hideFindUI();
}

void WebPage::countStringMatches(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    findController().countStringMatches(string, options, maxMatchCount, WTFMove(completionHandler));
}

void WebPage::replaceMatches(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    auto numberOfReplacements = findController().replaceMatches(matchIndices, replacementText, selectionOnly);
    completionHandler(numberOfReplacements);
}

void WebPage::didChangeSelectedIndexForActivePopupMenu(int32_t newIndex)
{
    changeSelectedIndex(newIndex);
    m_activePopupMenu = nullptr;
}

void WebPage::changeSelectedIndex(int32_t index)
{
    if (RefPtr menu = m_activePopupMenu)
        menu->didChangeSelectedIndex(index);
}

#if PLATFORM(IOS_FAMILY)
void WebPage::didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>& files, const String& displayString, std::span<const uint8_t> iconData)
{
    RefPtr activeOpenPanelResultListener = m_activeOpenPanelResultListener;
    if (!activeOpenPanelResultListener)
        return;

    RefPtr<Icon> icon;
    if (!iconData.empty()) {
        RetainPtr<CFDataRef> dataRef = adoptCF(CFDataCreate(nullptr, iconData.data(), iconData.size()));
        RetainPtr<CGDataProviderRef> imageProviderRef = adoptCF(CGDataProviderCreateWithCFData(dataRef.get()));
        RetainPtr<CGImageRef> imageRef = adoptCF(CGImageCreateWithPNGDataProvider(imageProviderRef.get(), nullptr, true, kCGRenderingIntentDefault));
        if (!imageRef)
            imageRef = adoptCF(CGImageCreateWithJPEGDataProvider(imageProviderRef.get(), nullptr, true, kCGRenderingIntentDefault));
        icon = Icon::create(WTFMove(imageRef));
    }

    activeOpenPanelResultListener->didChooseFilesWithDisplayStringAndIcon(files, displayString, icon.get());
    m_activeOpenPanelResultListener = nullptr;
}
#endif

void WebPage::didChooseFilesForOpenPanel(const Vector<String>& files, const Vector<String>& replacementFiles)
{
    if (RefPtr activeOpenPanelResultListener = std::exchange(m_activeOpenPanelResultListener, nullptr))
        activeOpenPanelResultListener->didChooseFiles(files, replacementFiles);
}

void WebPage::didCancelForOpenPanel()
{
    if (RefPtr activeOpenPanelResultListener = std::exchange(m_activeOpenPanelResultListener, nullptr))
        activeOpenPanelResultListener->didCancelFileChoosing();
}

#if ENABLE(SANDBOX_EXTENSIONS)
void WebPage::extendSandboxForFilesFromOpenPanel(Vector<SandboxExtension::Handle>&& handles)
{
    bool result = SandboxExtension::consumePermanently(handles);
    if (!result) {
        // We have reports of cases where this fails for some unknown reason, <rdar://problem/10156710>.
        WTFLogAlways("WebPage::extendSandboxForFileFromOpenPanel(): Could not consume a sandbox extension");
    }
}
#endif

#if ENABLE(GEOLOCATION)
void WebPage::didReceiveGeolocationPermissionDecision(GeolocationIdentifier geolocationID, const String& authorizationToken)
{
    m_geolocationPermissionRequestManager->didReceiveGeolocationPermissionDecision(geolocationID, authorizationToken);
}
#endif

#if ENABLE(MEDIA_STREAM)

void WebPage::userMediaAccessWasGranted(UserMediaRequestIdentifier userMediaID, WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice, WebCore::MediaDeviceHashSalts&& mediaDeviceIdentifierHashSalts, Vector<SandboxExtension::Handle>&& handles, CompletionHandler<void()>&& completionHandler)
{
    SandboxExtension::consumePermanently(handles);

    m_userMediaPermissionRequestManager->userMediaAccessWasGranted(userMediaID, WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(mediaDeviceIdentifierHashSalts), WTFMove(completionHandler));
}

void WebPage::userMediaAccessWasDenied(UserMediaRequestIdentifier userMediaID, uint64_t reason, String&& message, WebCore::MediaConstraintType invalidConstraint)
{
    m_userMediaPermissionRequestManager->userMediaAccessWasDenied(userMediaID, static_cast<MediaAccessDenialReason>(reason), WTFMove(message), invalidConstraint);
}

void WebPage::captureDevicesChanged()
{
    m_userMediaPermissionRequestManager->captureDevicesChanged();
}

void WebPage::voiceActivityDetected()
{
    protectedCorePage()->voiceActivityDetected();
}

#if USE(GSTREAMER)
void WebPage::setOrientationForMediaCapture(uint64_t rotation)
{
    m_page->forEachDocument([&](auto& document) {
        document.orientationChanged(rotation);
    });
}

void WebPage::setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted)
{
    MockRealtimeMediaSourceCenter::setMockCaptureDevicesInterrupted(isCameraInterrupted, isMicrophoneInterrupted);
}

void WebPage::triggerMockCaptureConfigurationChange(bool forCamera, bool forMicrophone, bool forDisplay)
{
    MockRealtimeMediaSourceCenter::singleton().triggerMockCaptureConfigurationChange(forCamera, forMicrophone, forDisplay);
}
#endif // USE(GSTREAMER)

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(ENCRYPTED_MEDIA)
void WebPage::mediaKeySystemWasGranted(MediaKeySystemRequestIdentifier mediaKeySystemID, String&& mediaKeysHashSalt)
{
    m_mediaKeySystemPermissionRequestManager->mediaKeySystemWasGranted(mediaKeySystemID, WTFMove(mediaKeysHashSalt));
}

void WebPage::mediaKeySystemWasDenied(MediaKeySystemRequestIdentifier mediaKeySystemID, String&& message)
{
    m_mediaKeySystemPermissionRequestManager->mediaKeySystemWasDenied(mediaKeySystemID, WTFMove(message));
}
#endif

#if !PLATFORM(IOS_FAMILY)
void WebPage::advanceToNextMisspelling(bool startBeforeSelection)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->protectedEditor()->advanceToNextMisspelling(startBeforeSelection);
}
#endif

bool WebPage::hasRichlyEditableSelection() const
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return false;

    if (m_page->dragCaretController().isContentRichlyEditable())
        return true;

    return frame->selection().selection().isContentRichlyEditable();
}

void WebPage::changeSpellingToWord(const String& word)
{
    replaceSelectionWithText(corePage()->focusController().protectedFocusedOrMainFrame().get(), word);
}

void WebPage::unmarkAllMisspellings()
{
    for (RefPtr frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(*frame);
        if (!localFrame)
            continue;
        if (RefPtr document = localFrame->document())
            document->checkedMarkers()->removeMarkers(DocumentMarkerType::Spelling);
    }
}

void WebPage::unmarkAllBadGrammar()
{
    for (RefPtr frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(*frame);
        if (!localFrame)
            continue;
        if (RefPtr document = localFrame->document())
            document->checkedMarkers()->removeMarkers(DocumentMarkerType::Grammar);
    }
}

#if USE(APPKIT)
void WebPage::uppercaseWord(FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;

    coreFrame->protectedEditor()->uppercaseWord();
}

void WebPage::lowercaseWord(FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;

    coreFrame->protectedEditor()->lowercaseWord();
}

void WebPage::capitalizeWord(FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;


    coreFrame->protectedEditor()->capitalizeWord();
}
#endif

void WebPage::setTextForActivePopupMenu(int32_t index)
{
    if (RefPtr menu = m_activePopupMenu)
        menu->setTextForIndex(index);
}

#if PLATFORM(GTK)
void WebPage::failedToShowPopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->client()->popupDidHide();
}
#endif

#if ENABLE(CONTEXT_MENUS)
void WebPage::didSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    if (auto contextMenu = std::exchange(m_contextMenu, nullptr))
        contextMenu->itemSelected(item);
}
#endif

void WebPage::replaceSelectionWithText(LocalFrame* frame, const String& text)
{
    return frame->protectedEditor()->replaceSelectionWithText(text, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No);
}

#if !PLATFORM(IOS_FAMILY)
void WebPage::clearSelection()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->checkedSelection()->clear();
}
#endif

void WebPage::restoreSelectionInFocusedEditableElement()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (!frame->selection().isNone())
        return;

    if (RefPtr document = frame->document()) {
        if (RefPtr element = document->focusedElement())
            element->updateFocusAppearance(SelectionRestorationMode::RestoreOrSelectAll, SelectionRevealMode::DoNotReveal);
    }
}

bool WebPage::mainFrameHasCustomContentProvider() const
{
    if (RefPtr frame = localMainFrame()) {
        auto* webFrameLoaderClient = dynamicDowncast<WebLocalFrameLoaderClient>(frame->loader().client());
        ASSERT(webFrameLoaderClient);
        return webFrameLoaderClient->frameHasCustomContentProvider();
    }

    return false;
}

void WebPage::updateMainFrameScrollOffsetPinning()
{
    RefPtr frameView = localMainFrameView();
    if (!frameView)
        return;

    auto pinnedState = frameView->edgePinnedState();
    if (pinnedState != m_cachedMainFramePinnedState) {
        send(Messages::WebPageProxy::DidChangeScrollOffsetPinningForMainFrame(pinnedState));
        m_cachedMainFramePinnedState = pinnedState;
    }
}

void WebPage::mainFrameDidLayout()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    unsigned pageCount = protectedCorePage()->pageCountAssumingLayoutIsUpToDate();
    if (pageCount != m_cachedPageCount) {
        send(Messages::WebPageProxy::DidChangePageCount(pageCount));
        m_cachedPageCount = pageCount;
    }

#if PLATFORM(COCOA) || PLATFORM(GTK)
    if (RefPtr viewGestureGeometryCollector = m_viewGestureGeometryCollector)
        viewGestureGeometryCollector->mainFrameDidLayout();
#endif
#if PLATFORM(IOS_FAMILY)
    if (RefPtr frameView = localMainFrameView()) {
        IntSize newContentSize = frameView->contentsSize();
        LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_identifier.toUInt64() << " mainFrameDidLayout setting content size to " << newContentSize);
        if (m_viewportConfiguration.setContentsSize(newContentSize))
            viewportConfigurationChanged();
    }
#endif
}

#if ENABLE(PDF_PLUGIN)

void WebPage::addPluginView(PluginView& pluginView)
{
    ASSERT(!m_pluginViews.contains(pluginView));
    m_pluginViews.add(pluginView);
}

void WebPage::removePluginView(PluginView& pluginView)
{
    ASSERT(m_pluginViews.contains(pluginView));
    m_pluginViews.remove(pluginView);
}

#endif // ENABLE(PDF_PLUGIN)

void WebPage::sendSetWindowFrame(const FloatRect& windowFrame)
{
#if PLATFORM(COCOA)
    m_hasCachedWindowFrame = false;
#endif
    send(Messages::WebPageProxy::SetWindowFrame(windowFrame));
}

#if PLATFORM(COCOA)

void WebPage::windowAndViewFramesChanged(const ViewWindowCoordinates& coordinates)
{
    m_windowFrameInScreenCoordinates = coordinates.windowFrameInScreenCoordinates;
    m_windowFrameInUnflippedScreenCoordinates = coordinates.windowFrameInUnflippedScreenCoordinates;
    m_viewFrameInWindowCoordinates = coordinates.viewFrameInWindowCoordinates;

    m_accessibilityPosition = coordinates.accessibilityViewCoordinates;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    cacheAXPosition(m_accessibilityPosition);
#endif

    m_hasCachedWindowFrame = !m_windowFrameInUnflippedScreenCoordinates.isEmpty();
}

#endif

void WebPage::setMainFrameIsScrollable(bool isScrollable)
{
    m_mainFrameIsScrollable = isScrollable;
    protectedDrawingArea()->mainFrameScrollabilityChanged(isScrollable);

    if (RefPtr frameView = m_mainFrame->coreLocalFrame()->view()) {
        frameView->setCanHaveScrollbars(isScrollable);
        frameView->setProhibitsScrolling(!isScrollable);
    }
}

bool WebPage::windowIsFocused() const
{
    return m_page->focusController().isActive();
}

bool WebPage::windowAndWebPageAreFocused() const
{
    return isVisible() && m_page->focusController().isFocused() && m_page->focusController().isActive();
}

bool WebPage::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::WebInspectorBackend::messageReceiverName()) {
        if (RefPtr inspector = this->inspector())
            inspector->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::WebInspectorUI::messageReceiverName()) {
        if (RefPtr inspectorUI = this->inspectorUI())
            inspectorUI->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteWebInspectorUI::messageReceiverName()) {
        if (RefPtr remoteInspectorUI = this->remoteInspectorUI())
            remoteInspectorUI->didReceiveMessage(connection, decoder);
        return true;
    }

#if ENABLE(FULLSCREEN_API)
    if (decoder.messageReceiverName() == Messages::WebFullScreenManager::messageReceiverName()) {
        protectedFullscreenManager()->didReceiveMessage(connection, decoder);
        return true;
    }
#endif
    return false;
}

#if ENABLE(ASYNC_SCROLLING)
ScrollingCoordinator* WebPage::scrollingCoordinator() const
{
    return protectedCorePage()->scrollingCoordinator();
}

RefPtr<ScrollingCoordinator> WebPage::protectedScrollingCoordinator() const
{
    return scrollingCoordinator();
}
#endif

WebPage::SandboxExtensionTracker::~SandboxExtensionTracker()
{
    invalidate();
}

void WebPage::SandboxExtensionTracker::invalidate()
{
    m_pendingProvisionalSandboxExtension = nullptr;

    if (RefPtr extension = std::exchange(m_provisionalSandboxExtension, nullptr))
        extension->revoke();

    if (RefPtr extension = std::exchange(m_committedSandboxExtension, nullptr))
        extension->revoke();
}

void WebPage::SandboxExtensionTracker::willPerformLoadDragDestinationAction(RefPtr<SandboxExtension>&& pendingDropSandboxExtension)
{
    setPendingProvisionalSandboxExtension(WTFMove(pendingDropSandboxExtension));
}

void WebPage::SandboxExtensionTracker::beginLoad(SandboxExtension::Handle&& handle)
{
    setPendingProvisionalSandboxExtension(SandboxExtension::create(WTFMove(handle)));
}

void WebPage::SandboxExtensionTracker::beginReload(WebFrame* frame, SandboxExtension::Handle&& handle)
{
    ASSERT_UNUSED(frame, frame->isMainFrame());

    // Maintain existing provisional SandboxExtension in case of a reload, if the new handle is null. This is needed
    // because the UIProcess sends us a null handle if it already sent us a handle for this path in the past.
    if (auto sandboxExtension = SandboxExtension::create(WTFMove(handle)))
        setPendingProvisionalSandboxExtension(WTFMove(sandboxExtension));
}

void WebPage::SandboxExtensionTracker::setPendingProvisionalSandboxExtension(RefPtr<SandboxExtension>&& pendingProvisionalSandboxExtension)
{
    m_pendingProvisionalSandboxExtension = WTFMove(pendingProvisionalSandboxExtension);
}

bool WebPage::SandboxExtensionTracker::shouldReuseCommittedSandboxExtension(WebFrame* frame)
{
    ASSERT(frame->isMainFrame());

    FrameLoader& frameLoader = frame->coreLocalFrame()->loader();
    FrameLoadType frameLoadType = frameLoader.loadType();

    // If the page is being reloaded, it should reuse whatever extension is committed.
    if (isReload(frameLoadType))
        return true;

    if (m_pendingProvisionalSandboxExtension)
        return false;

    RefPtr documentLoader = frameLoader.documentLoader();
    RefPtr provisionalDocumentLoader = frameLoader.provisionalDocumentLoader();
    if (!documentLoader || !provisionalDocumentLoader)
        return false;

    if (documentLoader->url().protocolIsFile() && provisionalDocumentLoader->url().protocolIsFile())
        return true;

    return false;
}

void WebPage::SandboxExtensionTracker::didStartProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // We should only reuse the commited sandbox extension if it is not null. It can be
    // null if the last load was for an error page.
    if (m_committedSandboxExtension && shouldReuseCommittedSandboxExtension(frame))
        m_pendingProvisionalSandboxExtension = m_committedSandboxExtension;

    ASSERT(!m_provisionalSandboxExtension);

    m_provisionalSandboxExtension = WTFMove(m_pendingProvisionalSandboxExtension);
    if (RefPtr extension = m_provisionalSandboxExtension)
        extension->consume();
}

void WebPage::SandboxExtensionTracker::didCommitProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (RefPtr committedSandboxExtension = m_committedSandboxExtension)
        committedSandboxExtension->revoke();

    m_committedSandboxExtension = WTFMove(m_provisionalSandboxExtension);

    // We can also have a non-null m_pendingProvisionalSandboxExtension if a new load is being started.
    // This extension is not cleared, because it does not pertain to the failed load, and will be needed.
}

void WebPage::SandboxExtensionTracker::didFailProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (RefPtr extension = std::exchange(m_provisionalSandboxExtension, nullptr))
        extension->revoke();

    // We can also have a non-null m_pendingProvisionalSandboxExtension if a new load is being started
    // (notably, if the current one fails because the new one cancels it). This extension is not cleared,
    // because it does not pertain to the failed load, and will be needed.
}

void WebPage::setCustomTextEncodingName(const String& encoding)
{
    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->loader().reloadWithOverrideEncoding(encoding);
}

void WebPage::didRemoveBackForwardItem(BackForwardItemIdentifier itemID)
{
    WebBackForwardListProxy::removeItem(itemID);
}

#if PLATFORM(MAC)
void WebPage::setCaretAnimatorType(WebCore::CaretAnimatorType caretType)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->checkedSelection()->caretAnimatorInvalidated(caretType);
}

void WebPage::setCaretBlinkingSuspended(bool suspended)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    frame->checkedSelection()->setCaretBlinkingSuspended(suspended);
}

#endif

void WebPage::setUseColorAppearance(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
    protectedCorePage()->setUseColorAppearance(useDarkAppearance, useElevatedUserInterfaceLevel);

    if (RefPtr inspectorUI = m_inspectorUI)
        inspectorUI->effectiveAppearanceDidChange(useDarkAppearance ? WebCore::InspectorFrontendClient::Appearance::Dark : WebCore::InspectorFrontendClient::Appearance::Light);
}

void WebPage::swipeAnimationDidStart()
{
    freezeLayerTree(LayerTreeFreezeReason::SwipeAnimation);
    corePage()->setIsInSwipeAnimation(true);
}

void WebPage::swipeAnimationDidEnd()
{
    unfreezeLayerTree(LayerTreeFreezeReason::SwipeAnimation);
    corePage()->setIsInSwipeAnimation(false);
}

void WebPage::beginPrinting(FrameIdentifier frameID, const PrintInfo& printInfo)
{
    RELEASE_LOG(Printing, "Begin printing.");

    PrintContextAccessScope scope { *this };

    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;

#if PLATFORM(COCOA)
    if (pdfDocumentForPrintingFrame(coreFrame.get()))
        return;
#endif

    if (!m_printContext) {
        m_printContext = PrintContext::create(coreFrame.get());
        protectedCorePage()->dispatchBeforePrintEvent();
    }
    RefPtr printContext = m_printContext;

    freezeLayerTree(LayerTreeFreezeReason::Printing);

    auto computedPageSize = printContext->computedPageSize(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight), printInfo.margin);

    printContext->begin(computedPageSize.width(), computedPageSize.height());

    // PrintContext::begin() performed a synchronous layout which might have executed a
    // script that closed the WebPage, clearing m_printContext.
    // See <rdar://problem/49731211> for cases of this happening.
    printContext = m_printContext;
    if (!printContext) {
        unfreezeLayerTree(LayerTreeFreezeReason::Printing);
        return;
    }

    float fullPageHeight;
    printContext->computePageRects(FloatRect(0, 0, computedPageSize.width(), computedPageSize.height()), 0, 0, printInfo.pageSetupScaleFactor, fullPageHeight, true);

#if PLATFORM(GTK)
    if (!m_printOperation)
        m_printOperation = makeUnique<WebPrintOperationGtk>(printInfo);
#endif
}

void WebPage::endPrinting(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(Printing, "End printing.");

    if (m_inActivePrintContextAccessScope) {
        m_shouldEndPrintingImmediately = true;
        completionHandler();
        return;
    }
    endPrintingImmediately();
    completionHandler();
}

void WebPage::endPrintingImmediately()
{
    RELEASE_ASSERT(!m_inActivePrintContextAccessScope);
    m_shouldEndPrintingImmediately = false;

    unfreezeLayerTree(LayerTreeFreezeReason::Printing);

    if (m_printContext) {
        m_printContext = nullptr;
        protectedCorePage()->dispatchAfterPrintEvent();
    }
}

void WebPage::computePagesForPrinting(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&& completionHandler)
{
    PrintContextAccessScope scope { *this };
    Vector<IntRect> resultPageRects;
    double resultTotalScaleFactorForPrinting = 1;
    auto computedPageMargin = printInfo.margin;
    computePagesForPrintingImpl(frameID, printInfo, resultPageRects, resultTotalScaleFactorForPrinting, computedPageMargin);
    completionHandler(resultPageRects, resultTotalScaleFactorForPrinting, computedPageMargin);
}

void WebPage::computePagesForPrintingImpl(FrameIdentifier frameID, const PrintInfo& printInfo, Vector<WebCore::IntRect>& resultPageRects, double& resultTotalScaleFactorForPrinting, FloatBoxExtent& computedPageMargin)
{
    ASSERT(resultPageRects.isEmpty());

    beginPrinting(frameID, printInfo);

    if (RefPtr printContext = m_printContext) {
        PrintContextAccessScope scope { *this };
        resultPageRects = printContext->pageRects();
        computedPageMargin = printContext->computedPageMargin(printInfo.margin);
        auto computedPageSize = printContext->computedPageSize(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight), printInfo.margin);
        resultTotalScaleFactorForPrinting = printContext->computeAutomaticScaleFactor(computedPageSize) * printInfo.pageSetupScaleFactor;
    }
#if PLATFORM(COCOA)
    else
        computePagesForPrintingPDFDocument(frameID, printInfo, resultPageRects);
#endif // PLATFORM(COCOA)

    // If we're asked to print, we should actually print at least a blank page.
    if (resultPageRects.isEmpty())
        resultPageRects.append(IntRect(0, 0, 1, 1));
}

void WebPage::paintRemoteFrameContents(FrameIdentifier frameID, const IntRect& rect, GraphicsContext& context)
{
#if ENABLE(GPU_PROCESS)
    // Painting remote frames supported only for snapshot purposes.
    if (!m_remoteSnapshotState || m_remoteSnapshotState->recorder.ptr() != &context)
        return;
    sendWithAsyncReply(Messages::WebPageProxy::DrawFrameToSnapshot(frameID, rect, m_remoteSnapshotState->identifier), Ref { m_remoteSnapshotState->callback }->chain());
    m_remoteSnapshotState->recorder->drawSnapshotFrame(frameID);
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(rect);
    UNUSED_PARAM(context);
#endif
}

void WebPage::drawToSnapshot(const std::optional<FloatRect>& rect, bool allowTransparentBackground, RemoteSnapshotIdentifier snapshotIdentifier, CompletionHandler<void(std::optional<IntSize>)>&& completionHandler)
{
#if ENABLE(GPU_PROCESS)
    ASSERT(m_page->settings().remoteSnapshottingEnabled());

    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame) {
        completionHandler(std::nullopt);
        return;
    }

    Ref frameView = *localMainFrame->view();
    auto snapshotRect = IntRect { rect.value_or(FloatRect { { }, frameView->contentsSize() }) };
    auto snapshotSize = snapshotRect.size();

    Ref remoteRenderingBackend = ensureRemoteRenderingBackendProxy();
    m_remoteSnapshotState = {
        snapshotIdentifier,
        remoteRenderingBackend->createSnapshotRecorder(snapshotIdentifier),
        MainRunLoopSuccessCallbackAggregator::create([completionHandler = WTFMove(completionHandler), snapshotSize] (bool success) mutable {
            completionHandler(success ? std::optional<IntSize>(snapshotSize) : std::nullopt);
        })
    };

    drawMainFrameToPDF(*localMainFrame, m_remoteSnapshotState->recorder, snapshotRect, allowTransparentBackground);

    remoteRenderingBackend->sinkSnapshotRecorderIntoSnapshotFrame(WTFMove(m_remoteSnapshotState->recorder), localMainFrame->frameID(), Ref { m_remoteSnapshotState->callback }->chain());
    m_remoteSnapshotState = std::nullopt;
#else
    UNUSED_PARAM(rect);
    UNUSED_PARAM(allowTransparentBackground);
    UNUSED_PARAM(snapshotIdentifier);
    UNUSED_PARAM(completionHandler);
#endif
}

void WebPage::drawFrameToSnapshot(FrameIdentifier frameID, const IntRect& rect, RemoteSnapshotIdentifier snapshotIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(GPU_PROCESS)
    ASSERT(m_page->settings().siteIsolationEnabled());

    // FIXME: Error handling, so that the GPUP doesn't wait for something not coming.

    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame) {
        ASSERT_NOT_REACHED();
        completionHandler(false);
        return;
    }

    RefPtr coreLocalFrame = webFrame->coreLocalFrame();
    if (!coreLocalFrame) {
        ASSERT_NOT_REACHED();
        completionHandler(false);
        return;
    }

    RefPtr frameView = coreLocalFrame->view();
    if (!frameView) {
        completionHandler(false);
        return;
    }

    Ref remoteRenderingBackend = ensureRemoteRenderingBackendProxy();
    m_remoteSnapshotState = { snapshotIdentifier, remoteRenderingBackend->createSnapshotRecorder(snapshotIdentifier), MainRunLoopSuccessCallbackAggregator::create(WTFMove(completionHandler)) };

    LocalFrameView::SelectionInSnapshot shouldPaintSelection = LocalFrameView::IncludeSelection;
    LocalFrameView::CoordinateSpaceForSnapshot coordinateSpace = LocalFrameView::DocumentCoordinates;

    frameView->paintContentsForSnapshot(m_remoteSnapshotState->recorder, rect, shouldPaintSelection, coordinateSpace);

    remoteRenderingBackend->sinkSnapshotRecorderIntoSnapshotFrame(WTFMove(m_remoteSnapshotState->recorder), frameID, Ref { m_remoteSnapshotState->callback }->chain());

    m_remoteSnapshotState = std::nullopt;
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(rect);
    UNUSED_PARAM(snapshotIdentifier);
    UNUSED_PARAM(completionHandler);
#endif
}

void WebPage::drawMainFrameToPDF(LocalFrame& localMainFrame, GraphicsContext& context, IntRect& snapshotRect, bool allowTransparentBackground)
{
    Ref frameView = *localMainFrame.view();

    auto originalLayoutViewportOverrideRect = frameView->layoutViewportOverrideRect();
    frameView->setLayoutViewportOverrideRect(LayoutRect(snapshotRect));
    auto originalPaintBehavior = frameView->paintBehavior();

    frameView->setPaintBehavior(originalPaintBehavior | PaintBehavior::AnnotateLinks);

    auto originalColor = frameView->baseBackgroundColor();
    if (allowTransparentBackground) {
        frameView->setTransparent(true);
        frameView->setBaseBackgroundColor(Color::transparentBlack);
    }

    pdfSnapshotAtSize(localMainFrame, context, snapshotRect, { });

    if (allowTransparentBackground) {
        frameView->setTransparent(false);
        frameView->setBaseBackgroundColor(originalColor);
    }

    frameView->setLayoutViewportOverrideRect(originalLayoutViewportOverrideRect);
    frameView->setPaintBehavior(originalPaintBehavior);
}

void WebPage::pdfSnapshotAtSize(LocalFrame& localMainFrame, GraphicsContext& context, const IntRect& snapshotRect, SnapshotOptions options)
{
    Ref frameView = *localMainFrame.view();

    auto rect = snapshotRect;
    auto bitmapSize = rect.size();

    int64_t remainingHeight = bitmapSize.height();
    int64_t nextRectY = rect.y();
    while (remainingHeight > 0) {
        // PDFs have a per-page height limit of 200 inches at 72dpi.
        // We'll export one PDF page at a time, up to that maximum height.
        static const int64_t maxPageHeight = 72 * 200;
        bitmapSize.setHeight(std::min(remainingHeight, maxPageHeight));
        rect.setHeight(bitmapSize.height());
        rect.setY(nextRectY);

        context.beginPage(FloatRect { { }, bitmapSize });
        context.scale({ 1, -1 });
        context.translate(0, -bitmapSize.height());

        paintSnapshotAtSize(rect, bitmapSize, options, localMainFrame, frameView, context);

        context.endPage();

        nextRectY += bitmapSize.height();
        remainingHeight -= maxPageHeight;
    }
}

#if PLATFORM(GTK)
void WebPage::drawPagesForPrinting(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(std::optional<SharedMemory::Handle>&&, WebCore::ResourceError&&)>&& completionHandler)
{
    beginPrinting(frameID, printInfo);
    if (RefPtr printContext = m_printContext; printContext && m_printOperation) {
        m_printOperation->startPrint(printContext.get(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (RefPtr<WebCore::FragmentedSharedBuffer>&& data, WebCore::ResourceError&& error) mutable {
            m_printOperation = nullptr;
            std::optional<SharedMemory::Handle> ipcHandle;
            if (error.isNull()) {
                auto sharedMemory = SharedMemory::copyBuffer(*data);
                ipcHandle = sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
            }
            completionHandler(WTFMove(ipcHandle), WTFMove(error));
        });
        return;
    }
    completionHandler(std::nullopt, { });
}
#endif

void WebPage::addResourceRequest(WebCore::ResourceLoaderIdentifier identifier, const WebCore::ResourceRequest& request, const DocumentLoader* loader, LocalFrame* frame)
{
    bool isHTTPRequest = request.url().protocolIsInHTTPFamily();

    // Ignore main resource loads here, since they can start in one process and end up in another.
    // See 283102@main for an explanation for how we handle main resource loads. We also ignore
    // very low priority loads like ping and beacon requests to match previous PLT heuristics.
    // We consider file requests as part of the PLT network heuristic for ease of API testing.
    if (frame && request.requester() != ResourceRequestRequester::Main && request.priority() != WebCore::ResourceLoadPriority::VeryLow && (isHTTPRequest || request.url().protocolIsFile())) {
        auto frameID = frame->frameID();
        auto& identifiers = m_networkResourceRequestIdentifiersForPageLoadTiming.ensure(frameID, [] {
            return HashSet<WebCore::ResourceLoaderIdentifier> { };
        }).iterator->value;
        if (identifiers.isEmpty())
            send(Messages::WebPageProxy::StartNetworkRequestsForPageLoadTiming(frameID));
        identifiers.add(identifier);
    }

    if (!isHTTPRequest)
        return;

    if (m_mainFrameProgressCompleted && !UserGestureIndicator::processingUserGesture())
        return;

    ASSERT(!m_trackedNetworkResourceRequestIdentifiers.contains(identifier));
    bool wasEmpty = m_trackedNetworkResourceRequestIdentifiers.isEmpty();
    m_trackedNetworkResourceRequestIdentifiers.add(identifier);
    if (wasEmpty)
        send(Messages::WebPageProxy::SetNetworkRequestsInProgress(true));
}

void WebPage::removeResourceRequest(WebCore::ResourceLoaderIdentifier identifier, LocalFrame* frame)
{
    if (frame) {
        auto frameID = frame->frameID();
        if (auto it = m_networkResourceRequestIdentifiersForPageLoadTiming.find(frameID); it != m_networkResourceRequestIdentifiersForPageLoadTiming.end()) {
            if (it->value.remove(identifier) && it->value.isEmpty())
                send(Messages::WebPageProxy::EndNetworkRequestsForPageLoadTiming(frameID, WallTime::now()));
        }
    }

    if (!m_trackedNetworkResourceRequestIdentifiers.remove(identifier))
        return;

    if (m_trackedNetworkResourceRequestIdentifiers.isEmpty())
        send(Messages::WebPageProxy::SetNetworkRequestsInProgress(false));
}

void WebPage::setMediaVolume(float volume)
{
    protectedCorePage()->setMediaVolume(volume);
}

void WebPage::setMuted(MediaProducerMutedStateFlags state, CompletionHandler<void()>&& completionHandler)
{
    protectedCorePage()->setMuted(state);
    completionHandler();
}

void WebPage::stopMediaCapture(MediaProducerMediaCaptureKind kind, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    protectedCorePage()->stopMediaCapture(kind);
#endif
    completionHandler();
}

void WebPage::processWillSuspend()
{
    if (RefPtr manager = mediaSessionManagerIfExists())
        manager->processWillSuspend();
}

void WebPage::processDidResume()
{
    if (RefPtr manager = mediaSessionManagerIfExists())
        manager->processDidResume();
}

void WebPage::didReceiveRemoteCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (RefPtr manager = mediaSessionManagerIfExists())
        manager->processDidReceiveRemoteControlCommand(type, argument);
}

void WebPage::setMayStartMediaWhenInWindow(bool mayStartMedia)
{
    if (mayStartMedia == m_mayStartMediaWhenInWindow)
        return;

    m_mayStartMediaWhenInWindow = mayStartMedia;
    if (m_mayStartMediaWhenInWindow && m_page->isInWindow())
        m_setCanStartMediaTimer.startOneShot(0_s);
}

void WebPage::runModal()
{
    if (m_isClosed)
        return;
    if (m_isRunningModal)
        return;

    m_isRunningModal = true;
    send(Messages::WebPageProxy::RunModal());
#if ASSERT_ENABLED
    Ref<WebPage> protector(*this);
#endif
    RunLoop::run();
}

bool WebPage::canHandleRequest(const WebCore::ResourceRequest& request)
{
    if (LegacySchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(request.url().protocol()))
        return true;

    if (request.url().protocolIsBlob())
        return true;

    return platformCanHandleRequest(request);
}

void WebPage::setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length, bool suppressUnderline, const Vector<CompositionHighlight>& highlights, const HashMap<String, Vector<WebCore::CharacterRange>>& annotations)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    Ref editor = frame->editor();
    if (!editor->canEdit())
        return;

    Vector<CompositionUnderline> underlines;
    if (!suppressUnderline)
        underlines.append(CompositionUnderline(0, compositionString.length(), CompositionUnderlineColor::TextColor, Color(Color::black), false));

    editor->setComposition(compositionString, underlines, highlights, annotations, from, from + length);
}

bool WebPage::hasCompositionForTesting()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return false;

    return frame->editor().hasComposition();
}

void WebPage::confirmCompositionForTesting(const String& compositionString)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    Ref editor = frame->editor();
    if (!editor->canEdit())
        return;

    if (compositionString.isNull())
        editor->confirmComposition();
    editor->confirmComposition(compositionString);
}

void WebPage::wheelEventHandlersChanged(bool hasHandlers)
{
    if (m_hasWheelEventHandlers == hasHandlers)
        return;

    m_hasWheelEventHandlers = hasHandlers;
    recomputeShortCircuitHorizontalWheelEventsState();
}

static bool hasEnabledHorizontalScrollbar(ScrollableArea* scrollableArea)
{
    RefPtr scrollbar = scrollableArea->horizontalScrollbar();
    return scrollbar && scrollbar->enabled();
}

static bool pageContainsAnyHorizontalScrollbars(LocalFrame* mainFrame)
{
    if (!mainFrame)
        return false;

    if (RefPtr frameView = mainFrame->view()) {
        if (hasEnabledHorizontalScrollbar(frameView.get()))
            return true;
    }

    for (RefPtr<Frame> frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(*frame);
        if (!localFrame)
            continue;

        RefPtr frameView = localFrame->view();
        if (!frameView)
            continue;

        auto scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (CheckedRef scrollableArea : *scrollableAreas) {
            if (!scrollableArea->scrollbarsCanBeActive())
                continue;

            if (hasEnabledHorizontalScrollbar(scrollableArea.ptr()))
                return true;
        }
    }

    return false;
}

void WebPage::recomputeShortCircuitHorizontalWheelEventsState()
{
    bool canShortCircuitHorizontalWheelEvents = !m_hasWheelEventHandlers;

    if (canShortCircuitHorizontalWheelEvents) {
        // Check if we have any horizontal scroll bars on the page.
        if (pageContainsAnyHorizontalScrollbars(localMainFrame().get()))
            canShortCircuitHorizontalWheelEvents = false;
    }

    if (m_canShortCircuitHorizontalWheelEvents == canShortCircuitHorizontalWheelEvents)
        return;

    m_canShortCircuitHorizontalWheelEvents = canShortCircuitHorizontalWheelEvents;
    send(Messages::WebPageProxy::SetCanShortCircuitHorizontalWheelEvents(m_canShortCircuitHorizontalWheelEvents));
}

Frame* WebPage::mainFrame() const
{
    return m_page ? &m_page->mainFrame() : nullptr;
}

RefPtr<WebCore::LocalFrame> WebPage::localMainFrame() const
{
    if (RefPtr page = m_page)
        return page->localMainFrame();
    return nullptr;
}

RefPtr<WebCore::Document> WebPage::localTopDocument() const
{
    if (RefPtr page = m_page)
        return page->localTopDocument();
    return nullptr;
}

FrameView* WebPage::mainFrameView() const
{
    if (RefPtr frame = mainFrame())
        return frame->virtualView();
    return nullptr;
}

LocalFrameView* WebPage::localMainFrameView() const
{
    return dynamicDowncast<LocalFrameView>(mainFrameView());
}

CheckedPtr<WebCore::LocalFrameView> WebPage::checkedLocalMainFrameView() const
{
    return localMainFrameView();
}

bool WebPage::shouldUseCustomContentProviderForResponse(const ResourceResponse& response)
{
    auto& mimeType = response.mimeType();
    if (mimeType.isNull())
        return false;

    return m_mimeTypesWithCustomContentProviders.contains(mimeType);
}

#if PLATFORM(GTK) || PLATFORM(WPE)

static RefPtr<LocalFrame> targetFrameForEditing(WebPage& page)
{
    RefPtr targetFrame = page.corePage()->focusController().focusedOrMainFrame();
    if (!targetFrame)
        return nullptr;

    Editor& editor = targetFrame->editor();
    if (!editor.canEdit())
        return nullptr;

    if (editor.hasComposition()) {
        // We should verify the parent node of this IME composition node are
        // editable because JavaScript may delete a parent node of the composition
        // node. In this case, WebKit crashes while deleting texts from the parent
        // node, which doesn't exist any longer.
        if (auto range = editor.compositionRange()) {
            if (!range->startContainer().isContentEditable())
                return nullptr;
        }
    }
    return targetFrame;
}

void WebPage::cancelComposition(const String& compositionString)
{
    if (RefPtr targetFrame = targetFrameForEditing(*this))
        targetFrame->protectedEditor()->confirmComposition(compositionString);
}

void WebPage::deleteSurrounding(int64_t offset, unsigned characterCount)
{
    RefPtr targetFrame = targetFrameForEditing(*this);
    if (!targetFrame)
        return;

    auto& selection = targetFrame->selection().selection();
    if (selection.isNone())
        return;

    auto selectionStart = selection.visibleStart();
    auto surroundingRange = makeSimpleRange(startOfEditableContent(selectionStart), selectionStart);
    if (!surroundingRange)
        return;

    Ref rootNode = surroundingRange->start.container->treeScope().rootNode();
    auto characterRange = WebCore::CharacterRange(WebCore::characterCount(*surroundingRange) + offset, characterCount);
    auto selectionRange = resolveCharacterRange(makeRangeSelectingNodeContents(rootNode), characterRange);

    targetFrame->editor().setIgnoreSelectionChanges(true);
    targetFrame->checkedSelection()->setSelection(VisibleSelection(selectionRange));
    targetFrame->editor().deleteSelectionWithSmartDelete(false);
    targetFrame->editor().setIgnoreSelectionChanges(false);
    sendEditorStateUpdate();
}

#endif

void WebPage::didApplyStyle()
{
    sendEditorStateUpdate();
}

void WebPage::didChangeContents()
{
    sendEditorStateUpdate();
}

void WebPage::didScrollSelection()
{
    didChangeSelectionOrOverflowScrollPosition();
}

void WebPage::didChangeSelection(LocalFrame& frame)
{
    didChangeSelectionOrOverflowScrollPosition();

    if (m_userIsInteracting && frame.selection().isRange())
        m_userInteractionsSincePageTransition.add(UserInteractionFlag::SelectedRange);

#if ENABLE(WRITING_TOOLS)
    protectedCorePage()->updateStateForSelectedSuggestionIfNeeded();
#endif

#if PLATFORM(IOS_FAMILY)
    resetLastSelectedReplacementRangeIfNeeded();

    if (!std::exchange(m_sendAutocorrectionContextAfterFocusingElement, false))
        return;

    callOnMainRunLoop([protectedThis = Ref { *this }, frame = Ref { frame }] {
        if (!frame->document() || !frame->document()->hasLivingRenderTree() || frame->selection().isNone()) [[unlikely]]
            return;

        protectedThis->preemptivelySendAutocorrectionContext();
    });
#endif // PLATFORM(IOS_FAMILY)
}

void WebPage::didChangeSelectionOrOverflowScrollPosition()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    // The act of getting Dictionary Popup info can make selection changes that we should not propagate to the UIProcess.
    // Specifically, if there is a caret selection, it will change to a range selection of the word around the caret. And
    // then it will change back.
    if (frame->editor().isGettingDictionaryPopupInfo())
        return;

    // Similarly, we don't want to propagate changes to the web process when inserting text asynchronously, since we will
    // end up with a range selection very briefly right before inserting the text.
    if (m_isSelectingTextWhileInsertingAsynchronously)
        return;

#if HAVE(TOUCH_BAR)
    bool hasPreviouslyFocusedDueToUserInteraction = m_userInteractionsSincePageTransition.contains(UserInteractionFlag::FocusedElement);
    if (m_userIsInteracting && m_focusedElement)
        m_userInteractionsSincePageTransition.add(UserInteractionFlag::FocusedElement);

    if (!hasPreviouslyFocusedDueToUserInteraction && m_userInteractionsSincePageTransition.contains(UserInteractionFlag::FocusedElement)) {
        RefPtr document = frame->document();
        if (document->quirks().isTouchBarUpdateSuppressedForHiddenContentEditable()) {
            m_isTouchBarUpdateSuppressedForHiddenContentEditable = true;
            send(Messages::WebPageProxy::SetIsTouchBarUpdateSuppressedForHiddenContentEditable(m_isTouchBarUpdateSuppressedForHiddenContentEditable));
        }

        if (document->quirks().isNeverRichlyEditableForTouchBar()) {
            m_isNeverRichlyEditableForTouchBar = true;
            send(Messages::WebPageProxy::SetIsNeverRichlyEditableForTouchBar(m_isNeverRichlyEditableForTouchBar));
        }

        send(Messages::WebPageProxy::SetHasFocusedElementWithUserInteraction(true));
    }

    // Abandon the current inline input session if selection changed for any other reason but an input method direct action.
    // FIXME: This logic should be in WebCore.
    // FIXME: Many changes that affect composition node do not go through didChangeSelection(). We need to do something when DOM manipulation affects the composition, because otherwise input method's idea about it will be different from Editor's.
    // FIXME: We can't cancel composition when selection changes to NoSelection, but we probably should.
    Ref editor = frame->editor();
    if (editor->hasComposition() && !frame->editor().ignoreSelectionChanges() && !frame->selection().isNone()) {
        editor->cancelComposition();
        if (RefPtr document = frame->document())
            discardedComposition(*document);
        return;
    }
#endif // HAVE(TOUCH_BAR)

    scheduleFullEditorStateUpdate();
}

void WebPage::resetFocusedElementForFrame(WebFrame* frame)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (frame->isMainFrame() || corePage()->focusController().focusedOrMainFrame() == frame->coreLocalFrame())
        m_page->editorClient().setInputMethodState(nullptr);
#endif

    if (!m_focusedElement)
        return;

    if (frame->isMainFrame() || m_focusedElement->document().frame() == frame->coreLocalFrame()) {
#if PLATFORM(IOS_FAMILY)
        m_sendAutocorrectionContextAfterFocusingElement = false;
        send(Messages::WebPageProxy::ElementDidBlur());
#elif PLATFORM(MAC)
        send(Messages::WebPageProxy::SetEditableElementIsFocused(false));
#endif
        m_focusedElement = nullptr;
    }
}

void WebPage::elementDidRefocus(Element& element, const FocusOptions& options)
{
    elementDidFocus(element, options);

    if (m_userIsInteracting)
        scheduleFullEditorStateUpdate();
}

bool WebPage::shouldDispatchUpdateAfterFocusingElement(const Element& element) const
{
    if (m_focusedElement == &element || m_recentlyBlurredElement == &element) {
#if PLATFORM(IOS_FAMILY)
        return !m_isShowingInputViewForFocusedElement;
#else
        return false;
#endif
    }
    return true;
}

static bool isTextFormControlOrEditableContent(const WebCore::Element& element)
{
    return is<HTMLTextFormControlElement>(element) || element.hasEditableStyle();
}

#if PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)
static bool shouldExitFullscreenAfterFocusingElement(const WebCore::Element& element)
{
    if (!element.document().fullscreen().isFullscreen())
        return false;

    if (RefPtr input = dynamicDowncast<const HTMLInputElement>(element))
        return input->isTextField();

    return is<HTMLTextAreaElement>(element) || element.hasEditableStyle();
}
#endif

void WebPage::elementDidFocus(Element& element, const FocusOptions& options)
{
#if PLATFORM(IOS_FAMILY)
    m_updateFocusedElementInformationTimer.stop();
#endif

    if (!shouldDispatchUpdateAfterFocusingElement(element)) {
        updateInputContextAfterBlurringAndRefocusingElementIfNeeded(element);
        m_focusedElement = element;
        m_recentlyBlurredElement = nullptr;
        return;
    }

    if (is<HTMLSelectElement>(element) || isTextFormControlOrEditableContent(element)) {
#if PLATFORM(IOS_FAMILY)
        bool isChangingFocusedElement = m_focusedElement != &element;
#endif
        m_focusedElement = element;
        m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement = false;

#if PLATFORM(IOS_FAMILY)

#if ENABLE(FULLSCREEN_API)
    if (shouldExitFullscreenAfterFocusingElement(element))
        element.document().fullscreen().fullyExitFullscreen();
#endif
        if (isChangingFocusedElement && (m_userIsInteracting || m_keyboardIsAttached))
            m_sendAutocorrectionContextAfterFocusingElement = true;

        auto information = focusedElementInformation();
        if (!information)
            return;

        RefPtr<API::Object> userData;

        m_formClient->willBeginInputSession(this, &element, WebFrame::fromCoreFrame(*element.document().frame()).get(), m_userIsInteracting, userData);

        if (!userData) {
            auto userInfo = element.userInfo();
            if (!userInfo.isNull()) {
                if (auto data = JSON::Value::parseJSON(element.userInfo()))
                    userData = userDataFromJSONData(*data);
            }
        }

        information->preventScroll = options.preventScroll;
        send(Messages::WebPageProxy::ElementDidFocus(information.value(), m_userIsInteracting, m_recentlyBlurredElement, m_lastActivityStateChanges, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
#elif PLATFORM(MAC)
        // FIXME: This can be unified with the iOS code above by bringing ElementDidFocus to macOS.
        // This also doesn't take other noneditable controls into account, such as input type color.
        send(Messages::WebPageProxy::SetEditableElementIsFocused(!element.hasTagName(WebCore::HTMLNames::selectTag)));
#endif
        m_recentlyBlurredElement = nullptr;
    }
}

void WebPage::elementDidBlur(WebCore::Element& element)
{
    if (m_focusedElement == &element) {
        m_recentlyBlurredElement = WTFMove(m_focusedElement);
        callOnMainRunLoop([protectedThis = Ref { *this }] {
            if (protectedThis->m_recentlyBlurredElement) {
#if PLATFORM(IOS_FAMILY)
                protectedThis->send(Messages::WebPageProxy::ElementDidBlur());
#elif PLATFORM(MAC)
                protectedThis->send(Messages::WebPageProxy::SetEditableElementIsFocused(false));
#endif
            }
            protectedThis->m_recentlyBlurredElement = nullptr;
        });
        m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement = false;
#if PLATFORM(IOS_FAMILY)
        m_sendAutocorrectionContextAfterFocusingElement = false;
#endif
    }
}

void WebPage::focusedElementDidChangeInputMode(WebCore::Element& element, WebCore::InputMode mode)
{
    if (m_focusedElement != &element)
        return;

#if PLATFORM(IOS_FAMILY)
    ASSERT(is<HTMLElement>(element));
    ASSERT(downcast<HTMLElement>(element).canonicalInputMode() == mode);

    if (!isTextFormControlOrEditableContent(element))
        return;

    send(Messages::WebPageProxy::FocusedElementDidChangeInputMode(mode));
#else
    UNUSED_PARAM(mode);
#endif
}

void WebPage::focusedSelectElementDidChangeOptions(const WebCore::HTMLSelectElement& element)
{
#if PLATFORM(IOS_FAMILY)
    if (m_focusedElement != &element)
        return;

    m_updateFocusedElementInformationTimer.restart();
#else
    UNUSED_PARAM(element);
#endif
}

void WebPage::didUpdateComposition()
{
    sendEditorStateUpdate();
}

void WebPage::didEndUserTriggeredSelectionChanges()
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (!frame->editor().ignoreSelectionChanges())
        sendEditorStateUpdate();
}

void WebPage::discardedComposition(const Document& document)
{
    send(Messages::WebPageProxy::CompositionWasCanceled());
    if (!document.hasLivingRenderTree())
        return;

    sendEditorStateUpdate();
}

void WebPage::canceledComposition()
{
    send(Messages::WebPageProxy::CompositionWasCanceled());
    sendEditorStateUpdate();
}

void WebPage::navigateServiceWorkerClient(ScriptExecutionContextIdentifier documentIdentifier, const URL& url, CompletionHandler<void(WebCore::ScheduleLocationChangeResult)>&& callback)
{
    RefPtr document = Document::allDocumentsMap().get(documentIdentifier);
    if (!document) {
        callback(WebCore::ScheduleLocationChangeResult::Stopped);
        return;
    }
    document->navigateFromServiceWorker(url, WTFMove(callback));
}

void WebPage::setAlwaysShowsHorizontalScroller(bool alwaysShowsHorizontalScroller)
{
    if (alwaysShowsHorizontalScroller == m_alwaysShowsHorizontalScroller)
        return;

    m_alwaysShowsHorizontalScroller = alwaysShowsHorizontalScroller;

    RefPtr view = protectedCorePage()->protectedMainFrame()->virtualView();
    if (!alwaysShowsHorizontalScroller)
        view->setHorizontalScrollbarLock(false);
    view->setHorizontalScrollbarMode(alwaysShowsHorizontalScroller ? ScrollbarMode::AlwaysOn : m_mainFrameIsScrollable ? ScrollbarMode::Auto : ScrollbarMode::AlwaysOff, alwaysShowsHorizontalScroller || !m_mainFrameIsScrollable);
}

void WebPage::setAlwaysShowsVerticalScroller(bool alwaysShowsVerticalScroller)
{
    if (alwaysShowsVerticalScroller == m_alwaysShowsVerticalScroller)
        return;

    m_alwaysShowsVerticalScroller = alwaysShowsVerticalScroller;

    RefPtr view = protectedCorePage()->protectedMainFrame()->virtualView();
    if (!alwaysShowsVerticalScroller)
        view->setVerticalScrollbarLock(false);
    view->setVerticalScrollbarMode(alwaysShowsVerticalScroller ? ScrollbarMode::AlwaysOn : m_mainFrameIsScrollable ? ScrollbarMode::Auto : ScrollbarMode::AlwaysOff, alwaysShowsVerticalScroller || !m_mainFrameIsScrollable);
}

void WebPage::setMinimumSizeForAutoLayout(const IntSize& size)
{
    if (m_minimumSizeForAutoLayout == size)
        return;

    m_minimumSizeForAutoLayout = size;

    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame)
        return;

    RefPtr view = localMainFrame->view();
    if (size.width() <= 0) {
        view->enableFixedWidthAutoSizeMode(false, { });
        return;
    }

    view->enableFixedWidthAutoSizeMode(true, { size.width(), std::max(size.height(), 1) });
}

void WebPage::setSizeToContentAutoSizeMaximumSize(const IntSize& size)
{
    if (m_sizeToContentAutoSizeMaximumSize == size)
        return;

    m_sizeToContentAutoSizeMaximumSize = size;

    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame)
        return;

    RefPtr view = localMainFrame->view();
    if (size.width() <= 0 || size.height() <= 0) {
        view->enableSizeToContentAutoSizeMode(false, { });
        return;
    }

    view->enableSizeToContentAutoSizeMode(true, size);
}

void WebPage::setAutoSizingShouldExpandToViewHeight(bool shouldExpand)
{
    if (m_autoSizingShouldExpandToViewHeight == shouldExpand)
        return;

    m_autoSizingShouldExpandToViewHeight = shouldExpand;

    if (RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame()))
        localMainFrame->protectedView()->setAutoSizeFixedMinimumHeight(shouldExpand ? m_viewSize.height() : 0);
}

void WebPage::setViewportSizeForCSSViewportUnits(std::optional<WebCore::FloatSize> viewportSize)
{
    if (m_viewportSizeForCSSViewportUnits == viewportSize)
        return;

    m_viewportSizeForCSSViewportUnits = viewportSize;
    if (m_viewportSizeForCSSViewportUnits) {
        if (RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame()))
            localMainFrame->protectedView()->setSizeForCSSDefaultViewportUnits(*m_viewportSizeForCSSViewportUnits);
    }
}

bool WebPage::isIOSurfaceLosslessCompressionEnabled() const
{
    return m_page->settings().iOSurfaceLosslessCompressionEnabled();
}

bool WebPage::isSmartInsertDeleteEnabled()
{
    return m_page->settings().smartInsertDeleteEnabled();
}

void WebPage::setSmartInsertDeleteEnabled(bool enabled)
{
    if (m_page->settings().smartInsertDeleteEnabled() != enabled) {
        m_page->settings().setSmartInsertDeleteEnabled(enabled);
        setSelectTrailingWhitespaceEnabled(!enabled);
    }
}

bool WebPage::isWebTransportEnabled() const
{
    return m_page->settings().webTransportEnabled();
}

bool WebPage::isSelectTrailingWhitespaceEnabled() const
{
    return m_page->settings().selectTrailingWhitespaceEnabled();
}

void WebPage::setSelectTrailingWhitespaceEnabled(bool enabled)
{
    if (m_page->settings().selectTrailingWhitespaceEnabled() != enabled) {
        m_page->settings().setSelectTrailingWhitespaceEnabled(enabled);
        setSmartInsertDeleteEnabled(!enabled);
    }
}

bool WebPage::canShowResponse(const WebCore::ResourceResponse& response) const
{
    return canShowMIMEType(response.mimeType(), [&](auto& mimeType, auto allowedPlugins) {
        return protectedCorePage()->pluginData().supportsWebVisibleMimeTypeForURL(mimeType, allowedPlugins, response.url());
    });
}

bool WebPage::canShowMIMEType(const String& mimeType) const
{
    return canShowMIMEType(mimeType, [&](auto& mimeType, auto allowedPlugins) {
        return protectedCorePage()->pluginData().supportsWebVisibleMimeType(mimeType, allowedPlugins);
    });
}

bool WebPage::canShowMIMEType(const String& mimeType, NOESCAPE const Function<bool(const String&, PluginData::AllowedPluginTypes)>& pluginsSupport) const
{
    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

    if (!mimeType.isNull() && m_mimeTypesWithCustomContentProviders.contains(mimeType))
        return true;

    // We can use application plugins even if plugins aren't enabled.
    if (pluginsSupport(mimeType, PluginData::OnlyApplicationPlugins))
        return true;

#if ENABLE(PDFJS)
    if (m_page->settings().pdfJSViewerEnabled() && MIMETypeRegistry::isPDFMIMEType(mimeType))
        return true;
#endif

    return false;
}

void WebPage::addTextCheckingRequest(TextCheckerRequestID requestID, Ref<TextCheckingRequest>&& request)
{
    m_pendingTextCheckingRequestMap.add(requestID, WTFMove(request));
}

void WebPage::didFinishCheckingText(TextCheckerRequestID requestID, const Vector<TextCheckingResult>& result)
{
    RefPtr<TextCheckingRequest> request = m_pendingTextCheckingRequestMap.take(requestID);
    if (!request)
        return;

    request->didSucceed(result);
}

void WebPage::didCancelCheckingText(TextCheckerRequestID requestID)
{
    RefPtr<TextCheckingRequest> request = m_pendingTextCheckingRequestMap.take(requestID);
    if (!request)
        return;

    request->didCancel();
}

void WebPage::willReplaceMultipartContent(const WebFrame& frame)
{
#if PLATFORM(IOS_FAMILY)
    if (!frame.isMainFrame())
        return;

    m_previousExposedContentRect = protectedDrawingArea()->exposedContentRect();
#endif
}

void WebPage::didReplaceMultipartContent(const WebFrame& frame)
{
#if PLATFORM(IOS_FAMILY)
    if (!frame.isMainFrame())
        return;

    // Restore the previous exposed content rect so that it remains fixed when replacing content
    // from multipart/x-mixed-replace streams.
    protectedDrawingArea()->setExposedContentRect(m_previousExposedContentRect);
#endif
}

#if ENABLE(META_VIEWPORT)
static void setCanIgnoreViewportArgumentsToAvoidExcessiveZoomIfNeeded(ViewportConfiguration& configuration, LocalFrame* frame, bool shouldIgnoreMetaViewport)
{
    if (auto* document = frame ? frame->document() : nullptr; document && document->quirks().shouldIgnoreViewportArgumentsToAvoidExcessiveZoom())
        configuration.setCanIgnoreViewportArgumentsToAvoidExcessiveZoom(shouldIgnoreMetaViewport);
}

static void setCanIgnoreViewportArgumentsToAvoidEnlargedViewIfNeeded(ViewportConfiguration& configuration, LocalFrame* frame)
{
    if (auto* document = frame ? frame->document() : nullptr; document && document->quirks().shouldIgnoreViewportArgumentsToAvoidEnlargedView())
        configuration.setCanIgnoreViewportArgumentsToAvoidEnlargedView(true);
}
#endif

void WebPage::didCommitLoad(WebFrame* frame)
{
#if PLATFORM(IOS_FAMILY)
    auto firstTransactionIDAfterDidCommitLoad = downcast<RemoteLayerTreeDrawingArea>(*protectedDrawingArea()).nextTransactionID();
    frame->setFirstLayerTreeTransactionIDAfterDidCommitLoad(firstTransactionIDAfterDidCommitLoad);
    cancelPotentialTapInFrame(*frame);
#endif
    resetFocusedElementForFrame(frame);

    if (frame->isMainFrame())
        m_textManipulationIncludesSubframes = false;
    else if (m_textManipulationIncludesSubframes)
        startTextManipulationForFrame(*frame->protectedCoreLocalFrame());

    if (!frame->isRootFrame())
        return;

    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->sendEnterAcceleratedCompositingModeIfNeeded();

    ASSERT(!frame->coreLocalFrame()->loader().stateMachine().creatingInitialEmptyDocument());
    unfreezeLayerTree(LayerTreeFreezeReason::ProcessSwap);

#if ENABLE(IMAGE_ANALYSIS)
    for (auto& [element, completionHandlers] : m_elementsPendingTextRecognition) {
        for (auto& completionHandler : completionHandlers)
            completionHandler({ });
    }
    m_elementsPendingTextRecognition.clear();
#endif

    clearLoadedSubresourceDomains();

    // If previous URL is invalid, then it's not a real page that's being navigated away from.
    // Most likely, this is actually the first load to be committed in this page.
    if (frame->coreLocalFrame()->loader().previousURL().isValid())
        reportUsedFeatures();

    // Only restore the scale factor for standard frame loads (of the main frame).
    if (frame->coreLocalFrame()->loader().loadType() == FrameLoadType::Standard) {
        RefPtr page = frame->coreLocalFrame()->page();

#if PLATFORM(MAC)
        // As a very special case, we disable non-default layout modes in WKView for main-frame PluginDocuments.
        // Ideally we would only worry about this in WKView or the WKViewLayoutStrategies, but if we allow
        // a round-trip to the UI process, you'll see the wrong scale temporarily. So, we reset it here, and then
        // again later from the UI process.
        if (frame->coreLocalFrame()->document()->isPluginDocument()) {
            scaleView(1);
            setUseFixedLayout(false);
        }
#endif

        if (page && page->pageScaleFactor() != 1)
            scalePage(1, IntPoint());
    }

    // This timer can race with loading and clobber the scroll position saved on the current history item.
    m_pageScrolledHysteresis.cancel();

    m_didUpdateRenderingAfterCommittingLoad = false;

#if PLATFORM(IOS_FAMILY)
    if (auto scope = std::exchange(m_ignoreSelectionChangeScopeForDictation, nullptr))
        scope->invalidate();
    m_sendAutocorrectionContextAfterFocusingElement = false;
    m_hasReceivedVisibleContentRectsAfterDidCommitLoad = false;
    m_hasRestoredExposedContentRectAfterDidCommitLoad = false;
    m_internals->lastTransactionIDWithScaleChange = firstTransactionIDAfterDidCommitLoad;
    m_scaleWasSetByUIProcess = false;
    m_userHasChangedPageScaleFactor = false;
    m_estimatedLatency = Seconds(1.0 / 60);
    m_shouldRevealCurrentSelectionAfterInsertion = true;
    m_internals->lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage = std::nullopt;
    m_lastSelectedReplacementRange = { };
    m_bidiSelectionFlippingState = BidiSelectionFlippingState::NotFlipping;

    invokePendingSyntheticClickCallback(SyntheticClickResult::PageInvalid);

#if ENABLE(IOS_TOUCH_EVENTS)
    auto queuedEvents = makeUniqueRef<EventDispatcher::TouchEventQueue>();
    WebProcess::singleton().eventDispatcher().takeQueuedTouchEventsForPage(*this, queuedEvents);
    cancelAsynchronousTouchEvents(WTFMove(queuedEvents));
#endif
    m_lastTouchLocationBeforeTap = { };
    m_hasAnyActiveTouchPoints = false;
    m_activeTextInteractionSources = { };
#endif // PLATFORM(IOS_FAMILY)

    RefPtr coreFrame = frame->coreLocalFrame();
#if ENABLE(META_VIEWPORT)
    resetViewportDefaultConfiguration(frame);

    bool viewportChanged = false;

    setCanIgnoreViewportArgumentsToAvoidExcessiveZoomIfNeeded(m_viewportConfiguration, coreFrame.get(), shouldIgnoreMetaViewport());
    setCanIgnoreViewportArgumentsToAvoidEnlargedViewIfNeeded(m_viewportConfiguration, coreFrame.get());

    m_viewportConfiguration.setPrefersHorizontalScrollingBelowDesktopViewportWidths(shouldEnableViewportBehaviorsForResizableWindows());

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_identifier.toUInt64() << " didCommitLoad setting content size to " << coreFrame->view()->contentsSize());
    if (m_viewportConfiguration.setContentsSize(coreFrame->view()->contentsSize()))
        viewportChanged = true;

    if (m_viewportConfiguration.setViewportArguments(coreFrame->document()->viewportArguments()))
        viewportChanged = true;

    if (m_viewportConfiguration.setIsKnownToLayOutWiderThanViewport(false))
        viewportChanged = true;

    if (viewportChanged)
        viewportConfigurationChanged();
#endif // ENABLE(META_VIEWPORT)

#if ENABLE(TEXT_AUTOSIZING)
    m_textAutoSizingAdjustmentTimer.stop();
#endif

#if USE(OS_STATE)
    m_loadCommitTime = WallTime::now();
#endif

#if PLATFORM(IOS_FAMILY)
    m_updateLayoutViewportHeightExpansionTimer.stop();
    m_shouldRescheduleLayoutViewportHeightExpansionTimer = false;
#endif
    removeReasonsToDisallowLayoutViewportHeightExpansion(m_disallowLayoutViewportHeightExpansionReasons);

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (coreFrame->isMainFrame() && !usesEphemeralSession()) {
        if (RefPtr loader = coreFrame->protectedDocument()->loader(); loader
            && loader->advancedPrivacyProtections().contains(AdvancedPrivacyProtections::BaselineProtections))
            WEBPAGE_RELEASE_LOG(AdvancedPrivacyProtections, "didCommitLoad: advanced privacy protections enabled in non-ephemeral session");
    }
#endif

    themeColorChanged();

    m_lastNodeBeforeWritingSuggestions = { };

    WebProcess::singleton().updateActivePages(m_processDisplayName);

    updateMainFrameScrollOffsetPinning();

    updateMockAccessibilityElementAfterCommittingLoad();

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    m_elementsToExcludeFromRemoveBackground.clear();
#endif

#if USE(UICONTEXTMENU)
    m_hasActiveContextMenuInteraction = false;
#endif

    m_needsFixedContainerEdgesUpdate = true;

    flushDeferredDidReceiveMouseEvent();

    if (frame && frame->isMainFrame())
        m_networkResourceRequestIdentifiersForPageLoadTiming.clear();
}

void WebPage::didFinishDocumentLoad(WebFrame& frame)
{
    if (!frame.isMainFrame())
        return;

#if ENABLE(VIEWPORT_RESIZING)
    shrinkToFitContent(ZoomToInitialScale::Yes);
#endif
}

void WebPage::didFinishLoad(WebFrame& frame)
{
    if (!frame.isMainFrame())
        return;

    WebProcess::singleton().sendPrewarmInformation(frame.url());

#if ENABLE(VIEWPORT_RESIZING)
    shrinkToFitContent(ZoomToInitialScale::Yes);
#endif

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    spatialBackdropSourceChanged();
#endif
}

void WebPage::didSameDocumentNavigationForFrame(WebFrame& frame)
{
    RefPtr<API::Object> userData;

    auto navigationID = frame.coreLocalFrame()->loader().protectedDocumentLoader()->navigationID();

    if (frame.isMainFrame())
        m_pendingNavigationID = std::nullopt;

    // Notify the bundle client.
    injectedBundleLoaderClient().didSameDocumentNavigationForFrame(*this, frame, SameDocumentNavigationType::AnchorNavigation, userData);

    // Notify the UIProcess.
    send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(frame.frameID(), navigationID, SameDocumentNavigationType::AnchorNavigation, frame.coreLocalFrame()->document()->url(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

#if ENABLE(PDF_PLUGIN)
    for (Ref pluginView : m_pluginViews)
        pluginView->didSameDocumentNavigationForFrame(frame);
#endif
}

void WebPage::didNavigateWithinPageForFrame(WebFrame& frame)
{
    if (frame.isMainFrame())
        m_pendingNavigationID = std::nullopt;
}

void WebPage::testProcessIncomingSyncMessagesWhenWaitingForSyncReply(CompletionHandler<void(bool)>&& reply)
{
    RELEASE_ASSERT(IPC::UnboundedSynchronousIPCScope::hasOngoingUnboundedSyncIPC());
    reply(true);
}

std::optional<SimpleRange> WebPage::currentSelectionAsRange()
{
    RefPtr frame = frameWithSelection(m_page.get());
    if (!frame)
        return std::nullopt;

    return frame->selection().selection().toNormalizedRange();
}

void WebPage::reportUsedFeatures()
{
    Vector<String> namedFeatures;
    m_loaderClient->featuresUsedInPage(*this, namedFeatures);
}

void WebPage::sendEditorStateUpdate()
{
    m_needsEditorStateVisualDataUpdate = true;

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->editor().ignoreSelectionChanges() || !frame->document() || !frame->document()->hasLivingRenderTree())
        return;

    m_pendingEditorStateUpdateStatus = PendingEditorStateUpdateStatus::NotScheduled;

    // If we immediately dispatch an EditorState update to the UI process, layout may not be up to date yet.
    // If that is the case, just send what we have (i.e. don't include post-layout data) and wait until the
    // next layer tree commit to compute and send the complete EditorState over.
    auto state = editorState();
    send(Messages::WebPageProxy::EditorStateChanged(state));
    if (!state.hasPostLayoutData() && !shouldAvoidComputingPostLayoutDataForEditorState())
        scheduleFullEditorStateUpdate();
}

void WebPage::scheduleFullEditorStateUpdate()
{
    m_needsEditorStateVisualDataUpdate = true;

    if (hasPendingEditorStateUpdate()) {
        if (m_isChangingSelectionForAccessibility)
            m_pendingEditorStateUpdateStatus = PendingEditorStateUpdateStatus::ScheduledDuringAccessibilitySelectionChange;
        return;
    }

    if (m_isChangingSelectionForAccessibility)
        m_pendingEditorStateUpdateStatus = PendingEditorStateUpdateStatus::ScheduledDuringAccessibilitySelectionChange;
    else
        m_pendingEditorStateUpdateStatus = PendingEditorStateUpdateStatus::Scheduled;

    protectedCorePage()->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void WebPage::loadAndDecodeImage(WebCore::ResourceRequest&& request, std::optional<WebCore::FloatSize> sizeConstraint, uint64_t maximumBytesFromNetwork, CompletionHandler<void(Expected<Ref<WebCore::ShareableBitmap>, WebCore::ResourceError>&&)>&& completionHandler)
{
    URL url = request.url();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::LoadImageForDecoding(WTFMove(request), m_webPageProxyIdentifier, maximumBytesFromNetwork), [completionHandler = WTFMove(completionHandler), sizeConstraint, url] (Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&& result) mutable {
        if (!result)
            return completionHandler(makeUnexpected(WTFMove(result.error())));

        Ref bitmapImage = WebCore::BitmapImage::create(nullptr);
        bitmapImage->setData(result->ptr(), true);
        RefPtr nativeImage = bitmapImage->primaryNativeImage();
        if (!nativeImage)
            return completionHandler(makeUnexpected(decodeError(url)));

        FloatSize sourceSize = nativeImage->size();
        FloatSize destinationSize = sourceSize;
        if (sizeConstraint)
            destinationSize = largestRectWithAspectRatioInsideRect(sourceSize.aspectRatio(), FloatRect({ }, sizeConstraint->shrunkTo(sourceSize))).size();

        IntSize roundedDestinationSize = flooredIntSize(destinationSize);
        auto sourceColorSpace = nativeImage->colorSpace();
        auto destinationColorSpace = sourceColorSpace.supportsOutput() ? sourceColorSpace : DestinationColorSpace::SRGB();
        auto bitmap = ShareableBitmap::create({ roundedDestinationSize, destinationColorSpace });
        if (!bitmap)
            return completionHandler(makeUnexpected<ResourceError>({ }));

        auto context = bitmap->createGraphicsContext();
        if (!context)
            return completionHandler(makeUnexpected<ResourceError>({ }));

        context->drawNativeImage(*nativeImage, FloatRect({ }, roundedDestinationSize), FloatRect({ }, sourceSize), { CompositeOperator::Copy });

        completionHandler(bitmap.releaseNonNull());
    });
}

#if PLATFORM(MAC) || PLATFORM(WPE) || PLATFORM(GTK)
void WebPage::flushPendingThemeColorChange()
{
    if (!m_pendingThemeColorChange)
        return;

    m_pendingThemeColorChange = false;

    send(Messages::WebPageProxy::ThemeColorChanged(protectedCorePage()->themeColor()));
}
#endif

void WebPage::flushPendingPageExtendedBackgroundColorChange()
{
    if (!m_pendingPageExtendedBackgroundColorChange)
        return;

    m_pendingPageExtendedBackgroundColorChange = false;

    send(Messages::WebPageProxy::PageExtendedBackgroundColorDidChange(protectedCorePage()->pageExtendedBackgroundColor()));
}

void WebPage::flushPendingSampledPageTopColorChange()
{
    if (!m_pendingSampledPageTopColorChange)
        return;

    m_pendingSampledPageTopColorChange = false;

    send(Messages::WebPageProxy::SampledPageTopColorChanged(protectedCorePage()->sampledPageTopColor()));
}

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
void WebPage::spatialBackdropSourceChanged()
{
    RefPtr page = m_page;
    if (page->settings().webPageSpatialBackdropEnabled())
        send(Messages::WebPageProxy::SpatialBackdropSourceChanged(page->spatialBackdropSource()));
}
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
void WebPage::canEnterImmersiveElement(const Element& element, CompletionHandler<void(bool)>&& completion)
{
    auto url = element.document().url();
    sendWithAsyncReply(Messages::WebPageProxy::CanEnterImmersiveElementFromURL(url), WTFMove(completion));
}
#endif

void WebPage::flushPendingEditorStateUpdate()
{
    if (!hasPendingEditorStateUpdate())
        return;

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->editor().ignoreSelectionChanges())
        return;

    sendEditorStateUpdate();
}

void WebPage::updateWebsitePolicies(WebsitePoliciesData&& websitePolicies)
{
    RefPtr page = m_page;
    if (!page)
        return;

    if (auto* remoteMainFrameClient = m_mainFrame->remoteFrameClient()) {
        remoteMainFrameClient->applyWebsitePolicies(WTFMove(websitePolicies));
        return;
    }

    RefPtr localMainFrame = this->localMainFrame();
    RefPtr documentLoader = localMainFrame ? localMainFrame->loader().documentLoader() : nullptr;
    if (!documentLoader)
        return;

    m_allowsContentJavaScriptFromMostRecentNavigation = websitePolicies.allowsContentJavaScript;
    WebsitePoliciesData::applyToDocumentLoader(WTFMove(websitePolicies), *documentLoader);

#if ENABLE(VIDEO)
    page->updateMediaElementRateChangeRestrictions();
#endif

#if ENABLE(META_VIEWPORT)
    setCanIgnoreViewportArgumentsToAvoidExcessiveZoomIfNeeded(m_viewportConfiguration, localMainFrame.get(), shouldIgnoreMetaViewport());
    setCanIgnoreViewportArgumentsToAvoidEnlargedViewIfNeeded(m_viewportConfiguration, localMainFrame.get());
#endif
}

unsigned WebPage::extendIncrementalRenderingSuppression()
{
    unsigned token = m_maximumRenderingSuppressionToken + 1;
    while (!HashSet<unsigned>::isValidValue(token) || m_activeRenderingSuppressionTokens.contains(token))
        token++;

    m_activeRenderingSuppressionTokens.add(token);
    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->protectedView()->setVisualUpdatesAllowedByClient(false);

    m_maximumRenderingSuppressionToken = token;

    return token;
}

void WebPage::stopExtendingIncrementalRenderingSuppression(unsigned token)
{
    if (!m_activeRenderingSuppressionTokens.remove(token))
        return;

    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->protectedView()->setVisualUpdatesAllowedByClient(!shouldExtendIncrementalRenderingSuppression());
}

WebCore::ScrollPinningBehavior WebPage::scrollPinningBehavior()
{
    return m_internals->scrollPinningBehavior;
}

void WebPage::setScrollPinningBehavior(WebCore::ScrollPinningBehavior pinning)
{
    m_internals->scrollPinningBehavior = pinning;
    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->protectedView()->setScrollPinningBehavior(m_internals->scrollPinningBehavior);
}

void WebPage::setScrollbarOverlayStyle(std::optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle)
{
    m_scrollbarOverlayStyle = scrollbarStyle;

    if (RefPtr localMainFrame = this->localMainFrame())
        localMainFrame->protectedView()->recalculateScrollbarOverlayStyle();
}

Ref<DocumentLoader> WebPage::createDocumentLoader(LocalFrame& frame, ResourceRequest&& request, SubstituteData&& substituteData)
{
    auto documentLoader = DocumentLoader::create(WTFMove(request), WTFMove(substituteData));

    documentLoader->setLastNavigationWasAppInitiated(m_lastNavigationWasAppInitiated);

    if (frame.isMainFrame() || m_page->settings().siteIsolationEnabled()) {
        if (m_pendingNavigationID) {
            documentLoader->setNavigationID(*m_pendingNavigationID);
            m_pendingNavigationID = std::nullopt;
        }

        if (m_internals->pendingWebsitePolicies && frame.isMainFrame()) {
            m_allowsContentJavaScriptFromMostRecentNavigation = m_internals->pendingWebsitePolicies->allowsContentJavaScript;
            WebsitePoliciesData::applyToDocumentLoader(*std::exchange(m_internals->pendingWebsitePolicies, std::nullopt), documentLoader);
        }
    }

    return documentLoader;
}

void WebPage::updateCachedDocumentLoader(DocumentLoader& documentLoader, LocalFrame& frame)
{
    if (m_pendingNavigationID && frame.isMainFrame()) {
        documentLoader.setNavigationID(*m_pendingNavigationID);
        m_pendingNavigationID = std::nullopt;
    }
}

void WebPage::getBytecodeProfile(CompletionHandler<void(const String&)>&& callback)
{
    if (!commonVM().m_perBytecodeProfiler) [[likely]]
        return callback({ });

    String result = commonVM().m_perBytecodeProfiler->toJSON()->toJSONString();
    ASSERT(result.length());
    callback(result);
}

void WebPage::getSamplingProfilerOutput(CompletionHandler<void(const String&)>&& callback)
{
#if ENABLE(SAMPLING_PROFILER)
    RefPtr samplingProfiler = commonVM().samplingProfiler();
    if (!samplingProfiler)
        return callback({ });

    StringPrintStream result;
    samplingProfiler->reportTopFunctions(result);
    samplingProfiler->reportTopBytecodes(result);
    callback(result.toString());
#else
    callback({ });
#endif
}

void WebPage::didChangeScrollOffsetForFrame(LocalFrame& frame)
{
    if (!frame.isMainFrame())
        return;

    // If this is called when tearing down a FrameView, the WebCore::Frame's
    // current FrameView will be null.
    if (!frame.view())
        return;

    updateMainFrameScrollOffsetPinning();
}

void WebPage::postMessage(const String& messageName, API::Object* messageBody)
{
    send(Messages::WebPageProxy::HandleMessage(messageName, UserData(WebProcess::singleton().transformObjectsToHandles(messageBody))));
}

void WebPage::postMessageIgnoringFullySynchronousMode(const String& messageName, API::Object* messageBody)
{
    send(Messages::WebPageProxy::HandleMessage(messageName, UserData(WebProcess::singleton().transformObjectsToHandles(messageBody))), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void WebPage::postSynchronousMessageForTesting(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    auto& webProcess = WebProcess::singleton();

    auto sendResult = sendSync(Messages::WebPageProxy::HandleSynchronousMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))), Seconds::infinity(), IPC::SendSyncOption::UseFullySynchronousModeForTesting);
    if (sendResult.succeeded()) {
        auto& [returnUserData] = sendResult.reply();
        returnData = webProcess.transformHandlesToObjects(returnUserData.protectedObject().get());
    } else
        returnData = nullptr;
}

void WebPage::setShouldScaleViewToFitDocument(bool shouldScaleViewToFitDocument)
{
    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->setShouldScaleViewToFitDocument(shouldScaleViewToFitDocument);
}

void WebPage::imageOrMediaDocumentSizeChanged(const IntSize& newSize)
{
    send(Messages::WebPageProxy::ImageOrMediaDocumentSizeChanged(newSize));
}

void WebPage::addUserScript(String&& source, InjectedBundleScriptWorld& world, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserScriptInjectionTime injectionTime, WebCore::UserContentMatchParentFrame matchParentFrame)
{
    WebCore::UserScript userScript { WTFMove(source), URL(aboutBlankURL()), Vector<String>(), Vector<String>(), injectionTime, injectedFrames, matchParentFrame };

    Ref { m_userContentController }->addUserScript(world, WTFMove(userScript));
}

void WebPage::addUserStyleSheet(const String& source, WebCore::UserContentInjectedFrames injectedFrames)
{
    WebCore::UserStyleSheet userStyleSheet { source, aboutBlankURL(), Vector<String>(), Vector<String>(), injectedFrames };

    Ref { m_userContentController }->addUserStyleSheet(InjectedBundleScriptWorld::normalWorldSingleton(), WTFMove(userStyleSheet));
}

void WebPage::removeAllUserContent()
{
    Ref { m_userContentController }->removeAllUserContent();
}

void WebPage::updateIntrinsicContentSizeIfNeeded(const WebCore::IntSize& size)
{
    m_pendingIntrinsicContentSize = std::nullopt;
    if (!minimumSizeForAutoLayout().width() && !sizeToContentAutoSizeMaximumSize().width() && !sizeToContentAutoSizeMaximumSize().height())
        return;
    ASSERT(localMainFrameView());
    ASSERT(localMainFrameView()->isFixedWidthAutoSizeEnabled() || localMainFrameView()->isSizeToContentAutoSizeEnabled());
    ASSERT(!localMainFrameView()->needsLayout());
    if (m_lastSentIntrinsicContentSize == size)
        return;
    m_lastSentIntrinsicContentSize = size;
    send(Messages::WebPageProxy::DidChangeIntrinsicContentSize(size));
}

void WebPage::flushPendingIntrinsicContentSizeUpdate()
{
    if (auto pendingSize = std::exchange(m_pendingIntrinsicContentSize, std::nullopt))
        updateIntrinsicContentSizeIfNeeded(*pendingSize);
}

void WebPage::scheduleIntrinsicContentSizeUpdate(const IntSize& size)
{
    if (!minimumSizeForAutoLayout().width() && !sizeToContentAutoSizeMaximumSize().width() && !sizeToContentAutoSizeMaximumSize().height())
        return;
    ASSERT(localMainFrameView());
    ASSERT(localMainFrameView()->isFixedWidthAutoSizeEnabled() || localMainFrameView()->isSizeToContentAutoSizeEnabled());
    ASSERT(!localMainFrameView()->needsLayout());
    m_pendingIntrinsicContentSize = size;
}

void WebPage::dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> milestones)
{
    RefPtr<API::Object> userData;
    injectedBundleLoaderClient().didReachLayoutMilestone(*this, milestones, userData);

    // Clients should not set userData for this message, and it won't be passed through.
    ASSERT(!userData);

    // The drawing area might want to defer dispatch of didLayout to the UI process.
    if (RefPtr drawingArea = m_drawingArea) {
        static auto paintMilestones = OptionSet<WebCore::LayoutMilestone> { WebCore::LayoutMilestone::DidHitRelevantRepaintedObjectsAreaThreshold, WebCore::LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering, WebCore::LayoutMilestone::DidRenderSignificantAmountOfText, WebCore::LayoutMilestone::DidFirstMeaningfulPaint };
        auto drawingAreaRelatedMilestones = milestones & paintMilestones;
        if (drawingAreaRelatedMilestones && drawingArea->addMilestonesToDispatch(drawingAreaRelatedMilestones))
            milestones.remove(drawingAreaRelatedMilestones);
    }
    if (milestones.contains(WebCore::LayoutMilestone::DidFirstLayout) && localMainFrameView()) {
        // Ensure we never send DidFirstLayout milestone without updating the intrinsic size.
        updateIntrinsicContentSizeIfNeeded(localMainFrameView()->autoSizingIntrinsicContentSize());
    }

    send(Messages::WebPageProxy::DidReachLayoutMilestone(milestones, WallTime::now()));
}

void WebPage::didRestoreScrollPosition()
{
    send(Messages::WebPageProxy::DidRestoreScrollPosition());
}

void WebPage::setUserInterfaceLayoutDirection(uint32_t direction)
{
    m_userInterfaceLayoutDirection = static_cast<WebCore::UserInterfaceLayoutDirection>(direction);
    protectedCorePage()->setUserInterfaceLayoutDirection(m_userInterfaceLayoutDirection);
}

#if ENABLE(GAMEPAD)

void WebPage::gamepadActivity(const Vector<std::optional<GamepadData>>& gamepadDatas, EventMakesGamepadsVisible eventVisibilty)
{
    WebGamepadProvider::singleton().gamepadActivity(gamepadDatas, eventVisibilty);
}

void WebPage::gamepadsRecentlyAccessed()
{
    send(Messages::WebPageProxy::GamepadsRecentlyAccessed());
}

#if PLATFORM(VISION)
void WebPage::allowGamepadAccess()
{
    corePage()->allowGamepadAccess();
}
#endif

#endif // ENABLE(GAMEPAD)

#if ENABLE(POINTER_LOCK)
void WebPage::didAcquirePointerLock()
{
    corePage()->pointerLockController().didAcquirePointerLock();
}

void WebPage::didNotAcquirePointerLock()
{
    corePage()->pointerLockController().didNotAcquirePointerLock();
}

void WebPage::didLosePointerLock()
{
    corePage()->pointerLockController().didLosePointerLock();
}
#endif

void WebPage::didGetLoadDecisionForIcon(bool decision, CallbackID loadIdentifier, CompletionHandler<void(const IPC::SharedBufferReference&)>&& completionHandler)
{
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame)
        return completionHandler({ });
    RefPtr documentLoader = localMainFrame->loader().documentLoader();
    if (!documentLoader)
        return completionHandler({ });

    documentLoader->didGetLoadDecisionForIcon(decision, loadIdentifier.toInteger(), [completionHandler = WTFMove(completionHandler)] (WebCore::FragmentedSharedBuffer* iconData) mutable {
        completionHandler(IPC::SharedBufferReference(RefPtr { iconData }));
    });
}

WebURLSchemeHandlerProxy* WebPage::urlSchemeHandlerForScheme(StringView scheme)
{
    return m_schemeToURLSchemeHandlerProxyMap.get<StringViewHashTranslator>(scheme);
}

void WebPage::stopAllURLSchemeTasks()
{
    HashSet<RefPtr<WebURLSchemeHandlerProxy>> handlers;
    for (auto& handler : m_schemeToURLSchemeHandlerProxyMap.values())
        handlers.add(handler.get());

    for (auto& handler : handlers)
        handler->stopAllTasks();
}

void WebPage::registerURLSchemeHandler(WebURLSchemeHandlerIdentifier handlerIdentifier, const String& scheme)
{
    WEBPAGE_RELEASE_LOG(Process, "registerURLSchemeHandler: Registered handler %" PRIu64 " for the '%s' scheme", handlerIdentifier.toUInt64(), scheme.utf8().data());
    WebCore::LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(scheme);
    WebCore::LegacySchemeRegistry::registerURLSchemeAsCORSEnabled(scheme);
    auto schemeResult = m_schemeToURLSchemeHandlerProxyMap.add(scheme, WebURLSchemeHandlerProxy::create(*this, handlerIdentifier));
    m_identifierToURLSchemeHandlerProxyMap.add(handlerIdentifier, Ref { *schemeResult.iterator->value }.get());
}

void WebPage::urlSchemeTaskWillPerformRedirection(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, ResourceResponse&& response, ResourceRequest&& request, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    RefPtr handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    auto actualNewRequest = request;
    handler->taskDidPerformRedirection(taskIdentifier, WTFMove(response), WTFMove(request), WTFMove(completionHandler));
}

void WebPage::urlSchemeTaskDidPerformRedirection(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, ResourceResponse&& response, ResourceRequest&& request)
{
    RefPtr handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidPerformRedirection(taskIdentifier, WTFMove(response), WTFMove(request), [] (ResourceRequest&&) {});
}

void WebPage::urlSchemeTaskDidReceiveResponse(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, ResourceResponse&& response)
{
    RefPtr handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidReceiveResponse(taskIdentifier, WTFMove(response));
}

void WebPage::urlSchemeTaskDidReceiveData(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, Ref<WebCore::SharedBuffer>&& data)
{
    RefPtr handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidReceiveData(taskIdentifier, WTFMove(data));
}

void WebPage::urlSchemeTaskDidComplete(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, const ResourceError& error)
{
    RefPtr handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidComplete(taskIdentifier, error);
}

void WebPage::setIsSuspended(bool suspended, CompletionHandler<void(std::optional<bool>)>&& completionHandler)
{
    if (m_isSuspended == suspended)
        return completionHandler({ });

    m_isSuspended = suspended;

    if (!suspended)
        return completionHandler({ });

    // Unfrozen on drawing area reset.
    freezeLayerTree(LayerTreeFreezeReason::PageSuspended);

    // Only the committed WebPage gets application visibility notifications from the UIProcess, so make sure
    // we don't hold a BackgroundApplication freeze reason when transitioning from committed to suspended.
    unfreezeLayerTree(LayerTreeFreezeReason::BackgroundApplication);

    WebProcess::singleton().sendPrewarmInformation(m_mainFrame->url());

    suspendForProcessSwap(WTFMove(completionHandler));
}

void WebPage::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebFrame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    if (hasPageLevelStorageAccess(topFrameDomain, subFrameDomain)) {
        completionHandler(true);
        return;
    }

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::HasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frame.frameID(), m_identifier), WTFMove(completionHandler));
}

void WebPage::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebFrame& frame, StorageAccessScope scope, HasOrShouldIgnoreUserGesture hasOrShouldIgnoreUserGesture, CompletionHandler<void(WebCore::RequestStorageAccessResult)>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::RequestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frame.frameID(), m_identifier, m_webPageProxyIdentifier, scope, hasOrShouldIgnoreUserGesture), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), frame = Ref { frame }, pageID = m_identifier, frameID = frame.frameID()](RequestStorageAccessResult result) mutable {
        if (result.wasGranted == StorageAccessWasGranted::Yes) {
            switch (result.scope) {
            case StorageAccessScope::PerFrame:
                frame->protectedLocalFrameLoaderClient()->setHasFrameSpecificStorageAccess({ frameID, pageID });
                break;
            case StorageAccessScope::PerPage:
                addDomainWithPageLevelStorageAccess(result.topFrameDomain, result.subFrameDomain);
                break;
            }
        }
        completionHandler(result);
    });
}

void WebPage::setLoginStatus(RegistrableDomain&& domain, IsLoggedIn loggedInStatus, CompletionHandler<void()>&& completionHandler)
{
    RefPtr page = corePage();
    if (!page)
        return completionHandler();
    auto lastAuthentication = page->lastAuthentication() ? std::optional(*page->lastAuthentication()) : std::nullopt;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::SetLoginStatus(WTFMove(domain), loggedInStatus, lastAuthentication), WTFMove(completionHandler));
}

void WebPage::isLoggedIn(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::IsLoggedIn(WTFMove(domain)), WTFMove(completionHandler));
}

void WebPage::addDomainWithPageLevelStorageAccess(const RegistrableDomain& topLevelDomain, const RegistrableDomain& resourceDomain)
{
    m_internals->domainsWithPageLevelStorageAccess.add(topLevelDomain, HashSet<RegistrableDomain> { }).iterator->value.add(resourceDomain);

    // Some sites have quirks where multiple login domains require storage access.
    if (auto additionalLoginDomain = NetworkStorageSession::findAdditionalLoginDomain(topLevelDomain, resourceDomain))
        m_internals->domainsWithPageLevelStorageAccess.add(topLevelDomain, HashSet<RegistrableDomain> { }).iterator->value.add(*additionalLoginDomain);
}

bool WebPage::hasPageLevelStorageAccess(const RegistrableDomain& topLevelDomain, const RegistrableDomain& resourceDomain) const
{
    auto it = m_internals->domainsWithPageLevelStorageAccess.find(topLevelDomain);
    return it != m_internals->domainsWithPageLevelStorageAccess.end() && it->value.contains(resourceDomain);
}

void WebPage::clearPageLevelStorageAccess()
{
    m_internals->domainsWithPageLevelStorageAccess.clear();
}

void WebPage::revokeFrameSpecificStorageAccess()
{
    for (RefPtr frame = m_mainFrame->coreFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* client = dynamicDowncast<WebLocalFrameLoaderClient>(localFrame->loader().client()))
            client->revokeFrameSpecificStorageAccess();
    }
}

void WebPage::wasLoadedWithDataTransferFromPrevalentResource()
{
    if (RefPtr localTopDocument = this->localTopDocument())
        localTopDocument->wasLoadedWithDataTransferFromPrevalentResource();
}

void WebPage::didLoadFromRegistrableDomain(RegistrableDomain&& targetDomain)
{
    if (targetDomain != RegistrableDomain(m_mainFrame->url()))
        m_internals->loadedSubresourceDomains.add(targetDomain);
}

void WebPage::getLoadedSubresourceDomains(CompletionHandler<void(Vector<RegistrableDomain>)>&& completionHandler)
{
    completionHandler(copyToVector(m_internals->loadedSubresourceDomains));
}

void WebPage::clearLoadedSubresourceDomains()
{
    m_internals->loadedSubresourceDomains.clear();
}

const HashSet<WebCore::RegistrableDomain>& WebPage::loadedSubresourceDomains() const
{
    return m_internals->loadedSubresourceDomains;
}

#if ENABLE(DEVICE_ORIENTATION)
void WebPage::shouldAllowDeviceOrientationAndMotionAccess(FrameIdentifier frameID, FrameInfoData&& frameInfo, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShouldAllowDeviceOrientationAndMotionAccess(frameID, WTFMove(frameInfo), mayPrompt), WTFMove(completionHandler));
}
#endif

void WebPage::showShareSheet(ShareDataWithParsedURL&& shareData, WTF::CompletionHandler<void(bool)>&& callback)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShowShareSheet(WTFMove(shareData)), WTFMove(callback));
}

void WebPage::showContactPicker(WebCore::ContactsRequestData&& requestData, CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShowContactPicker(WTFMove(requestData)), WTFMove(callback));
}

#if HAVE(DIGITAL_CREDENTIALS_UI)
void WebPage::showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData& requestData, CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShowDigitalCredentialsPicker(requestData), WTFMove(completionHandler));
}

void WebPage::dismissDigitalCredentialsPicker(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::DismissDigitalCredentialsPicker(), WTFMove(completionHandler));
}
#endif

WebCore::DOMPasteAccessResponse WebPage::requestDOMPasteAccess(DOMPasteAccessCategory pasteAccessCategory, FrameIdentifier frameID, const String& originIdentifier)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: Computing and sending an autocorrection context is a workaround for the fact that autocorrection context
    // requests on iOS are currently synchronous in the web process. This allows us to immediately fulfill pending
    // autocorrection context requests in the UI process on iOS before handling the DOM paste request. This workaround
    // should be removed once <rdar://problem/16207002> is resolved.
    preemptivelySendAutocorrectionContext();
#endif

    AXRelayProcessSuspendedNotification(*this);

    auto sendResult = sendSyncWithDelayedReply(Messages::WebPageProxy::RequestDOMPasteAccess(pasteAccessCategory, frameID, rectForElementAtInteractionLocation(), originIdentifier));
    auto [response] = sendResult.takeReplyOr(WebCore::DOMPasteAccessResponse::DeniedForGesture);
    return response;
}

void WebPage::simulateDeviceOrientationChange(double alpha, double beta, double gamma)
{
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    if (RefPtr localTopDocument = this->localTopDocument())
        localTopDocument->simulateDeviceOrientationChange(alpha, beta, gamma);
#endif
}

#if USE(SYSTEM_PREVIEW)
void WebPage::systemPreviewActionTriggered(WebCore::SystemPreviewInfo previewInfo, const String& message)
{
    auto* document = Document::allDocumentsMap().get(*previewInfo.element.documentIdentifier);
    if (!document)
        return;

    auto pageID = document->pageID();
    if (!pageID || previewInfo.element.webPageIdentifier != pageID.value())
        return;

    document->dispatchSystemPreviewActionEvent(previewInfo, message);
}
#endif

#if ENABLE(SPEECH_SYNTHESIS)
void WebPage::speakingErrorOccurred()
{
    if (auto observer = protectedCorePage()->speechSynthesisClient()->observer())
        observer->speakingErrorOccurred();
}

void WebPage::boundaryEventOccurred(bool wordBoundary, unsigned charIndex, unsigned charLength)
{
    if (auto observer = protectedCorePage()->speechSynthesisClient()->observer())
        observer->boundaryEventOccurred(wordBoundary, charIndex, charLength);
}

void WebPage::voicesDidChange()
{
    if (auto observer = protectedCorePage()->speechSynthesisClient()->observer())
        observer->voicesChanged();
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPage::insertAttachment(const String& identifier, std::optional<uint64_t>&& fileSize, const String& fileName, const String& contentType, CompletionHandler<void()>&& callback)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return callback();

    frame->protectedEditor()->insertAttachment(identifier, WTFMove(fileSize), AtomString { fileName }, AtomString { contentType });
    callback();
}

void WebPage::updateAttachmentAttributes(const String& identifier, std::optional<uint64_t>&& fileSize, const String& contentType, const String& fileName, const IPC::SharedBufferReference& associatedElementData, CompletionHandler<void()>&& callback)
{
    if (RefPtr attachment = attachmentElementWithIdentifier(identifier)) {
        attachment->protectedDocument()->updateLayout();
        attachment->updateAttributes(WTFMove(fileSize), AtomString { contentType }, AtomString { fileName });
        attachment->updateAssociatedElementWithData(contentType, associatedElementData.isNull() ? WebCore::SharedBuffer::create() : associatedElementData.unsafeBuffer().releaseNonNull());
    }
    callback();
}

void WebPage::updateAttachmentIcon(const String& identifier, std::optional<ShareableBitmap::Handle>&& iconHandle, const WebCore::FloatSize& size)
{
    if (RefPtr attachment = attachmentElementWithIdentifier(identifier)) {
        if (auto icon = iconHandle ? ShareableBitmap::create(WTFMove(*iconHandle)) : nullptr) {
            if (attachment->isWideLayout()) {
                if (auto imageBuffer = ImageBuffer::create(icon->size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.0, DestinationColorSpace::SRGB(), PixelFormat::BGRA8)) {
                    icon->paint(imageBuffer->context(), IntPoint::zero(), IntRect(IntPoint::zero(), icon->size()));
                    auto data = imageBuffer->toData("image/png"_s);
                    attachment->updateIconForWideLayout(WTFMove(data));
                    return;
                }
            } else {
                attachment->updateIconForNarrowLayout(icon->createImage(), size);
                return;
            }
        }

        if (attachment->isWideLayout())
            attachment->updateIconForWideLayout({ });
        else
            attachment->updateIconForNarrowLayout({ }, size);
    }
}

void WebPage::requestAttachmentIcon(const String& identifier, const WebCore::FloatSize& size)
{
    if (RefPtr attachment = attachmentElementWithIdentifier(identifier)) {
        String fileName;
        if (RefPtr file = attachment->file())
            fileName = file->path();
        send(Messages::WebPageProxy::RequestAttachmentIcon(identifier, attachment->attachmentType(), fileName, attachment->attachmentTitle(), size));
    }
}

RefPtr<HTMLAttachmentElement> WebPage::attachmentElementWithIdentifier(const String& identifier) const
{
    // FIXME: Handle attachment elements in subframes too as well.
    if (RefPtr localTopDocument = this->localTopDocument())
        return localTopDocument->attachmentForIdentifier(identifier);

    return nullptr;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(APPLICATION_MANIFEST)

void WebPage::getApplicationManifest(CompletionHandler<void(const std::optional<WebCore::ApplicationManifest>&)>&& completionHandler)
{
    RefPtr mainFrameDocument = m_mainFrame->coreLocalFrame()->document();
    RefPtr loader = mainFrameDocument ? mainFrameDocument->loader() : nullptr;
    if (!loader)
        return completionHandler(std::nullopt);

    loader->loadApplicationManifest(WTFMove(completionHandler));
}

#endif // ENABLE(APPLICATION_MANIFEST)

void WebPage::getTextFragmentMatch(CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!m_mainFrame->coreLocalFrame()) {
        completionHandler({ });
        return;
    }

    RefPtr document = m_mainFrame->coreLocalFrame()->document();
    if (!document) {
        completionHandler({ });
        return;
    }

    auto fragmentDirective = document->fragmentDirective();
    if (fragmentDirective.isEmpty()) {
        completionHandler({ });
        return;
    }
    FragmentDirectiveParser fragmentDirectiveParser(fragmentDirective);
    if (!fragmentDirectiveParser.isValid()) {
        completionHandler({ });
        return;
    }

    auto parsedTextDirectives = fragmentDirectiveParser.parsedTextDirectives();
    auto highlightRanges = FragmentDirectiveRangeFinder::findRangesFromTextDirectives(parsedTextDirectives, *document);
    if (highlightRanges.isEmpty()) {
        completionHandler({ });
        return;
    }

    completionHandler(plainText(highlightRanges.first()));
}

void WebPage::updateCurrentModifierState(OptionSet<PlatformEvent::Modifier> modifiers)
{
    PlatformKeyboardEvent::setCurrentModifierState(modifiers);
}

#if !PLATFORM(IOS_FAMILY)

WebCore::IntRect WebPage::rectForElementAtInteractionLocation() const
{
    return { };
}

void WebPage::updateInputContextAfterBlurringAndRefocusingElementIfNeeded(Element&)
{
}

#endif // !PLATFORM(IOS_FAMILY)

void WebPage::setCanShowPlaceholder(const WebCore::ElementContext& elementContext, bool canShowPlaceholder)
{
    RefPtr<Element> element = elementForContext(elementContext);
    if (RefPtr textFormControl = dynamicDowncast<HTMLTextFormControlElement>(element))
        textFormControl->setCanShowPlaceholder(canShowPlaceholder);
}

RefPtr<Element> WebPage::elementForContext(const ElementContext& elementContext) const
{
    if (elementContext.webPageIdentifier != m_identifier)
        return nullptr;

    RefPtr element = elementContext.nodeIdentifier ? dynamicDowncast<Element>(Node::fromIdentifier(*elementContext.nodeIdentifier)) : nullptr;
    if (!element)
        return nullptr;

    if (!element->isConnected() || element->document().identifier() != elementContext.documentIdentifier || element->document().page() != m_page.get())
        return nullptr;

    return element;
}

std::optional<WebCore::ElementContext> WebPage::contextForElement(const WebCore::Element& element) const
{
    Ref document = element.document();
    if (!m_page || document->page() != m_page.get())
        return std::nullopt;

    RefPtr frame = document->frame();
    if (!frame)
        return std::nullopt;

    return WebCore::ElementContext { element.boundingBoxInRootViewCoordinates(), m_identifier, document->identifier(), element.nodeIdentifier() };
}

void WebPage::startTextManipulations(Vector<WebCore::TextManipulationController::ExclusionRule>&& exclusionRules, bool includeSubframes, CompletionHandler<void()>&& completionHandler)
{
    if (!m_page)
        return completionHandler();

    m_internals->textManipulationExclusionRules = WTFMove(exclusionRules);
    m_textManipulationIncludesSubframes = includeSubframes;
    if (m_textManipulationIncludesSubframes) {
        for (RefPtr<Frame> frame = m_mainFrame->coreFrame(); frame; frame = frame->tree().traverseNext())
            startTextManipulationForFrame(*frame);
    } else if (RefPtr frame = m_mainFrame->coreLocalFrame())
        startTextManipulationForFrame(*frame);

    // For now, we assume startObservingParagraphs find all paragraphs synchronously at once.
    completionHandler();
}

void WebPage::startTextManipulationForFrame(WebCore::Frame& frame)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
    RefPtr document = localFrame ? localFrame->document() : nullptr;
    if (!document || document->textManipulationControllerIfExists())
        return;

    auto exclusionRules = *m_internals->textManipulationExclusionRules;
    document->checkedTextManipulationController()->startObservingParagraphs([webPage = WeakPtr { *this }] (Document& document, const Vector<WebCore::TextManipulationItem>& items) {
        RefPtr frame = document.frame();
        if (!webPage || !frame)
            return;

        RefPtr webFrame = WebFrame::fromCoreFrame(*frame);
        if (!webFrame)
            return;

        webPage->send(Messages::WebPageProxy::DidFindTextManipulationItems(items));
    }, WTFMove(exclusionRules));
}

void WebPage::completeTextManipulation(const Vector<WebCore::TextManipulationItem>& items,
    CompletionHandler<void(const WebCore::TextManipulationController::ManipulationResult&)>&& completionHandler)
{
    if (!m_page) {
        completionHandler({ });
        return;
    }

    if (items.isEmpty()) {
        completionHandler({ });
        return;
    }

    auto currentFrameID = items[0].frameID;

    auto completeManipulationForItems = [&](const Vector<WebCore::TextManipulationItem>& items) -> WebCore::TextManipulationController::ManipulationResult {
        ASSERT(!items.isEmpty());
        RefPtr frame = WebProcess::singleton().webFrame(currentFrameID);
        if (!frame)
            return { };

        RefPtr coreFrame = frame->coreLocalFrame();
        if (!coreFrame)
            return { };

        CheckedPtr controller = coreFrame->document()->textManipulationControllerIfExists();
        if (!controller)
            return { };

        return controller->completeManipulation(items);
    };

    bool containsItemsForMultipleFrames = std::ranges::any_of(items, [&](auto& item) {
        return currentFrameID != item.frameID;
    });
    if (!containsItemsForMultipleFrames)
        return completionHandler(completeManipulationForItems(items));

    WebCore::TextManipulationController::ManipulationResult resultForAllItems;

    auto completeManipulationForCurrentFrame = [&](uint64_t startIndexForCurrentFrame, Vector<WebCore::TextManipulationItem> itemsForCurrentFrame) {
        auto result = completeManipulationForItems(std::exchange(itemsForCurrentFrame, { }));
        for (auto& failure : result.failures)
            failure.index += startIndexForCurrentFrame;
        for (auto& index : result.succeededIndexes)
            index += startIndexForCurrentFrame;
        resultForAllItems.failures.appendVector(WTFMove(result.failures));
        resultForAllItems.succeededIndexes.appendVector(WTFMove(result.succeededIndexes));
    };

    uint64_t indexForCurrentItem = 0;
    uint64_t itemCount = 0;
    for (auto& item : items) {
        if (currentFrameID != item.frameID) {
            RELEASE_ASSERT(indexForCurrentItem >= itemCount);
            completeManipulationForCurrentFrame(indexForCurrentItem - itemCount, items.subspan(indexForCurrentItem - itemCount, itemCount));
            currentFrameID = item.frameID;
            itemCount = 0;
        }
        ++indexForCurrentItem;
        ++itemCount;
    }
    RELEASE_ASSERT(indexForCurrentItem >= itemCount);
    completeManipulationForCurrentFrame(indexForCurrentItem - itemCount, items.subspan(indexForCurrentItem - itemCount, itemCount));

    completionHandler(resultForAllItems);
}

PAL::SessionID WebPage::sessionID() const
{
    return WebProcess::singleton().sessionID();
}

bool WebPage::usesEphemeralSession() const
{
    return sessionID().isEphemeral();
}

void WebPage::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->configureLoggingChannel(channelName, state, level);
#endif

#if ENABLE(MODEL_PROCESS)
    if (auto* modelProcessConnection = WebProcess::singleton().existingModelProcessConnection())
        modelProcessConnection->configureLoggingChannel(channelName, state, level);
#endif

    send(Messages::WebPageProxy::ConfigureLoggingChannel(channelName, state, level));
}

#if !PLATFORM(COCOA)

void WebPage::getPDFFirstPageSize(WebCore::FrameIdentifier, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
    completionHandler({ });
}

void WebPage::getProcessDisplayName(CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebPage::updateMockAccessibilityElementAfterCommittingLoad()
{
}

#endif

#if !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)

void WebPage::didFinishLoadingImageForElement(WebCore::HTMLImageElement&)
{
}

#endif

#if ENABLE(MODEL_PROCESS)
void WebPage::setHasModelElement(bool hasModelElement)
{
    send(Messages::WebPageProxy::SetHasModelElement(hasModelElement));
}
#endif

#if ENABLE(TEXT_AUTOSIZING)
void WebPage::textAutoSizingAdjustmentTimerFired()
{
    protectedCorePage()->recomputeTextAutoSizingInAllFrames();
}

void WebPage::textAutosizingUsesIdempotentModeChanged()
{
    if (!m_page->settings().textAutosizingUsesIdempotentMode())
        m_textAutoSizingAdjustmentTimer.stop();
}
#endif // ENABLE(TEXT_AUTOSIZING)

#if ENABLE(WEBXR)
PlatformXRSystemProxy& WebPage::xrSystemProxy()
{
    if (!m_xrSystemProxy)
        lazyInitialize(m_xrSystemProxy, makeUniqueWithoutRefCountedCheck<PlatformXRSystemProxy>(*this));
    return *m_xrSystemProxy;
}
#endif

void WebPage::setOverriddenMediaType(const String& mediaType)
{
    if (mediaType == m_overriddenMediaType)
        return;

    m_overriddenMediaType = AtomString(mediaType);
    protectedCorePage()->updateStyleAfterChangeInEnvironment();
}

void WebPage::updateCORSDisablingPatterns(Vector<String>&& patterns)
{
    RefPtr page = m_page;
    if (!page)
        return;

    m_corsDisablingPatterns = WTFMove(patterns);
    synchronizeCORSDisablingPatternsWithNetworkProcess();
    page->setCORSDisablingPatterns(parseAndAllowAccessToCORSDisablingPatterns(m_corsDisablingPatterns));
}

void WebPage::synchronizeCORSDisablingPatternsWithNetworkProcess()
{
    // FIXME: We should probably have this mechanism done between UIProcess and NetworkProcess directly.
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCORSDisablingPatterns(m_identifier, m_corsDisablingPatterns), 0);
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebPage::isAnyAnimationAllowedToPlayDidChange(bool anyAnimationCanPlay)
{
    if (!m_page->settings().imageAnimationControlEnabled())
        return;
    send(Messages::WebPageProxy::IsAnyAnimationAllowedToPlayDidChange(anyAnimationCanPlay));
}
#endif

void WebPage::isPlayingMediaDidChange(WebCore::MediaProducerMediaStateFlags state)
{
    send(Messages::WebPageProxy::IsPlayingMediaDidChange(state));
}

#if ENABLE(MEDIA_USAGE)
void WebPage::addMediaUsageManagerSession(MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
{
    send(Messages::WebPageProxy::AddMediaUsageManagerSession(identifier, bundleIdentifier, pageURL));
}

void WebPage::updateMediaUsageManagerSessionState(MediaSessionIdentifier identifier, const MediaUsageInfo& usage)
{
    send(Messages::WebPageProxy::UpdateMediaUsageManagerSessionState(identifier, usage));
}

void WebPage::removeMediaUsageManagerSession(MediaSessionIdentifier identifier)
{
    send(Messages::WebPageProxy::RemoveMediaUsageManagerSession(identifier));
}
#endif // ENABLE(MEDIA_USAGE)

#if ENABLE(IMAGE_ANALYSIS)

void WebPage::requestTextRecognition(Element& element, TextRecognitionOptions&& options, CompletionHandler<void(RefPtr<Element>&&)>&& completion)
{
    RefPtr htmlElement = dynamicDowncast<HTMLElement>(element);
    if (!htmlElement) {
        if (completion)
            completion({ });
        return;
    }

    if (protectedCorePage()->hasCachedTextRecognitionResult(*htmlElement)) {
        if (completion) {
            RefPtr<Element> imageOverlayHost;
            if (ImageOverlay::hasOverlay(*htmlElement))
                imageOverlayHost = element;
            completion(WTFMove(imageOverlayHost));
        }
        return;
    }

    auto matchIndex = m_elementsPendingTextRecognition.findIf([&] (auto& elementAndCompletionHandlers) {
        return elementAndCompletionHandlers.first == &element;
    });

    if (matchIndex != notFound) {
        if (completion)
            m_elementsPendingTextRecognition[matchIndex].second.append(WTFMove(completion));
        return;
    }

    CheckedPtr renderImage = dynamicDowncast<RenderImage>(element.renderer());
    if (!renderImage) {
        if (completion)
            completion({ });
        return;
    }

    auto bitmap = createShareableBitmap(*renderImage, {
        std::nullopt,
        AllowAnimatedImages::No,
        options.allowSnapshots == TextRecognitionOptions::AllowSnapshots::Yes ? UseSnapshotForTransparentImages::Yes : UseSnapshotForTransparentImages::No
    });
    if (!bitmap) {
        if (completion)
            completion({ });
        return;
    }

    auto bitmapHandle = bitmap->createHandle();
    if (!bitmapHandle) {
        if (completion)
            completion({ });
        return;
    }

    Vector<CompletionHandler<void(RefPtr<Element>&&)>> completionHandlers;
    if (completion)
        completionHandlers.append(WTFMove(completion));
    m_elementsPendingTextRecognition.append({ WeakPtr { element }, WTFMove(completionHandlers) });

    auto cachedImage = renderImage->cachedImage();
    auto imageURL = cachedImage ? element.protectedDocument()->completeURL(cachedImage->url().string()) : URL { };
    sendWithAsyncReply(Messages::WebPageProxy::RequestTextRecognition(WTFMove(imageURL), WTFMove(*bitmapHandle), options.sourceLanguageIdentifier, options.targetLanguageIdentifier), [webPage = WeakPtr { *this }, weakElement = WeakPtr { element }] (auto&& result) {
        RefPtr protectedPage { webPage.get() };
        if (!protectedPage)
            return;

        protectedPage->m_elementsPendingTextRecognition.removeAllMatching([&] (auto& elementAndCompletionHandlers) {
            auto& [element, completionHandlers] = elementAndCompletionHandlers;
            if (element)
                return false;

            for (auto& completionHandler : completionHandlers)
                completionHandler({ });
            return true;
        });

        RefPtr htmlElement = downcast<HTMLElement>(weakElement.get());
        if (!htmlElement)
            return;

        ImageOverlay::updateWithTextRecognitionResult(*htmlElement, result);

        auto matchIndex = protectedPage->m_elementsPendingTextRecognition.findIf([&] (auto& elementAndCompletionHandlers) {
            return elementAndCompletionHandlers.first == htmlElement.get();
        });

        if (matchIndex == notFound)
            return;

        RefPtr imageOverlayHost = ImageOverlay::hasOverlay(*htmlElement) ? htmlElement.get() : nullptr;
        for (auto& completionHandler : protectedPage->m_elementsPendingTextRecognition[matchIndex].second)
            completionHandler(imageOverlayHost.copyRef());

        protectedPage->m_elementsPendingTextRecognition.removeAt(matchIndex);
    });
}

void WebPage::updateWithTextRecognitionResult(const TextRecognitionResult& result, const ElementContext& context, const FloatPoint& location, CompletionHandler<void(TextRecognitionUpdateResult)>&& completionHandler)
{
    auto elementToUpdate = elementForContext(context);
    RefPtr htmlElementToUpdate = dynamicDowncast<HTMLElement>(elementToUpdate);
    if (!htmlElementToUpdate) {
        completionHandler(TextRecognitionUpdateResult::NoText);
        return;
    }

    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame) {
        completionHandler(TextRecognitionUpdateResult::NoText);
        return;
    }

    ImageOverlay::updateWithTextRecognitionResult(*htmlElementToUpdate, result);
    auto hitTestResult = localMainFrame->eventHandler().hitTestResultAtPoint(roundedIntPoint(location), {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::AllowVisibleChildFrameContentOnly,
    });

    RefPtr nodeAtLocation = hitTestResult.innerNonSharedNode();
    auto updateResult = ([&] {
        if (!nodeAtLocation || nodeAtLocation->shadowHost() != elementToUpdate || !ImageOverlay::isInsideOverlay(*nodeAtLocation))
            return TextRecognitionUpdateResult::NoText;

#if ENABLE(DATA_DETECTION)
        if (DataDetection::findDataDetectionResultElementInImageOverlay(location, *htmlElementToUpdate))
            return TextRecognitionUpdateResult::DataDetector;
#endif

        if (ImageOverlay::isOverlayText(*nodeAtLocation))
            return TextRecognitionUpdateResult::Text;

        return TextRecognitionUpdateResult::NoText;
    })();

    completionHandler(updateResult);
}

void WebPage::startVisualTranslation(const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier)
{
    RefPtr frame = m_mainFrame->coreFrame();
    if (!frame)
        return;

    protectedCorePage()->protectedImageAnalysisQueue()->enqueueAllImagesIfNeeded(*frame, sourceLanguageIdentifier, targetLanguageIdentifier);
}

#endif // ENABLE(IMAGE_ANALYSIS)

void WebPage::requestImageBitmap(const ElementContext& context, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&, const String& sourceMIMEType)>&& completion)
{
    RefPtr element = elementForContext(context);
    if (!element) {
        completion({ }, { });
        return;
    }

    CheckedPtr renderImage = dynamicDowncast<RenderImage>(element->renderer());
    if (!renderImage) {
        completion({ }, { });
        return;
    }

    auto bitmap = createShareableBitmap(*renderImage);
    if (!bitmap) {
        completion({ }, { });
        return;
    }

    auto handle = bitmap->createHandle();
    if (!handle) {
        completion({ }, { });
        return;
    }

    String mimeType;
    if (auto* cachedImage = renderImage->cachedImage()) {
        if (RefPtr image = cachedImage->image())
            mimeType = image->mimeType();
    }
    ASSERT(!mimeType.isEmpty());
    completion(WTFMove(*handle), mimeType);
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
void WebPage::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, WebCore::HTMLMediaElementIdentifier identifier, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    RefPtr frame = m_mainFrame->coreFrame();
    if (!frame) {
        completionHandler(MediaControlsContextMenuItem::invalidID);
        return;
    }

    sendWithAsyncReply(Messages::WebPageProxy::ShowMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), WebFrame::fromCoreFrame(*frame)->info(), identifier), completionHandler);
}
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if !PLATFORM(IOS_FAMILY)

void WebPage::animationDidFinishForElement(const WebCore::Element&)
{
}

#endif

#if ENABLE(APP_BOUND_DOMAINS)
void WebPage::setIsNavigatingToAppBoundDomain(std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, WebFrame& frame)
{
    frame.setIsNavigatingToAppBoundDomain(isNavigatingToAppBoundDomain);

    m_navigationHasOccured = true;
}

void WebPage::notifyPageOfAppBoundBehavior()
{
    if (!m_navigationHasOccured && !m_limitsNavigationsToAppBoundDomains)
        send(Messages::WebPageProxy::SetHasExecutedAppBoundBehaviorBeforeNavigation());
}
#endif

#if ENABLE(GPU_PROCESS)
RemoteRenderingBackendProxy& WebPage::ensureRemoteRenderingBackendProxy()
{
    if (!m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy = RemoteRenderingBackendProxy::create(*this);
    return *m_remoteRenderingBackendProxy;
}

Ref<RemoteRenderingBackendProxy> WebPage::ensureProtectedRemoteRenderingBackendProxy()
{
    return ensureRemoteRenderingBackendProxy();
}
#endif

Vector<Ref<SandboxExtension>> WebPage::consumeSandboxExtensions(Vector<SandboxExtension::Handle>&& sandboxExtensions)
{
    return WTF::compactMap(WTFMove(sandboxExtensions), [](SandboxExtension::Handle&& sandboxExtension) -> RefPtr<SandboxExtension> {
        auto extension = SandboxExtension::create(WTFMove(sandboxExtension));
        if (!extension)
            return nullptr;
        bool ok = extension->consume();
        ASSERT_UNUSED(ok, ok);
        return extension;
    });
}

void WebPage::revokeSandboxExtensions(Vector<Ref<SandboxExtension>>& sandboxExtensions)
{
    for (auto& sandboxExtension : sandboxExtensions)
        sandboxExtension->revoke();
    sandboxExtensions.clear();
}

void WebPage::createTextFragmentDirectiveFromSelection(CompletionHandler<void(URL&&)>&& completionHandler)
{
    auto url = protectedCorePage()->fragmentDirectiveURLForSelectedText();
    completionHandler(WTFMove(url));
}

void WebPage::getTextFragmentRanges(CompletionHandler<void(const Vector<EditingRange>&&)>&& completionHandler)
{
    RefPtr focusedOrMainFrame = corePage()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame) {
        completionHandler({ });
        return;
    }
    RefPtr document = focusedOrMainFrame->document();

    RefPtr frame = document->frame();
    if (!frame) {
        completionHandler({ });
        return;
    }

    Vector<EditingRange> editingRanges;
    if (RefPtr highlightRegistry = document->fragmentHighlightRegistryIfExists()) {
        for (auto& highlight : highlightRegistry->map()) {
            for (auto& highlightRange : highlight.value->highlightRanges()) {
                Ref<AbstractRange> range = highlightRange->range();
                editingRanges.append(EditingRange::fromRange(*frame, makeSimpleRange(range)));
            }
        }
    }

    completionHandler(WTFMove(editingRanges));
}

#if ENABLE(APP_HIGHLIGHTS)
WebCore::CreateNewGroupForHighlight WebPage::highlightIsNewGroup() const
{
    return m_internals->highlightIsNewGroup;
}

WebCore::HighlightRequestOriginatedInApp WebPage::highlightRequestOriginatedInApp() const
{
    return m_internals->highlightRequestOriginatedInApp;
}

void WebPage::createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight createNewGroup, WebCore::HighlightRequestOriginatedInApp requestOriginatedInApp, CompletionHandler<void(WebCore::AppHighlight&&)>&& completionHandler)
{
    SetForScope highlightIsNewGroupScope { m_internals->highlightIsNewGroup, createNewGroup };
    SetForScope highlightRequestOriginScope { m_internals->highlightRequestOriginatedInApp, requestOriginatedInApp };

    RefPtr focusedOrMainFrame = corePage()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return;
    RefPtr document = focusedOrMainFrame->document();

    RefPtr frame = document->frame();
    if (!frame)
        return;

    auto selectionRange = frame->selection().selection().toNormalizedRange();
    if (!selectionRange)
        return;

    document->protectedAppHighlightRegistry()->addAnnotationHighlightWithRange(StaticRange::create(selectionRange.value()));
    document->appHighlightStorage().storeAppHighlight(StaticRange::create(selectionRange.value()), [completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }, this] (WebCore::AppHighlight&& highlight) mutable {
        highlight.isNewGroup = m_internals->highlightIsNewGroup;
        highlight.requestOriginatedInApp = m_internals->highlightRequestOriginatedInApp;
        completionHandler(WTFMove(highlight));
    });
}

void WebPage::restoreAppHighlightsAndScrollToIndex(Vector<SharedMemory::Handle>&& memoryHandles, const std::optional<unsigned> index)
{
    RefPtr focusedOrMainFrame = corePage()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return;
    RefPtr document = focusedOrMainFrame->document();

    unsigned i = 0;
    for (auto&& handle : memoryHandles) {
        auto sharedMemory = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadOnly);
        if (!sharedMemory)
            continue;

        document->appHighlightStorage().restoreAndScrollToAppHighlight(sharedMemory->createSharedBuffer(handle.size()), i == index ? ScrollToHighlight::Yes : ScrollToHighlight::No);
        i++;
    }
}

void WebPage::setAppHighlightsVisibility(WebCore::HighlightVisibility appHighlightVisibility)
{
    m_appHighlightsVisible = appHighlightVisibility;
    for (RefPtr<Frame> frame = m_mainFrame->coreLocalFrame(); frame; frame = frame->tree().traverseNextRendered()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        if (RefPtr document = localFrame->document())
            document->protectedAppHighlightRegistry()->setHighlightVisibility(appHighlightVisibility);
    }
}

#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void WebPage::createMediaSessionCoordinator(const String& identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr document = m_mainFrame->coreLocalFrame()->document();
    if (!document || !document->window()) {
        completionHandler(false);
        return;
    }

    protectedCorePage()->setMediaSessionCoordinator(RemoteMediaSessionCoordinator::create(*this, identifier));
    completionHandler(true);
}
#endif

void WebPage::lastNavigationWasAppInitiated(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr localTopDocument = this->localTopDocument();
    if (!localTopDocument)
        return completionHandler(false);
    return completionHandler(localTopDocument->loader()->lastNavigationWasAppInitiated());
}

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

void WebPage::handleContextMenuTranslation(const TranslationContextMenuInfo& info)
{
    send(Messages::WebPageProxy::HandleContextMenuTranslation(info));
}
#endif

void WebPage::scrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint&)
{
    RefPtr frameView = localMainFrameView();
    if (!frameView)
        return;
    frameView->setScrollPosition(IntPoint(targetRect.minXMinYCorner()));
}

void WebPage::setContentOffset(std::optional<int> x, std::optional<int> y, WebCore::ScrollIsAnimated animated)
{
    RefPtr frameView = localMainFrameView();
    if (!frameView)
        return;

    auto options = WebCore::ScrollPositionChangeOptions::createProgrammatic();
    options.animated = animated;

    frameView->setScrollOffsetWithOptions(x, y, options);
}

void WebPage::scrollToEdge(WebCore::RectEdges<bool> edges, WebCore::ScrollIsAnimated animated)
{
    RefPtr frameView = localMainFrameView();
    if (!frameView)
        return;

    auto options = WebCore::ScrollPositionChangeOptions::createProgrammatic();
    options.animated = animated;

    frameView->scrollToEdgeWithOptions(edges, options);
}

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)
void WebPage::beginTextRecognitionForVideoInElementFullScreen(const HTMLVideoElement& element)
{
    auto mediaPlayerIdentifier = element.playerIdentifier();
    if (!mediaPlayerIdentifier)
        return;

    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return;

    auto rectInRootView = renderer->videoBoxInRootView();
    if (rectInRootView.isEmpty())
        return;

    send(Messages::WebPageProxy::BeginTextRecognitionForVideoInElementFullScreen(*mediaPlayerIdentifier, rectInRootView));
}

void WebPage::cancelTextRecognitionForVideoInElementFullScreen()
{
    send(Messages::WebPageProxy::CancelTextRecognitionForVideoInElementFullScreen());
}
#endif // ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
void WebPage::modelInlinePreviewDidLoad(WebCore::PlatformLayerIdentifier layerID)
{
    ARKitInlinePreviewModelPlayerIOS::pageLoadedModelInlinePreview(*this, layerID);
}

void WebPage::modelInlinePreviewDidFailToLoad(WebCore::PlatformLayerIdentifier layerID, const WebCore::ResourceError& error)
{
    ARKitInlinePreviewModelPlayerIOS::pageFailedToLoadModelInlinePreview(*this, layerID, error);
}
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPage::shouldAllowRemoveBackground(const ElementContext& context, CompletionHandler<void(bool)>&& completion) const
{
    auto element = elementForContext(context);
    completion(element && !m_elementsToExcludeFromRemoveBackground.contains(*element));
}

#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

void WebPage::setIsWindowResizingEnabled(bool value)
{
    if (m_isWindowResizingEnabled == value)
        return;

    m_isWindowResizingEnabled = value;
    m_viewportConfiguration.setPrefersHorizontalScrollingBelowDesktopViewportWidths(shouldEnableViewportBehaviorsForResizableWindows());
}

#endif // HAVE(UIKIT_RESIZABLE_WINDOWS)

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

void WebPage::setInteractionRegionsEnabled(bool enable)
{
    WEBPAGE_RELEASE_LOG(Process, "setInteractionRegionsEnabled: enable state = %d for page %p", (int)enable, (void*)m_page.get());
    if (!m_page)
        return;

    m_page->setInteractionRegionsEnabled(enable);
}

#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

bool WebPage::handlesPageScaleGesture()
{
#if !ENABLE(PDF_PLUGIN)
    return false;
#else
    RefPtr plugin = mainFramePlugIn();
    return plugin && plugin->pluginHandlesPageScaleFactor();
#endif
}

void WebPage::generateTestReport(String&& message, String&& group)
{
    if (RefPtr localTopDocument = this->localTopDocument())
        localTopDocument->protectedReportingScope()->generateTestReport(WTFMove(message), WTFMove(group));
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebPage::updateImageAnimationEnabled()
{
    protectedCorePage()->setImageAnimationEnabled(WebProcess::singleton().imageAnimationEnabled());
}

void WebPage::pauseAllAnimations(CompletionHandler<void()>&& completionHandler)
{
    protectedCorePage()->setImageAnimationEnabled(false);
    completionHandler();
}

void WebPage::playAllAnimations(CompletionHandler<void()>&& completionHandler)
{
    protectedCorePage()->setImageAnimationEnabled(true);
    completionHandler();
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
void WebPage::updatePrefersNonBlinkingCursor()
{
    if (RefPtr page = corePage()) {
        page->setPrefersNonBlinkingCursor(WebProcess::singleton().prefersNonBlinkingCursor());
        page->forEachDocument([&](auto& document) {
            document.selection().setPrefersNonBlinkingCursor(WebProcess::singleton().prefersNonBlinkingCursor());
        });
    }
}
#endif

bool WebPage::isUsingUISideCompositing() const
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    return m_drawingAreaType == DrawingAreaType::RemoteLayerTree;
#elif PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

void WebPage::setLinkDecorationFilteringData(Vector<WebCore::LinkDecorationFilteringData>&& strings)
{
    m_internals->linkDecorationFilteringData.clear();

    for (auto& data : strings) {
        if (!m_internals->linkDecorationFilteringData.isValidKey(data.linkDecoration)) {
            WEBPAGE_RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to set link decoration filtering data (invalid key)");
            ASSERT_NOT_REACHED();
            continue;
        }

        auto it = m_internals->linkDecorationFilteringData.ensure(data.linkDecoration, [] {
            return Internals::LinkDecorationFilteringConditionals { };
        }).iterator;

        if (!data.domain.isEmpty()) {
            if (auto& domains = it->value.domains; domains.isValidValue(data.domain))
                domains.add(data.domain);
            else
                ASSERT_NOT_REACHED();
        }

        if (!data.path.isEmpty())
            it->value.paths.append(data.path);
    }
}

void WebPage::setAllowedQueryParametersForAdvancedPrivacyProtections(Vector<LinkDecorationFilteringData>&& allowStrings)
{
    m_internals->allowedQueryParametersForAdvancedPrivacyProtections.clear();
    for (auto& data : allowStrings) {
        if (!m_internals->allowedQueryParametersForAdvancedPrivacyProtections.isValidKey(data.domain))
            continue;

        m_internals->allowedQueryParametersForAdvancedPrivacyProtections.ensure(data.domain, [&] {
            return HashSet<String> { };
        }).iterator->value.add(data.linkDecoration);
    }
}

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

bool WebPage::shouldSkipDecidePolicyForResponse(const WebCore::ResourceResponse& response) const
{
    if (!m_skipDecidePolicyForResponseIfPossible)
        return false;

    auto statusCode = response.httpStatusCode();
    if (statusCode == httpStatus204NoContent || statusCode >= httpStatus400BadRequest)
        return false;

    if (!equalIgnoringASCIICase(response.mimeType(), "text/html"_s))
        return false;

    if (response.url().protocolIsFile())
        return false;

    if (auto components = response.httpHeaderField(HTTPHeaderName::ContentDisposition).split(';'); !components.isEmpty() && equalIgnoringASCIICase(components[0].trim(isASCIIWhitespaceWithoutFF<char16_t>), "attachment"_s))
        return false;

    return true;
}

const Logger& WebPage::logger() const
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        m_logger->setEnabled(this, isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}

uint64_t WebPage::logIdentifier() const
{
    return intHash(m_identifier.toUInt64());
}

void WebPage::useRedirectionForCurrentNavigation(WebCore::ResourceResponse&& response)
{
    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame) {
        WEBPAGE_RELEASE_LOG_ERROR(Loading, "WebPage::useRedirectionForCurrentNavigation failed without frame");
        return;
    }

    RefPtr loader = localMainFrame->loader().policyDocumentLoader();
    if (!loader)
        loader = localMainFrame->loader().provisionalDocumentLoader();

    if (!loader) {
        WEBPAGE_RELEASE_LOG_ERROR(Loading, "WebPage::useRedirectionForCurrentNavigation failed without loader");
        return;
    }

    if (RefPtr resourceLoader = loader->mainResourceLoader()) {
        WEBPAGE_RELEASE_LOG(Loading, "WebPage::useRedirectionForCurrentNavigation to network process");
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UseRedirectionForCurrentNavigation(*resourceLoader->identifier(), response), 0);
        return;
    }

    WEBPAGE_RELEASE_LOG(Loading, "WebPage::useRedirectionForCurrentNavigation as substiute data");
    loader->setRedirectionAsSubstituteData(WTFMove(response));
}

void WebPage::dispatchLoadEventToFrameOwnerElement(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreRemoteFrame = frame->coreRemoteFrame();
    if (!coreRemoteFrame)
        return;

    if (RefPtr ownerElement = coreRemoteFrame->ownerElement())
        ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void WebPage::elementWasFocusedInAnotherProcess(WebCore::FrameIdentifier frameID, WebCore::FocusOptions options)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    protectedCorePage()->focusController().setFocusedElement(nullptr, frame->protectedCoreFrame().get(), options, WebCore::BroadcastFocusedElement::No);
}

void WebPage::frameWasFocusedInAnotherProcess(std::optional<WebCore::FrameIdentifier>&& frameID)
{
    RefPtr frame = frameID ? WebProcess::singleton().webFrame(*frameID) : nullptr;
    RefPtr coreFrame = frame ? frame->coreFrame() : nullptr;
    protectedCorePage()->focusController().setFocusedFrame(coreFrame.get(), WebCore::BroadcastFocusedFrame::No);
}

void WebPage::remotePostMessage(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData>&& targetOrigin, const WebCore::MessageWithMessagePorts& message)
{
    RefPtr targetFrame = WebProcess::singleton().webFrame(target);
    if (!targetFrame)
        return;

    if (!targetFrame->coreLocalFrame())
        return;

    RefPtr targetWindow = targetFrame->protectedCoreLocalFrame()->window();
    if (!targetWindow)
        return;

    RefPtr targetCoreFrame = targetWindow->localFrame();
    if (!targetCoreFrame)
        return;

    RefPtr sourceFrame = WebProcess::singleton().webFrame(source);
    RefPtr sourceWindow = sourceFrame && sourceFrame->coreFrame() ? &sourceFrame->coreFrame()->windowProxy() : nullptr;

    CheckedRef script = targetCoreFrame->script();
    auto globalObject = script->globalObject(WebCore::mainThreadNormalWorldSingleton());
    if (!globalObject)
        return;

    targetWindow->postMessageFromRemoteFrame(*globalObject, WTFMove(sourceWindow), sourceOrigin, WTFMove(targetOrigin), message);
}

void WebPage::renderTreeAsTextForTesting(WebCore::FrameIdentifier frameID, uint64_t baseIndent, OptionSet<WebCore::RenderAsTextFlag> behavior, CompletionHandler<void(String&&)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing in web process"_s);
    }

    RefPtr coreLocalFrame = webFrame->coreLocalFrame();
    if (!coreLocalFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing LocalFrame in web process"_s);
    }

    CheckedPtr renderer = coreLocalFrame->contentRenderer();
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing RenderView in web process"_s);
    }

    auto ts = WebCore::createTextStream(*renderer);
    ts.setIndent(baseIndent);
    WebCore::externalRepresentationForLocalFrame(ts, *coreLocalFrame, behavior);
    completionHandler(ts.release());
}

void WebPage::layerTreeAsTextForTesting(WebCore::FrameIdentifier frameID, uint64_t baseIndent, OptionSet<WebCore::LayerTreeAsTextOptions> options, CompletionHandler<void(String&&)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing in web process"_s);
    }

    RefPtr coreLocalFrame = webFrame->coreLocalFrame();
    if (!coreLocalFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing LocalFrame in web process"_s);
    }

    CheckedPtr renderer = coreLocalFrame->contentRenderer();
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing RenderView in web process"_s);
    }

    auto ts = WebCore::createTextStream(*renderer);
    ts << coreLocalFrame->checkedContentRenderer()->checkedCompositor()->layerTreeAsText(options, baseIndent);
    completionHandler(ts.release());
}

void WebPage::frameTextForTesting(WebCore::FrameIdentifier frameID, CompletionHandler<void(String&&)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing in web process"_s);
    }
    constexpr bool includeSubframes { true };
    completionHandler(webFrame->frameTextForTesting(includeSubframes));
}

void WebPage::requestAllTextAndRects(CompletionHandler<void(Vector<std::pair<String, WebCore::FloatRect>>&&)>&& completion)
{
    RefPtr page = corePage();
    if (!page)
        return completion({ });

    completion(TextExtraction::extractAllTextAndRects(*page));
}

void WebPage::requestTargetedElement(TargetedElementRequest&& request, CompletionHandler<void(Vector<WebCore::TargetedElementInfo>&&)>&& completion)
{
    RefPtr page = corePage();
    if (!page)
        return completion({ });

    completion(page->checkedElementTargetingController()->findTargets(WTFMove(request)));
}

void WebPage::requestAllTargetableElements(float hitTestInterval, CompletionHandler<void(Vector<Vector<WebCore::TargetedElementInfo>>&&)>&& completion)
{
    RefPtr page = corePage();
    if (!page)
        return completion({ });

    completion(page->checkedElementTargetingController()->findAllTargets(hitTestInterval));
}

void WebPage::requestTextExtraction(TextExtraction::Request&& request, CompletionHandler<void(TextExtraction::Item&&)>&& completion)
{
    completion(TextExtraction::extractItem(WTFMove(request), Ref { *corePage() }));
}

void WebPage::takeSnapshotOfExtractedText(TextExtraction::ExtractedText&& extractedText, CompletionHandler<void(RefPtr<TextIndicator>&&)>&& completion)
{
    RefPtr frame = m_mainFrame->coreLocalFrame();
    if (!frame) {
        // FIXME: This logic will be moved into WebFrame once support for extracting content from subframes is
        // implemented, at which point we should use the WebFrame's local frame. Until then, we only support
        // content in the main frame anyways.
        return completion({ });
    }

    auto range = TextExtraction::rangeForExtractedText(*frame, WTFMove(extractedText));
    if (!range)
        return completion({ });

    using enum WebCore::TextIndicatorOption;
    constexpr OptionSet options {
        RespectTextColor,
        PaintBackgrounds,
        PaintAllContent,
        TightlyFitContent,
        UseBoundingRectAndPaintAllContentForComplexRanges,
        DoNotClipToVisibleRect
    };

    completion(TextIndicator::createWithRange(*range, options, TextIndicatorPresentationTransition::None));
}

void WebPage::describeTextExtractionInteraction(TextExtraction::Interaction&& interaction, CompletionHandler<void(TextExtraction::InteractionDescription&&)>&& completion)
{
    completion(TextExtraction::interactionDescription(interaction, Ref { *corePage() }));
}

void WebPage::handleTextExtractionInteraction(TextExtraction::Interaction&& interaction, CompletionHandler<void(bool, String&&)>&& completion)
{
    TextExtraction::handleInteraction(WTFMove(interaction), Ref { *corePage() }, WTFMove(completion));
}

void WebPage::updateTextExtractionFilterRules(Vector<WebCore::TextExtraction::FilterRuleData>&& ruleData)
{
    m_textExtractionFilterRules = TextExtraction::extractRules(WTFMove(ruleData));
}

void WebPage::applyTextExtractionFilter(const String& input, std::optional<NodeIdentifier>&& containerNodeID, CompletionHandler<void(const String&)>&& completion)
{
    TextExtraction::applyRules(input, WTFMove(containerNodeID), m_textExtractionFilterRules, Ref { *corePage() }, WTFMove(completion));
}

template<typename T> T WebPage::contentsToRootView(WebCore::FrameIdentifier frameID, T geometry)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return geometry;

    RefPtr coreFrame = webFrame->coreFrame();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return geometry;
    }

    RefPtr view = coreFrame->virtualView();
    if (!view) {
        ASSERT_NOT_REACHED();
        return geometry;
    }

    return view->contentsToRootView(geometry);
}

template<typename T> T WebPage::rootViewToContents(WebCore::FrameIdentifier frameID, T geometry)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return geometry;

    RefPtr coreFrame = webFrame->coreFrame();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return geometry;
    }

    RefPtr view = coreFrame->virtualView();
    if (!view) {
        ASSERT_NOT_REACHED();
        return geometry;
    }

    return view->rootViewToContents(geometry);
}

void WebPage::contentsToRootViewRect(FrameIdentifier frameID, FloatRect rect, CompletionHandler<void(FloatRect)>&& completionHandler)
{
    completionHandler(contentsToRootView(frameID, rect));
}

void WebPage::contentsToRootViewPoint(FrameIdentifier frameID, FloatPoint point, CompletionHandler<void(FloatPoint)>&& completionHandler)
{
    completionHandler(contentsToRootView(frameID, point));
}

void WebPage::remoteDictionaryPopupInfoToRootView(WebCore::FrameIdentifier frameID, WebCore::DictionaryPopupInfo popupInfo, CompletionHandler<void(WebCore::DictionaryPopupInfo)>&& completionHandler)
{
    RefPtr textIndicator = popupInfo.textIndicator;
    popupInfo.origin = contentsToRootView<FloatPoint>(frameID, popupInfo.origin);
    if (!textIndicator)
        return completionHandler(popupInfo);
#if PLATFORM(COCOA)
    auto textIndicatorData = textIndicator->data();
    textIndicatorData.selectionRectInRootViewCoordinates = contentsToRootView<FloatRect>(frameID, popupInfo.textIndicator->selectionRectInRootViewCoordinates());
    textIndicatorData.textBoundingRectInRootViewCoordinates = contentsToRootView<FloatRect>(frameID, popupInfo.textIndicator->textBoundingRectInRootViewCoordinates());
    textIndicatorData.contentImageWithoutSelectionRectInRootViewCoordinates = contentsToRootView<FloatRect>(frameID, popupInfo.textIndicator->contentImageWithoutSelectionRectInRootViewCoordinates());

    for (auto& textRect : textIndicatorData.textRectsInBoundingRectCoordinates)
        textRect = contentsToRootView<FloatRect>(frameID, textRect);
#endif
    completionHandler(popupInfo);
}

void WebPage::hitTestAtPoint(WebCore::FrameIdentifier frameID, WebCore::FloatPoint point, CompletionHandler<void(NodeHitTestResult)>&& completionHandler)
{
    RefPtr frame = WebFrame::webFrame(frameID);
    if (!frame)
        return completionHandler({ });

    auto options = WebFrame::defaultHitTestRequestTypes();
    options.remove(HitTestRequest::Type::AllowChildFrameContent);
    options.add(HitTestRequest::Type::SkipTransformToRootFrameCoordinates);
    RefPtr result = frame->hitTest(roundedIntPoint(point), options);
    if (!result)
        return completionHandler({ });

    RefPtr node = result->coreHitTestResult().innerNonSharedNode();
    if (!node)
        return completionHandler({ });

    if (RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(node.get())) {
        if (RefPtr contentFrame = frameOwner->contentFrame()) {
            auto transformedCoordinates = rootViewToContents(contentFrame->frameID(), contentsToRootView(frameID, point));
            switch (contentFrame->frameType()) {
            case Frame::FrameType::Remote:
                return completionHandler( { NodeHitTestResult::RemoteFrameInfo { contentFrame->frameID(), transformedCoordinates } });
            case Frame::FrameType::Local:
                return hitTestAtPoint(contentFrame->frameID(), transformedCoordinates, WTFMove(completionHandler));
            }
        }
    }

    RefPtr nodeFrame = node->document().frame();
    if (!nodeFrame)
        return completionHandler({ });

    RefPtr nodeWebFrame = WebFrame::fromCoreFrame(*nodeFrame);
    if (!nodeWebFrame)
        return completionHandler({ });

    Ref nodeHandle = [node] {
        Ref document = node->document();
        auto* lexicalGlobalObject = document->globalObject();
        RELEASE_ASSERT(lexicalGlobalObject->template inherits<WebCore::JSDOMGlobalObject>());
        auto* domGlobalObject = jsCast<WebCore::JSDOMGlobalObject*>(lexicalGlobalObject);
        JSLockHolder locker(lexicalGlobalObject);
        return WebCore::WebKitJSHandle::create(WebCore::toJS(lexicalGlobalObject, domGlobalObject, *node).toObject(lexicalGlobalObject));
    }();
    WebCore::WebKitJSHandle::jsHandleSentToAnotherProcess(nodeHandle->identifier());
    completionHandler({ JSHandleInfo { nodeHandle->identifier(), pageContentWorldIdentifier(), nodeWebFrame->info(), nodeHandle->windowFrameIdentifier() } });
}

void WebPage::adjustVisibilityForTargetedElements(Vector<TargetedElementAdjustment>&& adjustments, CompletionHandler<void(bool)>&& completion)
{
    RefPtr page = corePage();
    completion(page && page->checkedElementTargetingController()->adjustVisibility(WTFMove(adjustments)));
}

void WebPage::resetVisibilityAdjustmentsForTargetedElements(const Vector<TargetedElementIdentifiers>& identifiers, CompletionHandler<void(bool)>&& completion)
{
    RefPtr page = corePage();
    completion(page && page->checkedElementTargetingController()->resetVisibilityAdjustments(identifiers));
}

void WebPage::takeSnapshotForTargetedElement(NodeIdentifier nodeID, ScriptExecutionContextIdentifier documentID, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&& completion)
{
    RefPtr page = corePage();
    if (!page)
        return completion({ });

    RefPtr image = page->checkedElementTargetingController()->snapshotIgnoringVisibilityAdjustment(nodeID, documentID);
    if (!image)
        return completion({ });

    auto bitmap = ShareableBitmap::create({ IntSize { image->size() } });
    if (!bitmap)
        return completion({ });

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return completion({ });

    context->drawImage(*image, FloatPoint::zero());
    completion(bitmap->createHandle(SharedMemory::Protection::ReadOnly));
}

void WebPage::numberOfVisibilityAdjustmentRects(CompletionHandler<void(uint64_t)>&& completion)
{
    RefPtr page = corePage();
    completion(page ? page->checkedElementTargetingController()->numberOfVisibilityAdjustmentRects() : 0);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void WebPage::setDefaultSpatialTrackingLabel(const String& label)
{
    if (RefPtr page = corePage())
        page->setDefaultSpatialTrackingLabel(label);
}
#endif

void WebPage::startObservingNowPlayingMetadata()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    RefPtr sessionManager = mediaSessionManager();
    if (!sessionManager || m_nowPlayingMetadataObserver)
        return;

    m_nowPlayingMetadataObserver = NowPlayingMetadataObserver::create([weakThis = WeakPtr { *this }](auto& metadata) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->send(Messages::WebPageProxy::NowPlayingMetadataChanged { metadata });
    });

    sessionManager->addNowPlayingMetadataObserver(Ref { *m_nowPlayingMetadataObserver });
#endif
}

void WebPage::stopObservingNowPlayingMetadata()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    RefPtr nowPlayingMetadataObserver = std::exchange(m_nowPlayingMetadataObserver, nullptr);
    if (!nowPlayingMetadataObserver)
        return;

    if (RefPtr sessionManager = mediaSessionManager())
        sessionManager->removeNowPlayingMetadataObserver(*nowPlayingMetadataObserver);
#endif
}

void WebPage::didAdjustVisibilityWithSelectors(Vector<String>&& selectors)
{
    send(Messages::WebPageProxy::DidAdjustVisibilityWithSelectors(WTFMove(selectors)));
}

void WebPage::frameNameWasChangedInAnotherProcess(FrameIdentifier frameID, const String& frameName)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return;
    if (RefPtr coreFrame = webFrame->coreFrame())
        coreFrame->tree().setSpecifiedName(AtomString(frameName));
}

void WebPage::updateLastNodeBeforeWritingSuggestions(const KeyboardEvent& event)
{
    if (event.type() != eventNames().keydownEvent)
        return;

    if (RefPtr frame = corePage()->focusController().focusedOrMainFrame())
        m_lastNodeBeforeWritingSuggestions = frame->protectedEditor()->nodeBeforeWritingSuggestions();
}

void WebPage::didAddOrRemoveViewportConstrainedObjects()
{
    m_needsFixedContainerEdgesUpdate = true;

#if PLATFORM(IOS_FAMILY)
    scheduleLayoutViewportHeightExpansionUpdate();
#endif
}

void WebPage::addReasonsToDisallowLayoutViewportHeightExpansion(OptionSet<DisallowLayoutViewportHeightExpansionReason> reasons)
{
    bool wasEmpty = m_disallowLayoutViewportHeightExpansionReasons.isEmpty();
    m_disallowLayoutViewportHeightExpansionReasons.add(reasons);

    if (!m_page->settings().layoutViewportHeightExpansionFactor())
        return;

    if (wasEmpty && !m_disallowLayoutViewportHeightExpansionReasons.isEmpty())
        send(Messages::WebPageProxy::SetAllowsLayoutViewportHeightExpansion(false));
}

void WebPage::removeReasonsToDisallowLayoutViewportHeightExpansion(OptionSet<DisallowLayoutViewportHeightExpansionReason> reasons)
{
    bool wasEmpty = m_disallowLayoutViewportHeightExpansionReasons.isEmpty();
    m_disallowLayoutViewportHeightExpansionReasons.remove(reasons);

    if (!m_page->settings().layoutViewportHeightExpansionFactor())
        return;

    if (!wasEmpty && m_disallowLayoutViewportHeightExpansionReasons.isEmpty())
        send(Messages::WebPageProxy::SetAllowsLayoutViewportHeightExpansion(true));
}

void WebPage::hasActiveNowPlayingSessionChanged(bool hasActiveNowPlayingSession)
{
    send(Messages::WebPageProxy::HasActiveNowPlayingSessionChanged(hasActiveNowPlayingSession));
}

void WebPage::simulateClickOverFirstMatchingTextInViewportWithUserInteraction(const String& targetText, CompletionHandler<void(bool)>&& completion)
{
    ASSERT(!targetText.isEmpty());

    RefPtr localMainFrame = m_mainFrame->coreLocalFrame();
    if (!localMainFrame)
        return completion(false);

    RefPtr view = localMainFrame->view();
    if (!view)
        return completion(false);

    RefPtr document = localMainFrame->document();
    if (!document)
        return completion(false);

    RefPtr bodyElement = document->body();
    if (!bodyElement)
        return completion(false);

    struct Candidate {
        Ref<HTMLElement> target;
        IntPoint location;
    };

    Vector<Candidate> candidates;

    auto removeNonHitTestableCandidates = [&] {
        candidates.removeAllMatching([&](auto& targetAndLocation) {
            auto& [target, location] = targetAndLocation;
            auto result = localMainFrame->eventHandler().hitTestResultAtPoint(location, {
                HitTestRequest::Type::ReadOnly,
                HitTestRequest::Type::Active,
            });
            RefPtr innerNode = result.innerNonSharedNode();
            return !innerNode || !target->isShadowIncludingInclusiveAncestorOf(innerNode.get());
        });
    };

    static constexpr OptionSet findOptions = {
        FindOption::CaseInsensitive,
        FindOption::AtWordStarts,
        FindOption::TreatMedialCapitalAsWordStart,
        FindOption::DoNotRevealSelection,
        FindOption::DoNotSetSelection,
    };

    auto unobscuredContentRect = view->unobscuredContentRect();
    auto searchRange = makeRangeSelectingNodeContents(*bodyElement);
    while (is_lt(treeOrder<ComposedTree>(searchRange.start, searchRange.end))) {
        auto range = findPlainText(searchRange, targetText, findOptions);

        if (range.collapsed())
            break;

        searchRange.start = range.end;

        RefPtr target = [&] -> RefPtr<HTMLElement> {
            for (RefPtr ancestor = range.start.container.ptr(); ancestor; ancestor = ancestor->parentElementInComposedTree()) {
                RefPtr element = dynamicDowncast<HTMLElement>(*ancestor);
                if (!element)
                    continue;

                if (element->willRespondToMouseClickEvents() || element->isLink())
                    return element;
            }
            return { };
        }();

        if (!target)
            continue;

        auto textRects = RenderObject::absoluteBorderAndTextRects(range, {
            RenderObject::BoundingRectBehavior::RespectClipping,
            RenderObject::BoundingRectBehavior::UseVisibleBounds,
            RenderObject::BoundingRectBehavior::IgnoreTinyRects,
            RenderObject::BoundingRectBehavior::IgnoreEmptyTextSelections,
        });

        auto indexOfFirstRelevantTextRect = textRects.findIf([&](auto& textRect) {
            return unobscuredContentRect.intersects(enclosingIntRect(textRect));
        });

        if (indexOfFirstRelevantTextRect == notFound)
            continue;

        candidates.append({ target.releaseNonNull(), roundedIntPoint(textRects[indexOfFirstRelevantTextRect].center()) });
    }

    removeNonHitTestableCandidates();
    WEBPAGE_RELEASE_LOG(MouseHandling, "Simulating click - found %zu candidate(s) from visible text", candidates.size());

    if (candidates.isEmpty()) {
        // Fall back to checking DOM attributes and accessibility labels.
        auto hitTestResult = HitTestResult { LayoutRect { unobscuredContentRect } };
        document->hitTest({ HitTestSource::User, { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::CollectMultipleElements } }, hitTestResult);
        for (auto& node : hitTestResult.listBasedTestResult()) {
            RefPtr element = dynamicDowncast<HTMLElement>(node);
            if (!element)
                continue;

            bool isCandidate = false;
            if (auto ariaLabel = element->attributeWithoutSynchronization(HTMLNames::aria_labelAttr); !ariaLabel.isEmpty())
                isCandidate = containsPlainText(ariaLabel.string(), targetText, findOptions);

            if (!isCandidate) {
                if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); input && (input->isSubmitButton() || input->isTextButton())) {
                    if (auto value = input->visibleValue(); !value.isEmpty())
                        isCandidate = containsPlainText(value, targetText, findOptions);
                }
            }

            if (!isCandidate)
                continue;

            if (auto rendererAndBounds = element->boundingAbsoluteRectWithoutLayout())
                candidates.append({ element.releaseNonNull(), enclosingIntRect(rendererAndBounds->second).center() });
        }

        removeNonHitTestableCandidates();
        WEBPAGE_RELEASE_LOG(MouseHandling, "Simulating click - found %zu candidate(s) from DOM attributes", candidates.size());
    }

    if (candidates.isEmpty()) {
        WEBPAGE_RELEASE_LOG(MouseHandling, "Simulating click - no matches found");
        return completion(false);
    }

    if (candidates.size() > 1) {
        WEBPAGE_RELEASE_LOG(MouseHandling, "Simulating click - too many matches found (%zu)", candidates.size());
        // FIXME: We'll want to add a way to disambiguate between multiple matches in the future. For now, just exit without
        // trying to simulate a click.
        return completion(false);
    }

    auto& [target, location] = candidates.first();

    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    auto locationInWindow = view->contentsToWindow(location);
    auto makeSyntheticEvent = [&](PlatformEvent::Type type) -> PlatformMouseEvent {
        return { locationInWindow, locationInWindow, MouseButton::Left, type, 1, { }, MonotonicTime::now(), ForceAtClick, SyntheticClickType::OneFingerTap, mousePointerID };
    };

    WEBPAGE_RELEASE_LOG(MouseHandling, "Simulating click - dispatching events");
    localMainFrame->eventHandler().handleMousePressEvent(makeSyntheticEvent(PlatformEvent::Type::MousePressed)).wasHandled();
    if (m_isClosed)
        return completion(false);

    localMainFrame->eventHandler().handleMouseReleaseEvent(makeSyntheticEvent(PlatformEvent::Type::MouseReleased)).wasHandled();
    completion(true);
}

#if ENABLE(MEDIA_STREAM)
void WebPage::updateCaptureState(const WebCore::Document& document, bool isActive, WebCore::MediaProducerMediaCaptureKind kind, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    RefPtr frame = document.frame();
    if (!frame) {
        completionHandler(WebCore::Exception { ExceptionCode::InvalidStateError, "no frame available"_s });
        return;
    }

    RefPtr webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    sendWithAsyncReply(Messages::WebPageProxy::ValidateCaptureStateUpdate(UserMediaRequestIdentifier::generate(), document.clientOrigin(), webFrame->info(), isActive, kind), [weakThis = WeakPtr { *this }, isActive, kind, completionHandler = WTFMove(completionHandler)] (auto&& error) mutable {
        completionHandler(WTFMove(error));
        if (error)
            return;

        RefPtr webPage = weakThis.get();
        RefPtr page = webPage ? webPage->corePage() : nullptr;
        if (page)
            page->updateCaptureState(isActive, kind);
    });
}
#endif

RefPtr<DrawingArea> WebPage::protectedDrawingArea() const
{
    return m_drawingArea;
}

void WebPage::updateOpener(WebCore::FrameIdentifier frameID, WebCore::FrameIdentifier newOpenerIdentifier)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    RefPtr coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;

    RefPtr newOpener = WebProcess::singleton().webFrame(newOpenerIdentifier);
    if (!newOpener)
        return;
    RefPtr coreNewOpener = newOpener->coreFrame();
    if (!coreNewOpener)
        return;

    coreFrame->updateOpener(*coreNewOpener, WebCore::Frame::NotifyUIProcess::No);
    if (RefPtr provisionalFrame = frame->provisionalFrame())
        provisionalFrame->updateOpener(*coreNewOpener, WebCore::Frame::NotifyUIProcess::No);
}

void WebPage::setFramePrinting(WebCore::FrameIdentifier frameID, bool printing, FloatSize pageSize, FloatSize originalPageSize, float maximumShrinkRatio, AdjustViewSize shouldAdjustViewSize)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    RefPtr coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;
    coreFrame->setPrinting(printing, pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize, WebCore::Frame::NotifyUIProcess::No);
}

bool WebPage::isAlwaysOnLoggingAllowed() const
{
    RefPtr page { protectedCorePage() };
    return page && page->isAlwaysOnLoggingAllowed();
}

#if PLATFORM(IOS_FAMILY)

bool WebPage::canShowWhileLocked() const
{
    return m_page && m_page->canShowWhileLocked();
}

#else

void WebPage::callAfterPendingSyntheticClick(CompletionHandler<void(SyntheticClickResult)>&& completion)
{
    completion(SyntheticClickResult::Failed);
}

#endif

#if HAVE(AUDIT_TOKEN)
void WebPage::setPresentingApplicationAuditTokenAndBundleIdentifier(CoreIPCAuditToken&& auditToken, String&& bundleIdentifier)
{
    RefPtr page = corePage();
    if (!page)
        return;

    page->setPresentingApplicationAuditToken(auditToken.auditToken());
    page->setPresentingApplicationBundleIdentifier(WTFMove(bundleIdentifier));
}
#endif

void WebPage::frameViewLayoutOrVisualViewportChanged(const LocalFrameView& frameView)
{
#if ENABLE(PDF_PLUGIN)
    Ref frame = frameView.frame();
    if (RefPtr plugin = pluginViewForFrame(frame.ptr()))
        plugin->frameViewLayoutOrVisualViewportChanged(frameView.unobscuredContentRect());
#else
    UNUSED_PARAM(frameView);
#endif
}

RefPtr<MediaSessionManagerInterface> WebPage::mediaSessionManager() const
{
    RefPtr page { corePage() };
    return page ? page->mediaSessionManager() : nullptr;

}

MediaSessionManagerInterface* WebPage::mediaSessionManagerIfExists() const
{
    RefPtr page { corePage() };
    return page ? page->mediaSessionManagerIfExists() : nullptr;

}

#if ENABLE(MODEL_ELEMENT)
bool WebPage::shouldDisableModelLoadDelaysForTesting() const
{
    return m_page && m_page->shouldDisableModelLoadDelaysForTesting();
}
#endif

std::unique_ptr<FrameInfoData> WebPage::takeMainFrameNavigationInitiator()
{
    return std::exchange(m_mainFrameNavigationInitiator, nullptr);
}

#if !PLATFORM(IOS_FAMILY)
bool WebPage::hasAccessoryMousePointingDevice() const
{
    return true;
}
#endif

#if ENABLE(VIDEO)
void WebPage::showCaptionDisplaySettingsPreview(HTMLMediaElementIdentifier identifier)
{
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (RefPtr mediaElement = protectedPlaybackSessionManager()->mediaElementWithContextId(identifier))
        mediaElement->showCaptionDisplaySettingsPreview();
#else
    UNUSED_PARAM(identifier);
#endif
}

void WebPage::hideCaptionDisplaySettingsPreview(HTMLMediaElementIdentifier identifier)
{
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (RefPtr mediaElement = protectedPlaybackSessionManager()->mediaElementWithContextId(identifier))
        mediaElement->hideCaptionDisplaySettingsPreview();
#else
    UNUSED_PARAM(identifier);
#endif
}
#endif

} // namespace WebKit

#undef WEBPAGE_RELEASE_LOG
#undef WEBPAGE_RELEASE_LOG_ERROR
