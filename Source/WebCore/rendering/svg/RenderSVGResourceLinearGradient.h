/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2021, 2022, 2023 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "AffineTransform.h"
#include "LinearGradientAttributes.h"
#include "RenderSVGResourceGradient.h"
#include "SVGGradientElement.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class SVGLinearGradientElement;

class RenderSVGResourceLinearGradient final : public RenderSVGResourceGradient {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceLinearGradient);
public:
    RenderSVGResourceLinearGradient(SVGLinearGradientElement&, RenderStyle&&);
    virtual ~RenderSVGResourceLinearGradient();

    inline SVGLinearGradientElement& linearGradientElement() const;

    SVGUnitTypes::SVGUnitType gradientUnits() const final { return m_attributes ? m_attributes.value().gradientUnits() : SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN; }
    AffineTransform gradientTransform() const final { return m_attributes ? m_attributes.value().gradientTransform() : identity; }

    void invalidateGradient() final
    {
        m_gradient = nullptr;
        m_attributes = std::nullopt;
        repaintAllClients();
    }

private:
    void collectGradientAttributesIfNeeded() final;
    RefPtr<Gradient> createGradient(const RenderStyle&) final;

    void element() const = delete;
    ASCIILiteral renderName() const final { return "RenderSVGResourceLinearGradient"_s; }

    std::optional<LinearGradientAttributes> m_attributes;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceLinearGradient, isRenderSVGResourceLinearGradient())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
