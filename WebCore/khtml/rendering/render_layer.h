/*
 * This file contains the implementation of the layering system for the compositing
 * of stacked layers (and for proper event handling with stacked layers).
 *
 * Copyright (C) 2002 (hyatt@apple.com)
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
 
#ifndef render_layer_h
#define render_layer_h

#include <qcolor.h>
#include <qrect.h>
#include <assert.h>

#include "misc/khtmllayout.h"
#include "misc/loader_client.h"
#include "misc/helper.h"
#include "rendering/render_style.h"

namespace khtml {
    class RenderFlow;
    class RenderStyle;
    class RenderTable;
    class CachedObject;
    class RenderRoot;
    class RenderText;
    class RenderFrameSet;
    class RenderObject;
    
class RenderLayer
{
public:
    RenderLayer(RenderObject* object);
    ~RenderLayer();
    
    RenderLayer *parent() const { return m_parent; }
    RenderLayer *previousSibling() const { return m_previous; }
    RenderLayer *nextSibling() const { return m_next; }

    RenderLayer *firstChild() const { return m_first; }
    RenderLayer *lastChild() const { return m_last; }

    void addChild(RenderLayer *newChild, RenderLayer *beforeChild = 0);
    RenderLayer* removeChild(RenderLayer *oldChild);

    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    short width() const { return m_width; }
    int height() const { return m_height; }
 
    void setWidth( int width ) { m_width = width; }
    void setHeight( int height ) { m_height = height; }
    
    void setPos( int xPos, int yPos ) { m_x = xPos; m_y = yPos; }
    
private:
    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer* parent) { m_parent = parent; }
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }
    
protected:   
    RenderObject* m_object;
    
    RenderLayer *m_parent;
    RenderLayer *m_previous;
    RenderLayer *m_next;

    RenderLayer *m_first;
    RenderLayer *m_last;
    
    // Our (x,y) coordinates are in our parent layer's coordinate space.
    int m_height;
    int m_y;
    short m_x;
    short m_width;
};

}; // namespace
#endif
