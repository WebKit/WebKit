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

#include "LegacyRenderSVGEllipse.h"
#include "LegacyRenderSVGResource.h"
#include "NodeName.h"
#include "RenderSVGEllipse.h"
#include "SVGElementInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGCircleElement);

inline SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
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

void SVGCircleElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;

    switch (name.nodeName()) {
    case AttributeNames::cxAttr:
        m_cx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::cyAttr:
        m_cy->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::rAttr:
        m_r->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGGeometryElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGCircleElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setPresentationalHintStyleIsDirty();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGCircleElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGEllipse>(*this, WTFMove(style));
#endif

    return createRenderer<LegacyRenderSVGEllipse>(*this, WTFMove(style));
}

}
