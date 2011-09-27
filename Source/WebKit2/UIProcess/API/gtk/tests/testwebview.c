/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static void testWebViewDefaultContext(void)
{
    WebKitWebView *view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(view);
    g_assert(webkit_web_view_get_context(view) == webkit_web_context_get_default());
    g_object_unref(view);
}

int main(int argc, char **argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit2/webview/default_context",
                    testWebViewDefaultContext);

    return g_test_run();
}
