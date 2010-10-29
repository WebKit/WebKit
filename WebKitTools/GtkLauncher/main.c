/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#include <gtk/gtk.h>
#include <webkit/webkit.h>

static gint window_count = 0;

static GtkWidget* create_window (WebKitWebView** out_web_view);

static void
activate_uri_entry_cb (GtkWidget* entry, gpointer data)
{
    WebKitWebView *web_view = g_object_get_data (G_OBJECT (entry), "web-view");
    const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
    g_assert (uri);
    webkit_web_view_load_uri (web_view, uri);
}

static void
update_title (GtkWindow* window, WebKitWebView* web_view)
{
    GString *string = g_string_new (webkit_web_view_get_title(web_view));
    gdouble load_progress = webkit_web_view_get_progress (web_view) * 100;
    g_string_append (string, " - WebKit Launcher");
    if (load_progress < 100)
        g_string_append_printf (string, " (%f%%)", load_progress);
    gchar* title = g_string_free (string, FALSE);
    gtk_window_set_title (window, title);
    g_free (title);
}

static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, GtkStatusbar* statusbar)
{
    guint status_context_id =
      GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (statusbar), "link-hover-context"));
    /* underflow is allowed */
    gtk_statusbar_pop (statusbar, status_context_id);
    if (link)
        gtk_statusbar_push (statusbar, status_context_id, link);
}

static void
notify_title_cb (WebKitWebView* web_view, GParamSpec* pspec, GtkWidget* window)
{
    update_title (GTK_WINDOW (window), web_view);
}

static void
notify_load_status_cb (WebKitWebView* web_view, GParamSpec* pspec, GtkWidget* uri_entry)
{
    if (webkit_web_view_get_load_status (web_view) == WEBKIT_LOAD_COMMITTED) {
        WebKitWebFrame* frame = webkit_web_view_get_main_frame (web_view);
        const gchar* uri = webkit_web_frame_get_uri (frame);
        if (uri)
            gtk_entry_set_text (GTK_ENTRY (uri_entry), uri);
    }
}

static void
notify_progress_cb (WebKitWebView* web_view, GParamSpec* pspec, GtkWidget* window)
{
    update_title (GTK_WINDOW (window), web_view);
}

static void
destroy_cb (GtkWidget* widget, GtkWidget* window)
{
    if (g_atomic_int_dec_and_test (&window_count))
      gtk_main_quit ();
}

static void
go_back_cb (GtkWidget* widget,  WebKitWebView* web_view)
{
    webkit_web_view_go_back (web_view);
}

static void
go_forward_cb (GtkWidget* widget, WebKitWebView* web_view)
{
    webkit_web_view_go_forward (web_view);
}

static WebKitWebView*
create_web_view_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, GtkWidget* window)
{
    WebKitWebView *new_web_view;
    create_window (&new_web_view);
    return new_web_view;
}

static gboolean
web_view_ready_cb (WebKitWebView* web_view, GtkWidget* window)
{
    gtk_widget_grab_focus (GTK_WIDGET (web_view));
    gtk_widget_show_all (window);
    return FALSE;
}

static gboolean
close_web_view_cb (WebKitWebView* web_view, GtkWidget* window)
{
    gtk_widget_destroy (window);
    return TRUE;
}

static GtkWidget*
create_browser (GtkWidget* window, GtkWidget* uri_entry, GtkWidget* statusbar, WebKitWebView* web_view)
{
    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

    g_signal_connect (web_view, "notify::title", G_CALLBACK (notify_title_cb), window);
    g_signal_connect (web_view, "notify::load-status", G_CALLBACK (notify_load_status_cb), uri_entry);
    g_signal_connect (web_view, "notify::progress", G_CALLBACK (notify_progress_cb), window);
    g_signal_connect (web_view, "hovering-over-link", G_CALLBACK (link_hover_cb), statusbar);
    g_signal_connect (web_view, "create-web-view", G_CALLBACK (create_web_view_cb), window);
    g_signal_connect (web_view, "web-view-ready", G_CALLBACK (web_view_ready_cb), window);
    g_signal_connect (web_view, "close-web-view", G_CALLBACK (close_web_view_cb), window);

    return scrolled_window;
}

static GtkWidget*
create_statusbar ()
{
    GtkStatusbar *statusbar = GTK_STATUSBAR (gtk_statusbar_new ());
    guint status_context_id = gtk_statusbar_get_context_id (statusbar, "Link Hover");
    g_object_set_data (G_OBJECT (statusbar), "link-hover-context",
        GUINT_TO_POINTER(status_context_id));

    return GTK_WIDGET (statusbar);
}

static GtkWidget*
create_toolbar (GtkWidget* uri_entry, WebKitWebView* web_view)
{
    GtkWidget* toolbar = gtk_toolbar_new ();

#if GTK_CHECK_VERSION(2,15,0)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
#else
    gtk_toolbar_set_orientation (GTK_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);
#endif
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem* item;

    /* the back button */
    item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
    g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_back_cb), web_view);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* The forward button */
    item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
    g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_forward_cb), web_view);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* The URL entry */
    item = gtk_tool_item_new ();
    gtk_tool_item_set_expand (item, TRUE);
    gtk_container_add (GTK_CONTAINER (item), uri_entry);
    g_signal_connect (G_OBJECT (uri_entry), "activate", G_CALLBACK (activate_uri_entry_cb), NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* The go button */
    g_object_set_data (G_OBJECT (uri_entry), "web-view", web_view);
    item = gtk_tool_button_new_from_stock (GTK_STOCK_OK);
    g_signal_connect_swapped (G_OBJECT (item), "clicked", G_CALLBACK (activate_uri_entry_cb), (gpointer)uri_entry);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    return toolbar;
}

static GtkWidget*
create_window (WebKitWebView** out_web_view)
{
    WebKitWebView *web_view;
    GtkWidget *vbox;
    GtkWidget *window;
    GtkWidget *uri_entry;
    GtkWidget *statusbar;

    g_atomic_int_inc (&window_count);
   
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "GtkLauncher");

    web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    uri_entry = gtk_entry_new ();

    vbox = gtk_vbox_new (FALSE, 0);
    statusbar = create_statusbar (web_view);
    gtk_box_pack_start (GTK_BOX (vbox), create_toolbar (uri_entry, web_view), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_browser (window, uri_entry, statusbar, web_view), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), statusbar, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (window), vbox);

    g_signal_connect (window, "destroy", G_CALLBACK (destroy_cb), NULL);

    if (out_web_view)
        *out_web_view = web_view;

    return window;
}

static gchar* filenameToURL(const char* filename)
{
    if (!g_file_test(filename, G_FILE_TEST_EXISTS))
        return 0;

    GFile *gfile = g_file_new_for_path(filename);
    gchar *fileURL = g_file_get_uri(gfile);
    g_object_unref(gfile);

    return fileURL;
}

int
main (int argc, char* argv[])
{
    WebKitWebView *web_view;
    GtkWidget *main_window;

    gtk_init (&argc, &argv);
    if (!g_thread_supported ())
        g_thread_init (NULL);

    main_window = create_window (&web_view);

    gchar *uri = (gchar*) (argc > 1 ? argv[1] : "http://www.google.com/");
    gchar *fileURL = filenameToURL(uri);

    webkit_web_view_load_uri(web_view, fileURL ? fileURL : uri);
    g_free(fileURL);

    gtk_widget_grab_focus (GTK_WIDGET (web_view));
    gtk_widget_show_all (main_window);
    gtk_main ();

    return 0;
}
