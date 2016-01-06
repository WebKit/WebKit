/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "AnimationController.h"
#include "ApplicationCacheStorage.h"
#include "BackForwardController.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "Cursor.h"
#include "DOMPath.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
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
#include "MainFrame.h"
#include "MallocStatistics.h"
#include "MediaPlayer.h"
#include "MemoryCache.h"
#include "MemoryInfo.h"
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
#include "RenderLayerCompositor.h"
#include "RenderMenuList.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "ScriptedAnimationController.h"
#include "ScrollingCoordinator.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceBuffer.h"
#include "SpellChecker.h"
#include "StaticNodeList.h"
#include "StyleSheetContents.h"
#include "TextIterator.h"
#include "TreeScope.h"
#include "TypeConversions.h"
#include "UserMediaController.h"
#include "ViewportArguments.h"
#include "WebCoreJSClientData.h"
#include "WorkerThread.h"
#include "XMLHttpRequest.h"
#include <JavaScriptCore/Profile.h>
#include <bytecode/CodeBlock.h>
#include <inspector/InspectorAgentBase.h>
#include <inspector/InspectorFrontendChannel.h>
#include <inspector/InspectorValues.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSCJSValue.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(BATTERY_STATUS)
#include "BatteryController.h"
#endif

#if ENABLE(PROXIMITY_EVENTS)
#include "DeviceProximityController.h"
#endif

#if ENABLE(MOUSE_CURSOR_SCALE)
#include <wtf/dtoa.h>
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
#include "CDM.h"
#include "MockCDM.h"
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
#include "RTCPeerConnection.h"
#include "RTCPeerConnectionHandlerMock.h"
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

class InspectorStubFrontend : public InspectorFrontendClientLocal, public FrontendChannel {
public:
    InspectorStubFrontend(Page* inspectedPage, RefPtr<DOMWindow>&& frontendWindow);
    virtual ~InspectorStubFrontend();

    // InspectorFrontendClient API
    virtual void attachWindow(DockSide) override { }
    virtual void detachWindow() override { }
    virtual void closeWindow() override;
    virtual void bringToFront() override { }
    virtual String localizedStringsURL() override { return String(); }
    virtual void inspectedURLChanged(const String&) override { }
protected:
    virtual void setAttachedWindowHeight(unsigned) override { }
    virtual void setAttachedWindowWidth(unsigned) override { }

public:
    // Inspector::FrontendChannel API
    virtual bool sendMessageToFrontend(const String& message) override;
    virtual ConnectionType connectionType() const override { return ConnectionType::Local; }

private:
    Page* frontendPage() const
    {
        if (!m_frontendWindow || !m_frontendWindow->document())
            return nullptr;

        return m_frontendWindow->document()->page();
    }

    RefPtr<DOMWindow> m_frontendWindow;
    InspectorController& m_frontendController;
};

InspectorStubFrontend::InspectorStubFrontend(Page* inspectedPage, RefPtr<DOMWindow>&& frontendWindow)
    : InspectorFrontendClientLocal(&inspectedPage->inspectorController(), frontendWindow->document()->page(), std::make_unique<InspectorFrontendClientLocal::Settings>())
    , m_frontendWindow(frontendWindow.copyRef())
    , m_frontendController(frontendPage()->inspectorController())
{
    ASSERT_ARG(inspectedPage, inspectedPage);
    ASSERT_ARG(frontendWindow, frontendWindow);

    m_frontendController.setInspectorFrontendClient(this);
    inspectedPage->inspectorController().connectFrontend(this);
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

    m_frontendWindow->close(m_frontendWindow->scriptExecutionContext());
    m_frontendWindow = nullptr;
}

bool InspectorStubFrontend::sendMessageToFrontend(const String& message)
{
    ASSERT_ARG(message, !message.isEmpty());

    return InspectorClient::doDispatchMessageOnFrontendPage(frontendPage(), message);
}

static bool markerTypeFrom(const String& markerType, DocumentMarker::MarkerType& result)
{
    if (equalIgnoringCase(markerType, "Spelling"))
        result = DocumentMarker::Spelling;
    else if (equalIgnoringCase(markerType, "Grammar"))
        result = DocumentMarker::Grammar;
    else if (equalIgnoringCase(markerType, "TextMatch"))
        result = DocumentMarker::TextMatch;
    else if (equalIgnoringCase(markerType, "Replacement"))
        result = DocumentMarker::Replacement;
    else if (equalIgnoringCase(markerType, "CorrectionIndicator"))
        result = DocumentMarker::CorrectionIndicator;
    else if (equalIgnoringCase(markerType, "RejectedCorrection"))
        result = DocumentMarker::RejectedCorrection;
    else if (equalIgnoringCase(markerType, "Autocorrected"))
        result = DocumentMarker::Autocorrected;
    else if (equalIgnoringCase(markerType, "SpellCheckingExemption"))
        result = DocumentMarker::SpellCheckingExemption;
    else if (equalIgnoringCase(markerType, "DeletedAutocorrection"))
        result = DocumentMarker::DeletedAutocorrection;
    else if (equalIgnoringCase(markerType, "DictationAlternatives"))
        result = DocumentMarker::DictationAlternatives;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    else if (equalIgnoringCase(markerType, "TelephoneNumber"))
        result = DocumentMarker::TelephoneNumber;
#endif
    else
        return false;
    
    return true;
}

static bool markerTypesFrom(const String& markerType, DocumentMarker::MarkerTypes& result)
{
    DocumentMarker::MarkerType singularResult;

    if (markerType.isEmpty() || equalIgnoringCase(markerType, "all"))
        result = DocumentMarker::AllMarkers();
    else if (markerTypeFrom(markerType, singularResult))
        result = singularResult;
    else
        return false;

    return true;
}

const char* Internals::internalsId = "internals";

Ref<Internals> Internals::create(Document* document)
{
    return adoptRef(*new Internals(document));
}

Internals::~Internals()
{
}

void Internals::resetToConsistentState(Page* page)
{
    ASSERT(page);

    page->setPageScaleFactor(1, IntPoint(0, 0));
    page->setPagination(Pagination());

    page->setDefersLoading(false);
    
    page->mainFrame().setTextZoomFactor(1.0f);
    
    FrameView* mainFrameView = page->mainFrame().view();
    if (mainFrameView) {
        mainFrameView->setHeaderHeight(0);
        mainFrameView->setFooterHeight(0);
        page->setTopContentInset(0);
        mainFrameView->setUseFixedLayout(false);
        mainFrameView->setFixedLayoutSize(IntSize());
    }

    TextRun::setAllowsRoundingHacks(false);
    WebCore::overrideUserPreferredLanguages(Vector<String>());
    WebCore::Settings::setUsesOverlayScrollbars(false);
    page->inspectorController().setProfilerEnabled(false);
#if ENABLE(VIDEO_TRACK)
    page->group().captionPreferences()->setCaptionsStyleSheetOverride(emptyString());
    page->group().captionPreferences()->setTestingMode(false);
#endif
    if (!page->mainFrame().editor().isContinuousSpellCheckingEnabled())
        page->mainFrame().editor().toggleContinuousSpellChecking();
    if (page->mainFrame().editor().isOverwriteModeEnabled())
        page->mainFrame().editor().toggleOverwriteModeEnabled();
    page->mainFrame().loader().clearTestingOverrides();
    ApplicationCacheStorage::singleton().setDefaultOriginQuota(ApplicationCacheStorage::noQuota());
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

    page->setShowAllPlugins(false);
}

Internals::Internals(Document* document)
    : ContextDestructionObserver(document)
{
#if ENABLE(VIDEO_TRACK)
    if (document && document->page())
        document->page()->group().captionPreferences()->setTestingMode(true);
#endif

#if ENABLE(MEDIA_STREAM)
    setMockMediaCaptureDevicesEnabled(true);
    enableMockRTCPeerConnectionHandler();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (document && document->page())
        document->page()->setMockMediaPlaybackTargetPickerEnabled(true);
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
        return 0;
    Page* page = document->page();
    if (!page)
        return 0;
    return InternalSettings::from(page);
}

