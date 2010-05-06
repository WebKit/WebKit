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

#include "ChromeClient.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollbarGtk.h"
#include "ScrollbarTheme.h"

#include <gtk/gtk.h>

using namespace std;

namespace WebCore {

void ScrollView::platformInit()
{
    m_horizontalAdjustment = 0;
    m_verticalAdjustment = 0;
}

void ScrollView::platformDestroy()
{
    m_horizontalAdjustment = 0;
    m_verticalAdjustment = 0;
}

PassRefPtr<Scrollbar> ScrollView::createScrollbar(ScrollbarOrientation orientation)
{
    if (orientation == HorizontalScrollbar && m_horizontalAdjustment)
        return ScrollbarGtk::createScrollbar(this, orientation, m_horizontalAdjustment);
    else if (orientation == VerticalScrollbar && m_verticalAdjustment)
        return ScrollbarGtk::createScrollbar(this, orientation, m_verticalAdjustment);
    else
        return Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
}

/*
 * The following is assumed:
 *   (hadj && vadj) || (!hadj && !vadj)
 */
void ScrollView::setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj, bool resetValues)
{
    ASSERT(!hadj == !vadj);

    m_horizontalAdjustment = hadj;
    m_verticalAdjustment = vadj;

    // Reset the adjustments to a sane default
    if (m_horizontalAdjustment) {
        ScrollbarGtk* hScrollbar = reinterpret_cast<ScrollbarGtk*>(horizontalScrollbar());
        if (hScrollbar)
            hScrollbar->attachAdjustment(m_horizontalAdjustment);

        ScrollbarGtk* vScrollbar = reinterpret_cast<ScrollbarGtk*>(verticalScrollbar());
        if (vScrollbar)
            vScrollbar->attachAdjustment(m_verticalAdjustment);

        // We used to reset everything to 0 here, but when page cache
        // is enabled we reuse FrameViews that are cached. Since their
        // size is not going to change when being restored, (which is
        // what would cause the upper limit in the adjusments to be
        // set in the normal case), we make sure they are up-to-date
        // here. This is needed for the parent scrolling widget to be
        // able to report correct values.

        int horizontalPageStep = max(max<int>(frameRect().width() * Scrollbar::minFractionToStepWhenPaging(), frameRect().width() - Scrollbar::maxOverlapBetweenPages()), 1);
        gtk_adjustment_configure(m_horizontalAdjustment,
                                 resetValues ? 0 : scrollOffset().width(), 0,
                                 resetValues ? 0 : contentsSize().width(),
                                 resetValues ? 0 : Scrollbar::pixelsPerLineStep(),
                                 resetValues ? 0 : horizontalPageStep,
                                 resetValues ? 0 : frameRect().width());

        int verticalPageStep = max(max<int>(frameRect().height() * Scrollbar::minFractionToStepWhenPaging(), frameRect().height() - Scrollbar::maxOverlapBetweenPages()), 1);
        gtk_adjustment_configure(m_verticalAdjustment,
                                 resetValues ? 0 : scrollOffset().height(), 0,
                                 resetValues ? 0 : contentsSize().height(),
                                 resetValues ? 0 : Scrollbar::pixelsPerLineStep(),
                                 resetValues ? 0 : verticalPageStep,
                                 resetValues ? 0 : frameRect().height());
    } else {
        ScrollbarGtk* hScrollbar = reinterpret_cast<ScrollbarGtk*>(horizontalScrollbar());
        if (hScrollbar)
            hScrollbar->detachAdjustment();

        ScrollbarGtk* vScrollbar = reinterpret_cast<ScrollbarGtk*>(verticalScrollbar());
        if (vScrollbar)
            vScrollbar->detachAdjustment();
    }

    /* reconsider having a scrollbar */
    setHasVerticalScrollbar(false);
    setHasHorizontalScrollbar(false);
}

void ScrollView::platformAddChild(Widget* child)
{
    if (!GTK_IS_SOCKET(child->platformWidget()))
        gtk_container_add(GTK_CONTAINER(hostWindow()->platformPageClient()), child->platformWidget());
}

void ScrollView::platformRemoveChild(Widget* child)
{
    GtkWidget* parent;

    // HostWindow can be NULL here. If that's the case
    // let's grab the child's parent instead.
    if (hostWindow())
        parent = GTK_WIDGET(hostWindow()->platformPageClient());
    else
        parent = GTK_WIDGET(child->platformWidget()->parent);

    if (GTK_IS_CONTAINER(parent) && parent == child->platformWidget()->parent)
        gtk_container_remove(GTK_CONTAINER(parent), child->platformWidget());
}

IntRect ScrollView::visibleContentRect(bool includeScrollbars) const
{
    if (!m_horizontalAdjustment)
        return IntRect(IntPoint(m_scrollOffset.width(), m_scrollOffset.height()),
                       IntSize(max(0, width() - (verticalScrollbar() && !includeScrollbars ? verticalScrollbar()->width() : 0)),
                               max(0, height() - (horizontalScrollbar() && !includeScrollbars ? horizontalScrollbar()->height() : 0))));

    // Main frame.
    GtkWidget* measuredWidget = hostWindow()->platformPageClient();
    GtkWidget* parent = gtk_widget_get_parent(measuredWidget);

    // We may not be in a widget that displays scrollbars, but we may
    // have other kinds of decoration that make us smaller.
    if (parent && includeScrollbars)
        measuredWidget = parent;

    return IntRect(IntPoint(m_scrollOffset.width(), m_scrollOffset.height()),
                   IntSize(measuredWidget->allocation.width,
                           measuredWidget->allocation.height));
}

void ScrollView::setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode, bool, bool)
{
    if (horizontalMode == m_horizontalScrollbarMode && verticalMode == m_verticalScrollbarMode)
        return;

    m_horizontalScrollbarMode = horizontalMode;
    m_verticalScrollbarMode = verticalMode;

    // We don't really care about reporting policy changes on frames
    // that have no adjustments attached to them.
    if (!m_horizontalAdjustment) {
        updateScrollbars(scrollOffset());
        return;
    }

    if (!isFrameView())
        return;

    // For frames that do have adjustments attached, we want to report
    // policy changes, so that they may be applied to the widget to
    // which the WebView has been added, for instance.
    if (hostWindow())
        hostWindow()->scrollbarsModeDidChange();
}

}
