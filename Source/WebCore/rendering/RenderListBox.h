/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007, 2009, 2014 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include "RenderBlockFlow.h"
#include "ScrollableArea.h"

namespace WebCore {

class HTMLSelectElement;

class RenderListBox final : public RenderBlockFlow, public ScrollableArea {
    WTF_MAKE_ISO_ALLOCATED(RenderListBox);
public:
    RenderListBox(HTMLSelectElement&, RenderStyle&&);
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

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = nullptr, RenderBox* startBox = nullptr, const IntPoint& wheelEventAbsolutePoint = IntPoint()) override;

    bool scrolledToTop() const final;
    bool scrolledToBottom() const final;
    bool scrolledToLeft() const final;
    bool scrolledToRight() const final;

private:
    void willBeDestroyed() override;

    void element() const = delete;

    const char* renderName() const override { return "RenderListBox"; }

    bool isListBox() const override { return true; }

    void updateFromElement() override;
    bool hasControlClip() const override { return true; }
    void paintObject(PaintInfo&, const LayoutPoint&) override;
    LayoutRect controlClipRect(const LayoutPoint&) const override;

    bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) override;

    bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = nullptr) override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;
    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    void layout() override;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = nullptr) override;

    bool canBeProgramaticallyScrolled() const override { return true; }
    void autoscroll(const IntPoint&) override;
    void stopAutoscroll() override;

    virtual bool shouldPanScroll() const { return true; }
    void panScroll(const IntPoint&) override;

    int verticalScrollbarWidth() const override;
    int scrollLeft() const override;
    int scrollTop() const override;
    int scrollWidth() const override;
    int scrollHeight() const override;
    void setScrollLeft(int, const ScrollPositionChangeOptions&) override;
    void setScrollTop(int, const ScrollPositionChangeOptions&) override;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    // ScrollableArea interface.
    void setScrollOffset(const ScrollOffset&) final;

    ScrollPosition scrollPosition() const final;
    ScrollPosition minimumScrollPosition() const final;
    ScrollPosition maximumScrollPosition() const final;

    void invalidateScrollbarRect(Scrollbar&, const IntRect&) final;
    bool isActive() const final;
    bool isScrollCornerVisible() const final { return false; } // We don't support resize on list boxes yet. If we did these would have to change.
    IntRect scrollCornerRect() const final { return IntRect(); }
    void invalidateScrollCornerRect(const IntRect&) final { }
    IntRect convertFromScrollbarToContainingView(const Scrollbar&, const IntRect&) const final;
    IntRect convertFromContainingViewToScrollbar(const Scrollbar&, const IntRect&) const final;
    IntPoint convertFromScrollbarToContainingView(const Scrollbar&, const IntPoint&) const final;
    IntPoint convertFromContainingViewToScrollbar(const Scrollbar&, const IntPoint&) const final;
    Scrollbar* verticalScrollbar() const final { return m_vBar.get(); }
    IntSize contentsSize() const final;
    IntSize visibleSize() const final { return IntSize(width(), height()); }
    IntPoint lastKnownMousePositionInView() const final;
    bool isHandlingWheelEvent() const final;
    bool shouldSuspendScrollAnimations() const final;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;

    ScrollableArea* enclosingScrollableArea() const final;
    bool isScrollableOrRubberbandable() final;
    bool hasScrollableOrRubberbandableAncestor() final;
    IntRect scrollableAreaBoundingBox(bool* = nullptr) const final;
    bool usesMockScrollAnimator() const final;
    void logMockScrollAnimatorMessage(const String&) const final;
    String debugDescription() const final;

    // NOTE: This should only be called by the overridden setScrollOffset from ScrollableArea.
    void scrollTo(int newOffset);

    using PaintFunction = WTF::Function<void(PaintInfo&, const LayoutPoint&, int listItemIndex)>;
    void paintItem(PaintInfo&, const LayoutPoint&, const PaintFunction&);

    void setHasVerticalScrollbar(bool hasScrollbar);
    Ref<Scrollbar> createScrollbar();
    void destroyScrollbar();
    
    int maximumNumberOfItemsThatFitInPaddingBottomArea() const;

    int numberOfVisibleItemsInPaddingTop() const;
    int numberOfVisibleItemsInPaddingBottom() const;

    void computeFirstIndexesVisibleInPaddingTopBottomAreas();

    LayoutUnit itemHeight() const;
    void valueChanged(unsigned listIndex);

    enum class ConsiderPadding { Yes, No };
    int numVisibleItems(ConsiderPadding = ConsiderPadding::No) const;
    int numItems() const;
    LayoutUnit listHeight() const;
    void paintScrollbar(PaintInfo&, const LayoutPoint&);
    void paintItemForeground(PaintInfo&, const LayoutPoint&, int listIndex);
    void paintItemBackground(PaintInfo&, const LayoutPoint&, int listIndex);
    void scrollToRevealSelection();

    bool shouldPlaceVerticalScrollbarOnLeft() const final { return RenderBlockFlow::shouldPlaceVerticalScrollbarOnLeft(); }

    bool m_optionsChanged;
    bool m_scrollToRevealSelectionAfterLayout;
    bool m_inAutoscroll;
    int m_optionsWidth;
    int m_indexOffset;

    std::optional<int> m_indexOfFirstVisibleItemInsidePaddingTopArea;
    std::optional<int> m_indexOfFirstVisibleItemInsidePaddingBottomArea;

    RefPtr<Scrollbar> m_vBar;
};

} // namepace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RenderListBox)
    static bool isType(const WebCore::RenderObject& widget) { return widget.isListBox(); }
    static bool isType(const WebCore::ScrollableArea& area) { return area.isListBox(); }
SPECIALIZE_TYPE_TRAITS_END()
