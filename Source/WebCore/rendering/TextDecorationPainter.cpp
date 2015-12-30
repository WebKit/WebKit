/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2015 Apple Inc. All rights reserved.
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
#include "InlineTextBoxStyle.h"
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
static void strokeWavyTextDecoration(GraphicsContext& context, const FloatPoint& start, const FloatPoint& end, float strokeThickness)
{
    FloatPoint p1 = start;
    FloatPoint p2 = end;
    context.adjustLineToPixelBoundaries(p1, p2, strokeThickness, context.strokeStyle());

    Path path;
    path.moveTo(p1);

    float controlPointDistance;
    float step;
    getWavyStrokeParameters(strokeThickness, controlPointDistance, step);

    bool isVerticalLine = (p1.x() == p2.x());

    if (isVerticalLine) {
        ASSERT(p1.x() == p2.x());

        float xAxis = p1.x();
        float y1;
        float y2;

        if (p1.y() < p2.y()) {
            y1 = p1.y();
            y2 = p2.y();
        } else {
            y1 = p2.y();
            y2 = p1.y();
        }

        adjustStepToDecorationLength(step, controlPointDistance, y2 - y1);
        FloatPoint controlPoint1(xAxis + controlPointDistance, 0);
        FloatPoint controlPoint2(xAxis - controlPointDistance, 0);

        for (float y = y1; y + 2 * step <= y2;) {
            controlPoint1.setY(y + step);
            controlPoint2.setY(y + step);
            y += 2 * step;
            path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(xAxis, y));
        }
    } else {
        ASSERT(p1.y() == p2.y());

        float yAxis = p1.y();
        float x1;
        float x2;

        if (p1.x() < p2.x()) {
            x1 = p1.x();
            x2 = p2.x();
        } else {
            x1 = p2.x();
            x2 = p1.x();
        }

        adjustStepToDecorationLength(step, controlPointDistance, x2 - x1);
        FloatPoint controlPoint1(0, yAxis + controlPointDistance);
        FloatPoint controlPoint2(0, yAxis - controlPointDistance);

        for (float x = x1; x + 2 * step <= x2;) {
            controlPoint1.setX(x + step);
            controlPoint2.setX(x + step);
            x += 2 * step;
            path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
        }
    }

    context.setShouldAntialias(true);
    context.strokePath(path);
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

static void drawSkipInkUnderline(GraphicsContext& context, const FontCascade& font, const TextRun& textRun, const FloatPoint& textOrigin, const FloatPoint& localOrigin,
    float underlineOffset, float width, bool isPrinting, bool doubleLines)
{
    FloatPoint adjustedLocalOrigin = localOrigin;
    adjustedLocalOrigin.move(0, underlineOffset);
    FloatRect underlineBoundingBox = context.computeLineBoundsForText(adjustedLocalOrigin, width, isPrinting);
    DashArray intersections = font.dashesForIntersectionsWithRect(textRun, textOrigin, underlineBoundingBox);
    DashArray a = translateIntersectionPointsToSkipInkBoundaries(intersections, underlineBoundingBox.height(), width);
    ASSERT(!(a.size() % 2));
    context.drawLinesForText(adjustedLocalOrigin, a, isPrinting, doubleLines);
}
#endif

static StrokeStyle textDecorationStyleToStrokeStyle(TextDecorationStyle decorationStyle)
{
    StrokeStyle strokeStyle = SolidStroke;
    switch (decorationStyle) {
    case TextDecorationStyleSolid:
        strokeStyle = SolidStroke;
        break;
    case TextDecorationStyleDouble:
        strokeStyle = DoubleStroke;
        break;
    case TextDecorationStyleDotted:
        strokeStyle = DottedStroke;
        break;
    case TextDecorationStyleDashed:
        strokeStyle = DashedStroke;
        break;
    case TextDecorationStyleWavy:
        strokeStyle = WavyStroke;
        break;
    }

    return strokeStyle;
}

TextDecorationPainter::TextDecorationPainter(GraphicsContext& context, TextDecoration decoration, const RenderText& renderer, bool isFirstLine)
    : m_context(context)
    , m_decoration(decoration)
    , m_wavyOffset(wavyOffsetFromDecoration())
    , m_isPrinting(renderer.document().printing())
    , m_lineStyle(isFirstLine ? renderer.firstLineStyle() : renderer.style())
{
    renderer.getTextDecorationColorsAndStyles(m_decoration, m_underlineColor, m_overlineColor, m_linethroughColor, m_underlineStyle, m_overlineStyle,
        m_linethroughStyle);
    if (isFirstLine) {
        renderer.getTextDecorationColorsAndStyles(m_decoration, m_underlineColor, m_overlineColor, m_linethroughColor,
            m_underlineStyle, m_overlineStyle, m_linethroughStyle, true);
    }
}

