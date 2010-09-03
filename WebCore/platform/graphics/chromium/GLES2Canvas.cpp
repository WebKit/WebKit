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

#include "GLES2Canvas.h"

#include "DrawingBuffer.h"
#include "FloatRect.h"
#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "SharedGraphicsContext3D.h"
#include "SolidFillShader.h"
#include "TexShader.h"
#include "Texture.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <wtf/OwnArrayPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

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

GLES2Canvas::GLES2Canvas(SharedGraphicsContext3D* context, DrawingBuffer* drawingBuffer, const IntSize& size)
    : m_size(size)
    , m_context(context)
    , m_drawingBuffer(drawingBuffer)
    , m_state(0)
{
    m_flipMatrix.translate(-1.0f, 1.0f);
    m_flipMatrix.scale(2.0f / size.width(), -2.0f / size.height());

    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

GLES2Canvas::~GLES2Canvas()
{
}

void GLES2Canvas::bindFramebuffer()
{
    m_drawingBuffer->bind();
}

void GLES2Canvas::clearRect(const FloatRect& rect)
{
    bindFramebuffer();
    if (m_state->m_ctm.isIdentity()) {
        m_context->scissor(rect);
        m_context->enable(GraphicsContext3D::SCISSOR_TEST);
        m_context->clearColor(Color(RGBA32(0)));
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
    m_context->applyCompositeOperator(m_state->m_compositeOp);
    m_context->useQuadVertices();

    AffineTransform matrix(m_flipMatrix);
    matrix.multLeft(m_state->m_ctm);
    matrix.translate(rect.x(), rect.y());
    matrix.scale(rect.width(), rect.height());

    m_context->useFillSolidProgram(matrix, color);

    bindFramebuffer();
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

void GLES2Canvas::drawTexturedRect(unsigned texture, const IntSize& textureSize, const FloatRect& srcRect, const FloatRect& dstRect, ColorSpace colorSpace, CompositeOperator compositeOp)
{
    m_context->applyCompositeOperator(compositeOp);

    m_context->useQuadVertices();
    m_context->setActiveTexture(GraphicsContext3D::TEXTURE0);

    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, texture);

    drawQuad(textureSize, srcRect, dstRect, m_state->m_ctm, m_state->m_alpha);
}

void GLES2Canvas::drawTexturedRect(Texture* texture, const FloatRect& srcRect, const FloatRect& dstRect, ColorSpace colorSpace, CompositeOperator compositeOp)
{
    drawTexturedRect(texture, srcRect, dstRect, m_state->m_ctm, m_state->m_alpha, colorSpace, compositeOp);
}


void GLES2Canvas::drawTexturedRect(Texture* texture, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform& transform, float alpha, ColorSpace colorSpace, CompositeOperator compositeOp)
{
    m_context->applyCompositeOperator(compositeOp);
    const TilingData& tiles = texture->tiles();
    IntRect tileIdxRect = tiles.overlappedTileIndices(srcRect);

    m_context->useQuadVertices();
    m_context->setActiveTexture(GraphicsContext3D::TEXTURE0);

    for (int y = tileIdxRect.y(); y <= tileIdxRect.bottom(); y++) {
        for (int x = tileIdxRect.x(); x <= tileIdxRect.right(); x++)
            drawTexturedRectTile(texture, tiles.tileIndex(x, y), srcRect, dstRect, transform, alpha);
    }
}

void GLES2Canvas::drawTexturedRectTile(Texture* texture, int tile, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform& transform, float alpha)
{
    if (dstRect.isEmpty())
        return;

    const TilingData& tiles = texture->tiles();

    texture->bindTile(tile);

    FloatRect srcRectClippedInTileSpace;
    FloatRect dstRectIntersected;
    tiles.intersectDrawQuad(srcRect, dstRect, tile, &srcRectClippedInTileSpace, &dstRectIntersected);

    IntRect tileBoundsWithBorder = tiles.tileBoundsWithBorder(tile);

    drawQuad(tileBoundsWithBorder.size(), srcRectClippedInTileSpace, dstRectIntersected, transform, alpha);
}

void GLES2Canvas::drawQuad(const IntSize& textureSize, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform& transform, float alpha)
{
    AffineTransform matrix(m_flipMatrix);
    matrix.multLeft(transform);
    matrix.translate(dstRect.x(), dstRect.y());
    matrix.scale(dstRect.width(), dstRect.height());

    AffineTransform texMatrix;
    texMatrix.scale(1.0f / textureSize.width(), 1.0f / textureSize.height());
    texMatrix.translate(srcRect.x(), srcRect.y());
    texMatrix.scale(srcRect.width(), srcRect.height());

    bindFramebuffer();

    m_context->useTextureProgram(matrix, texMatrix, alpha);
    m_context->drawArrays(GraphicsContext3D::TRIANGLE_STRIP, 0, 4);
    checkGLError("glDrawArrays");
}

void GLES2Canvas::setCompositeOperation(CompositeOperator op)
{
    m_state->m_compositeOp = op;
}

Texture* GLES2Canvas::createTexture(NativeImagePtr ptr, Texture::Format format, int width, int height)
{
    return m_context->createTexture(ptr, format, width, height);
}

Texture* GLES2Canvas::getTexture(NativeImagePtr ptr)
{
    return m_context->getTexture(ptr);
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

