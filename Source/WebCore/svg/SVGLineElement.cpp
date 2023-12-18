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

#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGShape.h"
#include "NodeName.h"
#include "RenderSVGShape.h"
#include "SVGLengthValue.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGLineElement);

inline SVGLineElement::SVGLineElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
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

void SVGLineElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;

    switch (name.nodeName()) {
    case AttributeNames::x1Attr:
        m_x1->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::y1Attr:
        m_y1->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::x2Attr:
        m_x2->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::y2Attr:
        m_y2->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGGeometryElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGLineElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        updateRelativeLengthsInformation();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* shape = dynamicDowncast<RenderSVGShape>(renderer()))
            shape->setNeedsShapeUpdate();
#endif
        if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(renderer()))
            shape->setNeedsShapeUpdate();

        updateSVGRendererForElementChange();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

bool SVGLineElement::selfHasRelativeLengths() const
{
    return x1().isRelative()
        || y1().isRelative()
        || x2().isRelative()
        || y2().isRelative();
}

}
