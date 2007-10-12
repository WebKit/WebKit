/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#include "config.h"
#if ENABLE(SVG)
#include "CSSComputedStyleDeclaration.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "RenderStyle.h"

namespace WebCore {
    
PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getSVGPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    Node* node = m_node.get();
    if (!node)
        return 0;
    
    // Make sure our layout is up to date before we allow a query on these attributes.
    if (updateLayout)
        node->document()->updateLayout();
        
    RenderStyle* style = node->computedStyle();
    if (!style)
        return 0;
    
    const SVGRenderStyle* svgStyle = style->svgStyle();
    if (!svgStyle)
        return 0;
    
    switch (static_cast<CSSPropertyID>(propertyID)) {
        case CSS_PROP_CLIP_RULE: {
            if (svgStyle->clipRule() == RULE_NONZERO)
                return new CSSPrimitiveValue(CSS_VAL_NONZERO);
            else
                return new CSSPrimitiveValue(CSS_VAL_EVENODD);
        }
        case CSS_PROP_FLOOD_OPACITY:
            return new CSSPrimitiveValue(svgStyle->floodOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_STOP_OPACITY:
            return new CSSPrimitiveValue(svgStyle->stopOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_POINTER_EVENTS: {
            switch (svgStyle->pointerEvents()) {
            case PE_NONE:
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            case PE_STROKE:
                return new CSSPrimitiveValue(CSS_VAL_STROKE);
            case PE_FILL:
                return new CSSPrimitiveValue(CSS_VAL_FILL);
            case PE_PAINTED:
                return new CSSPrimitiveValue(CSS_VAL_PAINTED);
            case PE_VISIBLE:
                return new CSSPrimitiveValue(CSS_VAL_VISIBLE);
            case PE_VISIBLE_STROKE:
                return new CSSPrimitiveValue(CSS_VAL_VISIBLESTROKE);
            case PE_VISIBLE_FILL:
                return new CSSPrimitiveValue(CSS_VAL_VISIBLEFILL);
            case PE_VISIBLE_PAINTED:
                return new CSSPrimitiveValue(CSS_VAL_VISIBLEPAINTED);
            case PE_ALL:
                return new CSSPrimitiveValue(CSS_VAL_ALL);
            }
        }
        case CSS_PROP_COLOR_INTERPOLATION: {
            if (svgStyle->colorInterpolation() == CI_AUTO)
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            else if (svgStyle->colorInterpolation() == CI_SRGB)
                return new CSSPrimitiveValue(CSS_VAL_SRGB);
            else
                return new CSSPrimitiveValue(CSS_VAL_LINEARRGB);
        }
        case CSS_PROP_COLOR_INTERPOLATION_FILTERS: {
            if (svgStyle->colorInterpolationFilters() == CI_AUTO)
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            else if (svgStyle->colorInterpolationFilters() == CI_SRGB)
                return new CSSPrimitiveValue(CSS_VAL_SRGB);
            else
                return new CSSPrimitiveValue(CSS_VAL_LINEARRGB);
        }
        case CSS_PROP_FILL_OPACITY:
            return new CSSPrimitiveValue(svgStyle->fillOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_FILL_RULE: {
            if (svgStyle->fillRule() == RULE_NONZERO)
                return new CSSPrimitiveValue(CSS_VAL_NONZERO);
            else
                return new CSSPrimitiveValue(CSS_VAL_EVENODD);
        }
        case CSS_PROP_COLOR_RENDERING: {
            if (svgStyle->colorRendering() == CR_AUTO)
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            else if (svgStyle->colorRendering() == CR_OPTIMIZESPEED)
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZESPEED);
            else
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZEQUALITY);
        }
        case CSS_PROP_IMAGE_RENDERING: {
            if (svgStyle->imageRendering() == IR_AUTO)
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            else if (svgStyle->imageRendering() == IR_OPTIMIZESPEED)
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZESPEED);
            else
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZEQUALITY);
        }
        case CSS_PROP_SHAPE_RENDERING: {
            if (svgStyle->shapeRendering() == SR_AUTO)
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            else if (svgStyle->shapeRendering() == SR_OPTIMIZESPEED)
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZESPEED);
            else if (svgStyle->shapeRendering() == SR_CRISPEDGES)
                return new CSSPrimitiveValue(CSS_VAL_CRISPEDGES);
            else
                return new CSSPrimitiveValue(CSS_VAL_GEOMETRICPRECISION);
        }
        case CSS_PROP_STROKE_LINECAP: {
            if (svgStyle->capStyle() == ButtCap)
                return new CSSPrimitiveValue(CSS_VAL_BUTT);
            else if (svgStyle->capStyle() == RoundCap)
                return new CSSPrimitiveValue(CSS_VAL_ROUND);
            else
                return new CSSPrimitiveValue(CSS_VAL_SQUARE);
        }
        case CSS_PROP_STROKE_LINEJOIN: {
            if (svgStyle->joinStyle() == MiterJoin)
                return new CSSPrimitiveValue(CSS_VAL_MITER);
            else if (svgStyle->joinStyle() == RoundJoin)
                return new CSSPrimitiveValue(CSS_VAL_ROUND);
            else
                return new CSSPrimitiveValue(CSS_VAL_BEVEL);
        }
        case CSS_PROP_STROKE_MITERLIMIT:
            return new CSSPrimitiveValue(svgStyle->strokeMiterLimit(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_STROKE_OPACITY:
            return new CSSPrimitiveValue(svgStyle->strokeOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_TEXT_RENDERING: {
            switch (svgStyle->textRendering()) {
            case TR_AUTO:
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            case TR_OPTIMIZESPEED:
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZESPEED);
            case TR_OPTIMIZELEGIBILITY:
                return new CSSPrimitiveValue(CSS_VAL_OPTIMIZELEGIBILITY);
            case TR_GEOMETRICPRECISION:
                return new CSSPrimitiveValue(CSS_VAL_GEOMETRICPRECISION);
            }
        }
        case CSS_PROP_ALIGNMENT_BASELINE: {
            switch (svgStyle->alignmentBaseline()) {
            case AB_AUTO:
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            case AB_BASELINE:
                return new CSSPrimitiveValue(CSS_VAL_BASELINE);
            case AB_BEFORE_EDGE:
                return new CSSPrimitiveValue(CSS_VAL_BEFORE_EDGE);
            case AB_TEXT_BEFORE_EDGE:
                return new CSSPrimitiveValue(CSS_VAL_TEXT_BEFORE_EDGE);
            case AB_MIDDLE:
                return new CSSPrimitiveValue(CSS_VAL_MIDDLE);
            case AB_CENTRAL:
                return new CSSPrimitiveValue(CSS_VAL_CENTRAL);
            case AB_AFTER_EDGE:
                return new CSSPrimitiveValue(CSS_VAL_AFTER_EDGE);
            case AB_TEXT_AFTER_EDGE:
                return new CSSPrimitiveValue(CSS_VAL_TEXT_AFTER_EDGE);
            case AB_IDEOGRAPHIC:
                return new CSSPrimitiveValue(CSS_VAL_IDEOGRAPHIC);
            case AB_ALPHABETIC:
                return new CSSPrimitiveValue(CSS_VAL_ALPHABETIC);
            case AB_HANGING:
                return new CSSPrimitiveValue(CSS_VAL_HANGING);
            case AB_MATHEMATICAL:
                return new CSSPrimitiveValue(CSS_VAL_MATHEMATICAL);
            }
        }
        case CSS_PROP_DOMINANT_BASELINE: {
            switch (svgStyle->dominantBaseline()) {
            case DB_AUTO:
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            case DB_USE_SCRIPT:
                return new CSSPrimitiveValue(CSS_VAL_USE_SCRIPT);
            case DB_NO_CHANGE:
                return new CSSPrimitiveValue(CSS_VAL_NO_CHANGE);
            case DB_RESET_SIZE:
                return new CSSPrimitiveValue(CSS_VAL_RESET_SIZE);
            case DB_CENTRAL:
                return new CSSPrimitiveValue(CSS_VAL_CENTRAL);
            case DB_MIDDLE:
                return new CSSPrimitiveValue(CSS_VAL_MIDDLE);
            case DB_TEXT_BEFORE_EDGE:
                return new CSSPrimitiveValue(CSS_VAL_TEXT_BEFORE_EDGE);
            case DB_TEXT_AFTER_EDGE:
                return new CSSPrimitiveValue(CSS_VAL_TEXT_AFTER_EDGE);
            case DB_IDEOGRAPHIC:
                return new CSSPrimitiveValue(CSS_VAL_IDEOGRAPHIC);
            case DB_ALPHABETIC:
                return new CSSPrimitiveValue(CSS_VAL_ALPHABETIC);
            case DB_HANGING:
                return new CSSPrimitiveValue(CSS_VAL_HANGING);
            case DB_MATHEMATICAL:
                return new CSSPrimitiveValue(CSS_VAL_MATHEMATICAL);
            }
        }
        case CSS_PROP_TEXT_ANCHOR: {
            if (svgStyle->textAnchor() == TA_START)
                return new CSSPrimitiveValue(CSS_VAL_START);
            else if (svgStyle->textAnchor() == TA_MIDDLE)
                return new CSSPrimitiveValue(CSS_VAL_MIDDLE);
            else
                return new CSSPrimitiveValue(CSS_VAL_END);
        }
        case CSS_PROP_WRITING_MODE: {
            switch (svgStyle->writingMode()) {
            case WM_LRTB:
                return new CSSPrimitiveValue(CSS_VAL_LR_TB);
            case WM_LR:
                return new CSSPrimitiveValue(CSS_VAL_LR);
            case WM_RLTB:
                return new CSSPrimitiveValue(CSS_VAL_RL_TB);
            case WM_RL:
                return new CSSPrimitiveValue(CSS_VAL_RL);
            case WM_TBRL:
                return new CSSPrimitiveValue(CSS_VAL_TB_RL);
            case WM_TB:
                return new CSSPrimitiveValue(CSS_VAL_TB);
            }
        }
        case CSS_PROP_CLIP_PATH:
            if (!svgStyle->clipPath().isEmpty())
                return new CSSPrimitiveValue(svgStyle->clipPath(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_MASK:
            if (!svgStyle->maskElement().isEmpty())
                return new CSSPrimitiveValue(svgStyle->maskElement(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_FILTER:
            if (!svgStyle->filter().isEmpty())
                return new CSSPrimitiveValue(svgStyle->filter(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_FLOOD_COLOR:
            return new CSSPrimitiveValue(svgStyle->floodColor().rgb());
        case CSS_PROP_LIGHTING_COLOR:
            return new CSSPrimitiveValue(svgStyle->lightingColor().rgb());
        case CSS_PROP_STOP_COLOR:
            return new CSSPrimitiveValue(svgStyle->stopColor().rgb());
        case CSS_PROP_FILL:
            return svgStyle->fillPaint();
        case CSS_PROP_KERNING:
            return svgStyle->kerning();
        case CSS_PROP_MARKER_END:
            if (!svgStyle->endMarker().isEmpty())
                return new CSSPrimitiveValue(svgStyle->endMarker(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_MARKER_MID:
            if (!svgStyle->midMarker().isEmpty())
                return new CSSPrimitiveValue(svgStyle->midMarker(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_MARKER_START:
            if (!svgStyle->startMarker().isEmpty())
                return new CSSPrimitiveValue(svgStyle->startMarker(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_STROKE:
            return svgStyle->strokePaint();
        case CSS_PROP_STROKE_DASHARRAY:
            return svgStyle->strokeDashArray();
        case CSS_PROP_STROKE_DASHOFFSET:
            return svgStyle->strokeDashOffset();
        case CSS_PROP_STROKE_WIDTH:
            return svgStyle->strokeWidth();
        case CSS_PROP_BASELINE_SHIFT: {
            switch (svgStyle->baselineShift()) {
            case BS_BASELINE:
                return new CSSPrimitiveValue(CSS_VAL_BASELINE);
            case BS_SUPER:
                return new CSSPrimitiveValue(CSS_VAL_SUPER);
            case BS_SUB:
                return new CSSPrimitiveValue(CSS_VAL_SUB);
            case BS_LENGTH:
                return svgStyle->baselineShiftValue();
            }
        }
        case CSS_PROP_MARKER:
        case CSS_PROP_ENABLE_BACKGROUND:
        case CSS_PROP_COLOR_PROFILE:
        case CSS_PROP_GLYPH_ORIENTATION_HORIZONTAL:
        case CSS_PROP_GLYPH_ORIENTATION_VERTICAL:
            // the above properties are not yet implemented in the engine
            break;
    default:
        // If you crash here, it's because you added a css property and are not handling it
        // in either this switch statement or the one in CSSComputedStyleDelcaration::getPropertyCSSValue
        ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", propertyID);
    }
    LOG_ERROR("unimplemented propertyID: %d", propertyID);
    return 0;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
