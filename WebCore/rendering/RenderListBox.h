/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderListBox_h
#define RenderListBox_h

#include "RenderBlock.h"
#include "ScrollbarClient.h"

namespace WebCore {

class RenderListBox : public RenderBlock, private ScrollbarClient {
public:
    RenderListBox(Element*);
    virtual ~RenderListBox();

    void selectionChanged();

    void setOptionsChanged(bool changed) { m_optionsChanged = changed; }

    int listIndexAtOffset(int x, int y);
    IntRect itemBoundingBoxRect(int tx, int ty, int index);

    bool scrollToRevealElementAtListIndex(int index);
    bool listIndexIsVisible(int index);

    int scrollToward(const IntPoint&); // Returns the new index or -1 if no scroll occurred

private:
    virtual const char* renderName() const { return "RenderListBox"; }

    virtual bool isListBox() const { return true; }

    virtual void updateFromElement();

    virtual bool canHaveChildren() const { return false; }

    virtual bool hasControlClip() const { return true; }
    virtual void paintObject(PaintInfo&, int tx, int ty);
    virtual IntRect controlClipRect(int tx, int ty) const;

    virtual bool isPointInOverflowControl(HitTestResult&, int x, int y, int tx, int ty);

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Node** stopNode = 0);

    virtual void calcPrefWidths();
    virtual int baselinePosition(bool firstLine, bool isRootLineBox) const;
    virtual void calcHeight();

    virtual void layout();

    virtual bool canBeProgramaticallyScrolled(bool) const { return true; }
    virtual void autoscroll();
    virtual void stopAutoscroll();

    virtual bool shouldPanScroll() const { return true; }
    virtual void panScroll(const IntPoint&);

    virtual int verticalScrollbarWidth() const;
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    // ScrollbarClient interface.
    virtual int scrollSize(ScrollbarOrientation orientation) const;
    virtual void setScrollOffsetFromAnimation(const IntPoint&);
    virtual void valueChanged(Scrollbar*);
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
    virtual bool isActive() const;
    virtual bool scrollbarCornerPresent() const { return false; } // We don't support resize on list boxes yet.  If we did this would have to change.
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const;

    void setHasVerticalScrollbar(bool hasScrollbar);
    PassRefPtr<Scrollbar> createScrollbar();
    void destroyScrollbar();
    
    int itemHeight() const;
    void valueChanged(unsigned listIndex);
    int size() const;
    int numVisibleItems() const;
    int numItems() const;
    int listHeight() const;
    void paintScrollbar(PaintInfo&, int tx, int ty);
    void paintItemForeground(PaintInfo&, int tx, int ty, int listIndex);
    void paintItemBackground(PaintInfo&, int tx, int ty, int listIndex);
    void scrollToRevealSelection();

    bool m_optionsChanged;
    bool m_scrollToRevealSelectionAfterLayout;
    bool m_inAutoscroll;
    int m_optionsWidth;
    int m_indexOffset;

    RefPtr<Scrollbar> m_vBar;
};

inline RenderListBox* toRenderListBox(RenderObject* object)
{ 
    ASSERT(!object || object->isListBox());
    return static_cast<RenderListBox*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderListBox(const RenderListBox*);

} // namepace WebCore

#endif // RenderListBox_h
