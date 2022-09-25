/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGFilterElement.h"

#include "ElementName.h"
#include "RenderSVGResourceFilter.h"
#include "SVGElementInlines.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFilterElement);

inline SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
    ASSERT(hasTagName(SVGNames::filterTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::filterUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGFilterElement::m_filterUnits>();
        PropertyRegistry::registerProperty<SVGNames::primitiveUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGFilterElement::m_primitiveUnits>();
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGFilterElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGFilterElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGFilterElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGFilterElement::m_height>();
    });
}

Ref<SVGFilterElement> SVGFilterElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFilterElement(tagName, document));
}

void SVGFilterElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::filterUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            m_filterUnits->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
    } else if (name == SVGNames::primitiveUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            m_primitiveUnits->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
    } else if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));

    reportAttributeParsingError(parseError, name, value);

    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGFilterElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isAnimatedLengthAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setPresentationalHintStyleIsDirty();
        return;
    }

    if (PropertyRegistry::isKnownAttribute(attrName) || SVGURIReference::isKnownAttribute(attrName)) {
        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGFilterElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

    updateSVGRendererForElementChange();
}

RenderPtr<RenderElement> SVGFilterElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGResourceFilter>(*this, WTFMove(style));
}

bool SVGFilterElement::childShouldCreateRenderer(const Node& child) const
{
    using namespace ElementNames;

    if (!child.isSVGElement())
        return false;

    switch (downcast<SVGElement>(child).tagQName().elementName()) {
    case SVG::feBlend:
    case SVG::feColorMatrix:
    case SVG::feComponentTransfer:
    case SVG::feComposite:
    case SVG::feConvolveMatrix:
    case SVG::feDiffuseLighting:
    case SVG::feDisplacementMap:
    case SVG::feDistantLight:
    case SVG::feDropShadow:
    case SVG::feFlood:
    case SVG::feFuncA:
    case SVG::feFuncB:
    case SVG::feFuncG:
    case SVG::feFuncR:
    case SVG::feGaussianBlur:
    case SVG::feImage:
    case SVG::feMerge:
    case SVG::feMergeNode:
    case SVG::feMorphology:
    case SVG::feOffset:
    case SVG::fePointLight:
    case SVG::feSpecularLighting:
    case SVG::feSpotLight:
    case SVG::feTile:
    case SVG::feTurbulence:
        return true;
    default:
        break;
    }

    return false;
}

}
