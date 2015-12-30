/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Holger Hans Peter Freyther
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

#ifndef ScrollView_h
#define ScrollView_h

#include "FloatRect.h"
#include "IntRect.h"
#include "Scrollbar.h"
#include "ScrollableArea.h"
#include "ScrollTypes.h"
#include "Widget.h"

#include <wtf/HashSet.h>

#if PLATFORM(IOS)
OBJC_CLASS WAKScrollView;
OBJC_CLASS WAKView;

#ifndef NSScrollView
#define NSScrollView WAKScrollView
#endif

#ifndef NSView
#define NSView WAKView
#endif
#endif // PLATFORM(IOS)

#if PLATFORM(COCOA) && defined __OBJC__
@class NSScrollView;
@protocol WebCoreFrameScrollView;
#endif

namespace WebCore {

class HostWindow;
class LegacyTileCache;
class Scrollbar;

class ScrollView : public Widget, public ScrollableArea {
public:
    virtual ~ScrollView();

    // ScrollableArea functions.
    virtual int scrollSize(ScrollbarOrientation) const override;
    virtual int scrollPosition(Scrollbar*) const override;
    WEBCORE_EXPORT virtual void setScrollOffset(const IntPoint&) override;
    virtual bool isScrollCornerVisible() const override;
    virtual void scrollbarStyleChanged(ScrollbarStyle, bool forceUpdate) override;

    virtual void notifyPageThatContentAreaWillPaint() const;

    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    virtual void scrollTo(const IntSize& newOffset);

    // The window thats hosts the ScrollView. The ScrollView will communicate scrolls and repaints to the
    // host window in the window's coordinate space.
    virtual HostWindow* hostWindow() const = 0;

    // Returns a clip rect in host window coordinates. Used to clip the blit on a scroll.
    virtual IntRect windowClipRect() const = 0;

    // Functions for child manipulation and inspection.
    const HashSet<RefPtr<Widget>>& children() const { return m_children; }
    WEBCORE_EXPORT virtual void addChild(PassRefPtr<Widget>);
    virtual void removeChild(Widget&);

    // If the scroll view does not use a native widget, then it will have cross-platform Scrollbars. These functions
    // can be used to obtain those scrollbars.
    virtual Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    virtual Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }
    bool isScrollViewScrollbar(const Widget* child) const { return horizontalScrollbar() == child || verticalScrollbar() == child; }

    void positionScrollbarLayers();

