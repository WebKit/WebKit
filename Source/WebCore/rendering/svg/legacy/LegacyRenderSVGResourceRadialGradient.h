/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "LegacyRenderSVGResourceGradient.h"
#include "RadialGradientAttributes.h"

namespace WebCore {

class SVGRadialGradientElement;

class LegacyRenderSVGResourceRadialGradient final : public LegacyRenderSVGResourceGradient {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGResourceRadialGradient);
public:
    LegacyRenderSVGResourceRadialGradient(SVGRadialGradientElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGResourceRadialGradient();

    inline SVGRadialGradientElement& radialGradientElement() const;

    FloatPoint centerPoint(const RadialGradientAttributes&) const;
    FloatPoint focalPoint(const RadialGradientAttributes&) const;
    float radius(const RadialGradientAttributes&) const;
    float focalRadius(const RadialGradientAttributes&) const;

private:
    RenderSVGResourceType resourceType() const final { return RadialGradientResourceType; }

    SVGUnitTypes::SVGUnitType gradientUnits() const final { return m_attributes.gradientUnits(); }
    AffineTransform gradientTransform() const final { return m_attributes.gradientTransform(); }
    Ref<Gradient> buildGradient(const RenderStyle&) const final;

    void gradientElement() const = delete;

    ASCIILiteral renderName() const final { return "RenderSVGResourceRadialGradient"_s; }
    bool collectGradientAttributes() final;

    RadialGradientAttributes m_attributes;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_LEGACY_RENDER_SVG_RESOURCE(LegacyRenderSVGResourceRadialGradient, RadialGradientResourceType)
