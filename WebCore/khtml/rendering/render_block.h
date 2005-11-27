/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RENDER_BLOCK_H
#define RENDER_BLOCK_H

#include <qptrlist.h>

#include "render_flow.h"

typedef enum {
    CursorCaret,
    DragCaret,
} CaretType;

namespace DOM {
    class Position;
}

namespace khtml {

class RenderBlock : public RenderFlow
{
public:
    RenderBlock(DOM::NodeImpl* node);
    virtual ~RenderBlock();

    virtual const char *renderName() const;

    // These two functions are overridden for inline-block.
    virtual short lineHeight(bool b, bool isRootLineBox=false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox=false) const;
    
    virtual bool isRenderBlock() const { return true; }
    virtual bool isBlockFlow() const { return (!isInline() || isReplaced()) && !isTable(); }
    virtual bool isInlineFlow() const { return isInline() && !isReplaced(); }
    virtual bool isInlineBlockOrInlineTable() const { return isInline() && isReplaced(); }
    
    virtual bool childrenInline() const { return m_childrenInline; }
    virtual void setChildrenInline(bool b) { m_childrenInline = b; }
    void makeChildrenNonInline(RenderObject* insertionPoint = 0);

    // The height (and width) of a block when you include overflow spillage out of the bottom
    // of the block (e.g., a <div style="height:25px"> that has a 100px tall image inside
    // it would have an overflow height of borderTop() + paddingTop() + 100px.
    virtual int overflowHeight(bool includeInterior=true) const;
    virtual int overflowWidth(bool includeInterior=true) const;
    virtual int overflowLeft(bool includeInterior=true) const;
    virtual int overflowTop(bool includeInterior=true) const;
    virtual QRect overflowRect(bool includeInterior=true) const;
    virtual void setOverflowHeight(int h) { m_overflowHeight = h; }
    virtual void setOverflowWidth(int w) { m_overflowWidth = w; }
    
    virtual bool isSelfCollapsingBlock() const;
    virtual bool isTopMarginQuirk() const { return m_topMarginQuirk; }
    virtual bool isBottomMarginQuirk() const { return m_bottomMarginQuirk; }

    virtual int maxTopMargin(bool positive) const {
        if (positive)
            return m_maxTopPosMargin;
        else
            return m_maxTopNegMargin;
    }
    virtual int maxBottomMargin(bool positive) const {
        if (positive)
            return m_maxBottomPosMargin;
        else
            return m_maxBottomNegMargin;
    }

    void initMaxMarginValues() {
        if (m_marginTop >= 0)
            m_maxTopPosMargin = m_marginTop;
        else
            m_maxTopNegMargin = -m_marginTop;
        if (m_marginBottom >= 0)
            m_maxBottomPosMargin = m_marginBottom;
        else
            m_maxBottomNegMargin = -m_marginBottom;
    }

    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild);
    virtual void removeChild(RenderObject *oldChild);

    virtual void repaintObjectsBeforeLayout();
    virtual void repaintFloatingDescendants();
    virtual void getAbsoluteRepaintRectIncludingFloats(QRect& bounds, QRect& fullBounds);

    virtual void setStyle(RenderStyle* _style);

    virtual void layout();
    virtual void layoutBlock(bool relayoutChildren);
    void layoutBlockChildren(bool relayoutChildren);
    QRect layoutInlineChildren(bool relayoutChildren);

    void layoutPositionedObjects( bool relayoutChildren );
    void insertPositionedObject(RenderObject *o);
    void removePositionedObject(RenderObject *o);

    // Called to lay out the legend for a fieldset.
    virtual RenderObject* layoutLegend(bool relayoutChildren) { return 0; };
    
    // the implementation of the following functions is in bidi.cpp
    void bidiReorderLine(const BidiIterator &start, const BidiIterator &end, BidiState &bidi );
    RootInlineBox* determineStartPosition(bool fullLayout, BidiIterator &start, BidiState &bidi);
    RootInlineBox* determineEndPosition(RootInlineBox* startBox, BidiIterator& cleanLineStart,
                                        BidiStatus& cleanLineBidiStatus, BidiContext*& cleanLineBidiContext,
                                        int& yPos);
    bool matchedEndLine(const BidiIterator& start, const BidiStatus& status, BidiContext* context,
                        const BidiIterator& endLineStart, const BidiStatus& endLineStatus,
                        BidiContext* endLineContext, RootInlineBox*& endLine, int& endYPos);
    int skipWhitespace(BidiIterator &, BidiState &);
    BidiIterator findNextLineBreak(BidiIterator &start, BidiState &info );
    RootInlineBox* constructLine(const BidiIterator& start, const BidiIterator& end);
    InlineFlowBox* createLineBoxes(RenderObject* obj);
    int tabWidth(bool isWhitespacePre);
    void computeHorizontalPositionsForLine(RootInlineBox* lineBox, BidiState &bidi);
    void computeVerticalPositionsForLine(RootInlineBox* lineBox);
    void checkLinesForOverflow();
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    // end bidi.cpp functions
    
