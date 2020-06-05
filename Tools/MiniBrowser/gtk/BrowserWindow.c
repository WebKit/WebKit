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

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "BrowserWindow.h"

#include "BrowserDownloadsBar.h"
#include "BrowserSearchBar.h"
#include "BrowserSettingsDialog.h"
#include "BrowserTab.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>

struct _BrowserWindow {
    GtkApplicationWindow parent;

    WebKitWebContext *webContext;

    GtkWidget *mainBox;
    GtkWidget *toolbar;
    GtkWidget *uriEntry;
    GtkWidget *backItem;
    GtkWidget *forwardItem;
    GtkWidget *editToolbar;
    GtkWidget *settingsDialog;
    GtkWidget *notebook;
    BrowserTab *activeTab;
    GtkWidget *downloadsBar;
    gboolean searchBarVisible;
    gboolean fullScreenIsEnabled;
#if GTK_CHECK_VERSION(3, 98, 0)
    GdkTexture *favicon;
#else
    GdkPixbuf *favicon;
#endif
    GtkWidget *reloadOrStopButton;
    GtkWindow *parentWindow;
    guint resetEntryProgressTimeoutId;
    gchar *sessionFile;
    GdkRGBA backgroundColor;
};

struct _BrowserWindowClass {
    GtkApplicationWindowClass parent;
};

static const char *defaultWindowTitle = "WebKitGTK MiniBrowser";
static const gdouble minimumZoomLevel = 0.5;
static const gdouble maximumZoomLevel = 3;
static const gdouble defaultZoomLevel = 1;
static const gdouble zoomStep = 1.2;

G_DEFINE_TYPE(BrowserWindow, browser_window, GTK_TYPE_APPLICATION_WINDOW)

static char *getExternalURI(const char *uri)
{
    /* From the user point of view we support about: prefix. */
    if (uri && g_str_has_prefix(uri, BROWSER_ABOUT_SCHEME))
        return g_strconcat("about", uri + strlen(BROWSER_ABOUT_SCHEME), NULL);

    return g_strdup(uri);
}

static void browserWindowSetStatusText(BrowserWindow *window, const char *text)
{
    browser_tab_set_status_text(window->activeTab, text);
}

static void resetStatusText(GtkWidget *widget, BrowserWindow *window)
{
    browserWindowSetStatusText(window, NULL);
}

static void activateUriEntryCallback(BrowserWindow *window)
{
    browser_window_load_uri(window,
#if GTK_CHECK_VERSION(3, 98, 0)
        gtk_editable_get_text(GTK_EDITABLE(window->uriEntry))
#else
        gtk_entry_get_text(GTK_ENTRY(window->uriEntry))
#endif
    );
}

static void reloadOrStopCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    if (webkit_web_view_is_loading(webView))
        webkit_web_view_stop_loading(webView);
    else
        webkit_web_view_reload(webView);
}

static void goBackCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_tab_get_web_view(BROWSER_WINDOW(userData)->activeTab);
    webkit_web_view_go_back(webView);
}

static void goForwardCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_tab_get_web_view(BROWSER_WINDOW(userData)->activeTab);
    webkit_web_view_go_forward(webView);
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static void settingsCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    if (window->settingsDialog) {
        gtk_window_present(GTK_WINDOW(window->settingsDialog));
        return;
    }

    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    window->settingsDialog = browser_settings_dialog_new(webkit_web_view_get_settings(webView));
    gtk_window_set_transient_for(GTK_WINDOW(window->settingsDialog), GTK_WINDOW(window));
    g_object_add_weak_pointer(G_OBJECT(window->settingsDialog), (gpointer *)&window->settingsDialog);
    gtk_widget_show(window->settingsDialog);
}
#endif

static void webViewURIChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserWindow *window)
{
    char *externalURI = getExternalURI(webkit_web_view_get_uri(webView));
#if GTK_CHECK_VERSION(3, 98, 0)
    gtk_editable_set_text(GTK_EDITABLE(window->uriEntry), externalURI ? externalURI : "");
#else
    gtk_entry_set_text(GTK_ENTRY(window->uriEntry), externalURI ? externalURI : "");
#endif
    g_free(externalURI);
}

static void webViewTitleChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserWindow *window)
{
    const char *title = webkit_web_view_get_title(webView);
    if (!title)
        title = defaultWindowTitle;
    char *privateTitle = NULL;
    if (webkit_web_view_is_controlled_by_automation(webView))
        privateTitle = g_strdup_printf("[Automation] %s", title);
    else if (webkit_web_view_is_ephemeral(webView))
        privateTitle = g_strdup_printf("[Private] %s", title);
    gtk_window_set_title(GTK_WINDOW(window), privateTitle ? privateTitle : title);
    g_free(privateTitle);
}

static gboolean resetEntryProgress(BrowserWindow *window)
{
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), 0);
#endif
    window->resetEntryProgressTimeoutId = 0;
    return FALSE;
}

static void webViewLoadProgressChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserWindow *window)
{
    gdouble progress = webkit_web_view_get_estimated_load_progress(webView);
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), progress);
#endif
    if (progress == 1.0) {
        window->resetEntryProgressTimeoutId = g_timeout_add(500, (GSourceFunc)resetEntryProgress, window);
        g_source_set_name_by_id(window->resetEntryProgressTimeoutId, "[WebKit] resetEntryProgress");
    } else if (window->resetEntryProgressTimeoutId) {
        g_source_remove(window->resetEntryProgressTimeoutId);
        window->resetEntryProgressTimeoutId = 0;
    }
}