    // Functions for setting and retrieving the scrolling mode in each axis (horizontal/vertical). The mode has values of
    // AlwaysOff, AlwaysOn, and Auto. AlwaysOff means never show a scrollbar, AlwaysOn means always show a scrollbar.
    // Auto means show a scrollbar only when one is needed.
    // Note that for platforms with native widgets, these modes are considered advisory. In other words the underlying native
    // widget may choose not to honor the requested modes.
    void setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode, bool horizontalLock = false, bool verticalLock = false);
    void setHorizontalScrollbarMode(ScrollbarMode mode, bool lock = false) { setScrollbarModes(mode, verticalScrollbarMode(), lock, verticalScrollbarLock()); }
    void setVerticalScrollbarMode(ScrollbarMode mode, bool lock = false) { setScrollbarModes(horizontalScrollbarMode(), mode, horizontalScrollbarLock(), lock); };
    WEBCORE_EXPORT void scrollbarModes(ScrollbarMode& horizontalMode, ScrollbarMode& verticalMode) const;
    ScrollbarMode horizontalScrollbarMode() const { ScrollbarMode horizontal, vertical; scrollbarModes(horizontal, vertical); return horizontal; }
    ScrollbarMode verticalScrollbarMode() const { ScrollbarMode horizontal, vertical; scrollbarModes(horizontal, vertical); return vertical; }

    void setHorizontalScrollbarLock(bool lock = true) { m_horizontalScrollbarLock = lock; }
    bool horizontalScrollbarLock() const { return m_horizontalScrollbarLock; }
    void setVerticalScrollbarLock(bool lock = true) { m_verticalScrollbarLock = lock; }
    bool verticalScrollbarLock() const { return m_verticalScrollbarLock; }

    void setScrollingModesLock(bool lock = true) { m_horizontalScrollbarLock = m_verticalScrollbarLock = lock; }

    WEBCORE_EXPORT virtual void setCanHaveScrollbars(bool);
    bool canHaveScrollbars() const { return horizontalScrollbarMode() != ScrollbarAlwaysOff || verticalScrollbarMode() != ScrollbarAlwaysOff; }

    virtual bool avoidScrollbarCreation() const { return false; }

    virtual void setScrollbarOverlayStyle(ScrollbarOverlayStyle) override;

    // By default you only receive paint events for the area that is visible. In the case of using a
    // tiled backing store, this function can be set, so that the view paints the entire contents.
    bool paintsEntireContents() const { return m_paintsEntireContents; }
    WEBCORE_EXPORT void setPaintsEntireContents(bool);

    // By default, paint events are clipped to the visible area.  If set to
    // false, paint events are no longer clipped.  paintsEntireContents() implies !clipsRepaints().
    bool clipsRepaints() const { return m_clipsRepaints; }
    void setClipsRepaints(bool);

    // By default programmatic scrolling is handled by WebCore and not by the UI application.
    // In the case of using a tiled backing store, this mode can be set, so that the scroll requests
    // are delegated to the UI application.
    bool delegatesScrolling() const { return m_delegatesScrolling; }
    WEBCORE_EXPORT void setDelegatesScrolling(bool);

    // Overridden by FrameView to create custom CSS scrollbars if applicable.
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);

    void styleDidChange();

    // If the prohibits scrolling flag is set, then all scrolling in the view (even programmatic scrolling) is turned off.
    void setProhibitsScrolling(bool b) { m_prohibitsScrolling = b; }
    bool prohibitsScrolling() const { return m_prohibitsScrolling; }

    // Whether or not a scroll view will blit visible contents when it is scrolled. Blitting is disabled in situations
    // where it would cause rendering glitches (such as with fixed backgrounds or when the view is partially transparent).
    void setCanBlitOnScroll(bool);
    bool canBlitOnScroll() const;

    // There are at least three types of contentInset. Usually we just care about WebCoreContentInset, which is the inset
    // that is set on a Page that requires WebCore to move its layers to accomodate the inset. However, there are platform
    // concepts that are similar on both iOS and Mac when there is a platformWidget(). Sometimes we need the Mac platform value
    // for topContentInset, so when the TopContentInsetType is WebCoreOrPlatformContentInset, platformTopContentInset()
    // will be returned instead of the value set on Page.
    enum class TopContentInsetType { WebCoreContentInset, WebCoreOrPlatformContentInset };
    virtual float topContentInset(TopContentInsetType = TopContentInsetType::WebCoreContentInset) const { return 0; }

    // The visible content rect has a location that is the scrolled offset of the document. The width and height are the unobscured viewport
    // width and height. By default the scrollbars themselves are excluded from this rectangle, but an optional boolean argument allows them
    // to be included.
    // In the situation the client is responsible for the scrolling (ie. with a tiled backing store) it is possible to use
    // the setFixedVisibleContentRect instead for the mainframe, though this must be updated manually, e.g just before resuming the page
    // which usually will happen when panning, pinching and rotation ends, or when scale or position are changed manually.
    virtual IntSize visibleSize() const override { return visibleContentRect(LegacyIOSDocumentVisibleRect).size(); }

#if USE(COORDINATED_GRAPHICS)
    virtual void setFixedVisibleContentRect(const IntRect& visibleContentRect) { m_fixedVisibleContentRect = visibleContentRect; }
    IntRect fixedVisibleContentRect() const { return m_fixedVisibleContentRect; }
#endif

    // Parts of the document can be visible through transparent or blured UI widgets of the chrome. Those parts
    // contribute to painting but not to the scrollable area.
    // The unobscuredContentRect is the area that is not covered by UI elements.
    WEBCORE_EXPORT IntRect unobscuredContentRect(VisibleContentRectIncludesScrollbars = ExcludeScrollbars) const;
#if PLATFORM(IOS)
    IntRect unobscuredContentRectIncludingScrollbars() const { return unobscuredContentRect(IncludeScrollbars); }
