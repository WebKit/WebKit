/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
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
#import "CoreGraphicsSPI.h"
#import "CoreTextSPI.h"
#import "DashArray.h"
#import "Font.h"
#import "GlyphBuffer.h"
#import "GraphicsContext.h"
#import "LayoutRect.h"
#import "Logging.h"
#import "WebCoreSystemInterface.h"
#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif
#import <wtf/MathExtras.h>

#if ENABLE(LETTERPRESS)
#import "CoreUISPI.h"
#import "SoftLinking.h"

SOFT_LINK_PRIVATE_FRAMEWORK(CoreUI)
SOFT_LINK_CLASS(CoreUI, CUICatalog)
SOFT_LINK_CLASS(CoreUI, CUIStyleEffectConfiguration)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK(UIKit, _UIKitGetTextEffectsCatalog, CUICatalog *, (void), ())
#endif

#define SYNTHETIC_OBLIQUE_ANGLE 14

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

static inline void fillVectorWithHorizontalGlyphPositions(Vector<CGPoint, 256>& positions, CGContextRef context, const CGSize* advances, size_t count)
{
    CGAffineTransform matrix = CGAffineTransformInvert(CGContextGetTextMatrix(context));
    positions[0] = CGPointZero;
    for (size_t i = 1; i < count; ++i) {
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

static void showLetterpressedGlyphsWithAdvances(const FloatPoint& point, const Font& font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
#if ENABLE(LETTERPRESS)
    if (!count)
        return;

    const FontPlatformData& platformData = font.platformData();
    if (platformData.orientation() == Vertical) {
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

    [catalog drawGlyphs:glyphs atPositions:positions.data() inContext:context withFont:ctFont count:count stylePresetName:@"_UIKitNewLetterpressStyle" styleConfiguration:styleConfiguration foregroundColor:CGContextGetFillColorAsColor(context)];
#else
    UNUSED_PARAM(point);
    UNUSED_PARAM(font);
    UNUSED_PARAM(context);
    UNUSED_PARAM(glyphs);
    UNUSED_PARAM(advances);
    UNUSED_PARAM(count);
#endif
}

class RenderingStyleSaver {
public:
#if !PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
    RenderingStyleSaver(CTFontRef, CGContextRef) { }
#else
    RenderingStyleSaver(CTFontRef font, CGContextRef context)
        : m_context(context)
    {
        m_changed = CTFontSetRenderingStyle(font, context, &m_originalStyle, &m_originalDilation);
    }

    ~RenderingStyleSaver()
    {
        if (!m_changed)
            return;
        CGContextSetFontRenderingStyle(m_context, m_originalStyle);
        CGContextSetFontDilation(m_context, m_originalDilation);
    }

private:
    bool m_changed;
    CGContextRef m_context;
    CGFontRenderingStyle m_originalStyle;
    CGSize m_originalDilation;
#endif
};

static void showGlyphsWithAdvances(const FloatPoint& point, const Font& font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
    if (!count)
        return;

    CGContextSetTextPosition(context, point.x(), point.y());

    const FontPlatformData& platformData = font.platformData();
    Vector<CGPoint, 256> positions(count);
    if (platformData.isColorBitmapFont())
        fillVectorWithHorizontalGlyphPositions(positions, context, advances, count);
    if (platformData.orientation() == Vertical) {
        CGAffineTransform savedMatrix;
        CGAffineTransform rotateLeftTransform = CGAffineTransformMake(0, -1, 1, 0, 0, 0);
        savedMatrix = CGContextGetTextMatrix(context);
        CGAffineTransform runMatrix = CGAffineTransformConcat(savedMatrix, rotateLeftTransform);
        CGContextSetTextMatrix(context, runMatrix);

        Vector<CGSize, 256> translations(count);
        CTFontGetVerticalTranslationsForGlyphs(platformData.ctFont(), glyphs, translations.data(), count);

        CGAffineTransform transform = CGAffineTransformInvert(CGContextGetTextMatrix(context));

        CGPoint position = FloatPoint(point.x(), point.y() + font.fontMetrics().floatAscent(IdeographicBaseline) - font.fontMetrics().floatAscent());
        for (size_t i = 0; i < count; ++i) {
            CGSize translation = CGSizeApplyAffineTransform(translations[i], rotateLeftTransform);
            positions[i] = CGPointApplyAffineTransform(CGPointMake(position.x - translation.width, position.y + translation.height), transform);
            position.x += advances[i].width;
            position.y += advances[i].height;
        }
        if (!platformData.isColorBitmapFont()) {
            RenderingStyleSaver saver(platformData.ctFont(), context);
            CGContextShowGlyphsAtPositions(context, glyphs, positions.data(), count);
        } else
            CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
        CGContextSetTextMatrix(context, savedMatrix);
    } else {
        if (!platformData.isColorBitmapFont()) {
            RenderingStyleSaver saver(platformData.ctFont(), context);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CGContextShowGlyphsWithAdvances(context, glyphs, advances, count);
#pragma clang diagnostic pop
        } else
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

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
static CGSize dilationSizeForTextColor(const Color& color)
{
    double hue;
    double saturation;
    double lightness;
    color.getHSL(hue, saturation, lightness);
    
    // These values were derived empirically, and are only experimental.
    if (lightness < 0.3333) // Dark
        return CGSizeMake(0.007, 0.019);

    if (lightness < 0.6667) // Medium
        return CGSizeMake(0.032, 0.032);

    // Light
    return CGSizeMake(0.0475, 0.039);
}
#endif

void FontCascade::drawGlyphs(GraphicsContext& context, const Font& font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& anchorPoint) const
{
    const FontPlatformData& platformData = font.platformData();
    if (!platformData.size())
        return;

    CGContextRef cgContext = context.platformContext();

    bool shouldSmoothFonts;
    bool changeFontSmoothing;
    bool matchAntialiasedAndSmoothedFonts = context.antialiasedFontDilationEnabled();
    
    switch (fontDescription().fontSmoothing()) {
    case Antialiased: {
        context.setShouldAntialias(true);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        matchAntialiasedAndSmoothedFonts = false; // CSS has opted into strictly antialiased fonts.
        break;
    }
    case SubpixelAntialiased: {
        context.setShouldAntialias(true);
        shouldSmoothFonts = true;
        changeFontSmoothing = true;
        matchAntialiasedAndSmoothedFonts = true;
        break;
    }
    case NoSmoothing: {
        context.setShouldAntialias(false);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        matchAntialiasedAndSmoothedFonts = false;
        break;
    }
    case AutoSmoothing: {
        shouldSmoothFonts = true;
        changeFontSmoothing = false;
        matchAntialiasedAndSmoothedFonts = true;
        break;
    }
    }
    
    if (!shouldUseSmoothing()) {
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        matchAntialiasedAndSmoothedFonts = true;
    }

#if !PLATFORM(IOS)
    bool originalShouldUseFontSmoothing = false;
    if (changeFontSmoothing) {
        originalShouldUseFontSmoothing = CGContextGetShouldSmoothFonts(cgContext);
        CGContextSetShouldSmoothFonts(cgContext, shouldSmoothFonts);
    }
    
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    CGFontAntialiasingStyle oldAntialiasingStyle;
    bool resetAntialiasingStyle = false;
    if (antialiasedFontDilationEnabled() && !CGContextGetShouldSmoothFonts(cgContext) && matchAntialiasedAndSmoothedFonts) {
        resetAntialiasingStyle = true;
        oldAntialiasingStyle = CGContextGetFontAntialiasingStyle(cgContext);
        CGContextSetFontAntialiasingStyle(cgContext, kCGFontAntialiasingStyleUnfilteredCustomDilation);
        CGContextSetFontDilation(cgContext, dilationSizeForTextColor(context.fillColor()));
    }
#endif
#endif

    CGContextSetFont(cgContext, platformData.cgFont());

    bool useLetterpressEffect = shouldUseLetterpressEffect(context);
    FloatPoint point = anchorPoint;

    CGAffineTransform matrix = CGAffineTransformIdentity;
    if (!platformData.isColorBitmapFont())
        matrix = CTFontGetMatrix(platformData.font());
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;
    if (platformData.m_syntheticOblique) {
        static float obliqueSkew = tanf(SYNTHETIC_OBLIQUE_ANGLE * piFloat / 180);
        if (platformData.orientation() == Vertical)
            matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, obliqueSkew, 0, 1, 0, 0));
        else
            matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -obliqueSkew, 1, 0, 0));
    }
    CGContextSetTextMatrix(cgContext, matrix);

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
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
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

#if !PLATFORM(IOS)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    if (resetAntialiasingStyle)
        CGContextSetFontAntialiasingStyle(cgContext, oldAntialiasingStyle);
#endif
    
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
        moveToNextValidGlyph();
    }
private:
    virtual bool containsMorePaths() override
    {
        return m_index != m_glyphBuffer.size();
    }
    virtual Path path() override;
    virtual std::pair<float, float> extents() override;
    virtual GlyphUnderlineType underlineType() override;
    virtual void advance() override;
    void moveToNextValidGlyph();

    int m_index;
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

void MacGlyphToPathTranslator::moveToNextValidGlyph()
{
    if (!m_fontData->isSVGFont())
        return;
    advance();
}

void MacGlyphToPathTranslator::advance()
{
    do {
        GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
        m_translation = CGAffineTransformTranslate(m_translation, advance.width(), advance.height());
        ++m_index;
        if (m_index >= m_glyphBuffer.size())
            break;
        m_fontData = m_glyphBuffer.fontAt(m_index);
    } while (m_fontData->isSVGFont() && m_index < m_glyphBuffer.size());
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
    const Font* fontData = glyphBuffer.fontAt(0);
    std::unique_ptr<GlyphToPathTranslator> translator;
    bool isSVG = false;
    FloatPoint origin = FloatPoint(textOrigin.x() + deltaX, textOrigin.y());
    if (!fontData->isSVGFont())
        translator = std::make_unique<MacGlyphToPathTranslator>(run, glyphBuffer, origin);
    else {
        TextRun::RenderingContext* renderingContext = run.renderingContext();
        if (!renderingContext)
            return DashArray();
        translator = renderingContext->createGlyphToPathTranslator(*fontData, &run, glyphBuffer, 0, glyphBuffer.size(), origin);
        isSVG = true;
    }
    DashArray result;
    for (int index = 0; translator->containsMorePaths(); ++index, translator->advance()) {
        GlyphIterationState info = GlyphIterationState(CGPointMake(0, 0), CGPointMake(0, 0), lineExtents.y(), lineExtents.y() + lineExtents.height(), lineExtents.x() + lineExtents.width(), lineExtents.x());
        const Font* localFont = glyphBuffer.fontAt(index);
        if (!localFont || (!isSVG && localFont->isSVGFont()) || (isSVG && localFont != fontData)) {
            // The advances will get all messed up if we do anything other than bail here.
            result.clear();
            break;
        }
        switch (translator->underlineType()) {
        case GlyphToPathTranslator::GlyphUnderlineType::SkipDescenders: {
            Path path = translator->path();
            CGPathApply(path.platformPath(), &info, &findPathIntersections);
            if (info.minX < info.maxX) {
                result.append(info.minX - lineExtents.x());
                result.append(info.maxX - lineExtents.x());
            }
            break;
        }
        case GlyphToPathTranslator::GlyphUnderlineType::SkipGlyph: {
            std::pair<float, float> extents = translator->extents();
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
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    const auto& fontData = primaryFont();
    return !fontData.isSVGFont() && CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(fontData.platformData().ctFont())).get());
#else
    const String& firstFamily = this->firstFamily();
    return equalIgnoringASCIICase(firstFamily, "-webkit-system-font")
        || equalIgnoringASCIICase(firstFamily, "-apple-system-font")
        || equalIgnoringASCIICase(firstFamily, "-apple-system")
        || equalIgnoringASCIICase(firstFamily, "-apple-menu")
        || equalIgnoringASCIICase(firstFamily, "-apple-status-bar");
#endif
}

void FontCascade::adjustSelectionRectForComplexText(const TextRun& run, LayoutRect& selectionRect, int from, int to) const
{
    ComplexTextController controller(*this, run);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to);
    float afterWidth = controller.runWidthSoFar();

    if (run.rtl())
        selectionRect.move(controller.totalWidth() - afterWidth + controller.leadingExpansion(), 0);
    else
        selectionRect.move(beforeWidth, 0);
    selectionRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
}

float FontCascade::getGlyphsAndAdvancesForComplexText(const TextRun& run, int from, int to, GlyphBuffer& glyphBuffer, ForTextEmphasisOrNot forTextEmphasis) const
{
    float initialAdvance;

    ComplexTextController controller(*this, run, false, 0, forTextEmphasis);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to, &glyphBuffer);

    if (glyphBuffer.isEmpty())
        return 0;

    float afterWidth = controller.runWidthSoFar();

    if (run.rtl()) {
        initialAdvance = controller.totalWidth() + controller.finalRoundingWidth() - afterWidth + controller.leadingExpansion();
        glyphBuffer.reverse(0, glyphBuffer.size());
    } else
        initialAdvance = beforeWidth;

    return initialAdvance;
}