unsigned Internals::workerThreadCount() const
{
    return WorkerThread::workerThreadCount();
}

String Internals::address(Node* node)
{
    return String::format("%p", node);
}

bool Internals::nodeNeedsStyleRecalc(Node* node, ExceptionCode& exception)
{
    if (!node) {
        exception = INVALID_ACCESS_ERR;
        return false;
    }

    return node->needsStyleRecalc();
}

String Internals::description(Deprecated::ScriptValue value)
{
    return toString(value.jsValue());
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
#if ENABLE(CACHE_PARTITIONING)
    request.setDomainForCachePartition(contextDocument()->topOrigin()->domainForCachePartition());
#endif
    CachedResource* resource = MemoryCache::singleton().resourceForRequest(request, contextDocument()->page()->sessionID());
    return resource && resource->status() == CachedResource::Cached;
}

String Internals::xhrResponseSource(XMLHttpRequest* xhr)
{
    if (!xhr)
        return "Null xhr";
    if (xhr->resourceResponse().isNull())
        return "Null response";
    switch (xhr->resourceResponse().source()) {
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

bool Internals::isSharingStyleSheetContents(Element* a, Element* b)
{
    if (!is<HTMLLinkElement>(a) || !is<HTMLLinkElement>(b))
        return false;
    auto& aLink = downcast<HTMLLinkElement>(*a);
    auto& bLink = downcast<HTMLLinkElement>(*b);
    if (!aLink.sheet() || !bLink.sheet())
        return false;
    return &aLink.sheet()->contents() == &bLink.sheet()->contents();
}

bool Internals::isStyleSheetLoadingSubresources(Element* link)
{
    if (!is<HTMLLinkElement>(link))
        return false;
    auto& linkElement = downcast<HTMLLinkElement>(*link);
    return linkElement.sheet() && linkElement.sheet()->contents().isLoadingSubresources();
}

static ResourceRequestCachePolicy stringToResourceRequestCachePolicy(const String& policy)
{
    if (policy == "UseProtocolCachePolicy")
        return UseProtocolCachePolicy;
    if (policy == "ReloadIgnoringCacheData")
        return ReloadIgnoringCacheData;
    if (policy == "ReturnCacheDataElseLoad")
        return ReturnCacheDataElseLoad;
    if (policy == "ReturnCacheDataDontLoad")
        return ReturnCacheDataDontLoad;
    ASSERT_NOT_REACHED();
    return UseProtocolCachePolicy;
}

void Internals::setOverrideCachePolicy(const String& policy)
{
    frame()->loader().setOverrideCachePolicyForTesting(stringToResourceRequestCachePolicy(policy));
}

static ResourceLoadPriority stringToResourceLoadPriority(const String& policy)
{
    if (policy == "ResourceLoadPriorityVeryLow")
        return ResourceLoadPriority::VeryLow;
    if (policy == "ResourceLoadPriorityLow")
        return ResourceLoadPriority::Low;
    if (policy == "ResourceLoadPriorityMedium")
        return ResourceLoadPriority::Medium;
    if (policy == "ResourceLoadPriorityHigh")
        return ResourceLoadPriority::High;
    if (policy == "ResourceLoadPriorityVeryHigh")
        return ResourceLoadPriority::VeryHigh;
    ASSERT_NOT_REACHED();
    return ResourceLoadPriority::Low;
}

void Internals::setOverrideResourceLoadPriority(const String& priority)
{
    frame()->loader().setOverrideResourceLoadPriorityForTesting(stringToResourceLoadPriority(priority));
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

void Internals::clearPageCache()
{
    PageCache::singleton().pruneToSizeNow(0, PruningReason::None);
}

unsigned Internals::pageCacheSize() const
{
    return PageCache::singleton().pageCount();
}

Node* Internals::treeScopeRootNode(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    return &node->treeScope().rootNode();
}

Node* Internals::parentTreeScope(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }
    const TreeScope* parentTreeScope = node->treeScope().parentTreeScope();
    return parentTreeScope ? &parentTreeScope->rootNode() : nullptr;
}

unsigned Internals::lastSpatialNavigationCandidateCount(ExceptionCode& ec) const
{
    if (!contextDocument() || !contextDocument()->page()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return contextDocument()->page()->lastSpatialNavigationCandidateCount();
}

unsigned Internals::numberOfActiveAnimations() const
{
    return frame()->animation().numberOfActiveAnimations(frame()->document());
}

bool Internals::animationsAreSuspended(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return document->frame()->animation().isSuspended();
}

void Internals::suspendAnimations(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->frame()->animation().suspendAnimations();
}

void Internals::resumeAnimations(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->frame()->animation().resumeAnimations();
}

bool Internals::pauseAnimationAtTimeOnElement(const String& animationName, double pauseTime, Element* element, ExceptionCode& ec)
{
    if (!element || pauseTime < 0) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }
    return frame()->animation().pauseAnimationAtTime(element->renderer(), AtomicString(animationName), pauseTime);
}

bool Internals::pauseAnimationAtTimeOnPseudoElement(const String& animationName, double pauseTime, Element* element, const String& pseudoId, ExceptionCode& ec)
{
    if (!element || pauseTime < 0) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    if (pseudoId != "before" && pseudoId != "after") {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    PseudoElement* pseudoElement = pseudoId == "before" ? element->beforePseudoElement() : element->afterPseudoElement();
    if (!pseudoElement) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return frame()->animation().pauseAnimationAtTime(pseudoElement->renderer(), AtomicString(animationName), pauseTime);
}

bool Internals::pauseTransitionAtTimeOnElement(const String& propertyName, double pauseTime, Element* element, ExceptionCode& ec)
{
    if (!element || pauseTime < 0) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }
    return frame()->animation().pauseTransitionAtTime(element->renderer(), propertyName, pauseTime);
}

bool Internals::pauseTransitionAtTimeOnPseudoElement(const String& property, double pauseTime, Element* element, const String& pseudoId, ExceptionCode& ec)
{
    if (!element || pauseTime < 0) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    if (pseudoId != "before" && pseudoId != "after") {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    PseudoElement* pseudoElement = pseudoId == "before" ? element->beforePseudoElement() : element->afterPseudoElement();
    if (!pseudoElement) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return frame()->animation().pauseTransitionAtTime(pseudoElement->renderer(), property, pauseTime);
}

// FIXME: Remove.
bool Internals::attached(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return true;
}

String Internals::elementRenderTreeAsText(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    element->document().updateStyleIfNeeded();

    String representation = externalRepresentation(element);
    if (representation.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return representation;
}

bool Internals::hasPausedImageAnimations(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }
    return element->renderer() && element->renderer()->hasPausedImageAnimations();
}

RefPtr<CSSComputedStyleDeclaration> Internals::computedStyleIncludingVisitedInfo(Node* node, ExceptionCode& ec) const
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    bool allowVisitedStyle = true;
    return CSSComputedStyleDeclaration::create(node, allowVisitedStyle);
}

Node* Internals::ensureShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    if (ShadowRoot* shadowRoot = host->shadowRoot())
        return shadowRoot;

    return host->createShadowRoot(ec).get();
}

Node* Internals::ensureUserAgentShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }
    return &host->ensureUserAgentShadowRoot();
}

Node* Internals::createShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    return host->createShadowRoot(ec).get();
}

Node* Internals::shadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }
    return host->shadowRoot();
}

String Internals::shadowRootType(const Node* root, ExceptionCode& ec) const
{
    if (!is<ShadowRoot>(root)) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    switch (downcast<ShadowRoot>(*root).type()) {
    case ShadowRoot::Type::UserAgent:
        return String("UserAgentShadowRoot");
    case ShadowRoot::Type::Closed:
        return String("ClosedShadowRoot");
    case ShadowRoot::Type::Open:
        return String("OpenShadowRoot");
    default:
        ASSERT_NOT_REACHED();
        return String("Unknown");
    }
}

