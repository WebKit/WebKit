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

#include "LegacyRenderSVGRect.h"
#include "LegacyRenderSVGResource.h"
#include "NodeName.h"
#include "RenderSVGRect.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "SVGParsingError.h"
#include "SVGPropertyOwnerRegistry.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGRectElement);

inline SVGRectElement::SVGRectElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::rectTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGRectElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGRectElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGRectElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGRectElement::m_height>();
        PropertyRegistry::registerProperty<SVGNames::rxAttr, &SVGRectElement::m_rx>();
        PropertyRegistry::registerProperty<SVGNames::ryAttr, &SVGRectElement::m_ry>();
    }
}

Ref<SVGRectElement> SVGRectElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGRectElement(tagName, document));
}

SVGAnimatedProperty* SVGRectElement::propertyForAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::xAttr)
        return m_x.ptr();
    if (name == SVGNames::yAttr)
        return m_y.ptr();
    if (name == SVGNames::widthAttr)
        return m_width.ptr();
    if (name == SVGNames::heightAttr)
        return m_height.ptr();
    if (name == SVGNames::rxAttr)
        return m_rx.ptr();
    if (name == SVGNames::ryAttr)
        return m_ry.ptr();
    return nullptr;
}

void SVGRectElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    auto parseError = SVGParsingError::None;

    switch (name.nodeName()) {
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::rxAttr:
        Ref { m_rx }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::ryAttr:
        Ref { m_ry }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::widthAttr:
        Ref { m_width }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::heightAttr:
        Ref { m_height }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);
    SVGGeometryElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGRectElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setPresentationalHintStyleIsDirty();
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGRectElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGRect>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGRect>(*this, WTFMove(style));
}

}
