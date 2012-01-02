/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BrowserWindow.h"

enum {
    PROP_0,

    PROP_VIEW
};

struct _BrowserWindow {
    GtkWindow parent;

    GtkWidget *mainBox;
    GtkWidget *toolbar;
    GtkWidget *uriEntry;
    GtkWidget *backItem;
    GtkWidget *forwardItem;
    GtkWidget *statusLabel;
    WebKitWebView *webView;

};

struct _BrowserWindowClass {
    GtkWindowClass parent;
};

static const char *defaultWindowTitle = "WebKitGTK+ MiniBrwoser";
static gint windowCount = 0;

G_DEFINE_TYPE(BrowserWindow, browser_window, GTK_TYPE_WINDOW)

static void browserWindowSetStatusText(BrowserWindow *window, const char *text)
{
#if GTK_CHECK_VERSION(3, 2, 0)
    gtk_label_set_text(GTK_LABEL(window->statusLabel), text);
    gtk_widget_set_visible(window->statusLabel, !!text);
#endif
}

static void resetStatusText(GtkWidget *widget, BrowserWindow *window)
{
    browserWindowSetStatusText(window, NULL);
}

static void activateUriEntryCallback(BrowserWindow *window)
{
    webkit_web_view_load_uri(window->webView, gtk_entry_get_text(GTK_ENTRY(window->uriEntry)));
}

static void goBackCallback(BrowserWindow *window)
{
    webkit_web_view_go_back(window->webView);
}

static void goForwardCallback(BrowserWindow *window)
{
    webkit_web_view_go_forward(window->webView);
}

static void webViewURIChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserWindow *window)
{
    gtk_entry_set_text(GTK_ENTRY(window->uriEntry), webkit_web_view_get_uri(webView));
}

static void webViewTitleChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserWindow *window)
{
    const char *title = webkit_web_view_get_title(webView);
    gtk_window_set_title(GTK_WINDOW(window), title ? title : defaultWindowTitle);
}

static gboolean resetEntryProgress(GtkEntry *entry)
{
    gtk_entry_set_progress_fraction(entry, 0);
    return FALSE;
}

static void webViewLoadProgressChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserWindow *window)
{
    gdouble progress = webkit_web_view_get_estimated_load_progress(webView);
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), progress);
    if (progress == 1.0)
        g_timeout_add(500, (GSourceFunc)resetEntryProgress, window->uriEntry);
}

static void browserWindowHistoryItemSelected(BrowserWindow *window, GtkMenuItem *item)
{
    GtkAction *action = gtk_activatable_get_related_action(GTK_ACTIVATABLE(item));
    browserWindowSetStatusText(window, action ? gtk_action_get_name(action) : NULL);
}

static void browserWindowHistoryItemActivated(BrowserWindow *window, GtkAction *action)
{
    WebKitBackForwardListItem *item = g_object_get_data(G_OBJECT(action), "back-forward-list-item");
    if (!item)
        return;

    webkit_web_view_go_to_back_forward_list_item(window->webView, item);
}

static GtkWidget *browserWindowCreateBackForwardMenu(BrowserWindow *window, GList *list)
{
    if (!list)
        return NULL;

    GtkWidget *menu = gtk_menu_new();
    GList *listItem;
    for (listItem = list; listItem; listItem = g_list_next(listItem)) {
        WebKitBackForwardListItem *item = (WebKitBackForwardListItem *)listItem->data;
        const char *uri = webkit_back_forward_list_item_get_uri(item);
        const char *title = webkit_back_forward_list_item_get_title(item);

        GtkAction *action = gtk_action_new(uri, title, NULL, NULL);
        g_object_set_data_full(G_OBJECT(action), "back-forward-list-item", g_object_ref(item), g_object_unref);
        g_signal_connect_swapped(action, "activate", G_CALLBACK(browserWindowHistoryItemActivated), window);

        GtkWidget *menuItem = gtk_action_create_menu_item(action);
        g_signal_connect_swapped(menuItem, "select", G_CALLBACK(browserWindowHistoryItemSelected), window);
        g_object_unref(action);

        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menuItem);
        gtk_widget_show(menuItem);
    }

    /* FIXME: This shoulnd't be necessary when didMouseMoveOverElement
     * is implemented in WebKit2 GTK+ API.
     */
    g_signal_connect(menu, "hide", G_CALLBACK(resetStatusText), window);

    return menu;
}

