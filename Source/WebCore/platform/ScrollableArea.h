/*
 * Copyright (C) 2008-2016 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "ScrollSnapOffsetsInfo.h"
#include "ScrollTypes.h"
#include "Scrollbar.h"
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class FloatPoint;
class GraphicsContext;
class LayoutPoint;
class LayoutSize;
class PlatformTouchEvent;
class PlatformWheelEvent;
class ScrollAnimator;
class GraphicsLayer;
class TiledBacking;

// scrollPosition is in content coordinates (0,0 is at scrollOrigin), so may have negative components.
typedef IntPoint ScrollPosition;
// scrollOffset() is the value used by scrollbars (min is 0,0), and should never have negative components.
typedef IntPoint ScrollOffset;

class ScrollableArea : public CanMakeWeakPtr<ScrollableArea> {
public:
    WEBCORE_EXPORT bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1);
    WEBCORE_EXPORT void scrollToOffsetWithoutAnimation(const FloatPoint&, ScrollClamping = ScrollClamping::Clamped);
    void scrollToOffsetWithoutAnimation(ScrollbarOrientation, float offset);

    // Should be called when the scroll position changes externally, for example if the scroll layer position
    // is updated on the scrolling thread and we need to notify the main thread.
    WEBCORE_EXPORT void notifyScrollPositionChanged(const ScrollPosition&);

    // Allows subclasses to handle scroll position updates themselves. If this member function
    // returns true, the scrollable area won't actually update the scroll position and instead
    // expect it to happen sometime in the future.
    virtual bool requestScrollPositionUpdate(const ScrollPosition&) { return false; }

    WEBCORE_EXPORT bool handleWheelEvent(const PlatformWheelEvent&);

#if ENABLE(CSS_SCROLL_SNAP)
    WEBCORE_EXPORT const Vector<LayoutUnit>* horizontalSnapOffsets() const;
    WEBCORE_EXPORT const Vector<LayoutUnit>* verticalSnapOffsets() const;
    WEBCORE_EXPORT const Vector<ScrollOffsetRange<LayoutUnit>>* horizontalSnapOffsetRanges() const;
    WEBCORE_EXPORT const Vector<ScrollOffsetRange<LayoutUnit>>* verticalSnapOffsetRanges() const;
    virtual void updateSnapOffsets() { };
    void setHorizontalSnapOffsets(const Vector<LayoutUnit>&);
    void setVerticalSnapOffsets(const Vector<LayoutUnit>&);
    void setHorizontalSnapOffsetRanges(const Vector<ScrollOffsetRange<LayoutUnit>>&);
    void setVerticalSnapOffsetRanges(const Vector<ScrollOffsetRange<LayoutUnit>>&);
    void clearHorizontalSnapOffsets();
    void clearVerticalSnapOffsets();
    unsigned currentHorizontalSnapPointIndex() const { return m_currentHorizontalSnapPointIndex; }
    void setCurrentHorizontalSnapPointIndex(unsigned index) { m_currentHorizontalSnapPointIndex = index; }
    unsigned currentVerticalSnapPointIndex() const { return m_currentVerticalSnapPointIndex; }
    void setCurrentVerticalSnapPointIndex(unsigned index) { m_currentVerticalSnapPointIndex = index; }
    IntPoint nearestActiveSnapPoint(const IntPoint&);
#endif

    void updateScrollSnapState();

#if ENABLE(TOUCH_EVENTS)
    virtual bool handleTouchEvent(const PlatformTouchEvent&);
#endif

#if PLATFORM(IOS_FAMILY)
    virtual void didStartScroll() { }
    virtual void didEndScroll() { }
    virtual void didUpdateScroll() { }
#endif
    virtual void setIsUserScroll(bool) { }

    // Functions for controlling if you can scroll past the end of the document.
    bool constrainsScrollingToContentEdge() const { return m_constrainsScrollingToContentEdge; }
    void setConstrainsScrollingToContentEdge(bool constrainsScrollingToContentEdge) { m_constrainsScrollingToContentEdge = constrainsScrollingToContentEdge; }

    void setVerticalScrollElasticity(ScrollElasticity scrollElasticity) { m_verticalScrollElasticity = scrollElasticity; }
    ScrollElasticity verticalScrollElasticity() const { return static_cast<ScrollElasticity>(m_verticalScrollElasticity); }

    void setHorizontalScrollElasticity(ScrollElasticity scrollElasticity) { m_horizontalScrollElasticity = scrollElasticity; }
    ScrollElasticity horizontalScrollElasticity() const { return static_cast<ScrollElasticity>(m_horizontalScrollElasticity); }

    bool inLiveResize() const { return m_inLiveResize; }
    WEBCORE_EXPORT virtual void willStartLiveResize();
    WEBCORE_EXPORT virtual void willEndLiveResize();

    WEBCORE_EXPORT void contentAreaWillPaint() const;
    WEBCORE_EXPORT void mouseEnteredContentArea() const;
    WEBCORE_EXPORT void mouseExitedContentArea() const;
    WEBCORE_EXPORT void mouseMovedInContentArea() const;
    WEBCORE_EXPORT void mouseEnteredScrollbar(Scrollbar*) const;
    void mouseExitedScrollbar(Scrollbar*) const;
    void mouseIsDownInScrollbar(Scrollbar*, bool) const;
    void contentAreaDidShow() const;
    void contentAreaDidHide() const;

    void lockOverlayScrollbarStateToHidden(bool shouldLockState) const;
    WEBCORE_EXPORT bool scrollbarsCanBeActive() const;

    WEBCORE_EXPORT virtual void didAddScrollbar(Scrollbar*, ScrollbarOrientation);
    WEBCORE_EXPORT virtual void willRemoveScrollbar(Scrollbar*, ScrollbarOrientation);

    WEBCORE_EXPORT virtual void contentsResized();

    // Force the contents to recompute their size (i.e. do layout).
    virtual void updateContentsSize() { }

    enum class AvailableSizeChangeReason {
        ScrollbarsChanged,
        AreaSizeChanged
    };
    WEBCORE_EXPORT virtual void availableContentSizeChanged(AvailableSizeChangeReason);

    bool hasOverlayScrollbars() const;
    WEBCORE_EXPORT virtual void setScrollbarOverlayStyle(ScrollbarOverlayStyle);
    ScrollbarOverlayStyle scrollbarOverlayStyle() const { return static_cast<ScrollbarOverlayStyle>(m_scrollbarOverlayStyle); }
    bool useDarkAppearanceForScrollbars() const;

    virtual ScrollingNodeID scrollingNodeID() const { return 0; }

    // This getter will create a ScrollAnimator if it doesn't already exist.
    WEBCORE_EXPORT ScrollAnimator& scrollAnimator() const;

    // This getter will return null if the ScrollAnimator hasn't been created yet.
    ScrollAnimator* existingScrollAnimator() const { return m_scrollAnimator.get(); }

    virtual bool isActive() const = 0;
    virtual int scrollSize(ScrollbarOrientation) const = 0;
    virtual int scrollOffset(ScrollbarOrientation) const = 0;
    WEBCORE_EXPORT virtual void invalidateScrollbar(Scrollbar&, const IntRect&);
    virtual bool isScrollCornerVisible() const = 0;
    virtual IntRect scrollCornerRect() const = 0;
    WEBCORE_EXPORT virtual void invalidateScrollCorner(const IntRect&);

    virtual bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const = 0;

    // Convert points and rects between the scrollbar and its containing view.
    // The client needs to implement these in order to be aware of layout effects
    // like CSS transforms.
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
    {
        return scrollbar.Widget::convertToContainingView(scrollbarRect);
    }
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
    {
        return scrollbar.Widget::convertFromContainingView(parentRect);
    }
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
    {
        return scrollbar.Widget::convertToContainingView(scrollbarPoint);
    }
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
    {
        return scrollbar.Widget::convertFromContainingView(parentPoint);
    }

    int horizontalScrollbarIntrusion() const;
    int verticalScrollbarIntrusion() const;
    WEBCORE_EXPORT IntSize scrollbarIntrusion() const;

    virtual Scrollbar* horizontalScrollbar() const { return nullptr; }
    virtual Scrollbar* verticalScrollbar() const { return nullptr; }

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    bool scrollOriginChanged() const { return m_scrollOriginChanged; }

    virtual ScrollPosition scrollPosition() const;
    virtual ScrollPosition minimumScrollPosition() const;
    virtual ScrollPosition maximumScrollPosition() const;

    ScrollPosition constrainScrollPosition(const ScrollPosition& position) const
    {
        return position.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    }

    ScrollOffset maximumScrollOffset() const;

    WEBCORE_EXPORT ScrollPosition scrollPositionFromOffset(ScrollOffset) const;
    WEBCORE_EXPORT ScrollOffset scrollOffsetFromPosition(ScrollPosition) const;

    template<typename PositionType, typename SizeType>
    static PositionType scrollPositionFromOffset(PositionType offset, SizeType scrollOrigin)
    {
        return offset - scrollOrigin;
    }

    template<typename PositionType, typename SizeType>
    static PositionType scrollOffsetFromPosition(PositionType position, SizeType scrollOrigin)
    {
        return position + scrollOrigin;
    }

    WEBCORE_EXPORT virtual bool scrolledToTop() const;
    WEBCORE_EXPORT virtual bool scrolledToBottom() const;
    WEBCORE_EXPORT virtual bool scrolledToLeft() const;
    WEBCORE_EXPORT virtual bool scrolledToRight() const;

    bool isScrolledProgrammatically() const { return m_scrolledProgrammatically; }
    void setScrolledProgrammatically(bool state) { m_scrolledProgrammatically = state; }

    enum VisibleContentRectIncludesScrollbars { ExcludeScrollbars, IncludeScrollbars };
    enum VisibleContentRectBehavior {
        ContentsVisibleRect,
#if PLATFORM(IOS_FAMILY)
        LegacyIOSDocumentViewRect,
        LegacyIOSDocumentVisibleRect = LegacyIOSDocumentViewRect
#else
        LegacyIOSDocumentVisibleRect = ContentsVisibleRect
#endif
    };

    WEBCORE_EXPORT IntRect visibleContentRect(VisibleContentRectBehavior = ContentsVisibleRect) const;
    WEBCORE_EXPORT IntRect visibleContentRectIncludingScrollbars(VisibleContentRectBehavior = ContentsVisibleRect) const;

    int visibleWidth() const { return visibleSize().width(); }
    int visibleHeight() const { return visibleSize().height(); }
    virtual IntSize visibleSize() const = 0;

    virtual IntSize contentsSize() const = 0;
    virtual IntSize overhangAmount() const { return IntSize(); }
    virtual IntPoint lastKnownMousePosition() const { return IntPoint(); }
    virtual bool isHandlingWheelEvent() const { return false; }

    virtual int headerHeight() const { return 0; }
    virtual int footerHeight() const { return 0; }

    // The totalContentsSize() is equivalent to the contentsSize() plus the header and footer heights.
    WEBCORE_EXPORT IntSize totalContentsSize() const;

    virtual bool useDarkAppearance() const { return false; }

    virtual bool shouldSuspendScrollAnimations() const { return true; }
    WEBCORE_EXPORT virtual void scrollbarStyleChanged(ScrollbarStyle /*newStyle*/, bool /*forceUpdate*/);
    virtual void setVisibleScrollerThumbRect(const IntRect&) { }
    
    // Note that this only returns scrollable areas that can actually be scrolled.
    virtual ScrollableArea* enclosingScrollableArea() const = 0;

    virtual bool isScrollableOrRubberbandable() = 0;
    virtual bool hasScrollableOrRubberbandableAncestor() = 0;

    // Returns the bounding box of this scrollable area, in the coordinate system of the enclosing scroll view.
    virtual IntRect scrollableAreaBoundingBox(bool* = nullptr) const = 0;

    virtual bool isRubberBandInProgress() const { return false; }
    virtual bool isScrollSnapInProgress() const { return false; }

    virtual bool scrollAnimatorEnabled() const { return false; }

    // NOTE: Only called from Internals for testing.
    WEBCORE_EXPORT void setScrollOffsetFromInternals(const ScrollOffset&);

    WEBCORE_EXPORT static LayoutPoint constrainScrollPositionForOverhang(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, int headerHeight, int footetHeight);
    LayoutPoint constrainScrollPositionForOverhang(const LayoutPoint& scrollPosition);

    // Computes the double value for the scrollbar's current position and the current overhang amount.
    // This function is static so that it can be called from the main thread or the scrolling thread.
    WEBCORE_EXPORT static void computeScrollbarValueAndOverhang(float currentPosition, float totalSize, float visibleSize, float& doubleValue, float& overhangAmount);

    // Let subclasses provide a way of asking for and servicing scroll
    // animations.
    virtual bool scheduleAnimation() { return false; }
    void serviceScrollAnimations();

