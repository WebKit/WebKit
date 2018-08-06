/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedLength.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGURIReference.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class SVGFilterElement final : public SVGElement, public SVGExternalResourcesRequired, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGFilterElement);
public:
    static Ref<SVGFilterElement> create(const QualifiedName&, Document&);

    void setFilterRes(unsigned filterResX, unsigned filterResY);

    SVGUnitTypes::SVGUnitType filterUnits() const { return m_filterUnits.currentValue(attributeOwnerProxy()); }
    SVGUnitTypes::SVGUnitType primitiveUnits() const { return m_primitiveUnits.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& x() const { return m_x.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y() const { return m_y.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& width() const { return m_width.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& height() const { return m_height.currentValue(attributeOwnerProxy()); }
    int filterResX() const { return m_filterResX.currentValue(attributeOwnerProxy()); }
    int filterResY() const { return m_filterResY.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedEnumeration> filterUnitsAnimated() { return m_filterUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> primitiveUnitsAnimated() { return m_primitiveUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> widthAnimated() { return m_width.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> heightAnimated() { return m_height.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> filterResXAnimated() { return m_filterResX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> filterResYAnimated() { return m_filterResY.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFilterElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFilterElement, SVGElement, SVGExternalResourcesRequired, SVGURIReference>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static bool isAnimatedLengthAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isAnimatedLengthAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;
    void childrenChanged(const ChildChange&) final;

    bool needsPendingResourceHandling() const final { return false; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;

    bool selfHasRelativeLengths() const final { return true; }

    static const AtomicString& filterResXIdentifier();
    static const AtomicString& filterResYIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_filterUnits { SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_primitiveUnits { SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE };
    SVGAnimatedLengthAttribute m_x { LengthModeWidth, "-10%" };
    SVGAnimatedLengthAttribute m_y { LengthModeHeight, "-10%" };
    SVGAnimatedLengthAttribute m_width { LengthModeWidth, "120%" };
    SVGAnimatedLengthAttribute m_height { LengthModeHeight, "120%" };
    SVGAnimatedIntegerAttribute m_filterResX;
    SVGAnimatedIntegerAttribute m_filterResY;
};

} // namespace WebCore
