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

#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRectImpl.h"
#include "SVGPointImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGPathSegArcImpl.h"
#include "SVGPathSegListImpl.h"
#include "SVGPathElementImpl.h"
#include "SVGPathSegLinetoImpl.h"
#include "SVGPathSegMovetoImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGPathSegClosePathImpl.h"
#include "SVGPathSegCurvetoCubicImpl.h"
#include "SVGPathSegLinetoVerticalImpl.h"
#include "SVGPathSegLinetoHorizontalImpl.h"
#include "SVGPathSegCurvetoQuadraticImpl.h"
#include "SVGPathSegCurvetoCubicSmoothImpl.h"
#include "SVGPathSegCurvetoQuadraticSmoothImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

using namespace KSVG;

SVGPathElementImpl::SVGPathElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl(), SVGPathParser()
{
    m_pathLength = 0;
    m_pathSegList = 0;
}

SVGPathElementImpl::~SVGPathElementImpl()
{
    if(m_pathSegList)
        m_pathSegList->deref();

    if(m_pathLength)
        m_pathLength->deref();
}

SVGAnimatedNumberImpl *SVGPathElementImpl::pathLength() const
{
    if(!m_pathLength)
    {
        lazy_create<SVGAnimatedNumberImpl>(m_pathLength, this);
        m_pathLength->setBaseVal(0);
    }
    
    return m_pathLength;
}

double SVGPathElementImpl::getTotalLength()
{
    return 0;
}

SVGPointImpl *SVGPathElementImpl::getPointAtLength(double /*distance*/)
{
    SVGPointImpl *ret = SVGSVGElementImpl::createSVGPoint();
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

unsigned long SVGPathElementImpl::getPathSegAtLength(double)
{
    return 0;
}

SVGPathSegClosePathImpl *SVGPathElementImpl::createSVGPathSegClosePath()
{
    return new SVGPathSegClosePathImpl();
}

SVGPathSegMovetoAbsImpl *SVGPathElementImpl::createSVGPathSegMovetoAbs(double x, double y, const SVGStyledElementImpl *context)
{
    SVGPathSegMovetoAbsImpl *temp = new SVGPathSegMovetoAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegMovetoRelImpl *SVGPathElementImpl::createSVGPathSegMovetoRel(double x, double y, const SVGStyledElementImpl *context)
{
    SVGPathSegMovetoRelImpl *temp = new SVGPathSegMovetoRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoAbsImpl *SVGPathElementImpl::createSVGPathSegLinetoAbs(double x, double y, const SVGStyledElementImpl *context)
{
    SVGPathSegLinetoAbsImpl *temp = new SVGPathSegLinetoAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoRelImpl *SVGPathElementImpl::createSVGPathSegLinetoRel(double x, double y, const SVGStyledElementImpl *context)
{
    SVGPathSegLinetoRelImpl *temp = new SVGPathSegLinetoRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoCubicAbsImpl *SVGPathElementImpl::createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoCubicAbsImpl *temp = new SVGPathSegCurvetoCubicAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoCubicRelImpl *SVGPathElementImpl::createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoCubicRelImpl *temp = new SVGPathSegCurvetoCubicRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoQuadraticAbsImpl *SVGPathElementImpl::createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoQuadraticAbsImpl *temp = new SVGPathSegCurvetoQuadraticAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    return temp;
}

SVGPathSegCurvetoQuadraticRelImpl *SVGPathElementImpl::createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoQuadraticRelImpl *temp = new SVGPathSegCurvetoQuadraticRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX1(x1);
    temp->setY1(y1);
    return temp;
}

SVGPathSegArcAbsImpl *SVGPathElementImpl::createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElementImpl *context)
{
    SVGPathSegArcAbsImpl *temp = new SVGPathSegArcAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setR1(r1);
    temp->setR2(r2);
    temp->setAngle(angle);
    temp->setLargeArcFlag(largeArcFlag);
    temp->setSweepFlag(sweepFlag);
    return temp;
}

SVGPathSegArcRelImpl *SVGPathElementImpl::createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElementImpl *context)
{
    SVGPathSegArcRelImpl *temp = new SVGPathSegArcRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setR1(r1);
    temp->setR2(r2);
    temp->setAngle(angle);
    temp->setLargeArcFlag(largeArcFlag);
    temp->setSweepFlag(sweepFlag);
    return temp;
}

SVGPathSegLinetoHorizontalAbsImpl *SVGPathElementImpl::createSVGPathSegLinetoHorizontalAbs(double x, const SVGStyledElementImpl *context)
{
    SVGPathSegLinetoHorizontalAbsImpl *temp = new SVGPathSegLinetoHorizontalAbsImpl(context);
    temp->setX(x);
    return temp;
}

SVGPathSegLinetoHorizontalRelImpl *SVGPathElementImpl::createSVGPathSegLinetoHorizontalRel(double x, const SVGStyledElementImpl *context)
{
    SVGPathSegLinetoHorizontalRelImpl *temp = new SVGPathSegLinetoHorizontalRelImpl(context);
    temp->setX(x);
    return temp;
}

SVGPathSegLinetoVerticalAbsImpl *SVGPathElementImpl::createSVGPathSegLinetoVerticalAbs(double y, const SVGStyledElementImpl *context)
{
    SVGPathSegLinetoVerticalAbsImpl *temp = new SVGPathSegLinetoVerticalAbsImpl(context);
    temp->setY(y);
    return temp;
}

SVGPathSegLinetoVerticalRelImpl *SVGPathElementImpl::createSVGPathSegLinetoVerticalRel(double y, const SVGStyledElementImpl *context)
{
    SVGPathSegLinetoVerticalRelImpl *temp = new SVGPathSegLinetoVerticalRelImpl(context);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoCubicSmoothAbsImpl *SVGPathElementImpl::createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoCubicSmoothAbsImpl *temp = new SVGPathSegCurvetoCubicSmoothAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoCubicSmoothRelImpl *SVGPathElementImpl::createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoCubicSmoothRelImpl *temp = new SVGPathSegCurvetoCubicSmoothRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    temp->setX2(x2);
    temp->setY2(y2);
    return temp;
}

SVGPathSegCurvetoQuadraticSmoothAbsImpl *SVGPathElementImpl::createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoQuadraticSmoothAbsImpl *temp = new SVGPathSegCurvetoQuadraticSmoothAbsImpl(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

SVGPathSegCurvetoQuadraticSmoothRelImpl *SVGPathElementImpl::createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y, const SVGStyledElementImpl *context)
{
    SVGPathSegCurvetoQuadraticSmoothRelImpl *temp = new SVGPathSegCurvetoQuadraticSmoothRelImpl(context);
    temp->setX(x);
    temp->setY(y);
    return temp;
}

void SVGPathElementImpl::svgMoveTo(double x1, double y1, bool, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegMovetoAbs(x1, y1, this));
    else
        pathSegList()->appendItem(createSVGPathSegMovetoRel(x1, y1, this));
}

