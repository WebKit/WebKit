/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004-2009, 2014-2016 Apple Inc. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#pragma once

#include "AdjustViewSizeOrNot.h"
#include "Color.h"
#include "ContainerNode.h"
#include "LayoutMilestones.h"
#include "LayoutRect.h"
#include "Pagination.h"
#include "PaintPhase.h"
#include "RenderPtr.h"
#include "ScrollView.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AXObjectCache;
class Element;
class FloatSize;
class Frame;
class HTMLFrameOwnerElement;
class URL;
class Node;
class Page;
class RenderBox;
class RenderElement;
class RenderEmbeddedObject;
class RenderLayer;
class RenderObject;
class RenderScrollbarPart;
class RenderStyle;
class RenderView;
class RenderWidget;

Pagination::Mode paginationModeForRenderStyle(const RenderStyle&);

typedef unsigned long long DOMTimeStamp;

class FrameView final : public ScrollView {
public:
    friend class RenderView;
    friend class Internals;

    WEBCORE_EXPORT static Ref<FrameView> create(Frame&);
    static Ref<FrameView> create(Frame&, const IntSize& initialSize);

    virtual ~FrameView();

    HostWindow* hostWindow() const override;
    
    WEBCORE_EXPORT void invalidateRect(const IntRect&) override;
    void setFrameRect(const IntRect&) override;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    bool scheduleAnimation() override;
#endif

    Frame& frame() const { return const_cast<Frame&>(m_frame.get()); }

    WEBCORE_EXPORT RenderView* renderView() const;

    int mapFromLayoutToCSSUnits(LayoutUnit) const;
    LayoutUnit mapFromCSSToLayoutUnits(int) const;

    LayoutUnit marginWidth() const { return m_margins.width(); } // -1 means default
    LayoutUnit marginHeight() const { return m_margins.height(); } // -1 means default
    void setMarginWidth(LayoutUnit);
    void setMarginHeight(LayoutUnit);

    WEBCORE_EXPORT void setCanHaveScrollbars(bool) override;
    WEBCORE_EXPORT void updateCanHaveScrollbars();

    PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation) override;

    bool avoidScrollbarCreation() const override;

    void setContentsSize(const IntSize&) override;
    void updateContentsSize() override;

    void layout(bool allowSubtree = true);
    WEBCORE_EXPORT bool didFirstLayout() const;
    void layoutTimerFired();
    void scheduleRelayout();
    void scheduleRelayoutOfSubtree(RenderElement&);
    void unscheduleRelayout();
    void queuePostLayoutCallback(WTF::Function<void ()>&&);
    bool layoutPending() const;
    bool isInLayout() const { return m_layoutPhase != OutsideLayout; }
    bool isInRenderTreeLayout() const { return m_layoutPhase == InRenderTreeLayout; }
    WEBCORE_EXPORT bool inPaintableState() { return m_layoutPhase != InRenderTreeLayout && m_layoutPhase != InViewSizeAdjust && m_layoutPhase != InPostLayout; }

    RenderElement* layoutRoot() const { return m_layoutRoot; }
    void clearLayoutRoot() { m_layoutRoot = nullptr; }
    int layoutCount() const { return m_layoutCount; }

    WEBCORE_EXPORT bool needsLayout() const;
    WEBCORE_EXPORT void setNeedsLayout();
    void setViewportConstrainedObjectsNeedLayout();

    bool needsStyleRecalcOrLayout(bool includeSubframes = true) const;

    bool needsFullRepaint() const { return m_needsFullRepaint; }

    WEBCORE_EXPORT bool renderedCharactersExceed(unsigned threshold);

    WEBCORE_EXPORT void setViewportIsStable(bool stable) { m_viewportIsStable = stable; }
    bool viewportIsStable() const { return m_viewportIsStable; }

#if PLATFORM(IOS)
    bool useCustomFixedPositionLayoutRect() const { return m_useCustomFixedPositionLayoutRect; }
    IntRect customFixedPositionLayoutRect() const { return m_customFixedPositionLayoutRect; }
    WEBCORE_EXPORT void setCustomFixedPositionLayoutRect(const IntRect&);
    bool updateFixedPositionLayoutRect();

    IntSize customSizeForResizeEvent() const { return m_customSizeForResizeEvent; }
    WEBCORE_EXPORT void setCustomSizeForResizeEvent(IntSize);

    WEBCORE_EXPORT void setScrollVelocity(double horizontalVelocity, double verticalVelocity, double scaleChangeRate, double timestamp);
