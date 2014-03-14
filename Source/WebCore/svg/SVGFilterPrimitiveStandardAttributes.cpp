/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#if ENABLE(FILTERS)
#include "SVGFilterPrimitiveStandardAttributes.h"

#include "Attribute.h"
#include "FilterEffect.h"
#include "RenderSVGResourceFilterPrimitive.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGFilterBuilder.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGUnitTypes.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGFilterPrimitiveStandardAttributes, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGFilterPrimitiveStandardAttributes, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGFilterPrimitiveStandardAttributes, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGFilterPrimitiveStandardAttributes, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_STRING(SVGFilterPrimitiveStandardAttributes, SVGNames::resultAttr, Result, result)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(result)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
END_REGISTER_ANIMATED_PROPERTIES

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_x(LengthModeWidth, "0%")
    , m_y(LengthModeHeight, "0%")
    , m_width(LengthModeWidth, "100%")
    , m_height(LengthModeHeight, "100%")
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "0%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "100%" were specified.
    registerAnimatedPropertiesForSVGFilterPrimitiveStandardAttributes();
}

bool SVGFilterPrimitiveStandardAttributes::isSupportedAttribute(const QualifiedName& attrName)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
        supportedAttributes.add(SVGNames::resultAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFilterPrimitiveStandardAttributes::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGElement::parseAttribute(name, value);
    else if (name == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::resultAttr)
        setResultBaseValue(value);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFilterPrimitiveStandardAttributes::setFilterEffectAttribute(FilterEffect*, const QualifiedName&)
{
    // When all filters support this method, it will be changed to a pure virtual method.
    ASSERT_NOT_REACHED();
    return false;
}

void SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);    
    invalidate();
}

void SVGFilterPrimitiveStandardAttributes::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChangeSourceParser)
        return;
    invalidate();
}

void SVGFilterPrimitiveStandardAttributes::setStandardAttributes(FilterEffect* filterEffect) const
{
    ASSERT(filterEffect);
    if (!filterEffect)
        return;

    if (hasAttribute(SVGNames::xAttr))
        filterEffect->setHasX(true);
    if (hasAttribute(SVGNames::yAttr))
        filterEffect->setHasY(true);
    if (hasAttribute(SVGNames::widthAttr))
        filterEffect->setHasWidth(true);
    if (hasAttribute(SVGNames::heightAttr))
        filterEffect->setHasHeight(true);
}

RenderPtr<RenderElement> SVGFilterPrimitiveStandardAttributes::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSVGResourceFilterPrimitive>(*this, std::move(style));
}

bool SVGFilterPrimitiveStandardAttributes::rendererIsNeeded(const RenderStyle& style)
{
    if (parentNode() && (parentNode()->hasTagName(SVGNames::filterTag)))
        return SVGElement::rendererIsNeeded(style);

    return false;
}

void invalidateFilterPrimitiveParent(SVGElement* element)
{
    if (!element)
        return;

    ContainerNode* parent = element->parentNode();

    if (!parent)
        return;

    RenderElement* renderer = parent->renderer();
    if (!renderer || !renderer->isSVGResourceFilterPrimitive())
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer, false);
}

}

#endif
