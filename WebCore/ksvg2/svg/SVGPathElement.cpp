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

#ifdef SVG_SUPPORT
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
    , DeprecatedSVGPathParser()
    , m_pathLength(0.0)
{
}

SVGPathElement::~SVGPathElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGPathElement, double, Number, number, PathLength, pathLength, SVGNames::pathLengthAttr.localName(), m_pathLength)

double SVGPathElement::getTotalLength()
{
    return 0;
}

FloatPoint SVGPathElement::getPointAtLength(double /*distance*/)
{
    return FloatPoint();
}

unsigned long SVGPathElement::getPathSegAtLength(double)
{
    return 0;
}

SVGPathSegClosePath* SVGPathElement::createSVGPathSegClosePath()
{
    return new SVGPathSegClosePath();
}

SVGPathSegMovetoAbs* SVGPathElement::createSVGPathSegMovetoAbs(double x, double y)
{
    SVGPathSegMovetoAbs* temp = new SVGPathSegMovetoAbs();
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegMovetoRel* SVGPathElement::createSVGPathSegMovetoRel(double x, double y)
{
    SVGPathSegMovetoRel* temp = new SVGPathSegMovetoRel();
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoAbs* SVGPathElement::createSVGPathSegLinetoAbs(double x, double y)
{
    SVGPathSegLinetoAbs* temp = new SVGPathSegLinetoAbs();
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoRel* SVGPathElement::createSVGPathSegLinetoRel(double x, double y)
{
    SVGPathSegLinetoRel* temp = new SVGPathSegLinetoRel();
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoCubicAbs* SVGPathElement::createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2)
{
    SVGPathSegCurvetoCubicAbs* temp = new SVGPathSegCurvetoCubicAbs();
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoCubicRel* SVGPathElement::createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2)
{
    SVGPathSegCurvetoCubicRel* temp = new SVGPathSegCurvetoCubicRel();
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoQuadraticAbs* SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1)
{
    SVGPathSegCurvetoQuadraticAbs* temp = new SVGPathSegCurvetoQuadraticAbs();
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    return temp;
}

SVGPathSegCurvetoQuadraticRel* SVGPathElement::createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1)
{
    SVGPathSegCurvetoQuadraticRel* temp = new SVGPathSegCurvetoQuadraticRel();
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    return temp;
}

SVGPathSegArcAbs* SVGPathElement::createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag)
{
    SVGPathSegArcAbs* temp = new SVGPathSegArcAbs();
    temp->setX(x);
    temp->setY(y);
    temp->setR1(r1);
    temp->setR2(r2);
    temp->setAngle(angle);
    temp->setLargeArcFlag(largeArcFlag);
    temp->setSweepFlag(sweepFlag);
    return temp;
}

SVGPathSegArcRel* SVGPathElement::createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag)
{
    SVGPathSegArcRel* temp = new SVGPathSegArcRel();
    temp->setX(x);
    temp->setY(y);
    temp->setR1(r1);
    temp->setR2(r2);
    temp->setAngle(angle);
    temp->setLargeArcFlag(largeArcFlag);
    temp->setSweepFlag(sweepFlag);
    return temp;
}

SVGPathSegLinetoHorizontalAbs* SVGPathElement::createSVGPathSegLinetoHorizontalAbs(double x)
{
    SVGPathSegLinetoHorizontalAbs* temp = new SVGPathSegLinetoHorizontalAbs();
    temp->setX(x);
    return temp;
}

SVGPathSegLinetoHorizontalRel* SVGPathElement::createSVGPathSegLinetoHorizontalRel(double x)
{
    SVGPathSegLinetoHorizontalRel* temp = new SVGPathSegLinetoHorizontalRel();
    temp->setX(x);
    return temp;
}

SVGPathSegLinetoVerticalAbs* SVGPathElement::createSVGPathSegLinetoVerticalAbs(double y)
{
    SVGPathSegLinetoVerticalAbs* temp = new SVGPathSegLinetoVerticalAbs();
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoVerticalRel* SVGPathElement::createSVGPathSegLinetoVerticalRel(double y)
{
    SVGPathSegLinetoVerticalRel* temp = new SVGPathSegLinetoVerticalRel();
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoCubicSmoothAbs* SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2)
{
    SVGPathSegCurvetoCubicSmoothAbs* temp = new SVGPathSegCurvetoCubicSmoothAbs();
    temp->setX(x);
    temp->setY(y);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoCubicSmoothRel* SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2)
{
    SVGPathSegCurvetoCubicSmoothRel* temp = new SVGPathSegCurvetoCubicSmoothRel();
    temp->setX(x);
    temp->setY(y);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoQuadraticSmoothAbs* SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y)
{
    SVGPathSegCurvetoQuadraticSmoothAbs* temp = new SVGPathSegCurvetoQuadraticSmoothAbs();
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoQuadraticSmoothRel* SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y)
{
    SVGPathSegCurvetoQuadraticSmoothRel* temp = new SVGPathSegCurvetoQuadraticSmoothRel();
    temp->setX(x);
    temp->setY(y);
    return temp;
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
    if (attr->name() == SVGNames::dAttr) {
        ExceptionCode ec;
        pathSegList()->clear(ec);
        parseSVG(attr->value().deprecatedString(), true);
    } else {
        if (SVGTests::parseMappedAttribute(attr)) return;
        if (SVGLangSpace::parseMappedAttribute(attr)) return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
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

// FIXME: This probably belongs on SVGPathSegList instead. -- ecs 12/5/05
Path SVGPathElement::toPathData() const
{
    Path pathData;
    // TODO : also support non-normalized mode, at least in dom structure
    int len = pathSegList()->numberOfItems();
    if (len < 1)
        return pathData;

    ExceptionCode ec = 0;
    for (int i = 0; i < len; ++i) {
        SVGPathSeg* p = pathSegList()->getItem(i, ec).get();;
        switch (p->pathSegType())
        {
            case SVGPathSeg::PATHSEG_MOVETO_ABS:
            {
                SVGPathSegMovetoAbs* moveTo = static_cast<SVGPathSegMovetoAbs* >(p);
                pathData.moveTo(FloatPoint(moveTo->x(), moveTo->y()));
                break;
            }
            case SVGPathSeg::PATHSEG_LINETO_ABS:
            {
                SVGPathSegLinetoAbs* lineTo = static_cast<SVGPathSegLinetoAbs* >(p);
                pathData.addLineTo(FloatPoint(lineTo->x(), lineTo->y()));
                break;
            }
            case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
            {
                SVGPathSegCurvetoCubicAbs* curveTo = static_cast<SVGPathSegCurvetoCubicAbs* >(p);
                pathData.addBezierCurveTo(FloatPoint(curveTo->x1(), curveTo->y1()),
                                 FloatPoint(curveTo->x2(), curveTo->y2()),
                                 FloatPoint(curveTo->x(), curveTo->y()));
                break;
            }
            case SVGPathSeg::PATHSEG_CLOSEPATH:
                pathData.closeSubpath();
            default:
                break;
        }
    }

    return pathData;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
