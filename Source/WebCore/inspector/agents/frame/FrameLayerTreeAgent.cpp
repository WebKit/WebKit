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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLayerTreeAgent.h"

#include "Document.h"
#include "FrameDOMAgent.h"
#include "FrameDestructionObserverInlines.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include "RenderElement.h"
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameLayerTreeAgent);

FrameLayerTreeAgent::FrameLayerTreeAgent(FrameAgentContext& context)
    : InspectorAgentBase("LayerTree"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::LayerTreeFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::LayerTreeBackendDispatcher::create(Ref { context.backendDispatcher }, this))
    , m_inspectedFrame(context.inspectedFrame)
    , m_state(LayerTreeInspectionScope::Frame)
{
}

FrameLayerTreeAgent::~FrameLayerTreeAgent() = default;

void FrameLayerTreeAgent::didCreateFrontendAndBackend()
{
}

void FrameLayerTreeAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    std::ignore = disable();
}

void FrameLayerTreeAgent::reset()
{
    m_state.reset();
    m_suppressLayerChangeEvents = false;
}

Inspector::CommandResult<void> FrameLayerTreeAgent::enable()
{
    Ref agents = m_instrumentingAgents;
    if (agents->enabledFrameLayerTreeAgent() == this)
        return { };

    agents->setEnabledFrameLayerTreeAgent(this);

    return { };
}

Inspector::CommandResult<void> FrameLayerTreeAgent::disable()
{
    Ref agents = m_instrumentingAgents;
    if (agents->enabledFrameLayerTreeAgent() == this)
        agents->setEnabledFrameLayerTreeAgent(nullptr);

    reset();

    return { };
}

void FrameLayerTreeAgent::layerTreeDidChange()
{
    if (m_suppressLayerChangeEvents)
        return;

    m_suppressLayerChangeEvents = true;

    m_frontendDispatcher->layerTreeDidChange();
}

void FrameLayerTreeAgent::renderLayerDestroyed(const RenderLayer& renderLayer)
{
    m_state.renderLayerDestroyed(renderLayer);
}

void FrameLayerTreeAgent::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    m_state.pseudoElementDestroyed(pseudoElement);
}

void FrameLayerTreeAgent::frameDocumentUpdated(LocalFrame& frame)
{
    if (&frame != m_inspectedFrame.ptr())
        return;

    // RenderLayer::clearBacking doesn't report layers destroyed with the render tree.
    reset();
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>> FrameLayerTreeAgent::layersForNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    m_suppressLayerChangeEvents = false;

    Ref agents = m_instrumentingAgents;
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    RefPtr node = domAgent->nodeForId(nodeId);
    if (!node)
        return makeUnexpected("Missing node for given nodeId"_s);

    // FrameDOMAgent also binds nodes of same-process child frames, which have their own target (webkit.org/b/298980).
    if (node->document().frame() != m_inspectedFrame.ptr())
        return makeUnexpected("Node does not belong to this frame target"_s);

    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return makeUnexpected("Missing renderer of node for given nodeId"_s);

    if (!is<RenderElement>(*renderer))
        return makeUnexpected("Missing renderer of element for given nodeId"_s);

    return m_state.layersForRenderer(downcast<RenderElement>(*renderer), [this](Node* layerNode) {
        return idForNode(layerNode);
    });
}

Inspector::CommandResult<Ref<Inspector::Protocol::LayerTree::CompositingReasons>> FrameLayerTreeAgent::reasonsForCompositingLayer(const Inspector::Protocol::LayerTree::LayerId& layerId)
{
    return m_state.reasonsForCompositingLayer(layerId);
}

Inspector::CommandResult<String> FrameLayerTreeAgent::requestContent(const Inspector::Protocol::LayerTree::LayerId& layerId)
{
    return m_state.requestContent(layerId);
}

Inspector::Protocol::DOM::NodeId FrameLayerTreeAgent::idForNode(Node* node)
{
    if (!node)
        return 0;

    Ref agents = m_instrumentingAgents;
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return 0;

    if (auto nodeId = domAgent->boundNodeId(node))
        return nodeId;

    return domAgent->pushNodePathToFrontend(node);
}

} // namespace WebCore