Element* Internals::includerFor(Node*, ExceptionCode& ec)
{
    ec = INVALID_ACCESS_ERR;
    return nullptr;
}

String Internals::shadowPseudoId(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return element->shadowPseudoId().string();
}

void Internals::setShadowPseudoId(Element* element, const String& id, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    return element->setPseudo(id);
}

bool Internals::isTimerThrottled(int timeoutId, ExceptionCode& ec)
{
    DOMTimer* timer = scriptExecutionContext()->findTimeout(timeoutId);
    if (!timer) {
        ec = NOT_FOUND_ERR;
        return false;
    }
    return timer->m_throttleState == DOMTimer::ShouldThrottle;
}

bool Internals::isRequestAnimationFrameThrottled() const
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    auto* scriptedAnimationController = contextDocument()->scriptedAnimationController();
    if (!scriptedAnimationController)
        return false;
    return scriptedAnimationController->isThrottled();
#else
    return false;
#endif
}

bool Internals::areTimersThrottled() const
{
    return contextDocument()->isTimerThrottlingEnabled();
}

String Internals::visiblePlaceholder(Element* element)
{
    if (is<HTMLTextFormControlElement>(element)) {
        const HTMLTextFormControlElement& textFormControlElement = downcast<HTMLTextFormControlElement>(*element);
        if (!textFormControlElement.isPlaceholderVisible())
            return String();
        if (HTMLElement* placeholderElement = textFormControlElement.placeholderElement())
            return placeholderElement->textContent();
    }

    return String();
}

void Internals::selectColorInColorChooser(Element* element, const String& colorValue)
{
    if (!is<HTMLInputElement>(element))
        return;
    auto& inputElement = downcast<HTMLInputElement>(*element);
    inputElement.selectColor(Color(colorValue));
}

Vector<String> Internals::formControlStateOfPreviousHistoryItem(ExceptionCode& ec)
{
    HistoryItem* mainItem = frame()->loader().history().previousItem();
    if (!mainItem) {
        ec = INVALID_ACCESS_ERR;
        return Vector<String>();
    }
    String uniqueName = frame()->tree().uniqueName();
    if (mainItem->target() != uniqueName && !mainItem->childItemWithTarget(uniqueName)) {
        ec = INVALID_ACCESS_ERR;
        return Vector<String>();
    }
    return mainItem->target() == uniqueName ? mainItem->documentState() : mainItem->childItemWithTarget(uniqueName)->documentState();
}

void Internals::setFormControlStateOfPreviousHistoryItem(const Vector<String>& state, ExceptionCode& ec)
{
    HistoryItem* mainItem = frame()->loader().history().previousItem();
    if (!mainItem) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    String uniqueName = frame()->tree().uniqueName();
    if (mainItem->target() == uniqueName)
        mainItem->setDocumentState(state);
    else if (HistoryItem* subItem = mainItem->childItemWithTarget(uniqueName))
        subItem->setDocumentState(state);
    else
        ec = INVALID_ACCESS_ERR;
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

#if ENABLE(MEDIA_STREAM)
void Internals::enableMockRTCPeerConnectionHandler()
{
    RTCPeerConnectionHandler::create = RTCPeerConnectionHandlerMock::create;
}

void Internals::setMockMediaCaptureDevicesEnabled(bool enabled)
{
    WebCore::Settings::setMockCaptureDevicesEnabled(enabled);
}
#endif

Ref<ClientRect> Internals::absoluteCaretBounds(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRect::create();
    }

    return ClientRect::create(document->frame()->selection().absoluteCaretBounds());
}

Ref<ClientRect> Internals::boundingBox(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return ClientRect::create();
    }

    element->document().updateLayoutIgnorePendingStylesheets();
    auto renderer = element->renderer();
    if (!renderer)
        return ClientRect::create();
    return ClientRect::create(renderer->absoluteBoundingBoxRectIgnoringTransforms());
}

Ref<ClientRectList> Internals::inspectorHighlightRects(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRectList::create();
    }

    Highlight highlight;
    document->page()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::View);
    return ClientRectList::create(highlight.quads);
}

String Internals::inspectorHighlightObject(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return document->page()->inspectorController().buildObjectForHighlightedNodes()->toJSONString();
}

unsigned Internals::markerCountForNode(Node* node, const String& markerType, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    DocumentMarker::MarkerTypes markerTypes = 0;
    if (!markerTypesFrom(markerType, markerTypes)) {
        ec = SYNTAX_ERR;
        return 0;
    }

    node->document().frame()->editor().updateEditorUINowIfScheduled();

    return node->document().markers().markersFor(node, markerTypes).size();
}

RenderedDocumentMarker* Internals::markerAt(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
{
    node->document().updateLayoutIgnorePendingStylesheets();
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    DocumentMarker::MarkerTypes markerTypes = 0;
    if (!markerTypesFrom(markerType, markerTypes)) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    node->document().frame()->editor().updateEditorUINowIfScheduled();

    Vector<RenderedDocumentMarker*> markers = node->document().markers().markersFor(node, markerTypes);
    if (markers.size() <= index)
        return nullptr;
    return markers[index];
}

RefPtr<Range> Internals::markerRangeForNode(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
{
    RenderedDocumentMarker* marker = markerAt(node, markerType, index, ec);
    if (!marker)
        return nullptr;
    return Range::create(node->document(), node, marker->startOffset(), node, marker->endOffset());
}

String Internals::markerDescriptionForNode(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
{
    RenderedDocumentMarker* marker = markerAt(node, markerType, index, ec);
    if (!marker)
        return String();
    return marker->description();
}

String Internals::dumpMarkerRects(const String& markerTypeString, ExceptionCode& ec)
{
    DocumentMarker::MarkerType markerType;
    if (!markerTypeFrom(markerTypeString, markerType)) {
        ec = SYNTAX_ERR;
        return String();
    }

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

void Internals::addTextMatchMarker(const Range* range, bool isActive)
{
    range->ownerDocument().updateLayoutIgnorePendingStylesheets();
    range->ownerDocument().markers().addTextMatchMarker(range, isActive);
}

void Internals::setMarkedTextMatchesAreHighlighted(bool flag, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    document->frame()->editor().setMarkedTextMatchesAreHighlighted(flag);
}

void Internals::invalidateFontCache()
{
    FontCache::singleton().invalidate();
}

void Internals::setScrollViewPosition(long x, long y, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    bool constrainsScrollingToContentEdgeOldValue = frameView->constrainsScrollingToContentEdge();
    bool scrollbarsSuppressedOldValue = frameView->scrollbarsSuppressed();

    frameView->setConstrainsScrollingToContentEdge(false);
    frameView->setScrollbarsSuppressed(false);
    frameView->setScrollOffsetFromInternals(IntPoint(x, y));
    frameView->setScrollbarsSuppressed(scrollbarsSuppressedOldValue);
    frameView->setConstrainsScrollingToContentEdge(constrainsScrollingToContentEdgeOldValue);
}

void Internals::setViewBaseBackgroundColor(const String& colorValue, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->view()->setBaseBackgroundColor(Color(colorValue));
}

void Internals::setPagination(const String& mode, int gap, int pageLength, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    Page* page = document->page();

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
    else {
        ec = SYNTAX_ERR;
        return;
    }

    pagination.gap = gap;
    pagination.pageLength = pageLength;
    page->setPagination(pagination);
}

String Internals::configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }
    Page* page = document->page();

    const int defaultLayoutWidthForNonMobilePages = 980;

    ViewportArguments arguments = page->viewportArguments();
    ViewportAttributes attributes = computeViewportAttributes(arguments, defaultLayoutWidthForNonMobilePages, deviceWidth, deviceHeight, devicePixelRatio, IntSize(availableWidth, availableHeight));
    restrictMinimumScaleFactorToViewportSize(attributes, IntSize(availableWidth, availableHeight), devicePixelRatio);
    restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    return "viewport size " + String::number(attributes.layoutSize.width()) + "x" + String::number(attributes.layoutSize.height()) + " scale " + String::number(attributes.initialScale) + " with limits [" + String::number(attributes.minimumScale) + ", " + String::number(attributes.maximumScale) + "] and userScalable " + (attributes.userScalable ? "true" : "false");
}

bool Internals::wasLastChangeUserEdit(Element* textField, ExceptionCode& ec)
{
    if (!textField) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    if (is<HTMLInputElement>(*textField))
        return downcast<HTMLInputElement>(*textField).lastChangeWasUserEdit();

    if (is<HTMLTextAreaElement>(*textField))
        return downcast<HTMLTextAreaElement>(*textField).lastChangeWasUserEdit();

    ec = INVALID_NODE_TYPE_ERR;
    return false;
}

bool Internals::elementShouldAutoComplete(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    if (!is<HTMLInputElement>(*element)) {
        ec = INVALID_NODE_TYPE_ERR;
        return false;
    }

    return downcast<HTMLInputElement>(*element).shouldAutocomplete();
}

void Internals::setEditingValue(Element* element, const String& value, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!is<HTMLInputElement>(*element)) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    downcast<HTMLInputElement>(*element).setEditingValue(value);
}

void Internals::setAutofilled(Element* element, bool enabled, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!is<HTMLInputElement>(*element)) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    downcast<HTMLInputElement>(*element).setAutoFilled(enabled);
}