    virtual void paint(PaintInfo& i, int tx, int ty);
    virtual void paintObject(PaintInfo& i, int tx, int ty);
    void paintFloats(PaintInfo& i, int _tx, int _ty, bool paintSelection = false);
    void paintChildren(PaintInfo& i, int _tx, int _ty);
    void paintEllipsisBoxes(PaintInfo& i, int _tx, int _ty);
    void paintSelection(PaintInfo& i, int _tx, int _ty);
    void paintCaret(PaintInfo& i, CaretType);
    
    void insertFloatingObject(RenderObject *o);
    void removeFloatingObject(RenderObject *o);

    // called from lineWidth, to position the floats added in the last line.
    void positionNewFloats();
    void clearFloats();
    int getClearDelta(RenderObject *child);
    virtual void markAllDescendantsWithFloatsForLayout(RenderObject* floatToRemove = 0);
    void markPositionedObjectsForLayout();

    virtual bool containsFloats() { return m_floatingObjects!=0; }
    virtual bool containsFloat(RenderObject* o);

    virtual bool hasOverhangingFloats() { return floatBottom() > m_height; }
    void addIntrudingFloats(RenderBlock* prev, int xoffset, int yoffset);
    void addOverhangingFloats(RenderBlock* child, int xoffset, int yoffset);

    int nearestFloatBottom(int height) const;
    int floatBottom() const;
    inline int leftBottom();
    inline int rightBottom();
    virtual QRect floatRect() const;

    virtual int lineWidth(int y) const;
    virtual int lowestPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    virtual int rightmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    virtual int leftmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;

    int rightOffset() const;
    int rightRelOffset(int y, int fixedOffset, bool applyTextIndent = true,
                       int *heightRemaining = 0) const;
    int rightOffset(int y) const { return rightRelOffset(y, rightOffset(), true); }

    int leftOffset() const;
    int leftRelOffset(int y, int fixedOffset, bool applyTextIndent = true,
                      int *heightRemaining = 0) const;
    int leftOffset(int y) const { return leftRelOffset(y, leftOffset(), true); }

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty,
                             HitTestAction hitTestAction);

    bool isPointInScrollbar(int x, int y, int tx, int ty);

    virtual VisiblePosition positionForCoordinates(int x, int y);
    
    virtual void calcMinMaxWidth();
    void calcInlineMinMaxWidth();
    void calcBlockMinMaxWidth();

    virtual int getBaselineOfFirstLineBox() const;
    virtual int getBaselineOfLastLineBox() const;

    RootInlineBox* firstRootBox() const { return static_cast<RootInlineBox*>(m_firstLineBox); }
    RootInlineBox* lastRootBox() const { return static_cast<RootInlineBox*>(m_lastLineBox); }

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const;
    virtual void updateFirstLetter();
    
    bool inRootBlockContext() const;

    void setHasMarkupTruncation(bool b=true) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    virtual bool hasSelectedChildren() const { return m_selectionState != SelectionNone; }
    virtual SelectionState selectionState() const { return m_selectionState; }
    virtual void setSelectionState(SelectionState s);

    struct BlockSelectionInfo {
        RenderBlock* m_block;
        GapRects m_rects;
        SelectionState m_state;

        BlockSelectionInfo() { m_block = 0; m_state = SelectionNone; }
        BlockSelectionInfo(RenderBlock* b) { 
            m_block = b;
            m_state = m_block->selectionState();
            m_rects = m_block->selectionGapRects();
        }
        
        GapRects rects() const { return m_rects; }
        SelectionState state() const { return m_state; }
        RenderBlock* block() const { return m_block; }
    };
    
    virtual QRect selectionRect() { return selectionGapRects(); }
    GapRects selectionGapRects();
    virtual bool shouldPaintSelectionGaps() const;
    bool isSelectionRoot() const;
    GapRects fillSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, 
                               int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* i = 0);
    GapRects fillInlineSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                     int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* i);
    GapRects fillBlockSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                    int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* i);
    QRect fillVerticalSelectionGap(int lastTop, int lastLeft, int lastRight,
                                   int bottomY, RenderBlock* rootBlock, int blockX, int blockY, const PaintInfo* i);
    QRect fillLeftSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, const PaintInfo* i);
    QRect fillRightSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, const PaintInfo* i);
    QRect fillHorizontalSelectionGap(RenderObject* selObj, int xPos, int yPos, int width, int height, const PaintInfo* i);

    void getHorizontalSelectionGapInfo(SelectionState state, bool& leftGap, bool& rightGap);
    int leftSelectionOffset(RenderBlock* rootBlock, int y);
    int rightSelectionOffset(RenderBlock* rootBlock, int y);

