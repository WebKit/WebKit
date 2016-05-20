/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGLengthList.h"
#include "SVGNames.h"
#include "SVGNumberList.h"
#include "SVGTRefElement.h"
#include "SVGTSpanElement.h"
#include "SVGTextElement.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH_LIST(SVGTextPositioningElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH_LIST(SVGTextPositioningElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH_LIST(SVGTextPositioningElement, SVGNames::dxAttr, Dx, dx)
DEFINE_ANIMATED_LENGTH_LIST(SVGTextPositioningElement, SVGNames::dyAttr, Dy, dy)
DEFINE_ANIMATED_NUMBER_LIST(SVGTextPositioningElement, SVGNames::rotateAttr, Rotate, rotate)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGTextPositioningElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(dx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(dy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(rotate)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTextContentElement)
END_REGISTER_ANIMATED_PROPERTIES

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document& document)
    : SVGTextContentElement(tagName, document)
{
    registerAnimatedPropertiesForSVGTextPositioningElement();
}

void SVGTextPositioningElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::xAttr) {
        SVGLengthList newList;
        newList.parse(value, LengthModeWidth);
        detachAnimatedXListWrappers(newList.size());
        setXBaseValue(newList);
        return;
    }

    if (name == SVGNames::yAttr) {
        SVGLengthList newList;
        newList.parse(value, LengthModeHeight);
        detachAnimatedYListWrappers(newList.size());
        setYBaseValue(newList);
        return;
    }

    if (name == SVGNames::dxAttr) {
        SVGLengthList newList;
        newList.parse(value, LengthModeWidth);
        detachAnimatedDxListWrappers(newList.size());
        setDxBaseValue(newList);
        return;
    }

    if (name == SVGNames::dyAttr) {
        SVGLengthList newList;
        newList.parse(value, LengthModeHeight);
        detachAnimatedDyListWrappers(newList.size());
        setDyBaseValue(newList);
        return;
    }

    if (name == SVGNames::rotateAttr) {
        SVGNumberList newList;
        newList.parse(value);
        detachAnimatedRotateListWrappers(newList.size());
        setRotateBaseValue(newList);
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
    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr || attrName == SVGNames::dxAttr || attrName == SVGNames::dyAttr || attrName == SVGNames::rotateAttr) {
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
