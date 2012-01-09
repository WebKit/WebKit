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

#ifndef TextureMapper_h
#define TextureMapper_h

#if USE(ACCELERATED_COMPOSITING)
#if (defined(QT_OPENGL_LIB))
    #if defined(QT_OPENGL_ES_2) && !defined(TEXMAP_OPENGL_ES_2)
        #define TEXMAP_OPENGL_ES_2
    #endif
#endif

#include "GraphicsContext.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TextureMapperPlatformLayer.h"
#include "TransformationMatrix.h"
#include <wtf/UnusedParam.h>

/*
    TextureMapper is a mechanism that enables hardware acceleration of CSS animations (accelerated compositing) without
    a need for a platform specific scene-graph library like CoreAnimations or QGraphicsView.
*/

namespace WebCore {

class TextureMapper;

// A 2D texture that can be the target of software or GL rendering.
class BitmapTexture  : public RefCounted<BitmapTexture> {
public:
    enum PixelFormat { BGRAFormat, RGBAFormat, BGRFormat, RGBFormat };
    BitmapTexture() : m_lockCount(0) {}
    virtual ~BitmapTexture() { }
    virtual void destroy() { }

    virtual bool allowOfflineTextureUpload() const { return false; }
    virtual IntSize size() const = 0;
    virtual int bpp() const { return 32; }
    virtual bool isValid() const = 0;
    virtual void reset(const IntSize& size, bool opaque = false)
    {
        m_isOpaque = opaque;
        m_contentSize = size;
    }

    virtual void pack() { }
    virtual void unpack() { }
    virtual bool isPacked() const { return false; }

    virtual PlatformGraphicsContext* beginPaint(const IntRect& dirtyRect) = 0;
    virtual void endPaint() = 0;

    // For performance reasons, BitmapTexture might modify the bits directly (swizzle).
    // Thus, this method is only recommended for buffer update, such as used by WebKit2.
    virtual void updateContents(PixelFormat, const IntRect&, void* bits) = 0;
    virtual PlatformGraphicsContext* beginPaintMedia()
    {
        return beginPaint(IntRect(0, 0, size().width(), size().height()));
    }
    virtual void setContentsToImage(Image*) = 0;
    virtual bool save(const String&) { return false; }

    inline void lock() { ++m_lockCount; }
    inline void unlock() { --m_lockCount; }
    inline bool isLocked() { return m_lockCount; }
    inline IntSize contentSize() const { return m_contentSize; }
    inline int numberOfBytes() const { return size().width() * size().height() * bpp() >> 3; }

protected:
    int m_lockCount;
    IntSize m_contentSize;
    bool m_isOpaque;
};

// A "context" class used to encapsulate accelerated texture mapping functions: i.e. drawing a texture
// onto the screen or into another texture with a specified transform, opacity and mask.
class TextureMapper {
    friend class BitmapTexture;

public:
    static PassOwnPtr<TextureMapper> create(GraphicsContext* graphicsContext = 0);
    virtual ~TextureMapper() { }

    virtual void drawTexture(const BitmapTexture&, const FloatRect& target, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0f, const BitmapTexture* maskTexture = 0) = 0;

    // makes a surface the target for the following drawTexture calls.
    virtual void bindSurface(BitmapTexture* surface) = 0;
    virtual void setGraphicsContext(GraphicsContext*) = 0;
    virtual GraphicsContext* graphicsContext() = 0;
    virtual void beginClip(const TransformationMatrix&, const FloatRect&) = 0;
    virtual void endClip() = 0;
    virtual bool allowSurfaceForRoot() const = 0;
    virtual PassRefPtr<BitmapTexture> createTexture() = 0;
    IntSize viewportSize() const { return m_viewportSize; }
    void setViewportSize(const IntSize& s) { m_viewportSize = s; }

    void setImageInterpolationQuality(InterpolationQuality quality) { m_interpolationQuality = quality; }
    void setTextDrawingMode(TextDrawingModeFlags mode) { m_textDrawingMode = mode; }

    InterpolationQuality imageInterpolationQuality() const { return m_interpolationQuality; }
    TextDrawingModeFlags textDrawingMode() const { return m_textDrawingMode; }
    virtual bool allowPartialUpdates() const { return false; }
    virtual bool isOpenGLBacked() const { return false; }

    void setTransform(const TransformationMatrix& matrix) { m_transform = matrix; }
    TransformationMatrix transform() const { return m_transform; }

    virtual void beginPainting() { }
    virtual void endPainting() { }

    virtual void releaseTextureToPool(BitmapTexture* surface);
    virtual PassRefPtr<BitmapTexture> acquireTextureFromPool(const IntSize&);


protected:
    TextureMapper()
        : m_interpolationQuality(InterpolationDefault)
        , m_textDrawingMode(TextModeFill)
    {}

private:
    InterpolationQuality m_interpolationQuality;
    TextDrawingModeFlags m_textDrawingMode;
    TransformationMatrix m_transform;
    IntSize m_viewportSize;
    Vector<RefPtr<BitmapTexture> > m_texturePool;
};

};

#endif

#endif
