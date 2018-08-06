/*
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
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

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGNames.h"
#include "SVGTests.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class SVGMaskElement final : public SVGElement, public SVGExternalResourcesRequired, public SVGTests {
    WTF_MAKE_ISO_ALLOCATED(SVGMaskElement);
public:
    static Ref<SVGMaskElement> create(const QualifiedName&, Document&);

    const SVGLengthValue& x() const { return m_x.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y() const { return m_y.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& width() const { return m_width.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& height() const { return m_height.currentValue(attributeOwnerProxy()); }
    SVGUnitTypes::SVGUnitType maskUnits() const { return m_maskUnits.currentValue(attributeOwnerProxy()); }
    SVGUnitTypes::SVGUnitType maskContentUnits() const { return m_maskContentUnits.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> widthAnimated() { return m_width.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> heightAnimated() { return m_height.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> maskUnitsAnimated() { return m_maskUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> maskContentUnitsAnimated() { return m_maskContentUnits.animatedProperty(attributeOwnerProxy()); }

private:
    SVGMaskElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGMaskElement, SVGElement, SVGExternalResourcesRequired, SVGTests>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static bool isAnimatedLengthAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isAnimatedLengthAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;
    void childrenChanged(const ChildChange&) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool needsPendingResourceHandling() const final { return false; }
    bool selfHasRelativeLengths() const final { return true; }

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthAttribute m_x { LengthModeWidth, "-10%" };
    SVGAnimatedLengthAttribute m_y { LengthModeHeight, "-10%" };
    SVGAnimatedLengthAttribute m_width { LengthModeWidth, "120%" };
    SVGAnimatedLengthAttribute m_height { LengthModeHeight, "120%" };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_maskUnits { SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_maskContentUnits { SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE };
};

} // namespace WebCore
