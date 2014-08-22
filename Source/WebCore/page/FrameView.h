/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.

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

#ifndef FrameView_h
#define FrameView_h

#include "AdjustViewSizeOrNot.h"
#include "Color.h"
#include "LayoutMilestones.h"
#include "LayoutRect.h"
#include "Pagination.h"
#include "PaintPhase.h"
#include "RenderPtr.h"
#include "ScrollView.h"
#include <memory>
#include <wtf/Forward.h>
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

    static PassRefPtr<FrameView> create(Frame&);
    static PassRefPtr<FrameView> create(Frame&, const IntSize& initialSize);

    virtual ~FrameView();

    virtual HostWindow* hostWindow() const override;
    
    virtual void invalidateRect(const IntRect&) override;
    virtual void setFrameRect(const IntRect&) override;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual bool scheduleAnimation() override;
#endif

    Frame& frame() const { return *m_frame; }

    RenderView* renderView() const;

    int mapFromLayoutToCSSUnits(LayoutUnit) const;
    LayoutUnit mapFromCSSToLayoutUnits(int) const;

    LayoutUnit marginWidth() const { return m_margins.width(); } // -1 means default
    LayoutUnit marginHeight() const { return m_margins.height(); } // -1 means default
    void setMarginWidth(LayoutUnit);
    void setMarginHeight(LayoutUnit);

    virtual void setCanHaveScrollbars(bool) override;
    void updateCanHaveScrollbars();

    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation) override;

    virtual bool avoidScrollbarCreation() const override;

    virtual void setContentsSize(const IntSize&) override;

    void layout(bool allowSubtree = true);
    bool didFirstLayout() const;
    void layoutTimerFired(Timer<FrameView>&);
    void scheduleRelayout();
    void scheduleRelayoutOfSubtree(RenderElement&);
    void unscheduleRelayout();
    bool layoutPending() const;
    bool isInLayout() const { return m_layoutPhase == InLayout; }

    RenderObject* layoutRoot(bool onlyDuringLayout = false) const;
    void clearLayoutRoot() { m_layoutRoot = nullptr; }
    int layoutCount() const { return m_layoutCount; }

    bool needsLayout() const;
    void setNeedsLayout();
    void setViewportConstrainedObjectsNeedLayout();

    bool needsFullRepaint() const { return m_needsFullRepaint; }

    bool renderedCharactersExceed(unsigned threshold);

#if PLATFORM(IOS)
    bool useCustomFixedPositionLayoutRect() const { return m_useCustomFixedPositionLayoutRect; }
    IntRect customFixedPositionLayoutRect() const { return m_customFixedPositionLayoutRect; }
    void setCustomFixedPositionLayoutRect(const IntRect&);
    bool updateFixedPositionLayoutRect();

    IntSize customSizeForResizeEvent() const { return m_customSizeForResizeEvent; }
    void setCustomSizeForResizeEvent(IntSize);

    void setScrollVelocity(double horizontalVelocity, double verticalVelocity, double scaleChangeRate, double timestamp);
    FloatRect computeCoverageRect(double horizontalMargin, double verticalMargin) const;
