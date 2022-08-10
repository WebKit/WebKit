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

#include "FilterOperations.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HTMLAnchorElement.h"
#include "HTMLFontElement.h"
#include "InlineIteratorLineBox.h"
#include "InlineTextBoxStyle.h"
#include "RenderBlock.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "ShadowData.h"
#include "TextRun.h"

namespace WebCore {

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
    auto wavyStrokeParameters = getWavyStrokeParameters(fontSize);

    FloatPoint p1 = rect.minXMinYCorner();
    FloatPoint p2 = rect.maxXMinYCorner();

    // Extent the wavy line before and after the text so it can cover the whole length.
    p1.setX(p1.x() - 2 * wavyStrokeParameters.step);
    p2.setX(p2.x() + 2 * wavyStrokeParameters.step);

    auto bounds = rect;
    // Offset the bounds and set extra height to ensure the whole wavy line is covered.
    bounds.setY(bounds.y() - wavyStrokeParameters.controlPointDistance);
    bounds.setHeight(bounds.height() + 2 * wavyStrokeParameters.controlPointDistance);
    // Clip the extra wavy line added before
    GraphicsContextStateSaver stateSaver(context);
    context.clip(bounds);

    context.adjustLineToPixelBoundaries(p1, p2, rect.height(), context.strokeStyle());

    Path path;
    path.moveTo(p1);

    ASSERT(p1.y() == p2.y());

    float yAxis = p1.y();
    float x1 = std::min(p1.x(), p2.x());
    float x2 = std::max(p1.x(), p2.x());

    FloatPoint controlPoint1(0, yAxis + wavyStrokeParameters.controlPointDistance);
    FloatPoint controlPoint2(0, yAxis - wavyStrokeParameters.controlPointDistance);

    for (float x = x1; x + 2 * wavyStrokeParameters.step <= x2;) {
        controlPoint1.setX(x + wavyStrokeParameters.step);
        controlPoint2.setX(x + wavyStrokeParameters.step);
        x += 2 * wavyStrokeParameters.step;
        path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
    }

    context.setShouldAntialias(true);
    context.setStrokeThickness(rect.height());
    context.strokePath(path);
}

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

static inline OptionSet<TextDecorationLine> textDecorationsInEffectForStyle(const TextDecorationPainter::Styles& style)
{
    OptionSet<TextDecorationLine> decorations;
    if (style.underline.color.isValid())
        decorations.add(TextDecorationLine::Underline);
    if (style.overline.color.isValid())
        decorations.add(TextDecorationLine::Overline);
    if (style.linethrough.color.isValid())
        decorations.add(TextDecorationLine::LineThrough);
    return decorations;
}

bool TextDecorationPainter::Styles::operator==(const Styles& other) const
{
    return underline.color == other.underline.color && overline.color == other.overline.color && linethrough.color == other.linethrough.color
        && underline.decorationStyle == other.underline.decorationStyle && overline.decorationStyle == other.overline.decorationStyle && linethrough.decorationStyle == other.linethrough.decorationStyle;
}

TextDecorationPainter::TextDecorationPainter(GraphicsContext& context, const RenderStyle& decorationStyle, const FontCascade& font, const ShadowData* shadow, const FilterOperations* colorFilter, Styles styleOverride, bool isPrinting, bool isHorizontal, float deviceScaleFactor)
    : m_context(context)
    , m_isPrinting(isPrinting)
    , m_isHorizontal(isHorizontal)
    , m_shadow(shadow)
    , m_shadowColorFilter(colorFilter)
    , m_font(font)
    , m_deviceScaleFactor(deviceScaleFactor)
    , m_styles(styleOverride)
    , m_textDecorationStyle(decorationStyle)
{
    m_textDecorations = m_textDecorationStyle.textDecorationsInEffect();
    m_textDecorations.add(textDecorationsInEffectForStyle(styleOverride));
}

