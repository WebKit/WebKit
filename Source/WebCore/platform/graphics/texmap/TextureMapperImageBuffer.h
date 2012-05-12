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
    virtual IntSize size() const { return m_image->internalSize(); }
    virtual void didReset();
    virtual bool isValid() const { return m_image; }
    inline GraphicsContext* graphicsContext() { return m_image ? m_image->context() : 0; }
    virtual void updateContents(Image*, const IntRect&, const IntPoint&);
    virtual void updateContents(const void*, const IntRect& target, const IntPoint& sourceOffset, int bytesPerLine);
#if ENABLE(CSS_FILTERS)
    PassRefPtr<BitmapTexture> applyFilters(const BitmapTexture&, const FilterOperations&);
#endif

private:
    BitmapTextureImageBuffer() { }
    OwnPtr<ImageBuffer> m_image;
};


class TextureMapperImageBuffer : public TextureMapper {
public:
    static PassOwnPtr<TextureMapper> create() { return adoptPtr(new TextureMapperImageBuffer); }

    // TextureMapper implementation
    virtual void drawBorder(const Color& color, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix = TransformationMatrix()) OVERRIDE { };
    virtual void drawTexture(const BitmapTexture&, const FloatRect& targetRect, const TransformationMatrix&, float opacity, const BitmapTexture* maskTexture) OVERRIDE;
    virtual void beginClip(const TransformationMatrix&, const FloatRect&) OVERRIDE;
    virtual void bindSurface(BitmapTexture* surface) OVERRIDE { m_currentSurface = surface;}
    virtual void endClip() OVERRIDE { graphicsContext()->restore(); }
    virtual PassRefPtr<BitmapTexture> createTexture() OVERRIDE { return BitmapTextureImageBuffer::create(); }
    virtual AccelerationMode accelerationMode() const OVERRIDE { return SoftwareMode; }

    inline GraphicsContext* currentContext()
    {
        return m_currentSurface ? static_cast<BitmapTextureImageBuffer*>(m_currentSurface.get())->graphicsContext() : graphicsContext();
    }

private:
    RefPtr<BitmapTexture> m_currentSurface;
};

}
#endif // USE(TEXTURE_MAPPER)

#endif // TextureMapperImageBuffer_h
