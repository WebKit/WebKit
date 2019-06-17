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
#include "SVGLineElement.h"

#include "RenderSVGResource.h"
#include "SVGLengthValue.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGLineElement);

inline SVGLineElement::SVGLineElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
    , SVGExternalResourcesRequired(this)
{
    ASSERT(hasTagName(SVGNames::lineTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::x1Attr, &SVGLineElement::m_x1>();
        PropertyRegistry::registerProperty<SVGNames::y1Attr, &SVGLineElement::m_y1>();
        PropertyRegistry::registerProperty<SVGNames::x2Attr, &SVGLineElement::m_x2>();
        PropertyRegistry::registerProperty<SVGNames::y2Attr, &SVGLineElement::m_y2>();
    });
}

Ref<SVGLineElement> SVGLineElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGLineElement(tagName, document));
}

void SVGLineElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::x1Attr)
        m_x1->setBaseValInternal(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::y1Attr)
        m_y1->setBaseValInternal(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::x2Attr)
        m_x2->setBaseValInternal(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::y2Attr)
        m_y2->setBaseValInternal(SVGLengthValue::construct(LengthModeHeight, value, parseError));

    reportAttributeParsingError(parseError, name, value);

    SVGGeometryElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGLineElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        updateRelativeLengthsInformation();

        if (auto* renderer = downcast<RenderSVGShape>(this->renderer())) {
            renderer->setNeedsShapeUpdate();
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
    SVGExternalResourcesRequired::svgAttributeChanged(attrName);
}

bool SVGLineElement::selfHasRelativeLengths() const
{
    return x1().isRelative()
        || y1().isRelative()
        || x2().isRelative()
        || y2().isRelative();
}

}
