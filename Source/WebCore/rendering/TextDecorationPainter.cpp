/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "TextDecorationPainter.h"

#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HTMLAnchorElement.h"
#include "HTMLFontElement.h"
#include "InlineTextBoxStyle.h"
#include "RenderBlock.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "ShadowData.h"
#include "TextRun.h"

namespace WebCore {

static void adjustStepToDecorationLength(float& step, float& controlPointDistance, float length)
{
    ASSERT(step > 0);

    if (length <= 0)
        return;

    unsigned stepCount = static_cast<unsigned>(length / step);

    // Each Bezier curve starts at the same pixel that the previous one
    // ended. We need to subtract (stepCount - 1) pixels when calculating the
    // length covered to account for that.
    float uncoveredLength = length - (stepCount * step - (stepCount - 1));
    float adjustment = uncoveredLength / stepCount;
    step += adjustment;
    controlPointDistance += adjustment;
}

/*
 * Draw one cubic Bezier curve and repeat the same pattern long the the decoration's axis.
 * The start point (p1), controlPoint1, controlPoint2 and end point (p2) of the Bezier curve
 * form a diamond shape:
 *
 *                              step
 *                         |-----------|
 *
 *                   controlPoint1
 *                         +
 *
 *
 *                  . .
 *                .     .
 *              .         .
 * (x1, y1) p1 +           .            + p2 (x2, y2) - <--- Decoration's axis
 *                          .         .               |
 *                            .     .                 |
 *                              . .                   | controlPointDistance
 *                                                    |
 *                                                    |
 *                         +                          -
 *                   controlPoint2
 *
 *             |-----------|
 *                 step
 */
static void strokeWavyTextDecoration(GraphicsContext& context, const FloatRect& rect, float fontSize)
{
    FloatPoint p1 = rect.minXMinYCorner();
    FloatPoint p2 = rect.maxXMinYCorner();
    context.adjustLineToPixelBoundaries(p1, p2, rect.height(), context.strokeStyle());

    Path path;
    path.moveTo(p1);

    auto wavyStrokeParameters = getWavyStrokeParameters(fontSize);

    ASSERT(p1.y() == p2.y());

    float yAxis = p1.y();
    float x1 = std::min(p1.x(), p2.x());
    float x2 = std::max(p1.x(), p2.x());

    adjustStepToDecorationLength(wavyStrokeParameters.step, wavyStrokeParameters.controlPointDistance, x2 - x1);
    FloatPoint controlPoint1(0, yAxis + wavyStrokeParameters.controlPointDistance);
    FloatPoint controlPoint2(0, yAxis - wavyStrokeParameters.controlPointDistance);

    for (float x = x1; x + 2 * wavyStrokeParameters.step <= x2;) {
        controlPoint1.setX(x + wavyStrokeParameters.step);
        controlPoint2.setX(x + wavyStrokeParameters.step);
        x += 2 * wavyStrokeParameters.step;
        path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
    }

    context.setShouldAntialias(true);
    auto strokeThickness = context.strokeThickness();
    context.setStrokeThickness(rect.height());
    context.strokePath(path);
    context.setStrokeThickness(strokeThickness);
}

#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
static bool compareTuples(std::pair<float, float> l, std::pair<float, float> r)
{
    return l.first < r.first;
}

static DashArray translateIntersectionPointsToSkipInkBoundaries(const DashArray& intersections, float dilationAmount, float totalWidth)
{
    ASSERT(!(intersections.size() % 2));
    
    // Step 1: Make pairs so we can sort based on range starting-point. We dilate the ranges in this step as well.
    Vector<std::pair<float, float>> tuples;
    for (auto i = intersections.begin(); i != intersections.end(); i++, i++)
        tuples.append(std::make_pair(*i - dilationAmount, *(i + 1) + dilationAmount));
    std::sort(tuples.begin(), tuples.end(), &compareTuples);

    // Step 2: Deal with intersecting ranges.
    Vector<std::pair<float, float>> intermediateTuples;
    if (tuples.size() >= 2) {
        intermediateTuples.append(*tuples.begin());
        for (auto i = tuples.begin() + 1; i != tuples.end(); i++) {
            float& firstEnd = intermediateTuples.last().second;
            float secondStart = i->first;
            float secondEnd = i->second;
            if (secondStart <= firstEnd && secondEnd <= firstEnd) {
                // Ignore this range completely
            } else if (secondStart <= firstEnd)
                firstEnd = secondEnd;
            else
                intermediateTuples.append(*i);
        }
    } else
        intermediateTuples = tuples;

    // Step 3: Output the space between the ranges, but only if the space warrants an underline.
    float previous = 0;
    DashArray result;
    for (const auto& tuple : intermediateTuples) {
        if (tuple.first - previous > dilationAmount) {
            result.append(previous);
            result.append(tuple.first);
        }
        previous = tuple.second;
    }
    if (totalWidth - previous > dilationAmount) {
        result.append(previous);
        result.append(totalWidth);
    }
    
    return result;
}
#endif

static StrokeStyle textDecorationStyleToStrokeStyle(TextDecorationStyle decorationStyle)
{
    StrokeStyle strokeStyle = SolidStroke;
    switch (decorationStyle) {
    case TextDecorationStyle::Solid:
        strokeStyle = SolidStroke;
        break;
    case TextDecorationStyle::Double:
        strokeStyle = DoubleStroke;
        break;
    case TextDecorationStyle::Dotted:
        strokeStyle = DottedStroke;
        break;
    case TextDecorationStyle::Dashed:
        strokeStyle = DashedStroke;
        break;
    case TextDecorationStyle::Wavy:
        strokeStyle = WavyStroke;
        break;
    }

    return strokeStyle;
}

bool TextDecorationPainter::Styles::operator==(const Styles& other) const
{
    return underlineColor == other.underlineColor && overlineColor == other.overlineColor && linethroughColor == other.linethroughColor
        && underlineStyle == other.underlineStyle && overlineStyle == other.overlineStyle && linethroughStyle == other.linethroughStyle;
}

TextDecorationPainter::TextDecorationPainter(GraphicsContext& context, OptionSet<TextDecoration> decorations, const RenderText& renderer, bool isFirstLine, const FontCascade& font, std::optional<Styles> styles)
    : m_context { context }
    , m_decorations { decorations }
    , m_wavyOffset { wavyOffsetFromDecoration() }
    , m_isPrinting { renderer.document().printing() }
    , m_font { font }
    , m_styles { styles ? *WTFMove(styles) : stylesForRenderer(renderer, decorations, isFirstLine, PseudoId::None) }
    , m_lineStyle { isFirstLine ? renderer.firstLineStyle() : renderer.style() }
{
}

void TextDecorationPainter::paintTextDecoration(const TextRun& textRun, const FloatPoint& textOrigin, const FloatPoint& boxOrigin)
{
#if !ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
    UNUSED_PARAM(textRun);
    UNUSED_PARAM(textOrigin);
#endif
    const auto& fontMetrics = m_lineStyle.fontMetrics();
    float textDecorationThickness = textDecorationStrokeThickness(m_lineStyle.computedFontPixelSize());
    FloatPoint localOrigin = boxOrigin;

    auto paintDecoration = [&] (TextDecoration decoration, TextDecorationStyle style, const Color& color, const FloatRect& rect) {
        m_context.setStrokeColor(color);

        auto strokeStyle = textDecorationStyleToStrokeStyle(style);

        if (style == TextDecorationStyle::Wavy)
            strokeWavyTextDecoration(m_context, rect, m_lineStyle.computedFontPixelSize());
        else if (decoration == TextDecoration::Underline || decoration == TextDecoration::Overline) {
#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
            if ((m_lineStyle.textDecorationSkip() == TextDecorationSkip::Ink || m_lineStyle.textDecorationSkip() == TextDecorationSkip::Auto) && m_isHorizontal) {
                if (!m_context.paintingDisabled()) {
                    FloatRect underlineBoundingBox = m_context.computeUnderlineBoundsForText(rect, m_isPrinting);
                    DashArray intersections = m_font.dashesForIntersectionsWithRect(textRun, textOrigin, underlineBoundingBox);
                    DashArray boundaries = translateIntersectionPointsToSkipInkBoundaries(intersections, underlineBoundingBox.height(), rect.width());
                    ASSERT(!(boundaries.size() % 2));
                    // We don't use underlineBoundingBox here because drawLinesForText() will run computeUnderlineBoundsForText() internally.
                    m_context.drawLinesForText(rect.location(), rect.height(), boundaries, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
                }
            } else
                // FIXME: Need to support text-decoration-skip: none.
#endif
                m_context.drawLineForText(rect, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
            
        } else {
            ASSERT(decoration == TextDecoration::LineThrough);
            m_context.drawLineForText(rect, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
        }
    };

    bool areLinesOpaque = !m_isPrinting && (!m_decorations.contains(TextDecoration::Underline) || m_styles.underlineColor.isOpaque())
        && (!m_decorations.contains(TextDecoration::Overline) || m_styles.overlineColor.isOpaque())
        && (!m_decorations.contains(TextDecoration::LineThrough) || m_styles.linethroughColor.isOpaque());

    int extraOffset = 0;
    bool clipping = !areLinesOpaque && m_shadow && m_shadow->next();
    if (clipping) {
        FloatRect clipRect(localOrigin, FloatSize(m_width, fontMetrics.ascent() + 2));
        for (const ShadowData* shadow = m_shadow; shadow; shadow = shadow->next()) {
            int shadowExtent = shadow->paintingExtent();
            FloatRect shadowRect(localOrigin, FloatSize(m_width, fontMetrics.ascent() + 2));
            shadowRect.inflate(shadowExtent);
            int shadowX = m_isHorizontal ? shadow->x() : shadow->y();
            int shadowY = m_isHorizontal ? shadow->y() : -shadow->x();
            shadowRect.move(shadowX, shadowY);
            clipRect.unite(shadowRect);
            extraOffset = std::max(extraOffset, std::max(0, shadowY) + shadowExtent);
        }
        m_context.save();
        m_context.clip(clipRect);
        extraOffset += fontMetrics.ascent() + 2;
        localOrigin.move(0, extraOffset);
    }

    const ShadowData* shadow = m_shadow;
    do {
        if (shadow) {
            if (!shadow->next()) {
                // The last set of lines paints normally inside the clip.
                localOrigin.move(0, -extraOffset);
                extraOffset = 0;
            }
            int shadowX = m_isHorizontal ? shadow->x() : shadow->y();
            int shadowY = m_isHorizontal ? shadow->y() : -shadow->x();
            
            Color shadowColor = shadow->color();
            if (m_shadowColorFilter)
                m_shadowColorFilter->transformColor(shadowColor);
            m_context.setShadow(FloatSize(shadowX, shadowY - extraOffset), shadow->radius(), shadowColor);
            shadow = shadow->next();
        }

        // These decorations should match the visual overflows computed in visualOverflowForDecorations().
        if (m_decorations.contains(TextDecoration::Underline)) {
            int offset = computeUnderlineOffset(m_lineStyle.textUnderlinePosition(), m_lineStyle.fontMetrics(), m_inlineTextBox, textDecorationThickness);
            float wavyOffset = m_styles.underlineStyle == TextDecorationStyle::Wavy ? m_wavyOffset : 0;
            FloatRect rect(localOrigin, FloatSize(m_width, textDecorationThickness));
            rect.move(0, offset + wavyOffset);
            paintDecoration(TextDecoration::Underline, m_styles.underlineStyle, m_styles.underlineColor, rect);
        }
        if (m_decorations.contains(TextDecoration::Overline)) {
            float wavyOffset = m_styles.overlineStyle == TextDecorationStyle::Wavy ? m_wavyOffset : 0;
            FloatRect rect(localOrigin, FloatSize(m_width, textDecorationThickness));
            rect.move(0, -wavyOffset);
            paintDecoration(TextDecoration::Overline, m_styles.overlineStyle, m_styles.overlineColor, rect);
        }
        if (m_decorations.contains(TextDecoration::LineThrough)) {
            FloatRect rect(localOrigin, FloatSize(m_width, textDecorationThickness));
            rect.move(0, 2 * fontMetrics.floatAscent() / 3);
            paintDecoration(TextDecoration::LineThrough, m_styles.linethroughStyle, m_styles.linethroughColor, rect);
        }
    } while (shadow);

    if (clipping)
        m_context.restore();
    else if (m_shadow)
        m_context.clearShadow();
}

static Color decorationColor(const RenderStyle& style)
{
    // Check for text decoration color first.
    Color result = style.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextDecorationColor);
    if (result.isValid())
        return result;
    if (style.hasPositiveStrokeWidth()) {
        // Prefer stroke color if possible but not if it's fully transparent.
        result = style.computedStrokeColor();
        if (result.isVisible())
            return result;
    }
    
    return style.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor);
}

static void collectStylesForRenderer(TextDecorationPainter::Styles& result, const RenderObject& renderer, OptionSet<TextDecoration> remainingDecorations, bool firstLineStyle, PseudoId pseudoId)
{
    auto extractDecorations = [&] (const RenderStyle& style, OptionSet<TextDecoration> decorations) {
        auto color = decorationColor(style);
        auto decorationStyle = style.textDecorationStyle();

        if (decorations.contains(TextDecoration::Underline)) {
            remainingDecorations.remove(TextDecoration::Underline);
            result.underlineColor = color;
            result.underlineStyle = decorationStyle;
        }
        if (decorations.contains(TextDecoration::Overline)) {
            remainingDecorations.remove(TextDecoration::Overline);
            result.overlineColor = color;
            result.overlineStyle = decorationStyle;
        }
        if (decorations.contains(TextDecoration::LineThrough)) {
            remainingDecorations.remove(TextDecoration::LineThrough);
            result.linethroughColor = color;
            result.linethroughStyle = decorationStyle;
        }

    };

    auto styleForRenderer = [&] (const RenderObject& renderer) -> const RenderStyle& {
        if (pseudoId != PseudoId::None && renderer.style().hasPseudoStyle(pseudoId)) {
            if (is<RenderText>(renderer))
                return *downcast<RenderText>(renderer).getCachedPseudoStyle(pseudoId);
            return *downcast<RenderElement>(renderer).getCachedPseudoStyle(pseudoId);
        }
        return firstLineStyle ? renderer.firstLineStyle() : renderer.style();
    };

    auto* current = &renderer;
    do {
        const auto& style = styleForRenderer(*current);
        extractDecorations(style, style.textDecoration());

        if (current->isRubyText())
            return;

        current = current->parent();
        if (current && current->isAnonymousBlock() && downcast<RenderBlock>(*current).continuation())
            current = downcast<RenderBlock>(*current).continuation();

        if (remainingDecorations.isEmpty())
            break;

    } while (current && !is<HTMLAnchorElement>(current->node()) && !is<HTMLFontElement>(current->node()));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (!remainingDecorations.isEmpty() && current)
        extractDecorations(styleForRenderer(*current), remainingDecorations);
}

auto TextDecorationPainter::stylesForRenderer(const RenderObject& renderer, OptionSet<TextDecoration> requestedDecorations, bool firstLineStyle, PseudoId pseudoId) -> Styles
{
    Styles result;
    collectStylesForRenderer(result, renderer, requestedDecorations, false, pseudoId);
    if (firstLineStyle)
        collectStylesForRenderer(result, renderer, requestedDecorations, true, pseudoId);
    return result;
}

} // namespace WebCore
