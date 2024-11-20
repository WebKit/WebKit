/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Internals.h"

#include "AXObjectCache.h"
#include "AddEventListenerOptions.h"
#include "AnimationTimeline.h"
#include "AnimationTimelinesController.h"
#include "ApplicationCacheStorage.h"
#include "AudioSession.h"
#include "AudioTrackPrivateMediaStream.h"
#include "Autofill.h"
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "BitmapImage.h"
#include "Blob.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CacheStorageConnection.h"
#include "CacheStorageProvider.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CanvasBase.h"
#include "CertificateInfo.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientOrigin.h"
#include "ColorSerialization.h"
#include "ComposedTreeIterator.h"
#include "CookieJar.h"
#include "CrossOriginPreflightResultCache.h"
#include "Cursor.h"
#include "DOMPointReadOnly.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DOMStringList.h"
#include "DOMURL.h"
#include "DeprecatedGlobalSettings.h"
#include "DiagnosticLoggingClient.h"
#include "DisabledAdaptations.h"
#include "DisplayList.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentTimeline.h"
#include "Editor.h"
#include "Element.h"
#include "ElementRareData.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "ExtendableEvent.h"
#include "ExtensionStyleSheets.h"
#include "FetchRequest.h"
#include "FetchResponse.h"
#include "File.h"
#include "FloatQuad.h"
#include "FontCache.h"
#include "FormController.h"
#include "FragmentDirectiveGenerator.h"
#include "FrameLoader.h"
#include "FullscreenManager.h"
#include "GCObservation.h"
#include "GridPosition.h"
#include "HEVCUtilities.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLPictureElement.h"
#include "HTMLPlugInElement.h"
#include "HTMLPreloadScanner.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "HighlightRegistry.h"
#include "History.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "ImageOverlay.h"
#include "ImageOverlayController.h"
#include "InlineIteratorLineBox.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorDebuggableType.h"
#include "InspectorFrontendClientLocal.h"
#include "InspectorOverlay.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "InternalSettings.h"
#include "InternalsMapLike.h"
#include "InternalsSetLike.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFile.h"
#include "JSImageData.h"
#include "JSInternals.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "Location.h"
#include "MallocStatistics.h"
#include "MediaDevices.h"
#include "MediaEngineConfigurationFactory.h"
#include "MediaKeySession.h"
#include "MediaKeys.h"
#include "MediaMetadata.h"
#include "MediaPlayer.h"
#include "MediaProducer.h"
#include "MediaResourceLoader.h"
#include "MediaSession.h"
#include "MediaSessionActionDetails.h"
#include "MediaStrategy.h"
#include "MediaStreamTrack.h"
#include "MediaUsageInfo.h"
#include "MemoryCache.h"
#include "MemoryInfo.h"
#include "MessagePort.h"
#include "MockAudioDestinationCocoa.h"
#include "MockLibWebRTCPeerConnection.h"
#include "MockPageOverlay.h"
#include "MockPageOverlayClient.h"
#include "NavigatorBeacon.h"
#include "NavigatorMediaDevices.h"
#include "NetworkLoadInformation.h"
#include "Page.h"
#include "PageOverlay.h"
#include "PathUtilities.h"
#include "PictureInPictureSupport.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMediaSession.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "PluginViewBase.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "PushSubscription.h"
#include "PushSubscriptionData.h"
#include "Quirks.h"
#include "RTCNetworkManager.h"
#include "RTCRtpSFrameTransform.h"
#include "Range.h"
#include "ReadableStream.h"
#include "RenderEmbeddedObject.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "RenderSearchField.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "ResolvedStyle.h"
#include "ResourceLoadObserver.h"
#include "SMILTimeContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGPathStringBuilder.h"
#include "SVGSVGElement.h"
#include "SWClientConnection.h"
#include "ScriptController.h"
#include "ScriptedAnimationController.h"
#include "ScrollbarsControllerMock.h"
#include "ScrollingCoordinator.h"
#include "ScrollingMomentumCalculator.h"
#include "SecurityOrigin.h"
#include "SelectorFilter.h"
#include "SerializedScriptValue.h"
#include "ServiceWorker.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerRegistrationData.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceBuffer.h"
#include "SpeechSynthesisUtterance.h"
#include "SpellChecker.h"
#include "StaticNodeList.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StringCallback.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "SystemSoundManager.h"
#include "TextIterator.h"
#include "TextPainter.h"
#include "TextPlaceholderElement.h"
#include "TextRecognitionOptions.h"
#include "ThreadableBlobRegistry.h"
#include "TreeScope.h"
#include "TypeConversions.h"
#include "UserContentURLPattern.h"
#include "UserGestureIndicator.h"
#include "UserMediaController.h"
#include "ViewportArguments.h"
#include "VoidCallback.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include "WebCodecsVideoDecoder.h"
#include "WebCoreJSClientData.h"
#include "WebRTCProvider.h"
#include "WindowProxy.h"
#include "WorkerThread.h"
#include "WorkletGlobalScope.h"
#include "WritingDirection.h"
#include "XMLHttpRequest.h"
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/FileSystem.h>
#include <wtf/HexNumber.h>
#include <wtf/JSONValues.h>
#include <wtf/Language.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NativePromise.h>
#include <wtf/ProcessID.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URLHelpers.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(CG)
#include "PDFDocumentImage.h"
#endif

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(MOUSE_CURSOR_SCALE)
#include <wtf/dtoa.h>
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "LegacyCDM.h"
#include "LegacyMockCDM.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "MockCDMFactory.h"
#endif

#if ENABLE(VIDEO)
#include "CaptionUserPreferences.h"
#include "HTMLMediaElement.h"
#include "PageGroup.h"
#include "TextTrack.h"
#include "TextTrackCueGeneric.h"
#include "TimeRanges.h"
#include "VTTCue.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLRenderingContext.h"
#endif

#if ENABLE(SPEECH_SYNTHESIS)
#include "LocalDOMWindowSpeechSynthesis.h"
#include "PlatformSpeechSynthesizerMock.h"
#include "SpeechSynthesis.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#include "MockRealtimeMediaSourceCenter.h"
#include "VideoFrame.h"
#endif

#if ENABLE(MEDIA_RECORDER)
#include "MediaRecorder.h"
#include "MediaRecorderPrivateMock.h"
#endif

#if ENABLE(WEB_RTC)
#include "RTCPeerConnection.h"
#endif

#if USE(LIBWEBRTC)
#include "LibWebRTCProvider.h"
#endif

#if ENABLE(CONTENT_FILTERING)
#include "MockContentFilterSettings.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioContext.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContext.h"
#endif

#if ENABLE(POINTER_LOCK)
#include "PointerLockController.h"
#endif

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoader.h"
#include "MockPreviewLoaderClient.h"
#endif

#if ENABLE(APPLE_PAY)
#include "MockPaymentCoordinator.h"
#include "PaymentCoordinator.h"
#endif

#if ENABLE(WEBXR)
#include "NavigatorWebXR.h"
#include "WebXRSystem.h"
#include "WebXRTest.h"
#endif

#if PLATFORM(MAC)
#include "GraphicsChecksMac.h"
#include "ScrollbarsControllerMac.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "MediaSessionHelperIOS.h"
#include "RenderThemeIOS.h"
#endif

#if PLATFORM(COCOA)
#include "FontCacheCoreText.h"
#include "SystemBattery.h"
#include "VP9UtilitiesCocoa.h"
#include <pal/spi/cf/CoreTextSPI.h>
#include <wtf/spi/darwin/SandboxSPI.h>
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#include "MediaSessionCoordinator.h"
#include "MockMediaSessionCoordinator.h"
#include "NavigatorMediaSession.h"
#endif

#if ENABLE(MEDIA_SESSION) && USE(GLIB)
#include "MediaSessionManagerGLib.h"
#endif

#if ENABLE(IMAGE_ANALYSIS)
#include "TextRecognitionResult.h"
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#include "HTMLModelElement.h"
#endif

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

using JSC::CallData;
using JSC::CodeBlock;
using JSC::FunctionExecutable;
using JSC::Identifier;
using JSC::JSFunction;
using JSC::JSGlobalObject;
using JSC::JSObject;
using JSC::JSValue;
using JSC::MarkedArgumentBuffer;
using JSC::PropertySlot;
using JSC::ScriptExecutable;
using JSC::StackVisitor;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Internals);

using namespace Inspector;
using namespace HTMLNames;

class InspectorStubFrontend final : public InspectorFrontendClientLocal, public FrontendChannel {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InspectorStubFrontend);
public:
    InspectorStubFrontend(Page& inspectedPage, RefPtr<LocalDOMWindow>&& frontendWindow);
    virtual ~InspectorStubFrontend();

private:
    bool supportsDockSide(DockSide) final { return false; }
    void attachWindow(DockSide) final { }
    void detachWindow() final { }
    void closeWindow() final;
    void reopen() final { }
    void bringToFront() final { }
    void setForcedAppearance(InspectorFrontendClient::Appearance) final { }
    String localizedStringsURL() const final { return String(); }
    DebuggableType debuggableType() const final { return DebuggableType::Page; }
    String targetPlatformName() const { return "Unknown"_s; }
    String targetBuildVersion() const { return "Unknown"_s; }
    String targetProductVersion() const { return "Unknown"_s; }
    bool targetIsSimulator() const { return false; }
    void inspectedURLChanged(const String&) final { }
    void showCertificate(const CertificateInfo&) final { }
    void setAttachedWindowHeight(unsigned) final { }
    void setAttachedWindowWidth(unsigned) final { }
    void setSheetRect(const FloatRect&) final { }

    void sendMessageToFrontend(const String& message) final;
    ConnectionType connectionType() const final { return ConnectionType::Local; }

    RefPtr<LocalDOMWindow> m_frontendWindow;
};

InspectorStubFrontend::InspectorStubFrontend(Page& inspectedPage, RefPtr<LocalDOMWindow>&& frontendWindow)
    : InspectorFrontendClientLocal(&inspectedPage.inspectorController(), frontendWindow->document()->page(), makeUnique<InspectorFrontendClientLocal::Settings>())
    , m_frontendWindow(frontendWindow.copyRef())
{
    ASSERT_ARG(frontendWindow, frontendWindow);

    frontendPage()->inspectorController().setInspectorFrontendClient(this);
    inspectedPage.inspectorController().connectFrontend(*this);
}

InspectorStubFrontend::~InspectorStubFrontend()
{
    closeWindow();
}

void InspectorStubFrontend::closeWindow()
{
    if (!m_frontendWindow)
        return;

    frontendPage()->inspectorController().setInspectorFrontendClient(nullptr);
    inspectedPage()->inspectorController().disconnectFrontend(*this);

    m_frontendWindow->close();
    m_frontendWindow = nullptr;
}

void InspectorStubFrontend::sendMessageToFrontend(const String& message)
{
    frontendAPIDispatcher().dispatchMessageAsync(message);
}

static bool markerTypeFrom(const String& markerType, DocumentMarker::Type& result)
{
    if (equalLettersIgnoringASCIICase(markerType, "spelling"_s))
        result = DocumentMarker::Type::Spelling;
    else if (equalLettersIgnoringASCIICase(markerType, "grammar"_s))
        result = DocumentMarker::Type::Grammar;
    else if (equalLettersIgnoringASCIICase(markerType, "textmatch"_s))
        result = DocumentMarker::Type::TextMatch;
    else if (equalLettersIgnoringASCIICase(markerType, "replacement"_s))
        result = DocumentMarker::Type::Replacement;
    else if (equalLettersIgnoringASCIICase(markerType, "correctionindicator"_s))
        result = DocumentMarker::Type::CorrectionIndicator;
    else if (equalLettersIgnoringASCIICase(markerType, "rejectedcorrection"_s))
        result = DocumentMarker::Type::RejectedCorrection;
    else if (equalLettersIgnoringASCIICase(markerType, "autocorrected"_s))
        result = DocumentMarker::Type::Autocorrected;
    else if (equalLettersIgnoringASCIICase(markerType, "spellcheckingexemption"_s))
        result = DocumentMarker::Type::SpellCheckingExemption;
    else if (equalLettersIgnoringASCIICase(markerType, "deletedautocorrection"_s))
        result = DocumentMarker::Type::DeletedAutocorrection;
    else if (equalLettersIgnoringASCIICase(markerType, "dictationalternatives"_s))
        result = DocumentMarker::Type::DictationAlternatives;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    else if (equalLettersIgnoringASCIICase(markerType, "telephonenumber"_s))
        result = DocumentMarker::Type::TelephoneNumber;
#endif
#if ENABLE(WRITING_TOOLS)
    else if (equalLettersIgnoringASCIICase(markerType, "writingtoolstextsuggestion"_s))
        result = DocumentMarker::Type::WritingToolsTextSuggestion;
#endif
    else if (equalLettersIgnoringASCIICase(markerType, "transparentcontent"_s))
        result = DocumentMarker::Type::TransparentContent;
    else
        return false;

    return true;
}

static bool markerTypesFrom(const String& markerType, OptionSet<DocumentMarker::Type>& result)
{
    DocumentMarker::Type singularResult;

    if (markerType.isEmpty() || equalLettersIgnoringASCIICase(markerType, "all"_s))
        result = DocumentMarker::allMarkers();
    else if (markerTypeFrom(markerType, singularResult))
        result = singularResult;
    else
        return false;

    return true;
}

static std::unique_ptr<PrintContext>& printContextForTesting()
{
    static NeverDestroyed<std::unique_ptr<PrintContext>> context;
    return context;
}

Ref<Internals> Internals::create(Document& document)
{
    return adoptRef(*new Internals(document));
}

Internals::~Internals()
{
#if ENABLE(MEDIA_STREAM)
    stopObservingRealtimeMediaSource();
#endif
#if ENABLE(MEDIA_SESSION)
    if (m_artworkImagePromise)
        m_artworkImagePromise->reject(Exception { ExceptionCode::InvalidStateError });
#endif
}

void Internals::resetToConsistentState(Page& page)
{
    page.setPageScaleFactor(1, IntPoint(0, 0));
    page.setPagination(Pagination());

    page.setDefersLoading(false);
    page.setResourceCachingDisabledByWebInspector(false);

    auto* localMainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!localMainFrame)
        return;

    localMainFrame->setTextZoomFactor(1.0f);

    page.setCompositingPolicyOverride(WebCore::CompositingPolicy::Normal);

    auto* mainFrameView = localMainFrame->view();
    if (mainFrameView) {
        page.setHeaderHeight(0);
        page.setFooterHeight(0);
        page.setTopContentInset(0);
        mainFrameView->setUseFixedLayout(false);
        mainFrameView->setFixedLayoutSize(IntSize());
        mainFrameView->enableFixedWidthAutoSizeMode(false, { });

        if (auto* backing = mainFrameView->tiledBacking())
            backing->setTileSizeUpdateDelayDisabledForTesting(false);
    }

    if (RefPtr window = localMainFrame->window())
        window->history().setTotalStateObjectPayloadLimitOverride(std::nullopt);

    WTF::clearDefaultPortForProtocolMapForTesting();
    overrideUserPreferredLanguages(Vector<String>());
    WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(false);
    if (!localMainFrame->editor().isContinuousSpellCheckingEnabled())
        localMainFrame->editor().toggleContinuousSpellChecking();
    if (localMainFrame->editor().isOverwriteModeEnabled())
        localMainFrame->editor().toggleOverwriteModeEnabled();
    localMainFrame->loader().clearTestingOverrides();
    if (auto* applicationCacheStorage = page.applicationCacheStorage())
        applicationCacheStorage->setDefaultOriginQuota(ApplicationCacheStorage::noQuota());
#if ENABLE(VIDEO)
    page.group().ensureCaptionPreferences().setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode::ForcedOnly);
    page.group().ensureCaptionPreferences().setCaptionsStyleSheetOverride(emptyString());
    PlatformMediaSessionManager::singleton().resetHaveEverRegisteredAsNowPlayingApplicationForTesting();
    PlatformMediaSessionManager::singleton().resetRestrictions();
    PlatformMediaSessionManager::singleton().resetSessionState();
    PlatformMediaSessionManager::singleton().setWillIgnoreSystemInterruptions(true);
    PlatformMediaSessionManager::singleton().applicationWillEnterForeground(false);
    if (page.mediaPlaybackIsSuspended())
        page.resumeAllMediaPlayback();
#endif
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::singleton().setIsPlayingToAutomotiveHeadUnit(false);
#endif
    AXObjectCache::setEnhancedUserInterfaceAccessibility(false);
    AXObjectCache::disableAccessibility();

    MockPageOverlayClient::singleton().uninstallAllOverlays();

#if ENABLE(CONTENT_FILTERING)
    MockContentFilterSettings::reset();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    page.setMockMediaPlaybackTargetPickerEnabled(true);
    page.setMockMediaPlaybackTargetPickerState(emptyString(), MediaPlaybackTargetContext::MockState::Unknown);
#endif

#if ENABLE(VIDEO)
    MediaResourceLoader::recordResponsesForTesting();
#endif

    page.setShowAllPlugins(false);
    page.setLowPowerModeEnabledOverrideForTesting(std::nullopt);
    page.setOutsideViewportThrottlingEnabledForTesting(false);

#if USE(QUICK_LOOK)
    MockPreviewLoaderClient::singleton().setPassword(emptyString());
    LegacyPreviewLoader::setClientForTesting(nullptr);
#endif

    printContextForTesting() = nullptr;

#if ENABLE(WEB_RTC)
    auto& rtcProvider = page.webRTCProvider();
#if USE(LIBWEBRTC)
    auto& webRTCProvider = reinterpret_cast<LibWebRTCProvider&>(rtcProvider);
    WebCore::useRealRTCPeerConnectionFactory(webRTCProvider);
    webRTCProvider.disableNonLocalhostConnections();
    webRTCProvider.setVP9HardwareSupportForTesting({ });
#endif
    WebRTCProvider::setH264HardwareEncoderAllowed(true);
    page.settings().setWebRTCEncryptionEnabled(true);
    rtcProvider.setH265Support(true);
    rtcProvider.setVP9Support(true, true);
    rtcProvider.clearFactory();
#if USE(GSTREAMER_WEBRTC)
    page.settings().setPeerConnectionEnabled(true);
#endif
#endif

    page.setFullscreenAutoHideDuration(0_s);
    page.setFullscreenInsets({ });

    MediaEngineConfigurationFactory::disableMock();

#if ENABLE(MEDIA_STREAM)
    page.settings().setInterruptAudioOnPageVisibilityChangeEnabled(false);
#endif

#if ENABLE(MEDIA_RECORDER)
    WebCore::MediaRecorder::setCustomPrivateRecorderCreator(nullptr);
#endif

    CanvasBase::setMaxCanvasAreaForTesting(std::nullopt);
    LocalDOMWindow::overrideTransientActivationDurationForTesting(std::nullopt);

#if PLATFORM(IOS) || PLATFORM(VISION)
    WebCore::setContentSizeCategory(kCTFontContentSizeCategoryL);
#endif

#if ENABLE(MEDIA_SESSION) && USE(GLIB)
    auto& sessionManager = reinterpret_cast<MediaSessionManagerGLib&>(PlatformMediaSessionManager::singleton());
    sessionManager.setDBusNotificationsEnabled(false);
#endif

#if PLATFORM(COCOA)
    setOverrideEnhanceTextLegibility(false);
#endif

    TextPainter::setForceUseGlyphDisplayListForTesting(false);

#if USE(AUDIO_SESSION)
    AudioSession::sharedSession().setCategoryOverride(AudioSessionCategory::None);
    AudioSession::sharedSession().tryToSetActive(false);
    AudioSession::sharedSession().endInterruptionForTesting();
#endif
}

Internals::Internals(Document& document)
    : ContextDestructionObserver(&document)
#if ENABLE(MEDIA_STREAM)
    , m_orientationNotifier(0)
#endif
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (document.page())
        document.page()->setMockMediaPlaybackTargetPickerEnabled(true);
#endif

#if ENABLE(VIDEO)
    if (document.page())
        m_testingModeToken = document.page()->group().ensureCaptionPreferences().createTestingModeToken().moveToUniquePtr();
#endif

    if (contextDocument() && contextDocument()->frame()) {
        setAutomaticSpellingCorrectionEnabled(true);
        setAutomaticQuoteSubstitutionEnabled(false);
        setAutomaticDashSubstitutionEnabled(false);
        setAutomaticLinkDetectionEnabled(false);
        setAutomaticTextReplacementEnabled(true);
    }

    setConsoleMessageListener(nullptr);

#if ENABLE(APPLE_PAY)
    auto* frame = document.frame();
    if (frame && frame->page() && frame->isMainFrame()) {
        auto mockPaymentCoordinator = makeUniqueRefWithoutRefCountedCheck<MockPaymentCoordinator>(*frame->page());
        frame->page()->setPaymentCoordinator(PaymentCoordinator::create(WTFMove(mockPaymentCoordinator)));
    }
#endif

#if PLATFORM(COCOA) &&  ENABLE(WEB_AUDIO)
    AudioDestinationCocoa::createOverride = nullptr;
#endif

#if PLATFORM(COCOA)
    SystemBatteryStatusTestingOverrides::singleton().resetOverridesToDefaultValues();
#endif

#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().resetOverridesToDefaultValues();
#endif

    Scrollbar::setShouldUseFixedPixelsPerLineStepForTesting(true);
}

Document* Internals::contextDocument() const
{
    return downcast<Document>(scriptExecutionContext());
}

LocalFrame* Internals::frame() const
{
    if (!contextDocument())
        return nullptr;
    return contextDocument()->frame();
}

InternalSettings* Internals::settings() const
{
    Document* document = contextDocument();
    if (!document)
        return nullptr;
    Page* page = document->page();
    if (!page)
        return nullptr;
    return InternalSettings::from(page);
}

unsigned Internals::inflightBeaconsCount() const
{
    auto* document = contextDocument();
    if (!document)
        return 0;

    auto* window = document->domWindow();
    if (!window)
        return 0;

    auto* navigator = window->optionalNavigator();
    if (!navigator)
        return 0;

    return NavigatorBeacon::from(*navigator)->inflightBeaconsCount();
}

unsigned Internals::workerThreadCount() const
{
    return WorkerThread::workerThreadCount();
}

ExceptionOr<bool> Internals::areSVGAnimationsPaused() const
{
    RefPtr document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError, "No context document"_s };

    if (!document->svgExtensionsIfExists())
        return Exception { ExceptionCode::NotFoundError, "No SVG animations"_s };

    return document->checkedSVGExtensions()->areAnimationsPaused();
}

ExceptionOr<double> Internals::svgAnimationsInterval(SVGSVGElement& element) const
{
    RefPtr document = contextDocument();
    if (!document)
        return 0;

    CheckedPtr svgExtensions = document->svgExtensionsIfExists();
    if (!svgExtensions)
        return 0;

    if (svgExtensions->areAnimationsPaused())
        return 0;

    return element.timeContainer().animationFrameDelay().value();
}

Vector<Ref<SVGSVGElement>> Internals::allSVGSVGElements() const
{
    Vector<Ref<SVGSVGElement>> elements;
    for (auto& checkedDocument : Document::allDocuments()) {
        Ref document = checkedDocument.get();
        if (CheckedPtr svgExtensions = document->svgExtensionsIfExists())
            elements.appendVector(svgExtensions->allSVGSVGElements());
    }
    return elements;
}

String Internals::address(Node& node)
{
    return makeString("0x"_s, hex(reinterpret_cast<uintptr_t>(&node)));
}

bool Internals::nodeNeedsStyleRecalc(Node& node)
{
    return node.needsStyleRecalc();
}

static String styleValidityToToString(Style::Validity validity)
{
    switch (validity) {
    case Style::Validity::Valid:
        return "NoStyleChange"_s;
    case Style::Validity::AnimationInvalid:
        return "AnimationInvalid"_s;
    case Style::Validity::InlineStyleInvalid:
        return "InlineStyleInvalid"_s;
    case Style::Validity::ElementInvalid:
        return "InlineStyleChange"_s;
    case Style::Validity::SubtreeInvalid:
        return "FullStyleChange"_s;
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

String Internals::styleChangeType(Node& node)
{
    node.document().styleScope().flushPendingUpdate();

    return styleValidityToToString(node.styleValidity());
}

String Internals::description(JSC::JSValue value)
{
    return toString(value);
}

void Internals::log(const String& value)
{
    WTFLogAlways("%s", value.utf8().data());
}

bool Internals::isPreloaded(const String& url)
{
    Document* document = contextDocument();
    return document->cachedResourceLoader().isPreloaded(url);
}

bool Internals::isLoadingFromMemoryCache(const String& url)
{
    CachedResource* resource = resourceFromMemoryCache(url);
    return resource && resource->status() == CachedResource::Cached;
}

CachedResource* Internals::resourceFromMemoryCache(const String& url)
{   
    if (!contextDocument() || !contextDocument()->page())
        return nullptr;

    ResourceRequest request(contextDocument()->completeURL(url));
    request.setDomainForCachePartition(contextDocument()->domainForCachePartition());

    return MemoryCache::singleton().resourceForRequest(request, contextDocument()->page()->sessionID());
}

static String responseSourceToString(const ResourceResponse& response)
{
    if (response.isNull())
        return "Null response"_s;
    switch (response.source()) {
    case ResourceResponse::Source::Unknown:
        return "Unknown"_s;
    case ResourceResponse::Source::Network:
        return "Network"_s;
    case ResourceResponse::Source::ServiceWorker:
        return "Service worker"_s;
    case ResourceResponse::Source::DiskCache:
        return "Disk cache"_s;
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return "Disk cache after validation"_s;
    case ResourceResponse::Source::MemoryCache:
        return "Memory cache"_s;
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return "Memory cache after validation"_s;
    case ResourceResponse::Source::ApplicationCache:
        return "Application cache"_s;
    case ResourceResponse::Source::DOMCache:
        return "DOM cache"_s;
    case ResourceResponse::Source::InspectorOverride:
        return "Inspector override"_s;
    }
    ASSERT_NOT_REACHED();
    return "Error"_s;
}

String Internals::xhrResponseSource(XMLHttpRequest& request)
{
    return responseSourceToString(request.resourceResponse());
}

String Internals::fetchResponseSource(FetchResponse& response)
{
    return responseSourceToString(response.resourceResponse());
}

String Internals::blobInternalURL(const Blob& blob)
{
    return blob.url().string();
}

void Internals::isBlobInternalURLRegistered(const String& url, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    promise.resolve(!!ThreadableBlobRegistry::blobSize(URL { url }));
}

bool Internals::isSharingStyleSheetContents(HTMLLinkElement& a, HTMLLinkElement& b)
{
    if (!a.sheet() || !b.sheet())
        return false;
    return &a.sheet()->contents() == &b.sheet()->contents();
}

bool Internals::isStyleSheetLoadingSubresources(HTMLLinkElement& link)
{
    return link.sheet() && link.sheet()->contents().isLoadingSubresources();
}

static ResourceRequestCachePolicy toResourceRequestCachePolicy(Internals::CachePolicy policy)
{
    switch (policy) {
    case Internals::CachePolicy::UseProtocolCachePolicy:
        return ResourceRequestCachePolicy::UseProtocolCachePolicy;
    case Internals::CachePolicy::ReloadIgnoringCacheData:
        return ResourceRequestCachePolicy::ReloadIgnoringCacheData;
    case Internals::CachePolicy::ReturnCacheDataElseLoad:
        return ResourceRequestCachePolicy::ReturnCacheDataElseLoad;
    case Internals::CachePolicy::ReturnCacheDataDontLoad:
        return ResourceRequestCachePolicy::ReturnCacheDataDontLoad;
    }
    ASSERT_NOT_REACHED();
    return ResourceRequestCachePolicy::UseProtocolCachePolicy;
}

void Internals::setOverrideCachePolicy(CachePolicy policy)
{
    frame()->loader().setOverrideCachePolicyForTesting(toResourceRequestCachePolicy(policy));
}

ExceptionOr<void> Internals::setCanShowModalDialogOverride(bool allow)
{
    if (!contextDocument() || !contextDocument()->domWindow())
        return Exception { ExceptionCode::InvalidAccessError };

    contextDocument()->domWindow()->setCanShowModalDialogOverride(allow);
    return { };
}

static ResourceLoadPriority toResourceLoadPriority(Internals::ResourceLoadPriority priority)
{
    switch (priority) {
    case Internals::ResourceLoadPriority::ResourceLoadPriorityVeryLow:
        return ResourceLoadPriority::VeryLow;
    case Internals::ResourceLoadPriority::ResourceLoadPriorityLow:
        return ResourceLoadPriority::Low;
    case Internals::ResourceLoadPriority::ResourceLoadPriorityMedium:
        return ResourceLoadPriority::Medium;
    case Internals::ResourceLoadPriority::ResourceLoadPriorityHigh:
        return ResourceLoadPriority::High;
    case Internals::ResourceLoadPriority::ResourceLoadPriorityVeryHigh:
        return ResourceLoadPriority::VeryHigh;
    }
    ASSERT_NOT_REACHED();
    return ResourceLoadPriority::Low;
}

void Internals::setOverrideResourceLoadPriority(ResourceLoadPriority priority)
{
    frame()->loader().setOverrideResourceLoadPriorityForTesting(toResourceLoadPriority(priority));
}

void Internals::setStrictRawResourceValidationPolicyDisabled(bool disabled)
{
    frame()->loader().setStrictRawResourceValidationPolicyDisabledForTesting(disabled);
}

static Internals::ResourceLoadPriority toInternalsResourceLoadPriority(ResourceLoadPriority priority)
{
    switch (priority) {
    case ResourceLoadPriority::VeryLow:
        return Internals::ResourceLoadPriority::ResourceLoadPriorityVeryLow;
    case ResourceLoadPriority::Low:
        return Internals::ResourceLoadPriority::ResourceLoadPriorityLow;
    case ResourceLoadPriority::Medium:
        return Internals::ResourceLoadPriority::ResourceLoadPriorityMedium;
    case ResourceLoadPriority::High:
        return Internals::ResourceLoadPriority::ResourceLoadPriorityHigh;
    case ResourceLoadPriority::VeryHigh:
        return Internals::ResourceLoadPriority::ResourceLoadPriorityVeryHigh;
    }
    ASSERT_NOT_REACHED();
    return Internals::ResourceLoadPriority::ResourceLoadPriorityLow;
}

std::optional<Internals::ResourceLoadPriority> Internals::getResourcePriority(const String& url)
{
    auto* document = contextDocument();
    if (!document)
        return std::nullopt;
    auto* resource = document->cachedResourceLoader().cachedResource(url);
    if (!resource)
        resource = resourceFromMemoryCache(url);
    if (resource)
        return toInternalsResourceLoadPriority(resource->loadPriority());
    return std::nullopt;
}

bool Internals::isFetchObjectContextStopped(const FetchObject& object)
{
    return switchOn(object, [](const RefPtr<FetchRequest>& request) {
        return request->isContextStopped();
    }, [](auto& response) {
        return response->isContextStopped();
    });
}

void Internals::clearMemoryCache()
{
    MemoryCache::singleton().evictResources();
    CrossOriginPreflightResultCache::singleton().clear();
}

void Internals::pruneMemoryCacheToSize(unsigned size)
{
    MemoryCache::singleton().pruneDeadResourcesToSize(size);
    MemoryCache::singleton().pruneLiveResourcesToSize(size, true);
}
    
void Internals::destroyDecodedDataForAllImages()
{
    MemoryCache::singleton().destroyDecodedDataForAllImages();
}

unsigned Internals::memoryCacheSize() const
{
    return MemoryCache::singleton().size();
}

static Image* imageFromImageElement(HTMLImageElement& element)
{
    auto* cachedImage = element.cachedImage();
    return cachedImage ? cachedImage->image() : nullptr;
}

static BitmapImage* bitmapImageFromImageElement(HTMLImageElement& element)
{
    return dynamicDowncast<BitmapImage>(imageFromImageElement(element));
}

#if USE(CG)
static PDFDocumentImage* pdfDocumentImageFromImageElement(HTMLImageElement& element)
{
    return dynamicDowncast<PDFDocumentImage>(imageFromImageElement(element));
}
#endif

unsigned Internals::imageFrameIndex(HTMLImageElement& element)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    return bitmapImage ? bitmapImage->currentFrameIndex() : 0;
}

unsigned Internals::imageFrameCount(HTMLImageElement& element)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    return bitmapImage ? bitmapImage->frameCount() : 0;
}

