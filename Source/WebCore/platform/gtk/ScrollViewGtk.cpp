/*
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc. All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008, 2010 Collabora Ltd.
 *
 * All rights reserved.
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

#if USE(NATIVE_GTK_MAIN_FRAME_SCROLLBAR)

#include "ChromeClient.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "MainFrameScrollbarGtk.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollbarTheme.h"
#include <gtk/gtk.h>

using namespace std;

namespace WebCore {

void ScrollView::platformInit()
{
}

void ScrollView::platformDestroy()
{
    m_horizontalAdjustment = 0;
    m_verticalAdjustment = 0;
}

PassRefPtr<Scrollbar> ScrollView::createScrollbar(ScrollbarOrientation orientation)
{
    // If this is an interior frame scrollbar, we want to create a totally fake
    // scrollbar with no GtkAdjustment backing it.
    if (parent())
        return Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);

    // If this is the main frame, we want to create a Scrollbar that does no  painting
    // and defers to our GtkAdjustment for all of its state. Note that the GtkAdjustment
    // may be null here.
    if (orientation == HorizontalScrollbar)
        return MainFrameScrollbarGtk::create(this, orientation, m_horizontalAdjustment.get());

    // VerticalScrollbar
    return MainFrameScrollbarGtk::create(this, orientation, m_verticalAdjustment.get());
}

void ScrollView::setHorizontalAdjustment(GtkAdjustment* hadj, bool resetValues)
{
    ASSERT(!parent() || !hadj);
    if (parent())
        return;

    m_horizontalAdjustment = hadj;

    if (!m_horizontalAdjustment) {
        MainFrameScrollbarGtk* hScrollbar = reinterpret_cast<MainFrameScrollbarGtk*>(horizontalScrollbar());
        if (hScrollbar)
            hScrollbar->detachAdjustment();

        return;
    }

    // We may be lacking scrollbars when returning to a cached
    // page, this kicks the page to recreate the scrollbars.
    setHasHorizontalScrollbar(true);

    MainFrameScrollbarGtk* hScrollbar = reinterpret_cast<MainFrameScrollbarGtk*>(horizontalScrollbar());
    hScrollbar->attachAdjustment(m_horizontalAdjustment.get());

    // We used to reset everything to 0 here, but when page cache
    // is enabled we reuse FrameViews that are cached. Since their
    // size is not going to change when being restored, (which is
    // what would cause the upper limit in the adjusments to be
    // set in the normal case), we make sure they are up-to-date
    // here. This is needed for the parent scrolling widget to be
    // able to report correct values.
    int horizontalPageStep = max(max<int>(frameRect().width() * Scrollbar::minFractionToStepWhenPaging(), frameRect().width() - Scrollbar::maxOverlapBetweenPages()), 1);
    gtk_adjustment_configure(m_horizontalAdjustment.get(),
                             resetValues ? 0 : scrollOffset().width(), 0,
                             resetValues ? 0 : contentsSize().width(),
                             resetValues ? 0 : Scrollbar::pixelsPerLineStep(),
                             resetValues ? 0 : horizontalPageStep,
                             resetValues ? 0 : frameRect().width());
}

void ScrollView::setVerticalAdjustment(GtkAdjustment* vadj, bool resetValues)
{
    ASSERT(!parent() || !vadj);
    if (parent())
        return;

    m_verticalAdjustment = vadj;

    if (!m_verticalAdjustment) {
        MainFrameScrollbarGtk* vScrollbar = reinterpret_cast<MainFrameScrollbarGtk*>(verticalScrollbar());
        if (vScrollbar)
            vScrollbar->detachAdjustment();

        return;
    }

    // We may be lacking scrollbars when returning to a cached
    // page, this kicks the page to recreate the scrollbars.
    setHasVerticalScrollbar(true);

    MainFrameScrollbarGtk* vScrollbar = reinterpret_cast<MainFrameScrollbarGtk*>(verticalScrollbar());
    vScrollbar->attachAdjustment(m_verticalAdjustment.get());

    // We used to reset everything to 0 here, but when page cache
    // is enabled we reuse FrameViews that are cached. Since their
    // size is not going to change when being restored, (which is
    // what would cause the upper limit in the adjusments to be
    // set in the normal case), we make sure they are up-to-date
    // here. This is needed for the parent scrolling widget to be
    // able to report correct values.
    int verticalPageStep = max(max<int>(frameRect().width() * Scrollbar::minFractionToStepWhenPaging(), frameRect().width() - Scrollbar::maxOverlapBetweenPages()), 1);
    gtk_adjustment_configure(m_verticalAdjustment.get(),
                             resetValues ? 0 : scrollOffset().height(), 0,
                             resetValues ? 0 : contentsSize().height(),
                             resetValues ? 0 : Scrollbar::pixelsPerLineStep(),
                             resetValues ? 0 : verticalPageStep,
                             resetValues ? 0 : frameRect().height());
}

void ScrollView::setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj, bool resetValues)
{
    setHorizontalAdjustment(hadj, resetValues);
    setVerticalAdjustment(vadj, resetValues);
}

IntRect ScrollView::visibleContentRect(bool includeScrollbars) const
{
    // If we are an interior frame scrollbar or are in some sort of transition
    // state, just calculate our size based on what the GTK+ theme says the
    // scrollbar width should be.
    if (parent() || !hostWindow() || !hostWindow()->platformPageClient()) {
        return IntRect(IntPoint(m_scrollOffset.width(), m_scrollOffset.height()),
                       IntSize(max(0, m_boundsSize.width() - (verticalScrollbar() && !includeScrollbars ? verticalScrollbar()->width() : 0)),
                               max(0, m_boundsSize.height() - (horizontalScrollbar() && !includeScrollbars ? horizontalScrollbar()->height() : 0))));
    }

    // We don't have a parent, so we are the main frame and thus have
    // a parent widget which we can use to measure the visible region.
    GtkWidget* measuredWidget = hostWindow()->platformPageClient();
    GtkWidget* parentWidget = gtk_widget_get_parent(measuredWidget);

    // We may not be in a widget that displays scrollbars, but we may
    // have other kinds of decoration that make us smaller.
    if (parentWidget && includeScrollbars)
        measuredWidget = parentWidget;

    GtkAllocation allocation;
    gtk_widget_get_allocation(measuredWidget, &allocation);
    return IntRect(IntPoint(m_scrollOffset.width(), m_scrollOffset.height()),
                   IntSize(allocation.width, allocation.height));
}

void ScrollView::setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode, bool horizontalLock, bool verticalLock)
{
    // FIXME: Restructure the ScrollView abstraction so that we do not have to
    // copy this verbatim from ScrollView.cpp. Until then, we should make sure this
    // is kept in sync.
    bool needsUpdate = false;

    if (horizontalMode != horizontalScrollbarMode() && !m_horizontalScrollbarLock) {
        m_horizontalScrollbarMode = horizontalMode;
        needsUpdate = true;
    }

    if (verticalMode != verticalScrollbarMode() && !m_verticalScrollbarLock) {
        m_verticalScrollbarMode = verticalMode;
        needsUpdate = true;
    }

    if (horizontalLock)
        setHorizontalScrollbarLock();

    if (verticalLock)
        setVerticalScrollbarLock();

    if (needsUpdate)
        updateScrollbars(scrollOffset());

    // We don't need to report policy changes on ScrollView's unless this
    // one has an adjustment attached and it is a main frame.
    if (!m_horizontalAdjustment || parent() || !isFrameView())
        return;

    // For frames that do have adjustments attached, we want to report
    // policy changes, so that they may be applied to the widget to
    // which the WebView's container (e.g. GtkScrolledWindow).
    if (hostWindow())
        hostWindow()->scrollbarsModeDidChange();
}

}

#endif // USE(NATIVE_GTK_MAIN_FRAME_SCROLLBAR)
