/*
 * This file is part of the render object implementation for KHTML.
 *
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

#include <qsortedlist.h>

#include "render_flow.h"

namespace khtml {

class RenderBlock : public RenderFlow
{
public:
    RenderBlock(DOM::NodeImpl* node);
    virtual ~RenderBlock();

    virtual const char *renderName() const;

    virtual bool isRenderBlock() const { return true; }
    virtual bool containsSpecial() { return m_specialObjects!=0; }
    
    virtual bool childrenInline() const { return m_childrenInline; }
    virtual void setChildrenInline(bool b) { m_childrenInline = b; }
    void makeChildrenNonInline(RenderObject* insertionPoint = 0);

    // The height (and width) of a block when you include overflow spillage out of the bottom
    // of the block (e.g., a <div style="height:25px"> that has a 100px tall image inside
    // it would have an overflow height of borderTop() + paddingTop() + 100px.
    virtual int overflowHeight() const { return m_overflowHeight; }
    virtual int overflowWidth() const { return m_overflowWidth; }
    virtual void setOverflowHeight(int h) { m_overflowHeight = h; }
    virtual void setOverflowWidth(int w) { m_overflowWidth = w; }

    virtual bool isSelfCollapsingBlock() const { return m_height == 0; }
    virtual bool isTopMarginQuirk() const { return m_topMarginQuirk; }
    virtual bool isBottomMarginQuirk() const { return m_bottomMarginQuirk; }

    virtual short maxTopMargin(bool positive) const {
        if (positive)
            return m_maxTopPosMargin;
        else
            return m_maxTopNegMargin;
    }
    virtual short maxBottomMargin(bool positive) const {
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
        
    virtual void setStyle(RenderStyle* _style);
    
    virtual void layout();
    void layoutBlockChildren( bool relayoutChildren );
    void layoutInlineChildren( bool relayoutChildren );
    void layoutSpecialObjects( bool relayoutChildren );

    // the implementation of the following functions is in bidi.cpp
    void bidiReorderLine(const BidiIterator &start, const BidiIterator &end);
    BidiIterator findNextLineBreak(BidiIterator &start);
    InlineFlowBox* constructLine(const BidiIterator& start, const BidiIterator& end);
    InlineFlowBox* createLineBoxes(RenderObject* obj);
    void computeHorizontalPositionsForLine(InlineFlowBox* lineBox, BidiContext* endEmbed);
    void computeVerticalPositionsForLine(InlineFlowBox* lineBox);
    // end bidi.cpp functions
    
    virtual void paint(QPainter *, int x, int y, int w, int h,
                       int tx, int ty, PaintAction paintAction);
    virtual void paintObject(QPainter *, int x, int y, int w, int h,
                             int tx, int ty, PaintAction paintAction);
    void paintFloats(QPainter *p, int _x, int _y,
                     int _w, int _h, int _tx, int _ty, bool paintSelection = false);
    
    
    void insertSpecialObject(RenderObject *o);
    void removeSpecialObject(RenderObject *o);

    // called from lineWidth, to position the floats added in the last line.
    void positionNewFloats();
    void clearFloats();
    bool checkClear(RenderObject *child);
    virtual bool hasOverhangingFloats() { return floatBottom() > m_height; }
    void addOverHangingFloats( RenderBlock *block, int xoffset, int yoffset, bool child = false );
    
    int nearestFloatBottom(int height) const;
    int floatBottom() const;
    inline int leftBottom();
    inline int rightBottom();

    virtual unsigned short lineWidth(int y) const;
    virtual int lowestPosition() const;
    virtual int rightmostPosition() const;

    int rightOffset() const;
    int rightRelOffset(int y, int fixedOffset, int *heightRemaining = 0) const;
    int rightOffset(int y) const { return rightRelOffset(y, rightOffset()); }

    int leftOffset() const;
    int leftRelOffset(int y, int fixedOffset, int *heightRemaining = 0) const;
    int leftOffset(int y) const { return leftRelOffset(y, leftOffset()); }

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, bool inside=false);

    virtual void calcMinMaxWidth();
    void calcInlineMinMaxWidth();
    void calcBlockMinMaxWidth();

    virtual void close();

    virtual int getBaselineOfFirstLineBox();
    virtual InlineFlowBox* getFirstLineBox();
    
    // overrides RenderObject
    virtual bool requiresLayer() { return !isTableCell() &&
        (isPositioned() || isRelPositioned() || style()->overflow()==OHIDDEN); }
    
#ifndef NDEBUG
    virtual void printTree(int indent=0) const;
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif
    
protected:
    void newLine();
    
protected:
    struct SpecialObject {
        enum Type {
            FloatLeft,
            FloatRight,
            Positioned
        };

        SpecialObject(Type _type) {
            node = 0;
            startY = 0;
            endY = 0;
            type = _type;
            left = 0;
            width = 0;
            count = 0;
            noPaint = false;
        }

        RenderObject* node;
        int startY;
        int endY;
        short left;
        short width;
        short count;
        Type type : 2; // left or right aligned
        bool noPaint: 1;

        bool operator==(const SpecialObject& ) const
        {
            return false;
        }
        bool operator<(const SpecialObject& o) const
        {
            if(node->style()->zIndex() == o.node->style()->zIndex())
                return count < o.count;
            return node->style()->zIndex() < o.node->style()->zIndex();
        }
    };
    
protected:
    QSortedList<SpecialObject>* m_specialObjects;
    bool m_childrenInline : 1;
    bool m_pre            : 1;
    bool m_firstLine      : 1; // used in inline layouting
    EClear m_clearStatus  : 2; // used during layuting of paragraphs
    bool m_topMarginQuirk : 1;
    bool m_bottomMarginQuirk : 1;

    short m_maxTopPosMargin;
    short m_maxTopNegMargin;
    short m_maxBottomPosMargin;
    short m_maxBottomNegMargin;

    // How much content overflows out of our block vertically or horizontally (all we support
    // for now is spillage out of the bottom and the right, which are the common cases).
    // XXX Generalize to work with top and left as well.
    int m_overflowHeight;
    int m_overflowWidth;    
};

}; // namespace

#endif // RENDER_BLOCK_H

