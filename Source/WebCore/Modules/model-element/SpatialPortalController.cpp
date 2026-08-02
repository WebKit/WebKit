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
#include "SpatialPortalController.h"

#if ENABLE(SPATIAL_PORTAL)

#include "ContainerNodeInlines.h"
#include "Document.h"
#include "DocumentPage.h"
#include "Element.h"
#include "ElementInlines.h"
#include "GraphicsLayer.h"
#include "HTMLModelElement.h"
#include "IntersectionObserver.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "Model.h"
#include "ModelPlayer.h"
#include "ModelPlayerClient.h"
#include "ModelPlayerProvider.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "ResourceError.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SpatialPortalController);

class PortalModelPlayerClient final : public RefCounted<PortalModelPlayerClient>, public ModelPlayerClient {
public:
    static Ref<PortalModelPlayerClient> create(SpatialPortalController& controller)
    {
        return adoptRef(*new PortalModelPlayerClient(controller));
    }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    explicit PortalModelPlayerClient(SpatialPortalController& controller)
        : m_controller(controller)
    {
    }

    void didFinishLoading(ModelPlayer& player, NodeIdentifier nodeID) final
    {
        if (CheckedPtr controller = m_controller.get())
            controller->modelDidFinishLoading(player, nodeID);
    }

    void didFailLoading(ModelPlayer& player, NodeIdentifier nodeID, const ResourceError& error) final
    {
        if (CheckedPtr controller = m_controller.get())
            controller->modelDidFailLoading(player, nodeID, error);
    }

#if ENABLE(MODEL_PROCESS)
    void didConvertModelData(ModelPlayer&, Ref<SharedBuffer>&&, const String&) final { }
#endif

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    void didFinishEnvironmentMapLoading(ModelPlayer&, bool) final { }
#endif

    void didUnload(ModelPlayer& player) final
    {
        if (CheckedPtr controller = m_controller.get())
            controller->modelDidUnload(player);
    }

    void didUpdate(ModelPlayer& player) final
    {
        if (CheckedPtr controller = m_controller.get())
            controller->modelDidUpdate(player);
    }

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    void didUpdateEntityTransform(ModelPlayer&, NodeIdentifier, const TransformationMatrix&) final { }
#endif
#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    void didUpdateBoundingBox(ModelPlayer&, NodeIdentifier, const FloatPoint3D&, const FloatPoint3D&) final { }
#endif

    RefPtr<GraphicsLayer> graphicsLayer() const final
    {
        CheckedPtr controller = m_controller.get();
        return controller ? controller->portalGraphicsLayer() : nullptr;
    }

    bool isVisible() const final
    {
        CheckedPtr controller = m_controller.get();
        return controller && controller->isPortalVisible();
    }

    void logWarning(ModelPlayer& player, const String& warningMessage) final
    {
        if (CheckedPtr controller = m_controller.get())
            controller->logWarning(player, warningMessage);
    }

    const WeakPtr<SpatialPortalController> m_controller;
};

class PortalIntersectionObserverCallback final : public IntersectionObserverCallback {
public:
    static Ref<PortalIntersectionObserverCallback> create(Document& document, SpatialPortalController& controller)
    {
        return adoptRef(*new PortalIntersectionObserverCallback(document, controller));
    }

private:
    PortalIntersectionObserverCallback(Document& document, SpatialPortalController& controller)
        : IntersectionObserverCallback(&document)
        , m_controller(controller)
    {
    }

    bool NODELETE hasCallback() const final { return true; }

    CallbackResult<void> invoke(IntersectionObserver&, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver&) final
    {
        CheckedPtr controller = m_controller.get();
        if (!controller)
            return { };
        for (auto& entry : entries)
            controller->viewportIntersectionChanged(entry->isIntersecting());
        return { };
    }

    CallbackResult<void> invokeRethrowingException(IntersectionObserver& thisObserver, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver& observer) final
    {
        return invoke(thisObserver, entries, observer);
    }

    const WeakPtr<SpatialPortalController> m_controller;
};

SpatialPortalController::SpatialPortalController(Element& element)
    : m_portalElement(element)
{
}

SpatialPortalController::~SpatialPortalController()
{
    if (RefPtr observer = m_intersectionObserver)
        observer->disconnect();
    deleteModelPlayer();
}

