/*
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef render_canvas_h
#define render_canvas_h

#include "render_block.h"

class KHTMLView;
class QScrollView;

namespace khtml {

class RenderCanvas : public RenderBlock
{
public:
    RenderCanvas(DOM::NodeImpl* node, KHTMLView *view);
    virtual ~RenderCanvas();

    virtual const char *renderName() const { return "RenderCanvas"; }

    virtual bool isCanvas() const { return true; }

    virtual void layout();
    virtual void calcWidth();
    virtual void calcHeight();
    virtual void calcMinMaxWidth();
    virtual bool absolutePosition(int &xPos, int&yPos, bool f = false);
    virtual void close();

    int docHeight() const;
    int docWidth() const;

    KHTMLView *view() const { return m_view; }

    virtual void repaint(bool immediate=false);
    virtual void repaintRectangle(int x, int y, int w, int h, bool immediate = false, bool f=false);
    virtual void paint(QPainter *, int x, int y, int w, int h, int tx, int ty,
                       PaintAction paintAction);
    void paintObject(QPainter *p, int _x, int _y,
                     int _w, int _h, int _tx, int _ty, PaintAction paintAction);

    virtual void paintBoxDecorations(QPainter *p,int _x, int _y,
                                     int _w, int _h, int _tx, int _ty);
    
    virtual void setSelection(RenderObject *s, int sp, RenderObject *e, int ep);
    virtual void clearSelection(bool doRepaint=true);
    virtual RenderObject *selectionStart() const { return m_selectionStart; }
    virtual RenderObject *selectionEnd() const { return m_selectionEnd; }

    void setPrintingMode(bool print) { m_printingMode = print; }
    bool printingMode() const { return m_printingMode; }
    void setPrintImages(bool enable) { m_printImages = enable; }
    bool printImages() const { return m_printImages; }
#if APPLE_CHANGES
    void setTruncatedAt(int y) { m_truncatedAt = y; m_bestTruncatedAt = m_truncatorWidth = 0; }
    void setBestTruncatedAt(int y, RenderObject *forRenderer);
    int bestTruncatedAt() const { return m_bestTruncatedAt; }
private:
    int m_bestTruncatedAt;
    int m_truncatorWidth;
public:
#else
    void setTruncatedAt(int y) { m_truncatedAt = y; }
#endif
    int truncatedAt() const { return m_truncatedAt; }

    virtual void setWidth( int width ) { m_rootWidth = m_width = width; }
    virtual void setHeight( int height ) { m_rootHeight = m_height = height; }

    int viewportWidth() const { return m_viewportWidth; }
    int viewportHeight() const { return m_viewportHeight; }

    QRect selectionRect() const;
    
protected:

    virtual void selectionStartEnd(int& spos, int& epos);

    virtual QRect viewRect() const;

    KHTMLView *m_view;

    RenderObject* m_selectionStart;
    RenderObject* m_selectionEnd;
    int m_selectionStartPos;
    int m_selectionEndPos;

    int m_rootWidth;
    int m_rootHeight;

    int m_viewportWidth;
    int m_viewportHeight;
    
    // used to ignore viewport width when printing to the printer
    bool m_printingMode;
    bool m_printImages;
    int m_truncatedAt;
};

};
#endif
