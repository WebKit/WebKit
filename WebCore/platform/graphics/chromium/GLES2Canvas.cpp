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
#include "GLES2Texture.h"
#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "PlatformString.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <wtf/OwnArrayPtr.h>
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

GLES2Canvas::GLES2Canvas(GraphicsContext3D* context, const IntSize& size)
    : m_context(context)
    , m_quadVertices(0)
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
    , m_state(0)
{
    m_flipMatrix.translate(-1.0f, 1.0f);
    m_flipMatrix.scale(2.0f / size.width(), -2.0f / size.height());

    m_context->reshape(size.width(), size.height());
    m_context->viewport(0, 0, size.width(), size.height());

    m_stateStack.append(State());
    m_state = &m_stateStack.last();

    // Force the source over composite mode to be applied.
    m_lastCompositeOp = CompositeClear;
    applyCompositeOperator(CompositeSourceOver);
}

GLES2Canvas::~GLES2Canvas()
{
    m_context->deleteProgram(m_simpleProgram);
    m_context->deleteProgram(m_texProgram);
    m_context->deleteBuffer(m_quadVertices);
}

void GLES2Canvas::clearRect(const FloatRect& rect)
{
    if (m_state->m_ctm.isIdentity()) {
        m_context->scissor(rect.x(), rect.y(), rect.width(), rect.height());
        m_context->enable(GraphicsContext3D::SCISSOR_TEST);
        m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT);
        m_context->disable(GraphicsContext3D::SCISSOR_TEST);
    } else {
        save();
        setCompositeOperation(CompositeClear);
        fillRect(rect, Color(RGBA32(0)), DeviceColorSpace);
        restore();
    }
}

void GLES2Canvas::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    applyCompositeOperator(m_state->m_compositeOp);

    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, getQuadVertices());

    m_context->useProgram(getSimpleProgram());

    float rgba[4];
    color.getRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    m_context->uniform4f(m_simpleColorLocation, rgba[0] * rgba[3], rgba[1] * rgba[3], rgba[2] * rgba[3], rgba[3]);

    AffineTransform matrix(m_flipMatrix);
    matrix.multLeft(m_state->m_ctm);
    matrix.translate(rect.x(), rect.y());
    matrix.scale(rect.width(), rect.height());
    float mat[9];
    affineTo3x3(matrix, mat);
    m_context->uniformMatrix3fv(m_simpleMatrixLocation, false /*transpose*/, mat, 1 /*count*/);

    m_context->vertexAttribPointer(m_simplePositionLocation, 3, GraphicsContext3D::FLOAT, false, 0, 0);

    m_context->enableVertexAttribArray(m_simplePositionLocation);

    m_context->drawArrays(GraphicsContext3D::TRIANGLE_STRIP, 0, 4);
}

void GLES2Canvas::fillRect(const FloatRect& rect)
{
    fillRect(rect, m_state->m_fillColor, DeviceColorSpace);
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
    applyCompositeOperator(compositeOp);

    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, getQuadVertices());
    checkGLError("glBindBuffer");

    m_context->useProgram(getTexProgram());
    checkGLError("glUseProgram");

    m_context->activeTexture(GraphicsContext3D::TEXTURE0);

    m_context->uniform1i(m_texSamplerLocation, 0);
    checkGLError("glUniform1i");

    m_context->uniform1f(m_texAlphaLocation, alpha);
    checkGLError("glUniform1f for alpha");

    m_context->vertexAttribPointer(m_texPositionLocation, 3, GraphicsContext3D::FLOAT, false, 0, 0);

    m_context->enableVertexAttribArray(m_texPositionLocation);

    const TilingData& tiles = texture->tiles();
    IntRect tileIdxRect = tiles.overlappedTileIndices(srcRect);

    for (int y = tileIdxRect.y(); y <= tileIdxRect.bottom(); y++) {
        for (int x = tileIdxRect.x(); x <= tileIdxRect.right(); x++)
            drawTexturedRectTile(texture, tiles.tileIndex(x, y), srcRect, dstRect, transform);
    }
}