#else
    IntRect unobscuredContentRectIncludingScrollbars() const { return visibleContentRectIncludingScrollbars(); }
#endif

#if PLATFORM(IOS)
    // This is the area that is partially or fully exposed, and may extend under overlapping UI elements.
    WEBCORE_EXPORT FloatRect exposedContentRect() const;

    // The given rects are only used if there is no platform widget.
    WEBCORE_EXPORT void setExposedContentRect(const FloatRect&);
    const FloatSize& unobscuredContentSize() const { return m_unobscuredContentSize; }
    WEBCORE_EXPORT void setUnobscuredContentSize(const FloatSize&);

    void setActualScrollPosition(const IntPoint&);
    LegacyTileCache* legacyTileCache();
#endif

    virtual bool inProgrammaticScroll() const { return false; }

    // visibleContentRect().size() is computed from unscaledUnobscuredVisibleContentSize() divided by the value of visibleContentScaleFactor.
    // visibleContentScaleFactor is usually 1, except when the setting delegatesPageScaling is true and the
    // ScrollView is the main frame; in that case, visibleContentScaleFactor is equal to the page's pageScaleFactor.
    // Ports that don't use pageScaleFactor can treat unscaledUnobscuredVisibleContentSize and visibleContentRect().size() as equivalent.
    // unscaledVisibleContentSizeIncludingObscuredArea() includes areas in the content that might be obscured by UI elements.
    IntSize unscaledUnobscuredVisibleContentSize(VisibleContentRectIncludesScrollbars = ExcludeScrollbars) const;
    IntSize unscaledVisibleContentSizeIncludingObscuredArea(VisibleContentRectIncludesScrollbars = ExcludeScrollbars) const;
    virtual float visibleContentScaleFactor() const { return 1; }

    // Functions for getting/setting the size webkit should use to layout the contents. By default this is the same as the visible
    // content size. Explicitly setting a layout size value will cause webkit to layout the contents using this size instead.
    IntSize layoutSize() const;
    int layoutWidth() const { return layoutSize().width(); }
    int layoutHeight() const { return layoutSize().height(); }
    WEBCORE_EXPORT IntSize fixedLayoutSize() const;
    WEBCORE_EXPORT void setFixedLayoutSize(const IntSize&);
    WEBCORE_EXPORT bool useFixedLayout() const;
    WEBCORE_EXPORT void setUseFixedLayout(bool enable);

    // Functions for getting/setting the size of the document contained inside the ScrollView (as an IntSize or as individual width and height
    // values).
    WEBCORE_EXPORT virtual IntSize contentsSize() const override; // Always at least as big as the visibleWidth()/visibleHeight().
    int contentsWidth() const { return contentsSize().width(); }
    int contentsHeight() const { return contentsSize().height(); }
    virtual void setContentsSize(const IntSize&);

    // Functions for querying the current scrolled position (both as a point, a size, or as individual X and Y values).
    virtual ScrollPosition scrollPosition() const override { return visibleContentRect(LegacyIOSDocumentVisibleRect).location(); }

    virtual ScrollPosition maximumScrollPosition() const override; // The maximum position we can be scrolled to.
    virtual ScrollPosition minimumScrollPosition() const override; // The minimum position we can be scrolled to.

    // Adjust the passed in scroll position to keep it between the minimum and maximum positions.
    ScrollPosition adjustScrollPositionWithinRange(const ScrollPosition&) const;
    int scrollX() const { return scrollPosition().x(); }
    int scrollY() const { return scrollPosition().y(); }

    // Scroll position used by web-exposed features (has legacy iOS behavior).
    IntPoint contentsScrollPosition() const;
    void setContentsScrollPosition(const IntPoint&);

#if PLATFORM(IOS)
    int actualScrollX() const { return unobscuredContentRect().x(); }
    int actualScrollY() const { return unobscuredContentRect().y(); }
    // FIXME: maybe fix scrollPosition() on iOS to return the actual scroll position.
    IntPoint actualScrollPosition() const { return unobscuredContentRect().location(); }
