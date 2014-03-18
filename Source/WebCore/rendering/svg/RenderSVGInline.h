/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc.
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

#ifndef RenderSVGInline_h
#define RenderSVGInline_h

#include "RenderInline.h"
#include "SVGGraphicsElement.h"

namespace WebCore {

class RenderSVGInline : public RenderInline {
public:
    RenderSVGInline(SVGGraphicsElement&, PassRef<RenderStyle>);

    SVGGraphicsElement& graphicsElement() const { return toSVGGraphicsElement(nodeForNonAnonymous()); }

private:
    void element() const = delete;

    virtual const char* renderName() const override { return "RenderSVGInline"; }
    virtual bool requiresLayer() const override final { return false; }
    virtual bool isSVGInline() const override final { return true; }

    virtual void updateFromStyle() override final;

    // Chapter 10.4 of the SVG Specification say that we should use the
    // object bounding box of the parent text element.
    // We search for the root text element and take its bounding box.
    // It is also necessary to take the stroke and repaint rect of
    // this element, since we need it for filters.
    virtual FloatRect objectBoundingBox() const override final;
    virtual FloatRect strokeBoundingBox() const override final;
    virtual FloatRect repaintRectInLocalCoordinates() const override final;

    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override final;
    virtual void computeFloatRectForRepaint(const RenderLayerModelObject* repaintContainer, FloatRect&, bool fixed = false) const override final;
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const override final;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override final;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override final;

    virtual std::unique_ptr<InlineFlowBox> createInlineFlowBox() override final;

    virtual void willBeDestroyed() override final;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override final;

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override final;
    virtual void removeChild(RenderObject&) override final;
};

}

#endif // !RenderSVGTSpan_H
