/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc.
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

#include <pal/spi/cf/CoreTextSPI.h>

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
    static constexpr AffineTransform result(0, -1, 1, 0, 0, 0);
    return result;
}

AffineTransform computeBaseOverallTextMatrix(const std::optional<AffineTransform>& syntheticOblique)
{
    AffineTransform result;

    // This is a Y-flip, because text's coordinate system is increasing-Y-goes-up,
    // but WebKit's coordinate system is increasing-Y-goes-down.
    result.setB(-result.b());
    result.setD(-result.d());

    if (syntheticOblique)
        result = *syntheticOblique * result;

    return result;
}

AffineTransform computeOverallTextMatrix(const Font& font)
{
    std::optional<AffineTransform> syntheticOblique;
    auto& platformData = font.platformData();
    if (platformData.syntheticOblique()) {
        float obliqueSkew = tanf(deg2rad(FontCascade::syntheticObliqueAngle()));
        if (platformData.orientation() == FontOrientation::Vertical) {
            if (font.isTextOrientationFallback())
                syntheticOblique = AffineTransform(1, obliqueSkew, 0, 1, 0, 0);
            else
                syntheticOblique = AffineTransform(1, -obliqueSkew, 0, 1, 0, 0);
        } else
            syntheticOblique = AffineTransform(1, 0, -obliqueSkew, 1, 0, 0);
    }

    return computeBaseOverallTextMatrix(syntheticOblique);
}

AffineTransform computeBaseVerticalTextMatrix(const AffineTransform& previousTextMatrix)
{
    // The translation here ("e" and "f" fields) are irrelevant, because
    // this matrix is inverted in fillVectorWithVerticalGlyphPositions to place the glyphs in the CTM's coordinate system.
    // All we're trying to do here is rotate the text matrix so glyphs appear visually upright.
    // We have to include the previous text matrix because it includes things like synthetic oblique.
    //
    // Because this is a left-multiply, we're taking the points from user coordinates, which are increasing-Y-goes-down,
    // and we're rotating the points to the left in that coordinate system, to put them physically upright.
    return rotateLeftTransform() * previousTextMatrix;
}

