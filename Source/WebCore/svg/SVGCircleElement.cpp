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
#include "SVGParsingError.h"
#include "SVGPropertyOwnerRegistry.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGCircleElement);

inline SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::circleTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::cxAttr, &SVGCircleElement::m_cx>();
        PropertyRegistry::registerProperty<SVGNames::cyAttr, &SVGCircleElement::m_cy>();
        PropertyRegistry::registerProperty<SVGNames::rAttr, &SVGCircleElement::m_r>();
    }
}

Ref<SVGCircleElement> SVGCircleElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGCircleElement(tagName, document));
}

SVGAnimatedProperty* SVGCircleElement::propertyForAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::cxAttr)
        return m_cx.ptr();
    if (name == SVGNames::cyAttr)
        return m_cy.ptr();
    if (name == SVGNames::rAttr)
        return m_r.ptr();
    return nullptr;
}

void SVGCircleElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    auto parseError = SVGParsingError::None;

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
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGCircleElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGEllipse>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGEllipse>(*this, WTFMove(style));
}

}
