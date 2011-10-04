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
#include "DocumentMarkerController.h"
#include "Element.h"
#include "ExceptionCode.h"
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

#if ENABLE(INPUT_COLOR)
#include "ColorChooser.h"
#endif

namespace WebCore {

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

PassRefPtr<Element> Internals::createShadowContentElement(Document* document, ExceptionCode& ec)
{
    if (!document) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return ShadowContentElement::create(document);
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
bool Internals::connectColorChooserClient(Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return false;
    return inputElement->connectToColorChooser();
}

void Internals::selectColorInColorChooser(const String& colorValue)
{
    ColorChooser::chooser()->colorSelected(Color(colorValue));
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
    return ClientRect::create(renderer->absoluteBoundingBoxRect());
}

unsigned Internals::markerCountForNode(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return node->document()->markers()->markersFor(node).size();
}

PassRefPtr<Range> Internals::markerRangeForNode(Node* node, unsigned index, ExceptionCode& ec)
{
    if (!node) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }
    
    Vector<DocumentMarker*> markers = node->document()->markers()->markersFor(node);
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

void Internals::setZoomAnimatorTransform(Document *document, double scale, double tx, double ty, ExceptionCode& ec)
{
    if (!document || !document->settings()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    document->settings()->setZoomAnimatorScale(static_cast<float>(scale));
    document->settings()->setZoomAnimatorPosition(static_cast<float>(tx), static_cast<float>(ty));
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

void Internals::paintControlTints(Document* document, ExceptionCode& ec)
{
    if (!document || !document->view()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    FrameView* frameView = document->view();
    frameView->paintControlTints();
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

}