#else
    bool useCustomFixedPositionLayoutRect() const { return false; }
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
    WEBCORE_EXPORT void serviceScriptedAnimations();
#endif

    void willRecalcStyle();
    bool updateCompositingLayersAfterStyleChange();
    void updateCompositingLayersAfterLayout();

    void clearBackingStores();
    void restoreBackingStores();

    // Called when changes to the GraphicsLayer hierarchy have to be synchronized with
    // content rendered via the normal painting path.
    void setNeedsOneShotDrawingSynchronization();

    WEBCORE_EXPORT GraphicsLayer* graphicsLayerForPlatformWidget(PlatformWidget);
    WEBCORE_EXPORT void scheduleLayerFlushAllowingThrottling();

    WEBCORE_EXPORT TiledBacking* tiledBacking() const override;

    // In the future when any ScrollableArea can have a node in th ScrollingTree, this should
    // become a virtual function on ScrollableArea.
    uint64_t scrollLayerID() const;
    ScrollableArea* scrollableAreaForScrollLayerID(uint64_t) const;

    bool hasCompositedContent() const;
    WEBCORE_EXPORT void enterCompositingMode();
    WEBCORE_EXPORT bool isEnclosedInCompositingLayer() const;

    // Only used with accelerated compositing, but outside the #ifdef to make linkage easier.
    // Returns true if the flush was completed.
    WEBCORE_EXPORT bool flushCompositingStateIncludingSubframes();

    // Returns true when a paint with the PaintBehaviorFlattenCompositingLayers flag set gives
    // a faithful representation of the content.
    WEBCORE_EXPORT bool isSoftwareRenderable() const;

    void setIsInWindow(bool);

    void resetScrollbars();
    void resetScrollbarsAndClearContentsSize();
    void prepareForDetach();
    void detachCustomScrollbars();
    WEBCORE_EXPORT void recalculateScrollbarOverlayStyle();

    void clear();

    WEBCORE_EXPORT bool isTransparent() const;
    WEBCORE_EXPORT void setTransparent(bool isTransparent);
    
    // True if the FrameView is not transparent, and the base background color is opaque.
    bool hasOpaqueBackground() const;

    WEBCORE_EXPORT Color baseBackgroundColor() const;
    WEBCORE_EXPORT void setBaseBackgroundColor(const Color&);
    void updateBackgroundRecursively(const Color&, bool);

    enum ExtendedBackgroundModeFlags {
        ExtendedBackgroundModeNone          = 0,
        ExtendedBackgroundModeVertical      = 1 << 0,
        ExtendedBackgroundModeHorizontal    = 1 << 1,
        ExtendedBackgroundModeAll           = ExtendedBackgroundModeVertical | ExtendedBackgroundModeHorizontal,
    };
    typedef unsigned ExtendedBackgroundMode;

    void updateExtendBackgroundIfNecessary();
    void updateTilesForExtendedBackgroundMode(ExtendedBackgroundMode);
    ExtendedBackgroundMode calculateExtendedBackgroundMode() const;

    bool hasExtendedBackgroundRectForPainting() const;
    IntRect extendedBackgroundRectForPainting() const;

    bool shouldUpdateWhileOffscreen() const;
    WEBCORE_EXPORT void setShouldUpdateWhileOffscreen(bool);
    bool shouldUpdate() const;

    WEBCORE_EXPORT void adjustViewSize();
    
    WEBCORE_EXPORT void setViewportSizeForCSSViewportUnits(IntSize);
    IntSize viewportSizeForCSSViewportUnits() const;
    
    IntRect windowClipRect() const override;
    WEBCORE_EXPORT IntRect windowClipRectForFrameOwner(const HTMLFrameOwnerElement*, bool clipToLayerContents) const;

    float visibleContentScaleFactor() const override;

#if USE(COORDINATED_GRAPHICS)
    void setFixedVisibleContentRect(const IntRect&) override;
