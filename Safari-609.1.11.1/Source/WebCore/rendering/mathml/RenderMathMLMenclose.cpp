/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMathMLMenclose.h"

#if ENABLE(MATHML)

#include "GraphicsContext.h"
#include "MathMLNames.h"
#include "PaintInfo.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLMenclose);

// The MathML in HTML5 implementation note suggests drawing the left part of longdiv with a parenthesis.
// For now, we use a Bezier curve and this somewhat arbitrary value.
const unsigned short longDivLeftSpace = 10;

RenderMathMLMenclose::RenderMathMLMenclose(MathMLMencloseElement& element, RenderStyle&& style)
    : RenderMathMLRow(element, WTFMove(style))
{
}

// This arbitrary thickness value is used for the parameter \xi_8 from the MathML in HTML5 implementation note.
// For now, we take:
// - OverbarVerticalGap = UnderbarVerticalGap = 3\xi_8
// - OverbarRuleThickness = UnderbarRuleThickness = \xi_8
// - OverbarExtraAscender = UnderbarExtraAscender = \xi_8
// FIXME: OverBar and UnderBar parameters should be read from the MATH tables.
// See https://bugs.webkit.org/show_bug.cgi?id=122297
LayoutUnit RenderMathMLMenclose::ruleThickness() const
{
    return LayoutUnit(0.05f * style().fontCascade().size());
}

RenderMathMLMenclose::SpaceAroundContent RenderMathMLMenclose::spaceAroundContent(LayoutUnit contentWidth, LayoutUnit contentHeight) const
{
    SpaceAroundContent space;
    space.right = 0;
    space.top = 0;
    space.bottom = 0;
    space.left = 0;

    LayoutUnit thickness = ruleThickness();
    // In the MathML in HTML5 implementation note, the "left" notation is described as follows:
    // - left side is 3\xi_8 padding + \xi_8 border + \xi_8 margin = 5\xi_8
    // - top space is Overbar Vertical Gap + Overbar Rule Thickness = 3\xi_8 + \xi_8 = 4\xi_8
    // - bottom space is Underbar Vertical Gap + Underbar Rule Thickness = 3\xi_8 + \xi_8 = 4\xi_8
    // The "right" notation is symmetric.
    if (hasNotation(MathMLMencloseElement::Left))
        space.left = std::max(space.left, 5 * thickness);
    if (hasNotation(MathMLMencloseElement::Right))
        space.right = std::max(space.right, 5 * thickness);
    if (hasNotation(MathMLMencloseElement::Left) || hasNotation(MathMLMencloseElement::Right)) {
        LayoutUnit extraSpace = 4 * thickness;
        space.top = std::max(space.top, extraSpace);
        space.bottom = std::max(space.bottom, extraSpace);
    }

    // In the MathML in HTML5 implementation note, the "top" notation is described as follows:
    // - left and right space are 4\xi_8
    // - top side is Vertical Gap + Rule Thickness + Extra Ascender = 3\xi_8 + \xi_8 + \xi_8 = 5\xi_8
    // The "bottom" notation is symmetric.
    if (hasNotation(MathMLMencloseElement::Top))
        space.top = std::max(space.top, 5 * thickness);
    if (hasNotation(MathMLMencloseElement::Bottom))
        space.bottom = std::max(space.bottom, 5 * thickness);
    if (hasNotation(MathMLMencloseElement::Top) || hasNotation(MathMLMencloseElement::Bottom)) {
        LayoutUnit extraSpace = 4 * thickness;
        space.left = std::max(space.left, extraSpace);
        space.right = std::max(space.right, extraSpace);
    }

    // For longdiv, we use our own rules for now:
    // - top space is like "top" notation
    // - bottom space is like "bottom" notation
    // - right space is like "right" notation
    // - left space is longDivLeftSpace * \xi_8
    if (hasNotation(MathMLMencloseElement::LongDiv)) {
        space.top = std::max(space.top, 5 * thickness);
        space.bottom = std::max(space.bottom, 5 * thickness);
        space.left = std::max(space.left, longDivLeftSpace * thickness);
        space.right = std::max(space.right, 4 * thickness);
    }

    // In the MathML in HTML5 implementation note, the "rounded" notation is described as follows:
    // - top/bottom/left/right side have 3\xi_8 padding + \xi_8 border + \xi_8 margin = 5\xi_8
    if (hasNotation(MathMLMencloseElement::RoundedBox)) {
        LayoutUnit extraSpace = 5 * thickness;
        space.left = std::max(space.left, extraSpace);
        space.right = std::max(space.right, extraSpace);
        space.top = std::max(space.top, extraSpace);
        space.bottom = std::max(space.bottom, extraSpace);
    }

    // In the MathML in HTML5 implementation note, the "rounded" notation is described as follows:
    // - top/bottom/left/right spaces are \xi_8/2
    if (hasNotation(MathMLMencloseElement::UpDiagonalStrike) || hasNotation(MathMLMencloseElement::DownDiagonalStrike)) {
        LayoutUnit extraSpace = thickness / 2;
        space.left = std::max(space.left, extraSpace);
        space.right = std::max(space.right, extraSpace);
        space.top = std::max(space.top, extraSpace);
        space.bottom = std::max(space.bottom, extraSpace);
    }

    // In the MathML in HTML5 implementation note, the "circle" notation is described as follows:
    // - We draw the ellipse of axes the axes of symmetry of this ink box
    // - The radii of the ellipse are \sqrt{2}contentWidth/2 and \sqrt{2}contentHeight/2
    // - The thickness of the ellipse is \xi_8
    // - We add extra margin of \xi_8
    // Then for example the top space is \sqrt{2}contentHeight/2 - contentHeight/2 + \xi_8/2 + \xi_8.
    if (hasNotation(MathMLMencloseElement::Circle)) {
        LayoutUnit extraSpace { (contentWidth * (sqrtOfTwoFloat - 1) + 3 * thickness) / 2 };
        space.left = std::max(space.left, extraSpace);
        space.right = std::max(space.right, extraSpace);
        extraSpace = (contentHeight * (sqrtOfTwoFloat - 1) + 3 * thickness) / 2;
        space.top = std::max(space.top, extraSpace);
        space.bottom = std::max(space.bottom, extraSpace);
    }

    // In the MathML in HTML5 implementation note, the "vertical" and "horizontal" notations do not add space around the content.

    return space;
}