static void downloadStarted(WebKitWebContext *webContext, WebKitDownload *download, BrowserWindow *window)
{
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (!window->downloadsBar) {
        window->downloadsBar = browser_downloads_bar_new();
        gtk_box_pack_start(GTK_BOX(window->mainBox), window->downloadsBar, FALSE, FALSE, 0);
        gtk_box_reorder_child(GTK_BOX(window->mainBox), window->downloadsBar, 0);
        g_object_add_weak_pointer(G_OBJECT(window->downloadsBar), (gpointer *)&(window->downloadsBar));
        gtk_widget_show(window->downloadsBar);
    }
    browser_downloads_bar_add_download(BROWSER_DOWNLOADS_BAR(window->downloadsBar), download);
#endif
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static void browserWindowHistoryItemSelected(BrowserWindow *window, GtkMenuItem *menuItem)
{
    WebKitBackForwardListItem *item = g_object_get_data(G_OBJECT(menuItem), "back-forward-list-item");
    browserWindowSetStatusText(window, item ? webkit_back_forward_list_item_get_uri(item) : NULL);
}

static void browserWindowHistoryItemActivated(BrowserWindow *window, GtkMenuItem *menuItem)
{
    WebKitBackForwardListItem *item = g_object_get_data(G_OBJECT(menuItem), "back-forward-list-item");
    if (!item)
        return;

    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    webkit_web_view_go_to_back_forward_list_item(webView, item);
}

static GtkWidget *browserWindowCreateBackForwardMenu(BrowserWindow *window, GList *list)
{
    if (!list)
        return NULL;

    GtkWidget *menu = gtk_menu_new();
    GList *listItem;
    for (listItem = list; listItem; listItem = g_list_next(listItem)) {
        WebKitBackForwardListItem *item = (WebKitBackForwardListItem *)listItem->data;
        const char *title = webkit_back_forward_list_item_get_title(item);

        GtkWidget *menuItem = gtk_menu_item_new_with_label(title);
        g_object_set_data_full(G_OBJECT(menuItem), "back-forward-list-item", g_object_ref(item), g_object_unref);
        g_signal_connect_swapped(menuItem, "select", G_CALLBACK(browserWindowHistoryItemSelected), window);
        g_signal_connect_swapped(menuItem, "activate", G_CALLBACK(browserWindowHistoryItemActivated), window);

        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menuItem);
        gtk_widget_show(menuItem);
    }

    g_signal_connect(menu, "hide", G_CALLBACK(resetStatusText), window);

    return menu;
}
#endif

static void browserWindowUpdateNavigationMenu(BrowserWindow *window, WebKitBackForwardList *backForwardlist)
{
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(window), "go-back");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), webkit_web_view_can_go_back(webView));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "go-forward");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), webkit_web_view_can_go_forward(webView));

#if !GTK_CHECK_VERSION(3, 98, 0)
    GList *list = g_list_reverse(webkit_back_forward_list_get_back_list_with_limit(backForwardlist, 10));
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(window->backItem),
        browserWindowCreateBackForwardMenu(window, list));
    g_list_free(list);

    list = webkit_back_forward_list_get_forward_list_with_limit(backForwardlist, 10);
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(window->forwardItem),
        browserWindowCreateBackForwardMenu(window, list));
    g_list_free(list);
#endif
}

static void browserWindowTryCloseCurrentWebView(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    int currentPage = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));
    BrowserTab *tab = (BrowserTab *)gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), currentPage);
    webkit_web_view_try_close(browser_tab_get_web_view(tab));
}

static void browserWindowTryClose(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    GSList *webViews = NULL;
    int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    int i;

    for (i = 0; i < n; ++i) {
        BrowserTab *tab = (BrowserTab *)gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), i);
        webViews = g_slist_prepend(webViews, browser_tab_get_web_view(tab));
    }

    GSList *link;
    for (link = webViews; link; link = link->next)
        webkit_web_view_try_close(link->data);
}

static void backForwardlistChanged(WebKitBackForwardList *backForwardlist, WebKitBackForwardListItem *itemAdded, GList *itemsRemoved, BrowserWindow *window)
{
    browserWindowUpdateNavigationMenu(window, backForwardlist);
}

static void webViewClose(WebKitWebView *webView, BrowserWindow *window)
{
    int tabsCount = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    if (tabsCount == 1) {
#if GTK_CHECK_VERSION(3, 98, 4)
        gtk_window_destroy(GTK_WINDOW(window));
#else
        gtk_widget_destroy(GTK_WIDGET(window));
#endif
        return;
    }

    int i;
    for (i = 0; i < tabsCount; ++i) {
        BrowserTab *tab = (BrowserTab *)gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), i);
        if (browser_tab_get_web_view(tab) == webView) {
#if GTK_CHECK_VERSION(3, 98, 4)
            gtk_notebook_remove_page(GTK_NOTEBOOK(window->notebook), i);
#else
            gtk_widget_destroy(GTK_WIDGET(tab));
#endif
            return;
        }
    }
}

static void webViewRunAsModal(WebKitWebView *webView, BrowserWindow *window)
{
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), window->parentWindow);
}