void GLES2Canvas::drawTexturedRectTile(GLES2Texture* texture, int tile, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform& transform)
{
    if (dstRect.isEmpty())
        return;

    const TilingData& tiles = texture->tiles();

    texture->bindTile(tile);

    FloatRect srcRectClippedInTileSpace;
    FloatRect dstRectIntersected;
    tiles.intersectDrawQuad(srcRect, dstRect, tile, &srcRectClippedInTileSpace, &dstRectIntersected);

    IntRect tileBoundsWithBorder = tiles.tileBoundsWithBorder(tile);

    AffineTransform matrix(m_flipMatrix);
    matrix.multLeft(transform);
    matrix.translate(dstRectIntersected.x(), dstRectIntersected.y());
    matrix.scale(dstRectIntersected.width(), dstRectIntersected.height());
    float mat[9];
    affineTo3x3(matrix, mat);
    m_context->uniformMatrix3fv(m_texMatrixLocation, false /*transpose*/, mat, 1 /*count*/);
    checkGLError("glUniformMatrix3fv");

    AffineTransform texMatrix;
    texMatrix.scale(1.0f / tileBoundsWithBorder.width(), 1.0f / tileBoundsWithBorder.height());
    texMatrix.translate(srcRectClippedInTileSpace.x(), srcRectClippedInTileSpace.y());
    texMatrix.scale(srcRectClippedInTileSpace.width(), srcRectClippedInTileSpace.height());
    float texMat[9];
    affineTo3x3(texMatrix, texMat);
    m_context->uniformMatrix3fv(m_texTexMatrixLocation, false /*transpose*/, texMat, 1 /*count*/);
    checkGLError("glUniformMatrix3fv");

    m_context->drawArrays(GraphicsContext3D::TRIANGLE_STRIP, 0, 4);
    checkGLError("glDrawArrays");
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
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::ZERO);
        break;
    case CompositeCopy:
        m_context->disable(GraphicsContext3D::BLEND);
        break;
    case CompositeSourceOver:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeSourceIn:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::DST_ALPHA, GraphicsContext3D::ZERO);
        break;
    case CompositeSourceOut:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ZERO);
        break;
    case CompositeSourceAtop:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::DST_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationOver:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ONE);
        break;
    case CompositeDestinationIn:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::SRC_ALPHA);
        break;
    case CompositeDestinationOut:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationAtop:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::SRC_ALPHA);
        break;
    case CompositeXOR:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositePlusDarker:
    case CompositeHighlight:
        // unsupported
        m_context->disable(GraphicsContext3D::BLEND);
        break;
    case CompositePlusLighter:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE);
        break;
    }
    m_lastCompositeOp = op;
}

unsigned GLES2Canvas::getQuadVertices()
{
    if (!m_quadVertices) {
        float vertices[] = { 0.0f, 0.0f, 1.0f,
                             1.0f, 0.0f, 1.0f,
                             0.0f, 1.0f, 1.0f,
                             1.0f, 1.0f, 1.0f };
        m_quadVertices = m_context->createBuffer();
        m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVertices);
        m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(vertices), vertices, GraphicsContext3D::STATIC_DRAW);
    }
    return m_quadVertices;
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

