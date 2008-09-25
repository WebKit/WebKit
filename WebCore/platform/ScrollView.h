/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "IntRect.h"
#include "ScrollTypes.h"
#include "Widget.h"

#include <wtf/HashSet.h>

#if PLATFORM(MAC) && defined __OBJC__
@protocol WebCoreFrameScrollView;
#endif

#if PLATFORM(GTK)
typedef struct _GtkAdjustment GtkAdjustment;
#endif

#if PLATFORM(WIN)
typedef struct HRGN__* HRGN;
#endif

#if PLATFORM(WX)
class wxScrollWinEvent;
#endif

namespace WebCore {

    class PlatformWheelEvent;
    class Scrollbar;

    class ScrollView : public Widget {
    public:
        ScrollView();
        ~ScrollView();

        const HashSet<Widget*>* children() const { return &m_children; }
        void addChild(Widget*);
        void removeChild(Widget*);

        Scrollbar* horizontalScrollbar() const;
        Scrollbar* verticalScrollbar() const;

        void setCanBlitOnScroll(bool);
        bool canBlitOnScroll() const { return m_canBlitOnScroll; }

        int visibleWidth() const { return visibleContentRect().width(); }
        int visibleHeight() const { return visibleContentRect().height(); }
        IntRect visibleContentRect(bool includeScrollbars = false) const;

        int contentsWidth() const;
        int contentsHeight() const;
        int contentsX() const;
        int contentsY() const;
        IntSize scrollOffset() const;
        void scrollBy(int dx, int dy);
        virtual void scrollRectIntoViewRecursively(const IntRect&);

        virtual void setContentsPos(int x, int y);

        virtual void setVScrollbarMode(ScrollbarMode);
        virtual void setHScrollbarMode(ScrollbarMode);

        // Set the mode for both scrollbars at once.
        virtual void setScrollbarsMode(ScrollbarMode);

        // This gives us a means of blocking painting on our scrollbars until the first layout has occurred.
        void suppressScrollbars(bool suppressed, bool repaintOnUnsuppress = false);

        ScrollbarMode vScrollbarMode() const;
        ScrollbarMode hScrollbarMode() const;

        bool isScrollable();
        
        virtual void resizeContents(int w, int h);
        
        // Event coordinates are assumed to be in the coordinate space of a window that contains
        // the entire widget hierarchy. It is up to the platform to decide what the precise definition
        // of containing window is. (For example on Mac it is the containing NSWindow.)
        IntPoint windowToContents(const IntPoint&) const;
        IntPoint contentsToWindow(const IntPoint&) const;
        IntRect windowToContents(const IntRect&) const;
        IntRect contentsToWindow(const IntRect&) const;

        bool inWindow() const;
        virtual bool shouldUpdateWhileOffscreen() const = 0;

        // For platforms that need to hit test scrollbars from within the engine's event handlers (like Win32).
        Scrollbar* scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent);

        // This method exists for scrollviews that need to handle wheel events manually.
        // On Mac the underlying NSScrollView just does the scrolling, but on other platforms
        // (like Windows), we need this method in order to do the scroll ourselves.
        void wheelEvent(PlatformWheelEvent&);

        bool scroll(ScrollDirection, ScrollGranularity);
        
        IntPoint convertChildToSelf(const Widget* child, const IntPoint& point) const
        {
            IntPoint newPoint = point;
            if (!isScrollViewScrollbar(child))
                newPoint = point - scrollOffset();
            newPoint.move(child->x(), child->y());
            return newPoint;
        }

        IntPoint convertSelfToChild(const Widget* child, const IntPoint& point) const
        {
            IntPoint newPoint = point;
            if (!isScrollViewScrollbar(child))
                newPoint = point + scrollOffset();
            newPoint.move(-child->x(), -child->y());
            return newPoint;
        }
        
        bool isScrollViewScrollbar(const Widget*) const;
    
#if HAVE(ACCESSIBILITY)
        IntRect contentsToScreen(const IntRect&) const;
        IntPoint screenToContents(const IntPoint&) const;
#endif

    protected:
        void updateContents(const IntRect&, bool now = false);
        void updateWindowRect(const IntRect&, bool now = false);
    public:
        void update();

    private:
        HashSet<Widget*> m_children;
        bool m_canBlitOnScroll;
        
        void init();
        
        void platformAddChild(Widget*);
        void platformRemoveChild(Widget*);
        void platformSetCanBlitOnScroll();
        IntRect platformVisibleContentRect(bool includeScrollbars) const;
        
#if PLATFORM(MAC) && defined __OBJC__
    public:
        NSView* documentView() const;

    private:
        NSScrollView<WebCoreFrameScrollView>* scrollView() const;
#endif

#if !PLATFORM(MAC)
    private:
        IntSize maximumScroll() const;
#endif

        class ScrollViewPrivate;
        ScrollViewPrivate* m_data;

#if !PLATFORM(MAC) && !PLATFORM(WX)
    public:
        virtual void paint(GraphicsContext*, const IntRect&);

        virtual void geometryChanged() const;
        virtual void setFrameGeometry(const IntRect&);

        void addToDirtyRegion(const IntRect&);
        void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
        void updateBackingStore();

    private:
        void updateScrollbars(const IntSize& desiredOffset);
#endif

#if PLATFORM(WIN) || PLATFORM(QT) || PLATFORM(MAC)
    public:
        IntRect windowResizerRect();
        bool resizerOverlapsContent() const;
        void adjustOverlappingScrollbarCount(int overlapDelta);

    private:
        virtual void setParent(ScrollView*);
#else
    public:
        IntRect windowResizerRect() { return IntRect(); }
        bool resizerOverlapsContent() const { return false; }
        void adjustOverlappingScrollbarCount(int overlapDelta) {}
#endif

#if PLATFORM(WIN)
    public:
        virtual void themeChanged();

        void printPanScrollIcon(const IntPoint&);
        void removePanScrollIcon();

        virtual void show();
        virtual void hide();
        virtual void setParentVisible(bool);

        void setAllowsScrolling(bool);
        bool allowsScrolling() const;
#endif

#if PLATFORM(QT)
    private:
        void incrementNativeWidgetCount();
        void decrementNativeWidgetCount();
        bool hasNativeWidgets() const;
        void invalidateScrollbars();
#endif

#if PLATFORM(GTK)
    public:
        void setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj);
#endif

#if PLATFORM(WX)
    public:
        virtual void setPlatformWidget(wxWindow*);

    private:
        void adjustScrollbars(int x = -1, int y = -1, bool refresh = true);
#endif

    }; // class ScrollView

#if !PLATFORM(MAC)

// On Mac only, because of flipped NSWindow y-coordinates, we have to have a special implementation.
// Other platforms can just implement these helper methods using the corresponding point conversion methods.

inline IntRect ScrollView::contentsToWindow(const IntRect& rect) const
{
    return IntRect(contentsToWindow(rect.location()), rect.size());
}

inline IntRect ScrollView::windowToContents(const IntRect& rect) const
{
    return IntRect(windowToContents(rect.location()), rect.size());
}

#endif

} // namespace WebCore

#endif // ScrollView_h
