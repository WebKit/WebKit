/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Holger Hans Peter Freyther
 * Copyright (C) 2014 Igalia S.L.
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

#if USE(CAIRO)

#include "AffineTransform.h"
#include "CairoUtilities.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "PlatformContextCairo.h"
#include "PlatformPathCairo.h"
#include "ShadowBlur.h"

namespace WebCore {

static void drawGlyphsToContext(cairo_t* context, const Font& font, GlyphBufferGlyph* glyphs, int numGlyphs)
{
    cairo_matrix_t originalTransform;
    float syntheticBoldOffset = font.syntheticBoldOffset();
    if (syntheticBoldOffset)
        cairo_get_matrix(context, &originalTransform);

    cairo_set_scaled_font(context, font.platformData().scaledFont());
    cairo_show_glyphs(context, glyphs, numGlyphs);

    if (syntheticBoldOffset) {
        cairo_translate(context, syntheticBoldOffset, 0);
        cairo_show_glyphs(context, glyphs, numGlyphs);
    }

    if (syntheticBoldOffset)
        cairo_set_matrix(context, &originalTransform);
}

static void drawGlyphsShadow(GraphicsContext& graphicsContext, const FloatPoint& point, const Font& font, GlyphBufferGlyph* glyphs, int numGlyphs)
{
    ShadowBlur& shadow = graphicsContext.platformContext()->shadowBlur();

    if (!(graphicsContext.textDrawingMode() & TextModeFill) || shadow.type() == ShadowBlur::NoShadow)
        return;

    if (!graphicsContext.mustUseShadowBlur()) {
        // Optimize non-blurry shadows, by just drawing text without the ShadowBlur.
        cairo_t* context = graphicsContext.platformContext()->cr();
        cairo_save(context);

        FloatSize shadowOffset(graphicsContext.state().shadowOffset);
        cairo_translate(context, shadowOffset.width(), shadowOffset.height());
        setSourceRGBAFromColor(context, graphicsContext.state().shadowColor);
        drawGlyphsToContext(context, font, glyphs, numGlyphs);

        cairo_restore(context);
        return;
    }

    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(font.platformData().scaledFont(), glyphs, numGlyphs, &extents);
    FloatRect fontExtentsRect(point.x() + extents.x_bearing, point.y() + extents.y_bearing, extents.width, extents.height);

    if (GraphicsContext* shadowContext = shadow.beginShadowLayer(graphicsContext, fontExtentsRect)) {
        drawGlyphsToContext(shadowContext->platformContext()->cr(), font, glyphs, numGlyphs);
        shadow.endShadowLayer(graphicsContext);
    }
}

void FontCascade::drawGlyphs(GraphicsContext& context, const Font& font, const GlyphBuffer& glyphBuffer,
    int from, int numGlyphs, const FloatPoint& point, FontSmoothingMode)
{
    if (!font.platformData().size())
        return;

    GlyphBufferGlyph* glyphs = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(from));

    float offset = point.x();
    for (int i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = point.y();
        offset += glyphBuffer.advanceAt(from + i).width();
    }

    PlatformContextCairo* platformContext = context.platformContext();
    drawGlyphsShadow(context, point, font, glyphs, numGlyphs);

    cairo_t* cr = platformContext->cr();
    cairo_save(cr);

    if (context.textDrawingMode() & TextModeFill) {
        platformContext->prepareForFilling(context.state(), PlatformContextCairo::AdjustPatternForGlobalAlpha);
        drawGlyphsToContext(cr, font, glyphs, numGlyphs);
    }

