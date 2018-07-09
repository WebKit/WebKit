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

#pragma once

#include "RenderSVGModelObject.h"

namespace WebCore {

class SVGElement;

class RenderSVGContainer : public RenderSVGModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGContainer);
public:
    virtual ~RenderSVGContainer();

    void paint(PaintInfo&, const LayoutPoint&) override;
    void setNeedsBoundariesUpdate() final { m_needsBoundariesUpdate = true; }
    bool needsBoundariesUpdate() final { return m_needsBoundariesUpdate; }
    virtual bool didTransformToRootUpdate() { return false; }
    bool isObjectBoundingBoxValid() const { return m_objectBoundingBoxValid; }

protected:
    RenderSVGContainer(SVGElement&, RenderStyle&&);

    const char* renderName() const override { return "RenderSVGContainer"; }

    bool canHaveChildren() const final { return true; }

    void layout() override;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) final;

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const final { return m_strokeBoundingBox; }
    FloatRect repaintRectInLocalCoordinates() const final { return m_repaintBoundingBox; }

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

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
    bool isSVGContainer() const final { return true; }

    FloatRect m_objectBoundingBox;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;

    bool m_objectBoundingBoxValid { false };
    bool m_needsBoundariesUpdate { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGContainer, isSVGContainer())
