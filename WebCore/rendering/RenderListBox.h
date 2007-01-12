/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006 Apple Computer
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

#ifndef RenderListBox_h
#define RenderListBox_h

#include "RenderBlock.h"
#include "ScrollBar.h"

namespace WebCore {

class HTMLSelectElement;

class RenderListBox : public RenderBlock, public ScrollbarClient {
public:
    RenderListBox(HTMLSelectElement*);
    ~RenderListBox();

    virtual const char* renderName() const { return "RenderListBox"; }

    virtual bool isListBox() const { return true; }

    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();
    void setSelectionChanged(bool selectionChanged) { m_selectionChanged = selectionChanged; }

    virtual bool canHaveChildren() const { return false; }

    virtual bool hasControlClip() const { return true; }
    virtual void paintObject(PaintInfo&, int tx, int ty);
    virtual IntRect controlClipRect(int /*tx*/, int /*ty*/) const;

    virtual bool isPointInScrollbar(HitTestResult&, int x, int y, int tx, int ty);

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);

    virtual void calcMinMaxWidth();
    virtual short baselinePosition(bool firstLine, bool isRootLineBox) const;
    virtual void calcHeight();
    void setOptionsChanged(bool c) { m_optionsChanged = c; }
    void valueChanged(unsigned listIndex);

    // ScrollbarClient interface.
    virtual void valueChanged(Scrollbar*);
    virtual IntRect windowClipRect() const;

    int listIndexAtOffset(int x, int y);

    int itemHeight() const;

    bool scrollToRevealElementAtListIndex(int index);

    virtual bool shouldAutoscroll() const { return true; }
    virtual void autoscroll();
    virtual void stopAutoscroll();

    virtual int verticalScrollbarWidth() const;
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);

private:
    int size() const;
    int numItems() const;
    IntRect itemBoundingBoxRect(int tx, int ty, int index);
    void paintScrollbar(PaintInfo&);
    void paintItemForeground(PaintInfo&, int tx, int ty, int listIndex);
    void paintItemBackground(PaintInfo&, int tx, int ty, int listIndex);
    bool listIndexIsVisible(int index);

    bool m_optionsChanged;
    int m_optionsWidth;
    int m_indexOffset;
    bool m_selectionChanged;

    Scrollbar* m_vBar;
};

} // namepace WebCore

#endif // RenderListBox_h
