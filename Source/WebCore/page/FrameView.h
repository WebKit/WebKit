/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004-2017 Apple Inc. All rights reserved.

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
#include "FrameViewLayoutContext.h"
#include "GraphicsContext.h"
#include "LayoutMilestone.h"
#include "LayoutRect.h"
#include "Pagination.h"
#include "PaintPhase.h"
#include "RenderPtr.h"
#include "ScrollView.h"
#include "StyleColor.h"
#include "TiledBacking.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>
#include <wtf/ListHashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AXObjectCache;
class Element;
class FloatSize;
class Frame;
class HTMLFrameOwnerElement;
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

enum class FrameFlattening;
enum class SelectionRevealMode;

Pagination::Mode paginationModeForRenderStyle(const RenderStyle&);

class FrameView final : public ScrollView {
    WTF_MAKE_ISO_ALLOCATED(FrameView);
public:
    friend class RenderView;
    friend class Internals;
    friend class FrameViewLayoutContext;

    WEBCORE_EXPORT static Ref<FrameView> create(Frame&);
    static Ref<FrameView> create(Frame&, const IntSize& initialSize);

    virtual ~FrameView();

    HostWindow* hostWindow() const final;
    
    WEBCORE_EXPORT void invalidateRect(const IntRect&) final;
    void setFrameRect(const IntRect&) final;

    bool scheduleAnimation() final;

    Frame& frame() const { return m_frame; }

    WEBCORE_EXPORT RenderView* renderView() const;

    int mapFromLayoutToCSSUnits(LayoutUnit) const;
    LayoutUnit mapFromCSSToLayoutUnits(int) const;

    LayoutUnit marginWidth() const { return m_margins.width(); } // -1 means default
    LayoutUnit marginHeight() const { return m_margins.height(); } // -1 means default
    void setMarginWidth(LayoutUnit);
    void setMarginHeight(LayoutUnit);

    WEBCORE_EXPORT void setCanHaveScrollbars(bool) final;
    WEBCORE_EXPORT void updateCanHaveScrollbars();

    Ref<Scrollbar> createScrollbar(ScrollbarOrientation) final;

    bool avoidScrollbarCreation() const final;

    void setContentsSize(const IntSize&) final;
    void updateContentsSize() final;

    const FrameViewLayoutContext& layoutContext() const { return m_layoutContext; }
    FrameViewLayoutContext& layoutContext() { return m_layoutContext; }

    WEBCORE_EXPORT bool didFirstLayout() const;
    void queuePostLayoutCallback(WTF::Function<void ()>&&);

    WEBCORE_EXPORT bool needsLayout() const;
    WEBCORE_EXPORT void setNeedsLayout();
    void setViewportConstrainedObjectsNeedLayout();

    WEBCORE_EXPORT bool renderedCharactersExceed(unsigned threshold);

    void scheduleSelectionUpdate();

#if PLATFORM(IOS_FAMILY)
    bool useCustomFixedPositionLayoutRect() const;
    IntRect customFixedPositionLayoutRect() const { return m_customFixedPositionLayoutRect; }
    WEBCORE_EXPORT void setCustomFixedPositionLayoutRect(const IntRect&);
    bool updateFixedPositionLayoutRect();

    IntSize customSizeForResizeEvent() const { return m_customSizeForResizeEvent; }
    WEBCORE_EXPORT void setCustomSizeForResizeEvent(IntSize);

    WEBCORE_EXPORT void setScrollVelocity(double horizontalVelocity, double verticalVelocity, double scaleChangeRate, MonotonicTime timestamp);
#else
    bool useCustomFixedPositionLayoutRect() const { return false; }
#endif

    void willRecalcStyle();
    bool updateCompositingLayersAfterStyleChange();
    void updateCompositingLayersAfterLayout();

    // Called when changes to the GraphicsLayer hierarchy have to be synchronized with
    // content rendered via the normal painting path.
    void setNeedsOneShotDrawingSynchronization();

    WEBCORE_EXPORT GraphicsLayer* graphicsLayerForPlatformWidget(PlatformWidget);
    WEBCORE_EXPORT void scheduleLayerFlushAllowingThrottling();

    WEBCORE_EXPORT TiledBacking* tiledBacking() const final;