static void webViewReadyToShow(WebKitWebView *webView, BrowserWindow *window)
{
    WebKitWindowProperties *windowProperties = webkit_web_view_get_window_properties(webView);

    GdkRectangle geometry;
    webkit_window_properties_get_geometry(windowProperties, &geometry);
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (geometry.x >= 0 && geometry.y >= 0)
        gtk_window_move(GTK_WINDOW(window), geometry.x, geometry.y);
#endif
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

static GtkWidget *webViewCreate(WebKitWebView *webView, WebKitNavigationAction *navigation, BrowserWindow *window)
{
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(webkit_web_view_new_with_related_view(webView));
    webkit_web_view_set_settings(newWebView, webkit_web_view_get_settings(webView));

    GtkWidget *newWindow = browser_window_new(GTK_WINDOW(window), window->webContext);
    gtk_window_set_application(GTK_WINDOW(newWindow), gtk_window_get_application(GTK_WINDOW(window)));
    browser_window_append_view(BROWSER_WINDOW(newWindow), newWebView);
    gtk_widget_grab_focus(GTK_WIDGET(newWebView));
    g_signal_connect(newWebView, "ready-to-show", G_CALLBACK(webViewReadyToShow), newWindow);
    g_signal_connect(newWebView, "run-as-modal", G_CALLBACK(webViewRunAsModal), newWindow);
    g_signal_connect(newWebView, "close", G_CALLBACK(webViewClose), newWindow);
    return GTK_WIDGET(newWebView);
}

static gboolean webViewEnterFullScreen(WebKitWebView *webView, BrowserWindow *window)
{
    gtk_widget_hide(window->toolbar);
    browser_tab_enter_fullscreen(window->activeTab);
    return FALSE;
}

static gboolean webViewLeaveFullScreen(WebKitWebView *webView, BrowserWindow *window)
{
    browser_tab_leave_fullscreen(window->activeTab);
    gtk_widget_show(window->toolbar);
    return FALSE;
}

static gboolean webViewLoadFailed(WebKitWebView *webView, WebKitLoadEvent loadEvent, const char *failingURI, GError *error, BrowserWindow *window)
{
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), 0.);
    return FALSE;
}

static gboolean webViewDecidePolicy(WebKitWebView *webView, WebKitPolicyDecision *decision, WebKitPolicyDecisionType decisionType, BrowserWindow *window)
{
    if (decisionType != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;

    WebKitNavigationAction *navigationAction = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    if (webkit_navigation_action_get_navigation_type(navigationAction) != WEBKIT_NAVIGATION_TYPE_LINK_CLICKED
        || webkit_navigation_action_get_mouse_button(navigationAction) != GDK_BUTTON_MIDDLE)
        return FALSE;

    /* Multiple tabs are not allowed in editor mode. */
    if (webkit_web_view_is_editable(webView))
        return FALSE;

    /* Opening a new tab if link clicked with the middle button. */
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "is-controlled-by-automation", webkit_web_view_is_controlled_by_automation(webView),
        NULL));
    browser_window_append_view(window, newWebView);
    webkit_web_view_load_request(newWebView, webkit_navigation_action_get_request(navigationAction));

    webkit_policy_decision_ignore(decision);
    return TRUE;
}

static void webViewMouseTargetChanged(WebKitWebView *webView, WebKitHitTestResult *hitTestResult, guint mouseModifiers, BrowserWindow *window)
{
    if (!webkit_hit_test_result_context_is_link(hitTestResult)) {
        browserWindowSetStatusText(window, NULL);
        return;
    }
    browserWindowSetStatusText(window, webkit_hit_test_result_get_link_uri(hitTestResult));
}

static gboolean browserWindowCanZoomIn(BrowserWindow *window)
{
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) * zoomStep;
    return zoomLevel < maximumZoomLevel;
}

static gboolean browserWindowCanZoomOut(BrowserWindow *window)
{
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) / zoomStep;
    return zoomLevel > minimumZoomLevel;
}

static gboolean browserWindowZoomIn(BrowserWindow *window)
{
    if (browserWindowCanZoomIn(window)) {
        WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
        gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) * zoomStep;
        webkit_web_view_set_zoom_level(webView, zoomLevel);
        return TRUE;
    }
    return FALSE;
}

static gboolean browserWindowZoomOut(BrowserWindow *window)
{
    if (browserWindowCanZoomOut(window)) {
        WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
        gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) / zoomStep;
        webkit_web_view_set_zoom_level(webView, zoomLevel);
        return TRUE;
    }
    return FALSE;
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static gboolean scrollEventCallback(WebKitWebView *webView, const GdkEventScroll *event, BrowserWindow *window)
{
    GdkModifierType mod = gtk_accelerator_get_default_mod_mask();

    if ((event->state & mod) != GDK_CONTROL_MASK)
        return FALSE;

    if (event->delta_y < 0)
        return browserWindowZoomIn(window);

    return browserWindowZoomOut(window);
}
#endif

static void browserWindowUpdateZoomActions(BrowserWindow *window)
{
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-in");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), browserWindowCanZoomIn(window));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-out");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), browserWindowCanZoomOut(window));
}

static void webViewZoomLevelChanged(GObject *object, GParamSpec *paramSpec, BrowserWindow *window)
{
    browserWindowUpdateZoomActions(window);
}

static void updateUriEntryIcon(BrowserWindow *window)
{
    GtkEntry *entry = GTK_ENTRY(window->uriEntry);
    if (window->favicon)
#if GTK_CHECK_VERSION(3, 98, 0)
        gtk_entry_set_icon_from_paintable(entry, GTK_ENTRY_ICON_PRIMARY, GDK_PAINTABLE(window->favicon));
#else
        gtk_entry_set_icon_from_pixbuf(entry, GTK_ENTRY_ICON_PRIMARY, window->favicon);
#endif
    else
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_PRIMARY, "document-new");
}