void FontCascade::drawEmphasisMarksForComplexText(GraphicsContext& context, const TextRun& run, const AtomicString& mark, const FloatPoint& point, int from, int to) const
{
    GlyphBuffer glyphBuffer;
    float initialAdvance = getGlyphsAndAdvancesForComplexText(run, from, to, glyphBuffer, ForTextEmphasis);

    if (glyphBuffer.isEmpty())
        return;

    drawEmphasisMarks(context, run, glyphBuffer, mark, FloatPoint(point.x() + initialAdvance, point.y()));
}

float FontCascade::floatWidthForComplexText(const TextRun& run, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    ComplexTextController controller(*this, run, true, fallbackFonts);
    if (glyphOverflow) {
        glyphOverflow->top = std::max<int>(glyphOverflow->top, ceilf(-controller.minGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().ascent()));
        glyphOverflow->bottom = std::max<int>(glyphOverflow->bottom, ceilf(controller.maxGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().descent()));
        glyphOverflow->left = std::max<int>(0, ceilf(-controller.minGlyphBoundingBoxX()));
        glyphOverflow->right = std::max<int>(0, ceilf(controller.maxGlyphBoundingBoxX() - controller.totalWidth()));
    }
    return controller.totalWidth();
}

int FontCascade::offsetForPositionForComplexText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    ComplexTextController controller(*this, run);
    return controller.offsetForPosition(x, includePartialGlyphs);
}

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
#if PLATFORM(IOS)
        if (baseCharacter >= 0x0600 && baseCharacter <= 0x06ff && font->shouldNotBeUsedForArabic())
            continue;
#endif
        if (font->platformData().orientation() == Vertical) {
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
