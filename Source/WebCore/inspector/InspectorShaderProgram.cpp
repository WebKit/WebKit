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

#include "GPUComputePipeline.h"
#include "GPURenderPipeline.h"
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
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace Inspector;

#if ENABLE(WEBGL)

Ref<InspectorShaderProgram> InspectorShaderProgram::create(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(program, inspectorCanvas));
}

InspectorShaderProgram::InspectorShaderProgram(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
    : m_identifier(makeString("program:"_s, IdentifiersFactory::createIdentifier()))
    , m_canvas(inspectorCanvas)
    , m_program(program)
{
    ASSERT(is<WebGLRenderingContextBase>(inspectorCanvas.canvasContext()));
}

#endif // ENABLE(WEBGL)

Ref<InspectorShaderProgram> InspectorShaderProgram::create(GPUComputePipeline& pipeline, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(pipeline, inspectorCanvas));
}

Ref<InspectorShaderProgram> InspectorShaderProgram::create(GPURenderPipeline& pipeline, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(pipeline, inspectorCanvas));
}

InspectorShaderProgram::InspectorShaderProgram(GPUComputePipeline& pipeline, InspectorCanvas& inspectorCanvas)
    : m_identifier(makeString("pipeline:"_s, IdentifiersFactory::createIdentifier()))
    , m_canvas(inspectorCanvas)
    , m_program(pipeline)
{
    ASSERT(inspectorCanvas.deviceContext());
}

InspectorShaderProgram::InspectorShaderProgram(GPURenderPipeline& pipeline, InspectorCanvas& inspectorCanvas)
    : m_identifier(makeString("pipeline:"_s, IdentifiersFactory::createIdentifier()))
    , m_canvas(inspectorCanvas)
    , m_program(pipeline)
{
    ASSERT(inspectorCanvas.deviceContext());
}

#if ENABLE(WEBGL)

WebGLProgram* InspectorShaderProgram::program() const
{
    auto* program = std::get_if<WeakRef<WebGLProgram>>(&m_program);
    return program ? program->ptr() : nullptr;
}

#endif // ENABLE(WEBGL)

GPUComputePipeline* InspectorShaderProgram::computePipeline() const
{
    auto* pipeline = std::get_if<WeakRef<GPUComputePipeline>>(&m_program);
    return pipeline ? pipeline->ptr() : nullptr;
}

GPURenderPipeline* InspectorShaderProgram::renderPipeline() const
{
    auto* pipeline = std::get_if<WeakRef<GPURenderPipeline>>(&m_program);
    return pipeline ? pipeline->ptr() : nullptr;
}

#if ENABLE(WEBGL)

