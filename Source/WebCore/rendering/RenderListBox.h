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

#include "RenderBlockFlow.h"
#include "ScrollableArea.h"

namespace WebCore {

class HTMLSelectElement;

class RenderListBox final : public RenderBlockFlow, private ScrollableArea {
public:
    RenderListBox(HTMLSelectElement&, PassRef<RenderStyle>);
    virtual ~RenderListBox();

    HTMLSelectElement& selectElement() const;

    void selectionChanged();

    void setOptionsChanged(bool changed) { m_optionsChanged = changed; }

    int listIndexAtOffset(const LayoutSize&);
    LayoutRect itemBoundingBoxRect(const LayoutPoint&, int index);

    bool scrollToRevealElementAtListIndex(int index);
    bool listIndexIsVisible(int index);

    int scrollToward(const IntPoint&); // Returns the new index or -1 if no scroll occurred

    int size() const;

private:
    void element() const = delete;

    virtual const char* renderName() const override { return "RenderListBox"; }

    virtual bool isListBox() const override { return true; }

    virtual void updateFromElement() override;
    virtual bool canBeReplacedWithInlineRunIn() const override;
    virtual bool hasControlClip() const override { return true; }
    virtual void paintObject(PaintInfo&, const LayoutPoint&) override;
    virtual LayoutRect controlClipRect(const LayoutPoint&) const override;

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) override;

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = nullptr, RenderBox* startBox = nullptr, const IntPoint& wheelEventAbsolutePoint = IntPoint()) override;
    virtual bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = 0) override;

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    virtual void layout() override;

    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override;

    virtual bool canBeProgramaticallyScrolled() const override { return true; }
    virtual void autoscroll(const IntPoint&) override;
    virtual void stopAutoscroll() override;

    virtual bool shouldPanScroll() const { return true; }
    virtual void panScroll(const IntPoint&) override;

    virtual int verticalScrollbarWidth() const override;
    virtual int scrollLeft() const override;
    virtual int scrollTop() const override;
    virtual int scrollWidth() const override;
    virtual int scrollHeight() const override;
    virtual void setScrollLeft(int) override;
    virtual void setScrollTop(int) override;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    // ScrollableArea interface.
    virtual int scrollSize(ScrollbarOrientation) const override;
    virtual int scrollPosition(Scrollbar*) const override;
    virtual void setScrollOffset(const IntPoint&) override;
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) override;
    virtual bool isActive() const override;
    virtual bool isScrollCornerVisible() const override { return false; } // We don't support resize on list boxes yet. If we did these would have to change.
    virtual IntRect scrollCornerRect() const override { return IntRect(); }
    virtual void invalidateScrollCornerRect(const IntRect&) override { }
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const override;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const override;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const override;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const override;
    virtual Scrollbar* verticalScrollbar() const override { return m_vBar.get(); }
    virtual IntSize contentsSize() const override;
    virtual IntSize visibleSize() const override { return IntSize(height(), width()); }
    virtual IntPoint lastKnownMousePosition() const override;
    virtual bool isHandlingWheelEvent() const override;
    virtual bool shouldSuspendScrollAnimations() const override;
    virtual bool updatesScrollLayerPositionOnMainThread() const override { return true; }

    virtual ScrollableArea* enclosingScrollableArea() const override;
    virtual IntRect scrollableAreaBoundingBox() const override;

    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    void scrollTo(int newOffset);

    void setHasVerticalScrollbar(bool hasScrollbar);
    PassRefPtr<Scrollbar> createScrollbar();
    void destroyScrollbar();
    
    LayoutUnit itemHeight() const;
    void valueChanged(unsigned listIndex);
    int numVisibleItems() const;
    int numItems() const;
    LayoutUnit listHeight() const;
    void paintScrollbar(PaintInfo&, const LayoutPoint&);
    void paintItemForeground(PaintInfo&, const LayoutPoint&, int listIndex);
    void paintItemBackground(PaintInfo&, const LayoutPoint&, int listIndex);
    void scrollToRevealSelection();

    bool m_optionsChanged;
    bool m_scrollToRevealSelectionAfterLayout;
    bool m_inAutoscroll;
    int m_optionsWidth;
    int m_indexOffset;

    RefPtr<Scrollbar> m_vBar;
};

RENDER_OBJECT_TYPE_CASTS(RenderListBox, isListBox())

} // namepace WebCore

#endif // RenderListBox_h
