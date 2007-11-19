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

#include "webkitgtkpage.h"
#include "webkitgtkglobal.h"
#include <gtk/gtk.h>

static GtkWidget* main_window;
static GtkWidget* url_entry;
static GtkStatusbar* main_statusbar;
static WebKitPage* web_page;
static gchar* main_title;
static gint load_progress;
static guint status_context_id;

static void
activate_url_entry_cb (GtkWidget* entry, gpointer data)
{
    const gchar* url = gtk_entry_get_text (GTK_ENTRY (entry));
    g_assert (url);
    webkit_page_open (web_page, url);
}

static void
update_title (GtkWindow* window)
{
    GString* string = g_string_new (main_title);
    g_string_append (string, " - WebKit Launcher");
    if (load_progress < 100)
        g_string_append_printf (string, " (%d%%)", load_progress);
    gchar* title = g_string_free (string, FALSE);
    gtk_window_set_title (window, title);
    g_free (title);
}

static void
link_hover_cb (WebKitPage* page, const gchar* title, const gchar* link, gpointer data)
{
    /* underflow is allowed */
    gtk_statusbar_pop (main_statusbar, status_context_id);
    if (link)
        gtk_statusbar_push (main_statusbar, status_context_id, link);
}

static void
title_change_cb (WebKitPage* page, const gchar* title, const gchar* url, gpointer data)
{
    gtk_entry_set_text (GTK_ENTRY (url_entry), url);

    if (main_title)
        g_free (main_title);
    main_title = g_strdup (title);
    update_title (GTK_WINDOW (main_window));
}

static void
progress_change_cb (WebKitPage* page, gint progress, gpointer data)
{
    load_progress = progress;
    update_title (GTK_WINDOW (main_window));
}

static void
destroy_cb (GtkWidget* widget, gpointer data)
{
    gtk_main_quit ();
}

static void
go_back_cb (GtkWidget* widget, gpointer data)
{
    webkit_page_go_backward (web_page);
}

static void
go_forward_cb (GtkWidget* widget, gpointer data)
{
    webkit_page_go_forward (web_page);
}

static GtkWidget*
create_browser ()
{
    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    web_page = WEBKIT_PAGE (webkit_page_new ());
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_page));

    g_signal_connect (G_OBJECT (web_page), "title-changed", G_CALLBACK (title_change_cb), web_page);
    g_signal_connect (G_OBJECT (web_page), "load-progress-changed", G_CALLBACK (progress_change_cb), web_page);
    g_signal_connect (G_OBJECT (web_page), "hovering-over-link", G_CALLBACK (link_hover_cb), web_page);

    return scrolled_window;
}

static GtkWidget*
create_statusbar ()
{
    main_statusbar = GTK_STATUSBAR (gtk_statusbar_new ());
    status_context_id = gtk_statusbar_get_context_id (main_statusbar, "Link Hover");

    return (GtkWidget*)main_statusbar;
}

static GtkWidget*
create_toolbar ()
{
    GtkWidget* toolbar = gtk_toolbar_new ();

    gtk_toolbar_set_orientation (GTK_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem* item;

    /* the back button */
    item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
    g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_back_cb), NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* The forward button */
    item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
    g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_forward_cb), NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* The URL entry */
    item = gtk_tool_item_new ();
    gtk_tool_item_set_expand (item, TRUE);
    url_entry = gtk_entry_new ();
    gtk_container_add (GTK_CONTAINER (item), url_entry);
    g_signal_connect (G_OBJECT (url_entry), "activate", G_CALLBACK (activate_url_entry_cb), NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* The go button */
    item = gtk_tool_button_new_from_stock (GTK_STOCK_OK);
    g_signal_connect_swapped (G_OBJECT (item), "clicked", G_CALLBACK (activate_url_entry_cb), (gpointer)url_entry);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    return toolbar;
}

static GtkWidget*
create_window ()
{
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "GtkLauncher");
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), NULL);

    return window;
}

int
main (int argc, char* argv[])
{
    gtk_init (&argc, &argv);
    webkit_init ();

    GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_toolbar (), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_statusbar (), FALSE, FALSE, 0);

    main_window = create_window ();
    gtk_container_add (GTK_CONTAINER (main_window), vbox);

    gchar* url = (gchar*) (argc > 1 ? argv[1] : "http://www.google.com/");
    webkit_page_open (web_page, url);

    gtk_widget_grab_focus (GTK_WIDGET (web_page));
    gtk_widget_show_all (main_window);
    gtk_main ();

    return 0;
}
