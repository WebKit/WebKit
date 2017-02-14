/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "ActiveDOMCallbackMicrotask.h"
#include "ApplicationCacheStorage.h"
#include "Autofill.h"
#include "BackForwardController.h"
#include "BitmapImage.h"
#include "CSSAnimationController.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ComposedTreeIterator.h"
#include "Cursor.h"
#include "DOMPath.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
#include "DisplayList.h"
#include "Document.h"
#include "DocumentMarker.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "ExceptionCode.h"
#include "ExtensionStyleSheets.h"
#include "File.h"
#include "FontCache.h"
#include "FormController.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GCObservation.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HTMLPreloadScanner.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
#include "IconController.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorFrontendClientLocal.h"
#include "InspectorOverlay.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "InternalSettings.h"
#include "Language.h"
#include "LibWebRTCProvider.h"
#include "MainFrame.h"
#include "MallocStatistics.h"
#include "MediaPlayer.h"
#include "MediaProducer.h"
#include "MemoryCache.h"
#include "MemoryInfo.h"
#include "MemoryPressureHandler.h"
#include "MockLibWebRTCPeerConnection.h"
#include "MockPageOverlay.h"
#include "MockPageOverlayClient.h"
#include "Page.h"
#include "PageCache.h"
#include "PageOverlay.h"
#include "PathUtilities.h"
#include "PlatformMediaSessionManager.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "Range.h"
#include "RenderEmbeddedObject.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderMenuList.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "ResourceLoadObserver.h"
#include "SVGDocumentExtensions.h"
#include "SVGPathStringBuilder.h"
#include "SchemeRegistry.h"
#include "ScriptedAnimationController.h"
#include "ScrollingCoordinator.h"
#include "ScrollingMomentumCalculator.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceBuffer.h"
#include "SpellChecker.h"
#include "StaticNodeList.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "TextIterator.h"
#include "TreeScope.h"
#include "TypeConversions.h"
#include "UserGestureIndicator.h"
#include "UserMediaController.h"
#include "ViewportArguments.h"
#include "WebCoreJSClientData.h"
#include "WorkerThread.h"
#include "WritingDirection.h"
#include "XMLHttpRequest.h"
#include <bytecode/CodeBlock.h>
#include <inspector/InspectorAgentBase.h>
#include <inspector/InspectorFrontendChannel.h>
#include <inspector/InspectorValues.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSCJSValue.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(PROXIMITY_EVENTS)
#include "DeviceProximityController.h"
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
#endif

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#include "TimeRanges.h"
#endif

#if ENABLE(SPEECH_SYNTHESIS)
#include "DOMWindowSpeechSynthesis.h"
#include "PlatformSpeechSynthesizerMock.h"
#include "SpeechSynthesis.h"
#endif

#if ENABLE(VIBRATION)
#include "Vibration.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MockRealtimeMediaSourceCenter.h"
#endif

#if ENABLE(WEB_RTC)
#include "MockMediaEndpoint.h"
#include "RTCPeerConnection.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MockMediaPlayerMediaSource.h"
#endif

#if PLATFORM(MAC)
#include "DictionaryLookup.h"
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

#if USE(QUICK_LOOK)
#include "MockQuickLookHandleClient.h"
#include "QuickLook.h"
#endif

using JSC::CallData;
using JSC::CallType;
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

using namespace Inspector;

namespace WebCore {

using namespace HTMLNames;

class InspectorStubFrontend final : public InspectorFrontendClientLocal, public FrontendChannel {
public:
    InspectorStubFrontend(Page& inspectedPage, RefPtr<DOMWindow>&& frontendWindow);
    virtual ~InspectorStubFrontend();

private:
    void attachWindow(DockSide) final { }
    void detachWindow() final { }
    void closeWindow() final;
    void bringToFront() final { }
    String localizedStringsURL() final { return String(); }
    void inspectedURLChanged(const String&) final { }
    void setAttachedWindowHeight(unsigned) final { }
    void setAttachedWindowWidth(unsigned) final { }

    void sendMessageToFrontend(const String& message) final;
    ConnectionType connectionType() const final { return ConnectionType::Local; }

    Page* frontendPage() const
    {
        if (!m_frontendWindow || !m_frontendWindow->document())
            return nullptr;

        return m_frontendWindow->document()->page();
    }

