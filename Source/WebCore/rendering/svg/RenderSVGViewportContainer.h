/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGContainer.h"
#include "RenderSVGRoot.h"

namespace WebCore {

class SVGSVGElement;

class RenderSVGViewportContainer final : public RenderSVGContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGViewportContainer);
public:
    RenderSVGViewportContainer(RenderSVGRoot&, RenderStyle&&);
    RenderSVGViewportContainer(SVGSVGElement&, RenderStyle&&);

    SVGSVGElement& svgSVGElement() const;
    FloatRect viewport() const { return { { }, viewportSize() }; }
    FloatSize viewportSize() const { return m_viewport.size(); }

    void updateFromStyle() final;

private:
    ASCIILiteral renderName() const final { return "RenderSVGViewportContainer"_s; }

    void element() const = delete;

    bool isOutermostSVGViewportContainer() const { return isAnonymous(); }
    bool updateLayoutSizeIfNeeded() final;
    std::optional<FloatRect> overridenObjectBoundingBoxWithoutTransformations() const final { return std::make_optional(viewport()); }

    FloatPoint computeViewportLocation() const;
    FloatSize computeViewportSize() const;

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption>) const final;
    LayoutRect overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* = nullptr, OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, PaintPhase = PaintPhase::BlockBackground) const final;
    void updateLayerTransform() final;
    bool needsHasSVGTransformFlags() const final;

    AffineTransform m_supplementalLayerTransform;
    FloatRect m_viewport;
    SingleThreadWeakPtr<RenderSVGRoot> m_owningSVGRoot;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGViewportContainer, isRenderSVGViewportContainer())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
