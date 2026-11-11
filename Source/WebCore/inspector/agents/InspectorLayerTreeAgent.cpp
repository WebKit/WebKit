/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include "InspectorLayerTreeAgent.h"

#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "RenderElement.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorLayerTreeAgent);

InspectorLayerTreeAgent::InspectorLayerTreeAgent(WebAgentContext& context)
    : InspectorAgentBase("LayerTree"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::LayerTreeFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::LayerTreeBackendDispatcher::create(protect(context.backendDispatcher), this))
    , m_state(LayerTreeInspectionScope::Page)
{
}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent() = default;

void InspectorLayerTreeAgent::didCreateFrontendAndBackend()
{
}

void InspectorLayerTreeAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    std::ignore = disable();
}

void InspectorLayerTreeAgent::reset()
{
    m_state.reset();
    m_suppressLayerChangeEvents = false;
}

Inspector::Protocol::ErrorStringOr<void> InspectorLayerTreeAgent::enable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledLayerTreeAgent(this);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorLayerTreeAgent::disable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledLayerTreeAgent(nullptr);

    reset();

    return { };
}

void InspectorLayerTreeAgent::layerTreeDidChange()
{
    if (m_suppressLayerChangeEvents)
        return;

    m_suppressLayerChangeEvents = true;

    m_frontendDispatcher->layerTreeDidChange();
}

void InspectorLayerTreeAgent::renderLayerDestroyed(const RenderLayer& renderLayer)
{
    m_state.renderLayerDestroyed(renderLayer);
}

void InspectorLayerTreeAgent::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    m_state.pseudoElementDestroyed(pseudoElement);
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>> InspectorLayerTreeAgent::layersForNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    m_suppressLayerChangeEvents = false;

    Ref agents = m_instrumentingAgents.get();
    RefPtr node = CheckedPtr { agents->persistentDOMAgent() }->nodeForId(nodeId);
    if (!node)
        return makeUnexpected("Missing node for given nodeId"_s);

    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return makeUnexpected("Missing renderer of node for given nodeId"_s);

    if (!is<RenderElement>(*renderer))
        return makeUnexpected("Missing renderer of element for given nodeId"_s);

    return m_state.layersForRenderer(downcast<RenderElement>(*renderer), [this](Node* layerNode) {
        return idForNode(layerNode);
    });
}

Inspector::Protocol::DOM::NodeId InspectorLayerTreeAgent::idForNode(Node* node)
{
    if (!node)
        return 0;

    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentDOMAgent();
    
    auto nodeId = domAgent->boundNodeId(node);
    if (!nodeId) {
        // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
        nodeId = domAgent->pushNodeToFrontend(node);
    }

    return nodeId;
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::LayerTree::CompositingReasons>> InspectorLayerTreeAgent::reasonsForCompositingLayer(const Inspector::Protocol::LayerTree::LayerId& layerId)
{
    return m_state.reasonsForCompositingLayer(layerId);
}

Inspector::CommandResult<String> InspectorLayerTreeAgent::requestContent(const Inspector::Protocol::LayerTree::LayerId& layerId)
{
    return m_state.requestContent(layerId);
}

} // namespace WebCore