#endif
    WEBCORE_EXPORT void setScrollPosition(const ScrollPosition&) override;
    void updateLayerPositionsAfterScrolling() override;
    void updateCompositingLayersAfterScrolling() override;
    bool requestScrollPositionUpdate(const ScrollPosition&) override;
    bool isRubberBandInProgress() const override;
    WEBCORE_EXPORT ScrollPosition minimumScrollPosition() const override;
    WEBCORE_EXPORT ScrollPosition maximumScrollPosition() const override;

    void viewportContentsChanged();
    WEBCORE_EXPORT void resumeVisibleImageAnimationsIncludingSubframes();

    // This is different than visibleContentRect() in that it ignores negative (or overly positive)
    // offsets from rubber-banding, and it takes zooming into account. 
    LayoutRect viewportConstrainedVisibleContentRect() const;

    String mediaType() const;
    WEBCORE_EXPORT void setMediaType(const String&);
    void adjustMediaTypeForPrinting(bool printing);

    void setCannotBlitToWindow();
    void setIsOverlapped(bool);
    bool isOverlapped() const { return m_isOverlapped; }
    bool isOverlappedIncludingAncestors() const;
    void setContentIsOpaque(bool);

    void addSlowRepaintObject(RenderElement*);
    void removeSlowRepaintObject(RenderElement*);
    bool hasSlowRepaintObject(const RenderElement& renderer) const { return m_slowRepaintObjects && m_slowRepaintObjects->contains(&renderer); }
    bool hasSlowRepaintObjects() const { return m_slowRepaintObjects && m_slowRepaintObjects->size(); }

    // Includes fixed- and sticky-position objects.
    typedef HashSet<RenderElement*> ViewportConstrainedObjectSet;
    void addViewportConstrainedObject(RenderElement*);
    void removeViewportConstrainedObject(RenderElement*);
    const ViewportConstrainedObjectSet* viewportConstrainedObjects() const { return m_viewportConstrainedObjects.get(); }
    bool hasViewportConstrainedObjects() const { return m_viewportConstrainedObjects && m_viewportConstrainedObjects->size() > 0; }
    
    float frameScaleFactor() const;

    // Functions for querying the current scrolled position, negating the effects of overhang
    // and adjusting for page scale.
    LayoutPoint scrollPositionForFixedPosition() const
    {
        return scrollPositionForFixedPosition(visibleContentRect(), totalContentsSize(), scrollPosition(), scrollOrigin(), frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), scrollBehaviorForFixedElements(), headerHeight(), footerHeight());
    }
    
    // Static function can be called from another thread.
    static LayoutPoint scrollPositionForFixedPosition(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements, int headerHeight, int footerHeight);

    // These layers are positioned differently when there is a topContentInset, a header, or a footer. These value need to be computed
    // on both the main thread and the scrolling thread.
    static float yPositionForInsetClipLayer(const FloatPoint& scrollPosition, float topContentInset);
    WEBCORE_EXPORT static FloatPoint positionForRootContentLayer(const FloatPoint& scrollPosition, const FloatPoint& scrollOrigin, float topContentInset, float headerHeight);
    WEBCORE_EXPORT FloatPoint positionForRootContentLayer() const;

    static float yPositionForHeaderLayer(const FloatPoint& scrollPosition, float topContentInset);
    static float yPositionForFooterLayer(const FloatPoint& scrollPosition, float topContentInset, float totalContentsHeight, float footerHeight);

#if PLATFORM(IOS)
    WEBCORE_EXPORT LayoutRect viewportConstrainedObjectsRect() const;
    // Static function can be called from another thread.
    WEBCORE_EXPORT static LayoutRect rectForViewportConstrainedObjects(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements);
#endif
    
    bool fixedElementsLayoutRelativeToFrame() const;

    WEBCORE_EXPORT void disableLayerFlushThrottlingTemporarilyForInteraction();
    bool speculativeTilingEnabled() const { return m_speculativeTilingEnabled; }
    void loadProgressingStatusChanged();

#if ENABLE(DASHBOARD_SUPPORT)
    void updateAnnotatedRegions();
#endif
    WEBCORE_EXPORT void updateControlTints();

    void restoreScrollbar();

    WEBCORE_EXPORT bool wasScrolledByUser() const;
    WEBCORE_EXPORT void setWasScrolledByUser(bool);

    bool safeToPropagateScrollToParent() const { return m_safeToPropagateScrollToParent; }
    void setSafeToPropagateScrollToParent(bool isSafe) { m_safeToPropagateScrollToParent = isSafe; }

    void addEmbeddedObjectToUpdate(RenderEmbeddedObject&);
    void removeEmbeddedObjectToUpdate(RenderEmbeddedObject&);

    WEBCORE_EXPORT void paintContents(GraphicsContext&, const IntRect& dirtyRect) override;

    struct PaintingState {
        PaintBehavior paintBehavior;
        bool isTopLevelPainter;
        bool isFlatteningPaintOfRootFrame;
        PaintingState()
            : paintBehavior()
            , isTopLevelPainter(false)
            , isFlatteningPaintOfRootFrame(false)
        {
        }
    };

    void willPaintContents(GraphicsContext&, const IntRect& dirtyRect, PaintingState&);
    void didPaintContents(GraphicsContext&, const IntRect& dirtyRect, PaintingState&);