float Internals::imageFrameDurationAtIndex(HTMLImageElement& element, unsigned index)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    return bitmapImage ? bitmapImage->frameDurationAtIndex(index).value() : 0;
}
    
void Internals::setImageFrameDecodingDuration(HTMLImageElement& element, float duration)
{
    if (auto* bitmapImage = bitmapImageFromImageElement(element))
        bitmapImage->setMinimumDecodingDurationForTesting(Seconds { duration });
}

void Internals::resetImageAnimation(HTMLImageElement& element)
{
    if (auto* image = imageFromImageElement(element))
        image->resetAnimation();
}

bool Internals::isImageAnimating(HTMLImageElement& element)
{
    auto* image = imageFromImageElement(element);
    return image && (image->isAnimating() || image->animationPending());
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void Internals::setImageAnimationEnabled(bool enabled)
{
    if (auto* page = contextDocument() ? contextDocument()->page() : nullptr) {
        if (!page->settings().imageAnimationControlEnabled())
            return;

        // We need to set this here to mimic the behavior of the AX preference changing
        Image::setSystemAllowsAnimationControls(!enabled);
        page->setImageAnimationEnabled(enabled);
    }
}

void Internals::resumeImageAnimation(HTMLImageElement& element)
{
    element.setAllowsAnimation(true);
}

void Internals::pauseImageAnimation(HTMLImageElement& element)
{
    element.setAllowsAnimation(false);
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
void Internals::setPrefersNonBlinkingCursor(bool enabled)
{
    auto* document = contextDocument();
    if (RefPtr page = document ? document->page() : nullptr) {
        page->setPrefersNonBlinkingCursor(enabled);
        page->forEachDocument([&](auto& document) {
            document.selection().setPrefersNonBlinkingCursor(enabled);
        });
    }
}
#endif

unsigned Internals::imagePendingDecodePromisesCountForTesting(HTMLImageElement& element)
{
    return element.pendingDecodePromisesCountForTesting();
}

void Internals::setClearDecoderAfterAsyncFrameRequestForTesting(HTMLImageElement& element, bool enabled)
{
    if (auto* bitmapImage = bitmapImageFromImageElement(element))
        bitmapImage->setClearDecoderAfterAsyncFrameRequestForTesting(enabled);
}

unsigned Internals::imageDecodeCount(HTMLImageElement& element)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    return bitmapImage ? bitmapImage->decodeCountForTesting() : 0;
}

unsigned Internals::imageBlankDrawCount(HTMLImageElement& element)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    return bitmapImage ? bitmapImage->blankDrawCountForTesting() : 0;
}

AtomString Internals::imageLastDecodingOptions(HTMLImageElement& element)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    if (!bitmapImage)
        return { };

    auto options = bitmapImage->currentFrameDecodingOptions();
    StringBuilder builder;
    builder.append("{ decodingMode : "_s,
        options.decodingMode() == DecodingMode::Asynchronous ? "Asynchronous"_s : "Synchronous"_s);
    if (auto sizeForDrawing = options.sizeForDrawing()) {
        builder.append(", sizeForDrawing : { "_s, sizeForDrawing->width(),
            ", "_s, sizeForDrawing->height(), " }"_s);
    }
    builder.append(" }"_s);
    return builder.toAtomString();
}

unsigned Internals::imageCachedSubimageCreateCount(HTMLImageElement& element)
{
#if USE(CG)
    if (auto* pdfDocumentImage = pdfDocumentImageFromImageElement(element))
        return pdfDocumentImage->cachedSubimageCreateCountForTesting();
#else
    UNUSED_PARAM(element);
#endif
    return 0;
}

unsigned Internals::remoteImagesCountForTesting() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return 0;

    return document->page()->chrome().client().remoteImagesCountForTesting();
}

void Internals::setAsyncDecodingEnabledForTesting(HTMLImageElement& element, bool enabled)
{
    if (auto* bitmapImage = bitmapImageFromImageElement(element))
        bitmapImage->setAsyncDecodingEnabledForTesting(enabled);
}

void Internals::setForceUpdateImageDataEnabledForTesting(HTMLImageElement& element, bool enabled)
{
    if (auto* cachedImage = element.cachedImage())
        cachedImage->setForceUpdateImageDataEnabledForTesting(enabled);
}

#if ENABLE(WEB_CODECS)
bool Internals::hasPendingActivity(const WebCodecsVideoDecoder& decoder) const
{
    return decoder.hasPendingActivity();
}
#endif

void Internals::setGridMaxTracksLimit(unsigned maxTrackLimit)
{
    GridPosition::setMaxPositionForTesting(maxTrackLimit);
}

void Internals::clearBackForwardCache()
{
    BackForwardCache::singleton().pruneToSizeNow(0, PruningReason::None);
}

unsigned Internals::backForwardCacheSize() const
{
    return BackForwardCache::singleton().pageCount();
}

void Internals::preventDocumentFromEnteringBackForwardCache()
{
    if (auto* document = contextDocument())
        document->preventEnteringBackForwardCacheForTesting();
}

void Internals::disableTileSizeUpdateDelay()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return;

    auto* view = document->frame()->view();
    if (!view)
        return;

    if (auto* backing = view->tiledBacking())
        backing->setTileSizeUpdateDelayDisabledForTesting(true);
}

void Internals::setSpeculativeTilingDelayDisabledForTesting(bool disabled)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return;

    if (auto* frameView = document->frame()->view())
        frameView->setSpeculativeTilingDelayDisabledForTesting(disabled);
}


Node* Internals::treeScopeRootNode(Node& node)
{
    return &node.treeScope().rootNode();
}

Node* Internals::parentTreeScope(Node& node)
{
    const TreeScope* parentTreeScope = node.treeScope().parentTreeScope();
    return parentTreeScope ? &parentTreeScope->rootNode() : nullptr;
}

ExceptionOr<unsigned> Internals::lastSpatialNavigationCandidateCount() const
{
    if (!contextDocument() || !contextDocument()->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return contextDocument()->page()->lastSpatialNavigationCandidateCount();
}

bool Internals::animationWithIdExists(const String& id) const
{
    for (auto* animation : WebAnimation::instances()) {
        if (animation->id() == id)
            return true;
    }
    return false;
}

unsigned Internals::numberOfActiveAnimations() const
{
    return frame()->document()->timeline().numberOfActiveAnimationsForTesting();
}

ExceptionOr<bool> Internals::animationsAreSuspended() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->ensureTimelinesController().animationsAreSuspended();
}

double Internals::animationsInterval() const
{
    Document* document = contextDocument();
    if (!document)
        return INFINITY;

    if (auto timeline = document->existingTimeline())
        return timeline->animationInterval().seconds();
    return INFINITY;
}

ExceptionOr<void> Internals::suspendAnimations() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->ensureTimelinesController().suspendAnimations();
    for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* document = localFrame->document())
            document->ensureTimelinesController().suspendAnimations();
    }

    return { };
}

ExceptionOr<void> Internals::resumeAnimations() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->ensureTimelinesController().resumeAnimations();
    for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* document = localFrame->document())
            document->ensureTimelinesController().resumeAnimations();
    }

    return { };
}

Vector<Internals::AcceleratedAnimation> Internals::acceleratedAnimationsForElement(Element& element)
{
    Vector<Internals::AcceleratedAnimation> animations;
    for (const auto& animationAsPair : element.document().timeline().acceleratedAnimationsForElement(element))
        animations.append({ animationAsPair.first, animationAsPair.second });
    return animations;
}

unsigned Internals::numberOfAnimationTimelineInvalidations() const
{
    return frame()->document()->timeline().numberOfAnimationTimelineInvalidationsForTesting();
}

double Internals::timeToNextAnimationTick(WebAnimation& animation) const
{
    return secondsToWebAnimationsAPITime(animation.timeToNextTick());
}

ExceptionOr<RefPtr<Element>> Internals::pseudoElement(Element& element, const String& pseudoId)
{
    if (pseudoId != "before"_s && pseudoId != "after"_s)
        return Exception { ExceptionCode::InvalidAccessError };

    return pseudoId == "before"_s ? element.beforePseudoElement() : element.afterPseudoElement();
}

double Internals::preferredRenderingUpdateInterval()
{
    auto* document = contextDocument();
    if (!document)
        return 0;
    auto* page = document->page();
    if (!page)
        return 0;
    return page->preferredRenderingUpdateInterval().milliseconds();
}

ExceptionOr<String> Internals::elementRenderTreeAsText(Element& element)
{
    element.document().updateStyleIfNeeded();

    String representation = externalRepresentation(&element);
    if (representation.isEmpty())
        return Exception { ExceptionCode::InvalidAccessError };

    return representation;
}

bool Internals::hasPausedImageAnimations(Element& element)
{
    return element.renderer() && element.renderer()->hasPausedImageAnimations();
}

bool Internals::isFullyActive(Document& document)
{
    return document.isFullyActive();
}

    
bool Internals::isPaintingFrequently(Element& element)
{
    return element.renderer() && element.renderer()->enclosingLayer() && element.renderer()->enclosingLayer()->paintingFrequently();
}

void Internals::incrementFrequentPaintCounter(Element& element)
{
    if (element.renderer() && element.renderer()->enclosingLayer())
        element.renderer()->enclosingLayer()->simulateFrequentPaint();
}

void Internals::purgeFrontBuffer(Element& element)
{
    if (element.renderer()) {
        if (CheckedPtr layer = element.renderer()->enclosingLayer())
            layer->purgeFrontBufferForTesting();
    }
}

void Internals::purgeBackBuffer(Element& element)
{
    if (element.renderer()) {
        if (CheckedPtr layer = element.renderer()->enclosingLayer())
            layer->purgeBackBufferForTesting();
    }
}

void Internals::markFrontBufferVolatile(Element& element)
{
    if (element.renderer()) {
        if (CheckedPtr layer = element.renderer()->enclosingLayer())
            layer->markFrontBufferVolatileForTesting();
    }
}

Ref<CSSComputedStyleDeclaration> Internals::computedStyleIncludingVisitedInfo(Element& element) const
{
    return CSSComputedStyleDeclaration::create(element, CSSComputedStyleDeclaration::AllowVisited::Yes);
}

Node* Internals::ensureUserAgentShadowRoot(Element& host)
{
    return &host.ensureUserAgentShadowRoot();
}

Node* Internals::shadowRoot(Element& host)
{
    if (host.document().hasElementWithPendingUserAgentShadowTreeUpdate(host)) {
        host.updateUserAgentShadowTree();
        host.document().removeElementWithPendingUserAgentShadowTreeUpdate(host);
    }
    return host.shadowRoot();
}

ExceptionOr<String> Internals::shadowRootType(const Node& root) const
{
    if (!is<ShadowRoot>(root))
        return Exception { ExceptionCode::InvalidAccessError };

    switch (downcast<ShadowRoot>(root).mode()) {
    case ShadowRootMode::UserAgent:
        return "UserAgentShadowRoot"_str;
    case ShadowRootMode::Closed:
        return "ClosedShadowRoot"_str;
    case ShadowRootMode::Open:
        return "OpenShadowRoot"_str;
    default:
        ASSERT_NOT_REACHED();
        return "Unknown"_str;
    }
}

const AtomString& Internals::userAgentPart(Element& element)
{
    return element.userAgentPart();
}

void Internals::setUserAgentPart(Element& element, const AtomString& part)
{
    return element.setUserAgentPart(part);
}

ExceptionOr<bool> Internals::isTimerThrottled(int timeoutId)
{
    auto* timer = scriptExecutionContext()->findTimeout(timeoutId);
    if (!timer)
        return Exception { ExceptionCode::NotFoundError };

    if (timer->intervalClampedToMinimum() > timer->m_originalInterval)
        return true;

    return !!scriptExecutionContext()->alignedFireTime(timer->hasReachedMaxNestingLevel(), MonotonicTime { });
}

String Internals::requestAnimationFrameThrottlingReasons() const
{
    auto* scriptedAnimationController = contextDocument()->scriptedAnimationController();
    if (!scriptedAnimationController)
        return String();
        
    TextStream ts;
    ts << scriptedAnimationController->throttlingReasons();
    return ts.release();
}

double Internals::requestAnimationFrameInterval() const
{
    auto* scriptedAnimationController = contextDocument()->scriptedAnimationController();
    if (!scriptedAnimationController)
        return INFINITY;
    return scriptedAnimationController->interval().value();
}

bool Internals::scriptedAnimationsAreSuspended() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return true;

    return document->page()->scriptedAnimationsSuspended();
}

bool Internals::areTimersThrottled() const
{
    return contextDocument()->isTimerThrottlingEnabled();
}

void Internals::setEventThrottlingBehaviorOverride(std::optional<EventThrottlingBehavior> value)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;

    if (!value) {
        document->page()->setEventThrottlingBehaviorOverride(std::nullopt);
        return;
    }

    switch (value.value()) {
    case Internals::EventThrottlingBehavior::Responsive:
        document->page()->setEventThrottlingBehaviorOverride(WebCore::EventThrottlingBehavior::Responsive);
        break;
    case Internals::EventThrottlingBehavior::Unresponsive:
        document->page()->setEventThrottlingBehaviorOverride(WebCore::EventThrottlingBehavior::Unresponsive);
        break;
    }
}

std::optional<Internals::EventThrottlingBehavior> Internals::eventThrottlingBehaviorOverride() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return std::nullopt;

    auto behavior = document->page()->eventThrottlingBehaviorOverride();
    if (!behavior)
        return std::nullopt;

    switch (behavior.value()) {
    case WebCore::EventThrottlingBehavior::Responsive:
        return Internals::EventThrottlingBehavior::Responsive;
    case WebCore::EventThrottlingBehavior::Unresponsive:
        return Internals::EventThrottlingBehavior::Unresponsive;
    }

    return std::nullopt;
}

String Internals::visiblePlaceholder(Element& element)
{
    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (is<HTMLTextFormControlElement>(element)) {
        const HTMLTextFormControlElement& textFormControlElement = downcast<HTMLTextFormControlElement>(element);
        if (!textFormControlElement.isPlaceholderVisible())
            return String();
        if (HTMLElement* placeholderElement = textFormControlElement.placeholderElement())
            return placeholderElement->textContent();
    }

    return String();
}

void Internals::setCanShowPlaceholder(Element& element, bool canShowPlaceholder)
{
    if (is<HTMLTextFormControlElement>(element))
        downcast<HTMLTextFormControlElement>(element).setCanShowPlaceholder(canShowPlaceholder);
}

Element* Internals::insertTextPlaceholder(int width, int height)
{
    return frame()->editor().insertTextPlaceholder(IntSize { width, height }).get();
}

void Internals::removeTextPlaceholder(Element& element)
{
    if (is<TextPlaceholderElement>(element))
        frame()->editor().removeTextPlaceholder(downcast<TextPlaceholderElement>(element));
}

void Internals::selectColorInColorChooser(HTMLInputElement& element, const String& colorValue)
{
    element.selectColor(colorValue);
}

ExceptionOr<Vector<AtomString>> Internals::formControlStateOfPreviousHistoryItem()
{
    HistoryItem* mainItem = frame()->history().previousItem();
    if (!mainItem)
        return Exception { ExceptionCode::InvalidAccessError };
    auto frameID = frame()->frameID();
    if (mainItem->frameID() != frameID && !mainItem->childItemWithFrameID(frameID))
        return Exception { ExceptionCode::InvalidAccessError };
    return Vector<AtomString> { mainItem->frameID() == frameID ? mainItem->documentState() : mainItem->childItemWithFrameID(frameID)->documentState() };
}

ExceptionOr<void> Internals::setFormControlStateOfPreviousHistoryItem(const Vector<AtomString>& state)
{
    HistoryItem* mainItem = frame()->history().previousItem();
    if (!mainItem)
        return Exception { ExceptionCode::InvalidAccessError };
    auto frameID = frame()->frameID();
    if (mainItem->frameID() == frameID)
        mainItem->setDocumentState(state);
    else if (HistoryItem* subItem = mainItem->childItemWithFrameID(frameID))
        subItem->setDocumentState(state);
    else
        return Exception { ExceptionCode::InvalidAccessError };
    return { };
}

#if ENABLE(SPEECH_SYNTHESIS)
void Internals::simulateSpeechSynthesizerVoiceListChange()
{
    if (m_platformSpeechSynthesizer) {
        m_platformSpeechSynthesizer->client().voicesDidChange();
        return;
    }

    RefPtr document = contextDocument();
    if (!document || !document->domWindow())
        return;

    if (RefPtr synthesis = LocalDOMWindowSpeechSynthesis::speechSynthesis(*document->domWindow()))
        synthesis->simulateVoicesListChange();
}

void Internals::enableMockSpeechSynthesizer()
{
    auto document = contextDocument();
    if (!document || !document->domWindow())
        return;
    auto synthesis = LocalDOMWindowSpeechSynthesis::speechSynthesis(*document->domWindow());
    if (!synthesis)
        return;

    auto mock = PlatformSpeechSynthesizerMock::create(*synthesis);
    m_platformSpeechSynthesizer = static_cast<PlatformSpeechSynthesizerMock*>(mock.ptr());
    synthesis->setPlatformSynthesizer(WTFMove(mock));
}

void Internals::enableMockSpeechSynthesizerForMediaElement(HTMLMediaElement& element)
{
    auto& synthesis = element.speechSynthesis();
    auto mock = PlatformSpeechSynthesizerMock::create(synthesis);

    m_platformSpeechSynthesizer = static_cast<PlatformSpeechSynthesizerMock*>(mock.ptr());
    synthesis.setPlatformSynthesizer(WTFMove(mock));
}

ExceptionOr<void> Internals::setSpeechUtteranceDuration(double duration)
{
    if (!m_platformSpeechSynthesizer)
        return Exception { ExceptionCode::InvalidAccessError };

    m_platformSpeechSynthesizer->setUtteranceDuration(Seconds(duration));
    return { };
}

unsigned Internals::minimumExpectedVoiceCount()
{
    // https://webkit.org/b/250656
#if PLATFORM(MAC)
    return 21;
#elif USE(GLIB)
    return 4;
#else
    return 1;
#endif
}

#endif

#if ENABLE(WEB_RTC)

void Internals::emulateRTCPeerConnectionPlatformEvent(RTCPeerConnection& connection, const String& action)
{
    if (!WebRTCProvider::webRTCAvailable())
        return;

    connection.emulatePlatformEvent(action);
}

void Internals::useMockRTCPeerConnectionFactory(const String& testCase)
{
    if (!WebRTCProvider::webRTCAvailable())
        return;

#if USE(LIBWEBRTC)
    Document* document = contextDocument();
    auto* provider = (document && document->page()) ? &static_cast<LibWebRTCProvider&>(document->page()->webRTCProvider()) : nullptr;
    WebCore::useMockRTCPeerConnectionFactory(provider, testCase);
#else
    UNUSED_PARAM(testCase);
#endif
}

void Internals::setICECandidateFiltering(bool enabled)
{
    auto* page = contextDocument()->page();
    if (!page)
        return;

    auto& rtcController = page->rtcController();
    if (enabled)
        rtcController.enableICECandidateFiltering();
    else
        rtcController.disableICECandidateFilteringForAllOrigins();
}

void Internals::setEnumeratingAllNetworkInterfacesEnabled(bool enabled)
{
#if USE(LIBWEBRTC)
    Document* document = contextDocument();
    auto* page = document->page();
    if (!page)
        return;
    auto& rtcProvider = static_cast<LibWebRTCProvider&>(page->webRTCProvider());
    if (enabled)
        rtcProvider.enableEnumeratingAllNetworkInterfaces();
    else
        rtcProvider.disableEnumeratingAllNetworkInterfaces();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::stopPeerConnection(RTCPeerConnection& connection)
{
    ActiveDOMObject& object = connection;
    object.stop();
}

void Internals::clearPeerConnectionFactory()
{
    if (auto* page = contextDocument()->page())
        page->webRTCProvider().clearFactory();
}

void Internals::applyRotationForOutgoingVideoSources(RTCPeerConnection& connection)
{
    connection.applyRotationForOutgoingVideoSources();
}

void Internals::setWebRTCH265Support(bool value)
{
    if (auto* page = contextDocument()->page()) {
        page->webRTCProvider().setH265Support(value);
        page->webRTCProvider().clearFactory();
    }
}

void Internals::setWebRTCVP9Support(bool supportVP9Profile0, bool supportVP9Profile2)
{
    if (auto* page = contextDocument()->page()) {
        page->webRTCProvider().setVP9Support(supportVP9Profile0, supportVP9Profile2);
        page->webRTCProvider().clearFactory();
    }
}

void Internals::disableWebRTCHardwareVP9()
{
#if USE(LIBWEBRTC)
    if (auto* page = contextDocument()->page()) {
        auto& rtcProvider = static_cast<LibWebRTCProvider&>(page->webRTCProvider());
        rtcProvider.setVP9HardwareSupportForTesting(false);
        rtcProvider.clearFactory();
    }
#endif
}

bool Internals::isSupportingVP9HardwareDecoder() const
{
#if USE(LIBWEBRTC)
    if (auto* page = contextDocument()->page()) {
        auto& rtcProvider = static_cast<LibWebRTCProvider&>(page->webRTCProvider());
        return rtcProvider.isSupportingVP9HardwareDecoder();
    }
#endif
    return false;
}

void Internals::isVP9HardwareDecoderUsed(RTCPeerConnection& connection, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    connection.gatherDecoderImplementationName([promise = WTFMove(promise)](auto&& name) mutable {
        promise.resolve(!name.contains("fallback from:"_s) && !name.contains("libvpx"_s));
    });
}

void Internals::setSFrameCounter(RTCRtpSFrameTransform& transform, const String& counter)
{
    if (auto value = parseInteger<uint64_t>(counter))
        transform.setCounterForTesting(*value);
}

uint64_t Internals::sframeCounter(const RTCRtpSFrameTransform& transform)
{
    return transform.counterForTesting();
}

uint64_t Internals::sframeKeyId(const RTCRtpSFrameTransform& transform)
{
    return transform.keyIdForTesting();
}

void Internals::setEnableWebRTCEncryption(bool value)
{
#if USE(LIBWEBRTC)
    if (auto* page = contextDocument()->page())
        page->settings().setWebRTCEncryptionEnabled(value);
#else
    UNUSED_PARAM(value);
#endif
}

#endif // ENABLE(WEB_RTC)

#if ENABLE(MEDIA_STREAM)
void Internals::setShouldInterruptAudioOnPageVisibilityChange(bool shouldInterrupt)
{
    Document* document = contextDocument();
    if (auto* page = document->page())
        page->settings().setInterruptAudioOnPageVisibilityChangeEnabled(shouldInterrupt);
}
#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(MEDIA_RECORDER)
static ExceptionOr<std::unique_ptr<MediaRecorderPrivate>> createRecorderMockSource(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions&)
{
    return std::unique_ptr<MediaRecorderPrivate>(new MediaRecorderPrivateMock(stream));
}

void Internals::setCustomPrivateRecorderCreator()
{
    WebCore::MediaRecorder::setCustomPrivateRecorderCreator(createRecorderMockSource);
}
#endif // ENABLE(MEDIA_RECORDER)

ExceptionOr<Ref<DOMRect>> Internals::absoluteLineRectFromPoint(int x, int y)
{
    if (!contextDocument() || !contextDocument()->page())
        return Exception { ExceptionCode::InvalidAccessError };

    auto& document = *contextDocument();
    if (!document.frame() || !document.view())
        return Exception { ExceptionCode::InvalidAccessError };

    auto& frame = *document.frame();
    auto& view = *document.view();
    document.updateLayout(LayoutOptions::IgnorePendingStylesheets);

    auto position = frame.visiblePositionForPoint(view.rootViewToContents(IntPoint { x, y }));
    return DOMRect::create(position.absoluteSelectionBoundsForLine());
}

ExceptionOr<Ref<DOMRect>> Internals::absoluteCaretBounds()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return DOMRect::create(document->frame()->selection().absoluteCaretBounds());
}

ExceptionOr<bool> Internals::isCaretVisible()
{
    RefPtr document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->frame()->selection().isCaretVisible();
}

ExceptionOr<bool> Internals::isCaretBlinkingSuspended()
{
    auto* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };
    
    return isCaretBlinkingSuspended(*document);
}

ExceptionOr<bool> Internals::isCaretBlinkingSuspended(Document& document)
{
    if (!document.frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return document.frame()->selection().isCaretBlinkingSuspended();
}

Ref<DOMRect> Internals::boundingBox(Element& element)
{
    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);
    auto renderer = element.renderer();
    if (!renderer)
        return DOMRect::create();
    return DOMRect::create(renderer->absoluteBoundingBoxRectIgnoringTransforms());
}

