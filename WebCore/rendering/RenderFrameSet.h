/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef RenderFrameSet_h
#define RenderFrameSet_h

#include "RenderContainer.h"

namespace WebCore {

class HTMLFrameSetElement;
class MouseEvent;

class RenderFrameSet : public RenderContainer {
public:
    RenderFrameSet(HTMLFrameSetElement*);
    virtual ~RenderFrameSet();

    virtual const char* renderName() const { return "RenderFrameSet"; }
    virtual bool isFrameSet() const { return true; }

    virtual void layout();
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual void paint(PaintInfo& paintInfo, int tx, int ty);
    
    bool userResize(MouseEvent*);

    bool isResizingRow() const;
    bool isResizingColumn() const;

    bool canResizeRow(const IntPoint&) const;
    bool canResizeColumn(const IntPoint&) const;

#ifndef NDEBUG
    virtual void dump(TextStream*, DeprecatedString ind = "") const;
#endif

private:
    static const int noSplit = -1;

    class GridAxis : Noncopyable {
    public:
        GridAxis();
        void resize(int);
        Vector<int> m_sizes;
        Vector<int> m_deltas;
        Vector<bool> m_isSplitResizable;
        int m_splitBeingResized;
        int m_splitResizeOffset;
    };

    inline HTMLFrameSetElement* frameSet() const;

    bool canResize(const IntPoint&) const;
    void setIsResizing(bool);

    void layOutAxis(GridAxis&, const Length*, int availableSpace);
    void findNonResizableSplits();
    void positionFrames();

    int splitPosition(const GridAxis&, int split) const;
    int hitTestSplit(const GridAxis&, int position) const;

    void startResizing(GridAxis&, int position);
    void continueResizing(GridAxis&, int position);

    void paintRowBorder(const PaintInfo& paintInfo, const IntRect& rect);
    void paintColumnBorder(const PaintInfo& paintInfo, const IntRect& rect);

    GridAxis m_rows;
    GridAxis m_cols;

    bool m_isResizing;
    bool m_isChildResizing;
};

} // namespace WebCore

#endif // RenderFrameSet_h