#if PLATFORM(IOS)
    WEBCORE_EXPORT void didReplaceMultipartContent();
#endif

    WEBCORE_EXPORT void setPaintBehavior(PaintBehavior);
    WEBCORE_EXPORT PaintBehavior paintBehavior() const;
    bool isPainting() const;
    bool hasEverPainted() const { return m_lastPaintTime; }
    void setLastPaintTime(double lastPaintTime) { m_lastPaintTime = lastPaintTime; }
    WEBCORE_EXPORT void setNodeToDraw(Node*);

    enum SelectionInSnapshot { IncludeSelection, ExcludeSelection };
    enum CoordinateSpaceForSnapshot { DocumentCoordinates, ViewCoordinates };
    WEBCORE_EXPORT void paintContentsForSnapshot(GraphicsContext&, const IntRect& imageRect, SelectionInSnapshot shouldPaintSelection, CoordinateSpaceForSnapshot);

    void paintOverhangAreas(GraphicsContext&, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect) override;
    void paintScrollCorner(GraphicsContext&, const IntRect& cornerRect) override;
    void paintScrollbar(GraphicsContext&, Scrollbar&, const IntRect&) override;

    WEBCORE_EXPORT Color documentBackgroundColor() const;

    bool isInChildFrameWithFrameFlattening() const;

    void startDisallowingLayout() { ++m_layoutDisallowedCount; }
    void endDisallowingLayout() { ASSERT(m_layoutDisallowedCount > 0); --m_layoutDisallowedCount; }
    bool layoutDisallowed() const { return m_layoutDisallowedCount; }

    static double currentPaintTimeStamp() { return sCurrentPaintTimeStamp; } // returns 0 if not painting
    
    WEBCORE_EXPORT void updateLayoutAndStyleIfNeededRecursive();

    void incrementVisuallyNonEmptyCharacterCount(unsigned);
    void incrementVisuallyNonEmptyPixelCount(const IntSize&);
    void updateIsVisuallyNonEmpty();
    bool isVisuallyNonEmpty() const { return m_isVisuallyNonEmpty; }
    WEBCORE_EXPORT void enableAutoSizeMode(bool enable, const IntSize& minSize, const IntSize& maxSize);
    WEBCORE_EXPORT void setAutoSizeFixedMinimumHeight(int);
    IntSize autoSizingIntrinsicContentSize() const { return m_autoSizeContentSize; }

    WEBCORE_EXPORT void forceLayout(bool allowSubtree = false);
    WEBCORE_EXPORT void forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot);

    // FIXME: This method is retained because of embedded WebViews in AppKit.  When a WebView is embedded inside
    // some enclosing view with auto-pagination, no call happens to resize the view.  The new pagination model
    // needs the view to resize as a result of the breaks, but that means that the enclosing view has to potentially
    // resize around that view.  Auto-pagination uses the bounds of the actual view that's being printed to determine
    // the edges of the print operation, so the resize is necessary if the enclosing view's bounds depend on the
    // web document's bounds.
    // 
    // This is already a problem if the view needs to be a different size because of printer fonts or because of print stylesheets.
    // Mail/Dictionary work around this problem by using the _layoutForPrinting SPI
    // to at least get print stylesheets and printer fonts into play, but since WebKit doesn't know about the page offset or
    // page size, it can't actually paginate correctly during _layoutForPrinting.
    //
    // We can eventually move Mail to a newer SPI that would let them opt in to the layout-time pagination model,
    // but that doesn't solve the general problem of how other AppKit views could opt in to the better model.
    //
    // NO OTHER PLATFORM BESIDES MAC SHOULD USE THIS METHOD.
    WEBCORE_EXPORT void adjustPageHeightDeprecated(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

    bool scrollToFragment(const URL&);
    bool scrollToAnchor(const String&);
    void maintainScrollPositionAtAnchor(ContainerNode*);
    WEBCORE_EXPORT void scrollElementToRect(const Element&, const IntRect&);

    // Methods to convert points and rects between the coordinate space of the renderer, and this view.
    WEBCORE_EXPORT IntRect convertFromRendererToContainingView(const RenderElement*, const IntRect&) const;
    WEBCORE_EXPORT IntRect convertFromContainingViewToRenderer(const RenderElement*, const IntRect&) const;
    WEBCORE_EXPORT IntPoint convertFromRendererToContainingView(const RenderElement*, const IntPoint&) const;
    WEBCORE_EXPORT IntPoint convertFromContainingViewToRenderer(const RenderElement*, const IntPoint&) const;

    // Override ScrollView methods to do point conversion via renderers, in order to take transforms into account.
    IntRect convertToContainingView(const IntRect&) const override;
    IntRect convertFromContainingView(const IntRect&) const override;
    IntPoint convertToContainingView(const IntPoint&) const override;
    IntPoint convertFromContainingView(const IntPoint&) const override;

    bool isFrameViewScrollCorner(const RenderScrollbarPart& scrollCorner) const { return m_scrollCorner == &scrollCorner; }

    // isScrollable() takes an optional Scrollability parameter that allows the caller to define what they mean by 'scrollable.'
    // Most callers are interested in the default value, Scrollability::Scrollable, which means that there is actually content
    // to scroll to, and a scrollbar that will allow you to access it. In some cases, callers want to know if the FrameView is allowed
    // to rubber-band, which the main frame might be allowed to do even if there is no content to scroll to. In that case,
    // callers use Scrollability::ScrollableOrRubberbandable.
    enum class Scrollability { Scrollable, ScrollableOrRubberbandable };
    WEBCORE_EXPORT bool isScrollable(Scrollability definitionOfScrollable = Scrollability::Scrollable);

    bool isScrollableOrRubberbandable() override;
    bool hasScrollableOrRubberbandableAncestor() override;

    enum ScrollbarModesCalculationStrategy { RulesFromWebContentOnly, AnyRule };
    void calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy = AnyRule);

    IntPoint lastKnownMousePosition() const override;
    bool isHandlingWheelEvent() const override;
    bool shouldSetCursor() const;

    // FIXME: Remove this method once plugin loading is decoupled from layout.
    void flushAnyPendingPostLayoutTasks();

    bool shouldSuspendScrollAnimations() const override;
    void scrollbarStyleChanged(ScrollbarStyle, bool forceUpdate) override;

    RenderBox* embeddedContentBox() const;
    
    WEBCORE_EXPORT void setTracksRepaints(bool);
    bool isTrackingRepaints() const { return m_isTrackingRepaints; }
    WEBCORE_EXPORT void resetTrackedRepaints();
    const Vector<FloatRect>& trackedRepaintRects() const { return m_trackedRepaintRects; }
    String trackedRepaintRectsAsText() const;

    typedef HashSet<ScrollableArea*> ScrollableAreaSet;
    // Returns whether the scrollable area has just been newly added.
    WEBCORE_EXPORT bool addScrollableArea(ScrollableArea*);
    // Returns whether the scrollable area has just been removed.
    WEBCORE_EXPORT bool removeScrollableArea(ScrollableArea*);
    bool containsScrollableArea(ScrollableArea*) const;
    const ScrollableAreaSet* scrollableAreas() const { return m_scrollableAreas.get(); }

    void removeChild(Widget&) override;

    // This function exists for ports that need to handle wheel events manually.
    // On Mac WebKit1 the underlying NSScrollView just does the scrolling, but on most other platforms
    // we need this function in order to do the scroll ourselves.
    bool wheelEvent(const PlatformWheelEvent&);

    WEBCORE_EXPORT void setScrollingPerformanceLoggingEnabled(bool);

    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the page cache, but FrameView::pagination() only affects the current
    // FrameView. FrameView::pagination() will return m_pagination if it has been set. Otherwise,
    // it will return Page::pagination() since currently there are no callers that need to
    // distinguish between the two.
    const Pagination& pagination() const;
    void setPagination(const Pagination&);
    
    bool inProgrammaticScroll() const override { return m_inProgrammaticScroll; }
    void setInProgrammaticScroll(bool programmaticScroll) { m_inProgrammaticScroll = programmaticScroll; }

