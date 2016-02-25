/*
 * Copyright (C) 2011, 2013 Igalia S.L.
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
#include "GtkUtilities.h"

#include "IntPoint.h"
#include <gtk/gtk.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

IntPoint convertWidgetPointToScreenPoint(GtkWidget* widget, const IntPoint& point)
{
    // FIXME: This is actually a very tricky operation and the results of this function should
    // only be thought of as a guess. For instance, sometimes it may not correctly take into
    // account window decorations.

    GtkWidget* toplevelWidget = gtk_widget_get_toplevel(widget);
    if (!toplevelWidget || !gtk_widget_is_toplevel(toplevelWidget) || !GTK_IS_WINDOW(toplevelWidget))
        return point;

    GdkWindow* gdkWindow = gtk_widget_get_window(toplevelWidget);
    if (!gdkWindow)
        return point;

    int xInWindow, yInWindow;
    gtk_widget_translate_coordinates(widget, toplevelWidget, point.x(), point.y(), &xInWindow, &yInWindow);

    int windowOriginX, windowOriginY;
    gdk_window_get_origin(gdkWindow, &windowOriginX, &windowOriginY);

    return IntPoint(windowOriginX + xInWindow, windowOriginY + yInWindow);
}

bool widgetIsOnscreenToplevelWindow(GtkWidget* widget)
{
    return widget && gtk_widget_is_toplevel(widget) && GTK_IS_WINDOW(widget) && !GTK_IS_OFFSCREEN_WINDOW(widget);
}

#if ENABLE(DEVELOPER_MODE)
static CString topLevelPath()
{
    if (const char* topLevelDirectory = g_getenv("WEBKIT_TOP_LEVEL"))
        return topLevelDirectory;

    // If the environment variable wasn't provided then assume we were built into
    // WebKitBuild/Debug or WebKitBuild/Release. Obviously this will fail if the build
    // directory is non-standard, but we can't do much more about this.
    GUniquePtr<char> parentPath(g_path_get_dirname(getCurrentExecutablePath().data()));
    GUniquePtr<char> layoutTestsPath(g_build_filename(parentPath.get(), "..", "..", "..", nullptr));
    GUniquePtr<char> absoluteTopLevelPath(realpath(layoutTestsPath.get(), 0));
    return absoluteTopLevelPath.get();
}

CString webkitBuildDirectory()
{
    const char* webkitOutputDir = g_getenv("WEBKIT_OUTPUTDIR");
    if (webkitOutputDir)
        return webkitOutputDir;

    GUniquePtr<char> outputDir(g_build_filename(topLevelPath().data(), "WebKitBuild", nullptr));
    return outputDir.get();
}
#endif

} // namespace WebCore
