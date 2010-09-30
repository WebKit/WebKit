/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(ACCELERATED_2D_CANVAS)

#include "Shader.h"

#include "AffineTransform.h"
#include "GraphicsContext3D.h"

#include <wtf/text/CString.h>

namespace WebCore {

// static
void Shader::affineTo3x3(const AffineTransform& transform, float mat[9])
{
    mat[0] = transform.a();
    mat[1] = transform.b();
    mat[2] = 0.0f;
    mat[3] = transform.c();
    mat[4] = transform.d();
    mat[5] = 0.0f;
    mat[6] = transform.e();
    mat[7] = transform.f();
    mat[8] = 1.0f;
}

// static
unsigned Shader::loadShader(GraphicsContext3D* context, unsigned type, const char* shaderSource)
{
    unsigned shader = context->createShader(type);
    if (!shader)
        return 0;

    String shaderSourceStr(shaderSource);
    context->shaderSource(shader, shaderSourceStr);
    context->compileShader(shader);
    int compileStatus;
    context->getShaderiv(shader, GraphicsContext3D::COMPILE_STATUS, &compileStatus);
    if (!compileStatus) {
        String infoLog = context->getShaderInfoLog(shader);
        LOG_ERROR("%s", infoLog.utf8().data());
        context->deleteShader(shader);
        return 0;
    }
    return shader;
}

// static
unsigned Shader::loadProgram(GraphicsContext3D* context, const char* vertexShaderSource, const char* fragmentShaderSource)
{
    unsigned vertexShader = loadShader(context, GraphicsContext3D::VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader)
        return 0;
    unsigned fragmentShader = loadShader(context, GraphicsContext3D::FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader)
        return 0;
    unsigned program = context->createProgram();
    if (!program)
        return 0;
    context->attachShader(program, vertexShader);
    context->attachShader(program, fragmentShader);
    context->linkProgram(program);
    int linkStatus;
    context->getProgramiv(program, GraphicsContext3D::LINK_STATUS, &linkStatus);
    if (!linkStatus)
        context->deleteProgram(program);
    context->deleteShader(vertexShader);
    context->deleteShader(fragmentShader);
    return program;
}

Shader::Shader(GraphicsContext3D* context, unsigned program)
    : m_context(context)
    , m_program(program)
{
}

Shader::~Shader()
{
    m_context->deleteProgram(m_program);
}

}

#endif
