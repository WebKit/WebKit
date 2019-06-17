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

#pragma once

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

    const SVGLengthValue& x() const { return m_x->currentValue(); }
    const SVGLengthValue& y() const { return m_y->currentValue(); }
    const SVGLengthValue& width() const { return m_width->currentValue(); }
    const SVGLengthValue& height() const { return m_height->currentValue(); }
    SVGUnitTypes::SVGUnitType patternUnits() const { return m_patternUnits->currentValue<SVGUnitTypes::SVGUnitType>(); }
    SVGUnitTypes::SVGUnitType patternContentUnits() const { return m_patternContentUnits->currentValue<SVGUnitTypes::SVGUnitType>(); }
    const SVGTransformList& patternTransform() const { return m_patternTransform->currentValue(); }

    SVGAnimatedLength& xAnimated() { return m_x; }
    SVGAnimatedLength& yAnimated() { return m_y; }
    SVGAnimatedLength& widthAnimated() { return m_width; }
    SVGAnimatedLength& heightAnimated() { return m_height; }
    SVGAnimatedEnumeration& patternUnitsAnimated() { return m_patternUnits; }
    SVGAnimatedEnumeration& patternContentUnitsAnimated() { return m_patternContentUnits; }
    SVGAnimatedTransformList& patternTransformAnimated() { return m_patternTransform; }

private:
    SVGPatternElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGPatternElement, SVGElement, SVGExternalResourcesRequired, SVGFitToViewBox, SVGTests, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;
    void childrenChanged(const ChildChange&) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool needsPendingResourceHandling() const final { return false; }
    bool selfHasRelativeLengths() const final { return true; }

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedLength> m_x { SVGAnimatedLength::create(this, LengthModeWidth) };
    Ref<SVGAnimatedLength> m_y { SVGAnimatedLength::create(this, LengthModeHeight) };
    Ref<SVGAnimatedLength> m_width { SVGAnimatedLength::create(this, LengthModeWidth) };
    Ref<SVGAnimatedLength> m_height { SVGAnimatedLength::create(this, LengthModeHeight) };
    Ref<SVGAnimatedEnumeration> m_patternUnits { SVGAnimatedEnumeration::create(this, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) };
    Ref<SVGAnimatedEnumeration> m_patternContentUnits { SVGAnimatedEnumeration::create(this, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) };
    Ref<SVGAnimatedTransformList> m_patternTransform { SVGAnimatedTransformList::create(this) };
};

} // namespace WebCore
