/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc.
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

#include "config.h"
#include "FontCascade.h"

#include "ComplexTextController.h"
#include "DashArray.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "Logging.h"
#include "RuntimeApplicationChecks.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CoreTextSPI.h>
#else
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

namespace WebCore {

FontCascade::FontCascade(const FontPlatformData& fontData, FontSmoothingMode fontSmoothingMode)
    : m_fonts(FontCascadeFonts::createForPlatformFont(fontData))
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
    m_fontDescription.setFontSmoothing(fontSmoothingMode);
    m_fontDescription.setSpecifiedSize(CTFontGetSize(fontData.ctFont()));
    m_fontDescription.setComputedSize(CTFontGetSize(fontData.ctFont()));
    m_fontDescription.setIsItalic(CTFontGetSymbolicTraits(fontData.ctFont()) & kCTFontTraitItalic);
    m_fontDescription.setWeight((CTFontGetSymbolicTraits(fontData.ctFont()) & kCTFontTraitBold) ? boldWeightValue() : normalWeightValue());
}

static const AffineTransform& rotateLeftTransform()
{
    static AffineTransform result(0, -1, 1, 0, 0, 0);
    return result;
}

AffineTransform computeOverallTextMatrix(const Font& font)
{
    auto& platformData = font.platformData();
    AffineTransform result;
    if (!platformData.isColorBitmapFont())
        result = CTFontGetMatrix(platformData.ctFont());
    result.setB(-result.b());
    result.setD(-result.d());
    if (platformData.syntheticOblique()) {
        float obliqueSkew = tanf(deg2rad(FontCascade::syntheticObliqueAngle()));
        if (platformData.orientation() == FontOrientation::Vertical) {
            if (font.isTextOrientationFallback())
                result = AffineTransform(1, obliqueSkew, 0, 1, 0, 0) * result;
            else
                result = AffineTransform(1, -obliqueSkew, 0, 1, 0, 0) * result;
        } else
            result = AffineTransform(1, 0, -obliqueSkew, 1, 0, 0) * result;
    }

    // We're emulating the behavior of CGContextSetTextPosition() by adding constant amounts to each glyph's position
    // (see fillVectorWithHorizontalGlyphPositions() and fillVectorWithVerticalGlyphPositions()).
    // CGContextSetTextPosition() has the side effect of clobbering the E and F fields of the text matrix,
    // so we do that explicitly here.
    result.setE(0);
    result.setF(0);
    return result;
}

AffineTransform computeVerticalTextMatrix(const Font& font, const AffineTransform& previousTextMatrix)
{
    ASSERT_UNUSED(font, font.platformData().orientation() == FontOrientation::Vertical);
    // The translation here ("e" and "f" fields) are irrelevant, because
    // this matrix is inverted in fillVectorWithVerticalGlyphPositions to place the glyphs in the CTM's coordinate system.
    // All we're trying to do here is rotate the text matrix so glyphs appear visually upright.
    // We have to include the previous text matrix because it includes things like synthetic oblique.
    return rotateLeftTransform() * previousTextMatrix;
}

#if !PLATFORM(WIN)

// Confusingly, even when CGFontRenderingGetFontSmoothingDisabled() returns true, CGContextSetShouldSmoothFonts() still impacts text
// rendering, which is why this function uses the "subpixel antialiasing" rather than "smoothing" terminology.
bool FontCascade::isSubpixelAntialiasingAvailable()
{
#if HAVE(CG_FONT_RENDERING_GET_FONT_SMOOTHING_DISABLED)
    static bool subpixelAntialiasingEnabled;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&]() {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        subpixelAntialiasingEnabled = !CGFontRenderingGetFontSmoothingDisabled();
        ALLOW_DEPRECATED_DECLARATIONS_END
    });
    return subpixelAntialiasingEnabled;
#else
    return false;
#endif
}

static void fillVectorWithHorizontalGlyphPositions(Vector<CGPoint, 256>& positions, CGContextRef context, const CGSize* advances, unsigned count, const FloatPoint& point)
{
    // Keep this in sync as the inverse of `DrawGlyphsRecorder::recordDrawGlyphs`.
    // The input positions are in the context's coordinate system, without the text matrix.
    // However, the positions that CT/CG accept are in the text matrix's coordinate system.
    // CGContextGetTextMatrix() gives us the matrix that maps from text's coordinate system to the context's (non-text) coordinate system.
    // We need to figure out what to deliver CT, inside the text's coordinate system, such that it ends up coincident with the input in the context's coordinate system.
    //
    // CTM * text matrix * positions we need to deliver to CT = CTM * input positions
    // Solving for the positions we need to deliver to CT, we get
    // positions we need to deliver to CT = inverse(text matrix) * input positions
    CGAffineTransform matrix = CGAffineTransformInvert(CGContextGetTextMatrix(context));
    positions[0] = CGPointApplyAffineTransform(point, matrix);
    for (unsigned i = 1; i < count; ++i) {
        CGSize advance = CGSizeApplyAffineTransform(advances[i - 1], matrix);
        positions[i].x = positions[i - 1].x + advance.width;
        positions[i].y = positions[i - 1].y + advance.height;
    }
}

