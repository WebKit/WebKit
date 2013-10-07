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

#ifndef RenderLineBreak_h
#define RenderLineBreak_h

#include "RenderBoxModelObject.h"

namespace WebCore {

class HTMLElement;
class Position;

class RenderLineBreak FINAL : public RenderBoxModelObject {
public:
    explicit RenderLineBreak(HTMLElement&);
    virtual ~RenderLineBreak();

    // FIXME: The lies here keep render tree dump based test results unchanged.
    virtual const char* renderName() const OVERRIDE { return m_isWBR ? "RenderWordBreak" : "RenderBR"; }

    virtual bool isWBR() const OVERRIDE { return m_isWBR; }

    InlineBox* createInlineBox();
    InlineBox* inlineBoxWrapper() const { return m_inlineBoxWrapper; }
    void setInlineBoxWrapper(InlineBox*);
    void deleteInlineBoxWrapper();
    void replaceInlineBoxWrapper(InlineBox*);
    void dirtyLineBoxes(bool fullLayout);

    IntRect linesBoundingBox() const;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const OVERRIDE;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const OVERRIDE;

private:
    void node() const WTF_DELETED_FUNCTION;

    virtual bool canHaveChildren() const OVERRIDE { return false; }
    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE FINAL { }

    virtual VisiblePosition positionForPoint(const LayoutPoint&) OVERRIDE;
    virtual int caretMinOffset() const OVERRIDE;
    virtual int caretMaxOffset() const OVERRIDE;
    virtual bool canBeSelectionLeaf() const OVERRIDE;
    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine) OVERRIDE;
    virtual void setSelectionState(SelectionState) OVERRIDE;

    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode) const OVERRIDE;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode) const OVERRIDE;

    virtual LayoutUnit marginTop() const OVERRIDE { return 0; }
    virtual LayoutUnit marginBottom() const OVERRIDE { return 0; }
    virtual LayoutUnit marginLeft() const OVERRIDE { return 0; }
    virtual LayoutUnit marginRight() const OVERRIDE { return 0; }
    virtual LayoutUnit marginBefore(const RenderStyle*) const OVERRIDE { return 0; }
    virtual LayoutUnit marginAfter(const RenderStyle*) const OVERRIDE { return 0; }
    virtual LayoutUnit marginStart(const RenderStyle*) const OVERRIDE { return 0; }
    virtual LayoutUnit marginEnd(const RenderStyle*) const OVERRIDE { return 0; }
    virtual LayoutUnit offsetWidth() const OVERRIDE { return linesBoundingBox().width(); }
    virtual LayoutUnit offsetHeight() const OVERRIDE { return linesBoundingBox().height(); }
    virtual IntRect borderBoundingBox() const OVERRIDE;
    virtual LayoutRect frameRectForStickyPositioning() const OVERRIDE { ASSERT_NOT_REACHED(); return LayoutRect(); }
    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject*) const OVERRIDE { return LayoutRect(); }

    virtual void updateFromStyle() OVERRIDE;
    virtual bool requiresLayer() const OVERRIDE { return false; }

    InlineBox* m_inlineBoxWrapper;
    mutable int m_cachedLineHeight;
    bool m_isWBR;
};

inline RenderLineBreak& toRenderLineBreak(RenderObject& object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(object.isLineBreak());
    return static_cast<RenderLineBreak&>(object);
}

inline const RenderLineBreak& toRenderLineBreak(const RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isLineBreak());
    return static_cast<const RenderLineBreak&>(object);
}

inline RenderLineBreak* toRenderLineBreak(RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(object->isLineBreak());
    return static_cast<RenderLineBreak*>(object);
}

inline const RenderLineBreak* toRenderLineBreak(const RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object->isLineBreak());
    return static_cast<const RenderLineBreak*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderLineBreak(const RenderLineBreak&);

} // namespace WebCore

#endif // RenderLineBreak_h
