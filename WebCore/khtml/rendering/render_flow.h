/**
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
 * $Id$
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
    RenderFlow();

    virtual ~RenderFlow();

    virtual const char *renderName() const { return "RenderFlow"; }

    virtual void setStyle(RenderStyle *style);

    virtual bool isFlow() const { return true; }
    virtual bool childrenInline() const { return m_childrenInline; }
    virtual bool isRendered() const { return true; }

    bool haveAnonymousBox() const { return m_haveAnonymous; }
    void setHaveAnonymousBox(bool b = true) { m_haveAnonymous = b; }
    void makeChildrenNonInline();

    // overrides RenderObject

    virtual void print( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);
    virtual void printObject( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);
    void printSpecialObjects( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);

    virtual void layout( );

    virtual void close();

    virtual void addChild(RenderObject *newChild, RenderObject *beforeChild = 0);

    virtual void specialHandler(RenderObject */*special*/);

    virtual unsigned short lineWidth(int y) const;

    virtual int lowestPosition() const;
    virtual int rightmostPosition() const;

    int rightOffset() const;
    int rightRelOffset(int y, int fixedOffset, int *heightRemaining = 0) const;
    int rightOffset(int y) const { return rightRelOffset(y, rightOffset()); }

    int leftOffset() const;
    int leftRelOffset(int y, int fixedOffset, int *heightRemaining = 0) const;
    int leftOffset(int y) const { return leftRelOffset(y, leftOffset()); }

    virtual bool containsPoint(int _x, int _y, int _tx, int _ty);

    virtual void printTree(int indent=0) const;

protected:

    virtual void newLine();

    void layoutBlockChildren();
    void layoutInlineChildren();
    void layoutSpecialObjects();

public:
    int floatBottom() const;
    inline int leftBottom();
    inline int rightBottom();
    bool checkClear(RenderObject *child);

    // from BiDiParagraph
    virtual void closeParagraph() { positionNewFloats(); }

    void insertFloat(RenderObject *child);
    void insertPositioned(RenderObject *child);
    void removeSpecialObject(RenderObject *o);
    // called from lineWidth, to position the floats added in the last line.
    void positionNewFloats();
    void clearFloats();
    virtual void calcMinMaxWidth();

    virtual bool containsSpecial() { return specialObjects!=0; }
    virtual bool hasOverhangingFloats() { return floatBottom() > m_height; }

    void addOverHangingFloats( RenderFlow *flow, int xoffset, int yoffset, bool child = false );

    // implementation of the following functions is in bidi.cpp
    void appendRun(QList<BidiRun> &runs, BidiIterator &sor, BidiIterator &eor,
                   BidiContext *context, QChar::Direction dir);
    BidiContext *bidiReorderLine(BidiStatus &, const BidiIterator &start, const BidiIterator &end, BidiContext *startEmbed);
    BidiIterator findNextLineBreak(const BidiIterator &start);

public:
    RenderObject *first();
    RenderObject *next(RenderObject *current);
    RenderObject *nextObject(RenderObject *current);
protected:

    struct SpecialObject {
        SpecialObject() {
            count = 0;
            noPaint = false;
            startY = 0;
            endY = 0;
        }
        enum Type {
            FloatLeft,
            FloatRight,
            Positioned
         };

        RenderObject* node;
        int startY;
        int endY;
        Type type; // left or right aligned
        short left;
        short width;
        short count;
        bool noPaint;

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
    bool m_haveAnonymous  : 1;
    bool m_pre            : 1;
    bool firstLine        : 1; // used in inline layouting
    EClear m_clearStatus  : 2; // used during layuting of paragraphs
};


}; //namespace

#endif