    RefPtr<DOMWindow> m_frontendWindow;
    InspectorController& m_frontendController;
};

InspectorStubFrontend::InspectorStubFrontend(Page& inspectedPage, RefPtr<DOMWindow>&& frontendWindow)
    : InspectorFrontendClientLocal(&inspectedPage.inspectorController(), frontendWindow->document()->page(), std::make_unique<InspectorFrontendClientLocal::Settings>())
    , m_frontendWindow(frontendWindow.copyRef())
    , m_frontendController(frontendPage()->inspectorController())
{
    ASSERT_ARG(frontendWindow, frontendWindow);

    m_frontendController.setInspectorFrontendClient(this);
    inspectedPage.inspectorController().connectFrontend(this);
}

InspectorStubFrontend::~InspectorStubFrontend()
{
    closeWindow();
}

void InspectorStubFrontend::closeWindow()
{
    if (!m_frontendWindow)
        return;

    m_frontendController.setInspectorFrontendClient(nullptr);
    inspectedPage()->inspectorController().disconnectFrontend(this);

    m_frontendWindow->close();
    m_frontendWindow = nullptr;
}

void InspectorStubFrontend::sendMessageToFrontend(const String& message)
{
    ASSERT_ARG(message, !message.isEmpty());

    InspectorClient::doDispatchMessageOnFrontendPage(frontendPage(), message);
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

static bool markerTypesFrom(const String& markerType, DocumentMarker::MarkerTypes& result)
{
    DocumentMarker::MarkerType singularResult;

    if (markerType.isEmpty() || equalLettersIgnoringASCIICase(markerType, "all"))
        result = DocumentMarker::AllMarkers();
    else if (markerTypeFrom(markerType, singularResult))
        result = singularResult;
    else
        return false;

    return true;
}

const char* Internals::internalsId = "internals";

Ref<Internals> Internals::create(Document& document)
{
    return adoptRef(*new Internals(document));
}

Internals::~Internals()
{
}

void Internals::resetToConsistentState(Page& page)
{
    page.setPageScaleFactor(1, IntPoint(0, 0));
    page.setPagination(Pagination());
    page.setPaginationLineGridEnabled(false);

    page.setDefersLoading(false);
    
    page.mainFrame().setTextZoomFactor(1.0f);
    
    FrameView* mainFrameView = page.mainFrame().view();
    if (mainFrameView) {
        mainFrameView->setHeaderHeight(0);
        mainFrameView->setFooterHeight(0);
        page.setTopContentInset(0);
        mainFrameView->setUseFixedLayout(false);
        mainFrameView->setFixedLayoutSize(IntSize());
#if USE(COORDINATED_GRAPHICS)
        mainFrameView->setFixedVisibleContentRect(IntRect());
#endif
        if (auto* backing = mainFrameView->tiledBacking())
            backing->setTileSizeUpdateDelayDisabledForTesting(false);
    }

    WebCore::clearDefaultPortForProtocolMapForTesting();
    WebCore::overrideUserPreferredLanguages(Vector<String>());
    WebCore::Settings::setUsesOverlayScrollbars(false);
    WebCore::Settings::setUsesMockScrollAnimator(false);
#if ENABLE(VIDEO_TRACK)
    page.group().captionPreferences().setTestingMode(true);
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
#if HAVE(ACCESSIBILITY)
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

    page.setShowAllPlugins(false);

#if USE(QUICK_LOOK)
    MockQuickLookHandleClient::singleton().setPassword("");
    QuickLookHandle::setClientForTesting(nullptr);
#endif
}

Internals::Internals(Document& document)
    : ContextDestructionObserver(&document)
{
#if ENABLE(VIDEO_TRACK)
    if (document.page())
        document.page()->group().captionPreferences().setTestingMode(true);
#endif

#if ENABLE(MEDIA_STREAM)
    setMockMediaCaptureDevicesEnabled(true);
    WebCore::Settings::setMediaCaptureRequiresSecureConnection(false);
#endif

#if ENABLE(WEB_RTC)
    enableMockMediaEndpoint();
    useMockRTCPeerConnectionFactory(String());
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

unsigned Internals::workerThreadCount() const
{
    return WorkerThread::workerThreadCount();
}

bool Internals::areSVGAnimationsPaused() const
{
    auto* document = contextDocument();
    if (!document)
        return false;

    if (!document->svgExtensions())
        return false;

    return document->accessSVGExtensions().areAnimationsPaused();
}

String Internals::address(Node& node)
{
    return String::format("%p", &node);
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
    request.setDomainForCachePartition(contextDocument()->topOrigin().domainForCachePartition());
    
    CachedResource* resource = MemoryCache::singleton().resourceForRequest(request, contextDocument()->page()->sessionID());
    return resource && resource->status() == CachedResource::Cached;
}

String Internals::xhrResponseSource(XMLHttpRequest& request)
{
    if (request.resourceResponse().isNull())
        return "Null response";
    switch (request.resourceResponse().source()) {
    case ResourceResponse::Source::Unknown:
        return "Unknown";
    case ResourceResponse::Source::Network:
        return "Network";
    case ResourceResponse::Source::DiskCache:
        return "Disk cache";
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return "Disk cache after validation";
    case ResourceResponse::Source::MemoryCache:
        return "Memory cache";
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return "Memory cache after validation";
    }
    ASSERT_NOT_REACHED();
    return "Error";
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
        return UseProtocolCachePolicy;
    case Internals::CachePolicy::ReloadIgnoringCacheData:
        return ReloadIgnoringCacheData;
    case Internals::CachePolicy::ReturnCacheDataElseLoad:
        return ReturnCacheDataElseLoad;
    case Internals::CachePolicy::ReturnCacheDataDontLoad:
        return ReturnCacheDataDontLoad;
    }
    ASSERT_NOT_REACHED();
    return UseProtocolCachePolicy;
}

void Internals::setOverrideCachePolicy(CachePolicy policy)
{
    frame()->loader().setOverrideCachePolicyForTesting(toResourceRequestCachePolicy(policy));
}

ExceptionOr<void> Internals::setCanShowModalDialogOverride(bool allow)
{
    if (!contextDocument() || !contextDocument()->domWindow())
        return Exception { INVALID_ACCESS_ERR };

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

unsigned Internals::memoryCacheSize() const
{
    return MemoryCache::singleton().size();
}

unsigned Internals::imageFrameIndex(HTMLImageElement& element)
{
    auto* cachedImage = element.cachedImage();
    if (!cachedImage)
        return 0;

    auto* image = cachedImage->image();
    return is<BitmapImage>(image) ? downcast<BitmapImage>(*image).currentFrame() : 0;
}

void Internals::setImageFrameDecodingDuration(HTMLImageElement& element, float duration)
{
    auto* cachedImage = element.cachedImage();
    if (!cachedImage)
        return;
    
    auto* image = cachedImage->image();
    if (!is<BitmapImage>(image))
        return;
    
    downcast<BitmapImage>(*image).setFrameDecodingDurationForTesting(duration);
}

void Internals::resetImageAnimation(HTMLImageElement& element)
{
    auto* cachedImage = element.cachedImage();
    if (!cachedImage)
        return;

    auto* image = cachedImage->image();
    if (!is<BitmapImage>(image))
        return;

    image->resetAnimation();
}

void Internals::clearPageCache()
{
    PageCache::singleton().pruneToSizeNow(0, PruningReason::None);
}

unsigned Internals::pageCacheSize() const
{
    return PageCache::singleton().pageCount();
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
        return Exception { INVALID_ACCESS_ERR };

    return contextDocument()->page()->lastSpatialNavigationCandidateCount();
}

unsigned Internals::numberOfActiveAnimations() const
{
    return frame()->animation().numberOfActiveAnimations(frame()->document());
}

ExceptionOr<bool> Internals::animationsAreSuspended() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return document->frame()->animation().animationsAreSuspendedForDocument(document);
}

ExceptionOr<void> Internals::suspendAnimations() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->frame()->animation().suspendAnimationsForDocument(document);

    for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            frame->animation().suspendAnimationsForDocument(document);
    }

    return { };
}

ExceptionOr<void> Internals::resumeAnimations() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->frame()->animation().resumeAnimationsForDocument(document);

    for (Frame* frame = document->frame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            frame->animation().resumeAnimationsForDocument(document);
    }

    return { };
}

ExceptionOr<bool> Internals::pauseAnimationAtTimeOnElement(const String& animationName, double pauseTime, Element& element)
{
    if (pauseTime < 0)
        return Exception { INVALID_ACCESS_ERR };
    return frame()->animation().pauseAnimationAtTime(element.renderer(), AtomicString(animationName), pauseTime);
}

ExceptionOr<bool> Internals::pauseAnimationAtTimeOnPseudoElement(const String& animationName, double pauseTime, Element& element, const String& pseudoId)
{
    if (pauseTime < 0)
        return Exception { INVALID_ACCESS_ERR };

    if (pseudoId != "before" && pseudoId != "after")
        return Exception { INVALID_ACCESS_ERR };

    PseudoElement* pseudoElement = pseudoId == "before" ? element.beforePseudoElement() : element.afterPseudoElement();
    if (!pseudoElement)
        return Exception { INVALID_ACCESS_ERR };

    return frame()->animation().pauseAnimationAtTime(pseudoElement->renderer(), AtomicString(animationName), pauseTime);
}

ExceptionOr<bool> Internals::pauseTransitionAtTimeOnElement(const String& propertyName, double pauseTime, Element& element)
{
    if (pauseTime < 0)
        return Exception { INVALID_ACCESS_ERR };
    return frame()->animation().pauseTransitionAtTime(element.renderer(), propertyName, pauseTime);
}

ExceptionOr<bool> Internals::pauseTransitionAtTimeOnPseudoElement(const String& property, double pauseTime, Element& element, const String& pseudoId)
{
    if (pauseTime < 0)
        return Exception { INVALID_ACCESS_ERR };

    if (pseudoId != "before" && pseudoId != "after")
        return Exception { INVALID_ACCESS_ERR };

    PseudoElement* pseudoElement = pseudoId == "before" ? element.beforePseudoElement() : element.afterPseudoElement();
    if (!pseudoElement)
        return Exception { INVALID_ACCESS_ERR };

    return frame()->animation().pauseTransitionAtTime(pseudoElement->renderer(), property, pauseTime);
}

ExceptionOr<String> Internals::elementRenderTreeAsText(Element& element)
{
    element.document().updateStyleIfNeeded();

    String representation = externalRepresentation(&element);
    if (representation.isEmpty())
        return Exception { INVALID_ACCESS_ERR };

    return WTFMove(representation);
}

