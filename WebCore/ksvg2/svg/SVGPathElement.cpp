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
#if SVG_SUPPORT
#include "Attr.h"

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRect.h"
#include "SVGPoint.h"
#include "SVGSVGElement.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegList.h"
#include "SVGPathElement.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegMoveto.h"
#include "SVGAnimatedNumber.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/device/KRenderingDevice.h>

namespace WebCore {

SVGPathElement::SVGPathElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGPathParser()
{
}

SVGPathElement::~SVGPathElement()
{
}

SVGAnimatedNumber *SVGPathElement::pathLength() const
{
    if(!m_pathLength)
    {
        lazy_create<SVGAnimatedNumber>(m_pathLength, this);
        m_pathLength->setBaseVal(0);
    }
    
    return m_pathLength.get();
}

double SVGPathElement::getTotalLength()
{
    return 0;
}

SVGPoint *SVGPathElement::getPointAtLength(double /*distance*/)
{
    SVGPoint *ret = SVGSVGElement::createSVGPoint();
    /*double totalDistance = getTotalLength();
    T2P::BezierPath *path = ownerDoc()->canvas()->toBezierPath(m_item);
    if(path)
    {
        T2P::Point p;
        path->pointTangentNormalAt(distance / totalDistance, &p);
        ret->setX(p.x());
        ret->setY(p.y());
    }*/

    return ret;
}

unsigned long SVGPathElement::getPathSegAtLength(double)
{
    return 0;
}

SVGPathSegClosePath *SVGPathElement::createSVGPathSegClosePath()
{
    return new SVGPathSegClosePath();
}

SVGPathSegMovetoAbs *SVGPathElement::createSVGPathSegMovetoAbs(double x, double y, const SVGStyledElement *context)
{
    SVGPathSegMovetoAbs *temp = new SVGPathSegMovetoAbs(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegMovetoRel *SVGPathElement::createSVGPathSegMovetoRel(double x, double y, const SVGStyledElement *context)
{
    SVGPathSegMovetoRel *temp = new SVGPathSegMovetoRel(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoAbs *SVGPathElement::createSVGPathSegLinetoAbs(double x, double y, const SVGStyledElement *context)
{
    SVGPathSegLinetoAbs *temp = new SVGPathSegLinetoAbs(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoRel *SVGPathElement::createSVGPathSegLinetoRel(double x, double y, const SVGStyledElement *context)
{
    SVGPathSegLinetoRel *temp = new SVGPathSegLinetoRel(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoCubicAbs *SVGPathElement::createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElement *context)
{
    SVGPathSegCurvetoCubicAbs *temp = new SVGPathSegCurvetoCubicAbs(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoCubicRel *SVGPathElement::createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElement *context)
{
    SVGPathSegCurvetoCubicRel *temp = new SVGPathSegCurvetoCubicRel(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoQuadraticAbs *SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1, const SVGStyledElement *context)
{
    SVGPathSegCurvetoQuadraticAbs *temp = new SVGPathSegCurvetoQuadraticAbs(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    return temp;
}

SVGPathSegCurvetoQuadraticRel *SVGPathElement::createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1, const SVGStyledElement *context)
{
    SVGPathSegCurvetoQuadraticRel *temp = new SVGPathSegCurvetoQuadraticRel(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    return temp;
}

SVGPathSegArcAbs *SVGPathElement::createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElement *context)
{
    SVGPathSegArcAbs *temp = new SVGPathSegArcAbs(context);
    temp->setX(x);
    temp->setY(y);
    temp->setR1(r1);
    temp->setR2(r2);
    temp->setAngle(angle);
    temp->setLargeArcFlag(largeArcFlag);
    temp->setSweepFlag(sweepFlag);
    return temp;
}

SVGPathSegArcRel *SVGPathElement::createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElement *context)
{
    SVGPathSegArcRel *temp = new SVGPathSegArcRel(context);
    temp->setX(x);
    temp->setY(y);
    temp->setR1(r1);
    temp->setR2(r2);
    temp->setAngle(angle);
    temp->setLargeArcFlag(largeArcFlag);
    temp->setSweepFlag(sweepFlag);
    return temp;
}

SVGPathSegLinetoHorizontalAbs *SVGPathElement::createSVGPathSegLinetoHorizontalAbs(double x, const SVGStyledElement *context)
{
    SVGPathSegLinetoHorizontalAbs *temp = new SVGPathSegLinetoHorizontalAbs(context);
    temp->setX(x);
    return temp;
}

SVGPathSegLinetoHorizontalRel *SVGPathElement::createSVGPathSegLinetoHorizontalRel(double x, const SVGStyledElement *context)
{
    SVGPathSegLinetoHorizontalRel *temp = new SVGPathSegLinetoHorizontalRel(context);
    temp->setX(x);
    return temp;
}

SVGPathSegLinetoVerticalAbs *SVGPathElement::createSVGPathSegLinetoVerticalAbs(double y, const SVGStyledElement *context)
{
    SVGPathSegLinetoVerticalAbs *temp = new SVGPathSegLinetoVerticalAbs(context);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoVerticalRel *SVGPathElement::createSVGPathSegLinetoVerticalRel(double y, const SVGStyledElement *context)
{
    SVGPathSegLinetoVerticalRel *temp = new SVGPathSegLinetoVerticalRel(context);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoCubicSmoothAbs *SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2, const SVGStyledElement *context)
{
    SVGPathSegCurvetoCubicSmoothAbs *temp = new SVGPathSegCurvetoCubicSmoothAbs(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoCubicSmoothRel *SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2, const SVGStyledElement *context)
{
    SVGPathSegCurvetoCubicSmoothRel *temp = new SVGPathSegCurvetoCubicSmoothRel(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoQuadraticSmoothAbs *SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y, const SVGStyledElement *context)
{
    SVGPathSegCurvetoQuadraticSmoothAbs *temp = new SVGPathSegCurvetoQuadraticSmoothAbs(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoQuadraticSmoothRel *SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y, const SVGStyledElement *context)
{
    SVGPathSegCurvetoQuadraticSmoothRel *temp = new SVGPathSegCurvetoQuadraticSmoothRel(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

void SVGPathElement::svgMoveTo(double x1, double y1, bool, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegMovetoAbs(x1, y1, this));
    else
        pathSegList()->appendItem(createSVGPathSegMovetoRel(x1, y1, this));
}

void SVGPathElement::svgLineTo(double x1, double y1, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegLinetoAbs(x1, y1, this));
    else
        pathSegList()->appendItem(createSVGPathSegLinetoRel(x1, y1, this));
}

void SVGPathElement::svgLineToHorizontal(double x, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegLinetoHorizontalAbs(x, this));
    else
        pathSegList()->appendItem(createSVGPathSegLinetoHorizontalRel(x, this));
}

void SVGPathElement::svgLineToVertical(double y, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegLinetoVerticalAbs(y, this));
    else
        pathSegList()->appendItem(createSVGPathSegLinetoVerticalRel(y, this));
}

void SVGPathElement::svgCurveToCubic(double x1, double y1, double x2, double y2, double x, double y, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2, this));
}

void SVGPathElement::svgCurveToCubicSmooth(double x, double y, double x2, double y2, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y, this));
}

void SVGPathElement::svgCurveToQuadratic(double x, double y, double x1, double y1, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticRel(x1, y1, x, y, this));
}

void SVGPathElement::svgCurveToQuadraticSmooth(double x, double y, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticSmoothAbs(x, y, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticSmoothRel(x, y, this));
}

void SVGPathElement::svgArcTo(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag, this));
    else
        pathSegList()->appendItem(createSVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag, this));
}

void SVGPathElement::svgClosePath()
{
    pathSegList()->appendItem(createSVGPathSegClosePath());
}

void SVGPathElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == SVGNames::dAttr) {
        pathSegList()->clear();
        parseSVG(attr->value().deprecatedString(), true);
    } else {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

SVGPathSegList *SVGPathElement::pathSegList() const
{
    return lazy_create<SVGPathSegList>(m_pathSegList, this);
}

SVGPathSegList *SVGPathElement::normalizedPathSegList() const
{
    // TODO
    return 0;
}

SVGPathSegList *SVGPathElement::animatedPathSegList() const
{
    // TODO
    return 0;
}

SVGPathSegList *SVGPathElement::animatedNormalizedPathSegList() const
{
    // TODO
    return 0;
}

// FIXME: This probably belongs on SVGPathSegList instead. -- ecs 12/5/05
KCanvasPath* SVGPathElement::toPathData() const
{
    // TODO : also support non-normalized mode, at least in dom structure
    int len = pathSegList()->numberOfItems();
    if(len < 1)
        return 0;

    KCanvasPath* pathData = renderingDevice()->createPath();
    for(int i = 0; i < len; ++i)
    {
        SVGPathSeg *p = pathSegList()->getItem(i);
        switch(p->pathSegType())
        {
            case PATHSEG_MOVETO_ABS:
            {
                SVGPathSegMovetoAbs *moveTo = static_cast<SVGPathSegMovetoAbs *>(p);
                pathData->moveTo(moveTo->x(), moveTo->y());
                break;
            }
            case PATHSEG_LINETO_ABS:
            {
                SVGPathSegLinetoAbs *lineTo = static_cast<SVGPathSegLinetoAbs *>(p);
                pathData->lineTo(lineTo->x(), lineTo->y());
                break;
            }
            case PATHSEG_CURVETO_CUBIC_ABS:
            {
                SVGPathSegCurvetoCubicAbs *curveTo = static_cast<SVGPathSegCurvetoCubicAbs *>(p);
                pathData->curveTo(curveTo->x1(), curveTo->y1(),
                                 curveTo->x2(), curveTo->y2(),
                                 curveTo->x(), curveTo->y());
                break;
            }
            case PATHSEG_CLOSEPATH:
                pathData->closeSubpath();
            default:
                break;
        }
    }

    return pathData;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

