/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "config.h"
#include "SVGTextLayoutEngineBaseline.h"

#include "FontCascade.h"
#include "RenderElement.h"
#include "SVGLengthContext.h"
#include "SVGRenderStyle.h"
#include "SVGTextMetrics.h"

namespace WebCore {

SVGTextLayoutEngineBaseline::SVGTextLayoutEngineBaseline(const FontCascade& font)
    : m_font(font)
{
}

float SVGTextLayoutEngineBaseline::calculateBaselineShift(const SVGRenderStyle& style, SVGElement* context) const
{
    if (style.baselineShift() == BaselineShift::Length) {
        auto baselineShiftValueLength = style.baselineShiftValue();
        if (baselineShiftValueLength.lengthType() == SVGLengthType::Percentage)
            return baselineShiftValueLength.valueAsPercentage() * m_font.pixelSize();

        SVGLengthContext lengthContext(context);
        return baselineShiftValueLength.value(lengthContext);
    }

    switch (style.baselineShift()) {
    case BaselineShift::Baseline:
        return 0;
    case BaselineShift::Sub:
        return -m_font.fontMetrics().floatHeight() / 2;
    case BaselineShift::Super:
        return m_font.fontMetrics().floatHeight() / 2;
    case BaselineShift::Length:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

AlignmentBaseline SVGTextLayoutEngineBaseline::dominantBaselineToAlignmentBaseline(bool isVerticalText, const RenderObject* textRenderer) const
{
    ASSERT(textRenderer);
    ASSERT(textRenderer->parent());

    const SVGRenderStyle& svgStyle = textRenderer->style().svgStyle();

    DominantBaseline baseline = svgStyle.dominantBaseline();
    if (baseline == DominantBaseline::Auto) {
        if (isVerticalText)
            baseline = DominantBaseline::Central;
        else
            baseline = DominantBaseline::Alphabetic;
    }

    switch (baseline) {
    case DominantBaseline::UseScript:
        // FIXME: The dominant-baseline and the baseline-table components are set by determining the predominant script of the character data content.
        return AlignmentBaseline::Alphabetic;
    case DominantBaseline::NoChange:
        return dominantBaselineToAlignmentBaseline(isVerticalText, textRenderer->parent());
    case DominantBaseline::ResetSize:
        return dominantBaselineToAlignmentBaseline(isVerticalText, textRenderer->parent());
    case DominantBaseline::Ideographic:
        return AlignmentBaseline::Ideographic;
    case DominantBaseline::Alphabetic:
        return AlignmentBaseline::Alphabetic;
    case DominantBaseline::Hanging:
        return AlignmentBaseline::Hanging;
    case DominantBaseline::Mathematical:
        return AlignmentBaseline::Mathematical;
    case DominantBaseline::Central:
        return AlignmentBaseline::Central;
    case DominantBaseline::Middle:
        return AlignmentBaseline::Middle;
    case DominantBaseline::TextAfterEdge:
        return AlignmentBaseline::TextAfterEdge;
    case DominantBaseline::TextBeforeEdge:
        return AlignmentBaseline::TextBeforeEdge;
    default:
        ASSERT_NOT_REACHED();
        return AlignmentBaseline::Auto;
    }
}

float SVGTextLayoutEngineBaseline::calculateAlignmentBaselineShift(bool isVerticalText, const RenderObject& textRenderer) const
{
    const RenderObject* textRendererParent = textRenderer.parent();
    ASSERT(textRendererParent);

    AlignmentBaseline baseline = textRenderer.style().svgStyle().alignmentBaseline();
    if (baseline == AlignmentBaseline::Auto) {
        baseline = dominantBaselineToAlignmentBaseline(isVerticalText, textRendererParent);
        ASSERT(baseline != AlignmentBaseline::Auto);
    }

    const FontMetrics& fontMetrics = m_font.fontMetrics();

    // Note: http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
    switch (baseline) {
    case AlignmentBaseline::Baseline:
        // FIXME: This seems wrong. Why are we returning an enum value converted to a float?
        return static_cast<float>(dominantBaselineToAlignmentBaseline(isVerticalText, textRendererParent));
    case AlignmentBaseline::BeforeEdge:
    case AlignmentBaseline::TextBeforeEdge:
        return fontMetrics.floatAscent();
    case AlignmentBaseline::Middle:
        return fontMetrics.xHeight() / 2;
    case AlignmentBaseline::Central:
        return (fontMetrics.floatAscent() - fontMetrics.floatDescent()) / 2;
    case AlignmentBaseline::AfterEdge:
    case AlignmentBaseline::TextAfterEdge:
    case AlignmentBaseline::Ideographic:
        return fontMetrics.floatDescent();
    case AlignmentBaseline::Alphabetic:
        return 0;
    case AlignmentBaseline::Hanging:
        return fontMetrics.floatAscent() * 8 / 10.f;
    case AlignmentBaseline::Mathematical:
        return fontMetrics.floatAscent() / 2;
    case AlignmentBaseline::Auto:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

float SVGTextLayoutEngineBaseline::calculateGlyphOrientationAngle(bool isVerticalText, const SVGRenderStyle& style, const UChar& character) const
{
    switch (isVerticalText ? style.glyphOrientationVertical() : style.glyphOrientationHorizontal()) {
    case GlyphOrientation::Auto:
        // Spec: Fullwidth ideographic and fullwidth Latin text will be set with a glyph-orientation of 0-degrees.
        // Text which is not fullwidth will be set with a glyph-orientation of 90-degrees.
        // FIXME: There's not an accurate way to tell if text is fullwidth by looking at a single character.
        switch (static_cast<UEastAsianWidth>(u_getIntPropertyValue(character, UCHAR_EAST_ASIAN_WIDTH))) {
        case U_EA_NEUTRAL:
        case U_EA_HALFWIDTH:
        case U_EA_NARROW:
            return 90;
        case U_EA_AMBIGUOUS:
        case U_EA_FULLWIDTH:
        case U_EA_WIDE:
            return 0;
        }
        ASSERT_NOT_REACHED();
        break;
    case GlyphOrientation::Degrees90:
        return 90;
    case GlyphOrientation::Degrees180:
        return 180;
    case GlyphOrientation::Degrees270:
        return 270;
    case GlyphOrientation::Degrees0:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static inline bool glyphOrientationIsMultiplyOf180Degrees(float orientationAngle)
{
    return !fabsf(fmodf(orientationAngle, 180));
}

float SVGTextLayoutEngineBaseline::calculateGlyphAdvanceAndOrientation(bool isVerticalText, SVGTextMetrics& metrics, float angle, float& xOrientationShift, float& yOrientationShift) const
{
    bool orientationIsMultiplyOf180Degrees = glyphOrientationIsMultiplyOf180Degrees(angle);

    // The function is based on spec requirements:
    //
    // Spec: If the 'glyph-orientation-horizontal' results in an orientation angle that is not a multiple of
    // of 180 degrees, then the current text position is incremented according to the vertical metrics of the glyph.
    //
    // Spec: If if the 'glyph-orientation-vertical' results in an orientation angle that is not a multiple of
    // 180 degrees, then the current text position is incremented according to the horizontal metrics of the glyph.

    const FontMetrics& fontMetrics = m_font.fontMetrics();

    // Vertical orientation handling.
    if (isVerticalText) {
        float ascentMinusDescent = fontMetrics.floatAscent() - fontMetrics.floatDescent();
        if (!angle) {
            xOrientationShift = (ascentMinusDescent - metrics.width()) / 2;
            yOrientationShift = fontMetrics.floatAscent();
        } else if (angle == 180)
            xOrientationShift = (ascentMinusDescent + metrics.width()) / 2;
        else if (angle == 270) {
            yOrientationShift = metrics.width();
            xOrientationShift = ascentMinusDescent;
        }

        // Vertical advance calculation.
        if (angle && !orientationIsMultiplyOf180Degrees)
            return metrics.width();

        return metrics.height();
    }

    // Horizontal orientation handling.
    if (angle == 90)
        yOrientationShift = -metrics.width();
    else if (angle == 180) {
        xOrientationShift = metrics.width();
        yOrientationShift = -fontMetrics.floatAscent();
    } else if (angle == 270)
        xOrientationShift = metrics.width();

    // Horizontal advance calculation.
    if (angle && !orientationIsMultiplyOf180Degrees)
        return metrics.height();

    return metrics.width();
}

}