static void faviconChanged(WebKitWebView *webView, GParamSpec *paramSpec, BrowserWindow *window)
{
#if GTK_CHECK_VERSION(3, 98, 0)
    GdkTexture *favicon = NULL;
#else
    GdkPixbuf *favicon = NULL;
#endif
    cairo_surface_t *surface = webkit_web_view_get_favicon(webView);

    if (surface) {
        int width = cairo_image_surface_get_width(surface);
        int height = cairo_image_surface_get_height(surface);
#if GTK_CHECK_VERSION(3, 98, 0)
        int stride = cairo_image_surface_get_stride(surface);
        GBytes *bytes = g_bytes_new(cairo_image_surface_get_data(surface), stride * height);
        favicon = gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes, stride);
        g_bytes_unref(bytes);
#else
        favicon = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
#endif
    }

    if (window->favicon)
        g_object_unref(window->favicon);
    window->favicon = favicon;

    updateUriEntryIcon(window);
}

static void webViewIsLoadingChanged(WebKitWebView *webView, GParamSpec *paramSpec, BrowserWindow *window)
{
    gboolean isLoading = webkit_web_view_is_loading(webView);
    g_object_set(G_OBJECT(window->reloadOrStopButton), "label", isLoading ? "Stop" : "Reload", "icon-name", isLoading ? "process-stop" : "view-refresh", NULL);
}

static void zoomInCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browserWindowZoomIn(BROWSER_WINDOW(userData));
}

static void zoomOutCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browserWindowZoomOut(BROWSER_WINDOW(userData));
}

static void defaultZoomCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_tab_get_web_view(BROWSER_WINDOW(userData)->activeTab);
    webkit_web_view_set_zoom_level(webView, defaultZoomLevel);
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static void searchCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browser_tab_start_search(BROWSER_WINDOW(userData)->activeTab);
}
#endif

static void newTabCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    if (webkit_web_view_is_editable(webView))
        return;

    browser_window_append_view(window, WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "is-controlled-by-automation", webkit_web_view_is_controlled_by_automation(webView),
        NULL)));
    gtk_widget_grab_focus(window->uriEntry);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), -1);
}

static void toggleWebInspector(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browser_tab_toggle_inspector(BROWSER_WINDOW(userData)->activeTab);
}

static void openPrivateWindow(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "is-ephemeral", TRUE,
        "is-controlled-by-automation", webkit_web_view_is_controlled_by_automation(webView),
        NULL));
    GtkWidget *newWindow = browser_window_new(GTK_WINDOW(window), window->webContext);
    gtk_window_set_application(GTK_WINDOW(newWindow), gtk_window_get_application(GTK_WINDOW(window)));
    browser_window_append_view(BROWSER_WINDOW(newWindow), newWebView);
    gtk_widget_grab_focus(GTK_WIDGET(newWebView));
    gtk_widget_show(GTK_WIDGET(newWindow));
}

static void focusLocationBar(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    gtk_widget_grab_focus(BROWSER_WINDOW(userData)->uriEntry);
}

static void reloadPage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_tab_get_web_view(BROWSER_WINDOW(userData)->activeTab);
    webkit_web_view_reload(webView);
}

static void reloadPageIgnoringCache(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_tab_get_web_view(BROWSER_WINDOW(userData)->activeTab);
    webkit_web_view_reload_bypass_cache(webView);
}

static void stopPageLoad(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    browser_tab_stop_search(window->activeTab);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    if (webkit_web_view_is_loading(webView))
        webkit_web_view_stop_loading(webView);
}

static void loadHomePage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
}

static void toggleFullScreen(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    if (!window->fullScreenIsEnabled) {
        gtk_window_fullscreen(GTK_WINDOW(window));
        gtk_widget_hide(window->toolbar);
        window->fullScreenIsEnabled = TRUE;
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window));
        gtk_widget_show(window->toolbar);
        window->fullScreenIsEnabled = FALSE;
    }
}

static void webKitPrintOperationFailedCallback(WebKitPrintOperation *printOperation, GError *error)
{
    g_warning("Print failed: '%s'", error->message);
}

static void printPage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    WebKitPrintOperation *printOperation = webkit_print_operation_new(webView);

    g_signal_connect(printOperation, "failed", G_CALLBACK(webKitPrintOperationFailedCallback), NULL);
    webkit_print_operation_run_dialog(printOperation, GTK_WINDOW(window));
    g_object_unref(printOperation);
}

static void editingActionCallback(GSimpleAction *action, GVariant *prameter, gpointer userData)
{
    BrowserWindow *window = BROWSER_WINDOW(userData);
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    webkit_web_view_execute_editing_command(webView, g_action_get_name(G_ACTION(action)));
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static void insertImageCommandCallback(GtkWidget *widget, BrowserWindow *window)
{
    GtkWidget *fileChooser = gtk_file_chooser_dialog_new("Insert Image", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_pixbuf_formats(filter);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fileChooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(fileChooser)) == GTK_RESPONSE_ACCEPT) {
        char *uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(fileChooser));
        if (uri) {
            WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
            webkit_web_view_execute_editing_command_with_argument(webView, WEBKIT_EDITING_COMMAND_INSERT_IMAGE, uri);
            g_free(uri);
        }
    }

    gtk_widget_destroy(fileChooser);
}

static void insertLinkCommandCallback(GtkWidget *widget, BrowserWindow *window)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Insert Link", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Insert", GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "URL");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
    gtk_widget_show(entry);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