void Internals::setShowAutoFillButton(Element* element, bool show, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!is<HTMLInputElement>(*element)) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    downcast<HTMLInputElement>(*element).setShowAutoFillButton(show);
}


void Internals::scrollElementToRect(Element* element, long x, long y, long w, long h, ExceptionCode& ec)
{
    if (!element || !element->document().view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    FrameView* frameView = element->document().view();
    frameView->scrollElementToRect(*element, IntRect(x, y, w, h));
}

void Internals::paintControlTints(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->paintControlTints();
}

RefPtr<Range> Internals::rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, ExceptionCode& ec)
{
    if (!scope) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    return TextIterator::rangeFromLocationAndLength(scope, rangeLocation, rangeLength);
}

unsigned Internals::locationFromRange(Element* scope, const Range* range, ExceptionCode& ec)
{
    if (!scope || !range) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    size_t location = 0;
    size_t unusedLength = 0;
    TextIterator::getLocationAndLengthFromRange(scope, range, location, unusedLength);
    return location;
}

unsigned Internals::lengthFromRange(Element* scope, const Range* range, ExceptionCode& ec)
{
    if (!scope || !range) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    size_t unusedLocation = 0;
    size_t length = 0;
    TextIterator::getLocationAndLengthFromRange(scope, range, unusedLocation, length);
    return length;
}

String Internals::rangeAsText(const Range* range, ExceptionCode& ec)
{
    if (!range) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return range->text();
}

RefPtr<Range> Internals::subrange(Range* range, int rangeLocation, int rangeLength, ExceptionCode& ec)
{
    if (!range) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    return TextIterator::subrange(range, rangeLocation, rangeLength);
}

RefPtr<Range> Internals::rangeForDictionaryLookupAtLocation(int x, int y, ExceptionCode& ec)
{
#if PLATFORM(MAC)
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    document->updateLayoutIgnorePendingStylesheets();
    
    HitTestResult result = document->frame()->mainFrame().eventHandler().hitTestResultAtPoint(IntPoint(x, y));
    NSDictionary *options = nullptr;
    return DictionaryLookup::rangeAtHitTestResult(result, &options);
#else
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    ec = INVALID_ACCESS_ERR;
    return nullptr;
#endif
}

void Internals::setDelegatesScrolling(bool enabled, ExceptionCode& ec)
{
    Document* document = contextDocument();
    // Delegate scrolling is valid only on mainframe's view.
    if (!document || !document->view() || !document->page() || &document->page()->mainFrame() != document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->view()->setDelegatesScrolling(enabled);
}

int Internals::lastSpellCheckRequestSequence(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }

    return document->frame()->editor().spellChecker().lastRequestSequence();
}

int Internals::lastSpellCheckProcessedSequence(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }

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
    return document->page()->group().captionPreferences()->preferredAudioCharacteristics();
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
    document->page()->group().captionPreferences()->setPreferredAudioCharacteristic(characteristic);
#else
    UNUSED_PARAM(characteristic);
#endif
}

unsigned Internals::wheelEventHandlerCount(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->wheelEventHandlerCount();
}

unsigned Internals::touchEventHandlerCount(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->touchEventHandlerCount();
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
RefPtr<NodeList> Internals::nodesFromRect(Document* document, int centerX, int centerY, unsigned topPadding, unsigned rightPadding,
    unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, bool allowChildFrameContent, ExceptionCode& ec) const
{
    if (!document || !document->frame() || !document->frame()->view()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    Frame* frame = document->frame();
    FrameView* frameView = document->view();
    RenderView* renderView = document->renderView();
    if (!renderView)
        return nullptr;

    document->updateLayoutIgnorePendingStylesheets();

    float zoomFactor = frame->pageZoomFactor();
    LayoutPoint point(centerX * zoomFactor + frameView->scrollX(), centerY * zoomFactor + frameView->scrollY());

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active;
    if (ignoreClipping)
        hitType |= HitTestRequest::IgnoreClipping;
    if (!allowShadowContent)
        hitType |= HitTestRequest::DisallowShadowContent;
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

    return StaticNodeList::adopt(matches);
}

class GetCallerCodeBlockFunctor {
public:
    GetCallerCodeBlockFunctor()
        : m_iterations(0)
        , m_codeBlock(0)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        ++m_iterations;
        if (m_iterations < 2)
            return StackVisitor::Continue;

        m_codeBlock = visitor->codeBlock();
        return StackVisitor::Done;
    }

    CodeBlock* codeBlock() const { return m_codeBlock; }

private:
    int m_iterations;
    CodeBlock* m_codeBlock;
};

String Internals::parserMetaData(Deprecated::ScriptValue value)
{
    JSC::VM& vm = contextDocument()->vm();
    JSC::ExecState* exec = vm.topCallFrame;
    JSC::JSValue code = value.jsValue();
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
#if ENABLE(WEBASSEMBLY)
    else if (executable->isWebAssemblyExecutable())
        result.appendLiteral("WebAssembly");
#endif
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

void Internals::setBatteryStatus(const String& eventType, bool charging, double chargingTime, double dischargingTime, double level, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(BATTERY_STATUS)
    BatteryController::from(document->page())->didChangeBatteryStatus(eventType, BatteryStatus::create(charging, chargingTime, dischargingTime, level));
#else
    UNUSED_PARAM(eventType);
    UNUSED_PARAM(charging);
    UNUSED_PARAM(chargingTime);
    UNUSED_PARAM(dischargingTime);
    UNUSED_PARAM(level);
#endif
}

void Internals::setDeviceProximity(const String& eventType, double value, double min, double max, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(PROXIMITY_EVENTS)
    DeviceProximityController::from(document->page())->didChangeDeviceProximity(value, min, max);
#else
    UNUSED_PARAM(eventType);
    UNUSED_PARAM(value);
    UNUSED_PARAM(min);
    UNUSED_PARAM(max);
#endif
}

void Internals::updateEditorUINowIfScheduled()
{
    if (Document* document = contextDocument()) {
        if (Frame* frame = document->frame())
            frame->editor().updateEditorUINowIfScheduled();
    }
}

bool Internals::hasSpellingMarker(int from, int length, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}
    
bool Internals::hasAutocorrectedMarker(int from, int length, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    updateEditorUINowIfScheduled();

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Autocorrected, from, length);
}

void Internals::setContinuousSpellCheckingEnabled(bool enabled, ExceptionCode&)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

    if (enabled != contextDocument()->frame()->editor().isContinuousSpellCheckingEnabled())
        contextDocument()->frame()->editor().toggleContinuousSpellChecking();
}

