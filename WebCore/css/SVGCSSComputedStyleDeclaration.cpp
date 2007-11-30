/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

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

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "Document.h"

namespace WebCore {

static CSSPrimitiveValue* glyphOrientationToCSSPrimitiveValue(EGlyphOrientation orientation)
{
    switch (orientation) {
        case GO_0DEG:
            return new CSSPrimitiveValue(0.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_90DEG:
            return new CSSPrimitiveValue(90.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_180DEG:
            return new CSSPrimitiveValue(180.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_270DEG:
            return new CSSPrimitiveValue(270.0f, CSSPrimitiveValue::CSS_DEG);
        default:
            return 0;
    }
}

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
        case CSS_PROP_CLIP_RULE:
            return new CSSPrimitiveValue(svgStyle->clipRule());
        case CSS_PROP_FLOOD_OPACITY:
            return new CSSPrimitiveValue(svgStyle->floodOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_STOP_OPACITY:
            return new CSSPrimitiveValue(svgStyle->stopOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_POINTER_EVENTS:
            return new CSSPrimitiveValue(svgStyle->pointerEvents());
        case CSS_PROP_COLOR_INTERPOLATION:
            return new CSSPrimitiveValue(svgStyle->colorInterpolation());
        case CSS_PROP_COLOR_INTERPOLATION_FILTERS:
            return new CSSPrimitiveValue(svgStyle->colorInterpolationFilters());
        case CSS_PROP_FILL_OPACITY:
            return new CSSPrimitiveValue(svgStyle->fillOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_FILL_RULE:
            return new CSSPrimitiveValue(svgStyle->fillRule());
        case CSS_PROP_COLOR_RENDERING:
            return new CSSPrimitiveValue(svgStyle->colorRendering());
        case CSS_PROP_IMAGE_RENDERING:
            return new CSSPrimitiveValue(svgStyle->imageRendering());
        case CSS_PROP_SHAPE_RENDERING:
            return new CSSPrimitiveValue(svgStyle->shapeRendering());
        case CSS_PROP_STROKE_LINECAP:
            return new CSSPrimitiveValue(svgStyle->capStyle());
        case CSS_PROP_STROKE_LINEJOIN:
            return new CSSPrimitiveValue(svgStyle->joinStyle());
        case CSS_PROP_STROKE_MITERLIMIT:
            return new CSSPrimitiveValue(svgStyle->strokeMiterLimit(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_STROKE_OPACITY:
            return new CSSPrimitiveValue(svgStyle->strokeOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_TEXT_RENDERING:
            return new CSSPrimitiveValue(svgStyle->textRendering());
        case CSS_PROP_ALIGNMENT_BASELINE:
            return new CSSPrimitiveValue(svgStyle->alignmentBaseline());
        case CSS_PROP_DOMINANT_BASELINE:
            return new CSSPrimitiveValue(svgStyle->dominantBaseline());
        case CSS_PROP_TEXT_ANCHOR:
            return new CSSPrimitiveValue(svgStyle->textAnchor());
        case CSS_PROP_WRITING_MODE:
            return new CSSPrimitiveValue(svgStyle->writingMode());
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
        case CSS_PROP_GLYPH_ORIENTATION_HORIZONTAL:
            return glyphOrientationToCSSPrimitiveValue(svgStyle->glyphOrientationHorizontal());
        case CSS_PROP_GLYPH_ORIENTATION_VERTICAL: {
            if (CSSPrimitiveValue* value = glyphOrientationToCSSPrimitiveValue(svgStyle->glyphOrientationVertical()))
                return value;

            if (svgStyle->glyphOrientationVertical() == GO_AUTO)
                return new CSSPrimitiveValue(CSS_VAL_AUTO);

            return 0;
        }
        case CSS_PROP_MARKER:
        case CSS_PROP_ENABLE_BACKGROUND:
        case CSS_PROP_COLOR_PROFILE:
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
