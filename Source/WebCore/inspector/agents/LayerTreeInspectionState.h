/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/InlineWeakPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class LocalFrameView;
class Node;
class PseudoElement;
class RenderElement;
class RenderLayer;
class WeakPtrImplWithEventTargetData;

enum class LayerTreeInspectionScope : bool { Page, Frame };

class LayerTreeInspectionState {
    WTF_MAKE_NONCOPYABLE(LayerTreeInspectionState);
    WTF_MAKE_TZONE_ALLOCATED(LayerTreeInspectionState);
public:
    using NodeIdProvider = Function<Inspector::Protocol::DOM::NodeId(Node*)>;

    explicit LayerTreeInspectionState(LayerTreeInspectionScope);
    ~LayerTreeInspectionState();

    Ref<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>> layersForRenderer(RenderElement&, NOESCAPE const NodeIdProvider&);
    Inspector::CommandResult<Ref<Inspector::Protocol::LayerTree::CompositingReasons>> reasonsForCompositingLayer(const Inspector::Protocol::LayerTree::LayerId&);
    Inspector::CommandResult<String> requestContent(const Inspector::Protocol::LayerTree::LayerId&);

    void renderLayerDestroyed(const RenderLayer&);
    void pseudoElementDestroyed(PseudoElement&);
    void reset();

private:
    void gatherLayersUsingRenderObjectHierarchy(RenderElement&, const LocalFrameView& rootView, JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>&, NOESCAPE const NodeIdProvider&);
    void gatherLayersUsingRenderLayerHierarchy(RenderLayer*, const LocalFrameView& rootView, JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>&, NOESCAPE const NodeIdProvider&);

    Ref<Inspector::Protocol::LayerTree::Layer> buildObjectForLayer(RenderLayer*, const LocalFrameView& rootView, NOESCAPE const NodeIdProvider&);

    String bind(const RenderLayer*);
    void unbind(const RenderLayer*);
    void removeDestroyedLayers();

    String bindPseudoElement(PseudoElement*);
    void unbindPseudoElement(PseudoElement*);

    const LayerTreeInspectionScope m_scope;

    HashMap<const RenderLayer*, Inspector::Protocol::LayerTree::LayerId> m_documentLayerToIdMap;
    HashMap<Inspector::Protocol::LayerTree::LayerId, InlineWeakPtr<const RenderLayer>> m_idToLayer;

    WeakHashMap<PseudoElement, Inspector::Protocol::LayerTree::PseudoElementId, WeakPtrImplWithEventTargetData> m_pseudoElementToIdMap;
    HashMap<Inspector::Protocol::LayerTree::PseudoElementId, WeakPtr<PseudoElement, WeakPtrImplWithEventTargetData>> m_idToPseudoElement;
};

} // namespace WebCore
