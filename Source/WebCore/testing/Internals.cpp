/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "AnimationController.h"
#include "ApplicationCacheStorage.h"
#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ContentDistributor.h"
#include "Cursor.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentMarker.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "ExceptionCode.h"
#include "FormController.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorForwarding.h"
#include "InspectorFrontendClientLocal.h"
#include "InspectorInstrumentation.h"
#include "InspectorOverlay.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "InternalSettings.h"
#include "Language.h"
#include "MainFrame.h"
#include "MallocStatistics.h"
#include "MediaPlayer.h"
#include "MediaSessionManager.h"
#include "MemoryCache.h"
#include "MemoryInfo.h"
#include "Page.h"
#include "PageConsole.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "Range.h"
#include "RenderEmbeddedObject.h"
#include "RenderMenuList.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "ScrollingCoordinator.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpellChecker.h"
#include "StaticNodeList.h"
#include "StyleSheetContents.h"
#include "TextIterator.h"
#include "TreeScope.h"
#include "TypeConversions.h"
#include "ViewportArguments.h"
#include "WebConsoleAgent.h"
#include "WorkerThread.h"
#include <bytecode/CodeBlock.h>
#include <inspector/InspectorAgentBase.h>
#include <inspector/InspectorValues.h>
#include <runtime/JSCJSValue.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(BATTERY_STATUS)
#include "BatteryController.h"
#endif

#if ENABLE(NETWORK_INFO)
#include "NetworkInfo.h"
#include "NetworkInfoController.h"
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
#include "MockMediaStreamCenter.h"
#include "RTCPeerConnection.h"
#include "RTCPeerConnectionHandlerMock.h"
#include "UserMediaClientMock.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MockMediaPlayerMediaSource.h"
#endif

using JSC::CodeBlock;
using JSC::FunctionExecutable;
using JSC::JSFunction;
using JSC::JSValue;
using JSC::ScriptExecutable;
using JSC::StackVisitor;

using namespace Inspector;

namespace WebCore {

using namespace HTMLNames;

#if ENABLE(INSPECTOR)
class InspectorFrontendClientDummy : public InspectorFrontendClientLocal {
public:
    InspectorFrontendClientDummy(InspectorController*, Page*);
    virtual ~InspectorFrontendClientDummy() { }
    virtual void attachWindow(DockSide) override { }
    virtual void detachWindow() override { }

    virtual String localizedStringsURL() override { return String(); }

    virtual void bringToFront() override { }
    virtual void closeWindow() override { }