void TextDecorationPainter::paintTextDecoration(const TextRun& textRun, const FloatPoint& textOrigin, const FloatPoint& boxOrigin)
{
    ASSERT(m_font);
    float textDecorationThickness = textDecorationStrokeThickness(m_lineStyle.fontSize());
    m_context.setStrokeThickness(textDecorationThickness);
    FloatPoint localOrigin = boxOrigin;

    auto paintDecoration = [&](TextDecorationStyle style, Color color, StrokeStyle strokeStyle,
        const FloatPoint& start, const FloatPoint& end, int offset) {
        m_context.setStrokeColor(color);
        m_context.setStrokeStyle(strokeStyle);

        if (style == TextDecorationStyleWavy) {
            strokeWavyTextDecoration(m_context, start, end, textDecorationThickness);
        } else if (m_decoration & TextDecorationUnderline || m_decoration & TextDecorationOverline) {
#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
            if ((m_lineStyle.textDecorationSkip() == TextDecorationSkipInk || m_lineStyle.textDecorationSkip() == TextDecorationSkipAuto) && m_isHorizontal) {
                if (!m_context.paintingDisabled())
                    drawSkipInkUnderline(m_context, *m_font, textRun, textOrigin, localOrigin, offset, m_width, m_isPrinting, style == TextDecorationStyleDouble);
            } else
                // FIXME: Need to support text-decoration-skip: none.
#endif
                m_context.drawLineForText(FloatPoint(localOrigin.x(), localOrigin.y() + offset), m_width, m_isPrinting, style == TextDecorationStyleDouble);
            
        } else {
            ASSERT(m_decoration & TextDecorationLineThrough);
            m_context.drawLineForText(FloatPoint(localOrigin.x(), localOrigin.y() + 2 * m_baseline / 3), m_width, m_isPrinting, style == TextDecorationStyleDouble);
        }
    };

    bool linesAreOpaque = !m_isPrinting
        && (!(m_decoration & TextDecorationUnderline) || m_underlineColor.alpha() == 255)
        && (!(m_decoration & TextDecorationOverline) || m_overlineColor.alpha() == 255)
        && (!(m_decoration & TextDecorationLineThrough) || m_linethroughColor.alpha() == 255);

    int extraOffset = 0;
    bool clipping = !linesAreOpaque && m_shadow && m_shadow->next();
    if (clipping) {
        FloatRect clipRect(localOrigin, FloatSize(m_width, m_baseline + 2));
        for (const ShadowData* shadow = m_shadow; shadow; shadow = shadow->next()) {
            int shadowExtent = shadow->paintingExtent();
            FloatRect shadowRect(localOrigin, FloatSize(m_width, m_baseline + 2));
            shadowRect.inflate(shadowExtent);
            int shadowX = m_isHorizontal ? shadow->x() : shadow->y();
            int shadowY = m_isHorizontal ? shadow->y() : -shadow->x();
            shadowRect.move(shadowX, shadowY);
            clipRect.unite(shadowRect);
            extraOffset = std::max(extraOffset, std::max(0, shadowY) + shadowExtent);
        }
        m_context.save();
        m_context.clip(clipRect);
        extraOffset += m_baseline + 2;
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
            m_context.setShadow(FloatSize(shadowX, shadowY - extraOffset), shadow->radius(), shadow->color());
            shadow = shadow->next();
        }
        
        // These decorations should match the visual overflows computed in visualOverflowForDecorations()
        if (m_decoration & TextDecorationUnderline) {
            const int offset = computeUnderlineOffset(m_lineStyle.textUnderlinePosition(), m_lineStyle.fontMetrics(), m_inlineTextBox, textDecorationThickness);
            FloatPoint start(localOrigin.x(), localOrigin.y() + offset + m_wavyOffset);
            FloatPoint end(localOrigin.x() + m_width, localOrigin.y() + offset + m_wavyOffset);
            paintDecoration(m_underlineStyle, m_underlineColor, textDecorationStyleToStrokeStyle(m_underlineStyle), start, end, offset);
        }
        if (m_decoration & TextDecorationOverline) {
            FloatPoint start(localOrigin.x(), localOrigin.y() - m_wavyOffset);
            FloatPoint end(localOrigin.x() + m_width, localOrigin.y() - m_wavyOffset);
            paintDecoration(m_overlineStyle, m_overlineColor, textDecorationStyleToStrokeStyle(m_overlineStyle), start, end, 0);
        }
        if (m_decoration & TextDecorationLineThrough) {
            FloatPoint start(localOrigin.x(), localOrigin.y() + 2 * m_baseline / 3);
            FloatPoint end(localOrigin.x() + m_width, localOrigin.y() + 2 * m_baseline / 3);
            paintDecoration(m_linethroughStyle, m_linethroughColor, textDecorationStyleToStrokeStyle(m_linethroughStyle), start, end, 0);
        }
    } while (shadow);

    if (clipping)
        m_context.restore();
    else if (m_shadow)
        m_context.clearShadow();
}

} // namespace WebCore
