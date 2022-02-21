/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGModelObject.h"
#include "SVGBoundingBoxComputation.h"

namespace WebCore {

class SVGElement;

class RenderSVGContainer : public RenderSVGModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGContainer);
public:
    virtual ~RenderSVGContainer();

    void paint(PaintInfo&, const LayoutPoint&) override;
    bool isObjectBoundingBoxValid() const { return m_objectBoundingBoxValid; }

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const final { return m_strokeBoundingBox; }
    FloatRect repaintRectInLocalCoordinates() const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

protected:
    RenderSVGContainer(SVGElement&, RenderStyle&&);

    const char* renderName() const override { return "RenderSVGContainer"; }
    bool canHaveChildren() const final { return true; }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void layout() override;
    virtual void layoutChildren();
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    virtual void updateLayerInformation() { }
    virtual void calculateViewport();
    virtual bool pointIsInsideViewportClip(const FloatPoint&) { return true; }

    bool selfWillPaint();

    bool m_objectBoundingBoxValid { false };
    FloatRect m_objectBoundingBox;
    FloatRect m_strokeBoundingBox;

private:
    bool isSVGContainer() const final { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGContainer, isSVGContainer())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
