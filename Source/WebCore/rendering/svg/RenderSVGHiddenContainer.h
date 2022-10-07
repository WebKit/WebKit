/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (c) 2022 Igalia S.L.
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

namespace WebCore {
    
class SVGElement;

// This class is for containers which are never drawn, but do need to support style
// <defs>, <linearGradient>, <radialGradient> are all good examples
class RenderSVGHiddenContainer : public RenderSVGContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGHiddenContainer);
public:
    RenderSVGHiddenContainer(SVGElement&, RenderStyle&&);

protected:
    void layout() override;

private:
    bool isSVGHiddenContainer() const final { return true; }
    ASCIILiteral renderName() const override { return "RenderSVGHiddenContainer"_s; }

    void paint(PaintInfo&, const LayoutPoint&) final { }

    LayoutRect clippedOverflowRect(const RenderLayerModelObject*, VisibleRectContext) const final { return { }; }
    std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject*, VisibleRectContext) const final { return std::make_optional(rect); }

    void absoluteRects(Vector<IntRect>&, const LayoutPoint&) const final { }
    void absoluteQuads(Vector<FloatQuad>&, bool*) const final { }
    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject* = nullptr) const final { }

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction) final { return false; }
    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect&, OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const final { }
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGHiddenContainer, isSVGHiddenContainer())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