#if ENABLE(CSS_DEVICE_ADAPTATION)
    IntSize initialViewportSize() const { return m_initialViewportSize; }
    void setInitialViewportSize(const IntSize& size) { m_initialViewportSize = size; }
#endif

    bool isActive() const override;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override;

#if ENABLE(RUBBER_BANDING)
    WEBCORE_EXPORT GraphicsLayer* setWantsLayerForTopOverHangArea(bool) const;
    WEBCORE_EXPORT GraphicsLayer* setWantsLayerForBottomOverHangArea(bool) const;
#endif

    LayoutRect fixedScrollableAreaBoundsInflatedForScrolling(const LayoutRect& uninflatedBounds) const;
    LayoutPoint scrollPositionRespectingCustomFixedPosition() const;

    int headerHeight() const override { return m_headerHeight; }
    WEBCORE_EXPORT void setHeaderHeight(int);
    int footerHeight() const override { return m_footerHeight; }
    WEBCORE_EXPORT void setFooterHeight(int);

    WEBCORE_EXPORT float topContentInset(TopContentInsetType = TopContentInsetType::WebCoreContentInset) const override;
    void topContentInsetDidChange(float newTopContentInset);

    void topContentDirectionDidChange();

    WEBCORE_EXPORT void willStartLiveResize() override;
    WEBCORE_EXPORT void willEndLiveResize() override;

    WEBCORE_EXPORT void availableContentSizeChanged(AvailableSizeChangeReason) override;

    void adjustTiledBackingScrollability();

    void addPaintPendingMilestones(LayoutMilestones);
    void firePaintRelatedMilestonesIfNeeded();
    void fireLayoutRelatedMilestonesIfNeeded();
    LayoutMilestones milestonesPendingPaint() const { return m_milestonesPendingPaint; }

    bool visualUpdatesAllowedByClient() const { return m_visualUpdatesAllowedByClient; }
    WEBCORE_EXPORT void setVisualUpdatesAllowedByClient(bool);

    WEBCORE_EXPORT void setScrollPinningBehavior(ScrollPinningBehavior);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const;

    bool hasFlippedBlockRenderers() const { return m_hasFlippedBlockRenderers; }
    void setHasFlippedBlockRenderers(bool b) { m_hasFlippedBlockRenderers = b; }

    void updateWidgetPositions();
    void didAddWidgetToRenderTree(Widget&);
    void willRemoveWidgetFromRenderTree(Widget&);

    const HashSet<Widget*>& widgetsInRenderTree() const { return m_widgetsInRenderTree; }

    typedef Vector<Ref<FrameView>, 16> FrameViewList;
    FrameViewList renderedChildFrameViews() const;

    void addTrackedRepaintRect(const FloatRect&);

    // exposedRect represents WebKit's understanding of what part
    // of the view is actually exposed on screen (taking into account
    // clipping by other UI elements), whereas visibleContentRect is
    // internal to WebCore and doesn't respect those things.
    WEBCORE_EXPORT void setViewExposedRect(Optional<FloatRect>);
    Optional<FloatRect> viewExposedRect() const { return m_viewExposedRect; }

