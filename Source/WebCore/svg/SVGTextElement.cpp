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

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGTextElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(transform)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTextPositioningElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGTextElement::SVGTextElement(const QualifiedName& tagName, Document* doc)
    : SVGTextPositioningElement(tagName, doc)
{
    ASSERT(hasTagName(SVGNames::textTag));
    registerAnimatedPropertiesForSVGTextElement();
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
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGTextElement::parseAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGTextPositioningElement::parseAttribute(attr);
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

FloatRect SVGTextElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    return SVGTransformable::getBBox(this, styleUpdateStrategy);
}

AffineTransform SVGTextElement::getCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return SVGLocatable::computeCTM(this, SVGLocatable::NearestViewportScope, styleUpdateStrategy);
}

AffineTransform SVGTextElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return SVGLocatable::computeCTM(this, SVGLocatable::ScreenScope, styleUpdateStrategy);
}

AffineTransform SVGTextElement::animatedLocalTransform() const
{
    AffineTransform matrix;
    RenderStyle* style = renderer()->style();

    // if CSS property was set, use that, otherwise fallback to attribute (if set)
    if (style->hasTransform()) {
        TransformationMatrix t;
        // For now, the transform-origin is not taken into account
        // Also, any percentage values will not be taken into account
        style->applyTransform(t, IntSize(0, 0), RenderStyle::ExcludeTransformOrigin);
        // Flatten any 3D transform
        matrix = t.toAffineTransform();
    } else
        transform().concatenate(matrix);

    if (m_supplementalTransform)
        return *m_supplementalTransform * matrix;
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

}

#endif // ENABLE(SVG)
