/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageControlsMac.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonAtomStrings.h"
#include "ContextMenuController.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "HTMLAttachmentElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDivElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "MouseEvent.h"
#include "RenderAttachment.h"
#include "RenderImage.h"
#include "ShadowPseudoIds.h"
#include "ShadowRoot.h"
#include "TreeScopeInlines.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace ImageControlsMac {

#if ENABLE(SERVICE_CONTROLS)

static const AtomString& imageControlsElementIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-controls"_s);
    return identifier;
}

static const AtomString& imageControlsButtonIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-controls-button"_s);
    return identifier;
}

bool hasImageControls(const HTMLElement& element)
{
    auto shadowRoot = element.shadowRoot();
    if (!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent)
        return false;

    return shadowRoot->hasElementWithId(imageControlsElementIdentifier());
}

static RefPtr<HTMLElement> imageControlsHost(const Node& node)
{
    RefPtr host = dynamicDowncast<HTMLElement>(node.shadowHost());
    if (!host)
        return nullptr;

    return hasImageControls(*host) ? host : nullptr;
}

bool isImageControlsButtonElement(const Node& node)
{
    auto host = imageControlsHost(node);
    if (!host)
        return false;

    auto* element = dynamicDowncast<Element>(node);
    if (!element)
        return false;

    return element->getIdAttribute() == imageControlsButtonIdentifier();
}

bool isInsideImageControls(const Node& node)
{
    auto host = imageControlsHost(node);
    if (!host)
        return false;

    return host->userAgentShadowRoot()->contains(node);
}

void createImageControls(HTMLElement& element)
{
    Ref document = element.document();
    Ref shadowRoot = element.ensureUserAgentShadowRoot();
    auto controlLayer = HTMLDivElement::create(document.get());
    controlLayer->setIdAttribute(imageControlsElementIdentifier());
    controlLayer->setAttributeWithoutSynchronization(HTMLNames::contenteditableAttr, falseAtom());
    shadowRoot->appendChild(controlLayer);
    
    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(imageControlsMacUserAgentStyleSheet, sizeof(imageControlsMacUserAgentStyleSheet)));
    auto style = HTMLStyleElement::create(HTMLNames::styleTag, document.get(), false);
    style->setTextContent(String { shadowStyle });
    shadowRoot->appendChild(WTFMove(style));
    
    auto button = HTMLButtonElement::create(HTMLNames::buttonTag, element.document(), nullptr);
    button->setIdAttribute(imageControlsButtonIdentifier());
    controlLayer->appendChild(button);
    controlLayer->setPseudo(ShadowPseudoIds::appleAttachmentControlsContainer());
    
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(element.renderer()))
        renderImage->setHasShadowControls(true);
}

static Image* imageFromImageElementNode(Node& node)
{
    CheckedPtr renderer = dynamicDowncast<RenderImage>(node.renderer());
    if (!renderer)
        return nullptr;
    CachedResourceHandle image = renderer->cachedImage();
    if (!image || image->errorOccurred())
        return nullptr;
    return image->imageForRenderer(renderer.get());
}

bool handleEvent(HTMLElement& element, Event& event)
{
    if (event.type() != eventNames().clickEvent)
        return false;
    
    RefPtr frame = element.document().frame();
    if (!frame)
        return false;

    RefPtr page = element.document().page();
    if (!page)
        return false;
    
    RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event);
    if (!mouseEvent)
        return false;
    
    RefPtr node = dynamicDowncast<Node>(mouseEvent->target());
    if (!node)
        return false;

    if (ImageControlsMac::isImageControlsButtonElement(*node)) {
        Ref element = downcast<Element>(node.releaseNonNull());
        auto* renderer = element->renderer();
        if (!renderer)
            return false;

        RefPtr view = frame->view();
        if (!view)
            return false;

        auto point = view->contentsToWindow(renderer->absoluteBoundingBoxRect()).minXMaxYCorner();

        if (RefPtr shadowHost = dynamicDowncast<HTMLImageElement>(element->shadowHost())) {
            RefPtr image = imageFromImageElementNode(*shadowHost);
            if (!image)
                return false;
            page->chrome().client().handleImageServiceClick(point, *image, *shadowHost);
        } else if (RefPtr shadowHost = dynamicDowncast<HTMLAttachmentElement>(element->shadowHost()))
            page->chrome().client().handlePDFServiceClick(point, *shadowHost);

        event.setDefaultHandled();
        return true;
    }
    return false;
}

static bool isImageMenuEnabled(HTMLElement& element)
{
    if (RefPtr imageElement = dynamicDowncast<HTMLImageElement>(element))
        return imageElement->isImageMenuEnabled();

    if (RefPtr attachmentElement = dynamicDowncast<HTMLAttachmentElement>(element))
        return attachmentElement->isImageMenuEnabled();

    return false;
}

void updateImageControls(HTMLElement& element)
{
    // If this is an image element and it is inside a shadow tree then it is part of an image control.
    if (element.isInShadowTree())
        return;

    if (!element.document().settings().imageControlsEnabled())
        return;

    element.document().eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakElement = WeakPtr { element }] {
        RefPtr protectedElement = weakElement.get();
        if (!protectedElement)
            return;
        ASSERT(is<HTMLAttachmentElement>(*protectedElement) || is<HTMLImageElement>(*protectedElement));
        bool imageMenuEnabled = isImageMenuEnabled(*protectedElement);
        bool hasControls = hasImageControls(*protectedElement);
        if (!imageMenuEnabled && hasControls)
            destroyImageControls(*protectedElement);
        else if (imageMenuEnabled && !hasControls)
            tryCreateImageControls(*protectedElement);
    });
}

void tryCreateImageControls(HTMLElement& element)
{
    ASSERT(isImageMenuEnabled(element));
    ASSERT(!hasImageControls(element));
    createImageControls(element);
}

void destroyImageControls(HTMLElement& element)
{
    auto shadowRoot = element.userAgentShadowRoot();
    if (!shadowRoot)
        return;

    if (RefPtr node = shadowRoot->firstChild()) {
        RefPtr htmlElement = dynamicDowncast<HTMLElement>(node.releaseNonNull());
        if (!htmlElement)
            return;
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ImageControlsMac::hasImageControls(*htmlElement));
        shadowRoot->removeChild(*htmlElement);
    }

    auto* renderObject = element.renderer();
    if (!renderObject)
        return;

    if (auto* renderImage = dynamicDowncast<RenderImage>(*renderObject))
        renderImage->setHasShadowControls(false);
    else if (auto* renderAttachment = dynamicDowncast<RenderAttachment>(*renderObject))
        renderAttachment->setHasShadowControls(false);
}

#endif // ENABLE(SERVICE_CONTROLS)

} // namespace ImageControlsMac
} // namespace WebCore
