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

#include "GraphicsContext3D.h"
#include "GraphicsTypes3D.h"
#include "HTMLCanvasElement.h"
#include "InspectorCanvas.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLShader.h"
#include <JavaScriptCore/IdentifiersFactory.h>

namespace WebCore {

using namespace Inspector;

Ref<InspectorShaderProgram> InspectorShaderProgram::create(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
{
    return adoptRef(*new InspectorShaderProgram(program, inspectorCanvas));
}

InspectorShaderProgram::InspectorShaderProgram(WebGLProgram& program, InspectorCanvas& inspectorCanvas)
    : m_identifier("program:" + IdentifiersFactory::createIdentifier())
    , m_program(program)
    , m_canvas(inspectorCanvas)
{
}

WebGLRenderingContextBase& InspectorShaderProgram::context() const
{
    ASSERT(is<WebGLRenderingContextBase>(m_canvas.context()));
    return downcast<WebGLRenderingContextBase>(m_canvas.context());
}

WebGLShader* InspectorShaderProgram::shaderForType(const String& protocolType)
{
    GC3Denum shaderType;
    if (protocolType == "vertex")
        shaderType = GraphicsContext3D::VERTEX_SHADER;
    else if (protocolType == "fragment")
        shaderType = GraphicsContext3D::FRAGMENT_SHADER;
    else
        return nullptr;

    return m_program.getAttachedShader(shaderType);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
