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
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGTSpanElement.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_TRANSFORM_LIST(SVGTextElement, SVGNames::transformAttr, Transform, transform)

inline SVGTextElement::SVGTextElement(const QualifiedName& tagName, Document* doc)
    : SVGTextPositioningElement(tagName, doc)
{
    ASSERT(hasTagName(SVGNames::textTag));
}

PassRefPtr<SVGTextElement> SVGTextElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGTextElement(tagName, document));
}

bool SVGTextElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        supportedAttributes.add(SVGNames::transformAttr);
    return supportedAttributes.contains(attrName);
}

void SVGTextElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGTextPositioningElement::parseMappedAttribute(attr);
        return;
    }

    if (attr->name() == SVGNames::transformAttr) {
        SVGTransformList newList;
        if (!SVGTransformable::parseTransformAttribute(newList, attr->value()))
            newList.clear();

        detachAnimatedTransformListWrappers(newList.size());
        setTransformBaseValue(newList);
        return;
    }

    ASSERT_NOT_REACHED();
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
    AffineTransform matrix;
    transform().concatenate(matrix);
    if (m_supplementalTransform)
        matrix *= *m_supplementalTransform;
    return matrix;
}

AffineTransform* SVGTextElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform = adoptPtr(new AffineTransform);
    return m_supplementalTransform.get();
}

RenderObject* SVGTextElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGText(this);
}

bool SVGTextElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isTextNode()
        || child->hasTagName(SVGNames::aTag)
#if ENABLE(SVG_FONTS)
        || child->hasTagName(SVGNames::altGlyphTag)
#endif
        || child->hasTagName(SVGNames::textPathTag)
        || child->hasTagName(SVGNames::trefTag)
        || child->hasTagName(SVGNames::tspanTag))
        return true;

    return false;
}

void SVGTextElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGTextPositioningElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    RenderObject* renderer = this->renderer();
    if (!renderer)
        return;

    if (attrName == SVGNames::transformAttr) {
        renderer->setNeedsTransformUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGTextElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeTransform();
        SVGTextPositioningElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGTextPositioningElement::synchronizeProperty(attrName);
        return;
    }

    if (attrName == SVGNames::transformAttr) {
        synchronizeTransform();
        return;
    }

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGTextElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGTextElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();

    SVGTextPositioningElement::fillPassedAttributeToPropertyTypeMap(attributeToPropertyTypeMap);
    attributeToPropertyTypeMap.set(SVGNames::transformAttr, AnimatedTransformList);
}

}

#endif // ENABLE(SVG)