#if PLATFORM(IOS_FAMILY)
    bool isHorizontalScrollerPinnedToMinimumPosition() const { return !horizontalScrollbar() || scrollOffset(HorizontalScrollbar) <= 0; }
    bool isHorizontalScrollerPinnedToMaximumPosition() const { return !horizontalScrollbar() || scrollOffset(HorizontalScrollbar) >= maximumScrollOffset().x(); }
    bool isVerticalScrollerPinnedToMinimumPosition() const { return !verticalScrollbar() || scrollOffset(VerticalScrollbar) <= 0; }
    bool isVerticalScrollerPinnedToMaximumPosition() const { return !verticalScrollbar() || scrollOffset(VerticalScrollbar) >= maximumScrollOffset().y(); }

    bool isPinnedInBothDirections(const IntSize&) const; 
    bool isPinnedHorizontallyInDirection(int horizontalScrollDelta) const; 
    bool isPinnedVerticallyInDirection(int verticalScrollDelta) const;
#endif

    virtual TiledBacking* tiledBacking() const { return nullptr; }

    // True if scrolling happens by moving compositing layers.
    virtual bool usesCompositedScrolling() const { return false; }
    // True if the contents can be scrolled asynchronously (i.e. by a ScrollingCoordinator).
    virtual bool usesAsyncScrolling() const { return false; }

    virtual GraphicsLayer* layerForHorizontalScrollbar() const { return nullptr; }
    virtual GraphicsLayer* layerForVerticalScrollbar() const { return nullptr; }

    bool hasLayerForHorizontalScrollbar() const;
    bool hasLayerForVerticalScrollbar() const;

    void verticalScrollbarLayerDidChange();
    void horizontalScrollbarLayerDidChange();

    virtual bool usesMockScrollAnimator() const { return false; }
    virtual void logMockScrollAnimatorMessage(const String&) const { };

    virtual bool shouldPlaceBlockDirectionScrollbarOnLeft() const = 0;

