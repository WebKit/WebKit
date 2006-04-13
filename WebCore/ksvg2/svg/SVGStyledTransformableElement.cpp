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

#include "config.h"
#if SVG_SUPPORT
#include "RegularExpression.h"
#include "DeprecatedStringList.h"

#include "Attr.h"

#include <kcanvas/RenderPath.h>

#include "SVGHelper.h"
#include "SVGMatrix.h"
#include "SVGDocument.h"
#include "SVGStyledTransformableElement.h"
#include "SVGStyledElement.h"
#include "SVGDOMImplementation.h"
#include "SVGAnimatedTransformList.h"

using namespace WebCore;

SVGStyledTransformableElement::SVGStyledTransformableElement(const QualifiedName& tagName, Document *doc)
: SVGStyledLocatableElement(tagName, doc), SVGTransformable()
{
}

SVGStyledTransformableElement::~SVGStyledTransformableElement()
{
}

SVGAnimatedTransformList *SVGStyledTransformableElement::transform() const
{
    return lazy_create<SVGAnimatedTransformList>(m_transform, this);
}

SVGMatrix *SVGStyledTransformableElement::localMatrix() const
{
    return lazy_create<SVGMatrix>(m_localMatrix);
}

SVGMatrix *SVGStyledTransformableElement::getCTM() const
{
    SVGMatrix *ctm = SVGLocatable::getCTM(this);

    if(m_localMatrix)
        ctm->multiply(m_localMatrix.get());

    return ctm;
}

SVGMatrix *SVGStyledTransformableElement::getScreenCTM() const
{
    SVGMatrix *ctm = SVGLocatable::getScreenCTM(this);

    if(m_localMatrix)
        ctm->multiply(m_localMatrix.get());

    return ctm;
}

void SVGStyledTransformableElement::updateLocalTransform(SVGTransformList *localTransforms)
{
    // Update cached local matrix
    RefPtr<SVGTransform> localTransform = localTransforms->concatenate();
    if(localTransform) {
        m_localMatrix = localTransform->matrix();
        if (renderer()) {
            renderer()->setLocalTransform(m_localMatrix->qmatrix());
            renderer()->setNeedsLayout(true);
        }
    }
}

void SVGStyledTransformableElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == SVGNames::transformAttr) {
        SVGTransformList *localTransforms = transform()->baseVal();
        localTransforms->clear();
        
        SVGTransformable::parseTransformAttribute(localTransforms, attr->value());
        updateLocalTransform(localTransforms);
    } else
        SVGStyledLocatableElement::parseMappedAttribute(attr);
}

SVGElement *SVGStyledTransformableElement::nearestViewportElement() const
{
    return SVGTransformable::nearestViewportElement(this);
}

SVGElement *SVGStyledTransformableElement::farthestViewportElement() const
{
    return SVGTransformable::farthestViewportElement(this);
}

SVGRect *SVGStyledTransformableElement::getBBox() const
{
    return SVGTransformable::getBBox(this);
}

SVGMatrix *SVGStyledTransformableElement::getTransformToElement(SVGElement *) const
{
    return 0;
}

void SVGStyledTransformableElement::attach()
{
    SVGStyledElement::attach();

    if (renderer() && m_localMatrix)
        renderer()->setLocalTransform(m_localMatrix->qmatrix());
}


// vim:ts=4:noet
#endif // SVG_SUPPORT

