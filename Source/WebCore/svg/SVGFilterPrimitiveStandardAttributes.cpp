/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "SVGFilterPrimitiveStandardAttributes.h"

#include "FilterEffect.h"
#include "RenderSVGResourceFilterPrimitive.h"
#include "SVGElementInlines.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFilterPrimitiveStandardAttributes);

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry)
    : SVGElement(tagName, document, WTFMove(propertyRegistry))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGFilterPrimitiveStandardAttributes::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGFilterPrimitiveStandardAttributes::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGFilterPrimitiveStandardAttributes::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGFilterPrimitiveStandardAttributes::m_height>();
        PropertyRegistry::registerProperty<SVGNames::resultAttr, &SVGFilterPrimitiveStandardAttributes::m_result>();
    });
}

void SVGFilterPrimitiveStandardAttributes::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::resultAttr)
        m_result->setBaseValInternal(value);

    reportAttributeParsingError(parseError, name, value);

    SVGElement::parseAttribute(name, value);
}

OptionSet<FilterEffectGeometry::Flags> SVGFilterPrimitiveStandardAttributes::effectGeometryFlags() const
{
    OptionSet<FilterEffectGeometry::Flags> flags;

    if (hasAttribute(SVGNames::xAttr))
        flags.add(FilterEffectGeometry::Flags::HasX);
    if (hasAttribute(SVGNames::yAttr))
        flags.add(FilterEffectGeometry::Flags::HasY);
    if (hasAttribute(SVGNames::widthAttr))
        flags.add(FilterEffectGeometry::Flags::HasWidth);
    if (hasAttribute(SVGNames::heightAttr))
        flags.add(FilterEffectGeometry::Flags::HasHeight);

    return flags;
}

RefPtr<FilterEffect> SVGFilterPrimitiveStandardAttributes::filterEffect(const FilterEffectVector& inputs, const GraphicsContext& destinationContext)
{
    if (!m_effect)
        m_effect = createFilterEffect(inputs, destinationContext);
    return m_effect;
}

void SVGFilterPrimitiveStandardAttributes::primitiveAttributeChanged(const QualifiedName& attribute)
{
    if (m_effect && !setFilterEffectAttribute(*m_effect, attribute))
        return;

    if (auto* renderer = this->renderer())
        static_cast<RenderSVGResourceFilterPrimitive*>(renderer)->markFilterEffectForRepaint(m_effect.get());
}

void SVGFilterPrimitiveStandardAttributes::primitiveAttributeOnChildChanged(const Element& child, const QualifiedName& attribute)
{
    ASSERT(child.parentNode() == this);

    if (m_effect && !setFilterEffectAttributeFromChild(*m_effect, child, attribute))
        return;

    if (auto* renderer = this->renderer())
        static_cast<RenderSVGResourceFilterPrimitive*>(renderer)->markFilterEffectForRepaint(m_effect.get());
}

void SVGFilterPrimitiveStandardAttributes::markFilterEffectForRebuild()
{
    if (auto* renderer = this->renderer())
        static_cast<RenderSVGResourceFilterPrimitive*>(renderer)->markFilterEffectForRebuild();

    m_effect = nullptr;
}

void SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGFilterPrimitiveStandardAttributes::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;
    updateSVGRendererForElementChange();
}

RenderPtr<RenderElement> SVGFilterPrimitiveStandardAttributes::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGResourceFilterPrimitive>(*this, WTFMove(style));
}

bool SVGFilterPrimitiveStandardAttributes::rendererIsNeeded(const RenderStyle& style)
{
    if (parentNode() && (parentNode()->hasTagName(SVGNames::filterTag)))
        return SVGElement::rendererIsNeeded(style);

    return false;
}

void SVGFilterPrimitiveStandardAttributes::invalidateFilterPrimitiveParent(SVGElement* element)
{
    if (!element)
        return;

    RefPtr parent = element->parentNode();
    if (!parent)
        return;

    RenderElement* renderer = parent->renderer();
    if (!renderer || !renderer->isSVGResourceFilterPrimitive())
        return;

    downcast<SVGElement>(*parent).updateSVGRendererForElementChange();
}

}