void Internals::setAutomaticQuoteSubstitutionEnabled(bool enabled, ExceptionCode&)
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

void Internals::setAutomaticLinkDetectionEnabled(bool enabled, ExceptionCode&)
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

void Internals::setAutomaticDashSubstitutionEnabled(bool enabled, ExceptionCode&)
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

void Internals::setAutomaticTextReplacementEnabled(bool enabled, ExceptionCode&)
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

void Internals::setAutomaticSpellingCorrectionEnabled(bool enabled, ExceptionCode&)
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

bool Internals::isOverwriteModeEnabled(ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    return document->frame()->editor().isOverwriteModeEnabled();
}

void Internals::toggleOverwriteModeEnabled(ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return;

    document->frame()->editor().toggleOverwriteModeEnabled();
}

unsigned Internals::countMatchesForText(const String& text, unsigned findOptions, const String& markMatches, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

    bool mark = markMatches == "mark";
    return document->frame()->editor().countMatchesForText(text, nullptr, findOptions, 1000, mark, nullptr);
}

const ProfilesArray& Internals::consoleProfiles() const
{
    return contextDocument()->page()->console().profiles();
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
    Page* inspectedPage = contextDocument()->frame()->page();
    RefPtr<DOMWindow> window = inspectedPage->mainFrame().document()->domWindow();
    RefPtr<DOMWindow> frontendWindow = window->open(url, "", "", *window, *window);
    m_inspectorFrontend = std::make_unique<InspectorStubFrontend>(inspectedPage, frontendWindow.copyRef());

    return frontendWindow;
}

void Internals::closeDummyInspectorFrontend()
{
    m_inspectorFrontend = nullptr;
}

void Internals::setJavaScriptProfilingEnabled(bool enabled, ExceptionCode& ec)
{
    Page* page = contextDocument()->frame()->page();
    if (!page) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    page->inspectorController().setProfilerEnabled(enabled);
}

void Internals::setInspectorIsUnderTest(bool isUnderTest, ExceptionCode& ec)
{
    Page* page = contextDocument()->frame()->page();
    if (!page) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    page->inspectorController().setIsUnderTest(isUnderTest);
}

bool Internals::hasGrammarMarker(int from, int length, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return false;

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Grammar, from, length);
}

unsigned Internals::numberOfScrollableAreas(ExceptionCode&)
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
    
bool Internals::isPageBoxVisible(int pageNumber, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return document->isPageBoxVisible(pageNumber);
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
String Internals::layerTreeAsText(Document* document, ExceptionCode& ec) const
{
    return layerTreeAsText(document, 0, ec);
}

String Internals::layerTreeAsText(Document* document, unsigned flags, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    LayerTreeFlags layerTreeFlags = 0;
    if (flags & LAYER_TREE_INCLUDES_VISIBLE_RECTS)
        layerTreeFlags |= LayerTreeFlagsIncludeVisibleRects;
    if (flags & LAYER_TREE_INCLUDES_TILE_CACHES)
        layerTreeFlags |= LayerTreeFlagsIncludeTileCaches;
    if (flags & LAYER_TREE_INCLUDES_REPAINT_RECTS)
        layerTreeFlags |= LayerTreeFlagsIncludeRepaintRects;
    if (flags & LAYER_TREE_INCLUDES_PAINTING_PHASES)
        layerTreeFlags |= LayerTreeFlagsIncludePaintingPhases;
    if (flags & LAYER_TREE_INCLUDES_CONTENT_LAYERS)
        layerTreeFlags |= LayerTreeFlagsIncludeContentLayers;

    return document->frame()->layerTreeAsText(layerTreeFlags);
}

String Internals::repaintRectsAsText(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return document->frame()->trackedRepaintRectsAsText();
}

String Internals::scrollingStateTreeAsText(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    Page* page = document->page();
    if (!page)
        return String();

    return page->scrollingStateTreeAsText();
}

String Internals::mainThreadScrollingReasons(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    Page* page = document->page();
    if (!page)
        return String();

    return page->synchronousScrollingReasonsAsText();
}

RefPtr<ClientRectList> Internals::nonFastScrollableRects(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    Page* page = document->page();
    if (!page)
        return nullptr;

    return page->nonFastScrollableRects();
}

void Internals::garbageCollectDocumentResources(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    document->cachedResourceLoader().garbageCollectDocumentResources();
}

void Internals::allowRoundingHacks() const
{
    TextRun::setAllowsRoundingHacks(true);
}

void Internals::insertAuthorCSS(const String& css, ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto parsedSheet = StyleSheetContents::create(*document);
    parsedSheet.get().setIsUserStyleSheet(false);
    parsedSheet.get().parseString(css);
    document->extensionStyleSheets().addAuthorStyleSheetForTesting(WTFMove(parsedSheet));
}

void Internals::insertUserCSS(const String& css, ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    auto parsedSheet = StyleSheetContents::create(*document);
    parsedSheet.get().setIsUserStyleSheet(true);
    parsedSheet.get().parseString(css);
    document->extensionStyleSheets().addUserStyleSheet(WTFMove(parsedSheet));
}

String Internals::counterValue(Element* element)
{
    if (!element)
        return String();

    return counterValueForElement(element);
}

int Internals::pageNumber(Element* element, float pageWidth, float pageHeight)
{
    if (!element)
        return 0;

    return PrintContext::pageNumberForElement(element, FloatSize(pageWidth, pageHeight));
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

String Internals::pageProperty(String propertyName, int pageNumber, ExceptionCode& ec) const
{
    if (!frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return PrintContext::pageProperty(frame(), propertyName.utf8().data(), pageNumber);
}

String Internals::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft, ExceptionCode& ec) const
{
    if (!frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return PrintContext::pageSizeAndMarginsInPixels(frame(), pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft);
}

void Internals::setPageScaleFactor(float scaleFactor, int x, int y, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    Page* page = document->page();
    page->setPageScaleFactor(scaleFactor, IntPoint(x, y));
}

void Internals::setPageZoomFactor(float zoomFactor, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    Frame* frame = document->frame();
    frame->setPageZoomFactor(zoomFactor);
}

void Internals::setTextZoomFactor(float zoomFactor, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    Frame* frame = document->frame();
    frame->setTextZoomFactor(zoomFactor);
}

void Internals::setUseFixedLayout(bool useFixedLayout, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    FrameView* frameView = document->view();
    frameView->setUseFixedLayout(useFixedLayout);
}

void Internals::setFixedLayoutSize(int width, int height, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    FrameView* frameView = document->view();
    frameView->setFixedLayoutSize(IntSize(width, height));
}

void Internals::setHeaderHeight(float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;

    FrameView* frameView = document->view();
    frameView->setHeaderHeight(height);
}

void Internals::setFooterHeight(float height)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;

    FrameView* frameView = document->view();
    frameView->setFooterHeight(height);
}
    
void Internals::setTopContentInset(float contentInset)
{
    Document* document = contextDocument();
    if (!document)
        return;
    
    Page* page = document->page();
    page->setTopContentInset(contentInset);
}

#if ENABLE(FULLSCREEN_API)
void Internals::webkitWillEnterFullScreenForElement(Element* element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitWillEnterFullScreenForElement(element);
}

void Internals::webkitDidEnterFullScreenForElement(Element* element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitDidEnterFullScreenForElement(element);
}

void Internals::webkitWillExitFullScreenForElement(Element* element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitWillExitFullScreenForElement(element);
}

void Internals::webkitDidExitFullScreenForElement(Element* element)
{
    Document* document = contextDocument();
    if (!document)
        return;
    document->webkitDidExitFullScreenForElement(element);
}
#endif

void Internals::setApplicationCacheOriginQuota(unsigned long long quota)
{
    Document* document = contextDocument();
    if (!document)
        return;
    ApplicationCacheStorage::singleton().storeUpdatedQuotaForOrigin(document->securityOrigin(), quota);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(scheme);
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

void Internals::startTrackingRepaints(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->setTracksRepaints(true);
}

void Internals::stopTrackingRepaints(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->setTracksRepaints(false);
}

void Internals::startTrackingLayerFlushes(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->renderView()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->renderView()->compositor().startTrackingLayerFlushes();
}

unsigned long Internals::layerFlushCount(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->renderView()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->renderView()->compositor().layerFlushCount();
}

void Internals::startTrackingStyleRecalcs(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    document->startTrackingStyleRecalcs();
}

unsigned long Internals::styleRecalcCount(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    
    return document->styleRecalcCount();
}

void Internals::startTrackingCompositingUpdates(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->renderView()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->renderView()->compositor().startTrackingCompositingUpdates();
}

unsigned long Internals::compositingUpdateCount(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->renderView()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    
    return document->renderView()->compositor().compositingUpdateCount();
}

void Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(ExceptionCode& ec)
{
    updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(nullptr, ec);
}

void Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(Node* node, ExceptionCode& ec)
{
    Document* document;
    if (!node)
        document = contextDocument();
    else if (is<Document>(*node))
        document = downcast<Document>(node);
    else if (is<HTMLIFrameElement>(*node))
        document = downcast<HTMLIFrameElement>(*node).contentDocument();
    else {
        ec = TypeError;
        return;
    }

    document->updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
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

String Internals::getCurrentCursorInfo(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

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
    return "FAIL: Cursor details not available on this platform.";
#endif
}

RefPtr<ArrayBuffer> Internals::serializeObject(PassRefPtr<SerializedScriptValue> value) const
{
    Vector<uint8_t> bytes = value->data();
    return ArrayBuffer::create(bytes.data(), bytes.size());
}

RefPtr<SerializedScriptValue> Internals::deserializeBuffer(PassRefPtr<ArrayBuffer> buffer) const
{
    Vector<uint8_t> bytes;
    bytes.append(static_cast<const uint8_t*>(buffer->data()), buffer->byteLength());
    return SerializedScriptValue::adopt(bytes);
}

bool Internals::isFromCurrentWorld(Deprecated::ScriptValue value) const
{
    ASSERT(!value.hasNoValue());
    
    JSC::ExecState* exec = contextDocument()->vm().topCallFrame;
    if (!value.isObject() || &worldForDOMObject(value.jsValue().getObject()) == &currentWorld(exec))
        return true;
    return false;
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::Settings::setUsesOverlayScrollbars(enabled);
}

void Internals::forceReload(bool endToEnd)
{
    frame()->loader().reload(endToEnd);
}

void Internals::enableAutoSizeMode(bool enabled, int minimumWidth, int minimumHeight, int maximumWidth, int maximumHeight)
{
    Document* document = contextDocument();
    if (!document || !document->view())
        return;
    document->view()->enableAutoSizeMode(enabled, IntSize(minimumWidth, minimumHeight), IntSize(maximumWidth, maximumHeight));
}

#if ENABLE(ENCRYPTED_MEDIA_V2)
void Internals::initializeMockCDM()
{
    CDM::registerCDMFactory([](CDM* cdm) { return std::make_unique<MockCDM>(cdm); },
        MockCDM::supportsKeySystem, MockCDM::supportsKeySystemAndMimeType);
}
#endif

String Internals::markerTextForListItem(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }
    return WebCore::markerTextForListItem(element);
}

String Internals::toolTipFromElement(Element* element, ExceptionCode& ec) const
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }
    HitTestResult result;
    result.setInnerNode(element);
    TextDirection dir;
    return result.title(dir);
}

String Internals::getImageSourceURL(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }
    return element->imageSourceURL();
}

