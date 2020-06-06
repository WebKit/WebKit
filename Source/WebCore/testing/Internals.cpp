/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "ActivityState.h"
#include "AnimationTimeline.h"
#include "ApplicationCacheStorage.h"
#include "AudioSession.h"
#include "AudioTrackPrivateMediaStream.h"
#include "Autofill.h"
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "BitmapImage.h"
#include "CSSAnimationController.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSPropertyParser.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CacheStorageConnection.h"
#include "CacheStorageProvider.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CertificateInfo.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientOrigin.h"
#include "ComposedTreeIterator.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DOMStringList.h"
#include "DOMURL.h"
#include "DOMWindow.h"
#include "DeprecatedGlobalSettings.h"
#include "DiagnosticLoggingClient.h"
#include "DisabledAdaptations.h"
#include "DisplayList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentTimeline.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "ExtendableEvent.h"
#include "ExtensionStyleSheets.h"
#include "FetchResponse.h"
#include "File.h"
#include "FontCache.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "GCObservation.h"
#include "GridPosition.h"
#include "HEVCUtilities.h"
#include "HTMLAnchorElement.h"
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
#include "HistoryController.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
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
#include "JSImageData.h"
#include "LegacySchemeRegistry.h"
#include "LibWebRTCProvider.h"
#include "LoaderStrategy.h"
#include "Location.h"
#include "MallocStatistics.h"
#include "MediaDevices.h"
#include "MediaEngineConfigurationFactory.h"
#include "MediaPlayer.h"
#include "MediaProducer.h"
#include "MediaRecorderProvider.h"
#include "MediaResourceLoader.h"
#include "MediaStreamTrack.h"
#include "MediaUsageInfo.h"
#include "MemoryCache.h"
#include "MemoryInfo.h"
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
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "Range.h"
#include "ReadableStream.h"
#include "RenderEmbeddedObject.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "SMILTimeContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGPathStringBuilder.h"
#include "SVGSVGElement.h"
#include "SWClientConnection.h"
#include "ScriptController.h"
#include "ScriptedAnimationController.h"
#include "ScrollingCoordinator.h"
#include "ScrollingMomentumCalculator.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "ServiceWorker.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerRegistrationData.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceBuffer.h"
#include "SpellChecker.h"
#include "StaticNodeList.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StringCallback.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "TextIterator.h"
#include "TextPlaceholderElement.h"
#include "TreeScope.h"
#include "TypeConversions.h"
#include "UserGestureIndicator.h"
#include "UserMediaController.h"
#include "ViewportArguments.h"
#include "VoidCallback.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include "WebCoreJSClientData.h"
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
#include <wtf/HexNumber.h>
#include <wtf/JSONValues.h>
#include <wtf/Language.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ProcessID.h>
#include <wtf/URLHelpers.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

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

#if ENABLE(VIDEO_TRACK)
#include "CaptionUserPreferences.h"
#include "PageGroup.h"
#include "TextTrack.h"
#include "TextTrackCueGeneric.h"
#endif

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#include "TimeRanges.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLRenderingContext.h"
#endif

#if ENABLE(SPEECH_SYNTHESIS)
#include "DOMWindowSpeechSynthesis.h"
#include "PlatformSpeechSynthesizerMock.h"
#include "SpeechSynthesis.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaRecorder.h"
#include "MediaRecorderPrivateMock.h"
#include "MediaStream.h"
#include "MockRealtimeMediaSourceCenter.h"
#endif

#if ENABLE(WEB_RTC)
#include "RTCPeerConnection.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MockMediaPlayerMediaSource.h"
#endif

#if ENABLE(CONTENT_FILTERING)
#include "MockContentFilterSettings.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioContext.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSession.h"
#include "MediaSessionManager.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContext.h"
#endif

#if ENABLE(POINTER_LOCK)
#include "PointerLockController.h"
#endif

#if ENABLE(INDEXED_DATABASE)
#include "IDBRequest.h"
#include "IDBTransaction.h"
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

#if PLATFORM(MAC) && USE(LIBWEBRTC)
#include <webrtc/sdk/WebKit/VideoProcessingSoftLink.h>
#endif

#if PLATFORM(MAC)
#include "GraphicsContextGLOpenGLManager.h"
#include "NSScrollerImpDetails.h"
#include "ScrollbarThemeMac.h"
#endif

#if PLATFORM(COCOA)
#include "SystemBattery.h"
#include <wtf/spi/darwin/SandboxSPI.h>
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


namespace WebCore {
using namespace Inspector;

using namespace HTMLNames;

class InspectorStubFrontend final : public InspectorFrontendClientLocal, public FrontendChannel {
public:
    InspectorStubFrontend(Page& inspectedPage, RefPtr<DOMWindow>&& frontendWindow);
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

    RefPtr<DOMWindow> m_frontendWindow;
};

InspectorStubFrontend::InspectorStubFrontend(Page& inspectedPage, RefPtr<DOMWindow>&& frontendWindow)
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
    dispatchMessageAsync(message);
}

static bool markerTypeFrom(const String& markerType, DocumentMarker::MarkerType& result)
{
    if (equalLettersIgnoringASCIICase(markerType, "spelling"))
        result = DocumentMarker::Spelling;
    else if (equalLettersIgnoringASCIICase(markerType, "grammar"))
        result = DocumentMarker::Grammar;
    else if (equalLettersIgnoringASCIICase(markerType, "textmatch"))
        result = DocumentMarker::TextMatch;
    else if (equalLettersIgnoringASCIICase(markerType, "replacement"))
        result = DocumentMarker::Replacement;
    else if (equalLettersIgnoringASCIICase(markerType, "correctionindicator"))
        result = DocumentMarker::CorrectionIndicator;
    else if (equalLettersIgnoringASCIICase(markerType, "rejectedcorrection"))
        result = DocumentMarker::RejectedCorrection;
    else if (equalLettersIgnoringASCIICase(markerType, "autocorrected"))
        result = DocumentMarker::Autocorrected;
    else if (equalLettersIgnoringASCIICase(markerType, "spellcheckingexemption"))
        result = DocumentMarker::SpellCheckingExemption;
    else if (equalLettersIgnoringASCIICase(markerType, "deletedautocorrection"))
        result = DocumentMarker::DeletedAutocorrection;
    else if (equalLettersIgnoringASCIICase(markerType, "dictationalternatives"))
        result = DocumentMarker::DictationAlternatives;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    else if (equalLettersIgnoringASCIICase(markerType, "telephonenumber"))
        result = DocumentMarker::TelephoneNumber;
#endif
    else
        return false;

    return true;
}

static bool markerTypesFrom(const String& markerType, OptionSet<DocumentMarker::MarkerType>& result)
{
    DocumentMarker::MarkerType singularResult;

    if (markerType.isEmpty() || equalLettersIgnoringASCIICase(markerType, "all"))
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

const char* Internals::internalsId = "internals";

Ref<Internals> Internals::create(Document& document)
{
    return adoptRef(*new Internals(document));
}

Internals::~Internals()
{
#if ENABLE(MEDIA_STREAM)
    stopObservingRealtimeMediaSource();
#endif
}

void Internals::resetToConsistentState(Page& page)
{
    page.setPageScaleFactor(1, IntPoint(0, 0));
    page.setPagination(Pagination());
    page.setPaginationLineGridEnabled(false);

    page.setDefersLoading(false);

    page.mainFrame().setTextZoomFactor(1.0f);

    page.setCompositingPolicyOverride(WebCore::CompositingPolicy::Normal);

    FrameView* mainFrameView = page.mainFrame().view();
    if (mainFrameView) {
        page.setHeaderHeight(0);
        page.setFooterHeight(0);
        page.setTopContentInset(0);
        mainFrameView->setUseFixedLayout(false);
        mainFrameView->setFixedLayoutSize(IntSize());
        mainFrameView->enableFixedWidthAutoSizeMode(false, { });
#if USE(COORDINATED_GRAPHICS)
        mainFrameView->setFixedVisibleContentRect(IntRect());
#endif
        if (auto* backing = mainFrameView->tiledBacking())
            backing->setTileSizeUpdateDelayDisabledForTesting(false);
    }

    WTF::clearDefaultPortForProtocolMapForTesting();
    overrideUserPreferredLanguages(Vector<String>());
    WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(false);
    WebCore::DeprecatedGlobalSettings::setUsesMockScrollAnimator(false);
#if ENABLE(VIDEO_TRACK)
    page.group().captionPreferences().setTestingMode(true);
    page.group().captionPreferences().setCaptionDisplayMode(CaptionUserPreferences::ForcedOnly);
    page.group().captionPreferences().setCaptionsStyleSheetOverride(emptyString());
    page.group().captionPreferences().setTestingMode(false);
#endif
    if (!page.mainFrame().editor().isContinuousSpellCheckingEnabled())
        page.mainFrame().editor().toggleContinuousSpellChecking();
    if (page.mainFrame().editor().isOverwriteModeEnabled())
        page.mainFrame().editor().toggleOverwriteModeEnabled();
    page.mainFrame().loader().clearTestingOverrides();
    page.applicationCacheStorage().setDefaultOriginQuota(ApplicationCacheStorage::noQuota());
#if ENABLE(VIDEO)
    PlatformMediaSessionManager::sharedManager().resetRestrictions();
    PlatformMediaSessionManager::sharedManager().setWillIgnoreSystemInterruptions(true);
#endif
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::sharedManager().setIsPlayingToAutomotiveHeadUnit(false);
#endif
#if ENABLE(ACCESSIBILITY)
    AXObjectCache::setEnhancedUserInterfaceAccessibility(false);
    AXObjectCache::disableAccessibility();
#endif

    MockPageOverlayClient::singleton().uninstallAllOverlays();

#if ENABLE(CONTENT_FILTERING)
    MockContentFilterSettings::reset();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    page.setMockMediaPlaybackTargetPickerEnabled(true);
    page.setMockMediaPlaybackTargetPickerState(emptyString(), MediaPlaybackTargetContext::Unknown);
#endif

    MediaResourceLoader::recordResponsesForTesting();

    page.setShowAllPlugins(false);
    page.setLowPowerModeEnabledOverrideForTesting(WTF::nullopt);
    page.setOutsideViewportThrottlingEnabledForTesting(false);

#if USE(QUICK_LOOK)
    MockPreviewLoaderClient::singleton().setPassword("");
    LegacyPreviewLoader::setClientForTesting(nullptr);
#endif

    printContextForTesting() = nullptr;

#if USE(LIBWEBRTC)
    auto& rtcProvider = page.libWebRTCProvider();
    WebCore::useRealRTCPeerConnectionFactory(rtcProvider);
    rtcProvider.disableNonLocalhostConnections();
    RuntimeEnabledFeatures::sharedFeatures().setWebRTCH265CodecEnabled(true);
    page.settings().setWebRTCEncryptionEnabled(true);
#endif

    page.setFullscreenAutoHideDuration(0_s);
    page.setFullscreenInsets({ });
    page.setFullscreenControlsHidden(false);

    MediaEngineConfigurationFactory::disableMock();

#if ENABLE(MEDIA_STREAM)
    RuntimeEnabledFeatures::sharedFeatures().setInterruptAudioOnPageVisibilityChangeEnabled(false);
    WebCore::MediaRecorder::setCustomPrivateRecorderCreator(nullptr);
    page.mediaRecorderProvider().setUseGPUProcess(true);
#endif

    HTMLCanvasElement::setMaxPixelMemoryForTesting(0); // This means use the default value.
    DOMWindow::overrideTransientActivationDurationForTesting(WTF::nullopt);
}

Internals::Internals(Document& document)
    : ContextDestructionObserver(&document)
#if ENABLE(MEDIA_STREAM)
    , m_orientationNotifier(0)
#endif
{
#if ENABLE(VIDEO_TRACK)
    if (document.page())
        document.page()->group().captionPreferences().setTestingMode(true);
#endif

#if ENABLE(MEDIA_STREAM)
    setMediaCaptureRequiresSecureConnection(false);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (document.page())
        document.page()->setMockMediaPlaybackTargetPickerEnabled(true);
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
        auto mockPaymentCoordinator = new MockPaymentCoordinator(*frame->page());
        frame->page()->setPaymentCoordinator(makeUnique<PaymentCoordinator>(*mockPaymentCoordinator));
    }
#endif

#if PLATFORM(COCOA) &&  ENABLE(WEB_AUDIO)
    AudioDestinationCocoa::createOverride = nullptr;
#endif
}

Document* Internals::contextDocument() const
{
    return downcast<Document>(scriptExecutionContext());
}

Frame* Internals::frame() const
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
    auto* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError, "No context document"_s };

    if (!document->svgExtensions())
        return Exception { NotFoundError, "No SVG animations"_s };

    return document->accessSVGExtensions().areAnimationsPaused();
}

ExceptionOr<double> Internals::svgAnimationsInterval(SVGSVGElement& element) const
{
    auto* document = contextDocument();
    if (!document)
        return 0;

    if (!document->svgExtensions())
        return 0;

    if (document->accessSVGExtensions().areAnimationsPaused())
        return 0;

    return element.timeContainer().animationFrameDelay().value();
}

String Internals::address(Node& node)
{
    return makeString("0x", hex(reinterpret_cast<uintptr_t>(&node)));
}

bool Internals::nodeNeedsStyleRecalc(Node& node)
{
    return node.needsStyleRecalc();
}

