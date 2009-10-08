/*
    Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>

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
#include "SVGClipPathElement.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "MappedAttribute.h"
#include "SVGNames.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGClipPathElement::SVGClipPathElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_clipPathUnits(this, SVGNames::clipPathUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
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

void SVGClipPathElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledTransformableElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!m_clipper)
        return;

    m_clipper->invalidate();
}

SVGResource* SVGClipPathElement::canvasResource()
{
    if (!m_clipper)
        m_clipper = SVGResourceClipper::create();
    else
        m_clipper->resetClipData();

    bool bbox = clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    RefPtr<RenderStyle> clipPathStyle = styleForRenderer(); // FIXME: Manual style resolution is a hack
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        if (n->isSVGElement() && static_cast<SVGElement*>(n)->isStyledTransformable()) {
            SVGStyledTransformableElement* styled = static_cast<SVGStyledTransformableElement*>(n);
            RefPtr<RenderStyle> pathStyle = document()->styleSelector()->styleForElement(styled, clipPathStyle.get());
            if (pathStyle->display() != NONE) {
                Path pathData = styled->toClipPath();
                if (!pathData.isEmpty())
                    m_clipper->addClipData(pathData, pathStyle->svgStyle()->clipRule(), bbox);
            }
        }
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
