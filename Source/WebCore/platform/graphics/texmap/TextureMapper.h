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

#if PLATFORM(QT)
#include <qglobal.h>

#if defined(QT_OPENGL_LIB) || HAVE(QT5)
    #if defined(QT_OPENGL_ES_2) && !defined(TEXMAP_OPENGL_ES_2)
        #define TEXMAP_OPENGL_ES_2
    #endif
#endif
#endif

#include "FilterOperations.h"
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
    enum Flag {
        SupportsAlpha = 0x01
    };

    typedef unsigned Flags;

    BitmapTexture()
        : m_flags(0)
    {
    }

    virtual ~BitmapTexture() { }
    virtual bool isBackedByOpenGL() const { return false; }

    virtual IntSize size() const = 0;
    virtual void updateContents(Image*, const IntRect&, const IntPoint& offset) = 0;
    virtual void updateContents(const void*, const IntRect& target, const IntPoint& offset, int bytesPerLine) = 0;
    virtual bool isValid() const = 0;
    inline Flags flags() const { return m_flags; }

    virtual int bpp() const { return 32; }
    virtual bool canReuseWith(const IntSize& contentsSize, Flags flags = 0) { return false; }
    void reset(const IntSize& size, Flags flags = 0)
    {
        m_flags = flags;
        m_contentSize = size;
        didReset();
    }
    virtual void didReset() { }

    inline IntSize contentSize() const { return m_contentSize; }
    inline int numberOfBytes() const { return size().width() * size().height() * bpp() >> 3; }
    inline bool isOpaque() const { return !(m_flags & SupportsAlpha); }

#if ENABLE(CSS_FILTERS)
    virtual PassRefPtr<BitmapTexture> applyFilters(const BitmapTexture& contentTexture, const FilterOperations&) { return this; }
#endif

protected:
    IntSize m_contentSize;

private:
    Flags m_flags;
};

// A "context" class used to encapsulate accelerated texture mapping functions: i.e. drawing a texture
// onto the screen or into another texture with a specified transform, opacity and mask.
class TextureMapper {
    friend class BitmapTexture;

public:
    enum AccelerationMode { SoftwareMode, OpenGLMode };
    enum PaintFlag {
        PaintingMirrored = 1 << 0,
    };
    typedef unsigned PaintFlags;

    static PassOwnPtr<TextureMapper> create(AccelerationMode newMode = SoftwareMode);
    virtual ~TextureMapper() { }

    enum ExposedEdges {
        NoEdges = 0,
        LeftEdge = 1 << 0,
        RightEdge = 1 << 1,
        TopEdge = 1 << 2,
        BottomEdge = 1 << 3,
        AllEdges = LeftEdge | RightEdge | TopEdge | BottomEdge,
    };

    virtual void drawBorder(const Color&, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix = TransformationMatrix()) = 0;
    virtual void drawRepaintCounter(int value, int pointSize, const FloatPoint&, const TransformationMatrix& modelViewMatrix = TransformationMatrix()) = 0;
    virtual void drawTexture(const BitmapTexture&, const FloatRect& target, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0f, const BitmapTexture* maskTexture = 0, unsigned exposedEdges = AllEdges) = 0;

    // makes a surface the target for the following drawTexture calls.
    virtual void bindSurface(BitmapTexture* surface) = 0;
    virtual void setGraphicsContext(GraphicsContext* context) { m_context = context; }
    virtual GraphicsContext* graphicsContext() { return m_context; }
    virtual void beginClip(const TransformationMatrix&, const FloatRect&) = 0;
    virtual void endClip() = 0;
    virtual PassRefPtr<BitmapTexture> createTexture() = 0;

    void setImageInterpolationQuality(InterpolationQuality quality) { m_interpolationQuality = quality; }
    void setTextDrawingMode(TextDrawingModeFlags mode) { m_textDrawingMode = mode; }

    InterpolationQuality imageInterpolationQuality() const { return m_interpolationQuality; }
    TextDrawingModeFlags textDrawingMode() const { return m_textDrawingMode; }
    AccelerationMode accelerationMode() const { return m_accelerationMode; }

    virtual void beginPainting(PaintFlags flags = 0) { }
    virtual void endPainting() { }

    virtual IntSize maxTextureSize() const { return IntSize(INT_MAX, INT_MAX); }

    // A surface is released implicitly when dereferenced.
    virtual PassRefPtr<BitmapTexture> acquireTextureFromPool(const IntSize&);

protected:
    TextureMapper(AccelerationMode accelerationMode)
        : m_interpolationQuality(InterpolationDefault)
        , m_textDrawingMode(TextModeFill)
        , m_accelerationMode(accelerationMode)
    {}

private:
#if USE(TEXTURE_MAPPER_GL)
    static PassOwnPtr<TextureMapper> platformCreateAccelerated();
#else
    static PassOwnPtr<TextureMapper> platformCreateAccelerated()
    {
        return PassOwnPtr<TextureMapper>();
    }
#endif
    InterpolationQuality m_interpolationQuality;
    TextDrawingModeFlags m_textDrawingMode;
    Vector<RefPtr<BitmapTexture> > m_texturePool;
    GraphicsContext* m_context;
    AccelerationMode m_accelerationMode;
};

}

#endif

#endif