AffineTransform computeVerticalTextMatrix(const Font& font, const AffineTransform& previousTextMatrix)
{
    ASSERT_UNUSED(font, font.platformData().orientation() == FontOrientation::Vertical);
    return computeBaseVerticalTextMatrix(previousTextMatrix);
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

static void fillVectorWithVerticalGlyphPositions(Vector<CGPoint, 256>& positions, const CGSize translations[], const CGSize advances[], unsigned count, const FloatPoint& point, float ascentDelta, CGAffineTransform textMatrix)
{
    // Keep this function in sync as the inverse of `DrawGlyphsRecorder::recordDrawGlyphs`.

    // It's important to realize we're dealing with 4 coordinate systems here:
    // 1. Physical coordinate system. This is what the user sees.
    // 2. User coordinate system. This is the coordinate system of just the CTM. For vertical text, this is just like the normal WebKit increasing-Y-down
    //        coordinate system, except we're rotated right, so logical right is physical down. (We do this so logical inline progression proceeds in the
    //        logically increasing-X dimension, just as it would if we weren't doing vertical stuff.)
    // 3. Text coordinate system. This is the coordinate of the text matrix concatenated with the CTM. For vertical text, this is rotated such that
    //        increasing Y goes physically up, and increasing X goes physically right. The control points in font contours are authored according to this
    //        increasing-Y-up coordinate system.
    // 4. Synthetic-oblique-less text coordinate system. This would be identical to the text coordinate system if synthetic oblique was not in effect. This
    //        is useful because, when we're moving glyphs around, we usually don't want to consider synthetic oblique. Instead, synthetic oblique is just
    //        a rasterization-time effect, and not used for glyph positioning/layout.
    //        FIXME: Does this mean that synthetic oblique should always be applied on the result of rotateLeftTransform() in computeVerticalTextMatrix(),
    //        rather than the other way around?

    // Imagine an vertical upright glyph:
    // +--------------------------+
    // |      ___       ___       |
    // |      \  \     /  /       |
    // |       \  \   /  /        |
    // |        \  \ /  /         |
    // |         \  V  /          |
    // |          \   /           |
    // |          |   |           |
    // |          |   |           |
    // |          |   |           |
    // |          |   |           |
    // |          |___|           |
    // |                          |
    // +--------------------------+
    //
    // The ideographic baseline lies in the center of the glyph, and the alphabetic baseline lies to the left of it:
    //     |        |
    // +---|--------|-------------+
    // |   |  ___   |   ___       |
    // |   |  \  \  |  /  /       |
    // |   |   \  \ | /  /        |
    // |   |    \  \|/  /         |
    // |   |     \  |  /          |
    // |   |      \ | /           |
    // |   |      | | |           |
    // |   |      | | |           |
    // |   |      | | |           |
    // |   |      | | |           |
    // |   |      |_|_|           |
    // |   |        |             |
    // +---|--------|-------------+
    //     |        | <== ideographic baseline
    //     | <== alphabetic baseline
    //
    // The glyph itself has a local origin, which is the position sent to Core Text. The control points of the contours are defined relative to this point.
    // +--------------------------+
    // |      ___       ___       |
    // |      \  \     /  /       |
    // |       \  \   /  /        |
    // |        \  \ /  /         |
    // |         \  V  /          |
    // |          \   /           |
    // |          |   |           |
    // |          |   |           |
    // |          |   |           |
    // * <= here  |   |           |
    // |          |___|           |
    // |                          |
    // +--------------------------+
    //
    // Now, for horizontal text, we can do the simple thing of just:
    // 1. Place the pen at a position. Record this position as the local origin of the first glyph
    // 2. Move the pen according to the glyph's advance
    // 3. Record a new position as the local origin of the next glyph
    // 4. Go to 2
    // However, for vertical text, we can't get away with this because the glyph origins are not on the baseline.
    // This is what the "vertical translation for a glyph" is for. It contains this vector:
    // +---A--------B-------------+
    // |           /              |
    // |          /               |
    // |        --                |
    // |       /                  |
    // |      /                   |
    // |    --                    |
    // |   /                      |
    // |  /                       |
    // ||_                        |
    // C                          |
    // |                          |
    // |                          |
    // +--------------------------+
    // It points from the pen position on the ideographic baseline to the glyph's local origin. This is (usually) physically
    // down-and-to-the-left. Core Text gives us these vectors in the text coordinate system, and so therefore these vectors (usually) have
    // both X and Y components negative.

    // The goal of this function is to produce glyph origins in the text coordinate system, because that's what Core Text expects. The "advances"
    // and "point" parameters to this function are in the user coordinate system. The "translations" parameter is in the "synthetic-oblique-less
    // text coordinate system."

    // CGContextGetTextMatrix() transforms points from text coordinates to user coordinates. However, we're trying to produce text coordinates from
    // user coordinates, so we invert it.
    CGAffineTransform transform = CGAffineTransformInvert(textMatrix);

    // Because the "vertical translation for a glyph" vector starts at the ideographic baseline (the point B in the above diagram), we have to
    // adjust the pen position to start there. WebKit's text routines start out using the alphabetic baseline (point A in the diagram above) so we
    // adjust the start position here, which has the effect of shifting the whole run altogether.
    //
    // ascentDelta is (usually) a negative number, and represents the distance between the ideographic baseline to the alphabetic baseline.
    // In user coordinates, we want to adjust the Y component to make a horizontal physical change. And, because the user coordinate system is
    // logically increasing-Y-down, we add the value, which is negative, to move us logically up, which is physically to the right. Now our position
    // is at the point labeled B in the above diagram, in user coordinates.
    auto position = CGPointMake(point.x(), point.y() + ascentDelta);

    static const auto constantSyntheticTextMatrixOmittingOblique = computeBaseVerticalTextMatrix(computeBaseOverallTextMatrix(std::nullopt)); // See fillVectorWithVerticalGlyphPositions(), which describes what this is.

    for (unsigned i = 0; i < count; ++i) {
        // The "translations" parameter is in the "synthetic-oblique-less text coordinate system" and we want to add it to the position in the user
        // coordinate system. Luckily, the text matrix (or, at least the version of the text matrix that doesn't include synthetic oblique) does exactly
        // this. So, we just create the synthetic-oblique-less text matrix, and run the translation through it. This gives us the translation in user
        // coordinates.
        auto translationInUserCoordinates = CGSizeApplyAffineTransform(translations[i], constantSyntheticTextMatrixOmittingOblique);

        // Now we can add the position in user coordinates with the translation in user coordinates.
        auto positionInUserCoordinates = CGPointMake(position.x + translationInUserCoordinates.width, position.y + translationInUserCoordinates.height);

        // And then put it back in font coordinates for submission to Core Text. Yay!
        positions[i] = CGPointApplyAffineTransform(positionInUserCoordinates, transform);

        // Advance the position to the next position in user coordinates. Both the advances and position are in user coordinates.
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
        fillVectorWithVerticalGlyphPositions(positions, translations.data(), advances, count, point, ascentDelta, CGContextGetTextMatrix(context));
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

    if (!font.allowsAntialiasing())
        smoothingMode = FontSmoothingMode::NoSmoothing;

    bool shouldAntialias = true;
    bool shouldSmoothFonts = true;

    switch (smoothingMode) {
    case FontSmoothingMode::Antialiased:
        shouldSmoothFonts = false;
        break;
    case FontSmoothingMode::AutoSmoothing:
    case FontSmoothingMode::SubpixelAntialiased:
        break;
    case FontSmoothingMode::NoSmoothing:
        shouldAntialias = false;
        shouldSmoothFonts = false;
        break;
    }

#if PLATFORM(IOS_FAMILY)
    UNUSED_VARIABLE(shouldSmoothFonts);
#else
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

    auto shadow = context.dropShadow();

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

    bool hasSimpleShadow = context.textDrawingMode() == TextDrawingMode::Fill && shadow && shadow->color.isValid() && !shadow->radius && !platformData.isColorBitmapFont() && (!context.shadowsIgnoreTransforms() || contextCTM.isIdentityOrTranslationOrFlipped()) && !context.isInTransparencyLayer();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context.clearDropShadow();
        Color fillColor = context.fillColor();
        Color shadowFillColor = shadow->color.colorWithAlphaMultipliedBy(fillColor.alphaAsFloat());
        context.setFillColor(shadowFillColor);
        float shadowTextX = point.x() + shadow->offset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadow->offset.height() * (context.shadowsIgnoreTransforms() ? -1 : 1);
        showGlyphsWithAdvances(FloatPoint(shadowTextX, shadowTextY), font, cgContext, glyphs, advances, numGlyphs, textMatrix);
        if (syntheticBoldOffset)
            showGlyphsWithAdvances(FloatPoint(shadowTextX + syntheticBoldOffset, shadowTextY), font, cgContext, glyphs, advances, numGlyphs, textMatrix);
        context.setFillColor(fillColor);
    }

    showGlyphsWithAdvances(point, font, cgContext, glyphs, advances, numGlyphs, textMatrix);

    if (syntheticBoldOffset)
        showGlyphsWithAdvances(FloatPoint(point.x() + syntheticBoldOffset, point.y()), font, cgContext, glyphs, advances, numGlyphs, textMatrix);

    if (hasSimpleShadow)
        context.setDropShadow(*shadow);

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

const Font* FontCascade::fontForCombiningCharacterSequence(StringView stringView) const
{
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();

    ASSERT(!stringView.isEmpty());
    char32_t baseCharacter = *codePointsIterator;
    ++codePointsIterator;
    bool isOnlySingleCodePoint = codePointsIterator == codePoints.end();

    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacter, false, NormalVariant);

    if (!baseCharacterGlyphData.glyph)
        return nullptr;

    if (isOnlySingleCodePoint)
        return baseCharacterGlyphData.font.get();

    bool triedBaseCharacterFont = false;

    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        auto& fontRanges = fallbackRangesAt(i);
        if (fontRanges.isGeneric() && isPrivateUseAreaCharacter(baseCharacter))
            continue;
        const Font* font = fontRanges.fontForCharacter(baseCharacter);
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

        if (font->canRenderCombiningCharacterSequence(stringView))
            return font;
    }

    if (!triedBaseCharacterFont && baseCharacterGlyphData.font && baseCharacterGlyphData.font->canRenderCombiningCharacterSequence(stringView))
        return baseCharacterGlyphData.font.get();

    return Font::systemFallback();
}