#if GTK_CHECK_VERSION(3, 98, 0)
        const char *url = gtk_editable_get_text(GTK_EDITABLE(entry));
#else
        const char *url = gtk_entry_get_text(GTK_ENTRY(entry));
#endif
        if (url && *url) {
            WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
            webkit_web_view_execute_editing_command_with_argument(webView, WEBKIT_EDITING_COMMAND_CREATE_LINK, url);
        }
    }

    gtk_widget_destroy(dialog);
}

static void typingAttributesChanged(WebKitEditorState *editorState, GParamSpec *spec, BrowserWindow *window)
{
    unsigned typingAttributes = webkit_editor_state_get_typing_attributes(editorState);
    GActionGroup *actionGroup = gtk_widget_get_action_group(window->editToolbar, "edit");
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "Bold");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD));
    action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "Italic");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC));
    action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "Underline");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE));
    action = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "Strikethrough");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH));
}
#endif

static void browserWindowFinalize(GObject *gObject)
{
    BrowserWindow *window = BROWSER_WINDOW(gObject);

    g_signal_handlers_disconnect_matched(window->webContext, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
    g_object_unref(window->webContext);

    if (window->favicon) {
        g_object_unref(window->favicon);
        window->favicon = NULL;
    }

    if (window->resetEntryProgressTimeoutId)
        g_source_remove(window->resetEntryProgressTimeoutId);

    g_free(window->sessionFile);

    G_OBJECT_CLASS(browser_window_parent_class)->finalize(gObject);
}

static void browserWindowDispose(GObject *gObject)
{
    BrowserWindow *window = BROWSER_WINDOW(gObject);

    if (window->parentWindow) {
        g_object_remove_weak_pointer(G_OBJECT(window->parentWindow), (gpointer *)&window->parentWindow);
        window->parentWindow = NULL;
    }

    G_OBJECT_CLASS(browser_window_parent_class)->dispose(gObject);
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static GtkWidget *
browserWindowSetupEditorToolbarItem(BrowserWindow* window, GtkWidget* toolbar, GType type, const char* label, const char* namedIcon, const char* actionName)
{
    GtkWidget *item = g_object_new(type, "icon-name", namedIcon, NULL);
    gtk_widget_set_name(item, label);
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), label);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(item), actionName);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);
    gtk_widget_show(item);

    return item;
}
#endif

static const GActionEntry editActions[] = {
    { "Bold", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Italic", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Underline", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Strikethrough", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Cut", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Copy", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Paste", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Undo", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Redo", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "JustifyLeft", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "JustifyCenter", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "JustifyRight", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Indent", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Outdent", editingActionCallback, NULL, NULL, NULL, { 0 } },
};

static void browserWindowSetupEditorToolbar(BrowserWindow *window)
{
#if !GTK_CHECK_VERSION(3, 98, 0)
    GtkWidget *toolbar = gtk_toolbar_new();
    window->editToolbar = toolbar;
    gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(actionGroup), editActions, G_N_ELEMENTS(editActions), window);
    gtk_widget_insert_action_group(toolbar, "edit", G_ACTION_GROUP(actionGroup));
    g_object_unref(actionGroup);

    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOGGLE_TOOL_BUTTON, "Bold", "format-text-bold", "edit.Bold");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOGGLE_TOOL_BUTTON, "Italic", "format-text-italic", "edit.Italic");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOGGLE_TOOL_BUTTON, "Underline", "format-text-underline", "edit.Underline");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOGGLE_TOOL_BUTTON, "Strikethrough", "format-text-strikethrough", "edit.Strikethrough");

    GtkToolItem *item = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, WEBKIT_EDITING_COMMAND_CUT, "edit-cut", "edit.Cut");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, WEBKIT_EDITING_COMMAND_COPY, "edit-copy", "edit.Copy");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, WEBKIT_EDITING_COMMAND_PASTE, "edit-paste", "edit.Paste");

    item = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, WEBKIT_EDITING_COMMAND_UNDO, "edit-undo", "edit.Undo");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, WEBKIT_EDITING_COMMAND_REDO, "edit-redo", "edit.Redo");

    item = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_radio_tool_button_new(NULL);
    GSList *justifyRadioGroup = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(item));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "format-justify-left");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "edit.JustifyLeft");
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Justify Left");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_radio_tool_button_new(justifyRadioGroup);
    justifyRadioGroup = gtk_radio_tool_button_get_group(GTK_RADIO_TOOL_BUTTON(item));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "format-justify-center");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "edit.JustifyCenter");
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Justify Center");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_radio_tool_button_new(justifyRadioGroup);
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "format-justify-right");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "edit.JustifyRight");
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Justify Right");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Indent", "format-indent-more", "edit.Indent");
    browserWindowSetupEditorToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Outdent", "format-indent-less", "edit.Outdent");

    item = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_tool_button_new(NULL, NULL);
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "insert-image");
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Insert Image");
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(insertImageCommandCallback), window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_tool_button_new(NULL, NULL);
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "insert-link");
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Insert Link");
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(insertLinkCommandCallback), window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    gtk_box_pack_start(GTK_BOX(window->mainBox), toolbar, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(window->mainBox), toolbar, 1);
    gtk_widget_show(toolbar);
#endif
}

