/*
 * Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef RootInlineBox_h
#define RootInlineBox_h

#include "BidiContext.h"
#include "InlineFlowBox.h"

namespace WebCore {

class EllipsisBox;
class HitTestResult;

struct BidiStatus;
struct GapRects;

class RootInlineBox : public InlineFlowBox {
public:
    RootInlineBox(RenderObject* obj)
        : InlineFlowBox(obj)
        , m_lineBreakObj(0)
        , m_lineBreakPos(0)
        , m_lineTop(0)
        , m_lineBottom(0)
    {
    }

    virtual void destroy(RenderArena*);

    virtual bool isRootInlineBox() const { return true; }

    void detachEllipsisBox(RenderArena*);

    RootInlineBox* nextRootBox() const { return static_cast<RootInlineBox*>(m_nextLine); }
    RootInlineBox* prevRootBox() const { return static_cast<RootInlineBox*>(m_prevLine); }

    virtual void adjustPosition(int dx, int dy);

    int lineTop() const { return m_lineTop; }
    int lineBottom() const { return m_lineBottom; }

    int selectionTop() const;
    int selectionBottom() const { return lineBottom(); }
    int selectionHeight() const { return max(0, selectionBottom() - selectionTop()); }

    virtual int verticallyAlignBoxes(int heightOfBlock);
    void setLineTopBottomPositions(int top, int bottom);

    virtual RenderLineBoxList* rendererLineBoxes() const;

#if ENABLE(SVG)
    virtual void computePerCharacterLayoutInformation() { }
#endif

    RenderObject* lineBreakObj() const { return m_lineBreakObj; }
    BidiStatus lineBreakBidiStatus() const;
    void setLineBreakInfo(RenderObject*, unsigned breakPos, const BidiStatus&);

    unsigned lineBreakPos() const { return m_lineBreakPos; }
    void setLineBreakPos(unsigned p) { m_lineBreakPos = p; }

    int blockHeight() const { return m_blockHeight; }
    void setBlockHeight(int h) { m_blockHeight = h; }

    bool endsWithBreak() const { return m_endsWithBreak; }
    void setEndsWithBreak(bool b) { m_endsWithBreak = b; }

    void childRemoved(InlineBox* box);

    bool canAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth);
    void placeEllipsis(const AtomicString& ellipsisStr, bool ltr, int blockLeftEdge, int blockRightEdge, int ellipsisWidth, InlineBox* markupBox = 0);
    virtual int placeEllipsisBox(bool ltr, int blockLeftEdge, int blockRightEdge, int ellipsisWidth, bool& foundBox);

    EllipsisBox* ellipsisBox() const;

    void paintEllipsisBox(RenderObject::PaintInfo&, int tx, int ty) const;
    bool hitTestEllipsisBox(HitTestResult&, int x, int y, int tx, int ty, HitTestAction, bool);

    virtual void clearTruncation();

#if PLATFORM(MAC)
    void addHighlightOverflow();
    void paintCustomHighlight(RenderObject::PaintInfo&, int tx, int ty, const AtomicString& highlightType);
#endif

    virtual void paint(RenderObject::PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int);

    bool hasSelectedChildren() const { return m_hasSelectedChildren; }
    void setHasSelectedChildren(bool);

    virtual RenderObject::SelectionState selectionState();
    InlineBox* firstSelectedBox();
    InlineBox* lastSelectedBox();

    GapRects fillLineSelectionGap(int selTop, int selHeight, RenderBlock* rootBlock, int blockX, int blockY,
                                  int tx, int ty, const RenderObject::PaintInfo*);

    RenderBlock* block() const;

    InlineBox* closestLeafChildForXPos(int x, bool onlyEditableLeaves = false);

    Vector<RenderBox*>& floats()
    {
        ASSERT(!isDirty());
        if (!m_floats)
            m_floats.set(new Vector<RenderBox*>());
        return *m_floats;
    }

    Vector<RenderBox*>* floatsPtr() { ASSERT(!isDirty()); return m_floats.get(); }

    virtual void extractLineBoxFromRenderObject();
    virtual void attachLineBoxToRenderObject();
    virtual void removeLineBoxFromRenderObject();

protected:
    // Where this line ended.  The exact object and the position within that object are stored so that
    // we can create an InlineIterator beginning just after the end of this line.
    RenderObject* m_lineBreakObj;
    unsigned m_lineBreakPos;
    RefPtr<BidiContext> m_lineBreakContext;

    int m_lineTop;
    int m_lineBottom;

    // Floats hanging off the line are pushed into this vector during layout. It is only
    // good for as long as the line has not been marked dirty.
    OwnPtr<Vector<RenderBox*> > m_floats;

    // The height of the block at the end of this line.  This is where the next line starts.
    int m_blockHeight;

    WTF::Unicode::Direction m_lineBreakBidiStatusEor : 5;
    WTF::Unicode::Direction m_lineBreakBidiStatusLastStrong : 5;
    WTF::Unicode::Direction m_lineBreakBidiStatusLast : 5;
};

inline void RootInlineBox::setLineTopBottomPositions(int top, int bottom) 
{ 
    m_lineTop = top; 
    m_lineBottom = bottom; 
}

} // namespace WebCore

#endif // RootInlineBox_h
