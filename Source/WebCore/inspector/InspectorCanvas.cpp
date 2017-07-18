/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
#include "InspectorCanvas.h"

#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLCanvasElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#if ENABLE(WEBGL)
#include "WebGLRenderingContext.h"
#endif
#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#if ENABLE(WEBGPU)
#include "WebGPURenderingContext.h"
#endif
#include <inspector/IdentifiersFactory.h>

using namespace Inspector;

namespace WebCore {

Ref<InspectorCanvas> InspectorCanvas::create(HTMLCanvasElement& canvas, const String& cssCanvasName)
{
    return adoptRef(*new InspectorCanvas(canvas, cssCanvasName));
}

InspectorCanvas::InspectorCanvas(HTMLCanvasElement& canvas, const String& cssCanvasName)
    : m_identifier("canvas:" + IdentifiersFactory::createIdentifier())
    , m_canvas(canvas)
    , m_cssCanvasName(cssCanvasName)
{
}

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvas::buildObjectForCanvas(InstrumentingAgents& instrumentingAgents)
{
    Document& document = m_canvas.document();
    Frame* frame = document.frame();
    CanvasRenderingContext* context = m_canvas.renderingContext();

    Inspector::Protocol::Canvas::ContextType contextType;
    if (is<CanvasRenderingContext2D>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL;
#endif
#if ENABLE(WEBGL2)
    else if (is<WebGL2RenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL2;
#endif
#if ENABLE(WEBGPU)
    else if (is<WebGPURenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGPU;
#endif
    else {
        ASSERT_NOT_REACHED();
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
    }

    auto canvas = Inspector::Protocol::Canvas::Canvas::create()
        .setCanvasId(m_identifier)
        .setFrameId(instrumentingAgents.inspectorPageAgent()->frameId(frame))
        .setContextType(contextType)
        .release();

    if (!m_cssCanvasName.isEmpty())
        canvas->setCssCanvasName(m_cssCanvasName);
    else {
        InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent();
        int nodeId = domAgent->boundNodeId(&m_canvas);
        if (!nodeId) {
            if (int documentNodeId = domAgent->boundNodeId(&m_canvas.document())) {
                ErrorString ignored;
                nodeId = domAgent->pushNodeToFrontend(ignored, documentNodeId, &m_canvas);
            }
        }

        if (nodeId)
            canvas->setNodeId(nodeId);
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        if (std::optional<WebGLContextAttributes> attributes = downcast<WebGLRenderingContextBase>(context)->getContextAttributes()) {
            canvas->setContextAttributes(Inspector::Protocol::Canvas::ContextAttributes::create()
                .setAlpha(attributes->alpha)
                .setDepth(attributes->depth)
                .setStencil(attributes->stencil)
                .setAntialias(attributes->antialias)
                .setPremultipliedAlpha(attributes->premultipliedAlpha)
                .setPreserveDrawingBuffer(attributes->preserveDrawingBuffer)
                .setFailIfMajorPerformanceCaveat(attributes->failIfMajorPerformanceCaveat)
                .release());
        }
    }
#endif

    if (size_t memoryCost = m_canvas.memoryCost())
        canvas->setMemoryCost(memoryCost);

    return canvas;
}

} // namespace WebCore

