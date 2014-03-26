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
#import "Font.h"

#import "DashArray.h"
#import "GlyphBuffer.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "SimpleFontData.h"
#import "WebCoreSystemInterface.h"
#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif
#import <wtf/MathExtras.h>

#if ENABLE(LETTERPRESS)
#import "SoftLinking.h"
#if __has_include(<CoreGraphics/CoreGraphicsPrivate.h>)
#import <CoreGraphics/CoreGraphicsPrivate.h>
#else
extern CGColorRef CGContextGetFillColorAsColor(CGContextRef);
#endif
#import <CoreUI/CUICatalog.h>
#import <CoreUI/CUIStyleEffectConfiguration.h>

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

bool Font::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool Font::canExpandAroundIdeographsInComplexText()
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

static void showLetterpressedGlyphsWithAdvances(const FloatPoint& point, const SimpleFontData* font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
#if ENABLE(LETTERPRESS)
    if (!count)
        return;

    const FontPlatformData& platformData = font->platformData();
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
        styleConfiguration = [[getCUIStyleEffectConfigurationClass() alloc] init];
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

static void showGlyphsWithAdvances(const FloatPoint& point, const SimpleFontData* font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
    if (!count)
        return;

    CGContextSetTextPosition(context, point.x(), point.y());

    const FontPlatformData& platformData = font->platformData();
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

        CGPoint position = FloatPoint(point.x(), point.y() + font->fontMetrics().floatAscent(IdeographicBaseline) - font->fontMetrics().floatAscent());
        for (size_t i = 0; i < count; ++i) {
            CGSize translation = CGSizeApplyAffineTransform(translations[i], rotateLeftTransform);
            positions[i] = CGPointApplyAffineTransform(CGPointMake(position.x - translation.width, position.y + translation.height), transform);
            position.x += advances[i].width;
            position.y += advances[i].height;
        }
        if (!platformData.isColorBitmapFont())
            CGContextShowGlyphsAtPositions(context, glyphs, positions.data(), count);
        else
            CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
        CGContextSetTextMatrix(context, savedMatrix);
    } else {
        if (!platformData.isColorBitmapFont())
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CGContextShowGlyphsWithAdvances(context, glyphs, advances, count);
#pragma clang diagnostic pop
        else
            CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
    }
}