ExceptionOr<unsigned> Internals::inspectorGridOverlayCount()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->inspectorController().gridOverlayCount();
}

ExceptionOr<unsigned> Internals::inspectorFlexOverlayCount()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->inspectorController().flexOverlayCount();
}

ExceptionOr<Ref<DOMRectList>> Internals::inspectorHighlightRects()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    InspectorOverlay::Highlight highlight;
    document->page()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::View);
    return DOMRectList::create(highlight.quads);
}

ExceptionOr<unsigned> Internals::inspectorPaintRectCount()
{
    auto document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->inspectorController().paintRectCount();
}

ExceptionOr<unsigned> Internals::markerCountForNode(Node& node, const String& markerType)
{
    OptionSet<DocumentMarker::Type> markerTypes;
    if (!markerTypesFrom(markerType, markerTypes))
        return Exception { ExceptionCode::SyntaxError };

    node.document().editor().updateEditorUINowIfScheduled();
    return node.document().markers().markersFor(node, markerTypes).size();
}

ExceptionOr<RenderedDocumentMarker*> Internals::markerAt(Node& node, const String& markerType, unsigned index)
{
    node.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    OptionSet<DocumentMarker::Type> markerTypes;
    if (!markerTypesFrom(markerType, markerTypes))
        return Exception { ExceptionCode::SyntaxError };

    node.document().editor().updateEditorUINowIfScheduled();

    auto markers = node.document().markers().markersFor(node, markerTypes);
    if (markers.size() <= index)
        return nullptr;
    return markers[index].get();
}

ExceptionOr<RefPtr<Range>> Internals::markerRangeForNode(Node& node, const String& markerType, unsigned index)
{
    auto result = markerAt(node, markerType, index);
    if (result.hasException())
        return result.releaseException();
    auto marker = result.releaseReturnValue();
    if (!marker)
        return nullptr;
    return { createLiveRange(makeSimpleRange(node, *marker)) };
}

ExceptionOr<String> Internals::markerDescriptionForNode(Node& node, const String& markerType, unsigned index)
{
    auto result = markerAt(node, markerType, index);
    if (result.hasException())
        return result.releaseException();
    auto marker = result.releaseReturnValue();
    if (!marker)
        return String();
    return String { marker->description() };
}

ExceptionOr<String> Internals::dumpMarkerRects(const String& markerTypeString)
{
    DocumentMarker::Type markerType;
    if (!markerTypeFrom(markerTypeString, markerType))
        return Exception { ExceptionCode::SyntaxError };

    contextDocument()->markers().updateRectsForInvalidatedMarkersOfType(markerType);
    auto rects = contextDocument()->markers().renderedRectsForMarkers(markerType);

    // FIXME: Using fixed precision here for width because of test results that contain numbers with specific precision. Would be nice to update the test results and move to default formatting.
    StringBuilder rectString;
    rectString.append("marker rects: "_s);
    for (const auto& rect : rects)
        rectString.append('(', rect.x(), ", "_s, rect.y(), ", "_s, FormattedNumber::fixedPrecision(rect.width()), ", "_s, rect.height(), ") "_s);
    return rectString.toString();
}

ExceptionOr<void> Internals::setMarkedTextMatchesAreHighlighted(bool flag)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };
    document->editor().setMarkedTextMatchesAreHighlighted(flag);
    return { };
}

void Internals::invalidateFontCache()
{
    FontCache::invalidateAllFontCaches();
}

ExceptionOr<void> Internals::setLowPowerModeEnabled(bool isEnabled)
{
    auto* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };
    auto* page = document->page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

    page->setLowPowerModeEnabledOverrideForTesting(isEnabled);
    return { };
}

ExceptionOr<void> Internals::setOutsideViewportThrottlingEnabled(bool isEnabled)
{
    auto* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };
    auto* page = document->page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

    page->setOutsideViewportThrottlingEnabledForTesting(isEnabled);
    return { };
}

ExceptionOr<void> Internals::setScrollViewPosition(int x, int y)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    auto& frameView = *document->view();
    auto oldClamping = frameView.scrollClamping();
    bool scrollbarsSuppressedOldValue = frameView.scrollbarsSuppressed();

    frameView.setScrollClamping(ScrollClamping::Unclamped);
    frameView.setScrollbarsSuppressed(false);
    frameView.setScrollOffsetFromInternals({ x, y });
    frameView.setScrollbarsSuppressed(scrollbarsSuppressedOldValue);
    frameView.setScrollClamping(oldClamping);

    return { };
}

ExceptionOr<void> Internals::unconstrainedScrollTo(Element& element, double x, double y)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    element.scrollTo(ScrollToOptions(x, y), ScrollClamping::Unclamped);

    auto& frameView = *document->view();
    frameView.setViewportConstrainedObjectsNeedLayout();

    return { };
}

ExceptionOr<void> Internals::scrollBySimulatingWheelEvent(Element& element, double deltaX, double deltaY)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!element.renderBox())
        return Exception { ExceptionCode::InvalidAccessError };

    RenderBox& box = *element.renderBox();
    ScrollableArea* scrollableArea;

    if (&element == document->scrollingElementForAPI()) {

        auto* localFrame = dynamicDowncast<LocalFrame>(box.frame().mainFrame());
        if (!localFrame)
            return Exception { ExceptionCode::InvalidAccessError };

        auto* frameView = localFrame->view();
        if (!frameView || !frameView->isScrollable())
            return Exception { ExceptionCode::InvalidAccessError };

        scrollableArea = frameView;
    } else {
        if (!box.canBeScrolledAndHasScrollableArea())
            return Exception { ExceptionCode::InvalidAccessError };

        ASSERT(box.layer());
        scrollableArea = box.layer()->scrollableArea();
    }
    
    if (!scrollableArea)
        return Exception { ExceptionCode::InvalidAccessError };

    auto scrollingNodeID = scrollableArea->scrollingNodeID();
    if (!scrollingNodeID)
        return Exception { ExceptionCode::InvalidAccessError };

    auto page = document->page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return Exception { ExceptionCode::InvalidAccessError };

    scrollingCoordinator->scrollBySimulatingWheelEventForTesting(*scrollingNodeID, FloatSize(deltaX, deltaY));

    return { };
}

ExceptionOr<Ref<DOMRect>> Internals::layoutViewportRect()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    auto& frameView = *document->view();
    return DOMRect::create(frameView.layoutViewportRect());
}

ExceptionOr<Ref<DOMRect>> Internals::visualViewportRect()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    auto& frameView = *document->view();
    return DOMRect::create(frameView.visualViewportRect());
}

ExceptionOr<void> Internals::setViewIsTransparent(bool transparent)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setTransparent(transparent);
    return { };
}

ExceptionOr<String> Internals::viewBaseBackgroundColor()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    return serializationForCSS(document->view()->baseBackgroundColor());
}

ExceptionOr<void> Internals::setViewBaseBackgroundColor(const String& colorValue)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    if (colorValue == "transparent"_s) {
        document->view()->setBaseBackgroundColor(Color::transparentBlack);
        return { };
    }
    if (colorValue == "white"_s) {
        document->view()->setBaseBackgroundColor(Color::white);
        return { };
    }
    return Exception { ExceptionCode::SyntaxError };
}

using LazySlowPathColorParsingParameters = std::tuple<
    CSSPropertyParserHelpers::CSSColorParsingOptions,
    CSSUnresolvedColorResolutionState,
    std::optional<CSSUnresolvedColorResolutionDelegate>
>;

ExceptionOr<void> Internals::setUnderPageBackgroundColorOverride(const String& colorValue)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    auto color = CSSPropertyParserHelpers::parseColorRaw(colorValue, document->cssParserContext(), [] {
        return LazySlowPathColorParsingParameters { { }, { }, std::nullopt };
    });
    if (!color.isValid())
        return Exception { ExceptionCode::SyntaxError };

    document->page()->setUnderPageBackgroundColorOverride(WTFMove(color));
    return { };
}

ExceptionOr<String> Internals::documentBackgroundColor()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };
    return serializationForCSS(document->view()->documentBackgroundColor());
}

ExceptionOr<void> Internals::setPagination(const String& mode, int gap, int pageLength)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    Pagination pagination;
    if (mode == "Unpaginated"_s)
        pagination.mode = Pagination::Mode::Unpaginated;
    else if (mode == "LeftToRightPaginated"_s)
        pagination.mode = Pagination::Mode::LeftToRightPaginated;
    else if (mode == "RightToLeftPaginated"_s)
        pagination.mode = Pagination::Mode::RightToLeftPaginated;
    else if (mode == "TopToBottomPaginated"_s)
        pagination.mode = Pagination::Mode::TopToBottomPaginated;
    else if (mode == "BottomToTopPaginated"_s)
        pagination.mode = Pagination::Mode::BottomToTopPaginated;
    else
        return Exception { ExceptionCode::SyntaxError };

    pagination.gap = gap;
    pagination.pageLength = pageLength;
    document->page()->setPagination(pagination);

    return { };
}

ExceptionOr<uint64_t> Internals::lineIndexAfterPageBreak(Element& element)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!element.renderer() || !is<RenderBlockFlow>(element.renderer()))
        return Exception { ExceptionCode::NotFoundError };
    auto& blockFlow = downcast<RenderBlockFlow>(*element.renderer());
    if (!blockFlow.childrenInline())
        return Exception { ExceptionCode::NotFoundError };

    size_t lineIndex = 0;
    for (auto lineBox = InlineIterator::firstLineBoxFor(blockFlow); lineBox; lineBox.traverseNext(), ++lineIndex) {
        if (lineBox->isFirstAfterPageBreak())
            return lineIndex;
    }
    return Exception { ExceptionCode::NotFoundError };
}

ExceptionOr<String> Internals::configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    const int defaultLayoutWidthForNonMobilePages = 980;

    ViewportArguments arguments = document->page()->viewportArguments();
    ViewportAttributes attributes = computeViewportAttributes(arguments, defaultLayoutWidthForNonMobilePages, deviceWidth, deviceHeight, devicePixelRatio, IntSize(availableWidth, availableHeight));
    restrictMinimumScaleFactorToViewportSize(attributes, IntSize(availableWidth, availableHeight), devicePixelRatio);
    restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    // FIXME: Using fixed precision here because of test results that contain numbers with specific precision. Would be nice to update the test results and move to default formatting.
    return makeString("viewport size "_s, FormattedNumber::fixedPrecision(attributes.layoutSize.width()), 'x', FormattedNumber::fixedPrecision(attributes.layoutSize.height()), " scale "_s, FormattedNumber::fixedPrecision(attributes.initialScale), " with limits ["_s, FormattedNumber::fixedPrecision(attributes.minimumScale), ", "_s, FormattedNumber::fixedPrecision(attributes.maximumScale), "] and userScalable "_s, (attributes.userScalable ? "true"_s : "false"_s));
}

ExceptionOr<bool> Internals::wasLastChangeUserEdit(Element& textField)
{
    if (is<HTMLInputElement>(textField))
        return downcast<HTMLInputElement>(textField).lastChangeWasUserEdit();

    if (is<HTMLTextAreaElement>(textField))
        return downcast<HTMLTextAreaElement>(textField).lastChangeWasUserEdit();

    return Exception { ExceptionCode::InvalidNodeTypeError };
}

bool Internals::elementShouldAutoComplete(HTMLInputElement& element)
{
    return element.shouldAutocomplete();
}

void Internals::setAutofilled(HTMLInputElement& element, bool enabled)
{
    element.setAutofilled(enabled);
}

void Internals::setAutofilledAndViewable(HTMLInputElement& element, bool enabled)
{
    element.setAutofilledAndViewable(enabled);
}

void Internals::setAutofilledAndObscured(HTMLInputElement& element, bool enabled)
{
    element.setAutofilledAndObscured(enabled);
}

static AutoFillButtonType toAutofillButtonType(Internals::AutoFillButtonType type)
{
    switch (type) {
    case Internals::AutoFillButtonType::None:
        return AutoFillButtonType::None;
    case Internals::AutoFillButtonType::Credentials:
        return AutoFillButtonType::Credentials;
    case Internals::AutoFillButtonType::Contacts:
        return AutoFillButtonType::Contacts;
    case Internals::AutoFillButtonType::StrongPassword:
        return AutoFillButtonType::StrongPassword;
    case Internals::AutoFillButtonType::CreditCard:
        return AutoFillButtonType::CreditCard;
    case Internals::AutoFillButtonType::Loading:
        return AutoFillButtonType::Loading;
    }
    ASSERT_NOT_REACHED();
    return AutoFillButtonType::None;
}

static Internals::AutoFillButtonType toInternalsAutofillButtonType(AutoFillButtonType type)
{
    switch (type) {
    case AutoFillButtonType::None:
        return Internals::AutoFillButtonType::None;
    case AutoFillButtonType::Credentials:
        return Internals::AutoFillButtonType::Credentials;
    case AutoFillButtonType::Contacts:
        return Internals::AutoFillButtonType::Contacts;
    case AutoFillButtonType::StrongPassword:
        return Internals::AutoFillButtonType::StrongPassword;
    case AutoFillButtonType::CreditCard:
        return Internals::AutoFillButtonType::CreditCard;
    case AutoFillButtonType::Loading:
        return Internals::AutoFillButtonType::Loading;
    }
    ASSERT_NOT_REACHED();
    return Internals::AutoFillButtonType::None;
}

void Internals::setAutofillButtonType(HTMLInputElement& element, AutoFillButtonType type)
{
    element.setAutofillButtonType(toAutofillButtonType(type));
}

auto Internals::autofillButtonType(const HTMLInputElement& element) -> AutoFillButtonType
{
    return toInternalsAutofillButtonType(element.autofillButtonType());
}

auto Internals::lastAutofillButtonType(const HTMLInputElement& element) -> AutoFillButtonType
{
    return toInternalsAutofillButtonType(element.lastAutofillButtonType());
}

Vector<String> Internals::recentSearches(const HTMLInputElement& element)
{
    if (!element.isSearchField())
        return { };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);
    auto* renderer = element.renderer();
    if (!is<RenderSearchField>(renderer))
        return { };

    Vector<String> result;
    auto& searchField = downcast<RenderSearchField>(*renderer);
    for (auto search : searchField.recentSearches())
        result.append(search.string);

    return result;
}

ExceptionOr<void> Internals::scrollElementToRect(Element& element, int x, int y, int w, int h)
{
    auto* frameView = element.document().view();
    if (!frameView)
        return Exception { ExceptionCode::InvalidAccessError };
    frameView->scrollElementToRect(element, { x, y, w, h });
    return { };
}

ExceptionOr<String> Internals::autofillFieldName(Element& element)
{
    if (!is<HTMLFormControlElement>(element))
        return Exception { ExceptionCode::InvalidNodeTypeError };

    return String { downcast<HTMLFormControlElement>(element).autofillData().fieldName };
}

ExceptionOr<void> Internals::invalidateControlTints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->invalidateControlTints();
    return { };
}

RefPtr<Range> Internals::rangeFromLocationAndLength(Element& scope, unsigned rangeLocation, unsigned rangeLength)
{
    return createLiveRange(resolveCharacterRange(makeRangeSelectingNodeContents(scope), { rangeLocation, rangeLength }));
}

unsigned Internals::locationFromRange(Element& scope, const Range& range)
{
    return clampTo<unsigned>(characterRange(makeBoundaryPointBeforeNodeContents(scope), makeSimpleRange(range)).location);
}

unsigned Internals::lengthFromRange(Element& scope, const Range& range)
{
    return clampTo<unsigned>(characterRange(makeBoundaryPointBeforeNodeContents(scope), makeSimpleRange(range)).length);
}

String Internals::rangeAsText(const Range& liveRange)
{
    auto range = makeSimpleRange(liveRange);
    range.start.document().updateLayout();
    return plainText(range);
}

// FIXME: Move this to StringConcatenate.h.
static String join(Vector<String>&& strings)
{
    StringBuilder result;
    for (auto& string : strings)
        result.append(WTFMove(string));
    return result.toString();
}

String Internals::rangeAsTextUsingBackwardsTextIterator(const Range& liveRange)
{
    auto range = makeSimpleRange(liveRange);
    range.start.document().updateLayout();
    Vector<String> strings;
    for (SimplifiedBackwardsTextIterator backwardsIterator(range); !backwardsIterator.atEnd(); backwardsIterator.advance())
        strings.append(backwardsIterator.text().toString());
    strings.reverse();
    return join(WTFMove(strings));
}

Ref<Range> Internals::subrange(Range& liveRange, unsigned rangeLocation, unsigned rangeLength)
{
    auto range = makeSimpleRange(liveRange);
    range.start.document().updateLayout();
    return createLiveRange(resolveCharacterRange(range, { rangeLocation, rangeLength }));
}

RefPtr<Range> Internals::rangeOfStringNearLocation(const Range& liveRange, const String& text, unsigned targetOffset)
{
    auto range = makeSimpleRange(liveRange);
    range.start.document().updateLayout();
    return createLiveRange(findClosestPlainText(range, text, { }, targetOffset));
}

Vector<Internals::TextIteratorState> Internals::statesOfTextIterator(const Range& liveRange)
{
    auto simpleRange = makeSimpleRange(liveRange);
    simpleRange.start.document().updateLayout();

    Vector<TextIteratorState> states;
    for (TextIterator it(simpleRange); !it.atEnd(); it.advance())
        states.append({ it.text().toString(), createLiveRange(it.range()) });
    return states;
}

String Internals::textFragmentDirectiveForRange(const Range& range)
{
    auto simpleRange = makeSimpleRange(range);
    simpleRange.start.document().updateLayout();
    return FragmentDirectiveGenerator(simpleRange).urlWithFragment().string();
}

#if !PLATFORM(MAC)
ExceptionOr<RefPtr<Range>> Internals::rangeForDictionaryLookupAtLocation(int, int)
{
    return Exception { ExceptionCode::InvalidAccessError };
}
#endif

ExceptionOr<void> Internals::setDelegatesScrolling(bool enabled)
{
    Document* document = contextDocument();
    // Delegate scrolling is valid only on mainframe's view.
    if (!document || !document->view() || !document->page() || &document->page()->mainFrame() != document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setDelegatedScrollingMode(enabled ? DelegatedScrollingMode::DelegatedToNativeScrollView : DelegatedScrollingMode::NotDelegated);
    return { };
}

ExceptionOr<uint64_t> Internals::lastSpellCheckRequestSequence()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    auto lastRequestIdentifier = document->editor().spellChecker().lastRequestIdentifier();
    return lastRequestIdentifier ? lastRequestIdentifier->toUInt64() : 0;
}

ExceptionOr<uint64_t> Internals::lastSpellCheckProcessedSequence()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    auto lastProcessedIdentifier = document->editor().spellChecker().lastProcessedIdentifier();
    return lastProcessedIdentifier ? lastProcessedIdentifier->toUInt64() : 0;
}

void Internals::advanceToNextMisspelling()
{
#if !PLATFORM(IOS_FAMILY)
    if (auto* document = contextDocument())
        document->editor().advanceToNextMisspelling();
#endif
}

Vector<String> Internals::userPreferredLanguages() const
{
    return WTF::userPreferredLanguages(ShouldMinimizeLanguages::No);
}

void Internals::setUserPreferredLanguages(const Vector<String>& languages)
{
    overrideUserPreferredLanguages(languages);
}

Vector<String> Internals::userPreferredAudioCharacteristics() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Vector<String>();
#if ENABLE(VIDEO)
    return document->page()->group().ensureCaptionPreferences().preferredAudioCharacteristics();
#else
    return Vector<String>();
#endif
}

void Internals::setUserPreferredAudioCharacteristic(const String& characteristic)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;
#if ENABLE(VIDEO)
    document->page()->group().ensureCaptionPreferences().setPreferredAudioCharacteristic(characteristic);
#else
    UNUSED_PARAM(characteristic);
#endif
}

ExceptionOr<unsigned> Internals::wheelEventHandlerCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    return document->wheelEventHandlerCount();
}

ExceptionOr<unsigned> Internals::touchEventHandlerCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    return document->touchEventHandlerCount();
}

ExceptionOr<Ref<DOMRectList>> Internals::touchEventRectsForEvent(const String& eventName)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    std::array<EventTrackingRegions::EventType, 4> touchEvents = { {
        EventTrackingRegions::EventType::Touchstart,
        EventTrackingRegions::EventType::Touchmove,
        EventTrackingRegions::EventType::Touchend,
        EventTrackingRegions::EventType::Touchforcechange,
    } };

    std::optional<EventTrackingRegions::EventType> touchEvent;
    for (auto event : touchEvents) {
        if (eventName == EventTrackingRegions::eventName(event)) {
            touchEvent = event;
            break;
        }
    }

    if (!touchEvent)
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->touchEventRectsForEventForTesting(touchEvent.value());
}

ExceptionOr<Ref<DOMRectList>> Internals::passiveTouchEventListenerRects()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->passiveTouchEventListenerRectsForTesting();
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
ExceptionOr<RefPtr<NodeList>> Internals::nodesFromRect(Document& document, int centerX, int centerY, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowUserAgentShadowContent, bool allowChildFrameContent) const
{
    if (!document.frame() || !document.frame()->view())
        return Exception { ExceptionCode::InvalidAccessError };

    auto* frame = document.frame();
    auto* frameView = document.view();
    auto* renderView = document.renderView();
    if (!renderView)
        return nullptr;

    document.updateLayout(LayoutOptions::IgnorePendingStylesheets);

    float zoomFactor = frame->pageZoomFactor();
    LayoutPoint point(centerX * zoomFactor + frameView->scrollX(), centerY * zoomFactor + frameView->scrollY());

    OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::CollectMultipleElements };
    if (ignoreClipping)
        hitType.add(HitTestRequest::Type::IgnoreClipping);
    if (!allowUserAgentShadowContent)
        hitType.add(HitTestRequest::Type::DisallowUserAgentShadowContent);
    if (allowChildFrameContent)
        hitType.add(HitTestRequest::Type::AllowChildFrameContent);

    HitTestRequest request(hitType);

    auto hitTestResult = [&] {
        auto size = LayoutSize { leftPadding + rightPadding + 1, topPadding + bottomPadding + 1 };
        if (size.isEmpty())
            return HitTestResult { point };
        auto adjustedPosition = LayoutPoint { flooredIntPoint(point) } - LayoutSize  { leftPadding, topPadding };
        return HitTestResult { LayoutRect { adjustedPosition, size } };
    }();
    // When ignoreClipping is false, this method returns null for coordinates outside of the viewport.
    if (!request.ignoreClipping() && !hitTestResult.hitTestLocation().intersects(LayoutRect { frameView->visibleContentRect() }))
        return nullptr;

    document.hitTest(request, hitTestResult);
    auto matches = WTF::map(hitTestResult.listBasedTestResult(), [](const auto& node) { return node.copyRef(); });
    return RefPtr<NodeList> { StaticNodeList::create(WTFMove(matches)) };
}

class GetCallerCodeBlockFunctor {
public:
    GetCallerCodeBlockFunctor()
        : m_iterations(0)
        , m_codeBlock(0)
    {
    }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        ++m_iterations;
        if (m_iterations < 2)
            return IterationStatus::Continue;

        m_codeBlock = visitor->codeBlock();
        return IterationStatus::Done;
    }

    CodeBlock* codeBlock() const { return m_codeBlock; }

private:
    mutable int m_iterations;
    mutable CodeBlock* m_codeBlock;
};

String Internals::parserMetaData(JSC::JSValue code)
{
    auto& vm = contextDocument()->vm();
    auto callFrame = vm.topCallFrame;
    auto* globalObject = callFrame->lexicalGlobalObject(vm);

    ScriptExecutable* executable;
    if (!code || code.isNull() || code.isUndefined()) {
        GetCallerCodeBlockFunctor iter;
        StackVisitor::visit(callFrame, vm, iter);
        executable = iter.codeBlock()->ownerExecutable();
    } else if (code.isCallable())
        executable = JSC::jsCast<JSFunction*>(code.toObject(globalObject))->jsExecutable();
    else
        return String();

    ASCIILiteral prefix;
    String functionName;
    ASCIILiteral suffix = ""_s;

    if (executable->isFunctionExecutable()) {
        prefix = "function \""_s;
        functionName = static_cast<FunctionExecutable*>(executable)->ecmaName().string();
        suffix = "\""_s;
    } else if (executable->isEvalExecutable())
        prefix = "eval"_s;
    else if (executable->isModuleProgramExecutable())
        prefix = "module"_s;
    else if (executable->isProgramExecutable())
        prefix = "program"_s;
    else
        RELEASE_ASSERT_NOT_REACHED();

    return makeString(prefix, functionName, suffix, " { "_s,
        executable->firstLine(), ':', executable->startColumn(), " - "_s,
        executable->lastLine(), ':', executable->endColumn(), " }"_s);
}

void Internals::updateEditorUINowIfScheduled()
{
    if (Document* document = contextDocument()) {
        if (LocalFrame* frame = document->frame())
            frame->editor().updateEditorUINowIfScheduled();
    }
}

bool Internals::hasMarkerFor(DocumentMarker::Type type, int from, int length)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->editor().selectionStartHasMarkerFor(type, from, length);
}

ExceptionOr<void> Internals::setMarkerFor(const String& markerTypeString, int from, int length, const String& data)
{
    DocumentMarker::Type markerType;
    if (!markerTypeFrom(markerTypeString, markerType))
        return Exception { ExceptionCode::SyntaxError };

    Document* document = contextDocument();
    if (!document || !document->frame())
        return { };

    updateEditorUINowIfScheduled();

    document->editor().selectionStartSetMarkerForTesting(markerType, from, length, data);
    return { };
}

bool Internals::hasSpellingMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::Spelling, from, length);
}

bool Internals::hasGrammarMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::Grammar, from, length);
}

bool Internals::hasAutocorrectedMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::Autocorrected, from, length);
}

bool Internals::hasDictationAlternativesMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::DictationAlternatives, from, length);
}

bool Internals::hasCorrectionIndicatorMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::CorrectionIndicator, from, length);
}

#if ENABLE(WRITING_TOOLS)
bool Internals::hasWritingToolsTextSuggestionMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::WritingToolsTextSuggestion, from, length);
}
#endif

bool Internals::hasTransparentContentMarker(int from, int length)
{
    return hasMarkerFor(DocumentMarker::Type::TransparentContent, from, length);
}

void Internals::setContinuousSpellCheckingEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

    if (enabled != contextDocument()->editor().isContinuousSpellCheckingEnabled())
        contextDocument()->editor().toggleContinuousSpellChecking();
}

void Internals::setAutomaticQuoteSubstitutionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->editor().isAutomaticQuoteSubstitutionEnabled())
        contextDocument()->editor().toggleAutomaticQuoteSubstitution();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticLinkDetectionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->editor().isAutomaticLinkDetectionEnabled())
        contextDocument()->editor().toggleAutomaticLinkDetection();
#else
    UNUSED_PARAM(enabled);
#endif
}

bool Internals::testProcessIncomingSyncMessagesWhenWaitingForSyncReply()
{
    ASSERT(contextDocument());
    ASSERT(contextDocument()->page());
    return contextDocument()->page()->chrome().client().testProcessIncomingSyncMessagesWhenWaitingForSyncReply();
}

void Internals::setAutomaticDashSubstitutionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->editor().isAutomaticDashSubstitutionEnabled())
        contextDocument()->editor().toggleAutomaticDashSubstitution();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticTextReplacementEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->editor().isAutomaticTextReplacementEnabled())
        contextDocument()->editor().toggleAutomaticTextReplacement();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticSpellingCorrectionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->editor().isAutomaticSpellingCorrectionEnabled())
        contextDocument()->editor().toggleAutomaticSpellingCorrection();
#else
    UNUSED_PARAM(enabled);
#endif
}

bool Internals::isSpellcheckDisabledExceptTextReplacement(const HTMLInputElement& element) const
{
    return element.isSpellcheckDisabledExceptTextReplacement();
}

void Internals::handleAcceptedCandidate(const String& candidate, unsigned location, unsigned length)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

    TextCheckingResult result;
    result.type = TextCheckingType::None;
    result.range = { location, length };
    result.replacement = candidate;
    contextDocument()->editor().handleAcceptedCandidate(result);
}

void Internals::changeSelectionListType()
{
    if (RefPtr frame = this->frame())
        frame->editor().changeSelectionListType();
}

void Internals::changeBackToReplacedString(const String& replacedString)
{
    if (RefPtr frame = this->frame())
        frame->editor().changeBackToReplacedString(replacedString);
}

