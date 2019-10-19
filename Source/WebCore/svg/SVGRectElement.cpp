/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGRectElement.h"

#include "RenderSVGRect.h"
#include "RenderSVGResource.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGRectElement);

inline SVGRectElement::SVGRectElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::rectTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGRectElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGRectElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGRectElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGRectElement::m_height>();
        PropertyRegistry::registerProperty<SVGNames::rxAttr, &SVGRectElement::m_rx>();
        PropertyRegistry::registerProperty<SVGNames::ryAttr, &SVGRectElement::m_ry>();
    });
}

Ref<SVGRectElement> SVGRectElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGRectElement(tagName, document));
}

void SVGRectElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::rxAttr)
        m_rx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError, SVGLengthNegativeValuesMode::Forbid));
    else if (name == SVGNames::ryAttr)
        m_ry->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError, SVGLengthNegativeValuesMode::Forbid));
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError, SVGLengthNegativeValuesMode::Forbid));
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError, SVGLengthNegativeValuesMode::Forbid));

    reportAttributeParsingError(parseError, name, value);

    SVGGeometryElement::parseAttribute(name, value);
}

void SVGRectElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGRectElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGRect>(*this, WTFMove(style));
}

}
