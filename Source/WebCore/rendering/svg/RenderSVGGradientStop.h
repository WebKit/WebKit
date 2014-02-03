/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef RenderSVGGradientStop_h
#define RenderSVGGradientStop_h

#if ENABLE(SVG)
#include "RenderElement.h"
#include "SVGStopElement.h"

namespace WebCore {
    
class SVGGradientElement;

// This class exists mostly so we can hear about gradient stop style changes
class RenderSVGGradientStop final : public RenderElement {
public:
    RenderSVGGradientStop(SVGStopElement&, PassRef<RenderStyle>);
    virtual ~RenderSVGGradientStop();

    SVGStopElement& element() const { return toSVGStopElement(RenderObject::nodeForNonAnonymous()); }

private:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual void layout() override;

    // These overrides are needed to prevent ASSERTs on <svg><stop /></svg>
    // RenderObject's default implementations ASSERT_NOT_REACHED()
    // https://bugs.webkit.org/show_bug.cgi?id=20400
    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject*) const override { return LayoutRect(); }
    virtual FloatRect objectBoundingBox() const override { return FloatRect(); }
    virtual FloatRect strokeBoundingBox() const override { return FloatRect(); }
    virtual FloatRect repaintRectInLocalCoordinates() const override { return FloatRect(); }
    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint&, HitTestAction) override { return false; }

    virtual bool isSVGGradientStop() const override { return true; }
    virtual const char* renderName() const override { return "RenderSVGGradientStop"; }

    virtual bool canHaveChildren() const override { return false; }
    virtual void paint(PaintInfo&, const LayoutPoint&) override { }

    SVGGradientElement* gradientElement();
};

RENDER_OBJECT_TYPE_CASTS(RenderSVGGradientStop, isSVGGradientStop())

}

#endif // ENABLE(SVG)
#endif // RenderSVGGradientStop_h
