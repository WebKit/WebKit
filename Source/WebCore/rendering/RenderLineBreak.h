/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

class InlineElementBox;
class HTMLElement;
class Position;

class RenderLineBreak final : public RenderBoxModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderLineBreak);
public:
    RenderLineBreak(HTMLElement&, RenderStyle&&);
    virtual ~RenderLineBreak();

    // FIXME: The lies here keep render tree dump based test results unchanged.
    const char* renderName() const override { return m_isWBR ? "RenderWordBreak" : "RenderBR"; }

    bool isWBR() const override { return m_isWBR; }

    std::unique_ptr<InlineElementBox> createInlineBox();
    InlineElementBox* inlineBoxWrapper() const { return m_inlineBoxWrapper; }
    void setInlineBoxWrapper(InlineElementBox*);
    void deleteInlineBoxWrapper();
    void replaceInlineBoxWrapper(InlineElementBox&);
    void dirtyLineBoxes(bool fullLayout);

    IntRect linesBoundingBox() const;
    IntRect boundingBoxForRenderTreeDump() const;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;
#if PLATFORM(IOS_FAMILY)
void collectSelectionRects(Vector<SelectionRect>&, unsigned startOffset = 0, unsigned endOffset = std::numeric_limits<unsigned>::max()) override;
#endif
    void ensureLineBoxes();

private:
    void node() const = delete;

    bool canHaveChildren() const override { return false; }
    void paint(PaintInfo&, const LayoutPoint&) override { }

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) override;
    int caretMinOffset() const override;
    int caretMaxOffset() const override;
    bool canBeSelectionLeaf() const override;
    LayoutRect localCaretRect(InlineBox*, unsigned caretOffset, LayoutUnit* extraWidthToEndOfLine) override;
    void setSelectionState(SelectionState) override;

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode) const override;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode) const override;

    LayoutUnit marginTop() const override { return 0; }
    LayoutUnit marginBottom() const override { return 0; }
    LayoutUnit marginLeft() const override { return 0; }
    LayoutUnit marginRight() const override { return 0; }
    LayoutUnit marginBefore(const RenderStyle*) const override { return 0; }
    LayoutUnit marginAfter(const RenderStyle*) const override { return 0; }
    LayoutUnit marginStart(const RenderStyle*) const override { return 0; }
    LayoutUnit marginEnd(const RenderStyle*) const override { return 0; }
    LayoutUnit offsetWidth() const override { return linesBoundingBox().width(); }
    LayoutUnit offsetHeight() const override { return linesBoundingBox().height(); }
    LayoutRect borderBoundingBox() const override { return LayoutRect(LayoutPoint(), linesBoundingBox().size()); }
    LayoutRect frameRectForStickyPositioning() const override { ASSERT_NOT_REACHED(); return LayoutRect(); }
    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject*) const override { return LayoutRect(); }

    void updateFromStyle() override;
    bool requiresLayer() const override { return false; }

    InlineElementBox* m_inlineBoxWrapper;
    mutable int m_cachedLineHeight;
    bool m_isWBR;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderLineBreak, isLineBreak())
