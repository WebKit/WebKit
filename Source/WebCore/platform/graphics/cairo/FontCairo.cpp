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

void FontCascade::drawGlyphs(GraphicsContext& context, const Font& font, const GlyphBuffer& glyphBuffer,
    unsigned from, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode)
{
    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<cairo_glyph_t> glyphs(numGlyphs);
    {
        ASSERT(from + numGlyphs <= glyphBuffer.size());
        auto* glyphsData = glyphBuffer.glyphs(from);
        auto* advances = glyphBuffer.advances(from);

        auto yOffset = point.y();
        for (size_t i = 0; i < numGlyphs; ++i) {
            glyphs[i] = { glyphsData[i], xOffset, yOffset };
            xOffset += advances[i].width();
        }
    }

    cairo_scaled_font_t* scaledFont = font.platformData().scaledFont();
    double syntheticBoldOffset = font.syntheticBoldOffset();

    ASSERT(context.hasPlatformContext());
    auto& state = context.state();
    Cairo::drawGlyphs(*context.platformContext(), Cairo::FillSource(state), Cairo::StrokeSource(state),
        Cairo::ShadowState(state), point, scaledFont, syntheticBoldOffset, glyphs, xOffset,
        state.textDrawingMode, state.strokeThickness, state.shadowOffset, state.shadowColor);
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
    AffineTransform m_translation;
};

Path CairoGlyphToPathTranslator::path()
{
    Path path;
    path.ensurePlatformPath();

    cairo_glyph_t cairoGlyph = { m_glyphBuffer.glyphAt(m_index), 0, 0 };
    cairo_set_scaled_font(path.platformPath()->context(), m_fontData->platformData().scaledFont());
    cairo_glyph_path(path.platformPath()->context(), &cairoGlyph, 1);

    float syntheticBoldOffset = m_fontData->syntheticBoldOffset();
    if (syntheticBoldOffset) {
        cairo_translate(path.platformPath()->context(), syntheticBoldOffset, 0);
        cairo_glyph_path(path.platformPath()->context(), &cairoGlyph, 1);
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

void CairoGlyphToPathTranslator::advance()
{
    GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
    m_translation = m_translation.translate(advance.width(), advance.height());
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
    CairoGlyphToPathTranslator translator(run, glyphBuffer, origin);
    DashArray result;
    for (int index = 0; translator.containsMorePaths(); ++index, translator.advance()) {
        float centerOfLine = lineExtents.y() + (lineExtents.height() / 2);
        GlyphIterationState info = GlyphIterationState(FloatPoint(), FloatPoint(), centerOfLine, lineExtents.x() + lineExtents.width(), lineExtents.x());
        const Font* localFontData = glyphBuffer.fontAt(index);
        if (!localFontData) {
            // The advances will get all messed up if we do anything other than bail here.
            result.clear();
            break;
        }
        switch (translator.underlineType()) {
        case GlyphToPathTranslator::GlyphUnderlineType::SkipDescenders: {
            Path path = translator.path();
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
#endif // ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)

} // namespace WebCore

#endif // USE(CAIRO)
