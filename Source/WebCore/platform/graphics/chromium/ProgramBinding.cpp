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

#include "CCRendererGL.h" // For the GLC() macro.
#include "GeometryBinding.h"
#include "GraphicsContext3D.h"
#include "TraceEvent.h"
#include <public/WebGraphicsContext3D.h>
#include <wtf/text/CString.h>

using WebKit::WebGraphicsContext3D;

namespace WebCore {

ProgramBindingBase::ProgramBindingBase()
    : m_program(0)
    , m_vertexShaderId(0)
    , m_fragmentShaderId(0)
    , m_initialized(false)
{
}

ProgramBindingBase::~ProgramBindingBase()
{
    // If you hit these asserts, you initialized but forgot to call cleanup().
    ASSERT(!m_program);
    ASSERT(!m_vertexShaderId);
    ASSERT(!m_fragmentShaderId);
    ASSERT(!m_initialized);
}

static bool contextLost(WebGraphicsContext3D* context)
{
    return (context->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR);
}


void ProgramBindingBase::init(WebGraphicsContext3D* context, const String& vertexShader, const String& fragmentShader)
{
    TRACE_EVENT0("cc", "ProgramBindingBase::init");
    m_vertexShaderId = loadShader(context, GraphicsContext3D::VERTEX_SHADER, vertexShader);
    if (!m_vertexShaderId) {
        if (!contextLost(context))
            LOG_ERROR("Failed to create vertex shader");
        return;
    }

    m_fragmentShaderId = loadShader(context, GraphicsContext3D::FRAGMENT_SHADER, fragmentShader);
    if (!m_fragmentShaderId) {
        GLC(context, context->deleteShader(m_vertexShaderId));
        m_vertexShaderId = 0;
        if (!contextLost(context))
            LOG_ERROR("Failed to create fragment shader");
        return;
    }

    m_program = createShaderProgram(context, m_vertexShaderId, m_fragmentShaderId);
    ASSERT(m_program || contextLost(context));
}

void ProgramBindingBase::link(WebGraphicsContext3D* context)
{
    GLC(context, context->linkProgram(m_program));
    cleanupShaders(context);
#ifndef NDEBUG
    int linked = 0;
    GLC(context, context->getProgramiv(m_program, GraphicsContext3D::LINK_STATUS, &linked));
    if (!linked) {
        if (!contextLost(context))
            LOG_ERROR("Failed to link shader program");
        GLC(context, context->deleteProgram(m_program));
        return;
    }
#endif
}

void ProgramBindingBase::cleanup(WebGraphicsContext3D* context)
{
    m_initialized = false;
    if (!m_program)
        return;

    ASSERT(context);
    GLC(context, context->deleteProgram(m_program));
    m_program = 0;

    cleanupShaders(context);
}

unsigned ProgramBindingBase::loadShader(WebGraphicsContext3D* context, unsigned type, const String& shaderSource)
{
    unsigned shader = context->createShader(type);
    if (!shader)
        return 0;
    String sourceString(shaderSource);
    GLC(context, context->shaderSource(shader, sourceString.utf8().data()));
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

unsigned ProgramBindingBase::createShaderProgram(WebGraphicsContext3D* context, unsigned vertexShader, unsigned fragmentShader)
{
    unsigned programObject = context->createProgram();
    if (!programObject) {
        if (!contextLost(context))
            LOG_ERROR("Failed to create shader program");
        return 0;
    }

    GLC(context, context->attachShader(programObject, vertexShader));
    GLC(context, context->attachShader(programObject, fragmentShader));

    // Bind the common attrib locations.
    GLC(context, context->bindAttribLocation(programObject, GeometryBinding::positionAttribLocation(), "a_position"));
    GLC(context, context->bindAttribLocation(programObject, GeometryBinding::texCoordAttribLocation(), "a_texCoord"));

    return programObject;
}

void ProgramBindingBase::cleanupShaders(WebGraphicsContext3D* context)
{
    if (m_vertexShaderId) {
        GLC(context, context->deleteShader(m_vertexShaderId));
        m_vertexShaderId = 0;
    }
    if (m_fragmentShaderId) {
        GLC(context, context->deleteShader(m_fragmentShaderId));
        m_fragmentShaderId = 0;
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
