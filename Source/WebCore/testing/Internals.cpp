/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ComposedShadowTreeWalker.h"
#include "Cursor.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentMarker.h"
#include "DocumentMarkerController.h"
#include "Element.h"
#include "ElementShadow.h"
#include "EventHandler.h"
#include "ExceptionCode.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLContentElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HistoryItem.h"
#include "InspectorClient.h"
#include "InspectorConsoleAgent.h"
#include "InspectorController.h"
#include "InspectorCounters.h"
#include "InspectorFrontendChannel.h"
#include "InspectorFrontendClientLocal.h"
#include "InspectorInstrumentation.h"
#include "InspectorOverlay.h"
#include "InstrumentingAgents.h"
#include "InternalSettings.h"
#include "IntRect.h"
#include "Language.h"
#include "MallocStatistics.h"
#include "MockPagePopupDriver.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "ScrollingCoordinator.h"
#include "SelectRuleFeatureSet.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpellChecker.h"
#include "StyleSheetContents.h"
#include "TextIterator.h"
#include "TreeScope.h"
#include "TypeConversions.h"
#include "ViewportArguments.h"
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

#if ENABLE(PAGE_POPUP)
#include "PagePopupController.h"
#endif

#if ENABLE(TOUCH_ADJUSTMENT)
#include "WebKitPoint.h"
#endif

#if ENABLE(MOUSE_CURSOR_SCALE)
#include <wtf/dtoa.h>
#endif

#if PLATFORM(CHROMIUM)
#include "FilterOperation.h"
#include "FilterOperations.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerChromium.h"
#include "RenderLayerBacking.h"
#endif

namespace WebCore {

#if ENABLE(PAGE_POPUP)
static MockPagePopupDriver* s_pagePopupDriver = 0;
#endif

using namespace HTMLNames;

#if ENABLE(INSPECTOR)
class InspectorFrontendClientDummy : public InspectorFrontendClientLocal {
public:
    InspectorFrontendClientDummy(InspectorController*, Page*);
    virtual ~InspectorFrontendClientDummy() { }
    virtual void attachWindow() OVERRIDE { }
    virtual void detachWindow() OVERRIDE { }

    virtual String localizedStringsURL() OVERRIDE { return String(); }
    virtual String hiddenPanels() OVERRIDE { return String(); }

    virtual void bringToFront() OVERRIDE { }
    virtual void closeWindow() OVERRIDE { }