void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& anchorPoint) const
{
    const FontPlatformData& platformData = font->platformData();
    if (!platformData.size())
        return;

    CGContextRef cgContext = context->platformContext();

    bool shouldSmoothFonts;
    bool changeFontSmoothing;
    
    switch(fontDescription().fontSmoothing()) {
    case Antialiased: {
        context->setShouldAntialias(true);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        break;
    }
    case SubpixelAntialiased: {
        context->setShouldAntialias(true);
        shouldSmoothFonts = true;
        changeFontSmoothing = true;
        break;
    }
    case NoSmoothing: {
        context->setShouldAntialias(false);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        break;
    }
    case AutoSmoothing: {
        shouldSmoothFonts = true;
        changeFontSmoothing = false;
        break;
    }
    }
    
    if (!shouldUseSmoothing()) {
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
    }

#if !PLATFORM(IOS)
    bool originalShouldUseFontSmoothing = false;
    if (changeFontSmoothing) {
        originalShouldUseFontSmoothing = wkCGContextGetShouldSmoothFonts(cgContext);
        CGContextSetShouldSmoothFonts(cgContext, shouldSmoothFonts);
    }
#endif

#if !PLATFORM(IOS)
    NSFont* drawFont;
    if (!isPrinterFont()) {
        drawFont = [platformData.font() screenFont];
        if (drawFont != platformData.font())
            // We are getting this in too many places (3406411); use ERROR so it only prints on debug versions for now. (We should debug this also, eventually).
            LOG_ERROR("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.",
                [[[platformData.font() fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    } else {
        drawFont = [platformData.font() printerFont];
        if (drawFont != platformData.font())
            NSLog(@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.",
                [[[platformData.font() fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    }
#endif
    
    CGContextSetFont(cgContext, platformData.cgFont());

    bool useLetterpressEffect = shouldUseLetterpressEffect(*context);
    FloatPoint point = anchorPoint;
#if PLATFORM(IOS)
    float fontSize = platformData.size();
    CGAffineTransform matrix = useLetterpressEffect || platformData.isColorBitmapFont() ? CGAffineTransformIdentity : CGAffineTransformMakeScale(fontSize, fontSize);
    if (platformData.m_isEmoji) {
        if (!context->emojiDrawingEnabled())
            return;

        // Mimic the positioining of non-bitmap glyphs, which are not subpixel-positioned.
        point.setY(ceilf(point.y()));

        // Emoji glyphs snap to the CSS pixel grid.
        point.setX(floorf(point.x()));

        // Emoji glyphs are offset one CSS pixel to the right.
        point.move(1, 0);

        // Emoji glyphs are offset vertically based on font size.
        float y = point.y();
        if (fontSize <= 15) {
            // Undo Core Text's y adjustment.
            static float yAdjustmentFactor = iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_6_0) ? .19 : .1;
            point.setY(floorf(y - yAdjustmentFactor * (fontSize + 2) + 2));
        } else {
            if (fontSize < 26)
                y -= .35f * fontSize - 10;

            // Undo Core Text's y adjustment.
            static float yAdjustment = iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_6_0) ? 3.8 : 2;
            point.setY(floorf(y - yAdjustment));
        }
    }
#else
    CGAffineTransform matrix = CGAffineTransformIdentity;
    if (drawFont && !platformData.isColorBitmapFont())
        memcpy(&matrix, [drawFont matrix], sizeof(matrix));
#endif
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

#if PLATFORM(IOS)
    CGContextSetFontSize(cgContext, 1);
#else
    wkSetCGFontRenderingMode(cgContext, drawFont, context->shouldSubpixelQuantizeFonts());
    if (drawFont)
        CGContextSetFontSize(cgContext, 1);
    else
        CGContextSetFontSize(cgContext, platformData.m_size);
#endif


    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    ColorSpace shadowColorSpace;
    ColorSpace fillColorSpace = context->fillColorSpace();
    context->getShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);

    AffineTransform contextCTM = context->getCTM();
    float syntheticBoldOffset = font->syntheticBoldOffset();
    if (syntheticBoldOffset && !contextCTM.isIdentityOrTranslationOrFlipped()) {
        FloatSize horizontalUnitSizeInDevicePixels = contextCTM.mapSize(FloatSize(1, 0));
        float horizontalUnitLengthInDevicePixels = sqrtf(horizontalUnitSizeInDevicePixels.width() * horizontalUnitSizeInDevicePixels.width() + horizontalUnitSizeInDevicePixels.height() * horizontalUnitSizeInDevicePixels.height());
        if (horizontalUnitLengthInDevicePixels)
            syntheticBoldOffset /= horizontalUnitLengthInDevicePixels;
    };

    bool hasSimpleShadow = context->textDrawingMode() == TextModeFill && shadowColor.isValid() && !shadowBlur && !platformData.isColorBitmapFont() && (!context->shadowsIgnoreTransforms() || contextCTM.isIdentityOrTranslationOrFlipped()) && !context->isInTransparencyLayer();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context->clearShadow();
        Color fillColor = context->fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        context->setFillColor(shadowFillColor, shadowColorSpace);
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (context->shadowsIgnoreTransforms() ? -1 : 1);
        showGlyphsWithAdvances(FloatPoint(shadowTextX, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
#if !PLATFORM(IOS)
        if (syntheticBoldOffset)
#else
        if (syntheticBoldOffset && !platformData.m_isEmoji)
#endif
            showGlyphsWithAdvances(FloatPoint(shadowTextX + syntheticBoldOffset, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
        context->setFillColor(fillColor, fillColorSpace);
    }

    if (useLetterpressEffect)
        showLetterpressedGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
    else
        showGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
#if !PLATFORM(IOS)
    if (syntheticBoldOffset)
#else
    if (syntheticBoldOffset && !platformData.m_isEmoji)
#endif
        showGlyphsWithAdvances(FloatPoint(point.x() + syntheticBoldOffset, point.y()), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);

    if (hasSimpleShadow)
        context->setShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);

#if !PLATFORM(IOS)
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
    MacGlyphToPathTranslator(const GlyphBuffer& glyphBuffer, const FloatPoint& textOrigin)
        : m_index(0)
        , m_glyphBuffer(glyphBuffer)
        , m_fontData(glyphBuffer.fontDataAt(m_index))
        , m_translation(CGAffineTransformScale(CGAffineTransformMakeTranslation(textOrigin.x(), textOrigin.y()), 1, -1))
    {
        moveToNextValidGlyph();
    }
private:
    virtual bool containsMorePaths() override
    {
        return m_index != m_glyphBuffer.size();
    }
    virtual Path nextPath() override;
    void moveToNextValidGlyph();
    void incrementIndex();

    int m_index;
    const GlyphBuffer& m_glyphBuffer;
    const SimpleFontData* m_fontData;
    CGAffineTransform m_translation;
};

Path MacGlyphToPathTranslator::nextPath()
{
    RetainPtr<CGPathRef> result = adoptCF(CTFontCreatePathForGlyph(m_fontData->platformData().ctFont(), m_glyphBuffer.glyphAt(m_index), &m_translation));
    incrementIndex();
    return adoptCF(CGPathCreateMutableCopy(result.get()));
}

void MacGlyphToPathTranslator::moveToNextValidGlyph()
{
    if (!m_fontData->isSVGFont())
        return;
    incrementIndex();
}

void MacGlyphToPathTranslator::incrementIndex()
{
    do {
        GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
        m_translation = CGAffineTransformTranslate(m_translation, advance.width(), advance.height());
        ++m_index;
        if (m_index >= m_glyphBuffer.size())
            break;
        m_fontData = m_glyphBuffer.fontDataAt(m_index);
    } while (m_fontData->isSVGFont() && m_index < m_glyphBuffer.size());
}

DashArray Font::dashesForIntersectionsWithRect(const TextRun& run, const FloatPoint& textOrigin, const FloatRect& lineExtents) const
{
    if (loadingCustomFonts())
        return DashArray();

    GlyphBuffer glyphBuffer;
    float deltaX;
    if (codePath(run) != Font::Complex)
        deltaX = getGlyphsAndAdvancesForSimpleText(run, 0, run.length(), glyphBuffer);
    else
        deltaX = getGlyphsAndAdvancesForComplexText(run, 0, run.length(), glyphBuffer);

    if (!glyphBuffer.size())
        return DashArray();
    
    // FIXME: Handle SVG + non-SVG interleaved runs
    const SimpleFontData* fontData = glyphBuffer.fontDataAt(0);
    std::unique_ptr<GlyphToPathTranslator> translator;
    bool isSVG = false;
    FloatPoint origin = FloatPoint(textOrigin.x() + deltaX, textOrigin.y());
    if (!fontData->isSVGFont())
        translator = std::move(std::make_unique<MacGlyphToPathTranslator>(glyphBuffer, origin));
    else {
        translator = std::move(run.renderingContext()->createGlyphToPathTranslator(*fontData, glyphBuffer, 0, run.length(), origin));
        isSVG = true;
    }
    DashArray result;
    for (int index = 0; translator->containsMorePaths(); ++index) {
        GlyphIterationState info = GlyphIterationState(CGPointMake(0, 0), CGPointMake(0, 0), lineExtents.y(), lineExtents.y() + lineExtents.height(), lineExtents.x() + lineExtents.width(), lineExtents.x());
        const SimpleFontData* localFontData = glyphBuffer.fontDataAt(index);
        if (!localFontData || (!isSVG && localFontData->isSVGFont()) || (isSVG && localFontData != fontData))
            break; // The advances will get all messed up if we do anything other than bail here.
        Path path = translator->nextPath();
        CGPathApply(path.platformPath(), &info, &findPathIntersections);
        if (info.minX < info.maxX) {
            result.append(info.minX - lineExtents.x());
            result.append(info.maxX - lineExtents.x());
        }
    }
    return result;
}
#endif

}
