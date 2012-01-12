/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "CachedResourceLoader.h"
#include "ClientRect.h"
#include "Document.h"
#include "DocumentMarker.h"
#include "DocumentMarkerController.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "InspectorController.h"
#include "IntRect.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "Settings.h"
#include "ShadowContentElement.h"
#include "ShadowRoot.h"
#include "SpellChecker.h"
#include "TextIterator.h"

#if ENABLE(GESTURE_EVENTS)
#include "PlatformGestureEvent.h"
#endif

#if ENABLE(SMOOTH_SCROLLING)
#include "ScrollAnimator.h"
#endif

#if ENABLE(INPUT_COLOR)
#include "ColorChooser.h"
#endif

namespace WebCore {

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

PassRefPtr<Internals> Internals::create()
{
    return adoptRef(new Internals);
}

Internals::~Internals()
{
}

Internals::Internals()
    : passwordEchoDurationInSecondsBackedUp(false)
    , passwordEchoEnabledBackedUp(false)
{
}

bool Internals::isPreloaded(Document* document, const String& url)
{
    if (!document)
        return false;

    return document->cachedResourceLoader()->isPreloaded(url);
}

PassRefPtr<Element> Internals::createShadowContentElement(Document* document, const String& select, ExceptionCode& ec)
{
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return ShadowContentElement::create(document, select);
}

Element* Internals::getElementByIdInShadowRoot(Node* shadowRoot, const String& id, ExceptionCode& ec)
{
    if (!shadowRoot || !shadowRoot->isShadowRoot()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    return toShadowRoot(shadowRoot)->getElementById(id);
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

Node* Internals::ensureShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return host->ensureShadowRoot();
}

Node* Internals::shadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return host->shadowRoot();
}

void Internals::removeShadowRoot(Element* host, ExceptionCode& ec)
{
    if (!host) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    host->removeShadowRoot();
}

Element* Internals::includerFor(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return NodeRenderingContext(node).includer();
}

String Internals::shadowPseudoId(Element* element, ExceptionCode& ec)
{
    if (!element) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return element->shadowPseudoId().string();
}

#if ENABLE(INPUT_COLOR)
void Internals::selectColorInColorChooser(Element* element, const String& colorValue)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return;
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;
    inputElement->selectColorInColorChooser(Color(colorValue));
}
#endif

#if ENABLE(INSPECTOR)
void Internals::setInspectorResourcesDataSizeLimits(Document* document, int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode& ec)
{
    if (!document || !document->page() || !document->page()->inspectorController()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    document->page()->inspectorController()->setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
}
#endif

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

PassRefPtr<Range> Internals::markerRangeForNode(Node* node, const String& markerType, unsigned index, ExceptionCode& ec)
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
    return Range::create(node->document(), node, markers[index]->startOffset(), node, markers[index]->endOffset());
}

void Internals::setForceCompositingMode(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setForceCompositingMode(enabled);
}

void Internals::setEnableCompositingForFixedPosition(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setAcceleratedCompositingForFixedPositionEnabled(enabled);
}

void Internals::setEnableCompositingForScrollableFrames(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setAcceleratedCompositingForScrollableFramesEnabled(enabled);
}

void Internals::setAcceleratedDrawingEnabled(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setAcceleratedDrawingEnabled(enabled);
}

void Internals::setAcceleratedFiltersEnabled(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setAcceleratedFiltersEnabled(enabled);
}

void Internals::setEnableScrollAnimator(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(SMOOTH_SCROLLING)
    document->settings()->setEnableScrollAnimator(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void Internals::setZoomAnimatorTransform(Document *document, float scale, float tx, float ty, ExceptionCode& ec)
{
    if (!document || !document->view() || !document->view()->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(GESTURE_EVENTS)
    PlatformGestureEvent pge(PlatformEvent::GestureDoubleTap, IntPoint(tx, ty), IntPoint(tx, ty), 0, scale, 0.f, 0, 0, 0, 0);
    document->view()->frame()->eventHandler()->handleGestureEvent(pge);
#else
    UNUSED_PARAM(scale);
    UNUSED_PARAM(tx);
    UNUSED_PARAM(ty);
#endif
}

void Internals::setZoomParameters(Document* document, float scale, float x, float y, ExceptionCode& ec)
{
    if (!document || !document->view() || !document->view()->frame()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

#if ENABLE(SMOOTH_SCROLLING)
    document->view()->scrollAnimator()->setZoomParametersForTest(scale, x, y);
#else
    UNUSED_PARAM(scale);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
#endif
}

void Internals::setMockScrollbarsEnabled(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setMockScrollbarsEnabled(enabled);
}

void Internals::setPasswordEchoEnabled(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!passwordEchoEnabledBackedUp) {
        passwordEchoEnabledBackup = document->settings()->passwordEchoEnabled();
        passwordEchoEnabledBackedUp = true;
    }
    document->settings()->setPasswordEchoEnabled(enabled);
}

void Internals::setPasswordEchoDurationInSeconds(Document* document, double durationInSeconds, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!passwordEchoDurationInSecondsBackedUp) {
        passwordEchoDurationInSecondsBackup = document->settings()->passwordEchoDurationInSeconds();
        passwordEchoDurationInSecondsBackedUp = true;
    }
    document->settings()->setPasswordEchoDurationInSeconds(durationInSeconds);
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

void Internals::setPagination(Document* document, const String& mode, int gap, ExceptionCode& ec)
{
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    Page::Pagination pagination;
    if (mode == "Unpaginated")
        pagination.mode = Page::Pagination::Unpaginated;
    else if (mode == "HorizontallyPaginated")
        pagination.mode = Page::Pagination::HorizontallyPaginated;
    else if (mode == "VerticallyPaginated")
        pagination.mode = Page::Pagination::VerticallyPaginated;
    else {
        ec = SYNTAX_ERR;
        return;
    }

    pagination.gap = gap;

    document->page()->setPagination(pagination);
}

void Internals::reset(Document* document)
{
    if (!document || !document->settings())
        return;

    if (passwordEchoDurationInSecondsBackedUp) {
        document->settings()->setPasswordEchoDurationInSeconds(passwordEchoDurationInSecondsBackup);
        passwordEchoDurationInSecondsBackedUp = false;
    }

    if (passwordEchoEnabledBackedUp) {
        document->settings()->setPasswordEchoEnabled(passwordEchoEnabledBackup);
        passwordEchoEnabledBackedUp = false;
    }

    if (Page* page = document->page())
        page->setPagination(Page::Pagination());
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

void Internals::setShouldLayoutFixedElementsRelativeToFrame(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->setShouldLayoutFixedElementsRelativeToFrame(enabled);
}

void Internals::setUnifiedTextCheckingEnabled(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->frame() || !document->frame()->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->frame()->settings()->setUnifiedTextCheckerEnabled(enabled);
}

bool Internals::unifiedTextCheckingEnabled(Document* document, ExceptionCode& ec)
{
    if (!document || !document->frame() || !document->frame()->settings()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return document->frame()->settings()->unifiedTextCheckerEnabled();
}

float Internals::pageScaleFactor(Document *document, ExceptionCode& ec)
{
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return document->page()->pageScaleFactor();
}

void Internals::setPageScaleFactor(Document* document, float scaleFactor, int x, int y, ExceptionCode& ec)
{
    if (!document || !document->page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    document->page()->setPageScaleFactor(scaleFactor, IntPoint(x, y));
}

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

void Internals::setPerTileDrawingEnabled(Document* document, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setPerTileDrawingEnabled(enabled);
}

}
