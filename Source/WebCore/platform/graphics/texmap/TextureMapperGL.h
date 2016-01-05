/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2015 Igalia S.L.

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

#if USE(TEXTURE_MAPPER_GL)

#include "ClipStack.h"
#include "FilterOperation.h"
#include "FloatQuad.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"
#include "TextureMapper.h"
#include "TransformationMatrix.h"

namespace WebCore {

struct TextureMapperGLData;
class TextureMapperShaderProgram;
class FilterOperation;

// An OpenGL-ES2 implementation of TextureMapper.
class TextureMapperGL : public TextureMapper {
public:
    TextureMapperGL();
    virtual ~TextureMapperGL();

    enum Flag {
        ShouldBlend = 0x01,
        ShouldFlipTexture = 0x02,
        ShouldUseARBTextureRect = 0x04,
        ShouldAntialias = 0x08
    };

    typedef int Flags;

    // TextureMapper implementation
    virtual void drawBorder(const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    virtual void drawNumber(int number, const Color&, const FloatPoint&, const TransformationMatrix&) override;
    virtual void drawTexture(const BitmapTexture&, const FloatRect&, const TransformationMatrix&, float opacity, unsigned exposedEdges) override;
    virtual void drawTexture(Platform3DObject texture, Flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges = AllEdges);
    virtual void drawSolidColor(const FloatRect&, const TransformationMatrix&, const Color&) override;

    virtual void bindSurface(BitmapTexture* surface) override;
    BitmapTexture* currentSurface();
    virtual void beginClip(const TransformationMatrix&, const FloatRect&) override;
    virtual void beginPainting(PaintFlags = 0) override;
    virtual void endPainting() override;
    virtual void endClip() override;
    virtual IntRect clipBounds() override;
    virtual IntSize maxTextureSize() const override { return IntSize(2000, 2000); }
    virtual PassRefPtr<BitmapTexture> createTexture() override;
    inline GraphicsContext3D* graphicsContext3D() const { return m_context3D.get(); }

    void drawFiltered(const BitmapTexture& sourceTexture, const BitmapTexture* contentTexture, const FilterOperation&, int pass);

    void setEnableEdgeDistanceAntialiasing(bool enabled) { m_enableEdgeDistanceAntialiasing = enabled; }

private:
    void drawTexturedQuadWithProgram(TextureMapperShaderProgram*, uint32_t texture, Flags, const IntSize&, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);
    void draw(const FloatRect&, const TransformationMatrix& modelViewMatrix, TextureMapperShaderProgram*, GC3Denum drawingMode, Flags);

    void drawUnitRect(TextureMapperShaderProgram*, GC3Denum drawingMode);
    void drawEdgeTriangles(TextureMapperShaderProgram*);

    bool beginScissorClip(const TransformationMatrix&, const FloatRect&);
    void bindDefaultSurface();
    ClipStack& clipStack();
    inline TextureMapperGLData& data() { return *m_data; }
    RefPtr<GraphicsContext3D> m_context3D;
    TextureMapperGLData* m_data;
    ClipStack m_clipStack;
    bool m_enableEdgeDistanceAntialiasing;
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)

#endif // TextureMapperGL_h
