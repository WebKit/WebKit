/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#if ENABLE(INSPECTOR)

#include "InspectorLayerTreeAgent.h"

#include "IdentifiersFactory.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"

namespace WebCore {

namespace LayerTreeAgentState {
static const char layerTreeAgentEnabled[] = "layerTreeAgentEnabled";
};

InspectorLayerTreeAgent::InspectorLayerTreeAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, Page* page)
    : InspectorBaseAgent<InspectorLayerTreeAgent>("LayerTree", instrumentingAgents, state)
    , m_inspectedPage(page)
    , m_frontend(0)
{
}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent()
{
    reset();
}

void InspectorLayerTreeAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->layertree();
}

void InspectorLayerTreeAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorLayerTreeAgent::restore()
{
    enable(0);
}

void InspectorLayerTreeAgent::reset()
{
    m_documentLayerToIdMap.clear();
    m_idToLayer.clear();
}

void InspectorLayerTreeAgent::enable(ErrorString*)
{
    m_state->setBoolean(LayerTreeAgentState::layerTreeAgentEnabled, true);
    m_instrumentingAgents->setInspectorLayerTreeAgent(this);
}

void InspectorLayerTreeAgent::disable(ErrorString*)
{
    if (!m_state->getBoolean(LayerTreeAgentState::layerTreeAgentEnabled))
        return;
    m_state->setBoolean(LayerTreeAgentState::layerTreeAgentEnabled, false);
    m_instrumentingAgents->setInspectorLayerTreeAgent(0);
}

void InspectorLayerTreeAgent::layerTreeDidChange()
{
    m_frontend->layerTreeDidChange();
}

void InspectorLayerTreeAgent::renderLayerDestroyed(const RenderLayer* renderLayer)
{
    unbind(renderLayer);
}

void InspectorLayerTreeAgent::getLayerTree(ErrorString*, RefPtr<TypeBuilder::LayerTree::Layer>& object)
{
    object = buildObjectForRootLayer();
}

PassRefPtr<TypeBuilder::LayerTree::Layer> InspectorLayerTreeAgent::buildObjectForRootLayer()
{
    return buildObjectForLayer(m_inspectedPage->mainFrame()->contentRenderer()->compositor()->rootRenderLayer());
}

PassRefPtr<TypeBuilder::LayerTree::Layer> InspectorLayerTreeAgent::buildObjectForLayer(RenderLayer* renderLayer)
{
    bool isComposited = renderLayer->isComposited();

    // Basic set of properties.
    RefPtr<TypeBuilder::LayerTree::Layer> layerObject = TypeBuilder::LayerTree::Layer::create()
        .setLayerId(bind(renderLayer))
        .setBounds(buildObjectForIntRect(enclosingIntRect(renderLayer->localBoundingBox())))
        .setIsComposited(isComposited);

    // Optional properties for composited layers only.
    if (isComposited) {
        RenderLayerBacking* backing = renderLayer->backing();
        layerObject->setMemory(backing->backingStoreMemoryEstimate());
        layerObject->setCompositedBounds(buildObjectForIntRect(backing->compositedBounds()));
    }

    // Process children layers.
    RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> > childrenArray = TypeBuilder::Array<TypeBuilder::LayerTree::Layer>::create();

    renderLayer->updateLayerListsIfNeeded();

    // Check if we have a reflection layer.
    if (renderLayer->reflectionLayer())
        childrenArray->addItem(buildObjectForLayer(renderLayer->reflectionLayer()));

    if (renderLayer->isStackingContext()) {
        if (Vector<RenderLayer*>* negZOrderList = renderLayer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                childrenArray->addItem(buildObjectForLayer(negZOrderList->at(i)));
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = renderLayer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i)
            childrenArray->addItem(buildObjectForLayer(normalFlowList->at(i)));
    }
    
    if (renderLayer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = renderLayer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                childrenArray->addItem(buildObjectForLayer(posZOrderList->at(i)));
        }
    }

    layerObject->setChildLayers(childrenArray);
    
    return layerObject;
}

PassRefPtr<TypeBuilder::LayerTree::IntRect> InspectorLayerTreeAgent::buildObjectForIntRect(const IntRect& rect)
{
    return TypeBuilder::LayerTree::IntRect::create()
        .setX(rect.x())
        .setY(rect.y())
        .setWidth(rect.width())
        .setHeight(rect.height()).release();
}

String InspectorLayerTreeAgent::bind(const RenderLayer* layer)
{
    if (!layer)
        return "";
    String identifier = m_documentLayerToIdMap.get(layer);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_documentLayerToIdMap.set(layer, identifier);
        m_idToLayer.set(identifier, layer);
    }
    return identifier;
}

void InspectorLayerTreeAgent::unbind(const RenderLayer* layer)
{
    String identifier = m_documentLayerToIdMap.get(layer);
    if (identifier.isNull())
        return;

    m_documentLayerToIdMap.remove(layer);
    m_idToLayer.remove(identifier);
}

void InspectorLayerTreeAgent::nodeIdForLayerId(ErrorString* errorString, const String& layerId, int* resultNodeId)
{
    // Obtain the RenderLayer from the identifier provided.
    const RenderLayer* renderLayer = m_idToLayer.get(layerId);
    
    // Send an error if there is no such registered layer id.
    if (!renderLayer) {
        *errorString = "Could not find a bound layer for the provided id";
        return;
    }
    
    // Get the node id from the DOM agent and return it to the front-end.
    InspectorDOMAgent* inspectorDOMAgent = m_instrumentingAgents->inspectorDOMAgent();
    *resultNodeId = inspectorDOMAgent->pushNodePathForRenderLayerToFrontend(renderLayer);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
#endif // USE(ACCELERATED_COMPOSITING)
