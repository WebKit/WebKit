/*
 *  Copyright (C) 2014 Igalia S.L.
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

#ifndef GUniquePtrGtk_h
#define GUniquePtrGtk_h

#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTF {

#if !USE(GTK4)
WTF_DEFINE_GPTR_DELETER(GdkEvent, gdk_event_free)
#endif

WTF_DEFINE_GPTR_DELETER(GtkTreePath, gtk_tree_path_free)

} // namespace WTF

#endif
