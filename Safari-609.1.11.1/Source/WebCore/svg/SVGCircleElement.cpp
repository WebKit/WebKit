/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGCircleElement.h"

#include "RenderSVGEllipse.h"
#include "RenderSVGResource.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGCircleElement);

inline SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::circleTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::cxAttr, &SVGCircleElement::m_cx>();
        PropertyRegistry::registerProperty<SVGNames::cyAttr, &SVGCircleElement::m_cy>();
        PropertyRegistry::registerProperty<SVGNames::rAttr, &SVGCircleElement::m_r>();
    });
}

Ref<SVGCircleElement> SVGCircleElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGCircleElement(tagName, document));
}

void SVGCircleElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::cxAttr)
        m_cx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::cyAttr)
        m_cy->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::rAttr)
        m_r->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, value, parseError, SVGLengthNegativeValuesMode::Forbid));

    reportAttributeParsingError(parseError, name, value);

    SVGGeometryElement::parseAttribute(name, value);
}

void SVGCircleElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGCircleElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGEllipse>(*this, WTFMove(style));
}

}
