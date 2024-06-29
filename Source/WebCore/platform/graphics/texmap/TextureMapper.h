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

#pragma once

#if USE(TEXTURE_MAPPER)

#include "BitmapTexturePool.h"
#include "ClipStack.h"
#include "Color.h"
#include "FilterOperation.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TextureMapperGLHeaders.h"
#include "TransformationMatrix.h"
#include <array>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

// TextureMapper is a mechanism that enables hardware acceleration of CSS animations (accelerated compositing) without
// a need for a platform specific scene-graph library like CoreAnimation.

namespace WebCore {

class TextureMapperGLData;
class TextureMapperShaderProgram;
class FilterOperations;
class FloatRoundedRect;
enum class TextureMapperFlags : uint16_t;

class TextureMapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class WrapMode : uint8_t {
        Stretch,
        Repeat
    };

    WEBCORE_EXPORT static std::unique_ptr<TextureMapper> create();

    TextureMapper();
    WEBCORE_EXPORT ~TextureMapper();

    enum class AllEdgesExposed : bool { No, Yes };
    enum class FlipY : bool { No, Yes };

    WEBCORE_EXPORT void drawBorder(const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&);
    void drawNumber(int number, const Color&, const FloatPoint&, const TransformationMatrix&);

    WEBCORE_EXPORT void drawTexture(const BitmapTexture&, const FloatRect& target, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0f, AllEdgesExposed = AllEdgesExposed::Yes);
    void drawTexture(GLuint texture, OptionSet<TextureMapperFlags>, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, AllEdgesExposed = AllEdgesExposed::Yes);
    void drawTexturePlanarYUV(const std::array<GLuint, 3>& textures, const std::array<GLfloat, 16>& yuvToRgbMatrix, OptionSet<TextureMapperFlags>, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, std::optional<GLuint> alphaPlane, AllEdgesExposed = AllEdgesExposed::Yes);
    void drawTextureSemiPlanarYUV(const std::array<GLuint, 2>& textures, bool uvReversed, const std::array<GLfloat, 16>& yuvToRgbMatrix, OptionSet<TextureMapperFlags>, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, AllEdgesExposed = AllEdgesExposed::Yes);
    void drawTexturePackedYUV(GLuint texture, const std::array<GLfloat, 16>& yuvToRgbMatrix, OptionSet<TextureMapperFlags>, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, AllEdgesExposed = AllEdgesExposed::Yes);
    void drawTextureExternalOES(GLuint texture, OptionSet<TextureMapperFlags>, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);
    void drawSolidColor(const FloatRect&, const TransformationMatrix&, const Color&, bool);
    void clearColor(const Color&);

    // makes a surface the target for the following drawTexture calls.
    void bindSurface(BitmapTexture* surface);
    BitmapTexture* currentSurface();
    void beginClip(const TransformationMatrix&, const FloatRoundedRect&);
    WEBCORE_EXPORT void beginPainting(FlipY = FlipY::No, BitmapTexture* = nullptr);
    WEBCORE_EXPORT void endPainting();
    void endClip();
    IntRect clipBounds();
    IntSize maxTextureSize() const { return IntSize(2000, 2000); }
    void setDepthRange(double zNear, double zFar);
    void setMaskMode(bool m) { m_isMaskMode = m; }
    void setWrapMode(WrapMode m) { m_wrapMode = m; }
    void setPatternTransform(const TransformationMatrix& p) { m_patternTransform = p; }

    RefPtr<BitmapTexture> applyFilters(RefPtr<BitmapTexture>&, const FilterOperations&, bool defersLastPass);

    WEBCORE_EXPORT RefPtr<BitmapTexture> acquireTextureFromPool(const IntSize&, OptionSet<BitmapTexture::Flags>);

#if USE(GRAPHICS_LAYER_WC)
    WEBCORE_EXPORT void releaseUnusedTexturesNow();
#endif

private:
    bool isInMaskMode() const { return m_isMaskMode; }
    const TransformationMatrix& patternTransform() const { return m_patternTransform; }

    enum class Direction { X, Y };

    RefPtr<BitmapTexture> applyFilter(RefPtr<BitmapTexture>&, const Ref<const FilterOperation>&, bool defersLastPass);
    RefPtr<BitmapTexture> applyBlurFilter(RefPtr<BitmapTexture>&, const BlurFilterOperation&);
    RefPtr<BitmapTexture> applyDropShadowFilter(RefPtr<BitmapTexture>&, const DropShadowFilterOperation&);
    RefPtr<BitmapTexture> applySinglePassFilter(RefPtr<BitmapTexture>&, const Ref<const FilterOperation>&, bool shouldDefer);

    void drawTextureCopy(const BitmapTexture& sourceTexture, const FloatRect& sourceRect, const FloatRect& targetRect);
    void drawBlurred(const BitmapTexture& sourceTexture, const FloatRect&, float radius, Direction, bool alphaBlur = false);
    void drawFilterPass(const BitmapTexture& sourceTexture, const BitmapTexture* contentTexture, const FilterOperation&, int pass);
    void drawTexturedQuadWithProgram(TextureMapperShaderProgram&, uint32_t texture, OptionSet<TextureMapperFlags>, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);
    void drawTexturedQuadWithProgram(TextureMapperShaderProgram&, const Vector<std::pair<GLuint, GLuint> >& texturesAndSamplers, OptionSet<TextureMapperFlags>, const FloatRect&, const TransformationMatrix& modelViewMatrix, float opacity);
    void draw(const FloatRect&, const TransformationMatrix& modelViewMatrix, TextureMapperShaderProgram&, GLenum drawingMode, OptionSet<TextureMapperFlags>);
    void drawUnitRect(TextureMapperShaderProgram&, GLenum drawingMode);
    void drawEdgeTriangles(TextureMapperShaderProgram&);

    bool beginScissorClip(const TransformationMatrix&, const FloatRect&);
    bool beginRoundedRectClip(const TransformationMatrix&, const FloatRoundedRect&);
    void bindDefaultSurface();
    ClipStack& clipStack();
    TextureMapperGLData& data() { return *m_data; }

    void updateProjectionMatrix();

    BitmapTexturePool m_texturePool;
    bool m_isMaskMode { false };
    TransformationMatrix m_patternTransform;
    WrapMode m_wrapMode { WrapMode::Stretch };
    TextureMapperGLData* m_data;
    ClipStack m_clipStack;
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
