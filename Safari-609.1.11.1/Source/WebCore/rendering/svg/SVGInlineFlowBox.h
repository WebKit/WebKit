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

#pragma once

#include "InlineFlowBox.h"
#include "RenderSVGInline.h"

namespace WebCore {

class RenderSVGInlineText;

class SVGInlineFlowBox final : public InlineFlowBox {
    WTF_MAKE_ISO_ALLOCATED(SVGInlineFlowBox);
public:
    SVGInlineFlowBox(RenderSVGInline& renderer)
        : InlineFlowBox(renderer)
        , m_logicalHeight(0)
    {
    }

    RenderSVGInline& renderer() { return static_cast<RenderSVGInline&>(InlineFlowBox::renderer()); }

    FloatRect calculateBoundaries() const override;

    void setLogicalHeight(float h) { m_logicalHeight = h; }
    void paintSelectionBackground(PaintInfo&);

private:
    bool isSVGInlineFlowBox() const override { return true; }
    float virtualLogicalHeight() const override { return m_logicalHeight; }
    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;

    float m_logicalHeight;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(SVGInlineFlowBox, isSVGInlineFlowBox())