unsigned SpatialPortalController::numberOfLoadedModels() const
{
    unsigned count = 0;
    for (auto& hostedModel : m_hostedModels.values()) {
        if (hostedModel.loadedModel)
            ++count;
    }
    return count;
}

HTMLModelElement* SpatialPortalController::hostedModelElement(NodeIdentifier nodeID) const
{
    auto it = m_hostedModels.find(nodeID);
    if (it == m_hostedModels.end())
        return nullptr;
    return it->value.element.get();
}

void SpatialPortalController::unregisterChildModel(HTMLModelElement& model)
{
    auto nodeID = model.nodeIdentifier();
    if (hostedModelElement(nodeID) != &model)
        return;

    unloadChildModel(nodeID);
    m_hostedModels.remove(nodeID);

    if (m_hostedModels.isEmpty())
        deleteModelPlayer();
}

void SpatialPortalController::registerChildModel(HTMLModelElement& model)
{
    m_hostedModels.ensure(model.nodeIdentifier(), [&] {
        return HostedModel { model, nullptr };
    });

    observePortalVisibility();

    model.visibilityStateChanged();
    loadChildModelIfReady(model);
}

void SpatialPortalController::childModelDidChange(HTMLModelElement& model)
{
    if (hostedModelElement(model.nodeIdentifier()) != &model)
        return;
    loadChildModelIfReady(model);
}

void SpatialPortalController::loadChildModelsIfReady()
{
    for (auto& hostedModel : copyToVector(m_hostedModels.values())) {
        if (RefPtr element = hostedModel.element.get())
            loadChildModelIfReady(*element);
    }
}

void SpatialPortalController::loadChildModelIfReady(HTMLModelElement& model)
{
    auto nodeID = model.nodeIdentifier();

    RefPtr modelData = model.model();
    if (!modelData) {
        unloadChildModel(nodeID);
        return;
    }

    if (!isPortalVisible())
        return;

    auto it = m_hostedModels.find(nodeID);
    if (it == m_hostedModels.end() || it->value.loadedModel == modelData)
        return;

    RefPtr player = ensureModelPlayer();
    if (!player)
        return;

    it->value.loadedModel = modelData;
    player->load(nodeID, *modelData, portalContentSize(), false);
}

void SpatialPortalController::observePortalVisibility()
{
    if (m_intersectionObserver)
        return;

    RefPtr element = m_portalElement.get();
    if (!element)
        return;

    Ref document = element->document();
    auto callback = PortalIntersectionObserverCallback::create(document, *this);
    IntersectionObserver::Init options { .scrollMargin = "100%"_s };
    auto observer = IntersectionObserver::create(document, WTF::move(callback), WTF::move(options));
    if (observer.hasException())
        return;

    m_intersectionObserver = observer.returnValue().ptr();
    m_intersectionObserver->observe(*element);
}

void SpatialPortalController::viewportIntersectionChanged(bool isIntersecting)
{
    if (isIntersecting == m_isIntersectingViewport)
        return;

    m_isIntersectingViewport = isIntersecting;

    if (RefPtr player = m_modelPlayer)
        player->visibilityStateDidChange();

    for (auto& hostedModel : copyToVector(m_hostedModels.values())) {
        if (RefPtr element = hostedModel.element.get())
            element->visibilityStateChanged();
    }

    if (isIntersecting)
        loadChildModelsIfReady();
}

ModelPlayer* SpatialPortalController::ensureModelPlayer()
{
    if (m_modelPlayer)
        return m_modelPlayer.get();

    RefPtr element = m_portalElement.get();
    if (!element)
        return nullptr;

    RefPtr page = element->document().page();
    if (!page)
        return nullptr;

    m_modelPlayerProvider = page->modelPlayerProvider();
    RefPtr provider = m_modelPlayerProvider.get();
    if (!provider)
        return nullptr;

    if (!m_playerClient)
        lazyInitialize(m_playerClient, PortalModelPlayerClient::create(*this));

    m_modelPlayer = provider->createModelPlayer(*m_playerClient);
    return m_modelPlayer.get();
}

