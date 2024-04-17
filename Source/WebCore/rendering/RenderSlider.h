/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "RenderFlexibleBox.h"

namespace WebCore {

class HTMLInputElement;
class MouseEvent;

class RenderSlider final : public RenderFlexibleBox {
    WTF_MAKE_ISO_ALLOCATED(RenderSlider);
public:
    static const int defaultTrackLength;

    RenderSlider(HTMLInputElement&, RenderStyle&&);
    virtual ~RenderSlider();

    HTMLInputElement& element() const;

    bool inDragMode() const;

    double valueRatio() const;

private:
    ASCIILiteral renderName() const override { return "RenderSlider"_s; }

    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;

    bool isFlexibleBoxImpl() const override { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSlider, isRenderSlider())
