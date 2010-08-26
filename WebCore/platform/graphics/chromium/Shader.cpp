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
#include "Shader.h"

#include "GraphicsContext3D.h"

#include <wtf/text/CString.h>

namespace WebCore {

static inline void affineTo3x3(const AffineTransform& transform, float mat[9])
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

Shader::Shader(GraphicsContext3D* context, unsigned program)
    : m_context(context)
    , m_program(program)
{
}

Shader::~Shader()
{
    m_context->deleteProgram(m_program);
}

static unsigned loadShader(GraphicsContext3D* context, unsigned type, const char* shaderSource)
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
        LOG_ERROR(infoLog.utf8().data());
        context->deleteShader(shader);
        return 0;
    }
    return shader;
}

static unsigned loadProgram(GraphicsContext3D* context, const char* vertexShaderSource, const char* fragmentShaderSource)
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

SolidFillShader::SolidFillShader(GraphicsContext3D* context, unsigned program)
    : Shader(context, program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_colorLocation = context->getUniformLocation(program, "color");
    m_positionLocation = context->getAttribLocation(program, "position");
}

TexShader::TexShader(GraphicsContext3D* context, unsigned program)
    : Shader(context, program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_texMatrixLocation = context->getUniformLocation(program, "texMatrix");
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    m_positionLocation = context->getAttribLocation(program, "position");
    m_samplerLocation = context->getUniformLocation(program, "sampler");
}

PassOwnPtr<SolidFillShader> SolidFillShader::create(GraphicsContext3D* context)
{
    const char* vertexShaderSource = 
            "uniform mat3 matrix;\n"
            "uniform vec4 color;\n"
            "attribute vec3 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(matrix * position, 1.0);\n"
            "}\n";
    const char* fragmentShaderSource =
            "precision mediump float;\n"
            "uniform mat3 matrix;\n"
            "uniform vec4 color;\n"
            "void main() {\n"
            "    gl_FragColor = color;\n"
            "}\n";
    unsigned program = loadProgram(context, vertexShaderSource, fragmentShaderSource);
    if (!program)
        return 0;
    return new SolidFillShader(context, program);
}

void SolidFillShader::use(const AffineTransform& transform, const Color& color)
{
    m_context->useProgram(m_program);

    float rgba[4];
    color.getRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    m_context->uniform4f(m_colorLocation, rgba[0] * rgba[3], rgba[1] * rgba[3], rgba[2] * rgba[3], rgba[3]);

    float matrix[9];
    affineTo3x3(transform, matrix);
    m_context->uniformMatrix3fv(m_matrixLocation, false /*transpose*/, matrix, 1 /*count*/);

    m_context->vertexAttribPointer(m_positionLocation, 3, GraphicsContext3D::FLOAT, false, 0, 0);

    m_context->enableVertexAttribArray(m_positionLocation);
}

PassOwnPtr<TexShader> TexShader::create(GraphicsContext3D* context)
{
    const char* vertexShaderSource =
        "uniform mat3 matrix;\n"
        "uniform mat3 texMatrix;\n"
        "attribute vec3 position;\n"
        "varying vec3 texCoord;\n"
        "void main() {\n"
        "    texCoord = texMatrix * position;\n"
        "    gl_Position = vec4(matrix * position, 1.0);\n"
        "}\n";
    const char* fragmentShaderSource =
        "precision mediump float;\n"
        "uniform sampler2D sampler;\n"
        "uniform float alpha;\n"
        "varying vec3 texCoord;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(sampler, texCoord.xy)* vec4(alpha);\n"
        "}\n";
    unsigned program = loadProgram(context, vertexShaderSource, fragmentShaderSource);
    if (!program)
        return 0;
    return new TexShader(context, program);
}

void TexShader::use(const AffineTransform& transform, const AffineTransform& texTransform, int sampler, float alpha)
{
    m_context->useProgram(m_program);
    float matrix[9];
    affineTo3x3(transform, matrix);
    m_context->uniformMatrix3fv(m_matrixLocation, false /*transpose*/, matrix, 1 /*count*/);

    float texMatrix[9];
    affineTo3x3(texTransform, texMatrix);
    m_context->uniformMatrix3fv(m_texMatrixLocation, false /*transpose*/, texMatrix, 1 /*count*/);

    m_context->uniform1i(m_samplerLocation, sampler);
    m_context->uniform1f(m_alphaLocation, alpha);

    m_context->vertexAttribPointer(m_positionLocation, 3, GraphicsContext3D::FLOAT, false, 0, 0);

    m_context->enableVertexAttribArray(m_positionLocation);

}

}