bool Internals::isOverwriteModeEnabled()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    return document->editor().isOverwriteModeEnabled();
}

void Internals::toggleOverwriteModeEnabled()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return;

    document->editor().toggleOverwriteModeEnabled();
}

static ExceptionOr<FindOptions> parseFindOptions(const Vector<String>& optionList)
{
    const struct {
        ASCIILiteral name;
        FindOption value;
    } flagList[] = {
        { "CaseInsensitive"_s, FindOption::CaseInsensitive },
        { "AtWordStarts"_s, FindOption::AtWordStarts },
        { "TreatMedialCapitalAsWordStart"_s, FindOption::TreatMedialCapitalAsWordStart },
        { "Backwards"_s, FindOption::Backwards },
        { "WrapAround"_s, FindOption::WrapAround },
        { "StartInSelection"_s, FindOption::StartInSelection },
        { "DoNotRevealSelection"_s, FindOption::DoNotRevealSelection },
        { "AtWordEnds"_s, FindOption::AtWordEnds },
        { "DoNotTraverseFlatTree"_s, FindOption::DoNotTraverseFlatTree },
    };
    FindOptions result;
    for (auto& option : optionList) {
        bool found = false;
        for (auto& flag : flagList) {
            if (flag.name == option) {
                result.add(flag.value);
                found = true;
                break;
            }
        }
        if (!found)
            return Exception { ExceptionCode::SyntaxError };
    }
    return result;
}

ExceptionOr<RefPtr<Range>> Internals::rangeOfString(const String& text, RefPtr<Range>&& referenceRange, const Vector<String>& findOptions)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    return createLiveRange(document->editor().rangeOfString(text, makeSimpleRange(referenceRange), parsedOptions.releaseReturnValue()));
}

ExceptionOr<unsigned> Internals::countMatchesForText(const String& text, const Vector<String>& findOptions, const String& markMatches)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    bool mark = markMatches == "mark"_s;
    return document->editor().countMatchesForText(text, std::nullopt, parsedOptions.releaseReturnValue(), 1000, mark, nullptr);
}

ExceptionOr<unsigned> Internals::countFindMatches(const String& text, const Vector<String>& findOptions)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    return document->page()->countFindMatches(text, parsedOptions.releaseReturnValue(), 1000);
}

unsigned Internals::numberOfIDBTransactions() const
{
    return IDBTransaction::numberOfIDBTransactions;
}

unsigned Internals::numberOfLiveNodes() const
{
    unsigned nodeCount = 0;
    for (auto& document : Document::allDocuments())
        nodeCount += document->referencingNodeCount();
    return nodeCount;
}

unsigned Internals::numberOfLiveDocuments() const
{
    return Document::allDocuments().size();
}

unsigned Internals::referencingNodeCount(const Document& document) const
{
    return document.referencingNodeCount();
}

#if ENABLE(WEB_AUDIO)
uint64_t Internals::baseAudioContextIdentifier(const BaseAudioContext& context)
{
    return context.contextID();
}

bool Internals::isBaseAudioContextAlive(uint64_t contextID)
{
    ASSERT(contextID);
    return BaseAudioContext::isContextAlive(contextID);
}
#endif // ENABLE(WEB_AUDIO)

unsigned Internals::numberOfIntersectionObservers(const Document& document) const
{
    return document.numberOfIntersectionObservers();
}

unsigned Internals::numberOfResizeObservers(const Document& document) const
{
    return document.numberOfResizeObservers();
}

String Internals::documentIdentifier(const Document& document) const
{
    return document.identifier().object().toString();
}

bool Internals::isDocumentAlive(const String& documentIdentifier) const
{
    auto uuid = WTF::UUID::parseVersion4(documentIdentifier);
    ASSERT(uuid);
    return uuid ? Document::allDocumentsMap().contains({ *uuid, Process::identifier() }) : false;
}

uint64_t Internals::messagePortIdentifier(const MessagePort& port) const
{
    return port.identifier().portIdentifier.toUInt64();
}

bool Internals::isMessagePortAlive(uint64_t messagePortIdentifier) const
{
    MessagePortIdentifier portIdentifier { Process::identifier(), AtomicObjectIdentifier<PortIdentifierType>(messagePortIdentifier) };
    return MessagePort::isMessagePortAliveForTesting(portIdentifier);
}

uint64_t Internals::storageAreaMapCount() const
{
    auto* page = contextDocument() ? contextDocument()->page() : nullptr;
    if (!page)
        return 0;

    return page->storageNamespaceProvider().localStorageNamespace(page->sessionID()).storageAreaMapCountForTesting();
}

uint64_t Internals::elementIdentifier(Element& element) const
{
    return element.identifier().toUInt64();
}

bool Internals::isElementAlive(uint64_t elementIdentifier) const
{
    return Element::fromIdentifier(ObjectIdentifier<ElementIdentifierType>(elementIdentifier));
}

uint64_t Internals::pageIdentifier(const Document& document) const
{
    return document.pageID() ? document.pageID()->toUInt64() : 0;
}

bool Internals::isAnyWorkletGlobalScopeAlive() const
{
    return WorkletGlobalScope::numberOfWorkletGlobalScopes();
}

String Internals::serviceWorkerClientInternalIdentifier(const Document& document) const
{
    return document.identifier().toString();
}

RefPtr<WindowProxy> Internals::openDummyInspectorFrontend(const String& url)
{
    auto* inspectedPage = contextDocument()->frame()->page();
    auto* localMainFrame = dynamicDowncast<LocalFrame>(inspectedPage->mainFrame());
    if (!localMainFrame)
        return nullptr;

    auto* window = localMainFrame->document()->domWindow();
    if (!window)
        return nullptr;

    auto frontendWindowProxy = window->open(*window, *window, url, emptyAtom(), emptyString()).releaseReturnValue();
    m_inspectorFrontend = makeUnique<InspectorStubFrontend>(*inspectedPage, downcast<LocalDOMWindow>(frontendWindowProxy->window()));
    return frontendWindowProxy;
}

void Internals::closeDummyInspectorFrontend()
{
    m_inspectorFrontend = nullptr;
}

ExceptionOr<void> Internals::setInspectorIsUnderTest(bool isUnderTest)
{
    Page* page = contextDocument()->frame()->page();
    if (!page)
        return Exception { ExceptionCode::InvalidAccessError };

    page->inspectorController().setIsUnderTest(isUnderTest);
    return { };
}

unsigned Internals::numberOfScrollableAreas()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

    unsigned count = 0;
    LocalFrame* frame = document->frame();
    if (frame->view()->scrollableAreas())
        count += frame->view()->scrollableAreas()->computeSize();

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        auto* frameView = localChild->view();
        if (!frameView)
            continue;
        if (frameView->scrollableAreas())
            count += frameView->scrollableAreas()->computeSize();
    }

    return count;
}

ExceptionOr<bool> Internals::isPageBoxVisible(int pageNumber)
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    return document->isPageBoxVisible(pageNumber);
}

static OptionSet<LayerTreeAsTextOptions> toLayerTreeAsTextOptions(unsigned short flags)
{
    OptionSet<LayerTreeAsTextOptions> layerTreeFlags;
    if (flags & Internals::LAYER_TREE_INCLUDES_VISIBLE_RECTS)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeVisibleRects);
    if (flags & Internals::LAYER_TREE_INCLUDES_TILE_CACHES)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeTileCaches);
    if (flags & Internals::LAYER_TREE_INCLUDES_REPAINT_RECTS)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeRepaintRects);
    if (flags & Internals::LAYER_TREE_INCLUDES_PAINTING_PHASES)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludePaintingPhases);
    if (flags & Internals::LAYER_TREE_INCLUDES_CONTENT_LAYERS)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeContentLayers);
    if (flags & Internals::LAYER_TREE_INCLUDES_ACCELERATES_DRAWING)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeAcceleratesDrawing);
    if (flags & Internals::LAYER_TREE_INCLUDES_CLIPPING)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeClipping);
    if (flags & Internals::LAYER_TREE_INCLUDES_BACKING_STORE_ATTACHED)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeBackingStoreAttached);
    if (flags & Internals::LAYER_TREE_INCLUDES_ROOT_LAYER_PROPERTIES)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeRootLayerProperties);
    if (flags & Internals::LAYER_TREE_INCLUDES_EVENT_REGION)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeEventRegion);
    if (flags & Internals::LAYER_TREE_INCLUDES_EXTENDED_COLOR)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeExtendedColor);
    if (flags & Internals::LAYER_TREE_INCLUDES_DEVICE_SCALE)
        layerTreeFlags.add(LayerTreeAsTextOptions::IncludeDeviceScale);

    return layerTreeFlags;
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
ExceptionOr<String> Internals::layerTreeAsText(Document& document, unsigned short flags) const
{
    if (!document.frame() || !document.frame()->contentRenderer())
        return Exception { ExceptionCode::InvalidAccessError };
    return document.frame()->contentRenderer()->compositor().layerTreeAsText(toLayerTreeAsTextOptions(flags));
}

ExceptionOr<uint64_t> Internals::layerIDForElement(Element& element)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!element.renderer() || !element.renderer()->hasLayer())
        return Exception { ExceptionCode::NotFoundError };

    auto& layerModelObject = downcast<RenderLayerModelObject>(*element.renderer());
    if (!layerModelObject.layer()->isComposited())
        return Exception { ExceptionCode::NotFoundError };

    auto* backing = layerModelObject.layer()->backing();
    return backing->graphicsLayer()->primaryLayerID() ? backing->graphicsLayer()->primaryLayerID()->object().toUInt64() : 0;
}

ExceptionOr<Vector<uint64_t>> Internals::scrollingNodeIDForNode(Node* node)
{
    auto areaOrException = scrollableAreaForNode(node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto scrollingNodeID = areaOrException.returnValue()->scrollingNodeID();
    if (!scrollingNodeID)
        return Vector<uint64_t>({ 0, 0 });
    return Vector({ scrollingNodeID->object().toUInt64(), scrollingNodeID->processIdentifier().toUInt64() });
}

ExceptionOr<unsigned> Internals::scrollableAreaWidth(Node& node)
{
    auto areaOrException = scrollableAreaForNode(&node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();
    auto* scrollableArea = areaOrException.releaseReturnValue();
    return scrollableArea->contentsSize().width();
}

static OptionSet<PlatformLayerTreeAsTextFlags> toPlatformLayerTreeFlags(unsigned short flags)
{
    OptionSet<PlatformLayerTreeAsTextFlags> platformLayerTreeFlags = { };
    if (flags & Internals::PLATFORM_LAYER_TREE_DEBUG)
        platformLayerTreeFlags.add(PlatformLayerTreeAsTextFlags::Debug);
    if (flags & Internals::PLATFORM_LAYER_TREE_IGNORES_CHILDREN)
        platformLayerTreeFlags.add(PlatformLayerTreeAsTextFlags::IgnoreChildren);
    if (flags & Internals::PLATFORM_LAYER_TREE_INCLUDE_MODELS)
        platformLayerTreeFlags.add(PlatformLayerTreeAsTextFlags::IncludeModels);
    return platformLayerTreeFlags;
}

ExceptionOr<String> Internals::platformLayerTreeAsText(Element& element, unsigned short flags) const
{
    Document& document = element.document();
    if (!document.frame() || !document.frame()->contentRenderer())
        return Exception { ExceptionCode::InvalidAccessError };

    auto text = document.frame()->contentRenderer()->compositor().platformLayerTreeAsText(element, toPlatformLayerTreeFlags(flags));
    if (!text)
        return Exception { ExceptionCode::NotFoundError };

    return String { text.value() };
}

ExceptionOr<String> Internals::repaintRectsAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->frame()->trackedRepaintRectsAsText();
}

ExceptionOr<ScrollableArea*> Internals::scrollableAreaForNode(Node* node) const
{
    if (!node)
        node = contextDocument();

    if (!node)
        return Exception { ExceptionCode::InvalidAccessError };

    Ref nodeRef { *node };
    nodeRef->document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    ScrollableArea* scrollableArea = nullptr;
    if (is<Document>(nodeRef)) {
        auto* frameView = downcast<Document>(nodeRef.get()).view();
        if (!frameView)
            return Exception { ExceptionCode::InvalidAccessError };

        scrollableArea = frameView;
    } else if (node == nodeRef->document().scrollingElement()) {
        auto* frameView = nodeRef->document().view();
        if (!frameView)
            return Exception { ExceptionCode::InvalidAccessError };

        scrollableArea = frameView;
    } else if (is<Element>(nodeRef)) {
        auto& element = downcast<Element>(nodeRef.get());
        if (!element.renderBox())
            return Exception { ExceptionCode::InvalidAccessError };

        auto& renderBox = *element.renderBox();
        if (is<RenderListBox>(renderBox))
            scrollableArea = &downcast<RenderListBox>(renderBox);
        else {
            ASSERT(renderBox.layer());
            scrollableArea = renderBox.layer()->scrollableArea();
        }
    } else
        return Exception { ExceptionCode::InvalidNodeTypeError };

    if (!scrollableArea)
        return Exception { ExceptionCode::InvalidNodeTypeError };

    return scrollableArea;
}

ExceptionOr<String> Internals::scrollbarOverlayStyle(Node* node) const
{
    auto areaOrException = scrollableAreaForNode(node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    switch (scrollableArea->scrollbarOverlayStyle()) {
    case ScrollbarOverlayStyleDefault:
        return "default"_str;
    case ScrollbarOverlayStyleDark:
        return "dark"_str;
    case ScrollbarOverlayStyleLight:
        return "light"_str;
    }

    ASSERT_NOT_REACHED();
    return "unknown"_str;
}

ExceptionOr<bool> Internals::scrollbarUsingDarkAppearance(Node* node) const
{
    auto areaOrException = scrollableAreaForNode(node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    return scrollableArea->useDarkAppearance();
}

ExceptionOr<String> Internals::horizontalScrollbarState(Node* node) const
{
    auto areaOrException = scrollableAreaForNode(node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    return scrollableArea->horizontalScrollbarStateForTesting();
}

ExceptionOr<String> Internals::verticalScrollbarState(Node* node) const
{
    auto areaOrException = scrollableAreaForNode(node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    return scrollableArea->verticalScrollbarStateForTesting();
}

static String scrollbarsControllerTypeString(ScrollbarsController& controller)
{
#if PLATFORM(MAC)
    if (is<ScrollbarsControllerMac>(controller))
        return "ScrollbarsControllerMac"_s;
#endif
    if (is<ScrollbarsControllerMock>(controller))
        return "ScrollbarsControllerMock"_s;
    return "RemoteScrollbarsController"_s;
}

ExceptionOr<String> Internals::scrollbarsControllerTypeForNode(Node* node) const
{
    auto areaOrException = scrollableAreaForNode(node);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    return scrollbarsControllerTypeString(scrollableArea->scrollbarsController());
}

ExceptionOr<String> Internals::scrollingStateTreeAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    Page* page = document->page();
    if (!page)
        return String();

    return page->scrollingStateTreeAsText();
}

ExceptionOr<String> Internals::scrollingTreeAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    auto page = document->page();
    if (!page)
        return String();

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return String();

    scrollingCoordinator->commitTreeStateIfNeeded();
    return scrollingCoordinator->scrollingTreeAsText();
}

ExceptionOr<bool> Internals::haveScrollingTree() const
{
    auto* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    auto page = document->page();
    if (!page)
        return false;

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return false;

    return scrollingCoordinator->haveScrollingTree();
}

ExceptionOr<String> Internals::synchronousScrollingReasons() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    Page* page = document->page();
    if (!page)
        return String();

    return page->synchronousScrollingReasonsAsText();
}

ExceptionOr<Ref<DOMRectList>> Internals::nonFastScrollableRects() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    Page* page = document->page();
    if (!page)
        return DOMRectList::create();

    return page->nonFastScrollableRectsForTesting();
}

ExceptionOr<void> Internals::setElementUsesDisplayListDrawing(Element& element, bool usesDisplayListDrawing)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!element.renderer())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!element.renderer()->hasLayer())
        return Exception { ExceptionCode::InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { ExceptionCode::InvalidAccessError };

    layer->backing()->setUsesDisplayListDrawing(usesDisplayListDrawing);
    return { };
}

ExceptionOr<void> Internals::setElementTracksDisplayListReplay(Element& element, bool isTrackingReplay)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!element.renderer())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!element.renderer()->hasLayer())
        return Exception { ExceptionCode::InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { ExceptionCode::InvalidAccessError };

    layer->backing()->setIsTrackingDisplayListReplay(isTrackingReplay);
    return { };
}

static OptionSet<DisplayList::AsTextFlag> toDisplayListFlags(unsigned short flags)
{
    OptionSet<DisplayList::AsTextFlag> displayListFlags;
    if (flags & Internals::DISPLAY_LIST_INCLUDE_PLATFORM_OPERATIONS)
        displayListFlags.add(DisplayList::AsTextFlag::IncludePlatformOperations);
    if (flags & Internals::DISPLAY_LIST_INCLUDE_RESOURCE_IDENTIFIERS)
        displayListFlags.add(DisplayList::AsTextFlag::IncludeResourceIdentifiers);
    return displayListFlags;
}

ExceptionOr<String> Internals::displayListForElement(Element& element, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!element.renderer())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!element.renderer()->hasLayer())
        return Exception { ExceptionCode::InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { ExceptionCode::InvalidAccessError };

    return layer->backing()->displayListAsText(toDisplayListFlags(flags));
}

ExceptionOr<String> Internals::replayDisplayListForElement(Element& element, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!element.renderer())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!element.renderer()->hasLayer())
        return Exception { ExceptionCode::InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { ExceptionCode::InvalidAccessError };

    return layer->backing()->replayDisplayListAsText(toDisplayListFlags(flags));
}

void Internals::setForceUseGlyphDisplayListForTesting(bool enabled)
{
    TextPainter::setForceUseGlyphDisplayListForTesting(enabled);
}

void Internals::clearGlyphDisplayListCacheForTesting()
{
    TextPainter::clearGlyphDisplayListCacheForTesting();
}

ExceptionOr<String> Internals::cachedGlyphDisplayListsForTextNode(Node& node, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!is<Text>(node))
        return Exception { ExceptionCode::InvalidAccessError };

    node.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (!node.renderer())
        return Exception { ExceptionCode::InvalidAccessError };

    return TextPainter::cachedGlyphDisplayListsForTextNodeAsText(downcast<Text>(node), toDisplayListFlags(flags));
}

ExceptionOr<void> Internals::garbageCollectDocumentResources() const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };
    document->cachedResourceLoader().garbageCollectDocumentResources();
    return { };
}

bool Internals::isUnderMemoryWarning()
{
    return MemoryPressureHandler::singleton().isUnderMemoryWarning();
}

bool Internals::isUnderMemoryPressure()
{
    return MemoryPressureHandler::singleton().isUnderMemoryPressure();
}

void Internals::beginSimulatedMemoryWarning()
{
    MemoryPressureHandler::singleton().beginSimulatedMemoryWarning();
}

void Internals::endSimulatedMemoryWarning()
{
    MemoryPressureHandler::singleton().endSimulatedMemoryWarning();
}

void Internals::beginSimulatedMemoryPressure()
{
    MemoryPressureHandler::singleton().beginSimulatedMemoryPressure();
}

void Internals::endSimulatedMemoryPressure()
{
    MemoryPressureHandler::singleton().endSimulatedMemoryPressure();
}

ExceptionOr<void> Internals::insertAuthorCSS(const String& css) const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    auto parsedSheet = StyleSheetContents::create(*document);
    parsedSheet.get().setIsUserStyleSheet(false);
    parsedSheet.get().parseString(css);
    document->extensionStyleSheets().addAuthorStyleSheetForTesting(WTFMove(parsedSheet));
    return { };
}

ExceptionOr<void> Internals::insertUserCSS(const String& css) const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    auto parsedSheet = StyleSheetContents::create(*document);
    parsedSheet.get().setIsUserStyleSheet(true);
    parsedSheet.get().parseString(css);
    document->extensionStyleSheets().addUserStyleSheet(WTFMove(parsedSheet));
    return { };
}

String Internals::counterValue(Element& element)
{
    return counterValueForElement(&element);
}

int Internals::pageNumber(Element& element, float pageWidth, float pageHeight)
{
    return PrintContext::pageNumberForElement(&element, { pageWidth, pageHeight });
}

Vector<String> Internals::shortcutIconURLs() const
{
    if (!frame())
        return { };
    
    auto* documentLoader = frame()->loader().documentLoader();
    if (!documentLoader)
        return { };

    Vector<String> result;
    for (auto& linkIcon : documentLoader->linkIcons())
        result.append(linkIcon.url.string());
    
    return result;
}

int Internals::numberOfPages(float pageWidth, float pageHeight)
{
    if (!frame())
        return -1;

    return PrintContext::numberOfPages(*frame(), FloatSize(pageWidth, pageHeight));
}

ExceptionOr<String> Internals::pageProperty(const String& propertyName, int pageNumber) const
{
    if (!frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return PrintContext::pageProperty(frame(), propertyName.utf8().data(), pageNumber);
}

ExceptionOr<String> Internals::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    if (!frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return PrintContext::pageSizeAndMarginsInPixels(frame(), pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft);
}

ExceptionOr<float> Internals::pageScaleFactor() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->pageScaleFactor();
}

ExceptionOr<void> Internals::setPageZoomFactor(float zoomFactor)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->frame()->setPageZoomFactor(zoomFactor);
    return { };
}

ExceptionOr<void> Internals::setTextZoomFactor(float zoomFactor)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->frame()->setTextZoomFactor(zoomFactor);
    return { };
}

ExceptionOr<void> Internals::setUseFixedLayout(bool useFixedLayout)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setUseFixedLayout(useFixedLayout);
    return { };
}

ExceptionOr<void> Internals::setFixedLayoutSize(int width, int height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setFixedLayoutSize(IntSize(width, height));
    return { };
}

ExceptionOr<void> Internals::setViewExposedRect(float x, float y, float width, float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setViewExposedRect(FloatRect(x, y, width, height));
    return { };
}

void Internals::setPrinting(int width, int height)
{
    printContextForTesting() = makeUnique<PrintContext>(frame());
    printContextForTesting()->begin(width, height);
}

void Internals::setHeaderHeight(float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;

    document->page()->setHeaderHeight(height);
}

void Internals::setFooterHeight(float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;

    document->page()->setFooterHeight(height);
}

#if ENABLE(FULLSCREEN_API)

void Internals::webkitWillEnterFullScreenForElement(Element& element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->fullscreenManager().willEnterFullscreen(element);
}

void Internals::webkitDidEnterFullScreenForElement(Element&)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->fullscreenManager().didEnterFullscreen();
}

void Internals::webkitWillExitFullScreenForElement(Element&)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->fullscreenManager().willExitFullscreen();
}

void Internals::webkitDidExitFullScreenForElement(Element&)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->fullscreenManager().didExitFullscreen();
}

bool Internals::isAnimatingFullScreen() const
{
    Document* document = contextDocument();
    if (!document)
        return false;
    return document->fullscreenManager().isAnimatingFullscreen();
}

#endif

void Internals::setFullscreenInsets(FullscreenInsets insets)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    page->setFullscreenInsets(FloatBoxExtent(insets.top, insets.right, insets.bottom, insets.left));
}

void Internals::setFullscreenAutoHideDuration(double duration)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    page->setFullscreenAutoHideDuration(Seconds(duration));
}

#if ENABLE(VIDEO)
bool Internals::isChangingPresentationMode(HTMLVideoElement& element) const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return element.isChangingPresentationMode();
#else
    UNUSED_PARAM(element);
    return false;
#endif
}
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
void Internals::setMockVideoPresentationModeEnabled(bool enabled)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;

    document->page()->chrome().client().setMockVideoPresentationModeEnabled(enabled);
}
#endif

void Internals::setCanvasNoiseInjectionSalt(HTMLCanvasElement& element, unsigned long long salt)
{
    element.setNoiseInjectionSalt(salt);
}

bool Internals::doesCanvasHavePendingCanvasNoiseInjection(HTMLCanvasElement& element) const
{
    return element.havePendingCanvasNoiseInjection();
}

void Internals::setApplicationCacheOriginQuota(unsigned long long quota)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;
    if (auto* applicationCacheStorage = document->page()->applicationCacheStorage())
        applicationCacheStorage->storeUpdatedQuotaForOrigin(&document->securityOrigin(), quota);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    LegacySchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    LegacySchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(scheme);
}

void Internals::registerDefaultPortForProtocol(unsigned short port, const String& protocol)
{
    registerDefaultPortForProtocolForTesting(port, protocol);
}

Ref<MallocStatistics> Internals::mallocStatistics() const
{
    return MallocStatistics::create();
}

Ref<TypeConversions> Internals::typeConversions() const
{
    return TypeConversions::create();
}

Ref<MemoryInfo> Internals::memoryInfo() const
{
    return MemoryInfo::create();
}

Vector<String> Internals::getReferencedFilePaths() const
{
    frame()->history().saveDocumentAndScrollState();
    return FormController::referencedFilePaths(frame()->history().currentItem()->documentState());
}

ExceptionOr<void> Internals::startTrackingRepaints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setTracksRepaints(true);
    return { };
}

ExceptionOr<void> Internals::stopTrackingRepaints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->setTracksRepaints(false);
    return { };
}

ExceptionOr<void> Internals::startTrackingLayerFlushes()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    document->renderView()->compositor().startTrackingLayerFlushes();
    return { };
}

ExceptionOr<unsigned> Internals::layerFlushCount()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->renderView()->compositor().layerFlushCount();
}

ExceptionOr<void> Internals::startTrackingStyleRecalcs()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    document->startTrackingStyleRecalcs();
    return { };
}

ExceptionOr<unsigned> Internals::styleRecalcCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    return document->styleRecalcCount();
}

unsigned Internals::lastStyleUpdateSize() const
{
    Document* document = contextDocument();
    if (!document)
        return 0;
    return document->lastStyleUpdateSizeForTesting();
}

ExceptionOr<void> Internals::startTrackingLayoutUpdates()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->startTrackingLayoutUpdates();
    return { };
}

ExceptionOr<unsigned> Internals::layoutUpdateCount()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->view()->layoutUpdateCount();
}

ExceptionOr<void> Internals::startTrackingRenderLayerPositionUpdates()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    document->view()->startTrackingRenderLayerPositionUpdates();
    return { };
}

ExceptionOr<unsigned> Internals::renderLayerPositionUpdateCount()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->view()->renderLayerPositionUpdateCount();
}

ExceptionOr<void> Internals::startTrackingCompositingUpdates()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    document->renderView()->compositor().startTrackingCompositingUpdates();
    return { };
}

ExceptionOr<unsigned> Internals::compositingUpdateCount()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { ExceptionCode::InvalidAccessError };

    return document->renderView()->compositor().compositingUpdateCount();
}

ExceptionOr<void> Internals::startTrackingRenderingUpdates()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    document->page()->startTrackingRenderingUpdates();
    return { };
}

ExceptionOr<unsigned> Internals::renderingUpdateCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    return document->page()->renderingUpdateCount();
}

ExceptionOr<void> Internals::setCompositingPolicyOverride(std::optional<CompositingPolicy> policyOverride)
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    if (!policyOverride) {
        document->page()->setCompositingPolicyOverride(std::nullopt);
        return { };
    }

    switch (policyOverride.value()) {
    case Internals::CompositingPolicy::Normal:
        document->page()->setCompositingPolicyOverride(WebCore::CompositingPolicy::Normal);
        break;
    case Internals::CompositingPolicy::Conservative:
        document->page()->setCompositingPolicyOverride(WebCore::CompositingPolicy::Conservative);
        break;
    }
    
    return { };
}

ExceptionOr<std::optional<Internals::CompositingPolicy>> Internals::compositingPolicyOverride() const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };

    auto policyOverride = document->page()->compositingPolicyOverride();
    if (!policyOverride)
        return { std::nullopt };

    switch (policyOverride.value()) {
    case WebCore::CompositingPolicy::Normal:
        return { Internals::CompositingPolicy::Normal };
    case WebCore::CompositingPolicy::Conservative:
        return { Internals::CompositingPolicy::Conservative };
    }

    return { Internals::CompositingPolicy::Normal };
}

void Internals::updateLayoutAndStyleForAllFrames() const
{
    auto* document = contextDocument();
    if (!document || !document->view())
        return;
    document->view()->updateLayoutAndStyleIfNeededRecursive();
}

