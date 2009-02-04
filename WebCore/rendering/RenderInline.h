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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderInline_h
#define RenderInline_h

#include "RenderContainer.h"
#include "RenderLineBoxList.h"

namespace WebCore {

class Position;

class RenderInline : public RenderContainer {
public:
    RenderInline(Node*);
    virtual ~RenderInline();

    virtual void destroy();

    virtual const char* renderName() const;

    virtual bool isRenderInline() const { return true; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild = 0);

    void splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                      RenderObject* beforeChild, RenderContainer* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderContainer* oldCont);

    virtual void layout() { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    virtual void paint(PaintInfo&, int tx, int ty);

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual bool requiresLayer() const { return isRelPositioned() || isTransparent() || hasMask(); }

    virtual int offsetLeft() const;
    virtual int offsetTop() const;
    virtual int offsetWidth() const { return linesBoundingBox().width(); }
    virtual int offsetHeight() const { return linesBoundingBox().height(); }

    virtual void absoluteRects(Vector<IntRect>&, int tx, int ty, bool topLevel = true);
    virtual void absoluteQuads(Vector<FloatQuad>&, bool topLevel = true);

    virtual IntRect clippedOverflowRectForRepaint(RenderBox* repaintContainer);
    virtual IntRect rectWithOutlineForRepaint(RenderBox* repaintContainer, int outlineWidth);

    virtual VisiblePosition positionForCoordinates(int x, int y);

    IntRect linesBoundingBox() const;
    
    virtual IntRect borderBoundingBox() const
    {
        IntRect boundingBox = linesBoundingBox();
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

    virtual InlineBox* createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun=false);    
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox = false);
    virtual void dirtyLinesFromChangedChild(RenderObject* child) { m_lineBoxes.dirtyLinesFromChangedChild(this, child); }

    RenderLineBoxList* lineBoxes() { return &m_lineBoxes; }
    const RenderLineBoxList* lineBoxes() const { return &m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }

    virtual int lineHeight(bool firstLine, bool isRootLineBox = false) const;

    RenderContainer* continuation() const { return m_continuation; }
    RenderInline* inlineContinuation() const;
    void setContinuation(RenderContainer* c) { m_continuation = c; }
    
    virtual void updateDragState(bool dragOn);
    
    virtual void childBecameNonInline(RenderObject* child);

    virtual void updateHitTestResult(HitTestResult&, const IntPoint&);

    IntSize relativePositionedInlineOffset(const RenderObject* child) const;

    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);
    void paintOutline(GraphicsContext*, int tx, int ty);

    void calcMargins(int containerWidth)
    {
        m_marginLeft = style()->marginLeft().calcMinValue(containerWidth);
        m_marginRight = style()->marginRight().calcMinValue(containerWidth);
    }
    
protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    static RenderInline* cloneInline(RenderInline* src);

private:
    void paintOutlineForLine(GraphicsContext*, int tx, int ty, const IntRect& prevLine, const IntRect& thisLine, const IntRect& nextLine);
    RenderContainer* continuationBefore(RenderObject* beforeChild);

protected:
    RenderLineBoxList m_lineBoxes;   // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.

private:
    RenderContainer* m_continuation; // Can be either a block or an inline. <b><i><p>Hello</p></i></b>. In this example the <i> will have a block as its continuation but the
                                     // <b> will just have an inline as its continuation.
    mutable int m_lineHeight;
};

inline RenderInline* toRenderInline(RenderObject* o)
{ 
    ASSERT(!o || o->isRenderInline());
    return static_cast<RenderInline*>(o);
}

inline const RenderInline* toRenderInline(const RenderObject* o)
{ 
    ASSERT(!o || o->isRenderInline());
    return static_cast<const RenderInline*>(o);
}

// This will catch anyone doing an unnecessary cast.
void toRenderInline(const RenderInline*);

} // namespace WebCore

#endif // RenderInline_h