void RenderMathMLMenclose::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    RenderMathMLRow::computePreferredLogicalWidths();

    LayoutUnit preferredWidth = m_maxPreferredLogicalWidth;
    SpaceAroundContent space = spaceAroundContent(preferredWidth, 0);
    m_maxPreferredLogicalWidth = space.left + preferredWidth + space.right;
    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth;

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLMenclose::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutUnit contentWidth, contentAscent, contentDescent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(contentWidth, contentAscent, contentDescent);
    layoutRowItems(contentWidth, contentAscent);

    SpaceAroundContent space = spaceAroundContent(contentWidth, contentAscent + contentDescent);
    setLogicalWidth(space.left + contentWidth + space.right);
    setLogicalHeight(space.top + contentAscent + contentDescent + space.bottom);

    LayoutPoint contentLocation(space.left, space.top);
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
        child->setLocation(child->location() + contentLocation);

    m_contentRect = LayoutRect(space.left, space.top, contentWidth, contentAscent + contentDescent);

    layoutPositionedObjects(relayoutChildren);

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

// GraphicsContext::drawLine does not seem appropriate to draw menclose lines.
// To avoid unexpected behaviors and inconsistency with other notations, we just use strokePath.
static void drawLine(PaintInfo& info, const LayoutUnit& xStart, const LayoutUnit& yStart, const LayoutUnit& xEnd, const LayoutUnit& yEnd)
{
    Path line;
    line.moveTo(LayoutPoint(xStart, yStart));
    line.addLineTo(LayoutPoint(xEnd, yEnd));
    info.context().strokePath(line);
}

