/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

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
#include "SVGRenderStyle.h"

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "IntRect.h"
#include "NodeRenderStyle.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "SVGStyledElement.h"

using namespace std;

namespace WebCore {

SVGRenderStyle::SVGRenderStyle()
{
    static SVGRenderStyle* defaultStyle = new SVGRenderStyle(CreateDefault);

    fill = defaultStyle->fill;
    stroke = defaultStyle->stroke;
    text = defaultStyle->text;
    stops = defaultStyle->stops;
    clip = defaultStyle->clip;
    mask = defaultStyle->mask;
    misc = defaultStyle->misc;
    markers = defaultStyle->markers;
    shadowSVG = defaultStyle->shadowSVG;

    setBitDefaults();
}

SVGRenderStyle::SVGRenderStyle(CreateDefaultType)
{
    setBitDefaults();

    fill.init();
    stroke.init();
    text.init();
    stops.init();
    clip.init();
    mask.init();
    misc.init();
    markers.init();
    shadowSVG.init();
}

SVGRenderStyle::SVGRenderStyle(const SVGRenderStyle& other)
    : RefCounted<SVGRenderStyle>()
{
    fill = other.fill;
    stroke = other.stroke;
    text = other.text;
    stops = other.stops;
    clip = other.clip;
    mask = other.mask;
    misc = other.misc;
    markers = other.markers;
    shadowSVG = other.shadowSVG;

    svg_inherited_flags = other.svg_inherited_flags;
    svg_noninherited_flags = other.svg_noninherited_flags;
}

SVGRenderStyle::~SVGRenderStyle()
{
}

bool SVGRenderStyle::operator==(const SVGRenderStyle& o) const
{
    return (fill == o.fill && stroke == o.stroke && text == o.text &&
        stops == o.stops && clip == o.clip && mask == o.mask &&
        misc == o.misc && markers == o.markers && shadowSVG == o.shadowSVG &&
        svg_inherited_flags == o.svg_inherited_flags &&
        svg_noninherited_flags == o.svg_noninherited_flags);
}

bool SVGRenderStyle::inheritedNotEqual(const SVGRenderStyle* other) const
{
    return (fill != other->fill
            || stroke != other->stroke
            || markers != other->markers
            || text != other->text
            || svg_inherited_flags != other->svg_inherited_flags);
}

void SVGRenderStyle::inheritFrom(const SVGRenderStyle* svgInheritParent)
{
    if (!svgInheritParent)
        return;

    fill = svgInheritParent->fill;
    stroke = svgInheritParent->stroke;
    markers = svgInheritParent->markers;
    text = svgInheritParent->text;

    svg_inherited_flags = svgInheritParent->svg_inherited_flags;
}

float SVGRenderStyle::cssPrimitiveToLength(const RenderObject* item, CSSValue* value, float defaultValue)
{
    CSSPrimitiveValue* primitive = static_cast<CSSPrimitiveValue*>(value);

    unsigned short cssType = (primitive ? primitive->primitiveType() : (unsigned short) CSSPrimitiveValue::CSS_UNKNOWN);
    if (!(cssType > CSSPrimitiveValue::CSS_UNKNOWN && cssType <= CSSPrimitiveValue::CSS_PC))
        return defaultValue;

    if (cssType == CSSPrimitiveValue::CSS_PERCENTAGE) {
        SVGStyledElement* element = static_cast<SVGStyledElement*>(item->node());
        SVGElement* viewportElement = (element ? element->viewportElement() : 0);
        if (viewportElement) {
            float result = primitive->getFloatValue() / 100.0f;
            return SVGLength::PercentageOfViewport(result, element, LengthModeOther);
        }
    }

    return primitive->computeLengthFloat(const_cast<RenderStyle*>(item->style()), item->document()->documentElement()->renderStyle());
}


static void getSVGShadowExtent(ShadowData* shadow, int& top, int& right, int& bottom, int& left)
{
    top = 0;
    right = 0;
    bottom = 0;
    left = 0;

    int blurAndSpread = shadow->blur + shadow->spread;

    top = min(top, shadow->y - blurAndSpread);
    right = max(right, shadow->x + blurAndSpread);
    bottom = max(bottom, shadow->y + blurAndSpread);
    left = min(left, shadow->x - blurAndSpread);
}

void SVGRenderStyle::inflateForShadow(IntRect& repaintRect) const
{
    ShadowData* svgShadow = shadow();
    if (!svgShadow)
        return;

    FloatRect repaintFloatRect = FloatRect(repaintRect);
    inflateForShadow(repaintFloatRect);
    repaintRect = enclosingIntRect(repaintFloatRect);
}

void SVGRenderStyle::inflateForShadow(FloatRect& repaintRect) const
{
    ShadowData* svgShadow = shadow();
    if (!svgShadow)
        return;

    int shadowTop;
    int shadowRight;
    int shadowBottom;
    int shadowLeft;
    getSVGShadowExtent(svgShadow, shadowTop, shadowRight, shadowBottom, shadowLeft);

    int overflowLeft = repaintRect.x() + shadowLeft;
    int overflowRight = repaintRect.right() + shadowRight;
    int overflowTop = repaintRect.y() + shadowTop;
    int overflowBottom = repaintRect.bottom() + shadowBottom;

    repaintRect.setX(overflowLeft);
    repaintRect.setY(overflowTop);
    repaintRect.setWidth(overflowRight - overflowLeft);
    repaintRect.setHeight(overflowBottom - overflowTop);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
