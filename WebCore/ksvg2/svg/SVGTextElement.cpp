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
#include "SVGTextElement.h"

#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedTransformList.h"
#include "SVGMatrix.h"
#include "SVGRenderStyle.h"
#include "SVGTSpanElement.h"
#include "render_style.h"
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/RenderSVGText.h>

namespace WebCore {

SVGTextElement::SVGTextElement(const QualifiedName& tagName, Document *doc)
: SVGTextPositioningElement(tagName, doc), SVGTransformable()
{
}

SVGTextElement::~SVGTextElement()
{
}

SVGAnimatedTransformList *SVGTextElement::transform() const
{
    return lazy_create<SVGAnimatedTransformList>(m_transform, this);
}

SVGMatrix *SVGTextElement::localMatrix() const
{
    return lazy_create<SVGMatrix>(m_localMatrix);
}

void SVGTextElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == SVGNames::transformAttr) {
        SVGTransformList *localTransforms = transform()->baseVal();
        localTransforms->clear();
        
        SVGTransformable::parseTransformAttribute(localTransforms, attr->value());
        updateLocalTransform(localTransforms);
    } else
        SVGTextPositioningElement::parseMappedAttribute(attr);
}

void SVGTextElement::updateLocalTransform(SVGTransformList *localTransforms)
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

void SVGTextElement::attach()
{
    SVGStyledElement::attach();

    if (renderer() && m_localMatrix)
        renderer()->setLocalTransform(m_localMatrix->qmatrix());
}

SVGElement *SVGTextElement::nearestViewportElement() const
{
    return SVGTransformable::nearestViewportElement(this);
}

SVGElement *SVGTextElement::farthestViewportElement() const
{
    return SVGTransformable::farthestViewportElement(this);
}

SVGRect *SVGTextElement::getBBox() const
{
    return SVGTransformable::getBBox(this);
}

SVGMatrix *SVGTextElement::getScreenCTM() const
{
    return SVGLocatable::getScreenCTM(this);
}

SVGMatrix *SVGTextElement::getCTM() const
{
    return SVGLocatable::getScreenCTM(this);
}

RenderObject *SVGTextElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderSVGText(this);
}

bool SVGTextElement::childShouldCreateRenderer(Node *child) const
{
    if (child->isTextNode() || child->hasTagName(SVGNames::tspanTag) ||
        child->hasTagName(SVGNames::trefTag))
        return true;
    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

