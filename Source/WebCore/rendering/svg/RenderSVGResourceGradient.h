/*
 * Copyright (C) 2023, 2024 Igalia S.L.
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
#include "RenderSVGResourcePaintServer.h"
#include "SVGGradientElement.h"

namespace WebCore {

class Gradient;

class RenderSVGResourceGradient : public RenderSVGResourcePaintServer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceGradient);
public:
    virtual ~RenderSVGResourceGradient();

    inline SVGGradientElement& gradientElement() const;

    bool prepareFillOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) final;
    bool prepareStrokeOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) final;

    virtual void invalidateGradient() = 0;

    virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;

protected:
    RenderSVGResourceGradient(Type, SVGElement&, RenderStyle&&);

    virtual void collectGradientAttributesIfNeeded() = 0;
    virtual RefPtr<Gradient> createGradient(const RenderStyle&) = 0;

    virtual AffineTransform gradientTransform() const = 0;

    bool buildGradientIfNeeded(const RenderLayerModelObject&, const RenderStyle&, AffineTransform& userspaceTransform);
    GradientColorStops stopsByApplyingColorFilter(const GradientColorStops&, const RenderStyle&) const;
    GradientSpreadMethod platformSpreadMethodFromSVGType(SVGSpreadMethodType) const;

    RefPtr<Gradient> m_gradient;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceGradient, isRenderSVGResourceGradient())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
