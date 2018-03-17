/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGStopElement.h"

#include "Document.h"
#include "RenderSVGGradientStop.h"
#include "RenderSVGResource.h"
#include "SVGGradientElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGStopElement);

// Animated property definitions
DEFINE_ANIMATED_NUMBER(SVGStopElement, SVGNames::offsetAttr, Offset, offset)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGStopElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(offset)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGStopElement::SVGStopElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_offset(0)
{
    ASSERT(hasTagName(SVGNames::stopTag));
    registerAnimatedPropertiesForSVGStopElement();
}

Ref<SVGStopElement> SVGStopElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGStopElement(tagName, document));
}

void SVGStopElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::offsetAttr) {
        if (value.endsWith('%'))
            setOffsetBaseValue(value.string().left(value.length() - 1).toFloat() / 100.0f);
        else
            setOffsetBaseValue(value.toFloat());
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGStopElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::offsetAttr) {
        if (auto renderer = this->renderer()) {
            InstanceInvalidationGuard guard(*this);
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGStopElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGGradientStop>(*this, WTFMove(style));
}

bool SVGStopElement::rendererIsNeeded(const RenderStyle&)
{
    return true;
}

Color SVGStopElement::stopColorIncludingOpacity() const
{
    auto* style = renderer() ? &renderer()->style() : nullptr;
    // FIXME: This check for null style exists to address Bug WK 90814, a rare crash condition in
    // which the renderer or style is null. This entire class is scheduled for removal (Bug WK 86941)
    // and we will tolerate this null check until then.
    if (!style)
        return Color(Color::transparent, true); // Transparent black.

    const SVGRenderStyle& svgStyle = style->svgStyle();
    return colorWithOverrideAlpha(svgStyle.stopColor().rgb(), svgStyle.stopOpacity());
}

}
