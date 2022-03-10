/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InspectorCanvasCallTracer.h"

#include "CSSStyleImageValue.h"
#include "CanvasBase.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "DOMMatrix2DInit.h"
#include "DOMPointInit.h"
#include "Element.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "ImageDataSettings.h"
#include "InspectorCanvasAgent.h"
#include "InspectorInstrumentation.h"
#include "InstrumentingAgents.h"
#include "OffscreenCanvas.h"
#include "Path2D.h"
#include "RecordingSwizzleType.h"
#include "WebGL2RenderingContext.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArrayObject.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/TypedArrays.h>
#include <variant>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static InspectorCanvasAgent* enabledCanvasAgent(CanvasRenderingContext& canvasRenderingContext)
{
    ASSERT(InspectorInstrumentationPublic::hasFrontends());

    auto* agents = InspectorInstrumentation::instrumentingAgents(canvasRenderingContext.canvasBase().scriptExecutionContext());
    ASSERT(agents);
    if (!agents)
        return nullptr;

    ASSERT(agents->enabledCanvasAgent());
    return agents->enabledCanvasAgent();
}

#define PROCESS_ARGUMENT_DEFINITION(ArgumentType) \
std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvasCallTracer::processArgument(CanvasRenderingContext& canvasRenderingContext, ArgumentType argument) \
{ \
    if (auto* canvasAgent = enabledCanvasAgent(canvasRenderingContext)) \
        return canvasAgent->processArgument(canvasRenderingContext, argument); \
    return std::nullopt; \
} \
// end of PROCESS_ARGUMENT_DEFINITION
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_ARGUMENT(PROCESS_ARGUMENT_DEFINITION)
#undef PROCESS_ARGUMENT_DEFINITION

void InspectorCanvasCallTracer::recordAction(CanvasRenderingContext& canvasRenderingContext, String&& name, InspectorCanvasCallTracer::ProcessedArguments&& arguments)
{
    if (auto* canvasAgent = enabledCanvasAgent(canvasRenderingContext))
        canvasAgent->recordAction(canvasRenderingContext, WTFMove(name), WTFMove(arguments));
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvasCallTracer::processArgument(const HTMLCanvasElement& canvasElement, uint32_t argument)
{
    ASSERT(canvasElement.renderingContext());
    return processArgument(*canvasElement.renderingContext(), argument);
}

void InspectorCanvasCallTracer::recordAction(const HTMLCanvasElement& canvasElement, String&& name, InspectorCanvasCallTracer::ProcessedArguments&& arguments)
{
    ASSERT(canvasElement.renderingContext());
    recordAction(*canvasElement.renderingContext(), WTFMove(name), WTFMove(arguments));
}

} // namespace WebCore
