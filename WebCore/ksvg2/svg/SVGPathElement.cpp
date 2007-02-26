/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG)
#include "SVGPathElement.h"

#include "SVGNames.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegList.h"
#include "SVGPathSegMoveto.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGPathElement::SVGPathElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGPathParser()
    , m_pathLength(0.0)
{
}

SVGPathElement::~SVGPathElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGPathElement, double, Number, number, PathLength, pathLength, SVGNames::pathLengthAttr.localName(), m_pathLength)

double SVGPathElement::getTotalLength()
{
    // FIXME: this may wish to use the pathSegList instead of the pathdata if that's cheaper to build (or cached)
    return toPathData().length();
}

FloatPoint SVGPathElement::getPointAtLength(double length)
{
    // FIXME: this may wish to use the pathSegList instead of the pathdata if that's cheaper to build (or cached)
    bool ok = false;
    return toPathData().pointAtLength(length, ok);
}

unsigned long SVGPathElement::getPathSegAtLength(double length)
{
    return pathSegList()->getPathSegAtLength(length);
}

SVGPathSegClosePath* SVGPathElement::createSVGPathSegClosePath()
{
    return new SVGPathSegClosePath();
}

SVGPathSegMovetoAbs* SVGPathElement::createSVGPathSegMovetoAbs(double x, double y)
{
    return new SVGPathSegMovetoAbs(x, y);
}

SVGPathSegMovetoRel* SVGPathElement::createSVGPathSegMovetoRel(double x, double y)
{
    return new SVGPathSegMovetoRel(x, y);
}

SVGPathSegLinetoAbs* SVGPathElement::createSVGPathSegLinetoAbs(double x, double y)
{
    return new SVGPathSegLinetoAbs(x, y);
}

SVGPathSegLinetoRel* SVGPathElement::createSVGPathSegLinetoRel(double x, double y)
{
    return new SVGPathSegLinetoRel(x, y);
}

SVGPathSegCurvetoCubicAbs* SVGPathElement::createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2)
{
    return new SVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2);
}

SVGPathSegCurvetoCubicRel* SVGPathElement::createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2)
{
    return new SVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2);
}

SVGPathSegCurvetoQuadraticAbs* SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1)
{
    return new SVGPathSegCurvetoQuadraticAbs(x, y, x1, y1);
}

SVGPathSegCurvetoQuadraticRel* SVGPathElement::createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1)
{
    return new SVGPathSegCurvetoQuadraticRel(x, y, x1, y1);
}

SVGPathSegArcAbs* SVGPathElement::createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag)
{
    return new SVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

SVGPathSegArcRel* SVGPathElement::createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag)
{
    return new SVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

SVGPathSegLinetoHorizontalAbs* SVGPathElement::createSVGPathSegLinetoHorizontalAbs(double x)
{
    return new SVGPathSegLinetoHorizontalAbs(x);
}

SVGPathSegLinetoHorizontalRel* SVGPathElement::createSVGPathSegLinetoHorizontalRel(double x)
{
    return new SVGPathSegLinetoHorizontalRel(x);
}

SVGPathSegLinetoVerticalAbs* SVGPathElement::createSVGPathSegLinetoVerticalAbs(double y)
{
    return new SVGPathSegLinetoVerticalAbs(y);
}

SVGPathSegLinetoVerticalRel* SVGPathElement::createSVGPathSegLinetoVerticalRel(double y)
{
    return new SVGPathSegLinetoVerticalRel(y);
}

SVGPathSegCurvetoCubicSmoothAbs* SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2)
{
    return new SVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2);
}

SVGPathSegCurvetoCubicSmoothRel* SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2)
{
    return new SVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2);
}

SVGPathSegCurvetoQuadraticSmoothAbs* SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y)
{
    return new SVGPathSegCurvetoQuadraticSmoothAbs(x, y);
}

SVGPathSegCurvetoQuadraticSmoothRel* SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y)
{
    return new SVGPathSegCurvetoQuadraticSmoothRel(x, y);
}

void SVGPathElement::svgMoveTo(double x1, double y1, bool, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegMovetoAbs(x1, y1), ec);
    else
        pathSegList()->appendItem(createSVGPathSegMovetoRel(x1, y1), ec);
}

void SVGPathElement::svgLineTo(double x1, double y1, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegLinetoAbs(x1, y1), ec);
    else
        pathSegList()->appendItem(createSVGPathSegLinetoRel(x1, y1), ec);
}

void SVGPathElement::svgLineToHorizontal(double x, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegLinetoHorizontalAbs(x), ec);
    else
        pathSegList()->appendItem(createSVGPathSegLinetoHorizontalRel(x), ec);
}

void SVGPathElement::svgLineToVertical(double y, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegLinetoVerticalAbs(y), ec);
    else
        pathSegList()->appendItem(createSVGPathSegLinetoVerticalRel(y), ec);
}

void SVGPathElement::svgCurveToCubic(double x1, double y1, double x2, double y2, double x, double y, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2), ec);
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2), ec);
}

void SVGPathElement::svgCurveToCubicSmooth(double x, double y, double x2, double y2, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y), ec);
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y), ec);
}

void SVGPathElement::svgCurveToQuadratic(double x, double y, double x1, double y1, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y), ec);
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticRel(x1, y1, x, y), ec);
}

void SVGPathElement::svgCurveToQuadraticSmooth(double x, double y, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticSmoothAbs(x, y), ec);
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticSmoothRel(x, y), ec);
}

void SVGPathElement::svgArcTo(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, bool abs)
{
    ExceptionCode ec = 0;

    if (abs)
        pathSegList()->appendItem(createSVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag), ec);
    else
        pathSegList()->appendItem(createSVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag), ec);
}

void SVGPathElement::svgClosePath()
{
    ExceptionCode ec = 0;
    pathSegList()->appendItem(createSVGPathSegClosePath(), ec);
}

void SVGPathElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::dAttr) {
        ExceptionCode ec;
        pathSegList()->clear(ec);
        if (!parseSVG(value, true))
            document()->accessSVGExtensions()->reportError("Problem parsing d=\"" + value + "\"");
    } else if (attr->name() == SVGNames::pathLengthAttr) {
        m_pathLength = value.toDouble();
        if (m_pathLength < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for path attribute <pathLength> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGPathElement::notifyAttributeChange() const
{
    if (!ownerDocument()->parsing())
        rebuildRenderer();

    SVGStyledElement::notifyAttributeChange();
}

SVGPathSegList* SVGPathElement::pathSegList() const
{
    if (!m_pathSegList)
        m_pathSegList = new SVGPathSegList(this);

    return m_pathSegList.get();
}

SVGPathSegList* SVGPathElement::normalizedPathSegList() const
{
    // TODO
    return 0;
}

SVGPathSegList* SVGPathElement::animatedPathSegList() const
{
    // TODO
    return 0;
}

SVGPathSegList* SVGPathElement::animatedNormalizedPathSegList() const
{
    // TODO
    return 0;
}

Path SVGPathElement::toPathData() const
{
    return pathSegList()->toPathData();
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