ExceptionOr<void> Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(Node* node)
{
    Document* document;
    if (!node)
        document = contextDocument();
    else if (is<Document>(*node))
        document = downcast<Document>(node);
    else if (is<HTMLIFrameElement>(*node))
        document = downcast<HTMLIFrameElement>(*node).contentDocument();
    else
        return Exception { ExceptionCode::TypeError };

    document->updateLayout({ LayoutOptions::IgnorePendingStylesheets, LayoutOptions::RunPostLayoutTasksSynchronously });
    document->flushDeferredAXObjectCacheUpdate();

    return { };
}

#if !PLATFORM(IOS_FAMILY)
static ASCIILiteral cursorTypeToString(Cursor::Type cursorType)
{
    switch (cursorType) {
    case Cursor::Type::Pointer: return "Pointer"_s;
    case Cursor::Type::Cross: return "Cross"_s;
    case Cursor::Type::Hand: return "Hand"_s;
    case Cursor::Type::IBeam: return "IBeam"_s;
    case Cursor::Type::Wait: return "Wait"_s;
    case Cursor::Type::Help: return "Help"_s;
    case Cursor::Type::EastResize: return "EastResize"_s;
    case Cursor::Type::NorthResize: return "NorthResize"_s;
    case Cursor::Type::NorthEastResize: return "NorthEastResize"_s;
    case Cursor::Type::NorthWestResize: return "NorthWestResize"_s;
    case Cursor::Type::SouthResize: return "SouthResize"_s;
    case Cursor::Type::SouthEastResize: return "SouthEastResize"_s;
    case Cursor::Type::SouthWestResize: return "SouthWestResize"_s;
    case Cursor::Type::WestResize: return "WestResize"_s;
    case Cursor::Type::NorthSouthResize: return "NorthSouthResize"_s;
    case Cursor::Type::EastWestResize: return "EastWestResize"_s;
    case Cursor::Type::NorthEastSouthWestResize: return "NorthEastSouthWestResize"_s;
    case Cursor::Type::NorthWestSouthEastResize: return "NorthWestSouthEastResize"_s;
    case Cursor::Type::ColumnResize: return "ColumnResize"_s;
    case Cursor::Type::RowResize: return "RowResize"_s;
    case Cursor::Type::MiddlePanning: return "MiddlePanning"_s;
    case Cursor::Type::EastPanning: return "EastPanning"_s;
    case Cursor::Type::NorthPanning: return "NorthPanning"_s;
    case Cursor::Type::NorthEastPanning: return "NorthEastPanning"_s;
    case Cursor::Type::NorthWestPanning: return "NorthWestPanning"_s;
    case Cursor::Type::SouthPanning: return "SouthPanning"_s;
    case Cursor::Type::SouthEastPanning: return "SouthEastPanning"_s;
    case Cursor::Type::SouthWestPanning: return "SouthWestPanning"_s;
    case Cursor::Type::WestPanning: return "WestPanning"_s;
    case Cursor::Type::Move: return "Move"_s;
    case Cursor::Type::VerticalText: return "VerticalText"_s;
    case Cursor::Type::Cell: return "Cell"_s;
    case Cursor::Type::ContextMenu: return "ContextMenu"_s;
    case Cursor::Type::Alias: return "Alias"_s;
    case Cursor::Type::Progress: return "Progress"_s;
    case Cursor::Type::NoDrop: return "NoDrop"_s;
    case Cursor::Type::Copy: return "Copy"_s;
    case Cursor::Type::None: return "None"_s;
    case Cursor::Type::NotAllowed: return "NotAllowed"_s;
    case Cursor::Type::ZoomIn: return "ZoomIn"_s;
    case Cursor::Type::ZoomOut: return "ZoomOut"_s;
    case Cursor::Type::Grab: return "Grab"_s;
    case Cursor::Type::Grabbing: return "Grabbing"_s;
    case Cursor::Type::Custom: return "Custom"_s;
    case Cursor::Type::Invalid: break;
    }

    ASSERT_NOT_REACHED();
    return "UNKNOWN"_s;
}
#endif

ExceptionOr<String> Internals::getCurrentCursorInfo()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

#if !PLATFORM(IOS_FAMILY)
    Cursor cursor = document->frame()->eventHandler().currentMouseCursor();

    StringBuilder result;
    result.append("type="_s, cursorTypeToString(cursor.type()), " hotSpot="_s, cursor.hotSpot().x(), ',', cursor.hotSpot().y());
    if (cursor.image()) {
        FloatSize size = cursor.image()->size();
        result.append(" image="_s, size.width(), 'x', size.height());
    }
#if ENABLE(MOUSE_CURSOR_SCALE)
    if (cursor.imageScaleFactor() != 1)
        result.append(" scale="_s, cursor.imageScaleFactor());
#endif
    return result.toString();
#else
    return "FAIL: Cursor details not available on this platform."_str;
#endif
}

Ref<ArrayBuffer> Internals::serializeObject(const RefPtr<SerializedScriptValue>& value) const
{
    return ArrayBuffer::create(value->wireBytes());
}

Ref<SerializedScriptValue> Internals::deserializeBuffer(ArrayBuffer& buffer) const
{
    return SerializedScriptValue::createFromWireBytes(buffer.toVector());
}

bool Internals::isFromCurrentWorld(JSC::JSValue value) const
{
    JSC::VM& vm = contextDocument()->vm();
    return isWorldCompatible(*vm.topCallFrame->lexicalGlobalObject(vm), value);
}

JSC::JSValue Internals::evaluateInWorldIgnoringException(const String& name, const String& source)
{
    auto* document = contextDocument();
    auto& scriptController = document->frame()->script();
    auto world = ScriptController::createWorld(name);
    return scriptController.executeScriptInWorldIgnoringException(world, source, JSC::SourceTaintedOrigin::Untainted);
}

#if !PLATFORM(MAC)
void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(enabled);
}
#endif

void Internals::forceAXObjectCacheUpdate() const
{
    if (RefPtr document = contextDocument())
        document->axObjectCache()->performDeferredCacheUpdate(ForceLayout::Yes);
}

void Internals::forceReload(bool endToEnd)
{
    OptionSet<ReloadOption> reloadOptions;
    if (endToEnd)
        reloadOptions.add(ReloadOption::FromOrigin);

    frame()->loader().reload(reloadOptions);
}

void Internals::reloadExpiredOnly()
{
    frame()->loader().reload(ReloadOption::ExpiredOnly);
}

void Internals::enableFixedWidthAutoSizeMode(bool enabled, int width, int height)
{
    auto* document = contextDocument();
    if (!document || !document->view())
        return;
    document->view()->enableFixedWidthAutoSizeMode(enabled, { width, height });
}

void Internals::enableSizeToContentAutoSizeMode(bool enabled, int width, int height)
{
    auto* document = contextDocument();
    if (!document || !document->view())
        return;
    document->view()->enableSizeToContentAutoSizeMode(enabled, { width, height });
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

void Internals::initializeMockCDM()
{
    LegacyCDM::registerCDMFactory([] (LegacyCDM& cdm) { return makeUniqueWithoutRefCountedCheck<LegacyMockCDM>(cdm); },
        LegacyMockCDM::supportsKeySystem, LegacyMockCDM::supportsKeySystemAndMimeType);
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)

Ref<MockCDMFactory> Internals::registerMockCDM()
{
    return MockCDMFactory::create();
}

#endif

String Internals::markerTextForListItem(Element& element)
{
    return WebCore::markerTextForListItem(&element);
}

String Internals::toolTipFromElement(Element& element) const
{
    HitTestResult result;
    result.setInnerNode(&element);
    TextDirection direction;
    auto title = result.title(direction);
    return !title.isEmpty() ? title : result.innerTextIfTruncated(direction);
}

String Internals::getImageSourceURL(Element& element)
{
    return element.imageSourceURL();
}

#if ENABLE(VIDEO)

unsigned Internals::mediaElementCount()
{
    Document* document = contextDocument();
    if (!document)
        return 0;

    unsigned number = 0;
    for (auto& mediaElement : HTMLMediaElement::allMediaElements()) {
        if (&mediaElement->document() == document)
            ++number;
    }

    return number;
}

Vector<String> Internals::mediaResponseSources(HTMLMediaElement& media)
{
    auto* resourceLoader = media.lastMediaResourceLoaderForTesting();
    if (!resourceLoader)
        return { };
    Vector<String> result;
    auto responses = resourceLoader->responsesForTesting();
    for (auto& response : responses)
        result.append(responseSourceToString(response));
    return result;
}

Vector<String> Internals::mediaResponseContentRanges(HTMLMediaElement& media)
{
    auto* resourceLoader = media.lastMediaResourceLoaderForTesting();
    if (!resourceLoader)
        return { };
    Vector<String> result;
    auto responses = resourceLoader->responsesForTesting();
    for (auto& response : responses)
        result.append(response.httpHeaderField(HTTPHeaderName::ContentRange));
    return result;
}

void Internals::simulateAudioInterruption(HTMLMediaElement& element)
{
#if USE(GSTREAMER)
    element.protectedPlayer()->simulateAudioInterruption();
#else
    UNUSED_PARAM(element);
#endif
}

ExceptionOr<bool> Internals::mediaElementHasCharacteristic(HTMLMediaElement& element, const String& characteristic)
{
    if (equalLettersIgnoringASCIICase(characteristic, "audible"_s))
        return element.hasAudio();
    if (equalLettersIgnoringASCIICase(characteristic, "visual"_s))
        return element.hasVideo();
    if (equalLettersIgnoringASCIICase(characteristic, "legible"_s))
        return element.hasClosedCaptions();

    return Exception { ExceptionCode::SyntaxError };
}

void Internals::beginSimulatedHDCPError(HTMLMediaElement& element)
{
    if (RefPtr player = element.player())
        player->beginSimulatedHDCPError();
}

void Internals::endSimulatedHDCPError(HTMLMediaElement& element)
{
    if (RefPtr player = element.player())
        player->endSimulatedHDCPError();
}

ExceptionOr<bool> Internals::mediaPlayerRenderingCanBeAccelerated(HTMLMediaElement& element)
{
    return element.mediaPlayerRenderingCanBeAccelerated();
}

bool Internals::elementShouldBufferData(HTMLMediaElement& element)
{
    return element.bufferingPolicy() < MediaPlayer::BufferingPolicy::LimitReadAhead;
}

String Internals::elementBufferingPolicy(HTMLMediaElement& element)
{
    switch (element.bufferingPolicy()) {
    case MediaPlayer::BufferingPolicy::Default:
        return "Default"_s;
    case MediaPlayer::BufferingPolicy::LimitReadAhead:
        return "LimitReadAhead"_s;
    case MediaPlayer::BufferingPolicy::MakeResourcesPurgeable:
        return "MakeResourcesPurgeable"_s;
    case MediaPlayer::BufferingPolicy::PurgeResources:
        return "PurgeResources"_s;
    }

    ASSERT_NOT_REACHED();
    return "UNKNOWN"_s;
}

void Internals::setMediaElementBufferingPolicy(HTMLMediaElement& element, const String& policy)
{
    if (policy == "Default"_s) {
        element.setBufferingPolicy(MediaPlayer::BufferingPolicy::Default);
        return;
    }
    if (policy == "LimitReadAhead"_s) {
        element.setBufferingPolicy(MediaPlayer::BufferingPolicy::LimitReadAhead);
        return;
    }
    if (policy == "MakeResourcesPurgeable"_s) {
        element.setBufferingPolicy(MediaPlayer::BufferingPolicy::MakeResourcesPurgeable);
        return;
    }
    if (policy == "PurgeResources"_s) {
        element.setBufferingPolicy(MediaPlayer::BufferingPolicy::PurgeResources);
        return;
    }
    ASSERT_NOT_REACHED();
}

ExceptionOr<void> Internals::setOverridePreferredDynamicRangeMode(HTMLMediaElement& element, const String& modeString)
{
    DynamicRangeMode mode;
    if (modeString == "None"_s)
        mode = DynamicRangeMode::None;
    else if (modeString == "Standard"_s)
        mode = DynamicRangeMode::Standard;
    else if (modeString == "HLG"_s)
        mode = DynamicRangeMode::HLG;
    else if (modeString == "HDR10"_s)
        mode = DynamicRangeMode::HDR10;
    else if (modeString == "DolbyVisionPQ"_s)
        mode = DynamicRangeMode::DolbyVisionPQ;
    else
        return Exception { ExceptionCode::SyntaxError };

    element.setOverridePreferredDynamicRangeMode(mode);
    return { };
}

void Internals::enableGStreamerHolePunching(HTMLVideoElement& element)
{
#if USE(GSTREAMER)
    element.enableGStreamerHolePunching();
#else
    UNUSED_PARAM(element);
#endif
}

#endif

bool Internals::isSelectPopupVisible(HTMLSelectElement& element)
{
    element.document().updateLayout(LayoutOptions::IgnorePendingStylesheets);

    auto* renderer = element.renderer();
    if (!is<RenderMenuList>(renderer))
        return false;

#if !PLATFORM(IOS_FAMILY)
    return downcast<RenderMenuList>(*renderer).popupIsVisible();
#else
    return false;
#endif
}

ExceptionOr<String> Internals::captionsStyleSheetOverride()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

#if ENABLE(VIDEO)
    return document->page()->group().ensureCaptionPreferences().captionsStyleSheetOverride();
#else
    return String { emptyString() };
#endif
}

ExceptionOr<void> Internals::setCaptionsStyleSheetOverride(const String& override)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

#if ENABLE(VIDEO)
    document->page()->group().ensureCaptionPreferences().setCaptionsStyleSheetOverride(override);
#else
    UNUSED_PARAM(override);
#endif
    return { };
}

ExceptionOr<void> Internals::setPrimaryAudioTrackLanguageOverride(const String& language)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

#if ENABLE(VIDEO)
    document->page()->group().ensureCaptionPreferences().setPrimaryAudioTrackLanguageOverride(language);
#else
    UNUSED_PARAM(language);
#endif
    return { };
}

ExceptionOr<void> Internals::setCaptionDisplayMode(const String& mode)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

#if ENABLE(VIDEO)
    auto& captionPreferences = document->page()->group().ensureCaptionPreferences();

    if (equalLettersIgnoringASCIICase(mode, "automatic"_s))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode::Automatic);
    else if (equalLettersIgnoringASCIICase(mode, "forcedonly"_s))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode::ForcedOnly);
    else if (equalLettersIgnoringASCIICase(mode, "alwayson"_s))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode::AlwaysOn);
    else if (equalLettersIgnoringASCIICase(mode, "manual"_s))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode::Manual);
    else
        return Exception { ExceptionCode::SyntaxError };
#else
    UNUSED_PARAM(mode);
#endif
    return { };
}

#if ENABLE(VIDEO)
RefPtr<TextTrackCueGeneric> Internals::createGenericCue(double startTime, double endTime, String text)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return nullptr;
    return TextTrackCueGeneric::create(*document, MediaTime::createWithDouble(startTime), MediaTime::createWithDouble(endTime), text);
}

ExceptionOr<String> Internals::textTrackBCP47Language(TextTrack& track)
{
    return String { track.validBCP47Language() };
}

Ref<TimeRanges> Internals::createTimeRanges(Float32Array& startTimes, Float32Array& endTimes)
{
    ASSERT(startTimes.length() == endTimes.length());
    Ref<TimeRanges> ranges = TimeRanges::create();

    unsigned count = std::min(startTimes.length(), endTimes.length());
    for (unsigned i = 0; i < count; ++i)
        ranges->add(startTimes.item(i), endTimes.item(i));
    return ranges;
}

double Internals::closestTimeToTimeRanges(double time, TimeRanges& ranges)
{
    return ranges.nearest(time);
}

#endif

ExceptionOr<Ref<DOMRect>> Internals::selectionBounds()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    return DOMRect::create(document->frame()->selection().selectionBounds());
}

ExceptionOr<RefPtr<StaticRange>> Internals::selectedRange()
{
    RefPtr document = contextDocument();
    if (!document)
        return Exception { ExceptionCode::InvalidAccessError };
    auto range = contextDocument()->selection().selection().range();
    if (!range)
        return nullptr;
    return RefPtr { StaticRange::create(*range) };
}

void Internals::setSelectionWithoutValidation(Ref<Node> baseNode, unsigned baseOffset, RefPtr<Node> extentNode, unsigned extentOffset)
{
    contextDocument()->frame()->selection().moveTo(
        VisiblePosition { makeDeprecatedLegacyPosition(baseNode.ptr(), baseOffset) },
        VisiblePosition { makeDeprecatedLegacyPosition(extentNode.get(), extentOffset) });
}

void Internals::setSelectionFromNone()
{
    contextDocument()->frame()->selection().setSelectionFromNone();
}

ExceptionOr<bool> Internals::isPluginUnavailabilityIndicatorObscured(Element& element)
{
    if (!is<HTMLPlugInElement>(element))
        return Exception { ExceptionCode::InvalidAccessError };

    return downcast<HTMLPlugInElement>(element).isReplacementObscured();
}

ExceptionOr<String> Internals::unavailablePluginReplacementText(Element& element)
{
    if (!is<HTMLPlugInElement>(element))
        return Exception { ExceptionCode::InvalidAccessError };

    auto* renderer = element.renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return String { };

    return String { downcast<RenderEmbeddedObject>(*renderer).pluginReplacementTextIfUnavailable() };
}

bool Internals::isPluginSnapshotted(Element&)
{
    return false;
}

#if ENABLE(MEDIA_SOURCE)

void Internals::initializeMockMediaSource()
{
    RefPtr document = contextDocument();
    if (!document || (!document->settings().mediaSourceEnabled() && !document->settings().managedMediaSourceEnabled()))
        return;

    platformStrategies()->mediaStrategy().enableMockMediaSource();
}

void Internals::setMaximumSourceBufferSize(SourceBuffer& buffer, uint64_t maximumSize, DOMPromiseDeferred<void>&& promise)
{
    buffer.setMaximumSourceBufferSize(maximumSize)->whenSettled(RunLoop::current(), [promise = WTFMove(promise)]() mutable {
        promise.resolve();
    });
}

void Internals::bufferedSamplesForTrackId(SourceBuffer& buffer, const AtomString& trackId, BufferedSamplesPromise&& promise)
{
    buffer.bufferedSamplesForTrackId(parseInteger<uint64_t>(trackId).value_or(0))->whenSettled(RunLoop::current(), [promise = WTFMove(promise)](auto&& samples) mutable {
        if (!samples) {
            promise.reject(Exception { ExceptionCode::OperationError, makeString("Error "_s, samples.error()) });
            return;
        }
        promise.resolve(WTFMove(*samples));
    });
}

void Internals::enqueuedSamplesForTrackID(SourceBuffer& buffer, const AtomString& trackID, BufferedSamplesPromise&& promise)
{
    buffer.enqueuedSamplesForTrackID(parseInteger<uint64_t>(trackID).value_or(0))->whenSettled(RunLoop::current(), [promise = WTFMove(promise)](auto&& samples) mutable {
        if (!samples) {
            promise.reject(Exception { ExceptionCode::OperationError, makeString("Error "_s, samples.error()) });
            return;
        }
        promise.resolve(WTFMove(*samples));
    });
}

double Internals::minimumUpcomingPresentationTimeForTrackID(SourceBuffer& buffer, const AtomString& trackID)
{
    return buffer.minimumUpcomingPresentationTimeForTrackID(parseInteger<TrackID>(trackID).value_or(0)).toDouble();
}

void Internals::setShouldGenerateTimestamps(SourceBuffer& buffer, bool flag)
{
    buffer.setShouldGenerateTimestamps(flag);
}

void Internals::setMaximumQueueDepthForTrackID(SourceBuffer& buffer, const AtomString& trackID, size_t maxQueueDepth)
{
    buffer.setMaximumQueueDepthForTrackID(parseInteger<TrackID>(trackID).value_or(0), maxQueueDepth);
}

size_t Internals::evictableSize(SourceBuffer& buffer)
{
    return buffer.evictableSize();
}

#endif

void Internals::enableMockMediaCapabilities()
{
    MediaEngineConfigurationFactory::enableMock();
}

#if ENABLE(VIDEO)

ExceptionOr<void> Internals::beginMediaSessionInterruption(const String& interruptionString)
{
    PlatformMediaSession::InterruptionType interruption = PlatformMediaSession::InterruptionType::SystemInterruption;

    if (equalLettersIgnoringASCIICase(interruptionString, "system"_s))
        interruption = PlatformMediaSession::InterruptionType::SystemInterruption;
    else if (equalLettersIgnoringASCIICase(interruptionString, "systemsleep"_s))
        interruption = PlatformMediaSession::InterruptionType::SystemSleep;
    else if (equalLettersIgnoringASCIICase(interruptionString, "enteringbackground"_s))
        interruption = PlatformMediaSession::InterruptionType::EnteringBackground;
    else if (equalLettersIgnoringASCIICase(interruptionString, "suspendedunderlock"_s))
        interruption = PlatformMediaSession::InterruptionType::SuspendedUnderLock;
    else
        return Exception { ExceptionCode::InvalidAccessError };

    PlatformMediaSessionManager::singleton().beginInterruption(interruption);
    return { };
}

void Internals::endMediaSessionInterruption(const String& flagsString)
{
    PlatformMediaSession::EndInterruptionFlags flags = PlatformMediaSession::EndInterruptionFlags::NoFlags;

    if (equalLettersIgnoringASCIICase(flagsString, "mayresumeplaying"_s))
        flags = PlatformMediaSession::EndInterruptionFlags::MayResumePlaying;

    PlatformMediaSessionManager::singleton().endInterruption(flags);
}

void Internals::applicationWillBecomeInactive()
{
    PlatformMediaSessionManager::singleton().applicationWillBecomeInactive();
}

void Internals::applicationDidBecomeActive()
{
    PlatformMediaSessionManager::singleton().applicationDidBecomeActive();
}

void Internals::applicationWillEnterForeground(bool suspendedUnderLock) const
{
    PlatformMediaSessionManager::singleton().applicationWillEnterForeground(suspendedUnderLock);
}

void Internals::applicationDidEnterBackground(bool suspendedUnderLock) const
{
    PlatformMediaSessionManager::singleton().applicationDidEnterBackground(suspendedUnderLock);
}

static PlatformMediaSession::MediaType mediaTypeFromString(const String& mediaTypeString)
{
    if (equalLettersIgnoringASCIICase(mediaTypeString, "video"_s))
        return PlatformMediaSession::MediaType::Video;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "audio"_s))
        return PlatformMediaSession::MediaType::Audio;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "videoaudio"_s))
        return PlatformMediaSession::MediaType::VideoAudio;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "webaudio"_s))
        return PlatformMediaSession::MediaType::WebAudio;

    return PlatformMediaSession::MediaType::None;
}

ExceptionOr<void> Internals::setMediaSessionRestrictions(const String& mediaTypeString, StringView restrictionsString)
{
    auto mediaType = mediaTypeFromString(mediaTypeString);
    if (mediaType == PlatformMediaSession::MediaType::None)
        return Exception { ExceptionCode::InvalidAccessError };

    auto restrictions = PlatformMediaSessionManager::singleton().restrictions(mediaType);
    PlatformMediaSessionManager::singleton().removeRestriction(mediaType, restrictions);

    restrictions = PlatformMediaSessionManager::NoRestrictions;

    for (StringView restrictionString : restrictionsString.split(',')) {
        if (equalLettersIgnoringASCIICase(restrictionString, "concurrentplaybacknotpermitted"_s))
            restrictions |= PlatformMediaSessionManager::ConcurrentPlaybackNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "backgroundprocessplaybackrestricted"_s))
            restrictions |= PlatformMediaSessionManager::BackgroundProcessPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "backgroundtabplaybackrestricted"_s))
            restrictions |= PlatformMediaSessionManager::BackgroundTabPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "interruptedplaybacknotpermitted"_s))
            restrictions |= PlatformMediaSessionManager::InterruptedPlaybackNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "inactiveprocessplaybackrestricted"_s))
            restrictions |= PlatformMediaSessionManager::InactiveProcessPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "suspendedunderlockplaybackrestricted"_s))
            restrictions |= PlatformMediaSessionManager::SuspendedUnderLockPlaybackRestricted;
    }
    PlatformMediaSessionManager::singleton().addRestriction(mediaType, restrictions);
    return { };
}

ExceptionOr<String> Internals::mediaSessionRestrictions(const String& mediaTypeString) const
{
    PlatformMediaSession::MediaType mediaType = mediaTypeFromString(mediaTypeString);
    if (mediaType == PlatformMediaSession::MediaType::None)
        return Exception { ExceptionCode::InvalidAccessError };

    PlatformMediaSessionManager::SessionRestrictions restrictions = PlatformMediaSessionManager::singleton().restrictions(mediaType);
    if (restrictions == PlatformMediaSessionManager::NoRestrictions)
        return String();

    StringBuilder builder;
    if (restrictions & PlatformMediaSessionManager::ConcurrentPlaybackNotPermitted)
        builder.append("concurrentplaybacknotpermitted"_s);
    if (restrictions & PlatformMediaSessionManager::BackgroundProcessPlaybackRestricted) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append("backgroundprocessplaybackrestricted"_s);
    }
    if (restrictions & PlatformMediaSessionManager::BackgroundTabPlaybackRestricted) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append("backgroundtabplaybackrestricted"_s);
    }
    if (restrictions & PlatformMediaSessionManager::InterruptedPlaybackNotPermitted) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append("interruptedplaybacknotpermitted"_s);
    }
    return builder.toString();
}

void Internals::setMediaElementRestrictions(HTMLMediaElement& element, StringView restrictionsString)
{
    MediaElementSession::BehaviorRestrictions restrictions = element.mediaSession().behaviorRestrictions();
    element.mediaSession().removeBehaviorRestriction(restrictions);

    restrictions = MediaElementSession::NoRestrictions;

    for (StringView restrictionString : restrictionsString.split(',')) {
        if (equalLettersIgnoringASCIICase(restrictionString, "norestrictions"_s))
            restrictions |= MediaElementSession::NoRestrictions;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforload"_s))
            restrictions |= MediaElementSession::RequireUserGestureForLoad;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforvideoratechange"_s))
            restrictions |= MediaElementSession::RequireUserGestureForVideoRateChange;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforfullscreen"_s))
            restrictions |= MediaElementSession::RequireUserGestureForFullscreen;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsenttoloadmedia"_s))
            restrictions |= MediaElementSession::RequirePageConsentToLoadMedia;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsenttoresumemedia"_s))
            restrictions |= MediaElementSession::RequirePageConsentToResumeMedia;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergesturetoshowplaybacktargetpicker"_s))
            restrictions |= MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker;
        if (equalLettersIgnoringASCIICase(restrictionString, "wirelessvideoplaybackdisabled"_s))
            restrictions |= MediaElementSession::WirelessVideoPlaybackDisabled;
#endif
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforaudioratechange"_s))
            restrictions |= MediaElementSession::RequireUserGestureForAudioRateChange;
        if (equalLettersIgnoringASCIICase(restrictionString, "autopreloadingnotpermitted"_s))
            restrictions |= MediaElementSession::AutoPreloadingNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "invisibleautoplaynotpermitted"_s))
            restrictions |= MediaElementSession::InvisibleAutoplayNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "overrideusergesturerequirementformaincontent"_s))
            restrictions |= MediaElementSession::OverrideUserGestureRequirementForMainContent;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergesturetocontrolcontrolsmanager"_s))
            restrictions |= MediaElementSession::RequireUserGestureToControlControlsManager;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireplaybackTocontrolcontrolsmanager"_s))
            restrictions |= MediaElementSession::RequirePlaybackToControlControlsManager;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforvideoduetolowpowermode"_s))
            restrictions |= MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepagevisibilitytoplayaudio"_s))
            restrictions |= MediaElementSession::RequirePageVisibilityToPlayAudio;
    }
    element.mediaSession().addBehaviorRestriction(restrictions);
}

ExceptionOr<void> Internals::postRemoteControlCommand(const String& commandString, float argument)
{
    PlatformMediaSession::RemoteControlCommandType command;
    PlatformMediaSession::RemoteCommandArgument parameter { argument, { } };

    if (equalLettersIgnoringASCIICase(commandString, "play"_s))
        command = PlatformMediaSession::RemoteControlCommandType::PlayCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "pause"_s))
        command = PlatformMediaSession::RemoteControlCommandType::PauseCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "stop"_s))
        command = PlatformMediaSession::RemoteControlCommandType::StopCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "toggleplaypause"_s))
        command = PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "beginseekingbackward"_s))
        command = PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "endseekingbackward"_s))
        command = PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "beginseekingforward"_s))
        command = PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "endseekingforward"_s))
        command = PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "seektoplaybackposition"_s))
        command = PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "beginscrubbing"_s))
        command = PlatformMediaSession::RemoteControlCommandType::BeginScrubbingCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "endscrubbing"_s))
        command = PlatformMediaSession::RemoteControlCommandType::EndScrubbingCommand;
    else
        return Exception { ExceptionCode::InvalidAccessError };

    PlatformMediaSessionManager::singleton().processDidReceiveRemoteControlCommand(command, parameter);
    return { };
}

