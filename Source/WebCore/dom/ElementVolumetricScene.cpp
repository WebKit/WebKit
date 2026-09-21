/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ElementVolumetricScene.h"

#if ENABLE(CONNECTED_VOLUMETRIC_SCENE)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentPage.h"
#include "Element.h"
#include "HTMLModelElement.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindow.h"
#include "ModelPlayer.h"
#include "Page.h"

#if ENABLE(SPATIAL_PORTAL)
#include "SpatialPortalController.h"
#endif

namespace WebCore {

static RefPtr<HTMLModelElement> eligibleModelElement(Element& element)
{
    RefPtr model = dynamicDowncast<HTMLModelElement>(element);
    if (!model)
        return nullptr;
#if ENABLE(SPATIAL_PORTAL)
    // A <model> inside a portal has no player of its own; the portal is the eligible element.
    if (model->isInsidePortal())
        return nullptr;
#endif
    return model;
}

std::optional<ModelPresentationMode> ElementVolumetricScene::presentationMode(Element& element)
{
#if ENABLE(SPATIAL_PORTAL)
    if (CheckedPtr controller = element.spatialPortalController())
        return controller->presentationMode();
#endif
    if (RefPtr model = eligibleModelElement(element))
        return model->presentationMode();
    return std::nullopt;
}

void ElementVolumetricScene::setPresentationMode(Element& element, ModelPresentationMode mode)
{
#if ENABLE(SPATIAL_PORTAL)
    if (CheckedPtr controller = element.spatialPortalController()) {
        controller->setPresentationMode(mode);
        return;
    }
#endif
    if (RefPtr model = eligibleModelElement(element))
        model->setPresentationMode(mode);
}

RefPtr<ModelPlayer> ElementVolumetricScene::playerForElement(Element& element)
{
#if ENABLE(SPATIAL_PORTAL)
    if (CheckedPtr controller = element.spatialPortalController())
        return controller->liveModelPlayer();
#endif
    if (RefPtr model = eligibleModelElement(element))
        return model->liveModelPlayer();
    return nullptr;
}

bool ElementVolumetricScene::isPresentedInVolumetricScene(Element& element)
{
    return presentationMode(element) == ModelPresentationMode::Volumetric;
}

// "none" distinguishes an element that cannot host a scene from one that simply is not presenting.
String ElementVolumetricScene::presentationModeForTesting(Element& element)
{
    auto mode = presentationMode(element);
    if (!mode)
        return "none"_s;

    switch (*mode) {
    case ModelPresentationMode::Inline:
        return "inline"_s;
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    case ModelPresentationMode::Immersive:
        return "immersive"_s;
#endif
    case ModelPresentationMode::Volumetric:
        return "volumetric"_s;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

void ElementVolumetricScene::requestVolumetricScene(Element& element, DOMPromiseDeferred<void>&& promise)
{
    RefPtr page = element.document().page();
    if (!page) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Element is not associated with a page"_s });
        return;
    }

    auto mode = presentationMode(element);
    if (!mode) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Element is not a portal"_s });
        return;
    }

    switch (*mode) {
    case ModelPresentationMode::Volumetric:
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Element is already presented in a volumetric scene"_s });
        return;
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    case ModelPresentationMode::Immersive:
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Element is presented immersively"_s });
        return;
#endif
    case ModelPresentationMode::Inline:
        break;
    }

    if (RefPtr window = element.document().window(); !window || !window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Cannot request a volumetric scene without transient activation"_s });
        return;
    }

    // Before asking: the layer reconfiguration must release the page's hosting claim first.
    setPresentationMode(element, ModelPresentationMode::Volumetric);

    page->chrome().client().enterVolumetricSceneForElement(element, [protectedElement = Ref { element }, promise = WTF::move(promise)](bool success) mutable {
        if (!success) {
            setPresentationMode(protectedElement, ModelPresentationMode::Inline);
            promise.reject(Exception { ExceptionCode::InvalidStateError, "Failed to open a volumetric scene"_s });
            return;
        }
        promise.resolve();
    });
}

void ElementVolumetricScene::exitVolumetricScene(Element& element)
{
    if (!isPresentedInVolumetricScene(element))
        return;

    // The mode stays Volumetric until volumetricSceneDidClose(); see the header.
    if (RefPtr page = element.document().page())
        page->chrome().client().exitVolumetricSceneForElement(element);
}

void ElementVolumetricScene::volumetricSceneDidClose(Element& element)
{
    setPresentationMode(element, ModelPresentationMode::Inline);
}

void ElementVolumetricScene::documentVisibilityDidChange(Element& element)
{
    if (!element.document().hidden())
        return;
    exitVolumetricScene(element);
}

} // namespace WebCore

#endif // ENABLE(CONNECTED_VOLUMETRIC_SCENE)