static void fillVectorWithVerticalGlyphPositions(Vector<CGPoint, 256>& positions, CGContextRef context, const CGSize* translations, const CGSize* advances, unsigned count, const FloatPoint& point, float ascentDelta)
{
    // Keep this in sync as the inverse of `DrawGlyphsRecorder::recordDrawGlyphs`.
    CGAffineTransform transform = CGAffineTransformInvert(CGContextGetTextMatrix(context));

    auto position = CGPointMake(point.x(), point.y() + ascentDelta);
    for (unsigned i = 0; i < count; ++i) {
        CGSize translation = CGSizeApplyAffineTransform(translations[i], rotateLeftTransform());
        positions[i] = CGPointApplyAffineTransform(CGPointMake(position.x - translation.width, position.y + translation.height), transform);
        position.x += advances[i].width;
        position.y += advances[i].height;
    }
}

static void showGlyphsWithAdvances(const FloatPoint& point, const Font& font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, unsigned count, const AffineTransform& textMatrix)
{
    if (!count)
        return;

    const FontPlatformData& platformData = font.platformData();
    Vector<CGPoint, 256> positions(count);
    if (platformData.orientation() == FontOrientation::Vertical) {
        ScopedTextMatrix savedMatrix(computeVerticalTextMatrix(font, textMatrix), context);

        Vector<CGSize, 256> translations(count);
        CTFontGetVerticalTranslationsForGlyphs(platformData.ctFont(), glyphs, translations.data(), count);

        auto ascentDelta = font.fontMetrics().floatAscent(IdeographicBaseline) - font.fontMetrics().floatAscent();
        fillVectorWithVerticalGlyphPositions(positions, context, translations.data(), advances, count, point, ascentDelta);
        CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
    } else {
        fillVectorWithHorizontalGlyphPositions(positions, context, advances, count, point);
        CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
    }
}

static void setCGFontRenderingMode(GraphicsContext& context)
{
    CGContextRef cgContext = context.platformContext();
    CGContextSetShouldAntialiasFonts(cgContext, true);

    CGAffineTransform contextTransform = CGContextGetCTM(cgContext);
    bool isTranslationOrIntegralScale = WTF::isIntegral(contextTransform.a) && WTF::isIntegral(contextTransform.d) && contextTransform.b == 0.f && contextTransform.c == 0.f;
    bool isRotated = ((contextTransform.b || contextTransform.c) && (contextTransform.a || contextTransform.d));
    bool doSubpixelQuantization = isTranslationOrIntegralScale || (!isRotated && context.shouldSubpixelQuantizeFonts());

    CGContextSetShouldSubpixelPositionFonts(cgContext, true);
    CGContextSetShouldSubpixelQuantizeFonts(cgContext, doSubpixelQuantization);
}