    // In the future when any ScrollableArea can have a node in th ScrollingTree, this should
    // become a virtual function on ScrollableArea.
    uint64_t scrollLayerID() const;
    ScrollableArea* scrollableAreaForScrollLayerID(uint64_t) const;

    WEBCORE_EXPORT void enterCompositingMode();
    WEBCORE_EXPORT bool isEnclosedInCompositingLayer() const;

    // Only used with accelerated compositing, but outside the #ifdef to make linkage easier.
    // Returns true if the flush was completed.
    WEBCORE_EXPORT bool flushCompositingStateIncludingSubframes();

    // Returns true when a paint with the PaintBehavior::FlattenCompositingLayers flag set gives
    // a faithful representation of the content.
    WEBCORE_EXPORT bool isSoftwareRenderable() const;

    void setIsInWindow(bool);

    void resetScrollbars();
    void resetScrollbarsAndClearContentsSize();
    void prepareForDetach();
    void detachCustomScrollbars();
    WEBCORE_EXPORT void recalculateScrollbarOverlayStyle();
#if ENABLE(DARK_MODE_CSS)
    void recalculateBaseBackgroundColor();
#endif

    void clear();
    void resetLayoutMilestones();

    WEBCORE_EXPORT bool isTransparent() const;
    WEBCORE_EXPORT void setTransparent(bool isTransparent);
    
    // True if the FrameView is not transparent, and the base background color is opaque.
    bool hasOpaqueBackground() const;

    WEBCORE_EXPORT Color baseBackgroundColor() const;
    WEBCORE_EXPORT void setBaseBackgroundColor(const Color&);
    WEBCORE_EXPORT void updateBackgroundRecursively(bool);

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
    void clearViewportSizeOverrideForCSSViewportUnits();
    IntSize viewportSizeForCSSViewportUnits() const;

    IntRect windowClipRect() const final;
    WEBCORE_EXPORT IntRect windowClipRectForFrameOwner(const HTMLFrameOwnerElement*, bool clipToLayerContents) const;

    float visibleContentScaleFactor() const final;

#if USE(COORDINATED_GRAPHICS)
    WEBCORE_EXPORT void setFixedVisibleContentRect(const IntRect&) final;
#endif
    WEBCORE_EXPORT void setScrollPosition(const ScrollPosition&) final;
    void restoreScrollbar();
    void scheduleScrollToFocusedElement(SelectionRevealMode);
    void scrollToFocusedElementImmediatelyIfNeeded();
    void updateLayerPositionsAfterScrolling() final;
    void updateCompositingLayersAfterScrolling() final;
    bool requestScrollPositionUpdate(const ScrollPosition&) final;
    bool isRubberBandInProgress() const final;
    WEBCORE_EXPORT ScrollPosition minimumScrollPosition() const final;
    WEBCORE_EXPORT ScrollPosition maximumScrollPosition() const final;

    // The scrollOrigin, scrollPosition, minimumScrollPosition and maximumScrollPosition are all affected by frame scale,
    // but layoutViewport computations require unscaled scroll positions.
    ScrollPosition unscaledMinimumScrollPosition() const;
    ScrollPosition unscaledMaximumScrollPosition() const;

    IntPoint unscaledScrollOrigin() const;

    WEBCORE_EXPORT LayoutPoint minStableLayoutViewportOrigin() const;
    WEBCORE_EXPORT LayoutPoint maxStableLayoutViewportOrigin() const;

    enum class TriggerLayoutOrNot {
        No,
        Yes
    };
    // This origin can be overridden by setLayoutViewportOverrideRect.
    void setBaseLayoutViewportOrigin(LayoutPoint, TriggerLayoutOrNot = TriggerLayoutOrNot::Yes);
    // This size can be overridden by setLayoutViewportOverrideRect.
    WEBCORE_EXPORT LayoutSize baseLayoutViewportSize() const;
    
    // If set, overrides the default "m_layoutViewportOrigin, size of initial containing block" rect.
    // Used with delegated scrolling (i.e. iOS).
    WEBCORE_EXPORT void setLayoutViewportOverrideRect(std::optional<LayoutRect>, TriggerLayoutOrNot = TriggerLayoutOrNot::Yes);

    WEBCORE_EXPORT void setVisualViewportOverrideRect(std::optional<LayoutRect>);

