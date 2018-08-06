/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGLengthListValues.h"
#include "SVGNames.h"
#include "SVGNumberListValues.h"
#include "SVGTRefElement.h"
#include "SVGTSpanElement.h"
#include "SVGTextElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextPositioningElement);

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document& document)
    : SVGTextContentElement(tagName, document)
{
    registerAttributes();
}

void SVGTextPositioningElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::xAttr, &SVGTextPositioningElement::m_x>();
    registry.registerAttribute<SVGNames::yAttr, &SVGTextPositioningElement::m_y>();
    registry.registerAttribute<SVGNames::dxAttr, &SVGTextPositioningElement::m_dx>();
    registry.registerAttribute<SVGNames::dyAttr, &SVGTextPositioningElement::m_dy>();
    registry.registerAttribute<SVGNames::rotateAttr, &SVGTextPositioningElement::m_rotate>();
}

void SVGTextPositioningElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::xAttr) {
        SVGLengthListValues newList;
        newList.parse(value, LengthModeWidth);
        m_x.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_x.setValue(WTFMove(newList));
        return;
    }

    if (name == SVGNames::yAttr) {
        SVGLengthListValues newList;
        newList.parse(value, LengthModeHeight);
        m_y.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_y.setValue(WTFMove(newList));
        return;
    }

    if (name == SVGNames::dxAttr) {
        SVGLengthListValues newList;
        newList.parse(value, LengthModeWidth);
        m_dx.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_dx.setValue(WTFMove(newList));
        return;
    }

    if (name == SVGNames::dyAttr) {
        SVGLengthListValues newList;
        newList.parse(value, LengthModeHeight);
        m_dy.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_dy.setValue(WTFMove(newList));
        return;
    }

    if (name == SVGNames::rotateAttr) {
        SVGNumberListValues newList;
        newList.parse(value);
        m_rotate.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_rotate.setValue(WTFMove(newList));
        return;
    }

    SVGTextContentElement::parseAttribute(name, value);
}

void SVGTextPositioningElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
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
    if (isKnownAttribute(attrName)) {
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
#if ENABLE(SVG_FONTS)
        && !is<SVGAltGlyphElement>(element)
#endif
        && !is<SVGTRefElement>(element))
        return nullptr;

    // FIXME: This should use downcast<>().
    return &static_cast<SVGTextPositioningElement&>(element);
}

}
