/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
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

#if ENABLE(MATHML)
#include "RenderMathMLRadicalOperator.h"

namespace WebCore {

using namespace MathMLNames;

static const UChar gRadicalCharacter = 0x221A;

// This class relies on the RenderMathMLOperator class to draw a radical symbol.
// This does not work well when an OpenType MATH font is not available.
// In that case, we fallback to the old implementation of RenderMathMLRoot.cpp with graphic primitives.

// Normal width of the front of the radical sign, before the base & overbar (em)
const float gFrontWidthEms = 0.75f;
// Horizontal position of the bottom point of the radical (* frontWidth)
const float gRadicalBottomPointXFront = 0.5f;
// Lower the radical sign's bottom point (px)
const int gRadicalBottomPointLower = 3;
// Horizontal position of the top left point of the radical "dip" (* frontWidth)
const float gRadicalDipLeftPointXFront = 0.8f;
// Vertical position of the top left point of a sqrt radical "dip" (* baseHeight)
const float gSqrtRadicalDipLeftPointYPos = 0.5f;
// Vertical shift of the left end point of the radical (em)
const float gRadicalLeftEndYShiftEms = 0.05f;

// Radical line thickness (em)
const float gRadicalLineThicknessEms = 0.02f;
// Radical thick line thickness (em)
const float gRadicalThickLineThicknessEms = 0.1f;

RenderMathMLRadicalOperator::RenderMathMLRadicalOperator(Document& document, Ref<RenderStyle>&& style)
    : RenderMathMLOperator(document, WTFMove(style), String(&gRadicalCharacter, 1), MathMLOperatorDictionary::Prefix)
{
}

void RenderMathMLRadicalOperator::stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline)
{
    if (!style().fontCascade().primaryFont().mathData()) {
        // If we do not have an OpenType MATH font, we always make the radical depth a bit larger than the target.
        depthBelowBaseline += gRadicalBottomPointLower;
    }

    RenderMathMLOperator::stretchTo(heightAboveBaseline, depthBelowBaseline);
}

void RenderMathMLRadicalOperator::setOperatorProperties()
{
    RenderMathMLOperator::setOperatorProperties();
    // We remove spacing around the radical symbol.
    setLeadingSpace(0);
    setTrailingSpace(0);
}

void RenderMathMLRadicalOperator::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (style().fontCascade().primaryFont().mathData()) {
        RenderMathMLOperator::computePreferredLogicalWidths();
        return;
    }

    // If we do not have an OpenType MATH font, the front width is just given by the gFrontWidthEms constant.
    int frontWidth = lroundf(gFrontWidthEms * style().fontSize());
    m_minPreferredLogicalWidth = frontWidth;
    m_maxPreferredLogicalWidth = frontWidth;
}

void RenderMathMLRadicalOperator::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    if (style().fontCascade().primaryFont().mathData()) {
        RenderMathMLOperator::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
        return;
    }

    // If we do not have an OpenType MATH font, the logical height is always the stretch size.
    logicalHeight = stretchSize();
    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void RenderMathMLRadicalOperator::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    if (info.context().paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE)
        return;

    if (style().fontCascade().primaryFont().mathData()) {
        RenderMathMLOperator::paint(info, paintOffset);
        return;
    }

    // If we do not have an OpenType MATH font, we paint the radical sign with graphic primitives.
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + contentBoxRect().location());
    int frontWidth = lroundf(gFrontWidthEms * style().fontSize());
    int startX = adjustedPaintOffset.x() + frontWidth;
    int baseHeight = stretchSize() - gRadicalBottomPointLower;

    float radicalDipLeftPointYPos = gSqrtRadicalDipLeftPointYPos * baseHeight;

    FloatPoint overbarLeftPoint(startX, adjustedPaintOffset.y());
    FloatPoint bottomPoint(startX - gRadicalBottomPointXFront * frontWidth, adjustedPaintOffset.y() + baseHeight + gRadicalBottomPointLower);
    FloatPoint dipLeftPoint(startX - gRadicalDipLeftPointXFront * frontWidth, adjustedPaintOffset.y() + radicalDipLeftPointYPos);
    FloatPoint leftEnd(startX - frontWidth, dipLeftPoint.y() + gRadicalLeftEndYShiftEms * style().fontSize());

    GraphicsContextStateSaver stateSaver(info.context());

    info.context().setStrokeThickness(gRadicalLineThicknessEms * style().fontSize());
    info.context().setStrokeStyle(SolidStroke);
    info.context().setStrokeColor(style().visitedDependentColor(CSSPropertyColor));
    info.context().setLineJoin(MiterJoin);
    info.context().setMiterLimit(style().fontSize());

    Path root;

    root.moveTo(FloatPoint(overbarLeftPoint.x(), adjustedPaintOffset.y()));
    // draw from top left corner to bottom point of radical
    root.addLineTo(bottomPoint);
    // draw from bottom point to top of left part of radical base "dip"
    root.addLineTo(dipLeftPoint);
    // draw to end
    root.addLineTo(leftEnd);

    info.context().strokePath(root);

    GraphicsContextStateSaver maskStateSaver(info.context());

    // Build a mask to draw the thick part of the root.
    Path maskPath;

    maskPath.moveTo(overbarLeftPoint);
    maskPath.addLineTo(bottomPoint);
    maskPath.addLineTo(dipLeftPoint);
    maskPath.addLineTo(FloatPoint(2 * dipLeftPoint.x() - leftEnd.x(), 2 * dipLeftPoint.y() - leftEnd.y()));

    info.context().clipPath(maskPath);

    // Draw the thick part of the root.
    info.context().setStrokeThickness(gRadicalThickLineThicknessEms * style().fontSize());
    info.context().setLineCap(SquareCap);

    Path line;
    line.moveTo(bottomPoint);
    line.addLineTo(dipLeftPoint);

    info.context().strokePath(line);
}

}

#endif
