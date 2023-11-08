/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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
#include "FloatPoint.h"
#include "FloatRect.h"
#include "RenderSVGBlock.h"
#include "SVGBoundingBoxComputation.h"

namespace WebCore {

class SVGForeignObjectElement;

class RenderSVGForeignObject final : public RenderSVGBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGForeignObject);
public:
    RenderSVGForeignObject(SVGForeignObjectElement&, RenderStyle&&);
    virtual ~RenderSVGForeignObject();

    SVGForeignObjectElement& foreignObjectElement() const;

    void paint(PaintInfo&, const LayoutPoint&) override;

    void layout() override;

    FloatRect objectBoundingBox() const final { return m_viewport; }
    FloatRect strokeBoundingBox() const final { return m_viewport; }
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

private:
    void graphicsElement() const = delete;
    ASCIILiteral renderName() const override { return "RenderSVGForeignObject"_s; }

    void updateLogicalWidth() override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    LayoutRect overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* = nullptr, OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, PaintPhase = PaintPhase::BlockBackground) const final;

    void updateFromStyle() final;

    // Enforce <fO> to carry a transform: <fO> should behave as absolutely positioned container
    // for CSS content. Thus it needs to become a rootPaintingLayer during paint() such that
    // fixed position content uses the <fO> as ancestor layer (when computing offsets from the container).
    bool needsHasSVGTransformFlags() const final { return true; }

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption>) const final;

    FloatRect m_viewport;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGForeignObject, isRenderSVGForeignObject())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
