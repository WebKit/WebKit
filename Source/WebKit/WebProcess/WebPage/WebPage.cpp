/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
#include "InjectedBundleScriptWorld.h"
#include "LibWebRTCCodecs.h"
#include "LibWebRTCProvider.h"
#include "LoadParameters.h"
#include "Logging.h"
#include "MediaKeySystemPermissionRequestManager.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NotificationPermissionRequestManager.h"
#include "PageBanner.h"
#include "PluginView.h"
#include "PolicyDecision.h"
#include "PrintInfo.h"
#include "ProvisionalFrameCreationParameters.h"
#include "RemoteNativeImageBackendProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteScrollingCoordinator.h"
#include "RemoteWebInspectorUI.h"
#include "RemoteWebInspectorUIMessages.h"
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
#include "WebCoreArgumentCoders.h"
#include "WebCryptoClient.h"
#include "WebDataListSuggestionPicker.h"
#include "WebDatabaseProvider.h"
#include "WebDateTimeChooser.h"
#include "WebDiagnosticLoggingClient.h"
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
#include "WebGamepadProvider.h"
#include "WebGeolocationClient.h"
#include "WebHistoryItemClient.h"
#include "WebHitTestResultData.h"
#include "WebImage.h"
#include "WebInspectorClient.h"
#include "WebInspectorInternal.h"
#include "WebInspectorMessages.h"
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
#include "WebPageInspectorTargetController.h"
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
#include "WebProcessSyncClient.h"
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
#include <WebCore/ContextMenuController.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/CrossOriginOpenerPolicy.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/DataTransfer.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DocumentStorageAccess.h>
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
#include <WebCore/FontAttributeChanges.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/FormState.h>
#include <WebCore/FragmentDirectiveParser.h>
#include <WebCore/FragmentDirectiveRangeFinder.h>
#include <WebCore/FragmentDirectiveUtilities.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/HTMLAttachmentElement.h>
#include <WebCore/HTMLBodyElement.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLImageElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTMLSelectElement.h>
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
#include <WebCore/InspectorController.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaDocument.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NavigationScheduler.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/NotificationController.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PingLoader.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PointerCaptureController.h>
#include <WebCore/PrintContext.h>
#include <WebCore/ProcessCapabilities.h>
#include <WebCore/PromisedAttachmentInfo.h>
#include <WebCore/Quirks.h>
#include <WebCore/Range.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RemoteDOMWindow.h>
#include <WebCore/RemoteFrame.h>
#include <WebCore/RemoteFrameClient.h>
#include <WebCore/RemoteFrameView.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <WebCore/RenderImage.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderVideo.h>
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
#include <WebCore/TextExtraction.h>
#include <WebCore/TextIterator.h>
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
#include <WebCore/WritingDirection.h>
#include <WebCore/markup.h>
#include <pal/SessionID.h>
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
#include "RemoteLayerTreeTransaction.h"
#include "RemoteObjectRegistryMessages.h"
#include "TextAnimationController.h"
#include "TextCheckingControllerProxy.h"
#include "VideoPresentationManager.h"
#include "WKStringCF.h"
#include "WebRemoteObjectRegistry.h"
#include <WebCore/ImageUtilities.h>
#include <WebCore/LegacyWebArchive.h>
#include <WebCore/SVGImage.h>
#include <WebCore/TextPlaceholderElement.h>
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

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if ENABLE(DATA_DETECTION)
#include <WebCore/DataDetection.h>
#include <WebCore/DataDetectionResultsStorage.h>
#endif

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#endif

#if ENABLE(WEB_AUTHN)
#include "DigitalCredentialsCoordinator.h"
#include "WebAuthenticatorCoordinator.h"
#include <WebCore/AuthenticatorCoordinator.h>
#include <WebCore/CredentialRequestCoordinatorClient.h>
#endif

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

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionControllerProxy.h"
#endif

#if ENABLE(WEBXR) && !USE(OPENXR)
#include "PlatformXRSystemProxy.h"
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#import <WebCore/AcceleratedEffectStackUpdater.h>
#endif

#if ENABLE(PDF_HUD)
#include "PDFPluginBase.h"
#endif

#if PLATFORM(COCOA)
#include <pal/cf/CoreTextSoftLink.h>
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
#define WEBPAGE_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)

class SendStopResponsivenessTimer {
public:
    ~SendStopResponsivenessTimer()
    {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopResponsivenessTimer(), 0);
    }
};

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageCounter, ("WebPage"));

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

static PageConfiguration::MainFrameCreationParameters mainFrameCreationParameters(Ref<WebFrame>&& mainFrame, auto frameType, auto initialSandboxFlags)
{
    auto invalidator = mainFrame->makeInvalidator();
    switch (frameType) {
    case Frame::FrameType::Local:
        return PageConfiguration::LocalMainFrameCreationParameters {
            { [mainFrame = WTFMove(mainFrame), invalidator = WTFMove(invalidator)] (auto& localFrame, auto& frameLoader) mutable {
                return makeUniqueRefWithoutRefCountedCheck<WebLocalFrameLoaderClient>(localFrame, frameLoader, WTFMove(mainFrame), WTFMove(invalidator));
            } },
            initialSandboxFlags
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
    : m_identifier(pageID)
    , m_viewSize(parameters.viewSize)
    , m_layerHostingMode(parameters.layerHostingMode)
    , m_drawingArea(DrawingArea::create(*this, parameters))
    , m_webPageTesting(makeUnique<WebPageTesting>(*this))
    , m_mainFrame(WebFrame::create(*this, parameters.mainFrameIdentifier))
    , m_drawingAreaType(parameters.drawingAreaType)
    , m_alwaysShowsHorizontalScroller { parameters.alwaysShowsHorizontalScroller }
    , m_alwaysShowsVerticalScroller { parameters.alwaysShowsVerticalScroller }
    , m_shouldRenderCanvasInGPUProcess { parameters.shouldRenderCanvasInGPUProcess }
    , m_shouldRenderDOMInGPUProcess { parameters.shouldRenderDOMInGPUProcess }
    , m_shouldPlayMediaInGPUProcess { parameters.shouldPlayMediaInGPUProcess }
#if ENABLE(WEBGL)
    , m_shouldRenderWebGLInGPUProcess { parameters.shouldRenderWebGLInGPUProcess}
#endif
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    , m_textCheckingControllerProxy(makeUniqueRef<TextCheckingControllerProxy>(*this))
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK)
    , m_viewGestureGeometryCollector(ViewGestureGeometryCollector::create(*this))
#elif PLATFORM(GTK)
    , m_accessibilityObject(nullptr)
#endif
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    , m_nativeWindowHandle(parameters.nativeWindowHandle)
#endif
    , m_setCanStartMediaTimer(RunLoop::main(), this, &WebPage::setCanStartMediaTimerFired)
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
    , m_inspectorTargetController(makeUnique<WebPageInspectorTargetController>(*this))
    , m_userContentController(WebUserContentController::getOrCreate(parameters.userContentControllerParameters.identifier))
    , m_screenOrientationManager(makeUniqueRef<WebScreenOrientationManager>(*this))
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
    , m_userActivity("App nap disabled for page due to user activity"_s)
    , m_userInterfaceLayoutDirection(parameters.userInterfaceLayoutDirection)
    , m_overrideContentSecurityPolicy { parameters.overrideContentSecurityPolicy }
    , m_cpuLimit(parameters.cpuLimit)
#if USE(WPE_RENDERER)
    , m_hostFileDescriptor(WTFMove(parameters.hostFileDescriptor))
#endif
    , m_webPageProxyIdentifier(parameters.webPageProxyIdentifier)
#if ENABLE(TEXT_AUTOSIZING)
    , m_textAutoSizingAdjustmentTimer(*this, &WebPage::textAutoSizingAdjustmentTimerFired)
#endif
    , m_overriddenMediaType(parameters.overriddenMediaType)
    , m_processDisplayName(parameters.processDisplayName)
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
    , m_historyItemClient(WebHistoryItemClient::create())
#if ENABLE(WRITING_TOOLS)
    , m_textAnimationController(makeUniqueRef<TextAnimationController>(*this))
#endif
{
    WEBPAGE_RELEASE_LOG(Loading, "constructor:");

#if PLATFORM(IOS) || PLATFORM(VISION)
    setAllowsDeprecatedSynchronousXMLHttpRequestDuringUnload(parameters.allowsDeprecatedSynchronousXMLHttpRequestDuringUnload);
#endif

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
    auto shouldBlockMobileAsset = parameters.store.getBoolValueForKey(WebPreferencesKey::blockMobileAssetInWebContentSandboxKey()) && !WTF::CocoaApplication::isIBooks();
    if (shouldBlockMobileAsset)
        sandbox_enable_state_flag("BlockMobileAssetInWebContentSandbox", *auditToken);
#if PLATFORM(MAC)
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
        && m_drawingAreaType == DrawingAreaType::RemoteLayerTree
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
#if HAVE(CGIMAGESOURCE_DISABLE_HARDWARE_DECODING)
        static bool disabled { false };
        if (!std::exchange(disabled, true)) {
            OSStatus ok = CGImageSourceDisableHardwareDecoding();
            ASSERT_UNUSED(ok, ok == noErr);
        }
#endif
    }
#endif

    m_pageGroup = WebProcess::singleton().webPageGroup(parameters.pageGroupData);

    auto frameType = parameters.remotePageParameters ? Frame::FrameType::Remote : Frame::FrameType::Local;
    ASSERT(!parameters.remotePageParameters || parameters.remotePageParameters->frameTreeParameters.frameID == parameters.mainFrameIdentifier);

    PageConfiguration pageConfiguration(
        pageID,
        WebProcess::singleton().sessionID(),
        makeUniqueRef<WebEditorClient>(this),
        WebSocketProvider::create(parameters.webPageProxyIdentifier),
        createLibWebRTCProvider(*this),
        WebProcess::singleton().cacheStorageProvider(),
        m_userContentController,
        WebBackForwardListProxy::create(*this),
        WebProcess::singleton().cookieJar(),
        makeUniqueRef<WebProgressTrackerClient>(*this),
        mainFrameCreationParameters(m_mainFrame.copyRef(), frameType, parameters.initialSandboxFlags),
        m_mainFrame->frameID(),
        frameFromIdentifier(parameters.mainFrameOpenerIdentifier),
        makeUniqueRef<WebSpeechRecognitionProvider>(pageID),
        WebProcess::singleton().broadcastChannelRegistry(),
        makeUniqueRef<WebStorageProvider>(WebProcess::singleton().mediaKeysStorageDirectory(), WebProcess::singleton().mediaKeysStorageSalt()),
        makeUniqueRef<WebModelPlayerProvider>(*this),
        WebProcess::singleton().badgeClient(),
        m_historyItemClient.copyRef(),
#if ENABLE(CONTEXT_MENUS)
        makeUniqueRef<WebContextMenuClient>(this),
#endif
#if ENABLE(APPLE_PAY)
        makeUniqueRef<WebPaymentCoordinator>(*this),
#endif
        makeUniqueRef<WebChromeClient>(*this),
        makeUniqueRef<WebCryptoClient>(this->identifier()),
        makeUniqueRef<WebProcessSyncClient>(*this)
    );

#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient = makeUnique<WebDragClient>(this);
#endif
    pageConfiguration.inspectorClient = makeUnique<WebInspectorClient>(this);
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
    pageConfiguration.pluginInfoProvider = &WebPluginInfoProvider::singleton();
    pageConfiguration.storageNamespaceProvider = WebStorageNamespaceProvider::getOrCreate();
    pageConfiguration.visitedLinkStore = VisitedLinkTableController::getOrCreate(parameters.visitedLinkTableID);

#if ENABLE(WEB_AUTHN)
    pageConfiguration.authenticatorCoordinatorClient = makeUnique<WebAuthenticatorCoordinator>(*this);
    pageConfiguration.credentialRequestCoordinatorClient = makeUnique<DigitalCredentialsCoordinator>(*this);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    pageConfiguration.applicationManifest = parameters.applicationManifest;
#endif
    
#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    pageConfiguration.deviceOrientationUpdateProvider = WebDeviceOrientationUpdateProvider::create(*this);
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
    if (parameters.webExtensionControllerParameters)
        m_webExtensionController = WebExtensionControllerProxy::getOrCreate(parameters.webExtensionControllerParameters.value(), this);
#endif

    m_corsDisablingPatterns = WTFMove(parameters.corsDisablingPatterns);
    if (!m_corsDisablingPatterns.isEmpty())
        synchronizeCORSDisablingPatternsWithNetworkProcess();
    pageConfiguration.corsDisablingPatterns = parseAndAllowAccessToCORSDisablingPatterns(m_corsDisablingPatterns);

    pageConfiguration.maskedURLSchemes = parameters.maskedURLSchemes;
    pageConfiguration.userScriptsShouldWaitUntilNotification = parameters.userScriptsShouldWaitUntilNotification;
    pageConfiguration.loadsSubresources = parameters.loadsSubresources;
    pageConfiguration.allowedNetworkHosts = parameters.allowedNetworkHosts;
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

#if HAVE(HOSTED_CORE_ANIMATION)
    if (parameters.acceleratedCompositingPort)
        WebProcess::singleton().setCompositingRenderServerPort(WTFMove(parameters.acceleratedCompositingPort));
#endif

#if PLATFORM(IOS_FAMILY)
    pageConfiguration.canShowWhileLocked = parameters.canShowWhileLocked;
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    pageConfiguration.gamepadAccessRequiresExplicitConsent = parameters.gamepadAccessRequiresExplicitConsent;
#endif

    m_page = Page::create(WTFMove(pageConfiguration));

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
    m_page->setDeviceScaleFactor(parameters.deviceScaleFactor);

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
    m_page->settings().setScrollingCoordinatorEnabled(m_useAsyncScrolling);
#endif

    // Disable Back/Forward cache expiration in the WebContent process since management happens in the UIProcess
    // in modern WebKit.
    m_page->settings().setBackForwardCacheExpirationInterval(Seconds::infinity());

    m_mainFrame->initWithCoreMainFrame(*this, m_page->mainFrame());

    if (auto& remotePageParameters = parameters.remotePageParameters) {
        for (auto& childParameters : remotePageParameters->frameTreeParameters.children)
            constructFrameTree(m_mainFrame.get(), childParameters);
        m_page->setMainFrameURL(remotePageParameters->initialMainDocumentURL);
        if (auto websitePolicies = remotePageParameters->websitePoliciesData) {
            if (auto* remoteMainFrameClient = m_mainFrame->remoteFrameClient())
                remoteMainFrameClient->applyWebsitePolicies(WTFMove(*remotePageParameters->websitePoliciesData));
        }
    }

    drawingArea->updatePreferences(parameters.store);

    setBackgroundExtendsBeyondPage(parameters.backgroundExtendsBeyondPage);
    didSetPageZoomFactor(parameters.pageZoomFactor);
    didSetTextZoomFactor(parameters.textZoomFactor);

    // FIXME: These should use makeUnique and makeUniqueRef instead of new.
#if ENABLE(GEOLOCATION)
    WebCore::provideGeolocationTo(m_page.get(), *new WebGeolocationClient(*this));
#endif
#if ENABLE(NOTIFICATIONS)
    WebCore::provideNotification(m_page.get(), new WebNotificationClient(this));
#endif
#if ENABLE(MEDIA_STREAM)
    WebCore::provideUserMediaTo(m_page.get(), new WebUserMediaClient(*this));
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    WebCore::provideMediaKeySystemTo(*m_page.get(), *new WebMediaKeySystemClient(*this));
#endif

    m_page->setControlledByAutomation(parameters.controlledByAutomation);
    m_page->setHasResourceLoadClient(parameters.hasResourceLoadClient);

    m_page->setCanStartMedia(false);
    m_mayStartMediaWhenInWindow = parameters.mayStartMediaWhenInWindow;
    if (parameters.mediaPlaybackIsSuspended)
        m_page->suspendAllMediaPlayback();

    if (parameters.openedByDOM)
        m_page->setOpenedByDOM();

    m_page->setGroupName(m_pageGroup->identifier());
    m_page->setUserInterfaceLayoutDirection(m_userInterfaceLayoutDirection);
#if PLATFORM(IOS_FAMILY)
    m_page->setTextAutosizingWidth(parameters.textAutosizingWidth);
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
    setUseSystemAppearance(parameters.useSystemAppearance);
    setUseFormSemanticContext(parameters.useFormSemanticContext);
    setHeaderBannerHeight(parameters.headerBannerHeight);
    setFooterBannerHeight(parameters.footerBannerHeight);
    if (parameters.viewWindowCoordinates)
        windowAndViewFramesChanged(*parameters.viewWindowCoordinates);
#endif

    // If the page is created off-screen, its visibilityState should be prerender.
    m_page->setActivityState(m_activityState);
    if (!isVisible())
        m_page->setIsPrerender();

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

    setTopContentInset(parameters.topContentInset);

    m_userAgent = parameters.userAgent;

    setMediaVolume(parameters.mediaVolume);

    setMuted(parameters.muted, [] { });

    // We use the DidFirstVisuallyNonEmptyLayout milestone to determine when to unfreeze the layer tree.
    // We use LayoutMilestone::DidFirstMeaningfulPaint to generte WKPageLoadTiming.
    m_page->addLayoutMilestones({ WebCore::LayoutMilestone::DidFirstLayout, WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout, LayoutMilestone::DidFirstMeaningfulPaint });

    auto& webProcess = WebProcess::singleton();
    webProcess.addMessageReceiver(Messages::WebPage::messageReceiverName(), m_identifier, *this);

    // FIXME: This should be done in the object constructors, and the objects themselves should be message receivers.
    webProcess.addMessageReceiver(Messages::WebInspector::messageReceiverName(), m_identifier, *this);
    webProcess.addMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_identifier, *this);
    webProcess.addMessageReceiver(Messages::RemoteWebInspectorUI::messageReceiverName(), m_identifier, *this);
#if ENABLE(FULLSCREEN_API)
    webProcess.addMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_identifier, *this);
#endif

#ifndef NDEBUG
    webPageCounter.increment();
#endif

#if ENABLE(SCROLLING_THREAD)
    if (m_useAsyncScrolling)
        drawingArea->registerScrollingTree();
#endif

    for (auto& mimeType : parameters.mimeTypesWithCustomContentProviders)
        m_mimeTypesWithCustomContentProviders.add(mimeType);

    if (parameters.viewScaleFactor != 1)
        scaleView(parameters.viewScaleFactor);

    m_page->addLayoutMilestones(parameters.observedLayoutMilestones);

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

#if ENABLE(WEB_RTC)
    if (!parameters.iceCandidateFilteringEnabled)
        m_page->disableICECandidateFiltering();
#if USE(LIBWEBRTC)
    if (parameters.enumeratingAllNetworkInterfacesEnabled) {
        auto& rtcProvider = static_cast<LibWebRTCProvider&>(m_page->webRTCProvider());
        rtcProvider.enableEnumeratingAllNetworkInterfaces();
    }
    if (parameters.store.getBoolValueForKey(WebPreferencesKey::enumeratingVisibleNetworkInterfacesEnabledKey())) {
        auto& rtcProvider = static_cast<LibWebRTCProvider&>(m_page->webRTCProvider());
        rtcProvider.enableEnumeratingVisibleNetworkInterfaces();
    }
#endif
#endif

    for (const auto& iterator : parameters.urlSchemeHandlers)
        registerURLSchemeHandler(iterator.value, iterator.key);
    for (auto& scheme : parameters.urlSchemesWithLegacyCustomProtocolHandlers)
        LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler({ scheme });

    m_userContentController->addContentWorlds(parameters.userContentControllerParameters.userContentWorlds);
    m_userContentController->addUserScripts(WTFMove(parameters.userContentControllerParameters.userScripts), InjectUserScriptImmediately::No);
    m_userContentController->addUserStyleSheets(parameters.userContentControllerParameters.userStyleSheets);
    m_userContentController->addUserScriptMessageHandlers(parameters.userContentControllerParameters.messageHandlers);
#if ENABLE(CONTENT_EXTENSIONS)
    m_userContentController->addContentRuleLists(WTFMove(parameters.userContentControllerParameters.contentRuleLists));
#endif

#if PLATFORM(IOS_FAMILY)
    setViewportConfigurationViewLayoutSize(parameters.viewportConfigurationViewLayoutSize, parameters.viewportConfigurationLayoutSizeScaleFactorFromClient, parameters.viewportConfigurationMinimumEffectiveDeviceWidth);
#endif

#if USE(AUDIO_SESSION)
    PlatformMediaSessionManager::setShouldDeactivateAudioSession(true);
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW) && !HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
    m_contextForVisibilityPropagation = LayerHostingContext::createForExternalHostingProcess({
        canShowWhileLocked()
    });
    WEBPAGE_RELEASE_LOG(Process, "WebPage: Created context with ID %u for visibility propagation from UIProcess", m_contextForVisibilityPropagation->contextID());
    send(Messages::WebPageProxy::DidCreateContextInWebProcessForVisibilityPropagation(m_contextForVisibilityPropagation->cachedContextID()));
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW) && !HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(VP9)
    PlatformMediaSessionManager::setShouldEnableVP8Decoder(parameters.shouldEnableVP8Decoder);
    PlatformMediaSessionManager::setShouldEnableVP9Decoder(parameters.shouldEnableVP9Decoder);
#endif

    m_page->setCanUseCredentialStorage(parameters.canUseCredentialStorage);

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
        m_page->applyWindowFeatures(*parameters.windowFeatures);
        m_page->chrome().show();
        m_page->setOpenedByDOM();
    }
}

void WebPage::updateAfterDrawingAreaCreation(const WebPageCreationParameters& parameters)
{
#if PLATFORM(COCOA)
    m_page->settings().setForceCompositingMode(true);
#endif
#if PLATFORM(MAC)
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
    auto frame = WebFrame::createRemoteSubframe(*this, parent, treeCreationParameters.frameID, treeCreationParameters.frameName, treeCreationParameters.openerFrameID);
    for (auto& parameters : treeCreationParameters.children)
        constructFrameTree(frame, parameters);
}

void WebPage::createRemoteSubframe(WebCore::FrameIdentifier parentID, WebCore::FrameIdentifier newChildID, const String& newChildFrameName)
{
    RefPtr parentFrame = WebProcess::singleton().webFrame(parentID);
    if (!parentFrame) {
        ASSERT_NOT_REACHED();
        return;
    }
    WebFrame::createRemoteSubframe(*this, *parentFrame, newChildID, newChildFrameName, std::nullopt);
}