    // Prevent running into a long computation within cairo. If the stroke width is
    // twice the size of the width of the text we will not ask cairo to stroke
    // the text as even one single stroke would cover the full wdth of the text.
    //  See https://bugs.webkit.org/show_bug.cgi?id=33759.
    if (context.textDrawingMode() & TextModeStroke && context.strokeThickness() < 2 * offset) {
        platformContext->prepareForStroking(context.state());
        cairo_set_line_width(cr, context.strokeThickness());

        // This may disturb the CTM, but we are going to call cairo_restore soon after.
        cairo_set_scaled_font(cr, font.platformData().scaledFont());
        cairo_glyph_path(cr, glyphs, numGlyphs);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
struct GlyphIterationState {
    GlyphIterationState(FloatPoint startingPoint, FloatPoint currentPoint, float centerOfLine, float minX, float maxX)
        : startingPoint(startingPoint)
        , currentPoint(currentPoint)
        , centerOfLine(centerOfLine)
        , minX(minX)
        , maxX(maxX)
    {
    }
    FloatPoint startingPoint;
    FloatPoint currentPoint;
    float centerOfLine;
    float minX;
    float maxX;
};

static bool findIntersectionPoint(float y, FloatPoint p1, FloatPoint p2, float& x)
{
    x = p1.x() + (y - p1.y()) * (p2.x() - p1.x()) / (p2.y() - p1.y());
    return (p1.y() < y && p2.y() > y) || (p1.y() > y && p2.y() < y);
}

static void updateX(GlyphIterationState& state, float x)
{
    state.minX = std::min(state.minX, x);
    state.maxX = std::max(state.maxX, x);
}

// This function is called by Path::apply and is therefore invoked for each contour in a glyph. This
// function models each contours as a straight line and calculates the intersections between each
// pseudo-contour and the vertical center of the underline found in GlyphIterationState::centerOfLine.
// It keeps track of the leftmost and rightmost intersection in  GlyphIterationState::minX and 
// GlyphIterationState::maxX.
static void findPathIntersections(GlyphIterationState& state, const PathElement& element)
{
    bool doIntersection = false;
    FloatPoint point = FloatPoint();
    switch (element.type) {
    case PathElementMoveToPoint:
        state.startingPoint = element.points[0];
        state.currentPoint = element.points[0];
        break;
    case PathElementAddLineToPoint:
        doIntersection = true;
        point = element.points[0];
        break;
    case PathElementAddQuadCurveToPoint:
        doIntersection = true;
        point = element.points[1];
        break;
    case PathElementAddCurveToPoint:
        doIntersection = true;
        point = element.points[2];
        break;
    case PathElementCloseSubpath:
        doIntersection = true;
        point = state.startingPoint;
        break;
    }

    if (!doIntersection)
        return;

    float x;
    if (findIntersectionPoint(state.centerOfLine, state.currentPoint, point, x))
        updateX(state, x);

    state.currentPoint = point;
}

class CairoGlyphToPathTranslator final : public GlyphToPathTranslator {
public:
    CairoGlyphToPathTranslator(const TextRun& textRun, const GlyphBuffer& glyphBuffer, const FloatPoint& textOrigin)
        : m_index(0)
        , m_textRun(textRun)
        , m_glyphBuffer(glyphBuffer)
        , m_fontData(glyphBuffer.fontAt(m_index))
        , m_translation(AffineTransform().translate(textOrigin.x(), textOrigin.y()))
    {
        moveToNextValidGlyph();
    }
private:
    bool containsMorePaths() override
    {
        return m_index != m_glyphBuffer.size();
    }
    Path path() override;
    std::pair<float, float> extents() override;
    GlyphUnderlineType underlineType() override;
    void advance() override;
    void moveToNextValidGlyph();

    int m_index;
    const TextRun& m_textRun;
    const GlyphBuffer& m_glyphBuffer;
    const Font* m_fontData;
    AffineTransform m_translation;
};

Path CairoGlyphToPathTranslator::path()
{
    Path path;
    path.ensurePlatformPath();

    cairo_glyph_t cairoGlyph;
    cairoGlyph.index = m_glyphBuffer.glyphAt(m_index);
    cairo_set_scaled_font(path.platformPath()->context(), m_fontData->platformData().scaledFont());
    cairo_glyph_path(path.platformPath()->context(), &cairoGlyph, 1);

    float syntheticBoldOffset = m_fontData->syntheticBoldOffset();
    if (syntheticBoldOffset) {
        cairo_translate(path.platformPath()->context(), syntheticBoldOffset, 0);
        cairo_show_glyphs(path.platformPath()->context(), &cairoGlyph, 1);
    }

    path.transform(m_translation);
    return path;
}

std::pair<float, float> CairoGlyphToPathTranslator::extents()
{
    FloatPoint beginning = m_translation.mapPoint(FloatPoint());
    FloatSize end = m_translation.mapSize(m_glyphBuffer.advanceAt(m_index));
    return std::make_pair(static_cast<float>(beginning.x()), static_cast<float>(beginning.x() + end.width()));
}

GlyphToPathTranslator::GlyphUnderlineType CairoGlyphToPathTranslator::underlineType()
{
    return computeUnderlineType(m_textRun, m_glyphBuffer, m_index);
}

void CairoGlyphToPathTranslator::moveToNextValidGlyph()
{
    if (!m_fontData->isSVGFont())
        return;
    advance();
}

void CairoGlyphToPathTranslator::advance()
{
    do {
        GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
        m_translation = m_translation.translate(advance.width(), advance.height());
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
        translator = std::make_unique<CairoGlyphToPathTranslator>(run, glyphBuffer, origin);
#if ENABLE(SVG_FONTS)
    else {
        TextRun::RenderingContext* renderingContext = run.renderingContext();
        if (!renderingContext)
            return DashArray();
        translator = renderingContext->createGlyphToPathTranslator(*fontData, &run, glyphBuffer, 0, glyphBuffer.size(), origin);
        isSVG = true;
    }
#endif
    DashArray result;
    for (int index = 0; translator->containsMorePaths(); ++index, translator->advance()) {
        float centerOfLine = lineExtents.y() + (lineExtents.height() / 2);
        GlyphIterationState info = GlyphIterationState(FloatPoint(), FloatPoint(), centerOfLine, lineExtents.x() + lineExtents.width(), lineExtents.x());
        const Font* localFontData = glyphBuffer.fontAt(index);
        if (!localFontData || (!isSVG && localFontData->isSVGFont()) || (isSVG && localFontData != fontData)) {
            // The advances will get all messed up if we do anything other than bail here.
            result.clear();
            break;
        }
        switch (translator->underlineType()) {
        case GlyphToPathTranslator::GlyphUnderlineType::SkipDescenders: {
            Path path = translator->path();
            path.apply([&info](const PathElement& pathElement) {
                findPathIntersections(info, pathElement);
            });
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
#endif // ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)

} // namespace WebCore

#endif // USE(CAIRO)
