/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "SpatialImageControls.h"

#include "ElementInlines.h"
#include "ElementRareData.h"
#include "Event.h"
#include "EventLoop.h"
#include "HTMLButtonElement.h"
#include "HTMLDivElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "MouseEvent.h"
#include "RenderImage.h"
#include "ShadowRoot.h"
#include "TreeScopeInlines.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace SpatialImageControls {

#if ENABLE(SPATIAL_IMAGE_CONTROLS)

static const AtomString& spatialImageControlsElementIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("spatial-image-controls"_s);
    return identifier;
}

static const AtomString& spatialImageControlsButtonIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("spatial-image-controls-button"_s);
    return identifier;
}

bool hasSpatialImageControls(const HTMLElement& element)
{
    RefPtr shadowRoot = element.shadowRoot();
    if (!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent)
        return false;

    return shadowRoot->hasElementWithId(spatialImageControlsElementIdentifier());
}

static RefPtr<HTMLElement> spatialImageControlsHost(const Node& node)
{
    RefPtr host = dynamicDowncast<HTMLElement>(node.shadowHost());
    if (!host)
        return nullptr;

    return hasSpatialImageControls(*host) ? host : nullptr;
}

bool isSpatialImageControlsButtonElement(const Element& element)
{
    return spatialImageControlsHost(element) && element.getIdAttribute() == spatialImageControlsButtonIdentifier();
}

bool shouldHaveSpatialControls(HTMLImageElement& element)
{
    if (!element.document().settings().spatialImageControlsEnabled())
        return false;

    bool hasSpatialcontrolsAttribute = element.hasAttributeWithoutSynchronization(HTMLNames::controlsAttr);

    auto* cachedImage = element.cachedImage();
    if (!cachedImage)
        return false;

    auto* image = cachedImage->image();
    if (!image)
        return false;

    return hasSpatialcontrolsAttribute && image->isSpatial();
}

void ensureSpatialControls(HTMLImageElement& imageElement)
{
    if (!shouldHaveSpatialControls(imageElement) || hasSpatialImageControls(imageElement))
        return;

    imageElement.protectedDocument()->checkedEventLoop()->queueTask(TaskSource::InternalAsyncTask, [weakElement = WeakPtr { imageElement }] {
        RefPtr element = weakElement.get();
        if (!element)
            return;
        Ref shadowRoot = element->ensureUserAgentShadowRoot();
        Ref document = element->document();

        if (hasSpatialImageControls(*element))
            return;

        Ref controlLayer = HTMLDivElement::create(document.get());
        controlLayer->setIdAttribute(spatialImageControlsElementIdentifier());
        controlLayer->setAttributeWithoutSynchronization(HTMLNames::contenteditableAttr, falseAtom());
        controlLayer->setAttributeWithoutSynchronization(HTMLNames::styleAttr, "position: relative;"_s);
        shadowRoot->appendChild(controlLayer);

        static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(spatialImageControlsUserAgentStyleSheet));
        Ref style = HTMLStyleElement::create(HTMLNames::styleTag, document.get(), false);
        style->setTextContent(String { shadowStyle });
        controlLayer->appendChild(WTFMove(style));

        Ref button = HTMLButtonElement::create(HTMLNames::buttonTag, document.get(), nullptr);
        button->setIdAttribute(spatialImageControlsButtonIdentifier());
        controlLayer->appendChild(button);

        if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(element->renderer()))
            renderImage->setHasShadowControls(true);
    });
}

bool handleEvent(HTMLElement& element, Event& event)
{
    if (!isAnyClick(event))
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

    RefPtr target = dynamicDowncast<Element>(mouseEvent->target());
    if (!target)
        return false;

    if (SpatialImageControls::isSpatialImageControlsButtonElement(*target)) {
        RefPtr img = dynamicDowncast<HTMLImageElement>(target->shadowHost());
        img->webkitRequestFullscreen();

        event.setDefaultHandled();
        return true;
    }
    return false;
}

void destroySpatialImageControls(HTMLElement& element)
{
    element.protectedDocument()->checkedEventLoop()->queueTask(TaskSource::InternalAsyncTask, [weakElement = WeakPtr { element }] {
        RefPtr protectedElement = weakElement.get();
        if (!protectedElement)
            return;
        RefPtr shadowRoot = protectedElement->userAgentShadowRoot();
        if (!shadowRoot)
            return;

        if (RefPtr element = shadowRoot->getElementById(spatialImageControlsElementIdentifier()))
            element->remove();

        auto* renderObject = protectedElement->renderer();
        if (!renderObject)
            return;

        if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(*renderObject))
            renderImage->setHasShadowControls(false);
    });
}

void updateSpatialImageControls(HTMLImageElement& element)
{
    if (!element.document().settings().spatialImageControlsEnabled())
        return;

    bool shouldHaveControls = shouldHaveSpatialControls(element);
    bool hasControls = hasSpatialImageControls(element);

    if (shouldHaveControls && !hasControls)
        ensureSpatialControls(element);
    else if (!shouldHaveControls && hasControls)
        destroySpatialImageControls(element);
}

#endif

} // namespace SpatialImageControls
} // namespace WebCore
