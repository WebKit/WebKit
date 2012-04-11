/*
 * Copyright (C) 2012 Igalia, S.L.
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
#include "GLContext.h"

#include "GLContextGLX.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

namespace WebCore {

typedef HashMap<GtkWidget*, OwnPtr<GLContext> > WindowContextMap;
static WindowContextMap& windowContextsMap()
{
    DEFINE_STATIC_LOCAL(WindowContextMap, windowContexts, ());
    return windowContexts;
}

static void shutdownGLContext(GtkWidget* widget, void*)
{
    WindowContextMap& windowContexts = windowContextsMap();
    WindowContextMap::iterator i = windowContexts.find(widget);
    if (i != windowContexts.end())
        windowContexts.remove(i);
}

GLContext* GLContext::getContextForWidget(GtkWidget* widget)
{
    ASSERT(widget);

    WindowContextMap& windowContexts = windowContextsMap();
    WindowContextMap::iterator i = windowContextsMap().find(widget);
    if (i != windowContexts.end())
        return i->second.get();

    // It's important that this context doesn't hang around after the window
    // is unmapped, so we make sure to clean it up once that happens.
    if (!g_signal_handler_find(widget, G_SIGNAL_MATCH_FUNC, 0, 0, 0, reinterpret_cast<void*>(shutdownGLContext), 0))
        g_signal_connect(widget, "unmap", G_CALLBACK(shutdownGLContext), 0);

    GLContext* context = 0;

#if USE(GLX)
    // If this GDK window doesn't have its own native window then, we don't want
    // to use it for rendering, since we'll be drawing over some other widget's area.
    GdkWindow* gdkWindow = gtk_widget_get_window(widget);
    context = gdkWindow && gdk_window_has_native(gdkWindow) ?  GLContextGLX::createContext(GDK_WINDOW_XID(gdkWindow)) : GLContextGLX::createContext(0);
#endif

    if (!context)
        return 0;

    windowContexts.set(widget, adoptPtr(context));
    return context;
}

} // namespace WebCore
