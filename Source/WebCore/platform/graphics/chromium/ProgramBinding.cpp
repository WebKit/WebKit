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
#include "TraceEvent.h"

namespace WebCore {

ProgramBindingBase::ProgramBindingBase()
    : m_program(0)
    , m_initialized(false)
{
}

ProgramBindingBase::~ProgramBindingBase()
{
    // If you hit these asserts, you initialized but forgot to call cleanup().
    ASSERT(!m_program);
    ASSERT(!m_initialized);
}

void ProgramBindingBase::init(GraphicsContext3D* context, const String& vertexShader, const String& fragmentShader)
{
    m_program = createShaderProgram(context, vertexShader, fragmentShader);
    ASSERT(m_program);
}

void ProgramBindingBase::cleanup(GraphicsContext3D* context)
{
    m_initialized = false;
    if (!m_program)
        return;

    ASSERT(context);
    GLC(context, context->deleteProgram(m_program));
    m_program = 0;
}

unsigned ProgramBindingBase::loadShader(GraphicsContext3D* context, unsigned type, const String& shaderSource)
{
    unsigned shader = context->createShader(type);
    if (!shader)
        return 0;
    String sourceString(shaderSource);
    GLC(context, context->shaderSource(shader, sourceString));
    GLC(context, context->compileShader(shader));
#ifndef NDEBUG
    int compiled = 0;
    GLC(context, context->getShaderiv(shader, GraphicsContext3D::COMPILE_STATUS, &compiled));
    if (!compiled) {
        GLC(context, context->deleteShader(shader));
        return 0;
    }
#endif
    return shader;
}

unsigned ProgramBindingBase::createShaderProgram(GraphicsContext3D* context, const String& vertexShaderSource, const String& fragmentShaderSource)
{
    TRACE_EVENT("ProgramBindingBase::createShaderProgram", this, 0);
    unsigned vertexShader = loadShader(context, GraphicsContext3D::VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader) {
        LOG_ERROR("Failed to create vertex shader");
        return 0;
    }

    unsigned fragmentShader = loadShader(context, GraphicsContext3D::FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader) {
        GLC(context, context->deleteShader(vertexShader));
        LOG_ERROR("Failed to create fragment shader");
        return 0;
    }

    unsigned programObject = context->createProgram();
    if (!programObject) {
        LOG_ERROR("Failed to create shader program");
        return 0;
    }

    GLC(context, context->attachShader(programObject, vertexShader));
    GLC(context, context->attachShader(programObject, fragmentShader));

    // Bind the common attrib locations.
    GLC(context, context->bindAttribLocation(programObject, GeometryBinding::positionAttribLocation(), "a_position"));
    GLC(context, context->bindAttribLocation(programObject, GeometryBinding::texCoordAttribLocation(), "a_texCoord"));

    GLC(context, context->linkProgram(programObject));
#ifndef NDEBUG
    int linked = 0;
    GLC(context, context->getProgramiv(programObject, GraphicsContext3D::LINK_STATUS, &linked));
    if (!linked) {
        LOG_ERROR("Failed to link shader program");
        GLC(context, context->deleteProgram(programObject));
        return 0;
    }
#endif

    GLC(context, context->deleteShader(vertexShader));
    GLC(context, context->deleteShader(fragmentShader));
    return programObject;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
