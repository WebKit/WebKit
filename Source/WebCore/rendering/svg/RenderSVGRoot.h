/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderSVGRoot_h
#define RenderSVGRoot_h

#if ENABLE(SVG)
#include "FloatRect.h"
#include "RenderReplaced.h"

#include "SVGRenderSupport.h"

namespace WebCore {

class AffineTransform;
class SVGSVGElement;

class RenderSVGRoot final : public RenderReplaced {
public:
    RenderSVGRoot(SVGSVGElement&, PassRef<RenderStyle>);
    virtual ~RenderSVGRoot();

    SVGSVGElement& svgSVGElement() const;

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    virtual void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio, bool& isPercentageIntrinsicSize) const override;

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    virtual void setNeedsBoundariesUpdate() override { m_needsBoundariesOrTransformUpdate = true; }
    virtual bool needsBoundariesUpdate() override { return m_needsBoundariesOrTransformUpdate; }
    virtual void setNeedsTransformUpdate() override { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; }

    virtual bool hasRelativeDimensions() const override;
    virtual bool hasRelativeIntrinsicLogicalWidth() const override;
    virtual bool hasRelativeLogicalHeight() const override;

    // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS box coordinates.  
    const AffineTransform& localToBorderBoxTransform() const { return m_localToBorderBoxTransform; }

    // The flag is cleared at the beginning of each layout() pass. Elements then call this
    // method during layout when they are invalidated by a filter.
    static void addResourceForClientInvalidation(RenderSVGResourceContainer*);

    bool hasSVGShadow() const { return m_hasSVGShadow; }
    void setHasSVGShadow(bool hasShadow) { m_hasSVGShadow = hasShadow; }

private:
    void element() const WTF_DELETED_FUNCTION;

    virtual bool isSVGRoot() const override { return true; }
    virtual const char* renderName() const override { return "RenderSVGRoot"; }

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const override;
    virtual LayoutUnit computeReplacedLogicalHeight() const override;
    virtual void layout() override;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) override;

    virtual void willBeDestroyed() override;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override;
    virtual void removeChild(RenderObject&) override;

    virtual const AffineTransform& localToParentTransform() const override;

    bool fillContains(const FloatPoint&) const;
    bool strokeContains(const FloatPoint&) const;

    virtual FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    virtual FloatRect strokeBoundingBox() const override { return m_strokeBoundingBox; }
    virtual FloatRect repaintRectInLocalCoordinates() const override { return m_repaintBoundingBox; }
    virtual FloatRect repaintRectInLocalCoordinatesExcludingSVGShadow() const { return m_repaintBoundingBoxExcludingShadow; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;
    virtual void computeFloatRectForRepaint(const RenderLayerModelObject* repaintContainer, FloatRect& repaintRect, bool fixed) const override;

    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const override;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

    virtual bool canBeSelectionLeaf() const override { return false; }
    virtual bool canHaveChildren() const override { return true; }

    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    FloatRect m_repaintBoundingBoxExcludingShadow;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    HashSet<RenderSVGResourceContainer*> m_resourcesNeedingToInvalidateClients;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
    bool m_hasSVGShadow : 1;
};

template<> inline bool isRendererOfType<const RenderSVGRoot>(const RenderObject& renderer) { return renderer.isSVGRoot(); }
RENDER_OBJECT_TYPE_CASTS(RenderSVGRoot, isSVGRoot())

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGRoot_h