void SVGPathElementImpl::svgLineTo(double x1, double y1, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegLinetoAbs(x1, y1, this));
    else
        pathSegList()->appendItem(createSVGPathSegLinetoRel(x1, y1, this));
}

void SVGPathElementImpl::svgLineToHorizontal(double x, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegLinetoHorizontalAbs(x, this));
    else
        pathSegList()->appendItem(createSVGPathSegLinetoHorizontalRel(x, this));
}

void SVGPathElementImpl::svgLineToVertical(double y, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegLinetoVerticalAbs(y, this));
    else
        pathSegList()->appendItem(createSVGPathSegLinetoVerticalRel(y, this));
}

void SVGPathElementImpl::svgCurveToCubic(double x1, double y1, double x2, double y2, double x, double y, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2, this));
}

void SVGPathElementImpl::svgCurveToCubicSmooth(double x, double y, double x2, double y2, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y, this));
}

void SVGPathElementImpl::svgCurveToQuadratic(double x, double y, double x1, double y1, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticRel(x1, y1, x, y, this));
}

void SVGPathElementImpl::svgCurveToQuadraticSmooth(double x, double y, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticSmoothAbs(x, y, this));
    else
        pathSegList()->appendItem(createSVGPathSegCurvetoQuadraticSmoothRel(x, y, this));
}

void SVGPathElementImpl::svgArcTo(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, bool abs)
{
    if(abs)
        pathSegList()->appendItem(createSVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag, this));
    else
        pathSegList()->appendItem(createSVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag, this));
}

void SVGPathElementImpl::svgClosePath()
{
    pathSegList()->appendItem(createSVGPathSegClosePath());
}

void SVGPathElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    switch(id)
    {
        case ATTR_D:
        {
            parseSVG(KDOM::DOMString(attr->value()).string(), true);
            break;
        }
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGTransformableImpl::parseAttribute(attr)) return;

            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
}

SVGPathSegListImpl *SVGPathElementImpl::pathSegList() const
{
    return lazy_create<SVGPathSegListImpl>(m_pathSegList, this);
}

SVGPathSegListImpl *SVGPathElementImpl::normalizedPathSegList() const
{
    // TODO
    return 0;
}

SVGPathSegListImpl *SVGPathElementImpl::animatedPathSegList() const
{
    // TODO
    return 0;
}

SVGPathSegListImpl *SVGPathElementImpl::animatedNormalizedPathSegList() const
{
    // TODO
    return 0;
}

KCPathDataList SVGPathElementImpl::toPathData() const
{
    // TODO : also support non-normalized mode, at least in dom structure
    KCPathDataList pathData;
    int len = pathSegList()->numberOfItems();
    if(len < 1)
        return pathData;

    for(int i = 0; i < len; ++i)
    {
        SVGPathSegImpl *p = pathSegList()->getItem(i);
        switch(p->pathSegType())
        {
            case PATHSEG_MOVETO_ABS:
            {
                SVGPathSegMovetoAbsImpl *moveTo = static_cast<SVGPathSegMovetoAbsImpl *>(p);
                pathData.moveTo(moveTo->x(), moveTo->y());
                break;
            }
            case PATHSEG_LINETO_ABS:
            {
                SVGPathSegLinetoAbsImpl *lineTo = static_cast<SVGPathSegLinetoAbsImpl *>(p);
                pathData.lineTo(lineTo->x(), lineTo->y());
                break;
            }
            case PATHSEG_CURVETO_CUBIC_ABS:
            {
                SVGPathSegCurvetoCubicAbsImpl *curveTo = static_cast<SVGPathSegCurvetoCubicAbsImpl *>(p);
                pathData.curveTo(curveTo->x1(), curveTo->y1(),
                                 curveTo->x2(), curveTo->y2(),
                                 curveTo->x(), curveTo->y());
                break;
            }
            case PATHSEG_CLOSEPATH:
                pathData.closeSubpath();
            default:
                break;
        }
    }

    return pathData;
}

// vim:ts=4:noet
