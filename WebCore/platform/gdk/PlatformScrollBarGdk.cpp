/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Holger Hans Peter Freyther zecke@selfish.org
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
#include "PlatformScrollBar.h"
#include "IntRect.h"
#include "GraphicsContext.h"

#include "NotImplemented.h"
#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <stdio.h>

using namespace WebCore;

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation,
                                     ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
{ 
    GtkScrollbar* scrollBar = orientation == HorizontalScrollbar ?
                              GTK_SCROLLBAR(::gtk_hscrollbar_new(NULL)) :
                              GTK_SCROLLBAR(::gtk_vscrollbar_new(NULL));
    gtk_widget_show(GTK_WIDGET(scrollBar));
    setGtkWidget(GTK_WIDGET(scrollBar));

    /*
     * assign a sane default width and height to the ScrollBar, otherwise
     * we will end up with a 0 width scrollbar.
     */
    resize(PlatformScrollbar::horizontalScrollbarHeight(),
           PlatformScrollbar::verticalScrollbarWidth());    

    g_object_ref(G_OBJECT(scrollBar));
    gtk_object_sink(GTK_OBJECT(scrollBar));
}

PlatformScrollbar::~PlatformScrollbar()
{
    /*
     * the Widget does not take over ownership.
     */
    gtk_widget_destroy(gtkWidget());
    g_object_unref(G_OBJECT(gtkWidget()));
}

int PlatformScrollbar::width() const
{
    return Widget::width();
}

int PlatformScrollbar::height() const
{
    return Widget::height();
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    Widget::setEnabled(enabled);
}

void PlatformScrollbar::paint(GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    Widget::paint(graphicsContext, damageRect);
}

void PlatformScrollbar::updateThumbPosition()
{ 
    notImplemented();
}

void PlatformScrollbar::updateThumbProportion()
{
    notImplemented();
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    setFrameGeometry(rect);
}