#if ENABLE(VIDEO)
void Internals::simulateAudioInterruption(Node* node)
{
#if USE(GSTREAMER)
    HTMLMediaElement* element = downcast<HTMLMediaElement>(node);
    element->player()->simulateAudioInterruption();
#else
    UNUSED_PARAM(node);
#endif
}

bool Internals::mediaElementHasCharacteristic(Node* node, const String& characteristic, ExceptionCode& ec)
{
    if (!is<HTMLMediaElement>(*node))
        return false;

    HTMLMediaElement* element = downcast<HTMLMediaElement>(node);

    if (equalIgnoringCase(characteristic, "Audible"))
        return element->hasAudio();
    if (equalIgnoringCase(characteristic, "Visual"))
        return element->hasVideo();
    if (equalIgnoringCase(characteristic, "Legible"))
        return element->hasClosedCaptions();

    ec = SYNTAX_ERR;
    return false;
}
#endif

bool Internals::isSelectPopupVisible(Node* node)
{
    if (!is<HTMLSelectElement>(*node))
        return false;

    HTMLSelectElement& select = downcast<HTMLSelectElement>(*node);

    auto* renderer = select.renderer();
    ASSERT(renderer);
    if (!is<RenderMenuList>(*renderer))
        return false;

#if !PLATFORM(IOS)
    return downcast<RenderMenuList>(*renderer).popupIsVisible();
#else
    return false;
#endif // !PLATFORM(IOS)
}

String Internals::captionsStyleSheetOverride(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return emptyString();
    }

#if ENABLE(VIDEO_TRACK)
    return document->page()->group().captionPreferences()->captionsStyleSheetOverride();
#else
    return emptyString();
#endif
}

void Internals::setCaptionsStyleSheetOverride(const String& override, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(VIDEO_TRACK)
    document->page()->group().captionPreferences()->setCaptionsStyleSheetOverride(override);
#else
    UNUSED_PARAM(override);
#endif
}

void Internals::setPrimaryAudioTrackLanguageOverride(const String& language, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(VIDEO_TRACK)
    document->page()->group().captionPreferences()->setPrimaryAudioTrackLanguageOverride(language);
#else
    UNUSED_PARAM(language);
#endif
}

void Internals::setCaptionDisplayMode(const String& mode, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    
#if ENABLE(VIDEO_TRACK)
    CaptionUserPreferences* captionPreferences = document->page()->group().captionPreferences();
    
    if (equalIgnoringCase(mode, "Automatic"))
        captionPreferences->setCaptionDisplayMode(CaptionUserPreferences::Automatic);
    else if (equalIgnoringCase(mode, "ForcedOnly"))
        captionPreferences->setCaptionDisplayMode(CaptionUserPreferences::ForcedOnly);
    else if (equalIgnoringCase(mode, "AlwaysOn"))
        captionPreferences->setCaptionDisplayMode(CaptionUserPreferences::AlwaysOn);
    else
        ec = SYNTAX_ERR;
#else
    UNUSED_PARAM(mode);
#endif
}

#if ENABLE(VIDEO)
Ref<TimeRanges> Internals::createTimeRanges(Float32Array* startTimes, Float32Array* endTimes)
{
    ASSERT(startTimes && endTimes);
    ASSERT(startTimes->length() == endTimes->length());
    Ref<TimeRanges> ranges = TimeRanges::create();

    unsigned count = std::min(startTimes->length(), endTimes->length());
    for (unsigned i = 0; i < count; ++i)
        ranges->add(startTimes->item(i), endTimes->item(i));
    return ranges;
}

double Internals::closestTimeToTimeRanges(double time, TimeRanges* ranges)
{
    return ranges->nearest(time);
}
#endif

Ref<ClientRect> Internals::selectionBounds(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRect::create();
    }

    return ClientRect::create(document->frame()->selection().selectionBounds());
}

#if ENABLE(VIBRATION)
bool Internals::isVibrating()
{
    Page* page = contextDocument()->page();
    ASSERT(page);

    return Vibration::from(page)->isVibrating();
}
#endif

bool Internals::isPluginUnavailabilityIndicatorObscured(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    auto* renderer = element->renderer();
    if (!is<RenderEmbeddedObject>(renderer)) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return downcast<RenderEmbeddedObject>(*renderer).isReplacementObscured();
}
    