bool Internals::hasPausedImageAnimations(Element& element)
{
    return element.renderer() && element.renderer()->hasPausedImageAnimations();
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
        return Exception { INVALID_ACCESS_ERR };

    switch (downcast<ShadowRoot>(root).mode()) {
    case ShadowRootMode::UserAgent:
        return String("UserAgentShadowRoot");
    case ShadowRootMode::Closed:
        return String("ClosedShadowRoot");
    case ShadowRootMode::Open:
        return String("OpenShadowRoot");
    default:
        ASSERT_NOT_REACHED();
        return String("Unknown");
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
        if (is<StyleRule>(rule.get())) {
            auto* cssRule = downcast<StyleRule>(rule.get());
            if (!cssRule->propertiesWithoutDeferredParsing())
                count++;
            continue;
        }
        
        StyleRuleGroup* groupRule = nullptr;
        if (is<StyleRuleMedia>(rule.get()))
            groupRule = downcast<StyleRuleMedia>(rule.get());
        else if (is<StyleRuleSupports>(rule.get()))
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
        if (is<StyleRuleMedia>(rule.get()))
            groupRule = downcast<StyleRuleMedia>(rule.get());
        else if (is<StyleRuleSupports>(rule.get()))
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
        if (is<StyleRuleKeyframes>(rule.get())) {
            auto* cssRule = downcast<StyleRuleKeyframes>(rule.get());
            if (!cssRule->keyframesWithoutDeferredParsing())
                count++;
            continue;
        }
        
        StyleRuleGroup* groupRule = nullptr;
        if (is<StyleRuleMedia>(rule.get()))
            groupRule = downcast<StyleRuleMedia>(rule.get());
        else if (is<StyleRuleSupports>(rule.get()))
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
    DOMTimer* timer = scriptExecutionContext()->findTimeout(timeoutId);
    if (!timer)
        return Exception { NOT_FOUND_ERR };
    return timer->m_throttleState == DOMTimer::ShouldThrottle;
}

bool Internals::isRequestAnimationFrameThrottled() const
{
    auto* scriptedAnimationController = contextDocument()->scriptedAnimationController();
    if (!scriptedAnimationController)
        return false;
    return scriptedAnimationController->isThrottled();
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
    if (is<HTMLTextFormControlElement>(element)) {
        const HTMLTextFormControlElement& textFormControlElement = downcast<HTMLTextFormControlElement>(element);
        if (!textFormControlElement.isPlaceholderVisible())
            return String();
        if (HTMLElement* placeholderElement = textFormControlElement.placeholderElement())
            return placeholderElement->textContent();
    }

    return String();
}

void Internals::selectColorInColorChooser(HTMLInputElement& element, const String& colorValue)
{
    element.selectColor(Color(colorValue));
}

ExceptionOr<Vector<String>> Internals::formControlStateOfPreviousHistoryItem()
{
    HistoryItem* mainItem = frame()->loader().history().previousItem();
    if (!mainItem)
        return Exception { INVALID_ACCESS_ERR };
    String uniqueName = frame()->tree().uniqueName();
    if (mainItem->target() != uniqueName && !mainItem->childItemWithTarget(uniqueName))
        return Exception { INVALID_ACCESS_ERR };
    return Vector<String> { mainItem->target() == uniqueName ? mainItem->documentState() : mainItem->childItemWithTarget(uniqueName)->documentState() };
}

ExceptionOr<void> Internals::setFormControlStateOfPreviousHistoryItem(const Vector<String>& state)
{
    HistoryItem* mainItem = frame()->loader().history().previousItem();
    if (!mainItem)
        return Exception { INVALID_ACCESS_ERR };
    String uniqueName = frame()->tree().uniqueName();
    if (mainItem->target() == uniqueName)
        mainItem->setDocumentState(state);
    else if (HistoryItem* subItem = mainItem->childItemWithTarget(uniqueName))
        subItem->setDocumentState(state);
    else
        return Exception { INVALID_ACCESS_ERR };
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
    
    synthesis->setPlatformSynthesizer(std::make_unique<PlatformSpeechSynthesizerMock>(synthesis));
}

#endif

#if ENABLE(WEB_RTC)

void Internals::enableMockMediaEndpoint()
{
    MediaEndpoint::create = MockMediaEndpoint::create;
}

void Internals::emulateRTCPeerConnectionPlatformEvent(RTCPeerConnection& connection, const String& action)
{
    connection.emulatePlatformEvent(action);
}

void Internals::useMockRTCPeerConnectionFactory(const String& testCase)
{
#if USE(LIBWEBRTC)
    Document* document = contextDocument();
    LibWebRTCProvider* provider = (document && document->page()) ? &document->page()->libWebRTCProvider() : nullptr;
    WebCore::useMockRTCPeerConnectionFactory(provider, testCase);
#else
    UNUSED_PARAM(testCase);
#endif
}

#endif

#if ENABLE(MEDIA_STREAM)

void Internals::setMockMediaCaptureDevicesEnabled(bool enabled)
{
    WebCore::Settings::setMockCaptureDevicesEnabled(enabled);
}

#endif

ExceptionOr<Ref<ClientRect>> Internals::absoluteCaretBounds()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return ClientRect::create(document->frame()->selection().absoluteCaretBounds());
}

Ref<ClientRect> Internals::boundingBox(Element& element)
{
    element.document().updateLayoutIgnorePendingStylesheets();
    auto renderer = element.renderer();
    if (!renderer)
        return ClientRect::create();
    return ClientRect::create(renderer->absoluteBoundingBoxRectIgnoringTransforms());
}

ExceptionOr<Ref<ClientRectList>> Internals::inspectorHighlightRects()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

    Highlight highlight;
    document->page()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::View);
    return ClientRectList::create(highlight.quads);
}

ExceptionOr<String> Internals::inspectorHighlightObject()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

    return document->page()->inspectorController().buildObjectForHighlightedNodes()->toJSONString();
}

ExceptionOr<unsigned> Internals::markerCountForNode(Node& node, const String& markerType)
{
    DocumentMarker::MarkerTypes markerTypes = 0;
    if (!markerTypesFrom(markerType, markerTypes))
        return Exception { SYNTAX_ERR };

    node.document().frame()->editor().updateEditorUINowIfScheduled();
    return node.document().markers().markersFor(&node, markerTypes).size();
}

ExceptionOr<RenderedDocumentMarker*> Internals::markerAt(Node& node, const String& markerType, unsigned index)
{
    node.document().updateLayoutIgnorePendingStylesheets();

    DocumentMarker::MarkerTypes markerTypes = 0;
    if (!markerTypesFrom(markerType, markerTypes))
        return Exception { SYNTAX_ERR };

    node.document().frame()->editor().updateEditorUINowIfScheduled();

    Vector<RenderedDocumentMarker*> markers = node.document().markers().markersFor(&node, markerTypes);
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
        return Exception { SYNTAX_ERR };

    contextDocument()->markers().updateRectsForInvalidatedMarkersOfType(markerType);
    auto rects = contextDocument()->markers().renderedRectsForMarkers(markerType);

    StringBuilder rectString;
    rectString.appendLiteral("marker rects: ");
    for (const auto& rect : rects) {
        rectString.append('(');
        rectString.appendNumber(rect.x());
        rectString.appendLiteral(", ");
        rectString.appendNumber(rect.y());
        rectString.appendLiteral(", ");
        rectString.appendNumber(rect.width());
        rectString.appendLiteral(", ");
        rectString.appendNumber(rect.height());
        rectString.appendLiteral(") ");
    }
    return rectString.toString();
}

void Internals::addTextMatchMarker(const Range& range, bool isActive)
{
    range.ownerDocument().updateLayoutIgnorePendingStylesheets();
    range.ownerDocument().markers().addTextMatchMarker(&range, isActive);
}

ExceptionOr<void> Internals::setMarkedTextMatchesAreHighlighted(bool flag)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };
    document->frame()->editor().setMarkedTextMatchesAreHighlighted(flag);
    return { };
}

void Internals::invalidateFontCache()
{
    FontCache::singleton().invalidate();
}

ExceptionOr<void> Internals::setScrollViewPosition(int x, int y)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

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

ExceptionOr<Ref<ClientRect>> Internals::layoutViewportRect()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->updateLayoutIgnorePendingStylesheets();

    auto& frameView = *document->view();
    return ClientRect::create(frameView.layoutViewportRect());
}

ExceptionOr<Ref<ClientRect>> Internals::visualViewportRect()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->updateLayoutIgnorePendingStylesheets();

    auto& frameView = *document->view();
    return ClientRect::create(frameView.visualViewportRect());
}

ExceptionOr<void> Internals::setViewBaseBackgroundColor(const String& colorValue)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setBaseBackgroundColor(Color(colorValue));
    return { };
}