// Paint text-shadow, underline, overline
void TextDecorationPainter::paintBackgroundDecorations(const TextRun& textRun, const FloatPoint& textOrigin, const FloatPoint& boxOrigin, float width, float underlineOffset, float wavyOffset)
{
    const auto& fontMetrics = m_textDecorationStyle.metricsOfPrimaryFont();
    auto textDecorationThickness = ceilToDevicePixel(m_textDecorationStyle.textDecorationThickness().resolve(m_textDecorationStyle.computedFontSize(), fontMetrics), m_deviceScaleFactor);
    FloatPoint localOrigin = boxOrigin;

    auto paintDecoration = [&] (TextDecorationLine decoration, TextDecorationStyle style, const Color& color, const FloatRect& rect) {
        m_context.setStrokeColor(color);

        auto strokeStyle = textDecorationStyleToStrokeStyle(style);

        if (style == TextDecorationStyle::Wavy)
            strokeWavyTextDecoration(m_context, rect, m_textDecorationStyle.computedFontPixelSize());
        else if (decoration == TextDecorationLine::Underline || decoration == TextDecorationLine::Overline) {
            if ((m_textDecorationStyle.textDecorationSkipInk() == TextDecorationSkipInk::Auto || m_textDecorationStyle.textDecorationSkipInk() == TextDecorationSkipInk::All) && m_isHorizontal) {
                if (!m_context.paintingDisabled()) {
                    FloatRect underlineBoundingBox = m_context.computeUnderlineBoundsForText(rect, m_isPrinting);
                    DashArray intersections = m_font.dashesForIntersectionsWithRect(textRun, textOrigin, underlineBoundingBox);
                    DashArray boundaries = translateIntersectionPointsToSkipInkBoundaries(intersections, underlineBoundingBox.height(), rect.width());
                    ASSERT(!(boundaries.size() % 2));
                    // We don't use underlineBoundingBox here because drawLinesForText() will run computeUnderlineBoundsForText() internally.
                    m_context.drawLinesForText(rect.location(), rect.height(), boundaries, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
                }
            } else {
                // FIXME: Need to support text-decoration-skip: none.
                m_context.drawLineForText(rect, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
            }
        } else
            ASSERT_NOT_REACHED();
    };

    bool areLinesOpaque = !m_isPrinting && (!m_textDecorations.contains(TextDecorationLine::Underline) || m_styles.underline.color.isOpaque())
        && (!m_textDecorations.contains(TextDecorationLine::Overline) || m_styles.overline.color.isOpaque())
        && (!m_textDecorations.contains(TextDecorationLine::LineThrough) || m_styles.linethrough.color.isOpaque());

    float extraOffset = 0;
    bool clipping = !areLinesOpaque && m_shadow && m_shadow->next();
    if (clipping) {
        FloatRect clipRect(localOrigin, FloatSize(width, fontMetrics.ascent() + 2));
        for (const ShadowData* shadow = m_shadow; shadow; shadow = shadow->next()) {
            int shadowExtent = shadow->paintingExtent();
            FloatRect shadowRect(localOrigin, FloatSize(width, fontMetrics.ascent() + 2));
            shadowRect.inflate(shadowExtent);
            float shadowX = LayoutUnit(m_isHorizontal ? shadow->x().value() : shadow->y().value());
            float shadowY = LayoutUnit(m_isHorizontal ? shadow->y().value() : -shadow->x().value());
            shadowRect.move(shadowX, shadowY);
            clipRect.unite(shadowRect);
            extraOffset = std::max(extraOffset, std::max(0.f, shadowY) + shadowExtent);
        }
        m_context.save();
        m_context.clip(clipRect);
        extraOffset += fontMetrics.ascent() + 2;
        localOrigin.move(0, extraOffset);
    }

    // These decorations should match the visual overflows computed in visualOverflowForDecorations().
    auto underlineRect = FloatRect { localOrigin, FloatSize(width, textDecorationThickness) };
    auto overlineRect = underlineRect;
    if (m_textDecorations.contains(TextDecorationLine::Underline))
        underlineRect.move(0.f, underlineOffset + (m_styles.underline.decorationStyle == TextDecorationStyle::Wavy ? wavyOffset : 0.f));
    if (m_textDecorations.contains(TextDecorationLine::Overline)) {
        auto autoTextDecorationThickness = ceilToDevicePixel(TextDecorationThickness::createWithAuto().resolve(m_textDecorationStyle.computedFontSize(), fontMetrics), m_deviceScaleFactor);
        overlineRect.move(0.f, autoTextDecorationThickness - textDecorationThickness - (m_styles.overline.decorationStyle == TextDecorationStyle::Wavy ? wavyOffset : 0.f));
    }

    const ShadowData* shadow = m_shadow;
    do {
        if (shadow) {
            if (!shadow->next()) {
                // The last set of lines paints normally inside the clip.
                localOrigin.move(0, -extraOffset);
                extraOffset = 0;
            }
            float shadowX = LayoutUnit(m_isHorizontal ? shadow->x().value() : shadow->y().value());
            float shadowY = LayoutUnit(m_isHorizontal ? shadow->y().value() : -shadow->x().value());
            
            Color shadowColor = shadow->color();
            if (m_shadowColorFilter)
                m_shadowColorFilter->transformColor(shadowColor);
            m_context.setShadow(FloatSize(shadowX, shadowY - extraOffset), shadow->radius().value(), shadowColor);
            shadow = shadow->next();
        }
        if (m_textDecorations.contains(TextDecorationLine::Underline))
            paintDecoration(TextDecorationLine::Underline, m_styles.underline.decorationStyle, m_styles.underline.color, underlineRect);
        if (m_textDecorations.contains(TextDecorationLine::Overline))
            paintDecoration(TextDecorationLine::Overline, m_styles.overline.decorationStyle, m_styles.overline.color, overlineRect);
        // We only want to paint the shadow, hence the transparent color, not the actual line-through,
        // which will be painted in paintForegroundDecorations().
        if (shadow && m_textDecorations.contains(TextDecorationLine::LineThrough))
            paintLineThrough(Color::transparentBlack, textDecorationThickness, localOrigin, width);
    } while (shadow);

    if (clipping)
        m_context.restore();
    else if (m_shadow)
        m_context.clearShadow();
}

void TextDecorationPainter::paintForegroundDecorations(const FloatPoint& boxOrigin, float width)
{
    if (!m_textDecorations.contains(TextDecorationLine::LineThrough))
        return;

    auto textDecorationThickness = ceilToDevicePixel(m_textDecorationStyle.textDecorationThickness().resolve(m_textDecorationStyle.computedFontSize(), m_textDecorationStyle.metricsOfPrimaryFont()), m_deviceScaleFactor);
    paintLineThrough(m_styles.linethrough.color, textDecorationThickness, boxOrigin, width);
}

void TextDecorationPainter::paintLineThrough(const Color& color, float thickness, const FloatPoint& localOrigin, float width)
{
    const auto& fontMetrics = m_textDecorationStyle.metricsOfPrimaryFont();
    FloatRect rect(localOrigin, FloatSize(width, thickness));
    auto autoTextDecorationThickness = ceilToDevicePixel(TextDecorationThickness::createWithAuto().resolve(m_textDecorationStyle.computedFontSize(), fontMetrics), m_deviceScaleFactor);
    auto center = 2 * fontMetrics.floatAscent() / 3 + autoTextDecorationThickness / 2;
    rect.move(0, center - thickness / 2);

    m_context.setStrokeColor(color);

    TextDecorationStyle style = m_styles.linethrough.decorationStyle;
    auto strokeStyle = textDecorationStyleToStrokeStyle(style);

    if (style == TextDecorationStyle::Wavy)
        strokeWavyTextDecoration(m_context, rect, m_textDecorationStyle.computedFontPixelSize());
    else
        m_context.drawLineForText(rect, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
}

static void collectStylesForRenderer(TextDecorationPainter::Styles& result, const RenderObject& renderer, OptionSet<TextDecorationLine> remainingDecorations, bool firstLineStyle, PseudoId pseudoId)
{
    auto extractDecorations = [&] (const RenderStyle& style, OptionSet<TextDecorationLine> decorations) {
        if (decorations.isEmpty())
            return;

        auto color = TextDecorationPainter::decorationColor(style);
        auto decorationStyle = style.textDecorationStyle();

        if (decorations.contains(TextDecorationLine::Underline)) {
            remainingDecorations.remove(TextDecorationLine::Underline);
            result.underline.color = color;
            result.underline.decorationStyle = decorationStyle;
        }
        if (decorations.contains(TextDecorationLine::Overline)) {
            remainingDecorations.remove(TextDecorationLine::Overline);
            result.overline.color = color;
            result.overline.decorationStyle = decorationStyle;
        }
        if (decorations.contains(TextDecorationLine::LineThrough)) {
            remainingDecorations.remove(TextDecorationLine::LineThrough);
            result.linethrough.color = color;
            result.linethrough.decorationStyle = decorationStyle;
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
        extractDecorations(style, style.textDecorationLine());

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

Color TextDecorationPainter::decorationColor(const RenderStyle& style)
{
    return style.visitedDependentColorWithColorFilter(CSSPropertyTextDecorationColor);
}

auto TextDecorationPainter::stylesForRenderer(const RenderObject& renderer, OptionSet<TextDecorationLine> requestedDecorations, bool firstLineStyle, PseudoId pseudoId) -> Styles
{
    if (requestedDecorations.isEmpty())
        return { };

    Styles result;
    collectStylesForRenderer(result, renderer, requestedDecorations, false, pseudoId);
    if (firstLineStyle)
        collectStylesForRenderer(result, renderer, requestedDecorations, true, pseudoId);
    return result;
}

} // namespace WebCore
