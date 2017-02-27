/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2014 Igalia S.L.

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

#ifndef BitmapTextureGL_h
#define BitmapTextureGL_h

#if USE(TEXTURE_MAPPER_GL)

#include "BitmapTexture.h"
#include "ClipStack.h"
#include "FilterOperation.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"
#include "TextureMapperGL.h"

namespace WebCore {

class TextureMapper;
class TextureMapperGL;
class FilterOperation;

class BitmapTextureGL : public BitmapTexture {
public:
    static Ref<BitmapTexture> create(Ref<GraphicsContext3D>&& context3D, const Flags flags = NoFlag)
    {
        return adoptRef(*new BitmapTextureGL(WTFMove(context3D), flags));
    }

    virtual ~BitmapTextureGL();

    IntSize size() const override;
    bool isValid() const override;
    void didReset() override;
    void bindAsSurface(GraphicsContext3D*);
    void initializeStencil();
    void initializeDepthBuffer();
    virtual uint32_t id() const { return m_id; }
    uint32_t textureTarget() const { return GraphicsContext3D::TEXTURE_2D; }
    IntSize textureSize() const { return m_textureSize; }
    void updateContents(Image*, const IntRect&, const IntPoint&, UpdateContentsFlag) override;
    void updateContents(const void*, const IntRect& target, const IntPoint& sourceOffset, int bytesPerLine, UpdateContentsFlag) override;
    void updateContentsNoSwizzle(const void*, const IntRect& target, const IntPoint& sourceOffset, int bytesPerLine, unsigned bytesPerPixel = 4, Platform3DObject glFormat = GraphicsContext3D::RGBA);
    bool isBackedByOpenGL() const override { return true; }

    PassRefPtr<BitmapTexture> applyFilters(TextureMapper&, const FilterOperations&) override;
    struct FilterInfo {
        RefPtr<FilterOperation> filter;
        unsigned pass;
        RefPtr<BitmapTexture> contentTexture;

        FilterInfo(PassRefPtr<FilterOperation> f = 0, unsigned p = 0, PassRefPtr<BitmapTexture> t = 0)
            : filter(f)
            , pass(p)
            , contentTexture(t)
            { }
    };
    const FilterInfo* filterInfo() const { return &m_filterInfo; }
    ClipStack& clipStack() { return m_clipStack; }

    GC3Dint internalFormat() const { return m_internalFormat; }

private:
    BitmapTextureGL(RefPtr<GraphicsContext3D>&&, const Flags);

    Platform3DObject m_id { 0 };
    IntSize m_textureSize;
    IntRect m_dirtyRect;
    Platform3DObject m_fbo { 0 };
    Platform3DObject m_rbo { 0 };
    Platform3DObject m_depthBufferObject { 0 };
    bool m_shouldClear { true };
    ClipStack m_clipStack;
    RefPtr<GraphicsContext3D> m_context3D;

    void clearIfNeeded();
    void createFboIfNeeded();

    FilterInfo m_filterInfo;

    GC3Dint m_internalFormat;
    GC3Denum m_format;
    GC3Denum m_type {
#if OS(DARWIN)
        GL_UNSIGNED_INT_8_8_8_8_REV
#else
        GraphicsContext3D::UNSIGNED_BYTE
#endif
    };
};

BitmapTextureGL* toBitmapTextureGL(BitmapTexture*);

}

#endif // USE(TEXTURE_MAPPER_GL)

#endif // BitmapTextureGL_h
