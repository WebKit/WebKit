/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ProgramBinding.h"

#include "GeometryBinding.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"

namespace WebCore {

ProgramBindingBase::ProgramBindingBase(GraphicsContext3D* context)
    : m_context(context)
    , m_program(0)
    , m_initialized(false)
{
}

ProgramBindingBase::~ProgramBindingBase()
{
    if (m_program)
        GLC(m_context, m_context->deleteProgram(m_program));
}

bool ProgramBindingBase::init(const String& vertexShader, const String& fragmentShader)
{
    m_program = createShaderProgram(vertexShader, fragmentShader);
    if (!m_program) {
        LOG_ERROR("Failed to create shader program");
        return false;
    }
    return true;
}

unsigned ProgramBindingBase::loadShader(unsigned type, const String& shaderSource)
{
    unsigned shader = m_context->createShader(type);
    if (!shader)
        return 0;
    String sourceString(shaderSource);
    GLC(m_context, m_context->shaderSource(shader, sourceString));
    GLC(m_context, m_context->compileShader(shader));
    int compiled = 0;
    GLC(m_context, m_context->getShaderiv(shader, GraphicsContext3D::COMPILE_STATUS, &compiled));
    if (!compiled) {
        GLC(m_context, m_context->deleteShader(shader));
        return 0;
    }
    return shader;
}

unsigned ProgramBindingBase::createShaderProgram(const String& vertexShaderSource, const String& fragmentShaderSource)
{
    unsigned vertexShader = loadShader(GraphicsContext3D::VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader) {
        LOG_ERROR("Failed to create vertex shader");
        return 0;
    }

    unsigned fragmentShader = loadShader(GraphicsContext3D::FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader) {
        GLC(m_context, m_context->deleteShader(vertexShader));
        LOG_ERROR("Failed to create fragment shader");
        return 0;
    }

    unsigned programObject = m_context->createProgram();
    if (!programObject) {
        LOG_ERROR("Failed to create shader program");
        return 0;
    }

    GLC(m_context, m_context->attachShader(programObject, vertexShader));
    GLC(m_context, m_context->attachShader(programObject, fragmentShader));

    // Bind the common attrib locations.
    GLC(m_context, m_context->bindAttribLocation(programObject, GeometryBinding::positionAttribLocation(), "a_position"));
    GLC(m_context, m_context->bindAttribLocation(programObject, GeometryBinding::texCoordAttribLocation(), "a_texCoord"));

    GLC(m_context, m_context->linkProgram(programObject));
    int linked = 0;
    GLC(m_context, m_context->getProgramiv(programObject, GraphicsContext3D::LINK_STATUS, &linked));
    if (!linked) {
        LOG_ERROR("Failed to link shader program");
        GLC(m_context, m_context->deleteProgram(programObject));
        return 0;
    }

    GLC(m_context, m_context->deleteShader(vertexShader));
    GLC(m_context, m_context->deleteShader(fragmentShader));
    return programObject;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
