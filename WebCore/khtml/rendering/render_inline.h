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

#ifndef RENDER_INLINE_H
#define RENDER_INLINE_H

#include "render_flow.h"

namespace khtml {

class RenderInline : public RenderFlow
{
public:
    RenderInline(DOM::NodeImpl* node);
    virtual ~RenderInline();

    virtual const char *renderName() const
    {
        if (isRelPositioned())
            return "Inline (Rel Positioned)";
        else if (isAnonymousBox())
            return "Inline (Anonymous)";
        return "Inline";
    };

    virtual bool isRenderInline() const { return true; }
    
    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild);
    void splitInlines(RenderFlow* fromBlock, RenderFlow* toBlock, RenderFlow* middleBlock,
                      RenderObject* beforeChild, RenderFlow* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderFlow* newBlockBox,
                   RenderObject* newChild, RenderFlow* oldCont);
    
    virtual void setStyle(RenderStyle* _style);

    virtual void layout() {} // Do nothing for layout()
    
    virtual void paint(QPainter *, int x, int y, int w, int h,
                       int tx, int ty, PaintAction paintAction);
    virtual void paintObject(QPainter *, int x, int y, int w, int h,
                             int tx, int ty, PaintAction paintAction);

    virtual void calcMinMaxWidth();

    // overrides RenderObject
    virtual bool requiresLayer() { return isRelPositioned(); }

    // used to calculate offsetWidth/Height.  Overridden by inlines (render_flow) to return
    // the remaining width on a given line (and the height of a single line).
    virtual short offsetWidth() const;
    virtual int offsetHeight() const;
    virtual int offsetLeft() const;
    virtual int offsetTop() const;    
};

}; // namespace

#endif // RENDER_BLOCK_H