#else
    bool useCustomFixedPositionLayoutRect() const { return false; }
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void serviceScriptedAnimations(double monotonicAnimationStartTime);
#endif

    void updateCompositingLayersAfterStyleChange();
    void updateCompositingLayersAfterLayout();
    bool flushCompositingStateForThisFrame(Frame* rootFrameForFlush);

    void clearBackingStores();
    void restoreBackingStores();

    // Called when changes to the GraphicsLayer hierarchy have to be synchronized with
    // content rendered via the normal painting path.
    void setNeedsOneShotDrawingSynchronization();

    GraphicsLayer* graphicsLayerForPlatformWidget(PlatformWidget);
    void scheduleLayerFlushAllowingThrottling();

    virtual TiledBacking* tiledBacking() const override;

    // In the future when any ScrollableArea can have a node in th ScrollingTree, this should
    // become a virtual function on ScrollableArea.
    uint64_t scrollLayerID() const;
    ScrollableArea* scrollableAreaForScrollLayerID(uint64_t) const;

    bool hasCompositedContent() const;
    bool hasCompositedContentIncludingDescendants() const;
    bool hasCompositingAncestor() const;
    void enterCompositingMode();
    bool isEnclosedInCompositingLayer() const;

    // Only used with accelerated compositing, but outside the #ifdef to make linkage easier.
    // Returns true if the flush was completed.
    bool flushCompositingStateIncludingSubframes();

    // Returns true when a paint with the PaintBehaviorFlattenCompositingLayers flag set gives
    // a faithful representation of the content.
    bool isSoftwareRenderable() const;

    void didMoveOnscreen();
    void willMoveOffscreen();
    void setIsInWindow(bool);

    void resetScrollbars();
    void resetScrollbarsAndClearContentsSize();
    void prepareForDetach();
    void detachCustomScrollbars();
    void recalculateScrollbarOverlayStyle();

    void clear();

    bool isTransparent() const;
    void setTransparent(bool isTransparent);
    
    // True if the FrameView is not transparent, and the base background color is opaque.
    bool hasOpaqueBackground() const;

    Color baseBackgroundColor() const;
    void setBaseBackgroundColor(const Color&);
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
    void setShouldUpdateWhileOffscreen(bool);
    bool shouldUpdate() const;

    void adjustViewSize();
    
    void setViewportSizeForCSSViewportUnits(IntSize);
    IntSize viewportSizeForCSSViewportUnits() const;
    
    virtual IntRect windowClipRect(bool clipToContents = true) const override;
    IntRect windowClipRectForFrameOwner(const HTMLFrameOwnerElement*, bool clipToLayerContents) const;

    virtual IntRect windowResizerRect() const override;

    virtual float visibleContentScaleFactor() const override;

#if USE(TILED_BACKING_STORE)
    virtual void setFixedVisibleContentRect(const IntRect&) override;
#endif
    virtual void setScrollPosition(const IntPoint&) override;
    void scrollPositionChangedViaPlatformWidget(const IntPoint& oldPosition, const IntPoint& newPosition);
    virtual void updateLayerPositionsAfterScrolling() override;
    virtual void updateCompositingLayersAfterScrolling() override;
    virtual bool requestScrollPositionUpdate(const IntPoint&) override;
    virtual bool isRubberBandInProgress() const override;
    virtual IntPoint minimumScrollPosition() const override;
    virtual IntPoint maximumScrollPosition() const override;
    void delayedScrollEventTimerFired(Timer<FrameView>&);

    void resumeVisibleImageAnimationsIncludingSubframes();

    // This is different than visibleContentRect() in that it ignores negative (or overly positive)
    // offsets from rubber-banding, and it takes zooming into account. 
    LayoutRect viewportConstrainedVisibleContentRect() const;

    String mediaType() const;
    void setMediaType(const String&);
    void adjustMediaTypeForPrinting(bool printing);

    void setCannotBlitToWindow();
    void setIsOverlapped(bool);
    bool isOverlapped() const { return m_isOverlapped; }
    bool isOverlappedIncludingAncestors() const;
    void setContentIsOpaque(bool);

    void addSlowRepaintObject(RenderElement*);
    void removeSlowRepaintObject(RenderElement*);
    bool hasSlowRepaintObject(RenderElement* o) const { return m_slowRepaintObjects && m_slowRepaintObjects->contains(o); }
    bool hasSlowRepaintObjects() const { return m_slowRepaintObjects && m_slowRepaintObjects->size(); }

    // Includes fixed- and sticky-position objects.
    typedef HashSet<RenderElement*> ViewportConstrainedObjectSet;
    void addViewportConstrainedObject(RenderElement*);
    void removeViewportConstrainedObject(RenderElement*);
    const ViewportConstrainedObjectSet* viewportConstrainedObjects() const { return m_viewportConstrainedObjects.get(); }
    bool hasViewportConstrainedObjects() const { return m_viewportConstrainedObjects && m_viewportConstrainedObjects->size() > 0; }

    // Functions for querying the current scrolled position, negating the effects of overhang
    // and adjusting for page scale.
    LayoutSize scrollOffsetForFixedPosition() const;
    // Static function can be called from another thread.
    static LayoutSize scrollOffsetForFixedPosition(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements, int headerHeight, int footerHeight);

    // These layers are positioned differently when there is a topContentInset, a header, or a footer. These value need to be computed
    // on both the main thread and the scrolling thread.
    static float yPositionForInsetClipLayer(const FloatPoint& scrollPosition, float topContentInset);
    static float yPositionForRootContentLayer(const FloatPoint& scrollPosition, float topContentInset, float headerHeight);
    static float yPositionForHeaderLayer(const FloatPoint& scrollPosition, float topContentInset);
    static float yPositionForFooterLayer(const FloatPoint& scrollPosition, float topContentInset, float totalContentsHeight, float footerHeight);