#endif

    // scrollOffset() anchors its (0,0) point at the ScrollableArea's origin. When the Page has a
    // header, the header is positioned at (0,0), ABOVE the start of the Document. So when a page with
    // a header is pinned to the top, the scrollOffset() is (0,0), but the Document is actually at
    // (0, -headerHeight()). documentScrollOffsetRelativeToScrollableAreaOrigin() will return this
    // version of the offset, which tracks the top of Document relative to where scrolling was achored.
    IntSize documentScrollOffsetRelativeToScrollableAreaOrigin() const;

    // scrollOffset() anchors its (0,0) point at the ScrollableArea's origin. The top of the scrolling
    // layer does not represent the top of the view when there is a topContentInset. Additionally, as
    // detailed above, the origin of the scrolling layer also does not necessarily correspond with the
    // top of the document anyway, since there could also be header. documentScrollOffsetRelativeToViewOrigin()
    // will return a version of the current scroll offset which tracks the top of the Document
    // relative to the very top of the view.
    WEBCORE_EXPORT IntSize documentScrollOffsetRelativeToViewOrigin() const;
    IntPoint documentScrollPositionRelativeToViewOrigin() const;

    virtual IntSize overhangAmount() const override;

    void cacheCurrentScrollPosition() { m_cachedScrollPosition = scrollPosition(); }
    IntPoint cachedScrollPosition() const { return m_cachedScrollPosition; }

    // Functions for scrolling the view.
    virtual void setScrollPosition(const ScrollPosition&);
    void scrollBy(const IntSize& s) { return setScrollPosition(scrollPosition() + s); }

    // This function scrolls by lines, pages or pixels.
    bool scroll(ScrollDirection, ScrollGranularity);
    
    // A logical scroll that just ends up calling the corresponding physical scroll() based off the document's writing mode.
    bool logicalScroll(ScrollLogicalDirection, ScrollGranularity);

    // Scroll the actual contents of the view (either blitting or invalidating as needed).
    void scrollContents(const IntSize& scrollDelta);

    // This gives us a means of blocking painting on our scrollbars until the first layout has occurred.
    WEBCORE_EXPORT void setScrollbarsSuppressed(bool suppressed, bool repaintOnUnsuppress = false);
    bool scrollbarsSuppressed() const { return m_scrollbarsSuppressed; }

    WEBCORE_EXPORT IntPoint rootViewToContents(const IntPoint&) const;
    WEBCORE_EXPORT IntPoint contentsToRootView(const IntPoint&) const;
    IntRect rootViewToContents(const IntRect&) const;
    WEBCORE_EXPORT IntRect contentsToRootView(const IntRect&) const;

    IntPoint viewToContents(const IntPoint&) const;
    IntPoint contentsToView(const IntPoint&) const;

    IntRect viewToContents(IntRect) const;
    IntRect contentsToView(IntRect) const;

    IntPoint contentsToContainingViewContents(const IntPoint&) const;
    IntRect contentsToContainingViewContents(IntRect) const;

    WEBCORE_EXPORT IntPoint rootViewToTotalContents(const IntPoint&) const;

    // Event coordinates are assumed to be in the coordinate space of a window that contains
    // the entire widget hierarchy. It is up to the platform to decide what the precise definition
    // of containing window is. (For example on Mac it is the containing NSWindow.)
    WEBCORE_EXPORT IntPoint windowToContents(const IntPoint&) const;
    WEBCORE_EXPORT IntPoint contentsToWindow(const IntPoint&) const;
    WEBCORE_EXPORT IntRect windowToContents(const IntRect&) const;
    WEBCORE_EXPORT IntRect contentsToWindow(const IntRect&) const;

    // Functions for converting to and from screen coordinates.
    WEBCORE_EXPORT IntRect contentsToScreen(const IntRect&) const;
    IntPoint screenToContents(const IntPoint&) const;

    // The purpose of this function is to answer whether or not the scroll view is currently visible. Animations and painting updates can be suspended if
    // we know that we are either not in a window right now or if that window is not visible.
    bool isOffscreen() const;

    // Called when our frame rect changes (or the rect/scroll position of an ancestor changes).
    virtual void frameRectsChanged() override;
    
    // Widget override to update our scrollbars and notify our contents of the resize.
    virtual void setFrameRect(const IntRect&) override;

    // Widget override to notify our contents of a cliprect change.
    virtual void clipRectChanged() override;

    // For platforms that need to hit test scrollbars from within the engine's event handlers (like Win32).
    Scrollbar* scrollbarAtPoint(const IntPoint& windowPoint);

    IntPoint convertChildToSelf(const Widget* child, const IntPoint& point) const
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point - toIntSize(scrollPosition());
        newPoint.moveBy(child->location());
        return newPoint;
    }

    IntPoint convertSelfToChild(const Widget* child, const IntPoint& point) const
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point + toIntSize(scrollPosition());
        newPoint.moveBy(-child->location());
        return newPoint;
    }

    // Widget override. Handles painting of the contents of the view as well as the scrollbars.
    WEBCORE_EXPORT virtual void paint(GraphicsContext&, const IntRect&) override;
    void paintScrollbars(GraphicsContext&, const IntRect&);

    // Widget overrides to ensure that our children's visibility status is kept up to date when we get shown and hidden.
    WEBCORE_EXPORT virtual void show() override;
    WEBCORE_EXPORT virtual void hide() override;
    WEBCORE_EXPORT virtual void setParentVisible(bool) override;
    
    // Pan scrolling.
    static const int noPanScrollRadius = 15;
    void addPanScrollIcon(const IntPoint&);
    void removePanScrollIcon();
    void paintPanScrollIcon(GraphicsContext&);

    virtual bool isPointInScrollbarCorner(const IntPoint&);
    virtual bool scrollbarCornerPresent() const;
    virtual IntRect scrollCornerRect() const override;
    virtual void paintScrollCorner(GraphicsContext&, const IntRect& cornerRect);
    virtual void paintScrollbar(GraphicsContext&, Scrollbar&, const IntRect&);

    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const override;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const override;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const override;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const override;

    void calculateAndPaintOverhangAreas(GraphicsContext&, const IntRect& dirtyRect);

    virtual bool isScrollView() const override { return true; }

    WEBCORE_EXPORT void scrollPositionChangedViaPlatformWidget(const IntPoint& oldPosition, const IntPoint& newPosition);

