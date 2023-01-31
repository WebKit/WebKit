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
static void strokeWavyTextDecoration(GraphicsContext& context, const FloatRect& rect, WavyStrokeParameters wavyStrokeParameters)
{
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
    StrokeStyle strokeStyle = StrokeStyle::SolidStroke;
    switch (decorationStyle) {
    case TextDecorationStyle::Solid:
        strokeStyle = StrokeStyle::SolidStroke;
        break;
    case TextDecorationStyle::Double:
        strokeStyle = StrokeStyle::DoubleStroke;
        break;
    case TextDecorationStyle::Dotted:
        strokeStyle = StrokeStyle::DottedStroke;
        break;
    case TextDecorationStyle::Dashed:
        strokeStyle = StrokeStyle::DashedStroke;
        break;
    case TextDecorationStyle::Wavy:
        strokeStyle = StrokeStyle::WavyStroke;
        break;
    }

    return strokeStyle;
}

bool TextDecorationPainter::Styles::operator==(const Styles& other) const
{
    return underline.color == other.underline.color && overline.color == other.overline.color && linethrough.color == other.linethrough.color
        && underline.decorationStyle == other.underline.decorationStyle && overline.decorationStyle == other.overline.decorationStyle && linethrough.decorationStyle == other.linethrough.decorationStyle;
}

TextDecorationPainter::TextDecorationPainter(GraphicsContext& context, const FontCascade& font, const ShadowData* shadow, const FilterOperations* colorFilter, bool isPrinting, bool isHorizontal)
    : m_context(context)
    , m_isPrinting(isPrinting)
    , m_isHorizontal(isHorizontal)
    , m_shadow(shadow)
    , m_shadowColorFilter(colorFilter)
    , m_font(font)
{
}