bool Internals::isPluginSnapshotted(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }
    HTMLPlugInElement* pluginElement = downcast<HTMLPlugInElement>(element);
    return pluginElement->displayState() <= HTMLPlugInElement::DisplayingSnapshot;
}
    
#if ENABLE(MEDIA_SOURCE)
void Internals::initializeMockMediaSource()
{
#if USE(AVFOUNDATION)
    WebCore::Settings::setAVFoundationEnabled(false);
#endif
    MediaPlayerFactorySupport::callRegisterMediaEngine(MockMediaPlayerMediaSource::registerMediaEngine);
}

Vector<String> Internals::bufferedSamplesForTrackID(SourceBuffer* buffer, const AtomicString& trackID)
{
    if (!buffer)
        return Vector<String>();

    return buffer->bufferedSamplesForTrackID(trackID);
}
    
void Internals::setShouldGenerateTimestamps(SourceBuffer* buffer, bool flag)
{
    if (buffer)
        buffer->setShouldGenerateTimestamps(flag);
}
#endif

#if ENABLE(VIDEO)
void Internals::beginMediaSessionInterruption(const String& interruptionString, ExceptionCode& ec)
{
    PlatformMediaSession::InterruptionType interruption = PlatformMediaSession::SystemInterruption;

    if (equalIgnoringCase(interruptionString, "System"))
        interruption = PlatformMediaSession::SystemInterruption;
    else if (equalIgnoringCase(interruptionString, "SystemSleep"))
        interruption = PlatformMediaSession::SystemSleep;
    else if (equalIgnoringCase(interruptionString, "EnteringBackground"))
        interruption = PlatformMediaSession::EnteringBackground;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    PlatformMediaSessionManager::sharedManager().beginInterruption(interruption);
}

