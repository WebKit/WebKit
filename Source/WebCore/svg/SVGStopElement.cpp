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

#if ENABLE(SVG)
#include "SVGStopElement.h"

#include "Attribute.h"
#include "Document.h"
#include "RenderSVGGradientStop.h"
#include "RenderSVGResource.h"
#include "SVGElementInstance.h"
#include "SVGGradientElement.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_NUMBER(SVGStopElement, SVGNames::offsetAttr, Offset, offset)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGStopElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(offset)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGStyledElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGStopElement::SVGStopElement(const QualifiedName& tagName, Document* document)
    : SVGStyledElement(tagName, document)
    , m_offset(0)
{
    ASSERT(hasTagName(SVGNames::stopTag));
    registerAnimatedPropertiesForSVGStopElement();
}

PassRefPtr<SVGStopElement> SVGStopElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGStopElement(tagName, document));
}

bool SVGStopElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        supportedAttributes.add(SVGNames::offsetAttr);
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGStopElement::parseAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGStyledElement::parseAttribute(attr);
        return;
    }

    if (attr->name() == SVGNames::offsetAttr) {
        const String& value = attr->value();
        if (value.endsWith('%'))
            setOffsetBaseValue(value.left(value.length() - 1).toFloat() / 100.0f);
        else
            setOffsetBaseValue(value.toFloat());
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGStopElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGStyledElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (!renderer())
        return;

    if (attrName == SVGNames::offsetAttr) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer());
        return;
    }

    ASSERT_NOT_REACHED();
}

RenderObject* SVGStopElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGGradientStop(this);
}

bool SVGStopElement::rendererIsNeeded(const NodeRenderingContext&)
{
    return true;
}

Color SVGStopElement::stopColorIncludingOpacity() const
{
    ASSERT(renderer());
    ASSERT(renderer()->style());

    const SVGRenderStyle* svgStyle = renderer()->style()->svgStyle();
    return colorWithOverrideAlpha(svgStyle->stopColor().rgb(), svgStyle->stopOpacity());
}

}

#endif // ENABLE(SVG)
