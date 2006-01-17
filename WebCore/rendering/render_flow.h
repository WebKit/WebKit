/*
 * This file is part of the DOM implementation for KDE.
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
#ifndef RENDER_FLOW_H
#define RENDER_FLOW_H

#include "render_container.h"
#include "bidi.h"
#include "render_line.h"

namespace khtml {

/**
 * all geometry managing stuff is only in the block elements.
 *
 * Inline elements don't layout themselves, but the whole paragraph
 * gets flowed by the surrounding block element. This is, because
 * one needs to know the whole paragraph to calculate bidirectional
 * behaviour of text, so putting the layouting routines in the inline
 * elements is impossible.
 */
class RenderFlow : public RenderContainer
{
public:
    RenderFlow(DOM::NodeImpl* node)
      : RenderContainer(node), m_lineHeight(-1)
    { m_continuation = 0; m_firstLineBox = 0; m_lastLineBox = 0;}

    virtual RenderFlow* continuation() const { return m_continuation; }
    void setContinuation(RenderFlow* c) { m_continuation = c; }
    RenderFlow* continuationBefore(RenderObject* beforeChild);
    
    void addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild);
    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild) = 0;
    virtual void addChild(RenderObject *newChild, RenderObject *beforeChild = 0);

    static RenderFlow* createAnonymousFlow(DOM::DocumentImpl* doc, RenderStyle* style);

    void extractLineBox(InlineFlowBox* lineBox);
    void attachLineBox(InlineFlowBox* lineBox);
    void removeLineBox(InlineFlowBox* lineBox);
    void deleteLineBoxes();
    virtual void destroy();

    virtual void dirtyLinesFromChangedChild(RenderObject* child);
    
    virtual short lineHeight(bool firstLine, bool isRootLineBox=false) const;
    
    InlineFlowBox* firstLineBox() const { return m_firstLineBox; }
    InlineFlowBox* lastLineBox() const { return m_lastLineBox; }

    virtual InlineBox* createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun=false);
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox = false);
    
    void paintLines(PaintInfo& i, int _tx, int _ty);
    bool hitTestLines(NodeInfo& i, int x, int y, int tx, int ty, HitTestAction hitTestAction);

    virtual IntRect getAbsoluteRepaintRect();
    
    virtual int lowestPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    virtual int rightmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    virtual int leftmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    
    virtual IntRect caretRect(int offset, EAffinity affinity = UPSTREAM, int *extraWidthToEndOfLine = 0);

    virtual void addFocusRingRects(QPainter *painter, int _tx, int _ty);
    void paintFocusRing(QPainter *p, int tx, int ty);
    void paintOutlineForLine(QPainter *p, int tx, int ty, const IntRect &prevLine, const IntRect &thisLine, const IntRect &nextLine);
    void paintOutlines(QPainter *p, int tx, int ty);

protected:
    // An inline can be split with blocks occurring in between the inline content.
    // When this occurs we need a pointer to our next object.  We can basically be
    // split into a sequence of inlines and blocks.  The continuation will either be
    // an anonymous block (that houses other blocks) or it will be an inline flow.
    RenderFlow* m_continuation;

    // For block flows, each box represents the root inline box for a line in the
    // paragraph.
    // For inline flows, each box represents a portion of that inline.
    InlineFlowBox* m_firstLineBox;
    InlineFlowBox* m_lastLineBox;
    
    mutable short m_lineHeight;
};

    
}; //namespace

#endif
