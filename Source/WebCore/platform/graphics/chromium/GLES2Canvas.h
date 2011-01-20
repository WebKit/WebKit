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

#ifndef GLES2Canvas_h
#define GLES2Canvas_h

#include "AffineTransform.h"
#include "Color.h"
#include "ColorSpace.h"
#include "GraphicsTypes.h"
#include "ImageSource.h"
#include "Texture.h"

#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class Color;
class DrawingBuffer;
class FloatRect;
class GraphicsContext3D;
class SharedGraphicsContext3D;

class GLES2Canvas : public Noncopyable {
public:
    GLES2Canvas(SharedGraphicsContext3D*, DrawingBuffer*, const IntSize&);
    ~GLES2Canvas();

    void fillRect(const FloatRect&, const Color&, ColorSpace);
    void fillRect(const FloatRect&);
    void clearRect(const FloatRect&);
    void setFillColor(const Color&, ColorSpace);
    void setAlpha(float alpha);
    void setCompositeOperation(CompositeOperator);
    void translate(float x, float y);
    void rotate(float angleInRadians);
    void scale(const FloatSize&);
    void concatCTM(const AffineTransform&);

    void save();
    void restore();

    // non-standard functions
    // These are not standard GraphicsContext functions, and should be pushed
    // down into a PlatformContextGLES2 at some point.
    void drawTexturedRect(unsigned texture, const IntSize& textureSize, const FloatRect& srcRect, const FloatRect& dstRect, ColorSpace, CompositeOperator);
    void drawTexturedRect(Texture*, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform&, float alpha, ColorSpace, CompositeOperator);
    void drawTexturedRect(Texture*, const FloatRect& srcRect, const FloatRect& dstRect, ColorSpace, CompositeOperator);
    Texture* createTexture(NativeImagePtr, Texture::Format, int width, int height);
    Texture* getTexture(NativeImagePtr);

    SharedGraphicsContext3D* context() const { return m_context; }

    void bindFramebuffer();

    DrawingBuffer* drawingBuffer() const { return m_drawingBuffer; }

private:
    void drawTexturedRectTile(Texture* texture, int tile, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform&, float alpha);
    void drawQuad(const IntSize& textureSize, const FloatRect& srcRect, const FloatRect& dstRect, const AffineTransform&, float alpha);
    void applyCompositeOperator(CompositeOperator);
    void checkGLError(const char* header);

    IntSize m_size;

    SharedGraphicsContext3D* m_context;
    DrawingBuffer* m_drawingBuffer;

    struct State;
    WTF::Vector<State> m_stateStack;
    State* m_state;
    AffineTransform m_flipMatrix;
};

}

#endif // GLES2Canvas_h
