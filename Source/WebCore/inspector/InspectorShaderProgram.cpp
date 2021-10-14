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

#if ENABLE(WEBGL)

#include "InspectorCanvas.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <variant>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "GraphicsContextGL.h"
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
    return adoptRef(*new InspectorShaderProgram(program, inspectorCanvas));
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

#if ENABLE(WEBGL)
WebGLProgram* InspectorShaderProgram::program() const
{
    if (auto* programWrapper = std::get_if<std::reference_wrapper<WebGLProgram>>(&m_program))
        return &programWrapper->get();
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
        [&] (std::monostate) {
#if ENABLE(WEBGL)
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
        [&] (std::monostate) {
#if ENABLE(WEBGL)
            ASSERT_NOT_REACHED();
#endif
            return false;
        }
    );
}

Ref<Inspector::Protocol::Canvas::ShaderProgram> InspectorShaderProgram::buildObjectForShaderProgram()
{
    using ProgramTypeType = std::optional<Inspector::Protocol::Canvas::ProgramType>;
    auto programType = WTF::switchOn(m_program,
#if ENABLE(WEBGL)
        [&] (std::reference_wrapper<WebGLProgram>) -> ProgramTypeType {
            return Inspector::Protocol::Canvas::ProgramType::Render;
        },
#endif
        [&] (std::monostate) -> ProgramTypeType {
#if ENABLE(WEBGL)
            ASSERT_NOT_REACHED();
#endif
            return std::nullopt;
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
    return payload;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
