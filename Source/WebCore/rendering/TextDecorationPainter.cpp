/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "InlineIteratorLineBox.h"
#include "InlineTextBoxStyle.h"
#include "RenderBlock.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderText.h"
#include "StyleAppleColorFilter.h"
#include "StyleTextDecorationLine.h"
#include "TextBoxPainter.h"
#include "TextRun.h"

namespace WebCore {

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

static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out. For example, with a border width of 3, WebKit will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5. It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == StrokeStyle::DottedStroke || penStyle == StrokeStyle::DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (static_cast<int>(strokeWidth) % 2) {
        if (p1.x() == p2.x()) {
            // We're a vertical line. Adjust our x.
            p1.setX(p1.x() + 0.5f);
            p2.setX(p2.x() + 0.5f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5f);
            p2.setY(p2.y() + 0.5f);
        }
    }
}

/*
 * Draw one cubic Bezier curve and repeat the same pattern along the the decoration's axis.
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
static void strokeWavyTextDecoration(GraphicsContext& context, const FloatRect& rect, bool isPrinting, WavyStrokeParameters wavyStrokeParameters, StrokeStyle strokeStyle)
{
    if (rect.isEmpty() || !wavyStrokeParameters.step)
        return;

    // 1. Calculate the endpoints.
    FloatPoint p1 = rect.minXMinYCorner();
    FloatPoint p2 = rect.maxXMinYCorner();

    // Extent the wavy line before and after the text so it can cover the whole length.
    p1.setX(p1.x() - 2 * wavyStrokeParameters.step);
    p2.setX(p2.x() + 2 * wavyStrokeParameters.step);

    adjustLineToPixelBoundaries(p1, p2, rect.height(), context.strokeStyle());

    ASSERT(p1.y() == p2.y());
    float x1 = std::min(p1.x(), p2.x());
    float x2 = std::max(p1.x(), p2.x());

    // Ensure the wavy underline path will not have too many segments.
    static constexpr unsigned maxTextDecorationWaves = 1024;
    if (wavyStrokeParameters.step < 1 || (x2 - x1) / (2 * wavyStrokeParameters.step) > maxTextDecorationWaves) {
        context.drawLineForText(rect, isPrinting, false, strokeStyle);
        return;
    }

    // 2. Contruct the wavy underline path.
    float yAxis = p1.y();
    FloatPoint controlPoint1(0, yAxis + wavyStrokeParameters.controlPointDistance);
    FloatPoint controlPoint2(0, yAxis - wavyStrokeParameters.controlPointDistance);

    Path path;
    path.moveTo(p1);

    for (double x = x1; x + 2 * wavyStrokeParameters.step <= x2;) {
        controlPoint1.setX(x + wavyStrokeParameters.step);
        controlPoint2.setX(x + wavyStrokeParameters.step);
        x += 2 * wavyStrokeParameters.step;
        path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
    }

    // Offset the bounds and set extra height to ensure the whole wavy line is covered.
    auto bounds = rect;
    bounds.inflateY(wavyStrokeParameters.controlPointDistance);

    // 3. Draw the wavy underline path and clip the extra wavy line added before.
    GraphicsContextStateSaver stateSaver(context);
    context.clip(bounds);

    context.setShouldAntialias(true);
    context.setStrokeThickness(rect.height());
    context.strokePath(path);
}

bool TextDecorationPainter::Styles::operator==(const Styles& other) const
{
    return underline.color == other.underline.color && overline.color == other.overline.color && linethrough.color == other.linethrough.color
        && underline.decorationStyle == other.underline.decorationStyle && overline.decorationStyle == other.overline.decorationStyle && linethrough.decorationStyle == other.linethrough.decorationStyle;
}

TextDecorationPainter::TextDecorationPainter(GraphicsContext& context, const FontCascade& font, const Style::TextShadows& shadow, const Style::AppleColorFilter& colorFilter, bool isPrinting, WritingMode writingMode)
    : m_context(context)
    , m_isPrinting(isPrinting)
    , m_writingMode(writingMode)
    , m_shadow(shadow)
    , m_shadowColorFilter(colorFilter)
    , m_font(font)
{
}

// Paint text-shadow, underline, overline
void TextDecorationPainter::paintBackgroundDecorations(const RenderStyle& style, const TextRun& textRun, const BackgroundDecorationGeometry& decorationGeometry, Style::TextDecorationLine decorationType, const Styles& decorationStyle, float deviceScaleFactor)
{
    auto paintDecoration = [&] (auto decoration, auto underlineStyle, auto& color, auto& rect) {
        m_context.setStrokeColor(color);

        auto strokeStyle = textDecorationStyleToStrokeStyle(underlineStyle);
        auto paintRect = FloatRect { roundPointToDevicePixels(LayoutPoint { rect.location() }, deviceScaleFactor, textRun.ltr()), rect.size() };

        if (underlineStyle == TextDecorationStyle::Wavy)
            strokeWavyTextDecoration(m_context, paintRect, m_isPrinting, decorationGeometry.wavyStrokeParameters, strokeStyle);
        else if (decoration == Style::TextDecorationLine::Flag::Underline || decoration == Style::TextDecorationLine::Flag::Overline) {
            if ((style.textDecorationSkipInk() == TextDecorationSkipInk::Auto
                || style.textDecorationSkipInk() == TextDecorationSkipInk::All)
                && !m_writingMode.isVerticalTypographic()) {
                if (!m_context.paintingDisabled()) {
                    auto underlineBoundingBox = m_context.computeUnderlineBoundsForText(paintRect, m_isPrinting);
                    auto intersections = m_font.lineSegmentsForIntersectionsWithRect(textRun, decorationGeometry.textOrigin, underlineBoundingBox);
                    if (!intersections.isEmpty()) {
                        auto dilationAmount = std::min(underlineBoundingBox.height(), style.metricsOfPrimaryFont().height() / 5);
                        auto boundaries = differenceWithDilation({ 0, paintRect.width() }, WTF::move(intersections), dilationAmount);
                        // We don't use underlineBoundingBox here because drawLinesForText() will run computeUnderlineBoundsForText() internally.
                        m_context.drawLinesForText(paintRect.location(), paintRect.height(), boundaries.span(), m_isPrinting, underlineStyle == TextDecorationStyle::Double, strokeStyle);
                    } else
                    m_context.drawLineForText(paintRect, m_isPrinting, underlineStyle == TextDecorationStyle::Double, strokeStyle);
                }
            } else {
                // FIXME: Need to support text-decoration-skip: none.
                m_context.drawLineForText(paintRect, m_isPrinting, underlineStyle == TextDecorationStyle::Double, strokeStyle);
            }
        } else
            ASSERT_NOT_REACHED();
    };

    auto areLinesOpaque = !m_isPrinting && (!decorationType.hasUnderline() || decorationStyle.underline.color.isOpaque())
        && (!decorationType.hasOverline() || decorationStyle.overline.color.isOpaque())
        && (!decorationType.hasLineThrough() || decorationStyle.linethrough.color.isOpaque());

    float extraOffset = 0.f;
    auto boxOrigin = decorationGeometry.boxOrigin;
    bool clipping = m_shadow.size() > 1 && !areLinesOpaque;
    if (clipping) {
        auto clipRect = FloatRect { boxOrigin, FloatSize { decorationGeometry.textBoxWidth, decorationGeometry.clippingOffset } };
        const auto& zoomFactor = style.usedZoomForLength();
        for (const auto& shadow : m_shadow) {
            auto shadowExtent = Style::paintingExtent(shadow, zoomFactor);
            auto shadowRect = clipRect;
            shadowRect.inflate(shadowExtent);
            auto shadowOffset = TextBoxPainter::rotateShadowOffset(shadow.location, m_writingMode, zoomFactor);
            shadowRect.move(shadowOffset);
            clipRect.unite(shadowRect);
            extraOffset = std::max(extraOffset, std::max(0.f, shadowOffset.height()) + shadowExtent);
        }
        m_context.save();
        m_context.clip(clipRect);
        extraOffset += decorationGeometry.clippingOffset;
        boxOrigin.move(0.f, extraOffset);
    }

    // These decorations should match the visual overflows computed in visualOverflowForDecorations().
    auto underlineRect = FloatRect { boxOrigin, FloatSize { decorationGeometry.textBoxWidth, decorationGeometry.textDecorationThickness } };
    auto overlineRect = underlineRect;
    if (decorationType.hasUnderline())
        underlineRect.move(0.f, decorationGeometry.underlineOffset);
    if (decorationType.hasOverline())
        overlineRect.move(0.f, decorationGeometry.overlineOffset);

    auto draw = [&](const Style::TextShadow* shadow) {
        if (decorationType.hasUnderline() && !underlineRect.isEmpty())
            paintDecoration(Style::TextDecorationLine::Flag::Underline, decorationStyle.underline.decorationStyle, decorationStyle.underline.color, underlineRect);
        if (decorationType.hasOverline() && !overlineRect.isEmpty())
            paintDecoration(Style::TextDecorationLine::Flag::Overline, decorationStyle.overline.decorationStyle, decorationStyle.overline.color, overlineRect);
        // We only want to paint the shadow, hence the transparent color, not the actual line-through,
        // which will be painted in paintForegroundDecorations().
        if (shadow && decorationType.hasLineThrough()) {
            auto paintOrigin = roundPointToDevicePixels(LayoutPoint { boxOrigin }, deviceScaleFactor, textRun.ltr());
            paintLineThrough({ paintOrigin, decorationGeometry.textBoxWidth, decorationGeometry.textDecorationThickness, decorationGeometry.linethroughCenter, decorationGeometry.wavyStrokeParameters }, Color::transparentBlack, decorationStyle);
        }
    };

    if (m_shadow.isNone())
        draw(nullptr);
    else {
        const auto& zoomFactor = style.usedZoomForLength();
        for (const auto& shadow : m_shadow) {
            if (&shadow == &m_shadow.last()) {
                // The last set of lines paints normally inside the clip.
                boxOrigin.move(0, -extraOffset);
                extraOffset = 0;
            }
            Style::ColorResolver colorResolver { style };
            auto shadowColor = colorResolver.colorResolvingCurrentColor(shadow.color);

            m_shadowColorFilter.transformColor(shadowColor);

            auto shadowOffset = TextBoxPainter::rotateShadowOffset(shadow.location, m_writingMode, zoomFactor);
            shadowOffset.expand(0, -extraOffset);
            m_context.setDropShadow({ shadowOffset, shadow.blur.resolveZoom(zoomFactor), shadowColor, ShadowRadiusMode::Default });

            draw(&shadow);
        }
    }

    if (clipping)
        m_context.restore();
    else if (!m_shadow.isNone())
        m_context.clearDropShadow();
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
        strokeWavyTextDecoration(m_context, rect, m_isPrinting, foregroundDecorationGeometry.wavyStrokeParameters, strokeStyle);
    else
        m_context.drawLineForText(rect, m_isPrinting, style == TextDecorationStyle::Double, strokeStyle);
}

static void collectStylesForRenderer(TextDecorationPainter::Styles& result, const RenderObject& renderer, Style::TextDecorationLine remainingDecorations, bool firstLineStyle, OptionSet<PaintBehavior> paintBehavior, std::optional<PseudoElementType> pseudoElementType)
{
    auto extractDecorations = [&] (const RenderStyle& style, Style::TextDecorationLine decorations) {
        if (!decorations.containsAny({ Style::TextDecorationLine::Flag::Underline, Style::TextDecorationLine::Flag::Overline, Style::TextDecorationLine::Flag::LineThrough }))
            return;

        auto color = TextDecorationPainter::decorationColor(style, paintBehavior);
        auto decorationStyle = style.textDecorationStyle();

        if (decorations.hasUnderline()) {
            remainingDecorations.remove(Style::TextDecorationLine::Flag::Underline);
            result.underline.color = color;
            result.underline.decorationStyle = decorationStyle;
        }
        if (decorations.hasOverline()) {
            remainingDecorations.remove(Style::TextDecorationLine::Flag::Overline);
            result.overline.color = color;
            result.overline.decorationStyle = decorationStyle;
        }
        if (decorations.hasLineThrough()) {
            remainingDecorations.remove(Style::TextDecorationLine::Flag::LineThrough);
            result.linethrough.color = color;
            result.linethrough.decorationStyle = decorationStyle;
        }
    };

    auto styleForRenderer = [&] (const RenderObject& renderer) -> const RenderStyle& {
        if (pseudoElementType && renderer.style().hasPseudoStyle(*pseudoElementType)) {
            if (auto textRenderer = dynamicDowncast<RenderText>(renderer))
                return *textRenderer->getCachedPseudoStyle({ *pseudoElementType });
            return *downcast<RenderElement>(renderer).getCachedPseudoStyle({ *pseudoElementType });
        }
        return firstLineStyle ? renderer.firstLineStyle() : renderer.style();
    };

    auto* current = &renderer;
    do {
        const auto& style = styleForRenderer(*current);
        extractDecorations(style, style.textDecorationLine());

        if (current->style().display() == DisplayType::RubyAnnotation)
            return;

        current = current->parent();
        if (CheckedPtr currentBlock = dynamicDowncast<RenderBlock>(current); currentBlock && currentBlock->isAnonymousBlock()) {
            if (auto* continuation = currentBlock->continuation())
                current = continuation;
        }

        if (remainingDecorations.isNone())
            break;

    } while (current && !is<HTMLAnchorElement>(current->node()));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (!remainingDecorations.isNone() && current)
        extractDecorations(styleForRenderer(*current), remainingDecorations);
}

Color TextDecorationPainter::decorationColor(const RenderStyle& style, OptionSet<PaintBehavior> paintBehavior)
{
    if (paintBehavior.contains(PaintBehavior::ForceBlackText))
        return Color::black;

    if (paintBehavior.contains(PaintBehavior::ForceWhiteText))
        return Color::white;

    return style.visitedDependentTextDecorationColorApplyingColorFilter(paintBehavior);
}

auto TextDecorationPainter::stylesForRenderer(const RenderObject& renderer, Style::TextDecorationLine requestedDecorations, bool firstLineStyle, OptionSet<PaintBehavior> paintBehavior, std::optional<PseudoElementType> pseudoElementType) -> Styles
{
    if (requestedDecorations.isNone())
        return { };

    Styles result;
    collectStylesForRenderer(result, renderer, requestedDecorations, false, paintBehavior, pseudoElementType);
    if (firstLineStyle)
        collectStylesForRenderer(result, renderer, requestedDecorations, true, paintBehavior, pseudoElementType);
    return result;
}

Style::TextDecorationLine TextDecorationPainter::textDecorationsInEffectForStyle(const TextDecorationPainter::Styles& style)
{
    OptionSet<Style::TextDecorationLine::Flag> decorations;
    if (style.underline.color.isValid())
        decorations.add(Style::TextDecorationLine::Flag::Underline);
    if (style.overline.color.isValid())
        decorations.add(Style::TextDecorationLine::Flag::Overline);
    if (style.linethrough.color.isValid())
        decorations.add(Style::TextDecorationLine::Flag::LineThrough);
    return decorations;
}

} // namespace WebCore
