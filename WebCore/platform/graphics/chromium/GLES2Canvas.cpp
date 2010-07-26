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

#if USE(GLES2_RENDERING)

#include "GLES2Canvas.h"

#include "FloatRect.h"
#include "GLES2Context.h"
#include "GLES2Texture.h"

#include <GLES2/gl2.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <wtf/OwnArrayPtr.h>

namespace WebCore {

static inline void affineTo3x3(const AffineTransform& transform, GLfloat mat[9])
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

struct GLES2Canvas::State {
    State()
        : m_fillColor(0, 0, 0, 255)
        , m_alpha(1.0f)
        , m_compositeOp(CompositeSourceOver)
    {
    }
    Color m_fillColor;
    float m_alpha;
    CompositeOperator m_compositeOp;
    AffineTransform m_ctm;
};

GLES2Canvas::GLES2Canvas(GLES2Context* context, int width, int height)
    : m_gles2Context(context)
    , m_quadVertices(0)
    , m_quadIndices(0)
    , m_simpleProgram(0)
    , m_texProgram(0)
    , m_simpleMatrixLocation(-1)
    , m_simpleColorLocation(-1)
    , m_simplePositionLocation(-1)
    , m_texMatrixLocation(-1)
    , m_texTexMatrixLocation(-1)
    , m_texSamplerLocation(-1)
    , m_texAlphaLocation(-1)
    , m_texPositionLocation(-1)
    , m_width(width)
    , m_height(height)
    , m_state(0)
{
    m_flipMatrix.translate(-1.0f, 1.0f);
    m_flipMatrix.scale(2.0f / width, -2.0f / height);

    m_gles2Context->makeCurrent();
    m_gles2Context->resizeOffscreenContent(IntSize(width, height));
    m_gles2Context->swapBuffers();
    glViewport(0, 0, width, height);

    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

GLES2Canvas::~GLES2Canvas()
{
    m_gles2Context->makeCurrent();
    if (m_simpleProgram)
        glDeleteProgram(m_simpleProgram);
    if (m_texProgram)
        glDeleteProgram(m_texProgram);
    if (m_quadVertices)
        glDeleteBuffers(1, &m_quadVertices);
    if (m_quadIndices)
        glDeleteBuffers(1, &m_quadVertices);
}

void GLES2Canvas::clearRect(const FloatRect& rect)
{
    m_gles2Context->makeCurrent();

    glScissor(rect.x(), rect.y(), rect.width(), rect.height());
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

void GLES2Canvas::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    m_gles2Context->makeCurrent();

    glBindBuffer(GL_ARRAY_BUFFER, getQuadVertices());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, getQuadIndices());

    float rgba[4];
    color.getRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    glUniform4f(m_simpleColorLocation, rgba[0] * rgba[3], rgba[1] * rgba[3], rgba[2] * rgba[3], rgba[3]);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}

void GLES2Canvas::fillRect(const FloatRect& rect)
{
    m_gles2Context->makeCurrent();

    applyCompositeOperator(m_state->m_compositeOp);

    glBindBuffer(GL_ARRAY_BUFFER, getQuadVertices());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, getQuadIndices());

    glUseProgram(getSimpleProgram());

    float rgba[4];
    m_state->m_fillColor.getRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    glUniform4f(m_simpleColorLocation, rgba[0] * rgba[3], rgba[1] * rgba[3], rgba[2] * rgba[3], rgba[3]);

    AffineTransform matrix(m_flipMatrix);
    matrix.multLeft(m_state->m_ctm);
    matrix.translate(rect.x(), rect.y());
    matrix.scale(rect.width(), rect.height());
    GLfloat mat[9];
    affineTo3x3(matrix, mat);
    glUniformMatrix3fv(m_simpleMatrixLocation, 1, GL_FALSE, mat);

    glVertexAttribPointer(m_simplePositionLocation, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(0));

    glEnableVertexAttribArray(m_simplePositionLocation);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}

void GLES2Canvas::setFillColor(const Color& color, ColorSpace colorSpace)
{
    m_state->m_fillColor = color;
}

void GLES2Canvas::setAlpha(float alpha)
{
    m_state->m_alpha = alpha;
}

void GLES2Canvas::translate(float x, float y)
{
    m_state->m_ctm.translate(x, y);
}

void GLES2Canvas::rotate(float angleInRadians)
{
    m_state->m_ctm.rotate(angleInRadians * (180.0f / M_PI));
}

void GLES2Canvas::scale(const FloatSize& size)
{
    m_state->m_ctm.scale(size.width(), size.height());
}

