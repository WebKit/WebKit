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

#include "render_object.h"
#include "misc/loader.h"
#include "render_layer.h"

namespace khtml {
    class CachedObject;
    
    enum WidthType { Width, MinWidth, MaxWidth };
    enum HeightType { Height, MinHeight, MaxHeight };
    
class RenderBox : public RenderObject
{


// combines ElemImpl & PosElImpl (all rendering objects are positioned)
// should contain all border and padding handling

public:
    RenderBox(DOM::NodeImpl* node);
    virtual ~RenderBox();

    virtual const char *renderName() const { return "RenderBox"; }

    virtual void setStyle(RenderStyle *style);
    virtual void paint(PaintInfo& i, int _tx, int _ty);
    virtual bool nodeAtPoint(NodeInfo& i, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction);

    virtual void detach();
    
    virtual int minWidth() const { return m_minWidth; }
    virtual int maxWidth() const { return m_maxWidth; }

    virtual int contentWidth() const;
    virtual int contentHeight() const;

    virtual int overrideSize() const { return m_overrideSize; }
    virtual int overrideWidth() const;
    virtual int overrideHeight() const;
    virtual void setOverrideSize(int s) { m_overrideSize = s; }
    
    virtual bool absolutePosition(int &xPos, int &yPos, bool f = false);

    virtual void setPos( int xPos, int yPos );

    virtual int xPos() const { return m_x; }
    virtual int yPos() const { return m_y; }
    virtual int width() const;
    virtual int height() const;

    virtual int marginTop() const { return m_marginTop; }
    virtual int marginBottom() const { return m_marginBottom; }
    virtual int marginLeft() const { return m_marginLeft; }
    virtual int marginRight() const { return m_marginRight; }

    virtual void setWidth( int width ) { m_width = width; }
    virtual void setHeight( int height ) { m_height = height; }

    virtual QRect borderBox() const { return QRect(0, -borderTopExtra(), width(), height() + borderTopExtra() + borderBottomExtra()); }

    // This method is now public so that centered objects like tables that are
    // shifted right by left-aligned floats can recompute their left and
    // right margins (so that they can remain centered after being 
    // shifted. -dwh
    void calcHorizontalMargins(const Length& ml, const Length& mr, int cw);

    virtual void position(InlineBox* box, int from, int len, bool reverse);
    
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox=false);

    // For inline replaced elements, this function returns the inline box that owns us.  Enables
    // the replaced RenderObject to quickly determine what line it is contained on and to easily
    // iterate over structures on the line.
    virtual InlineBox* inlineBoxWrapper() const;
    virtual void setInlineBoxWrapper(InlineBox* b);
    void deleteLineBoxWrapper();
    
    virtual int lowestPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    virtual int rightmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;
    virtual int leftmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const;

    virtual QRect getAbsoluteRepaintRect();
    virtual void computeAbsoluteRepaintRect(QRect& r, bool f=false);

    virtual void repaintDuringLayoutIfMoved(int oldX, int oldY);
    
    virtual int containingBlockWidth() const;

    virtual void calcWidth();
    virtual void calcHeight();

    int calcWidthUsing(WidthType widthType, int cw, LengthType& lengthType);
    int calcHeightUsing(const Length& height);
    int calcReplacedWidthUsing(WidthType widthType) const;
    int calcReplacedHeightUsing(HeightType heightType) const;
    
    virtual int calcReplacedWidth() const;
    virtual int calcReplacedHeight() const;

    int calcPercentageHeight(const Length& height);

    virtual int availableHeight() const;
    int availableHeightUsing(const Length& h) const;
    
    void calcVerticalMargins();

    void relativePositionOffset(int &tx, int &ty);

    virtual RenderLayer* layer() const { return m_layer; }
    
    virtual QRect caretRect(int offset, EAffinity affinity = UPSTREAM, int *extraWidthToEndOfLine = 0);

    virtual void paintBackgroundExtended(QPainter *p, const QColor& c, const BackgroundLayer* bgLayer, int clipy, int cliph,
                                         int _tx, int _ty, int w, int height,
                                         int bleft, int bright);

    virtual void setStaticX(int staticX);
    virtual void setStaticY(int staticY);
    virtual int staticX() const { return m_staticX; }
    virtual int staticY() const { return m_staticY; }

protected:
    virtual void paintBoxDecorations(PaintInfo& i, int _tx, int _ty);
    void paintRootBoxDecorations(PaintInfo& i, int _tx, int _ty);

    void paintBackgrounds(QPainter *p, const QColor& c, const BackgroundLayer* bgLayer, int clipy, int cliph, int _tx, int _ty, int w, int h);
    void paintBackground(QPainter *p, const QColor& c, const BackgroundLayer* bgLayer, int clipy, int cliph, int _tx, int _ty, int w, int h);
    void outlineBox(QPainter *p, int _tx, int _ty, const char *color = "red");

    virtual int borderTopExtra() const { return 0; }
    virtual int borderBottomExtra() const { return 0; }

    void calcAbsoluteHorizontal();
    void calcAbsoluteVertical();
    void calcAbsoluteHorizontalValues(WidthType widthType, RenderObject* cb, int cw, int pab, int static_distance,
                                      int l, int r, int& w, int& ml, int& mr, int& x);
    void calcAbsoluteVerticalValues(HeightType heightType, RenderObject* cb, int ch, int pab, 
                                    int t, int b, int& h, int& mt, int& mb, int& y); 
    
    virtual QRect getOverflowClipRect(int tx, int ty);
    virtual QRect getClipRect(int tx, int ty);

    // the actual height of the contents + borders + padding
    int m_height;

    int m_y;

    int m_x;
    int m_width;

    int m_marginTop;
    int m_marginBottom;

    int m_marginLeft;
    int m_marginRight;

    /*
     * the minimum width the element needs, to be able to render
     * it's content without clipping
     */
    int m_minWidth;
    /* The maximum width the element can fill horizontally
     * ( = the width of the element with line breaking disabled)
     */
    int m_maxWidth;

    // Used by flexible boxes when flexing this element.
    int m_overrideSize;

    // Cached normal flow values for absolute positioned elements with static left/top values.
    int m_staticX;
    int m_staticY;
    
    // A pointer to our layer if we have one.  Currently only positioned elements
    // and floaters have layers.
    RenderLayer* m_layer;
    
    // For inline replaced elements, the inline box that owns us.
    InlineBox* m_inlineBoxWrapper;
};


}; //namespace

#endif