void SpatialPortalController::deleteModelPlayer()
{
    if (m_modelPlayer) {
        if (RefPtr provider = m_modelPlayerProvider.get())
            provider->deleteModelPlayer(*m_modelPlayer);
        m_modelPlayer = nullptr;
    }
    for (auto& hostedModel : m_hostedModels.values())
        hostedModel.loadedModel = nullptr;
}

void SpatialPortalController::unloadChildModel(NodeIdentifier nodeID)
{
    auto it = m_hostedModels.find(nodeID);
    if (it == m_hostedModels.end() || !it->value.loadedModel)
        return;

    it->value.loadedModel = nullptr;

    if (RefPtr player = m_modelPlayer)
        player->unload(nodeID);
}

LayoutSize SpatialPortalController::portalContentSize() const
{
    RefPtr element = m_portalElement.get();
    if (!element)
        return { };
    if (CheckedPtr box = dynamicDowncast<RenderBox>(element->renderer()))
        return box->paddingBoxRect().size();
    return { };
}

void SpatialPortalController::configureGraphicsLayer(GraphicsLayer& graphicsLayer, const Color& backgroundColor)
{
    RefPtr player = ensureModelPlayer();
    if (!player)
        return;

    player->configureGraphicsLayer(graphicsLayer, {
        .model = nullptr,
        .contentSize = portalContentSize(),
        .contentOrigin = LayoutPoint(graphicsLayer.contentsRect().location()),
        .backgroundColor = backgroundColor,
        .isInteractive = false,
#if ENABLE(MODEL_ELEMENT_PORTAL)
        .hasPortal = true, // N/A
#endif
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
        .detachedForImmersive = false, // N/A
#endif
    });
}

void SpatialPortalController::sizeMayHaveChanged()
{
    if (RefPtr player = m_modelPlayer)
        player->sizeDidChange(portalContentSize());
}

void SpatialPortalController::modelDidFinishLoading(ModelPlayer&, NodeIdentifier nodeID)
{
    RefPtr child = hostedModelElement(nodeID);
    if (!child)
        return;

    child->didFinishLoadingInsidePortal();

    reconfigurePortalLayer();
}

void SpatialPortalController::modelDidFailLoading(ModelPlayer&, NodeIdentifier nodeID, const ResourceError& error)
{
    auto it = m_hostedModels.find(nodeID);
    if (it == m_hostedModels.end())
        return;

    it->value.loadedModel = nullptr;

    if (RefPtr child = it->value.element.get())
        child->didFailLoadingInsidePortal(error);
}

void SpatialPortalController::modelDidUnload(ModelPlayer& player)
{
    //  Mirrors HTMLModelElement::didUnload().
    if (m_modelPlayer != &player)
        return;

    deleteModelPlayer();
    // FIXME: rdar://148027600 Prevent infinite reloading of model.
    loadChildModelsIfReady();
}

void SpatialPortalController::modelDidUpdate(ModelPlayer&)
{
    reconfigurePortalLayer();
}

// Mirrors HTMLModelElement::logWarning().
void SpatialPortalController::logWarning(ModelPlayer& player, const String& warningMessage)
{
    ASSERT_UNUSED(player, &player == m_modelPlayer);

    RefPtr element = m_portalElement.get();
    if (!element)
        return;

    Ref document = element->document();
    document->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, warningMessage);
}

void SpatialPortalController::reconfigurePortalLayer()
{
    if (RefPtr element = m_portalElement.get()) {
        if (CheckedPtr renderer = dynamicDowncast<RenderLayerModelObject>(element->renderer()))
            renderer->contentChanged(ContentChangeType::Model);
    }
}

RefPtr<GraphicsLayer> SpatialPortalController::portalGraphicsLayer() const
{
    RefPtr element = m_portalElement.get();
    if (!element)
        return nullptr;

    CheckedPtr renderLayerModelObject = dynamicDowncast<RenderLayerModelObject>(element->renderer());
    if (!renderLayerModelObject || !renderLayerModelObject->isComposited())
        return nullptr;

    return renderLayerModelObject->layer()->backing()->graphicsLayer();
}

bool SpatialPortalController::isPortalVisible() const
{
    RefPtr element = m_portalElement.get();
    return element && !element->document().hidden() && m_isIntersectingViewport;
}

} // namespace WebCore

#endif // ENABLE(SPATIAL_PORTAL)