static RefPtr<WebGLShader> shaderForType(WebGLProgram& program, Inspector::Protocol::Canvas::ShaderType shaderType)
{
    switch (shaderType) {
    case Inspector::Protocol::Canvas::ShaderType::Fragment:
        return program.fragmentShader();

    case Inspector::Protocol::Canvas::ShaderType::Vertex:
        return program.vertexShader();

    // Compute shaders are a WebGPU concept.
    case Inspector::Protocol::Canvas::ShaderType::Compute:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

#endif // ENABLE(WEBGL)

String InspectorShaderProgram::requestShaderSource(Inspector::Protocol::Canvas::ShaderType shaderType)
{
    return WTF::switchOn(m_program
#if ENABLE(WEBGL)
        , [&](const WeakRef<WebGLProgram>& weakProgram) {
            Ref program = weakProgram.get();
            RefPtr shader = shaderForType(program.get(), shaderType);
            return shader ? shader->getSource() : nullString();
        }
#endif // ENABLE(WEBGL)
        , [&](const WeakRef<GPUComputePipeline>& weakPipeline) {
            Ref pipeline = weakPipeline.get();
            return shaderType == Inspector::Protocol::Canvas::ShaderType::Compute ? pipeline->shaderSource() : nullString();
        }
        , [&](const WeakRef<GPURenderPipeline>& weakPipeline) {
            Ref pipeline = weakPipeline.get();
            switch (shaderType) {
            case Inspector::Protocol::Canvas::ShaderType::Vertex:
                return pipeline->vertexShaderSource();
            case Inspector::Protocol::Canvas::ShaderType::Fragment:
                return pipeline->fragmentShaderSource();
            case Inspector::Protocol::Canvas::ShaderType::Compute:
                return nullString();
            }

            ASSERT_NOT_REACHED();
            return nullString();
        }
    );
}

bool InspectorShaderProgram::updateShader(Inspector::Protocol::Canvas::ShaderType shaderType, const String& source)
{
    UNUSED_PARAM(shaderType);
    UNUSED_PARAM(source);
    return WTF::switchOn(m_program
#if ENABLE(WEBGL)
        , [&](const WeakRef<WebGLProgram>& weakProgram) {
            Ref program = weakProgram.get();
            RefPtr shader = shaderForType(program.get(), shaderType);
            if (!shader)
                return false;
            RefPtr context = dynamicDowncast<WebGLRenderingContextBase>(m_canvas->canvasContext());
            if (!context)
                return false;
            context->shaderSource(*shader, source);
            context->compileShader(*shader);
            auto compileStatus = context->getShaderParameter(*shader, GraphicsContextGL::COMPILE_STATUS);
            if (!std::holds_alternative<bool>(compileStatus))
                return false;
            if (std::get<bool>(compileStatus))
                context->linkProgramWithoutInvalidatingAttribLocations(program.get());
            else {
                auto errors = context->getShaderInfoLog(*shader);
                RefPtr scriptContext = m_canvas->scriptExecutionContext();
                for (auto error : StringView(errors).split('\n')) {
                    auto message = makeString("WebGL: "_s, error);
                    scriptContext->addConsoleMessage(makeUnique<ConsoleMessage>(MessageSource::Rendering, MessageType::Log, MessageLevel::Error, message));
                }
            }
            return true;
        }
#endif // ENABLE(WEBGL)
        , [](const WeakRef<GPUComputePipeline>&) { return false; }
        , [](const WeakRef<GPURenderPipeline>&) { return false; }
    );
}

bool InspectorShaderProgram::setDisabled(bool disabled)
{
    return WTF::switchOn(m_program
#if ENABLE(WEBGL)
        , [&](const WeakRef<WebGLProgram>&) {
            m_disabled = disabled;
            return true;
        }
#endif // ENABLE(WEBGL)
        , [](const WeakRef<GPUComputePipeline>&) { return false; }
        , [&](const WeakRef<GPURenderPipeline>&) {
            m_disabled = disabled;
            return true;
        }
    );
}

Ref<Inspector::Protocol::Canvas::ShaderProgram> InspectorShaderProgram::buildObjectForShaderProgram()
{
    auto programType = Inspector::Protocol::Canvas::ProgramType::Render;
    bool sharesVertexFragmentShader = false;
    String name;
    WTF::switchOn(m_program
#if ENABLE(WEBGL)
        , [](const WeakRef<WebGLProgram>&) { }
#endif // ENABLE(WEBGL)
        , [&](const WeakRef<GPUComputePipeline>& weakPipeline) {
            Ref pipeline = weakPipeline.get();

            programType = Inspector::Protocol::Canvas::ProgramType::Compute;

            name = pipeline->label();
        }
        , [&](const WeakRef<GPURenderPipeline>& weakPipeline) {
            Ref pipeline = weakPipeline.get();

            const String& fragmentShaderSource = pipeline->fragmentShaderSource();
            if (fragmentShaderSource.isNull())
                programType = Inspector::Protocol::Canvas::ProgramType::Vertex;
            else
                sharesVertexFragmentShader = pipeline->sharesVertexFragmentShader();

            name = pipeline->label();
        }
    );
    auto payload = Inspector::Protocol::Canvas::ShaderProgram::create()
        .setProgramId(m_identifier)
        .setProgramType(programType)
        .setCanvasId(m_canvas->identifier())
        .release();
    if (sharesVertexFragmentShader)
        payload->setSharesVertexFragmentShader(true);
    if (!name.isEmpty())
        payload->setName(name);
    return payload;
}

} // namespace WebCore
