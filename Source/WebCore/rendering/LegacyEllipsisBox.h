/**
 * Copyright (C) 2003, 2006 Apple Inc.
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

#include "LegacyInlineElementBox.h"
#include "RenderBlockFlow.h"

namespace WebCore {

class HitTestRequest;
class HitTestResult;

class LegacyEllipsisBox final : public LegacyInlineElementBox {
    WTF_MAKE_ISO_ALLOCATED(LegacyEllipsisBox);
public:
    LegacyEllipsisBox(RenderBlockFlow&, const AtomString& ellipsisStr, LegacyInlineFlowBox* parent, int width, int height, int y, bool firstLine, bool isHorizontal, LegacyInlineBox* markupBox);
    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) final;
    void setSelectionState(RenderObject::HighlightState s) { m_selectionState = s; }
    IntRect selectionRect();

    RenderBlockFlow& blockFlow() const { return downcast<RenderBlockFlow>(LegacyInlineBox::renderer()); }

private:
    void paintMarkupBox(PaintInfo&, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const RenderStyle&);
    int height() const { return m_height; }
    RenderObject::HighlightState selectionState() override { return m_selectionState; }
    void paintSelection(GraphicsContext&, const LayoutPoint&, const RenderStyle&, const FontCascade&);
    LegacyInlineBox* markupBox() const;

    bool m_shouldPaintMarkupBox;
    RenderObject::HighlightState m_selectionState { RenderObject::HighlightState::None };
    int m_height;
    AtomString m_str;
};

} // namespace WebCore
