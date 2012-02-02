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
#include "HTMLContentElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "InspectorController.h"
#include "InternalSettings.h"
#include "IntRect.h"
#include "Language.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "Settings.h"
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

PassRefPtr<Internals> Internals::create(Document* document)
{
    return adoptRef(new Internals(document));
}

Internals::~Internals()
{
}

Internals::Internals(Document* document)
    : FrameDestructionObserver(0)
{
    reset(document);
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

bool Internals::isValidContentSelect(Element* contentElement, ExceptionCode& ec)
{
    if (!contentElement || !contentElement->isContentElement()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    return toHTMLContentElement(contentElement)->isSelectValid();
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

size_t Internals::numberOfScopedHTMLStyleChildren(const Element* element, ExceptionCode& ec) const
{
    if (element)
#if ENABLE(STYLE_SCOPED)
        return element->numberOfScopedHTMLStyleChildren();
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

    return host->ensureShadowRoot();
}

Internals::ShadowRootIfShadowDOMEnabledOrNode* Internals::shadowRoot(Element* host, ExceptionCode& ec)
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

    observeFrame(document->frame());
    m_settings = InternalSettings::create(document->frame(), m_settings.get());
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

void Internals::setShouldDisplayTrackKind(Document* document, const String& kind, bool enabled, ExceptionCode& ec)
{
    if (!document || !document->frame() || !document->frame()->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    
#if ENABLE(VIDEO_TRACK)
    Settings* settings = document->frame()->settings();
    
    if (equalIgnoringCase(kind, "Subtitles"))
        settings->setShouldDisplaySubtitles(enabled);
    else if (equalIgnoringCase(kind, "Captions"))
        settings->setShouldDisplayCaptions(enabled);
    else if (equalIgnoringCase(kind, "TextDescriptions"))
        settings->setShouldDisplayTextDescriptions(enabled);
    else
        ec = SYNTAX_ERR;
#else
    UNUSED_PARAM(kind);
    UNUSED_PARAM(enabled);
#endif
}

bool Internals::shouldDisplayTrackKind(Document* document, const String& kind, ExceptionCode& ec)
{
    if (!document || !document->frame() || !document->frame()->settings()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }
    
#if ENABLE(VIDEO_TRACK)
    Settings* settings = document->frame()->settings();
    
    if (equalIgnoringCase(kind, "Subtitles"))
        return settings->shouldDisplaySubtitles();
    if (equalIgnoringCase(kind, "Captions"))
        return settings->shouldDisplayCaptions();
    if (equalIgnoringCase(kind, "TextDescriptions"))
        return settings->shouldDisplayTextDescriptions();

    ec = SYNTAX_ERR;
    return false;
#else
    UNUSED_PARAM(kind);
    return false;
#endif
}
    
}