    // These are in document coordinates, unaffected by page scale (but affected by zooming).
    WEBCORE_EXPORT LayoutRect layoutViewportRect() const;
    WEBCORE_EXPORT LayoutRect visualViewportRect() const;
    
    static LayoutRect visibleDocumentRect(const FloatRect& visibleContentRect, float headerHeight, float footerHeight, const FloatSize& totalContentsSize, float pageScaleFactor);

    // This is different than visibleContentRect() in that it ignores negative (or overly positive)
    // offsets from rubber-banding, and it takes zooming into account. 
    LayoutRect viewportConstrainedVisibleContentRect() const;
    
    LayoutRect rectForFixedPositionLayout() const;

    void viewportContentsChanged();
    WEBCORE_EXPORT void resumeVisibleImageAnimationsIncludingSubframes();

    String mediaType() const;
    WEBCORE_EXPORT void setMediaType(const String&);
    void adjustMediaTypeForPrinting(bool printing);

    void setCannotBlitToWindow();
    void setIsOverlapped(bool);
    void setContentIsOpaque(bool);

    void addSlowRepaintObject(RenderElement&);
    void removeSlowRepaintObject(RenderElement&);
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
    LayoutPoint scrollPositionForFixedPosition() const;
    
    // Static function can be called from another thread.
    static LayoutPoint scrollPositionForFixedPosition(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements, int headerHeight, int footerHeight);

    WEBCORE_EXPORT static LayoutSize expandedLayoutViewportSize(const LayoutSize& baseLayoutViewportSize, const LayoutSize& documentSize, double heightExpansionFactor);

    enum class LayoutViewportConstraint { ConstrainedToDocumentRect, Unconstrained };
    WEBCORE_EXPORT static LayoutRect computeUpdatedLayoutViewportRect(const LayoutRect& layoutViewport, const LayoutRect& documentRect, const LayoutSize& unobscuredContentSize, const LayoutRect& unobscuredContentRect, const LayoutSize& baseLayoutViewportSize, const LayoutPoint& stableLayoutViewportOriginMin, const LayoutPoint& stableLayoutViewportOriginMax, LayoutViewportConstraint);
    
    WEBCORE_EXPORT static LayoutPoint computeLayoutViewportOrigin(const LayoutRect& visualViewport, const LayoutPoint& stableLayoutViewportOriginMin, const LayoutPoint& stableLayoutViewportOriginMax, const LayoutRect& layoutViewport, ScrollBehaviorForFixedElements);

    // These layers are positioned differently when there is a topContentInset, a header, or a footer. These value need to be computed
    // on both the main thread and the scrolling thread.
    static float yPositionForInsetClipLayer(const FloatPoint& scrollPosition, float topContentInset);
    WEBCORE_EXPORT static FloatPoint positionForRootContentLayer(const FloatPoint& scrollPosition, const FloatPoint& scrollOrigin, float topContentInset, float headerHeight);
    WEBCORE_EXPORT FloatPoint positionForRootContentLayer() const;

    static float yPositionForHeaderLayer(const FloatPoint& scrollPosition, float topContentInset);
    static float yPositionForFooterLayer(const FloatPoint& scrollPosition, float topContentInset, float totalContentsHeight, float footerHeight);

#if PLATFORM(IOS_FAMILY)
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

    WEBCORE_EXPORT bool wasScrolledByUser() const;
    WEBCORE_EXPORT void setWasScrolledByUser(bool);

    bool safeToPropagateScrollToParent() const;

    void addEmbeddedObjectToUpdate(RenderEmbeddedObject&);
    void removeEmbeddedObjectToUpdate(RenderEmbeddedObject&);

    WEBCORE_EXPORT void paintContents(GraphicsContext&, const IntRect& dirtyRect, SecurityOriginPaintPolicy = SecurityOriginPaintPolicy::AnyOrigin) final;

    struct PaintingState {
        OptionSet<PaintBehavior> paintBehavior;
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

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void didReplaceMultipartContent();
#endif

    WEBCORE_EXPORT void setPaintBehavior(OptionSet<PaintBehavior>);
    WEBCORE_EXPORT OptionSet<PaintBehavior> paintBehavior() const;
    bool isPainting() const;
    bool hasEverPainted() const { return !!m_lastPaintTime; }
    void setLastPaintTime(MonotonicTime lastPaintTime) { m_lastPaintTime = lastPaintTime; }
    WEBCORE_EXPORT void setNodeToDraw(Node*);