void Internals::activeAudioRouteDidChange(bool shouldPause)
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    MediaSessionHelper::sharedHelper().activeAudioRouteDidChange(shouldPause ? MediaSessionHelperClient::ShouldPause::Yes : MediaSessionHelperClient::ShouldPause::No);
#else
    UNUSED_PARAM(shouldPause);
#endif
}

bool Internals::elementIsBlockingDisplaySleep(const HTMLMediaElement& element) const
{
    return element.isDisablingSleep();
}

bool Internals::isPlayerVisibleInViewport(const HTMLMediaElement& element) const
{
    RefPtr player = element.player();
    return player && player->isVisibleInViewport();
}

bool Internals::isPlayerMuted(const HTMLMediaElement& element) const
{
    RefPtr player = element.player();
    return player && player->muted();
}

bool Internals::isPlayerPaused(const HTMLMediaElement& element) const
{
    RefPtr player = element.player();
    return player && player->paused();
}

void Internals::beginAudioSessionInterruption()
{
#if USE(AUDIO_SESSION)
    AudioSession::sharedSession().beginInterruptionForTesting();
#endif
}


void Internals::endAudioSessionInterruption()
{
#if USE(AUDIO_SESSION)
    AudioSession::sharedSession().endInterruptionForTesting();
#endif
}

void Internals::clearAudioSessionInterruptionFlag()
{
#if USE(AUDIO_SESSION)
    AudioSession::sharedSession().clearInterruptionFlagForTesting();
#endif
}

void Internals::suspendAllMediaBuffering()
{
    auto frame = this->frame();
    if (!frame)
        return;

    auto page = frame->page();
    if (!page)
        return;

    page->suspendAllMediaBuffering();
}

void Internals::suspendAllMediaPlayback()
{
    auto frame = this->frame();
    if (!frame)
        return;

    auto page = frame->page();
    if (!page)
        return;

    page->suspendAllMediaPlayback();
}

void Internals::resumeAllMediaPlayback()
{
    auto frame = this->frame();
    if (!frame)
        return;

    auto page = frame->page();
    if (!page)
        return;

    page->resumeAllMediaPlayback();
}

#endif // ENABLE(VIDEO)

#if ENABLE(WEB_AUDIO)
void Internals::setAudioContextRestrictions(AudioContext& context, StringView restrictionsString)
{
    auto restrictions = context.behaviorRestrictions();
    context.removeBehaviorRestriction(restrictions);

    restrictions = AudioContext::NoRestrictions;

    for (StringView restrictionString : restrictionsString.split(',')) {
        if (equalLettersIgnoringASCIICase(restrictionString, "norestrictions"_s))
            restrictions |= AudioContext::NoRestrictions;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforaudiostart"_s))
            restrictions |= AudioContext::RequireUserGestureForAudioStartRestriction;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsentforaudiostart"_s))
            restrictions |= AudioContext::RequirePageConsentForAudioStartRestriction;
    }
    context.addBehaviorRestriction(restrictions);
}

void Internals::useMockAudioDestinationCocoa()
{
#if PLATFORM(COCOA)
    AudioDestinationCocoa::createOverride = MockAudioDestinationCocoa::create;
#endif
}
#endif

void Internals::simulateSystemSleep() const
{
#if ENABLE(VIDEO)
    PlatformMediaSessionManager::singleton().processSystemWillSleep();
#endif
}

void Internals::simulateSystemWake() const
{
#if ENABLE(VIDEO)
    PlatformMediaSessionManager::singleton().processSystemDidWake();
#endif
}

std::optional<Internals::NowPlayingMetadata> Internals::nowPlayingMetadata() const
{
    if (auto nowPlayingInfo = PlatformMediaSessionManager::singleton().nowPlayingInfo())
        return nowPlayingInfo->metadata;
    return std::nullopt;
}

ExceptionOr<Internals::NowPlayingState> Internals::nowPlayingState() const
{
#if ENABLE(VIDEO)
    auto lastUpdatedNowPlayingInfoUniqueIdentifier = PlatformMediaSessionManager::singleton().lastUpdatedNowPlayingInfoUniqueIdentifier();
    return { { PlatformMediaSessionManager::singleton().lastUpdatedNowPlayingTitle(),
        PlatformMediaSessionManager::singleton().lastUpdatedNowPlayingDuration(),
        PlatformMediaSessionManager::singleton().lastUpdatedNowPlayingElapsedTime(),
        lastUpdatedNowPlayingInfoUniqueIdentifier ? lastUpdatedNowPlayingInfoUniqueIdentifier->toUInt64() : 0,
        PlatformMediaSessionManager::singleton().hasActiveNowPlayingSession(),
        PlatformMediaSessionManager::singleton().registeredAsNowPlayingApplication(),
        PlatformMediaSessionManager::singleton().haveEverRegisteredAsNowPlayingApplication()
    } };
#else
    return Exception { ExceptionCode::InvalidAccessError };
#endif
}

#if ENABLE(VIDEO)
RefPtr<HTMLMediaElement> Internals::bestMediaElementForRemoteControls(Internals::PlaybackControlsPurpose purpose)
{
    return HTMLMediaElement::bestMediaElementForRemoteControls(purpose);
}

Internals::MediaSessionState Internals::mediaSessionState(HTMLMediaElement& element)
{
    return static_cast<Internals::MediaSessionState>(element.mediaSession().state());
}
#endif

ExceptionOr<Internals::MediaUsageState> Internals::mediaUsageState(HTMLMediaElement& element) const
{
#if ENABLE(VIDEO)
    element.mediaSession().updateMediaUsageIfChanged();
    auto info = element.mediaSession().mediaUsageInfo();
    if (!info)
        return Exception { ExceptionCode::NotSupportedError };

    return { { info.value().mediaURL.string(),
        info.value().isPlaying,
        info.value().canShowControlsManager,
        info.value().canShowNowPlayingControls,
        info.value().isSuspended,
        info.value().isInActiveDocument,
        info.value().isFullscreen,
        info.value().isMuted,
        info.value().isMediaDocumentInMainFrame,
        info.value().isVideo,
        info.value().isAudio,
        info.value().hasVideo,
        info.value().hasAudio,
        info.value().hasRenderer,
        info.value().audioElementWithUserGesture,
        info.value().userHasPlayedAudioBefore,
        info.value().isElementRectMostlyInMainFrame,
        info.value().playbackPermitted,
        info.value().pageMediaPlaybackSuspended,
        info.value().isMediaDocumentAndNotOwnerElement,
        info.value().pageExplicitlyAllowsElementToAutoplayInline,
        info.value().requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted,
        info.value().isVideoAndRequiresUserGestureForVideoRateChange,
        info.value().isAudioAndRequiresUserGestureForAudioRateChange,
        info.value().isVideoAndRequiresUserGestureForVideoDueToLowPowerMode,
        info.value().noUserGestureRequired,
        info.value().requiresPlaybackAndIsNotPlaying,
        info.value().hasEverNotifiedAboutPlaying,
        info.value().outsideOfFullscreen,
        info.value().isLargeEnoughForMainContent,
    } };

#else
    UNUSED_PARAM(element);
    return Exception { ExceptionCode::InvalidAccessError };
#endif
}

ExceptionOr<bool> Internals::elementShouldDisplayPosterImage(HTMLVideoElement& element) const
{
#if ENABLE(VIDEO)
    return element.shouldDisplayPosterImage();
#else
    UNUSED_PARAM(element);
    return Exception { ExceptionCode::InvalidAccessError };
#endif
}

#if ENABLE(VIDEO)
size_t Internals::mediaElementCount() const
{
    return HTMLMediaElement::allMediaElements().size();
}

void Internals::setMediaElementVolumeLocked(HTMLMediaElement& element, bool volumeLocked)
{
    element.setVolumeLocked(volumeLocked);
}

#if ENABLE(SPEECH_SYNTHESIS)
ExceptionOr<RefPtr<SpeechSynthesisUtterance>> Internals::speechSynthesisUtteranceForCue(const VTTCue& cue)
{
    return cue.speechUtterance();
}

ExceptionOr<RefPtr<VTTCue>> Internals::mediaElementCurrentlySpokenCue(HTMLMediaElement& element)
{
    auto cue = element.cueBeingSpoken();
    ASSERT(is<VTTCue>(cue));
    if (!is<VTTCue>(cue))
        return Exception { ExceptionCode::InvalidAccessError };

    return downcast<VTTCue>(cue.get());
}
#endif

bool Internals::elementIsActiveNowPlayingSession(HTMLMediaElement& element) const
{
    return element.isActiveNowPlayingSession();
}

#endif // ENABLE(VIDEO)

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void Internals::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    auto frame = this->frame();
    if (!frame || !frame->page())
        return;

    frame->page()->setMockMediaPlaybackTargetPickerEnabled(enabled);
}

ExceptionOr<void> Internals::setMockMediaPlaybackTargetPickerState(const String& deviceName, const String& deviceState)
{
    auto frame = this->frame();
    if (!frame || !frame->page())
        return Exception { ExceptionCode::InvalidAccessError };

    MediaPlaybackTargetContext::MockState state = MediaPlaybackTargetContext::MockState::Unknown;

    if (equalLettersIgnoringASCIICase(deviceState, "deviceavailable"_s))
        state = MediaPlaybackTargetContext::MockState::OutputDeviceAvailable;
    else if (equalLettersIgnoringASCIICase(deviceState, "deviceunavailable"_s))
        state = MediaPlaybackTargetContext::MockState::OutputDeviceUnavailable;
    else if (equalLettersIgnoringASCIICase(deviceState, "unknown"_s))
        state = MediaPlaybackTargetContext::MockState::Unknown;
    else
        return Exception { ExceptionCode::InvalidAccessError };

    frame->page()->setMockMediaPlaybackTargetPickerState(deviceName, state);
    return { };
}

void Internals::mockMediaPlaybackTargetPickerDismissPopup()
{
    auto frame = this->frame();
    if (!frame || !frame->page())
        return;

    frame->page()->mockMediaPlaybackTargetPickerDismissPopup();
}
#endif

bool Internals::isMonitoringWirelessRoutes() const
{
#if ENABLE(VIDEO)
    return PlatformMediaSessionManager::singleton().isMonitoringWirelessTargets();
#else
    return false;
#endif // ENABLE(VIDEO)
}

ExceptionOr<Ref<MockPageOverlay>> Internals::installMockPageOverlay(PageOverlayType type)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    return MockPageOverlayClient::singleton().installOverlay(*document->page(), type == PageOverlayType::View ? PageOverlay::OverlayType::View : PageOverlay::OverlayType::Document);
}

ExceptionOr<String> Internals::pageOverlayLayerTreeAsText(unsigned short flags) const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    return MockPageOverlayClient::singleton().layerTreeAsText(*document->page(), toLayerTreeAsTextOptions(flags));
}

void Internals::setPageMuted(StringView statesString)
{
    Document* document = contextDocument();
    if (!document)
        return;

    WebCore::MediaProducerMutedStateFlags state;
    for (StringView stateString : statesString.split(',')) {
        if (equalLettersIgnoringASCIICase(stateString, "audio"_s))
            state.add(MediaProducerMutedState::AudioIsMuted);
        if (equalLettersIgnoringASCIICase(stateString, "capturedevices"_s))
            state.add(MediaProducer::AudioAndVideoCaptureIsMuted);
        if (equalLettersIgnoringASCIICase(stateString, "screencapture"_s))
            state.add(MediaProducerMutedState::ScreenCaptureIsMuted);
    }

    if (Page* page = document->page())
        page->setMuted(state);
}

String Internals::pageMediaState()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return emptyString();

    auto state = document->page()->mediaState();
    StringBuilder string;
    if (state.containsAny(MediaProducerMediaState::IsPlayingAudio))
        string.append("IsPlayingAudio,"_s);
    if (state.containsAny(MediaProducerMediaState::IsPlayingVideo))
        string.append("IsPlayingVideo,"_s);
    if (state.containsAny(MediaProducerMediaState::IsPlayingToExternalDevice))
        string.append("IsPlayingToExternalDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::RequiresPlaybackTargetMonitoring))
        string.append("RequiresPlaybackTargetMonitoring,"_s);
    if (state.containsAny(MediaProducerMediaState::ExternalDeviceAutoPlayCandidate))
        string.append("ExternalDeviceAutoPlayCandidate,"_s);
    if (state.containsAny(MediaProducerMediaState::DidPlayToEnd))
        string.append("DidPlayToEnd,"_s);
    if (state.containsAny(MediaProducerMediaState::IsSourceElementPlaying))
        string.append("IsSourceElementPlaying,"_s);

    if (state.containsAny(MediaProducerMediaState::IsNextTrackControlEnabled))
        string.append("IsNextTrackControlEnabled,"_s);
    if (state.containsAny(MediaProducerMediaState::IsPreviousTrackControlEnabled))
        string.append("IsPreviousTrackControlEnabled,"_s);

    if (state.containsAny(MediaProducerMediaState::HasPlaybackTargetAvailabilityListener))
        string.append("HasPlaybackTargetAvailabilityListener,"_s);
    if (state.containsAny(MediaProducerMediaState::HasAudioOrVideo))
        string.append("HasAudioOrVideo,"_s);

    if (state.containsAny(MediaProducerMediaState::HasActiveAudioCaptureDevice))
        string.append("HasActiveAudioCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasMutedAudioCaptureDevice))
        string.append("HasMutedAudioCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasInterruptedAudioCaptureDevice))
        string.append("HasInterruptedAudioCaptureDevice,"_s);

    if (state.containsAny(MediaProducerMediaState::HasActiveVideoCaptureDevice))
        string.append("HasActiveVideoCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasMutedVideoCaptureDevice))
        string.append("HasMutedVideoCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasInterruptedVideoCaptureDevice))
        string.append("HasInterruptedVideoCaptureDevice,"_s);

    if (state.containsAny(MediaProducerMediaState::HasUserInteractedWithMediaElement))
        string.append("HasUserInteractedWithMediaElement,"_s);

    if (state.containsAny(MediaProducerMediaState::HasActiveScreenCaptureDevice))
        string.append("HasActiveScreenCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasMutedScreenCaptureDevice))
        string.append("HasMutedScreenCaptureDevice,"_s);

    if (state.containsAny(MediaProducerMediaState::HasActiveWindowCaptureDevice))
        string.append("HasActiveWindowCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasMutedWindowCaptureDevice))
        string.append("HasMutedWindowCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasInterruptedWindowCaptureDevice))
        string.append("HasInterruptedWindowCaptureDevice,"_s);

    if (state.containsAny(MediaProducerMediaState::HasActiveSystemAudioCaptureDevice))
        string.append("HasActiveSystemAudioCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasMutedSystemAudioCaptureDevice))
        string.append("HasMutedSystemAudioCaptureDevice,"_s);
    if (state.containsAny(MediaProducerMediaState::HasInterruptedSystemAudioCaptureDevice))
        string.append("HasInterruptedSystemAudioCaptureDevice,"_s);

    if (string.isEmpty())
        string.append("IsNotPlaying"_s);
    else
        string.shrink(string.length() - 1);

    return string.toString();
}

void Internals::setPageDefersLoading(bool defersLoading)
{
    Document* document = contextDocument();
    if (!document)
        return;
    if (Page* page = document->page())
        page->setDefersLoading(defersLoading);
}

ExceptionOr<bool> Internals::pageDefersLoading()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { ExceptionCode::InvalidAccessError };
    return document->page()->defersLoading();
}

void Internals::grantUniversalAccess()
{
    if (auto* document = contextDocument())
        document->securityOrigin().grantUniversalAccess();
}

void Internals::disableCORSForURL(const String& url)
{
    if (auto* page = contextDocument() ? contextDocument()->page() : nullptr)
        page->addCORSDisablingPatternForTesting(UserContentURLPattern { url });
}

RefPtr<File> Internals::createFile(const String& path)
{
    Document* document = contextDocument();
    if (!document)
        return nullptr;

    URL url = document->completeURL(path);
    if (!url.protocolIsFile())
        return nullptr;

    if (auto* page = document->page())
        page->chrome().client().registerBlobPathForTesting(url.fileSystemPath(), [] () { });

    return File::create(document, url.fileSystemPath());
}
void Internals::asyncCreateFile(const String& path, DOMPromiseDeferred<IDLInterface<File>>&& promise)
{
    Document* document = contextDocument();
    if (!document) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    URL url = document->completeURL(path);
    if (!url.protocolIsFile()) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    if (auto* page = document->page()) {
        auto fileSystemPath = url.fileSystemPath();
        page->chrome().client().registerBlobPathForTesting(fileSystemPath, [promise = WTFMove(promise), weakDocument = WeakPtr { *document }, url = WTFMove(url)] () mutable {
            if (!weakDocument) {
                promise.reject(ExceptionCode::InvalidStateError);
                return;
            }
            promise.resolve(File::create(weakDocument.get(), url.fileSystemPath()));
        });
    } else
        promise.reject(ExceptionCode::InvalidStateError);
}


String Internals::createTemporaryFile(const String& name, const String& contents)
{
    if (name.isEmpty())
        return nullString();

    auto [path, file] = FileSystem::openTemporaryFile(makeString("WebCoreTesting-"_s, name));
    if (!FileSystem::isHandleValid(file))
        return nullString();

    auto contentsUTF8 = contents.utf8();
    FileSystem::writeToFile(file, contentsUTF8.span());

    FileSystem::closeFile(file);

    return path;
}

void Internals::queueMicroTask(int testNumber)
{
    Document* document = contextDocument();
    if (!document)
        return;

    ScriptExecutionContext* context = document;
    auto& eventLoop = context->eventLoop();
    eventLoop.queueMicrotask([document = Ref { *document }, testNumber]() {
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Debug, makeString("MicroTask #"_s, testNumber, " has run."_s));
    });
}

#if ENABLE(CONTENT_FILTERING)

MockContentFilterSettings& Internals::mockContentFilterSettings()
{
    return MockContentFilterSettings::singleton();
}

#endif

static void serializeOffset(StringBuilder& builder, const SnapOffset<LayoutUnit>& snapOffset)
{
    builder.append(snapOffset.offset.toUnsigned());
    if (snapOffset.stop == ScrollSnapStop::Always)
        builder.append(" (always)"_s);
}

static void serializeOffsets(StringBuilder& builder, const Vector<SnapOffset<LayoutUnit>>& snapOffsets)
{
    builder.append("{ "_s, interleave(snapOffsets, serializeOffset, ", "_s), " }"_s);
}

void Internals::setPlatformMomentumScrollingPredictionEnabled(bool enabled)
{
    ScrollingMomentumCalculator::setPlatformMomentumScrollingPredictionEnabled(enabled);
}

ExceptionOr<String> Internals::scrollSnapOffsets(Element& element)
{
    auto areaOrException = scrollableAreaForNode(&element);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    if (!scrollableArea)
        return Exception { ExceptionCode::InvalidAccessError };

    auto* offsetInfo = scrollableArea->snapOffsetsInfo();
    StringBuilder result;
    if (offsetInfo && !offsetInfo->horizontalSnapOffsets.isEmpty()) {
        result.append("horizontal = "_s);
        serializeOffsets(result, offsetInfo->horizontalSnapOffsets);
    }

    if (offsetInfo && !offsetInfo->verticalSnapOffsets.isEmpty()) {
        if (result.length())
            result.append(", "_s);
        result.append("vertical = "_s);
        serializeOffsets(result, offsetInfo->verticalSnapOffsets);
    }

    return result.toString();
}

ExceptionOr<bool> Internals::isScrollSnapInProgress(Element& element)
{
    auto areaOrException = scrollableAreaForNode(&element);
    if (areaOrException.hasException())
        return areaOrException.releaseException();

    auto* scrollableArea = areaOrException.releaseReturnValue();
    if (!scrollableArea)
        return Exception { ExceptionCode::InvalidAccessError };

    return scrollableArea->isScrollSnapInProgress();
}

bool Internals::testPreloaderSettingViewport()
{
    return testPreloadScannerViewportSupport(contextDocument());
}

ExceptionOr<String> Internals::pathStringWithShrinkWrappedRects(const Vector<double>& rectComponents, double radius)
{
    if (rectComponents.size() % 4)
        return Exception { ExceptionCode::InvalidAccessError };

    Vector<FloatRect> rects;
    for (unsigned i = 0; i < rectComponents.size(); i += 4)
        rects.append(FloatRect(rectComponents[i], rectComponents[i + 1], rectComponents[i + 2], rectComponents[i + 3]));

    SVGPathStringBuilder builder;
    PathUtilities::pathWithShrinkWrappedRects(rects, radius).applyElements([&builder](const PathElement& element) {
        switch (element.type) {
        case PathElement::Type::MoveToPoint:
            builder.moveTo(element.points[0], false, AbsoluteCoordinates);
            return;
        case PathElement::Type::AddLineToPoint:
            builder.lineTo(element.points[0], AbsoluteCoordinates);
            return;
        case PathElement::Type::AddQuadCurveToPoint:
            builder.curveToQuadratic(element.points[0], element.points[1], AbsoluteCoordinates);
            return;
        case PathElement::Type::AddCurveToPoint:
            builder.curveToCubic(element.points[0], element.points[1], element.points[2], AbsoluteCoordinates);
            return;
        case PathElement::Type::CloseSubpath:
            builder.closePath();
            return;
        }
        ASSERT_NOT_REACHED();
    });
    return builder.result();
}

void Internals::systemBeep()
{
    SystemSoundManager::singleton().systemBeep();
}

#if ENABLE(VIDEO)

String Internals::getCurrentMediaControlsStatusForElement(HTMLMediaElement& mediaElement)
{
    return mediaElement.getCurrentMediaControlsStatus();
}

void Internals::setMediaControlsMaximumRightContainerButtonCountOverride(HTMLMediaElement& mediaElement, size_t count)
{
    mediaElement.setMediaControlsMaximumRightContainerButtonCountOverride(count);
}

void Internals::setMediaControlsHidePlaybackRates(HTMLMediaElement& mediaElement, bool hidePlaybackRates)
{
    mediaElement.setMediaControlsHidePlaybackRates(hidePlaybackRates);
}

#endif // ENABLE(VIDEO)

void Internals::setPageMediaVolume(float volume)
{
    Document* document = contextDocument();
    if (!document)
        return;

    Page* page = document->page();
    if (!page)
        return;

    page->setMediaVolume(volume);
}

#if !PLATFORM(COCOA)

String Internals::userVisibleString(const DOMURL& url)
{
    return WTF::URLHelpers::userVisibleURL(url.href().string().utf8());
}

#endif

void Internals::setShowAllPlugins(bool show)
{
    Document* document = contextDocument();
    if (!document)
        return;

    Page* page = document->page();
    if (!page)
        return;

    page->setShowAllPlugins(show);
}

bool Internals::isReadableStreamDisturbed(ReadableStream& stream)
{
    return stream.isDisturbed();
}

JSValue Internals::cloneArrayBuffer(JSC::JSGlobalObject& lexicalGlobalObject, JSValue buffer, JSValue srcByteOffset, JSValue srcLength)
{
    auto& vm = lexicalGlobalObject.vm();
    const Identifier& privateName = builtinNames(vm).cloneArrayBufferPrivateName();
    JSValue value;
    PropertySlot propertySlot(value, PropertySlot::InternalMethodType::Get);
    lexicalGlobalObject.methodTable()->getOwnPropertySlot(&lexicalGlobalObject, &lexicalGlobalObject, privateName, propertySlot);
    value = propertySlot.getValue(&lexicalGlobalObject, privateName);
    ASSERT(value.isCallable());

    JSObject* function = value.getObject();
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != JSC::CallData::Type::None);
    MarkedArgumentBuffer arguments;
    arguments.append(buffer);
    arguments.append(srcByteOffset);
    arguments.append(srcLength);
    ASSERT(!arguments.hasOverflowed());

    return JSC::call(&lexicalGlobalObject, function, callData, JSC::jsUndefined(), arguments);
}

String Internals::resourceLoadStatisticsForURL(const DOMURL& url)
{
    return ResourceLoadObserver::shared().statisticsForURL(url.href());
}

void Internals::setTrackingPreventionEnabled(bool enable)
{
    DeprecatedGlobalSettings::setTrackingPreventionEnabled(enable);
}

String Internals::composedTreeAsText(Node& node)
{
    if (!is<ContainerNode>(node))
        return emptyString();
    return WebCore::composedTreeAsText(downcast<ContainerNode>(node));
}

bool Internals::isProcessingUserGesture()
{
    return UserGestureIndicator::processingUserGesture();
}

void Internals::withUserGesture(Ref<VoidCallback>&& callback)
{
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, contextDocument());
    callback->handleEvent();
}

void Internals::withoutUserGesture(Ref<VoidCallback>&& callback)
{
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::No, contextDocument());
    callback->handleEvent();
}

bool Internals::userIsInteracting()
{
    if (auto* document = contextDocument()) {
        if (auto* page = document->page())
            return page->chrome().client().userIsInteracting();
    }
    return false;
}

bool Internals::hasTransientActivation()
{
    if (auto* document = contextDocument()) {
        if (auto* window = document->domWindow())
            return window->hasTransientActivation();
    }
    return false;
}

bool Internals::consumeTransientActivation()
{
    if (auto* document = contextDocument()) {
        if (auto* window = document->domWindow())
            return window->consumeTransientActivation();
    }
    return false;
}

double Internals::lastHandledUserGestureTimestamp()
{
    Document* document = contextDocument();
    if (!document)
        return 0;

    return document->lastHandledUserGestureTimestamp().secondsSinceEpoch().value();
}

bool Internals::hasHistoryActionActivation()
{
    if (auto* document = contextDocument()) {
        if (auto* window = document->domWindow())
            return window->hasHistoryActionActivation();
    }
    return false;
}

bool Internals::consumeHistoryActionUserActivation()
{
    if (auto* document = contextDocument()) {
        if (auto* window = document->domWindow())
            return window->consumeHistoryActionUserActivation();
    }
    return false;
}

RefPtr<GCObservation> Internals::observeGC(JSC::JSValue value)
{
    if (!value.isObject())
        return nullptr;
    return GCObservation::create(asObject(value));
}

void Internals::setUserInterfaceLayoutDirection(UserInterfaceLayoutDirection userInterfaceLayoutDirection)
{
    Document* document = contextDocument();
    if (!document)
        return;

    Page* page = document->page();
    if (!page)
        return;

    page->setUserInterfaceLayoutDirection(userInterfaceLayoutDirection == UserInterfaceLayoutDirection::LTR ? WebCore::UserInterfaceLayoutDirection::LTR : WebCore::UserInterfaceLayoutDirection::RTL);
}

#if !PLATFORM(COCOA)

bool Internals::userPrefersReducedMotion() const
{
    return false;
}

bool Internals::userPrefersContrast() const
{
    return false;
}

#if ENABLE(VIDEO)
double Internals::privatePlayerVolume(const HTMLMediaElement&)
{
    return 0;
}

bool Internals::privatePlayerMuted(const HTMLMediaElement&)
{
    return false;
}
#endif

RefPtr<SharedBuffer> Internals::pngDataForTesting()
{
    return nullptr;
}

#endif // !PLATFORM(COCOA)

#if ENABLE(VIDEO)
bool Internals::isMediaElementHidden(const HTMLMediaElement& media)
{
    return media.elementIsHidden();
}

double Internals::elementEffectivePlaybackRate(const HTMLMediaElement& media)
{
    return media.effectivePlaybackRate();
}
#endif

ExceptionOr<void> Internals::setIsPlayingToBluetoothOverride(std::optional<bool> isPlaying)
{
#if ENABLE(ROUTING_ARBITRATION)
    AudioSession::sharedSession().setIsPlayingToBluetoothOverride(isPlaying);
    return { };
#else
    UNUSED_PARAM(isPlaying);
    return Exception { ExceptionCode::NotSupportedError };
#endif
}

void Internals::reportBacktrace()
{
    WTFReportBacktrace();
}

void Internals::setBaseWritingDirection(BaseWritingDirection direction)
{
    if (auto* document = contextDocument()) {
        if (auto* frame = document->frame()) {
            switch (direction) {
            case BaseWritingDirection::Ltr:
                frame->editor().setBaseWritingDirection(WritingDirection::LeftToRight);
                break;
            case BaseWritingDirection::Rtl:
                frame->editor().setBaseWritingDirection(WritingDirection::RightToLeft);
                break;
            case BaseWritingDirection::Natural:
                frame->editor().setBaseWritingDirection(WritingDirection::Natural);
                break;
            }
        }
    }
}

#if ENABLE(POINTER_LOCK)
bool Internals::pageHasPendingPointerLock() const
{
    Document* document = contextDocument();
    if (!document)
        return false;

    Page* page = document->page();
    if (!page)
        return false;

    return page->pointerLockController().lockPending();
}

bool Internals::pageHasPointerLock() const
{
    Document* document = contextDocument();
    if (!document)
        return false;

    Page* page = document->page();
    if (!page)
        return false;

    auto& controller = page->pointerLockController();
    return controller.element() && !controller.lockPending();
}
#endif

void Internals::markContextAsInsecure()
{
    auto* document = contextDocument();
    if (!document)
        return;

    document->securityOrigin().setIsPotentiallyTrustworthy(false);
}

void Internals::postTask(Ref<VoidCallback>&& callback)
{
    auto* document = contextDocument();
    if (!document) {
        callback->handleEvent();
        return;
    }

    document->postTask([callback = WTFMove(callback)](ScriptExecutionContext&) {
        callback->handleEvent();
    });
}

static std::optional<TaskSource> taskSourceFromString(const String& taskSourceName)
{
    if (taskSourceName == "DOMManipulation"_s)
        return TaskSource::DOMManipulation;
    return std::nullopt;
}

ExceptionOr<void> Internals::queueTask(ScriptExecutionContext& context, const String& taskSourceName, Ref<VoidCallback>&& callback)
{
    auto source = taskSourceFromString(taskSourceName);
    if (!source)
        return Exception { ExceptionCode::NotSupportedError };

    context.eventLoop().queueTask(*source, [callback = WTFMove(callback)] {
        callback->handleEvent();
    });

    return { };
}

ExceptionOr<void> Internals::queueTaskToQueueMicrotask(Document& document, const String& taskSourceName, Ref<VoidCallback>&& callback)
{
    auto source = taskSourceFromString(taskSourceName);
    if (!source)
        return Exception { ExceptionCode::NotSupportedError };

    ScriptExecutionContext& context = document; // This avoids unnecessarily exporting Document::eventLoop.
    context.eventLoop().queueTask(*source, [movedCallback = WTFMove(callback), protectedDocument = Ref { document }]() mutable {
        ScriptExecutionContext& context = protectedDocument.get();
        context.eventLoop().queueMicrotask([callback = WTFMove(movedCallback)] {
            callback->handleEvent();
        });
    });

    return { };
}

ExceptionOr<bool> Internals::hasSameEventLoopAs(WindowProxy& proxy)
{
    RefPtr<ScriptExecutionContext> context = contextDocument();
    if (!context || !proxy.frame())
        return Exception { ExceptionCode::InvalidStateError };

    auto& proxyFrame = *proxy.frame();
    if (!is<LocalFrame>(proxyFrame))
        return false;
    RefPtr<ScriptExecutionContext> proxyContext = downcast<LocalFrame>(proxyFrame).document();
    if (!proxyContext)
        return Exception { ExceptionCode::InvalidStateError };

    return context->eventLoop().hasSameEventLoopAs(proxyContext->eventLoop());
}

Vector<String> Internals::accessKeyModifiers() const
{
    Vector<String> accessKeyModifierStrings;

    for (auto modifier : EventHandler::accessKeyModifiers()) {
        switch (modifier) {
        case PlatformEvent::Modifier::AltKey:
            accessKeyModifierStrings.append("altKey"_s);
            break;
        case PlatformEvent::Modifier::ControlKey:
            accessKeyModifierStrings.append("ctrlKey"_s);
            break;
        case PlatformEvent::Modifier::MetaKey:
            accessKeyModifierStrings.append("metaKey"_s);
            break;
        case PlatformEvent::Modifier::ShiftKey:
            accessKeyModifierStrings.append("shiftKey"_s);
            break;
        case PlatformEvent::Modifier::CapsLockKey:
            accessKeyModifierStrings.append("capsLockKey"_s);
            break;
        case PlatformEvent::Modifier::AltGraphKey:
            ASSERT_NOT_REACHED(); // AltGraph is only for DOM API.
            break;
        }
    }

    return accessKeyModifierStrings;
}

void Internals::setQuickLookPassword(const String& password)
{
#if PLATFORM(IOS_FAMILY) && USE(QUICK_LOOK)
    auto& quickLookHandleClient = MockPreviewLoaderClient::singleton();
    LegacyPreviewLoader::setClientForTesting(&quickLookHandleClient);
    quickLookHandleClient.setPassword(password);
#else
    UNUSED_PARAM(password);
#endif
}

void Internals::setAsRunningUserScripts(Document& document)
{
    document.setAsRunningUserScripts();
}

#if ENABLE(WEBGL)
void Internals::simulateEventForWebGLContext(SimulatedWebGLContextEvent event, WebGLRenderingContext& context)
{
    WebGLRenderingContext::SimulatedEventForTesting contextEvent;
    switch (event) {
    case SimulatedWebGLContextEvent::GPUStatusFailure:
        contextEvent = WebGLRenderingContext::SimulatedEventForTesting::GPUStatusFailure;
        break;
    case SimulatedWebGLContextEvent::Timeout:
        contextEvent = WebGLRenderingContext::SimulatedEventForTesting::Timeout;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }
    context.simulateEventForTesting(contextEvent);
}

Internals::RequestedGPU Internals::requestedGPU(WebGLRenderingContext& context)
{
    switch (context.creationAttributes().powerPreference) {
    case WebGLPowerPreference::Default:
        return RequestedGPU::Default;
    case WebGLPowerPreference::LowPower:
        return RequestedGPU::LowPower;
    case WebGLPowerPreference::HighPerformance:
        return RequestedGPU::HighPerformance;
    }
    ASSERT_NOT_REACHED();
    return RequestedGPU::Default;

}
#endif

void Internals::setPageVisibility(bool isVisible)
{
    updatePageActivityState(ActivityState::IsVisible, isVisible);
}

void Internals::setPageIsFocused(bool isFocused)
{
    updatePageActivityState(ActivityState::IsFocused, isFocused);
}

void Internals::setPageIsFocusedAndActive(bool isFocusedAndActive)
{
    updatePageActivityState({ ActivityState::IsFocused, ActivityState::WindowIsActive }, isFocusedAndActive);
}

void Internals::setPageIsInWindow(bool isInWindow)
{
    updatePageActivityState(ActivityState::IsInWindow, isInWindow);
}

void Internals::updatePageActivityState(OptionSet<ActivityState> statesToChange, bool newValue)
{
    auto* page = contextDocument() ? contextDocument()->page() : nullptr;
    if (!page)
        return;
    auto state = page->activityState();

    if (!newValue)
        state.remove(statesToChange);
    else
        state.add(statesToChange);

    page->setActivityState(state);
}

bool Internals::isPageActive() const
{
    auto* document = contextDocument();
    if (!document || !document->page())
        return false;
    auto& page = *document->page();
    return page.activityState().contains(ActivityState::WindowIsActive);
}

#if ENABLE(WEB_RTC)
void Internals::setH264HardwareEncoderAllowed(bool allowed)
{
    WebRTCProvider::setH264HardwareEncoderAllowed(allowed);
}
#endif

#if ENABLE(MEDIA_STREAM)
void Internals::setMockAudioTrackChannelNumber(MediaStreamTrack& track, unsigned short channelNumber)
{
    auto& source = track.source();
    if (!is<MockRealtimeAudioSource>(source))
        return;
    downcast<MockRealtimeAudioSource>(source).setChannelCount(channelNumber);
}

void Internals::setCameraMediaStreamTrackOrientation(MediaStreamTrack& track, int orientation)
{
    auto& source = track.source();
    if (!source.isCaptureSource())
        return;
    m_orientationNotifier.orientationChanged(orientation);
    source.monitorOrientation(m_orientationNotifier);
}

void Internals::stopObservingRealtimeMediaSource()
{
    if (!m_trackSource)
        return;

    switch (m_trackSource->type()) {
    case RealtimeMediaSource::Type::Audio:
        m_trackSource->removeAudioSampleObserver(*this);
        break;
    case RealtimeMediaSource::Type::Video:
        m_trackSource->removeVideoFrameObserver(*this);
        break;
    }
    m_trackSource->removeObserver(*this);

    m_trackSource = nullptr;
    m_trackAudioSampleCount = 0;
    m_trackVideoSampleCount = 0;
}

void Internals::observeMediaStreamTrack(MediaStreamTrack& track)
{
    stopObservingRealtimeMediaSource();

    m_trackVideoRotation = -1;
    m_trackSource = &track.source();
    m_trackSource->addObserver(*this);
    switch (m_trackSource->type()) {
    case RealtimeMediaSource::Type::Audio:
        m_trackSource->addAudioSampleObserver(*this);
        break;
    case RealtimeMediaSource::Type::Video:
        m_trackSource->addVideoFrameObserver(*this);
        break;
    }
}

void Internals::mediaStreamTrackVideoFrameRotation(DOMPromiseDeferred<IDLShort>&& promise)
{
    promise.resolve(m_trackVideoRotation);
}

void Internals::videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata)
{
    callOnMainThread([this, weakThis = WeakPtr { *this }, videoFrame = Ref { videoFrame }] {
        if (!weakThis)
            return;
        m_trackVideoSampleCount++;
        m_trackVideoRotation = static_cast<int>(videoFrame->rotation());
    });
}

void Internals::delayMediaStreamTrackSamples(MediaStreamTrack& track, float delay)
{
    track.source().delaySamples(Seconds { delay });
}

void Internals::setMediaStreamTrackMuted(MediaStreamTrack& track, bool muted)
{
    track.source().setMuted(muted);
}

void Internals::removeMediaStreamTrack(MediaStream& stream, MediaStreamTrack& track)
{
    stream.privateStream().removeTrack(track.privateTrack());
}

void Internals::simulateMediaStreamTrackCaptureSourceFailure(MediaStreamTrack& track)
{
    track.source().captureFailed();
}

void Internals::setMediaStreamTrackIdentifier(MediaStreamTrack& track, String&& id)
{
    track.setIdForTesting(WTFMove(id));
}

void Internals::setMediaStreamSourceInterrupted(MediaStreamTrack& track, bool interrupted)
{
    track.source().setInterruptedForTesting(interrupted);
}

bool Internals::isMediaStreamSourceInterrupted(MediaStreamTrack& track) const
{
    return track.source().interrupted();
}

bool Internals::isMediaStreamSourceEnded(MediaStreamTrack& track) const
{
    return track.source().isEnded();
}

bool Internals::isMockRealtimeMediaSourceCenterEnabled()
{
    return MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled();
}

bool Internals::shouldAudioTrackPlay(const AudioTrack& track)
{
    if (!is<AudioTrackPrivateMediaStream>(track.privateTrack()))
        return false;
    return downcast<AudioTrackPrivateMediaStream>(track.privateTrack()).shouldPlay();
}
#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(WEB_RTC)
String Internals::rtcNetworkInterfaceName() const
{
    RefPtr document = contextDocument();
    RefPtr rtcNetworkManager = document ? document->rtcNetworkManager() : nullptr;
    if (!rtcNetworkManager)
        return { };

    return rtcNetworkManager->interfaceNameForTesting();
}
#endif // ENABLE(WEB_RTC)

bool Internals::isHardwareVP9DecoderExpected()
{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    return false;
#elif PLATFORM(IOS_FAMILY)
    return true;
#elif PLATFORM(MAC) && CPU(ARM64)
    return true;
#else
    return false;
#endif
}

bool Internals::supportsAudioSession() const
{
#if USE(AUDIO_SESSION)
    return true;
#else
    return false;
#endif
}

auto Internals::audioSessionCategory() const -> AudioSessionCategory
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().category();
#else
    return AudioSessionCategory::None;
#endif
}

auto Internals::audioSessionMode() const -> AudioSessionMode
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().mode();
#else
    return AudioSessionMode::Default;
#endif
}

