/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "InspectorShaderProgram.h"

#if ENABLE(WEBGL)

#include "InspectorCanvas.h"
#include "JSExecState.h"
#include "ScriptExecutionContext.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>

namespace WebCore {

using namespace Inspector;

Ref<InspectorShaderProgram> InspectorShaderProgram::create(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(program, inspectorCanvas));
}

InspectorShaderProgram::InspectorShaderProgram(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
    : m_identifier("program:" + IdentifiersFactory::createIdentifier())
    , m_canvas(inspectorCanvas)
    , m_program(program)
{
    ASSERT(is<WebGLRenderingContextBase>(m_canvas.canvasContext()));
}

static WebGLShader* shaderForType(WebGLProgram& program, Inspector::Protocol::Canvas::ShaderType shaderType)
{
    switch (shaderType) {
    case Inspector::Protocol::Canvas::ShaderType::Fragment:
        return program.getAttachedShader(GraphicsContextGL::FRAGMENT_SHADER);

    case Inspector::Protocol::Canvas::ShaderType::Vertex:
        return program.getAttachedShader(GraphicsContextGL::VERTEX_SHADER);

    // Compute shaders are a WebGPU concept.
    case Inspector::Protocol::Canvas::ShaderType::Compute:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

String InspectorShaderProgram::requestShaderSource(Inspector::Protocol::Canvas::ShaderType shaderType)
{
    auto* shader = shaderForType(m_program, shaderType);
    if (!shader)
        return String();
    return shader->getSource();
}

bool InspectorShaderProgram::updateShader(Inspector::Protocol::Canvas::ShaderType shaderType, const String& source)
{
    auto* shader = shaderForType(m_program, shaderType);
    if (!shader)
        return false;
    auto* context = dynamicDowncast<WebGLRenderingContextBase>(m_canvas.canvasContext());
    if (!context)
        return false;
    context->shaderSource(*shader, source);
    context->compileShader(*shader);
    auto compileStatus = context->getShaderParameter(*shader, GraphicsContextGL::COMPILE_STATUS);
    if (!std::holds_alternative<bool>(compileStatus))
        return false;
    if (std::get<bool>(compileStatus))
        context->linkProgramWithoutInvalidatingAttribLocations(&m_program);
    else {
        auto errors = context->getShaderInfoLog(*shader);
        auto* scriptContext = m_canvas.scriptExecutionContext();
        for (auto error : StringView(errors).split('\n')) {
            auto message = makeString("WebGL: "_s, error);
            scriptContext->addConsoleMessage(makeUnique<ConsoleMessage>(MessageSource::Rendering, MessageType::Log, MessageLevel::Error, message));
        }
    }
    return true;
}

Ref<Inspector::Protocol::Canvas::ShaderProgram> InspectorShaderProgram::buildObjectForShaderProgram()
{
    return Inspector::Protocol::Canvas::ShaderProgram::create()
        .setProgramId(m_identifier)
        .setProgramType(Inspector::Protocol::Canvas::ProgramType::Render)
        .setCanvasId(m_canvas.identifier())
        .release();
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