#if ENABLE(CSS_SCROLL_SNAP)
    void updateSnapOffsets() override;
    bool isScrollSnapInProgress() const override;
    void updateScrollingCoordinatorScrollSnapProperties() const;
#endif

    float adjustScrollStepForFixedContent(float step, ScrollbarOrientation, ScrollGranularity) override;

    void didChangeScrollOffset();

    void show() override;

    bool shouldPlaceBlockDirectionScrollbarOnLeft() const final;

protected:
    bool scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect) override;
    void scrollContentsSlowPath(const IntRect& updateRect) override;
    
    void repaintSlowRepaintObjects();

    bool isVerticalDocument() const override;
    bool isFlippedDocument() const override;

private:
    explicit FrameView(Frame&);

    void reset();
    void init();

    enum LayoutPhase {
        OutsideLayout,
        InPreLayout,
        InPreLayoutStyleUpdate,
        InRenderTreeLayout,
        InViewSizeAdjust,
        InPostLayout,
        InPostLayerPositionsUpdatedAfterLayout,
    };
    LayoutPhase layoutPhase() const { return m_layoutPhase; }

    bool inPreLayoutStyleUpdate() const { return m_layoutPhase == InPreLayoutStyleUpdate; }

    bool isFrameView() const override { return true; }

    friend class RenderWidget;
    bool useSlowRepaints(bool considerOverlap = true) const;
    bool useSlowRepaintsIfNotOverlapped() const;
    void updateCanBlitOnScrollRecursively();
    bool shouldLayoutAfterContentsResized() const;

    bool shouldUpdateCompositingLayersAfterScrolling() const;
    bool flushCompositingStateForThisFrame(const Frame& rootFrameForFlush);

    bool shouldDeferScrollUpdateAfterContentSizeChange() override;

    void scrollOffsetChangedViaPlatformWidgetImpl(const ScrollOffset& oldOffset, const ScrollOffset& newOffset) override;

    void applyOverflowToViewport(const RenderElement&, ScrollbarMode& hMode, ScrollbarMode& vMode);
    void applyPaginationToViewport();

    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    WEBCORE_EXPORT void paintControlTints();

    void forceLayoutParentViewIfNeeded();
    void flushPostLayoutTasksQueue();
    void performPostLayoutTasks();
    void autoSizeIfEnabled();

    void applyRecursivelyWithVisibleRect(const std::function<void (FrameView& frameView, const IntRect& visibleRect)>&);
    void resumeVisibleImageAnimations(const IntRect& visibleRect);
    void updateScriptedAnimationsAndTimersThrottlingState(const IntRect& visibleRect);

    void updateLayerFlushThrottling();
    WEBCORE_EXPORT void adjustTiledBackingCoverage();

    void repaintContentRectangle(const IntRect&) override;
    void addedOrRemovedScrollbar() override;

    void delegatesScrollingDidChange() override;

    // ScrollableArea interface
    void invalidateScrollbarRect(Scrollbar&, const IntRect&) override;
    void scrollTo(const ScrollPosition&) override;
    void setVisibleScrollerThumbRect(const IntRect&) override;
    ScrollableArea* enclosingScrollableArea() const override;
    IntRect scrollableAreaBoundingBox(bool* = nullptr) const override;
    bool scrollAnimatorEnabled() const override;
    GraphicsLayer* layerForScrolling() const override;
    GraphicsLayer* layerForHorizontalScrollbar() const override;
    GraphicsLayer* layerForVerticalScrollbar() const override;
    GraphicsLayer* layerForScrollCorner() const override;
