/*
    Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>

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
#include "SVGClipPathElement.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "MappedAttribute.h"
#include "RenderSVGHiddenContainer.h"
#include "SVGNames.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGClipPathElement::SVGClipPathElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_clipPathUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
{
}

SVGClipPathElement::~SVGClipPathElement()
{
}

void SVGClipPathElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::clipPathUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setClipPathUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setClipPathUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
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

void SVGClipPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    if (!m_clipper)
        return;

    if (attrName == SVGNames::clipPathUnitsAttr ||
        SVGTests::isKnownAttribute(attrName) || 
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledTransformableElement::isKnownAttribute(attrName))
        m_clipper->invalidate();
}

void SVGClipPathElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeClipPathUnits();
        synchronizeExternalResourcesRequired();
        return;
    }

    if (attrName == SVGNames::clipPathUnitsAttr)
        synchronizeClipPathUnits();
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
}

void SVGClipPathElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledTransformableElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!m_clipper)
        return;

    m_clipper->invalidate();
}

RenderObject* SVGClipPathElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGHiddenContainer(this);
}

SVGResource* SVGClipPathElement::canvasResource(const RenderObject*)
{
    if (!m_clipper)
        m_clipper = SVGResourceClipper::create();
    else
        m_clipper->resetClipData();

    bool bbox = clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyledTransformable())
            continue;
        SVGStyledTransformableElement* styled = static_cast<SVGStyledTransformableElement*>(node);
        RenderStyle* style = styled->renderer() ? styled->renderer()->style() : 0;
        if (!style || style->display() == NONE)
            continue;
        Path pathData = styled->toClipPath();
        if (pathData.isEmpty())
            continue;
        m_clipper->addClipData(pathData, style->svgStyle()->clipRule(), bbox);
    }
    if (m_clipper->clipData().isEmpty()) {
        Path pathData;
        pathData.addRect(FloatRect());
        m_clipper->addClipData(pathData, RULE_EVENODD, bbox);
    }
    return m_clipper.get();
}

}

#endif // ENABLE(SVG)
