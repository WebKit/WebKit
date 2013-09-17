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

#ifndef RenderBR_h
#define RenderBR_h

#include "RenderBoxModelObject.h"

namespace WebCore {

class Position;

class RenderBR FINAL : public RenderBoxModelObject {
public:
    explicit RenderBR(Element*);
    virtual ~RenderBR();

    static RenderBR* createAnonymous(Document&);

    virtual const char* renderName() const { return "RenderBR"; }

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
};

inline RenderBR& toRenderBR(RenderObject& object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(object.isBR());
    return static_cast<RenderBR&>(object);
}

inline const RenderBR& toRenderBR(const RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isBR());
    return static_cast<const RenderBR&>(object);
}

inline RenderBR* toRenderBR(RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(object->isBR());
    return static_cast<RenderBR*>(object);
}

inline const RenderBR* toRenderBR(const RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object->isBR());
    return static_cast<const RenderBR*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderBR(const RenderBR&);

} // namespace WebCore

#endif // RenderBR_h