#ifndef NDEBUG
    virtual void printTree(int indent=0) const;
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    // Helper methods for computing line counts and heights for line counts.
    RootInlineBox* lineAtIndex(int i);
    int lineCount();
    int heightForLineCount(int l);
    void clearTruncation();

protected:
    void newLine();

private:
    DOM::Position positionForBox(InlineBox *box, bool start=true) const;
    DOM::Position positionForRenderer(RenderObject *renderer, bool start=true) const;
    
protected:
    struct FloatingObject {
        enum Type {
            FloatLeft,
            FloatRight
        };

        FloatingObject(Type _type) {
            node = 0;
            startY = 0;
            endY = 0;
            type = _type;
            left = 0;
            width = 0;
            noPaint = false;
        }

        RenderObject* node;
        int startY;
        int endY;
        int left;
        int width;
        Type type : 1; // left or right aligned
        bool noPaint : 1;
    };
    
    // The following helper functions and structs are used by layoutBlockChildren.
    class CompactInfo {
        // A compact child that needs to be collapsed into the margin of the following block.
        RenderObject* m_compact;
        
        // The block with the open margin that the compact child is going to place itself within.
        RenderObject* m_block;

    public:
        RenderObject* compact() const { return m_compact; }
        RenderObject* block() const { return m_block; }
        bool matches(RenderObject* child) const { return m_compact && m_block == child; }
        
        void clear() { set(0, 0);  }
        void set(RenderObject* c, RenderObject* b) { m_compact = c; m_block = b; }
        
        CompactInfo() { clear(); }
    };

    class MarginInfo {
        // Collapsing flags for whether we can collapse our margins with our children's margins.
        bool m_canCollapseWithChildren : 1;
        bool m_canCollapseTopWithChildren : 1;
        bool m_canCollapseBottomWithChildren : 1;
        
        // Whether or not we are a quirky container, i.e., do we collapse away top and bottom
        // margins in our container.  Table cells and the body are the common examples. We
        // also have a custom style property for Safari RSS to deal with TypePad blog articles.
        bool m_quirkContainer : 1;

        // This flag tracks whether we are still looking at child margins that can all collapse together at the beginning of a block.  
        // They may or may not collapse with the top margin of the block (|m_canCollapseTopWithChildren| tells us that), but they will
        // always be collapsing with one another.  This variable can remain set to true through multiple iterations 
        // as long as we keep encountering self-collapsing blocks.
        bool m_atTopOfBlock : 1;

        // This flag is set when we know we're examining bottom margins and we know we're at the bottom of the block.
        bool m_atBottomOfBlock : 1;

        // If our last normal flow child was a self-collapsing block that cleared a float,
        // we track it in this variable.
        bool m_selfCollapsingBlockClearedFloat : 1;
    
        // These variables are used to detect quirky margins that we need to collapse away (in table cells
        // and in the body element).
        bool m_topQuirk : 1;
        bool m_bottomQuirk : 1;
        bool m_determinedTopQuirk : 1;

        // These flags track the previous maximal positive and negative margins.
        int m_posMargin;
        int m_negMargin;

    public:
        MarginInfo(RenderBlock* b, int top, int bottom);
        
        void setAtTopOfBlock(bool b) { m_atTopOfBlock = b; }
        void setAtBottomOfBlock(bool b) { m_atBottomOfBlock = b; }
        void clearMargin() { m_posMargin = m_negMargin = 0; }
        void setSelfCollapsingBlockClearedFloat(bool b) { m_selfCollapsingBlockClearedFloat = b; }
        void setTopQuirk(bool b) { m_topQuirk = b; }
        void setBottomQuirk(bool b) { m_bottomQuirk = b; }
        void setDeterminedTopQuirk(bool b) { m_determinedTopQuirk = b; }
        void setPosMargin(int p) { m_posMargin = p; }
        void setNegMargin(int n) { m_negMargin = n; }
        void setPosMarginIfLarger(int p) { if (p > m_posMargin) m_posMargin = p; }
        void setNegMarginIfLarger(int n) { if (n > m_negMargin) m_negMargin = n; }

        void setMargin(int p, int n) { m_posMargin = p; m_negMargin = n; }

        bool atTopOfBlock() const { return m_atTopOfBlock; }
        bool canCollapseWithTop() const { return m_atTopOfBlock && m_canCollapseTopWithChildren; }
        bool canCollapseWithBottom() const { return m_atBottomOfBlock && m_canCollapseBottomWithChildren; }
        bool canCollapseTopWithChildren() const { return m_canCollapseTopWithChildren; }
        bool canCollapseBottomWithChildren() const { return m_canCollapseBottomWithChildren; }
        bool selfCollapsingBlockClearedFloat() const { return m_selfCollapsingBlockClearedFloat; }
        bool quirkContainer() const { return m_quirkContainer; }
        bool determinedTopQuirk() const { return m_determinedTopQuirk; }
        bool topQuirk() const { return m_topQuirk; }
        bool bottomQuirk() const { return m_bottomQuirk; }
        int posMargin() const { return m_posMargin; }
        int negMargin() const { return m_negMargin; }
        int margin() const { return m_posMargin - m_negMargin; }
    };
    
    void adjustPositionedBlock(RenderObject* child, const MarginInfo& marginInfo);
    void adjustFloatingBlock(const MarginInfo& marginInfo);
    RenderObject* handleSpecialChild(RenderObject* child, const MarginInfo& marginInfo, CompactInfo& compactInfo, bool& handled);
    RenderObject* handleFloatingChild(RenderObject* child, const MarginInfo& marginInfo, bool& handled);
    RenderObject* handlePositionedChild(RenderObject* child, const MarginInfo& marginInfo, bool& handled);
    RenderObject* handleCompactChild(RenderObject* child, CompactInfo& compactInfo, bool& handled);
    RenderObject* handleRunInChild(RenderObject* child, bool& handled);
    void collapseMargins(RenderObject* child, MarginInfo& marginInfo, int yPosEstimate);
    void clearFloatsIfNeeded(RenderObject* child, MarginInfo& marginInfo, int oldTopPosMargin, int oldTopNegMargin);
    void insertCompactIfNeeded(RenderObject* child, CompactInfo& compactInfo);
    int estimateVerticalPosition(RenderObject* child, const MarginInfo& info);
    void determineHorizontalPosition(RenderObject* child);
    void handleBottomOfBlock(int top, int bottom, MarginInfo& marginInfo);
    void setCollapsedBottomMargin(const MarginInfo& marginInfo);
    // End helper functions and structs used by layoutBlockChildren.

protected:
    QPtrList<FloatingObject>* m_floatingObjects;
    QPtrList<RenderObject>* m_positionedObjects;
    
    bool m_childrenInline : 1;
    bool m_firstLine      : 1;
    EClear m_clearStatus  : 2;
    bool m_topMarginQuirk : 1;
    bool m_bottomMarginQuirk : 1;
    bool m_hasMarkupTruncation : 1;
    SelectionState m_selectionState : 3;

    int m_maxTopPosMargin;
    int m_maxTopNegMargin;
    int m_maxBottomPosMargin;
    int m_maxBottomNegMargin;

    // How much content overflows out of our block vertically or horizontally (all we support
    // for now is spillage out of the bottom and the right, which are the common cases).
    // XXX Generalize to work with top and left as well.
    int m_overflowHeight;
    int m_overflowWidth;
    
    // Left and top overflow.  Does not affect scrolling dimensions, but we do at least use it
    // when dirty rect checking and hit testing.
    int m_overflowLeft;
    int m_overflowTop;
    
    // full width of a tab character
    int m_tabWidth;
};

}; // namespace

#endif // RENDER_BLOCK_H