    enum SelectionInSnapshot { IncludeSelection, ExcludeSelection };
    enum CoordinateSpaceForSnapshot { DocumentCoordinates, ViewCoordinates };
    WEBCORE_EXPORT void paintContentsForSnapshot(GraphicsContext&, const IntRect& imageRect, SelectionInSnapshot shouldPaintSelection, CoordinateSpaceForSnapshot);

    void paintOverhangAreas(GraphicsContext&, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect) final;
    void paintScrollCorner(GraphicsContext&, const IntRect& cornerRect) final;
    void paintScrollbar(GraphicsContext&, Scrollbar&, const IntRect&) final;

    WEBCORE_EXPORT Color documentBackgroundColor() const;

    bool isInChildFrameWithFrameFlattening() const;

    void startDisallowingLayout() { layoutContext().startDisallowingLayout(); }
    void endDisallowingLayout() { layoutContext().endDisallowingLayout(); }

    static MonotonicTime currentPaintTimeStamp() { return sCurrentPaintTimeStamp; } // returns 0 if not painting
    
    WEBCORE_EXPORT void updateLayoutAndStyleIfNeededRecursive();

    void incrementVisuallyNonEmptyCharacterCount(const String&);
    void incrementVisuallyNonEmptyPixelCount(const IntSize&);
    void updateIsVisuallyNonEmpty();
    void updateSignificantRenderedTextMilestoneIfNeeded();
    bool isVisuallyNonEmpty() const { return m_isVisuallyNonEmpty; }
    WEBCORE_EXPORT void enableAutoSizeMode(bool enable, const IntSize& minSize, const IntSize& maxSize);
    WEBCORE_EXPORT void setAutoSizeFixedMinimumHeight(int);
    IntSize autoSizingIntrinsicContentSize() const { return m_autoSizeContentSize; }

    WEBCORE_EXPORT void forceLayout(bool allowSubtreeLayout = false);
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

    // Coordinate systems:
    //
    // "View"
    //     Top left is top left of the FrameView/ScrollView/Widget. Size is Widget::boundsRect().size(). 
    //
    // "TotalContents"
    //    Relative to ScrollView's scrolled contents, including headers and footers. Size is totalContentsSize().
    //
    // "Contents"
    //    Relative to ScrollView's scrolled contents, excluding headers and footers, so top left is top left of the scroll view's
    //    document, and size is contentsSize().
    //
    // "Absolute"
    //    Relative to the document's scroll origin (non-zero for RTL documents), but affected by page zoom and page scale. Mostly used
    //    in rendering code.
    //
    // "Document"
    //    Relative to the document's scroll origin, but not affected by page zoom or page scale. Size is equivalent to CSS pixel dimensions.
    //    FIXME: some uses are affected by page zoom (e.g. layout and visual viewports).
    //
    // "Client"
    //    Relative to the visible part of the document (or, more strictly, the layout viewport rect), and with the same scaling
    //    as Document coordinates, i.e. matching CSS pixels. Affected by scroll origin.
    //
    // "LayoutViewport"
    //    Similar to client coordinates, but affected by page zoom (but not page scale).
    //

    // Methods to convert points and rects between the coordinate space of the renderer, and this view.
    WEBCORE_EXPORT IntRect convertFromRendererToContainingView(const RenderElement*, const IntRect&) const;
    WEBCORE_EXPORT IntRect convertFromContainingViewToRenderer(const RenderElement*, const IntRect&) const;
    WEBCORE_EXPORT FloatRect convertFromContainingViewToRenderer(const RenderElement*, const FloatRect&) const;
    WEBCORE_EXPORT IntPoint convertFromRendererToContainingView(const RenderElement*, const IntPoint&) const;
    WEBCORE_EXPORT IntPoint convertFromContainingViewToRenderer(const RenderElement*, const IntPoint&) const;

    // Override ScrollView methods to do point conversion via renderers, in order to take transforms into account.
    IntRect convertToContainingView(const IntRect&) const final;
    IntRect convertFromContainingView(const IntRect&) const final;
    FloatRect convertFromContainingView(const FloatRect&) const final;
    IntPoint convertToContainingView(const IntPoint&) const final;
    IntPoint convertFromContainingView(const IntPoint&) const final;

