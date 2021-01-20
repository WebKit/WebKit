/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGURIReference.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class SVGFilterElement final : public SVGElement, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGFilterElement);
public:
    static Ref<SVGFilterElement> create(const QualifiedName&, Document&);

    SVGUnitTypes::SVGUnitType filterUnits() const { return m_filterUnits->currentValue<SVGUnitTypes::SVGUnitType>(); }
    SVGUnitTypes::SVGUnitType primitiveUnits() const { return m_primitiveUnits->currentValue<SVGUnitTypes::SVGUnitType>(); }
    const SVGLengthValue& x() const { return m_x->currentValue(); }
    const SVGLengthValue& y() const { return m_y->currentValue(); }
    const SVGLengthValue& width() const { return m_width->currentValue(); }
    const SVGLengthValue& height() const { return m_height->currentValue(); }

    SVGAnimatedEnumeration& filterUnitsAnimated() { return m_filterUnits; }
    SVGAnimatedEnumeration& primitiveUnitsAnimated() { return m_primitiveUnits; }
    SVGAnimatedLength& xAnimated() { return m_x; }
    SVGAnimatedLength& yAnimated() { return m_y; }
    SVGAnimatedLength& widthAnimated() { return m_width; }
    SVGAnimatedLength& heightAnimated() { return m_height; }

private:
    SVGFilterElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFilterElement, SVGElement, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;
    void childrenChanged(const ChildChange&) final;

    bool needsPendingResourceHandling() const final { return false; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;

    bool selfHasRelativeLengths() const final { return true; }

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedEnumeration> m_filterUnits { SVGAnimatedEnumeration::create(this, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) };
    Ref<SVGAnimatedEnumeration> m_primitiveUnits { SVGAnimatedEnumeration::create(this, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) };
    Ref<SVGAnimatedLength> m_x { SVGAnimatedLength::create(this, SVGLengthMode::Width, "-10%") };
    Ref<SVGAnimatedLength> m_y { SVGAnimatedLength::create(this, SVGLengthMode::Height, "-10%") };
    Ref<SVGAnimatedLength> m_width { SVGAnimatedLength::create(this, SVGLengthMode::Width, "120%") };
    Ref<SVGAnimatedLength> m_height { SVGAnimatedLength::create(this, SVGLengthMode::Height, "120%") };
};

} // namespace WebCore