#if PLATFORM(IOS)
    LayoutRect viewportConstrainedObjectsRect() const;
    // Static function can be called from another thread.
    static LayoutRect rectForViewportConstrainedObjects(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements);
#endif
    
    bool fixedElementsLayoutRelativeToFrame() const;

    void disableLayerFlushThrottlingTemporarilyForInteraction();
    bool speculativeTilingEnabled() const { return m_speculativeTilingEnabled; }
    void loadProgressingStatusChanged();

#if ENABLE(DASHBOARD_SUPPORT)
    void updateAnnotatedRegions();
#endif
    void updateControlTints();

    void restoreScrollbar();

    void postLayoutTimerFired(Timer<FrameView>&);

    bool wasScrolledByUser() const;
    void setWasScrolledByUser(bool);

    bool safeToPropagateScrollToParent() const { return m_safeToPropagateScrollToParent; }
    void setSafeToPropagateScrollToParent(bool isSafe) { m_safeToPropagateScrollToParent = isSafe; }

    void addEmbeddedObjectToUpdate(RenderEmbeddedObject&);
    void removeEmbeddedObjectToUpdate(RenderEmbeddedObject&);

    virtual void paintContents(GraphicsContext*, const IntRect& dirtyRect) override;

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

    void willPaintContents(GraphicsContext*, const IntRect& dirtyRect, PaintingState&);
    void didPaintContents(GraphicsContext*, const IntRect& dirtyRect, PaintingState&);

    void setPaintBehavior(PaintBehavior);
    PaintBehavior paintBehavior() const;
    bool isPainting() const;
    bool hasEverPainted() const { return m_lastPaintTime; }
    void setLastPaintTime(double lastPaintTime) { m_lastPaintTime = lastPaintTime; }
    void setNodeToDraw(Node*);

    enum SelectionInSnapshot { IncludeSelection, ExcludeSelection };
    enum CoordinateSpaceForSnapshot { DocumentCoordinates, ViewCoordinates };
    void paintContentsForSnapshot(GraphicsContext*, const IntRect& imageRect, SelectionInSnapshot shouldPaintSelection, CoordinateSpaceForSnapshot);

    virtual void paintOverhangAreas(GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect) override;
    virtual void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect) override;
    virtual void paintScrollbar(GraphicsContext*, Scrollbar*, const IntRect&) override;

    Color documentBackgroundColor() const;

    bool isInChildFrameWithFrameFlattening() const;

    static double currentPaintTimeStamp() { return sCurrentPaintTimeStamp; } // returns 0 if not painting
    
    void updateLayoutAndStyleIfNeededRecursive();

    void incrementVisuallyNonEmptyCharacterCount(unsigned);
    void incrementVisuallyNonEmptyPixelCount(const IntSize&);
    void updateIsVisuallyNonEmpty();
    bool isVisuallyNonEmpty() const { return m_isVisuallyNonEmpty; }
    void enableAutoSizeMode(bool enable, const IntSize& minSize, const IntSize& maxSize);
    void setAutoSizeFixedMinimumHeight(int fixedMinimumHeight);
    IntSize autoSizingIntrinsicContentSize() const { return m_autoSizeContentSize; }

    void forceLayout(bool allowSubtree = false);
    void forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot);

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
    void adjustPageHeightDeprecated(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

    bool scrollToFragment(const URL&);
    bool scrollToAnchor(const String&);
    void maintainScrollPositionAtAnchor(Node*);
    void scrollElementToRect(Element*, const IntRect&);

    // Methods to convert points and rects between the coordinate space of the renderer, and this view.
    IntRect convertFromRendererToContainingView(const RenderElement*, const IntRect&) const;
    IntRect convertFromContainingViewToRenderer(const RenderElement*, const IntRect&) const;
    IntPoint convertFromRendererToContainingView(const RenderElement*, const IntPoint&) const;
    IntPoint convertFromContainingViewToRenderer(const RenderElement*, const IntPoint&) const;

    bool isFrameViewScrollCorner(RenderScrollbarPart* scrollCorner) const { return m_scrollCorner == scrollCorner; }

    bool isScrollable();

    enum ScrollbarModesCalculationStrategy { RulesFromWebContentOnly, AnyRule };
    void calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy = AnyRule);

    virtual IntPoint lastKnownMousePosition() const override;
    virtual bool isHandlingWheelEvent() const override;
    bool shouldSetCursor() const;

    // FIXME: Remove this method once plugin loading is decoupled from layout.
    void flushAnyPendingPostLayoutTasks();

    virtual bool shouldSuspendScrollAnimations() const override;
    virtual void scrollbarStyleChanged(int newStyle, bool forceUpdate) override;

    RenderBox* embeddedContentBox() const;
    
    void setTracksRepaints(bool);
    bool isTrackingRepaints() const { return m_isTrackingRepaints; }
    void resetTrackedRepaints();
    const Vector<FloatRect>& trackedRepaintRects() const { return m_trackedRepaintRects; }
    String trackedRepaintRectsAsText() const;

    typedef HashSet<ScrollableArea*> ScrollableAreaSet;
    // Returns whether the scrollable area has just been newly added.
    bool addScrollableArea(ScrollableArea*);
    // Returns whether the scrollable area has just been removed.
    bool removeScrollableArea(ScrollableArea*);
    bool containsScrollableArea(ScrollableArea*) const;
    const ScrollableAreaSet* scrollableAreas() const { return m_scrollableAreas.get(); }

    virtual void removeChild(Widget*) override;

    // This function exists for ports that need to handle wheel events manually.
    // On Mac WebKit1 the underlying NSScrollView just does the scrolling, but on most other platforms
    // we need this function in order to do the scroll ourselves.
    bool wheelEvent(const PlatformWheelEvent&);

    void setScrollingPerformanceLoggingEnabled(bool);

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

    virtual bool isActive() const override;
    virtual bool updatesScrollLayerPositionOnMainThread() const override;
    virtual bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override;