static String styleValidityToToString(Style::Validity validity)
{
    switch (validity) {
    case Style::Validity::Valid:
        return "NoStyleChange";
    case Style::Validity::ElementInvalid:
        return "InlineStyleChange";
    case Style::Validity::SubtreeInvalid:
        return "FullStyleChange";
    case Style::Validity::SubtreeAndRenderersInvalid:
        return "ReconstructRenderTree";
    }
    ASSERT_NOT_REACHED();
    return "";
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

bool Internals::isPreloaded(const String& url)
{
    Document* document = contextDocument();
    return document->cachedResourceLoader().isPreloaded(url);
}

bool Internals::isLoadingFromMemoryCache(const String& url)
{
    if (!contextDocument() || !contextDocument()->page())
        return false;

    ResourceRequest request(contextDocument()->completeURL(url));
    request.setDomainForCachePartition(contextDocument()->domainForCachePartition());

    CachedResource* resource = MemoryCache::singleton().resourceForRequest(request, contextDocument()->page()->sessionID());
    return resource && resource->status() == CachedResource::Cached;
}

static String responseSourceToString(const ResourceResponse& response)
{
    if (response.isNull())
        return "Null response";
    switch (response.source()) {
    case ResourceResponse::Source::Unknown:
        return "Unknown";
    case ResourceResponse::Source::Network:
        return "Network";
    case ResourceResponse::Source::ServiceWorker:
        return "Service worker";
    case ResourceResponse::Source::DiskCache:
        return "Disk cache";
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return "Disk cache after validation";
    case ResourceResponse::Source::MemoryCache:
        return "Memory cache";
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return "Memory cache after validation";
    case ResourceResponse::Source::ApplicationCache:
        return "Application cache";
    case ResourceResponse::Source::DOMCache:
        return "DOM cache";
    case ResourceResponse::Source::InspectorOverride:
        return "Inspector override";
    }
    ASSERT_NOT_REACHED();
    return "Error";
}

String Internals::xhrResponseSource(XMLHttpRequest& request)
{
    return responseSourceToString(request.resourceResponse());
}

String Internals::fetchResponseSource(FetchResponse& response)
{
    return responseSourceToString(response.resourceResponse());
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
        return Exception { InvalidAccessError };

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

void Internals::clearMemoryCache()
{
    MemoryCache::singleton().evictResources();
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
    auto* image = imageFromImageElement(element);
    return image && is<BitmapImage>(image) ? &downcast<BitmapImage>(*image) : nullptr;
}

#if USE(CG)
static PDFDocumentImage* pdfDocumentImageFromImageElement(HTMLImageElement& element)
{
    auto* image = imageFromImageElement(element);
    return image && is<PDFDocumentImage>(image) ? &downcast<PDFDocumentImage>(*image) : nullptr;
}
#endif

unsigned Internals::imageFrameIndex(HTMLImageElement& element)
{
    auto* bitmapImage = bitmapImageFromImageElement(element);
    return bitmapImage ? bitmapImage->currentFrame() : 0;
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
        bitmapImage->setFrameDecodingDurationForTesting(Seconds { duration });
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

unsigned Internals::pdfDocumentCachingCount(HTMLImageElement& element)
{
#if USE(CG)
    auto* pdfDocumentImage = pdfDocumentImageFromImageElement(element);
    return pdfDocumentImage ? pdfDocumentImage->cachingCountForTesting() : 0;
#else
    UNUSED_PARAM(element);
    return 0;
#endif
}

void Internals::setLargeImageAsyncDecodingEnabledForTesting(HTMLImageElement& element, bool enabled)
{
    if (auto* bitmapImage = bitmapImageFromImageElement(element))
        bitmapImage->setLargeImageAsyncDecodingEnabledForTesting(enabled);
}
    
void Internals::setForceUpdateImageDataEnabledForTesting(HTMLImageElement& element, bool enabled)
{
    if (auto* cachedImage = element.cachedImage())
        cachedImage->setForceUpdateImageDataEnabledForTesting(enabled);
}

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
        return Exception { InvalidAccessError };

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
    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled())
        return frame()->document()->timeline().numberOfActiveAnimationsForTesting();
    return frame()->legacyAnimation().numberOfActiveAnimations(frame()->document());
}

ExceptionOr<bool> Internals::animationsAreSuspended() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled())
        return document->ensureTimelinesController().animationsAreSuspended();
    return document->frame()->legacyAnimation().animationsAreSuspendedForDocument(document);
}

double Internals::animationsInterval() const
{
    Document* document = contextDocument();
    if (!document)
        return INFINITY;

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        if (auto timeline = document->existingTimeline())
            return timeline->animationInterval().seconds();
        return INFINITY;
    }

    if (!document->frame())
        return INFINITY;
    return document->frame()->legacyAnimation().animationInterval().value();
}

ExceptionOr<void> Internals::suspendAnimations() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        document->ensureTimelinesController().suspendAnimations();
        for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
            if (Document* document = frame->document())
                document->ensureTimelinesController().suspendAnimations();
        }
    } else {
        document->frame()->legacyAnimation().suspendAnimationsForDocument(document);

        for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
            if (Document* document = frame->document())
                frame->legacyAnimation().suspendAnimationsForDocument(document);
        }
    }

    return { };
}

ExceptionOr<void> Internals::resumeAnimations() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        document->ensureTimelinesController().resumeAnimations();
        for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
            if (Document* document = frame->document())
                document->ensureTimelinesController().resumeAnimations();
        }
    } else {
        document->frame()->legacyAnimation().resumeAnimationsForDocument(document);

        for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
            if (Document* document = frame->document())
                frame->legacyAnimation().resumeAnimationsForDocument(document);
        }
    }

    return { };
}

ExceptionOr<bool> Internals::pauseAnimationAtTimeOnElement(const String& animationName, double pauseTime, Element& element)
{
    if (pauseTime < 0)
        return Exception { InvalidAccessError };
    return frame()->legacyAnimation().pauseAnimationAtTime(element, AtomString(animationName), pauseTime);
}

ExceptionOr<bool> Internals::pauseAnimationAtTimeOnPseudoElement(const String& animationName, double pauseTime, Element& element, const String& pseudoId)
{
    if (pauseTime < 0)
        return Exception { InvalidAccessError };

    if (pseudoId != "before" && pseudoId != "after")
        return Exception { InvalidAccessError };

    PseudoElement* pseudoElement = pseudoId == "before" ? element.beforePseudoElement() : element.afterPseudoElement();
    if (!pseudoElement)
        return Exception { InvalidAccessError };

    return frame()->legacyAnimation().pauseAnimationAtTime(*pseudoElement, AtomString(animationName), pauseTime);
}

ExceptionOr<bool> Internals::pauseTransitionAtTimeOnElement(const String& propertyName, double pauseTime, Element& element)
{
    if (pauseTime < 0)
        return Exception { InvalidAccessError };
    return frame()->legacyAnimation().pauseTransitionAtTime(element, propertyName, pauseTime);
}

ExceptionOr<bool> Internals::pauseTransitionAtTimeOnPseudoElement(const String& property, double pauseTime, Element& element, const String& pseudoId)
{
    if (pauseTime < 0)
        return Exception { InvalidAccessError };

    if (pseudoId != "before" && pseudoId != "after")
        return Exception { InvalidAccessError };

    PseudoElement* pseudoElement = pseudoId == "before" ? element.beforePseudoElement() : element.afterPseudoElement();
    if (!pseudoElement)
        return Exception { InvalidAccessError };

    return frame()->legacyAnimation().pauseTransitionAtTime(*pseudoElement, property, pauseTime);
}

Vector<Internals::AcceleratedAnimation> Internals::acceleratedAnimationsForElement(Element& element)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled())
        return { };

    Vector<Internals::AcceleratedAnimation> animations;
    for (const auto& animationAsPair : element.document().timeline().acceleratedAnimationsForElement(element))
        animations.append({ animationAsPair.first, animationAsPair.second });
    return animations;
}

unsigned Internals::numberOfAnimationTimelineInvalidations() const
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled())
        return frame()->document()->timeline().numberOfAnimationTimelineInvalidationsForTesting();
    return 0;
}

double Internals::timeToNextAnimationTick(WebAnimation& animation) const
{
    return secondsToWebAnimationsAPITime(animation.timeToNextTick());
}

ExceptionOr<RefPtr<Element>> Internals::pseudoElement(Element& element, const String& pseudoId)
{
    if (pseudoId != "before" && pseudoId != "after")
        return Exception { InvalidAccessError };

    return pseudoId == "before" ? element.beforePseudoElement() : element.afterPseudoElement();
}

ExceptionOr<String> Internals::elementRenderTreeAsText(Element& element)
{
    element.document().updateStyleIfNeeded();

    String representation = externalRepresentation(&element);
    if (representation.isEmpty())
        return Exception { InvalidAccessError };

    return representation;
}

bool Internals::hasPausedImageAnimations(Element& element)
{
    return element.renderer() && element.renderer()->hasPausedImageAnimations();
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

Ref<CSSComputedStyleDeclaration> Internals::computedStyleIncludingVisitedInfo(Element& element) const
{
    bool allowVisitedStyle = true;
    return CSSComputedStyleDeclaration::create(element, allowVisitedStyle);
}

Node* Internals::ensureUserAgentShadowRoot(Element& host)
{
    return &host.ensureUserAgentShadowRoot();
}

Node* Internals::shadowRoot(Element& host)
{
    return host.shadowRoot();
}

ExceptionOr<String> Internals::shadowRootType(const Node& root) const
{
    if (!is<ShadowRoot>(root))
        return Exception { InvalidAccessError };

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

String Internals::shadowPseudoId(Element& element)
{
    return element.shadowPseudoId().string();
}

void Internals::setShadowPseudoId(Element& element, const String& id)
{
    return element.setPseudo(id);
}

static unsigned deferredStyleRulesCountForList(const Vector<RefPtr<StyleRuleBase>>& childRules)
{
    unsigned count = 0;
    for (auto rule : childRules) {
        if (is<StyleRule>(rule)) {
            auto* cssRule = downcast<StyleRule>(rule.get());
            if (!cssRule->propertiesWithoutDeferredParsing())
                count++;
            continue;
        }

        StyleRuleGroup* groupRule = nullptr;
        if (is<StyleRuleMedia>(rule))
            groupRule = downcast<StyleRuleMedia>(rule.get());
        else if (is<StyleRuleSupports>(rule))
            groupRule = downcast<StyleRuleSupports>(rule.get());
        if (!groupRule)
            continue;

        auto* groupChildRules = groupRule->childRulesWithoutDeferredParsing();
        if (!groupChildRules)
            continue;

        count += deferredStyleRulesCountForList(*groupChildRules);
    }

    return count;
}

unsigned Internals::deferredStyleRulesCount(StyleSheet& styleSheet)
{
    return deferredStyleRulesCountForList(downcast<CSSStyleSheet>(styleSheet).contents().childRules());
}

static unsigned deferredGroupRulesCountForList(const Vector<RefPtr<StyleRuleBase>>& childRules)
{
    unsigned count = 0;
    for (auto rule : childRules) {
        StyleRuleGroup* groupRule = nullptr;
        if (is<StyleRuleMedia>(rule))
            groupRule = downcast<StyleRuleMedia>(rule.get());
        else if (is<StyleRuleSupports>(rule))
            groupRule = downcast<StyleRuleSupports>(rule.get());
        if (!groupRule)
            continue;

        auto* groupChildRules = groupRule->childRulesWithoutDeferredParsing();
        if (!groupChildRules)
            count++;
        else
            count += deferredGroupRulesCountForList(*groupChildRules);
    }
    return count;
}

unsigned Internals::deferredGroupRulesCount(StyleSheet& styleSheet)
{
    return deferredGroupRulesCountForList(downcast<CSSStyleSheet>(styleSheet).contents().childRules());
}

static unsigned deferredKeyframesRulesCountForList(const Vector<RefPtr<StyleRuleBase>>& childRules)
{
    unsigned count = 0;
    for (auto rule : childRules) {
        if (is<StyleRuleKeyframes>(rule)) {
            auto* cssRule = downcast<StyleRuleKeyframes>(rule.get());
            if (!cssRule->keyframesWithoutDeferredParsing())
                count++;
            continue;
        }

        StyleRuleGroup* groupRule = nullptr;
        if (is<StyleRuleMedia>(rule))
            groupRule = downcast<StyleRuleMedia>(rule.get());
        else if (is<StyleRuleSupports>(rule))
            groupRule = downcast<StyleRuleSupports>(rule.get());
        if (!groupRule)
            continue;

        auto* groupChildRules = groupRule->childRulesWithoutDeferredParsing();
        if (!groupChildRules)
            continue;

        count += deferredKeyframesRulesCountForList(*groupChildRules);
    }

    return count;
}

unsigned Internals::deferredKeyframesRulesCount(StyleSheet& styleSheet)
{
    StyleSheetContents& contents = downcast<CSSStyleSheet>(styleSheet).contents();
    return deferredKeyframesRulesCountForList(contents.childRules());
}

ExceptionOr<bool> Internals::isTimerThrottled(int timeoutId)
{
    auto* timer = scriptExecutionContext()->findTimeout(timeoutId);
    if (!timer)
        return Exception { NotFoundError };

    if (timer->intervalClampedToMinimum() > timer->m_originalInterval)
        return true;

    return !!timer->alignedFireTime(MonotonicTime { });
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

void Internals::setEventThrottlingBehaviorOverride(Optional<EventThrottlingBehavior> value)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;

    if (!value) {
        document->page()->setEventThrottlingBehaviorOverride(WTF::nullopt);
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

Optional<Internals::EventThrottlingBehavior> Internals::eventThrottlingBehaviorOverride() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return WTF::nullopt;

    auto behavior = document->page()->eventThrottlingBehaviorOverride();
    if (!behavior)
        return WTF::nullopt;

    switch (behavior.value()) {
    case WebCore::EventThrottlingBehavior::Responsive:
        return Internals::EventThrottlingBehavior::Responsive;
    case WebCore::EventThrottlingBehavior::Unresponsive:
        return Internals::EventThrottlingBehavior::Unresponsive;
    }

    return WTF::nullopt;
}

String Internals::visiblePlaceholder(Element& element)
{
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

ExceptionOr<Vector<String>> Internals::formControlStateOfPreviousHistoryItem()
{
    HistoryItem* mainItem = frame()->loader().history().previousItem();
    if (!mainItem)
        return Exception { InvalidAccessError };
    String uniqueName = frame()->tree().uniqueName();
    if (mainItem->target() != uniqueName && !mainItem->childItemWithTarget(uniqueName))
        return Exception { InvalidAccessError };
    return Vector<String> { mainItem->target() == uniqueName ? mainItem->documentState() : mainItem->childItemWithTarget(uniqueName)->documentState() };
}

ExceptionOr<void> Internals::setFormControlStateOfPreviousHistoryItem(const Vector<String>& state)
{
    HistoryItem* mainItem = frame()->loader().history().previousItem();
    if (!mainItem)
        return Exception { InvalidAccessError };
    String uniqueName = frame()->tree().uniqueName();
    if (mainItem->target() == uniqueName)
        mainItem->setDocumentState(state);
    else if (HistoryItem* subItem = mainItem->childItemWithTarget(uniqueName))
        subItem->setDocumentState(state);
    else
        return Exception { InvalidAccessError };
    return { };
}

#if ENABLE(SPEECH_SYNTHESIS)

void Internals::enableMockSpeechSynthesizer()
{
    Document* document = contextDocument();
    if (!document || !document->domWindow())
        return;
    SpeechSynthesis* synthesis = DOMWindowSpeechSynthesis::speechSynthesis(*document->domWindow());
    if (!synthesis)
        return;

    synthesis->setPlatformSynthesizer(makeUnique<PlatformSpeechSynthesizerMock>(synthesis));
}

#endif

#if ENABLE(WEB_RTC)

void Internals::emulateRTCPeerConnectionPlatformEvent(RTCPeerConnection& connection, const String& action)
{
    if (!LibWebRTCProvider::webRTCAvailable())
        return;

    connection.emulatePlatformEvent(action);
}

void Internals::useMockRTCPeerConnectionFactory(const String& testCase)
{
    if (!LibWebRTCProvider::webRTCAvailable())
        return;

#if USE(LIBWEBRTC)
    Document* document = contextDocument();
    LibWebRTCProvider* provider = (document && document->page()) ? &document->page()->libWebRTCProvider() : nullptr;
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
    auto& rtcProvider = page->libWebRTCProvider();
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
#if USE(LIBWEBRTC)
    if (auto* page = contextDocument()->page())
        page->libWebRTCProvider().clearFactory();
#endif
}

void Internals::applyRotationForOutgoingVideoSources(RTCPeerConnection& connection)
{
    connection.applyRotationForOutgoingVideoSources();
}

void Internals::setEnableWebRTCEncryption(bool value)
{
#if USE(LIBWEBRTC)
    if (auto* page = contextDocument()->page())
        page->settings().setWebRTCEncryptionEnabled(value);
#endif
}

void Internals::setUseDTLS10(bool useDTLS10)
{
#if USE(LIBWEBRTC)
    auto* document = contextDocument();
    if (!document || !document->page())
        return;
    document->page()->libWebRTCProvider().setUseDTLS10(useDTLS10);
#endif
}

void Internals::setUseGPUProcessForWebRTC(bool useGPUProcess)
{
#if USE(LIBWEBRTC)
    auto* document = contextDocument();
    if (!document || !document->page())
        return;

    document->page()->mediaRecorderProvider().setUseGPUProcess(useGPUProcess);
#endif
}
#endif

#if ENABLE(MEDIA_STREAM)
void Internals::setShouldInterruptAudioOnPageVisibilityChange(bool shouldInterrupt)
{
    RuntimeEnabledFeatures::sharedFeatures().setInterruptAudioOnPageVisibilityChangeEnabled(shouldInterrupt);
}

void Internals::setMediaCaptureRequiresSecureConnection(bool enabled)
{
    Document* document = contextDocument();
    if (auto* page = document->page())
        page->settings().setMediaCaptureRequiresSecureConnection(enabled);
}

static std::unique_ptr<MediaRecorderPrivate> createRecorderMockSource(MediaStreamPrivate& stream)
{
    return std::unique_ptr<MediaRecorderPrivateMock>(new MediaRecorderPrivateMock(stream));
}

void Internals::setCustomPrivateRecorderCreator()
{
    WebCore::MediaRecorder::setCustomPrivateRecorderCreator(createRecorderMockSource);
}

#endif

ExceptionOr<Ref<DOMRect>> Internals::absoluteCaretBounds()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    return DOMRect::create(document->frame()->selection().absoluteCaretBounds());
}
    
ExceptionOr<bool> Internals::isCaretBlinkingSuspended()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };
    
    return document->frame()->selection().isCaretBlinkingSuspended();
}

Ref<DOMRect> Internals::boundingBox(Element& element)
{
    element.document().updateLayoutIgnorePendingStylesheets();
    auto renderer = element.renderer();
    if (!renderer)
        return DOMRect::create();
    return DOMRect::create(renderer->absoluteBoundingBoxRectIgnoringTransforms());
}

ExceptionOr<Ref<DOMRectList>> Internals::inspectorHighlightRects()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    Highlight highlight;
    document->page()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::View);
    return DOMRectList::create(highlight.quads);
}

