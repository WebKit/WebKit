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

#include "RenderSVGInline.h"
#include "RenderSVGResource.h"
#include "RenderSVGText.h"
#include "SVGAltGlyphElement.h"
#include "SVGNames.h"
#include "SVGTRefElement.h"
#include "SVGTSpanElement.h"
#include "SVGTextElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextPositioningElement);

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document& document)
    : SVGTextContentElement(tagName, document)
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

void SVGTextPositioningElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::xAttr) {
        m_x->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::yAttr) {
        m_y->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::dxAttr) {
        m_dx->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::dyAttr) {
        m_dy->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::rotateAttr) {
        m_rotate->baseVal()->parse(value);
        return;
    }

    SVGTextContentElement::parseAttribute(name, value);
}

void SVGTextPositioningElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == SVGNames::xAttr || name == SVGNames::yAttr)
        return;
    SVGTextContentElement::collectStyleForPresentationAttribute(name, value, style);
}

bool SVGTextPositioningElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::xAttr || name == SVGNames::yAttr)
        return false;
    return SVGTextContentElement::isPresentationAttribute(name);
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
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }

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