void WebPage::getFrameInfo(WebCore::FrameIdentifier frameID, CompletionHandler<void(std::optional<FrameInfoData>&&)>&& completionHandler)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return completionHandler(std::nullopt);

    completionHandler(frame->info());
}

void WebPage::getFrameTree(CompletionHandler<void(FrameTreeNodeData&&)>&& completionHandler)
{
    completionHandler(m_mainFrame->frameTreeData());
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
    frame->removeFromTree();
}

void WebPage::processSyncDataChangedInAnotherProcess(const WebCore::ProcessSyncData& data)
{
    if (auto* page = corePage())
        page->updateProcessSyncData(data);
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
    if (!m_page->mediaPlaybackExists())
        return completionHandler(MediaPlaybackState::NoMediaPlayback);
    if (m_page->mediaPlaybackIsPaused())
        return completionHandler(MediaPlaybackState::MediaPlaybackPaused);
    if (m_page->mediaPlaybackIsSuspended())
        return completionHandler(MediaPlaybackState::MediaPlaybackSuspended);

    completionHandler(MediaPlaybackState::MediaPlaybackPlaying);
}

void WebPage::pauseAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    m_page->pauseAllMediaPlayback();
    completionHandler();
}

void WebPage::suspendAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    m_page->suspendAllMediaPlayback();
    completionHandler();
}

void WebPage::resumeAllMediaPlayback(CompletionHandler<void()>&& completionHandler)
{
    m_page->resumeAllMediaPlayback();
    completionHandler();
}

void WebPage::suspendAllMediaBuffering()
{
    m_page->suspendAllMediaBuffering();
}

void WebPage::resumeAllMediaBuffering()
{
    m_page->resumeAllMediaBuffering();
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
    if (m_layerHostingMode != parameters.layerHostingMode)
        setLayerHostingMode(parameters.layerHostingMode);

#if HAVE(APP_ACCENT_COLORS)
    setAccentColor(parameters.accentColor);
#if PLATFORM(MAC)
    setAppUsesCustomAccentColor(parameters.appUsesCustomAccentColor);
#endif
#endif

    setUseColorAppearance(parameters.useDarkAppearance, parameters.useElevatedUserInterfaceLevel);

    platformReinitialize();
}

void WebPage::updateThrottleState()
{
    bool isThrottleable = this->isThrottleable();

    // The UserActivity prevents App Nap. So if we want to allow App Nap of the page, stop the activity.
    // If the page should not be app nap'd, start it.
    if (isThrottleable)
        m_userActivity.stop();
    else
        m_userActivity.start();

    if (m_page && m_page->settings().serviceWorkersEnabled()) {
        RunLoop::main().dispatch([isThrottleable] {
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
    for (auto& pluginView : m_pluginViews)
        pluginView.webPageDestroyed();
#endif

#if !PLATFORM(IOS_FAMILY)
    if (m_headerBanner)
        m_headerBanner->detachFromPage();
    if (m_footerBanner)
        m_footerBanner->detachFromPage();
#endif

    WebStorageNamespaceProvider::decrementUseCount(sessionStorageNamespaceIdentifier());

#ifndef NDEBUG
    webPageCounter.decrement();
#endif

#if ENABLE(GPU_PROCESS) && HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (auto* gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->destroyVisibilityPropagationContextForPage(*this);
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS) && HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (auto* modelProcessConnection = WebProcess::singleton().existingModelProcessConnection())
        modelProcessConnection->destroyVisibilityPropagationContextForPage(*this);
#endif // ENABLE(MODEL_PROCESS) && HAVE(VISIBILITY_PROPAGATION_VIEW)
    
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_playbackSessionManager)
        m_playbackSessionManager->invalidate();

    if (m_videoPresentationManager)
        m_videoPresentationManager->invalidate();
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

#if ENABLE(FULLSCREEN_API)
void WebPage::initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClientBase* client)
{
    m_fullScreenClient.initialize(client);
}
#endif

bool WebPage::hasPendingEditorStateUpdate() const
{
    return m_pendingEditorStateUpdateStatus != PendingEditorStateUpdateStatus::NotScheduled;
}

EditorState WebPage::editorState(ShouldPerformLayout shouldPerformLayout) const
{
    // Always return an EditorState with a valid identifier or it will fail to decode and this process will be terminated.
    EditorState result;
    result.identifier = m_lastEditorStateIdentifier.increment();

    // Ref the frame because this function may perform layout, which may cause frame destruction.
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return result;

    auto sanitizeEditorStateOnceCreated = makeScopeExit([&result] {
        result.clipOwnedRectExtentsToNumericLimits();
    });

#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = focusedPluginViewForFrame(*frame)) {
        if (!pluginView->selectionString().isNull()) {
            result.selectionIsNone = false;
            result.selectionIsRange = true;
            result.isInPlugin = true;
            return result;
        }
    }
#endif

    const VisibleSelection& selection = frame->selection().selection();
    auto& editor = frame->editor();

    result.selectionIsNone = selection.isNone();
    result.selectionIsRange = selection.isRange();
    result.isContentEditable = selection.hasEditableStyle();
    result.isContentRichlyEditable = selection.isContentRichlyEditable();
    result.isInPasswordField = selection.isInPasswordField();
    result.hasComposition = editor.hasComposition();
    result.shouldIgnoreSelectionChanges = editor.ignoreSelectionChanges() || (editor.client() && !editor.client()->shouldRevealCurrentSelectionAfterInsertion());
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

    if (auto* frameView = document->view(); frameView && !frameView->needsLayout()) {
        if (!result.postLayoutData)
            result.postLayoutData = std::optional<EditorState::PostLayoutData> { EditorState::PostLayoutData { } };
        result.postLayoutData->canCut = editor.canCut();
        result.postLayoutData->canCopy = editor.canCopy();
        result.postLayoutData->canPaste = editor.canEdit();

        if (!result.visualData)
            result.visualData = std::optional<EditorState::VisualData> { EditorState::VisualData { } };

        if (m_needsFontAttributes)
            result.postLayoutData->fontAttributes = editor.fontAttributesAtSelectionStart();
    }

    getPlatformEditorState(*frame, result);

    return result;
}

void WebPage::changeFontAttributes(WebCore::FontAttributeChanges&& changes)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable())
        frame->editor().applyStyleToSelection(changes.createEditingStyle(), changes.editAction(), Editor::ColorFilterMode::InvertColor);
}

void WebPage::changeFont(WebCore::FontChanges&& changes)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable())
        frame->editor().applyStyleToSelection(changes.createEditingStyle(), EditAction::SetFont, Editor::ColorFilterMode::InvertColor);
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
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    RefPtr document = localMainFrame ? localMainFrame->document() : nullptr;
    return document && document->quirks().shouldDispatchSyntheticMouseEventsWhenModifyingSelection();
}

#if !PLATFORM(IOS_FAMILY)

void WebPage::platformDidSelectAll()
{
}

#endif // !PLATFORM(IOS_FAMILY)

#if !PLATFORM(COCOA)
void WebPage::bindRemoteAccessibilityFrames(int, WebCore::FrameIdentifier, Vector<uint8_t>, CompletionHandler<void(Vector<uint8_t>, int)>&&)
{
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

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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
    return externalRepresentation(m_mainFrame->coreLocalFrame(), toRenderAsTextFlags(options));
}

String WebPage::renderTreeExternalRepresentationForPrinting() const
{
    return externalRepresentation(m_mainFrame->coreLocalFrame(), { RenderAsTextFlag::PrintingMode });
}

uint64_t WebPage::renderTreeSize() const
{
    if (!m_page)
        return 0;
    return m_page->renderTreeSize();
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
    if (auto* view = localMainFrameView())
        return view->isTrackingRepaints();

    return false;
}

Ref<API::Array> WebPage::trackedRepaintRects()
{
    auto* view = localMainFrameView();
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
    return static_cast<PluginView*>(document->pluginWidget());
}

PluginView* WebPage::mainFramePlugIn() const
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(mainFrame());
    return pluginViewForFrame(localMainFrame.get());
}

#endif

void WebPage::executeEditingCommand(const String& commandName, const String& argument)
{
    platformWillPerformEditingCommand();

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = focusedPluginViewForFrame(*frame)) {
        pluginView->handleEditingCommand(commandName, argument);
        return;
    }
#endif
    
    frame->editor().command(commandName).execute(argument);
}

void WebPage::setEditable(bool editable)
{
    m_page->setEditable(editable);
    m_page->setTabKeyCyclesThroughElements(!editable);
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (editable) {
        frame->editor().applyEditingStyleToBodyElement();
        // If the page is made editable and the selection is empty, set it to something.
        if (frame->selection().isNone())
            frame->selection().setSelectionFromNone();
    }
}

void WebPage::increaseListLevel()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().increaseSelectionListLevel();
}

void WebPage::decreaseListLevel()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().decreaseSelectionListLevel();
}

void WebPage::changeListType()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().changeSelectionListType();
}

void WebPage::setBaseWritingDirection(WritingDirection direction)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().setBaseWritingDirection(direction);
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

    WEBPAGE_RELEASE_LOG(Loading, "close:");

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ClearPageSpecificData(m_identifier), 0);

    m_isClosed = true;

    // If there is still no URL, then we never loaded anything in this page, so nothing to report.
    if (!mainWebFrame().url().isEmpty())
        reportUsedFeatures();

    if (WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->willDestroyPage(Ref { *this });

    if (m_inspector) {
        m_inspector->disconnectFromPage();
        m_inspector = nullptr;
    }

    m_page->inspectorController().disconnectAllFrontends();

#if ENABLE(FULLSCREEN_API)
    if (auto manager = std::exchange(m_fullScreenManager, { }))
        manager->invalidate();
#endif

    if (m_activePopupMenu) {
        m_activePopupMenu->disconnectFromPage();
        m_activePopupMenu = nullptr;
    }

    if (m_activeOpenPanelResultListener) {
        m_activeOpenPanelResultListener->disconnectFromPage();
        m_activeOpenPanelResultListener = nullptr;
    }

#if ENABLE(INPUT_TYPE_COLOR)
    if (RefPtr activeColorChooser = m_activeColorChooser.get()) {
        activeColorChooser->disconnectFromPage();
        m_activeColorChooser = nullptr;
    }
#endif

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
#if ENABLE(FULLSCREEN_API)
    m_fullScreenClient.initialize(0);
#endif

    m_printContext = nullptr;
    if (RefPtr localFrame = m_mainFrame->coreLocalFrame())
        localFrame->loader().detachFromParent();

#if ENABLE(SCROLLING_THREAD)
    if (m_useAsyncScrolling)
        protectedDrawingArea()->unregisterScrollingTree();
#endif

    m_drawingArea = nullptr;
    m_webPageTesting = nullptr;
    m_page = nullptr;

    bool isRunningModal = m_isRunningModal;
    m_isRunningModal = false;

#if PLATFORM(COCOA)
    if (m_remoteObjectRegistry)
        m_remoteObjectRegistry->close();
    ASSERT(!m_remoteObjectRegistry);
#endif

    auto& webProcess = WebProcess::singleton();
    webProcess.removeMessageReceiver(Messages::WebPage::messageReceiverName(), m_identifier);
    // FIXME: This should be done in the object destructors, and the objects themselves should be message receivers.
    webProcess.removeMessageReceiver(Messages::WebInspector::messageReceiverName(), m_identifier);
    webProcess.removeMessageReceiver(Messages::WebInspectorUI::messageReceiverName(), m_identifier);
    webProcess.removeMessageReceiver(Messages::RemoteWebInspectorUI::messageReceiverName(), m_identifier);
#if ENABLE(FULLSCREEN_API)
    webProcess.removeMessageReceiver(Messages::WebFullScreenManager::messageReceiverName(), m_identifier);
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK)
    m_viewGestureGeometryCollector = nullptr;
#endif

    stopObservingNowPlayingMetadata();

    // The WebPage can be destroyed by this call.
    WebProcess::singleton().removeWebPage(m_identifier);

    WebProcess::singleton().updateActivePages(m_processDisplayName);

    if (isRunningModal)
        RunLoop::main().stop();
}

