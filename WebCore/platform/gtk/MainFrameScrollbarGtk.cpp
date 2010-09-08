/*
 *  Copyright (C) 2007, 2009 Holger Hans Peter Freyther zecke@selfish.org
 *  Copyright (C) 2010 Gustavo Noronha Silva <gns@gnome.org>
 *  Copyright (C) 2010 Collabora Ltd.
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "MainFrameScrollbarGtk.h"

#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "IntRect.h"
#include <gtk/gtk.h>

using namespace WebCore;

PassRefPtr<MainFrameScrollbarGtk> MainFrameScrollbarGtk::create(ScrollbarClient* client, ScrollbarOrientation orientation, GtkAdjustment* adj)
{
    return adoptRef(new MainFrameScrollbarGtk(client, orientation, adj));
}

// Main frame scrollbars are slaves to a GtkAdjustment. If a main frame
// scrollbar has an m_adjustment, it belongs to the container (a GtkWidget such
// as GtkScrolledWindow). The adjustment may also be null, in which case there
// is no containing view or the parent ScrollView is in some sort of transition
// state. These scrollbars are never painted, as the container takes care of
// that. They exist only to shuttle data from the GtkWidget container into
// WebCore and vice-versa.
MainFrameScrollbarGtk::MainFrameScrollbarGtk(ScrollbarClient* client, ScrollbarOrientation orientation, GtkAdjustment* adjustment)
    : Scrollbar(client, orientation, RegularScrollbar)
    , m_adjustment(0)
{
    attachAdjustment(adjustment);

    // We have nothing to show as we are solely operating on the GtkAdjustment.
    resize(0, 0);
}

MainFrameScrollbarGtk::~MainFrameScrollbarGtk()
{
    if (m_adjustment)
        detachAdjustment();
}

void MainFrameScrollbarGtk::attachAdjustment(GtkAdjustment* adjustment)
{
    if (m_adjustment)
        detachAdjustment();

    m_adjustment = adjustment;
    if (!m_adjustment)
        return;

    g_signal_connect(m_adjustment.get(), "value-changed", G_CALLBACK(MainFrameScrollbarGtk::gtkValueChanged), this);
    updateThumbProportion();
    updateThumbPosition();
}

void MainFrameScrollbarGtk::detachAdjustment()
{
    if (!m_adjustment)
        return;

    g_signal_handlers_disconnect_by_func(G_OBJECT(m_adjustment.get()), (gpointer)MainFrameScrollbarGtk::gtkValueChanged, this);

    // For the case where we only operate on the GtkAdjustment it is best to
    // reset the values so that the surrounding scrollbar gets updated, or
    // e.g. for a GtkScrolledWindow the scrollbar gets hidden.
    gtk_adjustment_configure(m_adjustment.get(), 0, 0, 0, 0, 0, 0);

    m_adjustment = 0;
}

void MainFrameScrollbarGtk::updateThumbPosition()
{
    if (!m_adjustment || gtk_adjustment_get_value(m_adjustment.get()) == m_currentPos)
        return;
    gtk_adjustment_set_value(m_adjustment.get(), m_currentPos);
}

void MainFrameScrollbarGtk::updateThumbProportion()
{
    if (!m_adjustment)
        return;
    gtk_adjustment_configure(m_adjustment.get(),
                             gtk_adjustment_get_value(m_adjustment.get()),
                             gtk_adjustment_get_lower(m_adjustment.get()),
                             m_totalSize,
                             m_lineStep,
                             m_pageStep,
                             m_visibleSize);
}

void MainFrameScrollbarGtk::gtkValueChanged(GtkAdjustment*, MainFrameScrollbarGtk* that)
{
    that->setValue(static_cast<int>(gtk_adjustment_get_value(that->m_adjustment.get())), NotFromScrollAnimator);
}

void MainFrameScrollbarGtk::paint(GraphicsContext* context, const IntRect& rect)
{
    // Main frame scrollbars are not painted by WebCore.
    return;
}
