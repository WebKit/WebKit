/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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

#include "RenderElement.h"

namespace WebCore {

class RenderInline;
class RenderListBox;
struct PaintInfo;

class OutlinePainter {
public:
    OutlinePainter(const PaintInfo&);

    void paintOutline(const RenderElement&, const LayoutRect& paintRect) const;
    void paintOutline(const RenderInline&, const LayoutPoint& paintOffset) const;

    static Vector<LayoutRect> collectFocusRingRects(const RenderElement&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);

private:
    void paintOutlineWithLineRects(const RenderInline&, const LayoutPoint& paintOffset, const Vector<LayoutRect>& lineRects) const;
    void paintFocusRing(const RenderElement&, const Vector<LayoutRect>&) const;

    static void collectFocusRingRects(const RenderElement&, Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);
    static void collectFocusRingRectsForInline(const RenderInline&, Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject*);
    static bool collectFocusRingRectsForListBox(const RenderListBox&, Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject*);
    static bool collectFocusRingRectsForBlock(const RenderBlock&, Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject*);
    static void collectFocusRingRectsForInlineChildren(const RenderBlockFlow&, Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject*);
    static void collectFocusRingRectsForChildBox(const RenderBox&, Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject*);

    void addPDFURLAnnotationForLink(const RenderElement&, const LayoutPoint& paintOffset) const;

    const PaintInfo& m_paintInfo;
};

}