#if ENABLE(RUBBER_BANDING)
    GraphicsLayer* setWantsLayerForTopOverHangArea(bool) const;
    GraphicsLayer* setWantsLayerForBottomOverHangArea(bool) const;
#endif

    virtual int headerHeight() const override { return m_headerHeight; }
    void setHeaderHeight(int);
    virtual int footerHeight() const override { return m_footerHeight; }
    void setFooterHeight(int);

    virtual float topContentInset(TopContentInsetType = TopContentInsetType::WebCoreContentInset) const override;
    void topContentInsetDidChange(float newTopContentInset);

    virtual void willStartLiveResize() override;
    virtual void willEndLiveResize() override;

    void addPaintPendingMilestones(LayoutMilestones);
    void firePaintRelatedMilestonesIfNeeded();
    void fireLayoutRelatedMilestonesIfNeeded();
    LayoutMilestones milestonesPendingPaint() const { return m_milestonesPendingPaint; }

    bool visualUpdatesAllowedByClient() const { return m_visualUpdatesAllowedByClient; }
    void setVisualUpdatesAllowedByClient(bool);

    void setScrollPinningBehavior(ScrollPinningBehavior);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const;

    void updateWidgetPositions();
    void didAddWidgetToRenderTree(Widget&);
    void willRemoveWidgetFromRenderTree(Widget&);

    void addTrackedRepaintRect(const FloatRect&);

    // exposedRect represents WebKit's understanding of what part
    // of the view is actually exposed on screen (taking into account
    // clipping by other UI elements), whereas visibleContentRect is
    // internal to WebCore and doesn't respect those things.
    void setExposedRect(FloatRect);
    FloatRect exposedRect() const { return m_exposedRect; }

