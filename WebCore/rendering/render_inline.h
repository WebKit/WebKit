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

#ifndef RENDER_INLINE_H
#define RENDER_INLINE_H

#include "render_flow.h"

namespace DOM {
    class Position;
}

namespace khtml {

class RenderInline : public RenderFlow
{
public:
    RenderInline(DOM::NodeImpl* node);
    virtual ~RenderInline();

    virtual const char *renderName() const;

    virtual bool isRenderInline() const { return true; }
    virtual bool isInlineFlow() const { return true; }
    virtual bool childrenInline() const { return true; }

    virtual bool isInlineContinuation() const;
    
    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild);
    void splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                      RenderObject* beforeChild, RenderFlow* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderFlow* oldCont);
    
    virtual void setStyle(RenderStyle* _style);

    virtual void layout() {} // Do nothing for layout()
    
    virtual void paint(PaintInfo& i, int tx, int ty);

    virtual bool nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                             HitTestAction hitTestAction);
    
    virtual void calcMinMaxWidth();

    // overrides RenderObject
    virtual bool requiresLayer();

    virtual int width() const;
    virtual int height() const;
    
    // used to calculate offsetWidth/Height.  Overridden by inlines (render_flow) to return
    // the remaining width on a given line (and the height of a single line).
    virtual int offsetLeft() const;
    virtual int offsetTop() const;

    void absoluteRects(QValueList<IntRect>& rects, int _tx, int _ty);

    virtual VisiblePosition positionForCoordinates(int x, int y);
    
protected:
    static RenderInline* cloneInline(RenderFlow* src);

private:
    bool m_isContinuation : 1; // Whether or not we're a continuation of an inline.
};

}; // namespace

#endif // RENDER_BLOCK_H

