/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef RenderFlow_h
#define RenderFlow_h

#include "RenderContainer.h"

namespace WebCore {

/**
 * all geometry managing stuff is only in the block elements.
 *
 * Inline elements don't layout themselves, but the whole paragraph
 * gets flowed by the surrounding block element. This is, because
 * one needs to know the whole paragraph to calculate bidirectional
 * behaviour of text, so putting the layouting routines in the inline
 * elements is impossible.
 */
class RenderFlow : public RenderContainer {
public:
    RenderFlow(Node* node)
        : RenderContainer(node)
        , m_continuation(0)
        , m_firstLineBox(0)
        , m_lastLineBox(0)
        , m_lineHeight(-1)
        , m_childrenInline(true)
        , m_firstLine(false)
        , m_topMarginQuirk(false) 
        , m_bottomMarginQuirk(false)
        , m_hasMarkupTruncation(false)
        , m_selectionState(SelectionNone)
        , m_hasColumns(false)
        , m_isContinuation(false)
        , m_cellWidthChanged(false)
    {
    }
#ifndef NDEBUG
    virtual ~RenderFlow();
#endif

    virtual RenderFlow* virtualContinuation() const { return continuation(); }
    RenderFlow* continuation() const { return m_continuation; }
    void setContinuation(RenderFlow* c) { m_continuation = c; }
    RenderFlow* continuationBefore(RenderObject* beforeChild);

    void addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild);
    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild) = 0;
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);

    static RenderFlow* createAnonymousFlow(Document*, PassRefPtr<RenderStyle>);

    void extractLineBox(InlineFlowBox*);
    void attachLineBox(InlineFlowBox*);
    void removeLineBox(InlineFlowBox*);
    void deleteLineBoxes();
    virtual void destroy();

    virtual void dirtyLinesFromChangedChild(RenderObject* child);

    virtual int lineHeight(bool firstLine, bool isRootLineBox = false) const;

    InlineFlowBox* firstLineBox() const { return m_firstLineBox; }
    InlineFlowBox* lastLineBox() const { return m_lastLineBox; }

    virtual InlineBox* createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun=false);
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox = false);

    void paintLines(PaintInfo&, int tx, int ty);
    bool hitTestLines(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual IntRect absoluteClippedOverflowRect();

    virtual int lowestPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int rightmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int leftmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;

    virtual IntRect localCaretRect(InlineBox*, int caretOffset, int* extraWidthToEndOfLine = 0);

    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);
    void paintOutlineForLine(GraphicsContext*, int tx, int ty, const IntRect& prevLine, const IntRect& thisLine, const IntRect& nextLine);
    void paintOutline(GraphicsContext*, int tx, int ty);

    virtual bool hasColumns() const { return m_hasColumns; }

    void calcMargins(int containerWidth);

    void checkConsistency() const;

    IntRect linesBoundingBox() const;
    
    virtual IntRect borderBoundingBox() const
    {
        if (isInlineFlow()) {
            IntRect boundingBox = linesBoundingBox();
            return IntRect(0, 0, boundingBox.width(), boundingBox.height());
        }
        return borderBoxRect();
    }
    
private:
    // An inline can be split with blocks occurring in between the inline content.
    // When this occurs we need a pointer to our next object.  We can basically be
    // split into a sequence of inlines and blocks.  The continuation will either be
    // an anonymous block (that houses other blocks) or it will be an inline flow.
    RenderFlow* m_continuation;

protected:
    // For block flows, each box represents the root inline box for a line in the
    // paragraph.
    // For inline flows, each box represents a portion of that inline.
    InlineFlowBox* m_firstLineBox;
    InlineFlowBox* m_lastLineBox;

    mutable int m_lineHeight;
    
    // These bitfields are moved here from subclasses to pack them together
    // from RenderBlock
    bool m_childrenInline : 1;
    bool m_firstLine : 1;
    bool m_topMarginQuirk : 1;
    bool m_bottomMarginQuirk : 1;
    bool m_hasMarkupTruncation : 1;
    unsigned m_selectionState : 3; // SelectionState
    bool m_hasColumns : 1;
    
    // from RenderInline
    bool m_isContinuation : 1; // Whether or not we're a continuation of an inline.
    
    // from RenderTableCell
    bool m_cellWidthChanged : 1;
};

#ifdef NDEBUG
inline void RenderFlow::checkConsistency() const
{
}
#endif

} // namespace WebCore

#endif // RenderFlow_h
