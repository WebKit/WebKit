/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef TextureMapperImageBuffer_h
#define TextureMapperImageBuffer_h

#include "ImageBuffer.h"
#include "TextureMapper.h"

#if USE(TEXTURE_MAPPER)
namespace WebCore {

class BitmapTextureImageBuffer : public BitmapTexture {
    friend class TextureMapperImageBuffer;
public:
    static PassRefPtr<BitmapTexture> create() { return adoptRef(new BitmapTextureImageBuffer); }
    ~BitmapTextureImageBuffer() { destroy(); }
    virtual void destroy() { m_image.clear(); }
    virtual IntSize size() const { return m_image->size(); }
    virtual void reset(const IntSize& size, bool opaque)
    {
        BitmapTexture::reset(size, opaque);
        m_image = ImageBuffer::create(size);
    }

    virtual bool isValid() const { return m_image; }
    inline GraphicsContext* graphicsContext() { return m_image ? m_image->context() : 0; }
    virtual void updateContents(Image*, const IntRect&, const IntRect&, PixelFormat);
    void updateContents(const void* data, const IntRect& targetRect);
private:
    BitmapTextureImageBuffer() { }
    OwnPtr<ImageBuffer> m_image;
};


class TextureMapperImageBuffer : public TextureMapper {
public:
    virtual void drawTexture(const BitmapTexture&, const FloatRect& targetRect, const TransformationMatrix&, float opacity, const BitmapTexture* maskTexture);
    virtual void beginClip(const TransformationMatrix&, const FloatRect&);
    virtual void bindSurface(BitmapTexture* surface) { m_currentSurface = surface;}
    virtual void endClip() { graphicsContext()->restore(); }
    static PassOwnPtr<TextureMapper> create() { return adoptPtr(new TextureMapperImageBuffer); }
    PassRefPtr<BitmapTexture> createTexture() { return BitmapTextureImageBuffer::create(); }
    inline GraphicsContext* currentContext()
    {
        return m_currentSurface ? static_cast<BitmapTextureImageBuffer*>(m_currentSurface.get())->graphicsContext() : graphicsContext();
    }

    virtual AccelerationMode accelerationMode() const { return SoftwareMode; }

private:
    RefPtr<BitmapTexture> m_currentSurface;
};

}
#endif // USE(TEXTURE_MAPPER)

#endif // TextureMapperImageBuffer_h