protected:
    WEBCORE_EXPORT ScrollableArea();
    WEBCORE_EXPORT virtual ~ScrollableArea();

    void setScrollOrigin(const IntPoint&);
    void resetScrollOriginChanged() { m_scrollOriginChanged = false; }

    WEBCORE_EXPORT virtual float adjustScrollStepForFixedContent(float step, ScrollbarOrientation, ScrollGranularity);
    virtual void invalidateScrollbarRect(Scrollbar&, const IntRect&) = 0;
    virtual void invalidateScrollCornerRect(const IntRect&) = 0;

    friend class ScrollingCoordinator;
    virtual GraphicsLayer* layerForScrolling() const { return nullptr; }
    virtual GraphicsLayer* layerForScrollCorner() const { return nullptr; }
#if ENABLE(RUBBER_BANDING)
    virtual GraphicsLayer* layerForOverhangAreas() const { return nullptr; }
#endif

    bool hasLayerForScrollCorner() const;

private:
    WEBCORE_EXPORT virtual IntRect visibleContentRectInternal(VisibleContentRectIncludesScrollbars, VisibleContentRectBehavior) const;
    void scrollPositionChanged(const ScrollPosition&);
    
    // NOTE: Only called from the ScrollAnimator.
    friend class ScrollAnimator;
    void setScrollOffsetFromAnimation(const ScrollOffset&);

    // This function should be overridden by subclasses to perform the actual
    // scroll of the content.
    virtual void setScrollOffset(const ScrollOffset&) = 0;
    ScrollSnapOffsetsInfo<LayoutUnit>& ensureSnapOffsetsInfo();

    mutable std::unique_ptr<ScrollAnimator> m_scrollAnimator;