    virtual void inspectedURLChanged(const String&) override { }

protected:
    virtual void setAttachedWindowHeight(unsigned) override { }
    virtual void setAttachedWindowWidth(unsigned) override { }
    virtual void setToolbarHeight(unsigned) override { }
};

InspectorFrontendClientDummy::InspectorFrontendClientDummy(InspectorController* controller, Page* page)
    : InspectorFrontendClientLocal(controller, page, std::make_unique<InspectorFrontendClientLocal::Settings>())
{
}

class InspectorFrontendChannelDummy : public InspectorFrontendChannel {
public:
    explicit InspectorFrontendChannelDummy(Page*);
    virtual ~InspectorFrontendChannelDummy() { }
    virtual bool sendMessageToFrontend(const String& message) override;

private:
    Page* m_frontendPage;
};

InspectorFrontendChannelDummy::InspectorFrontendChannelDummy(Page* page)
    : m_frontendPage(page)
{
}

bool InspectorFrontendChannelDummy::sendMessageToFrontend(const String& message)
{
    return InspectorClient::doDispatchMessageOnFrontendPage(m_frontendPage, message);
}
#endif // ENABLE(INSPECTOR)

static bool markerTypesFrom(const String& markerType, DocumentMarker::MarkerTypes& result)
{
    if (markerType.isEmpty() || equalIgnoringCase(markerType, "all"))
        result = DocumentMarker::AllMarkers();
    else if (equalIgnoringCase(markerType, "Spelling"))
        result =  DocumentMarker::Spelling;
    else if (equalIgnoringCase(markerType, "Grammar"))
        result =  DocumentMarker::Grammar;
    else if (equalIgnoringCase(markerType, "TextMatch"))
        result =  DocumentMarker::TextMatch;
    else if (equalIgnoringCase(markerType, "Replacement"))
        result =  DocumentMarker::Replacement;
    else if (equalIgnoringCase(markerType, "CorrectionIndicator"))
        result =  DocumentMarker::CorrectionIndicator;
    else if (equalIgnoringCase(markerType, "RejectedCorrection"))
        result =  DocumentMarker::RejectedCorrection;
    else if (equalIgnoringCase(markerType, "Autocorrected"))
        result =  DocumentMarker::Autocorrected;
    else if (equalIgnoringCase(markerType, "SpellCheckingExemption"))
        result =  DocumentMarker::SpellCheckingExemption;
    else if (equalIgnoringCase(markerType, "DeletedAutocorrection"))
        result =  DocumentMarker::DeletedAutocorrection;
    else if (equalIgnoringCase(markerType, "DictationAlternatives"))
        result =  DocumentMarker::DictationAlternatives;
    else
        return false;

    return true;
}

const char* Internals::internalsId = "internals";

PassRefPtr<Internals> Internals::create(Document* document)
{
    return adoptRef(new Internals(document));
}

Internals::~Internals()
{
}

void Internals::resetToConsistentState(Page* page)
{
    ASSERT(page);

    page->setPageScaleFactor(1, IntPoint(0, 0));
    page->setPagination(Pagination());

    FrameView* mainFrameView = page->mainFrame().view();
    if (mainFrameView) {
        mainFrameView->setHeaderHeight(0);
        mainFrameView->setFooterHeight(0);
    }

    TextRun::setAllowsRoundingHacks(false);
    WebCore::overrideUserPreferredLanguages(Vector<String>());
    WebCore::Settings::setUsesOverlayScrollbars(false);
#if ENABLE(INSPECTOR)
    page->inspectorController().setProfilerEnabled(false);
#endif
#if ENABLE(VIDEO_TRACK) && !PLATFORM(WIN)
    page->group().captionPreferences()->setCaptionsStyleSheetOverride(emptyString());
    page->group().captionPreferences()->setTestingMode(false);
#endif
    if (!page->mainFrame().editor().isContinuousSpellCheckingEnabled())
        page->mainFrame().editor().toggleContinuousSpellChecking();
    if (page->mainFrame().editor().isOverwriteModeEnabled())
        page->mainFrame().editor().toggleOverwriteModeEnabled();
    cacheStorage().setDefaultOriginQuota(ApplicationCacheStorage::noQuota());
#if ENABLE(VIDEO)
    MediaSessionManager::sharedManager().resetRestrictions();
#endif
#if HAVE(ACCESSIBILITY)
    AXObjectCache::setEnhancedUserInterfaceAccessibility(false);
    AXObjectCache::disableAccessibility();
#endif
}

Internals::Internals(Document* document)
    : ContextDestructionObserver(document)
{
#if ENABLE(VIDEO_TRACK) && !PLATFORM(WIN)
    if (document && document->page())
        document->page()->group().captionPreferences()->setTestingMode(true);
#endif

#if ENABLE(MEDIA_STREAM)
    MockMediaStreamCenter::registerMockMediaStreamCenter();
    enableMockRTCPeerConnectionHandler();
    WebCore::provideUserMediaTo(document->page(), new UserMediaClientMock());
#endif
}

Document* Internals::contextDocument() const
{
    return toDocument(scriptExecutionContext());
}

Frame* Internals::frame() const
{
    if (!contextDocument())
        return 0;
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
    char buf[32];
    sprintf(buf, "%p", node);

    return String(buf);
}

bool Internals::isPreloaded(const String& url)
{
    Document* document = contextDocument();
    return document->cachedResourceLoader()->isPreloaded(url);
}

bool Internals::isLoadingFromMemoryCache(const String& url)
{
    if (!contextDocument() || !contextDocument()->page())
        return false;
    CachedResource* resource = memoryCache()->resourceForURL(contextDocument()->completeURL(url), contextDocument()->page()->sessionID());
    return resource && resource->status() == CachedResource::Cached;
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

PassRefPtr<CSSComputedStyleDeclaration> Internals::computedStyleIncludingVisitedInfo(Node* node, ExceptionCode& ec) const
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
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
        return 0;
    }
    return host->shadowRoot();
}