static void browserWindowSwitchTab(GtkNotebook *notebook, BrowserTab *tab, guint tabIndex, BrowserWindow *window)
{
    if (window->activeTab == tab)
        return;

    if (window->activeTab) {
        browser_tab_set_status_text(window->activeTab, NULL);
        g_clear_object(&window->favicon);

        WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
        g_signal_handlers_disconnect_by_data(webView, window);

        /* We always want close to be connected even for not active tabs */
        g_signal_connect(webView, "close", G_CALLBACK(webViewClose), window);

        WebKitBackForwardList *backForwardlist = webkit_web_view_get_back_forward_list(webView);
        g_signal_handlers_disconnect_by_data(backForwardlist, window);
    }

    window->activeTab = tab;

    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    if (webkit_web_view_is_editable(webView)) {
        browserWindowSetupEditorToolbar(window);
#if !GTK_CHECK_VERSION(3, 98, 0)
        g_signal_connect(webkit_web_view_get_editor_state(webView), "notify::typing-attributes", G_CALLBACK(typingAttributesChanged), window);
#endif
    }
    webViewURIChanged(webView, NULL, window);
    webViewTitleChanged(webView, NULL, window);
    webViewIsLoadingChanged(webView, NULL, window);
    faviconChanged(webView, NULL, window);
    browserWindowUpdateZoomActions(window);
    if (webkit_web_view_is_loading(webView))
        webViewLoadProgressChanged(webView, NULL, window);

    g_signal_connect(webView, "notify::uri", G_CALLBACK(webViewURIChanged), window);
    g_signal_connect(webView, "notify::estimated-load-progress", G_CALLBACK(webViewLoadProgressChanged), window);
    g_signal_connect(webView, "notify::title", G_CALLBACK(webViewTitleChanged), window);
    g_signal_connect(webView, "notify::is-loading", G_CALLBACK(webViewIsLoadingChanged), window);
    g_signal_connect(webView, "create", G_CALLBACK(webViewCreate), window);
    g_signal_connect(webView, "close", G_CALLBACK(webViewClose), window);
    g_signal_connect(webView, "load-failed", G_CALLBACK(webViewLoadFailed), window);
    g_signal_connect(webView, "decide-policy", G_CALLBACK(webViewDecidePolicy), window);
    g_signal_connect(webView, "mouse-target-changed", G_CALLBACK(webViewMouseTargetChanged), window);
    g_signal_connect(webView, "notify::zoom-level", G_CALLBACK(webViewZoomLevelChanged), window);
    g_signal_connect(webView, "notify::favicon", G_CALLBACK(faviconChanged), window);
    g_signal_connect(webView, "enter-fullscreen", G_CALLBACK(webViewEnterFullScreen), window);
    g_signal_connect(webView, "leave-fullscreen", G_CALLBACK(webViewLeaveFullScreen), window);
#if !GTK_CHECK_VERSION(3, 98, 0)
    g_signal_connect(webView, "scroll-event", G_CALLBACK(scrollEventCallback), window);
#endif

    WebKitBackForwardList *backForwardlist = webkit_web_view_get_back_forward_list(webView);
    browserWindowUpdateNavigationMenu(window, backForwardlist);
    g_signal_connect(backForwardlist, "changed", G_CALLBACK(backForwardlistChanged), window);
}

static void browserWindowTabAddedOrRemoved(GtkNotebook *notebook, BrowserTab *tab, guint tabIndex, BrowserWindow *window)
{
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook), gtk_notebook_get_n_pages(notebook) > 1);
}

#if GTK_CHECK_VERSION(3, 98, 0)
static GtkWidget* browserWindowSetupToolbarItem(BrowserWindow* window, GtkBox* box, const char* namedIcon, GCallback callback)
{
    GtkWidget *button = gtk_button_new_from_icon_name(namedIcon);
    gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
    gtk_box_append(box, button);
    g_signal_connect_swapped(button, "clicked", callback, (gpointer)window);

    return button;
}
#else
static GtkWidget* browserWindowSetupToolbarItem(BrowserWindow *window, GtkWidget *toolbar, GType type, const char *label, const char *namedIcon, const char *actionName)
{
    GtkWidget *item = g_object_new(type, "icon-name", namedIcon, NULL);
    if (type == GTK_TYPE_MENU_TOOL_BUTTON)
        gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(item), 0);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(item), actionName);
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), label);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);
    gtk_widget_show(item);

    return item;
}
#endif

static const GActionEntry actions[] = {
    { "reload", reloadPage, NULL, NULL, NULL, { 0 } },
    { "reload-no-cache", reloadPageIgnoringCache, NULL, NULL, NULL, { 0 } },
    { "reload-stop", reloadOrStopCallback, NULL, NULL, NULL, { 0 } },
    { "toggle-inspector", toggleWebInspector, NULL, NULL, NULL, { 0 } },
    { "open-private-window", openPrivateWindow, NULL, NULL, NULL, { 0 } },
    { "focus-location", focusLocationBar, NULL, NULL, NULL, { 0 } },
    { "stop-load", stopPageLoad, NULL, NULL, NULL, { 0 } },
    { "load-homepage", loadHomePage, NULL, NULL, NULL, { 0 } },
    { "go-back", goBackCallback, NULL, NULL, NULL, { 0 } },
    { "go-forward", goForwardCallback, NULL, NULL, NULL, { 0 } },
    { "zoom-in", zoomInCallback, NULL, NULL, NULL, { 0 } },
    { "zoom-out", zoomOutCallback, NULL, NULL, NULL, { 0 } },
    { "zoom-default", defaultZoomCallback, NULL, NULL, NULL, { 0 } },
#if !GTK_CHECK_VERSION(3, 98, 0)
    { "find", searchCallback, NULL, NULL, NULL, { 0 } },
    { "preferences", settingsCallback, NULL, NULL, NULL, { 0 } },
#endif
    { "new-tab", newTabCallback, NULL, NULL, NULL, { 0 } },
    { "toggle-fullscreen", toggleFullScreen, NULL, NULL, NULL, { 0 } },
    { "print", printPage, NULL, NULL, NULL, { 0 } },
    { "close", browserWindowTryCloseCurrentWebView, NULL, NULL, NULL, { 0 } },
    { "quit", browserWindowTryClose, NULL, NULL, NULL, { 0 } },
};