ExceptionOr<unsigned> Internals::markerCountForNode(Node& node, const String& markerType)
{
    OptionSet<DocumentMarker::MarkerType> markerTypes;
    if (!markerTypesFrom(markerType, markerTypes))
        return Exception { SyntaxError };

    node.document().editor().updateEditorUINowIfScheduled();
    return node.document().markers().markersFor(node, markerTypes).size();
}

ExceptionOr<RenderedDocumentMarker*> Internals::markerAt(Node& node, const String& markerType, unsigned index)
{
    node.document().updateLayoutIgnorePendingStylesheets();

    OptionSet<DocumentMarker::MarkerType> markerTypes;
    if (!markerTypesFrom(markerType, markerTypes))
        return Exception { SyntaxError };

    node.document().editor().updateEditorUINowIfScheduled();

    Vector<RenderedDocumentMarker*> markers = node.document().markers().markersFor(node, markerTypes);
    if (markers.size() <= index)
        return nullptr;
    return markers[index];
}

ExceptionOr<RefPtr<Range>> Internals::markerRangeForNode(Node& node, const String& markerType, unsigned index)
{
    auto result = markerAt(node, markerType, index);
    if (result.hasException())
        return result.releaseException();
    auto marker = result.releaseReturnValue();
    if (!marker)
        return nullptr;
    return RefPtr<Range> { Range::create(node.document(), &node, marker->startOffset(), &node, marker->endOffset()) };
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
    DocumentMarker::MarkerType markerType;
    if (!markerTypeFrom(markerTypeString, markerType))
        return Exception { SyntaxError };

    contextDocument()->markers().updateRectsForInvalidatedMarkersOfType(markerType);
    auto rects = contextDocument()->markers().renderedRectsForMarkers(markerType);

    // FIXME: Using fixed precision here for width because of test results that contain numbers with specific precision. Would be nice to update the test results and move to default formatting.
    StringBuilder rectString;
    rectString.appendLiteral("marker rects: ");
    for (const auto& rect : rects)
        rectString.append('(', rect.x(), ", ", rect.y(), ", ", FormattedNumber::fixedPrecision(rect.width()), ", ", rect.height(), ") ");
    return rectString.toString();
}

ExceptionOr<void> Internals::setMarkedTextMatchesAreHighlighted(bool flag)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };
    document->editor().setMarkedTextMatchesAreHighlighted(flag);
    return { };
}

void Internals::invalidateFontCache()
{
    FontCache::singleton().invalidate();
}

void Internals::setFontSmoothingEnabled(bool enabled)
{
    FontCascade::setShouldUseSmoothing(enabled);
}

ExceptionOr<void> Internals::setLowPowerModeEnabled(bool isEnabled)
{
    auto* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };
    auto* page = document->page();
    if (!page)
        return Exception { InvalidAccessError };

    page->setLowPowerModeEnabledOverrideForTesting(isEnabled);
    return { };
}

ExceptionOr<void> Internals::setOutsideViewportThrottlingEnabled(bool isEnabled)
{
    auto* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };
    auto* page = document->page();
    if (!page)
        return Exception { InvalidAccessError };

    page->setOutsideViewportThrottlingEnabledForTesting(isEnabled);
    return { };
}

ExceptionOr<void> Internals::setScrollViewPosition(int x, int y)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    auto& frameView = *document->view();
    bool constrainsScrollingToContentEdgeOldValue = frameView.constrainsScrollingToContentEdge();
    bool scrollbarsSuppressedOldValue = frameView.scrollbarsSuppressed();

    frameView.setConstrainsScrollingToContentEdge(false);
    frameView.setScrollbarsSuppressed(false);
    frameView.setScrollOffsetFromInternals({ x, y });
    frameView.setScrollbarsSuppressed(scrollbarsSuppressedOldValue);
    frameView.setConstrainsScrollingToContentEdge(constrainsScrollingToContentEdgeOldValue);

    return { };
}

ExceptionOr<void> Internals::unconstrainedScrollTo(Element& element, double x, double y)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    element.scrollTo(ScrollToOptions(x, y), ScrollClamping::Unclamped);

    auto& frameView = *document->view();
    frameView.setViewportConstrainedObjectsNeedLayout();

    return { };
}

ExceptionOr<Ref<DOMRect>> Internals::layoutViewportRect()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->updateLayoutIgnorePendingStylesheets();

    auto& frameView = *document->view();
    return DOMRect::create(frameView.layoutViewportRect());
}

ExceptionOr<Ref<DOMRect>> Internals::visualViewportRect()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->updateLayoutIgnorePendingStylesheets();

    auto& frameView = *document->view();
    return DOMRect::create(frameView.visualViewportRect());
}

ExceptionOr<void> Internals::setViewIsTransparent(bool transparent)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };
    Optional<Color> backgroundColor;
    if (transparent)
        backgroundColor = Color(Color::transparent);
    document->view()->updateBackgroundRecursively(backgroundColor);
    return { };
}

ExceptionOr<String> Internals::viewBaseBackgroundColor()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };
    return document->view()->baseBackgroundColor().cssText();
}

ExceptionOr<void> Internals::setViewBaseBackgroundColor(const String& colorValue)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    if (colorValue == "transparent") {
        document->view()->setBaseBackgroundColor(Color::transparent);
        return { };
    }
    if (colorValue == "white") {
        document->view()->setBaseBackgroundColor(Color::white);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> Internals::setPagination(const String& mode, int gap, int pageLength)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    Pagination pagination;
    if (mode == "Unpaginated")
        pagination.mode = Pagination::Unpaginated;
    else if (mode == "LeftToRightPaginated")
        pagination.mode = Pagination::LeftToRightPaginated;
    else if (mode == "RightToLeftPaginated")
        pagination.mode = Pagination::RightToLeftPaginated;
    else if (mode == "TopToBottomPaginated")
        pagination.mode = Pagination::TopToBottomPaginated;
    else if (mode == "BottomToTopPaginated")
        pagination.mode = Pagination::BottomToTopPaginated;
    else
        return Exception { SyntaxError };

    pagination.gap = gap;
    pagination.pageLength = pageLength;
    document->page()->setPagination(pagination);

    return { };
}

ExceptionOr<void> Internals::setPaginationLineGridEnabled(bool enabled)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };
    document->page()->setPaginationLineGridEnabled(enabled);
    return { };
}

ExceptionOr<String> Internals::configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    const int defaultLayoutWidthForNonMobilePages = 980;

    ViewportArguments arguments = document->page()->viewportArguments();
    ViewportAttributes attributes = computeViewportAttributes(arguments, defaultLayoutWidthForNonMobilePages, deviceWidth, deviceHeight, devicePixelRatio, IntSize(availableWidth, availableHeight));
    restrictMinimumScaleFactorToViewportSize(attributes, IntSize(availableWidth, availableHeight), devicePixelRatio);
    restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    // FIXME: Using fixed precision here because of test results that contain numbers with specific precision. Would be nice to update the test results and move to default formatting.
    return makeString("viewport size ", FormattedNumber::fixedPrecision(attributes.layoutSize.width()), 'x', FormattedNumber::fixedPrecision(attributes.layoutSize.height()), " scale ", FormattedNumber::fixedPrecision(attributes.initialScale), " with limits [", FormattedNumber::fixedPrecision(attributes.minimumScale), ", ", FormattedNumber::fixedPrecision(attributes.maximumScale), "] and userScalable ", (attributes.userScalable ? "true" : "false"));
}

ExceptionOr<bool> Internals::wasLastChangeUserEdit(Element& textField)
{
    if (is<HTMLInputElement>(textField))
        return downcast<HTMLInputElement>(textField).lastChangeWasUserEdit();

    if (is<HTMLTextAreaElement>(textField))
        return downcast<HTMLTextAreaElement>(textField).lastChangeWasUserEdit();

    return Exception { InvalidNodeTypeError };
}

bool Internals::elementShouldAutoComplete(HTMLInputElement& element)
{
    return element.shouldAutocomplete();
}

void Internals::setAutofilled(HTMLInputElement& element, bool enabled)
{
    element.setAutoFilled(enabled);
}

void Internals::setAutoFilledAndViewable(HTMLInputElement& element, bool enabled)
{
    element.setAutoFilledAndViewable(enabled);
}

static AutoFillButtonType toAutoFillButtonType(Internals::AutoFillButtonType type)
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
    }
    ASSERT_NOT_REACHED();
    return AutoFillButtonType::None;
}

static Internals::AutoFillButtonType toInternalsAutoFillButtonType(AutoFillButtonType type)
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
    }
    ASSERT_NOT_REACHED();
    return Internals::AutoFillButtonType::None;
}

void Internals::setShowAutoFillButton(HTMLInputElement& element, AutoFillButtonType type)
{
    element.setShowAutoFillButton(toAutoFillButtonType(type));
}

auto Internals::autoFillButtonType(const HTMLInputElement& element) -> AutoFillButtonType
{
    return toInternalsAutoFillButtonType(element.autoFillButtonType());
}

auto Internals::lastAutoFillButtonType(const HTMLInputElement& element) -> AutoFillButtonType
{
    return toInternalsAutoFillButtonType(element.lastAutoFillButtonType());
}

ExceptionOr<void> Internals::scrollElementToRect(Element& element, int x, int y, int w, int h)
{
    FrameView* frameView = element.document().view();
    if (!frameView)
        return Exception { InvalidAccessError };
    frameView->scrollElementToRect(element, { x, y, w, h });
    return { };
}

ExceptionOr<String> Internals::autofillFieldName(Element& element)
{
    if (!is<HTMLFormControlElement>(element))
        return Exception { InvalidNodeTypeError };

    return String { downcast<HTMLFormControlElement>(element).autofillData().fieldName };
}

ExceptionOr<void> Internals::invalidateControlTints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    document->view()->invalidateControlTints();
    return { };
}

RefPtr<Range> Internals::rangeFromLocationAndLength(Element& scope, unsigned rangeLocation, unsigned rangeLength)
{
    return createLiveRange(resolveCharacterRange(makeRangeSelectingNodeContents(scope), { rangeLocation, rangeLength }));
}

unsigned Internals::locationFromRange(Element& scope, const Range& range)
{
    return clampTo<unsigned>(characterRange(makeBoundaryPointBeforeNodeContents(scope), range).location);
}

unsigned Internals::lengthFromRange(Element& scope, const Range& range)
{
    return clampTo<unsigned>(characterRange(makeBoundaryPointBeforeNodeContents(scope), range).length);
}

String Internals::rangeAsText(const Range& range)
{
    return range.text();
}

String Internals::rangeAsTextUsingBackwardsTextIterator(const Range& range)
{
    String result;
    for (SimplifiedBackwardsTextIterator backwardsIterator(range); !backwardsIterator.atEnd(); backwardsIterator.advance())
        result.insert(backwardsIterator.text().toString(), 0);
    return result;
}

