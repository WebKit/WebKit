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
#include "Element.h"
#include "ExceptionCode.h"
#include "InspectorController.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "ShadowContentElement.h"
#include "ShadowRoot.h"

namespace WebCore {

PassRefPtr<Internals> Internals::create()
{
    return adoptRef(new Internals);
}

Internals::~Internals()
{
}

Internals::Internals()
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

}
