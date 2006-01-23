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

#include "RenderBlock.h"
#include <kxmlcore/HashSet.h>

class FrameView;
class QScrollView;

namespace WebCore {

class RenderCanvas : public RenderBlock
{
public:
    RenderCanvas(DOM::NodeImpl* node, FrameView *view);
    virtual ~RenderCanvas();

    virtual const char *renderName() const { return "RenderCanvas"; }

    virtual bool isCanvas() const { return true; }

    virtual void layout();
    virtual void calcWidth();
    virtual void calcHeight();
    virtual void calcMinMaxWidth();
    virtual bool absolutePosition(int &xPos, int&yPos, bool f = false);
    
    int docHeight() const;
    int docWidth() const;

    FrameView *view() const { return m_view; }

    virtual bool hasOverhangingFloats() { return false; }
    
    virtual IntRect getAbsoluteRepaintRect();
    virtual void computeAbsoluteRepaintRect(IntRect& r, bool f=false);
    virtual void repaintViewRectangle(const IntRect& r, bool immediate = false);
    
    virtual void paint(PaintInfo& i, int tx, int ty);
    virtual void paintBoxDecorations(PaintInfo& i, int _tx, int _ty);
    
    void setSelection(RenderObject *s, int sp, RenderObject *e, int ep);
    void clearSelection();
    virtual RenderObject *selectionStart() const { return m_selectionStart; }
    virtual RenderObject *selectionEnd() const { return m_selectionEnd; }

    void setPrintingMode(bool print) { m_printingMode = print; }
    bool printingMode() const { return m_printingMode; }
    void setPrintImages(bool enable) { m_printImages = enable; }
    bool printImages() const { return m_printImages; }
    void setTruncatedAt(int y) { m_truncatedAt = y; m_bestTruncatedAt = m_truncatorWidth = 0; m_forcedPageBreak = false; }
    void setBestTruncatedAt(int y, RenderObject *forRenderer, bool forcedBreak = false);
    int bestTruncatedAt() const { return m_bestTruncatedAt; }
private:
    int m_bestTruncatedAt;
    int m_truncatorWidth;
    bool m_forcedPageBreak;
public:
    int truncatedAt() const { return m_truncatedAt; }

    virtual void setWidth( int width ) { m_rootWidth = m_width = width; }
    virtual void setHeight( int height ) { m_rootHeight = m_height = height; }

    int viewportWidth() const { return m_viewportWidth; }
    int viewportHeight() const { return m_viewportHeight; }

    virtual void absoluteRects(QValueList<IntRect>& rects, int _tx, int _ty);
    
    IntRect selectionRect() const;
    
    void setMaximalOutlineSize(int o) { m_maximalOutlineSize = o; }
    int maximalOutlineSize() const { return m_maximalOutlineSize; }

    virtual IntRect viewRect() const;

    virtual void selectionStartEnd(int& spos, int& epos);

    IntRect printRect() const { return m_printRect; }
    void setPrintRect(const IntRect& r) { m_printRect = r; }

    void updateWidgetPositions();
    void addWidget(RenderObject *);
    void removeWidget(RenderObject *);

protected:

    FrameView *m_view;

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
    
    int m_maximalOutlineSize; // Used to apply a fudge factor to dirty-rect checks on blocks/tables.
    IntRect m_printRect; // Used when printing.

    typedef HashSet<RenderObject *, PointerHash<RenderObject *> > RenderObjectSet;

    RenderObjectSet m_widgets;
};

};
#endif