#if ENABLE(CSS_SCROLL_SNAP)
    std::unique_ptr<ScrollSnapOffsetsInfo<LayoutUnit>> m_snapOffsetsInfo;
    unsigned m_currentHorizontalSnapPointIndex { 0 };
    unsigned m_currentVerticalSnapPointIndex { 0 };
#endif

    // There are 8 possible combinations of writing mode and direction. Scroll origin will be non-zero in the x or y axis
    // if there is any reversed direction or writing-mode. The combinations are:
    // writing-mode / direction     scrollOrigin.x() set    scrollOrigin.y() set
    // horizontal-tb / ltr          NO                      NO
    // horizontal-tb / rtl          YES                     NO
    // horizontal-bt / ltr          NO                      YES
    // horizontal-bt / rtl          YES                     YES
    // vertical-lr / ltr            NO                      NO
    // vertical-lr / rtl            NO                      YES
    // vertical-rl / ltr            YES                     NO
    // vertical-rl / rtl            YES                     YES
    IntPoint m_scrollOrigin;

    unsigned m_constrainsScrollingToContentEdge : 1;

    unsigned m_inLiveResize : 1;

    unsigned m_verticalScrollElasticity : 2; // ScrollElasticity
    unsigned m_horizontalScrollElasticity : 2; // ScrollElasticity

    unsigned m_scrollbarOverlayStyle : 2; // ScrollbarOverlayStyle

    unsigned m_scrollOriginChanged : 1;
    unsigned m_scrolledProgrammatically : 1;
};

} // namespace WebCore