Ref<Range> Internals::subrange(Range& range, unsigned rangeLocation, unsigned rangeLength)
{
    return createLiveRange(resolveCharacterRange(range, { rangeLocation, rangeLength }));
}

RefPtr<Range> Internals::rangeOfStringNearLocation(const Range& searchRange, const String& text, unsigned targetOffset)
{
    return createLiveRange(findClosestPlainText(searchRange, text, { }, targetOffset));
}

#if !PLATFORM(MAC)
ExceptionOr<RefPtr<Range>> Internals::rangeForDictionaryLookupAtLocation(int, int)
{
    return Exception { InvalidAccessError };
}
#endif

ExceptionOr<void> Internals::setDelegatesScrolling(bool enabled)
{
    Document* document = contextDocument();
    // Delegate scrolling is valid only on mainframe's view.
    if (!document || !document->view() || !document->page() || &document->page()->mainFrame() != document->frame())
        return Exception { InvalidAccessError };

    document->view()->setDelegatesScrolling(enabled);
    return { };
}

ExceptionOr<uint64_t> Internals::lastSpellCheckRequestSequence()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    return document->editor().spellChecker().lastRequestIdentifier().toUInt64();
}

ExceptionOr<uint64_t> Internals::lastSpellCheckProcessedSequence()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    return document->editor().spellChecker().lastProcessedIdentifier().toUInt64();
}

Vector<String> Internals::userPreferredLanguages() const
{
    return WTF::userPreferredLanguages();
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
#if ENABLE(VIDEO_TRACK)
    return document->page()->group().captionPreferences().preferredAudioCharacteristics();
#else
    return Vector<String>();
#endif
}

void Internals::setUserPreferredAudioCharacteristic(const String& characteristic)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;
#if ENABLE(VIDEO_TRACK)
    document->page()->group().captionPreferences().setPreferredAudioCharacteristic(characteristic);
#else
    UNUSED_PARAM(characteristic);
#endif
}

ExceptionOr<unsigned> Internals::wheelEventHandlerCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    return document->wheelEventHandlerCount();
}

ExceptionOr<unsigned> Internals::touchEventHandlerCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    return document->touchEventHandlerCount();
}

ExceptionOr<Ref<DOMRectList>> Internals::touchEventRectsForEvent(const String& eventName)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    return document->page()->touchEventRectsForEventForTesting(eventName);
}

ExceptionOr<Ref<DOMRectList>> Internals::passiveTouchEventListenerRects()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    return document->page()->passiveTouchEventListenerRectsForTesting();
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
ExceptionOr<RefPtr<NodeList>> Internals::nodesFromRect(Document& document, int centerX, int centerY, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowUserAgentShadowContent, bool allowChildFrameContent) const
{
    if (!document.frame() || !document.frame()->view())
        return Exception { InvalidAccessError };

    Frame* frame = document.frame();
    FrameView* frameView = document.view();
    RenderView* renderView = document.renderView();
    if (!renderView)
        return nullptr;

    document.updateLayoutIgnorePendingStylesheets();

    float zoomFactor = frame->pageZoomFactor();
    LayoutPoint point(centerX * zoomFactor + frameView->scrollX(), centerY * zoomFactor + frameView->scrollY());

    OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::CollectMultipleElements };
    if (ignoreClipping)
        hitType.add(HitTestRequest::IgnoreClipping);
    if (!allowUserAgentShadowContent)
        hitType.add(HitTestRequest::DisallowUserAgentShadowContent);
    if (allowChildFrameContent)
        hitType.add(HitTestRequest::AllowChildFrameContent);

    HitTestRequest request(hitType);

    // When ignoreClipping is false, this method returns null for coordinates outside of the viewport.
    if (!request.ignoreClipping() && !frameView->visibleContentRect().intersects(HitTestLocation::rectForPoint(point, topPadding, rightPadding, bottomPadding, leftPadding)))
        return nullptr;

    HitTestResult result(point, topPadding, rightPadding, bottomPadding, leftPadding);
    document.hitTest(request, result);
    auto matches = WTF::map(result.listBasedTestResult(), [](const auto& node) { return node.copyRef(); });
    return RefPtr<NodeList> { StaticNodeList::create(WTFMove(matches)) };
}

class GetCallerCodeBlockFunctor {
public:
    GetCallerCodeBlockFunctor()
        : m_iterations(0)
        , m_codeBlock(0)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        ++m_iterations;
        if (m_iterations < 2)
            return StackVisitor::Continue;

        m_codeBlock = visitor->codeBlock();
        return StackVisitor::Done;
    }

    CodeBlock* codeBlock() const { return m_codeBlock; }

private:
    mutable int m_iterations;
    mutable CodeBlock* m_codeBlock;
};

String Internals::parserMetaData(JSC::JSValue code)
{
    JSC::VM& vm = contextDocument()->vm();
    JSC::CallFrame* callFrame = vm.topCallFrame;
    JSC::JSGlobalObject* globalObject = callFrame->lexicalGlobalObject(vm);
    ScriptExecutable* executable;

    if (!code || code.isNull() || code.isUndefined()) {
        GetCallerCodeBlockFunctor iter;
        callFrame->iterate(vm, iter);
        CodeBlock* codeBlock = iter.codeBlock();
        executable = codeBlock->ownerExecutable();
    } else if (code.isCallable(vm)) {
        JSFunction* funcObj = JSC::jsCast<JSFunction*>(code.toObject(globalObject));
        executable = funcObj->jsExecutable();
    } else
        return String();

    unsigned startLine = executable->firstLine();
    unsigned startColumn = executable->startColumn();
    unsigned endLine = executable->lastLine();
    unsigned endColumn = executable->endColumn();

    StringBuilder result;

    if (executable->isFunctionExecutable()) {
        FunctionExecutable* funcExecutable = reinterpret_cast<FunctionExecutable*>(executable);
        String inferredName = funcExecutable->ecmaName().string();
        result.appendLiteral("function \"");
        result.append(inferredName);
        result.append('"');
    } else if (executable->isEvalExecutable())
        result.appendLiteral("eval");
    else if (executable->isModuleProgramExecutable())
        result.appendLiteral("module");
    else if (executable->isProgramExecutable())
        result.appendLiteral("program");
    else
        ASSERT_NOT_REACHED();

    result.appendLiteral(" { ");
    result.appendNumber(startLine);
    result.append(':');
    result.appendNumber(startColumn);
    result.appendLiteral(" - ");
    result.appendNumber(endLine);
    result.append(':');
    result.appendNumber(endColumn);
    result.appendLiteral(" }");

    return result.toString();
}

void Internals::updateEditorUINowIfScheduled()
{
    if (Document* document = contextDocument()) {
        if (Frame* frame = document->frame())
            frame->editor().updateEditorUINowIfScheduled();
    }
}

bool Internals::hasSpellingMarker(int from, int length)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->editor().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

bool Internals::hasAutocorrectedMarker(int from, int length)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->editor().selectionStartHasMarkerFor(DocumentMarker::Autocorrected, from, length);
}

bool Internals::hasDictationAlternativesMarker(int from, int length)
{
    auto* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::DictationAlternatives, from, length);
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
    if (auto frame = makeRefPtr(this->frame()))
        frame->editor().changeSelectionListType();
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
        const char* name;
        FindOptionFlag value;
    } flagList[] = {
        {"CaseInsensitive", CaseInsensitive},
        {"AtWordStarts", AtWordStarts},
        {"TreatMedialCapitalAsWordStart", TreatMedialCapitalAsWordStart},
        {"Backwards", Backwards},
        {"WrapAround", WrapAround},
        {"StartInSelection", StartInSelection},
        {"DoNotRevealSelection", DoNotRevealSelection},
        {"AtWordEnds", AtWordEnds},
        {"DoNotTraverseFlatTree", DoNotTraverseFlatTree},
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
            return Exception { SyntaxError };
    }
    return result;
}

ExceptionOr<RefPtr<Range>> Internals::rangeOfString(const String& text, RefPtr<Range>&& referenceRange, const Vector<String>& findOptions)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    return document->editor().rangeOfString(text, referenceRange.get(), parsedOptions.releaseReturnValue());
}

ExceptionOr<unsigned> Internals::countMatchesForText(const String& text, const Vector<String>& findOptions, const String& markMatches)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    bool mark = markMatches == "mark";
    return document->editor().countMatchesForText(text, nullptr, parsedOptions.releaseReturnValue(), 1000, mark, nullptr);
}

ExceptionOr<unsigned> Internals::countFindMatches(const String& text, const Vector<String>& findOptions)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    return document->page()->countFindMatches(text, parsedOptions.releaseReturnValue(), 1000);
}

#if ENABLE(INDEXED_DATABASE)
unsigned Internals::numberOfIDBTransactions() const
{
    return IDBTransaction::numberOfIDBTransactions;
}
#endif

unsigned Internals::numberOfLiveNodes() const
{
    unsigned nodeCount = 0;
    for (auto* document : Document::allDocuments())
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

#if ENABLE(INTERSECTION_OBSERVER)
unsigned Internals::numberOfIntersectionObservers(const Document& document) const
{
    return document.numberOfIntersectionObservers();
}
#endif

uint64_t Internals::documentIdentifier(const Document& document) const
{
    return document.identifier().toUInt64();
}

bool Internals::isDocumentAlive(uint64_t documentIdentifier) const
{
    return Document::allDocumentsMap().contains(makeObjectIdentifier<DocumentIdentifierType>(documentIdentifier));
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
    return element.document().identifierForElement(element).toUInt64();
}

uint64_t Internals::frameIdentifier(const Document& document) const
{
    if (auto* page = document.page())
        return page->mainFrame().loader().frameID().valueOr(FrameIdentifier { }).toUInt64();
    return 0;
}

uint64_t Internals::pageIdentifier(const Document& document) const
{
    return document.pageID().valueOr(PageIdentifier { }).toUInt64();
}

bool Internals::isAnyWorkletGlobalScopeAlive() const
{
#if ENABLE(CSS_PAINTING_API)
    return !WorkletGlobalScope::allWorkletGlobalScopesSet().isEmpty();
#else
    return false;
#endif
}

String Internals::serviceWorkerClientIdentifier(const Document& document) const
{
#if ENABLE(SERVICE_WORKER)
    return ServiceWorkerClientIdentifier { ServiceWorkerProvider::singleton().serviceWorkerConnection().serverConnectionIdentifier(), document.identifier() }.toString();
#else
    UNUSED_PARAM(document);
    return String();
#endif
}

RefPtr<WindowProxy> Internals::openDummyInspectorFrontend(const String& url)
{
    auto* inspectedPage = contextDocument()->frame()->page();
    auto* window = inspectedPage->mainFrame().document()->domWindow();
    auto frontendWindowProxy = window->open(*window, *window, url, "", "").releaseReturnValue();
    m_inspectorFrontend = makeUnique<InspectorStubFrontend>(*inspectedPage, downcast<DOMWindow>(frontendWindowProxy->window()));
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
        return Exception { InvalidAccessError };

    page->inspectorController().setIsUnderTest(isUnderTest);
    return { };
}

bool Internals::hasGrammarMarker(int from, int length)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    return document->editor().selectionStartHasMarkerFor(DocumentMarker::Grammar, from, length);
}

unsigned Internals::numberOfScrollableAreas()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

    unsigned count = 0;
    Frame* frame = document->frame();
    if (frame->view()->scrollableAreas())
        count += frame->view()->scrollableAreas()->size();

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->view() && child->view()->scrollableAreas())
            count += child->view()->scrollableAreas()->size();
    }

    return count;
}

ExceptionOr<bool> Internals::isPageBoxVisible(int pageNumber)
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    return document->isPageBoxVisible(pageNumber);
}

static LayerTreeFlags toLayerTreeFlags(unsigned short flags)
{
    LayerTreeFlags layerTreeFlags = 0;
    if (flags & Internals::LAYER_TREE_INCLUDES_VISIBLE_RECTS)
        layerTreeFlags |= LayerTreeFlagsIncludeVisibleRects;
    if (flags & Internals::LAYER_TREE_INCLUDES_TILE_CACHES)
        layerTreeFlags |= LayerTreeFlagsIncludeTileCaches;
    if (flags & Internals::LAYER_TREE_INCLUDES_REPAINT_RECTS)
        layerTreeFlags |= LayerTreeFlagsIncludeRepaintRects;
    if (flags & Internals::LAYER_TREE_INCLUDES_PAINTING_PHASES)
        layerTreeFlags |= LayerTreeFlagsIncludePaintingPhases;
    if (flags & Internals::LAYER_TREE_INCLUDES_CONTENT_LAYERS)
        layerTreeFlags |= LayerTreeFlagsIncludeContentLayers;
    if (flags & Internals::LAYER_TREE_INCLUDES_ACCELERATES_DRAWING)
        layerTreeFlags |= LayerTreeFlagsIncludeAcceleratesDrawing;
    if (flags & Internals::LAYER_TREE_INCLUDES_CLIPPING)
        layerTreeFlags |= LayerTreeFlagsIncludeClipping;
    if (flags & Internals::LAYER_TREE_INCLUDES_BACKING_STORE_ATTACHED)
        layerTreeFlags |= LayerTreeFlagsIncludeBackingStoreAttached;
    if (flags & Internals::LAYER_TREE_INCLUDES_ROOT_LAYER_PROPERTIES)
        layerTreeFlags |= LayerTreeFlagsIncludeRootLayerProperties;
    if (flags & Internals::LAYER_TREE_INCLUDES_EVENT_REGION)
        layerTreeFlags |= LayerTreeFlagsIncludeEventRegion;
    if (flags & Internals::LAYER_TREE_INCLUDES_DEEP_COLOR)
        layerTreeFlags |= LayerTreeFlagsIncludeDeepColor;

    return layerTreeFlags;
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
ExceptionOr<String> Internals::layerTreeAsText(Document& document, unsigned short flags) const
{
    if (!document.frame())
        return Exception { InvalidAccessError };

    document.updateLayoutIgnorePendingStylesheets();
    return document.frame()->layerTreeAsText(toLayerTreeFlags(flags));
}

ExceptionOr<uint64_t> Internals::layerIDForElement(Element& element)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    element.document().updateLayoutIgnorePendingStylesheets();

    if (!element.renderer() || !element.renderer()->hasLayer())
        return Exception { NotFoundError };

    auto& layerModelObject = downcast<RenderLayerModelObject>(*element.renderer());
    if (!layerModelObject.layer()->isComposited())
        return Exception { NotFoundError };

    auto* backing = layerModelObject.layer()->backing();
    return backing->graphicsLayer()->primaryLayerID();
}

