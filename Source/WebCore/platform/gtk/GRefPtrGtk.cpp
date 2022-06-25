/*
 *  Copyright (C) 2008 Collabora Ltd.
 *  Copyright (C) 2009 Martin Robinson
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
#include "GRefPtrGtk.h"

#include <glib.h>
#include <gtk/gtk.h>

#if USE(LIBSECRET)
#define SECRET_WITH_UNSTABLE 1
#define SECRET_API_SUBJECT_TO_CHANGE 1
#include <libsecret/secret.h>
#endif

namespace WTF {

#if !USE(GTK4)
template <> GtkTargetList* refGPtr(GtkTargetList* ptr)
{
    if (ptr)
        gtk_target_list_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GtkTargetList* ptr)
{
    if (ptr)
        gtk_target_list_unref(ptr);
}
#endif

#if USE(LIBSECRET)
template <> SecretValue* refGPtr(SecretValue* ptr)
{
    if (ptr)
        secret_value_ref(ptr);
    return ptr;
}

template <> void derefGPtr(SecretValue* ptr)
{
    if (ptr)
        secret_value_unref(ptr);
}
#endif

#if !USE(GTK4)
template <> GtkWidgetPath* refGPtr(GtkWidgetPath* ptr)
{
    if (ptr)
        gtk_widget_path_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GtkWidgetPath* ptr)
{
    if (ptr)
        gtk_widget_path_unref(ptr);
}
#endif

#if USE(GTK4)
template <> GskRenderNode* refGPtr(GskRenderNode* ptr)
{
    if (ptr)
        gsk_render_node_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GskRenderNode* ptr)
{
    if (ptr)
        gsk_render_node_unref(ptr);
}
#endif

}
