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
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ComposedShadowTreeWalker.h"
#include "DOMStringList.h"
#include "Document.h"
#include "DocumentMarker.h"
#include "DocumentMarkerController.h"
#include "Element.h"
#include "ElementShadow.h"
#include "ExceptionCode.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLContentElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HistoryItem.h"
#include "InspectorConsoleAgent.h"
#include "InspectorController.h"
#include "InspectorCounters.h"
#include "InspectorInstrumentation.h"
#include "InspectorOverlay.h"
#include "InstrumentingAgents.h"
#include "InternalSettings.h"
#include "IntRect.h"
#include "Language.h"
#include "MallocStatistics.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "PrintContext.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpellChecker.h"
#include "TextIterator.h"
#include "TreeScope.h"
#include "ViewportArguments.h"

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

#if ENABLE(PAGE_POPUP)
#include "PagePopupController.h"
#endif

#if ENABLE(TOUCH_ADJUSTMENT)
#include "EventHandler.h"
#include "WebKitPoint.h"
#endif

#if PLATFORM(CHROMIUM)
#include "FilterOperation.h"
#include "FilterOperations.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerChromium.h"
#include "RenderLayerBacking.h"
#endif

namespace WebCore {

using namespace HTMLNames;

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

Element* Internals::getElementByIdInShadowRoot(Node* shadowRoot, const String& id, ExceptionCode& ec)
{
    if (!shadowRoot || !shadowRoot->isShadowRoot()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    return toShadowRoot(shadowRoot)->getElementById(id);
}

bool Internals::isValidContentSelect(Element* insertionPoint, ExceptionCode& ec)
{
    if (!insertionPoint || !isInsertionPoint(insertionPoint)) {
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

    return element->setShadowPseudoId(id, ec);
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

#if ENABLE(PAGE_POPUP)
PassRefPtr<PagePopupController> Internals::pagePopupController()
{
    InternalSettings* settings = this->settings();
    if (!settings)
        return 0;
    return settings->pagePopupController();
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

void Internals::setPagination(Document*, const String& mode, int gap, int pageLength, ExceptionCode& ec)
{
    settings()->setPagination(mode, gap, pageLength, ec);
}

String Internals::configurationForViewport(Document*, float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight, ExceptionCode& ec)
{
    return settings()->configurationForViewport(devicePixelRatio, deviceWidth, deviceHeight, availableWidth, availableHeight, ec);
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
    return settings()->userPreferredLanguages();
}

void Internals::setUserPreferredLanguages(const Vector<String>& languages)
{
    settings()->setUserPreferredLanguages(languages);
}

void Internals::setShouldDisplayTrackKind(Document*, const String& kind, bool enabled, ExceptionCode& ec)
{
    settings()->setShouldDisplayTrackKind(kind, enabled, ec);
}

bool Internals::shouldDisplayTrackKind(Document*, const String& kind, ExceptionCode& ec)
{
    return settings()->shouldDisplayTrackKind(kind, ec);
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

    return document->touchEventHandlerCount();
}

bool Internals::hasTouchEventListener(Document* document, ExceptionCode& ec)
{
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->hasListenerType(Document::TOUCH_LISTENER);
}


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
    settings()->allowRoundingHacks();
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

PassRefPtr<DOMStringList> Internals::getReferencedFilePaths() const
{
    RefPtr<DOMStringList> stringList = DOMStringList::create();
    frame()->loader()->history()->saveDocumentAndScrollState();
    const Vector<String>& filePaths = FormController::getReferencedFilePaths(frame()->loader()->history()->currentItem()->documentState());
    for (size_t i = 0; i < filePaths.size(); ++i)
        stringList->append(filePaths[i]);
    return stringList.release();
}

}