    float documentToAbsoluteScaleFactor(std::optional<float> effectiveZoom = std::nullopt) const;
    float absoluteToDocumentScaleFactor(std::optional<float> effectiveZoom = std::nullopt) const;

    FloatRect absoluteToDocumentRect(FloatRect, std::optional<float> effectiveZoom = std::nullopt) const;
    FloatPoint absoluteToDocumentPoint(FloatPoint, std::optional<float> effectiveZoom = std::nullopt) const;

    FloatRect absoluteToClientRect(FloatRect, std::optional<float> effectiveZoom = std::nullopt) const;

    FloatSize documentToClientOffset() const;
    FloatRect documentToClientRect(FloatRect) const;
    FloatPoint documentToClientPoint(FloatPoint) const;
    WEBCORE_EXPORT FloatRect clientToDocumentRect(FloatRect) const;
    WEBCORE_EXPORT FloatPoint clientToDocumentPoint(FloatPoint) const;

    FloatPoint layoutViewportToAbsolutePoint(FloatPoint) const;

    // Unlike client coordinates, layout viewport coordinates are affected by page zoom.
    FloatPoint clientToLayoutViewportPoint(FloatPoint) const;

    bool isFrameViewScrollCorner(const RenderScrollbarPart& scrollCorner) const { return m_scrollCorner.get() == &scrollCorner; }

    // isScrollable() takes an optional Scrollability parameter that allows the caller to define what they mean by 'scrollable.'
    // Most callers are interested in the default value, Scrollability::Scrollable, which means that there is actually content
    // to scroll to, and a scrollbar that will allow you to access it. In some cases, callers want to know if the FrameView is allowed
    // to rubber-band, which the main frame might be allowed to do even if there is no content to scroll to. In that case,
    // callers use Scrollability::ScrollableOrRubberbandable.
    enum class Scrollability { Scrollable, ScrollableOrRubberbandable };
    WEBCORE_EXPORT bool isScrollable(Scrollability definitionOfScrollable = Scrollability::Scrollable);

    bool isScrollableOrRubberbandable() final;
    bool hasScrollableOrRubberbandableAncestor() final;

    enum ScrollbarModesCalculationStrategy { RulesFromWebContentOnly, AnyRule };
    void calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy = AnyRule);

    IntPoint lastKnownMousePosition() const final;
    bool isHandlingWheelEvent() const final;
    bool shouldSetCursor() const;

    WEBCORE_EXPORT bool useDarkAppearance() const final;
    OptionSet<StyleColor::Options> styleColorOptions() const;

    // FIXME: Remove this method once plugin loading is decoupled from layout.
    void flushAnyPendingPostLayoutTasks();

    bool shouldSuspendScrollAnimations() const final;
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

    WEBCORE_EXPORT void addChild(Widget&) final;
    WEBCORE_EXPORT void removeChild(Widget&) final;

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
    
    bool inProgrammaticScroll() const final { return m_inProgrammaticScroll; }
    void setInProgrammaticScroll(bool programmaticScroll) { m_inProgrammaticScroll = programmaticScroll; }

#if ENABLE(CSS_DEVICE_ADAPTATION)
    IntSize initialViewportSize() const { return m_initialViewportSize; }
    void setInitialViewportSize(const IntSize& size) { m_initialViewportSize = size; }
#endif

    bool isActive() const final;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;

#if ENABLE(RUBBER_BANDING)
    WEBCORE_EXPORT GraphicsLayer* setWantsLayerForTopOverHangArea(bool) const;
    WEBCORE_EXPORT GraphicsLayer* setWantsLayerForBottomOverHangArea(bool) const;
#endif

    // This function "smears" the "position:fixed" uninflatedBounds for scrolling, returning a rect that is the union of
    // all possible locations of the given rect under page scrolling.
    LayoutRect fixedScrollableAreaBoundsInflatedForScrolling(const LayoutRect& uninflatedBounds) const;

    LayoutPoint scrollPositionRespectingCustomFixedPosition() const;

    int headerHeight() const final { return m_headerHeight; }
    WEBCORE_EXPORT void setHeaderHeight(int);
    int footerHeight() const final { return m_footerHeight; }
    WEBCORE_EXPORT void setFooterHeight(int);