ExceptionOr<void> Internals::setPagination(const String& mode, int gap, int pageLength)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

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
        return Exception { SYNTAX_ERR };

    pagination.gap = gap;
    pagination.pageLength = pageLength;
    document->page()->setPagination(pagination);

    return { };
}

ExceptionOr<void> Internals::setPaginationLineGridEnabled(bool enabled)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };
    document->page()->setPaginationLineGridEnabled(enabled);
    return { };
}

ExceptionOr<String> Internals::configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

    const int defaultLayoutWidthForNonMobilePages = 980;

    ViewportArguments arguments = document->page()->viewportArguments();
    ViewportAttributes attributes = computeViewportAttributes(arguments, defaultLayoutWidthForNonMobilePages, deviceWidth, deviceHeight, devicePixelRatio, IntSize(availableWidth, availableHeight));
    restrictMinimumScaleFactorToViewportSize(attributes, IntSize(availableWidth, availableHeight), devicePixelRatio);
    restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    return String { "viewport size " + String::number(attributes.layoutSize.width()) + "x" + String::number(attributes.layoutSize.height()) + " scale " + String::number(attributes.initialScale) + " with limits [" + String::number(attributes.minimumScale) + ", " + String::number(attributes.maximumScale) + "] and userScalable " + (attributes.userScalable ? "true" : "false") };
}

ExceptionOr<bool> Internals::wasLastChangeUserEdit(Element& textField)
{
    if (is<HTMLInputElement>(textField))
        return downcast<HTMLInputElement>(textField).lastChangeWasUserEdit();

    if (is<HTMLTextAreaElement>(textField))
        return downcast<HTMLTextAreaElement>(textField).lastChangeWasUserEdit();

    return Exception { INVALID_NODE_TYPE_ERR };
}

bool Internals::elementShouldAutoComplete(HTMLInputElement& element)
{
    return element.shouldAutocomplete();
}

void Internals::setEditingValue(HTMLInputElement& element, const String& value)
{
    element.setEditingValue(value);
}

void Internals::setAutofilled(HTMLInputElement& element, bool enabled)
{
    element.setAutoFilled(enabled);
}

static AutoFillButtonType toAutoFillButtonType(Internals::AutoFillButtonType type)
{
    switch (type) {
    case Internals::AutoFillButtonType::AutoFillButtonTypeNone:
        return AutoFillButtonType::None;
    case Internals::AutoFillButtonType::AutoFillButtonTypeCredentials:
        return AutoFillButtonType::Credentials;
    case Internals::AutoFillButtonType::AutoFillButtonTypeContacts:
        return AutoFillButtonType::Contacts;
    }
    ASSERT_NOT_REACHED();
    return AutoFillButtonType::None;
}

void Internals::setShowAutoFillButton(HTMLInputElement& element, AutoFillButtonType type)
{
    element.setShowAutoFillButton(toAutoFillButtonType(type));
}

ExceptionOr<void> Internals::scrollElementToRect(Element& element, int x, int y, int w, int h)
{
    FrameView* frameView = element.document().view();
    if (!frameView)
        return Exception { INVALID_ACCESS_ERR };
    frameView->scrollElementToRect(element, { x, y, w, h });
    return { };
}

ExceptionOr<String> Internals::autofillFieldName(Element& element)
{
    if (!is<HTMLFormControlElement>(element))
        return Exception { INVALID_NODE_TYPE_ERR };

    return String { downcast<HTMLFormControlElement>(element).autofillData().fieldName };
}

ExceptionOr<void> Internals::paintControlTints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->paintControlTints();
    return { };
}

RefPtr<Range> Internals::rangeFromLocationAndLength(Element& scope, int rangeLocation, int rangeLength)
{
    return TextIterator::rangeFromLocationAndLength(&scope, rangeLocation, rangeLength);
}

unsigned Internals::locationFromRange(Element& scope, const Range& range)
{
    size_t location = 0;
    size_t unusedLength = 0;
    TextIterator::getLocationAndLengthFromRange(&scope, &range, location, unusedLength);
    return location;
}

unsigned Internals::lengthFromRange(Element& scope, const Range& range)
{
    size_t unusedLocation = 0;
    size_t length = 0;
    TextIterator::getLocationAndLengthFromRange(&scope, &range, unusedLocation, length);
    return length;
}

String Internals::rangeAsText(const Range& range)
{
    return range.text();
}

Ref<Range> Internals::subrange(Range& range, int rangeLocation, int rangeLength)
{
    return TextIterator::subrange(&range, rangeLocation, rangeLength);
}

RefPtr<Range> Internals::rangeOfStringNearLocation(const Range& searchRange, const String& text, unsigned targetOffset)
{
    return findClosestPlainText(searchRange, text, 0, targetOffset);
}

ExceptionOr<RefPtr<Range>> Internals::rangeForDictionaryLookupAtLocation(int x, int y)
{
#if PLATFORM(MAC)
    auto* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->updateLayoutIgnorePendingStylesheets();
    
    HitTestResult result = document->frame()->mainFrame().eventHandler().hitTestResultAtPoint(IntPoint(x, y));
    NSDictionary *options = nullptr;
    return DictionaryLookup::rangeAtHitTestResult(result, &options);
#else
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    return Exception { INVALID_ACCESS_ERR };
#endif
}

ExceptionOr<void> Internals::setDelegatesScrolling(bool enabled)
{
    Document* document = contextDocument();
    // Delegate scrolling is valid only on mainframe's view.
    if (!document || !document->view() || !document->page() || &document->page()->mainFrame() != document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setDelegatesScrolling(enabled);
    return { };
}

ExceptionOr<int> Internals::lastSpellCheckRequestSequence()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return document->frame()->editor().spellChecker().lastRequestSequence();
}

ExceptionOr<int> Internals::lastSpellCheckProcessedSequence()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return document->frame()->editor().spellChecker().lastProcessedSequence();
}

Vector<String> Internals::userPreferredLanguages() const
{
    return WebCore::userPreferredLanguages();
}

void Internals::setUserPreferredLanguages(const Vector<String>& languages)
{
    WebCore::overrideUserPreferredLanguages(languages);
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
        return Exception { INVALID_ACCESS_ERR };

    return document->wheelEventHandlerCount();
}

