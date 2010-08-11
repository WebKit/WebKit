/*
 * Copyright (C) 2010 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GtkVersioning.h"

#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION(2, 14, 0)
void gtk_adjustment_set_value(GtkAdjustment* adjusment, gdouble value)
{
        m_adjustment->value = m_currentPos;
        gtk_adjustment_value_changed(m_adjustment);
}

void gtk_adjustment_configure(GtkAdjustment* adjustment, gdouble value, gdouble lower, gdouble upper,
                              gdouble stepIncrement, gdouble pageIncrement, gdouble pageSize)
{
    g_object_freeze_notify(G_OBJECT(adjustment));

    g_object_set(adjustment,
                 "lower", lower,
                 "upper", upper,
                 "step-increment", stepIncrement,
                 "page-increment", pageIncrement,
                 "page-size", pageSize,
                 NULL);

    g_object_thaw_notify(G_OBJECT(adjustment));

    gtk_adjustment_changed(adjustment);
    gtk_adjustment_value_changed(adjustment);
}
#endif
