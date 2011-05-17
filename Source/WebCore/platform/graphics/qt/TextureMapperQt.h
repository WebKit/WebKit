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

#include "texmap/TextureMapper.h"

#ifndef TextureMapperQt_h
#define TextureMapperQt_h

namespace WebCore {

class BitmapTextureQt : public BitmapTexture {
    friend class TextureMapperQt;
public:
    BitmapTextureQt();
    ~BitmapTextureQt() { destroy(); }
    virtual void destroy();
    virtual IntSize size() const { return IntSize(m_pixmap.width(), m_pixmap.height()); }
    virtual void reset(const IntSize&, bool opaque);
    virtual PlatformGraphicsContext* beginPaint(const IntRect& dirtyRect);
    virtual void endPaint();
    virtual void setContentsToImage(Image*);
    virtual bool save(const String& path);
    virtual bool isValid() const { return !m_pixmap.isNull() || !m_image.isNull(); }
    IntRect sourceRect() const { return IntRect(0, 0, contentSize().width(), contentSize().height()); }
    virtual void pack();
    virtual void unpack();
    virtual bool isPacked() const { return m_isPacked; }

    QPainter* painter() { return &m_painter; }

private:
    QPainter m_painter;
    QPixmap m_pixmap;
    QImage m_image;
    bool m_isPacked;
};

class TextureMapperQt : public TextureMapper {
public:
    TextureMapperQt();

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

    static void initialize(QPainter* painter)
    {
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, false);
    }

    static PassOwnPtr<TextureMapper> create() { return adoptPtr(new TextureMapperQt); }
private:
    inline QPainter* currentPainter() { return m_currentSurface ? m_currentSurface->painter() : m_painter; }

    QPainter* m_painter;
    GraphicsContext* m_context;
    RefPtr<BitmapTextureQt> m_currentSurface;
};

}
#endif