ExceptionOr<unsigned> Internals::touchEventHandlerCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { INVALID_ACCESS_ERR };

    return document->touchEventHandlerCount();
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
ExceptionOr<RefPtr<NodeList>> Internals::nodesFromRect(Document& document, int centerX, int centerY, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, bool allowChildFrameContent) const
{
    if (!document.frame() || !document.frame()->view())
        return Exception { INVALID_ACCESS_ERR };

    Frame* frame = document.frame();
    FrameView* frameView = document.view();
    RenderView* renderView = document.renderView();
    if (!renderView)
        return nullptr;

    document.updateLayoutIgnorePendingStylesheets();

    float zoomFactor = frame->pageZoomFactor();
    LayoutPoint point(centerX * zoomFactor + frameView->scrollX(), centerY * zoomFactor + frameView->scrollY());

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active;
    if (ignoreClipping)
        hitType |= HitTestRequest::IgnoreClipping;
    if (!allowShadowContent)
        hitType |= HitTestRequest::DisallowUserAgentShadowContent;
    if (allowChildFrameContent)
        hitType |= HitTestRequest::AllowChildFrameContent;

    HitTestRequest request(hitType);

    // When ignoreClipping is false, this method returns null for coordinates outside of the viewport.
    if (!request.ignoreClipping() && !frameView->visibleContentRect().intersects(HitTestLocation::rectForPoint(point, topPadding, rightPadding, bottomPadding, leftPadding)))
        return nullptr;

    Vector<Ref<Node>> matches;

    // Need padding to trigger a rect based hit test, but we want to return a NodeList
    // so we special case this.
    if (!topPadding && !rightPadding && !bottomPadding && !leftPadding) {
        HitTestResult result(point);
        renderView->hitTest(request, result);
        if (result.innerNode())
            matches.append(*result.innerNode()->deprecatedShadowAncestorNode());
    } else {
        HitTestResult result(point, topPadding, rightPadding, bottomPadding, leftPadding);
        renderView->hitTest(request, result);
        
        const HitTestResult::NodeSet& nodeSet = result.rectBasedTestResult();
        matches.reserveInitialCapacity(nodeSet.size());
        for (auto& node : nodeSet)
            matches.uncheckedAppend(*node);
    }

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
    JSC::ExecState* exec = vm.topCallFrame;
    ScriptExecutable* executable;

    if (!code || code.isNull() || code.isUndefined()) {
        GetCallerCodeBlockFunctor iter;
        exec->iterate(iter);
        CodeBlock* codeBlock = iter.codeBlock();
        executable = codeBlock->ownerScriptExecutable();
    } else if (code.isFunction()) {
        JSFunction* funcObj = JSC::jsCast<JSFunction*>(code.toObject(exec));
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
        String inferredName = funcExecutable->inferredName().string();
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

ExceptionOr<void> Internals::setDeviceProximity(const String&, double value, double min, double max)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

#if ENABLE(PROXIMITY_EVENTS)
    DeviceProximityController::from(document->page())->didChangeDeviceProximity(value, min, max);
#else
    UNUSED_PARAM(value);
    UNUSED_PARAM(min);
    UNUSED_PARAM(max);
#endif
    return { };
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

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}
    
bool Internals::hasAutocorrectedMarker(int from, int length)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Autocorrected, from, length);
}

void Internals::setContinuousSpellCheckingEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

    if (enabled != contextDocument()->frame()->editor().isContinuousSpellCheckingEnabled())
        contextDocument()->frame()->editor().toggleContinuousSpellChecking();
}

void Internals::setAutomaticQuoteSubstitutionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->frame()->editor().isAutomaticQuoteSubstitutionEnabled())
        contextDocument()->frame()->editor().toggleAutomaticQuoteSubstitution();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticLinkDetectionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->frame()->editor().isAutomaticLinkDetectionEnabled())
        contextDocument()->frame()->editor().toggleAutomaticLinkDetection();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticDashSubstitutionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->frame()->editor().isAutomaticDashSubstitutionEnabled())
        contextDocument()->frame()->editor().toggleAutomaticDashSubstitution();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticTextReplacementEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->frame()->editor().isAutomaticTextReplacementEnabled())
        contextDocument()->frame()->editor().toggleAutomaticTextReplacement();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setAutomaticSpellingCorrectionEnabled(bool enabled)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    if (enabled != contextDocument()->frame()->editor().isAutomaticSpellingCorrectionEnabled())
        contextDocument()->frame()->editor().toggleAutomaticSpellingCorrection();
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::handleAcceptedCandidate(const String& candidate, unsigned location, unsigned length)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

    TextCheckingResult result;
    result.type = TextCheckingTypeNone;
    result.location = location;
    result.length = length;
    result.replacement = candidate;
    contextDocument()->frame()->editor().handleAcceptedCandidate(result);
}

bool Internals::isOverwriteModeEnabled()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    return document->frame()->editor().isOverwriteModeEnabled();
}

void Internals::toggleOverwriteModeEnabled()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return;

    document->frame()->editor().toggleOverwriteModeEnabled();
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
    FindOptions result = 0;
    for (auto& option : optionList) {
        bool found = false;
        for (auto& flag : flagList) {
            if (flag.name == option) {
                result |= flag.value;
                found = true;
                break;
            }
        }
        if (!found)
            return Exception { SYNTAX_ERR };
    }
    return result;
}

ExceptionOr<RefPtr<Range>> Internals::rangeOfString(const String& text, RefPtr<Range>&& referenceRange, const Vector<String>& findOptions)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    return document->frame()->editor().rangeOfString(text, referenceRange.get(), parsedOptions.releaseReturnValue());
}

ExceptionOr<unsigned> Internals::countMatchesForText(const String& text, const Vector<String>& findOptions, const String& markMatches)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    bool mark = markMatches == "mark";
    return document->frame()->editor().countMatchesForText(text, nullptr, parsedOptions.releaseReturnValue(), 1000, mark, nullptr);
}

ExceptionOr<unsigned> Internals::countFindMatches(const String& text, const Vector<String>& findOptions)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

    auto parsedOptions = parseFindOptions(findOptions);
    if (parsedOptions.hasException())
        return parsedOptions.releaseException();

    return document->page()->countFindMatches(text, parsedOptions.releaseReturnValue(), 1000);
}

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

RefPtr<DOMWindow> Internals::openDummyInspectorFrontend(const String& url)
{
    auto* inspectedPage = contextDocument()->frame()->page();
    auto* window = inspectedPage->mainFrame().document()->domWindow();
    auto frontendWindow = window->open(url, "", "", *window, *window);
    m_inspectorFrontend = std::make_unique<InspectorStubFrontend>(*inspectedPage, frontendWindow.copyRef());
    return frontendWindow;
}

void Internals::closeDummyInspectorFrontend()
{
    m_inspectorFrontend = nullptr;
}

ExceptionOr<void> Internals::setInspectorIsUnderTest(bool isUnderTest)
{
    Page* page = contextDocument()->frame()->page();
    if (!page)
        return Exception { INVALID_ACCESS_ERR };

    page->inspectorController().setIsUnderTest(isUnderTest);
    return { };
}

bool Internals::hasGrammarMarker(int from, int length)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Grammar, from, length);
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
        return Exception { INVALID_ACCESS_ERR };

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

    return layerTreeFlags;
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
ExceptionOr<String> Internals::layerTreeAsText(Document& document, unsigned short flags) const
{
    if (!document.frame())
        return Exception { INVALID_ACCESS_ERR };

    return document.frame()->layerTreeAsText(toLayerTreeFlags(flags));
}

ExceptionOr<String> Internals::repaintRectsAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return document->frame()->trackedRepaintRectsAsText();
}

ExceptionOr<String> Internals::scrollingStateTreeAsText() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    Page* page = document->page();
    if (!page)
        return String();

    return page->scrollingStateTreeAsText();
}

ExceptionOr<String> Internals::mainThreadScrollingReasons() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    Page* page = document->page();
    if (!page)
        return String();

    return page->synchronousScrollingReasonsAsText();
}

ExceptionOr<RefPtr<ClientRectList>> Internals::nonFastScrollableRects() const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    Page* page = document->page();
    if (!page)
        return nullptr;

    return RefPtr<ClientRectList> { page->nonFastScrollableRects() };
}

ExceptionOr<void> Internals::setElementUsesDisplayListDrawing(Element& element, bool usesDisplayListDrawing)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };

    if (!element.renderer())
        return Exception { INVALID_ACCESS_ERR };

    if (is<HTMLCanvasElement>(element)) {
        downcast<HTMLCanvasElement>(element).setUsesDisplayListDrawing(usesDisplayListDrawing);
        return { };
    }

    if (!element.renderer()->hasLayer())
        return Exception { INVALID_ACCESS_ERR };
    
    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { INVALID_ACCESS_ERR };
    
    layer->backing()->setUsesDisplayListDrawing(usesDisplayListDrawing);
    return { };
}

ExceptionOr<void> Internals::setElementTracksDisplayListReplay(Element& element, bool isTrackingReplay)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };

    if (!element.renderer())
        return Exception { INVALID_ACCESS_ERR };

    if (is<HTMLCanvasElement>(element)) {
        downcast<HTMLCanvasElement>(element).setTracksDisplayListReplay(isTrackingReplay);
        return { };
    }

    if (!element.renderer()->hasLayer())
        return Exception { INVALID_ACCESS_ERR };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { INVALID_ACCESS_ERR };
    
    layer->backing()->setIsTrackingDisplayListReplay(isTrackingReplay);
    return { };
}