    WEBCORE_EXPORT float topContentInset(TopContentInsetType = TopContentInsetType::WebCoreContentInset) const final;
    void topContentInsetDidChange(float newTopContentInset);

    void topContentDirectionDidChange();

    WEBCORE_EXPORT void willStartLiveResize() final;
    WEBCORE_EXPORT void willEndLiveResize() final;

    WEBCORE_EXPORT void availableContentSizeChanged(AvailableSizeChangeReason) final;

    void updateTiledBackingAdaptiveSizing();
    TiledBacking::Scrollability computeScrollability() const;

    void addPaintPendingMilestones(OptionSet<LayoutMilestone>);
    void firePaintRelatedMilestonesIfNeeded();
    void fireLayoutRelatedMilestonesIfNeeded();
    OptionSet<LayoutMilestone> milestonesPendingPaint() const { return m_milestonesPendingPaint; }

    bool visualUpdatesAllowedByClient() const { return m_visualUpdatesAllowedByClient; }
    WEBCORE_EXPORT void setVisualUpdatesAllowedByClient(bool);

    WEBCORE_EXPORT void setScrollPinningBehavior(ScrollPinningBehavior);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const;

    bool hasFlippedBlockRenderers() const { return m_hasFlippedBlockRenderers; }
    void setHasFlippedBlockRenderers(bool b) { m_hasFlippedBlockRenderers = b; }

    void updateWidgetPositions();
    void scheduleUpdateWidgetPositions();

    void didAddWidgetToRenderTree(Widget&);
    void willRemoveWidgetFromRenderTree(Widget&);

    const HashSet<Widget*>& widgetsInRenderTree() const { return m_widgetsInRenderTree; }

    void addTrackedRepaintRect(const FloatRect&);

    // exposedRect represents WebKit's understanding of what part
    // of the view is actually exposed on screen (taking into account
    // clipping by other UI elements), whereas visibleContentRect is
    // internal to WebCore and doesn't respect those things.
    WEBCORE_EXPORT void setViewExposedRect(std::optional<FloatRect>);
    std::optional<FloatRect> viewExposedRect() const { return m_viewExposedRect; }

#if ENABLE(CSS_SCROLL_SNAP)
    void updateSnapOffsets() final;
    bool isScrollSnapInProgress() const final;
    void updateScrollingCoordinatorScrollSnapProperties() const;
#endif

    float adjustScrollStepForFixedContent(float step, ScrollbarOrientation, ScrollGranularity) final;

    void didChangeScrollOffset();

    void show() final;
    void hide() final;

    bool shouldPlaceBlockDirectionScrollbarOnLeft() const final;

    void didRestoreFromPageCache();

    void willDestroyRenderTree();
    void didDestroyRenderTree();

    void setSpeculativeTilingDelayDisabledForTesting(bool disabled) { m_speculativeTilingDelayDisabledForTesting = disabled; }

    WEBCORE_EXPORT FrameFlattening effectiveFrameFlattening() const;

    WEBCORE_EXPORT void traverseForPaintInvalidation(GraphicsContext::PaintInvalidationReasons);
    void invalidateControlTints() { traverseForPaintInvalidation(GraphicsContext::PaintInvalidationReasons::InvalidatingControlTints); }
    void invalidateImagesWithAsyncDecodes() { traverseForPaintInvalidation(GraphicsContext::PaintInvalidationReasons::InvalidatingImagesWithAsyncDecodes); }

protected:
    bool scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect) final;
    void scrollContentsSlowPath(const IntRect& updateRect) final;
    
    void repaintSlowRepaintObjects();

    bool isVerticalDocument() const final;
    bool isFlippedDocument() const final;

