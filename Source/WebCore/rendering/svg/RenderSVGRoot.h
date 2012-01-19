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
class SVGStyledElement;

class RenderSVGRoot : public RenderReplaced {
public:
    explicit RenderSVGRoot(SVGStyledElement*);
    virtual ~RenderSVGRoot();

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    virtual void computeIntrinsicRatioInformation(FloatSize& intrinsicRatio, bool& isPercentageIntrinsicSize) const;
    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    bool needsSizeNegotiationWithHostDocument() const { return m_needsSizeNegotiationWithHostDocument; }

    void scheduledSizeNegotiationWithHostDocument()
    {
        ASSERT(m_needsSizeNegotiationWithHostDocument);
        m_needsSizeNegotiationWithHostDocument = false;
    }

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    virtual void setNeedsBoundariesUpdate() { m_needsBoundariesOrTransformUpdate = true; }
    virtual void setNeedsTransformUpdate() { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; }

private:
    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }
    virtual bool canHaveChildren() const { return true; }

    virtual bool isSVGRoot() const { return true; }
    virtual const char* renderName() const { return "RenderSVGRoot"; }

    virtual LayoutUnit computeReplacedLogicalWidth(bool includeMaxWidth = true) const;
    virtual LayoutUnit computeReplacedLogicalHeight() const;
    virtual void layout();
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&);

    virtual void willBeDestroyed();
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);
    virtual void updateFromElement();

    virtual const AffineTransform& localToParentTransform() const;

    bool fillContains(const FloatPoint&) const;
    bool strokeContains(const FloatPoint&) const;

    virtual FloatRect objectBoundingBox() const { return m_objectBoundingBox; }
    virtual FloatRect strokeBoundingBox() const { return m_strokeBoundingBox; }
    virtual FloatRect repaintRectInLocalCoordinates() const { return m_repaintBoundingBox; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual LayoutRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer) const;
    virtual void computeFloatRectForRepaint(RenderBoxModelObject* repaintContainer, FloatRect& repaintRect, bool fixed) const;

    virtual void mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState&, bool* wasFixed = 0) const;
    virtual bool canBeSelectionLeaf() const { return false; }

    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    RenderObjectChildList m_children;
    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
    bool m_needsSizeNegotiationWithHostDocument : 1;
};

inline RenderSVGRoot* toRenderSVGRoot(RenderObject* object)
{ 
    ASSERT(!object || object->isSVGRoot());
    return static_cast<RenderSVGRoot*>(object);
}

inline const RenderSVGRoot* toRenderSVGRoot(const RenderObject* object)
{ 
    ASSERT(!object || object->isSVGRoot());
    return static_cast<const RenderSVGRoot*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderSVGRoot(const RenderSVGRoot*);

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGRoot_h
