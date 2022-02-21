/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderBox.h"
#include "RenderLayerModelObject.h"
#include "SVGBoundingBoxComputation.h"
#include "SVGRenderSupport.h"

namespace WebCore {

// Most renderers in the SVG rendering tree will inherit from this class
// but not all. RenderSVGForeignObject, RenderSVGBlock, etc. inherit from
// existing RenderBlock classes, that all inherit from RenderLayerModelObject
// directly, without RenderSVGModelObject inbetween. Therefore code which
// needs to be shared between all SVG renderers goes to RenderLayerModelObject.
class SVGElement;

class RenderSVGModelObject : public RenderLayerModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGModelObject);
public:
    bool requiresLayer() const override { return true; }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    static bool checkIntersection(RenderElement*, const FloatRect&);
    static bool checkEnclosure(RenderElement*, const FloatRect&);

    inline SVGElement& element() const;

    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const override;

    // Mimic the RenderBox accessors - by sharing the same terminology the painting / hit testing / layout logic is
    // similar to read compared to non-SVG renderers such as RenderBox & friends.
    LayoutRect borderBoxRectEquivalent() const { return { LayoutPoint(), m_layoutRect.size() }; }
    LayoutRect contentBoxRectEquivalent() const { return borderBoxRectEquivalent(); }
    LayoutRect frameRectEquivalent() const { return m_layoutRect; }

    LayoutRect visualOverflowRectEquivalent() const { return SVGBoundingBoxComputation::computeVisualOverflowRect(*this); }

    void applyTopLeftLocationOffsetEquivalent(LayoutPoint& point) const { point.moveBy(layoutLocation()); }

    LayoutRect layoutRect() const { return m_layoutRect; }
    void setLayoutRect(const LayoutRect& layoutRect) { m_layoutRect = layoutRect; }
    void setLayoutLocation(const LayoutPoint& layoutLocation) { m_layoutRect.setLocation(layoutLocation); }

    LayoutPoint paintingLocation() const { return toLayoutPoint(layoutLocation() - flooredLayoutPoint(objectBoundingBox().minXMinYCorner())); }
    LayoutPoint layoutLocation() const { return m_layoutRect.location(); }
    LayoutSize layoutLocationOffset() const { return toLayoutSize(m_layoutRect.location()); }
    LayoutSize layoutSize() const { return m_layoutRect.size(); }

    // For RenderLayer only
    FloatRect borderBoxRectInFragmentEquivalent(RenderFragmentContainer*, RenderBox::RenderBoxFragmentInfoFlags = RenderBox::CacheRenderBoxFragmentInfo) const;
    virtual LayoutRect overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* = nullptr, OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, PaintPhase = PaintPhase::BlockBackground) const;
    LayoutRect overflowClipRectForChildLayers(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy relevancy) { return overflowClipRect(location, fragment, relevancy); }

protected:
    RenderSVGModelObject(SVGElement&, RenderStyle&&);

    void willBeDestroyed() override;
    void updateFromStyle() override;
    bool shouldPaintSVGRenderer(const PaintInfo&) const;

    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const override;
    std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;
    void mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode>, TransformState&) const override;
    void mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const final;
    LayoutRect outlineBoundsForRepaint(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap*) const final;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject*, RenderGeometryMap&) const override;
    LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override;
    // FIXME: [LBSE] Upstream SVG outline painting functionality
    // void paintSVGOutline(PaintInfo&, const LayoutPoint& adjustedPaintOffset);

private:
    bool isRenderSVGModelObject() const final { return true; }

    LayoutRect m_layoutRect;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGModelObject, isRenderSVGModelObject())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
