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
#include "Document.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "InspectorCanvas.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "Page.h"

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {

using namespace Inspector;

PageCanvasAgent::PageCanvasAgent(PageAgentContext& context)
    : InspectorCanvasAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageCanvasAgent::~PageCanvasAgent() = default;

bool PageCanvasAgent::enabled() const
{
    return m_instrumentingAgents.enabledPageCanvasAgent() == this && InspectorCanvasAgent::enabled();
}

void PageCanvasAgent::internalEnable()
{
    m_instrumentingAgents.setEnabledPageCanvasAgent(this);

    InspectorCanvasAgent::internalEnable();
}

void PageCanvasAgent::internalDisable()
{
    m_instrumentingAgents.setEnabledPageCanvasAgent(nullptr);

    InspectorCanvasAgent::internalDisable();
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> PageCanvasAgent::requestNode(const Protocol::Canvas::CanvasId& canvasId)
{
    Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto* node = inspectorCanvas->canvasElement();
    if (!node)
        return makeUnexpected("Missing element of canvas for given canvasId"_s);

    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
    int documentNodeId = m_instrumentingAgents.persistentDOMAgent()->boundNodeId(&node->document());
    if (!documentNodeId)
        return makeUnexpected("Document must have been requested"_s);

    return m_instrumentingAgents.persistentDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, node);
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOM::NodeId>>> PageCanvasAgent::requestClientNodes(const Protocol::Canvas::CanvasId& canvasId)
{
    Protocol::ErrorString errorString;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto clientNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
    for (auto& clientNode : inspectorCanvas->clientNodes()) {
        // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
        if (auto documentNodeId = domAgent->boundNodeId(&clientNode->document()))
            clientNodeIds->addItem(domAgent->pushNodeToFrontend(errorString, documentNodeId, clientNode));
    }
    return clientNodeIds;
}

void PageCanvasAgent::frameNavigated(LocalFrame& frame)
{
    if (frame.isMainFrame()) {
        reset();
        return;
    }

    Vector<InspectorCanvas*> inspectorCanvases;
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (auto* canvasElement = inspectorCanvas->canvasElement()) {
            if (canvasElement->document().frame() == &frame)
                inspectorCanvases.append(inspectorCanvas.get());
        }
    }
    for (auto* inspectorCanvas : inspectorCanvases)
        unbindCanvas(*inspectorCanvas);
}

void PageCanvasAgent::didChangeCSSCanvasClientNodes(CanvasBase& canvasBase)
{
    auto* context = canvasBase.renderingContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto inspectorCanvas = findInspectorCanvas(*context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->clientNodesChanged(inspectorCanvas->identifier());
}

bool PageCanvasAgent::matchesCurrentContext(ScriptExecutionContext* scriptExecutionContext) const
{
    auto* document = dynamicDowncast<Document>(scriptExecutionContext);
    if (!document)
        return false;

    // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
    return document->page() == &m_inspectedPage;
}

} // namespace WebCore