    virtual void inspectedURLChanged(const String&) OVERRIDE { }

protected:
    virtual void setAttachedWindowHeight(unsigned) OVERRIDE { }
};

InspectorFrontendClientDummy::InspectorFrontendClientDummy(InspectorController* controller, Page* page)
    : InspectorFrontendClientLocal(controller, page, adoptPtr(new InspectorFrontendClientLocal::Settings()))
{
}

class InspectorFrontendChannelDummy : public InspectorFrontendChannel {
public:
    explicit InspectorFrontendChannelDummy(Page*);
    virtual ~InspectorFrontendChannelDummy() { }
    virtual bool sendMessageToFrontend(const String& message) OVERRIDE;

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

static SpellChecker* spellchecker(Document* document)
{
    if (!document || !document->frame() || !document->frame()->editor())
        return 0;

    return document->frame()->editor()->spellChecker();
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
    TextRun::setAllowsRoundingHacks(false);
    WebCore::overrideUserPreferredLanguages(Vector<String>());
    WebCore::Settings::setUsesOverlayScrollbars(false);
#if ENABLE(PAGE_POPUP)
    delete s_pagePopupDriver;
    s_pagePopupDriver = 0;
    if (page->chrome())
        page->chrome()->client()->resetPagePopupDriver();
#endif
#if ENABLE(INSPECTOR) && ENABLE(JAVASCRIPT_DEBUGGER)
    if (page->inspectorController())
        page->inspectorController()->setProfilerEnabled(false);
#endif
}

Internals::Internals(Document* document)
    : ContextDestructionObserver(document)
{
}

Document* Internals::contextDocument() const
{
    return static_cast<Document*>(scriptExecutionContext());
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

String Internals::address(Node* node)
{
    char buf[32];
    sprintf(buf, "%p", node);

    return String(buf);
}

bool Internals::isPreloaded(Document* document, const String& url)
{
    if (!document)
        return false;

    return document->cachedResourceLoader()->isPreloaded(url);
}

PassRefPtr<Element> Internals::createContentElement(Document* document, ExceptionCode& ec)
{
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return HTMLContentElement::create(document);
}

bool Internals::isValidContentSelect(Element* insertionPoint, ExceptionCode& ec)
{
    if (!insertionPoint || !insertionPoint->isInsertionPoint()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return toInsertionPoint(insertionPoint)->isSelectValid();
}

Node* Internals::treeScopeRootNode(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return node->treeScope()->rootNode();
}

Node* Internals::parentTreeScope(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    const TreeScope* parentTreeScope = node->treeScope()->parentTreeScope();
    return parentTreeScope ? parentTreeScope->rootNode() : 0;
}

bool Internals::hasSelectorForIdInShadow(Element* host, const String& idValue, ExceptionCode& ec)
{
    if (!host || !host->shadow()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    host->shadow()->ensureSelectFeatureSetCollected();
    return host->shadow()->selectRuleFeatureSet().hasSelectorForId(idValue);
}

bool Internals::hasSelectorForClassInShadow(Element* host, const String& className, ExceptionCode& ec)
{
    if (!host || !host->shadow()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    host->shadow()->ensureSelectFeatureSetCollected();
    return host->shadow()->selectRuleFeatureSet().hasSelectorForClass(className);
}

bool Internals::hasSelectorForAttributeInShadow(Element* host, const String& attributeName, ExceptionCode& ec)
{
    if (!host || !host->shadow()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    host->shadow()->ensureSelectFeatureSetCollected();
    return host->shadow()->selectRuleFeatureSet().hasSelectorForAttribute(attributeName);
}

bool Internals::hasSelectorForPseudoClassInShadow(Element* host, const String& pseudoClass, ExceptionCode& ec)
{
    if (!host || !host->shadow()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    host->shadow()->ensureSelectFeatureSetCollected();
    const SelectRuleFeatureSet& featureSet = host->shadow()->selectRuleFeatureSet();
    if (pseudoClass == "checked")
        return featureSet.hasSelectorForChecked();
    if (pseudoClass == "enabled")
        return featureSet.hasSelectorForEnabled();
    if (pseudoClass == "disabled")
        return featureSet.hasSelectorForDisabled();
    if (pseudoClass == "indeterminate")
        return featureSet.hasSelectorForIndeterminate();
    if (pseudoClass == "link")
        return featureSet.hasSelectorForLink();
    if (pseudoClass == "target")
        return featureSet.hasSelectorForTarget();
    if (pseudoClass == "visited")
        return featureSet.hasSelectorForVisited();

    ASSERT_NOT_REACHED();
    return false;
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

    PseudoElement* pseudoElement = element->pseudoElement(pseudoId == "before" ? BEFORE : AFTER);
    if (!pseudoElement) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return frame()->animation()->pauseAnimationAtTime(pseudoElement->renderer(), animationName, pauseTime);
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

    PseudoElement* pseudoElement = element->pseudoElement(pseudoId == "before" ? BEFORE : AFTER);
    if (!pseudoElement) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return frame()->animation()->pauseTransitionAtTime(pseudoElement->renderer(), property, pauseTime);
}

bool Internals::hasShadowInsertionPoint(const Node* root, ExceptionCode& ec) const
{
    if (root && root->isShadowRoot())
        return toShadowRoot(root)->hasShadowInsertionPoint();

    ec = INVALID_ACCESS_ERR;
    return 0;
}

bool Internals::hasContentElement(const Node* root, ExceptionCode& ec) const
{
    if (root && root->isShadowRoot())
        return toShadowRoot(root)->hasContentElement();

    ec = INVALID_ACCESS_ERR;
    return 0;
}

size_t Internals::countElementShadow(const Node* root, ExceptionCode& ec) const
{
    if (!root || !root->isShadowRoot()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return toShadowRoot(root)->countElementShadow();
}

bool Internals::attached(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return node->attached();
}

Node* Internals::nextSiblingByWalker(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    ComposedShadowTreeWalker walker(node);
    walker.nextSibling();
    return walker.get();
}

Node* Internals::firstChildByWalker(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    ComposedShadowTreeWalker walker(node);
    walker.firstChild();
    return walker.get();
}

Node* Internals::lastChildByWalker(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    ComposedShadowTreeWalker walker(node);
    walker.lastChild();
    return walker.get();
}

Node* Internals::nextNodeByWalker(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    ComposedShadowTreeWalker walker(node);
    walker.next();
    return walker.get();
}

Node* Internals::previousNodeByWalker(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    ComposedShadowTreeWalker walker(node);
    walker.previous();
    return walker.get();
}

String Internals::elementRenderTreeAsText(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    String representation = externalRepresentation(element);
    if (representation.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return representation;
}

size_t Internals::numberOfScopedHTMLStyleChildren(const Node* scope, ExceptionCode& ec) const
{
    if (scope && (scope->isElementNode() || scope->isShadowRoot()))
#if ENABLE(STYLE_SCOPED)
        return scope->numberOfScopedHTMLStyleChildren();
#else
        return 0;
#endif

    ec = INVALID_ACCESS_ERR;
    return 0;
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::ensureShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    if (ElementShadow* shadow = host->shadow())
        return shadow->youngestShadowRoot();

    return ShadowRoot::create(host, ec).get();
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::createShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    return ShadowRoot::create(host, ec).get();
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::shadowRoot(Element* host, ExceptionCode& ec)
{
    // FIXME: Internals::shadowRoot() in tests should be converted to youngestShadowRoot() or oldestShadowRoot().
    // https://bugs.webkit.org/show_bug.cgi?id=78465
    return youngestShadowRoot(host, ec);
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::youngestShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    if (ElementShadow* shadow = host->shadow())
        return shadow->youngestShadowRoot();
    return 0;
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::oldestShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    if (ElementShadow* shadow = host->shadow())
        return shadow->oldestShadowRoot();
    return 0;
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::youngerShadowRoot(Node* shadow, ExceptionCode& ec)
{
    if (!shadow || !shadow->isShadowRoot()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return toShadowRoot(shadow)->youngerShadowRoot();
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::olderShadowRoot(Node* shadow, ExceptionCode& ec)
{
    if (!shadow || !shadow->isShadowRoot()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return toShadowRoot(shadow)->olderShadowRoot();
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
    case ShadowRoot::AuthorShadowRoot:
        return String("AuthorShadowRoot");
    default:
        ASSERT_NOT_REACHED();
        return String("Unknown");
    }
}

Element* Internals::includerFor(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return NodeRenderingContext(node).insertionPoint();
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
    HTMLTextFormControlElement* textControl = toTextFormControl(element);
    if (textControl && textControl->placeholderShouldBeVisible())
        return textControl->placeholderElement()->textContent();
    return String();
}

#if ENABLE(INPUT_TYPE_COLOR)
void Internals::selectColorInColorChooser(Element* element, const String& colorValue)
{
    if (!element->hasTagName(inputTag))
        return;
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;
    inputElement->selectColorInColorChooser(Color(colorValue));
}
#endif

PassRefPtr<DOMStringList> Internals::formControlStateOfPreviousHistoryItem(ExceptionCode& ec)
{
    HistoryItem* mainItem = frame()->loader()->history()->previousItem();
    if (!mainItem) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    String uniqueName = frame()->tree()->uniqueName();
    if (mainItem->target() != uniqueName && !mainItem->childItemWithTarget(uniqueName)) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    const Vector<String>& state = mainItem->target() == uniqueName ? mainItem->documentState() : mainItem->childItemWithTarget(uniqueName)->documentState();
    RefPtr<DOMStringList> stringList = DOMStringList::create();
    for (unsigned i = 0; i < state.size(); ++i)
        stringList->append(state[i]);
    return stringList.release();
}

void Internals::setFormControlStateOfPreviousHistoryItem(PassRefPtr<DOMStringList> state, ExceptionCode& ec)
{
    HistoryItem* mainItem = frame()->loader()->history()->previousItem();
    if (!state || !mainItem) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    String uniqueName = frame()->tree()->uniqueName();
    if (mainItem->target() == uniqueName)
        mainItem->setDocumentState(*state.get());
    else if (HistoryItem* subItem = mainItem->childItemWithTarget(uniqueName))
        subItem->setDocumentState(*state.get());
    else
        ec = INVALID_ACCESS_ERR;
}

void Internals::setEnableMockPagePopup(bool enabled, ExceptionCode& ec)
{
#if ENABLE(PAGE_POPUP)
    Document* document = contextDocument();
    if (!document || !document->page() || !document->page()->chrome())
        return;
    Page* page = document->page();
    if (!enabled) {
        page->chrome()->client()->resetPagePopupDriver();
        return;
    }
    if (!s_pagePopupDriver)
        s_pagePopupDriver = MockPagePopupDriver::create(page->mainFrame()).leakPtr();
    page->chrome()->client()->setPagePopupDriver(s_pagePopupDriver);
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(ec);
#endif
}

#if ENABLE(PAGE_POPUP)
PassRefPtr<PagePopupController> Internals::pagePopupController()
{
    return s_pagePopupDriver ? s_pagePopupDriver->pagePopupController() : 0;
}
#endif

PassRefPtr<ClientRect> Internals::absoluteCaretBounds(Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame() || !document->frame()->selection()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRect::create();
    }

    return ClientRect::create(document->frame()->selection()->absoluteCaretBounds());
}

PassRefPtr<ClientRect> Internals::boundingBox(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return ClientRect::create();
    }

    element->document()->updateLayoutIgnorePendingStylesheets();
    RenderObject* renderer = element->renderer();
    if (!renderer)
        return ClientRect::create();
    return ClientRect::create(renderer->absoluteBoundingBoxRectIgnoringTransforms());
}

PassRefPtr<ClientRectList> Internals::inspectorHighlightRects(Document* document, ExceptionCode& ec)
{
#if ENABLE(INSPECTOR)
    if (!document || !document->page() || !document->page()->inspectorController()) {
        ec = INVALID_ACCESS_ERR;
        return ClientRectList::create();
    }

    Highlight highlight;
    document->page()->inspectorController()->getHighlight(&highlight);
    return ClientRectList::create(highlight.quads);
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(ec);
    return ClientRectList::create();
#endif
}

#if PLATFORM(CHROMIUM)
void Internals::setBackgroundBlurOnNode(Node* node, int blurLength, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    RenderObject* renderObject = node->renderer();
    if (!renderObject) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    RenderLayer* renderLayer = renderObject->enclosingLayer();
    if (!renderLayer || !renderLayer->isComposited()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    GraphicsLayer* graphicsLayer = renderLayer->backing()->graphicsLayer();
    if (!graphicsLayer) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    FilterOperations filters;
    filters.operations().append(BlurFilterOperation::create(Length(blurLength, Fixed), FilterOperation::BLUR));
    static_cast<GraphicsLayerChromium*>(graphicsLayer)->setBackgroundFilters(filters);
}
#else
void Internals::setBackgroundBlurOnNode(Node*, int, ExceptionCode&)
{
}
#endif

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

    return node->document()->markers()->markersFor(node, markerTypes).size();
}

DocumentMarker* Internals::markerAt(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
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

    Vector<DocumentMarker*> markers = node->document()->markers()->markersFor(node, markerTypes);
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
    range->ownerDocument()->updateLayoutIgnorePendingStylesheets();
    range->ownerDocument()->markers()->addTextMatchMarker(range, isActive);
}

void Internals::setScrollViewPosition(Document* document, long x, long y, ExceptionCode& ec)
{
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

void Internals::setPagination(Document* document, const String& mode, int gap, int pageLength, ExceptionCode& ec)
{
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

String Internals::configurationForViewport(Document* document, float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight, ExceptionCode& ec)
{
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
        return static_cast<HTMLTextAreaElement*>(textField)->lastChangeWasUserEdit();

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

void Internals::scrollElementToRect(Element* element, long x, long y, long w, long h, ExceptionCode& ec)
{
    if (!element || !element->document() || !element->document()->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    FrameView* frameView = element->document()->view();
    frameView->scrollElementToRect(element, IntRect(x, y, w, h));
}

void Internals::paintControlTints(Document* document, ExceptionCode& ec)
{
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

void Internals::setDelegatesScrolling(bool enabled, Document* document, ExceptionCode& ec)
{
    // Delegate scrolling is valid only on mainframe's view.
    if (!document || !document->view() || !document->page() || document->page()->mainFrame() != document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->view()->setDelegatesScrolling(enabled);
}

#if ENABLE(TOUCH_ADJUSTMENT)
PassRefPtr<WebKitPoint> Internals::touchPositionAdjustedToBestClickableNode(long x, long y, long width, long height, Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode;
    IntPoint adjustedPoint;

    bool foundNode = document->frame()->eventHandler()->bestClickableNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    if (foundNode)
        return WebKitPoint::create(adjustedPoint.x(), adjustedPoint.y());

    return 0;
}

Node* Internals::touchNodeAdjustedToBestClickableNode(long x, long y, long width, long height, Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode;
    IntPoint adjustedPoint;
    document->frame()->eventHandler()->bestClickableNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    return targetNode;
}

PassRefPtr<WebKitPoint> Internals::touchPositionAdjustedToBestContextMenuNode(long x, long y, long width, long height, Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode = 0;
    IntPoint adjustedPoint;

    bool foundNode = document->frame()->eventHandler()->bestContextMenuNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    if (foundNode)
        return WebKitPoint::create(adjustedPoint.x(), adjustedPoint.y());

    return WebKitPoint::create(x, y);
}

Node* Internals::touchNodeAdjustedToBestContextMenuNode(long x, long y, long width, long height, Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode = 0;
    IntPoint adjustedPoint;
    document->frame()->eventHandler()->bestContextMenuNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    return targetNode;
}

PassRefPtr<ClientRect> Internals::bestZoomableAreaForTouchPoint(long x, long y, long width, long height, Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode;
    IntRect zoomableArea;
    bool foundNode = document->frame()->eventHandler()->bestZoomableAreaForTouchPoint(point, radius, zoomableArea, targetNode);
    if (foundNode)
        return ClientRect::create(zoomableArea);

    return 0;
}
#endif


int Internals::lastSpellCheckRequestSequence(Document* document, ExceptionCode& ec)
{
    SpellChecker* checker = spellchecker(document);

    if (!checker) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }

    return checker->lastRequestSequence();
}

int Internals::lastSpellCheckProcessedSequence(Document* document, ExceptionCode& ec)
{
    SpellChecker* checker = spellchecker(document);

    if (!checker) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }

    return checker->lastProcessedSequence();
}

Vector<String> Internals::userPreferredLanguages() const
{
    return WebCore::userPreferredLanguages();
}

void Internals::setUserPreferredLanguages(const Vector<String>& languages)
{
    WebCore::overrideUserPreferredLanguages(languages);
}

unsigned Internals::wheelEventHandlerCount(Document* document, ExceptionCode& ec)
{
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->wheelEventHandlerCount();
}

unsigned Internals::touchEventHandlerCount(Document* document, ExceptionCode& ec)
{
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

#if ENABLE(TOUCH_EVENT_TRACKING)
PassRefPtr<ClientRectList> Internals::touchEventTargetClientRects(Document* document, ExceptionCode& ec)
{
    if (!document || !document->view() || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    if (!document->page()->scrollingCoordinator())
        return ClientRectList::create();

    document->updateLayoutIgnorePendingStylesheets();

    Vector<IntRect> absoluteRects;
    document->page()->scrollingCoordinator()->computeAbsoluteTouchEventTargetRects(document, absoluteRects);
    Vector<FloatQuad> absoluteQuads(absoluteRects.size());

    for (size_t i = 0; i < absoluteRects.size(); ++i)
        absoluteQuads[i] = FloatQuad(absoluteRects[i]);

    return ClientRectList::create(absoluteQuads);
}
#endif

PassRefPtr<NodeList> Internals::nodesFromRect(Document* document, int x, int y, unsigned topPadding, unsigned rightPadding,
    unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, ExceptionCode& ec) const
{
    if (!document || !document->frame() || !document->frame()->view()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->nodesFromRect(x, y, topPadding, rightPadding, bottomPadding, leftPadding, ignoreClipping, allowShadowContent);
}

void Internals::emitInspectorDidBeginFrame()
{
    InspectorInstrumentation::didBeginFrame(contextDocument()->frame()->page());
}

void Internals::emitInspectorDidCancelFrame()
{
    InspectorInstrumentation::didCancelFrame(contextDocument()->frame()->page());
}

void Internals::setBatteryStatus(Document* document, const String& eventType, bool charging, double chargingTime, double dischargingTime, double level, ExceptionCode& ec)
{
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

void Internals::setNetworkInformation(Document* document, const String& eventType, double bandwidth, bool metered, ExceptionCode& ec)
{
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

void Internals::setDeviceProximity(Document* document, const String& eventType, double value, double min, double max, ExceptionCode& ec)
{
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

bool Internals::hasSpellingMarker(Document* document, int from, int length, ExceptionCode&)
{
    if (!document || !document->frame())
        return 0;

    return document->frame()->editor()->selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}
    
bool Internals::hasAutocorrectedMarker(Document* document, int from, int length, ExceptionCode&)
{
    if (!document || !document->frame())
        return 0;
    
    return document->frame()->editor()->selectionStartHasMarkerFor(DocumentMarker::Autocorrected, from, length);
}

#if ENABLE(INSPECTOR)
unsigned Internals::numberOfLiveNodes() const
{
    return InspectorCounters::counterValue(InspectorCounters::NodeCounter);
}

unsigned Internals::numberOfLiveDocuments() const
{
    return InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
}

Vector<String> Internals::consoleMessageArgumentCounts(Document* document) const
{
    InstrumentingAgents* instrumentingAgents = instrumentationForPage(document->page());
    if (!instrumentingAgents)
        return Vector<String>();
    InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent();
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

    DOMWindow* window = page->mainFrame()->document()->domWindow();
    ASSERT(window);

    m_frontendWindow = window->open(url, "", "", window, window);
    ASSERT(m_frontendWindow);

    Page* frontendPage = m_frontendWindow->document()->page();
    ASSERT(frontendPage);

    OwnPtr<InspectorFrontendClientDummy> frontendClient = adoptPtr(new InspectorFrontendClientDummy(page->inspectorController(), frontendPage));

    frontendPage->inspectorController()->setInspectorFrontendClient(frontendClient.release());

    m_frontendChannel = adoptPtr(new InspectorFrontendChannelDummy(frontendPage));

    page->inspectorController()->connectFrontend(m_frontendChannel.get());

    return m_frontendWindow;
}

void Internals::closeDummyInspectorFrontend()
{
    Page* page = contextDocument()->frame()->page();
    ASSERT(page);
    ASSERT(m_frontendWindow);

    page->inspectorController()->disconnectFrontend();

    m_frontendChannel.release();

    m_frontendWindow->close(m_frontendWindow->scriptExecutionContext());
    m_frontendWindow.release();
}

void Internals::setInspectorResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode& ec)
{
    Page* page = contextDocument()->frame()->page();
    if (!page || !page->inspectorController()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    page->inspectorController()->setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
}

void Internals::setJavaScriptProfilingEnabled(bool enabled, ExceptionCode& ec)
{
    Page* page = contextDocument()->frame()->page();
    if (!page || !page->inspectorController()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    page->inspectorController()->setProfilerEnabled(enabled);
}
#endif // ENABLE(INSPECTOR)

bool Internals::hasGrammarMarker(Document* document, int from, int length, ExceptionCode&)
{
    if (!document || !document->frame())
        return 0;

    return document->frame()->editor()->selectionStartHasMarkerFor(DocumentMarker::Grammar, from, length);
}

unsigned Internals::numberOfScrollableAreas(Document* document, ExceptionCode&)
{
    unsigned count = 0;
    Frame* frame = document->frame();
    if (frame->view()->scrollableAreas())
        count += frame->view()->scrollableAreas()->size();

    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (child->view() && child->view()->scrollableAreas())
            count += child->view()->scrollableAreas()->size();
    }

    return count;
}
    
bool Internals::isPageBoxVisible(Document* document, int pageNumber, ExceptionCode& ec)
{
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return document->isPageBoxVisible(pageNumber);
}

void Internals::suspendAnimations(Document* document, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    AnimationController* controller = document->frame()->animation();
    if (!controller)
        return;

    controller->suspendAnimations();
}

void Internals::resumeAnimations(Document* document, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    AnimationController* controller = document->frame()->animation();
    if (!controller)
        return;

    controller->resumeAnimations();
}

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

    return document->frame()->layerTreeAsText(layerTreeFlags);
}

String Internals::repaintRectsAsText(Document* document, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return document->frame()->trackedRepaintRectsAsText();
}

String Internals::scrollingStateTreeAsText(Document* document, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    Page* page = document->page();
    if (!page)
        return String();

    return page->scrollingStateTreeAsText();
}

String Internals::mainThreadScrollingReasons(Document* document, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    Page* page = document->page();
    if (!page)
        return String();

    return page->mainThreadScrollingReasonsAsText();
}

PassRefPtr<ClientRectList> Internals::nonFastScrollableRects(Document* document, ExceptionCode& ec) const
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    Page* page = document->page();
    if (!page)
        return 0;

    return page->nonFastScrollableRects(document->frame());
}

void Internals::garbageCollectDocumentResources(Document* document, ExceptionCode& ec) const
{
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

void Internals::insertAuthorCSS(Document* document, const String& css) const
{
    RefPtr<StyleSheetContents> parsedSheet = StyleSheetContents::create(document);
    parsedSheet->setIsUserStyleSheet(false);
    parsedSheet->parseString(css);
    document->styleSheetCollection()->addAuthorSheet(parsedSheet);
}

void Internals::insertUserCSS(Document* document, const String& css) const
{
    RefPtr<StyleSheetContents> parsedSheet = StyleSheetContents::create(document);
    parsedSheet->setIsUserStyleSheet(true);
    parsedSheet->parseString(css);
    document->styleSheetCollection()->addUserSheet(parsedSheet);
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

PassRefPtr<DOMStringList> Internals::iconURLs(Document* document) const
{
    Vector<IconURL> iconURLs = document->iconURLs();
    RefPtr<DOMStringList> stringList = DOMStringList::create();

    Vector<IconURL>::const_iterator iter(iconURLs.begin());
    for (; iter != iconURLs.end(); ++iter)
        stringList->append(iter->m_iconURL.string());

    return stringList.release();
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

#if ENABLE(FULLSCREEN_API)
void Internals::webkitWillEnterFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    document->webkitWillEnterFullScreenForElement(element);
}

void Internals::webkitDidEnterFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    document->webkitDidEnterFullScreenForElement(element);
}

void Internals::webkitWillExitFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    document->webkitWillExitFullScreenForElement(element);
}

void Internals::webkitDidExitFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    document->webkitDidExitFullScreenForElement(element);
}
#endif

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

PassRefPtr<DOMStringList> Internals::getReferencedFilePaths() const
{
    RefPtr<DOMStringList> stringList = DOMStringList::create();
    frame()->loader()->history()->saveDocumentAndScrollState();
    const Vector<String>& filePaths = FormController::getReferencedFilePaths(frame()->loader()->history()->currentItem()->documentState());
    for (size_t i = 0; i < filePaths.size(); ++i)
        stringList->append(filePaths[i]);
    return stringList.release();
}

void Internals::startTrackingRepaints(Document* document, ExceptionCode& ec)
{
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->setTracksRepaints(true);
}

void Internals::stopTrackingRepaints(Document* document, ExceptionCode& ec)
{
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->setTracksRepaints(false);
}

#if USE(LAZY_NATIVE_CURSOR)
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

String Internals::getCurrentCursorInfo(Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    Cursor cursor = document->frame()->eventHandler()->currentMouseCursor();

#if USE(LAZY_NATIVE_CURSOR)
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
#if USE(V8)
    String stringValue = value->toWireString();
    return ArrayBuffer::create(static_cast<const void*>(stringValue.impl()->characters()), stringValue.sizeInBytes());
#else
    Vector<uint8_t> bytes = value->data();
    return ArrayBuffer::create(bytes.data(), bytes.size());
#endif
}

PassRefPtr<SerializedScriptValue> Internals::deserializeBuffer(PassRefPtr<ArrayBuffer> buffer) const
{
#if USE(V8)
    String value(static_cast<const UChar*>(buffer->data()), buffer->byteLength() / sizeof(UChar));
    return SerializedScriptValue::createFromWire(value);
#else
    Vector<uint8_t> bytes;
    bytes.append(static_cast<const uint8_t*>(buffer->data()), buffer->byteLength());
    return SerializedScriptValue::adopt(bytes);
#endif
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::Settings::setUsesOverlayScrollbars(enabled);
}

}
