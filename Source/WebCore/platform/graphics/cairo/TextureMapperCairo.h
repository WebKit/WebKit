/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef TextureMapperCairo_h
#define TextureMapperCairo_h

#include "GraphicsContext.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include "texmap/TextureMapper.h"

namespace WebCore {

class BitmapTextureCairo : public BitmapTexture {
    friend class TextureMapperCairo;
public:
    BitmapTextureCairo();
    ~BitmapTextureCairo() { destroy(); }
    virtual void destroy();
    virtual IntSize size() const;
    virtual void reset(const IntSize&, bool opaque);
    virtual PlatformGraphicsContext* beginPaint(const IntRect& dirtyRect);
    virtual void endPaint();
    virtual void setContentsToImage(Image*);
    virtual bool save(const String& path);
    virtual bool isValid() const { return !m_surface;}
    IntRect sourceRect() const { return IntRect(0, 0, contentSize().width(), contentSize().height()); }
    virtual void pack() { }
    virtual void unpack() { }
    virtual bool isPacked() const { return 0; }
    virtual void updateContents(PixelFormat, const IntRect&, void* bits);

    cairo_t* cr() { return m_context->cr(); }

private:
    void clearRect(const IntRect&);

    PlatformContextCairo* m_context;
    RefPtr<cairo_surface_t> m_surface;
};

class TextureMapperCairo : public TextureMapper {
public:
    TextureMapperCairo();

    virtual void drawTexture(const BitmapTexture&, const FloatRect& targetRect, const TransformationMatrix&, float opacity, const BitmapTexture* maskTexture);
    virtual void bindSurface(BitmapTexture* surface);
    virtual void beginClip(const TransformationMatrix&, const FloatRect&);
    virtual void endClip();
    virtual void setGraphicsContext(GraphicsContext*);
    virtual GraphicsContext* graphicsContext();
    virtual bool allowSurfaceForRoot() const { return false; }
    virtual PassRefPtr<BitmapTexture> createTexture();
    virtual IntSize viewportSize() const;
    virtual void beginPainting();
    virtual void endPainting();

    static PassOwnPtr<TextureMapper> create() { return adoptPtr(new TextureMapperCairo); }
private:
    GraphicsContext* m_context;
    RefPtr<BitmapTextureCairo> m_texture;
};

}
#endif
