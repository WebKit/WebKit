/*
 * Copyright (C) 2003, 2009, 2012, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Igalia S.L.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#pragma once

#include "RenderLayer.h"
#include "ScrollableArea.h"

namespace WebCore {

class RenderMarquee;

class RenderLayerScrollableArea final : public ScrollableArea {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerScrollableArea(RenderLayer&);
    virtual ~RenderLayerScrollableArea();

    RenderLayer& layer() { return m_layer; }

    void clear();

    RenderMarquee* marquee() const { return m_marquee.get(); }
    void updateMarqueePosition();
    void createOrDestroyMarquee();

    void restoreScrollPosition();

#if ENABLE(IOS_TOUCH_EVENTS)
    void registerAsTouchEventListenerForScrolling();
    void unregisterAsTouchEventListenerForScrolling();
#endif

    void setPostLayoutScrollPosition(std::optional<ScrollPosition>);
    void applyPostLayoutScrollPositionIfNeeded();

    int scrollWidth() const;
    int scrollHeight() const;

    void panScrollFromPoint(const IntPoint&);

    // Scrolling methods for layers that can scroll their overflow.
    void scrollByRecursively(const IntSize& delta, ScrollableArea** scrolledArea = nullptr);

    // Attempt to scroll the given ScrollOffset, returning the real target offset after it has
    // been adjusted by scroll snapping.
    WEBCORE_EXPORT ScrollOffset scrollToOffset(const ScrollOffset&, const ScrollPositionChangeOptions& = ScrollPositionChangeOptions::createProgrammatic());

    void scrollToXPosition(int x, const ScrollPositionChangeOptions&);
    void scrollToYPosition(int y, const ScrollPositionChangeOptions&);
    void setScrollPosition(const ScrollPosition&, const ScrollPositionChangeOptions&);

    // These are only used by marquee.
    void scrollToXOffset(int x) { scrollToOffset(ScrollOffset(x, scrollOffset().y()), ScrollPositionChangeOptions::createProgrammaticUnclamped()); }
    void scrollToYOffset(int y) { scrollToOffset(ScrollOffset(scrollOffset().x(), y), ScrollPositionChangeOptions::createProgrammaticUnclamped()); }

    bool scrollsOverflow() const;
    bool hasScrollableHorizontalOverflow() const;
    bool hasScrollableVerticalOverflow() const;
    bool hasScrollbars() const { return horizontalScrollbar() || verticalScrollbar(); }
    bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
    bool hasVerticalScrollbar() const { return verticalScrollbar(); }
    void setHasHorizontalScrollbar(bool);
    void setHasVerticalScrollbar(bool);

    bool requiresScrollPositionReconciliation() const { return m_requiresScrollPositionReconciliation; }
    void setRequiresScrollPositionReconciliation(bool requiresReconciliation = true) { m_requiresScrollPositionReconciliation = requiresReconciliation; }

    // Returns true when the layer could do touch scrolling, but doesn't look at whether there is actually scrollable overflow.
    bool canUseCompositedScrolling() const;
    // Returns true when there is actually scrollable overflow (requires layout to be up-to-date).
    bool hasCompositedScrollableOverflow() const { return m_hasCompositedScrollableOverflow; }

    int verticalScrollbarWidth(OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const;
    int horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const;

    bool hasOverflowControls() const;
    bool hitTestOverflowControls(HitTestResult&, const IntPoint& localPoint);
    bool hitTestResizerInFragments(const LayerFragments&, const HitTestLocation&, LayoutPoint& pointInFragment) const;

    void paintOverflowControls(GraphicsContext&, const IntPoint&, const IntRect& damageRect, bool paintingOverlayControls = false);
    void paintScrollCorner(GraphicsContext&, const IntPoint&, const IntRect& damageRect);
    void paintResizer(GraphicsContext&, const LayoutPoint&, const LayoutRect& damageRect);
    void paintOverlayScrollbars(GraphicsContext&, const LayoutRect& damageRect, OptionSet<PaintBehavior>, RenderObject* subtreePaintRoot = nullptr);

    void updateScrollInfoAfterLayout();
    void updateScrollbarSteps();

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1);

public:
    // All methods in this section override ScrollableaArea methods (final).
    void availableContentSizeChanged(AvailableSizeChangeReason) final;

    bool horizontalScrollbarHiddenByStyle() const final;
    bool verticalScrollbarHiddenByStyle() const final;

    ScrollPosition scrollPosition() const final { return m_scrollPosition; }

    Scrollbar* horizontalScrollbar() const final { return m_hBar.get(); }
    Scrollbar* verticalScrollbar() const final { return m_vBar.get(); }
    ScrollableArea* enclosingScrollableArea() const final;

    bool handleWheelEventForScrolling(const PlatformWheelEvent&, std::optional<WheelScrollGestureState>) final;
    bool isScrollableOrRubberbandable() final;
    bool hasScrollableOrRubberbandableAncestor() final;
    bool useDarkAppearance() const final;
    void updateSnapOffsets() final;

#if PLATFORM(IOS_FAMILY)
#if ENABLE(IOS_TOUCH_EVENTS)
    bool handleTouchEvent(const PlatformTouchEvent&) final;
#endif

    void didStartScroll() final;
    void didEndScroll() final;
    void didUpdateScroll() final;
#endif

    GraphicsLayer* layerForHorizontalScrollbar() const final;
    GraphicsLayer* layerForVerticalScrollbar() const final;
    GraphicsLayer* layerForScrollCorner() const final;

    bool usesCompositedScrolling() const final;
    bool usesAsyncScrolling() const final;

    bool shouldPlaceVerticalScrollbarOnLeft() const final;

    bool isRenderLayer() const final { return true; }
    void invalidateScrollbarRect(Scrollbar&, const IntRect&) final;
    void invalidateScrollCornerRect(const IntRect&) final;
    bool isActive() const final;
    bool isScrollCornerVisible() const final;
    IntRect scrollCornerRect() const final;
    IntRect convertFromScrollbarToContainingView(const Scrollbar&, const IntRect&) const final;
    IntRect convertFromContainingViewToScrollbar(const Scrollbar&, const IntRect&) const final;
    IntPoint convertFromScrollbarToContainingView(const Scrollbar&, const IntPoint&) const final;
    IntPoint convertFromContainingViewToScrollbar(const Scrollbar&, const IntPoint&) const final;
    void setScrollOffset(const ScrollOffset&) final;
    ScrollingNodeID scrollingNodeID() const final;

    IntRect visibleContentRectInternal(VisibleContentRectIncludesScrollbars, VisibleContentRectBehavior) const final;
    IntSize overhangAmount() const final;
    IntPoint lastKnownMousePositionInView() const final;
    bool isHandlingWheelEvent() const final;
    bool shouldSuspendScrollAnimations() const final;
    IntRect scrollableAreaBoundingBox(bool* isInsideFixed = nullptr) const final;
    bool isUserScrollInProgress() const final;
    bool isRubberBandInProgress() const final;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;
    bool isScrollSnapInProgress() const final;
    bool usesMockScrollAnimator() const final;
    void logMockScrollAnimatorMessage(const String&) const final;

    String debugDescription() const final;

    IntSize visibleSize() const final;
    IntSize contentsSize() const final;
    IntSize reachableTotalContentsSize() const final;

    bool requestScrollPositionUpdate(const ScrollPosition&, ScrollType = ScrollType::User, ScrollClamping = ScrollClamping::Clamped) final;

    bool containsDirtyOverlayScrollbars() const { return m_containsDirtyOverlayScrollbars; }
    void setContainsDirtyOverlayScrollbars(bool dirtyScrollbars) { m_containsDirtyOverlayScrollbars = dirtyScrollbars; }

    void updateScrollbarsAfterStyleChange(const RenderStyle* oldStyle);
    void updateScrollbarsAfterLayout();

    void positionOverflowControls(const IntSize&);

    void updateAllScrollbarRelatedStyle();

    LayoutUnit overflowTop() const;
    LayoutUnit overflowBottom() const;
    LayoutUnit overflowLeft() const;
    LayoutUnit overflowRight() const;

    RenderLayer::OverflowControlRects overflowControlsRects() const;

    bool overflowControlsIntersectRect(const IntRect& localRect) const;

    bool scrollingMayRevealBackground() const;

    void computeHasCompositedScrollableOverflow();

    // NOTE: This should only be called by the overridden setScrollOffset from ScrollableArea.
    void scrollTo(const ScrollPosition&);
    void updateCompositingLayersAfterScroll();

    IntSize scrollbarOffset(const Scrollbar&) const;

    void updateLayerPositionsAfterOverflowScroll();
    void updateLayerPositionsAfterDocumentScroll();

    std::optional<LayoutRect> updateScrollPosition(const ScrollPositionChangeOptions&, const LayoutRect& revealRect, const LayoutRect& localExposeRect);

#if PLATFORM(IOS_FAMILY)
    bool adjustForIOSCaretWhenScrolling() const { return m_adjustForIOSCaretWhenScrolling; }
    void setAdjustForIOSCaretWhenScrolling(bool adjustForIOSCaretWhenScrolling) { m_adjustForIOSCaretWhenScrolling = adjustForIOSCaretWhenScrolling; }
#endif

private:
    bool hasHorizontalOverflow() const;
    bool hasVerticalOverflow() const;

    bool showsOverflowControls() const;

    ScrollOffset clampScrollOffset(const ScrollOffset&) const;

    void computeScrollDimensions();
    void computeScrollOrigin();

    void updateScrollableAreaSet(bool hasOverflow);

    void updateScrollCornerStyle();
    void updateResizerStyle();

    void drawPlatformResizerImage(GraphicsContext&, const LayoutRect& resizerCornerRect);

    Ref<Scrollbar> createScrollbar(ScrollbarOrientation);
    void destroyScrollbar(ScrollbarOrientation);

    void clearScrollCorner();
    void clearResizer();

    void updateScrollbarPresenceAndState(std::optional<bool> hasHorizontalOverflow = std::nullopt, std::optional<bool> hasVerticalOverflow = std::nullopt);

private:
    bool m_scrollDimensionsDirty { true };
    bool m_inOverflowRelayout { false };
    bool m_registeredScrollableArea { false };
    bool m_hasCompositedScrollableOverflow { false };

#if PLATFORM(IOS_FAMILY)
#if ENABLE(IOS_TOUCH_EVENTS)
    bool m_registeredAsTouchEventListenerForScrolling { false };
#endif
    bool m_adjustForIOSCaretWhenScrolling { false };
#endif
    bool m_requiresScrollPositionReconciliation { false };
    bool m_containsDirtyOverlayScrollbars { false };
    bool m_updatingMarqueePosition { false };

    // The width/height of our scrolled area.
    int m_scrollWidth { 0 };
    int m_scrollHeight { 0 };

    RenderLayer& m_layer;
    ScrollPosition m_scrollPosition;
    std::optional<ScrollPosition> m_postLayoutScrollPosition;

    // For layers with overflow, we have a pair of scrollbars.
    RefPtr<Scrollbar> m_hBar;
    RefPtr<Scrollbar> m_vBar;

    IntPoint m_cachedOverlayScrollbarOffset;

    // Renderers to hold our custom scroll corner and resizer.
    RenderPtr<RenderScrollbarPart> m_scrollCorner;
    RenderPtr<RenderScrollbarPart> m_resizer;

    std::unique_ptr<RenderMarquee> m_marquee; // Used for <marquee>.
};

} // namespace WebCore
