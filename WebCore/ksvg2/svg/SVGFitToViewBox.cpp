/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGFitToViewBox.h"

#include "FloatRect.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "StringImpl.h"

namespace WebCore {

SVGFitToViewBox::SVGFitToViewBox()
    : m_viewBox()
    , m_preserveAspectRatio(new SVGPreserveAspectRatio(0))
{
}

SVGFitToViewBox::~SVGFitToViewBox()
{
}

ANIMATED_PROPERTY_DEFINITIONS_WITH_CONTEXT(SVGFitToViewBox, FloatRect, Rect, rect, ViewBox, viewBox, SVGNames::viewBoxAttr.localName(), m_viewBox)
ANIMATED_PROPERTY_DEFINITIONS_WITH_CONTEXT(SVGFitToViewBox, SVGPreserveAspectRatio*, PreserveAspectRatio, preserveAspectRatio, PreserveAspectRatio, preserveAspectRatio, SVGNames::preserveAspectRatioAttr.localName(), m_preserveAspectRatio.get())

void SVGFitToViewBox::parseViewBox(const String& str)
{
    double x = 0, y = 0, w = 0, h = 0;
    const UChar* c = str.characters();
    const UChar* end = c + str.length();

    skipOptionalSpaces(c, end);

    if (!parseNumber(c, end, x))
        goto bail_out;

    if (!parseNumber(c, end, y))
        goto bail_out;

    if (!parseNumber(c, end, w) || w < 0.0) // check that width is positive
        goto bail_out;

    if (!parseNumber(c, end, h, false) || h < 0.0) // check that height is positive
        goto bail_out;

    skipOptionalSpaces(c, end);

    if (c < end) // nothing should come after the last, fourth number
        goto bail_out;

    setViewBoxBaseValue(FloatRect(x, y, w, h));
    return;

bail_out:;
    // FIXME: Per the spec we are supposed to set the document into an "error state" here.
}

AffineTransform SVGFitToViewBox::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    FloatRect viewBoxRect = viewBox();
    if (!viewBoxRect.width() || !viewBoxRect.height())
        return AffineTransform();

    return preserveAspectRatio()->getCTM(viewBoxRect.x(),
            viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(),
            0, 0, viewWidth, viewHeight);
}

bool SVGFitToViewBox::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::viewBoxAttr) {
        parseViewBox(attr->value());
        return true;
    } else if (attr->name() == SVGNames::preserveAspectRatioAttr) {
        preserveAspectRatioBaseValue()->parsePreserveAspectRatio(attr->value());
        return true;
    }

    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