void GLES2Canvas::concatCTM(const AffineTransform& affine)
{
    m_state->m_ctm.multLeft(affine);
}

void GLES2Canvas::save()
{
    m_stateStack.append(State(m_stateStack.last()));
    m_state = &m_stateStack.last();
}

void GLES2Canvas::restore()
{
    ASSERT(!m_stateStack.isEmpty());
    m_stateStack.removeLast();
    m_state = &m_stateStack.last();
}

void GLES2Canvas::drawTexturedRect(GLES2Texture* texture, const FloatRect& srcRect, const FloatRect& dstRect, ColorSpace colorSpace, CompositeOperator compositeOp)
{
    drawTexturedRect(texture, srcRect, dstRect, m_state->m_ctm, m_state->m_alpha, colorSpace, compositeOp);
}

void GLES2Canvas::drawTexturedRect(GLES2Texture* texture, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform& transform, float alpha, ColorSpace colorSpace, CompositeOperator compositeOp)
{
    m_gles2Context->makeCurrent();

    applyCompositeOperator(compositeOp);

    glBindBuffer(GL_ARRAY_BUFFER, getQuadVertices());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, getQuadIndices());
    checkGLError("glBindBuffer");

    glUseProgram(getTexProgram());
    checkGLError("glUseProgram");

    glActiveTexture(GL_TEXTURE0);
    texture->bind();

    glUniform1i(m_texSamplerLocation, 0);
    checkGLError("glUniform1i");

    glUniform1f(m_texAlphaLocation, alpha);
    checkGLError("glUniform1f for alpha");

    AffineTransform matrix(m_flipMatrix);
    matrix.multLeft(transform);
    matrix.translate(dstRect.x(), dstRect.y());
    matrix.scale(dstRect.width(), dstRect.height());
    GLfloat mat[9];
    affineTo3x3(matrix, mat);
    glUniformMatrix3fv(m_texMatrixLocation, 1, GL_FALSE, mat);
    checkGLError("glUniformMatrix3fv");

    AffineTransform texMatrix;
    texMatrix.scale(1.0f / texture->width(), 1.0f / texture->height());
    texMatrix.translate(srcRect.x(), srcRect.y());
    texMatrix.scale(srcRect.width(), srcRect.height());
    GLfloat texMat[9];
    affineTo3x3(texMatrix, texMat);
    glUniformMatrix3fv(m_texTexMatrixLocation, 1, GL_FALSE, texMat);
    checkGLError("glUniformMatrix3fv");

    glVertexAttribPointer(m_texPositionLocation, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(0));

    glEnableVertexAttribArray(m_texPositionLocation);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    checkGLError("glDrawElements");
}

void GLES2Canvas::setCompositeOperation(CompositeOperator op)
{
    m_state->m_compositeOp = op;
}

void GLES2Canvas::applyCompositeOperator(CompositeOperator op)
{
    if (op == m_lastCompositeOp)
        return;

    switch (op) {
    case CompositeClear:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ZERO);
        break;
    case CompositeCopy:
        glDisable(GL_BLEND);
        break;
    case CompositeSourceOver:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeSourceIn:
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_ALPHA, GL_ZERO);
        break;
    case CompositeSourceOut:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ZERO);
        break;
    case CompositeSourceAtop:
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationOver:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
        break;
    case CompositeDestinationIn:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
        break;
    case CompositeDestinationOut:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationAtop:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA);
        break;
    case CompositeXOR:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case CompositePlusDarker:
    case CompositeHighlight:
        // unsupported
        glDisable(GL_BLEND);
        break;
    case CompositePlusLighter:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        break;
    }
    m_lastCompositeOp = op;
}

unsigned GLES2Canvas::getQuadVertices()
{
    if (!m_quadVertices) {
        GLfloat vertices[] = { 0.0f, 0.0f, 1.0f,
                               1.0f, 0.0f, 1.0f,
                               1.0f, 1.0f, 1.0f,
                               0.0f, 1.0f, 1.0f };
        glGenBuffers(1, &m_quadVertices);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVertices);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    }
    return m_quadVertices;
}


unsigned GLES2Canvas::getQuadIndices()
{
    if (!m_quadIndices) {
        GLushort indices[] = { 0, 1, 2, 0, 2, 3};

        glGenBuffers(1, &m_quadIndices);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIndices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }
    return m_quadIndices;
}