unsigned GLES2Canvas::getSimpleProgram()
{
    if (!m_simpleProgram) {
        unsigned vertexShader = loadShader(m_context, GraphicsContext3D::VERTEX_SHADER,
            "uniform mat3 matrix;\n"
            "uniform vec4 color;\n"
            "attribute vec3 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(matrix * position, 1.0);\n"
            "}\n");
        if (!vertexShader)
            return 0;
        unsigned fragmentShader = loadShader(m_context, GraphicsContext3D::FRAGMENT_SHADER,
            "precision mediump float;\n"
            "uniform mat3 matrix;\n"
            "uniform vec4 color;\n"
            "void main() {\n"
            "    gl_FragColor = color;\n"
            "}\n");
        if (!fragmentShader)
            return 0;
        m_simpleProgram = m_context->createProgram();
        if (!m_simpleProgram)
            return 0;
        m_context->attachShader(m_simpleProgram, vertexShader);
        m_context->attachShader(m_simpleProgram, fragmentShader);
        m_context->linkProgram(m_simpleProgram);
        int linkStatus;
        m_context->getProgramiv(m_simpleProgram, GraphicsContext3D::LINK_STATUS, &linkStatus);
        if (!linkStatus) {
            m_context->deleteProgram(m_simpleProgram);
            m_simpleProgram = 0;
        }
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        m_simplePositionLocation = m_context->getAttribLocation(m_simpleProgram, "position");
        m_simpleMatrixLocation = m_context->getUniformLocation(m_simpleProgram, "matrix");
        m_simpleColorLocation = m_context->getUniformLocation(m_simpleProgram, "color");
    }
    return m_simpleProgram;
}

unsigned GLES2Canvas::getTexProgram()
{
    if (!m_texProgram) {
        unsigned vertexShader = loadShader(m_context, GraphicsContext3D::VERTEX_SHADER,
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
        unsigned fragmentShader = loadShader(m_context, GraphicsContext3D::FRAGMENT_SHADER,
            "precision mediump float;\n"
            "uniform sampler2D sampler;\n"
            "uniform float alpha;\n"
            "varying vec3 texCoord;\n"
            "void main() {\n"
            "    gl_FragColor = texture2D(sampler, texCoord.xy)* vec4(alpha);\n"
            "}\n");
        if (!fragmentShader)
            return 0;
        m_texProgram = m_context->createProgram();
        if (!m_texProgram)
            return 0;
        m_context->attachShader(m_texProgram, vertexShader);
        m_context->attachShader(m_texProgram, fragmentShader);
        m_context->linkProgram(m_texProgram);
        int linkStatus;
        m_context->getProgramiv(m_texProgram, GraphicsContext3D::LINK_STATUS, &linkStatus);
        if (!linkStatus) {
            m_context->deleteProgram(m_texProgram);
            m_texProgram = 0;
        }
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        m_texMatrixLocation = m_context->getUniformLocation(m_texProgram, "matrix");
        m_texSamplerLocation = m_context->getUniformLocation(m_texProgram, "sampler");
        m_texTexMatrixLocation = m_context->getUniformLocation(m_texProgram, "texMatrix");
        m_texPositionLocation = m_context->getAttribLocation(m_texProgram, "position");
        m_texAlphaLocation = m_context->getUniformLocation(m_texProgram, "alpha");
    }
    return m_texProgram;
}

GLES2Texture* GLES2Canvas::createTexture(NativeImagePtr ptr, GLES2Texture::Format format, int width, int height)
{
    PassRefPtr<GLES2Texture> texture = m_textures.get(ptr);
    if (texture)
        return texture.get();

    texture = GLES2Texture::create(m_context, format, width, height);
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
    unsigned err;
    while ((err = m_context->getError()) != GraphicsContext3D::NO_ERROR) {
        const char* errorStr = "*** UNKNOWN ERROR ***";
        switch (err) {
        case GraphicsContext3D::INVALID_ENUM:
            errorStr = "GraphicsContext3D::INVALID_ENUM";
            break;
        case GraphicsContext3D::INVALID_VALUE:
            errorStr = "GraphicsContext3D::INVALID_VALUE";
            break;
        case GraphicsContext3D::INVALID_OPERATION:
            errorStr = "GraphicsContext3D::INVALID_OPERATION";
            break;
        case GraphicsContext3D::INVALID_FRAMEBUFFER_OPERATION:
            errorStr = "GraphicsContext3D::INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GraphicsContext3D::OUT_OF_MEMORY:
            errorStr = "GraphicsContext3D::OUT_OF_MEMORY";
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
