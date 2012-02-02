/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#ifndef TextureMapperGL_h
#define TextureMapperGL_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatQuad.h"
#include "IntSize.h"
#include "TextureMapper.h"
#include "TransformationMatrix.h"

namespace WebCore {

class TextureMapperGLData;
class GraphicsContext;

// An OpenGL-ES2 implementation of TextureMapper.
class TextureMapperGL : public TextureMapper {
public:
    TextureMapperGL();
    virtual ~TextureMapperGL();

    // reimps from TextureMapper
    virtual void drawTexture(const BitmapTexture&, const FloatRect&, const TransformationMatrix&, float opacity, const BitmapTexture* maskTexture);
    virtual void drawTexture(uint32_t texture, bool opaque, const FloatSize&, const FloatRect&, const TransformationMatrix&, float opacity, const BitmapTexture* maskTexture, bool flip);
    virtual void bindSurface(BitmapTexture* surface);
    virtual void beginClip(const TransformationMatrix&, const FloatRect&);
    virtual void endClip();
    virtual bool allowSurfaceForRoot() const { return false; }
    virtual PassRefPtr<BitmapTexture> createTexture();
    virtual const char* type() const;
    static PassOwnPtr<TextureMapperGL> create() { return adoptPtr(new TextureMapperGL); }
    void beginPainting();
    void endPainting();
    void setGraphicsContext(GraphicsContext* context) { m_context = context; }
    GraphicsContext* graphicsContext() { return m_context; }
    virtual bool isOpenGLBacked() const { return true; }

private:
    bool beginScissorClip(const TransformationMatrix&, const FloatRect&);
    bool endScissorClip();
    inline TextureMapperGLData& data() { return *m_data; }
    TextureMapperGLData* m_data;
    GraphicsContext* m_context;
    friend class BitmapTextureGL;
};

// An offscreen buffer to be rendered by software.
class BGRA32PremultimpliedBuffer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~BGRA32PremultimpliedBuffer() { }
    virtual PlatformGraphicsContext* beginPaint(const IntRect& dirtyRect, bool opaque) = 0;
    virtual void endPaint() = 0;
    virtual void* data() = 0;
    static PassOwnPtr<BGRA32PremultimpliedBuffer> create();
};

static inline int nextPowerOfTwo(int num)
{
    for (int i = 0x10000000; i > 0; i >>= 1) {
        if (num == i)
            return num;
        if (num & i)
            return (i << 1);
    }
    return 1;
}

static inline IntSize nextPowerOfTwo(const IntSize& size)
{
    return IntSize(nextPowerOfTwo(size.width()), nextPowerOfTwo(size.height()));
}

typedef uint64_t ImageUID;
ImageUID uidForImage(Image*);

};

#endif

#endif