auto Internals::routeSharingPolicy() const -> RouteSharingPolicy
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().routeSharingPolicy();
#else
    return RouteSharingPolicy::Default;
#endif
}

#if ENABLE(VIDEO)
auto Internals::categoryAtMostRecentPlayback(HTMLMediaElement& element) const -> AudioSessionCategory
{
#if USE(AUDIO_SESSION)
    return element.categoryAtMostRecentPlayback();
#else
    UNUSED_PARAM(element);
    return AudioSessionCategory::None;
#endif
}

auto Internals::modeAtMostRecentPlayback(HTMLMediaElement& element) const -> AudioSessionMode
{
#if USE(AUDIO_SESSION)
    WTFLogAlways("Internals::modeAtMostRecentPlayback");
    return element.modeAtMostRecentPlayback();
#else
    UNUSED_PARAM(element);
    return AudioSessionMode::Default;
#endif
}
#endif

double Internals::preferredAudioBufferSize() const
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().preferredBufferSize();
#endif
    return 0;
}

double Internals::currentAudioBufferSize() const
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().bufferSize();
#endif
    return 0;
}


bool Internals::audioSessionActive() const
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().isActive();
#endif
    return false;
}

void Internals::storeRegistrationsOnDisk(DOMPromiseDeferred<void>&& promise)
{
    if (!contextDocument())
        return;

    auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
    connection.storeRegistrationsOnDiskForTesting([promise = WTFMove(promise)]() mutable {
        promise.resolve();
    });
}

void Internals::sendH2Ping(String url, DOMPromiseDeferred<IDLDouble>&& promise)
{
    auto* document = contextDocument();
    if (!document) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    auto* frame = document->frame();
    if (!frame) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    frame->loader().client().sendH2Ping(URL { url }, [promise = WTFMove(promise)] (Expected<Seconds, ResourceError>&& result) mutable {
        if (result.has_value())
            promise.resolve(result.value().value());
        else
            promise.reject(ExceptionCode::InvalidStateError);
    });
}

void Internals::clearCacheStorageMemoryRepresentation(DOMPromiseDeferred<void>&& promise)
{
    RefPtr document = contextDocument();
    if (!document)
        return;

    if (!m_cacheStorageConnection) {
        if (RefPtr page = document->page())
            m_cacheStorageConnection = page->cacheStorageProvider().createCacheStorageConnection();
        if (!m_cacheStorageConnection)
            return;
    }

    document->enqueueTaskWhenSettled(m_cacheStorageConnection->clearMemoryRepresentation(ClientOrigin { document->topOrigin().data(), document->securityOrigin().data() }), TaskSource::DOMManipulation, [promise = WTFMove(promise)] (auto&&) mutable {
        promise.resolve();
    });
}

void Internals::cacheStorageEngineRepresentation(DOMPromiseDeferred<IDLDOMString>&& promise)
{
    RefPtr document = contextDocument();
    if (!document)
        return;

    if (!m_cacheStorageConnection) {
        if (RefPtr page = document->page())
            m_cacheStorageConnection = page->cacheStorageProvider().createCacheStorageConnection();
        if (!m_cacheStorageConnection)
            return;
    }
    document->enqueueTaskWhenSettled(m_cacheStorageConnection->engineRepresentation(), TaskSource::DOMManipulation, [promise = WTFMove(promise)](auto&& result) mutable {
        if (!result) {
            promise.reject(Exception { ExceptionCode::InvalidStateError, "internal error"_s });
            return;
        }
        promise.resolve(WTFMove(result.value()));
    });
}

void Internals::updateQuotaBasedOnSpaceUsage()
{
    auto* document = contextDocument();
    if (!document)
        return;

    if (!m_cacheStorageConnection) {
        if (auto* page = contextDocument()->page())
            m_cacheStorageConnection = page->cacheStorageProvider().createCacheStorageConnection();
        if (!m_cacheStorageConnection)
            return;
    }

    m_cacheStorageConnection->updateQuotaBasedOnSpaceUsage(ClientOrigin { document->topOrigin().data(), document->securityOrigin().data() });
}

void Internals::setConsoleMessageListener(RefPtr<StringCallback>&& listener)
{
    if (!contextDocument())
        return;

    contextDocument()->setConsoleMessageListener(WTFMove(listener));
}

void Internals::setResponseSizeWithPadding(FetchResponse& response, uint64_t size)
{
    response.setBodySizeWithPadding(size);
}

uint64_t Internals::responseSizeWithPadding(FetchResponse& response) const
{
    return response.bodySizeWithPadding();
}

const String& Internals::responseNetworkLoadMetricsProtocol(const FetchResponse& response)
{
    return response.networkLoadMetrics().protocol;
}

void Internals::hasServiceWorkerRegistration(const String& clientURL, HasRegistrationPromise&& promise)
{
    if (!contextDocument())
        return;

    URL parsedURL = contextDocument()->completeURL(clientURL);

    return ServiceWorkerProvider::singleton().serviceWorkerConnection().matchRegistration(SecurityOriginData { contextDocument()->topOrigin().data() }, parsedURL, [promise = WTFMove(promise)] (auto&& result) mutable {
        promise.resolve(!!result);
    });
}

void Internals::terminateServiceWorker(ServiceWorker& worker, DOMPromiseDeferred<void>&& promise)
{
    ServiceWorkerProvider::singleton().terminateWorkerForTesting(worker.identifier(), [promise = WTFMove(promise)]() mutable {
        promise.resolve();
    });
}

void Internals::whenServiceWorkerIsTerminated(ServiceWorker& worker, DOMPromiseDeferred<void>&& promise)
{
    return ServiceWorkerProvider::singleton().serviceWorkerConnection().whenServiceWorkerIsTerminatedForTesting(worker.identifier(), [promise = WTFMove(promise)]() mutable {
        promise.resolve();
    });
}

void Internals::terminateWebContentProcess()
{
    exit(0);
}

#if ENABLE(APPLE_PAY)
MockPaymentCoordinator& Internals::mockPaymentCoordinator(Document& document)
{
    return downcast<MockPaymentCoordinator>(document.frame()->page()->paymentCoordinator().client());
}
#endif

Internals::ImageOverlayLine::~ImageOverlayLine() = default;
Internals::ImageOverlayText::~ImageOverlayText() = default;
Internals::ImageOverlayBlock::~ImageOverlayBlock() = default;
Internals::ImageOverlayDataDetector::~ImageOverlayDataDetector() = default;

#if ENABLE(IMAGE_ANALYSIS)

template<typename T>
static FloatQuad getQuad(const T& overlayTextOrLine)
{
    return {
        FloatPoint(overlayTextOrLine.topLeft->x(), overlayTextOrLine.topLeft->y()),
        FloatPoint(overlayTextOrLine.topRight->x(), overlayTextOrLine.topRight->y()),
        FloatPoint(overlayTextOrLine.bottomRight->x(), overlayTextOrLine.bottomRight->y()),
        FloatPoint(overlayTextOrLine.bottomLeft->x(), overlayTextOrLine.bottomLeft->y()),
    };
}

static TextRecognitionLineData makeDataForLine(const Internals::ImageOverlayLine& line)
{
    return {
        getQuad<Internals::ImageOverlayLine>(line),
        line.children.map([](auto& textChild) -> TextRecognitionWordData {
            return { textChild.text, getQuad(textChild), textChild.hasLeadingWhitespace };
        }),
        line.hasTrailingNewline,
        line.isVertical
    };
}

void Internals::requestTextRecognition(Element& element, Ref<VoidCallback>&& callback)
{
    auto page = contextDocument()->page();
    if (!page)
        callback->handleEvent();

    page->chrome().client().requestTextRecognition(element, { }, [callback = WTFMove(callback)] (auto&&) {
        callback->handleEvent();
    });
}

RefPtr<Element> Internals::textRecognitionCandidate() const
{
    if (RefPtr frame = contextDocument()->frame())
        return frame->eventHandler().textRecognitionCandidateElement();

    return nullptr;
}

#endif // ENABLE(IMAGE_ANALYSIS)

void Internals::installImageOverlay(Element& element, Vector<ImageOverlayLine>&& lines, Vector<ImageOverlayBlock>&& blocks, Vector<ImageOverlayDataDetector>&& dataDetectors)
{
    if (!is<HTMLElement>(element))
        return;

#if ENABLE(IMAGE_ANALYSIS)
    ImageOverlay::updateWithTextRecognitionResult(downcast<HTMLElement>(element), TextRecognitionResult {
        lines.map([] (auto& line) -> TextRecognitionLineData {
            return makeDataForLine(line);
        })
#if ENABLE(DATA_DETECTION)
        , dataDetectors.map([](auto& dataDetector) -> TextRecognitionDataDetector {
            return { fakeDataDetectorResultForTesting(), { getQuad(dataDetector) } };
        })
#endif // ENABLE(DATA_DETECTION)
        , blocks.map([] (auto& block) {
            return TextRecognitionBlockData { block.text, getQuad(block) };
        })
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        , TextRecognitionResult::encodeVKCImageAnalysis(fakeImageAnalysisResultForTesting(lines))
#endif
    });
#else
    UNUSED_PARAM(blocks);
    UNUSED_PARAM(dataDetectors);
    UNUSED_PARAM(lines);
#endif
}

bool Internals::hasActiveDataDetectorHighlight() const
{
#if ENABLE(DATA_DETECTION) && ENABLE(IMAGE_ANALYSIS)
    if (auto* controller = contextDocument()->page()->imageOverlayControllerIfExists())
        return controller->hasActiveDataDetectorHighlightForTesting();
#endif
    return false;
}

bool Internals::isSystemPreviewLink(Element& element) const
{
#if USE(SYSTEM_PREVIEW)
    return is<HTMLAnchorElement>(element) && downcast<HTMLAnchorElement>(element).isSystemPreviewLink();
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

bool Internals::isSystemPreviewImage(Element& element) const
{
#if USE(SYSTEM_PREVIEW)
    if (is<HTMLImageElement>(element))
        return downcast<HTMLImageElement>(element).isSystemPreviewImage();
    if (is<HTMLPictureElement>(element))
        return downcast<HTMLPictureElement>(element).isSystemPreviewImage();
    return false;
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

bool Internals::usingAppleInternalSDK() const
{
#if USE(APPLE_INTERNAL_SDK)
    return true;
#else
    return false;
#endif
}

bool Internals::usingGStreamer() const
{
#if USE(GSTREAMER)
    return true;
#else
    return false;
#endif
}

void Internals::setCaptureExtraNetworkLoadMetricsEnabled(bool value)
{
    platformStrategies()->loaderStrategy()->setCaptureExtraNetworkLoadMetricsEnabled(value);
}

String Internals::ongoingLoadsDescriptions() const
{
    StringBuilder builder;
    builder.append('[');
    bool isStarting = true;
    for (auto& identifier : platformStrategies()->loaderStrategy()->ongoingLoads()) {
        if (isStarting)
            isStarting = false;
        else
            builder.append(',');

        builder.append('[');

        for (auto& info : platformStrategies()->loaderStrategy()->intermediateLoadInformationFromResourceLoadIdentifier(identifier))
            builder.append('[', (int)info.type, ",\""_s, info.request.url().string(), "\",\""_s, info.request.httpMethod(), "\","_s, info.response.httpStatusCode(), ']');

        builder.append(']');
    }
    builder.append(']');
    return builder.toString();
}

void Internals::reloadWithoutContentExtensions()
{
    if (auto* frame = this->frame())
        frame->loader().reload(ReloadOption::DisableContentBlockers);
}

void Internals::disableContentExtensionsChecks()
{
    RefPtr frame = this->frame();
    RefPtr loader = frame ? frame->loader().documentLoader() : nullptr;
    if (loader)
        loader->setContentExtensionEnablement({ ContentExtensionDefaultEnablement::Disabled, { } });
}

void Internals::setUseSystemAppearance(bool value)
{
    if (!contextDocument() || !contextDocument()->page())
        return;
    contextDocument()->page()->setUseSystemAppearance(value);
}

size_t Internals::pluginCount()
{
    if (!contextDocument() || !contextDocument()->page())
        return 0;

    return contextDocument()->page()->pluginData().webVisiblePlugins().size();
}

static std::optional<ScrollPosition> scrollPositionForPlugin(Element& element)
{
    auto* pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (auto* pluginViewBase = pluginElement ? pluginElement->pluginWidget() : nullptr)
        return pluginViewBase->scrollPositionForTesting();
    return std::nullopt;
}

ExceptionOr<unsigned> Internals::pluginScrollPositionX(Element& element)
{
    if (std::optional scrollPosition = scrollPositionForPlugin(element))
        return scrollPosition->x();
    return Exception { ExceptionCode::InvalidAccessError };
}

ExceptionOr<unsigned> Internals::pluginScrollPositionY(Element& element)
{
    if (std::optional scrollPosition = scrollPositionForPlugin(element))
        return scrollPosition->y();
    return Exception { ExceptionCode::InvalidAccessError };
}

void Internals::notifyResourceLoadObserver()
{
    ResourceLoadObserver::shared().updateCentralStatisticsStore([] { });
}

unsigned Internals::primaryScreenDisplayID()
{
#if PLATFORM(COCOA)
    return WebCore::primaryScreenDisplayID();
#else
    return 0;
#endif
}

bool Internals::capsLockIsOn()
{
    return WebCore::PlatformKeyboardEvent::currentCapsLockState();
}

auto Internals::parseHEVCCodecParameters(StringView string) -> std::optional<HEVCParameterSet>
{
    return WebCore::parseHEVCCodecParameters(string);
}

String Internals::createHEVCCodecParametersString(const HEVCParameterSet& parameters)
{
    return WebCore::createHEVCCodecParametersString(parameters);
}

auto Internals::parseDoViCodecParameters(StringView string) -> std::optional<DoViParameterSet>
{
    auto parseResult = WebCore::parseDoViCodecParameters(string);
    if (!parseResult)
        return std::nullopt;
    DoViParameterSet convertedResult;
    switch (parseResult->codec) {
    case DoViParameters::Codec::AVC1:
        convertedResult.codecName = "avc1"_s;
        break;
    case DoViParameters::Codec::AVC3:
        convertedResult.codecName = "avc3"_s;
        break;
    case DoViParameters::Codec::HEV1:
        convertedResult.codecName = "hev1"_s;
        break;
    case DoViParameters::Codec::HVC1:
        convertedResult.codecName = "hvc1"_s;
        break;
    }
    convertedResult.bitstreamProfileID = parseResult->bitstreamProfileID;
    convertedResult.bitstreamLevelID = parseResult->bitstreamLevelID;
    return convertedResult;
}

String Internals::createDoViCodecParametersString(const DoViParameterSet& parameterSet)
{
    DoViParameters::Codec codec;
    if (parameterSet.codecName == "avc1"_s)
        codec = DoViParameters::Codec::AVC1;
    else if (parameterSet.codecName == "avc3"_s)
        codec = DoViParameters::Codec::AVC3;
    else if (parameterSet.codecName == "hev1"_s)
        codec = DoViParameters::Codec::HEV1;
    else if (parameterSet.codecName == "hvc1"_s)
        codec = DoViParameters::Codec::HVC1;
    else
        return emptyString();

    return WebCore::createDoViCodecParametersString({ codec, parameterSet.bitstreamProfileID, parameterSet.bitstreamLevelID });
}

std::optional<VPCodecConfigurationRecord> Internals::parseVPCodecParameters(StringView string)
{
    return WebCore::parseVPCodecParameters(string);
}

std::optional<AV1CodecConfigurationRecord> Internals::parseAV1CodecParameters(const String& parameters)
{
    return WebCore::parseAV1CodecParameters(parameters);
}

String Internals::createAV1CodecParametersString(const AV1CodecConfigurationRecord& configuration)
{
    return WebCore::createAV1CodecParametersString(configuration);
}

bool Internals::validateAV1PerLevelConstraints(const String& parameters, const VideoConfiguration& configuration)
{
    if (auto record = WebCore::parseAV1CodecParameters(parameters))
        return WebCore::validateAV1PerLevelConstraints(*record, configuration);
    return false;
}

bool Internals::validateAV1ConfigurationRecord(const String& parameters)
{
    if (auto record = WebCore::parseAV1CodecParameters(parameters))
        return WebCore::validateAV1ConfigurationRecord(*record);
    return false;
}

auto Internals::getCookies() const -> Vector<CookieData>
{
    auto* document = contextDocument();
    if (!document)
        return { };

    auto* page = document->page();
    if (!page)
        return { };

    Vector<Cookie> cookies;
    page->cookieJar().getRawCookies(*document, document->cookieURL(), cookies);
    return WTF::map(cookies, [](auto& cookie) {
        return CookieData { cookie };
    });
}

void Internals::setAlwaysAllowLocalWebarchive(bool alwaysAllowLocalWebarchive)
{
    auto* localFrame = frame();
    if (!localFrame)
        return;
    localFrame->loader().setAlwaysAllowLocalWebarchive(alwaysAllowLocalWebarchive);
}

void Internals::processWillSuspend()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::singleton().processWillSuspend();
#endif
}

void Internals::processDidResume()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::singleton().processDidResume();
#endif
}