ResolvedEmojiPolicy FontCascade::resolveEmojiPolicy(FontVariantEmoji fontVariantEmoji, char32_t character)
{
    // You may think that this function should be different between macOS and iOS. And you may even be right!
    //
    // For "unqualified" characters on https://unicode.org/Public/emoji/latest/emoji-test.txt the apparent behavior
    // of macOS and iOS is different. Both OSes cascade through the default cascade list, but on macOS,
    // STIXTwo is ahead of AppleColorEmoji in the list. On iOS, however, AppleColorEmoji is really early in the list
    // (it appears before almost everything else). So the observed effect is that a lot of these "unqualified"
    // characters will be emoji style on iOS whereas they will be text style on macOS.
    //
    // On the other hand, when Unicode says that a character is Emoji_Presentation, then it needs to be rendered a
    // emoji style, regardless of which OS you're on. Them's the rules.
    //
    // The fact that this function is the same on macOS and iOS is a somewhat-intentional choice. We *could* gather up
    // all the characters that apparently render differently on macOS and iOS, and force them to maintain those
    // differences here. However, that has 2 downsides:
    // 1. Having a big list of characters in WebKit source code is unmaintanable. And generating it at build time is a
    //        bit of a science project, given Apple's internal build system.
    // 2. More importantly, it probably isn't what authors want. If authors have their own font-family fallback list,
    //        they probably don't want us to sidestep _most_ of it in search of an emoji font, just because of the
    //        particular order of Core Text's native cascade list for native apps.
    //
    // So, where we end up here is a situation where these characters will get platform-specific rendering, but only if
    // the author is using `font-family: system-ui` or we end up falling off the end of the fallback list altogether.
    // Otherwise, we honor the author's given font-family list. This is probably the best of both words:
    // 1. If we have a positive signal from Unicode that a character has gotta be rendered in emoji style, then we'll
    //        honor that,
    // 2. In all other cases we'll honor the author's fallback list...
    // 3. Unless the author has (intentionally or unintentionally) asked us to perform a platform-specific fallback
    //        (via either asking for system-ui or by falling off the end of the list).

    switch (fontVariantEmoji) {
    case FontVariantEmoji::Normal:
    case FontVariantEmoji::Unicode:
        // https://www.unicode.org/reports/tr51/#Presentation_Style
        // There had been no clear line for implementers between three categories of Unicode characters:
        // 1. emoji-default: those expected to have an emoji presentation by default, but can also have a text presentation
        // 2. text-default: those expected to have a text presentation by default, but could also have an emoji presentation
        // 3. text-only: those that should only have a text presentation
        // These categories can be distinguished using properties listed in Annex A: Emoji Properties and Data Files.
        // The first category are characters with Emoji=Yes and Emoji_Presentation=Yes.
        // The second category are characters with Emoji=Yes and Emoji_Presentation=No.
        // The third category are characters with Emoji=No.
        if (u_hasBinaryProperty(character, UCHAR_EMOJI_PRESENTATION))
            return ResolvedEmojiPolicy::RequireEmoji;
        return ResolvedEmojiPolicy::NoPreference;
    case FontVariantEmoji::Text:
        return ResolvedEmojiPolicy::RequireText;
    case FontVariantEmoji::Emoji:
        return ResolvedEmojiPolicy::RequireEmoji;
    }
}

} // namespace WebCore