ExceptionOr<String> Internals::displayListForElement(Element& element, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };

    if (!element.renderer())
        return Exception { INVALID_ACCESS_ERR };

    DisplayList::AsTextFlags displayListFlags = 0;
    if (flags & DISPLAY_LIST_INCLUDES_PLATFORM_OPERATIONS)
        displayListFlags |= DisplayList::AsTextFlag::IncludesPlatformOperations;

    if (is<HTMLCanvasElement>(element))
        return downcast<HTMLCanvasElement>(element).displayListAsText(displayListFlags);

    if (!element.renderer()->hasLayer())
        return Exception { INVALID_ACCESS_ERR };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { INVALID_ACCESS_ERR };

    return layer->backing()->displayListAsText(displayListFlags);
}

ExceptionOr<String> Internals::replayDisplayListForElement(Element& element, unsigned short flags)
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };

    if (!element.renderer())
        return Exception { INVALID_ACCESS_ERR };

    DisplayList::AsTextFlags displayListFlags = 0;
    if (flags & DISPLAY_LIST_INCLUDES_PLATFORM_OPERATIONS)
        displayListFlags |= DisplayList::AsTextFlag::IncludesPlatformOperations;

    if (is<HTMLCanvasElement>(element))
        return downcast<HTMLCanvasElement>(element).replayDisplayListAsText(displayListFlags);

    if (!element.renderer()->hasLayer())
        return Exception { INVALID_ACCESS_ERR };

    RenderLayer* layer = downcast<RenderLayerModelObject>(element.renderer())->layer();
    if (!layer->isComposited())
        return Exception { INVALID_ACCESS_ERR };

    return layer->backing()->replayDisplayListAsText(displayListFlags);
}

ExceptionOr<void> Internals::garbageCollectDocumentResources() const
{
    Document* document = contextDocument();
    if (!document)
        return Exception { INVALID_ACCESS_ERR };
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
        return Exception { INVALID_ACCESS_ERR };

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
        return Exception { INVALID_ACCESS_ERR };

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
    Vector<String> vector;

    if (!frame())
        return vector;

    auto string = frame()->loader().icon().url().string();
    if (!string.isNull())
        vector.append(string);
    return vector;
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
        return Exception { INVALID_ACCESS_ERR };

    return PrintContext::pageProperty(frame(), propertyName.utf8().data(), pageNumber);
}

ExceptionOr<String> Internals::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    if (!frame())
        return Exception { INVALID_ACCESS_ERR };

    return PrintContext::pageSizeAndMarginsInPixels(frame(), pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft);
}

ExceptionOr<float> Internals::pageScaleFactor() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

    return document->page()->pageScaleFactor();
}

ExceptionOr<void> Internals::setPageScaleFactor(float scaleFactor, int x, int y)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

    document->page()->setPageScaleFactor(scaleFactor, IntPoint(x, y));
    return { };
}

ExceptionOr<void> Internals::setPageZoomFactor(float zoomFactor)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->frame()->setPageZoomFactor(zoomFactor);
    return { };
}

ExceptionOr<void> Internals::setTextZoomFactor(float zoomFactor)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->frame()->setTextZoomFactor(zoomFactor);
    return { };
}

ExceptionOr<void> Internals::setUseFixedLayout(bool useFixedLayout)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setUseFixedLayout(useFixedLayout);
    return { };
}

ExceptionOr<void> Internals::setFixedLayoutSize(int width, int height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setFixedLayoutSize(IntSize(width, height));
    return { };
}

ExceptionOr<void> Internals::setViewExposedRect(float x, float y, float width, float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setViewExposedRect(FloatRect(x, y, width, height));
    return { };
}

void Internals::setHeaderHeight(float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;

    document->view()->setHeaderHeight(height);
}

void Internals::setFooterHeight(float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;

    document->view()->setFooterHeight(height);
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
    document->webkitWillEnterFullScreenForElement(&element);
}

void Internals::webkitDidEnterFullScreenForElement(Element& element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitDidEnterFullScreenForElement(&element);
}

void Internals::webkitWillExitFullScreenForElement(Element& element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitWillExitFullScreenForElement(&element);
}

void Internals::webkitDidExitFullScreenForElement(Element& element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitDidExitFullScreenForElement(&element);
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
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(scheme);
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
    return FormController::getReferencedFilePaths(frame()->loader().history().currentItem()->documentState());
}

ExceptionOr<void> Internals::startTrackingRepaints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setTracksRepaints(true);
    return { };
}

ExceptionOr<void> Internals::stopTrackingRepaints()
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return Exception { INVALID_ACCESS_ERR };

    document->view()->setTracksRepaints(false);
    return { };
}

ExceptionOr<void> Internals::startTrackingLayerFlushes()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };

    document->renderView()->compositor().startTrackingLayerFlushes();
    return { };
}

ExceptionOr<unsigned> Internals::layerFlushCount()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };

    return document->renderView()->compositor().layerFlushCount();
}

ExceptionOr<void> Internals::startTrackingStyleRecalcs()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { INVALID_ACCESS_ERR };

    document->startTrackingStyleRecalcs();
    return { };
}

ExceptionOr<unsigned> Internals::styleRecalcCount()
{
    Document* document = contextDocument();
    if (!document)
        return Exception { INVALID_ACCESS_ERR };
    
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
        return Exception { INVALID_ACCESS_ERR };

    document->renderView()->compositor().startTrackingCompositingUpdates();
    return { };
}

ExceptionOr<unsigned> Internals::compositingUpdateCount()
{
    Document* document = contextDocument();
    if (!document || !document->renderView())
        return Exception { INVALID_ACCESS_ERR };
    
    return document->renderView()->compositor().compositingUpdateCount();
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
    return document->view()->layoutCount();
}

#if !PLATFORM(IOS)
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
        return Exception { INVALID_ACCESS_ERR };

#if !PLATFORM(IOS)
    Cursor cursor = document->frame()->eventHandler().currentMouseCursor();

    StringBuilder result;
    result.appendLiteral("type=");
    result.append(cursorTypeToString(cursor.type()));
    result.appendLiteral(" hotSpot=");
    result.appendNumber(cursor.hotSpot().x());
    result.append(',');
    result.appendNumber(cursor.hotSpot().y());
    if (cursor.image()) {
        FloatSize size = cursor.image()->size();
        result.appendLiteral(" image=");
        result.appendNumber(size.width());
        result.append('x');
        result.appendNumber(size.height());
    }
#if ENABLE(MOUSE_CURSOR_SCALE)
    if (cursor.imageScaleFactor() != 1) {
        result.appendLiteral(" scale=");
        NumberToStringBuffer buffer;
        result.append(numberToFixedPrecisionString(cursor.imageScaleFactor(), 8, buffer, true));
    }
#endif
    return result.toString();
#else
    return String { "FAIL: Cursor details not available on this platform." };
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
    auto& state = *contextDocument()->vm().topCallFrame;
    return !value.isObject() || &worldForDOMObject(asObject(value)) == &currentWorld(&state);
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::Settings::setUsesOverlayScrollbars(enabled);
}

void Internals::setUsesMockScrollAnimator(bool enabled)
{
    WebCore::Settings::setUsesMockScrollAnimator(enabled);
}

void Internals::forceReload(bool endToEnd)
{
    frame()->loader().reload(endToEnd);
}