private:
    explicit FrameView(Frame&);

    void reset();
    void init();

    enum LayoutPhase {
        OutsideLayout,
        InPreLayout,
        InRenderTreeLayout,
        InViewSizeAdjust,
        InPostLayout
    };

    bool isFrameView() const final { return true; }

    friend class RenderWidget;
    bool useSlowRepaints(bool considerOverlap = true) const;
    bool useSlowRepaintsIfNotOverlapped() const;
    void updateCanBlitOnScrollRecursively();
    bool shouldLayoutAfterContentsResized() const;

    bool shouldUpdateCompositingLayersAfterScrolling() const;
    bool flushCompositingStateForThisFrame(const Frame& rootFrameForFlush);

    bool shouldDeferScrollUpdateAfterContentSizeChange() final;

    void scrollOffsetChangedViaPlatformWidgetImpl(const ScrollOffset& oldOffset, const ScrollOffset& newOffset) final;

    void applyOverflowToViewport(const RenderElement&, ScrollbarMode& hMode, ScrollbarMode& vMode);
    void applyPaginationToViewport();

    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void forceLayoutParentViewIfNeeded();
    void flushPostLayoutTasksQueue();
    void performPostLayoutTasks();
    void autoSizeIfEnabled();

    void applyRecursivelyWithVisibleRect(const WTF::Function<void (FrameView& frameView, const IntRect& visibleRect)>&);
    void resumeVisibleImageAnimations(const IntRect& visibleRect);
    void updateScriptedAnimationsAndTimersThrottlingState(const IntRect& visibleRect);

    void updateLayerFlushThrottling();
    WEBCORE_EXPORT void adjustTiledBackingCoverage();

    void repaintContentRectangle(const IntRect&) final;
    void addedOrRemovedScrollbar() final;

    void scrollToFocusedElementTimerFired();
    void scrollToFocusedElementInternal();

    void delegatesScrollingDidChange() final;

    // ScrollableArea interface
    void invalidateScrollbarRect(Scrollbar&, const IntRect&) final;
    void scrollTo(const ScrollPosition&) final;
    void setVisibleScrollerThumbRect(const IntRect&) final;
    ScrollableArea* enclosingScrollableArea() const final;
    IntRect scrollableAreaBoundingBox(bool* = nullptr) const final;
    bool scrollAnimatorEnabled() const final;
    GraphicsLayer* layerForScrolling() const final;
    GraphicsLayer* layerForHorizontalScrollbar() const final;
    GraphicsLayer* layerForVerticalScrollbar() const final;
    GraphicsLayer* layerForScrollCorner() const final;
#if ENABLE(RUBBER_BANDING)
    GraphicsLayer* layerForOverhangAreas() const final;
#endif
    void contentsResized() final;

#if PLATFORM(IOS_FAMILY)
    void unobscuredContentSizeChanged() final;
#endif

#if ENABLE(DARK_MODE_CSS)
    RenderObject* rendererForSupportedColorSchemes() const;
#endif

    bool usesCompositedScrolling() const final;
    bool usesAsyncScrolling() const final;
    bool usesMockScrollAnimator() const final;
    void logMockScrollAnimatorMessage(const String&) const final;

    // Override scrollbar notifications to update the AXObject cache.
    void didAddScrollbar(Scrollbar*, ScrollbarOrientation) final;
    void willRemoveScrollbar(Scrollbar*, ScrollbarOrientation) final;

    IntSize sizeForResizeEvent() const;
    void sendResizeEventIfNeeded();

    void adjustScrollbarsForLayout(bool firstLayout);

    void handleDeferredScrollbarsUpdateAfterDirectionChange();

    void updateScrollableAreaSet();
    void updateLayoutViewport();

    void notifyPageThatContentAreaWillPaint() const final;

    void enableSpeculativeTilingIfNeeded();
    void speculativeTilingEnableTimerFired();

    void updateEmbeddedObjectsTimerFired();
    bool updateEmbeddedObjects();
    void updateEmbeddedObject(RenderEmbeddedObject&);

    void updateWidgetPositionsTimerFired();

    void scrollToAnchor();
    void scrollPositionChanged(const ScrollPosition& oldPosition, const ScrollPosition& newPosition);
    void scrollableAreaSetChanged();
    void sendScrollEvent();
    void resetScrollAnchor();

    bool hasCustomScrollbars() const;

    void updateScrollCorner() final;

    FrameView* parentFrameView() const;

    bool frameFlatteningEnabled() const;
    bool isFrameFlatteningValidForThisFrame() const;

    void markRootOrBodyRendererDirty() const;

    bool qualifiesAsVisuallyNonEmpty() const;
    bool isViewForDocumentInFrame() const;

    AXObjectCache* axObjectCache() const;
    void notifyWidgetsInAllFrames(WidgetNotification);
    void removeFromAXObjectCache();
    void notifyWidgets(WidgetNotification);

    RenderElement* viewportRenderer() const;
    
    void willDoLayout(WeakPtr<RenderElement> layoutRoot);
    void didLayout(WeakPtr<RenderElement> layoutRoot);

    struct OverrideViewportSize {
        std::optional<int> width;
        std::optional<int> height;

        bool operator==(const OverrideViewportSize& rhs) const { return rhs.width == width && rhs.height == height; }
    };
    void overrideViewportSizeForCSSViewportUnits(OverrideViewportSize);

    HashSet<Widget*> m_widgetsInRenderTree;

    static MonotonicTime sCurrentPaintTimeStamp; // used for detecting decoded resource thrash in the cache

    LayoutSize m_size;
    LayoutSize m_margins;

    std::unique_ptr<ListHashSet<RenderEmbeddedObject*>> m_embeddedObjectsToUpdate;
    const Ref<Frame> m_frame;

    std::unique_ptr<HashSet<const RenderElement*>> m_slowRepaintObjects;

    bool m_canHaveScrollbars;
    bool m_cannotBlitToWindow;
    bool m_isOverlapped { false };
    bool m_contentIsOpaque;

    Timer m_updateEmbeddedObjectsTimer;
    Timer m_updateWidgetPositionsTimer;

    bool m_firstLayoutCallbackPending;

    bool m_isTransparent;
