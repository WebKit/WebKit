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
#include "SVGGradientElement.h"

#include "CSSStyleSelector.h"
#include "RenderPath.h"
#include "RenderSVGHiddenContainer.h"
#include "SVGNames.h"
#include "SVGPaintServerLinearGradient.h"
#include "SVGPaintServerRadialGradient.h"
#include "SVGStopElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGURIReference()
    , SVGExternalResourcesRequired()
    , m_spreadMethod(0)
    , m_gradientUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_gradientTransform(new SVGTransformList(SVGNames::gradientTransformAttr))
{
}

SVGGradientElement::~SVGGradientElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGGradientElement, int, Enumeration, enumeration, GradientUnits, gradientUnits, SVGNames::gradientUnitsAttr, m_gradientUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGGradientElement, SVGTransformList*, TransformList, transformList, GradientTransform, gradientTransform, SVGNames::gradientTransformAttr, m_gradientTransform.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGGradientElement, int, Enumeration, enumeration, SpreadMethod, spreadMethod, SVGNames::spreadMethodAttr, m_spreadMethod)

void SVGGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::gradientUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setGradientUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setGradientUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::gradientTransformAttr) {
        SVGTransformList* gradientTransforms = gradientTransformBaseValue();
        if (!SVGTransformable::parseTransformAttribute(gradientTransforms, attr->value())) {
            ExceptionCode ec = 0;
            gradientTransforms->clear(ec);
        }
    } else if (attr->name() == SVGNames::spreadMethodAttr) {
        if (attr->value() == "reflect")
            setSpreadMethodBaseValue(SVG_SPREADMETHOD_REFLECT);
        else if (attr->value() == "repeat")
            setSpreadMethodBaseValue(SVG_SPREADMETHOD_REPEAT);
        else if (attr->value() == "pad")
            setSpreadMethodBaseValue(SVG_SPREADMETHOD_PAD);
    } else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (!m_resource)
        return;

    if (attrName == SVGNames::gradientUnitsAttr ||
        attrName == SVGNames::gradientTransformAttr ||
        attrName == SVGNames::spreadMethodAttr ||
        SVGURIReference::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName))
        m_resource->invalidate();
}

void SVGGradientElement::childrenChanged(bool changedByParser)
{
    SVGStyledElement::childrenChanged(changedByParser);

    if (m_resource)
        m_resource->invalidate();
}

RenderObject* SVGGradientElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGHiddenContainer(this);
}

SVGResource* SVGGradientElement::canvasResource()
{
    if (!m_resource) {
        if (gradientType() == LinearGradientPaintServer)
            m_resource = new SVGPaintServerLinearGradient(this);
        else
            m_resource = new SVGPaintServerRadialGradient(this);
    }

    return m_resource.get();
}

Vector<SVGGradientStop> SVGGradientElement::buildStops() const
{
    Vector<SVGGradientStop> stops;
    RenderStyle* gradientStyle = 0;

    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        SVGElement* element = n->isSVGElement() ? static_cast<SVGElement*>(n) : 0;

        if (element && element->isGradientStop()) {
            SVGStopElement* stop = static_cast<SVGStopElement*>(element);
            float stopOffset = stop->offset();

            Color color;
            float opacity;

            if (stop->renderer()) {
                RenderStyle* stopStyle = stop->renderer()->style();
                color = stopStyle->svgStyle()->stopColor();
                opacity = stopStyle->svgStyle()->stopOpacity();
            } else {
                // If there is no renderer for this stop element, then a parent element
                // set display="none" - ie. <g display="none"><linearGradient><stop>..
                // Unfortunately we have to manually rebuild the stop style. See pservers-grad-19-b.svg
                if (!gradientStyle)
                    gradientStyle = const_cast<SVGGradientElement*>(this)->styleForRenderer(parent()->renderer());

                RenderStyle* stopStyle = stop->resolveStyle(gradientStyle);

                color = stopStyle->svgStyle()->stopColor();
                opacity = stopStyle->svgStyle()->stopOpacity();

                stopStyle->deref(document()->renderArena());
            }

            stops.append(makeGradientStop(stopOffset, makeRGBA(color.red(), color.green(), color.blue(), int(opacity * 255.))));
        }
    }

    if (gradientStyle)
        gradientStyle->deref(document()->renderArena());

    return stops;
}

}

#endif // ENABLE(SVG)
