/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FontCascade.h"

#if USE(DIRECT2D)

#include "AffineTransform.h"
#include "COMPtr.h"
#include "FloatConversion.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "GraphicsContextPlatformPrivateDirect2D.h"
#include "IntRect.h"
#include "PlatformContextDirect2D.h"
#include "WebCoreTextRenderer.h"
#include <d2d1.h>
#include <dwrite_3.h>
#include <wtf/MathExtras.h>

namespace WebCore {

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font, const GlyphBuffer& glyphBuffer,
    unsigned from, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode smoothingMode)
{
    auto context = graphicsContext.platformContext()->renderTarget();
    bool shouldUseFontSmoothing = WebCoreShouldUseFontSmoothing();

    switch (smoothingMode) {
    case FontSmoothingMode::Antialiased:
        graphicsContext.setShouldAntialias(true);
        shouldUseFontSmoothing = false;
        break;
    case FontSmoothingMode::SubpixelAntialiased:
        graphicsContext.setShouldAntialias(true);
        shouldUseFontSmoothing = true;
        break;
    case FontSmoothingMode::NoSmoothing:
        graphicsContext.setShouldAntialias(false);
        shouldUseFontSmoothing = false;
        break;
    case FontSmoothingMode::AutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    const FontPlatformData& platformData = font.platformData();

    graphicsContext.save();

    D2D1_MATRIX_3X2_F matrix;
    context->GetTransform(&matrix);

    if (platformData.syntheticOblique()) {
        static float skew = -tanf(syntheticObliqueAngle() * piFloat / 180.0f);
        auto skewMatrix = D2D1::Matrix3x2F::Skew(skew, 0);
        context->SetTransform(matrix * skewMatrix);
    }

    RELEASE_ASSERT(platformData.dwFont());
    RELEASE_ASSERT(platformData.dwFontFace());

    Vector<FLOAT> horizontalAdvances(numGlyphs);
    Vector<DWRITE_GLYPH_OFFSET> offsetAdvances(numGlyphs);
    for (unsigned i = 0; i < numGlyphs; ++i) {
        if (i + from >= glyphBuffer.advancesCount())
            break;

        auto advance = glyphBuffer.advances(i + from);
        if (!advance)
            continue;

        horizontalAdvances[i] = advance->width();
        offsetAdvances[i].advanceOffset = advance->width();
        offsetAdvances[i].ascenderOffset = advance->height();
    }

    DWRITE_GLYPH_RUN glyphRun;
    glyphRun.fontFace = platformData.dwFontFace();
    glyphRun.fontEmSize = platformData.size();
    glyphRun.glyphCount = numGlyphs;
    glyphRun.glyphIndices = glyphBuffer.glyphs(from);
    glyphRun.glyphAdvances = horizontalAdvances.data();
    glyphRun.glyphOffsets = nullptr;
    glyphRun.isSideways = platformData.orientation() == FontOrientation::Vertical;
    glyphRun.bidiLevel = 0;

    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    graphicsContext.getShadow(shadowOffset, shadowBlur, shadowColor);

    bool hasSimpleShadow = graphicsContext.textDrawingMode() == TextDrawingMode::Fill && shadowColor.isValid() && !shadowBlur && (!graphicsContext.shadowsIgnoreTransforms() || graphicsContext.getCTM().isIdentityOrTranslationOrFlipped());
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        graphicsContext.clearShadow();
        Color fillColor = graphicsContext.fillColor();
        Color shadowFillColor = shadowColor.colorWithAlphaMultipliedBy(fillColor.alphaAsFloat());
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (graphicsContext.shadowsIgnoreTransforms() ? -1 : 1);

        auto shadowBrush = graphicsContext.brushWithColor(shadowFillColor);
        context->DrawGlyphRun(D2D1::Point2F(shadowTextX, shadowTextY), &glyphRun, shadowBrush);
        if (font.syntheticBoldOffset())
            context->DrawGlyphRun(D2D1::Point2F(shadowTextX + font.syntheticBoldOffset(), shadowTextY), &glyphRun, shadowBrush);
    }

    context->DrawGlyphRun(D2D1::Point2F(point.x(), point.y()), &glyphRun, graphicsContext.solidFillBrush());
    if (font.syntheticBoldOffset())
        context->DrawGlyphRun(D2D1::Point2F(point.x() + font.syntheticBoldOffset(), point.y()), &glyphRun, graphicsContext.solidFillBrush());

    if (hasSimpleShadow)
        graphicsContext.setShadow(shadowOffset, shadowBlur, shadowColor);

    graphicsContext.restore();
}

}

#endif
