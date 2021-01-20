/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#pragma once

#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IntRect;
class Node;
class PseudoElement;
class RenderElement;
class RenderLayer;

class InspectorLayerTreeAgent final : public InspectorAgentBase, public Inspector::LayerTreeBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorLayerTreeAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorLayerTreeAgent(WebAgentContext&);
    ~InspectorLayerTreeAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // LayerTreeBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>> layersForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::LayerTree::CompositingReasons>> reasonsForCompositingLayer(const Inspector::Protocol::LayerTree::LayerId&);

    // InspectorInstrumentation
    void layerTreeDidChange();
    void renderLayerDestroyed(const RenderLayer&);
    void pseudoElementDestroyed(PseudoElement&);
    void reset();

private:
    // RenderLayer-related methods.
    String bind(const RenderLayer*);
    void unbind(const RenderLayer*);

    void gatherLayersUsingRenderObjectHierarchy(RenderElement&, JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>&);
    void gatherLayersUsingRenderLayerHierarchy(RenderLayer*, JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>&);

    Ref<Inspector::Protocol::LayerTree::Layer> buildObjectForLayer(RenderLayer*);
    Ref<Inspector::Protocol::LayerTree::IntRect> buildObjectForIntRect(const IntRect&);

    Inspector::Protocol::DOM::NodeId idForNode(Node*);

    String bindPseudoElement(PseudoElement*);
    void unbindPseudoElement(PseudoElement*);

    std::unique_ptr<Inspector::LayerTreeFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::LayerTreeBackendDispatcher> m_backendDispatcher;

    HashMap<const RenderLayer*, Inspector::Protocol::LayerTree::LayerId> m_documentLayerToIdMap;
    HashMap<Inspector::Protocol::LayerTree::LayerId, const RenderLayer*> m_idToLayer;

    HashMap<PseudoElement*, Inspector::Protocol::LayerTree::PseudoElementId> m_pseudoElementToIdMap;
    HashMap<Inspector::Protocol::LayerTree::PseudoElementId, PseudoElement*> m_idToPseudoElement;

    bool m_suppressLayerChangeEvents { false };
};

} // namespace WebCore
