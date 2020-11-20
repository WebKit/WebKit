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
#include "IntSize.h"
#include "TextureMapper.h"
#include "TextureMapperContextAttributes.h"
#include "TextureMapperGLHeaders.h"
#include "TransformationMatrix.h"
#include <array>
#include <wtf/Vector.h>

namespace WebCore {

class TextureMapperGLData;
class TextureMapperShaderProgram;
class FilterOperation;

// An OpenGL-ES2 implementation of TextureMapper.
class TextureMapperGL : public TextureMapper {
public:
    TextureMapperGL();
    virtual ~TextureMapperGL();

    enum Flag {
        NoFlag = 0x00,
        ShouldBlend = 0x01,
        ShouldFlipTexture = 0x02,
        ShouldUseARBTextureRect = 0x04,
        ShouldAntialias = 0x08,
        ShouldRotateTexture90 = 0x10,
        ShouldRotateTexture180 = 0x20,
        ShouldRotateTexture270 = 0x40,
        ShouldConvertTextureBGRAToRGBA = 0x80,
        ShouldConvertTextureARGBToRGBA = 0x100,
        ShouldNotBlend = 0x200,
        ShouldUseExternalOESTextureRect = 0x400
    };

    typedef int Flags;

    // TextureMapper implementation
    void drawBorder(const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    void drawNumber(int number, const Color&, const FloatPoint&, const TransformationMatrix&) override;
    void drawTexture(const BitmapTexture&, const FloatRect&, const TransformationMatrix&, float opacity, unsigned exposedEdges) override;
    virtual void drawTexture(GLuint texture, Flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges = AllEdges);
    void drawTexturePlanarYUV(const std::array<GLuint, 3>& textures, const std::array<GLfloat, 9>& yuvToRgbMatrix, Flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges = AllEdges);
    void drawTextureSemiPlanarYUV(const std::array<GLuint, 2>& textures, bool uvReversed, const std::array<GLfloat, 9>& yuvToRgbMatrix, Flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges = AllEdges);
    void drawTexturePackedYUV(GLuint texture, const std::array<GLfloat, 9>& yuvToRgbMatrix, Flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges = AllEdges);
    void drawSolidColor(const FloatRect&, const TransformationMatrix&, const Color&, bool) override;
    void clearColor(const Color&) override;

    void bindSurface(BitmapTexture* surface) override;
    BitmapTexture* currentSurface();
    void beginClip(const TransformationMatrix&, const FloatRoundedRect&) override;
    void beginPainting(PaintFlags = 0) override;
    void endPainting() override;
    void endClip() override;
    IntRect clipBounds() override;
    IntSize maxTextureSize() const override { return IntSize(2000, 2000); }
    Ref<BitmapTexture> createTexture() override { return createTexture(GL_DONT_CARE); }
    Ref<BitmapTexture> createTexture(GLint internalFormat) override;

    void drawFiltered(const BitmapTexture& sourceTexture, const BitmapTexture* contentTexture, const FilterOperation&, int pass);

    void setEnableEdgeDistanceAntialiasing(bool enabled) { m_enableEdgeDistanceAntialiasing = enabled; }
    void drawTextureExternalOES(GLuint texture, Flags, const IntSize&, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);

private:
    void drawTexturedQuadWithProgram(TextureMapperShaderProgram&, uint32_t texture, Flags, const IntSize&, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);
    void drawTexturedQuadWithProgram(TextureMapperShaderProgram&, const Vector<std::pair<GLuint, GLuint> >& texturesAndSamplers, Flags, const IntSize&, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);
    void draw(const FloatRect&, const TransformationMatrix& modelViewMatrix, TextureMapperShaderProgram&, GLenum drawingMode, Flags);

    void drawUnitRect(TextureMapperShaderProgram&, GLenum drawingMode);
    void drawEdgeTriangles(TextureMapperShaderProgram&);

    bool beginScissorClip(const TransformationMatrix&, const FloatRect&);
    bool beginRoundedRectClip(const TransformationMatrix&, const FloatRoundedRect&);
    void bindDefaultSurface();
    ClipStack& clipStack();
    inline TextureMapperGLData& data() { return *m_data; }

    TextureMapperContextAttributes m_contextAttributes;
    TextureMapperGLData* m_data;
    ClipStack m_clipStack;
    bool m_enableEdgeDistanceAntialiasing;
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)

#endif // TextureMapperGL_h