void FontCascade::drawGlyphs(GraphicsContext& context, const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode smoothingMode)
{
    const auto& platformData = font.platformData();
    if (!platformData.size())
        return;

    if (isInGPUProcess() && font.hasAnyComplexColorFormatGlyphs(glyphs, numGlyphs)) {
        ASSERT_NOT_REACHED();
        return;
    }

    CGContextRef cgContext = context.platformContext();

    bool shouldAntialias = true;
    bool shouldSmoothFonts = true;

    if (!font.allowsAntialiasing())
        smoothingMode = FontSmoothingMode::NoSmoothing;

    switch (smoothingMode) {
    case FontSmoothingMode::Antialiased:
        shouldSmoothFonts = false;
        break;
    case FontSmoothingMode::AutoSmoothing:
    case FontSmoothingMode::SubpixelAntialiased:
        shouldAntialias = true;
        break;
    case FontSmoothingMode::NoSmoothing:
        shouldAntialias = false;
        shouldSmoothFonts = false;
        break;
    }

    if (!shouldUseSmoothing())
        shouldSmoothFonts = false;

#if !PLATFORM(IOS_FAMILY)
    bool originalShouldUseFontSmoothing = CGContextGetShouldSmoothFonts(cgContext);
    if (shouldSmoothFonts != originalShouldUseFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, shouldSmoothFonts);
#endif

    bool originalShouldAntialias = CGContextGetShouldAntialias(cgContext);
    if (shouldAntialias != originalShouldAntialias)
        CGContextSetShouldAntialias(cgContext, shouldAntialias);

    FloatPoint point = anchorPoint;

    auto textMatrix = computeOverallTextMatrix(font);
    ScopedTextMatrix restorer(textMatrix, cgContext);

    setCGFontRenderingMode(context);
    CGContextSetFontSize(cgContext, platformData.size());

    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    context.getShadow(shadowOffset, shadowBlur, shadowColor);

    AffineTransform contextCTM = context.getCTM();
    float syntheticBoldOffset = font.syntheticBoldOffset();
    if (syntheticBoldOffset && !contextCTM.isIdentityOrTranslationOrFlipped()) {
        FloatSize horizontalUnitSizeInDevicePixels = contextCTM.mapSize(FloatSize(1, 0));
        float horizontalUnitLengthInDevicePixels = sqrtf(horizontalUnitSizeInDevicePixels.width() * horizontalUnitSizeInDevicePixels.width() + horizontalUnitSizeInDevicePixels.height() * horizontalUnitSizeInDevicePixels.height());
        if (horizontalUnitLengthInDevicePixels) {
            // Make sure that a scaled down context won't blow up the gap between the glyphs.
            syntheticBoldOffset = std::min(syntheticBoldOffset, syntheticBoldOffset / horizontalUnitLengthInDevicePixels);
        }
    };

    bool hasSimpleShadow = context.textDrawingMode() == TextDrawingMode::Fill && shadowColor.isValid() && !shadowBlur && !platformData.isColorBitmapFont() && (!context.shadowsIgnoreTransforms() || contextCTM.isIdentityOrTranslationOrFlipped()) && !context.isInTransparencyLayer();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context.clearShadow();
        Color fillColor = context.fillColor();
        Color shadowFillColor = shadowColor.colorWithAlphaMultipliedBy(fillColor.alphaAsFloat());
        context.setFillColor(shadowFillColor);
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (context.shadowsIgnoreTransforms() ? -1 : 1);
        showGlyphsWithAdvances(FloatPoint(shadowTextX, shadowTextY), font, cgContext, glyphs, advances, numGlyphs, textMatrix);
        if (syntheticBoldOffset)
            showGlyphsWithAdvances(FloatPoint(shadowTextX + syntheticBoldOffset, shadowTextY), font, cgContext, glyphs, advances, numGlyphs, textMatrix);
        context.setFillColor(fillColor);
    }

    showGlyphsWithAdvances(point, font, cgContext, glyphs, advances, numGlyphs, textMatrix);

    if (syntheticBoldOffset)
        showGlyphsWithAdvances(FloatPoint(point.x() + syntheticBoldOffset, point.y()), font, cgContext, glyphs, advances, numGlyphs, textMatrix);

    if (hasSimpleShadow)
        context.setShadow(shadowOffset, shadowBlur, shadowColor);

#if !PLATFORM(IOS_FAMILY)
    if (shouldSmoothFonts != originalShouldUseFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
#endif

    if (shouldAntialias != originalShouldAntialias)
        CGContextSetShouldAntialias(cgContext, originalShouldAntialias);
}

bool FontCascade::primaryFontIsSystemFont() const
{
    const auto& fontData = primaryFont();
    return isSystemFont(fontData.platformData().ctFont());
}

// FIXME: Use this on all ports.
const Font* FontCascade::fontForCombiningCharacterSequence(const UChar* characters, size_t length) const
{
    UChar32 baseCharacter;
    size_t baseCharacterLength = 0;
    U16_NEXT(characters, baseCharacterLength, length, baseCharacter);

    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacter, false, NormalVariant);

    if (!baseCharacterGlyphData.glyph)
        return nullptr;

    if (length == baseCharacterLength)
        return baseCharacterGlyphData.font;

    bool triedBaseCharacterFont = false;

    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        const Font* font = fallbackRangesAt(i).fontForCharacter(baseCharacter);
        if (!font)
            continue;
#if PLATFORM(IOS_FAMILY)
        if (baseCharacter >= 0x0600 && baseCharacter <= 0x06ff && font->shouldNotBeUsedForArabic())
            continue;
#endif
        if (font->platformData().orientation() == FontOrientation::Vertical) {
            if (isCJKIdeographOrSymbol(baseCharacter)) {
                if (!font->hasVerticalGlyphs())
                    font = &font->brokenIdeographFont();
            } else if (m_fontDescription.nonCJKGlyphOrientation() == NonCJKGlyphOrientation::Mixed) {
                const Font& verticalRightFont = font->verticalRightOrientationFont();
                Glyph verticalRightGlyph = verticalRightFont.glyphForCharacter(baseCharacter);
                if (verticalRightGlyph == baseCharacterGlyphData.glyph)
                    font = &verticalRightFont;
            } else {
                const Font& uprightFont = font->uprightOrientationFont();
                Glyph uprightGlyph = uprightFont.glyphForCharacter(baseCharacter);
                if (uprightGlyph != baseCharacterGlyphData.glyph)
                    font = &uprightFont;
            }
        }

        if (font == baseCharacterGlyphData.font)
            triedBaseCharacterFont = true;

        if (font->canRenderCombiningCharacterSequence(characters, length))
            return font;
    }

    if (!triedBaseCharacterFont && baseCharacterGlyphData.font && baseCharacterGlyphData.font->canRenderCombiningCharacterSequence(characters, length))
        return baseCharacterGlyphData.font;

    return Font::systemFallback();
}

#endif

}