ExceptionOr<String> Internals::repaintRectsAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    return document->frame()->trackedRepaintRectsAsText();
}

ExceptionOr<ScrollableArea*> Internals::scrollableAreaForNode(Node* node) const
{
    if (!node)
        node = contextDocument();

    if (!node)
        return Exception { InvalidAccessError };

    auto nodeRef = makeRef(*node);
    nodeRef->document().updateLayoutIgnorePendingStylesheets();

    ScrollableArea* scrollableArea = nullptr;
    if (is<Document>(nodeRef)) {
        auto* frameView = downcast<Document>(nodeRef.get()).view();
        if (!frameView)
            return Exception { InvalidAccessError };

        scrollableArea = frameView;
    } else if (is<Element>(nodeRef)) {
        auto& element = downcast<Element>(nodeRef.get());
        if (!element.renderBox())
            return Exception { InvalidAccessError };

        auto& renderBox = *element.renderBox();
        if (is<RenderListBox>(renderBox))
            scrollableArea = &downcast<RenderListBox>(renderBox);
        else
            scrollableArea = renderBox.layer();
    } else
        return Exception { InvalidNodeTypeError };

    if (!scrollableArea)
        return Exception { InvalidNodeTypeError };

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

ExceptionOr<String> Internals::scrollingStateTreeAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->updateLayoutIgnorePendingStylesheets();

    Page* page = document->page();
    if (!page)
        return String();

    return page->scrollingStateTreeAsText();
}

ExceptionOr<String> Internals::scrollingTreeAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->updateLayoutIgnorePendingStylesheets();

    auto page = document->page();
    if (!page)
        return String();

    auto scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return String();

    scrollingCoordinator->commitTreeStateIfNeeded();
    return scrollingCoordinator->scrollingTreeAsText();
}

ExceptionOr<String> Internals::mainThreadScrollingReasons() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    Page* page = document->page();
    if (!page)
        return String();

    return page->synchronousScrollingReasonsAsText();
}

ExceptionOr<Ref<DOMRectList>> Internals::nonFastScrollableRects() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    Page* page = document->page();
    if (!page)
        return DOMRectList::create();

    return page->nonFastScrollableRectsForTesting();
}

ExceptionOr<void> Internals::setElementUsesDisplayListDrawing(Element& element, bool usesDisplayListDrawing)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    element.document().updateLayoutIgnorePendingStylesheets();

    if (!element.renderer())
        return Exception { InvalidAccessError };

    if (is<HTMLCanvasElement>(element)) {
        downcast<HTMLCanvasElement>(element).setUsesDisplayListDrawing(usesDisplayListDrawing);
        return { };
    }

    if (!element.renderer()->hasLayer())
        return Exception { InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { InvalidAccessError };

    layer->backing()->setUsesDisplayListDrawing(usesDisplayListDrawing);
    return { };
}

ExceptionOr<void> Internals::setElementTracksDisplayListReplay(Element& element, bool isTrackingReplay)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    element.document().updateLayoutIgnorePendingStylesheets();

    if (!element.renderer())
        return Exception { InvalidAccessError };

    if (is<HTMLCanvasElement>(element)) {
        downcast<HTMLCanvasElement>(element).setTracksDisplayListReplay(isTrackingReplay);
        return { };
    }

    if (!element.renderer()->hasLayer())
        return Exception { InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { InvalidAccessError };

    layer->backing()->setIsTrackingDisplayListReplay(isTrackingReplay);
    return { };
}

ExceptionOr<String> Internals::displayListForElement(Element& element, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    element.document().updateLayoutIgnorePendingStylesheets();

    if (!element.renderer())
        return Exception { InvalidAccessError };

    DisplayList::AsTextFlags displayListFlags = 0;
    if (flags & DISPLAY_LIST_INCLUDES_PLATFORM_OPERATIONS)
        displayListFlags |= DisplayList::AsTextFlag::IncludesPlatformOperations;

    if (is<HTMLCanvasElement>(element))
        return downcast<HTMLCanvasElement>(element).displayListAsText(displayListFlags);

    if (!element.renderer()->hasLayer())
        return Exception { InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { InvalidAccessError };

    return layer->backing()->displayListAsText(displayListFlags);
}

ExceptionOr<String> Internals::replayDisplayListForElement(Element& element, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    element.document().updateLayoutIgnorePendingStylesheets();

    if (!element.renderer())
        return Exception { InvalidAccessError };

    DisplayList::AsTextFlags displayListFlags = 0;
    if (flags & DISPLAY_LIST_INCLUDES_PLATFORM_OPERATIONS)
        displayListFlags |= DisplayList::AsTextFlag::IncludesPlatformOperations;

    if (is<HTMLCanvasElement>(element))
        return downcast<HTMLCanvasElement>(element).replayDisplayListAsText(displayListFlags);

    if (!element.renderer()->hasLayer())
        return Exception { InvalidAccessError };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { InvalidAccessError };

    return layer->backing()->replayDisplayListAsText(displayListFlags);
}

ExceptionOr<void> Internals::garbageCollectDocumentResources() const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };
    document->cachedResourceLoader().garbageCollectDocumentResources();
    return { };
}

bool Internals::isUnderMemoryPressure()
{
    return MemoryPressureHandler::singleton().isUnderMemoryPressure();
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
        return Exception { InvalidAccessError };

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
        return Exception { InvalidAccessError };

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
        return Exception { InvalidAccessError };

    return PrintContext::pageProperty(frame(), propertyName.utf8().data(), pageNumber);
}

ExceptionOr<String> Internals::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    if (!frame())
        return Exception { InvalidAccessError };

    return PrintContext::pageSizeAndMarginsInPixels(frame(), pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft);
}

ExceptionOr<float> Internals::pageScaleFactor() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    return document->page()->pageScaleFactor();
}

ExceptionOr<void> Internals::setPageScaleFactor(float scaleFactor, int x, int y)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    document->page()->setPageScaleFactor(scaleFactor, IntPoint(x, y));
    return { };
}

ExceptionOr<void> Internals::setPageZoomFactor(float zoomFactor)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->frame()->setPageZoomFactor(zoomFactor);
    return { };
}

ExceptionOr<void> Internals::setTextZoomFactor(float zoomFactor)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->frame()->setTextZoomFactor(zoomFactor);
    return { };
}

ExceptionOr<void> Internals::setUseFixedLayout(bool useFixedLayout)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    document->view()->setUseFixedLayout(useFixedLayout);
    return { };
}

ExceptionOr<void> Internals::setFixedLayoutSize(int width, int height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    document->view()->setFixedLayoutSize(IntSize(width, height));
    return { };
}

ExceptionOr<void> Internals::setViewExposedRect(float x, float y, float width, float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

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

void Internals::setTopContentInset(float contentInset)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;

    document->page()->setTopContentInset(contentInset);
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

void Internals::setFullscreenControlsHidden(bool hidden)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    page->setFullscreenControlsHidden(hidden);
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
void Internals::setMockVideoPresentationModeEnabled(bool enabled)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;

    document->page()->chrome().client().setMockVideoPresentationModeEnabled(enabled);
}
#endif

void Internals::setApplicationCacheOriginQuota(unsigned long long quota)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;
    document->page()->applicationCacheStorage().storeUpdatedQuotaForOrigin(&document->securityOrigin(), quota);
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
    frame()->loader().history().saveDocumentAndScrollState();
    return FormController::referencedFilePaths(frame()->loader().history().currentItem()->documentState());
}

ExceptionOr<void> Internals::startTrackingRepaints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    document->view()->setTracksRepaints(true);
    return { };
}

ExceptionOr<void> Internals::stopTrackingRepaints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { InvalidAccessError };

    document->view()->setTracksRepaints(false);
    return { };
}

ExceptionOr<void> Internals::startTrackingLayerFlushes()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    document->renderView()->compositor().startTrackingLayerFlushes();
    return { };
}

ExceptionOr<unsigned> Internals::layerFlushCount()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    return document->renderView()->compositor().layerFlushCount();
}

ExceptionOr<void> Internals::startTrackingStyleRecalcs()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    document->startTrackingStyleRecalcs();
    return { };
}

ExceptionOr<unsigned> Internals::styleRecalcCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    return document->styleRecalcCount();
}

unsigned Internals::lastStyleUpdateSize() const
{
    Document* document = contextDocument();
    if (!document)
        return 0;
    return document->lastStyleUpdateSizeForTesting();
}

ExceptionOr<void> Internals::startTrackingCompositingUpdates()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    document->renderView()->compositor().startTrackingCompositingUpdates();
    return { };
}

ExceptionOr<unsigned> Internals::compositingUpdateCount()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { InvalidAccessError };

    return document->renderView()->compositor().compositingUpdateCount();
}

ExceptionOr<void> Internals::setCompositingPolicyOverride(Optional<CompositingPolicy> policyOverride)
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    if (!policyOverride) {
        document->page()->setCompositingPolicyOverride(WTF::nullopt);
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

ExceptionOr<Optional<Internals::CompositingPolicy>> Internals::compositingPolicyOverride() const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { InvalidAccessError };

    auto policyOverride = document->page()->compositingPolicyOverride();
    if (!policyOverride)
        return { WTF::nullopt };

    switch (policyOverride.value()) {
    case WebCore::CompositingPolicy::Normal:
        return { Internals::CompositingPolicy::Normal };
    case WebCore::CompositingPolicy::Conservative:
        return { Internals::CompositingPolicy::Conservative };
    }

    return { Internals::CompositingPolicy::Normal };
}

void Internals::updateLayoutAndStyleForAllFrames()
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
        return Exception { TypeError };

    document->updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
    return { };
}

unsigned Internals::layoutCount() const
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return 0;
    return document->view()->layoutContext().layoutCount();
}

#if !PLATFORM(IOS_FAMILY)
static const char* cursorTypeToString(Cursor::Type cursorType)
{
    switch (cursorType) {
    case Cursor::Pointer: return "Pointer";
    case Cursor::Cross: return "Cross";
    case Cursor::Hand: return "Hand";
    case Cursor::IBeam: return "IBeam";
    case Cursor::Wait: return "Wait";
    case Cursor::Help: return "Help";
    case Cursor::EastResize: return "EastResize";
    case Cursor::NorthResize: return "NorthResize";
    case Cursor::NorthEastResize: return "NorthEastResize";
    case Cursor::NorthWestResize: return "NorthWestResize";
    case Cursor::SouthResize: return "SouthResize";
    case Cursor::SouthEastResize: return "SouthEastResize";
    case Cursor::SouthWestResize: return "SouthWestResize";
    case Cursor::WestResize: return "WestResize";
    case Cursor::NorthSouthResize: return "NorthSouthResize";
    case Cursor::EastWestResize: return "EastWestResize";
    case Cursor::NorthEastSouthWestResize: return "NorthEastSouthWestResize";
    case Cursor::NorthWestSouthEastResize: return "NorthWestSouthEastResize";
    case Cursor::ColumnResize: return "ColumnResize";
    case Cursor::RowResize: return "RowResize";
    case Cursor::MiddlePanning: return "MiddlePanning";
    case Cursor::EastPanning: return "EastPanning";
    case Cursor::NorthPanning: return "NorthPanning";
    case Cursor::NorthEastPanning: return "NorthEastPanning";
    case Cursor::NorthWestPanning: return "NorthWestPanning";
    case Cursor::SouthPanning: return "SouthPanning";
    case Cursor::SouthEastPanning: return "SouthEastPanning";
    case Cursor::SouthWestPanning: return "SouthWestPanning";
    case Cursor::WestPanning: return "WestPanning";
    case Cursor::Move: return "Move";
    case Cursor::VerticalText: return "VerticalText";
    case Cursor::Cell: return "Cell";
    case Cursor::ContextMenu: return "ContextMenu";
    case Cursor::Alias: return "Alias";
    case Cursor::Progress: return "Progress";
    case Cursor::NoDrop: return "NoDrop";
    case Cursor::Copy: return "Copy";
    case Cursor::None: return "None";
    case Cursor::NotAllowed: return "NotAllowed";
    case Cursor::ZoomIn: return "ZoomIn";
    case Cursor::ZoomOut: return "ZoomOut";
    case Cursor::Grab: return "Grab";
    case Cursor::Grabbing: return "Grabbing";
    case Cursor::Custom: return "Custom";
    }

    ASSERT_NOT_REACHED();
    return "UNKNOWN";
}
#endif

ExceptionOr<String> Internals::getCurrentCursorInfo()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

#if !PLATFORM(IOS_FAMILY)
    Cursor cursor = document->frame()->eventHandler().currentMouseCursor();

    StringBuilder result;
    result.append("type=", cursorTypeToString(cursor.type()), " hotSpot=", cursor.hotSpot().x(), ',', cursor.hotSpot().y());
    if (cursor.image()) {
        FloatSize size = cursor.image()->size();
        result.append(" image=", size.width(), 'x', size.height());
    }
#if ENABLE(MOUSE_CURSOR_SCALE)
    if (cursor.imageScaleFactor() != 1)
        result.append(" scale=", cursor.imageScaleFactor());
#endif
    return result.toString();
#else
    return "FAIL: Cursor details not available on this platform."_str;
#endif
}

Ref<ArrayBuffer> Internals::serializeObject(const RefPtr<SerializedScriptValue>& value) const
{
    auto& bytes = value->data();
    return ArrayBuffer::create(bytes.data(), bytes.size());
}

Ref<SerializedScriptValue> Internals::deserializeBuffer(ArrayBuffer& buffer) const
{
    Vector<uint8_t> bytes;
    bytes.append(static_cast<const uint8_t*>(buffer.data()), buffer.byteLength());
    return SerializedScriptValue::adopt(WTFMove(bytes));
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
    return scriptController.executeScriptInWorldIgnoringException(world, source);
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(enabled);
#if PLATFORM(MAC)
    ScrollerStyle::setUseOverlayScrollbars(enabled);
    ScrollbarTheme& theme = ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    static_cast<ScrollbarThemeMac&>(theme).preferencesChanged();
#endif
}