static void browser_window_init(BrowserWindow *window)
{
    window->backgroundColor.red = window->backgroundColor.green = window->backgroundColor.blue = 255;
    window->backgroundColor.alpha = 1;

    gtk_window_set_title(GTK_WINDOW(window), defaultWindowTitle);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    window->uriEntry = gtk_entry_new();
    g_signal_connect_swapped(window->uriEntry, "activate", G_CALLBACK(activateUriEntryCallback), (gpointer)window);
    gtk_entry_set_icon_activatable(GTK_ENTRY(window->uriEntry), GTK_ENTRY_ICON_PRIMARY, FALSE);
    updateUriEntryIcon(window);

    g_action_map_add_action_entries(G_ACTION_MAP(window), actions, G_N_ELEMENTS(actions), window);

#if GTK_CHECK_VERSION(3, 98, 0)
    GtkWidget *toolbar = gtk_center_box_new();
    window->toolbar = toolbar;
    gtk_widget_set_hexpand(window->uriEntry, TRUE);
    gtk_center_box_set_center_widget(GTK_CENTER_BOX(toolbar), window->uriEntry);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    window->backItem = browserWindowSetupToolbarItem(window, GTK_BOX(box), "go-previous", G_CALLBACK(goBackCallback));
    window->forwardItem = browserWindowSetupToolbarItem(window, GTK_BOX(box), "go-next", G_CALLBACK(goForwardCallback));
    window->reloadOrStopButton = browserWindowSetupToolbarItem(window, GTK_BOX(box), "view-refresh", G_CALLBACK(reloadOrStopCallback));
    gtk_center_box_set_start_widget(GTK_CENTER_BOX(toolbar), box);
#else
    GtkWidget *toolbar = gtk_toolbar_new();
    window->toolbar = toolbar;
    gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    window->backItem = browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_MENU_TOOL_BUTTON, "Back", "go-previous", "win.go-back");
    window->forwardItem = browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_MENU_TOOL_BUTTON, "Forward", "go-next", "win.go-forward");
    browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Preferences", "preferences-system", "win.preferences");

    browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Zoom Out", "zoom-out", "win.zoom-out");
    browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Zoom In", "zoom-in", "win.zoom-in");

    browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Find", "edit-find", "win.find");

    browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Home", "go-home", "win.load-homepage");

    GtkToolItem *item = gtk_tool_button_new(gtk_image_new_from_icon_name("tab-new", GTK_ICON_SIZE_SMALL_TOOLBAR), NULL);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "win.new-tab");
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "New Tab");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show_all(GTK_WIDGET(item));

    item = gtk_tool_item_new();
    gtk_tool_item_set_expand(item, TRUE);
    gtk_container_add(GTK_CONTAINER(item), window->uriEntry);
    gtk_widget_show(window->uriEntry);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    window->reloadOrStopButton = browserWindowSetupToolbarItem(window, toolbar, GTK_TYPE_TOOL_BUTTON, "Reload", "view-refresh", "win-reload-stop");
#endif
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    window->mainBox = vbox;
#if GTK_CHECK_VERSION(3, 98, 4)
    gtk_box_append(GTK_BOX(vbox), toolbar);
#else
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_widget_show(toolbar);
#endif

    window->notebook = gtk_notebook_new();
    g_signal_connect(window->notebook, "switch-page", G_CALLBACK(browserWindowSwitchTab), window);
    g_signal_connect(window->notebook, "page-added", G_CALLBACK(browserWindowTabAddedOrRemoved), window);
    g_signal_connect(window->notebook, "page-removed", G_CALLBACK(browserWindowTabAddedOrRemoved), window);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(window->notebook), FALSE);
#if !GTK_CHECK_VERSION(3, 98, 4)
    gtk_box_pack_start(GTK_BOX(window->mainBox), window->notebook, TRUE, TRUE, 0);
    gtk_widget_show(window->notebook);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);
#else
    gtk_box_append(GTK_BOX(window->mainBox), window->notebook);
    gtk_window_set_child(GTK_WINDOW(window), vbox);
#endif
}

static void browserWindowConstructed(GObject *gObject)
{
    G_OBJECT_CLASS(browser_window_parent_class)->constructed(gObject);
}

static void browserWindowSaveSession(BrowserWindow *window)
{
    if (!window->sessionFile)
        return;

    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    WebKitWebViewSessionState *state = webkit_web_view_get_session_state(webView);
    GBytes *bytes = webkit_web_view_session_state_serialize(state);
    webkit_web_view_session_state_unref(state);
    g_file_set_contents(window->sessionFile, g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes), NULL);
    g_bytes_unref(bytes);
}

#if !GTK_CHECK_VERSION(3, 98, 0)
static gboolean browserWindowDeleteEvent(GtkWidget *widget, GdkEventAny* event)
{
    BrowserWindow *window = BROWSER_WINDOW(widget);
    browserWindowSaveSession(window);
    browserWindowTryClose(NULL, NULL, window);
    return TRUE;
}
#endif