// Paint text-shadow, underline, overline
void TextDecorationPainter::paintBackgroundDecorations(const TextRun& textRun, const BackgroundDecorationGeometry& decorationGeometry, OptionSet<TextDecorationLine> decorationType, const Styles& decorationStyle)
{
    auto paintDecoration = [&] (auto decoration, auto style, auto& color, auto& rect) {
        m_context.setStrokeColor(color);

        auto strokeStyle = textDecorationStyleToStrokeStyle(style);

        if (style == TextDecorationStyle::Wavy)
            strokeWavyTextDecoration(m_context, rect, decorationGeometry.wavyStrokeParameters);
        else if (decoration == TextDecorationLine::Underline || decoration == TextDecorationLine::Overline) {
            if ((decorationStyle.skipInk == TextDecorationSkipInk::Auto || decorationStyle.skipInk == TextDecorationSkipInk::All) && m_isHorizontal) {
                if (!m_context.paintingDisabled()) {
                    auto underlineBoundingBox = m_context.computeUnderlineBoundsForText(rect, m_isPrinting);
                    DashArray intersections = m_font.dashesForIntersectionsWithRect(textRun, decorationGeometry.textOrigin, underlineBoundingBox);
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

    auto areLinesOpaque = !m_isPrinting && (!decorationType.contains(TextDecorationLine::Underline) || decorationStyle.underline.color.isOpaque())
        && (!decorationType.contains(TextDecorationLine::Overline) || decorationStyle.overline.color.isOpaque())
        && (!decorationType.contains(TextDecorationLine::LineThrough) || decorationStyle.linethrough.color.isOpaque());

    float extraOffset = 0.f;
    auto boxOrigin = decorationGeometry.boxOrigin;
    bool clipping = m_shadow && m_shadow->next() && !areLinesOpaque;
    if (clipping) {
        auto clipRect = FloatRect { boxOrigin, FloatSize { decorationGeometry.textBoxWidth, decorationGeometry.clippingOffset } };
        for (const ShadowData* shadow = m_shadow; shadow; shadow = shadow->next()) {
            auto shadowExtent = shadow->paintingExtent();
            auto shadowRect = clipRect;
            shadowRect.inflate(shadowExtent);
            auto shadowX = m_isHorizontal ? shadow->x().value() : shadow->y().value();
            auto shadowY = m_isHorizontal ? shadow->y().value() : -shadow->x().value();
            shadowRect.move(shadowX, shadowY);
            clipRect.unite(shadowRect);
            extraOffset = std::max(extraOffset, std::max(0.f, shadowY) + shadowExtent);
        }
        m_context.save();
        m_context.clip(clipRect);
        extraOffset += decorationGeometry.clippingOffset;
        boxOrigin.move(0.f, extraOffset);
    }

    // These decorations should match the visual overflows computed in visualOverflowForDecorations().
    auto underlineRect = FloatRect { boxOrigin, FloatSize { decorationGeometry.textBoxWidth, decorationGeometry.textDecorationThickness } };
    auto overlineRect = underlineRect;
    if (decorationType.contains(TextDecorationLine::Underline))
        underlineRect.move(0.f, decorationGeometry.underlineOffset);
    if (decorationType.contains(TextDecorationLine::Overline))
        overlineRect.move(0.f, decorationGeometry.overlineOffset);

    auto* shadow = m_shadow;
    do {
        auto applyShadowIfNeeded = [&] {
            if (!shadow)
                return;
            if (!shadow->next()) {
                // The last set of lines paints normally inside the clip.
                boxOrigin.move(0, -extraOffset);
                extraOffset = 0;
            }
            auto shadowColor = shadow->color();
            if (m_shadowColorFilter)
                m_shadowColorFilter->transformColor(shadowColor);

            auto shadowX = m_isHorizontal ? shadow->x().value() : shadow->y().value();
            auto shadowY = m_isHorizontal ? shadow->y().value() : -shadow->x().value();
            m_context.setShadow(FloatSize { shadowX, shadowY - extraOffset }, shadow->radius().value(), shadowColor);
            shadow = shadow->next();
        };
        applyShadowIfNeeded();

        if (decorationType.contains(TextDecorationLine::Underline))
            paintDecoration(TextDecorationLine::Underline, decorationStyle.underline.decorationStyle, decorationStyle.underline.color, underlineRect);
        if (decorationType.contains(TextDecorationLine::Overline))
            paintDecoration(TextDecorationLine::Overline, decorationStyle.overline.decorationStyle, decorationStyle.overline.color, overlineRect);
        // We only want to paint the shadow, hence the transparent color, not the actual line-through,
        // which will be painted in paintForegroundDecorations().
        if (shadow && decorationType.contains(TextDecorationLine::LineThrough))
            paintLineThrough({ boxOrigin, decorationGeometry.textBoxWidth, decorationGeometry.textDecorationThickness, decorationGeometry.linethroughCenter, decorationGeometry.wavyStrokeParameters }, Color::transparentBlack, decorationStyle);
    } while (shadow);

    if (clipping)
        m_context.restore();
    else if (m_shadow)
        m_context.clearShadow();
}

void TextDecorationPainter::paintForegroundDecorations(const ForegroundDecorationGeometry& foregroundDecorationGeometry, const Styles& decorationStyle)
{
    paintLineThrough(foregroundDecorationGeometry, decorationStyle.linethrough.color, decorationStyle);
}

void TextDecorationPainter::paintLineThrough(const ForegroundDecorationGeometry& foregroundDecorationGeometry, const Color& color, const Styles& decorationStyle)
{
    auto rect = FloatRect { foregroundDecorationGeometry.boxOrigin, FloatSize { foregroundDecorationGeometry.textBoxWidth, foregroundDecorationGeometry.textDecorationThickness } };
    rect.move(0.f, foregroundDecorationGeometry.linethroughCenter);

    m_context.setStrokeColor(color);

    TextDecorationStyle style = decorationStyle.linethrough.decorationStyle;
    auto strokeStyle = textDecorationStyleToStrokeStyle(style);

    if (style == TextDecorationStyle::Wavy)
        strokeWavyTextDecoration(m_context, rect, foregroundDecorationGeometry.wavyStrokeParameters);
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
    result.skipInk = renderer.style().textDecorationSkipInk();
    return result;
}

OptionSet<TextDecorationLine> TextDecorationPainter::textDecorationsInEffectForStyle(const TextDecorationPainter::Styles& style)
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

} // namespace WebCore
