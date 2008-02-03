/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPathElement.h"

#include "RenderPath.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
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
    , m_pathLength(0.0f)
{
}

SVGPathElement::~SVGPathElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGPathElement, float, Number, number, PathLength, pathLength, SVGNames::pathLengthAttr, m_pathLength)

float SVGPathElement::getTotalLength()
{
    // FIXME: this may wish to use the pathSegList instead of the pathdata if that's cheaper to build (or cached)
    return toPathData().length();
}

FloatPoint SVGPathElement::getPointAtLength(float length)
{
    // FIXME: this may wish to use the pathSegList instead of the pathdata if that's cheaper to build (or cached)
    bool ok = false;
    return toPathData().pointAtLength(length, ok);
}

unsigned long SVGPathElement::getPathSegAtLength(float length)
{
    return pathSegList()->getPathSegAtLength(length);
}

SVGPathSegClosePath* SVGPathElement::createSVGPathSegClosePath()
{
    return new SVGPathSegClosePath();
}

SVGPathSegMovetoAbs* SVGPathElement::createSVGPathSegMovetoAbs(float x, float y)
{
    return new SVGPathSegMovetoAbs(x, y);
}

SVGPathSegMovetoRel* SVGPathElement::createSVGPathSegMovetoRel(float x, float y)
{
    return new SVGPathSegMovetoRel(x, y);
}

SVGPathSegLinetoAbs* SVGPathElement::createSVGPathSegLinetoAbs(float x, float y)
{
    return new SVGPathSegLinetoAbs(x, y);
}

SVGPathSegLinetoRel* SVGPathElement::createSVGPathSegLinetoRel(float x, float y)
{
    return new SVGPathSegLinetoRel(x, y);
}

SVGPathSegCurvetoCubicAbs* SVGPathElement::createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2)
{
    return new SVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2);
}

SVGPathSegCurvetoCubicRel* SVGPathElement::createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2)
{
    return new SVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2);
}

SVGPathSegCurvetoQuadraticAbs* SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1)
{
    return new SVGPathSegCurvetoQuadraticAbs(x, y, x1, y1);
}

SVGPathSegCurvetoQuadraticRel* SVGPathElement::createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1)
{
    return new SVGPathSegCurvetoQuadraticRel(x, y, x1, y1);
}

SVGPathSegArcAbs* SVGPathElement::createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
{
    return new SVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

SVGPathSegArcRel* SVGPathElement::createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
{
    return new SVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

SVGPathSegLinetoHorizontalAbs* SVGPathElement::createSVGPathSegLinetoHorizontalAbs(float x)
{
    return new SVGPathSegLinetoHorizontalAbs(x);
}

SVGPathSegLinetoHorizontalRel* SVGPathElement::createSVGPathSegLinetoHorizontalRel(float x)
{
    return new SVGPathSegLinetoHorizontalRel(x);
}

SVGPathSegLinetoVerticalAbs* SVGPathElement::createSVGPathSegLinetoVerticalAbs(float y)
{
    return new SVGPathSegLinetoVerticalAbs(y);
}

SVGPathSegLinetoVerticalRel* SVGPathElement::createSVGPathSegLinetoVerticalRel(float y)
{
    return new SVGPathSegLinetoVerticalRel(y);
}

SVGPathSegCurvetoCubicSmoothAbs* SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2)
{
    return new SVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2);
}

SVGPathSegCurvetoCubicSmoothRel* SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2)
{
    return new SVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2);
}

SVGPathSegCurvetoQuadraticSmoothAbs* SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
{
    return new SVGPathSegCurvetoQuadraticSmoothAbs(x, y);
}

SVGPathSegCurvetoQuadraticSmoothRel* SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y)
{
    return new SVGPathSegCurvetoQuadraticSmoothRel(x, y);
}

void SVGPathElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::dAttr) {
        ExceptionCode ec;
        pathSegList()->clear(ec);
        if (!pathSegListFromSVGData(pathSegList(), attr->value(), true))
            document()->accessSVGExtensions()->reportError("Problem parsing d=\"" + attr->value() + "\"");
    } else if (attr->name() == SVGNames::pathLengthAttr) {
        m_pathLength = attr->value().toFloat();
        if (m_pathLength < 0.0f)
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

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    if (!renderer())
        return;

    if (attrName == SVGNames::dAttr || attrName == SVGNames::pathLengthAttr ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledTransformableElement::isKnownAttribute(attrName))
        renderer()->setNeedsLayout(true);
}

SVGPathSegList* SVGPathElement::pathSegList() const
{
    if (!m_pathSegList)
        m_pathSegList = new SVGPathSegList(SVGNames::dAttr);

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