void WebPage::tryClose(CompletionHandler<void(bool)>&& completionHandler)
{
    auto* coreFrame = m_mainFrame->coreLocalFrame();
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

void WebPage::suspendForProcessSwap()
{
    flushDeferredDidReceiveMouseEvent();

    auto failedToSuspend = [this, protectedThis = Ref { *this }] {
        send(Messages::WebPageProxy::DidFailToSuspendAfterProcessSwap());
    };

    // FIXME: Make this work if the main frame is not a LocalFrame.
    RefPtr currentHistoryItem = m_mainFrame->coreLocalFrame()->history().currentItem();
    if (!currentHistoryItem) {
        failedToSuspend();
        return;
    }

    if (!BackForwardCache::singleton().addIfCacheable(*currentHistoryItem, corePage())) {
        failedToSuspend();
        return;
    }

    // Back/forward cache does not break the opener link for the main frame (only does so for the subframes) because the
    // main frame is normally re-used for the navigation. However, in the case of process-swapping, the main frame
    // is now hosted in another process and the one in this process is in the cache.
    if (m_mainFrame->coreLocalFrame())
        m_mainFrame->coreLocalFrame()->detachFromAllOpenedFrames();

    send(Messages::WebPageProxy::DidSuspendAfterProcessSwap());
}

void WebPage::loadURLInFrame(URL&& url, const String& referrer, FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreLocalFrame = frame->coreLocalFrame();
    coreLocalFrame->loader().load(FrameLoadRequest(*coreLocalFrame, ResourceRequest(url, referrer)));
}

void WebPage::loadDataInFrame(std::span<const uint8_t> data, String&& type, String&& encodingName, URL&& baseURL, FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    ASSERT(&mainWebFrame() != frame);

    Ref sharedBuffer = SharedBuffer::create(data);
    ResourceResponse response(baseURL, type, sharedBuffer->size(), encodingName);
    SubstituteData substituteData(WTFMove(sharedBuffer), baseURL, WTFMove(response), SubstituteData::SessionHistoryVisibility::Hidden);
    frame->coreLocalFrame()->loader().load(FrameLoadRequest(*frame->coreLocalFrame(), ResourceRequest(baseURL), WTFMove(substituteData)));
}

#if !PLATFORM(COCOA)
void WebPage::platformDidReceiveLoadParameters(const LoadParameters& loadParameters)
{
}
#endif

void WebPage::createProvisionalFrame(ProvisionalFrameCreationParameters&& parameters, WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    ASSERT(frame->page() == this);
    frame->createProvisionalFrame(WTFMove(parameters));
}

void WebPage::destroyProvisionalFrame(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    ASSERT(frame->page() == this);
    frame->destroyProvisionalFrame();
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
    WEBPAGE_RELEASE_LOG(Loading, "loadRequest: navigationID=%" PRIu64 ", shouldTreatAsContinuingLoad=%u, lastNavigationWasAppInitiated=%d, existingNetworkResourceLoadIdentifierToResume=%" PRIu64, loadParameters.navigationID ? loadParameters.navigationID->toUInt64() : 0, static_cast<unsigned>(loadParameters.shouldTreatAsContinuingLoad), loadParameters.request.isAppInitiated(), loadParameters.existingNetworkResourceLoadIdentifierToResume ? loadParameters.existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);

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
    m_pendingWebsitePolicies = WTFMove(loadParameters.websitePolicies);

    m_sandboxExtensionTracker.beginLoad(WTFMove(loadParameters.sandboxExtensionHandle));

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient->willLoadURLRequest(*this, loadParameters.request, WebProcess::singleton().transformHandlesToObjects(loadParameters.userData.object()).get());

    platformDidReceiveLoadParameters(loadParameters);

    // Initate the load in WebCore.
    ASSERT(localFrame->document());
    FrameLoadRequest frameLoadRequest { *localFrame, loadParameters.request };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(loadParameters.shouldOpenExternalURLsPolicy);
    frameLoadRequest.setShouldTreatAsContinuingLoad(loadParameters.shouldTreatAsContinuingLoad);
    frameLoadRequest.setLockHistory(loadParameters.lockHistory);
    frameLoadRequest.setLockBackForwardList(loadParameters.lockBackForwardList);
    frameLoadRequest.setClientRedirectSourceForHistory(loadParameters.clientRedirectSourceForHistory);
    if (loadParameters.isRequestFromClientOrUserInput)
        frameLoadRequest.setIsRequestFromClientOrUserInput();
    if (loadParameters.advancedPrivacyProtections)
        frameLoadRequest.setAdvancedPrivacyProtections(*loadParameters.advancedPrivacyProtections);

    if (loadParameters.effectiveSandboxFlags)
        localFrame->updateSandboxFlags(loadParameters.effectiveSandboxFlags, Frame::NotifyUIProcess::No);

    if (auto onwerPermissionsPolicy = std::exchange(loadParameters.ownerPermissionsPolicy, { }))
        localFrame->setOwnerPermissionsPolicy(WTFMove(*onwerPermissionsPolicy));

    localFrame->loader().setHTTPFallbackInProgress(loadParameters.isPerformingHTTPFallback);

    localFrame->loader().load(WTFMove(frameLoadRequest));

    ASSERT(!m_pendingNavigationID);
    ASSERT(!m_pendingWebsitePolicies);
}

// LoadRequestWaitingForProcessLaunch should never be sent to the WebProcess. It must always be converted to a LoadRequest message.
void WebPage::loadRequestWaitingForProcessLaunch(LoadParameters&&, URL&&, WebPageProxyIdentifier, bool)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void WebPage::loadDataImpl(std::optional<WebCore::NavigationIdentifier> navigationID, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<WebsitePoliciesData>&& websitePolicies, Ref<FragmentedSharedBuffer>&& sharedBuffer, ResourceRequest&& request, ResourceResponse&& response, const URL& unreachableURL, const UserData& userData, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
#if ENABLE(APP_BOUND_DOMAINS)
    Ref mainFrame = m_mainFrame.copyRef();
    setIsNavigatingToAppBoundDomain(isNavigatingToAppBoundDomain, mainFrame.get());
#else
    UNUSED_PARAM(isNavigatingToAppBoundDomain);
#endif

    SendStopResponsivenessTimer stopper;

    m_pendingNavigationID = navigationID;
    m_pendingWebsitePolicies = WTFMove(websitePolicies);

    SubstituteData substituteData(WTFMove(sharedBuffer), unreachableURL, response, sessionHistoryVisibility);

    // Let the InjectedBundle know we are about to start the load, passing the user data from the UIProcess
    // to all the client to set up any needed state.
    m_loaderClient->willLoadDataRequest(*this, request, substituteData.content(), substituteData.mimeType(), substituteData.textEncoding(), substituteData.failingURL(), WebProcess::singleton().transformHandlesToObjects(userData.object()).get());

    RefPtr localFrame = m_mainFrame->coreLocalFrame() ? m_mainFrame->coreLocalFrame() : m_mainFrame->provisionalFrame();
    if (!localFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Initate the load in WebCore.
    FrameLoadRequest frameLoadRequest(*localFrame, request, substituteData);
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
        baseURL = URL { loadParameters.baseURLString };
        if (baseURL.isValid() && !baseURL.protocolIsInHTTPFamily())
            LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(baseURL.protocol().toString());
    }

    if (loadParameters.isServiceWorkerLoad && corePage())
        corePage()->markAsServiceWorkerPage();

    ResourceResponse response(URL(), loadParameters.MIMEType, sharedBuffer->size(), loadParameters.encodingName);
    loadDataImpl(loadParameters.navigationID, loadParameters.shouldTreatAsContinuingLoad, WTFMove(loadParameters.websitePolicies), sharedBuffer.releaseNonNull(), ResourceRequest(baseURL), WTFMove(response), URL(), loadParameters.userData, loadParameters.isNavigatingToAppBoundDomain, loadParameters.sessionHistoryVisibility, loadParameters.shouldOpenExternalURLsPolicy);
}

void WebPage::loadAlternateHTML(LoadParameters&& loadParameters)
{
    platformDidReceiveLoadParameters(loadParameters);

    URL baseURL = loadParameters.baseURLString.isEmpty() ? aboutBlankURL() : URL { loadParameters.baseURLString };
    URL unreachableURL = loadParameters.unreachableURLString.isEmpty() ? URL() : URL { loadParameters.unreachableURLString };
    URL provisionalLoadErrorURL = loadParameters.provisionalLoadErrorURLString.isEmpty() ? URL() : URL { loadParameters.provisionalLoadErrorURLString };
    RefPtr sharedBuffer = loadParameters.data;
    if (!sharedBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_mainFrame->coreLocalFrame()->loader().setProvisionalLoadErrorBeingHandledURL(provisionalLoadErrorURL);

    ResourceResponse response(URL(), loadParameters.MIMEType, sharedBuffer->size(), loadParameters.encodingName);
    loadDataImpl(loadParameters.navigationID, loadParameters.shouldTreatAsContinuingLoad, WTFMove(loadParameters.websitePolicies), sharedBuffer.releaseNonNull(), ResourceRequest(baseURL), WTFMove(response), unreachableURL, loadParameters.userData, loadParameters.isNavigatingToAppBoundDomain, WebCore::SubstituteData::SessionHistoryVisibility::Hidden);
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
    WEBPAGE_RELEASE_LOG(Loading, "goToBackForwardItem: navigationID=%" PRIu64 ", backForwardItemID=%s, shouldTreatAsContinuingLoad=%u, lastNavigationWasAppInitiated=%d, existingNetworkResourceLoadIdentifierToResume=%" PRIu64, parameters.navigationID.toUInt64(), parameters.frameState->identifier->toString().utf8().data(), static_cast<unsigned>(parameters.shouldTreatAsContinuingLoad), parameters.lastNavigationWasAppInitiated, parameters.existingNetworkResourceLoadIdentifierToResume ? parameters.existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);
    SendStopResponsivenessTimer stopper;

    m_sandboxExtensionTracker.beginLoad(WTFMove(parameters.sandboxExtensionHandle));

    m_lastNavigationWasAppInitiated = parameters.lastNavigationWasAppInitiated;
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(corePage()->mainFrame())) {
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

    ASSERT(!m_pendingNavigationID);
    m_pendingNavigationID = parameters.navigationID;
    m_pendingWebsitePolicies = WTFMove(parameters.websitePolicies);

    Ref targetFrame = m_mainFrame;
    if (!item->wasRestoredFromSession()) {
        if (RefPtr historyItemFrame = WebProcess::singleton().webFrame(item->frameID()))
            targetFrame = historyItemFrame.releaseNonNull();
    }

    ASSERT(targetFrame == m_mainFrame || m_page->settings().siteIsolationEnabled());

    if (RefPtr targetLocalFrame = targetFrame->provisionalFrame() ? targetFrame->provisionalFrame() : targetFrame->coreLocalFrame())
        m_page->goToItem(*targetLocalFrame, *item, parameters.backForwardType, parameters.shouldTreatAsContinuingLoad);
}

// GoToBackForwardItemWaitingForProcessLaunch should never be sent to the WebProcess. It must always be converted to a GoToBackForwardItem message.
void WebPage::goToBackForwardItemWaitingForProcessLaunch(GoToBackForwardItemParameters&&, WebKit::WebPageProxyIdentifier)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void WebPage::tryRestoreScrollPosition()
{
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->checkedHistory()->restoreScrollPositionAndViewState();
}

WebPage* WebPage::fromCorePage(Page& page)
{
    auto& client = page.chrome().client();
    return client.isEmptyChromeClient() ? nullptr : &static_cast<WebChromeClient&>(client).page();
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    if (m_viewSize == viewSize)
        return;

    m_viewSize = viewSize;
    RefPtr view = m_page->mainFrame().virtualView();
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
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;
    RefPtr mainFrameView = localMainFrame->view();
    LocalDefaultSystemAppearance localAppearance(mainFrameView ? mainFrameView->useDarkAppearance() : false);
#endif

    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);

    m_mainFrame->coreLocalFrame()->view()->paint(graphicsContext, rect);

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
    if (auto* pluginView = mainFramePlugIn())
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
    if (auto* pluginView = mainFramePlugIn()) {
        pluginView->setPageScaleFactor(zoomFactor, std::nullopt);
        return;
    }
#endif

    if (!m_page)
        return;

    for (WeakRef frame : m_page->rootFrames())
        frame->setTextZoomFactor(static_cast<float>(zoomFactor));
}

double WebPage::pageZoomFactor() const
{
#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = mainFramePlugIn()) {
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
    if (auto* pluginView = mainFramePlugIn()) {
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
    std::stable_sort(children.begin(), children.end(), [] (auto& a, auto& b) {
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

#if PLATFORM(IOS_FAMILY)
    if (willChangeScaleFactor) {
        if (!m_inDynamicSizeUpdate)
            m_dynamicSizeUpdateHistory.clear();
        m_scaleWasSetByUIProcess = false;
    }
#endif

#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = mainFramePlugIn()) {
        // Since the main-frame PDF plug-in handles the page scale factor, make sure to reset WebCore's page scale.
        // Otherwise, we can end up with an immutable but non-1 page scale applied by WebCore on top of whatever the plugin does.
        if (m_page->pageScaleFactor() != 1)
            m_page->setPageScaleFactor(1, origin);
        pluginView->setPageScaleFactor(totalScale, { origin });
        return;
    }
#endif

    m_page->setPageScaleFactor(totalScale, origin);

    // We can't early return before setPageScaleFactor because the origin might be different.
    if (!willChangeScaleFactor)
        return;

#if ENABLE(PDF_PLUGIN)
    for (auto& pluginView : m_pluginViews)
        pluginView.setPageScaleFactor(totalScale, { origin });
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    protectedDrawingArea()->deviceOrPageScaleFactorChanged();
#endif

    platformDidScalePage();
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
    if (auto* pluginView = mainFramePlugIn())
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

    IntPoint scrollPositionAtNewScale;
    if (RefPtr mainFrameView = m_page->mainFrame().virtualView()) {
        double scaleRatio = scale / viewScaleFactor();
        scrollPositionAtNewScale = mainFrameView->scrollPosition();
        scrollPositionAtNewScale.scale(scaleRatio);
    }

    m_page->setViewScaleFactor(scale);
    didScalePage(pageScale, scrollPositionAtNewScale);
}

void WebPage::scaleView(double scale)
{
    if (scale == viewScaleFactor())
        return;
    didScaleView(scale);
    send(Messages::WebPageProxy::ViewScaleFactorDidChange(scale));
}

#if ENABLE(PDF_PLUGIN)
void WebPage::setPluginScaleFactor(double scaleFactor, WebCore::IntPoint origin)
{
    if (RefPtr plugin = mainFramePlugIn())
        plugin->setPageScaleFactor(scaleFactor, origin);
}

#if PLATFORM(IOS_FAMILY)
void WebPage::pluginDidInstallPDFDocument(double initialScale)
{
    send(Messages::WebPageProxy::PluginDidInstallPDFDocument(initialScale));
}
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(PDF_PLUGIN)

void WebPage::setDeviceScaleFactor(float scaleFactor)
{
    if (scaleFactor == m_page->deviceScaleFactor())
        return;

    m_page->setDeviceScaleFactor(scaleFactor);

    // Tell all our plug-in views that the device scale factor changed.
#if PLATFORM(MAC)
    for (auto& pluginView : m_pluginViews)
        pluginView.setDeviceScaleFactor(scaleFactor);

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

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    protectedDrawingArea()->deviceOrPageScaleFactorChanged();
#endif
}

float WebPage::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

void WebPage::accessibilitySettingsDidChange()
{
    m_page->accessibilitySettingsDidChange();
}

void WebPage::screenPropertiesDidChange()
{
    m_page->screenPropertiesDidChange();
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
#else
    UNUSED_PARAM(viewportArguments);
#endif
}

#if !PLATFORM(IOS_FAMILY)

FloatSize WebPage::screenSizeForFingerprintingProtections(const LocalFrame& frame, FloatSize defaultSize) const
{
    return frame.view() ? FloatSize { frame.view()->unobscuredContentRectIncludingScrollbars().size() } : defaultSize;
}

#endif // !PLATFORM(IOS_FAMILY)

void WebPage::listenForLayoutMilestones(OptionSet<WebCore::LayoutMilestone> milestones)
{
    if (!m_page)
        return;
    m_page->addLayoutMilestones(milestones);
}

void WebPage::setSuppressScrollbarAnimations(bool suppressAnimations)
{
    m_page->setShouldSuppressScrollbarAnimations(suppressAnimations);
}
    
void WebPage::setEnableVerticalRubberBanding(bool enableVerticalRubberBanding)
{
    m_page->setVerticalScrollElasticity(enableVerticalRubberBanding ? ScrollElasticity::Allowed : ScrollElasticity::None);
}
    
void WebPage::setEnableHorizontalRubberBanding(bool enableHorizontalRubberBanding)
{
    m_page->setHorizontalScrollElasticity(enableHorizontalRubberBanding ? ScrollElasticity::Allowed : ScrollElasticity::None);
}

void WebPage::setBackgroundExtendsBeyondPage(bool backgroundExtendsBeyondPage)
{
    if (m_page->settings().backgroundShouldExtendBeyondPage() != backgroundExtendsBeyondPage)
        m_page->settings().setBackgroundShouldExtendBeyondPage(backgroundExtendsBeyondPage);
}

void WebPage::setPaginationMode(Pagination::Mode mode)
{
    Pagination pagination = m_page->pagination();
    pagination.mode = static_cast<Pagination::Mode>(mode);
    m_page->setPagination(pagination);
}

void WebPage::setPaginationBehavesLikeColumns(bool behavesLikeColumns)
{
    Pagination pagination = m_page->pagination();
    pagination.behavesLikeColumns = behavesLikeColumns;
    m_page->setPagination(pagination);
}

void WebPage::setPageLength(double pageLength)
{
    Pagination pagination = m_page->pagination();
    pagination.pageLength = pageLength;
    m_page->setPagination(pagination);
}

void WebPage::setGapBetweenPages(double gap)
{
    Pagination pagination = m_page->pagination();
    pagination.gap = gap;
    m_page->setPagination(pagination);
}

void WebPage::postInjectedBundleMessage(const String& messageName, const UserData& userData)
{
    auto& webProcess = WebProcess::singleton();
    RefPtr injectedBundle = webProcess.injectedBundle();
    if (!injectedBundle)
        return;

    injectedBundle->didReceiveMessageToPage(Ref { *this }, messageName, webProcess.transformHandlesToObjects(userData.object()));
}

void WebPage::setUnderPageBackgroundColorOverride(WebCore::Color&& underPageBackgroundColorOverride)
{
    m_page->setUnderPageBackgroundColorOverride(WTFMove(underPageBackgroundColorOverride));
}

#if !PLATFORM(IOS_FAMILY)

void WebPage::setHeaderPageBanner(PageBanner* pageBanner)
{
    if (m_headerBanner)
        m_headerBanner->detachFromPage();

    m_headerBanner = pageBanner;

    if (m_headerBanner)
        m_headerBanner->addToPage(PageBanner::Header, this);
}

PageBanner* WebPage::headerPageBanner()
{
    return m_headerBanner.get();
}

void WebPage::setFooterPageBanner(PageBanner* pageBanner)
{
    if (m_footerBanner)
        m_footerBanner->detachFromPage();

    m_footerBanner = pageBanner;

    if (m_footerBanner)
        m_footerBanner->addToPage(PageBanner::Footer, this);
}

PageBanner* WebPage::footerPageBanner()
{
    return m_footerBanner.get();
}

void WebPage::hidePageBanners()
{
    if (m_headerBanner)
        m_headerBanner->hide();
    if (m_footerBanner)
        m_footerBanner->hide();
}

void WebPage::showPageBanners()
{
    if (m_headerBanner)
        m_headerBanner->showIfHidden();
    if (m_footerBanner)
        m_footerBanner->showIfHidden();
}

#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
void WebPage::setHeaderBannerHeight(int height)
{
    corePage()->setHeaderHeight(height);
}

void WebPage::setFooterBannerHeight(int height)
{
    corePage()->setFooterHeight(height);
}
#endif

void WebPage::takeSnapshot(IntRect snapshotRect, IntSize bitmapSize, SnapshotOptions snapshotOptions, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    std::optional<ShareableBitmap::Handle> handle;
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    if (!coreFrame) {
        completionHandler(WTFMove(handle));
        return;
    }

    RefPtr frameView = coreFrame->view();
    if (!frameView) {
        completionHandler(WTFMove(handle));
        return;
    }

    snapshotOptions.add(SnapshotOption::Shareable);

    auto originalLayoutViewportOverrideRect = frameView->layoutViewportOverrideRect();
    auto originalPaintBehavior = frameView->paintBehavior();
    bool isPaintBehaviorChanged = false;

    if (snapshotOptions.contains(SnapshotOption::VisibleContentRect))
        snapshotRect = frameView->visibleContentRect();
    else if (snapshotOptions.contains(SnapshotOption::FullContentRect)) {
        snapshotRect = IntRect({ 0, 0 }, frameView->contentsSize());
        frameView->setLayoutViewportOverrideRect(LayoutRect(snapshotRect));
        frameView->setPaintBehavior(originalPaintBehavior | PaintBehavior::AnnotateLinks);
        isPaintBehaviorChanged = true;
    }

    if (bitmapSize.isEmpty()) {
        bitmapSize = snapshotRect.size();
        if (!snapshotOptions.contains(SnapshotOption::ExcludeDeviceScaleFactor))
            bitmapSize.scale(corePage()->deviceScaleFactor());
    }

    if (auto image = snapshotAtSize(snapshotRect, bitmapSize, snapshotOptions, *coreFrame, *frameView))
        handle = image->createHandle(SharedMemory::Protection::ReadOnly);

    if (isPaintBehaviorChanged) {
        frameView->setLayoutViewportOverrideRect(originalLayoutViewportOverrideRect);
        frameView->setPaintBehavior(originalPaintBehavior);
    }

    completionHandler(WTFMove(handle));
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
        FloatRect selectionRectangle = frame.selection().selectionBounds();
        graphicsContext.setStrokeColor(Color::red);
        graphicsContext.strokeRect(selectionRectangle, 1);
    }

    if (options.contains(SnapshotOption::TransparentBackground))
        frameView.setBaseBackgroundColor(savedBackgroundColor);
}

static DestinationColorSpace snapshotColorSpace(SnapshotOptions options, WebPage& page)
{
#if USE(CG)
    if (options.contains(SnapshotOption::UseScreenColorSpace))
        return screenColorSpace(page.corePage()->mainFrame().virtualView());
#endif
    return DestinationColorSpace::SRGB();
}

RefPtr<WebImage> WebPage::snapshotAtSize(const IntRect& rect, const IntSize& bitmapSize, SnapshotOptions options, LocalFrame& frame, LocalFrameView& frameView)
{
#if ENABLE(PDF_PLUGIN)
    auto imageOptions = m_pluginViews.computeSize() ? ImageOption::Local : ImageOption::Shareable;
#else
    auto imageOptions = ImageOption::Shareable;
#endif

    auto snapshot = WebImage::create(bitmapSize, imageOptions, snapshotColorSpace(options, *this), &m_page->chrome().client());
    if (!snapshot->context())
        return nullptr;

    auto& graphicsContext = *snapshot->context();
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
    IntRect snapshotRect = snappedIntRect(node.renderer()->paintingRootRect(topLevelRect));
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
        m_dynamicSizeUpdateHistory.clear();
#endif
    m_uiClient->pageDidScroll(this);

    m_pageScrolledHysteresis.impulse();

    if (RefPtr view = m_page->protectedMainFrame()->virtualView())
        send(Messages::WebPageProxy::PageDidScroll(view->scrollPosition()));
}

void WebPage::pageStoppedScrolling()
{
    // Maintain the current history item's scroll position up-to-date.
    if (RefPtr frame = m_mainFrame->coreLocalFrame())
        frame->checkedHistory()->saveScrollPositionAndViewStateToItem(frame->history().currentItem());
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

WebContextMenu* WebPage::contextMenuAtPointInWindow(FrameIdentifier frameID, const IntPoint& point)
{
    auto* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return nullptr;

    auto* coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    corePage()->contextMenuController().clearContextMenu();

    // Simulate a mouse click to generate the correct menu.
    PlatformMouseEvent mousePressEvent(point, point, MouseButton::Right, PlatformEvent::Type::MousePressed, 1, { }, WallTime::now(), WebCore::ForceAtClick, WebCore::SyntheticClickType::NoTap);
    coreFrame->eventHandler().handleMousePressEvent(mousePressEvent);
    bool handled = coreFrame->eventHandler().sendContextMenuEvent(mousePressEvent);
    auto* menu = handled ? &contextMenu() : nullptr;
    PlatformMouseEvent mouseReleaseEvent(point, point, MouseButton::Right, PlatformEvent::Type::MouseReleased, 1, { }, WallTime::now(), WebCore::ForceAtClick, WebCore::SyntheticClickType::NoTap);
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
    WEBPAGE_RELEASE_LOG(ProcessSuspension, "freezeLayerTree: Adding a reason to freeze layer tree (reason=%d, new=%d, old=%d)", static_cast<unsigned>(reason), m_layerTreeFreezeReasons.toRaw(), oldReasons);
    updateDrawingAreaLayerTreeFreezeState();
}

void WebPage::unfreezeLayerTree(LayerTreeFreezeReason reason)
{
    auto oldReasons = m_layerTreeFreezeReasons.toRaw();
    UNUSED_PARAM(oldReasons);
    m_layerTreeFreezeReasons.remove(reason);
    WEBPAGE_RELEASE_LOG(ProcessSuspension, "unfreezeLayerTree: Removing a reason to freeze layer tree (reason=%d, new=%d, old=%d)", static_cast<unsigned>(reason), m_layerTreeFreezeReasons.toRaw(), oldReasons);
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
    if (m_videoPresentationManager && m_videoPresentationManager->hasVideoPlayingInPictureInPicture() && m_layerTreeFreezeReasons.hasExactlyOneBitSet() && m_layerTreeFreezeReasons.contains(LayerTreeFreezeReason::BackgroundApplication)) {
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

void WebPage::updateFrameSize(WebCore::FrameIdentifier frameID, WebCore::IntSize newSize)
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

    RefPtr frameView = frame->view();
    if (!frameView)
        return;

    if (frameView->size() == newSize)
        return;

    frameView->resize(newSize);
#if PLATFORM(IOS_FAMILY)
    // FIXME: This ensures cross-site iframe render correctly;
    // it should be removed after rdar://122429810 is fixed.
    frameView->setExposedContentRect(frameView->frameRect());
    frameView->setUnobscuredContentSize(frameView->size());
#endif

    if (RefPtr drawingArea = m_drawingArea) {
        drawingArea->setNeedsDisplay();
        drawingArea->triggerRenderingUpdate();
    }
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
    WEBPAGE_RELEASE_LOG(Layers, "markLayersVolatile:");

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

    WEBPAGE_RELEASE_LOG(Layers, "markLayersVolatile: Failed to mark all layers as volatile, will retry in %g ms", m_layerVolatilityTimerInterval.milliseconds());
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

void WebPage::showContextMenuFromFrame(const WebCore::FrameIdentifier& frameID, const ContextMenuContextData& contextMenuContextData, const UserData& userData)
{
    flushPendingEditorStateUpdate();
    send(Messages::WebPageProxy::ShowContextMenuFromFrame(frameID, contextMenuContextData, userData));
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

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    bool handled = frame->eventHandler().sendContextMenuEventForKey();
#if ENABLE(CONTEXT_MENUS)
    if (handled)
        contextMenu().show();
#else
    UNUSED_PARAM(handled);
#endif
}
#endif

void WebPage::mouseEvent(FrameIdentifier frameID, const WebMouseEvent& mouseEvent, std::optional<Vector<SandboxExtension::Handle>>&& sandboxExtensions)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    m_userActivity.impulse();

    bool shouldHandleEvent = true;
#if ENABLE(DRAG_SUPPORT)
    if (m_isStartingDrag)
        shouldHandleEvent = false;
#endif

    if (!shouldHandleEvent) {
        send(Messages::WebPageProxy::DidReceiveEvent(mouseEvent.type(), false, std::nullopt));
        return;
    }

    Vector<Ref<SandboxExtension>> mouseEventSandboxExtensions;
    if (sandboxExtensions)
        mouseEventSandboxExtensions = consumeSandboxExtensions(WTFMove(*sandboxExtensions));

    bool handled = false;

#if !PLATFORM(IOS_FAMILY)
    if (!handled && m_headerBanner)
        handled = m_headerBanner->mouseEvent(mouseEvent);
    if (!handled && m_footerBanner)
        handled = m_footerBanner->mouseEvent(mouseEvent);
#endif // !PLATFORM(IOS_FAMILY)

    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID); !handled && frame) {
        CurrentEvent currentEvent(mouseEvent);
        auto mouseEventResult = frame->handleMouseEvent(mouseEvent);
        if (auto remoteMouseEventData = mouseEventResult.remoteUserInputEventData()) {
            revokeSandboxExtensions(mouseEventSandboxExtensions);
            send(Messages::WebPageProxy::DidReceiveEvent(mouseEvent.type(), false, *remoteMouseEventData));
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

    send(Messages::WebPageProxy::DidReceiveEvent(mouseEvent.type(), handled, std::nullopt));
}

void WebPage::setLastKnownMousePosition(WebCore::FrameIdentifier frameID, IntPoint eventPoint, IntPoint globalPoint)
{
    auto* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame || !frame->coreLocalFrame() || !frame->coreLocalFrame()->view())
        return;

    frame->coreLocalFrame()->eventHandler().setLastKnownMousePosition(eventPoint, globalPoint);
}

void WebPage::startDeferringResizeEvents()
{
    corePage()->startDeferringResizeEvents();
}

void WebPage::flushDeferredResizeEvents()
{
    corePage()->flushDeferredResizeEvents();
}

void WebPage::startDeferringScrollEvents()
{
    corePage()->startDeferringScrollEvents();
}

void WebPage::flushDeferredScrollEvents()
{
    corePage()->flushDeferredScrollEvents();
}

void WebPage::flushDeferredDidReceiveMouseEvent()
{
    if (auto info = std::exchange(m_deferredDidReceiveMouseEvent, std::nullopt))
        send(Messages::WebPageProxy::DidReceiveEvent(*info->type, info->handled, std::nullopt));
}

void WebPage::performHitTestForMouseEvent(const WebMouseEvent& event, CompletionHandler<void(WebHitTestResultData&&, OptionSet<WebEventModifier>, UserData&&)>&& completionHandler)
{
    auto modifiers = event.modifiers();
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame || !localMainFrame->view()) {
        completionHandler({ }, modifiers, { });
        return;
    }

    auto hitTestResult = localMainFrame->eventHandler().getHitTestResultForMouseEvent(platform(event));

    String toolTip;
    TextDirection toolTipDirection;
    corePage()->chrome().getToolTip(hitTestResult, toolTip, toolTipDirection);

    RefPtr<API::Object> userData;
    WebHitTestResultData hitTestResultData { hitTestResult, toolTip };
    injectedBundleUIClient().mouseDidMoveOverElement(this, hitTestResult, modifiers, userData);

    completionHandler(WTFMove(hitTestResultData), modifiers, UserData(WebProcess::singleton().transformObjectsToHandles(WTFMove(userData).get()).get()));
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

    auto handleWheelEventResult = wheelEvent(frameID, event, processingSteps);
#if ENABLE(ASYNC_SCROLLING)
    if (remoteScrollingCoordinator) {
        auto gestureInfo = remoteScrollingCoordinator->takeCurrentWheelGestureInfo();
        completionHandler(gestureInfo.wheelGestureNode, gestureInfo.wheelGestureState, handleWheelEventResult.wasHandled(), handleWheelEventResult.remoteUserInputEventData());
        return;
    }
#endif
    completionHandler({ }, { }, handleWheelEventResult.wasHandled(), handleWheelEventResult.remoteUserInputEventData());
}

HandleUserInputEventResult WebPage::wheelEvent(const FrameIdentifier& frameID, const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    m_userActivity.impulse();

    CurrentEvent currentEvent(wheelEvent);

    auto dispatchWheelEvent = [&](const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps) {
        auto* frame = WebProcess::singleton().webFrame(frameID);
        if (!frame || !frame->coreLocalFrame() || !frame->coreLocalFrame()->view())
            return HandleUserInputEventResult { false };

        auto platformWheelEvent = platform(wheelEvent);
        return frame->coreLocalFrame()->eventHandler().handleWheelEvent(platformWheelEvent, processingSteps);
    };

    auto handleWheelEventResult = dispatchWheelEvent(wheelEvent, processingSteps);
    LOG_WITH_STREAM(WheelEvents, stream << "WebPage::wheelEvent - processing steps " << processingSteps << " handled " << handleWheelEventResult.wasHandled());
    return handleWheelEventResult;
}

#if PLATFORM(IOS_FAMILY)
void WebPage::dispatchWheelEventWithoutScrolling(FrameIdentifier frameID, const WebWheelEvent& wheelEvent, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(KINETIC_SCROLLING)
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    auto gestureState =  localMainFrame ? localMainFrame->eventHandler().wheelScrollGestureState() : std::nullopt;
    bool isCancelable = !gestureState || gestureState == WheelScrollGestureState::Blocking || wheelEvent.phase() == WebWheelEvent::PhaseBegan;
#else
    bool isCancelable = true;
#endif
    bool handled = this->wheelEvent(frameID, wheelEvent, { isCancelable ? WheelEventProcessingSteps::BlockingDOMEventDispatch : WheelEventProcessingSteps::NonBlockingDOMEventDispatch }).wasHandled();
    // The caller of dispatchWheelEventWithoutScrolling never cares about DidReceiveEvent being sent back.
    completionHandler(handled);
}
#endif

void WebPage::keyEvent(FrameIdentifier frameID, const WebKeyboardEvent& keyboardEvent)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    m_userActivity.impulse();

    PlatformKeyboardEvent::setCurrentModifierState(platform(keyboardEvent).modifiers());

    CurrentEvent currentEvent(keyboardEvent);

    bool handled = false;
    if (WebFrame* frame = WebProcess::singleton().webFrame(frameID))
        handled = frame->handleKeyEvent(keyboardEvent);

    send(Messages::WebPageProxy::DidReceiveEvent(keyboardEvent.type(), handled, std::nullopt));
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
    return m_page->checkedFocusController()->relinquishFocusToChrome(FocusDirection::Backward);
}

void WebPage::validateCommand(const String& commandName, CompletionHandler<void(bool, int32_t)>&& completionHandler)
{
    bool isEnabled = false;
    int32_t state = 0;
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler({ }, { });

#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = focusedPluginViewForFrame(*frame))
        isEnabled = pluginView->isEditingCommandEnabled(commandName);
    else
#endif
    {
        Editor::Command command = frame->editor().command(commandName);
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
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(corePage()->mainFrame()))
        localMainFrame->checkedHistory()->setCurrentItem(toHistoryItem(m_historyItemClient, mainFrameState));
}

void WebPage::requestFontAttributesAtSelectionStart(CompletionHandler<void(const WebCore::FontAttributes&)>&& completionHandler)
{
    RefPtr focusedOrMainFrame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return completionHandler({ });
    completionHandler(focusedOrMainFrame->editor().fontAttributesAtSelectionStart());
}

void WebPage::cancelCurrentInteractionInformationRequest()
{
#if PLATFORM(IOS_FAMILY)
    if (auto reply = WTFMove(m_pendingSynchronousPositionInformationReply))
        reply(InteractionInformationAtPosition::invalidInformation());
#endif
}

#if ENABLE(TOUCH_EVENTS)
static HandleUserInputEventResult handleTouchEvent(FrameIdentifier frameID, const WebTouchEvent& touchEvent, Page* page)
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

#if ENABLE(IOS_TOUCH_EVENTS)
HandleUserInputEventResult WebPage::dispatchTouchEvent(FrameIdentifier frameID, const WebTouchEvent& touchEvent)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };
    m_lastInteractionLocation = touchEvent.position();
    CurrentEvent currentEvent(touchEvent);
    auto handleTouchEventResult = handleTouchEvent(frameID, touchEvent, m_page.get());
    updatePotentialTapSecurityOrigin(touchEvent, handleTouchEventResult.wasHandled());
    return handleTouchEventResult;
}