static void browserWindowUpdateNavigationActions(BrowserWindow *window, WebKitBackForwardList *backForwadlist)
{
    gtk_widget_set_sensitive(window->backItem, webkit_web_view_can_go_back(window->webView));
    gtk_widget_set_sensitive(window->forwardItem, webkit_web_view_can_go_forward(window->webView));

    GList *list = webkit_back_forward_list_get_back_list_with_limit(backForwadlist, 10);
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(window->backItem),
                                  browserWindowCreateBackForwardMenu(window, list));
    g_list_free(list);

    list = webkit_back_forward_list_get_forward_list_with_limit(backForwadlist, 10);
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(window->forwardItem),
                                  browserWindowCreateBackForwardMenu(window, list));
    g_list_free(list);
}

static void backForwadlistChanged(WebKitBackForwardList *backForwadlist, WebKitBackForwardListItem *itemAdded, GList *itemsRemoved, BrowserWindow *window)
{
    browserWindowUpdateNavigationActions(window, backForwadlist);
}

static void webViewClose(WebKitWebView *webView, BrowserWindow *window)
{
    gtk_widget_destroy(GTK_WIDGET(window));
}

static void webViewReadyToShow(WebKitWebView *webView, BrowserWindow *window)
{
    WebKitWindowProperties *windowProperties = webkit_web_view_get_window_properties(webView);

    GdkRectangle geometry;
    webkit_window_properties_get_geometry(windowProperties, &geometry);
    if (geometry.x >= 0 && geometry.y >= 0)
        gtk_window_move(GTK_WINDOW(window), geometry.x, geometry.y);
    if (geometry.width > 0 && geometry.height > 0)
        gtk_window_resize(GTK_WINDOW(window), geometry.width, geometry.height);

    if (!webkit_window_properties_get_toolbar_visible(windowProperties))
        gtk_widget_hide(window->toolbar);
    else if (!webkit_window_properties_get_locationbar_visible(windowProperties))
        gtk_widget_hide(window->uriEntry);

    if (!webkit_window_properties_get_resizable(windowProperties))
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    gtk_widget_show(GTK_WIDGET(window));
}

static GtkWidget *webViewCreate(WebKitWebView *webView, BrowserWindow *window)
{
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(webkit_web_view_get_context(webView)));
    webkit_web_view_set_settings(newWebView, webkit_web_view_get_settings(webView));

    GtkWidget *newWindow = browser_window_new(newWebView);
    g_signal_connect(newWebView, "ready-to-show", G_CALLBACK(webViewReadyToShow), newWindow);
    g_signal_connect(newWebView, "close", G_CALLBACK(webViewClose), newWindow);
    return GTK_WIDGET(newWebView);
}

static void browserWindowFinalize(GObject *gObject)
{
    G_OBJECT_CLASS(browser_window_parent_class)->finalize(gObject);

    if (g_atomic_int_dec_and_test(&windowCount))
        gtk_main_quit();
}