static void browser_window_class_init(BrowserWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->constructed = browserWindowConstructed;
    gobjectClass->dispose = browserWindowDispose;
    gobjectClass->finalize = browserWindowFinalize;

#if !GTK_CHECK_VERSION(3, 98, 0)
    GtkWidgetClass *widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->delete_event = browserWindowDeleteEvent;
#endif
}

/* Public API. */
GtkWidget *browser_window_new(GtkWindow *parent, WebKitWebContext *webContext)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(webContext), NULL);

    BrowserWindow *window = BROWSER_WINDOW(g_object_new(BROWSER_TYPE_WINDOW,
#if !GTK_CHECK_VERSION(3, 98, 0)
        "type", GTK_WINDOW_TOPLEVEL,
#endif
        NULL));

    window->webContext = g_object_ref(webContext);
    g_signal_connect(window->webContext, "download-started", G_CALLBACK(downloadStarted), window);
    if (parent) {
        window->parentWindow = parent;
        g_object_add_weak_pointer(G_OBJECT(parent), (gpointer *)&window->parentWindow);
    }

    return GTK_WIDGET(window);
}

WebKitWebContext *browser_window_get_web_context(BrowserWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_WINDOW(window), NULL);

    return window->webContext;
}

void browser_window_append_view(BrowserWindow *window, WebKitWebView *webView)
{
    g_return_if_fail(BROWSER_IS_WINDOW(window));
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (window->activeTab && webkit_web_view_is_editable(browser_tab_get_web_view(window->activeTab))) {
        g_warning("Only one tab is allowed in editable mode");
        return;
    }

    GtkWidget *tab = browser_tab_new(webView);
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (gtk_widget_get_app_paintable(GTK_WIDGET(window)))
        browser_tab_set_background_color(BROWSER_TAB(tab), &window->backgroundColor);
#endif
    gtk_notebook_append_page(GTK_NOTEBOOK(window->notebook), tab, browser_tab_get_title_widget(BROWSER_TAB(tab)));
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_container_child_set(GTK_CONTAINER(window->notebook), tab, "tab-expand", TRUE, NULL);
#endif
    gtk_widget_show(tab);
}

void browser_window_load_uri(BrowserWindow *window, const char *uri)
{
    g_return_if_fail(BROWSER_IS_WINDOW(window));
    g_return_if_fail(uri);

    browser_tab_load_uri(window->activeTab, uri);
}

void browser_window_load_session(BrowserWindow *window, const char *sessionFile)
{
    g_return_if_fail(BROWSER_IS_WINDOW(window));
    g_return_if_fail(sessionFile);

    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    window->sessionFile = g_strdup(sessionFile);
    gchar *data = NULL;
    gsize dataLength;
    if (g_file_get_contents(sessionFile, &data, &dataLength, NULL)) {
        GBytes *bytes = g_bytes_new_take(data, dataLength);
        WebKitWebViewSessionState *state = webkit_web_view_session_state_new(bytes);
        g_bytes_unref(bytes);

        if (state) {
            webkit_web_view_restore_session_state(webView, state);
            webkit_web_view_session_state_unref(state);
        }
    }

    WebKitBackForwardList *bfList = webkit_web_view_get_back_forward_list(webView);
    WebKitBackForwardListItem *item = webkit_back_forward_list_get_current_item(bfList);
    if (item)
        webkit_web_view_go_to_back_forward_list_item(webView, item);
    else
        webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);

}

void browser_window_set_background_color(BrowserWindow *window, GdkRGBA *rgba)
{
    g_return_if_fail(BROWSER_IS_WINDOW(window));
    g_return_if_fail(rgba);

    g_assert(!window->activeTab);

    if (gdk_rgba_equal(rgba, &window->backgroundColor))
        return;

    window->backgroundColor = *rgba;

#if !GTK_CHECK_VERSION(3, 98, 0)
    GdkVisual *rgbaVisual = gdk_screen_get_rgba_visual(gtk_window_get_screen(GTK_WINDOW(window)));
    if (!rgbaVisual)
        return;

    gtk_widget_set_visual(GTK_WIDGET(window), rgbaVisual);
    gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
#endif
}

WebKitWebView *browser_window_get_or_create_web_view_for_automation(BrowserWindow *window)
{
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook)) == 1 && !webkit_web_view_get_uri(webView)) {
        webkit_web_view_load_uri(webView, "about:blank");
        return webView;
    }

    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "is-controlled-by-automation", TRUE,
        NULL));
    GtkWidget *newWindow = browser_window_new(GTK_WINDOW(window), window->webContext);
    gtk_window_set_application(GTK_WINDOW(newWindow), gtk_window_get_application(GTK_WINDOW(window)));
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_window_set_focus_on_map(GTK_WINDOW(newWindow), FALSE);
#endif
    browser_window_append_view(BROWSER_WINDOW(newWindow), newWebView);
    webkit_web_view_load_uri(newWebView, "about:blank");
    gtk_widget_show(newWindow);
    return newWebView;
}

WebKitWebView *browser_window_create_web_view_in_new_tab_for_automation(BrowserWindow *window)
{
    WebKitWebView *webView = browser_tab_get_web_view(window->activeTab);
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "is-controlled-by-automation", TRUE,
        "automation-presentation-type", WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB,
        NULL));
    browser_window_append_view(window, newWebView);
    webkit_web_view_load_uri(newWebView, "about:blank");
    return newWebView;
}