void WebPage::resetPotentialTapSecurityOrigin()
{
    m_potentialTapSecurityOrigin = nullptr;
}

void WebPage::updatePotentialTapSecurityOrigin(const WebTouchEvent& touchEvent, bool wasHandled)
{
    if (wasHandled)
        return;

    if (!touchEvent.isPotentialTap())
        return;

    if (touchEvent.type() != WebEventType::TouchStart)
        return;

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
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
        m_potentialTapSecurityOrigin = &targetDocument->securityOrigin();
}
#elif ENABLE(TOUCH_EVENTS)
void WebPage::touchEvent(const WebTouchEvent& touchEvent, CompletionHandler<void(std::optional<WebEventType>, bool)>&& completionHandler)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;

    CurrentEvent currentEvent(touchEvent);

    bool handled = handleTouchEvent(localMainFrame->frameID(), touchEvent, m_page.get()).wasHandled();

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

void WebPage::gestureEvent(FrameIdentifier frameID, const WebGestureEvent& gestureEvent)
{
    CurrentEvent currentEvent(gestureEvent);
    auto result = handleGestureEvent(frameID, gestureEvent, m_page.get());
    send(Messages::WebPageProxy::DidReceiveEvent(gestureEvent.type(), result.wasHandled(), result.remoteUserInputEventData()));
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
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;
    frame->selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignCenterAlways);
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

void WebPage::connectInspector(const String& targetId, Inspector::FrontendChannel::ConnectionType connectionType)
{
    m_inspectorTargetController->connectInspector(targetId, connectionType);
}

void WebPage::disconnectInspector(const String& targetId)
{
    m_inspectorTargetController->disconnectInspector(targetId);
}

void WebPage::sendMessageToTargetBackend(const String& targetId, const String& message)
{
    m_inspectorTargetController->sendMessageToTargetBackend(targetId, message);
}

void WebPage::insertNewlineInQuotedContent()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;
    if (frame->selection().isNone())
        return;
    frame->editor().insertParagraphSeparatorInQuotedContent();
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

#if PLATFORM(COCOA)
void WebPage::setTopContentInsetFenced(float contentInset, const WTF::MachSendRight& machSendRight)
{
    protectedDrawingArea()->addFence(machSendRight);
    setTopContentInset(contentInset);
}
#endif

void WebPage::setTopContentInset(float contentInset)
{
    if (contentInset == m_page->topContentInset())
        return;

    m_page->setTopContentInset(contentInset);

#if ENABLE(PDF_PLUGIN)
    for (auto& pluginView : m_pluginViews)
        pluginView.topContentInsetDidChange();
#endif
}

void WebPage::viewWillStartLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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
    frame->document()->setFocusedElement(nullptr);

    if (isKeyboardEventValid && event && event->type() == WebEventType::KeyDown) {
        PlatformKeyboardEvent platformEvent(platform(*event));
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
    if (m_page)
        m_page->setCanStartMedia(true);
}

