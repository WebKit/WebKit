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

#include "RenderSVGResourceContainer.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class GraphicsContext;
class SVGClipPathElement;
class SVGGraphicsElement;

class RenderSVGResourceClipper final : public RenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceClipper);
public:
    RenderSVGResourceClipper(SVGClipPathElement&, RenderStyle&&);
    virtual ~RenderSVGResourceClipper();

    inline SVGClipPathElement& clipPathElement() const;

    SVGGraphicsElement* shouldApplyPathClipping() const;
    void applyPathClipping(GraphicsContext&, const RenderLayerModelObject& targetRenderer, const FloatRect& objectBoundingBox, SVGGraphicsElement&);
    void applyMaskClipping(PaintInfo&, const RenderLayerModelObject& targetRenderer, const FloatRect& objectBoundingBox);

    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation);

    bool hitTestClipContent(const FloatRect&, const LayoutPoint&);

    inline SVGUnitTypes::SVGUnitType clipPathUnits() const;

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption>) const final;

private:
    void element() const = delete;

    bool needsHasSVGTransformFlags() const final;

    void updateFromStyle() final;

    ASCIILiteral renderName() const final { return "RenderSVGResourceClipper"_s; }
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceClipper, isRenderSVGResourceClipper())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
