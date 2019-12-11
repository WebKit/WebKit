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
#include "SVGEllipseElement.h"

#include "RenderSVGEllipse.h"
#include "RenderSVGResource.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGEllipseElement);

inline SVGEllipseElement::SVGEllipseElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::ellipseTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::cxAttr, &SVGEllipseElement::m_cx>();
        PropertyRegistry::registerProperty<SVGNames::cyAttr, &SVGEllipseElement::m_cy>();
        PropertyRegistry::registerProperty<SVGNames::rxAttr, &SVGEllipseElement::m_rx>();
        PropertyRegistry::registerProperty<SVGNames::ryAttr, &SVGEllipseElement::m_ry>();
    });
}    

Ref<SVGEllipseElement> SVGEllipseElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGEllipseElement(tagName, document));
}

void SVGEllipseElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::cxAttr)
        m_cx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::cyAttr)
        m_cy->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::rxAttr)
        m_rx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError, SVGLengthNegativeValuesMode::Forbid));
    else if (name == SVGNames::ryAttr)
        m_ry->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError, SVGLengthNegativeValuesMode::Forbid));

    reportAttributeParsingError(parseError, name, value);

    SVGGeometryElement::parseAttribute(name, value);
}

void SVGEllipseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGEllipseElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGEllipse>(*this, WTFMove(style));
}

}