void WebPage::updateIsInWindow(bool isInitialState)
{
    bool isInWindow = m_activityState.contains(WebCore::ActivityState::IsInWindow);

    if (!isInWindow) {
        m_setCanStartMediaTimer.stop();
        m_page->setCanStartMedia(false);
        
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
    for (auto& pluginView : m_pluginViews)
        pluginView.didChangeIsInWindow();
#endif
}

void WebPage::visibilityDidChange()
{
    bool isVisible = m_activityState.contains(ActivityState::IsVisible);
    if (!isVisible) {
        // We save the document / scroll state when backgrounding a tab so that we are able to restore it
        // if it gets terminated while in the background.
        if (RefPtr frame = m_mainFrame->coreLocalFrame())
            frame->checkedHistory()->saveDocumentAndScrollState();
    }
}

void WebPage::windowActivityDidChange()
{
#if ENABLE(PDF_PLUGIN)
    for (auto& pluginView : m_pluginViews)
        pluginView.windowActivityDidChange();
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
    if (m_page) {
        SetForScope currentlyChangingActivityState { m_lastActivityStateChanges, changed };
        m_page->setActivityState(activityState);
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

void WebPage::setLayerHostingMode(LayerHostingMode layerHostingMode)
{
    m_layerHostingMode = layerHostingMode;

    protectedDrawingArea()->setLayerHostingMode(m_layerHostingMode);
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
    if (!m_page)
        return;

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (RefPtr document = localMainFrame ? localMainFrame->document() : nullptr)
        document->setNeedsDOMWindowResizeEvent();
}

String WebPage::userAgent(const URL& webCoreURL) const
{
    String userAgent = platformUserAgent(webCoreURL);
    if (!userAgent.isEmpty())
        return userAgent;
    return m_userAgent;
}

void WebPage::setUserAgent(const String& userAgent)
{
    if (m_userAgent == userAgent)
        return;

    m_userAgent = userAgent;

    if (m_page)
        m_page->userAgentChanged();
}

void WebPage::setHasCustomUserAgent(bool hasCustomUserAgent)
{
    m_hasCustomUserAgent = hasCustomUserAgent;
}

void WebPage::suspendActiveDOMObjectsAndAnimations()
{
    m_page->suspendActiveDOMObjectsAndAnimations();
}

void WebPage::resumeActiveDOMObjectsAndAnimations()
{
    m_page->resumeActiveDOMObjectsAndAnimations();
}

void WebPage::suspend(CompletionHandler<void(bool)>&& completionHandler)
{
    WEBPAGE_RELEASE_LOG(Loading, "suspend: m_page=%p", m_page.get());
    if (!m_page)
        return completionHandler(false);

    freezeLayerTree(LayerTreeFreezeReason::PageSuspended);

    m_cachedPage = BackForwardCache::singleton().suspendPage(*m_page);
    ASSERT(m_cachedPage);
    if (RefPtr mainFrame = m_mainFrame->coreLocalFrame())
        mainFrame->detachFromAllOpenedFrames();
    completionHandler(true);
}

void WebPage::resume(CompletionHandler<void(bool)>&& completionHandler)
{
    WEBPAGE_RELEASE_LOG(Loading, "resume: m_page=%p", m_page.get());
    if (!m_page)
        return completionHandler(false);

    auto cachedPage = std::exchange(m_cachedPage, nullptr);
    ASSERT(cachedPage);
    if (!cachedPage)
        return completionHandler(false);

    cachedPage->restore(*m_page);
    unfreezeLayerTree(LayerTreeFreezeReason::PageSuspended);
    completionHandler(true);
}

IntPoint WebPage::screenToRootView(const IntPoint& point)
{
    auto sendResult = sendSync(Messages::WebPageProxy::ScreenToRootView(point));
    auto [windowPoint] = sendResult.takeReplyOr(IntPoint { });
    return windowPoint;
}
    
IntRect WebPage::rootViewToScreen(const IntRect& rect)
{
    auto sendResult = sendSync(Messages::WebPageProxy::RootViewToScreen(rect.toRectWithExtentsClippedToNumericLimits()));
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

void WebPage::runJavaScript(WebFrame* frame, RunJavaScriptParameters&& parameters, ContentWorldIdentifier worldIdentifier, CompletionHandler<void(std::span<const uint8_t>, const std::optional<WebCore::ExceptionDetails>&)>&& completionHandler)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    if (!frame || !frame->coreLocalFrame()) {
        completionHandler({ }, ExceptionDetails { "Unable to execute JavaScript: Target frame could not be found in the page"_s, 0, 0, ExceptionDetails::Type::InvalidTargetFrame });
        return;
    }

    RefPtr world = m_userContentController->worldForIdentifier(worldIdentifier);
    if (!world) {
        completionHandler({ }, ExceptionDetails { "Unable to execute JavaScript: Cannot find specified content world"_s });
        return;
    }

#if ENABLE(APP_BOUND_DOMAINS)
    if (frame->shouldEnableInAppBrowserPrivacyProtections()) {
        completionHandler({ }, ExceptionDetails { "Unable to execute JavaScript in a frame that is not in an app-bound domain"_s, 0, 0, ExceptionDetails::Type::AppBoundDomain });
        RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
        if (RefPtr document = localMainFrame ? localMainFrame->document() : nullptr)
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user script injection for non-app bound domain."_s);
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
    auto resolveFunction = [world = Ref { *world }, frame = Ref { *frame }, coreFrame = Ref { *frame->coreLocalFrame() }, completionHandler = WTFMove(completionHandler)] (ValueOrException result) mutable {
        RefPtr<SerializedScriptValue> serializedResultValue;
        if (result) {
            serializedResultValue = SerializedScriptValue::create(frame->jsContextForWorld(world.ptr()),
                toRef(coreFrame->script().globalObject(Ref { world->coreWorld() }), result.value()), nullptr);
        }

        std::span<const uint8_t> dataReference;
        if (serializedResultValue)
            dataReference = serializedResultValue->wireBytes();

        std::optional<ExceptionDetails> details;
        if (!result)
            details = result.error();

        completionHandler(dataReference, details);
    };
    JSLockHolder lock(commonVM());
    frame->coreLocalFrame()->script().executeAsynchronousUserAgentScriptInWorld(world->coreWorld(), WTFMove(parameters), WTFMove(resolveFunction));
}

void WebPage::runJavaScriptInFrameInScriptWorld(RunJavaScriptParameters&& parameters, std::optional<WebCore::FrameIdentifier> frameID, const ContentWorldData& worldData, CompletionHandler<void(std::span<const uint8_t>, const std::optional<WebCore::ExceptionDetails>&)>&& completionHandler)
{
    WEBPAGE_RELEASE_LOG(Process, "runJavaScriptInFrameInScriptWorld: frameID=%" PRIu64, frameID ? frameID->object().toUInt64() : 0);
    RefPtr webFrame = frameID ? WebProcess::singleton().webFrame(*frameID) : &mainWebFrame();

    if (RefPtr newWorld = m_userContentController->addContentWorld(worldData)) {
        auto& coreWorld = newWorld->coreWorld();
        for (RefPtr<Frame> frame = mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get()))
                localFrame->loader().client().dispatchGlobalObjectAvailable(coreWorld);
        }
    }

    runJavaScript(webFrame.get(), WTFMove(parameters), worldData.identifier, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](std::span<const uint8_t> result, const std::optional<WebCore::ExceptionDetails>& exception) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        if (exception)
            WEBPAGE_RELEASE_LOG_ERROR(Process, "runJavaScriptInFrameInScriptWorld: Request to run JavaScript failed with error %" PRIVATE_LOG_STRING, exception->message.utf8().data());
        else
            WEBPAGE_RELEASE_LOG(Process, "runJavaScriptInFrameInScriptWorld: Request to run JavaScript succeeded");
        completionHandler(result, exception);
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

static LocalFrame* frameWithSelection(Page* page)
{
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (localFrame->selection().isRange())
            return localFrame;
    }
    return nullptr;
}

void WebPage::getSelectionAsWebArchiveData(CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& callback)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data;
    if (RefPtr frame = frameWithSelection(m_page.get()))
        data = LegacyWebArchive::createFromSelection(frame.get())->rawDataRepresentation();
#endif

    IPC::SharedBufferReference dataBuffer;
#if PLATFORM(COCOA)
    if (data)
        dataBuffer = IPC::SharedBufferReference(SharedBuffer::create(data.get()));
#endif
    callback(dataBuffer);
}

void WebPage::copyLinkWithHighlight()
{
    auto url = m_page->fragmentDirectiveURLForSelectedText();
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (url.isValid())
        frame->editor().copyURL(url, { });
}

void WebPage::getSelectionOrContentsAsString(CompletionHandler<void(const String&)>&& callback)
{
    RefPtr focusedOrMainCoreFrame = m_page->checkedFocusController()->focusedOrMainFrame();
    RefPtr focusedOrMainFrame = focusedOrMainCoreFrame ? WebFrame::fromCoreFrame(*focusedOrMainCoreFrame) : nullptr;
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
        if (auto* pluginView = pluginViewForFrame(coreFrame.get()))
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
        buffer = resourceDataForFrame(frame->coreLocalFrame(), resourceURL);
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

void WebPage::getWebArchiveOfFrame(std::optional<FrameIdentifier> frameID, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& callback)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data;
    RefPtr<WebFrame> frame;
    if (frameID)
        frame = WebProcess::singleton().webFrame(*frameID);
    else
        frame = m_mainFrame.ptr();
    if (frame)
        data = frame->webArchiveData(nullptr, nullptr);
    callback(IPC::SharedBufferReference(SharedBuffer::create(data.get())));
#else
    UNUSED_PARAM(frameID);
    callback({ });
#endif
}

void WebPage::getAccessibilityTreeData(CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& callback)
{
    IPC::SharedBufferReference dataBuffer;
#if PLATFORM(COCOA)
    if (auto treeData = m_page->accessibilityTreeData()) {
        auto stream = adoptCF(CFWriteStreamCreateWithAllocatedBuffers(0, 0));
        CFWriteStreamOpen(stream.get());

        auto writeTreeToStream = [&stream](auto& tree) {
            auto utf8 = tree.utf8();
            auto utf8Span = utf8.span();
            CFWriteStreamWrite(stream.get(), utf8Span.data(), utf8Span.size());
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
        auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::WaitForSharedPreferencesForWebProcessToSync { *sharedPreferencesVersion }, 0);
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
        settings.setDownloadableBinaryFontTrustedTypes(
            (settings.lockdownFontParserEnabled() && PAL::canLoad_CoreText_CTFontManagerCreateMemorySafeFontDescriptorFromData())
                ? DownloadableBinaryFontTrustedTypes::SafeFontParser
                : DownloadableBinaryFontTrustedTypes::Restricted);
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
    static_cast<WebMediaStrategy&>(platformStrategies()->mediaStrategy()).setUseGPUProcess(m_shouldPlayMediaInGPUProcess);
#if ENABLE(VIDEO)
    WebProcess::singleton().remoteMediaPlayerManager().setUseGPUProcess(m_shouldPlayMediaInGPUProcess);
#endif
#if HAVE(AVASSETREADER)
    WebProcess::singleton().remoteImageDecoderAVFManager().setUseGPUProcess(m_shouldPlayMediaInGPUProcess);
#endif
    WebProcess::singleton().setUseGPUProcessForCanvasRendering(m_shouldRenderCanvasInGPUProcess);
    bool usingGPUProcessForDOMRendering = m_shouldRenderDOMInGPUProcess && DrawingArea::supportsGPUProcessRendering(m_drawingAreaType);
    WebProcess::singleton().setUseGPUProcessForDOMRendering(usingGPUProcessForDOMRendering);
    WebProcess::singleton().setUseGPUProcessForMedia(m_shouldPlayMediaInGPUProcess);
#if ENABLE(WEBGL)
    WebProcess::singleton().setUseGPUProcessForWebGL(m_shouldRenderWebGLInGPUProcess);
#endif
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(IPC_TESTING_API)
    m_ipcTestingAPIEnabled = store.getBoolValueForKey(WebPreferencesKey::ipcTestingAPIEnabledKey());

    WebProcess::singleton().parentProcessConnection()->setIgnoreInvalidMessageForTesting();
    if (auto* gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->connection().setIgnoreInvalidMessageForTesting();
#if ENABLE(MODEL_PROCESS)
    if (auto* modelProcessConnection = WebProcess::singleton().existingModelProcessConnection())
        modelProcessConnection->connection().setIgnoreInvalidMessageForTesting();
#endif // ENABLE(MODEL_PROCESS)
#endif // ENABLE(IPC_TESTING_API)
    
#if ENABLE(ALTERNATE_WEBM_PLAYER)
    PlatformMediaSessionManager::setAlternateWebMPlayerEnabled(settings.alternateWebMPlayerEnabled());
#endif

#if HAVE(SC_CONTENT_SHARING_PICKER)
    PlatformMediaSessionManager::setUseSCContentSharingPicker(settings.useSCContentSharingPicker());
#endif

#if ENABLE(VORBIS)
    PlatformMediaSessionManager::setVorbisDecoderEnabled(DeprecatedGlobalSettings::vorbisDecoderEnabled());
#endif

#if ENABLE(OPUS)
    PlatformMediaSessionManager::setOpusDecoderEnabled(DeprecatedGlobalSettings::opusDecoderEnabled());
#endif

#if ENABLE(VP9)
    PlatformMediaSessionManager::setSWVPDecodersAlwaysEnabled(store.getBoolValueForKey(WebPreferencesKey::sWVPDecodersAlwaysEnabledKey()));
#endif

    // FIXME: This should be automated by adding a new field in WebPreferences*.yaml
    // that indicates override state for Lockdown mode. https://webkit.org/b/233100.
    if (WebProcess::singleton().isLockdownModeEnabled())
        adjustSettingsForLockdownMode(settings, &store);

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
    for (auto& pluginView : m_pluginViews)
        pluginView.didChangeSettings();
#endif

    m_page->settingsDidChange();
}

#if ENABLE(DATA_DETECTION)

void WebPage::setDataDetectionResults(NSArray *detectionResults)
{
    DataDetectionResult dataDetectionResult;
    dataDetectionResult.results = detectionResults;
    send(Messages::WebPageProxy::SetDataDetectionResult(dataDetectionResult));
}

void WebPage::removeDataDetectedLinks(CompletionHandler<void(const DataDetectionResult&)>&& completionHandler)
{
    for (RefPtr frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
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
            results->setDocumentLevelResults({ });
        }
    }
    completionHandler({ });
}

static void detectDataInFrame(const Ref<Frame>& frame, OptionSet<WebCore::DataDetectorType> dataDetectorTypes, const std::optional<double>& dataDetectionReferenceDate, UniqueRef<DataDetectionResult>&& mainFrameResult, CompletionHandler<void(const DataDetectionResult&)>&& completionHandler)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
    if (!localFrame) {
        completionHandler(mainFrameResult.get());
        return;
    }

    DataDetection::detectContentInFrame(localFrame.get(), dataDetectorTypes, dataDetectionReferenceDate, [localFrame, mainFrameResult = WTFMove(mainFrameResult), dataDetectionReferenceDate, completionHandler = WTFMove(completionHandler), dataDetectorTypes](NSArray *results) mutable {
        auto retainedResults = retainPtr(results);

        RefPtr protectedLocalFrame { localFrame };

        protectedLocalFrame->dataDetectionResults().setDocumentLevelResults(retainedResults.get());
        if (protectedLocalFrame->isMainFrame())
            mainFrameResult->results = retainedResults;

        RefPtr next = localFrame->tree().traverseNext();
        if (!next) {
            completionHandler(mainFrameResult.get());
            return;
        }

        detectDataInFrame(Ref { *next }, dataDetectorTypes, dataDetectionReferenceDate, WTFMove(mainFrameResult), WTFMove(completionHandler));
    });
}

void WebPage::detectDataInAllFrames(OptionSet<WebCore::DataDetectorType> dataDetectorTypes, CompletionHandler<void(const DataDetectionResult&)>&& completionHandler)
{
    auto mainFrameResult = makeUniqueRef<DataDetectionResult>();
    detectDataInFrame(m_page->protectedMainFrame().get(), dataDetectorTypes, m_dataDetectionReferenceDate, WTFMove(mainFrameResult), WTFMove(completionHandler));
}

#endif // ENABLE(DATA_DETECTION)

#if PLATFORM(COCOA)
void WebPage::willCommitLayerTree(RemoteLayerTreeTransaction& layerTransaction, WebCore::FrameIdentifier rootFrameID)
{
    RefPtr rootFrame = WebProcess::singleton().webFrame(rootFrameID);
    if (!rootFrame)
        return;

    RefPtr localRootFrame = rootFrame->coreLocalFrame();
    if (!localRootFrame)
        return;

    RefPtr frameView = localRootFrame->view();
    if (!frameView)
        return;

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    if (auto* document = localRootFrame->document()) {
        if (CheckedPtr timelinesController = document->timelinesController()) {
            if (auto* acceleratedEffectStackUpdater = timelinesController->existingAcceleratedEffectStackUpdater())
                layerTransaction.setAcceleratedTimelineTimeOrigin(acceleratedEffectStackUpdater->timeOrigin());
        }
    }
#endif

    layerTransaction.setContentsSize(frameView->contentsSize());
    layerTransaction.setScrollOrigin(frameView->scrollOrigin());
    layerTransaction.setPageScaleFactor(corePage()->pageScaleFactor());
    layerTransaction.setRenderTreeSize(corePage()->renderTreeSize());
    layerTransaction.setThemeColor(corePage()->themeColor());
    layerTransaction.setPageExtendedBackgroundColor(corePage()->pageExtendedBackgroundColor());
    layerTransaction.setSampledPageTopColor(corePage()->sampledPageTopColor());

    layerTransaction.setBaseLayoutViewportSize(frameView->baseLayoutViewportSize());
    layerTransaction.setMinStableLayoutViewportOrigin(frameView->minStableLayoutViewportOrigin());
    layerTransaction.setMaxStableLayoutViewportOrigin(frameView->maxStableLayoutViewportOrigin());

#if PLATFORM(IOS_FAMILY)
    layerTransaction.setScaleWasSetByUIProcess(scaleWasSetByUIProcess());
    layerTransaction.setMinimumScaleFactor(m_viewportConfiguration.minimumScale());
    layerTransaction.setMaximumScaleFactor(m_viewportConfiguration.maximumScale());
    layerTransaction.setInitialScaleFactor(m_viewportConfiguration.initialScale());
    layerTransaction.setViewportMetaTagWidth(m_viewportConfiguration.viewportArguments().width);
    layerTransaction.setViewportMetaTagWidthWasExplicit(m_viewportConfiguration.viewportArguments().widthWasExplicit);
    layerTransaction.setViewportMetaTagCameFromImageDocument(m_viewportConfiguration.viewportArguments().type == ViewportArguments::Type::ImageDocument);
    layerTransaction.setAvoidsUnsafeArea(m_viewportConfiguration.avoidsUnsafeArea());
    layerTransaction.setIsInStableState(m_isInStableState);
    layerTransaction.setAllowsUserScaling(allowsUserScaling());
    if (m_pendingDynamicViewportSizeUpdateID) {
        layerTransaction.setDynamicViewportSizeUpdateID(*m_pendingDynamicViewportSizeUpdateID);
        m_pendingDynamicViewportSizeUpdateID = std::nullopt;
    }
    if (m_lastTransactionPageScaleFactor != layerTransaction.pageScaleFactor()) {
        m_lastTransactionPageScaleFactor = layerTransaction.pageScaleFactor();
        m_lastTransactionIDWithScaleChange = layerTransaction.transactionID();
    }
#endif

    layerTransaction.setScrollPosition(frameView->scrollPosition());

    m_pendingThemeColorChange = false;
    m_pendingPageExtendedBackgroundColorChange = false;
    m_pendingSampledPageTopColorChange = false;

    if (hasPendingEditorStateUpdate() || m_needsEditorStateVisualDataUpdate) {
        layerTransaction.setEditorState(editorState());
        m_pendingEditorStateUpdateStatus = PendingEditorStateUpdateStatus::NotScheduled;
        m_needsEditorStateVisualDataUpdate = false;
    }
}

void WebPage::didFlushLayerTreeAtTime(MonotonicTime timestamp, bool flushSucceeded)
{
#if PLATFORM(IOS_FAMILY)
    if (m_oldestNonStableUpdateVisibleContentRectsTimestamp != MonotonicTime()) {
        Seconds elapsed = timestamp - m_oldestNonStableUpdateVisibleContentRectsTimestamp;
        m_oldestNonStableUpdateVisibleContentRectsTimestamp = MonotonicTime();

        m_estimatedLatency = m_estimatedLatency * 0.80 + elapsed * 0.20;
    }
#else
    UNUSED_PARAM(timestamp);
#endif
#if ENABLE(GPU_PROCESS)
    if (!flushSucceeded && m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->didBecomeUnresponsive();
#endif
}
#endif

void WebPage::layoutIfNeeded()
{
    m_page->layoutIfNeeded();
}
    
void WebPage::updateRendering()
{
    m_page->updateRendering();

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

void WebPage::didUpdateRendering()
{
    didPaintLayers();

    if (m_didUpdateRenderingAfterCommittingLoad)
        return;

    m_didUpdateRenderingAfterCommittingLoad = true;
    send(Messages::WebPageProxy::DidUpdateRenderingAfterCommittingLoad());
}

void WebPage::didPaintLayers()
{
#if ENABLE(GPU_PROCESS)
    if (m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->didPaintLayers();
#endif
}

bool WebPage::shouldTriggerRenderingUpdate(unsigned rescheduledRenderingUpdateCount) const
{
#if ENABLE(GPU_PROCESS)
    static constexpr unsigned maxRescheduledRenderingUpdateCount = FullSpeedFramesPerSecond;
    if (rescheduledRenderingUpdateCount >= maxRescheduledRenderingUpdateCount)
        return true;

    static constexpr unsigned maxDelayedRenderingUpdateCount = 2;
    if (m_remoteRenderingBackendProxy && m_remoteRenderingBackendProxy->delayedRenderingUpdateCount() > maxDelayedRenderingUpdateCount)
        return false;
#endif
    return true;
}

void WebPage::finalizeRenderingUpdate(OptionSet<FinalizeRenderingUpdateFlags> flags)
{
#if !PLATFORM(COCOA)
    WTFBeginSignpost(this, FinalizeRenderingUpdate);
#endif

    m_page->finalizeRenderingUpdate(flags);
#if ENABLE(GPU_PROCESS)
    if (m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->finalizeRenderingUpdate();
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
    m_page->willStartRenderingUpdateDisplay();
}

void WebPage::didCompleteRenderingUpdateDisplay()
{
    if (m_isClosed)
        return;
    m_page->didCompleteRenderingUpdateDisplay();
}

void WebPage::didCompleteRenderingFrame()
{
    if (m_isClosed)
        return;
    m_page->didCompleteRenderingFrame();
}

void WebPage::releaseMemory(Critical)
{
#if ENABLE(GPU_PROCESS)
    if (m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().releaseMemory();
#endif
}

void WebPage::willDestroyDecodedDataForAllImages()
{
#if ENABLE(GPU_PROCESS)
    if (m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().releaseAllImageResources();
#endif

    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->setNextRenderingUpdateRequiresSynchronousImageDecoding();
}

unsigned WebPage::remoteImagesCountForTesting() const
{
#if ENABLE(GPU_PROCESS)
    if (!m_remoteRenderingBackendProxy)
        return 0;
    return m_remoteRenderingBackendProxy->remoteResourceCacheProxy().imagesCount();
#else
    return 0;
#endif
}

WebInspector* WebPage::inspector(LazyCreationPolicy behavior)
{
    if (m_isClosed)
        return nullptr;
    if (!m_inspector && behavior == LazyCreationPolicy::CreateIfNeeded)
        m_inspector = WebInspector::create(*this);
    return m_inspector.get();
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

VideoPresentationManager& WebPage::videoPresentationManager()
{
    if (!m_videoPresentationManager)
        m_videoPresentationManager = VideoPresentationManager::create(*this, playbackSessionManager());
    return *m_videoPresentationManager;
}

Ref<VideoPresentationManager> WebPage::protectedVideoPresentationManager()
{
    return videoPresentationManager();
}

void WebPage::videoControlsManagerDidChange()
{
#if ENABLE(FULLSCREEN_API)
    if (auto* manager = fullScreenManager())
        manager->videoControlsManagerDidChange();
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
    AudioSession::sharedSession().setSceneIdentifier(sceneIdentifier);
    m_page->setSceneIdentifier(WTFMove(sceneIdentifier));
}

void WebPage::setAllowsMediaDocumentInlinePlayback(bool allows)
{
    m_page->setAllowsMediaDocumentInlinePlayback(allows);
}
#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManager* WebPage::fullScreenManager()
{
    if (!m_fullScreenManager)
        m_fullScreenManager = WebFullScreenManager::create(*this);
    return m_fullScreenManager.get();
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

    injectedBundleFullScreenClient().closeFullScreen(this);
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
    if (auto* frame = WebProcess::singleton().webFrame(frameID))
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
        document->reportingScope().notifyReportObservers(WTFMove(report));
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
        PingLoader::sendViolationReport(*frame->coreLocalFrame(), URL { baseURL, url }, Ref { *report.get() }, reportType);

    RefPtr document = frame->coreLocalFrame()->document();
    if (!document)
        return;

    for (auto& token : endpointTokens) {
        if (auto url = document->endpointURIForToken(token); !url.isEmpty())
            PingLoader::sendViolationReport(*frame->coreLocalFrame(), URL { baseURL, url }, Ref { *report.get() }, reportType);
    }
}

NotificationPermissionRequestManager* WebPage::notificationPermissionRequestManager()
{
    if (m_notificationPermissionRequestManager)
        return m_notificationPermissionRequestManager.get();

    m_notificationPermissionRequestManager = NotificationPermissionRequestManager::create(this);
    return m_notificationPermissionRequestManager.get();
}

#if ENABLE(DRAG_SUPPORT)

#if PLATFORM(GTK)
void WebPage::performDragControllerAction(DragControllerAction action, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation> draggingSourceOperationMask, SelectionData&& selectionData, OptionSet<DragApplicationFlags> flags, CompletionHandler<void(std::optional<DragOperation>, DragHandlingMethod, bool, unsigned, IntRect, IntRect, std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
    if (!m_page)
        return completionHandler(std::nullopt, DragHandlingMethod::None, false, 0, { }, { }, std::nullopt);

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
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

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &mainWebFrame();
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
    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &mainWebFrame();
    if (!frame)
        return completionHandler(std::nullopt);

    RefPtr localFrame = frame->coreLocalFrame();
    if (!localFrame)
        return completionHandler(std::nullopt);

    RefPtr view = localFrame->view();
    if (!view)
        return completionHandler(std::nullopt);

    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(adjustedClientPosition, adjustedGlobalPosition, MouseButton::Left, PlatformEvent::Type::MouseMoved, 0, { }, WallTime::now(), 0, WebCore::SyntheticClickType::NoTap);
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
    for (size_t i = 0; i < m_pendingDropExtensionsForFileUpload.size(); i++)
        m_pendingDropExtensionsForFileUpload[i]->consumePermanently();
    m_pendingDropExtensionsForFileUpload.clear();
}

void WebPage::didStartDrag()
{
    m_isStartingDrag = false;
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->eventHandler().didStartDrag();
}

void WebPage::dragCancelled()
{
    m_isStartingDrag = false;
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->eventHandler().dragCancelled();
}

#endif // ENABLE(DRAG_SUPPORT)

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

    step->step().unapply();
}

void WebPage::reapplyEditCommand(WebUndoStepID stepID)
{
    RefPtr step = webUndoStep(stepID);
    if (!step)
        return;

    setIsInRedo(true);
    step->step().reapply();
    setIsInRedo(false);
}

void WebPage::didRemoveEditCommand(WebUndoStepID commandID)
{
    removeWebEditCommand(commandID);
}

void WebPage::closeCurrentTypingCommand()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (RefPtr document = frame->document())
        document->editor().closeTyping();
}

void WebPage::setActivePopupMenu(WebPopupMenu* menu)
{
    m_activePopupMenu = menu;
}

#if ENABLE(INPUT_TYPE_COLOR)

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

#endif

#if ENABLE(DATALIST_ELEMENT)

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

#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

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

#endif

void WebPage::setActiveOpenPanelResultListener(Ref<WebOpenPanelResultListener>&& openPanelResultListener)
{
    m_activeOpenPanelResultListener = WTFMove(openPanelResultListener);
}

void WebPage::setTextIndicator(const WebCore::TextIndicatorData& indicatorData)
{
    send(Messages::WebPageProxy::SetTextIndicatorFromFrame(m_mainFrame->frameID(), indicatorData, static_cast<uint64_t>(WebCore::TextIndicatorLifetime::Temporary)));
}

bool WebPage::findStringFromInjectedBundle(const String& target, OptionSet<FindOptions> options)
{
    return m_page->findString(target, core(options)).has_value();
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

void WebPage::findRectsForStringMatches(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(Vector<FloatRect>&&)>&& completionHandler)
{
    findController().findRectsForStringMatches(string, options, maxMatchCount, WTFMove(completionHandler));
}

void WebPage::hideFindIndicator()
{
    findController().hideFindIndicator();
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
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->didChangeSelectedIndex(index);
}

#if PLATFORM(IOS_FAMILY)
void WebPage::didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>& files, const String& displayString, std::span<const uint8_t> iconData)
{
    if (!m_activeOpenPanelResultListener)
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

    m_activeOpenPanelResultListener->didChooseFilesWithDisplayStringAndIcon(files, displayString, icon.get());
    m_activeOpenPanelResultListener = nullptr;
}
#endif

void WebPage::didChooseFilesForOpenPanel(const Vector<String>& files, const Vector<String>& replacementFiles)
{
    if (!m_activeOpenPanelResultListener)
        return;

    m_activeOpenPanelResultListener->didChooseFiles(files, replacementFiles);
    m_activeOpenPanelResultListener = nullptr;
}

void WebPage::didCancelForOpenPanel()
{
    if (!m_activeOpenPanelResultListener)
        return;

    m_activeOpenPanelResultListener->didCancelFileChoosing();
    m_activeOpenPanelResultListener = nullptr;
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
    geolocationPermissionRequestManager().didReceiveGeolocationPermissionDecision(geolocationID, authorizationToken);
}

Ref<GeolocationPermissionRequestManager> WebPage::protectedGeolocationPermissionRequestManager()
{
    return m_geolocationPermissionRequestManager.get();
}
#endif

#if ENABLE(MEDIA_STREAM)

Ref<UserMediaPermissionRequestManager> WebPage::protectedUserMediaPermissionRequestManager()
{
    return m_userMediaPermissionRequestManager.get();
}

void WebPage::userMediaAccessWasGranted(UserMediaRequestIdentifier userMediaID, WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice, WebCore::MediaDeviceHashSalts&& mediaDeviceIdentifierHashSalts, Vector<SandboxExtension::Handle>&& handles, CompletionHandler<void()>&& completionHandler)
{
    SandboxExtension::consumePermanently(handles);

    protectedUserMediaPermissionRequestManager()->userMediaAccessWasGranted(userMediaID, WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(mediaDeviceIdentifierHashSalts), WTFMove(completionHandler));
}

void WebPage::userMediaAccessWasDenied(UserMediaRequestIdentifier userMediaID, uint64_t reason, String&& message, WebCore::MediaConstraintType invalidConstraint)
{
    protectedUserMediaPermissionRequestManager()->userMediaAccessWasDenied(userMediaID, static_cast<MediaAccessDenialReason>(reason), WTFMove(message), invalidConstraint);
}

void WebPage::captureDevicesChanged()
{
    protectedUserMediaPermissionRequestManager()->captureDevicesChanged();
}

void WebPage::voiceActivityDetected()
{
    corePage()->voiceActivityDetected();
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
#endif // USE(GSTREAMER)

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(ENCRYPTED_MEDIA)
Ref<MediaKeySystemPermissionRequestManager> WebPage::protectedMediaKeySystemPermissionRequestManager()
{
    return m_mediaKeySystemPermissionRequestManager.get();
}

void WebPage::mediaKeySystemWasGranted(MediaKeySystemRequestIdentifier mediaKeySystemID)
{
    protectedMediaKeySystemPermissionRequestManager()->mediaKeySystemWasGranted(mediaKeySystemID);
}

void WebPage::mediaKeySystemWasDenied(MediaKeySystemRequestIdentifier mediaKeySystemID, String&& message)
{
    protectedMediaKeySystemPermissionRequestManager()->mediaKeySystemWasDenied(mediaKeySystemID, WTFMove(message));
}
#endif

#if !PLATFORM(IOS_FAMILY)
void WebPage::advanceToNextMisspelling(bool startBeforeSelection)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().advanceToNextMisspelling(startBeforeSelection);
}
#endif

bool WebPage::hasRichlyEditableSelection() const
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return false;

    if (m_page->dragCaretController().isContentRichlyEditable())
        return true;

    return frame->selection().selection().isContentRichlyEditable();
}

void WebPage::changeSpellingToWord(const String& word)
{
    replaceSelectionWithText(m_page->checkedFocusController()->focusedOrMainFrame(), word);
}

void WebPage::unmarkAllMisspellings()
{
    for (auto* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (RefPtr document = localFrame->document())
            document->markers().removeMarkers(DocumentMarker::Type::Spelling);
    }
}

void WebPage::unmarkAllBadGrammar()
{
    for (auto* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (RefPtr document = localFrame->document())
            document->markers().removeMarkers(DocumentMarker::Type::Grammar);
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

    coreFrame->editor().uppercaseWord();
}

void WebPage::lowercaseWord(FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;

    coreFrame->editor().lowercaseWord();
}

void WebPage::capitalizeWord(FrameIdentifier frameID)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame)
        return;


    coreFrame->editor().capitalizeWord();
}
#endif
    
void WebPage::setTextForActivePopupMenu(int32_t index)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->setTextForIndex(index);
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
    return frame->editor().replaceSelectionWithText(text, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No);
}

#if !PLATFORM(IOS_FAMILY)
void WebPage::clearSelection()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->selection().clear();
}
#endif

void WebPage::restoreSelectionInFocusedEditableElement()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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
    if (RefPtr frame = dynamicDowncast<LocalFrame>(mainFrame())) {
        auto* webFrameLoaderClient = toWebLocalFrameLoaderClient(frame->loader().client());
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

    unsigned pageCount = m_page->pageCountAssumingLayoutIsUpToDate();
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
    if (decoder.messageReceiverName() == Messages::WebInspector::messageReceiverName()) {
        if (WebInspector* inspector = this->inspector())
            inspector->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::WebInspectorUI::messageReceiverName()) {
        if (WebInspectorUI* inspectorUI = this->inspectorUI())
            inspectorUI->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteWebInspectorUI::messageReceiverName()) {
        if (RemoteWebInspectorUI* remoteInspectorUI = this->remoteInspectorUI())
            remoteInspectorUI->didReceiveMessage(connection, decoder);
        return true;
    }

#if ENABLE(FULLSCREEN_API)
    if (decoder.messageReceiverName() == Messages::WebFullScreenManager::messageReceiverName()) {
        fullScreenManager()->didReceiveMessage(connection, decoder);
        return true;
    }
#endif
    return false;
}

#if ENABLE(ASYNC_SCROLLING)
ScrollingCoordinator* WebPage::scrollingCoordinator() const
{
    return m_page->scrollingCoordinator();
}
#endif

WebPage::SandboxExtensionTracker::~SandboxExtensionTracker()
{
    invalidate();
}

void WebPage::SandboxExtensionTracker::invalidate()
{
    m_pendingProvisionalSandboxExtension = nullptr;

    if (m_provisionalSandboxExtension) {
        m_provisionalSandboxExtension->revoke();
        m_provisionalSandboxExtension = nullptr;
    }

    if (m_committedSandboxExtension) {
        m_committedSandboxExtension->revoke();
        m_committedSandboxExtension = nullptr;
    }
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
    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->consume();
}

void WebPage::SandboxExtensionTracker::didCommitProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (m_committedSandboxExtension)
        m_committedSandboxExtension->revoke();

    m_committedSandboxExtension = WTFMove(m_provisionalSandboxExtension);

    // We can also have a non-null m_pendingProvisionalSandboxExtension if a new load is being started.
    // This extension is not cleared, because it does not pertain to the failed load, and will be needed.
}

void WebPage::SandboxExtensionTracker::didFailProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->revoke();
    m_provisionalSandboxExtension = nullptr;

    // We can also have a non-null m_pendingProvisionalSandboxExtension if a new load is being started
    // (notably, if the current one fails because the new one cancels it). This extension is not cleared,
    // because it does not pertain to the failed load, and will be needed.
}

void WebPage::setCustomTextEncodingName(const String& encoding)
{
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->loader().reloadWithOverrideEncoding(encoding);
}

void WebPage::didRemoveBackForwardItem(const BackForwardItemIdentifier& itemID)
{
    WebBackForwardListProxy::removeItem(itemID);
}

#if PLATFORM(COCOA)

bool WebPage::isSpeaking() const
{
    auto sendResult = const_cast<WebPage*>(this)->sendSync(Messages::WebPageProxy::GetIsSpeaking());
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

#endif

#if PLATFORM(MAC)
void WebPage::setCaretAnimatorType(WebCore::CaretAnimatorType caretType)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->selection().caretAnimatorInvalidated(caretType);
}

void WebPage::setCaretBlinkingSuspended(bool suspended)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->selection().setCaretBlinkingSuspended(suspended);
}

void WebPage::setUseSystemAppearance(bool useSystemAppearance)
{
    corePage()->setUseSystemAppearance(useSystemAppearance);
}

#endif

#if PLATFORM(COCOA)
RetainPtr<PDFDocument> WebPage::pdfDocumentForPrintingFrame(LocalFrame* coreFrame)
{
#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = pluginViewForFrame(coreFrame))
        return pluginView->pdfDocumentForPrinting();
#endif
    return nullptr;
}
#endif