void Internals::setUsesMockScrollAnimator(bool enabled)
{
    WebCore::DeprecatedGlobalSettings::setUsesMockScrollAnimator(enabled);
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

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

void Internals::initializeMockCDM()
{
    LegacyCDM::registerCDMFactory([] (LegacyCDM* cdm) { return makeUnique<LegacyMockCDM>(cdm); },
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
    return result.title(direction);
}

String Internals::getImageSourceURL(Element& element)
{
    return element.imageSourceURL();
}

#if ENABLE(VIDEO)

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
    element.player()->simulateAudioInterruption();
#else
    UNUSED_PARAM(element);
#endif
}

ExceptionOr<bool> Internals::mediaElementHasCharacteristic(HTMLMediaElement& element, const String& characteristic)
{
    if (equalLettersIgnoringASCIICase(characteristic, "audible"))
        return element.hasAudio();
    if (equalLettersIgnoringASCIICase(characteristic, "visual"))
        return element.hasVideo();
    if (equalLettersIgnoringASCIICase(characteristic, "legible"))
        return element.hasClosedCaptions();

    return Exception { SyntaxError };
}

void Internals::beginSimulatedHDCPError(HTMLMediaElement& element)
{
    if (auto player = element.player())
        player->beginSimulatedHDCPError();
}

void Internals::endSimulatedHDCPError(HTMLMediaElement& element)
{
    if (auto player = element.player())
        player->endSimulatedHDCPError();
}

bool Internals::elementShouldBufferData(HTMLMediaElement& element)
{
    return element.bufferingPolicy() < MediaPlayer::BufferingPolicy::LimitReadAhead;
}

String Internals::elementBufferingPolicy(HTMLMediaElement& element)
{
    switch (element.bufferingPolicy()) {
    case MediaPlayer::BufferingPolicy::Default:
        return "Default";
    case MediaPlayer::BufferingPolicy::LimitReadAhead:
        return "LimitReadAhead";
    case MediaPlayer::BufferingPolicy::MakeResourcesPurgeable:
        return "MakeResourcesPurgeable";
    case MediaPlayer::BufferingPolicy::PurgeResources:
        return "PurgeResources";
    }

    ASSERT_NOT_REACHED();
    return "UNKNOWN";
}
#endif

bool Internals::isSelectPopupVisible(HTMLSelectElement& element)
{
    element.document().updateLayoutIgnorePendingStylesheets();

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
        return Exception { InvalidAccessError };

#if ENABLE(VIDEO_TRACK)
    return document->page()->group().captionPreferences().captionsStyleSheetOverride();
#else
    return String { emptyString() };
#endif
}

ExceptionOr<void> Internals::setCaptionsStyleSheetOverride(const String& override)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

#if ENABLE(VIDEO_TRACK)
    document->page()->group().captionPreferences().setCaptionsStyleSheetOverride(override);
#else
    UNUSED_PARAM(override);
#endif
    return { };
}

ExceptionOr<void> Internals::setPrimaryAudioTrackLanguageOverride(const String& language)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

#if ENABLE(VIDEO_TRACK)
    document->page()->group().captionPreferences().setPrimaryAudioTrackLanguageOverride(language);
#else
    UNUSED_PARAM(language);
#endif
    return { };
}

ExceptionOr<void> Internals::setCaptionDisplayMode(const String& mode)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

#if ENABLE(VIDEO_TRACK)
    auto& captionPreferences = document->page()->group().captionPreferences();

    if (equalLettersIgnoringASCIICase(mode, "automatic"))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::Automatic);
    else if (equalLettersIgnoringASCIICase(mode, "forcedonly"))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::ForcedOnly);
    else if (equalLettersIgnoringASCIICase(mode, "alwayson"))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::AlwaysOn);
    else if (equalLettersIgnoringASCIICase(mode, "manual"))
        captionPreferences.setCaptionDisplayMode(CaptionUserPreferences::Manual);
    else
        return Exception { SyntaxError };
#else
    UNUSED_PARAM(mode);
#endif
    return { };
}

#if ENABLE(VIDEO_TRACK)
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
#endif

#if ENABLE(VIDEO)

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
        return Exception { InvalidAccessError };

    return DOMRect::create(document->frame()->selection().selectionBounds());
}

void Internals::setSelectionWithoutValidation(Ref<Node> baseNode, unsigned baseOffset, RefPtr<Node> extentNode, unsigned extentOffset)
{
    contextDocument()->frame()->selection().moveTo(
        VisiblePosition { createLegacyEditingPosition(baseNode.ptr(), baseOffset) },
        VisiblePosition { createLegacyEditingPosition(extentNode.get(), extentOffset) });
}

ExceptionOr<bool> Internals::isPluginUnavailabilityIndicatorObscured(Element& element)
{
    if (!is<HTMLPlugInElement>(element))
        return Exception { InvalidAccessError };

    return downcast<HTMLPlugInElement>(element).isReplacementObscured();
}

ExceptionOr<String> Internals::unavailablePluginReplacementText(Element& element)
{
    if (!is<HTMLPlugInElement>(element))
        return Exception { InvalidAccessError };

    auto* renderer = element.renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return String { };

    return String { downcast<RenderEmbeddedObject>(*renderer).pluginReplacementTextIfUnavailable() };
}

bool Internals::isPluginSnapshotted(Element& element)
{
    return is<HTMLPlugInElement>(element) && downcast<HTMLPlugInElement>(element).displayState() <= HTMLPlugInElement::DisplayingSnapshot;
}

bool Internals::pluginIsBelowSizeThreshold(Element& element)
{
    return is<HTMLPlugInElement>(element) && downcast<HTMLPlugInElement>(element).isBelowSizeThreshold();
}

#if ENABLE(MEDIA_SOURCE)

void Internals::initializeMockMediaSource()
{
#if USE(AVFOUNDATION)
    WebCore::DeprecatedGlobalSettings::setAVFoundationEnabled(false);
#endif
#if USE(GSTREAMER)
    WebCore::DeprecatedGlobalSettings::setGStreamerEnabled(false);
#endif
    MediaPlayerFactorySupport::callRegisterMediaEngine(MockMediaPlayerMediaSource::registerMediaEngine);
}

Vector<String> Internals::bufferedSamplesForTrackID(SourceBuffer& buffer, const AtomString& trackID)
{
    return buffer.bufferedSamplesForTrackID(trackID);
}

Vector<String> Internals::enqueuedSamplesForTrackID(SourceBuffer& buffer, const AtomString& trackID)
{
    return buffer.enqueuedSamplesForTrackID(trackID);
}

double Internals::minimumUpcomingPresentationTimeForTrackID(SourceBuffer& buffer, const AtomString& trackID)
{
    return buffer.minimumUpcomingPresentationTimeForTrackID(trackID).toDouble();
}

void Internals::setShouldGenerateTimestamps(SourceBuffer& buffer, bool flag)
{
    buffer.setShouldGenerateTimestamps(flag);
}

void Internals::setMaximumQueueDepthForTrackID(SourceBuffer& buffer, const AtomString& trackID, size_t maxQueueDepth)
{
    buffer.setMaximumQueueDepthForTrackID(trackID, maxQueueDepth);
}

#endif

void Internals::enableMockMediaCapabilities()
{
    MediaEngineConfigurationFactory::enableMock();
}

#if ENABLE(VIDEO)

ExceptionOr<void> Internals::beginMediaSessionInterruption(const String& interruptionString)
{
    PlatformMediaSession::InterruptionType interruption = PlatformMediaSession::SystemInterruption;

    if (equalLettersIgnoringASCIICase(interruptionString, "system"))
        interruption = PlatformMediaSession::SystemInterruption;
    else if (equalLettersIgnoringASCIICase(interruptionString, "systemsleep"))
        interruption = PlatformMediaSession::SystemSleep;
    else if (equalLettersIgnoringASCIICase(interruptionString, "enteringbackground"))
        interruption = PlatformMediaSession::EnteringBackground;
    else if (equalLettersIgnoringASCIICase(interruptionString, "suspendedunderlock"))
        interruption = PlatformMediaSession::SuspendedUnderLock;
    else
        return Exception { InvalidAccessError };

    PlatformMediaSessionManager::sharedManager().beginInterruption(interruption);
    return { };
}

void Internals::endMediaSessionInterruption(const String& flagsString)
{
    PlatformMediaSession::EndInterruptionFlags flags = PlatformMediaSession::NoFlags;

    if (equalLettersIgnoringASCIICase(flagsString, "mayresumeplaying"))
        flags = PlatformMediaSession::MayResumePlaying;

    PlatformMediaSessionManager::sharedManager().endInterruption(flags);
}

void Internals::applicationWillBecomeInactive()
{
    PlatformMediaSessionManager::sharedManager().applicationWillBecomeInactive();
}

void Internals::applicationDidBecomeActive()
{
    PlatformMediaSessionManager::sharedManager().applicationDidBecomeActive();
}

void Internals::applicationWillEnterForeground(bool suspendedUnderLock) const
{
    PlatformMediaSessionManager::sharedManager().applicationWillEnterForeground(suspendedUnderLock);
}

void Internals::applicationDidEnterBackground(bool suspendedUnderLock) const
{
    PlatformMediaSessionManager::sharedManager().applicationDidEnterBackground(suspendedUnderLock);
}

static PlatformMediaSession::MediaType mediaTypeFromString(const String& mediaTypeString)
{
    if (equalLettersIgnoringASCIICase(mediaTypeString, "video"))
        return PlatformMediaSession::MediaType::Video;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "audio"))
        return PlatformMediaSession::MediaType::Audio;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "videoaudio"))
        return PlatformMediaSession::MediaType::VideoAudio;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "webaudio"))
        return PlatformMediaSession::MediaType::WebAudio;

    return PlatformMediaSession::MediaType::None;
}

ExceptionOr<void> Internals::setMediaSessionRestrictions(const String& mediaTypeString, StringView restrictionsString)
{
    auto mediaType = mediaTypeFromString(mediaTypeString);
    if (mediaType == PlatformMediaSession::MediaType::None)
        return Exception { InvalidAccessError };

    auto restrictions = PlatformMediaSessionManager::sharedManager().restrictions(mediaType);
    PlatformMediaSessionManager::sharedManager().removeRestriction(mediaType, restrictions);

    restrictions = PlatformMediaSessionManager::NoRestrictions;

    for (StringView restrictionString : restrictionsString.split(',')) {
        if (equalLettersIgnoringASCIICase(restrictionString, "concurrentplaybacknotpermitted"))
            restrictions |= PlatformMediaSessionManager::ConcurrentPlaybackNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "backgroundprocessplaybackrestricted"))
            restrictions |= PlatformMediaSessionManager::BackgroundProcessPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "backgroundtabplaybackrestricted"))
            restrictions |= PlatformMediaSessionManager::BackgroundTabPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "interruptedplaybacknotpermitted"))
            restrictions |= PlatformMediaSessionManager::InterruptedPlaybackNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "inactiveprocessplaybackrestricted"))
            restrictions |= PlatformMediaSessionManager::InactiveProcessPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "suspendedunderlockplaybackrestricted"))
            restrictions |= PlatformMediaSessionManager::SuspendedUnderLockPlaybackRestricted;
    }
    PlatformMediaSessionManager::sharedManager().addRestriction(mediaType, restrictions);
    return { };
}

ExceptionOr<String> Internals::mediaSessionRestrictions(const String& mediaTypeString) const
{
    PlatformMediaSession::MediaType mediaType = mediaTypeFromString(mediaTypeString);
    if (mediaType == PlatformMediaSession::MediaType::None)
        return Exception { InvalidAccessError };

    PlatformMediaSessionManager::SessionRestrictions restrictions = PlatformMediaSessionManager::sharedManager().restrictions(mediaType);
    if (restrictions == PlatformMediaSessionManager::NoRestrictions)
        return String();

    StringBuilder builder;
    if (restrictions & PlatformMediaSessionManager::ConcurrentPlaybackNotPermitted)
        builder.append("concurrentplaybacknotpermitted");
    if (restrictions & PlatformMediaSessionManager::BackgroundProcessPlaybackRestricted) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append("backgroundprocessplaybackrestricted");
    }
    if (restrictions & PlatformMediaSessionManager::BackgroundTabPlaybackRestricted) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append("backgroundtabplaybackrestricted");
    }
    if (restrictions & PlatformMediaSessionManager::InterruptedPlaybackNotPermitted) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append("interruptedplaybacknotpermitted");
    }
    return builder.toString();
}

void Internals::setMediaElementRestrictions(HTMLMediaElement& element, StringView restrictionsString)
{
    MediaElementSession::BehaviorRestrictions restrictions = element.mediaSession().behaviorRestrictions();
    element.mediaSession().removeBehaviorRestriction(restrictions);

    restrictions = MediaElementSession::NoRestrictions;

    for (StringView restrictionString : restrictionsString.split(',')) {
        if (equalLettersIgnoringASCIICase(restrictionString, "norestrictions"))
            restrictions |= MediaElementSession::NoRestrictions;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforload"))
            restrictions |= MediaElementSession::RequireUserGestureForLoad;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforvideoratechange"))
            restrictions |= MediaElementSession::RequireUserGestureForVideoRateChange;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforfullscreen"))
            restrictions |= MediaElementSession::RequireUserGestureForFullscreen;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsenttoloadmedia"))
            restrictions |= MediaElementSession::RequirePageConsentToLoadMedia;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsenttoresumemedia"))
            restrictions |= MediaElementSession::RequirePageConsentToResumeMedia;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergesturetoshowplaybacktargetpicker"))
            restrictions |= MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker;
        if (equalLettersIgnoringASCIICase(restrictionString, "wirelessvideoplaybackdisabled"))
            restrictions |= MediaElementSession::WirelessVideoPlaybackDisabled;
#endif
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforaudioratechange"))
            restrictions |= MediaElementSession::RequireUserGestureForAudioRateChange;
        if (equalLettersIgnoringASCIICase(restrictionString, "autopreloadingnotpermitted"))
            restrictions |= MediaElementSession::AutoPreloadingNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "invisibleautoplaynotpermitted"))
            restrictions |= MediaElementSession::InvisibleAutoplayNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "overrideusergesturerequirementformaincontent"))
            restrictions |= MediaElementSession::OverrideUserGestureRequirementForMainContent;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergesturetocontrolcontrolsmanager"))
            restrictions |= MediaElementSession::RequireUserGestureToControlControlsManager;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireplaybackTocontrolcontrolsmanager"))
            restrictions |= MediaElementSession::RequirePlaybackToControlControlsManager;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforvideoduetolowpowermode"))
            restrictions |= MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode;
    }
    element.mediaSession().addBehaviorRestriction(restrictions);
}