protected:
    virtual bool scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect) override;
    virtual void scrollContentsSlowPath(const IntRect& updateRect) override;
    
    void repaintSlowRepaintObjects();

    virtual bool isVerticalDocument() const override;
    virtual bool isFlippedDocument() const override;

private:
    explicit FrameView(Frame&);

    void reset();
    void init();

    enum LayoutPhase {
        OutsideLayout,
        InPreLayout,
        InPreLayoutStyleUpdate,
        InLayout,
        InViewSizeAdjust,
        InPostLayout,
    };
    LayoutPhase layoutPhase() const { return m_layoutPhase; }

    bool inPreLayoutStyleUpdate() const { return m_layoutPhase == InPreLayoutStyleUpdate; }

    virtual bool isFrameView() const override { return true; }

    friend class RenderWidget;
    bool useSlowRepaints(bool considerOverlap = true) const;
    bool useSlowRepaintsIfNotOverlapped() const;
    void updateCanBlitOnScrollRecursively();
    bool contentsInCompositedLayer() const;
    bool shouldLayoutAfterContentsResized() const;

    bool shouldUpdateCompositingLayersAfterScrolling() const;

    void applyOverflowToViewport(RenderElement*, ScrollbarMode& hMode, ScrollbarMode& vMode);
    void applyPaginationToViewport();

    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void paintControlTints();

    void forceLayoutParentViewIfNeeded();
    void performPostLayoutTasks();
    void autoSizeIfEnabled();

    void updateLayerFlushThrottling();
    void adjustTiledBackingCoverage();

    virtual void repaintContentRectangle(const IntRect&) override;
    virtual void contentsResized() override;
    virtual void visibleContentsResized() override;
    virtual void addedOrRemovedScrollbar() override;
    virtual void fixedLayoutSizeChanged() override;

    virtual void delegatesScrollingDidChange() override;

    // Override ScrollView methods to do point conversion via renderers, in order to
    // take transforms into account.
    virtual IntRect convertToContainingView(const IntRect&) const override;
    virtual IntRect convertFromContainingView(const IntRect&) const override;
    virtual IntPoint convertToContainingView(const IntPoint&) const override;
    virtual IntPoint convertFromContainingView(const IntPoint&) const override;

    // ScrollableArea interface
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) override;
    virtual void scrollTo(const IntSize&) override;
    virtual void setVisibleScrollerThumbRect(const IntRect&) override;
    virtual ScrollableArea* enclosingScrollableArea() const override;
    virtual IntRect scrollableAreaBoundingBox() const override;
    virtual bool scrollAnimatorEnabled() const override;
    virtual GraphicsLayer* layerForScrolling() const override;
    virtual GraphicsLayer* layerForHorizontalScrollbar() const override;
    virtual GraphicsLayer* layerForVerticalScrollbar() const override;
    virtual GraphicsLayer* layerForScrollCorner() const override;
#if ENABLE(RUBBER_BANDING)
    virtual GraphicsLayer* layerForOverhangAreas() const override;