void WebPage::setUseColorAppearance(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
    corePage()->setUseColorAppearance(useDarkAppearance, useElevatedUserInterfaceLevel);

    if (m_inspectorUI)
        m_inspectorUI->effectiveAppearanceDidChange(useDarkAppearance ? WebCore::InspectorFrontendClient::Appearance::Dark : WebCore::InspectorFrontendClient::Appearance::Light);
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

#if PLATFORM(MAC)
    if (pdfDocumentForPrintingFrame(coreFrame.get()))
        return;
#endif

    if (!m_printContext) {
        m_printContext = makeUnique<PrintContext>(coreFrame.get());
        m_page->dispatchBeforePrintEvent();
    }

    freezeLayerTree(LayerTreeFreezeReason::Printing);

    auto computedPageSize = m_printContext->computedPageSize(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight), printInfo.margin);

    m_printContext->begin(computedPageSize.width(), computedPageSize.height());

    // PrintContext::begin() performed a synchronous layout which might have executed a
    // script that closed the WebPage, clearing m_printContext.
    // See <rdar://problem/49731211> for cases of this happening.
    if (!m_printContext) {
        unfreezeLayerTree(LayerTreeFreezeReason::Printing);
        return;
    }

    float fullPageHeight;
    m_printContext->computePageRects(FloatRect(0, 0, computedPageSize.width(), computedPageSize.height()), 0, 0, printInfo.pageSetupScaleFactor, fullPageHeight, true);

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
        m_page->dispatchAfterPrintEvent();
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

    if (m_printContext) {
        PrintContextAccessScope scope { *this };
        resultPageRects = m_printContext->pageRects();
        computedPageMargin = m_printContext->computedPageMargin(printInfo.margin);
        auto computedPageSize = m_printContext->computedPageSize(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight), printInfo.margin);
        resultTotalScaleFactorForPrinting = m_printContext->computeAutomaticScaleFactor(computedPageSize) * printInfo.pageSetupScaleFactor;
    }
#if PLATFORM(COCOA)
    else
        computePagesForPrintingPDFDocument(frameID, printInfo, resultPageRects);
#endif // PLATFORM(COCOA)

    // If we're asked to print, we should actually print at least a blank page.
    if (resultPageRects.isEmpty())
        resultPageRects.append(IntRect(0, 0, 1, 1));
}

#if PLATFORM(COCOA)
void WebPage::drawToPDF(FrameIdentifier frameID, const std::optional<FloatRect>& rect, bool allowTransparentBackground,  CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;

    Ref frameView = *localMainFrame->view();
    IntSize snapshotSize;
    if (rect)
        snapshotSize = IntSize(rect->size());
    else
        snapshotSize = { frameView->contentsSize() };

    IntRect snapshotRect;
    if (rect)
        snapshotRect = { {(int)rect->x(), (int)rect->y()}, snapshotSize };
    else
        snapshotRect = { {0, 0}, snapshotSize };

    auto originalLayoutViewportOverrideRect = frameView->layoutViewportOverrideRect();
    frameView->setLayoutViewportOverrideRect(LayoutRect(snapshotRect));
    auto originalPaintBehavior = frameView->paintBehavior();

    frameView->setPaintBehavior(originalPaintBehavior | PaintBehavior::AnnotateLinks);

    auto originalColor = frameView->baseBackgroundColor();
    if (allowTransparentBackground) {
        frameView->setTransparent(true);
        frameView->setBaseBackgroundColor(Color::transparentBlack);
    }

    auto pdfData = pdfSnapshotAtSize(snapshotRect, snapshotSize, { });

    if (allowTransparentBackground) {
        frameView->setTransparent(false);
        frameView->setBaseBackgroundColor(originalColor);
    }

    frameView->setLayoutViewportOverrideRect(originalLayoutViewportOverrideRect);
    frameView->setPaintBehavior(originalPaintBehavior);

    completionHandler(SharedBuffer::create(pdfData.get()));
}

void WebPage::drawRectToImage(FrameIdentifier frameID, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    PrintContextAccessScope scope { *this };
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = frame ? frame->coreLocalFrame() : 0;

    RefPtr<WebImage> image;

#if USE(CG)
    if (coreFrame) {
#if PLATFORM(MAC)
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame.get()));
#else
        ASSERT(coreFrame->document()->printing());
#endif
        
        image = WebImage::create(imageSize, ImageOption::Local, DestinationColorSpace::SRGB(), &m_page->chrome().client());
        if (!image || !image->context()) {
            ASSERT_NOT_REACHED();
            return completionHandler({ });
        }

        auto& graphicsContext = *image->context();
        float printingScale = static_cast<float>(imageSize.width()) / rect.width();
        graphicsContext.scale(printingScale);

#if PLATFORM(MAC)
        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame.get())) {
            ASSERT(!m_printContext);
            graphicsContext.scale(FloatSize(1, -1));
            graphicsContext.translate(0, -rect.height());
            drawPDFDocument(graphicsContext.platformContext(), pdfDocument.get(), printInfo, rect);
        } else
#endif
        {
            m_printContext->spoolRect(graphicsContext, rect);
        }
    }
#endif

    std::optional<ShareableBitmap::Handle> handle;
    if (image)
        handle = image->createHandle(SharedMemory::Protection::ReadOnly);

    completionHandler(WTFMove(handle));
}

void WebPage::drawPagesToPDF(FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& callback)
{
    PrintContextAccessScope scope { *this };
    RetainPtr<CFMutableDataRef> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, first, count, pdfPageData);
    callback(SharedBuffer::create(pdfPageData.get()));
}

void WebPage::drawPagesToPDFImpl(FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, RetainPtr<CFMutableDataRef>& pdfPageData)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = frame ? frame->coreLocalFrame() : 0;

    pdfPageData = adoptCF(CFDataCreateMutable(0, 0));

#if USE(CG)
    if (coreFrame) {
#if PLATFORM(MAC)
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame.get()));
#else
        ASSERT(coreFrame->document()->printing());
#endif

        // FIXME: Use CGDataConsumerCreate with callbacks to avoid copying the data.
        RetainPtr<CGDataConsumerRef> pdfDataConsumer = adoptCF(CGDataConsumerCreateWithCFData(pdfPageData.get()));

        CGRect mediaBox = (m_printContext && m_printContext->pageCount()) ? m_printContext->pageRect(0) : CGRectMake(0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight);

        RetainPtr<CGContextRef> context = adoptCF(CGPDFContextCreate(pdfDataConsumer.get(), &mediaBox, 0));

#if PLATFORM(MAC)
        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame.get())) {
            ASSERT(!m_printContext);
            drawPagesToPDFFromPDFDocument(context.get(), pdfDocument.get(), printInfo, first, count);
        } else
#endif
        {
            if (!m_printContext)
                return;

            for (uint32_t page = first; page < first + count; ++page) {
                if (page >= m_printContext->pageCount())
                    break;

                RetainPtr<CFDictionaryRef> pageInfo = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
                CGPDFContextBeginPage(context.get(), pageInfo.get());

                GraphicsContextCG ctx(context.get());
                ctx.scale(FloatSize(1, -1));
                ctx.translate(0, -m_printContext->pageRect(page).height());
                m_printContext->spoolPage(ctx, page, m_printContext->pageRect(page).width());

                CGPDFContextEndPage(context.get());
            }
        }
        CGPDFContextClose(context.get());
    }
#endif
}

#elif PLATFORM(GTK)
void WebPage::drawPagesForPrinting(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(std::optional<SharedMemory::Handle>&&, WebCore::ResourceError&&)>&& completionHandler)
{
    beginPrinting(frameID, printInfo);
    if (m_printContext && m_printOperation) {
        m_printOperation->startPrint(m_printContext.get(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (RefPtr<WebCore::FragmentedSharedBuffer>&& data, WebCore::ResourceError&& error) mutable {
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

void WebPage::addResourceRequest(WebCore::ResourceLoaderIdentifier identifier, bool isMainResourceLoad, const WebCore::ResourceRequest& request, const DocumentLoader* loader, LocalFrame* frame)
{
    if (frame && !isMainResourceLoad) {
        auto frameID = frame->frameID();
        auto addResult = m_networkResourceRequestCountForPageLoadTiming.add(frameID, 0);
        if (!addResult.iterator->value)
            send(Messages::WebPageProxy::StartNetworkRequestsForPageLoadTiming(frameID));
        ++addResult.iterator->value;
    }

    if (!request.url().protocolIsInHTTPFamily())
        return;

    if (m_mainFrameProgressCompleted && !UserGestureIndicator::processingUserGesture())
        return;

    ASSERT(!m_trackedNetworkResourceRequestIdentifiers.contains(identifier));
    bool wasEmpty = m_trackedNetworkResourceRequestIdentifiers.isEmpty();
    m_trackedNetworkResourceRequestIdentifiers.add(identifier);
    if (wasEmpty)
        send(Messages::WebPageProxy::SetNetworkRequestsInProgress(true));
}

void WebPage::removeResourceRequest(WebCore::ResourceLoaderIdentifier identifier, bool isMainResourceLoad, LocalFrame* frame)
{
    if (frame && !isMainResourceLoad) {
        auto frameID = frame->frameID();
        auto it = m_networkResourceRequestCountForPageLoadTiming.find(frameID);
        ASSERT(it != m_networkResourceRequestCountForPageLoadTiming.end());
        --it->value;
        if (!it->value)
            send(Messages::WebPageProxy::EndNetworkRequestsForPageLoadTiming(frameID, WallTime::now()));
    }

    if (!m_trackedNetworkResourceRequestIdentifiers.remove(identifier))
        return;

    if (m_trackedNetworkResourceRequestIdentifiers.isEmpty())
        send(Messages::WebPageProxy::SetNetworkRequestsInProgress(false));
}

void WebPage::setMediaVolume(float volume)
{
    m_page->setMediaVolume(volume);
}

void WebPage::setMuted(MediaProducerMutedStateFlags state, CompletionHandler<void()>&& completionHandler)
{
    m_page->setMuted(state);
    completionHandler();
}

void WebPage::stopMediaCapture(MediaProducerMediaCaptureKind kind, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    m_page->stopMediaCapture(kind);
#endif
    completionHandler();
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

#if PLATFORM(COCOA)
void WebPage::handleAlternativeTextUIResult(const String& result)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().handleAlternativeTextUIResult(result);
}
#endif

void WebPage::setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length, bool suppressUnderline, const Vector<CompositionHighlight>& highlights, const HashMap<String, Vector<WebCore::CharacterRange>>& annotations)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (!frame->editor().canEdit())
        return;

    Vector<CompositionUnderline> underlines;
    if (!suppressUnderline)
        underlines.append(CompositionUnderline(0, compositionString.length(), CompositionUnderlineColor::TextColor, Color(Color::black), false));

    frame->editor().setComposition(compositionString, underlines, highlights, annotations, from, from + length);
}

bool WebPage::hasCompositionForTesting()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return false;

    return frame->editor().hasComposition();
}

void WebPage::confirmCompositionForTesting(const String& compositionString)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (!frame->editor().canEdit())
        return;

    if (compositionString.isNull())
        frame->editor().confirmComposition();
    frame->editor().confirmComposition(compositionString);
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
    if (Scrollbar* scrollbar = scrollableArea->horizontalScrollbar())
        return scrollbar->enabled();

    return false;
}

static bool pageContainsAnyHorizontalScrollbars(LocalFrame* mainFrame)
{
    if (!mainFrame)
        return false;

    if (RefPtr frameView = mainFrame->view()) {
        if (hasEnabledHorizontalScrollbar(frameView.get()))
            return true;
    }

    for (Frame* frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;

        RefPtr frameView = localFrame->view();
        if (!frameView)
            continue;

        auto scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (auto& area : *scrollableAreas) {
            CheckedPtr<ScrollableArea> scrollableArea(area);
            if (!scrollableArea->scrollbarsCanBeActive())
                continue;

            if (hasEnabledHorizontalScrollbar(scrollableArea.get()))
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
        if (pageContainsAnyHorizontalScrollbars(dynamicDowncast<LocalFrame>(mainFrame())))
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

FrameView* WebPage::mainFrameView() const
{
    if (auto* frame = mainFrame())
        return frame->virtualView();
    return nullptr;
}

LocalFrameView* WebPage::localMainFrameView() const
{
    return dynamicDowncast<LocalFrameView>(mainFrameView());
}

bool WebPage::shouldUseCustomContentProviderForResponse(const ResourceResponse& response)
{
    auto& mimeType = response.mimeType();
    if (mimeType.isNull())
        return false;

    return m_mimeTypesWithCustomContentProviders.contains(mimeType);
}

#if PLATFORM(COCOA)

void WebPage::setTextAsync(const String& text)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable()) {
        UserTypingGestureIndicator indicator(*frame);
        frame->selection().selectAll();
        if (text.isEmpty())
            frame->editor().deleteSelectionWithSmartDelete(false);
        else
            frame->editor().insertText(text, nullptr, TextEventInputKeyboard);
        return;
    }

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(m_focusedElement)) {
        input->setValueForUser(text);
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebPage::insertTextAsync(const String& text, const EditingRange& replacementEditingRange, InsertTextOptions&& options)
{
    platformWillPerformEditingCommand();

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    UserGestureIndicator gestureIndicator { options.processingUserGesture ? IsProcessingUserGesture::Yes : IsProcessingUserGesture::No, frame->document() };

    bool replacesText = false;
    if (replacementEditingRange.location != notFound) {
        if (auto replacementRange = EditingRange::toRange(*frame, replacementEditingRange, options.editingRangeIsRelativeTo)) {
            SetForScope isSelectingTextWhileInsertingAsynchronously(m_isSelectingTextWhileInsertingAsynchronously, options.suppressSelectionUpdate);
            frame->selection().setSelection(VisibleSelection(*replacementRange));
            replacesText = replacementEditingRange.length;
        }
    }
    
    if (options.registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping());

    RefPtr focusedElement = frame->document() ? frame->document()->focusedElement() : nullptr;
    if (focusedElement && options.shouldSimulateKeyboardInput)
        focusedElement->dispatchEvent(Event::create(eventNames().keydownEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));

    if (!frame->editor().hasComposition()) {
        if (text.isEmpty() && frame->selection().isRange())
            frame->editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::CharacterGranularity, false, true);
        else {
            // An insertText: might be handled by other responders in the chain if we don't handle it.
            // One example is space bar that results in scrolling down the page.
            frame->editor().insertText(text, nullptr, replacesText ? TextEventInputAutocompletion : TextEventInputKeyboard);
        }
    } else
        frame->editor().confirmComposition(text);

    if (focusedElement && options.shouldSimulateKeyboardInput) {
        focusedElement->dispatchEvent(Event::create(eventNames().keyupEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
        focusedElement->dispatchEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
    }
}

void WebPage::hasMarkedText(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr focusedOrMainFrame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return completionHandler(false);
    completionHandler(focusedOrMainFrame->editor().hasComposition());
}

void WebPage::getMarkedRangeAsync(CompletionHandler<void(const EditingRange&)>&& completionHandler)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

    completionHandler(EditingRange::fromRange(*frame, frame->editor().compositionRange()));
}

void WebPage::getSelectedRangeAsync(CompletionHandler<void(const EditingRange&)>&& completionHandler)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

    completionHandler(EditingRange::fromRange(*frame, frame->selection().selection().toNormalizedRange()));
}

void WebPage::characterIndexForPointAsync(const WebCore::IntPoint& point, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent,  HitTestRequest::Type::AllowChildFrameContent };
    auto result = localMainFrame->eventHandler().hitTestResultAtPoint(point, hitType);
    RefPtr frame = result.innerNonSharedNode() ? result.innerNodeFrame() : m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });
    auto range = frame->rangeForPoint(result.roundedPointInInnerNodeFrame());
    auto editingRange = EditingRange::fromRange(*frame, range);
    completionHandler(editingRange.location);
}

void WebPage::firstRectForCharacterRangeAsync(const EditingRange& editingRange, CompletionHandler<void(const WebCore::IntRect&, const EditingRange&)>&& completionHandler)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler({ }, { });

    auto range = EditingRange::toRange(*frame, editingRange);
    if (!range)
        return completionHandler({ }, editingRange);

    auto rect = RefPtr(frame->view())->contentsToWindow(frame->editor().firstRectForRange(*range));
    auto startPosition = makeContainerOffsetPosition(range->start);

    auto endPosition = endOfLine(startPosition);
    if (endPosition.isNull())
        endPosition = startPosition;
    else if (endPosition.affinity() == Affinity::Downstream && inSameLine(startPosition, endPosition)) {
        auto nextLineStartPosition = positionOfNextBoundaryOfGranularity(endPosition, TextGranularity::LineGranularity, SelectionDirection::Forward);
        if (nextLineStartPosition.isNotNull() && endPosition < nextLineStartPosition)
            endPosition = nextLineStartPosition;
    }

    auto endBoundary = makeBoundaryPoint(endPosition);
    if (!endBoundary)
        return completionHandler({ }, editingRange);

    auto rangeForFirstLine = EditingRange::fromRange(*frame, makeSimpleRange(range->start, WTFMove(endBoundary)));

    rangeForFirstLine.location = std::min(std::max(rangeForFirstLine.location, editingRange.location), editingRange.location + editingRange.length);
    rangeForFirstLine.length = std::min(rangeForFirstLine.location + rangeForFirstLine.length, editingRange.location + editingRange.length) - rangeForFirstLine.location;

    completionHandler(rect, rangeForFirstLine);
}

void WebPage::setCompositionAsync(const String& text, const Vector<CompositionUnderline>& underlines, const Vector<CompositionHighlight>& highlights, const HashMap<String, Vector<CharacterRange>>& annotations, const EditingRange& selection, const EditingRange& replacementEditingRange)
{
    platformWillPerformEditingCommand();

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable()) {
        if (replacementEditingRange.location != notFound) {
            if (auto replacementRange = EditingRange::toRange(*frame, replacementEditingRange))
                frame->selection().setSelection(VisibleSelection(*replacementRange));
        }
        frame->editor().setComposition(text, underlines, highlights, annotations, selection.location, selection.location + selection.length);
    }
}

void WebPage::setWritingSuggestion(const String& fullTextWithPrediction, const EditingRange& selection)
{
#if PLATFORM(COCOA)
    platformWillPerformEditingCommand();

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().setWritingSuggestion(fullTextWithPrediction, { selection.location, selection.length });
#else
    UNUSED_PARAM(fullTextWithPrediction);
    UNUSED_PARAM(selection);
#endif
}

void WebPage::confirmCompositionAsync()
{
    platformWillPerformEditingCommand();

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    frame->editor().confirmComposition();
}

#endif // PLATFORM(COCOA)

#if PLATFORM(GTK) || PLATFORM(WPE)

static RefPtr<LocalFrame> targetFrameForEditing(WebPage& page)
{
    RefPtr targetFrame = page.corePage()->checkedFocusController()->focusedOrMainFrame();
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
        targetFrame->editor().confirmComposition(compositionString);
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
    targetFrame->selection().setSelection(VisibleSelection(selectionRange));
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
    corePage()->updateStateForSelectedSuggestionIfNeeded();
#endif

#if PLATFORM(IOS_FAMILY)
    resetLastSelectedReplacementRangeIfNeeded();

    if (!std::exchange(m_sendAutocorrectionContextAfterFocusingElement, false))
        return;

    callOnMainRunLoop([protectedThis = Ref { *this }, frame = Ref { frame }] {
        if (UNLIKELY(!frame->document() || !frame->document()->hasLivingRenderTree() || frame->selection().isNone()))
            return;

        protectedThis->preemptivelySendAutocorrectionContext();
    });
#else
    UNUSED_PARAM(frame);
#endif // PLATFORM(IOS_FAMILY)
}

void WebPage::didChangeSelectionOrOverflowScrollPosition()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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
        if (frame->document()->quirks().isTouchBarUpdateSuppressedForHiddenContentEditable()) {
            m_isTouchBarUpdateSuppressedForHiddenContentEditable = true;
            send(Messages::WebPageProxy::SetIsTouchBarUpdateSuppressedForHiddenContentEditable(m_isTouchBarUpdateSuppressedForHiddenContentEditable));
        }

        if (frame->document()->quirks().isNeverRichlyEditableForTouchBar()) {
            m_isNeverRichlyEditableForTouchBar = true;
            send(Messages::WebPageProxy::SetIsNeverRichlyEditableForTouchBar(m_isNeverRichlyEditableForTouchBar));
        }

        send(Messages::WebPageProxy::SetHasFocusedElementWithUserInteraction(true));
    }

    // Abandon the current inline input session if selection changed for any other reason but an input method direct action.
    // FIXME: This logic should be in WebCore.
    // FIXME: Many changes that affect composition node do not go through didChangeSelection(). We need to do something when DOM manipulation affects the composition, because otherwise input method's idea about it will be different from Editor's.
    // FIXME: We can't cancel composition when selection changes to NoSelection, but we probably should.
    if (frame->editor().hasComposition() && !frame->editor().ignoreSelectionChanges() && !frame->selection().isNone()) {
        frame->editor().cancelComposition();
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
    if (frame->isMainFrame() || m_page->checkedFocusController()->focusedOrMainFrame() == frame->coreLocalFrame())
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

void WebPage::elementDidFocus(Element& element, const FocusOptions& options)
{
#if PLATFORM(IOS_FAMILY)
    m_updateFocusedElementInformationTimer.stop();
#endif

    if (!shouldDispatchUpdateAfterFocusingElement(element)) {
        updateInputContextAfterBlurringAndRefocusingElementIfNeeded(element);
        m_focusedElement = &element;
        m_recentlyBlurredElement = nullptr;
        return;
    }

    if (is<HTMLSelectElement>(element) || isTextFormControlOrEditableContent(element)) {
#if PLATFORM(IOS_FAMILY)
        bool isChangingFocusedElement = m_focusedElement != &element;
#endif
        m_focusedElement = &element;
        m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement = false;

#if PLATFORM(IOS_FAMILY)

#if ENABLE(FULLSCREEN_API)
        if (element.document().fullscreenManager().isFullscreen())
            element.document().fullscreenManager().cancelFullscreen();
#endif
        if (isChangingFocusedElement && (m_userIsInteracting || m_keyboardIsAttached))
            m_sendAutocorrectionContextAfterFocusingElement = true;

        auto information = focusedElementInformation();
        if (!information)
            return;

        RefPtr<API::Object> userData;

        m_formClient->willBeginInputSession(this, &element, WebFrame::fromCoreFrame(*element.document().frame()).get(), m_userIsInteracting, userData);

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
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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

    RefPtr view = corePage()->mainFrame().virtualView();
    if (!alwaysShowsHorizontalScroller)
        view->setHorizontalScrollbarLock(false);
    view->setHorizontalScrollbarMode(alwaysShowsHorizontalScroller ? ScrollbarMode::AlwaysOn : m_mainFrameIsScrollable ? ScrollbarMode::Auto : ScrollbarMode::AlwaysOff, alwaysShowsHorizontalScroller || !m_mainFrameIsScrollable);
}

void WebPage::setAlwaysShowsVerticalScroller(bool alwaysShowsVerticalScroller)
{
    if (alwaysShowsVerticalScroller == m_alwaysShowsVerticalScroller)
        return;

    m_alwaysShowsVerticalScroller = alwaysShowsVerticalScroller;

    RefPtr view = corePage()->mainFrame().virtualView();
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

    if (size.width() <= 0) {
        localMainFrame->view()->enableFixedWidthAutoSizeMode(false, { });
        return;
    }

    localMainFrame->view()->enableFixedWidthAutoSizeMode(true, { size.width(), std::max(size.height(), 1) });
}

void WebPage::setSizeToContentAutoSizeMaximumSize(const IntSize& size)
{
    if (m_sizeToContentAutoSizeMaximumSize == size)
        return;

    m_sizeToContentAutoSizeMaximumSize = size;

    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame)
        return;

    if (size.width() <= 0 || size.height() <= 0) {
        localMainFrame->view()->enableSizeToContentAutoSizeMode(false, { });
        return;
    }

    localMainFrame->view()->enableSizeToContentAutoSizeMode(true, size);
}

void WebPage::setAutoSizingShouldExpandToViewHeight(bool shouldExpand)
{
    if (m_autoSizingShouldExpandToViewHeight == shouldExpand)
        return;

    m_autoSizingShouldExpandToViewHeight = shouldExpand;

    if (RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame()))
        localMainFrame->view()->setAutoSizeFixedMinimumHeight(shouldExpand ? m_viewSize.height() : 0);
}