protected:
    ScrollView();

    virtual void repaintContentRectangle(const IntRect&);
    virtual void paintContents(GraphicsContext&, const IntRect& damageRect) = 0;

    virtual void paintOverhangAreas(GraphicsContext&, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect);

    virtual void availableContentSizeChanged(AvailableSizeChangeReason) override;
    virtual void addedOrRemovedScrollbar() = 0;
    virtual void delegatesScrollingDidChange() { }

    // These functions are used to create/destroy scrollbars.
    // They return true if the scrollbar was added or removed.
    bool setHasHorizontalScrollbar(bool, bool* contentSizeAffected = 0);
    bool setHasVerticalScrollbar(bool, bool* contentSizeAffected = 0);

    virtual void updateScrollCorner();
    virtual void invalidateScrollCornerRect(const IntRect&) override;

    // Scroll the content by blitting the pixels.
    virtual bool scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect);
    // Scroll the content by invalidating everything.
    virtual void scrollContentsSlowPath(const IntRect& updateRect);

    void setScrollOrigin(const IntPoint&, bool updatePositionAtAll, bool updatePositionSynchronously);

    // Subclassed by FrameView to check the writing-mode of the document.
    virtual bool isVerticalDocument() const { return true; }
    virtual bool isFlippedDocument() const { return false; }

    // Called to update the scrollbars to accurately reflect the state of the view.
    void updateScrollbars(const ScrollPosition& desiredPosition);

    float platformTopContentInset() const;
    void platformSetTopContentInset(float);

    virtual void handleDeferredScrollUpdateAfterContentSizeChange();

    virtual bool shouldDeferScrollUpdateAfterContentSizeChange() { return false; }

    virtual void scrollPositionChangedViaPlatformWidgetImpl(const IntPoint&, const IntPoint&) { }

