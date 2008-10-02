/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScrollView.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Scrollbar.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderTheme.h"
#include "Scrollbar.h"
#include "ScrollbarClient.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate {
public:
    ScrollViewPrivate(ScrollView* view)
        : m_view(view)
    {
    }

    void scrollBackingStore(const IntSize& scrollDelta);

    ScrollView* m_view;
};

const int panIconSizeLength = 20;

void ScrollView::ScrollViewPrivate::scrollBackingStore(const IntSize& scrollDelta)
{
    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    HWND containingWindowHandle = m_view->containingWindow();
    IntRect clipRect = static_cast<Widget*>(m_view)->windowClipRect();
    IntRect scrollViewRect = m_view->convertToContainingWindow(IntRect(0, 0, m_view->visibleWidth(), m_view->visibleHeight()));
    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);
    RECT r = updateRect;
    ::InvalidateRect(containingWindowHandle, &r, false);

    if (m_view->m_drawPanScrollIcon) {
        int panIconDirtySquareSizeLength = 2 * (panIconSizeLength + max(abs(scrollDelta.width()), abs(scrollDelta.height()))); // We only want to repaint what's necessary
        IntPoint panIconDirtySquareLocation = IntPoint(m_view->m_panScrollIconPoint.x() - (panIconDirtySquareSizeLength / 2), m_view->m_panScrollIconPoint.y() - (panIconDirtySquareSizeLength / 2));
        IntRect panScrollIconDirtyRect = IntRect(panIconDirtySquareLocation , IntSize(panIconDirtySquareSizeLength, panIconDirtySquareSizeLength));
        
        m_view->updateWindowRect(panScrollIconDirtyRect);
    }

    if (m_view->canBlitOnScroll()) // The main frame can just blit the WebView window
       // FIXME: Find a way to blit subframes without blitting overlapping content
       m_view->scrollBackingStore(-scrollDelta.width(), -scrollDelta.height(), scrollViewRect, clipRect);
    else  {
       // We need to go ahead and repaint the entire backing store.  Do it now before moving the
       // plugins.
       m_view->addToDirtyRegion(updateRect);
       m_view->updateBackingStore();
    }

    // This call will move child HWNDs (plugins) and invalidate them as well.
    m_view->frameRectsChanged();

    // Now update the window (which should do nothing but a blit of the backing store's updateRect and so should
    // be very fast).
    ::UpdateWindow(containingWindowHandle);
}

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate(this))
{
    init();
}

ScrollView::~ScrollView()
{
    destroy();
    delete m_data;
}

void ScrollView::platformAddChild(Widget*)
{
}

void ScrollView::platformRemoveChild(Widget*)
{
}

void ScrollView::updateWindowRect(const IntRect& rect, bool now)
{
    RECT containingWindowRectWin = rect;
    HWND containingWindowHandle = containingWindow();

    ::InvalidateRect(containingWindowHandle, &containingWindowRectWin, false);
      
    // Cache the dirty spot.
    addToDirtyRegion(rect);

    if (now)
        ::UpdateWindow(containingWindowHandle);
}

void ScrollView::printPanScrollIcon(const IntPoint& iconPosition)
{
    m_drawPanScrollIcon = true;    
    m_panScrollIconPoint = IntPoint(iconPosition.x() - panIconSizeLength / 2 , iconPosition.y() - panIconSizeLength / 2) ;

    updateWindowRect(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength,panIconSizeLength)), true);    
}

void ScrollView::removePanScrollIcon()
{
    m_drawPanScrollIcon = false; 

    updateWindowRect(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)), true);
}

void ScrollView::themeChanged()
{
    ScrollbarTheme::nativeTheme()->themeChanged();
    theme()->themeChanged();
    invalidate();
}

void ScrollView::scrollContents(const IntSize& size)
{
    m_data->scrollBackingStore(size);
}

void ScrollView::addToDirtyRegion(const IntRect& containingWindowRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->addToDirtyRegion(containingWindowRect);
}

void ScrollView::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->scrollBackingStore(dx, dy, scrollViewRect, clipRect);
}

void ScrollView::updateBackingStore()
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->updateBackingStore();
}

} // namespace WebCore
