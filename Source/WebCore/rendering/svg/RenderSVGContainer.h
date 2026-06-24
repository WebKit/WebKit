/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "RenderSVGModelObject.h"
#include "SVGBoundingBoxComputation.h"

namespace WebCore {

class SVGElement;

class RenderSVGContainer : public RenderSVGModelObject {
    WTF_MAKE_TZONE_ALLOCATED(RenderSVGContainer);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGContainer);
public:
    virtual ~RenderSVGContainer();

    void paint(PaintInfo&, const LayoutPoint&) override;

    bool isObjectBoundingBoxValid() const
    {
        updateSVGTransformDependentBoundingBoxesIfNeeded();
        return m_objectBoundingBoxValid;
    }
    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    bool didTransformToRootUpdate() const { return m_didTransformToRootUpdate; }

    FloatRect objectBoundingBox() const final
    {
        updateSVGTransformDependentBoundingBoxesIfNeeded();
        return m_objectBoundingBox;
    }
    FloatRect objectBoundingBoxWithoutTransformations() const final { return m_objectBoundingBoxWithoutTransformations; }
    // A viewport-establishing container (inner <svg>, <marker>) overrides this to return its viewport
    // rectangle, so that it contributes its viewport (not its clipped descendant geometry) to an
    // ancestor's "without transformations" object bounding box. Public so SVGBoundingBoxComputation
    // can honor it while recursing.
    virtual std::optional<FloatRect> overridenObjectBoundingBoxWithoutTransformations() const { return std::nullopt; }
    FloatRect strokeBoundingBox() const final;

    void invalidateCachedSVGTransformDependentBoundingBoxes() final { m_transformDependentBoundingBoxesDirty = true; }
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }
    FloatRect decoratedBoundingBox() const final { return SVGBoundingBoxComputation::computeDecoratedBoundingBox(*this); }

protected:
    RenderSVGContainer(Type, Document&, Style::ComputedStyle&&, OptionSet<SVGModelObjectFlag> = { });
    RenderSVGContainer(Type, SVGElement&, Style::ComputedStyle&&, OptionSet<SVGModelObjectFlag> = { });

    ASCIILiteral renderName() const override { return "RenderSVGContainer"_s; }
    bool canHaveChildren() const final { return true; }

    void layout() override;

    virtual void layoutChildren();
    virtual bool pointIsInsideViewportClip(const FloatPoint&) { return true; }
    virtual bool updateLayoutSizeIfNeeded() { return false; }
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    void addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* container) const override;

    bool m_isLayoutSizeChanged { false };
    bool m_didTransformToRootUpdate { false };

private:
    // Recompute m_objectBoundingBox / m_strokeBoundingBox lazily after a descendant transform
    // changes via the deferred (layout-free) flush. Both fold in descendant transforms and go
    // stale, unlike the without-transform and visual-overflow caches. Kept private so every read
    // funnels through the getters above, which honor m_transformDependentBoundingBoxesDirty.
    void updateSVGTransformDependentBoundingBoxesIfNeeded() const;

    mutable bool m_objectBoundingBoxValid { false };
    mutable bool m_transformDependentBoundingBoxesDirty { false };
    mutable FloatRect m_objectBoundingBox;
    FloatRect m_objectBoundingBoxWithoutTransformations;
    mutable Markable<FloatRect> m_strokeBoundingBox;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGContainer, isRenderSVGContainer())