void Internals::enableAutoSizeMode(bool enabled, int minimumWidth, int minimumHeight, int maximumWidth, int maximumHeight)
{
    auto* document = contextDocument();
    if (!document || !document->view())
        return;
    document->view()->enableAutoSizeMode(enabled, IntSize(minimumWidth, minimumHeight), IntSize(maximumWidth, maximumHeight));
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

void Internals::initializeMockCDM()
{
    CDM::registerCDMFactory([] (CDM* cdm) { return std::make_unique<MockCDM>(cdm); },
        MockCDM::supportsKeySystem, MockCDM::supportsKeySystemAndMimeType);
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

    return Exception { SYNTAX_ERR };
}

#endif

bool Internals::isSelectPopupVisible(HTMLSelectElement& element)
{
    auto* renderer = element.renderer();
    ASSERT(renderer);
    if (!is<RenderMenuList>(*renderer))
        return false;

#if !PLATFORM(IOS)
    return downcast<RenderMenuList>(*renderer).popupIsVisible();
#else
    return false;
#endif
}

ExceptionOr<String> Internals::captionsStyleSheetOverride()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Exception { INVALID_ACCESS_ERR };

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
        return Exception { INVALID_ACCESS_ERR };

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
        return Exception { INVALID_ACCESS_ERR };

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
        return Exception { INVALID_ACCESS_ERR };
    
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
        return Exception { SYNTAX_ERR };
#else
    UNUSED_PARAM(mode);
#endif
    return { };
}

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

ExceptionOr<Ref<ClientRect>> Internals::selectionBounds()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return ClientRect::create(document->frame()->selection().selectionBounds());
}

#if ENABLE(VIBRATION)

bool Internals::isVibrating()
{
    auto* document = contextDocument();
    auto* page = document ? document->page() : nullptr;
    return page && Vibration::from(page)->isVibrating();
}

#endif

ExceptionOr<bool> Internals::isPluginUnavailabilityIndicatorObscured(Element& element)
{
    auto* renderer = element.renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return Exception { INVALID_ACCESS_ERR };

    return downcast<RenderEmbeddedObject>(*renderer).isReplacementObscured();
}
    
bool Internals::isPluginSnapshotted(Element& element)
{
    return is<HTMLPlugInElement>(element) && downcast<HTMLPlugInElement>(element).displayState() <= HTMLPlugInElement::DisplayingSnapshot;
}
    
#if ENABLE(MEDIA_SOURCE)

void Internals::initializeMockMediaSource()
{
#if USE(AVFOUNDATION)
    WebCore::Settings::setAVFoundationEnabled(false);
#endif
#if USE(GSTREAMER)
    WebCore::Settings::setGStreamerEnabled(false);
#endif
    MediaPlayerFactorySupport::callRegisterMediaEngine(MockMediaPlayerMediaSource::registerMediaEngine);
}

Vector<String> Internals::bufferedSamplesForTrackID(SourceBuffer& buffer, const AtomicString& trackID)
{
    return buffer.bufferedSamplesForTrackID(trackID);
}
    
Vector<String> Internals::enqueuedSamplesForTrackID(SourceBuffer& buffer, const AtomicString& trackID)
{
    return buffer.enqueuedSamplesForTrackID(trackID);
}

void Internals::setShouldGenerateTimestamps(SourceBuffer& buffer, bool flag)
{
    buffer.setShouldGenerateTimestamps(flag);
}

#endif

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
        return Exception { INVALID_ACCESS_ERR };

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

void Internals::applicationDidEnterForeground() const
{
    PlatformMediaSessionManager::sharedManager().applicationDidEnterForeground();
}

void Internals::applicationWillEnterBackground() const
{
    PlatformMediaSessionManager::sharedManager().applicationWillEnterBackground();
}

static PlatformMediaSession::MediaType mediaTypeFromString(const String& mediaTypeString)
{
    if (equalLettersIgnoringASCIICase(mediaTypeString, "video"))
        return PlatformMediaSession::Video;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "audio"))
        return PlatformMediaSession::Audio;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "videoaudio"))
        return PlatformMediaSession::VideoAudio;
    if (equalLettersIgnoringASCIICase(mediaTypeString, "webaudio"))
        return PlatformMediaSession::WebAudio;

    return PlatformMediaSession::None;
}

ExceptionOr<void> Internals::setMediaSessionRestrictions(const String& mediaTypeString, const String& restrictionsString)
{
    PlatformMediaSession::MediaType mediaType = mediaTypeFromString(mediaTypeString);
    if (mediaType == PlatformMediaSession::None)
        return Exception { INVALID_ACCESS_ERR };

    PlatformMediaSessionManager::SessionRestrictions restrictions = PlatformMediaSessionManager::sharedManager().restrictions(mediaType);
    PlatformMediaSessionManager::sharedManager().removeRestriction(mediaType, restrictions);

    restrictions = PlatformMediaSessionManager::NoRestrictions;

    Vector<String> restrictionsArray;
    restrictionsString.split(',', false, restrictionsArray);
    for (auto& restrictionString : restrictionsArray) {
        if (equalLettersIgnoringASCIICase(restrictionString, "concurrentplaybacknotpermitted"))
            restrictions |= PlatformMediaSessionManager::ConcurrentPlaybackNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "backgroundprocessplaybackrestricted"))
            restrictions |= PlatformMediaSessionManager::BackgroundProcessPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "backgroundtabplaybackrestricted"))
            restrictions |= PlatformMediaSessionManager::BackgroundTabPlaybackRestricted;
        if (equalLettersIgnoringASCIICase(restrictionString, "interruptedplaybacknotpermitted"))
            restrictions |= PlatformMediaSessionManager::InterruptedPlaybackNotPermitted;
    }
    PlatformMediaSessionManager::sharedManager().addRestriction(mediaType, restrictions);
    return { };
}

ExceptionOr<String> Internals::mediaSessionRestrictions(const String& mediaTypeString) const
{
    PlatformMediaSession::MediaType mediaType = mediaTypeFromString(mediaTypeString);
    if (mediaType == PlatformMediaSession::None)
        return Exception { INVALID_ACCESS_ERR };

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

void Internals::setMediaElementRestrictions(HTMLMediaElement& element, const String& restrictionsString)
{
    MediaElementSession::BehaviorRestrictions restrictions = element.mediaSession().behaviorRestrictions();
    element.mediaSession().removeBehaviorRestriction(restrictions);

    restrictions = MediaElementSession::NoRestrictions;

    Vector<String> restrictionsArray;
    restrictionsString.split(',', false, restrictionsArray);
    for (auto& restrictionString : restrictionsArray) {
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
        if (equalLettersIgnoringASCIICase(restrictionString, "metadatapreloadingnotpermitted"))
            restrictions |= MediaElementSession::MetadataPreloadingNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "autopreloadingnotpermitted"))
            restrictions |= MediaElementSession::AutoPreloadingNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "invisibleautoplaynotpermitted"))
            restrictions |= MediaElementSession::InvisibleAutoplayNotPermitted;
        if (equalLettersIgnoringASCIICase(restrictionString, "overrideusergesturerequirementformaincontent"))
            restrictions |= MediaElementSession::OverrideUserGestureRequirementForMainContent;
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
        return Exception { INVALID_ACCESS_ERR };
    
    PlatformMediaSessionManager::sharedManager().didReceiveRemoteControlCommand(command, &parameter);
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

void Internals::setAudioContextRestrictions(AudioContext& context, const String& restrictionsString)
{
    AudioContext::BehaviorRestrictions restrictions = context.behaviorRestrictions();
    context.removeBehaviorRestriction(restrictions);

    restrictions = AudioContext::NoRestrictions;

    Vector<String> restrictionsArray;
    restrictionsString.split(',', false, restrictionsArray);
    for (auto& restrictionString : restrictionsArray) {
        if (equalLettersIgnoringASCIICase(restrictionString, "norestrictions"))
            restrictions |= AudioContext::NoRestrictions;
        if (equalLettersIgnoringASCIICase(restrictionString, "requireusergestureforaudiostart"))
            restrictions |= AudioContext::RequireUserGestureForAudioStartRestriction;
        if (equalLettersIgnoringASCIICase(restrictionString, "requirepageconsentforaudiostart"))
            restrictions |= AudioContext::RequirePageConsentForAudioStartRestriction;
    }
    context.addBehaviorRestriction(restrictions);
}