String Internals::shadowRootType(const Node* root, ExceptionCode& ec) const
{
    if (!root || !root->isShadowRoot()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    switch (toShadowRoot(root)->type()) {
    case ShadowRoot::UserAgentShadowRoot:
        return String("UserAgentShadowRoot");
    default:
        ASSERT_NOT_REACHED();
        return String("Unknown");
    }
}

Element* Internals::includerFor(Node*, ExceptionCode& ec)
{
    ec = INVALID_ACCESS_ERR;
    return 0;
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

String Internals::visiblePlaceholder(Element* element)
{
    if (element && isHTMLTextFormControlElement(*element)) {
        if (toHTMLTextFormControlElement(*element).placeholderShouldBeVisible())
            return toHTMLTextFormControlElement(*element).placeholderElement()->textContent();
    }

    return String();
}

#if ENABLE(INPUT_TYPE_COLOR)
void Internals::selectColorInColorChooser(Element* element, const String& colorValue)
{
    if (!isHTMLInputElement(element))
        return;
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;
    inputElement->selectColorInColorChooser(Color(colorValue));
}
#endif

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
    SpeechSynthesis* synthesis = DOMWindowSpeechSynthesis::speechSynthesis(document->domWindow());
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
#endif

PassRefPtr<ClientRect> Internals::absoluteCaretBounds(ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRect::create();
    }

    return ClientRect::create(document->frame()->selection().absoluteCaretBounds());
}

PassRefPtr<ClientRect> Internals::boundingBox(Element* element, ExceptionCode& ec)
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

PassRefPtr<ClientRectList> Internals::inspectorHighlightRects(ExceptionCode& ec)
{
#if ENABLE(INSPECTOR)
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRectList::create();
    }

    Highlight highlight;
    document->page()->inspectorController().getHighlight(&highlight);
    return ClientRectList::create(highlight.quads);
#else
    UNUSED_PARAM(ec);
    return ClientRectList::create();
#endif
}

String Internals::inspectorHighlightObject(ExceptionCode& ec)
{
#if ENABLE(INSPECTOR)
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }
    RefPtr<InspectorObject> object = document->page()->inspectorController().buildObjectForHighlightedNode();
    return object ? object->toJSONString() : String();
#else
    UNUSED_PARAM(ec);
    return String();
#endif
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

