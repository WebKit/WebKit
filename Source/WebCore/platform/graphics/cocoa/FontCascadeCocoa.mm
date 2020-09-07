/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006-2011, 2016 Apple Inc.
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

#import "config.h"
#import "FontCascade.h"

#import "ComplexTextController.h"
#import "DashArray.h"
#import "Font.h"
#import "GlyphBuffer.h"
#import "GraphicsContext.h"
#import "LayoutRect.h"
#import "Logging.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/CoreTextSPI.h>
#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif
#import <wtf/MathExtras.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/CoreUISPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CoreUI)
SOFT_LINK_CLASS(CoreUI, CUICatalog)
SOFT_LINK_CLASS(CoreUI, CUIStyleEffectConfiguration)
#endif

namespace WebCore {

// Confusingly, even when CGFontRenderingGetFontSmoothingDisabled() returns true, CGContextSetShouldSmoothFonts() still impacts text
// rendering, which is why this function uses the "subpixel antialiasing" rather than "smoothing" terminology.
bool FontCascade::isSubpixelAntialiasingAvailable()
{
#if HAVE(CG_FONT_RENDERING_GET_FONT_SMOOTHING_DISABLED)
    static bool subpixelAntialiasingEnabled;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] () {
        subpixelAntialiasingEnabled = !CGFontRenderingGetFontSmoothingDisabled();
    });
    return subpixelAntialiasingEnabled;
#elif PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return true;
}

static inline void fillVectorWithHorizontalGlyphPositions(Vector<CGPoint, 256>& positions, CGContextRef context, const CGSize* advances, unsigned count)
{
    CGAffineTransform matrix = CGAffineTransformInvert(CGContextGetTextMatrix(context));
    positions[0] = CGPointZero;
    for (unsigned i = 1; i < count; ++i) {
        CGSize advance = CGSizeApplyAffineTransform(advances[i - 1], matrix);
        positions[i].x = positions[i - 1].x + advance.width;
        positions[i].y = positions[i - 1].y + advance.height;
    }
}

static inline bool shouldUseLetterpressEffect(const GraphicsContext& context)
{
#if ENABLE(LETTERPRESS)
    return context.textDrawingMode().contains(TextDrawingMode::Letterpress);
#else
    UNUSED_PARAM(context);
    return false;
#endif
}

static void showLetterpressedGlyphsWithAdvances(const FloatPoint& point, const Font& font, GraphicsContext& coreContext, const CGGlyph* glyphs, const CGSize* advances, unsigned count)
{
#if ENABLE(LETTERPRESS)
    if (!count)
        return;

    const FontPlatformData& platformData = font.platformData();
    if (platformData.orientation() == FontOrientation::Vertical) {
        // FIXME: Implement support for vertical text. See <rdar://problem/13737298>.
        return;
    }

    CGContextRef context = coreContext.platformContext();

    CGContextSetTextPosition(context, point.x(), point.y());
    Vector<CGPoint, 256> positions(count);
    fillVectorWithHorizontalGlyphPositions(positions, context, advances, count);

    CTFontRef ctFont = platformData.ctFont();
    CGContextSetFontSize(context, CTFontGetSize(ctFont));

    static CUICatalog *catalog = PAL::softLink_UIKit__UIKitGetTextEffectsCatalog();
    if (!catalog)
        return;

    static CUIStyleEffectConfiguration *styleConfiguration;
    if (!styleConfiguration) {
        styleConfiguration = [allocCUIStyleEffectConfigurationInstance() init];
        styleConfiguration.useSimplifiedEffect = YES;
    }

#if HAVE(OS_DARK_MODE_SUPPORT)
    styleConfiguration.appearanceName = coreContext.useDarkAppearance() ? @"UIAppearanceDark" : @"UIAppearanceLight";
#endif

    CGContextSetFont(context, adoptCF(CTFontCopyGraphicsFont(ctFont, nullptr)).get());
    CGContextSetFontSize(context, platformData.size());

    [catalog drawGlyphs:glyphs atPositions:positions.data() inContext:context withFont:ctFont count:count stylePresetName:@"_UIKitNewLetterpressStyle" styleConfiguration:styleConfiguration foregroundColor:CGContextGetFillColorAsColor(context)];

    CGContextSetFont(context, nullptr);
    CGContextSetFontSize(context, 0);
#else
    UNUSED_PARAM(point);
    UNUSED_PARAM(font);
    UNUSED_PARAM(coreContext);
    UNUSED_PARAM(glyphs);
    UNUSED_PARAM(advances);
    UNUSED_PARAM(count);
#endif
}

