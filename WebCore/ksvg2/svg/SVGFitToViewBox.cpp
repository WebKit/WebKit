/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include "Attr.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRect.h"
#include "SVGSVGElement.h"
#include "StringImpl.h"
#include "svgpathparser.h"

namespace WebCore {

SVGFitToViewBox::SVGFitToViewBox()
{
}

SVGFitToViewBox::~SVGFitToViewBox()
{
}

SVGAnimatedRect* SVGFitToViewBox::viewBox() const
{
    if (!m_viewBox) {
        //const SVGStyledElement *context = dynamic_cast<const SVGStyledElement *>(this);
        m_viewBox = new SVGAnimatedRect(0); // FIXME: 0 is a hack
    }

    return m_viewBox.get();
}

SVGAnimatedPreserveAspectRatio* SVGFitToViewBox::preserveAspectRatio() const
{
    if (!m_preserveAspectRatio) {
        //const SVGStyledElement *context = dynamic_cast<const SVGStyledElement *>(this);
        m_preserveAspectRatio = new SVGAnimatedPreserveAspectRatio(0); // FIXME: 0 is a hack
    }

    return m_preserveAspectRatio.get();
}

void SVGFitToViewBox::parseViewBox(StringImpl* str)
{
    double x = 0, y = 0, w = 0, h = 0;
    DeprecatedString viewbox = String(str).deprecatedString();
    const char *p = viewbox.latin1();
    const char *end = p + viewbox.length();
    const char *c = p;
    p = parseCoord(c, x);
    if (p == c)
        goto bail_out;
    if (*p == ',')
        p++;
    while (*p == ' ')
        p++;

    c = p;
    p = parseCoord(c, y);
    if (p == c)
        goto bail_out;

    if (*p == ',')
        p++;
    while (*p == ' ')
        p++;

    c = p;
    p = parseCoord(c, w);
    if(w < 0.0 || p == c) // check that width is positive
        goto bail_out;
    if (*p == ',')
        p++;
    while (*p == ' ')
        p++;

    c = p;
    p = parseCoord(c, h);
    if (h < 0.0 || p == c) // check that height is positive
        goto bail_out;
    if (*p == ',')
        p++;
    while (*p == ' ')
        p++;

    if (p < end) // nothing should come after the last, fourth number
        goto bail_out;

    viewBox()->baseVal()->setX(x);
    viewBox()->baseVal()->setY(y);
    viewBox()->baseVal()->setWidth(w);
    viewBox()->baseVal()->setHeight(h);
    return;

bail_out:;
    // FIXME: Per the spec we are supposed to set the document into an "error state" here.
}

SVGMatrix* SVGFitToViewBox::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    SVGRect* viewBoxRect = viewBox()->baseVal();
    if(viewBoxRect->width() == 0 || viewBoxRect->height() == 0)
        return SVGSVGElement::createSVGMatrix();

    return preserveAspectRatio()->baseVal()->getCTM(viewBoxRect->x(),
            viewBoxRect->y(), viewBoxRect->width(), viewBoxRect->height(),
            0, 0, viewWidth, viewHeight);
}

bool SVGFitToViewBox::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::viewBoxAttr) {
        parseViewBox(attr->value().impl());
        return true;
    } else if (attr->name() == SVGNames::preserveAspectRatioAttr) {
        preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(attr->value().impl());
        return true;
    }

    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