void WebPage::setViewportSizeForCSSViewportUnits(std::optional<WebCore::FloatSize> viewportSize)
{
    if (m_viewportSizeForCSSViewportUnits == viewportSize)
        return;

    m_viewportSizeForCSSViewportUnits = viewportSize;
    if (m_viewportSizeForCSSViewportUnits) {
        if (RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame()))
            localMainFrame->view()->setSizeForCSSDefaultViewportUnits(*m_viewportSizeForCSSViewportUnits);
    }
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
        return m_page->pluginData().supportsWebVisibleMimeTypeForURL(mimeType, allowedPlugins, response.url());
    });
}

bool WebPage::canShowMIMEType(const String& mimeType) const
{
    return canShowMIMEType(mimeType, [&](auto& mimeType, auto allowedPlugins) {
        return m_page->pluginData().supportsWebVisibleMimeType(mimeType, allowedPlugins);
    });
}

bool WebPage::canShowMIMEType(const String& mimeType, const Function<bool(const String&, PluginData::AllowedPluginTypes)>& pluginsSupport) const
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
        startTextManipulationForFrame(*frame->coreLocalFrame());

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
        Page* page = frame->coreLocalFrame()->page();

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
    m_lastTransactionIDWithScaleChange = firstTransactionIDAfterDidCommitLoad;
    m_scaleWasSetByUIProcess = false;
    m_userHasChangedPageScaleFactor = false;
    m_estimatedLatency = Seconds(1.0 / 60);
    m_shouldRevealCurrentSelectionAfterInsertion = true;
    m_lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage = std::nullopt;
    m_lastSelectedReplacementRange = { };

    invokePendingSyntheticClickCallback(SyntheticClickResult::PageInvalid);

#if ENABLE(IOS_TOUCH_EVENTS)
    auto queuedEvents = makeUniqueRef<EventDispatcher::TouchEventQueue>();
    WebProcess::singleton().eventDispatcher().takeQueuedTouchEventsForPage(*this, queuedEvents);
    cancelAsynchronousTouchEvents(WTFMove(queuedEvents));
#endif
#endif // PLATFORM(IOS_FAMILY)

    RefPtr coreFrame = frame->coreLocalFrame();
#if ENABLE(META_VIEWPORT)
    resetViewportDefaultConfiguration(frame);
    
    bool viewportChanged = false;

    setCanIgnoreViewportArgumentsToAvoidExcessiveZoomIfNeeded(m_viewportConfiguration, coreFrame.get(), shouldIgnoreMetaViewport());
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
        if (RefPtr loader = coreFrame->document()->loader(); loader
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

    flushDeferredDidReceiveMouseEvent();
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
}

void WebPage::didSameDocumentNavigationForFrame(WebFrame& frame)
{
    RefPtr<API::Object> userData;

    auto navigationID = frame.coreLocalFrame()->loader().documentLoader()->navigationID();

    if (frame.isMainFrame())
        m_pendingNavigationID = std::nullopt;

    // Notify the bundle client.
    injectedBundleLoaderClient().didSameDocumentNavigationForFrame(*this, frame, SameDocumentNavigationType::AnchorNavigation, userData);

    // Notify the UIProcess.
    send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(frame.frameID(), navigationID, SameDocumentNavigationType::AnchorNavigation, frame.coreLocalFrame()->document()->url(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

#if ENABLE(PDF_PLUGIN)
    for (auto& pluginView : m_pluginViews)
        pluginView.didSameDocumentNavigationForFrame(frame);
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
    auto* frame = frameWithSelection(m_page.get());
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

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
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

    m_page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void WebPage::loadAndDecodeImage(WebCore::ResourceRequest&& request, std::optional<WebCore::FloatSize> sizeConstraint, size_t maximumBytesFromNetwork, CompletionHandler<void(std::variant<WebCore::ResourceError, Ref<WebCore::ShareableBitmap>>&&)>&& completionHandler)
{
    URL url = request.url();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::LoadImageForDecoding(WTFMove(request), m_webPageProxyIdentifier, maximumBytesFromNetwork), [completionHandler = WTFMove(completionHandler), sizeConstraint, url] (std::variant<WebCore::ResourceError, Ref<WebCore::FragmentedSharedBuffer>>&& result) mutable {
        WTF::switchOn(WTFMove(result), [&] (WebCore::ResourceError&& error) {
            completionHandler(WTFMove(error));
        }, [&] (Ref<WebCore::FragmentedSharedBuffer>&& buffer) {
            Ref bitmapImage = WebCore::BitmapImage::create(nullptr);
            bitmapImage->setData(buffer.ptr(), true);
            RefPtr nativeImage = bitmapImage->primaryNativeImage();
            if (!nativeImage)
                return completionHandler(decodeError(url));

            FloatSize sourceSize = nativeImage->size();
            FloatSize destinationSize = sourceSize;
            if (sizeConstraint)
                destinationSize = largestRectWithAspectRatioInsideRect(sourceSize.aspectRatio(), FloatRect({ }, sizeConstraint->shrunkTo(sourceSize))).size();

            IntSize roundedDestinationSize = flooredIntSize(destinationSize);
            auto sourceColorSpace = nativeImage->colorSpace();
            auto destinationColorSpace = sourceColorSpace.supportsOutput() ? sourceColorSpace : DestinationColorSpace::SRGB();
            auto bitmap = ShareableBitmap::create({ roundedDestinationSize, destinationColorSpace });
            if (!bitmap)
                return completionHandler({ });

            auto context = bitmap->createGraphicsContext();
            if (!context)
                return completionHandler({ });

            context->drawNativeImage(*nativeImage, FloatRect({ }, roundedDestinationSize), FloatRect({ }, sourceSize), { CompositeOperator::Copy });

            completionHandler(bitmap.releaseNonNull());
        });
    });
}

#if PLATFORM(COCOA)
void WebPage::getInformationFromImageData(const Vector<uint8_t>& data, CompletionHandler<void(Expected<std::pair<String, Vector<IntSize>>, WebCore::ImageDecodingError>&&)>&& completionHandler)
{
    if (m_isClosed)
        return completionHandler(makeUnexpected(ImageDecodingError::Internal));

    if (SVGImage::isDataDecodable(m_page->settings(), data.span()))
        return completionHandler(std::make_pair(String { "public.svg-image"_s }, Vector<IntSize> { }));

    completionHandler(utiAndAvailableSizesFromImageData(data.span()));
}
#endif

#if PLATFORM(MAC)
void WebPage::flushPendingThemeColorChange()
{
    if (!m_pendingThemeColorChange)
        return;

    m_pendingThemeColorChange = false;

    send(Messages::WebPageProxy::ThemeColorChanged(m_page->themeColor()));
}
#endif

void WebPage::flushPendingPageExtendedBackgroundColorChange()
{
    if (!m_pendingPageExtendedBackgroundColorChange)
        return;

    m_pendingPageExtendedBackgroundColorChange = false;

    send(Messages::WebPageProxy::PageExtendedBackgroundColorDidChange(m_page->pageExtendedBackgroundColor()));
}

void WebPage::flushPendingSampledPageTopColorChange()
{
    if (!m_pendingSampledPageTopColorChange)
        return;

    m_pendingSampledPageTopColorChange = false;

    send(Messages::WebPageProxy::SampledPageTopColorChanged(m_page->sampledPageTopColor()));
}

void WebPage::flushPendingEditorStateUpdate()
{
    if (!hasPendingEditorStateUpdate())
        return;

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->editor().ignoreSelectionChanges())
        return;

    sendEditorStateUpdate();
}

void WebPage::updateWebsitePolicies(WebsitePoliciesData&& websitePolicies)
{
    if (!m_page)
        return;

    if (auto* remoteMainFrameClient = m_mainFrame->remoteFrameClient()) {
        remoteMainFrameClient->applyWebsitePolicies(WTFMove(websitePolicies));
        return;
    }

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    RefPtr documentLoader = localMainFrame ? localMainFrame->loader().documentLoader() : nullptr;
    if (!documentLoader)
        return;

    m_allowsContentJavaScriptFromMostRecentNavigation = websitePolicies.allowsContentJavaScript;
    WebsitePoliciesData::applyToDocumentLoader(WTFMove(websitePolicies), *documentLoader);
    
#if ENABLE(VIDEO)
    m_page->updateMediaElementRateChangeRestrictions();
#endif

#if ENABLE(META_VIEWPORT)
    setCanIgnoreViewportArgumentsToAvoidExcessiveZoomIfNeeded(m_viewportConfiguration, localMainFrame.get(), shouldIgnoreMetaViewport());
#endif
}

void WebPage::notifyUserScripts()
{
    if (!m_page)
        return;

    m_page->notifyToInjectUserScripts();
}

unsigned WebPage::extendIncrementalRenderingSuppression()
{
    unsigned token = m_maximumRenderingSuppressionToken + 1;
    while (!HashSet<unsigned>::isValidValue(token) || m_activeRenderingSuppressionTokens.contains(token))
        token++;

    m_activeRenderingSuppressionTokens.add(token);
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->view()->setVisualUpdatesAllowedByClient(false);

    m_maximumRenderingSuppressionToken = token;

    return token;
}

void WebPage::stopExtendingIncrementalRenderingSuppression(unsigned token)
{
    if (!m_activeRenderingSuppressionTokens.remove(token))
        return;

    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->view()->setVisualUpdatesAllowedByClient(!shouldExtendIncrementalRenderingSuppression());
}
    
void WebPage::setScrollPinningBehavior(WebCore::ScrollPinningBehavior pinning)
{
    m_scrollPinningBehavior = pinning;
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->view()->setScrollPinningBehavior(m_scrollPinningBehavior);
}

void WebPage::setScrollbarOverlayStyle(std::optional<uint32_t> scrollbarStyle)
{
    if (scrollbarStyle)
        m_scrollbarOverlayStyle = static_cast<ScrollbarOverlayStyle>(scrollbarStyle.value());
    else
        m_scrollbarOverlayStyle = std::optional<ScrollbarOverlayStyle>();
    
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->view()->recalculateScrollbarOverlayStyle();
}

Ref<DocumentLoader> WebPage::createDocumentLoader(LocalFrame& frame, const ResourceRequest& request, const SubstituteData& substituteData)
{
    auto documentLoader = DocumentLoader::create(request, substituteData);

    documentLoader->setLastNavigationWasAppInitiated(m_lastNavigationWasAppInitiated);

    if (frame.isMainFrame() || m_page->settings().siteIsolationEnabled()) {
        if (m_pendingNavigationID) {
            documentLoader->setNavigationID(*m_pendingNavigationID);
            m_pendingNavigationID = std::nullopt;
        }

        if (m_pendingWebsitePolicies && frame.isMainFrame()) {
            m_allowsContentJavaScriptFromMostRecentNavigation = m_pendingWebsitePolicies->allowsContentJavaScript;
            WebsitePoliciesData::applyToDocumentLoader(WTFMove(*m_pendingWebsitePolicies), documentLoader);
            m_pendingWebsitePolicies = std::nullopt;
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
    if (LIKELY(!commonVM().m_perBytecodeProfiler))
        return callback({ });

    String result = commonVM().m_perBytecodeProfiler->toJSON()->toJSONString();
    ASSERT(result.length());
    callback(result);
}

void WebPage::getSamplingProfilerOutput(CompletionHandler<void(const String&)>&& callback)
{
#if ENABLE(SAMPLING_PROFILER)
    SamplingProfiler* samplingProfiler = commonVM().samplingProfiler();
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

void WebPage::postMessageWithAsyncReply(const String& messageName, API::Object* messageBody, CompletionHandler<void(API::Object*)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::HandleMessageWithAsyncReply(messageName, UserData(messageBody)), [completionHandler = WTFMove(completionHandler)] (UserData reply) mutable {
        completionHandler(reply.object());
    });
}

void WebPage::postMessageIgnoringFullySynchronousMode(const String& messageName, API::Object* messageBody)
{
    send(Messages::WebPageProxy::HandleMessage(messageName, UserData(WebProcess::singleton().transformObjectsToHandles(messageBody))), IPC::SendOption::IgnoreFullySynchronousMode);
}

void WebPage::postSynchronousMessageForTesting(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    auto& webProcess = WebProcess::singleton();

    auto sendResult = sendSync(Messages::WebPageProxy::HandleSynchronousMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))), Seconds::infinity(), IPC::SendSyncOption::UseFullySynchronousModeForTesting);
    if (sendResult.succeeded()) {
        auto& [returnUserData] = sendResult.reply();
        returnData = webProcess.transformHandlesToObjects(returnUserData.object());
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

void WebPage::addUserScript(String&& source, InjectedBundleScriptWorld& world, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserScriptInjectionTime injectionTime)
{
    WebCore::UserScript userScript { WTFMove(source), URL(aboutBlankURL()), Vector<String>(), Vector<String>(), injectionTime, injectedFrames, WebCore::WaitForNotificationBeforeInjecting::No };

    Ref { m_userContentController }->addUserScript(world, WTFMove(userScript));
}

void WebPage::addUserStyleSheet(const String& source, WebCore::UserContentInjectedFrames injectedFrames)
{
    WebCore::UserStyleSheet userStyleSheet { source, aboutBlankURL(), Vector<String>(), Vector<String>(), injectedFrames, UserStyleLevel::User };

    Ref { m_userContentController }->addUserStyleSheet(InjectedBundleScriptWorld::normalWorld(), WTFMove(userStyleSheet));
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
    m_page->setUserInterfaceLayoutDirection(m_userInterfaceLayoutDirection);
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

void WebPage::setUseIconLoadingClient(bool useIconLoadingClient)
{
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame)
        return;
    if (auto* client = toWebLocalFrameLoaderClient(localMainFrame->loader().client()))
        client->setUseIconLoadingClient(useIconLoadingClient);
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
    m_identifierToURLSchemeHandlerProxyMap.add(handlerIdentifier, *schemeResult.iterator->value);
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
    
void WebPage::urlSchemeTaskDidReceiveResponse(WebURLSchemeHandlerIdentifier handlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, const ResourceResponse& response)
{
    RefPtr handler = m_identifierToURLSchemeHandlerProxyMap.get(handlerIdentifier);
    ASSERT(handler);

    handler->taskDidReceiveResponse(taskIdentifier, response);
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

void WebPage::setIsSuspended(bool suspended)
{
    if (m_isSuspended == suspended)
        return;

    m_isSuspended = suspended;

    if (!suspended)
        return;

    // Unfrozen on drawing area reset.
    freezeLayerTree(LayerTreeFreezeReason::PageSuspended);

    // Only the committed WebPage gets application visibility notifications from the UIProcess, so make sure
    // we don't hold a BackgroundApplication freeze reason when transitioning from committed to suspended.
    unfreezeLayerTree(LayerTreeFreezeReason::BackgroundApplication);

    WebProcess::singleton().sendPrewarmInformation(mainWebFrame().url());

    suspendForProcessSwap();
}

void WebPage::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebFrame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    if (hasPageLevelStorageAccess(topFrameDomain, subFrameDomain)) {
        completionHandler(true);
        return;
    }

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::HasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frame.frameID(), m_identifier), WTFMove(completionHandler));
}

void WebPage::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebFrame& frame, StorageAccessScope scope, CompletionHandler<void(WebCore::RequestStorageAccessResult)>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::RequestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frame.frameID(), m_identifier, m_webPageProxyIdentifier, scope), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), frame = Ref { frame }, pageID = m_identifier, frameID = frame.frameID()](RequestStorageAccessResult result) mutable {
        if (result.wasGranted == StorageAccessWasGranted::Yes) {
            switch (result.scope) {
            case StorageAccessScope::PerFrame:
                frame->localFrameLoaderClient()->setHasFrameSpecificStorageAccess({ frameID, pageID });
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
    auto lastAuthentication = page->lastAuthentication();
    WebProcess::singleton().ensureNetworkProcessConnection().protectedConnection()->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::SetLoginStatus(WTFMove(domain), loggedInStatus, lastAuthentication), WTFMove(completionHandler));
}

void WebPage::isLoggedIn(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().protectedConnection()->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::IsLoggedIn(WTFMove(domain)), WTFMove(completionHandler));
}

void WebPage::addDomainWithPageLevelStorageAccess(const RegistrableDomain& topLevelDomain, const RegistrableDomain& resourceDomain)
{
    m_domainsWithPageLevelStorageAccess.add(topLevelDomain, HashSet<RegistrableDomain> { }).iterator->value.add(resourceDomain);

    // Some sites have quirks where multiple login domains require storage access.
    if (auto additionalLoginDomain = NetworkStorageSession::findAdditionalLoginDomain(topLevelDomain, resourceDomain))
        m_domainsWithPageLevelStorageAccess.add(topLevelDomain, HashSet<RegistrableDomain> { }).iterator->value.add(*additionalLoginDomain);
}

bool WebPage::hasPageLevelStorageAccess(const RegistrableDomain& topLevelDomain, const RegistrableDomain& resourceDomain) const
{
    auto it = m_domainsWithPageLevelStorageAccess.find(topLevelDomain);
    return it != m_domainsWithPageLevelStorageAccess.end() && it->value.contains(resourceDomain);
}

void WebPage::clearPageLevelStorageAccess()
{
    m_domainsWithPageLevelStorageAccess.clear();
}

void WebPage::wasLoadedWithDataTransferFromPrevalentResource()
{
    auto* frame = dynamicDowncast<LocalFrame>(mainFrame());
    if (!frame || !frame->document())
        return;

    frame->document()->wasLoadedWithDataTransferFromPrevalentResource();
}

void WebPage::didLoadFromRegistrableDomain(RegistrableDomain&& targetDomain)
{
    if (targetDomain != RegistrableDomain(mainWebFrame().url()))
        m_loadedSubresourceDomains.add(targetDomain);
}

void WebPage::getLoadedSubresourceDomains(CompletionHandler<void(Vector<RegistrableDomain>)>&& completionHandler)
{
    completionHandler(copyToVector(m_loadedSubresourceDomains));
}

void WebPage::clearLoadedSubresourceDomains()
{
    m_loadedSubresourceDomains.clear();
}

#if ENABLE(DEVICE_ORIENTATION)
void WebPage::shouldAllowDeviceOrientationAndMotionAccess(FrameIdentifier frameID, FrameInfoData&& frameInfo, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShouldAllowDeviceOrientationAndMotionAccess(frameID, WTFMove(frameInfo), mayPrompt), WTFMove(completionHandler));
}
#endif
    
void WebPage::showShareSheet(ShareDataWithParsedURL& shareData, WTF::CompletionHandler<void(bool)>&& callback)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShowShareSheet(WTFMove(shareData)), WTFMove(callback));
}

void WebPage::showContactPicker(const WebCore::ContactsRequestData& requestData, CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShowContactPicker(requestData), WTFMove(callback));
}

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
    auto* frame = dynamicDowncast<LocalFrame>(mainFrame());
    if (!frame || !frame->document())
        return;

    frame->document()->simulateDeviceOrientationChange(alpha, beta, gamma);
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
    if (auto observer = corePage()->speechSynthesisClient()->observer())
        observer->speakingErrorOccurred();
}

void WebPage::boundaryEventOccurred(bool wordBoundary, unsigned charIndex, unsigned charLength)
{
    if (auto observer = corePage()->speechSynthesisClient()->observer())
        observer->boundaryEventOccurred(wordBoundary, charIndex, charLength);
}

