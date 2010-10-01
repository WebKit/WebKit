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

#include "AffineTransform.h"
#include "Attr.h"
#include "MappedAttribute.h"
#include "RenderPath.h"
#include "SVGDocument.h"
#include "SVGStyledElement.h"
#include "SVGTransformList.h"

namespace WebCore {

SVGStyledTransformableElement::SVGStyledTransformableElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGTransformable()
    , m_transform(SVGTransformList::create(SVGNames::transformAttr))
{
}

SVGStyledTransformableElement::~SVGStyledTransformableElement()
{
}

AffineTransform SVGStyledTransformableElement::getCTM(StyleUpdateStrategy styleUpdateStrategy) const
{
    return SVGLocatable::computeCTM(this, SVGLocatable::NearestViewportScope, styleUpdateStrategy);
}

AffineTransform SVGStyledTransformableElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy) const
{
    return SVGLocatable::computeCTM(this, SVGLocatable::ScreenScope, styleUpdateStrategy);
}

AffineTransform SVGStyledTransformableElement::animatedLocalTransform() const
{
    return m_supplementalTransform ? transform()->concatenate().matrix() * *m_supplementalTransform : transform()->concatenate().matrix();
}
    
AffineTransform* SVGStyledTransformableElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform.set(new AffineTransform());
    return m_supplementalTransform.get();
}

void SVGStyledTransformableElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (SVGTransformable::isKnownAttribute(attr->name())) {
        SVGTransformList* localTransforms = transformBaseValue();
        if (!SVGTransformable::parseTransformAttribute(localTransforms, attr->value())) {
            ExceptionCode ec = 0;
            localTransforms->clear(ec);
        }
    } else 
        SVGStyledLocatableElement::parseMappedAttribute(attr);
}

void SVGStyledTransformableElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledLocatableElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || SVGTransformable::isKnownAttribute(attrName))
        synchronizeTransform();
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

FloatRect SVGStyledTransformableElement::getBBox(StyleUpdateStrategy styleUpdateStrategy) const
{
    return SVGTransformable::getBBox(this, styleUpdateStrategy);
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