ExceptionOr<void> Internals::postRemoteControlCommand(const String& commandString, float argument)
{
    PlatformMediaSession::RemoteControlCommandType command;
    PlatformMediaSession::RemoteCommandArgument parameter { argument };

    if (equalLettersIgnoringASCIICase(commandString, "play"))
        command = PlatformMediaSession::PlayCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "pause"))
        command = PlatformMediaSession::PauseCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "stop"))
        command = PlatformMediaSession::StopCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "toggleplaypause"))
        command = PlatformMediaSession::TogglePlayPauseCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "beginseekingbackward"))
        command = PlatformMediaSession::BeginSeekingBackwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "endseekingbackward"))
        command = PlatformMediaSession::EndSeekingBackwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "beginseekingforward"))
        command = PlatformMediaSession::BeginSeekingForwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "endseekingforward"))
        command = PlatformMediaSession::EndSeekingForwardCommand;
    else if (equalLettersIgnoringASCIICase(commandString, "seektoplaybackposition"))
        command = PlatformMediaSession::SeekToPlaybackPositionCommand;
    else
        return Exception { InvalidAccessError };

    PlatformMediaSessionManager::sharedManager().processDidReceiveRemoteControlCommand(command, &parameter);
    return { };
}

bool Internals::elementIsBlockingDisplaySleep(HTMLMediaElement& element) const
{
    return element.isDisablingSleep();
}

#endif // ENABLE(VIDEO)

#if ENABLE(MEDIA_SESSION)

void Internals::sendMediaSessionStartOfInterruptionNotification(MediaSessionInterruptingCategory category)
{
    MediaSessionManager::singleton().didReceiveStartOfInterruptionNotification(category);
}

void Internals::sendMediaSessionEndOfInterruptionNotification(MediaSessionInterruptingCategory category)
{
    MediaSessionManager::singleton().didReceiveEndOfInterruptionNotification(category);
}

String Internals::mediaSessionCurrentState(MediaSession* session) const
{
    switch (session->currentState()) {
    case MediaSession::State::Active:
        return "active";
    case MediaSession::State::Interrupted:
        return "interrupted";
    case MediaSession::State::Idle:
        return "idle";
    }
}

double Internals::mediaElementPlayerVolume(HTMLMediaElement* element) const
{
    ASSERT_ARG(element, element);
    return element->playerVolume();
}

void Internals::sendMediaControlEvent(MediaControlEvent event)
{
    // FIXME: No good reason to use a single function with an argument instead of three functions.
    switch (event) {
    case MediControlEvent::PlayPause:
        MediaSessionManager::singleton().togglePlayback();
        break;
    case MediControlEvent::NextTrack:
        MediaSessionManager::singleton().skipToNextTrack();
        break;
    case MediControlEvent::PreviousTrack:
        MediaSessionManager::singleton().skipToPreviousTrack();
        break;
    }
}

#endif // ENABLE(MEDIA_SESSION)

#if ENABLE(WEB_AUDIO)
void Internals::setAudioContextRestrictions(AudioContext& context, StringView restrictionsString)
{
    AudioContext::BehaviorRestrictions restrictions = context.behaviorRestrictions();
    context.removeBehaviorRestriction(restrictions);

    restrictions = AudioContext::NoRestrictions;

    for (StringView restrictionString : restrictionsString.split(',')) {
        if (equalLettersIgnoringASCIICase(restrictionString, "norestrictions"))
            restrictions |= AudioContext::NoRestrictions;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforaudiostart"))
            restrictions |= AudioContext::RequireUserGestureForAudioStartRestriction;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsentforaudiostart"))
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
    PlatformMediaSessionManager::sharedManager().processSystemWillSleep();
#endif
}

void Internals::simulateSystemWake() const
{
#if ENABLE(VIDEO)
    PlatformMediaSessionManager::sharedManager().processSystemDidWake();
#endif
}

ExceptionOr<Internals::NowPlayingState> Internals::nowPlayingState() const
{
#if ENABLE(VIDEO)
    return { { PlatformMediaSessionManager::sharedManager().lastUpdatedNowPlayingTitle(),
        PlatformMediaSessionManager::sharedManager().lastUpdatedNowPlayingDuration(),
        PlatformMediaSessionManager::sharedManager().lastUpdatedNowPlayingElapsedTime(),
        PlatformMediaSessionManager::sharedManager().lastUpdatedNowPlayingInfoUniqueIdentifier().toUInt64(),
        PlatformMediaSessionManager::sharedManager().hasActiveNowPlayingSession(),
        PlatformMediaSessionManager::sharedManager().registeredAsNowPlayingApplication(),
        PlatformMediaSessionManager::sharedManager().haveEverRegisteredAsNowPlayingApplication()
    } };
#else
    return Exception { InvalidAccessError };
#endif
}

#if ENABLE(VIDEO)
RefPtr<HTMLMediaElement> Internals::bestMediaElementForShowingPlaybackControlsManager(Internals::PlaybackControlsPurpose purpose)
{
    return HTMLMediaElement::bestMediaElementForShowingPlaybackControlsManager(purpose);
}

Internals::MediaSessionState Internals::mediaSessionState(HTMLMediaElement& element)
{
    return element.mediaSession().state();
}
#endif

ExceptionOr<Internals::MediaUsageState> Internals::mediaUsageState(HTMLMediaElement& element) const
{
#if ENABLE(VIDEO)
    element.mediaSession().updateMediaUsageIfChanged();
    auto info = element.mediaSession().mediaUsageInfo();
    if (!info)
        return Exception { NotSupportedError };

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
        info.value().hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture,
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
    return Exception { InvalidAccessError };
#endif
}

ExceptionOr<bool> Internals::elementShouldDisplayPosterImage(HTMLVideoElement& element) const
{
#if ENABLE(VIDEO)
    return element.shouldDisplayPosterImage();
#else
    UNUSED_PARAM(element);
    return Exception { InvalidAccessError };
#endif
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void Internals::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    page->setMockMediaPlaybackTargetPickerEnabled(enabled);
}

ExceptionOr<void> Internals::setMockMediaPlaybackTargetPickerState(const String& deviceName, const String& deviceState)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    MediaPlaybackTargetContext::State state = MediaPlaybackTargetContext::Unknown;

    if (equalLettersIgnoringASCIICase(deviceState, "deviceavailable"))
        state = MediaPlaybackTargetContext::OutputDeviceAvailable;
    else if (equalLettersIgnoringASCIICase(deviceState, "deviceunavailable"))
        state = MediaPlaybackTargetContext::OutputDeviceUnavailable;
    else if (equalLettersIgnoringASCIICase(deviceState, "unknown"))
        state = MediaPlaybackTargetContext::Unknown;
    else
        return Exception { InvalidAccessError };

    page->setMockMediaPlaybackTargetPickerState(deviceName, state);
    return { };
}

void Internals::mockMediaPlaybackTargetPickerDismissPopup()
{
    auto* page = contextDocument()->frame()->page();
    ASSERT(page);

    page->mockMediaPlaybackTargetPickerDismissPopup();
}

#endif

ExceptionOr<Ref<MockPageOverlay>> Internals::installMockPageOverlay(PageOverlayType type)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    return MockPageOverlayClient::singleton().installOverlay(*document->page(), type == PageOverlayType::View ? PageOverlay::OverlayType::View : PageOverlay::OverlayType::Document);
}

ExceptionOr<String> Internals::pageOverlayLayerTreeAsText(unsigned short flags) const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { InvalidAccessError };

    document->updateLayoutIgnorePendingStylesheets();

    return MockPageOverlayClient::singleton().layerTreeAsText(*document->page(), toLayerTreeFlags(flags));
}

void Internals::setPageMuted(StringView statesString)
{
    Document* document = contextDocument();
    if (!document)
        return;

    WebCore::MediaProducer::MutedStateFlags state = MediaProducer::NoneMuted;
    for (StringView stateString : statesString.split(',')) {
        if (equalLettersIgnoringASCIICase(stateString, "audio"))
            state |= MediaProducer::AudioIsMuted;
        if (equalLettersIgnoringASCIICase(stateString, "capturedevices"))
            state |= MediaProducer::AudioAndVideoCaptureIsMuted;
        if (equalLettersIgnoringASCIICase(stateString, "screencapture"))
            state |= MediaProducer::ScreenCaptureIsMuted;
    }

    if (Page* page = document->page())
        page->setMuted(state);
}

String Internals::pageMediaState()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return emptyString();

    WebCore::MediaProducer::MediaStateFlags state = document->page()->mediaState();
    StringBuilder string;
    if (state & MediaProducer::IsPlayingAudio)
        string.append("IsPlayingAudio,");
    if (state & MediaProducer::IsPlayingVideo)
        string.append("IsPlayingVideo,");
    if (state & MediaProducer::IsPlayingToExternalDevice)
        string.append("IsPlayingToExternalDevice,");
    if (state & MediaProducer::RequiresPlaybackTargetMonitoring)
        string.append("RequiresPlaybackTargetMonitoring,");
    if (state & MediaProducer::ExternalDeviceAutoPlayCandidate)
        string.append("ExternalDeviceAutoPlayCandidate,");
    if (state & MediaProducer::DidPlayToEnd)
        string.append("DidPlayToEnd,");
    if (state & MediaProducer::IsSourceElementPlaying)
        string.append("IsSourceElementPlaying,");

    if (state & MediaProducer::IsNextTrackControlEnabled)
        string.append("IsNextTrackControlEnabled,");
    if (state & MediaProducer::IsPreviousTrackControlEnabled)
        string.append("IsPreviousTrackControlEnabled,");

    if (state & MediaProducer::HasPlaybackTargetAvailabilityListener)
        string.append("HasPlaybackTargetAvailabilityListener,");
    if (state & MediaProducer::HasAudioOrVideo)
        string.append("HasAudioOrVideo,");
    if (state & MediaProducer::HasActiveAudioCaptureDevice)
        string.append("HasActiveAudioCaptureDevice,");
    if (state & MediaProducer::HasActiveVideoCaptureDevice)
        string.append("HasActiveVideoCaptureDevice,");
    if (state & MediaProducer::HasMutedAudioCaptureDevice)
        string.append("HasMutedAudioCaptureDevice,");
    if (state & MediaProducer::HasMutedVideoCaptureDevice)
        string.append("HasMutedVideoCaptureDevice,");
    if (state & MediaProducer::HasUserInteractedWithMediaElement)
        string.append("HasUserInteractedWithMediaElement,");
    if (state & MediaProducer::HasActiveDisplayCaptureDevice)
        string.append("HasActiveDisplayCaptureDevice,");
    if (state & MediaProducer::HasMutedDisplayCaptureDevice)
        string.append("HasMutedDisplayCaptureDevice,");

    if (string.isEmpty())
        string.append("IsNotPlaying");
    else
        string.resize(string.length() - 1);

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
        return Exception { InvalidAccessError };
    return document->page()->defersLoading();
}

RefPtr<File> Internals::createFile(const String& path)
{
    Document* document = contextDocument();
    if (!document)
        return nullptr;

    URL url = document->completeURL(path);
    if (!url.isLocalFile())
        return nullptr;

    return File::create(url.fileSystemPath());
}

void Internals::queueMicroTask(int testNumber)
{
    Document* document = contextDocument();
    if (!document)
        return;

    ScriptExecutionContext* context = document;
    auto& eventLoop = context->eventLoop();
    eventLoop.queueMicrotask([document = makeRef(*document), testNumber]() {
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Debug, makeString("MicroTask #", testNumber, " has run."));
    });
}

#if ENABLE(CONTENT_FILTERING)

MockContentFilterSettings& Internals::mockContentFilterSettings()
{
    return MockContentFilterSettings::singleton();
}

#endif

#if ENABLE(CSS_SCROLL_SNAP)

static void appendOffsets(StringBuilder& builder, const Vector<LayoutUnit>& snapOffsets)
{
    bool justStarting = true;

    builder.appendLiteral("{ ");
    for (auto& coordinate : snapOffsets) {
        if (!justStarting)
            builder.appendLiteral(", ");
        else
            justStarting = false;

        builder.appendNumber(coordinate.toUnsigned());
    }
    builder.appendLiteral(" }");
}

void Internals::setPlatformMomentumScrollingPredictionEnabled(bool enabled)
{
    ScrollingMomentumCalculator::setPlatformMomentumScrollingPredictionEnabled(enabled);
}

ExceptionOr<String> Internals::scrollSnapOffsets(Element& element)
{
    element.document().updateLayoutIgnorePendingStylesheets();

    if (!element.renderBox())
        return String();

    RenderBox& box = *element.renderBox();
    ScrollableArea* scrollableArea;

    if (box.isBody()) {
        FrameView* frameView = box.frame().mainFrame().view();
        if (!frameView || !frameView->isScrollable())
            return Exception { InvalidAccessError };
        scrollableArea = frameView;

    } else {
        if (!box.canBeScrolledAndHasScrollableArea())
            return Exception { InvalidAccessError };
        scrollableArea = box.layer();
    }

    if (!scrollableArea)
        return String();

    StringBuilder result;

    if (auto* offsets = scrollableArea->horizontalSnapOffsets()) {
        if (offsets->size()) {
            result.appendLiteral("horizontal = ");
            appendOffsets(result, *offsets);
        }
    }

    if (auto* offsets = scrollableArea->verticalSnapOffsets()) {
        if (offsets->size()) {
            if (result.length())
                result.appendLiteral(", ");

            result.appendLiteral("vertical = ");
            appendOffsets(result, *offsets);
        }
    }

    return result.toString();
}

#endif

bool Internals::testPreloaderSettingViewport()
{
    return testPreloadScannerViewportSupport(contextDocument());
}

