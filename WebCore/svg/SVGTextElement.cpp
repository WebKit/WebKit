/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGTextElement.h"

#include "AffineTransform.h"
#include "Attribute.h"
#include "FloatRect.h"
#include "RenderSVGResource.h"
#include "RenderSVGText.h"
#include "SVGLengthList.h"
#include "SVGRenderStyle.h"
#include "SVGTSpanElement.h"
#include "SVGTransformList.h"

namespace WebCore {

inline SVGTextElement::SVGTextElement(const QualifiedName& tagName, Document* doc)
    : SVGTextPositioningElement(tagName, doc)
    , SVGTransformable()
    , m_transform(SVGTransformList::create(SVGNames::transformAttr))
{
}

PassRefPtr<SVGTextElement> SVGTextElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGTextElement(tagName, document));
}

void SVGTextElement::parseMappedAttribute(Attribute* attr)
{
    if (SVGTransformable::isKnownAttribute(attr->name())) {
        SVGTransformList* localTransforms = transformBaseValue();
        if (!SVGTransformable::parseTransformAttribute(localTransforms, attr->value())) {
            ExceptionCode ec = 0;
            localTransforms->clear(ec);
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

FloatRect SVGTextElement::getBBox(StyleUpdateStrategy styleUpdateStrategy) const
{
    return SVGTransformable::getBBox(this, styleUpdateStrategy);
}

AffineTransform SVGTextElement::getCTM(StyleUpdateStrategy styleUpdateStrategy) const
{
    return SVGLocatable::computeCTM(this, SVGLocatable::NearestViewportScope, styleUpdateStrategy);
}

AffineTransform SVGTextElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy) const
{
    return SVGLocatable::computeCTM(this, SVGLocatable::ScreenScope, styleUpdateStrategy);
}

AffineTransform SVGTextElement::animatedLocalTransform() const
{
    return m_supplementalTransform ? transform()->concatenate().matrix() * *m_supplementalTransform : transform()->concatenate().matrix();
}

AffineTransform* SVGTextElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform.set(new AffineTransform());
    return m_supplementalTransform.get();
}

RenderObject* SVGTextElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGText(this);
}

bool SVGTextElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isTextNode()
#if ENABLE(SVG_FONTS)
        || child->hasTagName(SVGNames::altGlyphTag)
#endif
        || child->hasTagName(SVGNames::tspanTag) || child->hasTagName(SVGNames::trefTag) || child->hasTagName(SVGNames::aTag) || child->hasTagName(SVGNames::textPathTag))
        return true;
    return false;
}

void SVGTextElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGTextPositioningElement::svgAttributeChanged(attrName);

    RenderObject* renderer = this->renderer();
    if (!renderer)
        return;

    if (SVGTransformable::isKnownAttribute(attrName)) {
        renderer->setNeedsTransformUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
    }
}

void SVGTextElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGTextPositioningElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || SVGTransformable::isKnownAttribute(attrName))
        synchronizeTransform();
}

}

#endif // ENABLE(SVG)
