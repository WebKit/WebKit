/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGAnimatedTransformList.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGTests.h"
#include "SVGURIReference.h"
#include "SVGUnitTypes.h"

namespace WebCore {

struct PatternAttributes;
 
class SVGPatternElement final : public SVGElement, public SVGExternalResourcesRequired, public SVGFitToViewBox, public SVGTests, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGPatternElement);
public:
    static Ref<SVGPatternElement> create(const QualifiedName&, Document&);

    void collectPatternAttributes(PatternAttributes&) const;

    AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const final;

    const SVGLengthValue& x() const { return m_x.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y() const { return m_y.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& width() const { return m_width.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& height() const { return m_height.currentValue(attributeOwnerProxy()); }
    SVGUnitTypes::SVGUnitType patternUnits() const { return m_patternUnits.currentValue(attributeOwnerProxy()); }
    SVGUnitTypes::SVGUnitType patternContentUnits() const { return m_patternContentUnits.currentValue(attributeOwnerProxy()); }
    const SVGTransformListValues& patternTransform() const { return m_patternTransform.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> widthAnimated() { return m_width.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> heightAnimated() { return m_height.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> patternUnitsAnimated() { return m_patternUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> patternContentUnitsAnimated() { return m_patternContentUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedTransformList> patternTransformAnimated() { return m_patternTransform.animatedProperty(attributeOwnerProxy()); }

private:
    SVGPatternElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGPatternElement, SVGElement, SVGExternalResourcesRequired, SVGFitToViewBox, SVGTests, SVGURIReference>;
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
    SVGAnimatedLengthAttribute m_x { LengthModeWidth };
    SVGAnimatedLengthAttribute m_y { LengthModeHeight };
    SVGAnimatedLengthAttribute m_width { LengthModeWidth };
    SVGAnimatedLengthAttribute m_height { LengthModeHeight };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_patternUnits { SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_patternContentUnits { SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE };
    SVGAnimatedTransformListAttribute m_patternTransform;
};

} // namespace WebCore
