/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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
#include "SVGTextPositioningElement.h"

#include "LegacyRenderSVGResource.h"
#include "NodeName.h"
#include "RenderSVGInline.h"
#include "RenderSVGText.h"
#include "SVGAltGlyphElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGTRefElement.h"
#include "SVGTSpanElement.h"
#include "SVGTextElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextPositioningElement);

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry)
    : SVGTextContentElement(tagName, document, WTFMove(propertyRegistry))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGTextPositioningElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGTextPositioningElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::dxAttr, &SVGTextPositioningElement::m_dx>();
        PropertyRegistry::registerProperty<SVGNames::dyAttr, &SVGTextPositioningElement::m_dy>();
        PropertyRegistry::registerProperty<SVGNames::rotateAttr, &SVGTextPositioningElement::m_rotate>();
    });
}

void SVGTextPositioningElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::xAttr:
        m_x->baseVal()->parse(newValue);
        break;
    case AttributeNames::yAttr:
        m_y->baseVal()->parse(newValue);
        break;
    case AttributeNames::dxAttr:
        m_dx->baseVal()->parse(newValue);
        break;
    case AttributeNames::dyAttr:
        m_dy->baseVal()->parse(newValue);
        break;
    case AttributeNames::rotateAttr:
        m_rotate->baseVal()->parse(newValue);
        break;
    default:
        break;
    }

    SVGTextContentElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGTextPositioningElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == SVGNames::xAttr || name == SVGNames::yAttr)
        return;
    SVGTextContentElement::collectPresentationalHintsForAttribute(name, value, style);
}

bool SVGTextPositioningElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::xAttr || name == SVGNames::yAttr)
        return false;
    return SVGTextContentElement::hasPresentationalHintsForAttribute(name);
}

void SVGTextPositioningElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);

        if (attrName != SVGNames::rotateAttr)
            updateRelativeLengthsInformation();

        if (auto renderer = this->renderer()) {
            if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*renderer))
                textAncestor->setNeedsPositioningValuesUpdate();
        }
        updateSVGRendererForElementChange();
        return;
    }

    SVGTextContentElement::svgAttributeChanged(attrName);
}

SVGTextPositioningElement* SVGTextPositioningElement::elementFromRenderer(RenderBoxModelObject& renderer)
{
    if (!is<RenderSVGText>(renderer) && !is<RenderSVGInline>(renderer))
        return nullptr;

    ASSERT(renderer.element());
    SVGElement& element = downcast<SVGElement>(*renderer.element());

    if (!is<SVGTextElement>(element)
        && !is<SVGTSpanElement>(element)
        && !is<SVGAltGlyphElement>(element)
        && !is<SVGTRefElement>(element))
        return nullptr;

    // FIXME: This should use downcast<>().
    return &static_cast<SVGTextPositioningElement&>(element);
}

}
