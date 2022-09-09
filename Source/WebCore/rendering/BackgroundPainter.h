/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "RenderBoxModelObject.h"

namespace WebCore {

class BackgroundPainter {
public:
    BackgroundPainter(RenderBoxModelObject&, const PaintInfo&);

    void paintFillLayer(const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, const InlineIterator::InlineBoxIterator&, const LayoutRect& backgroundImageStrip = { }, CompositeOperator = CompositeOperator::SourceOver, RenderElement* backgroundObject = nullptr, BaseBackgroundColorUsage = BaseBackgroundColorUse);

    static void clipRoundedInnerRect(GraphicsContext&, const FloatRect&, const FloatRoundedRect& clipRect);

private:
    RoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(const LayoutRect& borderRect, BackgroundBleedAvoidance, const InlineIterator::InlineBoxIterator&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;
    RoundedRect getBackgroundRoundedRect(const LayoutRect& borderRect, const InlineIterator::InlineBoxIterator&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;

    const Document& document() const;
    const RenderView& view() const;

    RenderBoxModelObject& m_renderer;
    const PaintInfo& m_paintInfo;
};

}
