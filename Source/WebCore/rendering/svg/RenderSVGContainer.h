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

#ifndef RenderSVGContainer_h
#define RenderSVGContainer_h

#if ENABLE(SVG)

#include "RenderSVGModelObject.h"

namespace WebCore {

class SVGElement;

class RenderSVGContainer : public RenderSVGModelObject {
public:
    virtual ~RenderSVGContainer();

    virtual void paint(PaintInfo&, const LayoutPoint&) override;
    virtual void setNeedsBoundariesUpdate() override final { m_needsBoundariesUpdate = true; }
    virtual bool needsBoundariesUpdate() override final { return m_needsBoundariesUpdate; }
    virtual bool didTransformToRootUpdate() { return false; }
    bool isObjectBoundingBoxValid() const { return m_objectBoundingBoxValid; }

protected:
    RenderSVGContainer(SVGElement&, PassRef<RenderStyle>);

    virtual bool isSVGContainer() const override final { return true; }
    virtual const char* renderName() const override { return "RenderSVGContainer"; }

    virtual bool canHaveChildren() const override final { return true; }

    virtual void layout() override;

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override final;
    virtual void removeChild(RenderObject&) override final;
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override final;

    virtual FloatRect objectBoundingBox() const override final { return m_objectBoundingBox; }
    virtual FloatRect strokeBoundingBox() const override final { return m_strokeBoundingBox; }
    virtual FloatRect repaintRectInLocalCoordinates() const override final { return m_repaintBoundingBox; }

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

    // Allow RenderSVGTransformableContainer to hook in at the right time in layout()
    virtual bool calculateLocalTransform() { return false; }

    // Allow RenderSVGViewportContainer to hook in at the right times in layout(), paint() and nodeAtFloatPoint()
    virtual void calcViewport() { }
    virtual void applyViewportClip(PaintInfo&) { }
    virtual bool pointIsInsideViewportClip(const FloatPoint& /*pointInParent*/) { return true; }

    virtual void determineIfLayoutSizeChanged() { }

    bool selfWillPaint();
    void updateCachedBoundaries();

private:
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    bool m_needsBoundariesUpdate : 1;
};

RENDER_OBJECT_TYPE_CASTS(RenderSVGContainer, isSVGContainer())

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGContainer_h