static GLuint loadShader(GLenum type, const char* shaderSource)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;

    glShaderSource(shader, 1, &shaderSource, 0);
    glCompileShader(shader);
    GLint compileStatus;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (!compileStatus) {
        int length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        OwnArrayPtr<char> log(new char[length]);
        glGetShaderInfoLog(shader, length, 0, log.get());
        LOG_ERROR(log.get());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned GLES2Canvas::getSimpleProgram()
{
    if (!m_simpleProgram) {
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER,
            "uniform mat3 matrix;\n"
            "uniform vec4 color;\n"
            "attribute vec3 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(matrix * position, 1.0);\n"
            "}\n");
        if (!vertexShader)
            return 0;
        GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER,
            "precision mediump float;\n"
            "uniform mat3 matrix;\n"
            "uniform vec4 color;\n"
            "void main() {\n"
            "    gl_FragColor = color;\n"
            "}\n");
        if (!fragmentShader)
            return 0;
        m_simpleProgram = glCreateProgram();
        if (!m_simpleProgram)
            return 0;
        glAttachShader(m_simpleProgram, vertexShader);
        glAttachShader(m_simpleProgram, fragmentShader);
        glLinkProgram(m_simpleProgram);
        GLint linkStatus;
        glGetProgramiv(m_simpleProgram, GL_LINK_STATUS, &linkStatus);
        if (!linkStatus) {
            glDeleteProgram(m_simpleProgram);
            m_simpleProgram = 0;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        m_simplePositionLocation = glGetAttribLocation(m_simpleProgram, "position");
        m_simpleMatrixLocation = glGetUniformLocation(m_simpleProgram, "matrix");
        m_simpleColorLocation = glGetUniformLocation(m_simpleProgram, "color");
    }
    return m_simpleProgram;
}

unsigned GLES2Canvas::getTexProgram()
{
    if (!m_texProgram) {
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER,
            "uniform mat3 matrix;\n"
            "uniform mat3 texMatrix;\n"
            "attribute vec3 position;\n"
            "varying vec3 texCoord;\n"
            "void main() {\n"
            "    texCoord = texMatrix * position;\n"
            "    gl_Position = vec4(matrix * position, 1.0);\n"
            "}\n");
        if (!vertexShader)
            return 0;
        GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER,
            "precision mediump float;\n"
            "uniform sampler2D sampler;\n"
            "uniform float alpha;\n"
            "varying vec3 texCoord;\n"
            "void main() {\n"
            "    gl_FragColor = texture2D(sampler, texCoord.xy)* vec4(alpha);\n"
            "}\n");
        if (!fragmentShader)
            return 0;
        m_texProgram = glCreateProgram();
        if (!m_texProgram)
            return 0;
        glAttachShader(m_texProgram, vertexShader);
        glAttachShader(m_texProgram, fragmentShader);
        glLinkProgram(m_texProgram);
        GLint linkStatus;
        glGetProgramiv(m_texProgram, GL_LINK_STATUS, &linkStatus);
        if (!linkStatus) {
            glDeleteProgram(m_texProgram);
            m_texProgram = 0;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        m_texMatrixLocation = glGetUniformLocation(m_texProgram, "matrix");
        m_texSamplerLocation = glGetUniformLocation(m_texProgram, "sampler");
        m_texTexMatrixLocation = glGetUniformLocation(m_texProgram, "texMatrix");
        m_texPositionLocation = glGetAttribLocation(m_texProgram, "position");
        m_texAlphaLocation = glGetUniformLocation(m_texProgram, "alpha");
    }
    return m_texProgram;
}

GLES2Texture* GLES2Canvas::createTexture(NativeImagePtr ptr, GLES2Texture::Format format, int width, int height)
{
    PassRefPtr<GLES2Texture> texture = m_textures.get(ptr);
    if (texture)
        return texture.get();

    texture = GLES2Texture::create(format, width, height);
    GLES2Texture* t = texture.get();
    m_textures.set(ptr, texture);
    return t;
}

GLES2Texture* GLES2Canvas::getTexture(NativeImagePtr ptr)
{
    PassRefPtr<GLES2Texture> texture = m_textures.get(ptr);
    return texture ? texture.get() : 0;
}

void GLES2Canvas::checkGLError(const char* header)
{
#ifndef NDEBUG
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        const char* errorStr = "*** UNKNOWN ERROR ***";
        switch (err) {
        case GL_INVALID_ENUM:
            errorStr = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errorStr = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errorStr = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            errorStr = "GL_OUT_OF_MEMORY";
            break;
        }
        if (header)
            LOG_ERROR("%s:  %s", header, errorStr);
        else
            LOG_ERROR("%s", errorStr);
    }
#endif
}

}

#endif