private:
    virtual IntRect visibleContentRectInternal(VisibleContentRectIncludesScrollbars, VisibleContentRectBehavior) const override;
    WEBCORE_EXPORT IntRect unobscuredContentRectInternal(VisibleContentRectIncludesScrollbars = ExcludeScrollbars) const;

    void completeUpdatesAfterScrollTo(const IntSize& scrollDelta);

    RefPtr<Scrollbar> m_horizontalScrollbar;
    RefPtr<Scrollbar> m_verticalScrollbar;
    ScrollbarMode m_horizontalScrollbarMode;
    ScrollbarMode m_verticalScrollbarMode;

    bool m_horizontalScrollbarLock;
    bool m_verticalScrollbarLock;

    bool m_prohibitsScrolling;

    HashSet<RefPtr<Widget>> m_children;

    // This bool is unused on Mac OS because we directly ask the platform widget
    // whether it is safe to blit on scroll.
    bool m_canBlitOnScroll;

    // FIXME: exposedContentRect is a very similar concept to fixedVisibleContentRect except it does not differentiate
    // between exposed and unobscured areas. The two attributes should eventually be merged.
#if PLATFORM(IOS)
    FloatRect m_exposedContentRect;
    FloatSize m_unobscuredContentSize;
#else
    IntRect m_fixedVisibleContentRect;
#endif
    IntSize m_scrollOffset; // FIXME: Would rather store this as a position, but we will wait to make this change until more code is shared.
    IntPoint m_cachedScrollPosition;
    IntSize m_fixedLayoutSize;
    IntSize m_contentsSize;

    std::unique_ptr<IntSize> m_deferredScrollDelta; // Needed for WebKit scrolling
    std::unique_ptr<std::pair<IntPoint, IntPoint>> m_deferredScrollPositions; // Needed for platform widget scrolling

    bool m_scrollbarsSuppressed;

    bool m_inUpdateScrollbars;
    unsigned m_updateScrollbarsPass;

    IntPoint m_panScrollIconPoint;
    bool m_drawPanScrollIcon;
    bool m_useFixedLayout;

    bool m_paintsEntireContents;
    bool m_clipsRepaints;
    bool m_delegatesScrolling;

    void init();
    void destroy();

    IntRect rectToCopyOnScroll() const;

    // Called when the scroll position within this view changes.  FrameView overrides this to generate repaint invalidations.
    virtual void updateLayerPositionsAfterScrolling() { }
    virtual void updateCompositingLayersAfterScrolling() { }

    void platformAddChild(Widget*);
    void platformRemoveChild(Widget*);
    void platformSetScrollbarModes();
    void platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const;
    void platformSetCanBlitOnScroll(bool);
    bool platformCanBlitOnScroll() const;
    IntRect platformVisibleContentRect(bool includeScrollbars) const;
    IntSize platformVisibleContentSize(bool includeScrollbars) const;
    IntRect platformVisibleContentRectIncludingObscuredArea(bool includeScrollbars) const;
    IntSize platformVisibleContentSizeIncludingObscuredArea(bool includeScrollbars) const;
    void platformSetContentsSize();
    IntRect platformContentsToScreen(const IntRect&) const;
    IntPoint platformScreenToContents(const IntPoint&) const;
    void platformSetScrollPosition(const IntPoint&);
    bool platformScroll(ScrollDirection, ScrollGranularity);
    void platformSetScrollbarsSuppressed(bool repaintOnUnsuppress);
    void platformRepaintContentRectangle(const IntRect&);
    bool platformIsOffscreen() const;
    void platformSetScrollbarOverlayStyle(ScrollbarOverlayStyle);
   
    void platformSetScrollOrigin(const IntPoint&, bool updatePositionAtAll, bool updatePositionSynchronously);

    void calculateOverhangAreasForPainting(IntRect& horizontalOverhangRect, IntRect& verticalOverhangRect);
    void updateOverhangAreas();

#if PLATFORM(COCOA) && defined __OBJC__
public:
    WEBCORE_EXPORT NSView* documentView() const;

private:
    NSScrollView<WebCoreFrameScrollView>* scrollView() const;
#endif
}; // class ScrollView

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WIDGET(ScrollView, isScrollView())

#endif // ScrollView_h
