/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Computer, Inc.  All rights reserved.
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

    class FloatRect;
    class IntRect;
    class PlatformWheelEvent;
    class PlatformScrollbar;

    class ScrollView : public Widget {
    public:
        int visibleWidth() const;
        int visibleHeight() const;
        FloatRect visibleContentRect() const;
        FloatRect visibleContentRectConsideringExternalScrollers() const;

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

        void addChild(Widget*);
        void removeChild(Widget*);

        virtual void resizeContents(int w, int h);
        void updateContents(const IntRect&, bool now = false);
        void update();

        // Event coordinates are assumed to be in the coordinate space of a window that contains
        // the entire widget hierarchy.  It is up to the platform to decide what the precise definition
        // of containing window is.  (For example on Mac it is the containing NSWindow.)
        IntPoint windowToContents(const IntPoint&) const;
        IntPoint contentsToWindow(const IntPoint&) const;
        
#if PLATFORM(MAC)
        // On Mac only because of flipped NSWindow y-coordinates, we have to have a special implementation.
        IntRect windowToContents(const IntRect&) const;
        IntRect contentsToWindow(const IntRect&) const;
#else
        // Other platforms can just implement these helper methods using the corresponding point conversion methods.
        IntRect contentsToWindow(const IntRect& rect) const { return IntRect(contentsToWindow(rect.location()), rect.size()); }
        IntRect windowToContents(const IntRect& rect) const { return IntRect(windowToContents(rect.location()), rect.size()); }
#endif

        void setStaticBackground(bool);

        bool inWindow() const;

        // For platforms that need to hit test scrollbars from within the engine's event handlers (like Win32).
        PlatformScrollbar* scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent);

        // This method exists for scrollviews that need to handle wheel events manually.
        // On Mac the underlying NSScrollView just does the scrolling, but on other platforms
        // (like Windows), we need this method in order to do the scroll ourselves.
        void wheelEvent(PlatformWheelEvent&);

        bool scroll(ScrollDirection, ScrollGranularity);

#if PLATFORM(MAC)
        NSView* getDocumentView() const;
#endif

#if PLATFORM(WIN)
        ScrollView();
        ~ScrollView();

        virtual void paint(GraphicsContext*, const IntRect&);
        virtual void themeChanged();
        
        virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
        virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;
        
        virtual void geometryChanged() const;
        virtual void setFrameGeometry(const IntRect&);
        
        IntRect windowResizerRect();
        bool resizerOverlapsContent() const;
        void adjustOverlappingScrollbarCount(int overlapDelta);
        
        virtual void setParent(ScrollView*);

        virtual void attachToWindow();
        virtual void detachFromWindow();
        bool isAttachedToWindow() const;

        virtual void show();
        virtual void hide();

        void addToDirtyRegion(const IntRect&);
        void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
        void updateBackingStore();

        void setAllowsScrolling(bool);
        bool allowsScrolling() const;

        HashSet<Widget*>* children();

    private:
        void updateScrollbars(const IntSize& desiredOffset);
        IntSize maximumScroll() const;
        class ScrollViewPrivate;
        ScrollViewPrivate* m_data;
#endif

#if PLATFORM(GTK)
        ScrollView();
        ~ScrollView();

        virtual void setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj);
        virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
        virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;
        virtual void geometryChanged() const;

        virtual void paint(GraphicsContext*, const IntRect&);
        virtual void setFrameGeometry(const IntRect&);

        void addToDirtyRegion(const IntRect&);
        void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
        void updateBackingStore();

    protected:
        HashSet<Widget*>* children() const;

    private:
        IntSize maximumScroll() const;
        void updateScrollbars(const IntSize& desiredOffset);

        class ScrollViewPrivate;
        ScrollViewPrivate* m_data;
#endif

#if PLATFORM(QT)
        ScrollView();
        ~ScrollView();

        virtual void paint(GraphicsContext*, const IntRect&);

        virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
        virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;

        virtual void geometryChanged() const;
        virtual void setFrameGeometry(const IntRect&);

        IntRect windowResizerRect();
        bool resizerOverlapsContent() const;
        void adjustOverlappingScrollbarCount(int overlapDelta);

        virtual void setParent(ScrollView*);

        void addToDirtyRegion(const IntRect&);
        void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
        void updateBackingStore();

        PlatformScrollbar *horizontalScrollBar() const;
        PlatformScrollbar *verticalScrollBar() const;

        HashSet<Widget*>* children();

    private:
        void updateScrollbars(const IntSize& desiredOffset);
        IntSize maximumScroll() const;
        class ScrollViewPrivate;
        ScrollViewPrivate* m_data;
#endif
#if PLATFORM(WX)
        ScrollView();
        ~ScrollView();
        HashSet<Widget*>* children();
        virtual void setNativeWindow(wxWindow* window);

    private:
        void adjustScrollbars(int x = -1, int y = -1, bool refresh = true);
        IntSize maximumScroll() const;

        class ScrollViewPrivate;
        ScrollViewPrivate* m_data;
#endif
    };

} // namespace WebCore

#endif // ScrollView_h
