/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGStyledTransformableElement.h"

#include "Attr.h"
#include "MappedAttribute.h"
#include "RenderPath.h"
#include "SVGDocument.h"
#include "SVGStyledElement.h"
#include "SVGTransformList.h"
#include "TransformationMatrix.h"

namespace WebCore {

char SVGStyledTransformableElementIdentifier[] = "SVGStyledTransformableElement";

SVGStyledTransformableElement::SVGStyledTransformableElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGTransformable()
    , m_transform(this, SVGNames::transformAttr, SVGTransformList::create(SVGNames::transformAttr))
{
}

SVGStyledTransformableElement::~SVGStyledTransformableElement()
{
}

TransformationMatrix SVGStyledTransformableElement::getCTM() const
{
    return SVGTransformable::getCTM(this);
}

TransformationMatrix SVGStyledTransformableElement::getScreenCTM() const
{
    return SVGTransformable::getScreenCTM(this);
}

TransformationMatrix SVGStyledTransformableElement::animatedLocalTransform() const
{
    return m_supplementalTransform ? transform()->concatenate().matrix() * *m_supplementalTransform : transform()->concatenate().matrix();
}
    
TransformationMatrix* SVGStyledTransformableElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform.set(new TransformationMatrix());
    return m_supplementalTransform.get();
}

void SVGStyledTransformableElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::transformAttr) {
        SVGTransformList* localTransforms = transformBaseValue();

        ExceptionCode ec = 0;
        localTransforms->clear(ec);
 
        if (!SVGTransformable::parseTransformAttribute(localTransforms, attr->value()))
            localTransforms->clear(ec);
        else
            setTransformBaseValue(localTransforms);
    } else
        SVGStyledLocatableElement::parseMappedAttribute(attr);
}

bool SVGStyledTransformableElement::isKnownAttribute(const QualifiedName& attrName)
{
    return SVGTransformable::isKnownAttribute(attrName) ||
           SVGStyledLocatableElement::isKnownAttribute(attrName);
}

SVGElement* SVGStyledTransformableElement::nearestViewportElement() const
{
    return SVGTransformable::nearestViewportElement(this);
}

SVGElement* SVGStyledTransformableElement::farthestViewportElement() const
{
    return SVGTransformable::farthestViewportElement(this);
}

FloatRect SVGStyledTransformableElement::getBBox() const
{
    return SVGTransformable::getBBox(this);
}

RenderObject* SVGStyledTransformableElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    // By default, any subclass is expected to do path-based drawing
    return new (arena) RenderPath(this);
}

Path SVGStyledTransformableElement::toClipPath() const
{
    Path pathData = toPathData();
    // FIXME: How do we know the element has done a layout?
    pathData.transform(animatedLocalTransform());
    return pathData;
}

}

#endif // ENABLE(SVG)