void Internals::endMediaSessionInterruption(const String& flagsString)
{
    PlatformMediaSession::EndInterruptionFlags flags = PlatformMediaSession::NoFlags;

    if (equalIgnoringCase(flagsString, "MayResumePlaying"))
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

void Internals::setMediaSessionRestrictions(const String& mediaTypeString, const String& restrictionsString, ExceptionCode& ec)
{
    PlatformMediaSession::MediaType mediaType = PlatformMediaSession::None;
    if (equalIgnoringCase(mediaTypeString, "Video"))
        mediaType = PlatformMediaSession::Video;
    else if (equalIgnoringCase(mediaTypeString, "Audio"))
        mediaType = PlatformMediaSession::Audio;
    else if (equalIgnoringCase(mediaTypeString, "WebAudio"))
        mediaType = PlatformMediaSession::WebAudio;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    PlatformMediaSessionManager::SessionRestrictions restrictions = PlatformMediaSessionManager::sharedManager().restrictions(mediaType);
    PlatformMediaSessionManager::sharedManager().removeRestriction(mediaType, restrictions);

    restrictions = PlatformMediaSessionManager::NoRestrictions;

    Vector<String> restrictionsArray;
    restrictionsString.split(',', false, restrictionsArray);
    for (auto& restrictionString : restrictionsArray) {
        if (equalIgnoringCase(restrictionString, "ConcurrentPlaybackNotPermitted"))
            restrictions |= PlatformMediaSessionManager::ConcurrentPlaybackNotPermitted;
        if (equalIgnoringCase(restrictionString, "BackgroundProcessPlaybackRestricted"))
            restrictions |= PlatformMediaSessionManager::BackgroundProcessPlaybackRestricted;
        if (equalIgnoringCase(restrictionString, "BackgroundTabPlaybackRestricted"))
            restrictions |= PlatformMediaSessionManager::BackgroundTabPlaybackRestricted;
        if (equalIgnoringCase(restrictionString, "InterruptedPlaybackNotPermitted"))
            restrictions |= PlatformMediaSessionManager::InterruptedPlaybackNotPermitted;
    }
    PlatformMediaSessionManager::sharedManager().addRestriction(mediaType, restrictions);
}

void Internals::setMediaElementRestrictions(HTMLMediaElement* element, const String& restrictionsString, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    MediaElementSession::BehaviorRestrictions restrictions = element->mediaSession().behaviorRestrictions();
    element->mediaSession().removeBehaviorRestriction(restrictions);

    restrictions = MediaElementSession::NoRestrictions;

    Vector<String> restrictionsArray;
    restrictionsString.split(',', false, restrictionsArray);
    for (auto& restrictionString : restrictionsArray) {
        if (equalIgnoringCase(restrictionString, "NoRestrictions"))
            restrictions |= MediaElementSession::NoRestrictions;
        if (equalIgnoringCase(restrictionString, "RequireUserGestureForLoad"))
            restrictions |= MediaElementSession::RequireUserGestureForLoad;
        if (equalIgnoringCase(restrictionString, "RequireUserGestureForRateChange"))
            restrictions |= MediaElementSession::RequireUserGestureForRateChange;
        if (equalIgnoringCase(restrictionString, "RequireUserGestureForFullscreen"))
            restrictions |= MediaElementSession::RequireUserGestureForFullscreen;
        if (equalIgnoringCase(restrictionString, "RequirePageConsentToLoadMedia"))
            restrictions |= MediaElementSession::RequirePageConsentToLoadMedia;
        if (equalIgnoringCase(restrictionString, "RequirePageConsentToResumeMedia"))
            restrictions |= MediaElementSession::RequirePageConsentToResumeMedia;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (equalIgnoringCase(restrictionString, "RequireUserGestureToShowPlaybackTargetPicker"))
            restrictions |= MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker;
        if (equalIgnoringCase(restrictionString, "WirelessVideoPlaybackDisabled"))
            restrictions |= MediaElementSession::WirelessVideoPlaybackDisabled;
#endif
        if (equalIgnoringCase(restrictionString, "RequireUserGestureForAudioRateChange"))
            restrictions |= MediaElementSession::RequireUserGestureForAudioRateChange;
        if (equalIgnoringCase(restrictionString, "MetadataPreloadingNotPermitted"))
            restrictions |= MediaElementSession::MetadataPreloadingNotPermitted;
        if (equalIgnoringCase(restrictionString, "AutoPreloadingNotPermitted"))
            restrictions |= MediaElementSession::AutoPreloadingNotPermitted;
        if (equalIgnoringCase(restrictionString, "InvisibleAutoplayNotPermitted"))
            restrictions |= MediaElementSession::InvisibleAutoplayNotPermitted;
    }
    element->mediaSession().addBehaviorRestriction(restrictions);
}

void Internals::postRemoteControlCommand(const String& commandString, ExceptionCode& ec)
{
    PlatformMediaSession::RemoteControlCommandType command;
    
    if (equalIgnoringCase(commandString, "Play"))
        command = PlatformMediaSession::PlayCommand;
    else if (equalIgnoringCase(commandString, "Pause"))
        command = PlatformMediaSession::PauseCommand;
    else if (equalIgnoringCase(commandString, "Stop"))
        command = PlatformMediaSession::StopCommand;
    else if (equalIgnoringCase(commandString, "TogglePlayPause"))
        command = PlatformMediaSession::TogglePlayPauseCommand;
    else if (equalIgnoringCase(commandString, "BeginSeekingBackward"))
        command = PlatformMediaSession::BeginSeekingBackwardCommand;
    else if (equalIgnoringCase(commandString, "EndSeekingBackward"))
        command = PlatformMediaSession::EndSeekingBackwardCommand;
    else if (equalIgnoringCase(commandString, "BeginSeekingForward"))
        command = PlatformMediaSession::BeginSeekingForwardCommand;
    else if (equalIgnoringCase(commandString, "EndSeekingForward"))
        command = PlatformMediaSession::EndSeekingForwardCommand;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    
    PlatformMediaSessionManager::sharedManager().didReceiveRemoteControlCommand(command);
}

bool Internals::elementIsBlockingDisplaySleep(Element* element) const
{
    HTMLMediaElement* mediaElement = downcast<HTMLMediaElement>(element);
    return mediaElement ? mediaElement->isDisablingSleep() : false;
}

#endif // ENABLE(VIDEO)

#if ENABLE(MEDIA_SESSION)
static MediaSessionInterruptingCategory interruptingCategoryFromString(const String& interruptingCategoryString)
{
    if (interruptingCategoryString == "content")
        return MediaSessionInterruptingCategory::Content;
    if (interruptingCategoryString == "transient")
        return MediaSessionInterruptingCategory::Transient;
    if (interruptingCategoryString == "transient-solo")
        return MediaSessionInterruptingCategory::TransientSolo;
    ASSERT_NOT_REACHED();
    return MediaSessionInterruptingCategory::Content;
}

void Internals::sendMediaSessionStartOfInterruptionNotification(const String& interruptingCategoryString)
{
    MediaSessionManager::singleton().didReceiveStartOfInterruptionNotification(interruptingCategoryFromString(interruptingCategoryString));
}

void Internals::sendMediaSessionEndOfInterruptionNotification(const String& interruptingCategoryString)
{
    MediaSessionManager::singleton().didReceiveEndOfInterruptionNotification(interruptingCategoryFromString(interruptingCategoryString));
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

void Internals::sendMediaControlEvent(const String& event)
{
    if (event == "play-pause")
        MediaSessionManager::singleton().togglePlayback();
    else if (event == "next-track")
        MediaSessionManager::singleton().skipToNextTrack();
    else if (event == "previous-track")
        MediaSessionManager::singleton().skipToPreviousTrack();
    else
        ASSERT_NOT_REACHED();
}
#endif // ENABLE(MEDIA_SESSION)

#if ENABLE(WEB_AUDIO)
void Internals::setAudioContextRestrictions(AudioContext* context, const String &restrictionsString, ExceptionCode &ec)
{
    if (!context) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    AudioContext::BehaviorRestrictions restrictions = context->behaviorRestrictions();
    context->removeBehaviorRestriction(restrictions);

    restrictions = MediaElementSession::NoRestrictions;

    Vector<String> restrictionsArray;
    restrictionsString.split(',', false, restrictionsArray);
    for (auto& restrictionString : restrictionsArray) {
        if (equalIgnoringCase(restrictionString, "NoRestrictions"))
            restrictions |= AudioContext::NoRestrictions;
        if (equalIgnoringCase(restrictionString, "RequireUserGestureForAudioStart"))
            restrictions |= AudioContext::RequireUserGestureForAudioStartRestriction;
        if (equalIgnoringCase(restrictionString, "RequirePageConsentForAudioStart"))
            restrictions |= AudioContext::RequirePageConsentForAudioStartRestriction;
    }
    context->addBehaviorRestriction(restrictions);
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

void Internals::setMockMediaPlaybackTargetPickerState(const String& deviceName, const String& deviceState, ExceptionCode& ec)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    MediaPlaybackTargetContext::State state = MediaPlaybackTargetContext::Unknown;

    if (equalIgnoringCase(deviceState, "DeviceAvailable"))
        state = MediaPlaybackTargetContext::OutputDeviceAvailable;
    else if (equalIgnoringCase(deviceState, "DeviceUnavailable"))
        state = MediaPlaybackTargetContext::OutputDeviceUnavailable;
    else if (equalIgnoringCase(deviceState, "Unknown"))
        state = MediaPlaybackTargetContext::Unknown;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    page->setMockMediaPlaybackTargetPickerState(deviceName, state);
}
#endif

RefPtr<MockPageOverlay> Internals::installMockPageOverlay(const String& overlayType, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    return MockPageOverlayClient::singleton().installOverlay(document->frame()->mainFrame(), overlayType == "view" ? PageOverlay::OverlayType::View : PageOverlay::OverlayType::Document);
}

String Internals::pageOverlayLayerTreeAsText(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    document->updateLayout();

    return MockPageOverlayClient::singleton().layerTreeAsText(document->frame()->mainFrame());
}

void Internals::setPageMuted(bool muted)
{
    Document* document = contextDocument();
    if (!document)
        return;

    if (Page* page = document->page())
        page->setMuted(muted);
}

bool Internals::isPagePlayingAudio()
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return false;

    return !!(document->page()->mediaState() & MediaProducer::IsPlayingAudio);
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

    builder.append("{ ");
    for (auto& coordinate : snapOffsets) {
        if (!justStarting)
            builder.append(", ");
        else
            justStarting = false;
        
        builder.append(String::number(coordinate.toUnsigned()));
    }
    builder.append(" }");
}
    
String Internals::scrollSnapOffsets(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    if (!element->renderBox())
        return String();

    RenderBox& box = *element->renderBox();
    ScrollableArea* scrollableArea;
    
    if (box.isBody()) {
        FrameView* frameView = box.frame().mainFrame().view();
        if (!frameView || !frameView->isScrollable()) {
            ec = INVALID_ACCESS_ERR;
            return String();
        }
        scrollableArea = frameView;
        
    } else {
        if (!box.canBeScrolledAndHasScrollableArea()) {
            ec = INVALID_ACCESS_ERR;
            return String();
        }
        scrollableArea = box.layer();
    }

    if (!scrollableArea)
        return String();
    
    StringBuilder result;

    if (scrollableArea->horizontalSnapOffsets()) {
        result.append("horizontal = ");
        appendOffsets(result, *scrollableArea->horizontalSnapOffsets());
    }

    if (scrollableArea->verticalSnapOffsets()) {
        if (result.length())
            result.append(", ");

        result.append("vertical = ");
        appendOffsets(result, *scrollableArea->verticalSnapOffsets());
    }

    return result.toString();
}
#endif

bool Internals::testPreloaderSettingViewport()
{
    return testPreloadScannerViewportSupport(contextDocument());
}

String Internals::pathStringWithShrinkWrappedRects(Vector<double> rectComponents, double radius, ExceptionCode& ec)
{
    if (rectComponents.size() % 4) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    Vector<FloatRect> rects;
    while (!rectComponents.isEmpty()) {
        double height = rectComponents.takeLast();
        double width = rectComponents.takeLast();
        double y = rectComponents.takeLast();
        double x = rectComponents.takeLast();

        rects.append(FloatRect(x, y, width, height));
    }

    rects.reverse();

    Path path = PathUtilities::pathWithShrinkWrappedRects(rects, radius);

    String pathString;
    buildStringFromPath(path, pathString);

    return pathString;
}


String Internals::getCurrentMediaControlsStatusForElement(HTMLMediaElement* mediaElement)
{
#if !ENABLE(MEDIA_CONTROLS_SCRIPT)
    UNUSED_PARAM(mediaElement);
    return String();
#else
    if (!mediaElement)
        return String();

    return mediaElement->getCurrentMediaControlsStatus();
#endif
}

#if !PLATFORM(COCOA)
String Internals::userVisibleString(const DOMURL*)
{
    // Not implemented in WebCore.
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

#if ENABLE(STREAMS_API)
bool Internals::isReadableStreamDisturbed(ScriptState& state, JSValue stream)
{
    JSGlobalObject* globalObject = state.vmEntryGlobalObject();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(state.vm().clientData);
    const Identifier& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().isReadableStreamDisturbedPrivateName();
    JSValue value;
    PropertySlot propertySlot(value);
    globalObject->fastGetOwnPropertySlot(&state, state.vm(), *globalObject->structure(), privateName, propertySlot);
    value = propertySlot.getValue(&state, privateName);
    ASSERT(value.isFunction());

    JSObject* function = value.getObject();
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != JSC::CallTypeNone);
    MarkedArgumentBuffer arguments;
    arguments.append(stream);
    JSValue returnedValue = JSC::call(&state, function, callType, callData, JSC::jsUndefined(), arguments);
    ASSERT(returnedValue.isBoolean());

    return returnedValue.asBoolean();
}
#endif

}