#if ENABLE(RUBBER_BANDING)
    GraphicsLayer* layerForOverhangAreas() const override;
#endif
    void contentsResized() override;

#if PLATFORM(IOS)
    void unobscuredContentSizeChanged() override;
#endif

    bool usesCompositedScrolling() const override;
    bool usesAsyncScrolling() const override;
    bool usesMockScrollAnimator() const override;
    void logMockScrollAnimatorMessage(const String&) const override;

    // Override scrollbar notifications to update the AXObject cache.
    void didAddScrollbar(Scrollbar*, ScrollbarOrientation) override;
    void willRemoveScrollbar(Scrollbar*, ScrollbarOrientation) override;

    IntSize sizeForResizeEvent() const;
    void sendResizeEventIfNeeded();

    void handleDeferredScrollbarsUpdateAfterDirectionChange();

    void updateScrollableAreaSet();

    void notifyPageThatContentAreaWillPaint() const override;

    void enableSpeculativeTilingIfNeeded();
    void speculativeTilingEnableTimerFired();

    void updateEmbeddedObjectsTimerFired();
    bool updateEmbeddedObjects();
    void updateEmbeddedObject(RenderEmbeddedObject&);
    void scrollToAnchor();
    void scrollPositionChanged(const ScrollPosition& oldPosition, const ScrollPosition& newPosition);
    void scrollableAreaSetChanged();
    void sendScrollEvent();

    bool hasCustomScrollbars() const;

    void updateScrollCorner() override;

    FrameView* parentFrameView() const;

    void startLayoutAtMainFrameViewIfNeeded(bool allowSubtree);
    bool frameFlatteningEnabled() const;
    bool isFrameFlatteningValidForThisFrame() const;

    bool qualifiesAsVisuallyNonEmpty() const;
    bool isViewForDocumentInFrame() const;

    AXObjectCache* axObjectCache() const;
    void notifyWidgetsInAllFrames(WidgetNotification);
    void removeFromAXObjectCache();
    void notifyWidgets(WidgetNotification);

    void convertSubtreeLayoutToFullLayout();

    RenderElement* viewportRenderer() const;

    HashSet<Widget*> m_widgetsInRenderTree;

    static double sCurrentPaintTimeStamp; // used for detecting decoded resource thrash in the cache

    LayoutSize m_size;
    LayoutSize m_margins;

    std::unique_ptr<ListHashSet<RenderEmbeddedObject*>> m_embeddedObjectsToUpdate;
    const Ref<Frame> m_frame;

    std::unique_ptr<HashSet<const RenderElement*>> m_slowRepaintObjects;

    bool m_needsFullRepaint;
    
    bool m_canHaveScrollbars;
    bool m_cannotBlitToWindow;
    bool m_isOverlapped;
    bool m_contentIsOpaque;

    Timer m_layoutTimer;
    bool m_delayedLayout;
    RenderElement* m_layoutRoot { nullptr };

    LayoutPhase m_layoutPhase;
    bool m_layoutSchedulingEnabled;
    bool m_inSynchronousPostLayout;
    int m_layoutCount;
    unsigned m_nestedLayoutCount;
    Timer m_postLayoutTasksTimer;
    Timer m_updateEmbeddedObjectsTimer;
    bool m_firstLayoutCallbackPending;

    bool m_firstLayout;
    bool m_isTransparent;
    Color m_baseBackgroundColor;
    IntSize m_lastViewportSize;
    float m_lastZoomFactor;

    String m_mediaType;
    String m_mediaTypeWhenNotPrinting;

    bool m_overflowStatusDirty;
    bool m_horizontalOverflow;
    bool m_verticalOverflow;
    enum class ViewportRendererType { None, Document, Body };
    ViewportRendererType m_viewportRendererType { ViewportRendererType::None };

    Pagination m_pagination;

    bool m_wasScrolledByUser;
    bool m_inProgrammaticScroll;
    bool m_safeToPropagateScrollToParent;
    Timer m_delayedScrollEventTimer;

    double m_lastPaintTime;

    bool m_isTrackingRepaints; // Used for testing.
    Vector<FloatRect> m_trackedRepaintRects;

    bool m_shouldUpdateWhileOffscreen;

    Optional<FloatRect> m_viewExposedRect;

    unsigned m_deferSetNeedsLayoutCount;
    bool m_setNeedsLayoutWasDeferred;
    int m_layoutDisallowedCount { 0 };

    RefPtr<Node> m_nodeToDraw;
    PaintBehavior m_paintBehavior;
    bool m_isPainting;

    unsigned m_visuallyNonEmptyCharacterCount;
    unsigned m_visuallyNonEmptyPixelCount;
    bool m_isVisuallyNonEmpty;
    bool m_firstVisuallyNonEmptyLayoutCallbackPending;

    bool m_viewportIsStable { true };

    bool m_needsDeferredScrollbarsUpdate { false };

    RefPtr<ContainerNode> m_maintainScrollPositionAnchor;

    // Renderer to hold our custom scroll corner.
    RenderPtr<RenderScrollbarPart> m_scrollCorner;

    bool m_speculativeTilingEnabled;
    Timer m_speculativeTilingEnableTimer;

