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
#ifndef RENDER_BOX_H
#define RENDER_BOX_H

#include "render_container.h"
#include "misc/loader.h"
#include "render_layer.h"

namespace khtml {
    class CachedObject;
    
    enum WidthType { Width, MinWidth, MaxWidth };
    
class RenderBox : public RenderContainer
{


// combines ElemImpl & PosElImpl (all rendering objects are positioned)
// should contain all border and padding handling

public:
    RenderBox(DOM::NodeImpl* node);
    virtual ~RenderBox();

    virtual const char *renderName() const { return "RenderBox"; }

    virtual void setStyle(RenderStyle *style);
    virtual void paint(QPainter *p, int _x, int _y, int _w, int _h,
                       int _tx, int _ty, PaintAction paintAction);

    virtual void close();

    virtual void detach(RenderArena* renderArena);
    
    virtual short minWidth() const { return m_minWidth; }
    virtual short maxWidth() const { return m_maxWidth; }

    virtual short contentWidth() const;
    virtual int contentHeight() const;

    virtual bool absolutePosition(int &xPos, int &yPos, bool f = false);

    virtual void setPos( int xPos, int yPos );

    virtual int xPos() const { return m_x; }
    virtual int yPos() const { return m_y; }
    virtual short width() const;
    virtual int height() const;

    virtual short marginTop() const { return m_marginTop; }
    virtual short marginBottom() const { return m_marginBottom; }
    virtual short marginLeft() const { return m_marginLeft; }
    virtual short marginRight() const { return m_marginRight; }

    virtual void setWidth( int width ) { m_width = width; }
    virtual void setHeight( int height ) { m_height = height; }

    // This method is now public so that centered objects like tables that are
    // shifted right by left-aligned floats can recompute their left and
    // right margins (so that they can remain centered after being 
    // shifted. -dwh
    void calcHorizontalMargins(const Length& ml, const Length& mr, int cw);

    virtual void position(InlineBox* box, int from, int len, bool reverse);
    
    virtual int lowestPosition(bool includeOverflowInterior=true) const;
    virtual int rightmostPosition(bool includeOverflowInterior=true) const;

    virtual void repaint(bool immediate=false);

    virtual void repaintRectangle(int x, int y, int w, int h, bool immediate=false, bool f=false);

    virtual void setPixmap(const QPixmap &, const QRect&, CachedImage *);

    virtual short containingBlockWidth() const;

    virtual void calcWidth();
    virtual void calcHeight();

    int calcWidthUsing(WidthType widthType, int cw, LengthType& lengthType);
    
    virtual short calcReplacedWidth() const;
    virtual int   calcReplacedHeight() const;

    virtual int availableHeight() const;
    
    void calcVerticalMargins();

    void relativePositionOffset(int &tx, int &ty);

    virtual RenderLayer* layer() const { return m_layer; }

    virtual void paintBackgroundExtended(QPainter *p, const QColor &c, CachedImage *bg, int clipy, int cliph,
                                         int _tx, int _ty, int w, int height,
                                         int bleft, int bright);

    virtual void setStaticX(short staticX);
    virtual void setStaticY(int staticY);

protected:
    virtual void paintBoxDecorations(QPainter *p,int _x, int _y,
                                     int _w, int _h, int _tx, int _ty);
    void paintRootBoxDecorations(QPainter *p,int, int _y,
                                 int, int _h, int _tx, int _ty);

    void paintBackground(QPainter *p, const QColor &c, CachedImage *bg, int clipy, int cliph, int _tx, int _ty, int w, int h);
    void outlineBox(QPainter *p, int _tx, int _ty, const char *color = "red");

    virtual int borderTopExtra() { return 0; }
    virtual int borderBottomExtra() { return 0; }

    void calcAbsoluteHorizontal();
    void calcAbsoluteVertical();

    virtual QRect getOverflowClipRect(int tx, int ty);
    virtual QRect getClipRect(int tx, int ty);

    // Called to correct the z-index after the style has been changed.
    void adjustZIndex();
    
    // the actual height of the contents + borders + padding
    int m_height;

    int m_y;

    short m_x;
    short m_width;

    short m_marginTop;
    short m_marginBottom;

    short m_marginLeft;
    short m_marginRight;

    /*
     * the minimum width the element needs, to be able to render
     * it's content without clipping
     */
    short m_minWidth;
    /* The maximum width the element can fill horizontally
     * ( = the width of the element with line breaking disabled)
     */
    short m_maxWidth;

    // Cached normal flow values for absolute positioned elements with static left/top values.
    short m_staticX;
    int m_staticY;
    
    // A pointer to our layer if we have one.  Currently only positioned elements
    // and floaters have layers.
    RenderLayer* m_layer;
};


}; //namespace

#endif
