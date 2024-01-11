/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "LegacyRenderSVGContainer.h"

namespace WebCore {

class SVGElement;

// This class is for containers which are never drawn, but do need to support style
// <defs>, <linearGradient>, <radialGradient> are all good examples
class LegacyRenderSVGHiddenContainer : public LegacyRenderSVGContainer {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGHiddenContainer);
public:
    LegacyRenderSVGHiddenContainer(Type, SVGElement&, RenderStyle&&, OptionSet<SVGModelObjectFlag> = { });

protected:
    void layout() override;

private:
    ASCIILiteral renderName() const override { return "RenderSVGHiddenContainer"_s; }

    void paint(PaintInfo&, const LayoutPoint&) final;

    LayoutRect clippedOverflowRect(const RenderLayerModelObject*, VisibleRectContext) const final { return { }; }
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(LegacyRenderSVGHiddenContainer, isLegacyRenderSVGHiddenContainer())
