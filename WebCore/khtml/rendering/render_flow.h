/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#ifndef RENDER_FLOW_H
#define RENDER_FLOW_H

#include <qsortedlist.h>

#include "render_box.h"
#include "bidi.h"

namespace khtml {

/**
 * all geometry managing stuff is only in the block elements.
 *
 * Inline elements don't layout themselves, but the whole paragraph
 * gets layouted by the surrounding block element. This is, because
 * one needs to know the whole paragraph to calculate bidirectional
 * behaviour of text, so putting the layouting routines in the inline
 * elements is impossible.
 */
class RenderFlow : public RenderBox
{

public:
    RenderFlow(DOM::NodeImpl* node);

    virtual ~RenderFlow();

    virtual const char *renderName() const 
    {
        if (isFloating())
            return "Block (Floating)";
        if (isPositioned())
            return "Block (Positioned)";
        if (isAnonymousBox()) 
            return "Block (Anonymous)";
        if (isInline()) {
            if (isRelPositioned())
                return "Inline (Rel Positioned)";
            return "Inline";
        }
        if (isRelPositioned())
            return "Block (Rel Positioned)";
        return "Block";
    };
    
    virtual void setStyle(RenderStyle *style);

    virtual bool isFlow() const { return true; }
    virtual bool childrenInline() const { return m_childrenInline; }
    virtual void setChildrenInline(bool b) { m_childrenInline = b; }
    
    virtual bool isRendered() const { return true; }

    virtual RenderFlow* continuation() const { return m_continuation; }
    void setContinuation(RenderFlow* c) { m_continuation = c; }
    RenderFlow* continuationBefore(RenderObject* beforeChild);
    
    void splitInlines(RenderFlow* fromBlock, RenderFlow* toBlock, RenderFlow* middleBlock,
                      RenderObject* beforeChild, RenderFlow* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderFlow* newBlockBox, RenderFlow* oldCont);
    void addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild);
    void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild);
    void removeChild(RenderObject *oldChild);
    
    void makeChildrenNonInline(RenderObject *box2Start = 0);

    // overrides RenderObject

    virtual void paint(QPainter *, int x, int y, int w, int h,
                       int tx, int ty, int paintPhase);
    virtual void paintObject(QPainter *, int x, int y, int w, int h,
                             int tx, int ty, int paintPhase);
    void paintFloats(QPainter *p, int _x, int _y,
                     int _w, int _h, int _tx, int _ty);
                                       
    virtual void layout( );

    virtual void close();

    virtual void addChild(RenderObject *newChild, RenderObject *beforeChild = 0);

    virtual unsigned short lineWidth(int y) const;

    virtual int lowestPosition() const;
    virtual int rightmostPosition() const;

    int rightOffset() const;
    int rightRelOffset(int y, int fixedOffset, int *heightRemaining = 0) const;
    int rightOffset(int y) const { return rightRelOffset(y, rightOffset()); }

    int leftOffset() const;
    int leftRelOffset(int y, int fixedOffset, int *heightRemaining = 0) const;
    int leftOffset(int y) const { return leftRelOffset(y, leftOffset()); }

#ifndef NDEBUG
    virtual void printTree(int indent=0) const;
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, bool inside=false);

protected:

    virtual void newLine();

    void layoutBlockChildren( bool relayoutChildren );
    void layoutInlineChildren( bool relayoutChildren );
    void layoutSpecialObjects( bool relayoutChildren );

public:
    int floatBottom() const;
    inline int leftBottom();
    inline int rightBottom();
    bool checkClear(RenderObject *child);

    // used to calculate offsetWidth/Height.  Overridden by inlines (render_flow) to return
    // the remaining width on a given line (and the height of a single line).
    virtual short offsetWidth() const;
    virtual int offsetHeight() const;
    virtual int offsetLeft() const;
    virtual int offsetTop() const;
    
    void insertSpecialObject(RenderObject *o);

    // from BiDiParagraph
    virtual void closeParagraph() { positionNewFloats(); }

    void removeSpecialObject(RenderObject *o);
    // called from lineWidth, to position the floats added in the last line.
    void positionNewFloats();
    void clearFloats();
    virtual void calcMinMaxWidth();
    void calcInlineMinMaxWidth();
    void calcBlockMinMaxWidth();
    
    virtual bool containsSpecial() { return specialObjects!=0; }
    virtual bool hasOverhangingFloats() { return floatBottom() > m_height; }

    void addOverHangingFloats( RenderFlow *flow, int xoffset, int yoffset, bool child = false );

    // implementation of the following functions is in bidi.cpp
    void bidiReorderLine(const BidiIterator &start, const BidiIterator &end);
    BidiIterator findNextLineBreak(BidiIterator &start, QPtrList<BidiIterator>& midpoints);

    // The height (and width) of a block when you include overflow spillage out of the bottom
    // of the block (e.g., a <div style="height:25px"> that has a 100px tall image inside
    // it would have an overflow height of borderTop() + paddingTop() + 100px.
    virtual int overflowHeight() const { return m_overflowHeight; }
    virtual int overflowWidth() const { return m_overflowWidth; }
    
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

    QSortedList<SpecialObject>* specialObjects;

private:
    bool m_childrenInline : 1;
    bool m_pre            : 1;
    bool firstLine        : 1; // used in inline layouting
    EClear m_clearStatus  : 2; // used during layuting of paragraphs
     
    short m_maxTopPosMargin;
    short m_maxTopNegMargin;
    short m_maxBottomPosMargin;
    short m_maxBottomNegMargin;
    bool m_topMarginQuirk;
    bool m_bottomMarginQuirk;
    
    // How much content overflows out of our block vertically or horizontally (all we support
    // for now is spillage out of the bottom and the right, which are the common cases).
    // XXX Generalize to work with top and left as well.
    int m_overflowHeight;
    int m_overflowWidth;
    
    // An inline can be split with blocks occurring in between the inline content.
    // When this occurs we need a pointer to our next object.  We can basically be
    // split into a sequence of inlines and blocks.  The continuation will either be
    // an anonymous block (that houses other blocks) or it will be an inline flow.
    RenderFlow* m_continuation;
};

    
}; //namespace

#endif
