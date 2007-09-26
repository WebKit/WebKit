/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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

#include "SVGStyledTransformableElement.h"

#include "Attr.h"
#include "RegularExpression.h"
#include "RenderPath.h"
#include "SVGDocument.h"
#include "AffineTransform.h"
#include "SVGStyledElement.h"
#include "SVGTransformList.h"

namespace WebCore {

SVGStyledTransformableElement::SVGStyledTransformableElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGTransformable()
    , m_transform(new SVGTransformList)
{
}

SVGStyledTransformableElement::~SVGStyledTransformableElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGStyledTransformableElement, SVGTransformList*, TransformList, transformList, Transform, transform, SVGNames::transformAttr.localName(), m_transform.get())

AffineTransform SVGStyledTransformableElement::localMatrix() const
{
    return m_localMatrix;
}

AffineTransform SVGStyledTransformableElement::getCTM() const
{
    return SVGTransformable::getCTM(this);
}

AffineTransform SVGStyledTransformableElement::getScreenCTM() const
{
    return SVGTransformable::getScreenCTM(this);
}

void SVGStyledTransformableElement::updateLocalTransform(SVGTransformList* localTransforms)
{
    // Update cached local matrix
    SVGTransform localTransform = localTransforms->concatenate();
    if (localTransform.isValid()) {
        m_localMatrix = localTransform.matrix();
        if (renderer()) {
            renderer()->setLocalTransform(m_localMatrix);
            renderer()->setNeedsLayout(true);
        }
    }
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
            updateLocalTransform(localTransforms);
    } else
        SVGStyledLocatableElement::parseMappedAttribute(attr);
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

void SVGStyledTransformableElement::attach()
{
    SVGStyledElement::attach();

    if (renderer() && !m_localMatrix.isIdentity())
        renderer()->setLocalTransform(m_localMatrix);
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