#if ENABLE(DARK_MODE_CSS)
    bool m_usesDarkAppearance { false };
#endif
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
    Timer m_delayedScrollEventTimer;
    bool m_shouldScrollToFocusedElement { false };
    SelectionRevealMode m_selectionRevealModeForFocusedElement;
    Timer m_delayedScrollToFocusedElementTimer;

    MonotonicTime m_lastPaintTime;

    bool m_isTrackingRepaints; // Used for testing.
    Vector<FloatRect> m_trackedRepaintRects;

    bool m_shouldUpdateWhileOffscreen;

    std::optional<FloatRect> m_viewExposedRect;
    
    LayoutPoint m_layoutViewportOrigin;
    std::optional<LayoutRect> m_layoutViewportOverrideRect;
    std::optional<LayoutRect> m_visualViewportOverrideRect; // Used when the iOS keyboard is showing.

    RefPtr<Node> m_nodeToDraw;
    OptionSet<PaintBehavior> m_paintBehavior;
    bool m_isPainting;

    unsigned m_visuallyNonEmptyCharacterCount;
    unsigned m_visuallyNonEmptyPixelCount;
    bool m_isVisuallyNonEmpty;
    bool m_firstVisuallyNonEmptyLayoutCallbackPending;

    unsigned m_textRendererCountForVisuallyNonEmptyCharacters { 0 };
    bool m_renderedSignificantAmountOfText;
    bool m_significantRenderedTextMilestonePending;

    bool m_needsDeferredScrollbarsUpdate { false };

    RefPtr<ContainerNode> m_maintainScrollPositionAnchor;

    // Renderer to hold our custom scroll corner.
    RenderPtr<RenderScrollbarPart> m_scrollCorner;

    bool m_speculativeTilingEnabled;
    Timer m_speculativeTilingEnableTimer;

#if PLATFORM(IOS_FAMILY)
    bool m_useCustomFixedPositionLayoutRect;
    IntRect m_customFixedPositionLayoutRect;

    bool m_useCustomSizeForResizeEvent;
    IntSize m_customSizeForResizeEvent;
#endif

    std::optional<OverrideViewportSize> m_overrideViewportSize;

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

    OptionSet<LayoutMilestone> m_milestonesPendingPaint;

    static const unsigned visualCharacterThreshold = 200;
    static const unsigned visualPixelThreshold = 32 * 32;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    // Size of viewport before any UA or author styles have overridden
    // the viewport given by the window or viewing area of the UA.
    IntSize m_initialViewportSize;
#endif

    bool m_visualUpdatesAllowedByClient;
    bool m_hasFlippedBlockRenderers;
    bool m_speculativeTilingDelayDisabledForTesting { false };

    ScrollPinningBehavior m_scrollPinningBehavior;

    IntRect* m_cachedWindowClipRect { nullptr };
    Vector<WTF::Function<void ()>> m_postLayoutCallbackQueue;

    FrameViewLayoutContext m_layoutContext;
};

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
