/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "PageCanvasAgent.h"

#include "CSSStyleImageValue.h"
#include "CanvasBase.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext.h"
#include "DocumentPage.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "GPUCanvasContext.h"
#include "GPUDevice.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "InspectorCanvas.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageCanvasAgent);

PageCanvasAgent::PageCanvasAgent(PageAgentContext& context)
    : InspectorCanvasAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageCanvasAgent::~PageCanvasAgent() = default;

bool PageCanvasAgent::enabled() const
{
    return Ref { m_instrumentingAgents.get() }->enabledPageCanvasAgent() == this && InspectorCanvasAgent::enabled();
}

void PageCanvasAgent::internalEnable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledPageCanvasAgent(this);

    InspectorCanvasAgent::internalEnable();
}

void PageCanvasAgent::internalDisable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledPageCanvasAgent(nullptr);

    InspectorCanvasAgent::internalDisable();
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> PageCanvasAgent::requestNodes(const Inspector::Protocol::Canvas::CanvasId& canvasId)
{
    Inspector::Protocol::ErrorString errorString;

    CheckedPtr domAgent = Ref { m_instrumentingAgents.get() }->persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    for (RefPtr canvasElement : inspectorCanvas->canvasElements()) {
        // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
        auto documentNodeId = domAgent->boundNodeId(protect(canvasElement->document()).ptr());
        if (!documentNodeId)
            return makeUnexpected("Document must have been requested"_s);

        auto currentNodeId = domAgent->pushNodeToFrontend(errorString, documentNodeId, canvasElement);
        if (!currentNodeId)
            return makeUnexpected(errorString);

        nodeIds->addItem(currentNodeId);
    }

    m_pendingNodesChange.remove(*inspectorCanvas);

    return nodeIds;
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> PageCanvasAgent::requestCSSCanvasClientNodes(const Inspector::Protocol::Canvas::CanvasId& canvasId)
{
    Inspector::Protocol::ErrorString errorString;

    CheckedPtr domAgent = Ref { m_instrumentingAgents.get() }->persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    for (RefPtr cssCanvasClientNode : inspectorCanvas->cssCanvasClientNodes()) {
        // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
        if (auto documentNodeId = domAgent->boundNodeId(protect(cssCanvasClientNode->document()).ptr()))
            nodeIds->addItem(domAgent->pushNodeToFrontend(errorString, documentNodeId, cssCanvasClientNode));
    }

    m_pendingCSSCanvasClientNodesChange.remove(*inspectorCanvas);

    return nodeIds;
}

void PageCanvasAgent::frameNavigated(LocalFrame& frame)
{
    if (frame.isMainFrame()) {
        reset();
        return;
    }

    Vector<InspectorCanvas*> inspectorCanvases;
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (!inspectorCanvas->canvasContext())
            continue;
        for (RefPtr canvasElement : inspectorCanvas->canvasElements()) {
            if (canvasElement->document().frame() == &frame) {
                inspectorCanvases.append(inspectorCanvas.ptr());
                break;
            }
        }
    }
    for (RefPtr inspectorCanvas : inspectorCanvases)
        unbindCanvas(*inspectorCanvas);
}

void PageCanvasAgent::didChangeCSSCanvasClientNodes(CanvasBase& canvasBase)
{
    RefPtr context = canvasBase.renderingContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr<InspectorCanvas> inspectorCanvas;
    if (WeakPtr gpuCanvasContext = dynamicDowncast<GPUCanvasContext>(*context)) {
        WeakPtr device = gpuCanvasContext->device();
        if (!device)
            return;
        inspectorCanvas = findInspectorCanvas(*device);
    } else
        inspectorCanvas = findInspectorCanvas(*context);

    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    dispatchCSSCanvasClientNodesChanged(*inspectorCanvas);
}

void PageCanvasAgent::didChangeGPUDeviceClientNodes(GPUDevice& device)
{
    InspectorCanvasAgent::didChangeGPUDeviceClientNodes(device);

    RefPtr inspectorCanvas = findInspectorCanvas(device);
    if (!inspectorCanvas)
        return;

    dispatchCSSCanvasNamesChanged(*inspectorCanvas);
    dispatchCSSCanvasClientNodesChanged(*inspectorCanvas);
    dispatchNodesChanged(*inspectorCanvas);
}

void PageCanvasAgent::dispatchNodesChanged(InspectorCanvas& inspectorCanvas)
{
    if (!m_pendingNodesChange.add(inspectorCanvas).isNewEntry)
        return;

    m_frontendDispatcher->nodesChanged(inspectorCanvas.identifier());
}

void PageCanvasAgent::dispatchCSSCanvasClientNodesChanged(InspectorCanvas& inspectorCanvas)
{
    if (!m_pendingCSSCanvasClientNodesChange.add(inspectorCanvas).isNewEntry)
        return;

    m_frontendDispatcher->cssCanvasClientNodesChanged(inspectorCanvas.identifier());
}

void PageCanvasAgent::dispatchCSSCanvasNamesChanged(InspectorCanvas& inspectorCanvas)
{
    Ref cssCanvasNames = JSON::ArrayOf<String>::create();
    for (auto& cssCanvasName : inspectorCanvas.cssCanvasNames())
        cssCanvasNames->addItem(cssCanvasName);
    m_frontendDispatcher->cssCanvasNamesChanged(inspectorCanvas.identifier(), WTF::move(cssCanvasNames));
}

bool PageCanvasAgent::matchesCurrentContext(ScriptExecutionContext* scriptExecutionContext) const
{
    auto* document = dynamicDowncast<Document>(scriptExecutionContext);
    if (!document)
        return false;

    // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
    return document->page() == m_inspectedPage.ptr();
}

} // namespace WebCore
