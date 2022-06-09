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
#include "ContextMenuController.h"
#include "ElementInlines.h"
#include "EventHandler.h"
#include "HTMLAttachmentElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDivElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "RenderImage.h"
#include "ShadowRoot.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace ImageControlsMac {

#if ENABLE(SERVICE_CONTROLS)

static const AtomString& imageControlsElementIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-controls", AtomString::ConstructFromLiteral);
    return identifier;
}

static const AtomString& imageControlsButtonIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-controls-button", AtomString::ConstructFromLiteral);
    return identifier;
}

bool hasControls(const HTMLElement& element)
{
    auto shadowRoot = element.shadowRoot();
    if (!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent)
        return false;

    return shadowRoot->hasElementWithId(*imageControlsElementIdentifier().impl());
}

bool isImageControlsButtonElement(const Node& node)
{
    return is<Element>(node) && downcast<Element>(node).getIdAttribute() == imageControlsButtonIdentifier();
}

bool isInsideImageControls(const Node& node)
{
    RefPtr host = node.shadowHost();
    if (!is<HTMLElement>(host.get()) || !hasControls(downcast<HTMLElement>(*host)))
        return false;
    return is<Element>(node) && downcast<Element>(node).getIdAttribute() == imageControlsElementIdentifier();
}

void createImageControls(HTMLElement& element)
{
    Ref document = element.document();
    Ref shadowRoot = element.ensureUserAgentShadowRoot();
    auto controlLayer = HTMLDivElement::create(document.get());
    controlLayer->setIdAttribute(imageControlsElementIdentifier());
    shadowRoot->appendChild(controlLayer);
    
    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(imageControlsMacUserAgentStyleSheet, sizeof(imageControlsMacUserAgentStyleSheet)));
    auto style = HTMLStyleElement::create(HTMLNames::styleTag, document.get(), false);
    style->setTextContent(shadowStyle);
    shadowRoot->appendChild(WTFMove(style));
    
    auto button = HTMLButtonElement::create(HTMLNames::buttonTag, element.document(), nullptr);
    button->setIdAttribute(imageControlsButtonIdentifier());
    controlLayer->appendChild(button);
    
    if (auto* renderObject = element.renderer(); is<RenderImage>(renderObject))
        downcast<RenderImage>(*renderObject).setHasShadowControls(true);
}

static Image* imageFromImageElementNode(Node& node)
{
    auto* renderer = node.renderer();
    if (!is<RenderImage>(renderer))
        return nullptr;
    auto* image = downcast<RenderImage>(*renderer).cachedImage();
    if (!image || image->errorOccurred())
        return nullptr;
    return image->imageForRenderer(renderer);
}

bool handleEvent(HTMLElement& element, Event& event)
{
    if (event.type() != eventNames().clickEvent)
        return false;
    
    RefPtr frame = element.document().frame();
    if (!frame)
        return false;

    Page* page = element.document().page();
    if (!page)
        return false;
    
    if (!is<MouseEvent>(event))
        return false;
    
    auto& mouseEvent = downcast<MouseEvent>(event);
    if (!is<Node>(mouseEvent.target()))
        return false;
    auto& node = downcast<Node>(*mouseEvent.target());

    if (ImageControlsMac::isImageControlsButtonElement(node)) {
        auto shadowHost = node.shadowHost();
        if (!is<HTMLImageElement>(*shadowHost))
            return false;
        if (auto* image = imageFromImageElementNode(*shadowHost)) {
            HTMLImageElement& imageElement = downcast<HTMLImageElement>(*shadowHost);
            auto attachmentID = HTMLAttachmentElement::getAttachmentIdentifier(imageElement);
            page->chrome().client().handleImageServiceClick(roundedIntPoint(mouseEvent.absoluteLocation()), *image, imageElement.isContentEditable(), imageElement.renderBox()->absoluteContentQuad().enclosingBoundingBox(), attachmentID);
            event.setDefaultHandled();
            return true;
        }
    }
    return false;
}

#endif // ENABLE(SERVICE_CONTROLS)

} // namespace ImageControlsMac
} // namespace WebCore