#endif

    // Override scrollbar notifications to update the AXObject cache.
    virtual void didAddScrollbar(Scrollbar*, ScrollbarOrientation) override;
    virtual void willRemoveScrollbar(Scrollbar*, ScrollbarOrientation) override;

    IntSize sizeForResizeEvent() const;
    void sendResizeEventIfNeeded();

    void updateScrollableAreaSet();

    virtual void notifyPageThatContentAreaWillPaint() const override;

    void enableSpeculativeTilingIfNeeded();
    void speculativeTilingEnableTimerFired(Timer<FrameView>&);

    void updateEmbeddedObjectsTimerFired(Timer<FrameView>*);
    bool updateEmbeddedObjects();
    void updateEmbeddedObject(RenderEmbeddedObject&);
    void scrollToAnchor();
    void scrollPositionChanged(const IntPoint& oldPosition, const IntPoint& newPosition);
    void scrollableAreaSetChanged();
    void sendScrollEvent();

    bool hasCustomScrollbars() const;

    virtual void updateScrollCorner() override;

    FrameView* parentFrameView() const;

    void startLayoutAtMainFrameViewIfNeeded(bool allowSubtree);
    bool frameFlatteningEnabled() const;
    bool isFrameFlatteningValidForThisFrame() const;

    bool qualifiesAsVisuallyNonEmpty() const;

    AXObjectCache* axObjectCache() const;
    void notifyWidgetsInAllFrames(WidgetNotification);
    void removeFromAXObjectCache();
    void notifyWidgets(WidgetNotification);

    HashSet<Widget*> m_widgetsInRenderTree;

    static double sCurrentPaintTimeStamp; // used for detecting decoded resource thrash in the cache

    LayoutSize m_size;
    LayoutSize m_margins;

    std::unique_ptr<ListHashSet<RenderEmbeddedObject*>> m_embeddedObjectsToUpdate;
    const RefPtr<Frame> m_frame;

    std::unique_ptr<HashSet<RenderElement*>> m_slowRepaintObjects;

    bool m_needsFullRepaint;
    
    bool m_canHaveScrollbars;
    bool m_cannotBlitToWindow;
    bool m_isOverlapped;
    bool m_contentIsOpaque;

    Timer<FrameView> m_layoutTimer;
    bool m_delayedLayout;
    RenderElement* m_layoutRoot;

    LayoutPhase m_layoutPhase;
    bool m_layoutSchedulingEnabled;
    bool m_inSynchronousPostLayout;
    int m_layoutCount;
    unsigned m_nestedLayoutCount;
    Timer<FrameView> m_postLayoutTasksTimer;
    Timer<FrameView> m_updateEmbeddedObjectsTimer;
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
    RenderElement* m_viewportRenderer;

    Pagination m_pagination;

    bool m_wasScrolledByUser;
    bool m_inProgrammaticScroll;
    bool m_safeToPropagateScrollToParent;
    Timer<FrameView> m_delayedScrollEventTimer;

    double m_lastPaintTime;

    bool m_isTrackingRepaints; // Used for testing.
    Vector<FloatRect> m_trackedRepaintRects;

    bool m_shouldUpdateWhileOffscreen;

    FloatRect m_exposedRect;

    unsigned m_deferSetNeedsLayouts;
    bool m_setNeedsLayoutWasDeferred;

    RefPtr<Node> m_nodeToDraw;
    PaintBehavior m_paintBehavior;
    bool m_isPainting;

    unsigned m_visuallyNonEmptyCharacterCount;
    unsigned m_visuallyNonEmptyPixelCount;
    bool m_isVisuallyNonEmpty;
    bool m_firstVisuallyNonEmptyLayoutCallbackPending;

    RefPtr<Node> m_maintainScrollPositionAnchor;

    // Renderer to hold our custom scroll corner.
    RenderPtr<RenderScrollbarPart> m_scrollCorner;

    bool m_speculativeTilingEnabled;
    Timer<FrameView> m_speculativeTilingEnableTimer;

#if PLATFORM(IOS)
    bool m_useCustomFixedPositionLayoutRect;
    IntRect m_customFixedPositionLayoutRect;

    bool m_useCustomSizeForResizeEvent;
    IntSize m_customSizeForResizeEvent;

    double m_horizontalVelocity;
    double m_verticalVelocity;
    double m_scaleChangeRate;
    double m_lastVelocityUpdateTime;
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
    
    ScrollPinningBehavior m_scrollPinningBehavior;
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

WIDGET_TYPE_CASTS(FrameView, isFrameView());

} // namespace WebCore

#endif // FrameView_h
