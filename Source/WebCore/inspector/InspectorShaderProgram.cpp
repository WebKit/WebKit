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
#include "InspectorShaderProgram.h"

#if ENABLE(WEBGL) || ENABLE(WEBGPU)

#include "InspectorCanvas.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "GraphicsContextGLOpenGL.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLShader.h"
#endif

#if ENABLE(WEBGPU)
#include "GPUShaderModule.h"
#include "WHLSLPrepare.h"
#include "WebGPUComputePipeline.h"
#include "WebGPUPipeline.h"
#include "WebGPURenderPipeline.h"
#include "WebGPUShaderModule.h"
#endif

namespace WebCore {

using namespace Inspector;

#if ENABLE(WEBGL)
Ref<InspectorShaderProgram> InspectorShaderProgram::create(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(program, inspectorCanvas));
}
#endif

#if ENABLE(WEBGPU)
Ref<InspectorShaderProgram> InspectorShaderProgram::create(WebGPUPipeline& pipeline, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(pipeline, inspectorCanvas));
}
#endif

#if ENABLE(WEBGL)
InspectorShaderProgram::InspectorShaderProgram(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
    : m_identifier("program:" + IdentifiersFactory::createIdentifier())
    , m_canvas(inspectorCanvas)
    , m_program(program)
{
    ASSERT(is<WebGLRenderingContextBase>(m_canvas.canvasContext()));
}
#endif

#if ENABLE(WEBGPU)
InspectorShaderProgram::InspectorShaderProgram(WebGPUPipeline& pipeline, InspectorCanvas& inspectorCanvas)
    : m_identifier("pipeline:" + IdentifiersFactory::createIdentifier())
    , m_canvas(inspectorCanvas)
    , m_program(pipeline)
{
    ASSERT(m_canvas.deviceContext());
}
#endif

#if ENABLE(WEBGL)
WebGLProgram* InspectorShaderProgram::program() const
{
    if (auto* programWrapper = WTF::get_if<std::reference_wrapper<WebGLProgram>>(m_program))
        return &programWrapper->get();
    return nullptr;
}
#endif

#if ENABLE(WEBGPU)
WebGPUPipeline* InspectorShaderProgram::pipeline() const
{
    if (auto* pipelineWrapper = WTF::get_if<std::reference_wrapper<WebGPUPipeline>>(m_program))
        return &pipelineWrapper->get();
    return nullptr;
}
#endif

#if ENABLE(WEBGL)
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
#endif

#if ENABLE(WEBGPU)
static RefPtr<WebGPUShaderModule> shaderForType(WebGPUPipeline& pipeline, Inspector::Protocol::Canvas::ShaderType shaderType)
{
    switch (shaderType) {
    case Inspector::Protocol::Canvas::ShaderType::Compute:
        if (is<WebGPUComputePipeline>(pipeline))
            return downcast<WebGPUComputePipeline>(pipeline).computeShader();
        return nullptr;

    case Inspector::Protocol::Canvas::ShaderType::Fragment:
        if (is<WebGPURenderPipeline>(pipeline))
            return downcast<WebGPURenderPipeline>(pipeline).fragmentShader();
        return nullptr;

    case Inspector::Protocol::Canvas::ShaderType::Vertex:
        if (is<WebGPURenderPipeline>(pipeline))
            return downcast<WebGPURenderPipeline>(pipeline).vertexShader();
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

String InspectorShaderProgram::requestShaderSource(Inspector::Protocol::Canvas::ShaderType shaderType)
{
    return WTF::switchOn(m_program,
#if ENABLE(WEBGL)
        [&] (std::reference_wrapper<WebGLProgram> programWrapper) {
            auto& program = programWrapper.get();
            if (auto* shader = shaderForType(program, shaderType))
                return shader->getSource();
            return String();
        },
#endif
#if ENABLE(WEBGPU)
        [&] (std::reference_wrapper<WebGPUPipeline> pipelineWrapper) {
            auto& pipeline = pipelineWrapper.get();
            if (auto shader = shaderForType(pipeline, shaderType))
                return shader->source();
            return String();
        },
#endif
        [&] (Monostate) {
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
            ASSERT_NOT_REACHED();
#endif
            return String();
        }
    );
}

bool InspectorShaderProgram::updateShader(Inspector::Protocol::Canvas::ShaderType shaderType, const String& source)
{
    return WTF::switchOn(m_program,
#if ENABLE(WEBGL)
        [&] (std::reference_wrapper<WebGLProgram> programWrapper) {
            auto& program = programWrapper.get();
            if (auto* shader = shaderForType(program, shaderType)) {
                if (auto* context = m_canvas.canvasContext()) {
                    if (is<WebGLRenderingContextBase>(context)) {
                        auto& contextWebGLBase = downcast<WebGLRenderingContextBase>(*context);
                        contextWebGLBase.shaderSource(*shader, source);
                        contextWebGLBase.compileShader(*shader);
                        if (shader->isValid()) {
                            contextWebGLBase.linkProgramWithoutInvalidatingAttribLocations(&program);
                            return true;
                        }
                    }
                }
            }
            return false;
        },
#endif
#if ENABLE(WEBGPU)
        [&] (std::reference_wrapper<WebGPUPipeline> pipelineWrapper) {
            auto& pipeline = pipelineWrapper.get();
            if (auto* device = m_canvas.deviceContext()) {
                if (pipeline.cloneShaderModules(*device)) {
                    if (auto shader = shaderForType(pipeline, shaderType)) {
                        shader->update(*device, source);
                        if (pipeline.recompile(*device))
                            return true;
                    }
                }
            }
            return false;
        },
#endif
        [&] (Monostate) {
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
            ASSERT_NOT_REACHED();
#endif
            return false;
        }
    );
}

Ref<Inspector::Protocol::Canvas::ShaderProgram> InspectorShaderProgram::buildObjectForShaderProgram()
{
    bool sharesVertexFragmentShader = false;

    using ProgramTypeType = Optional<Inspector::Protocol::Canvas::ProgramType>;
    auto programType = WTF::switchOn(m_program,
#if ENABLE(WEBGL)
        [&] (std::reference_wrapper<WebGLProgram>) -> ProgramTypeType {
            return Inspector::Protocol::Canvas::ProgramType::Render;
        },
#endif
#if ENABLE(WEBGPU)
        [&] (std::reference_wrapper<WebGPUPipeline> pipelineWrapper) -> ProgramTypeType {
            auto& pipeline = pipelineWrapper.get();
            if (is<WebGPUComputePipeline>(pipeline))
                return Inspector::Protocol::Canvas::ProgramType::Compute;
            if (is<WebGPURenderPipeline>(pipeline)) {
                auto& renderPipeline = downcast<WebGPURenderPipeline>(pipeline);
                if (renderPipeline.vertexShader() == renderPipeline.fragmentShader())
                    sharesVertexFragmentShader = true;
                return Inspector::Protocol::Canvas::ProgramType::Render;
            }
            return WTF::nullopt;
        },
#endif
        [&] (Monostate) -> ProgramTypeType {
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
            ASSERT_NOT_REACHED();
#endif
            return WTF::nullopt;
        }
    );
    if (!programType) {
        ASSERT_NOT_REACHED();
        programType = Inspector::Protocol::Canvas::ProgramType::Render;
    }

    auto payload = Inspector::Protocol::Canvas::ShaderProgram::create()
        .setProgramId(m_identifier)
        .setProgramType(programType.value())
        .setCanvasId(m_canvas.identifier())
        .release();
    if (sharesVertexFragmentShader)
        payload->setSharesVertexFragmentShader(true);
    return payload;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)