#if PLATFORM(IOS)
    bool m_useCustomFixedPositionLayoutRect;
    IntRect m_customFixedPositionLayoutRect;

    bool m_useCustomSizeForResizeEvent;
    IntSize m_customSizeForResizeEvent;
#endif

    IntSize m_overrideViewportSize;
    bool m_hasOverrideViewportSize;

    // If true, automatically resize the frame view around its content.
    bool m_shouldAutoSize;
    bool m_inAutoSize;
    // True if autosize has been run since m_shouldAutoSize was set.
    bool m_didRunAutosize;
    // The lower bound on the size when autosizing.
    IntSize m_minAutoSize;
    // The upper bound on the size when autosizing.
    IntSize m_maxAutoSize;
    // The fixed height to resize the view to after autosizing is complete.
    int m_autoSizeFixedMinimumHeight;
    // The intrinsic content size decided by autosizing.
    IntSize m_autoSizeContentSize;

    std::unique_ptr<ScrollableAreaSet> m_scrollableAreas;
    std::unique_ptr<ViewportConstrainedObjectSet> m_viewportConstrainedObjects;

    int m_headerHeight;
    int m_footerHeight;

    LayoutMilestones m_milestonesPendingPaint;

    static const unsigned visualCharacterThreshold = 200;
    static const unsigned visualPixelThreshold = 32 * 32;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    // Size of viewport before any UA or author styles have overridden
    // the viewport given by the window or viewing area of the UA.
    IntSize m_initialViewportSize;
#endif

    bool m_visualUpdatesAllowedByClient;
    bool m_hasFlippedBlockRenderers;

    ScrollPinningBehavior m_scrollPinningBehavior;

    IntRect* m_cachedWindowClipRect { nullptr };
    Vector<WTF::Function<void ()>> m_postLayoutCallbackQueue;
};

inline void FrameView::incrementVisuallyNonEmptyCharacterCount(unsigned count)
{
    if (m_isVisuallyNonEmpty)
        return;
    m_visuallyNonEmptyCharacterCount += count;
    if (m_visuallyNonEmptyCharacterCount <= visualCharacterThreshold)
        return;
    updateIsVisuallyNonEmpty();
}

inline void FrameView::incrementVisuallyNonEmptyPixelCount(const IntSize& size)
{
    if (m_isVisuallyNonEmpty)
        return;
    m_visuallyNonEmptyPixelCount += size.width() * size.height();
    if (m_visuallyNonEmptyPixelCount <= visualPixelThreshold)
        return;
    updateIsVisuallyNonEmpty();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WIDGET(FrameView, isFrameView())