void RenderMathMLMenclose::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLRow::paint(info, paintOffset);

    if (info.context().paintingDisabled() || info.phase != PaintPhase::Foreground || style().visibility() != Visibility::Visible)
        return;

    LayoutUnit thickness = ruleThickness();

    // Make a copy of the PaintInfo because applyTransform will modify its rect.
    PaintInfo paintInfo(info);
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().setStrokeThickness(thickness);
    paintInfo.context().setStrokeStyle(SolidStroke);
    paintInfo.context().setStrokeColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
    paintInfo.context().setFillColor(Color::transparent);
    paintInfo.applyTransform(AffineTransform().translate(paintOffset + location()));

    // In the MathML in HTML5 implementation note, the "left" notation is described as follows:
    // - center of the left vertical bar is at 3\xi_8 padding + \xi_8 border/2 = 7\xi_8/2
    // - top space is Overbar Vertical Gap + Overbar Rule Thickness = 3\xi_8 + \xi_8 = 4\xi_8
    // - bottom space is Underbar Vertical Gap + Underbar Rule Thickness = 3\xi_8 + \xi_8 = 4\xi_8
    if (hasNotation(MathMLMencloseElement::Left)) {
        LayoutUnit x = m_contentRect.x() - 7 * thickness / 2;
        LayoutUnit yStart = m_contentRect.y() - 4 * thickness;
        LayoutUnit yEnd = m_contentRect.maxY() + 4 * thickness;
        drawLine(info, x, yStart, x, yEnd);
    }

    // In the MathML in HTML5 implementation note, the "right" notation is described as follows:
    // - center of the right vertical bar is at 3\xi_8 padding + \xi_8 border/2 = 7\xi_8/2
    // - top space is Overbar Vertical Gap + Overbar Rule Thickness = 3\xi_8 + \xi_8 = 4\xi_8
    // - bottom space is Underbar Vertical Gap + Underbar Rule Thickness = 3\xi_8 + \xi_8 = 4\xi_8
    if (hasNotation(MathMLMencloseElement::Right)) {
        LayoutUnit x = m_contentRect.maxX() + 7 * thickness / 2;
        LayoutUnit yStart = m_contentRect.y() - 4 * thickness;
        LayoutUnit yEnd = m_contentRect.maxY() + 4 * thickness;
        drawLine(info, x, yStart, x, yEnd);
    }

    // In the MathML in HTML5 implementation note, the "vertical" notation is horizontally centered.
    if (hasNotation(MathMLMencloseElement::VerticalStrike)) {
        LayoutUnit x = m_contentRect.x() + (m_contentRect.width() - thickness) / 2;
        LayoutUnit yStart = m_contentRect.y();
        LayoutUnit yEnd = m_contentRect.maxY();
        drawLine(info, x, yStart, x, yEnd);
    }

    // In the MathML in HTML5 implementation note, the "top" notation is described as follows:
    // - middle of the top horizontal bar is at Vertical Gap + Rule Thickness / 2 = 7\xi_8/2
    // - left and right spaces have size 4\xi_8
    if (hasNotation(MathMLMencloseElement::Top)) {
        LayoutUnit y = m_contentRect.y() - 7 * thickness / 2;
        LayoutUnit xStart = m_contentRect.x() - 4 * thickness;
        LayoutUnit xEnd = m_contentRect.maxX() + 4 * thickness;
        drawLine(info, xStart, y, xEnd, y);
    }

    // In the MathML in HTML5 implementation note, the "bottom" notation is described as follows:
    // - middle of the bottom horizontal bar is at Vertical Gap + Rule Thickness / 2 = 7\xi_8/2
    // - left and right spaces have size 4\xi_8
    if (hasNotation(MathMLMencloseElement::Bottom)) {
        LayoutUnit y = m_contentRect.maxY() + 7 * thickness / 2;
        LayoutUnit xStart = m_contentRect.x() - 4 * thickness;
        LayoutUnit xEnd = m_contentRect.maxX() + 4 * thickness;
        drawLine(info, xStart, y, xEnd, y);
    }

    // In the MathML in HTML5 implementation note, the "vertical" notation is vertically centered.
    if (hasNotation(MathMLMencloseElement::HorizontalStrike)) {
        LayoutUnit y = m_contentRect.y() + (m_contentRect.height() - thickness) / 2;
        LayoutUnit xStart = m_contentRect.x();
        LayoutUnit xEnd = m_contentRect.maxX();
        drawLine(info, xStart, y, xEnd, y);
    }

    // In the MathML in HTML5 implementation note, the "updiagonalstrike" goes from the bottom left corner
    // to the top right corner.
    if (hasNotation(MathMLMencloseElement::UpDiagonalStrike))
        drawLine(info, m_contentRect.x(), m_contentRect.maxY(), m_contentRect.maxX(), m_contentRect.y());

    // In the MathML in HTML5 implementation note, the "downdiagonalstrike" goes from the top left corner
    // to the bottom right corner.
    if (hasNotation(MathMLMencloseElement::DownDiagonalStrike))
        drawLine(info, m_contentRect.x(), m_contentRect.y(), m_contentRect.maxX(), m_contentRect.maxY());

    // In the MathML in HTML5 implementation note, the "roundedbox" has radii size 3\xi_8 and is obtained
    // by inflating the content box by 3\xi_8 + \xi_8/2 = 7\xi_8/2
    if (hasNotation(MathMLMencloseElement::RoundedBox)) {
        LayoutSize radiiSize(3 * thickness, 3 * thickness);
        RoundedRect::Radii radii(radiiSize, radiiSize, radiiSize, radiiSize);
        RoundedRect roundedRect(m_contentRect, radii);
        roundedRect.inflate(7 * thickness / 2);
        Path path;
        path.addRoundedRect(roundedRect);
        paintInfo.context().strokePath(path);
    }

    // For longdiv, we use our own rules for now:
    // - top space is like "top" notation
    // - bottom space is like "bottom" notation
    // - right space is like "right" notation
    // - left space is longDivLeftSpace * \xi_8
    // - We subtract half of the thickness from these spaces to obtain "top", "bottom", "left"
    //   and "right" coordinates.
    // - The top bar is drawn from "right" to "left" and positioned at vertical offset "top".
    // - The left part is draw as a quadratic Bezier curve with end points going from "top" to
    //   "bottom" and positioned at horizontal offset "left".
    // - In order to force the curvature of the left part, we use a middle point that is vertically
    //   centered and shifted towards the right by longDivLeftSpace * \xi_8
    if (hasNotation(MathMLMencloseElement::LongDiv)) {
        LayoutUnit top = m_contentRect.y() - 7 * thickness / 2;
        LayoutUnit bottom = m_contentRect.maxY() + 7 * thickness / 2;
        LayoutUnit left = m_contentRect.x() - longDivLeftSpace * thickness + thickness / 2;
        LayoutUnit right = m_contentRect.maxX() + 4 * thickness;
        LayoutUnit midX = left + longDivLeftSpace * thickness;
        LayoutUnit midY = (top + bottom) / 2;
        Path path;
        path.moveTo(LayoutPoint(right, top));
        path.addLineTo(LayoutPoint(left, top));
        path.addQuadCurveTo(LayoutPoint(midX, midY), FloatPoint(left, bottom));
        paintInfo.context().strokePath(path);
    }

    // In the MathML in HTML5 implementation note, the "circle" notation is described as follows:
    // - The center and axes are the same as the content bounding box.
    // - The width of the bounding box is \xi_8/2 + contentWidth * \sqrt{2} + \xi_8/2
    // - The height is \xi_8/2 + contentHeight * \sqrt{2} + \xi_8/2
    if (hasNotation(MathMLMencloseElement::Circle)) {
        LayoutRect ellipseRect;
        ellipseRect.setWidth(m_contentRect.width() * sqrtOfTwoFloat + thickness);
        ellipseRect.setHeight(m_contentRect.height() * sqrtOfTwoFloat + thickness);
        ellipseRect.setX(m_contentRect.x() - (ellipseRect.width() - m_contentRect.width()) / 2);
        ellipseRect.setY(m_contentRect.y() - (ellipseRect.height() - m_contentRect.height()) / 2);
        Path path;
        path.addEllipse(ellipseRect);
        paintInfo.context().strokePath(path);
    }
}

}
#endif // ENABLE(MATHML)