DocumentMarker* Internals::markerAt(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
{
    node->document().updateLayoutIgnorePendingStylesheets();
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

    Vector<DocumentMarker*> markers = node->document().markers().markersFor(node, markerTypes);
    if (markers.size() <= index)
        return 0;
    return markers[index];
}

PassRefPtr<Range> Internals::markerRangeForNode(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
{
    DocumentMarker* marker = markerAt(node, markerType, index, ec);
    if (!marker)
        return 0;
    return Range::create(node->document(), node, marker->startOffset(), node, marker->endOffset());
}

String Internals::markerDescriptionForNode(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
{
    DocumentMarker* marker = markerAt(node, markerType, index, ec);
    if (!marker)
        return String();
    return marker->description();
}

void Internals::addTextMatchMarker(const Range* range, bool isActive)
{
    range->ownerDocument().updateLayoutIgnorePendingStylesheets();
    range->ownerDocument().markers().addTextMatchMarker(range, isActive);
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

    if (HTMLInputElement* inputElement = textField->toInputElement())
        return inputElement->lastChangeWasUserEdit();

    // FIXME: We should be using hasTagName instead but Windows port doesn't link QualifiedNames properly.
    if (textField->tagName() == "TEXTAREA")
        return toHTMLTextAreaElement(textField)->lastChangeWasUserEdit();

    ec = INVALID_NODE_TYPE_ERR;
    return false;
}

bool Internals::elementShouldAutoComplete(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    if (HTMLInputElement* inputElement = element->toInputElement())
        return inputElement->shouldAutocomplete();

    ec = INVALID_NODE_TYPE_ERR;
    return false;
}

String Internals::suggestedValue(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement) {
        ec = INVALID_NODE_TYPE_ERR;
        return String();
    }

    return inputElement->suggestedValue();
}

void Internals::setSuggestedValue(Element* element, const String& value, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    inputElement->setSuggestedValue(value);
}

void Internals::setEditingValue(Element* element, const String& value, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    inputElement->setEditingValue(value);
}

void Internals::setAutofilled(Element* element, bool enabled, ExceptionCode& ec)
{
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    inputElement->setAutofilled(enabled);
}

void Internals::scrollElementToRect(Element* element, long x, long y, long w, long h, ExceptionCode& ec)
{
    if (!element || !element->document().view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    FrameView* frameView = element->document().view();
    frameView->scrollElementToRect(element, IntRect(x, y, w, h));
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

PassRefPtr<Range> Internals::rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, ExceptionCode& ec)
{
    if (!scope) {
        ec = INVALID_ACCESS_ERR;
        return 0;
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

    const TouchEventTargetSet* touchHandlers = document->touchEventTargets();
    if (!touchHandlers)
        return 0;

    unsigned count = 0;
    for (TouchEventTargetSet::const_iterator iter = touchHandlers->begin(); iter != touchHandlers->end(); ++iter)
        count += iter->value;
    return count;
}

// FIXME: Remove the document argument. It is almost always the same as
// contextDocument(), with the exception of a few tests that pass a
// different document, and could just make the call through another Internals
// instance instead.
PassRefPtr<NodeList> Internals::nodesFromRect(Document* document, int centerX, int centerY, unsigned topPadding, unsigned rightPadding,
    unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, bool allowChildFrameContent, ExceptionCode& ec) const
{
    if (!document || !document->frame() || !document->frame()->view()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    Frame* frame = document->frame();
    FrameView* frameView = document->view();
    RenderView* renderView = document->renderView();
    if (!renderView)
        return 0;

    document->updateLayoutIgnorePendingStylesheets();

    float zoomFactor = frame->pageZoomFactor();
    LayoutPoint point = roundedLayoutPoint(FloatPoint(centerX * zoomFactor + frameView->scrollX(), centerY * zoomFactor + frameView->scrollY()));

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
        return 0;

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
        for (auto it = nodeSet.begin(), end = nodeSet.end(); it != end; ++it)
            matches.uncheckedAppend(*it->get());
    }

    return StaticNodeList::adopt(matches);
}

void Internals::emitInspectorDidBeginFrame()
{
#if ENABLE(INSPECTOR)
    contextDocument()->frame()->page()->inspectorController().didBeginFrame();
#endif
}

void Internals::emitInspectorDidCancelFrame()
{
#if ENABLE(INSPECTOR)
    contextDocument()->frame()->page()->inspectorController().didCancelFrame();
#endif
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
        executable = codeBlock->ownerExecutable(); 
    } else if (code.isFunction()) {
        JSFunction* funcObj = JSC::jsCast<JSFunction*>(code.toObject(exec));
        executable = funcObj->jsExecutable();
    } else
        return String();

    unsigned startLine = executable->lineNo();
    unsigned startColumn = executable->startColumn();
    unsigned endLine = executable->lastLine();
    unsigned endColumn = executable->endColumn();

    StringBuilder result;

    if (executable->isFunctionExecutable()) {
        FunctionExecutable* funcExecutable = reinterpret_cast<FunctionExecutable*>(executable);
        String inferredName = funcExecutable->inferredName().string();
        result.append("function \"");
        result.append(inferredName);
        result.append("\"");
    } else if (executable->isEvalExecutable())
        result.append("eval");
    else {
        ASSERT(executable->isProgramExecutable());
        result.append("program");
    }

    result.append(" { ");
    result.appendNumber(startLine);
    result.append(":");
    result.appendNumber(startColumn);
    result.append(" - ");
    result.appendNumber(endLine);
    result.append(":");
    result.appendNumber(endColumn);
    result.append(" }");

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

void Internals::setNetworkInformation(const String& eventType, double bandwidth, bool metered, ExceptionCode& ec)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(NETWORK_INFO)
    NetworkInfoController::from(document->page())->didChangeNetworkInformation(eventType, NetworkInfo::create(bandwidth, metered));
#else
    UNUSED_PARAM(eventType);
    UNUSED_PARAM(bandwidth);
    UNUSED_PARAM(metered);
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

Node* Internals::findEditingDeleteButton()
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

    updateEditorUINowIfScheduled();

    // FIXME: We shouldn't pollute the id namespace with this name.
    return document->getElementById(String(ASCIILiteral("WebKit-Editing-Delete-Button")));
}

bool Internals::hasSpellingMarker(int from, int length, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

    updateEditorUINowIfScheduled();

    return document->frame()->editor().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}
    
bool Internals::hasAutocorrectedMarker(int from, int length, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

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
        return 0;

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
    return document->frame()->editor().countMatchesForText(text, nullptr, findOptions, std::numeric_limits<unsigned>::max(), mark, nullptr);
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

#if ENABLE(INSPECTOR)
Vector<String> Internals::consoleMessageArgumentCounts() const
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return Vector<String>();

    InstrumentingAgents* instrumentingAgents = instrumentationForPage(document->page());
    if (!instrumentingAgents)
        return Vector<String>();

    InspectorConsoleAgent* consoleAgent = instrumentingAgents->webConsoleAgent();
    if (!consoleAgent)
        return Vector<String>();

    Vector<unsigned> counts = consoleAgent->consoleMessageArgumentCounts();
    Vector<String> result(counts.size());
    for (size_t i = 0; i < counts.size(); i++)
        result[i] = String::number(counts[i]);
    return result;
}

PassRefPtr<DOMWindow> Internals::openDummyInspectorFrontend(const String& url)
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);

    DOMWindow* window = page->mainFrame().document()->domWindow();
    ASSERT(window);

    m_frontendWindow = window->open(url, "", "", *window, *window);
    ASSERT(m_frontendWindow);

    Page* frontendPage = m_frontendWindow->document()->page();
    ASSERT(frontendPage);

    auto frontendClient = std::make_unique<InspectorFrontendClientDummy>(&page->inspectorController(), frontendPage);

    frontendPage->inspectorController().setInspectorFrontendClient(std::move(frontendClient));

    m_frontendChannel = adoptPtr(new InspectorFrontendChannelDummy(frontendPage));

    page->inspectorController().connectFrontend(m_frontendChannel.get());

    return m_frontendWindow;
}

void Internals::closeDummyInspectorFrontend()
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);
    ASSERT(m_frontendWindow);

    page->inspectorController().disconnectFrontend(InspectorDisconnectReason::InspectorDestroyed);

    m_frontendChannel.release();

    m_frontendWindow->close(m_frontendWindow->scriptExecutionContext());
    m_frontendWindow.release();
}