ExceptionOr<String> Internals::pathStringWithShrinkWrappedRects(const Vector<double>& rectComponents, double radius)
{
    if (rectComponents.size() % 4)
        return Exception { InvalidAccessError };

    Vector<FloatRect> rects;
    for (unsigned i = 0; i < rectComponents.size(); i += 4)
        rects.append(FloatRect(rectComponents[i], rectComponents[i + 1], rectComponents[i + 2], rectComponents[i + 3]));

    SVGPathStringBuilder builder;
    PathUtilities::pathWithShrinkWrappedRects(rects, radius).apply([&builder](const PathElement& element) {
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


String Internals::getCurrentMediaControlsStatusForElement(HTMLMediaElement& mediaElement)
{
    return mediaElement.getCurrentMediaControlsStatus();
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

#if ENABLE(STREAMS_API)

bool Internals::isReadableStreamDisturbed(JSC::JSGlobalObject& lexicalGlobalObject, JSValue stream)
{
    return ReadableStream::isDisturbed(lexicalGlobalObject, stream);
}

JSValue Internals::cloneArrayBuffer(JSC::JSGlobalObject& lexicalGlobalObject, JSValue buffer, JSValue srcByteOffset, JSValue srcLength)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(vm.clientData);
    const Identifier& privateName = clientData->builtinNames().cloneArrayBufferPrivateName();
    JSValue value;
    PropertySlot propertySlot(value, PropertySlot::InternalMethodType::Get);
    lexicalGlobalObject.methodTable(vm)->getOwnPropertySlot(&lexicalGlobalObject, &lexicalGlobalObject, privateName, propertySlot);
    value = propertySlot.getValue(&lexicalGlobalObject, privateName);
    ASSERT(value.isCallable(vm));

    JSObject* function = value.getObject();
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != JSC::CallData::Type::None);
    MarkedArgumentBuffer arguments;
    arguments.append(buffer);
    arguments.append(srcByteOffset);
    arguments.append(srcLength);
    ASSERT(!arguments.hasOverflowed());

    return JSC::call(&lexicalGlobalObject, function, callData, JSC::jsUndefined(), arguments);
}

#endif

String Internals::resourceLoadStatisticsForURL(const DOMURL& url)
{
    return ResourceLoadObserver::shared().statisticsForURL(url.href());
}

void Internals::setResourceLoadStatisticsEnabled(bool enable)
{
    DeprecatedGlobalSettings::setResourceLoadStatisticsEnabled(enable);
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

void Internals::withUserGesture(RefPtr<VoidCallback>&& callback)
{
    UserGestureIndicator gestureIndicator(ProcessingUserGesture, contextDocument());
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

double Internals::lastHandledUserGestureTimestamp()
{
    Document* document = contextDocument();
    if (!document)
        return 0;

    return document->lastHandledUserGestureTimestamp().secondsSinceEpoch().value();
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

#if ENABLE(VIDEO)
double Internals::privatePlayerVolume(const HTMLMediaElement&)
{
    return 0;
}
#endif

#endif

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

void Internals::postTask(RefPtr<VoidCallback>&& callback)
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

static Optional<TaskSource> taskSourceFromString(const String& taskSourceName)
{
    if (taskSourceName == "DOMManipulation")
        return TaskSource::DOMManipulation;
    return WTF::nullopt;
}

ExceptionOr<void> Internals::queueTask(ScriptExecutionContext& context, const String& taskSourceName, RefPtr<VoidCallback>&& callback)
{
    auto source = taskSourceFromString(taskSourceName);
    if (!source)
        return Exception { NotSupportedError };

    context.eventLoop().queueTask(*source, [callback = WTFMove(callback)] {
        callback->handleEvent();
    });

    return { };
}

ExceptionOr<void> Internals::queueTaskToQueueMicrotask(Document& document, const String& taskSourceName, RefPtr<VoidCallback>&& callback)
{
    auto source = taskSourceFromString(taskSourceName);
    if (!source)
        return Exception { NotSupportedError };

    ScriptExecutionContext& context = document; // This avoids unnecessarily exporting Document::eventLoop.
    context.eventLoop().queueTask(*source, [movedCallback = WTFMove(callback), protectedDocument = makeRef(document)]() mutable {
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
        return Exception { InvalidStateError };

    auto& proxyFrame = *proxy.frame();
    if (!is<Frame>(proxyFrame))
        return false;
    RefPtr<ScriptExecutionContext> proxyContext = downcast<Frame>(proxyFrame).document();
    if (!proxyContext)
        return Exception { InvalidStateError };

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

#if ENABLE(APPLE_PAY)
void Internals::setApplePayIsActive(Document& document)
{
    document.setApplePayIsActive();
}
#endif

#if ENABLE(WEBGL)
void Internals::simulateWebGLContextChanged(WebGLRenderingContext& context)
{
    context.simulateContextChanged();
}

void Internals::failNextGPUStatusCheck(WebGLRenderingContext& context)
{
    context.setFailNextGPUStatusCheck();
}

bool Internals::hasLowAndHighPowerGPUs()
{
#if PLATFORM(MAC)
    return WebCore::hasLowAndHighPowerGPUs();
#else
    return false;
#endif
}
#endif

void Internals::setPageVisibility(bool isVisible)
{
    auto* document = contextDocument();
    if (!document || !document->page())
        return;
    auto& page = *document->page();
    auto state = page.activityState();

    if (!isVisible)
        state.remove(ActivityState::IsVisible);
    else
        state.add(ActivityState::IsVisible);

    page.setActivityState(state);
}

void Internals::setPageIsFocusedAndActive(bool isFocusedAndActive)
{
    auto* document = contextDocument();
    if (!document || !document->page())
        return;
    auto& page = *document->page();
    auto state = page.activityState();

    if (!isFocusedAndActive)
        state.remove({ ActivityState::IsFocused, ActivityState::WindowIsActive });
    else
        state.add({ ActivityState::IsFocused, ActivityState::WindowIsActive });

    page.setActivityState(state);
}

#if ENABLE(WEB_RTC)
void Internals::setH264HardwareEncoderAllowed(bool allowed)
{
    auto* document = contextDocument();
    if (!document || !document->page())
        return;
    document->page()->libWebRTCProvider().setH264HardwareEncoderAllowed(allowed);
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
        m_trackSource->removeVideoSampleObserver(*this);
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
    m_trackSource->removeObserver(*this);

    m_trackSource = nullptr;
    m_trackAudioSampleCount = 0;
    m_trackVideoSampleCount = 0;
}

void Internals::observeMediaStreamTrack(MediaStreamTrack& track)
{
    stopObservingRealtimeMediaSource();

    m_trackSource = &track.source();
    m_trackSource->addObserver(*this);
    switch (m_trackSource->type()) {
    case RealtimeMediaSource::Type::Audio:
        m_trackSource->addAudioSampleObserver(*this);
        break;
    case RealtimeMediaSource::Type::Video:
        m_trackSource->addVideoSampleObserver(*this);
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
}

void Internals::grabNextMediaStreamTrackFrame(TrackFramePromise&& promise)
{
    m_nextTrackFramePromise = WTF::makeUnique<TrackFramePromise>(promise);
}

void Internals::videoSampleAvailable(MediaSample& sample)
{
    m_trackVideoSampleCount++;
    if (!m_nextTrackFramePromise)
        return;

    auto& videoSettings = m_trackSource->settings();
    if (!videoSettings.width() || !videoSettings.height())
        return;
    
    auto rgba = sample.getRGBAImageData();
    if (!rgba)
        return;
    
    auto imageData = ImageData::create(rgba.releaseNonNull(), videoSettings.width(), videoSettings.height());
    if (!imageData.hasException())
        m_nextTrackFramePromise->resolve(imageData.releaseReturnValue());
    else
        m_nextTrackFramePromise->reject(imageData.exception().code());
    m_nextTrackFramePromise = nullptr;
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
#endif

bool Internals::supportsAudioSession() const
{
#if USE(AUDIO_SESSION)
    return true;
#else
    return false;
#endif
}

String Internals::audioSessionCategory() const
{
#if USE(AUDIO_SESSION)
    switch (AudioSession::sharedSession().category()) {
    case AudioSession::AmbientSound:
        return "AmbientSound"_s;
    case AudioSession::SoloAmbientSound:
        return "SoloAmbientSound"_s;
    case AudioSession::MediaPlayback:
        return "MediaPlayback"_s;
    case AudioSession::RecordAudio:
        return "RecordAudio"_s;
    case AudioSession::PlayAndRecord:
        return "PlayAndRecord"_s;
    case AudioSession::AudioProcessing:
        return "AudioProcessing"_s;
    case AudioSession::None:
        return "None"_s;
    }
#endif
    return emptyString();
}

double Internals::preferredAudioBufferSize() const
{
#if USE(AUDIO_SESSION)
    return AudioSession::sharedSession().preferredBufferSize();
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
#if ENABLE(SERVICE_WORKER)
    if (!contextDocument())
        return;

    auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
    connection.storeRegistrationsOnDiskForTesting([promise = WTFMove(promise)]() mutable {
        promise.resolve();
    });
#else
    promise.resolve();
#endif
}

void Internals::clearCacheStorageMemoryRepresentation(DOMPromiseDeferred<void>&& promise)
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
    m_cacheStorageConnection->clearMemoryRepresentation(ClientOrigin { document->topOrigin().data(), document->securityOrigin().data() }, [promise = WTFMove(promise)] (auto && result) mutable {
        ASSERT_UNUSED(result, !result);
        promise.resolve();
    });
}

void Internals::cacheStorageEngineRepresentation(DOMPromiseDeferred<IDLDOMString>&& promise)
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
    m_cacheStorageConnection->engineRepresentation([promise = WTFMove(promise)](const String& result) mutable {
        promise.resolve(result);
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

#if ENABLE(SERVICE_WORKER)
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
#endif

#if ENABLE(APPLE_PAY)
MockPaymentCoordinator& Internals::mockPaymentCoordinator(Document& document)
{
    return downcast<MockPaymentCoordinator>(document.frame()->page()->paymentCoordinator().client());
}
#endif

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
            builder.append('[', (int)info.type, ",\"", info.request.url().string(), "\",\"", info.request.httpMethod(), "\",", info.response.httpStatusCode(), ']');

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

void Internals::notifyResourceLoadObserver()
{
    ResourceLoadObserver::shared().updateCentralStatisticsStore();
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

bool Internals::supportsVCPEncoder()
{
#if defined(ENABLE_VCP_ENCODER)
    return ENABLE_VCP_ENCODER || ENABLE_VCP_VTB_ENCODER;
#else
    return false;
#endif
}

Optional<HEVCParameterSet> Internals::parseHEVCCodecParameters(const String& codecString)
{
    return WebCore::parseHEVCCodecParameters(codecString);
}

Optional<DoViParameterSet> Internals::parseDoViCodecParameters(const String& codecString)
{
    return WebCore::parseDoViCodecParameters(codecString);
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
    PlatformMediaSessionManager::sharedManager().processWillSuspend();
#endif
}

void Internals::processDidResume()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::sharedManager().processDidResume();
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
    dictionary.set("stringKey"_s, String("stringValue"));
    dictionary.set("uint64Key"_s, std::numeric_limits<uint64_t>::max());
    dictionary.set("int64Key"_s, std::numeric_limits<int64_t>::min());
    dictionary.set("boolKey"_s, true);
    dictionary.set("doubleKey"_s, 2.7182818284590452353602874);

    page->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary("testMessage"_s, "testDescription"_s, dictionary, ShouldSample::No);
}

void Internals::setXHRMaximumIntervalForUserGestureForwarding(XMLHttpRequest& request, double interval)
{
    request.setMaximumIntervalForUserGestureForwarding(interval);
}

void Internals::setTransientActivationDuration(double seconds)
{
    DOMWindow::overrideTransientActivationDurationForTesting(Seconds { seconds });
}

void Internals::setIsPlayingToAutomotiveHeadUnit(bool isPlaying)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::sharedManager().setIsPlayingToAutomotiveHeadUnit(isPlaying);
#endif
}

String Internals::highlightPseudoElementColor(const String& highlightName, Element& element)
{
    element.document().updateStyleIfNeeded();

    auto& styleResolver = element.document().styleScope().resolver();
    auto* parentStyle = element.computedStyle();
    if (!parentStyle)
        return { };

    auto style = styleResolver.pseudoStyleForElement(element, { PseudoId::Highlight, highlightName }, *parentStyle);
    if (!style)
        return { };

    return style->color().cssText();
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
    auto indicator = TextIndicator::createWithRange(range, options.coreOptions(), TextIndicatorPresentationTransition::None);
    return indicator->data();
}

void Internals::addPrefetchLoadEventListener(HTMLLinkElement& link, RefPtr<EventListener>&& listener)
{
    if (RuntimeEnabledFeatures::sharedFeatures().linkPrefetchEnabled() && equalLettersIgnoringASCIICase(link.rel(), "prefetch")) {
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

void Internals::setMaxCanvasPixelMemory(unsigned size)
{
    HTMLCanvasElement::setMaxPixelMemoryForTesting(size);
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
    if (process == "com.apple.WebKit.WebContent")
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
    if (process == "com.apple.WebKit.WebContent")
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

String Internals::windowLocationHost(DOMWindow& window)
{
    return window.location().host();
}

String Internals::systemColorForCSSValue(const String& cssValue, bool useDarkModeAppearance, bool useElevatedUserInterfaceLevel)
{
    CSSValueID id = cssValueKeywordID(cssValue);
    RELEASE_ASSERT(StyleColor::isSystemColor(id));

    OptionSet<StyleColor::Options> options;
    if (useDarkModeAppearance)
        options.add(StyleColor::Options::UseDarkAppearance);
    if (useElevatedUserInterfaceLevel)
        options.add(StyleColor::Options::UseElevatedUserInterfaceLevel);
    
    return RenderTheme::singleton().systemColor(id, options).cssText();
}

bool Internals::systemHasBattery() const
{
#if PLATFORM(COCOA)
    return WebCore::systemHasBattery();
#else
    return false;
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

String Internals::getUTIFromTag(const String&, const String&, const String&)
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

bool Internals::supportsPictureInPicture()
{
    return WebCore::supportsPictureInPicture();
}

String Internals::focusRingColor()
{
    OptionSet<StyleColor::Options> options;
    return RenderTheme::singleton().focusRingColor(options).cssText();
}

unsigned Internals::createSleepDisabler(const String& reason, bool display)
{
    static unsigned lastUsedIdentifier = 0;
    auto sleepDisabler = makeUnique<WebCore::SleepDisabler>(reason.utf8().data(), display ? PAL::SleepDisabler::Type::Display : PAL::SleepDisabler::Type::System);
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
    if (!RuntimeEnabledFeatures::sharedFeatures().webXREnabled())
        return Exception { InvalidAccessError };

    if (!m_xrTest) {
        if (!contextDocument() || !contextDocument()->domWindow())
            return Exception { InvalidAccessError };

        auto* navigator = contextDocument()->domWindow()->optionalNavigator();
        if (!navigator)
            return Exception { InvalidAccessError };

        m_xrTest = WebXRTest::create(makeWeakPtr(&NavigatorWebXR::xr(*navigator)));
    }
    return m_xrTest.get();
}

#endif


} // namespace WebCore
