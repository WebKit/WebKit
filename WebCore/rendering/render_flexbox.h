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

#ifndef RENDER_FLEXIBLE_BOX_H
#define RENDER_FLEXIBLE_BOX_H

#include "RenderBlock.h"
#include "render_style.h"

namespace khtml {

class RenderFlexibleBox : public RenderBlock
{
public:
    RenderFlexibleBox(DOM::NodeImpl* node);
    virtual ~RenderFlexibleBox();

    virtual void calcMinMaxWidth();
    void calcHorizontalMinMaxWidth();
    void calcVerticalMinMaxWidth();

    virtual void layoutBlock(bool relayoutChildren);
    void layoutHorizontalBox(bool relayoutChildren);
    void layoutVerticalBox(bool relayoutChildren);

    virtual bool isFlexibleBox() const { return true; }
    virtual bool isFlexingChildren() const { return m_flexingChildren; }
    virtual bool isStretchingChildren() const { return m_stretchingChildren; }
    
    virtual const char *renderName() const;

    void placeChild(RenderObject* child, int x, int y);

protected:
    int allowedChildFlex(RenderObject* child, bool expanding, unsigned int group);

    bool hasMultipleLines() { return style()->boxLines() == MULTIPLE; }
    bool isVertical() { return style()->boxOrient() == VERTICAL; }
    bool isHorizontal() { return style()->boxOrient() == HORIZONTAL; }

    bool m_flexingChildren : 1;
    bool m_stretchingChildren : 1;
};

}; // namespace

#endif // RENDER_FLEXIBLE_BOX_H



