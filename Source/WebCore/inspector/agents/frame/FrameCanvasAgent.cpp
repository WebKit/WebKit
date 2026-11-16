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
#include "FrameCanvasAgent.h"

#include "CanvasBase.h"
#include "Document.h"
#include "FrameDOMAgent.h"
#include "FrameDestructionObserverInlines.h"
#include "GPUDevice.h"
#include "HTMLCanvasElement.h"
#include "InspectorCanvas.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameCanvasAgent);

FrameCanvasAgent::FrameCanvasAgent(FrameAgentContext& context)
    : InspectorCanvasAgent(context)
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameCanvasAgent::~FrameCanvasAgent() = default;

bool FrameCanvasAgent::enabled() const
{
    return Ref { m_instrumentingAgents.get() }->enabledFrameCanvasAgent() == this && InspectorCanvasAgent::enabled();
}

void FrameCanvasAgent::internalEnable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledFrameCanvasAgent(this);

    InspectorCanvasAgent::internalEnable();
}

void FrameCanvasAgent::internalDisable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledFrameCanvasAgent(nullptr);

    InspectorCanvasAgent::internalDisable();
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> FrameCanvasAgent::requestNodes(const Inspector::Protocol::Canvas::CanvasId& canvasId)
{
    Inspector::Protocol::ErrorString errorString;

    CheckedPtr domAgent = Ref { m_instrumentingAgents.get() }->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    for (RefPtr canvasElement : inspectorCanvas->canvasElements()) {
        if (!domAgent->boundNodeId(protect(canvasElement->document()).ptr()))
            return makeUnexpected("Document must have been requested"_s);

        auto currentNodeId = domAgent->pushNodePathToFrontend(canvasElement.get());
        if (!currentNodeId)
            return makeUnexpected("Unable to push node to frontend"_s);

        nodeIds->addItem(currentNodeId);
    }

    m_pendingNodesChange.remove(*inspectorCanvas);

    return nodeIds;
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> FrameCanvasAgent::requestCSSCanvasClientNodes(const Inspector::Protocol::Canvas::CanvasId& canvasId)
{
    Inspector::Protocol::ErrorString errorString;

    CheckedPtr domAgent = Ref { m_instrumentingAgents.get() }->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    for (Ref cssCanvasClientNode : inspectorCanvas->cssCanvasClientNodes()) {
        if (!domAgent->boundNodeId(protect(cssCanvasClientNode->document()).ptr()))
            continue;

        if (auto nodeId = domAgent->pushNodePathToFrontend(cssCanvasClientNode.ptr()))
            nodeIds->addItem(nodeId);
    }

    m_pendingCSSCanvasClientNodesChange.remove(*inspectorCanvas);

    return nodeIds;
}

void FrameCanvasAgent::frameNavigated(LocalFrame& frame)
{
    if (&frame != m_inspectedFrame.ptr())
        return;

    if (frame.isMainFrame()) {
        reset();
        return;
    }

    for (auto& inspectorCanvas : copyToVector(m_identifierToInspectorCanvas.values()))
        unbindCanvas(inspectorCanvas);
}

void FrameCanvasAgent::didChangeGPUDeviceClientNodes(GPUDevice& device)
{
    InspectorCanvasAgent::didChangeGPUDeviceClientNodes(device);

    RefPtr inspectorCanvas = findInspectorCanvas(device);
    if (!inspectorCanvas)
        return;

    dispatchCSSCanvasNamesChanged(*inspectorCanvas);
    dispatchCSSCanvasClientNodesChanged(*inspectorCanvas);
    dispatchNodesChanged(*inspectorCanvas);
}

bool FrameCanvasAgent::matchesCurrentContext(ScriptExecutionContext* scriptExecutionContext) const
{
    auto* document = dynamicDowncast<Document>(scriptExecutionContext);
    if (!document)
        return false;

    return document->frame() == m_inspectedFrame.ptr();
}

} // namespace WebCore
