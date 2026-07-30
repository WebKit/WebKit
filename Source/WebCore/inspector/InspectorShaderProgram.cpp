/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "GPUShaderModule.h"
#include "InspectorCanvas.h"
#include "JSExecState.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/text/MakeString.h>

#if ENABLE(WEBGL)
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#endif

namespace WebCore {

using namespace Inspector;

#if ENABLE(WEBGL)
Ref<InspectorShaderProgram> InspectorShaderProgram::create(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(Backing { WeakRef { program } }, inspectorCanvas));
}
#endif

Ref<InspectorShaderProgram> InspectorShaderProgram::create(GPURenderPipeline& pipeline, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(Backing { WeakRef { pipeline } }, inspectorCanvas));
}

Ref<InspectorShaderProgram> InspectorShaderProgram::create(GPUComputePipeline& pipeline, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(Backing { WeakRef { pipeline } }, inspectorCanvas));
}

InspectorShaderProgram::InspectorShaderProgram(Backing&& backing, InspectorCanvas& inspectorCanvas)
    : m_identifier(makeString("program:"_s, IdentifiersFactory::createIdentifier()))
    , m_canvas(inspectorCanvas)
    , m_backing(WTF::move(backing))
{
}

#if ENABLE(WEBGL)
WebGLProgram* InspectorShaderProgram::program() const
{
    auto* backing = std::get_if<WeakRef<WebGLProgram>>(&m_backing);
    return backing ? backing->ptr() : nullptr;
}
#endif

GPURenderPipeline* InspectorShaderProgram::renderPipeline() const
{
    auto* backing = std::get_if<WeakRef<GPURenderPipeline>>(&m_backing);
    return backing ? backing->ptr() : nullptr;
}

GPUComputePipeline* InspectorShaderProgram::computePipeline() const
{
    auto* backing = std::get_if<WeakRef<GPUComputePipeline>>(&m_backing);
    return backing ? backing->ptr() : nullptr;
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
#endif

static String wgslSourceForType(GPURenderPipeline& pipeline, Inspector::Protocol::Canvas::ShaderType shaderType)
{
    switch (shaderType) {
    case Inspector::Protocol::Canvas::ShaderType::Vertex:
        return pipeline.vertexModule().wgslSource();

    case Inspector::Protocol::Canvas::ShaderType::Fragment:
        if (RefPtr fragmentModule = pipeline.fragmentModule())
            return fragmentModule->wgslSource();
        return String();

    case Inspector::Protocol::Canvas::ShaderType::Compute:
        return String();
    }

    ASSERT_NOT_REACHED();
    return String();
}

String InspectorShaderProgram::requestShaderSource(Inspector::Protocol::Canvas::ShaderType shaderType)
{
    return WTF::switchOn(m_backing,
        [&](const WeakRef<GPURenderPipeline>& pipeline) -> String {
            RefPtr protectedPipeline = pipeline.get();
            if (!protectedPipeline)
                return String();
            return wgslSourceForType(*protectedPipeline, shaderType);
        },
        [&](const WeakRef<GPUComputePipeline>& pipeline) -> String {
            RefPtr protectedPipeline = pipeline.get();
            if (!protectedPipeline || shaderType != Inspector::Protocol::Canvas::ShaderType::Compute)
                return String();
            return protectedPipeline->computeModule().wgslSource();
        }
#if ENABLE(WEBGL)
        , [&](const WeakRef<WebGLProgram>& program) -> String {
            RefPtr protectedProgram = program.get();
            if (!protectedProgram)
                return String();
            RefPtr shader = shaderForType(*protectedProgram, shaderType);
            if (!shader)
                return String();
            return shader->getSource();
        }
#endif
    );
}

bool InspectorShaderProgram::updateShader(Inspector::Protocol::Canvas::ShaderType shaderType, const String& source)
{
#if ENABLE(WEBGL)
    RefPtr program = this->program();
    if (!program)
        return false;
    RefPtr shader = shaderForType(*program, shaderType);
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
        context->linkProgramWithoutInvalidatingAttribLocations(*program);
    else {
        auto errors = context->getShaderInfoLog(*shader);
        RefPtr scriptContext = m_canvas->scriptExecutionContext();
        for (auto error : StringView(errors).split('\n')) {
            auto message = makeString("WebGL: "_s, error);
            scriptContext->addConsoleMessage(makeUnique<ConsoleMessage>(MessageSource::Rendering, MessageType::Log, MessageLevel::Error, message));
        }
    }
    return true;
#else
    UNUSED_PARAM(shaderType);
    UNUSED_PARAM(source);
    return false;
#endif
}

Ref<Inspector::Protocol::Canvas::ShaderProgram> InspectorShaderProgram::buildObjectForShaderProgram()
{
    auto programType = Inspector::Protocol::Canvas::ProgramType::Render;
    if (computePipeline())
        programType = Inspector::Protocol::Canvas::ProgramType::Compute;

    auto payload = Inspector::Protocol::Canvas::ShaderProgram::create()
        .setProgramId(m_identifier)
        .setProgramType(programType)
        .setCanvasId(m_canvas->identifier())
        .release();

    if (RefPtr pipeline = renderPipeline(); pipeline && pipeline->sharesVertexFragmentModule())
        payload->setSharesVertexFragmentShader(true);

    return payload;
}

} // namespace WebCore