void WebPage::voicesDidChange()
{
    if (auto observer = corePage()->speechSynthesisClient()->observer())
        observer->voicesChanged();
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPage::insertAttachment(const String& identifier, std::optional<uint64_t>&& fileSize, const String& fileName, const String& contentType, CompletionHandler<void()>&& callback)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return callback();

    frame->editor().insertAttachment(identifier, WTFMove(fileSize), AtomString { fileName }, AtomString { contentType });
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

void WebPage::updateAttachmentThumbnail(const String& identifier, std::optional<ShareableBitmap::Handle>&& qlThumbnailHandle)
{
    if (RefPtr attachment = attachmentElementWithIdentifier(identifier)) {
        if (RefPtr thumbnail = qlThumbnailHandle ? ShareableBitmap::create(WTFMove(*qlThumbnailHandle)) : nullptr) {
            if (attachment->isWideLayout()) {
                if (auto imageBuffer = ImageBuffer::create(thumbnail->size(), RenderingPurpose::Unspecified, 1.0, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8)) {
                    thumbnail->paint(imageBuffer->context(), IntPoint::zero(), IntRect(IntPoint::zero(), thumbnail->size()));
                    auto data = imageBuffer->toData("image/png"_s);
                    attachment->updateThumbnailForWideLayout(WTFMove(data));
                }
            } else
                attachment->updateThumbnailForNarrowLayout(thumbnail->createImage());
        }
    }
}

void WebPage::updateAttachmentIcon(const String& identifier, std::optional<ShareableBitmap::Handle>&& iconHandle, const WebCore::FloatSize& size)
{
    if (RefPtr attachment = attachmentElementWithIdentifier(identifier)) {
        if (auto icon = iconHandle ? ShareableBitmap::create(WTFMove(*iconHandle)) : nullptr) {
            if (attachment->isWideLayout()) {
                if (auto imageBuffer = ImageBuffer::create(icon->size(), RenderingPurpose::Unspecified, 1.0, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8)) {
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
        if (File* file = attachment->file())
            fileName = file->path();
        send(Messages::WebPageProxy::RequestAttachmentIcon(identifier, attachment->attachmentType(), fileName, attachment->attachmentTitle(), size));
    }
}

RefPtr<HTMLAttachmentElement> WebPage::attachmentElementWithIdentifier(const String& identifier) const
{
    // FIXME: Handle attachment elements in subframes too as well.
    RefPtr frame = dynamicDowncast<LocalFrame>(mainFrame());
    if (!frame)
        return nullptr;

    RefPtr document = frame->document();
    if (!document)
        return nullptr;

    return document->attachmentForIdentifier(identifier);
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

    RefPtr element = elementContext.elementIdentifier ? Element::fromIdentifier(*elementContext.elementIdentifier) : nullptr;
    if (!element)
        return nullptr;

    if (!element->isConnected() || element->document().identifier() != elementContext.documentIdentifier || element->document().page() != m_page.get())
        return nullptr;

    return element;
}

std::optional<WebCore::ElementContext> WebPage::contextForElement(WebCore::Element& element) const
{
    Ref document = element.document();
    if (!m_page || document->page() != m_page.get())
        return std::nullopt;

    RefPtr frame = document->frame();
    if (!frame)
        return std::nullopt;

    return WebCore::ElementContext { element.boundingBoxInRootViewCoordinates(), m_identifier, document->identifier(), element.identifier() };
}

void WebPage::startTextManipulations(Vector<WebCore::TextManipulationController::ExclusionRule>&& exclusionRules, bool includeSubframes, CompletionHandler<void()>&& completionHandler)
{
    if (!m_page)
        return completionHandler();

    m_textManipulationExclusionRules = WTFMove(exclusionRules);
    m_textManipulationIncludesSubframes = includeSubframes;
    if (m_textManipulationIncludesSubframes) {
        for (RefPtr<Frame> frame = m_mainFrame->coreLocalFrame(); frame; frame = frame->tree().traverseNextRendered())
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

    auto exclusionRules = *m_textManipulationExclusionRules;
    document->textManipulationController().startObservingParagraphs([webPage = WeakPtr { *this }] (Document& document, const Vector<WebCore::TextManipulationItem>& items) {
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
    CompletionHandler<void(bool allFailed, const Vector<WebCore::TextManipulationController::ManipulationFailure>&)>&& completionHandler)
{
    if (!m_page) {
        completionHandler(true, { });
        return;
    }

    if (items.isEmpty()) {
        completionHandler(true, { });
        return;
    }

    auto currentFrameID = items[0].frameID;

    auto completeManipulationForItems = [&](const Vector<WebCore::TextManipulationItem>& items) -> std::optional<Vector<TextManipulationControllerManipulationFailure>> {
        ASSERT(!items.isEmpty());
        RefPtr frame = WebProcess::singleton().webFrame(currentFrameID);
        if (!frame)
            return std::nullopt;

        RefPtr coreFrame = frame->coreLocalFrame();
        if (!coreFrame)
            return std::nullopt;

        auto* controller = coreFrame->document()->textManipulationControllerIfExists();
        if (!controller)
            return std::nullopt;

        return controller->completeManipulation(items);
    };

    bool containsItemsForMultipleFrames = WTF::anyOf(items, [&](auto& item) {
        return currentFrameID != item.frameID;
    });
    if (!containsItemsForMultipleFrames) {
        auto failures = completeManipulationForItems(items);
        if (failures)
            completionHandler(false, *failures);
        else
            completionHandler(true, { });
        return;
    }

    Vector<WebCore::TextManipulationController::ManipulationFailure> failuresForAllItems;

    auto completeManipulationForCurrentFrame = [&](uint64_t startIndexForCurrentFrame, Vector<WebCore::TextManipulationItem> itemsForCurrentFrame) {
        auto failures = completeManipulationForItems(std::exchange(itemsForCurrentFrame, { }));
        if (!failures) {
            uint64_t index = startIndexForCurrentFrame;
            for (auto& item : itemsForCurrentFrame) {
                failuresForAllItems.append(TextManipulationControllerManipulationFailure {
                    *item.frameID, item.identifier, index, TextManipulationControllerManipulationFailure::Type::NotAvailable });
                ++index;
            }
            return;
        }
        for (auto& failure : *failures) {
            failuresForAllItems.append(WTFMove(failure));
            failuresForAllItems.last().index += startIndexForCurrentFrame;
        }
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

    completionHandler(false, WTFMove(failuresForAllItems));
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

#if ENABLE(TEXT_AUTOSIZING)
void WebPage::textAutoSizingAdjustmentTimerFired()
{
    m_page->recomputeTextAutoSizingInAllFrames();
}

void WebPage::textAutosizingUsesIdempotentModeChanged()
{
    if (!m_page->settings().textAutosizingUsesIdempotentMode())
        m_textAutoSizingAdjustmentTimer.stop();
}
#endif // ENABLE(TEXT_AUTOSIZING)

#if ENABLE(WEBXR) && !USE(OPENXR)
PlatformXRSystemProxy& WebPage::xrSystemProxy()
{
    if (!m_xrSystemProxy)
        m_xrSystemProxy = makeUniqueWithoutRefCountedCheck<PlatformXRSystemProxy>(*this);
    return *m_xrSystemProxy;
}
#endif

void WebPage::setOverriddenMediaType(const String& mediaType)
{
    if (mediaType == m_overriddenMediaType)
        return;

    m_overriddenMediaType = AtomString(mediaType);
    m_page->updateStyleAfterChangeInEnvironment();
}

void WebPage::updateCORSDisablingPatterns(Vector<String>&& patterns)
{
    if (!m_page)
        return;

    m_corsDisablingPatterns = WTFMove(patterns);
    synchronizeCORSDisablingPatternsWithNetworkProcess();
    m_page->setCORSDisablingPatterns(parseAndAllowAccessToCORSDisablingPatterns(m_corsDisablingPatterns));
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

    if (corePage()->hasCachedTextRecognitionResult(*htmlElement)) {
        if (completion) {
            RefPtr<Element> imageOverlayHost;
            if (ImageOverlay::hasOverlay(*htmlElement))
                imageOverlayHost = &element;
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
    auto imageURL = cachedImage ? element.document().completeURL(cachedImage->url().string()) : URL { };
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

        RefPtr protectedElement { weakElement.get() };
        if (!protectedElement)
            return;

        auto& htmlElement = downcast<HTMLElement>(*protectedElement);
        ImageOverlay::updateWithTextRecognitionResult(htmlElement, result);

        auto matchIndex = protectedPage->m_elementsPendingTextRecognition.findIf([&] (auto& elementAndCompletionHandlers) {
            return elementAndCompletionHandlers.first == &htmlElement;
        });

        if (matchIndex == notFound)
            return;

        RefPtr imageOverlayHost = ImageOverlay::hasOverlay(htmlElement) ? &htmlElement : nullptr;
        for (auto& completionHandler : protectedPage->m_elementsPendingTextRecognition[matchIndex].second)
            completionHandler(imageOverlayHost.copyRef());

        protectedPage->m_elementsPendingTextRecognition.remove(matchIndex);
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
    if (RefPtr document = m_mainFrame->coreLocalFrame()->document())
        corePage()->protectedImageAnalysisQueue()->enqueueAllImagesIfNeeded(*document, sourceLanguageIdentifier, targetLanguageIdentifier);
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
        if (auto* image = cachedImage->image())
            mimeType = image->mimeType();
    }
    ASSERT(!mimeType.isEmpty());
    completion(WTFMove(*handle), mimeType);
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
void WebPage::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::ShowMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items)), completionHandler);
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

#if ENABLE(APP_HIGHLIGHTS)
bool WebPage::createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight createNewGroup, WebCore::HighlightRequestOriginatedInApp requestOriginatedInApp, CompletionHandler<void(WebCore::AppHighlight&&)>&& completionHandler)
{
    SetForScope highlightIsNewGroupScope { m_highlightIsNewGroup, createNewGroup };
    SetForScope highlightRequestOriginScope { m_highlightRequestOriginatedInApp, requestOriginatedInApp };

    RefPtr focusedOrMainFrame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return false;
    RefPtr document = focusedOrMainFrame->document();

    RefPtr frame = document->frame();
    if (!frame)
        return false;

    auto selectionRange = frame->selection().selection().toNormalizedRange();
    if (!selectionRange)
        return false;

    document->appHighlightRegistry().addAnnotationHighlightWithRange(StaticRange::create(selectionRange.value()));
    document->appHighlightStorage().storeAppHighlight(StaticRange::create(selectionRange.value()), [completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }, this] (WebCore::AppHighlight&& highlight) mutable {
        highlight.isNewGroup = m_highlightIsNewGroup;
        highlight.requestOriginatedInApp = m_highlightRequestOriginatedInApp;
        completionHandler(WTFMove(highlight));
    });

    return true;
}

void WebPage::restoreAppHighlightsAndScrollToIndex(Vector<SharedMemory::Handle>&& memoryHandles, const std::optional<unsigned> index)
{
    RefPtr focusedOrMainFrame = m_page->checkedFocusController()->focusedOrMainFrame();
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
            document->appHighlightRegistry().setHighlightVisibility(appHighlightVisibility);
    }
}

#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void WebPage::createMediaSessionCoordinator(const String& identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr document = m_mainFrame->coreLocalFrame()->document();
    if (!document || !document->domWindow()) {
        completionHandler(false);
        return;
    }

    m_page->setMediaSessionCoordinator(RemoteMediaSessionCoordinator::create(*this, identifier));
    completionHandler(true);
}
#endif

void WebPage::lastNavigationWasAppInitiated(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr mainFrame = dynamicDowncast<LocalFrame>(this->mainFrame());
    if (!mainFrame)
        return completionHandler(false);
    return completionHandler(mainFrame->document()->loader()->lastNavigationWasAppInitiated());
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

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)
void WebPage::beginTextRecognitionForVideoInElementFullScreen(const HTMLVideoElement& element)
{
    RefPtr view = element.document().view();
    if (!view)
        return;

    auto mediaPlayerIdentifier = element.playerIdentifier();
    if (!mediaPlayerIdentifier)
        return;

    auto* renderer = element.renderer();
    if (!renderer)
        return;

    auto rectInRootView = view->contentsToRootView(renderer->videoBox());
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
#if !ENABLE(LEGACY_PDFKIT_PLUGIN)
    return false;
#else
    return mainFramePlugIn();
#endif
}

#if PLATFORM(COCOA)
void WebPage::insertTextPlaceholder(const IntSize& size, CompletionHandler<void(const std::optional<WebCore::ElementContext>&)>&& completionHandler)
{
    // Inserting the placeholder may run JavaScript, which can do anything, including frame destruction.
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

    auto placeholder = frame->editor().insertTextPlaceholder(size);
    completionHandler(placeholder ? contextForElement(*placeholder) : std::nullopt);
}

void WebPage::removeTextPlaceholder(const ElementContext& placeholder, CompletionHandler<void()>&& completionHandler)
{
    if (auto element = elementForContext(placeholder)) {
        if (RefPtr frame = element->document().frame())
            frame->editor().removeTextPlaceholder(downcast<TextPlaceholderElement>(*element));
    }
    completionHandler();
}
#endif

void WebPage::generateTestReport(String&& message, String&& group)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (RefPtr document = localMainFrame ? localMainFrame->document() : nullptr)
        document->reportingScope().generateTestReport(WTFMove(message), WTFMove(group));
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebPage::updateImageAnimationEnabled()
{
    corePage()->setImageAnimationEnabled(WebProcess::singleton().imageAnimationEnabled());
}

void WebPage::pauseAllAnimations(CompletionHandler<void()>&& completionHandler)
{
    corePage()->setImageAnimationEnabled(false);
    completionHandler();
}

void WebPage::playAllAnimations(CompletionHandler<void()>&& completionHandler)
{
    corePage()->setImageAnimationEnabled(true);
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
#if PLATFORM(COCOA)
    return m_drawingAreaType == DrawingAreaType::RemoteLayerTree;
#else
    return false;
#endif
}

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

void WebPage::setLinkDecorationFilteringData(Vector<WebCore::LinkDecorationFilteringData>&& strings)
{
    m_linkDecorationFilteringData.clear();

    for (auto& data : strings) {
        if (!m_linkDecorationFilteringData.isValidKey(data.linkDecoration)) {
            WEBPAGE_RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to set link decoration filtering data (invalid key)");
            ASSERT_NOT_REACHED();
            continue;
        }

        auto it = m_linkDecorationFilteringData.ensure(data.linkDecoration, [] {
            return LinkDecorationFilteringConditionals { };
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
    m_allowedQueryParametersForAdvancedPrivacyProtections.clear();
    for (auto& data : allowStrings) {
        if (!m_allowedQueryParametersForAdvancedPrivacyProtections.isValidKey(data.domain))
            continue;

        m_allowedQueryParametersForAdvancedPrivacyProtections.ensure(data.domain, [&] {
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

    if (auto components = response.httpHeaderField(HTTPHeaderName::ContentDisposition).split(';'); !components.isEmpty() && equalIgnoringASCIICase(components[0].trim(isASCIIWhitespaceWithoutFF<UChar>), "attachment"_s))
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
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
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

    if (auto* resourceLoader = loader->mainResourceLoader()) {
        WEBPAGE_RELEASE_LOG(Loading, "WebPage::useRedirectionForCurrentNavigation to network process");
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UseRedirectionForCurrentNavigation(*resourceLoader->identifier(), response), 0);
        return;
    }

    WEBPAGE_RELEASE_LOG(Loading, "WebPage::useRedirectionForCurrentNavigation as substiute data");
    loader->setRedirectionAsSubstituteData(WTFMove(response));
}

void WebPage::dispatchLoadEventToFrameOwnerElement(WebCore::FrameIdentifier frameID)
{
    auto* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;

    auto* coreRemoteFrame = frame->coreRemoteFrame();
    if (!coreRemoteFrame)
        return;

    if (coreRemoteFrame->ownerElement())
        coreRemoteFrame->ownerElement()->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void WebPage::frameWasFocusedInAnotherProcess(WebCore::FrameIdentifier frameID)
{
    auto* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame)
        return;
    m_page->focusController().setFocusedFrame(frame->coreFrame(), FocusController::BroadcastFocusedFrame::No);
}

void WebPage::remotePostMessage(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData>&& targetOrigin, const WebCore::MessageWithMessagePorts& message)
{
    RefPtr targetFrame = WebProcess::singleton().webFrame(target);
    if (!targetFrame)
        return;

    if (!targetFrame->coreLocalFrame())
        return;

    RefPtr targetWindow = targetFrame->coreLocalFrame()->window();
    if (!targetWindow)
        return;

    RefPtr targetCoreFrame = targetWindow->frame();
    if (!targetCoreFrame)
        return;

    RefPtr sourceFrame = WebProcess::singleton().webFrame(source);
    RefPtr sourceWindow = sourceFrame && sourceFrame->coreFrame() ? &sourceFrame->coreFrame()->windowProxy() : nullptr;

    auto& script = targetCoreFrame->script();
    auto globalObject = script.globalObject(WebCore::mainThreadNormalWorld());
    if (!globalObject)
        return;

    targetWindow->postMessageFromRemoteFrame(*globalObject, WTFMove(sourceWindow), sourceOrigin, WTFMove(targetOrigin), message);
}

void WebPage::renderTreeAsTextForTesting(WebCore::FrameIdentifier frameID, size_t baseIndent, OptionSet<WebCore::RenderAsTextFlag> behavior, CompletionHandler<void(String&&)>&& completionHandler)
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

    auto* renderer = coreLocalFrame->contentRenderer();
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing RenderView in web process"_s);
    }

    auto ts = WebCore::createTextStream(*renderer);
    ts.setIndent(baseIndent);
    WebCore::externalRepresentationForLocalFrame(ts, *coreLocalFrame, behavior);
    completionHandler(ts.release());
}

void WebPage::layerTreeAsTextForTesting(WebCore::FrameIdentifier frameID, size_t baseIndent, OptionSet<WebCore::LayerTreeAsTextOptions> options, CompletionHandler<void(String&&)>&& completionHandler)
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

    auto* renderer = coreLocalFrame->contentRenderer();
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return completionHandler("Test Error - WebFrame missing RenderView in web process"_s);
    }

    auto ts = WebCore::createTextStream(*renderer);
    ts << coreLocalFrame->contentRenderer()->compositor().layerTreeAsText(options, baseIndent);
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

void WebPage::requestTextExtraction(std::optional<FloatRect>&& collectionRectInRootView, CompletionHandler<void(TextExtraction::Item&&)>&& completion)
{
    completion(TextExtraction::extractItem(WTFMove(collectionRectInRootView), Ref { *corePage() }));
}

template<typename T> T WebPage::remoteViewToRootView(WebCore::FrameIdentifier frameID, T geometry)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return geometry;

    RefPtr coreRemoteFrame = webFrame->coreRemoteFrame();
    if (!coreRemoteFrame) {
        ASSERT_NOT_REACHED();
        return geometry;
    }

    RefPtr view = coreRemoteFrame->view();
    if (!view)
        return geometry;

    return view->contentsToRootView(geometry);
}

void WebPage::remoteViewRectToRootView(FrameIdentifier frameID, FloatRect rect, CompletionHandler<void(FloatRect)>&& completionHandler)
{
    completionHandler(remoteViewToRootView(frameID, rect));
}

void WebPage::remoteViewPointToRootView(FrameIdentifier frameID, FloatPoint point, CompletionHandler<void(FloatPoint)>&& completionHandler)
{
    completionHandler(remoteViewToRootView(frameID, point));
}

void WebPage::remoteDictionaryPopupInfoToRootView(WebCore::FrameIdentifier frameID, WebCore::DictionaryPopupInfo popupInfo, CompletionHandler<void(WebCore::DictionaryPopupInfo)>&& completionHandler)
{
    popupInfo.origin = remoteViewToRootView<FloatPoint>(frameID, popupInfo.origin);
#if PLATFORM(COCOA)
    popupInfo.textIndicator.selectionRectInRootViewCoordinates = remoteViewToRootView<FloatRect>(frameID, popupInfo.textIndicator.selectionRectInRootViewCoordinates);
    popupInfo.textIndicator.textBoundingRectInRootViewCoordinates = remoteViewToRootView<FloatRect>(frameID, popupInfo.textIndicator.textBoundingRectInRootViewCoordinates);
    popupInfo.textIndicator.contentImageWithoutSelectionRectInRootViewCoordinates = remoteViewToRootView<FloatRect>(frameID, popupInfo.textIndicator.contentImageWithoutSelectionRectInRootViewCoordinates);

    for (auto& textRect : popupInfo.textIndicator.textRectsInBoundingRectCoordinates)
        textRect = remoteViewToRootView<FloatRect>(frameID, textRect);
#endif
    completionHandler(popupInfo);
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

void WebPage::takeSnapshotForTargetedElement(ElementIdentifier elementID, ScriptExecutionContextIdentifier documentID, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&& completion)
{
    RefPtr page = corePage();
    if (!page)
        return completion({ });

    RefPtr image = page->checkedElementTargetingController()->snapshotIgnoringVisibilityAdjustment(elementID, documentID);
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
    if (m_nowPlayingMetadataObserver)
        return;

    m_nowPlayingMetadataObserver = makeUnique<NowPlayingMetadataObserver>([weakThis = WeakPtr { *this }](auto& metadata) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->send(Messages::WebPageProxy::NowPlayingMetadataChanged { metadata });
    });

    WebCore::PlatformMediaSessionManager::singleton().addNowPlayingMetadataObserver(*m_nowPlayingMetadataObserver);
#endif
}

void WebPage::stopObservingNowPlayingMetadata()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    auto nowPlayingMetadataObserver = std::exchange(m_nowPlayingMetadataObserver, nullptr);
    if (!nowPlayingMetadataObserver)
        return;

    WebCore::PlatformMediaSessionManager::singleton().removeNowPlayingMetadataObserver(*nowPlayingMetadataObserver);
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

    if (RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame())
        m_lastNodeBeforeWritingSuggestions = frame->editor().nodeBeforeWritingSuggestions();
}

void WebPage::didAddOrRemoveViewportConstrainedObjects()
{
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
            return !innerNode || !target->containsIncludingShadowDOM(innerNode.get());
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
        return { locationInWindow, locationInWindow, MouseButton::Left, type, 1, { }, WallTime::now(), ForceAtClick, SyntheticClickType::OneFingerTap, mousePointerID };
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

    sendWithAsyncReply(Messages::WebPageProxy::ValidateCaptureStateUpdate(UserMediaRequestIdentifier::generate(), document.clientOrigin(), webFrame->frameID(), isActive, kind), [weakThis = WeakPtr { *this }, isActive, kind, completionHandler = WTFMove(completionHandler)] (auto&& error) mutable {
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

bool WebPage::isAlwaysOnLoggingAllowed() const
{
    RefPtr page { protectedCorePage() };
    return page && page->isAlwaysOnLoggingAllowed();
}

#if !PLATFORM(IOS_FAMILY)

void WebPage::callAfterPendingSyntheticClick(CompletionHandler<void(SyntheticClickResult)>&& completion)
{
    completion(SyntheticClickResult::Failed);
}

#endif

} // namespace WebKit

#undef WEBPAGE_RELEASE_LOG
#undef WEBPAGE_RELEASE_LOG_ERROR