void Internals::testDictionaryLogging()
{
    auto* document = contextDocument();
    if (!document)
        return;

    auto* page = document->page();
    if (!page)
        return;

    DiagnosticLoggingClient::ValueDictionary dictionary;
    dictionary.set("stringKey"_s, String("stringValue"_s));
    dictionary.set("uint64Key"_s, std::numeric_limits<uint64_t>::max());
    dictionary.set("int64Key"_s, std::numeric_limits<int64_t>::min());
    dictionary.set("boolKey"_s, true);
    dictionary.set("doubleKey"_s, 2.7182818284590452353602874);

    page->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary("testMessage"_s, "testDescription"_s, dictionary, ShouldSample::No);
}

void Internals::setMaximumIntervalForUserGestureForwardingForFetch(double interval)
{
    UserGestureToken::setMaximumIntervalForUserGestureForwardingForFetchForTesting(Seconds(interval));
}

void Internals::setTransientActivationDuration(double seconds)
{
    LocalDOMWindow::overrideTransientActivationDurationForTesting(Seconds { seconds });
}

void Internals::setIsPlayingToAutomotiveHeadUnit(bool isPlaying)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::singleton().setIsPlayingToAutomotiveHeadUnit(isPlaying);
#else
    UNUSED_PARAM(isPlaying);
#endif
}

String Internals::highlightPseudoElementColor(const AtomString& highlightName, Element& element)
{
    element.document().updateStyleIfNeeded();

    auto& styleResolver = element.document().styleScope().resolver();
    auto* parentStyle = element.computedStyle();
    if (!parentStyle)
        return { };

    auto resolvedStyle = styleResolver.styleForPseudoElement(element, { PseudoId::Highlight, highlightName }, { parentStyle });
    if (!resolvedStyle)
        return { };

    return serializationForCSS(resolvedStyle->style->color());
}
    
Internals::TextIndicatorInfo::TextIndicatorInfo()
{
}

Internals::TextIndicatorInfo::TextIndicatorInfo(const WebCore::TextIndicatorData& data)
    : textBoundingRectInRootViewCoordinates(DOMRect::create(data.textBoundingRectInRootViewCoordinates))
    , textRectsInBoundingRectCoordinates(DOMRectList::create(data.textRectsInBoundingRectCoordinates))
{
}
    
Internals::TextIndicatorInfo::~TextIndicatorInfo() = default;

Internals::TextIndicatorInfo Internals::textIndicatorForRange(const Range& range, TextIndicatorOptions options)
{
    auto indicator = TextIndicator::createWithRange(makeSimpleRange(range), options.coreOptions(), TextIndicatorPresentationTransition::None);
    return indicator->data();
}

void Internals::addPrefetchLoadEventListener(HTMLLinkElement& link, RefPtr<EventListener>&& listener)
{
    if (link.document().settings().linkPrefetchEnabled() && equalLettersIgnoringASCIICase(link.rel(), "prefetch"_s)) {
        link.allowPrefetchLoadAndErrorForTesting();
        link.addEventListener(eventNames().loadEvent, listener.releaseNonNull(), false);
    }
}

#if ENABLE(WEB_AUTHN)
void Internals::setMockWebAuthenticationConfiguration(const MockWebAuthenticationConfiguration& configuration)
{
    auto* document = contextDocument();
    if (!document)
        return;
    auto* page = document->page();
    if (!page)
        return;
    page->chrome().client().setMockWebAuthenticationConfiguration(configuration);
}
#endif

void Internals::setMaxCanvasArea(unsigned size)
{
    CanvasBase::setMaxCanvasAreaForTesting(size);
}

int Internals::processIdentifier() const
{
    return getCurrentProcessID();
}

Ref<InternalsMapLike> Internals::createInternalsMapLike()
{
    return InternalsMapLike::create();
}

Ref<InternalsSetLike> Internals::createInternalsSetLike()
{
    return InternalsSetLike::create();
}

bool Internals::hasSandboxMachLookupAccessToGlobalName(const String& process, const String& service)
{
#if PLATFORM(COCOA)
    pid_t pid;
    if (process == "com.apple.WebKit.WebContent"_s)
        pid = getpid();
    else
        RELEASE_ASSERT_NOT_REACHED();

    return !sandbox_check(pid, "mach-lookup", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_GLOBAL_NAME | SANDBOX_CHECK_NO_REPORT), service.utf8().data());
#else
    UNUSED_PARAM(process);
    UNUSED_PARAM(service);
    return false;
#endif
}

bool Internals::hasSandboxMachLookupAccessToXPCServiceName(const String& process, const String& service)
{
#if PLATFORM(COCOA)
    pid_t pid;
    if (process == "com.apple.WebKit.WebContent"_s)
        pid = getpid();
    else
        RELEASE_ASSERT_NOT_REACHED();

    return !sandbox_check(pid, "mach-lookup", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_XPC_SERVICE_NAME | SANDBOX_CHECK_NO_REPORT), service.utf8().data());
#else
    UNUSED_PARAM(process);
    UNUSED_PARAM(service);
    return false;
#endif
}

bool Internals::hasSandboxUnixSyscallAccess(const String& process, unsigned syscall) const
{
#if PLATFORM(COCOA)
    RELEASE_ASSERT(process == "com.apple.WebKit.WebContent"_s);
    auto pid = getpid();
    return !sandbox_check(pid, "syscall-unix", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_SYSCALL_NUMBER | SANDBOX_CHECK_NO_REPORT), syscall);
#else
    UNUSED_PARAM(process);
    UNUSED_PARAM(syscall);
    return false;
#endif
}

String Internals::windowLocationHost(DOMWindow& window)
{
    return window.location().host();
}

String Internals::systemColorForCSSValue(const String& cssValue, bool useDarkModeAppearance, bool useElevatedUserInterfaceLevel)
{
    CSSValueID id = cssValueKeywordID(cssValue);
    RELEASE_ASSERT(StyleColor::isSystemColorKeyword(id));

    OptionSet<StyleColorOptions> options;
    if (useDarkModeAppearance)
        options.add(StyleColorOptions::UseDarkAppearance);
    if (useElevatedUserInterfaceLevel)
        options.add(StyleColorOptions::UseElevatedUserInterfaceLevel);
    
    return serializationForCSS(RenderTheme::singleton().systemColor(id, options));
}

bool Internals::systemHasBattery() const
{
#if PLATFORM(COCOA)
    return WebCore::systemHasBattery();
#else
    return false;
#endif
}

void Internals::setSystemHasBatteryForTesting(bool hasBattery)
{
#if PLATFORM(COCOA)
    SystemBatteryStatusTestingOverrides::singleton().setHasBattery(hasBattery);
#else
    UNUSED_PARAM(hasBattery);
#endif
}

void Internals::setSystemHasACForTesting(bool hasAC)
{
#if PLATFORM(COCOA)
    SystemBatteryStatusTestingOverrides::singleton().setHasAC(hasAC);
#else
    UNUSED_PARAM(hasAC);
#endif
}

void Internals::setHardwareVP9DecoderDisabledForTesting(bool disabled)
{
#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setHardwareDecoderDisabled(disabled);
#else
    UNUSED_PARAM(disabled);
#endif
}

void Internals::setVP9DecoderDisabledForTesting(bool disabled)
{
#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setVP9DecoderDisabled(disabled);
#else
    UNUSED_PARAM(disabled);
#endif
}

void Internals::setVP9ScreenSizeAndScaleForTesting(double width, double height, double scale)
{
#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setVP9ScreenSizeAndScale(ScreenDataOverrides { width, height, scale });
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(scale);
#endif
}

int Internals::readPreferenceInteger(const String& domain, const String& key)
{
#if PLATFORM(COCOA)
    Boolean keyExistsAndHasValidFormat = false;
    return CFPreferencesGetAppIntegerValue(key.createCFString().get(), domain.createCFString().get(), &keyExistsAndHasValidFormat);
#else
    UNUSED_PARAM(domain);
    UNUSED_PARAM(key);
    return -1;
#endif
}

#if !PLATFORM(COCOA)
String Internals::encodedPreferenceValue(const String&, const String&)
{
    return emptyString();
}

bool Internals::isRemoteUIAppForAccessibility()
{
    return false;
}

bool Internals::hasSandboxIOKitOpenAccessToClass(const String& process, const String& ioKitClass)
{
    UNUSED_PARAM(process);
    UNUSED_PARAM(ioKitClass);
    return false;
}
#endif

#if ENABLE(APP_HIGHLIGHTS)
Vector<String> Internals::appHighlightContextMenuItemTitles() const
{
    return {{
        contextMenuItemTagAddHighlightToCurrentQuickNote(),
        contextMenuItemTagAddHighlightToNewQuickNote(),
    }};
}

unsigned Internals::numberOfAppHighlights()
{
    Document* document = contextDocument();
    if (!document)
        return 0;
    auto appHighlightRegistry = document->appHighlightRegistryIfExists();
    if (!appHighlightRegistry)
        return 0;
    unsigned numHighlights = 0;
    for (auto& highlight : appHighlightRegistry->map())
        numHighlights += highlight.value->highlightRanges().size();
    return numHighlights;
}
#endif

bool Internals::supportsPictureInPicture()
{
    return WebCore::supportsPictureInPicture();
}

String Internals::focusRingColor()
{
    return serializationForCSS(RenderTheme::singleton().focusRingColor(StyleColorOptions::UseSystemAppearance));
}

ExceptionOr<unsigned> Internals::createSleepDisabler(const String& reason, bool display)
{
    auto* document = contextDocument();
    if (!document || !document->pageID())
        return Exception { ExceptionCode::InvalidAccessError };

    static unsigned lastUsedIdentifier = 0;
    auto sleepDisabler = makeUnique<WebCore::SleepDisabler>(reason, display ? PAL::SleepDisabler::Type::Display : PAL::SleepDisabler::Type::System, *document->pageID());
    m_sleepDisablers.add(++lastUsedIdentifier, WTFMove(sleepDisabler));
    return lastUsedIdentifier;
}

bool Internals::destroySleepDisabler(unsigned identifier)
{
    return m_sleepDisablers.remove(identifier);
}

#if ENABLE(WEBXR)

ExceptionOr<RefPtr<WebXRTest>> Internals::xrTest()
{
    auto* document = contextDocument();
    if (!document || !document->domWindow() || !document->settings().webXREnabled())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!m_xrTest) {
        auto* navigator = contextDocument()->domWindow()->optionalNavigator();
        if (!navigator)
            return Exception { ExceptionCode::InvalidAccessError };

        m_xrTest = WebXRTest::create(NavigatorWebXR::xr(*navigator));
    }
    return m_xrTest.get();
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)
unsigned Internals::mediaKeysInternalInstanceObjectRefCount(const MediaKeys& mediaKeys) const
{
    return mediaKeys.internalInstanceObjectRefCount();
}

unsigned Internals::mediaKeySessionInternalInstanceSessionObjectRefCount(const MediaKeySession& mediaKeySession) const
{
    return mediaKeySession.internalInstanceSessionObjectRefCount();
}
#endif

void Internals::setContentSizeCategory(Internals::ContentSizeCategory category)
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    CFStringRef ctCategory = nil;
    switch (category) {
    case Internals::ContentSizeCategory::L:
        ctCategory = kCTFontContentSizeCategoryL;
        break;
    case Internals::ContentSizeCategory::XXXL:
        ctCategory = kCTFontContentSizeCategoryXXXL;
        break;
    }
    WebCore::setContentSizeCategory(ctCategory);
    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
#else
    UNUSED_PARAM(category);
#endif
}

#if ENABLE(ATTACHMENT_ELEMENT)

ExceptionOr<Internals::AttachmentThumbnailInfo> Internals::attachmentThumbnailInfo(const HTMLAttachmentElement& element)
{
#if HAVE(QUICKLOOK_THUMBNAILING)
    AttachmentThumbnailInfo info;
    if (auto image = element.thumbnail()) {
        auto size = image->size();
        info.width = size.width();
        info.height = size.height();
    }
    return info;
#else
    UNUSED_PARAM(element);
    return Exception { ExceptionCode::InvalidAccessError };
#endif
}

#if ENABLE(SERVICE_CONTROLS)
bool Internals::hasImageControls(const HTMLImageElement& element) const
{
    return ImageControlsMac::hasImageControls(element);
}
#endif

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(MEDIA_SESSION)
ExceptionOr<double> Internals::currentMediaSessionPosition(const MediaSession& session)
{
    if (auto currentPosition = session.currentPosition())
        return *currentPosition;
    return Exception { ExceptionCode::InvalidStateError };
}

ExceptionOr<void> Internals::sendMediaSessionAction(MediaSession& session, const MediaSessionActionDetails& actionDetails)
{
    if (session.callActionHandler(actionDetails))
        return { };

    return Exception { ExceptionCode::InvalidStateError };
}

void Internals::loadArtworkImage(String&& url, ArtworkImagePromise&& promise)
{
    if (!contextDocument()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "No document."_s });
        return;
    }
    if (m_artworkImagePromise) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Another download is currently pending."_s });
        return;
    }
    m_artworkImagePromise = makeUnique<ArtworkImagePromise>(WTFMove(promise));
    m_artworkLoader = makeUnique<ArtworkImageLoader>(*contextDocument(), url, [this](Image* image) {
        if (image) {
            auto imageData = ImageData::create(image->width(), image->height(), { { PredefinedColorSpace::SRGB } });
            if (!imageData.hasException())
                m_artworkImagePromise->resolve(imageData.releaseReturnValue());
            else
                m_artworkImagePromise->reject(imageData.exception().code());
        } else
            m_artworkImagePromise->reject(Exception { ExceptionCode::InvalidAccessError, "No image retrieved."_s });
        m_artworkImagePromise = nullptr;
    });
    m_artworkLoader->requestImageResource();
}

ExceptionOr<Vector<String>> Internals::platformSupportedCommands() const
{
    if (!contextDocument())
        return Exception { ExceptionCode::InvalidAccessError };
    auto commands = PlatformMediaSessionManager::singleton().supportedCommands();
    Vector<String> commandStrings;
    for (auto command : commands)
        commandStrings.append(convertEnumerationToString(command));

    return commandStrings;
}

#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
ExceptionOr<void> Internals::registerMockMediaSessionCoordinator(ScriptExecutionContext& context, Ref<StringCallback>&& listener)
{
    if (m_mockMediaSessionCoordinator)
        return { };

    auto* document = contextDocument();
    if (!document || !document->domWindow())
        return Exception { ExceptionCode::InvalidAccessError };

    if (!document->settings().mediaSessionCoordinatorEnabled())
        return Exception { ExceptionCode::InvalidAccessError };

    auto& session = NavigatorMediaSession::mediaSession(document->domWindow()->navigator());
    auto mock = MockMediaSessionCoordinator::create(context, WTFMove(listener));
    m_mockMediaSessionCoordinator = mock.ptr();
    session.coordinator().setMediaSessionCoordinatorPrivate(WTFMove(mock));

    return { };
}

ExceptionOr<void> Internals::setMockMediaSessionCoordinatorCommandsShouldFail(bool shouldFail)
{
    if (!m_mockMediaSessionCoordinator)
        return Exception { ExceptionCode::InvalidStateError };

    m_mockMediaSessionCoordinator->setCommandsShouldFail(shouldFail);
    return { };
}
#endif // ENABLE(MEDIA_SESSION)

constexpr ASCIILiteral string(std::partial_ordering ordering)
{
    if (is_lt(ordering))
        return "less"_s;
    if (is_gt(ordering))
        return "greater"_s;
    if (WebCore::is_eq(ordering))
        return "equivalent"_s;
    return "unordered"_s;
}

constexpr TreeType convertType(Internals::TreeType type)
{
    switch (type) {
    case Internals::Tree:
        return Tree;
    case Internals::ShadowIncludingTree:
        return ShadowIncludingTree;
    case Internals::ComposedTree:
        return ComposedTree;
    }
    ASSERT_NOT_REACHED();
    return Tree;
}

String Internals::treeOrder(Node& a, Node& b, TreeType type)
{
    return string(treeOrderForTesting(convertType(type), a, b));
}

String Internals::treeOrderBoundaryPoints(Node& containerA, unsigned offsetA, Node& containerB, unsigned offsetB, TreeType type)
{
    return string(treeOrderForTesting(convertType(type), BoundaryPoint( containerA, offsetA ), BoundaryPoint(containerB, offsetB)));
}

bool Internals::rangeContainsNode(const AbstractRange& range, Node& node, TreeType type)
{
    return contains(convertType(type), makeSimpleRange(range), node);
}

bool Internals::rangeContainsBoundaryPoint(const AbstractRange& range, Node& container, unsigned offset, TreeType type)
{
    return contains(convertType(type), makeSimpleRange(range), { container, offset });
}

bool Internals::rangeContainsRange(const AbstractRange& outerRange, const AbstractRange& innerRange, TreeType type)
{
    return contains(convertType(type), makeSimpleRange(outerRange), makeSimpleRange(innerRange));
}

bool Internals::rangeIntersectsNode(const AbstractRange& range, Node& node, TreeType type)
{
    return intersectsForTesting(convertType(type), makeSimpleRange(range), node);
}

bool Internals::rangeIntersectsRange(const AbstractRange& a, const AbstractRange& b, TreeType type)
{
    return intersectsForTesting(convertType(type), makeSimpleRange(a), makeSimpleRange(b));
}

String Internals::dumpStyleResolvers()
{
    auto* document = contextDocument();
    if (!document || !document->domWindow())
        return { };

    document->updateStyleIfNeeded();

    unsigned currentIdentifier = 0;
    HashMap<Style::Resolver*, unsigned> resolverIdentifiers;

    StringBuilder result;

    auto dumpResolver = [&](auto name, auto& resolver) {
        auto identifier = resolverIdentifiers.ensure(&resolver, [&] {
            return currentIdentifier++;
        }).iterator->value;

        result.append('(', name, ' ', "(identifier="_s, identifier, ") (author rule count="_s,
            resolver.ruleSets().authorStyle().ruleCount(), "))\n"_s);
    };

    dumpResolver("document resolver"_s, document->styleScope().resolver());

    for (auto& shadowRoot : document->inDocumentShadowRoots()) {
        auto name = shadowRoot.mode() == ShadowRootMode::UserAgent ? "shadow root resolver (user agent)"_s : "shadow root resolver (author)"_s;
        dumpResolver(name, const_cast<ShadowRoot&>(shadowRoot).styleScope().resolver());
    }

    return result.toString();
}

ExceptionOr<void> Internals::setDocumentAutoplayPolicy(Document& document, Internals::AutoplayPolicy policy)
{
    static_assert(static_cast<uint8_t>(WebCore::AutoplayPolicy::Default) == static_cast<uint8_t>(Internals::AutoplayPolicy::Default), "Internals::Default != WebCore::Default");
    static_assert(static_cast<uint8_t>(WebCore::AutoplayPolicy::Allow) == static_cast<uint8_t>(Internals::AutoplayPolicy::Allow), "Internals::Allow != WebCore::Allow");
    static_assert(static_cast<uint8_t>(WebCore::AutoplayPolicy::AllowWithoutSound) == static_cast<uint8_t>(Internals::AutoplayPolicy::AllowWithoutSound), "Internals::AllowWithoutSound != WebCore::AllowWithoutSound");
    static_assert(static_cast<uint8_t>(WebCore::AutoplayPolicy::Deny) == static_cast<uint8_t>(Internals::AutoplayPolicy::Deny), "Internals::Deny != WebCore::Deny");

    auto* loader = document.loader();
    if (!loader)
        return Exception { ExceptionCode::InvalidStateError };

    loader->setAutoplayPolicy(static_cast<WebCore::AutoplayPolicy>(policy));

    return { };
}

void Internals::retainTextIteratorForDocumentContent()
{
    auto* document = contextDocument();
    if (!document)
        return;

    auto range = makeRangeSelectingNodeContents(*document);
    m_textIterator = makeUnique<TextIterator>(range);
}

RefPtr<PushSubscription> Internals::createPushSubscription(const String& endpoint, std::optional<EpochTimeStamp> expirationTime, const ArrayBuffer& serverVAPIDPublicKey, const ArrayBuffer& clientECDHPublicKey, const ArrayBuffer& auth)
{
    return PushSubscription::create(PushSubscriptionData { std::nullopt, { endpoint }, expirationTime, serverVAPIDPublicKey.toVector(), clientECDHPublicKey.toVector(), auth.toVector() });
}

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)

void Internals::modelInlinePreviewUUIDs(ModelInlinePreviewUUIDsPromise&& promise) const
{
    auto* document = contextDocument();
    if (!document) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    auto* frame = document->frame();
    if (!frame) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    CompletionHandler<void(Vector<String>&&)> completionHandler = [promise = WTFMove(promise)] (Vector<String> uuids) mutable {
        promise.resolve(uuids);
    };

    frame->loader().client().modelInlinePreviewUUIDs(WTFMove(completionHandler));
}

String Internals::modelInlinePreviewUUIDForModelElement(const HTMLModelElement& modelElement) const
{
    return modelElement.inlinePreviewUUIDForTesting();
}

#endif

bool Internals::hasSleepDisabler() const
{
    auto* document = contextDocument();
    return document ? document->hasSleepDisabler() : false;
}

void Internals::acceptTypedArrays(Int32Array&)
{
    // Do nothing.
}

Internals::SelectorFilterHashCounts Internals::selectorFilterHashCounts(const String& selector)
{
    auto parser = CSSParser { { *contextDocument() } };
    auto selectorList = parser.parseSelectorList(selector);
    if (!selectorList)
        return { };
    
    auto hashes = SelectorFilter::collectHashesForTesting(*selectorList->first());

    return { hashes.ids.size(), hashes.classes.size(), hashes.tags.size(), hashes.attributes.size() };
}

bool Internals::isVisuallyNonEmpty() const
{
    auto* document = contextDocument();
    if (!document || !document->frame())
        return false;

    auto* frameView = document->frame()->view();
    return frameView && frameView->isVisuallyNonEmpty();
}

bool Internals::isUsingUISideCompositing() const
{
    auto* document = contextDocument();
    if (!document)
        return false;
    auto* page = document->page();
    if (!page)
        return false;
    return page->chrome().client().isUsingUISideCompositing();
}

AccessibilityObject* Internals::axObjectForElement(Element& element) const
{
    auto* document = contextDocument();
    if (!document)
        return nullptr;
    WebCore::AXObjectCache::enableAccessibility();

    auto* cache = document->axObjectCache();
    return cache ? cache->getOrCreate(element) : nullptr;
}

String Internals::getComputedLabel(Element& element) const
{
    // Force a layout. If we don't, and this request has come in before the render tree was built,
    // the accessibility object for this element will not be created (because it doesn't yet have its renderer).
    updateLayoutAndStyleForAllFrames();

    RefPtr axObject = axObjectForElement(element);
    return axObject ? axObject->computedLabel() : ""_s;
}

String Internals::getComputedRole(Element& element) const
{
    updateLayoutAndStyleForAllFrames();

    RefPtr axObject = axObjectForElement(element);
    return axObject ? axObject->computedRoleString() : ""_s;
}

bool Internals::hasScopeBreakingHasSelectors() const
{
    contextDocument()->styleScope().flushPendingUpdate();
    return !!contextDocument()->styleScope().resolver().ruleSets().scopeBreakingHasPseudoClassInvalidationRuleSet();
}

void Internals::setHistoryTotalStateObjectPayloadLimitOverride(uint32_t limit)
{
    RefPtr window = contextDocument() ? contextDocument()->domWindow() : nullptr;
    if (!window)
        return;
    window->history().setTotalStateObjectPayloadLimitOverride(limit);
}

void Internals::setPDFDisplayModeForTesting(Element& element, const String& displayMode) const
{
    RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (!pluginElement)
        return;

    RefPtr pluginViewBase = pluginElement->pluginWidget();
    if (!pluginViewBase)
        return;

    pluginViewBase->setPDFDisplayModeForTesting(displayMode);
}

void Internals::unlockPDFDocumentForTesting(Element& element, const String& password) const
{
    RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (!pluginElement)
        return;

    RefPtr pluginViewBase = pluginElement->pluginWidget();
    if (!pluginViewBase)
        return;

    pluginViewBase->unlockPDFDocumentForTesting(password);
}

bool Internals::sendEditingCommandToPDFForTesting(Element& element, const String& commandName, const String& argument) const
{
    RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (!pluginElement)
        return false;

    RefPtr pluginViewBase = pluginElement->pluginWidget();
    if (!pluginViewBase)
        return false;

    return pluginViewBase->sendEditingCommandToPDFForTesting(commandName, argument);
}

Vector<Internals::PDFAnnotationRect> Internals::pdfAnnotationRectsForTesting(Element& element) const
{
    Vector<PDFAnnotationRect> annotationRects;
    if (RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(element)) {
        if (RefPtr pluginViewBase = pluginElement ? pluginElement->pluginWidget() : nullptr) {
            for (auto& annotationRect : pluginViewBase->pdfAnnotationRectsForTesting())
                annotationRects.append({ annotationRect.x(), annotationRect.y(), annotationRect.width(), annotationRect.height() });
        }
    }
    return annotationRects;
}

void Internals::setPDFTextAnnotationValueForTesting(Element& element, unsigned pageIndex, unsigned annotationIndex, const String& value)
{
    RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (!pluginElement)
        return;

    RefPtr pluginView = pluginElement->pluginWidget();
    if (!pluginView)
        return;

    pluginView->setPDFTextAnnotationValueForTesting(pageIndex, annotationIndex, value);
}

void Internals::registerPDFTest(Ref<VoidCallback>&& callback, Element& element)
{
    RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (!pluginElement)
        return;

    if (RefPtr pluginViewBase = pluginElement->pluginWidget())
        pluginViewBase->registerPDFTestCallback(WTFMove(callback));
}

const String& Internals::defaultSpatialTrackingLabel() const
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    auto* document = contextDocument();
    if (!document)
        return nullString();
    if (RefPtr page = document->page())
        return page->defaultSpatialTrackingLabel();
#endif
    return nullString();
}

#if ENABLE(VIDEO)
bool Internals::isEffectivelyMuted(const HTMLMediaElement& element)
{
    return element.effectiveMuted();
}
#endif

std::optional<RenderingMode> Internals::getEffectiveRenderingModeOfNewlyCreatedAcceleratedImageBuffer()
{
    RefPtr document = contextDocument();
    if (!document || !document->page())
        return std::nullopt;

    if (RefPtr imageBuffer = ImageBuffer::create({ 100, 100 }, RenderingPurpose::DOM, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, ImageBufferOptions::Accelerated, &document->page()->chrome())) {
        imageBuffer->ensureBackendCreated();
        if (imageBuffer->hasBackend())
            return imageBuffer->renderingMode();
    }
    return std::nullopt;
}

void Internals::getImageBufferResourceLimits(ImageBufferResourceLimitsPromise&& promise)
{
    RefPtr document = contextDocument();
    if (!document || !document->page()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    document->page()->chrome().client().getImageBufferResourceLimitsForTesting([promise = WTFMove(promise)](auto&& limits) mutable {
        if (!limits) {
            promise.reject(Exception { ExceptionCode::InvalidStateError });
            return;
        }
        promise.resolve(*limits);
    });
}

void Internals::setResourceCachingDisabledByWebInspector(bool disabled)
{
    RefPtr document = contextDocument();
    if (!document || !document->page())
        return;

    document->page()->setResourceCachingDisabledByWebInspector(disabled);
}

void Internals::setTopDocumentURLForQuirks(const String& urlString)
{
    RefPtr document = contextDocument();
    if (!document)
        return;

    document->protectedPage()->settings().setNeedsSiteSpecificQuirks(true);
    document->quirks().setTopDocumentURLForTesting(URL { urlString });
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
