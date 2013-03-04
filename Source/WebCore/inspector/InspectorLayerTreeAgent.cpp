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
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"

namespace WebCore {

namespace LayerTreeAgentState {
static const char layerTreeAgentEnabled[] = "layerTreeAgentEnabled";
};

InspectorLayerTreeAgent::InspectorLayerTreeAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state)
    : InspectorBaseAgent<InspectorLayerTreeAgent>("LayerTree", instrumentingAgents, state)
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
    if (m_state->getBoolean(LayerTreeAgentState::layerTreeAgentEnabled))
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

PassRefPtr<TypeBuilder::LayerTree::Layer> InspectorLayerTreeAgent::buildObjectForLayer(RenderLayer* renderLayer)
{
    // Basic set of properties.
    RefPtr<TypeBuilder::LayerTree::Layer> layerObject = TypeBuilder::LayerTree::Layer::create()
        .setLayerId(bind(renderLayer))
        .setBounds(buildObjectForIntRect(enclosingIntRect(renderLayer->localBoundingBox())));

    // Optional properties for composited layers only.
    if (renderLayer->isComposited()) {
        RenderLayerBacking* backing = renderLayer->backing();
        layerObject->setMemory(backing->backingStoreMemoryEstimate());
        layerObject->setCompositedBounds(buildObjectForIntRect(backing->compositedBounds()));
        layerObject->setPaintCount(backing->graphicsLayer()->repaintCount());
        layerObject->setIsComposited(true);
    }

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

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
#endif // USE(ACCELERATED_COMPOSITING)
