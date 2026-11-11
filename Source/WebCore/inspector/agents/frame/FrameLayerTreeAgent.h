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

#pragma once

#include "InspectorWebAgentBase.h"
#include "LayerTreeInspectionState.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace Inspector {
class LayerTreeFrontendDispatcher;
}

namespace WebCore {

class LocalFrame;
class Node;
class PseudoElement;
class RenderLayer;

class FrameLayerTreeAgent final : public InspectorAgentBase, public Inspector::LayerTreeBackendDispatcherHandler, public CanMakeCheckedPtr<FrameLayerTreeAgent> {
    WTF_MAKE_NONCOPYABLE(FrameLayerTreeAgent);
    WTF_MAKE_TZONE_ALLOCATED(FrameLayerTreeAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameLayerTreeAgent);
public:
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);

    explicit FrameLayerTreeAgent(FrameAgentContext&);
    ~FrameLayerTreeAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend() override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // LayerTreeBackendDispatcherHandler
    Inspector::CommandResult<void> enable() override;
    Inspector::CommandResult<void> disable() override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>> layersForNode(Inspector::Protocol::DOM::NodeId) override;
    Inspector::CommandResult<Ref<Inspector::Protocol::LayerTree::CompositingReasons>> reasonsForCompositingLayer(const Inspector::Protocol::LayerTree::LayerId&) override;
    Inspector::CommandResult<String> requestContent(const Inspector::Protocol::LayerTree::LayerId&) override;

    // InspectorInstrumentation
    void layerTreeDidChange();
    void renderLayerDestroyed(const RenderLayer&);
    void pseudoElementDestroyed(PseudoElement&);
    void frameDocumentUpdated(LocalFrame&);

private:
    void reset();
    Inspector::Protocol::DOM::NodeId idForNode(Node*);

    const UniqueRef<Inspector::LayerTreeFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::LayerTreeBackendDispatcher> m_backendDispatcher;

    WeakRef<LocalFrame> m_inspectedFrame;

    LayerTreeInspectionState m_state;

    bool m_suppressLayerChangeEvents { false };
};

} // namespace WebCore
