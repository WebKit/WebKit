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
    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    bool didTransformToRootUpdate() const { return m_didTransformToRootUpdate; }

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect objectBoundingBoxWithoutTransformations() const final { return m_objectBoundingBoxWithoutTransformations; }
    FloatRect strokeBoundingBox() const final;
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

protected:
    RenderSVGContainer(Type, Document&, RenderStyle&&, OptionSet<SVGModelObjectFlag> = { });
    RenderSVGContainer(Type, SVGElement&, RenderStyle&&, OptionSet<SVGModelObjectFlag> = { });

    ASCIILiteral renderName() const override { return "RenderSVGContainer"_s; }
    bool canHaveChildren() const final { return true; }

    void layout() override;

    virtual void layoutChildren();
    virtual bool pointIsInsideViewportClip(const FloatPoint&) { return true; }
    virtual bool updateLayoutSizeIfNeeded() { return false; }
    virtual std::optional<FloatRect> overridenObjectBoundingBoxWithoutTransformations() const { return std::nullopt; }
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    bool m_objectBoundingBoxValid { false };
    bool m_isLayoutSizeChanged { false };
    bool m_didTransformToRootUpdate { false };
    FloatRect m_objectBoundingBox;
    FloatRect m_objectBoundingBoxWithoutTransformations;
    mutable Markable<FloatRect, FloatRect::MarkableTraits> m_strokeBoundingBox;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGContainer, isRenderSVGContainer())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
