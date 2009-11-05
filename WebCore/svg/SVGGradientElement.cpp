/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
#include "MappedAttribute.h"
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

char SVGGradientElementIdentifier[] = "SVGGradientElement";

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGURIReference()
    , SVGExternalResourcesRequired()
    , m_spreadMethod(this, SVGNames::spreadMethodAttr)
    , m_gradientUnits(this, SVGNames::gradientUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_gradientTransform(this, SVGNames::gradientTransformAttr, SVGTransformList::create(SVGNames::gradientTransformAttr))
    , m_href(this, XLinkNames::hrefAttr)
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
{
}

SVGGradientElement::~SVGGradientElement()
{
}

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
            setSpreadMethodBaseValue(SpreadMethodReflect);
        else if (attr->value() == "repeat")
            setSpreadMethodBaseValue(SpreadMethodRepeat);
        else if (attr->value() == "pad")
            setSpreadMethodBaseValue(SpreadMethodPad);
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

void SVGGradientElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

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
            m_resource = SVGPaintServerLinearGradient::create(this);
        else
            m_resource = SVGPaintServerRadialGradient::create(this);
    }

    return m_resource.get();
}

Vector<SVGGradientStop> SVGGradientElement::buildStops() const
{
    Vector<SVGGradientStop> stops;
    RefPtr<RenderStyle> gradientStyle;

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
                    gradientStyle = const_cast<SVGGradientElement*>(this)->styleForRenderer();

                RefPtr<RenderStyle> stopStyle = stop->resolveStyle(gradientStyle.get());

                color = stopStyle->svgStyle()->stopColor();
                opacity = stopStyle->svgStyle()->stopOpacity();
            }

            stops.append(makeGradientStop(stopOffset, makeRGBA(color.red(), color.green(), color.blue(), int(opacity * 255.))));
        }
    }

    return stops;
}

}

#endif // ENABLE(SVG)