static void showGlyphsWithAdvances(const FloatPoint& point, const Font& font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, unsigned count)
{
    if (!count)
        return;

    CGContextSetTextPosition(context, point.x(), point.y());

    const FontPlatformData& platformData = font.platformData();
    Vector<CGPoint, 256> positions(count);
    if (platformData.orientation() == FontOrientation::Vertical) {
        CGAffineTransform rotateLeftTransform = CGAffineTransformMake(0, -1, 1, 0, 0, 0);
        CGAffineTransform textMatrix = CGContextGetTextMatrix(context);
        CGAffineTransform runMatrix = CGAffineTransformConcat(textMatrix, rotateLeftTransform);
        ScopedTextMatrix savedMatrix(runMatrix, context);

        Vector<CGSize, 256> translations(count);
        CTFontGetVerticalTranslationsForGlyphs(platformData.ctFont(), glyphs, translations.data(), count);

        CGAffineTransform transform = CGAffineTransformInvert(CGContextGetTextMatrix(context));

        CGPoint position = FloatPoint(point.x(), point.y() + font.fontMetrics().floatAscent(IdeographicBaseline) - font.fontMetrics().floatAscent());
        for (unsigned i = 0; i < count; ++i) {
            CGSize translation = CGSizeApplyAffineTransform(translations[i], rotateLeftTransform);
            positions[i] = CGPointApplyAffineTransform(CGPointMake(position.x - translation.width, position.y + translation.height), transform);
            position.x += advances[i].width;
            position.y += advances[i].height;
        }
        CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
    } else {
        fillVectorWithHorizontalGlyphPositions(positions, context, advances, count);
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

void FontCascade::drawGlyphs(GraphicsContext& context, const Font& font, const GlyphBuffer& glyphBuffer, unsigned from, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode smoothingMode)
{
    const FontPlatformData& platformData = font.platformData();
    if (!platformData.size())
        return;

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

    bool useLetterpressEffect = shouldUseLetterpressEffect(context);
    FloatPoint point = anchorPoint;

    CGAffineTransform matrix = CGAffineTransformIdentity;
    if (!platformData.isColorBitmapFont())
        matrix = CTFontGetMatrix(platformData.font());
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;
    if (platformData.syntheticOblique()) {
        static float obliqueSkew = tanf(syntheticObliqueAngle() * piFloat / 180);
        if (platformData.orientation() == FontOrientation::Vertical) {
            if (font.isTextOrientationFallback())
                matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, obliqueSkew, 0, 1, 0, 0));
            else
                matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, -obliqueSkew, 0, 1, 0, 0));
        } else
            matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -obliqueSkew, 1, 0, 0));
    }
    ScopedTextMatrix restorer(matrix, cgContext);

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
        showGlyphsWithAdvances(FloatPoint(shadowTextX, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        if (syntheticBoldOffset)
            showGlyphsWithAdvances(FloatPoint(shadowTextX + syntheticBoldOffset, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        context.setFillColor(fillColor);
    }

    if (useLetterpressEffect)
        showLetterpressedGlyphsWithAdvances(point, font, context, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    else
        showGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (syntheticBoldOffset)
        showGlyphsWithAdvances(FloatPoint(point.x() + syntheticBoldOffset, point.y()), font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);

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
            if (isCJKIdeographOrSymbol(baseCharacter) && !font->hasVerticalGlyphs())
                font = &font->brokenIdeographFont();
            else if (m_fontDescription.nonCJKGlyphOrientation() == NonCJKGlyphOrientation::Mixed) {
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

}