void Internals::setInspectorResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode& ec)
{
    Page* page = contextDocument()->frame()->page();
    if (!page) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    page->inspectorController().setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
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
#endif // ENABLE(INSPECTOR)

bool Internals::hasGrammarMarker(int from, int length, ExceptionCode&)
{
    Document* document = contextDocument();
    if (!document || !document->frame())
        return 0;

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

PassRefPtr<ClientRectList> Internals::nonFastScrollableRects(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    Page* page = document->page();
    if (!page)
        return 0;

    return page->nonFastScrollableRects(document->frame());
}

void Internals::garbageCollectDocumentResources(ExceptionCode& ec) const
{
    Document* document = contextDocument();
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    CachedResourceLoader* cachedResourceLoader = document->cachedResourceLoader();
    if (!cachedResourceLoader)
        return;
    cachedResourceLoader->garbageCollectDocumentResources();
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
    document->styleSheetCollection().addAuthorSheet(std::move(parsedSheet));
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
    document->styleSheetCollection().addUserSheet(std::move(parsedSheet));
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

Vector<String> Internals::iconURLs(Document* document, int iconTypesMask) const
{
    Vector<IconURL> iconURLs = document->iconURLs(iconTypesMask);
    Vector<String> array;

    Vector<IconURL>::const_iterator iter(iconURLs.begin());
    for (; iter != iconURLs.end(); ++iter)
        array.append(iter->m_iconURL.string());

    return array;
}

Vector<String> Internals::shortcutIconURLs() const
{
    return iconURLs(contextDocument(), Favicon);
}

Vector<String> Internals::allIconURLs() const
{
    return iconURLs(contextDocument(), Favicon | TouchIcon | TouchPrecomposedIcon);
}

int Internals::numberOfPages(float pageWidth, float pageHeight)
{
    if (!frame())
        return -1;

    return PrintContext::numberOfPages(frame(), FloatSize(pageWidth, pageHeight));
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
    cacheStorage().storeUpdatedQuotaForOrigin(document->securityOrigin(), quota);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(scheme);
}

PassRefPtr<MallocStatistics> Internals::mallocStatistics() const
{
    return MallocStatistics::create();
}

PassRefPtr<TypeConversions> Internals::typeConversions() const
{
    return TypeConversions::create();
}

PassRefPtr<MemoryInfo> Internals::memoryInfo() const
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
    result.append("type=");
    result.append(cursorTypeToString(cursor.type()));
    result.append(" hotSpot=");
    result.appendNumber(cursor.hotSpot().x());
    result.append(",");
    result.appendNumber(cursor.hotSpot().y());
    if (cursor.image()) {
        IntSize size = cursor.image()->size();
        result.append(" image=");
        result.appendNumber(size.width());
        result.append("x");
        result.appendNumber(size.height());
    }
#if ENABLE(MOUSE_CURSOR_SCALE)
    if (cursor.imageScaleFactor() != 1) {
        result.append(" scale=");
        NumberToStringBuffer buffer;
        result.append(numberToFixedPrecisionString(cursor.imageScaleFactor(), 8, buffer, true));
    }
#endif
    return result.toString();
#else
    return "FAIL: Cursor details not available on this platform.";
#endif
}

PassRefPtr<ArrayBuffer> Internals::serializeObject(PassRefPtr<SerializedScriptValue> value) const
{
    Vector<uint8_t> bytes = value->data();
    return ArrayBuffer::create(bytes.data(), bytes.size());
}

PassRefPtr<SerializedScriptValue> Internals::deserializeBuffer(PassRefPtr<ArrayBuffer> buffer) const
{
    Vector<uint8_t> bytes;
    bytes.append(static_cast<const uint8_t*>(buffer->data()), buffer->byteLength());
    return SerializedScriptValue::adopt(bytes);
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::Settings::setUsesOverlayScrollbars(enabled);
}

void Internals::forceReload(bool endToEnd)
{
    frame()->loader().reload(endToEnd);
}

#if ENABLE(ENCRYPTED_MEDIA_V2)
void Internals::initializeMockCDM()
{
    CDM::registerCDMFactory(MockCDM::create, MockCDM::supportsKeySystem, MockCDM::supportsKeySystemAndMimeType);
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
    HTMLMediaElement* element = toHTMLMediaElement(node);
    element->player()->simulateAudioInterruption();
#else
    UNUSED_PARAM(node);
#endif
}
#endif

bool Internals::isSelectPopupVisible(Node* node)
{
    if (!isHTMLSelectElement(node))
        return false;

    HTMLSelectElement* select = toHTMLSelectElement(node);

    auto renderer = select->renderer();
    if (!renderer->isMenuList())
        return false;

#if !PLATFORM(IOS)
    RenderMenuList* menuList = toRenderMenuList(renderer);
    return menuList->popupIsVisible();
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

#if ENABLE(VIDEO_TRACK) && !PLATFORM(WIN)
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

#if ENABLE(VIDEO_TRACK) && !PLATFORM(WIN)
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

#if ENABLE(VIDEO_TRACK) && !PLATFORM(WIN)
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
    
#if ENABLE(VIDEO_TRACK) && !PLATFORM(WIN)
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
PassRefPtr<TimeRanges> Internals::createTimeRanges(Float32Array* startTimes, Float32Array* endTimes)
{
    ASSERT(startTimes && endTimes);
    ASSERT(startTimes->length() == endTimes->length());
    RefPtr<TimeRanges> ranges = TimeRanges::create();

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

PassRefPtr<ClientRect> Internals::selectionBounds(ExceptionCode& ec)
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

    auto renderer = element->renderer();
    if (!renderer || !renderer->isEmbeddedObject()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    RenderEmbeddedObject* embed = toRenderEmbeddedObject(renderer);
    return embed->isReplacementObscured();
}

#if ENABLE(MEDIA_SOURCE)
void Internals::initializeMockMediaSource()
{
#if USE(AVFOUNDATION)
    WebCore::Settings::setAVFoundationEnabled(false);
#endif
    MediaPlayerFactorySupport::callRegisterMediaEngine(MockMediaPlayerMediaSource::registerMediaEngine);
}
#endif

void Internals::beginMediaSessionInterruption()
{
    MediaSessionManager::sharedManager().beginInterruption();
}

void Internals::endMediaSessionInterruption(const String& flagsString)
{
    MediaSession::EndInterruptionFlags flags = MediaSession::NoFlags;

    if (equalIgnoringCase(flagsString, "MayResumePlaying"))
        flags = MediaSession::MayResumePlaying;
    
    MediaSessionManager::sharedManager().endInterruption(flags);
}

void Internals::applicationWillEnterForeground() const
{
    MediaSessionManager::sharedManager().applicationWillEnterForeground();
}

void Internals::applicationWillEnterBackground() const
{
    MediaSessionManager::sharedManager().applicationWillEnterBackground();
}

void Internals::setMediaSessionRestrictions(const String& mediaTypeString, const String& restrictionsString, ExceptionCode& ec)
{
    MediaSession::MediaType mediaType = MediaSession::None;
    if (equalIgnoringCase(mediaTypeString, "Video"))
        mediaType = MediaSession::Video;
    else if (equalIgnoringCase(mediaTypeString, "Audio"))
        mediaType = MediaSession::Audio;
    else if (equalIgnoringCase(mediaTypeString, "WebAudio"))
        mediaType = MediaSession::WebAudio;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    MediaSessionManager::SessionRestrictions restrictions = MediaSessionManager::sharedManager().restrictions(mediaType);
    MediaSessionManager::sharedManager().removeRestriction(mediaType, restrictions);

    restrictions = MediaSessionManager::NoRestrictions;
    
    if (equalIgnoringCase(restrictionsString, "ConcurrentPlaybackNotPermitted"))
        restrictions = MediaSessionManager::ConcurrentPlaybackNotPermitted;
    if (equalIgnoringCase(restrictionsString, "InlineVideoPlaybackRestricted"))
        restrictions += MediaSessionManager::InlineVideoPlaybackRestricted;
    if (equalIgnoringCase(restrictionsString, "MetadataPreloadingNotPermitted"))
        restrictions += MediaSessionManager::MetadataPreloadingNotPermitted;
    if (equalIgnoringCase(restrictionsString, "AutoPreloadingNotPermitted"))
        restrictions += MediaSessionManager::AutoPreloadingNotPermitted;
    if (equalIgnoringCase(restrictionsString, "BackgroundPlaybackNotPermitted"))
        restrictions += MediaSessionManager::BackgroundPlaybackNotPermitted;

    MediaSessionManager::sharedManager().addRestriction(mediaType, restrictions);
}
    
void Internals::postRemoteControlCommand(const String& commandString, ExceptionCode& ec)
{
    MediaSession::RemoteControlCommandType command;
    
    if (equalIgnoringCase(commandString, "Play"))
        command = MediaSession::PlayCommand;
    else if (equalIgnoringCase(commandString, "Pause"))
        command = MediaSession::PauseCommand;
    else if (equalIgnoringCase(commandString, "Stop"))
        command = MediaSession::StopCommand;
    else if (equalIgnoringCase(commandString, "TogglePlayPause"))
        command = MediaSession::TogglePlayPauseCommand;
    else if (equalIgnoringCase(commandString, "BeginSeekingBackward"))
        command = MediaSession::BeginSeekingBackwardCommand;
    else if (equalIgnoringCase(commandString, "EndSeekingBackward"))
        command = MediaSession::EndSeekingBackwardCommand;
    else if (equalIgnoringCase(commandString, "BeginSeekingForward"))
        command = MediaSession::BeginSeekingForwardCommand;
    else if (equalIgnoringCase(commandString, "EndSeekingForward"))
        command = MediaSession::EndSeekingForwardCommand;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    
    MediaSessionManager::sharedManager().didReceiveRemoteControlCommand(command);
}
    
}
