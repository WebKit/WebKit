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
#import <pal/spi/ios/CoreUISPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CoreUI)
SOFT_LINK_CLASS(CoreUI, CUICatalog)
SOFT_LINK_CLASS(CoreUI, CUIStyleEffectConfiguration)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK(UIKit, _UIKitGetTextEffectsCatalog, CUICatalog *, (void), ())
SOFT_LINK_CLASS(UIKit, UIColor)
#endif

#ifdef __LP64__
#define URefCon void*
#else
#define URefCon UInt32
#endif

namespace WebCore {

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
    return context.textDrawingMode() & TextModeLetterpress;
#else
    UNUSED_PARAM(context);
    return false;
#endif
}

static void showLetterpressedGlyphsWithAdvances(const FloatPoint& point, const Font& font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, unsigned count)
{
#if ENABLE(LETTERPRESS)
    if (!count)
        return;

    const FontPlatformData& platformData = font.platformData();
    if (platformData.orientation() == FontOrientation::Vertical) {
        // FIXME: Implement support for vertical text. See <rdar://problem/13737298>.
        return;
    }

    CGContextSetTextPosition(context, point.x(), point.y());
    Vector<CGPoint, 256> positions(count);
    fillVectorWithHorizontalGlyphPositions(positions, context, advances, count);

    CTFontRef ctFont = platformData.ctFont();
    CGContextSetFontSize(context, CTFontGetSize(ctFont));

    static CUICatalog *catalog = _UIKitGetTextEffectsCatalog();
    if (!catalog)
        return;

    static CUIStyleEffectConfiguration *styleConfiguration;
    if (!styleConfiguration) {
        styleConfiguration = [allocCUIStyleEffectConfigurationInstance() init];
        styleConfiguration.useSimplifiedEffect = YES;
    }

    CGContextSetFont(context, adoptCF(CTFontCopyGraphicsFont(ctFont, nullptr)).get());
    CGContextSetFontSize(context, platformData.size());

    [catalog drawGlyphs:glyphs atPositions:positions.data() inContext:context withFont:ctFont count:count stylePresetName:@"_UIKitNewLetterpressStyle" styleConfiguration:styleConfiguration foregroundColor:CGContextGetFillColorAsColor(context)];

    CGContextSetFont(context, nullptr);
    CGContextSetFontSize(context, 0);
#else
    UNUSED_PARAM(point);
    UNUSED_PARAM(font);
    UNUSED_PARAM(context);
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

    bool shouldSmoothFonts;
    bool changeFontSmoothing;
    
    switch (smoothingMode) {
    case FontSmoothingMode::Antialiased: {
        context.setShouldAntialias(true);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        break;
    }
    case FontSmoothingMode::SubpixelAntialiased: {
        context.setShouldAntialias(true);
        shouldSmoothFonts = true;
        changeFontSmoothing = true;
        break;
    }
    case FontSmoothingMode::NoSmoothing: {
        context.setShouldAntialias(false);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        break;
    }
    case FontSmoothingMode::AutoSmoothing: {
        shouldSmoothFonts = true;
        changeFontSmoothing = false;
        break;
    }
    }
    
    if (!shouldUseSmoothing()) {
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
    }

#if !PLATFORM(IOS_FAMILY)
    bool originalShouldUseFontSmoothing = false;
    if (changeFontSmoothing) {
        originalShouldUseFontSmoothing = CGContextGetShouldSmoothFonts(cgContext);
        CGContextSetShouldSmoothFonts(cgContext, shouldSmoothFonts);
    }
#endif

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
        if (horizontalUnitLengthInDevicePixels)
            syntheticBoldOffset /= horizontalUnitLengthInDevicePixels;
    };

    bool hasSimpleShadow = context.textDrawingMode() == TextModeFill && shadowColor.isValid() && !shadowBlur && !platformData.isColorBitmapFont() && (!context.shadowsIgnoreTransforms() || contextCTM.isIdentityOrTranslationOrFlipped()) && !context.isInTransparencyLayer();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context.clearShadow();
        Color fillColor = context.fillColor();
        Color shadowFillColor = shadowColor.colorWithAlphaMultipliedBy(fillColor.alphaAsFloat());
        context.setFillColor(shadowFillColor);
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (context.shadowsIgnoreTransforms() ? -1 : 1);
        showGlyphsWithAdvances(FloatPoint(shadowTextX, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
        if (syntheticBoldOffset)
            showGlyphsWithAdvances(FloatPoint(shadowTextX + syntheticBoldOffset, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
        context.setFillColor(fillColor);
    }

    if (useLetterpressEffect)
        showLetterpressedGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
    else
        showGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
    if (syntheticBoldOffset)
        showGlyphsWithAdvances(FloatPoint(point.x() + syntheticBoldOffset, point.y()), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);

    if (hasSimpleShadow)
        context.setShadow(shadowOffset, shadowBlur, shadowColor);

#if !PLATFORM(IOS_FAMILY)
    if (changeFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
#endif
}

#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
struct GlyphIterationState {
    GlyphIterationState(CGPoint startingPoint, CGPoint currentPoint, CGFloat y1, CGFloat y2, CGFloat minX, CGFloat maxX)
        : startingPoint(startingPoint)
        , currentPoint(currentPoint)
        , y1(y1)
        , y2(y2)
        , minX(minX)
        , maxX(maxX)
    {
    }
    CGPoint startingPoint;
    CGPoint currentPoint;
    CGFloat y1;
    CGFloat y2;
    CGFloat minX;
    CGFloat maxX;
};

static bool findIntersectionPoint(float y, CGPoint p1, CGPoint p2, CGFloat& x)
{
    x = p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
    return (p1.y < y && p2.y > y) || (p1.y > y && p2.y < y);
}

static void updateX(GlyphIterationState& state, CGFloat x)
{
    state.minX = std::min(state.minX, x);
    state.maxX = std::max(state.maxX, x);
}

// This function is called by CGPathApply and is therefore invoked for each
// contour in a glyph. This function models each contours as a straight line
// and calculates the intersections between each pseudo-contour and
// two horizontal lines (the upper and lower bounds of an underline) found in
// GlyphIterationState::y1 and GlyphIterationState::y2. It keeps track of the
// leftmost and rightmost intersection in GlyphIterationState::minX and
// GlyphIterationState::maxX.
static void findPathIntersections(void* stateAsVoidPointer, const CGPathElement* e)
{
    auto& state = *static_cast<GlyphIterationState*>(stateAsVoidPointer);
    bool doIntersection = false;
    CGPoint point = CGPointZero;
    switch (e->type) {
    case kCGPathElementMoveToPoint:
        state.startingPoint = e->points[0];
        state.currentPoint = e->points[0];
        break;
    case kCGPathElementAddLineToPoint:
        doIntersection = true;
        point = e->points[0];
        break;
    case kCGPathElementAddQuadCurveToPoint:
        doIntersection = true;
        point = e->points[1];
        break;
    case kCGPathElementAddCurveToPoint:
        doIntersection = true;
        point = e->points[2];
        break;
    case kCGPathElementCloseSubpath:
        doIntersection = true;
        point = state.startingPoint;
        break;
    }
    if (!doIntersection)
        return;
    CGFloat x;
    if (findIntersectionPoint(state.y1, state.currentPoint, point, x))
        updateX(state, x);
    if (findIntersectionPoint(state.y2, state.currentPoint, point, x))
        updateX(state, x);
    if ((state.currentPoint.y >= state.y1 && state.currentPoint.y <= state.y2)
        || (state.currentPoint.y <= state.y1 && state.currentPoint.y >= state.y2))
        updateX(state, state.currentPoint.x);
    state.currentPoint = point;
}

class MacGlyphToPathTranslator final : public GlyphToPathTranslator {
public:
    MacGlyphToPathTranslator(const TextRun& textRun, const GlyphBuffer& glyphBuffer, const FloatPoint& textOrigin)
        : m_index(0)
        , m_textRun(textRun)
        , m_glyphBuffer(glyphBuffer)
        , m_fontData(glyphBuffer.fontAt(m_index))
        , m_translation(CGAffineTransformScale(CGAffineTransformMakeTranslation(textOrigin.x(), textOrigin.y()), 1, -1))
    {
    }
    bool containsMorePaths() final { return m_index != m_glyphBuffer.size(); }
    Path path() final;
    std::pair<float, float> extents() final;
    GlyphUnderlineType underlineType() final;
    void advance() final;

private:
    unsigned m_index;
    const TextRun& m_textRun;
    const GlyphBuffer& m_glyphBuffer;
    const Font* m_fontData;
    CGAffineTransform m_translation;
};

Path MacGlyphToPathTranslator::path()
{
    RetainPtr<CGPathRef> result = adoptCF(CTFontCreatePathForGlyph(m_fontData->platformData().ctFont(), m_glyphBuffer.glyphAt(m_index), &m_translation));
    return adoptCF(CGPathCreateMutableCopy(result.get()));
}

std::pair<float, float> MacGlyphToPathTranslator::extents()
{
    CGPoint beginning = CGPointApplyAffineTransform(CGPointMake(0, 0), m_translation);
    CGSize end = CGSizeApplyAffineTransform(m_glyphBuffer.advanceAt(m_index), m_translation);
    return std::make_pair(static_cast<float>(beginning.x), static_cast<float>(beginning.x + end.width));
}

auto MacGlyphToPathTranslator::underlineType() -> GlyphUnderlineType
{
    return computeUnderlineType(m_textRun, m_glyphBuffer, m_index);
}

void MacGlyphToPathTranslator::advance()
{
    GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
    m_translation = CGAffineTransformTranslate(m_translation, advance.width(), advance.height());
    ++m_index;
    if (m_index < m_glyphBuffer.size())
        m_fontData = m_glyphBuffer.fontAt(m_index);
}

DashArray FontCascade::dashesForIntersectionsWithRect(const TextRun& run, const FloatPoint& textOrigin, const FloatRect& lineExtents) const
{
    if (isLoadingCustomFonts())
        return DashArray();

    GlyphBuffer glyphBuffer;
    glyphBuffer.saveOffsetsInString();
    float deltaX;
    if (codePath(run) != FontCascade::Complex)
        deltaX = getGlyphsAndAdvancesForSimpleText(run, 0, run.length(), glyphBuffer);
    else
        deltaX = getGlyphsAndAdvancesForComplexText(run, 0, run.length(), glyphBuffer);

    if (!glyphBuffer.size())
        return DashArray();
    
    // FIXME: Handle SVG + non-SVG interleaved runs. https://bugs.webkit.org/show_bug.cgi?id=133778
    FloatPoint origin = FloatPoint(textOrigin.x() + deltaX, textOrigin.y());
    MacGlyphToPathTranslator translator(run, glyphBuffer, origin);
    DashArray result;
    for (unsigned index = 0; translator.containsMorePaths(); ++index, translator.advance()) {
        GlyphIterationState info = GlyphIterationState(CGPointMake(0, 0), CGPointMake(0, 0), lineExtents.y(), lineExtents.y() + lineExtents.height(), lineExtents.x() + lineExtents.width(), lineExtents.x());
        const Font* localFont = glyphBuffer.fontAt(index);
        if (!localFont) {
            // The advances will get all messed up if we do anything other than bail here.
            result.clear();
            break;
        }
        switch (translator.underlineType()) {
        case GlyphToPathTranslator::GlyphUnderlineType::SkipDescenders: {
            Path path = translator.path();
            CGPathApply(path.platformPath(), &info, &findPathIntersections);
            if (info.minX < info.maxX) {
                result.append(info.minX - lineExtents.x());
                result.append(info.maxX - lineExtents.x());
            }
            break;
        }
        case GlyphToPathTranslator::GlyphUnderlineType::SkipGlyph: {
            std::pair<float, float> extents = translator.extents();
            result.append(extents.first - lineExtents.x());
            result.append(extents.second - lineExtents.x());
            break;
        }
        case GlyphToPathTranslator::GlyphUnderlineType::DrawOverGlyph:
            // Nothing to do
            break;
        }
    }
    return result;
}
#endif

bool FontCascade::primaryFontIsSystemFont() const
{
    const auto& fontData = primaryFont();
    return CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(fontData.platformData().ctFont())).get());
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