static void browserWindowGetProperty(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    BrowserWindow *window = BROWSER_WINDOW(object);

    switch (propId) {
    case PROP_VIEW:
        g_value_set_object(value, browser_window_get_view(window));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserWindowSetProperty(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    BrowserWindow* window = BROWSER_WINDOW(object);

    switch (propId) {
    case PROP_VIEW:
        window->webView = g_value_get_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browser_window_init(BrowserWindow *window)
{
    g_atomic_int_inc(&windowCount);

    gtk_window_set_title(GTK_WINDOW(window), defaultWindowTitle);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    window->uriEntry = gtk_entry_new();
    g_signal_connect_swapped(window->uriEntry, "activate", G_CALLBACK(activateUriEntryCallback), (gpointer)window);

    GtkWidget *toolbar = gtk_toolbar_new();
    window->toolbar = toolbar;
    gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem *item = gtk_menu_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    window->backItem = GTK_WIDGET(item);
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(item), 0);
    g_signal_connect_swapped(item, "clicked", G_CALLBACK(goBackCallback), (gpointer)window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_menu_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    window->forwardItem = GTK_WIDGET(item);
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(item), 0);
    g_signal_connect_swapped(G_OBJECT(item), "clicked", G_CALLBACK(goForwardCallback), (gpointer)window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_tool_item_new();
    gtk_tool_item_set_expand(item, TRUE);
    gtk_container_add(GTK_CONTAINER(item), window->uriEntry);
    gtk_widget_show(window->uriEntry);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_tool_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect_swapped(item, "clicked", G_CALLBACK(activateUriEntryCallback), (gpointer)window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    window->mainBox = vbox;
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_widget_show(toolbar);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);
}

static void browserWindowConstructed(GObject *gObject)
{
    BrowserWindow *window = BROWSER_WINDOW(gObject);

    g_signal_connect(window->webView, "notify::uri", G_CALLBACK(webViewURIChanged), window);
    g_signal_connect(window->webView, "notify::estimated-load-progress", G_CALLBACK(webViewLoadProgressChanged), window);
    g_signal_connect(window->webView, "notify::title", G_CALLBACK(webViewTitleChanged), window);
    g_signal_connect(window->webView, "create", G_CALLBACK(webViewCreate), window);

    WebKitBackForwardList *backForwadlist = webkit_web_view_get_back_forward_list(window->webView);
    g_signal_connect(backForwadlist, "changed", G_CALLBACK(backForwadlistChanged), window);

#if GTK_CHECK_VERSION(3, 2, 0)
    GtkWidget *overlay = gtk_overlay_new();
    gtk_box_pack_start(GTK_BOX(window->mainBox), overlay, TRUE, TRUE, 0);
    gtk_widget_show(overlay);

    window->statusLabel = gtk_label_new(NULL);
    gtk_widget_set_halign(window->statusLabel, GTK_ALIGN_START);
    gtk_widget_set_valign(window->statusLabel, GTK_ALIGN_END);
    gtk_widget_set_margin_left(window->statusLabel, 1);
    gtk_widget_set_margin_right(window->statusLabel, 1);
    gtk_widget_set_margin_top(window->statusLabel, 1);
    gtk_widget_set_margin_bottom(window->statusLabel, 1);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), window->statusLabel);

    gtk_container_add(GTK_CONTAINER(overlay), GTK_WIDGET(window->webView));
#else
    gtk_box_pack_start(GTK_BOX(window->mainBox), GTK_WIDGET(window->webView), TRUE, TRUE, 0);
#endif
    gtk_widget_show(GTK_WIDGET(window->webView));
}

static void browser_window_class_init(BrowserWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->constructed = browserWindowConstructed;
    gobjectClass->get_property = browserWindowGetProperty;
    gobjectClass->set_property = browserWindowSetProperty;
    gobjectClass->finalize = browserWindowFinalize;

    g_object_class_install_property(gobjectClass,
                                    PROP_VIEW,
                                    g_param_spec_object("view",
                                                        "View",
                                                        "The web view of this window",
                                                        WEBKIT_TYPE_WEB_VIEW,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

// Public API.
GtkWidget *browser_window_new(WebKitWebView *view)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(view), 0);

    return GTK_WIDGET(g_object_new(BROWSER_TYPE_WINDOW,
                                   "type", GTK_WINDOW_TOPLEVEL,
                                   "view", view, NULL));
}

WebKitWebView *browser_window_get_view(BrowserWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_WINDOW(window), 0);

    return window->webView;
}
