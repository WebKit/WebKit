/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Holger Hans Peter Freyther
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

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderLayer.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate
{
public:
    ScrollViewPrivate()
        : hasStaticBackground(false)
        , suppressScrollbars(false)
        , vScrollbarMode(ScrollbarAuto)
        , hScrollbarMode(ScrollbarAuto)
        , layout(0)
        , scrollBarsNeedUpdate(false)
    { }

    bool hasStaticBackground;
    bool suppressScrollbars;
    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
    GtkLayout *layout;
    IntSize contentsSize;
    IntSize viewPortSize;
    bool scrollBarsNeedUpdate;
};

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate)
{}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateView(const IntRect& updateRect, bool now)
{
    GdkRectangle rect = { updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height() };
    GdkDrawable* gdkdrawable = Widget::gdkDrawable();
    if (GDK_IS_WINDOW(gdkdrawable)) {
        GdkWindow* window = GDK_WINDOW(gdkdrawable);
        gdk_window_invalidate_rect(window, &rect, true);
        if (now)
            gdk_window_process_updates(window, true);
    }
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    updateView(updateRect, now);
}

void ScrollView::update()
{
    ASSERT(gtkWidget());
    gtk_widget_queue_draw(gtkWidget());

    if (m_data->scrollBarsNeedUpdate)
        updateScrollbars();
}

int ScrollView::visibleWidth() const
{
    return m_data->viewPortSize.width();
}

int ScrollView::visibleHeight() const
{
    return m_data->viewPortSize.height();
}

// Region of the content currently visible in the viewport in the content view's coordinate system.
FloatRect ScrollView::visibleContentRect() const
{
    return FloatRect(contentsX(), contentsY(), visibleWidth(), visibleHeight());
}

FloatRect ScrollView::visibleContentRectConsideringExternalScrollers() const
{
    // external scrollers not supported for now
    return visibleContentRect();
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::resizeContents(int w, int h)
{
    IntSize newSize(w, h);
    if (m_data->contentsSize == newSize)
        return;

    m_data->contentsSize = newSize;
    
    if (m_data->layout) {
        gtk_layout_set_size(m_data->layout, w, h);
        m_data->scrollBarsNeedUpdate = true;
    }
}

int ScrollView::contentsX() const
{
    g_return_val_if_fail(gtk_layout_get_hadjustment(m_data->layout), 0);
    return static_cast<int>(gtk_adjustment_get_value(gtk_layout_get_hadjustment(m_data->layout)));
}

int ScrollView::contentsY() const
{
    g_return_val_if_fail(gtk_layout_get_vadjustment(m_data->layout), 0);
    return static_cast<int>(gtk_adjustment_get_value(gtk_layout_get_vadjustment(m_data->layout)));
}

int ScrollView::contentsWidth() const
{
    return m_data->contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->contentsSize.height();
}

IntSize ScrollView::scrollOffset() const
{
    g_return_val_if_fail(m_data, IntSize());
    return IntSize(contentsX(), contentsY());
}

void ScrollView::scrollBy(int dx, int dy)
{
    GtkAdjustment* hadj = gtk_layout_get_hadjustment(m_data->layout);
    GtkAdjustment* vadj = gtk_layout_get_vadjustment(m_data->layout);
    g_return_if_fail(hadj);
    g_return_if_fail(vadj);

    int current_x = contentsX();
    int current_y = contentsY();


    gtk_adjustment_set_value(hadj,
                             CLAMP(current_x+dx, hadj->lower,
                                   MAX(0.0, hadj->upper - hadj->page_size)));
    gtk_adjustment_set_value(vadj,
                             CLAMP(current_y+dy, vadj->lower,
                                   MAX(0.0, vadj->upper - vadj->page_size)));
}

ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->hScrollbarMode;
}

ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->vScrollbarMode;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    m_data->suppressScrollbars = suppressed;
    if (repaintOnSuppress)
        updateScrollbars();
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->hScrollbarMode != newMode) {
        m_data->hScrollbarMode = newMode;
        updateScrollbars();
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->vScrollbarMode != newMode) {
        m_data->vScrollbarMode = newMode;
        updateScrollbars();
    }
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    m_data->hScrollbarMode = m_data->vScrollbarMode = newMode;
    updateScrollbars();
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

/*
 * only update the adjustments, the GtkLayout is doing the work
 * for us (assuming it is in a box...).
 */
void ScrollView::setFrameGeometry(const IntRect& r)
{
    Widget::setFrameGeometry(r);
    updateGeometry(r.width(), r.height());
}

void ScrollView::updateGeometry(int width, int height)
{
    m_data->viewPortSize = IntSize(width,height);
}

void ScrollView::setGtkWidget(GtkLayout* layout)
{
    g_return_if_fail(GTK_LAYOUT(layout));
    m_data->layout = layout;

    Widget::setGtkWidget(GTK_WIDGET(layout));
}

void ScrollView::addChild(Widget* w)
{ 
    ASSERT(w->gtkWidget());
    ASSERT(m_data->layout);

    gtk_layout_put(m_data->layout, w->gtkWidget(), 0, 0);
}

void ScrollView::removeChild(Widget* w)
{
    ASSERT(w->gtkWidget());
    ASSERT(m_data->layout);

    gtk_container_remove(GTK_CONTAINER(m_data->layout), w->gtkWidget());
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect&)
{
    notImplemented();
}

bool ScrollView::inWindow() const
{
    notImplemented();
    return true;
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{
    notImplemented();
}

void ScrollView::updateScrollbars()
{
    GtkAdjustment* hadj = gtk_layout_get_hadjustment(m_data->layout);
    GtkAdjustment* vadj = gtk_layout_get_vadjustment(m_data->layout);

    g_return_if_fail(hadj);
    g_return_if_fail(vadj);
    
    hadj->page_size = visibleWidth();
    hadj->step_increment = 13;
    
    vadj->page_size = visibleHeight();
    vadj->step_increment = 13;
    
    gtk_adjustment_changed(hadj);
    gtk_adjustment_value_changed(hadj);
    
    gtk_adjustment_changed(vadj);
    gtk_adjustment_value_changed(vadj);

    m_data->scrollBarsNeedUpdate = false;
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{ 
    return point;
}

IntPoint ScrollView::contentsToWindow(const IntPoint& point) const
{
    return point;
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{ 
    notImplemented();
    return 0;
}

}
