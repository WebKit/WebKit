/*
    Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGTextElement.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderSVGText.h"
#include "SVGLengthList.h"
#include "SVGRenderStyle.h"
#include "SVGTSpanElement.h"
#include "SVGTransformList.h"

namespace WebCore {

SVGTextElement::SVGTextElement(const QualifiedName& tagName, Document* doc)
    : SVGTextPositioningElement(tagName, doc)
    , SVGTransformable()
    , m_transform(new SVGTransformList(SVGNames::transformAttr))
{
}

SVGTextElement::~SVGTextElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGTextElement, SVGTransformList*, TransformList, transformList, Transform, transform, SVGNames::transformAttr, m_transform.get())

void SVGTextElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::transformAttr) {
        SVGTransformList* localTransforms = transformBaseValue();

        ExceptionCode ec = 0;
        localTransforms->clear(ec);

        if (!SVGTransformable::parseTransformAttribute(localTransforms, attr->value()))
            localTransforms->clear(ec);
        else {
            setTransformBaseValue(localTransforms);
            if (renderer())
                renderer()->setNeedsLayout(true); // should be in setTransformBaseValue
        }
    } else
        SVGTextPositioningElement::parseMappedAttribute(attr);
}

SVGElement* SVGTextElement::nearestViewportElement() const
{
    return SVGTransformable::nearestViewportElement(this);
}

SVGElement* SVGTextElement::farthestViewportElement() const
{
    return SVGTransformable::farthestViewportElement(this);
}

FloatRect SVGTextElement::getBBox() const
{
    return SVGTransformable::getBBox(this);
}

AffineTransform SVGTextElement::getScreenCTM() const
{
    return SVGTransformable::getScreenCTM(this);
}

AffineTransform SVGTextElement::getCTM() const
{
    return SVGTransformable::getCTM(this);
}

AffineTransform SVGTextElement::animatedLocalTransform() const
{
    return transform()->concatenate().matrix();
}

RenderObject* SVGTextElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderSVGText(this);
}

bool SVGTextElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isTextNode() || child->hasTagName(SVGNames::tspanTag) ||
        child->hasTagName(SVGNames::trefTag) || child->hasTagName(SVGNames::aTag) || child->hasTagName(SVGNames::textPathTag))
        return true;
    return false;
}

}

#endif // ENABLE(SVG)