#endif

void Internals::simulateSystemSleep() const
{
#if ENABLE(VIDEO)
    PlatformMediaSessionManager::sharedManager().systemWillSleep();
#endif
}

void Internals::simulateSystemWake() const
{
#if ENABLE(VIDEO)
    PlatformMediaSessionManager::sharedManager().systemDidWake();
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
        return Exception { INVALID_ACCESS_ERR };

    page->setMockMediaPlaybackTargetPickerState(deviceName, state);
    return { };
}

#endif

ExceptionOr<Ref<MockPageOverlay>> Internals::installMockPageOverlay(PageOverlayType type)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    return MockPageOverlayClient::singleton().installOverlay(document->frame()->mainFrame(), type == PageOverlayType::View ? PageOverlay::OverlayType::View : PageOverlay::OverlayType::Document);
}

ExceptionOr<String> Internals::pageOverlayLayerTreeAsText(unsigned short flags) const
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return Exception { INVALID_ACCESS_ERR };

    document->updateLayout();

    return MockPageOverlayClient::singleton().layerTreeAsText(document->frame()->mainFrame(), toLayerTreeFlags(flags));
}

void Internals::setPageMuted(const String& states)
{
    Document* document = contextDocument();
    if (!document)
        return;

    WebCore::MediaProducer::MutedStateFlags state = MediaProducer::NoneMuted;
    Vector<String> stateString;
    states.split(',', false, stateString);
    for (auto& muteString : stateString) {
        if (equalLettersIgnoringASCIICase(muteString, "audio"))
            state |= MediaProducer::AudioIsMuted;
        if (equalLettersIgnoringASCIICase(muteString, "capturedevices"))
            state |= MediaProducer::CaptureDevicesAreMuted;
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

    auto microtask = std::make_unique<ActiveDOMCallbackMicrotask>(MicrotaskQueue::mainThreadQueue(), *document, [document, testNumber]() {
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Debug, makeString("MicroTask #", String::number(testNumber), " has run."));
    });

    MicrotaskQueue::mainThreadQueue().append(WTFMove(microtask));
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
        
        builder.append(String::number(coordinate.toUnsigned()));
    }
    builder.appendLiteral(" }");
}
    
void Internals::setPlatformMomentumScrollingPredictionEnabled(bool enabled)
{
    ScrollingMomentumCalculator::setPlatformMomentumScrollingPredictionEnabled(enabled);
}

ExceptionOr<String> Internals::scrollSnapOffsets(Element& element)
{
    if (!element.renderBox())
        return String();

    element.document().updateLayout();

    RenderBox& box = *element.renderBox();
    ScrollableArea* scrollableArea;
    
    if (box.isBody()) {
        FrameView* frameView = box.frame().mainFrame().view();
        if (!frameView || !frameView->isScrollable())
            return Exception { INVALID_ACCESS_ERR };
        scrollableArea = frameView;
        
    } else {
        if (!box.canBeScrolledAndHasScrollableArea())
            return Exception { INVALID_ACCESS_ERR };
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
        return Exception { INVALID_ACCESS_ERR };

    Vector<FloatRect> rects;
    for (unsigned i = 0; i < rectComponents.size(); i += 4)
        rects.append(FloatRect(rectComponents[i], rectComponents[i + 1], rectComponents[i + 2], rectComponents[i + 3]));

    SVGPathStringBuilder builder;
    PathUtilities::pathWithShrinkWrappedRects(rects, radius).apply([&builder](const PathElement& element) {
        switch (element.type) {
        case PathElementMoveToPoint:
            builder.moveTo(element.points[0], false, AbsoluteCoordinates);
            return;
        case PathElementAddLineToPoint:
            builder.lineTo(element.points[0], AbsoluteCoordinates);
            return;
        case PathElementAddQuadCurveToPoint:
            builder.curveToQuadratic(element.points[0], element.points[1], AbsoluteCoordinates);
            return;
        case PathElementAddCurveToPoint:
            builder.curveToCubic(element.points[0], element.points[1], element.points[2], AbsoluteCoordinates);
            return;
        case PathElementCloseSubpath:
            builder.closePath();
            return;
        }
        ASSERT_NOT_REACHED();
    });
    return builder.result();
}


String Internals::getCurrentMediaControlsStatusForElement(HTMLMediaElement& mediaElement)
{
#if !ENABLE(MEDIA_CONTROLS_SCRIPT)
    UNUSED_PARAM(mediaElement);
    return String();
#else
    return mediaElement.getCurrentMediaControlsStatus();
#endif
}

#if !PLATFORM(COCOA)

String Internals::userVisibleString(const DOMURL&)
{
    // Cocoa-specific function. Could ASSERT_NOT_REACHED, but that's probably overkill.
    return String();
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

#if ENABLE(READABLE_STREAM_API)

bool Internals::isReadableStreamDisturbed(JSC::ExecState& state, JSValue stream)
{
    JSGlobalObject* globalObject = state.vmEntryGlobalObject();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(state.vm().clientData);
    const Identifier& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().isReadableStreamDisturbedPrivateName();
    JSValue value;
    PropertySlot propertySlot(value, PropertySlot::InternalMethodType::Get);
    globalObject->methodTable()->getOwnPropertySlot(globalObject, &state, privateName, propertySlot);
    value = propertySlot.getValue(&state, privateName);
    ASSERT(value.isFunction());

    JSObject* function = value.getObject();
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != JSC::CallType::None);
    MarkedArgumentBuffer arguments;
    arguments.append(stream);
    JSValue returnedValue = JSC::call(&state, function, callType, callData, JSC::jsUndefined(), arguments);
    ASSERT(returnedValue.isBoolean());

    return returnedValue.asBoolean();
}

#endif

String Internals::resourceLoadStatisticsForOrigin(const String& origin)
{
    return ResourceLoadObserver::sharedObserver().statisticsForOrigin(origin);
}

void Internals::setResourceLoadStatisticsEnabled(bool enable)
{
    Settings::setResourceLoadStatisticsEnabled(enable);
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
                frame->editor().setBaseWritingDirection(LeftToRightWritingDirection);
                break;
            case BaseWritingDirection::Rtl:
                frame->editor().setBaseWritingDirection(RightToLeftWritingDirection);
                break;
            case BaseWritingDirection::Natural:
                frame->editor().setBaseWritingDirection(NaturalWritingDirection);
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

Vector<String> Internals::accessKeyModifiers() const
{
    Vector<String> accessKeyModifierStrings;

    for (auto modifier : EventHandler::accessKeyModifiers()) {
        switch (modifier) {
        case PlatformEvent::Modifier::AltKey:
            accessKeyModifierStrings.append(ASCIILiteral("altKey"));
            break;
        case PlatformEvent::Modifier::CtrlKey:
            accessKeyModifierStrings.append(ASCIILiteral("ctrlKey"));
            break;
        case PlatformEvent::Modifier::MetaKey:
            accessKeyModifierStrings.append(ASCIILiteral("metaKey"));
            break;
        case PlatformEvent::Modifier::ShiftKey:
            accessKeyModifierStrings.append(ASCIILiteral("shiftKey"));
            break;
        case PlatformEvent::Modifier::CapsLockKey:
            accessKeyModifierStrings.append(ASCIILiteral("capsLockKey"));
            break;
        }
    }

    return accessKeyModifierStrings;
}

#if PLATFORM(IOS)
void Internals::setQuickLookPassword(const String& password)
{
#if USE(QUICK_LOOK)
    auto& quickLookHandleClient = MockQuickLookHandleClient::singleton();
    QuickLookHandle::setClientForTesting(&quickLookHandleClient);
    quickLookHandleClient.setPassword(password);
#else
    UNUSED_PARAM(password);
#endif
}
#endif

void Internals::setAsRunningUserScripts(Document& document)
{
    if (document.page())
        document.page()->setAsRunningUserScripts();
}

} // namespace WebCore
