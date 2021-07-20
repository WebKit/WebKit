/*
 * Copyright (C) 2017, 2020 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include "config.h"
#include "WebKitWebView.h"

#include "WebKitAuthenticationDialog.h"
#include "WebKitScriptDialogImpl.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <WebCore/Color.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PlatformScreen.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

gboolean webkitWebViewAuthenticate(WebKitWebView* webView, WebKitAuthenticationRequest* request)
{
    switch (webkit_authentication_request_get_scheme(request)) {
    case WEBKIT_AUTHENTICATION_SCHEME_DEFAULT:
    case WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC:
    case WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST:
    case WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM:
    case WEBKIT_AUTHENTICATION_SCHEME_NTLM:
    case WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE:
    case WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED:
    case WEBKIT_AUTHENTICATION_SCHEME_UNKNOWN: {
        CredentialStorageMode credentialStorageMode = webkit_authentication_request_can_save_credentials(request) ? AllowPersistentStorage : DisallowPersistentStorage;
        webkitWebViewBaseAddDialog(WEBKIT_WEB_VIEW_BASE(webView), webkitAuthenticationDialogNew(request, credentialStorageMode));
        break;
    }
    case WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED:
    case WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_PIN_REQUESTED:
        webkit_authentication_request_authenticate(request, nullptr);
        break;
    }

    return TRUE;
}

gboolean webkitWebViewScriptDialog(WebKitWebView* webView, WebKitScriptDialog* scriptDialog)
{
    GUniquePtr<char> title(g_strdup_printf("JavaScript - %s", webkitWebViewGetPage(webView).pageLoadState().url().utf8().data()));
    // Limit script dialog size to 80% of the web view size.
    GtkRequisition maxSize = { static_cast<int>(gtk_widget_get_allocated_width(GTK_WIDGET(webView)) * 0.80), static_cast<int>(gtk_widget_get_allocated_height(GTK_WIDGET(webView)) * 0.80) };
    webkitWebViewBaseAddDialog(WEBKIT_WEB_VIEW_BASE(webView), webkitScriptDialogImplNew(scriptDialog, title.get(), &maxSize));

    return TRUE;
}

static void fileChooserDialogResponseCallback(GtkFileChooser* dialog, gint responseID, WebKitFileChooserRequest* request)
{
    GRefPtr<WebKitFileChooserRequest> adoptedRequest = adoptGRef(request);
    if (responseID == GTK_RESPONSE_ACCEPT) {
        GRefPtr<GPtrArray> filesArray = adoptGRef(g_ptr_array_new_with_free_func(g_free));
#if USE(GTK4)
        GRefPtr<GListModel> filesList = adoptGRef(gtk_file_chooser_get_files(dialog));
        unsigned itemCount = g_list_model_get_n_items(filesList.get());
        for (unsigned i = 0; i < itemCount; ++i) {
            GRefPtr<GFile> file = adoptGRef(G_FILE(g_list_model_get_item(filesList.get(), i)));
            if (gchar* filename = g_file_get_path(file.get()))
                g_ptr_array_add(filesArray.get(), filename);
        }
#else
        GSList* filesList = gtk_file_chooser_get_files(dialog);
        for (GSList* file = filesList; file; file = g_slist_next(file)) {
            if (gchar* filename = g_file_get_path(G_FILE(file->data)))
                g_ptr_array_add(filesArray.get(), filename);
        }
        g_slist_free_full(filesList, g_object_unref);
#endif
        g_ptr_array_add(filesArray.get(), nullptr);

        webkit_file_chooser_request_select_files(adoptedRequest.get(), reinterpret_cast<const gchar* const*>(filesArray->pdata));
    } else
        webkit_file_chooser_request_cancel(adoptedRequest.get());

    g_object_unref(dialog);
}

gboolean webkitWebViewRunFileChooser(WebKitWebView* webView, WebKitFileChooserRequest* request)
{
    GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(webView));
    if (!WebCore::widgetIsOnscreenToplevelWindow(toplevel))
        toplevel = 0;

    gboolean allowsMultipleSelection = webkit_file_chooser_request_get_select_multiple(request);

    GtkFileChooserNative* dialog = gtk_file_chooser_native_new(allowsMultipleSelection ? _("Select Files") : _("Select File"),
        toplevel ? GTK_WINDOW(toplevel) : nullptr, GTK_FILE_CHOOSER_ACTION_OPEN, nullptr, nullptr);
    if (toplevel)
        gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(dialog), TRUE);

    if (GtkFileFilter* filter = webkit_file_chooser_request_get_mime_types_filter(request))
        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), allowsMultipleSelection);

    if (const gchar* const* selectedFiles = webkit_file_chooser_request_get_selected_files(request)) {
        GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(selectedFiles[0]));
        gtk_file_chooser_set_file(GTK_FILE_CHOOSER(dialog), file.get(), nullptr);
    }

    g_signal_connect(dialog, "response", G_CALLBACK(fileChooserDialogResponseCallback), g_object_ref(request));

    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));

    return TRUE;
}

struct WindowStateEvent {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    enum class Type { Maximize, Minimize, Restore };

    WindowStateEvent(Type type, CompletionHandler<void()>&& completionHandler)
        : type(type)
        , completionHandler(WTFMove(completionHandler))
        , completeTimer(RunLoop::main(), this, &WindowStateEvent::complete)
    {
        // Complete the event if not done after one second.
        completeTimer.startOneShot(1_s);
    }

    ~WindowStateEvent()
    {
        complete();
    }

    void complete()
    {
        if (auto handler = std::exchange(completionHandler, nullptr))
            handler();
    }

    Type type;
    CompletionHandler<void()> completionHandler;
    RunLoop::Timer<WindowStateEvent> completeTimer;
};

static const char* gWindowStateEventID = "wk-window-state-event";

#if USE(GTK4)
static void surfaceStateChangedCallback(GdkSurface* surface, GParamSpec*, WebKitWebView* view)
{
    auto* state = static_cast<WindowStateEvent*>(g_object_get_data(G_OBJECT(view), gWindowStateEventID));
    if (!state) {
        g_signal_handlers_disconnect_by_func(surface, reinterpret_cast<gpointer>(surfaceStateChangedCallback), view);
        return;
    }

    auto surfaceState = gdk_toplevel_get_state(GDK_TOPLEVEL(surface));
    bool eventCompleted = false;
    switch (state->type) {
    case WindowStateEvent::Type::Maximize:
        if (surfaceState & GDK_TOPLEVEL_STATE_MAXIMIZED)
            eventCompleted = true;
        break;
    case WindowStateEvent::Type::Minimize:
        if ((surfaceState & GDK_TOPLEVEL_STATE_MINIMIZED) || !gdk_surface_get_mapped(surface))
            eventCompleted = true;
        break;
    case WindowStateEvent::Type::Restore:
        if (!(surfaceState & GDK_TOPLEVEL_STATE_MAXIMIZED) && !(surfaceState & GDK_TOPLEVEL_STATE_MINIMIZED))
            eventCompleted = true;
        break;
    }

    if (eventCompleted) {
        g_signal_handlers_disconnect_by_func(surface, reinterpret_cast<gpointer>(surfaceStateChangedCallback), view);
        g_object_set_data(G_OBJECT(view), gWindowStateEventID, nullptr);
    }
}
#else
static gboolean windowStateEventCallback(GtkWidget* window, GdkEventWindowState* event, WebKitWebView* view)
{
    auto* state = static_cast<WindowStateEvent*>(g_object_get_data(G_OBJECT(view), gWindowStateEventID));
    if (!state) {
        g_signal_handlers_disconnect_by_func(window, reinterpret_cast<gpointer>(windowStateEventCallback), view);
        return FALSE;
    }

    bool eventCompleted = false;
    switch (state->type) {
    case WindowStateEvent::Type::Maximize:
        if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
            eventCompleted = true;
        break;
    case WindowStateEvent::Type::Minimize:
        if ((event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) || !gtk_widget_get_mapped(window))
            eventCompleted = true;
        break;
    case WindowStateEvent::Type::Restore:
        if (!(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) && !(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED))
            eventCompleted = true;
        break;
    }

    if (eventCompleted) {
        g_signal_handlers_disconnect_by_func(window, reinterpret_cast<gpointer>(windowStateEventCallback), view);
        g_object_set_data(G_OBJECT(view), gWindowStateEventID, nullptr);
    }

    return FALSE;
}
#endif

static void
webkitWebViewMonitorWindowState(WebKitWebView* view, GtkWindow* window, WindowStateEvent::Type type, CompletionHandler<void()>&& completionHandler)
{
    g_object_set_data_full(G_OBJECT(view), gWindowStateEventID, new WindowStateEvent(type, WTFMove(completionHandler)), [](gpointer userData) {
        delete static_cast<WindowStateEvent*>(userData);
    });

#if USE(GTK4)
    g_signal_connect_object(gtk_native_get_surface(GTK_NATIVE(window)), "notify::state", G_CALLBACK(surfaceStateChangedCallback), view, G_CONNECT_AFTER);
#else
    g_signal_connect_object(window, "window-state-event", G_CALLBACK(windowStateEventCallback), view, G_CONNECT_AFTER);
#endif
}

void webkitWebViewMaximizeWindow(WebKitWebView* view, CompletionHandler<void()>&& completionHandler)
{
    auto* topLevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
    if (!gtk_widget_is_toplevel(topLevel)) {
        completionHandler();
        return;
    }

    auto* window = GTK_WINDOW(topLevel);
    if (gtk_window_is_maximized(window)) {
        completionHandler();
        return;
    }

    webkitWebViewMonitorWindowState(view, window, WindowStateEvent::Type::Maximize, WTFMove(completionHandler));
    gtk_window_maximize(window);

#if ENABLE(DEVELOPER_MODE)
    // Xvfb doesn't support maximize, so we resize the window to the screen size.
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11) {
        const char* underXvfb = g_getenv("UNDER_XVFB");
        if (!g_strcmp0(underXvfb, "yes")) {
            auto screenRect = WebCore::screenAvailableRect(nullptr);
            gtk_window_move(window, screenRect.x(), screenRect.y());
            gtk_window_resize(window, screenRect.width(), screenRect.height());
        }
    }
#endif
    gtk_widget_show(topLevel);
}

void webkitWebViewMinimizeWindow(WebKitWebView* view, CompletionHandler<void()>&& completionHandler)
{
    auto* topLevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
    if (!gtk_widget_is_toplevel(topLevel)) {
        completionHandler();
        return;
    }

    auto* window = GTK_WINDOW(topLevel);
    webkitWebViewMonitorWindowState(view, window, WindowStateEvent::Type::Minimize, WTFMove(completionHandler));
    gtk_window_minimize(window);
    gtk_widget_hide(topLevel);
}

void webkitWebViewRestoreWindow(WebKitWebView* view, CompletionHandler<void()>&& completionHandler)
{
    auto* topLevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
    if (!gtk_widget_is_toplevel(topLevel)) {
        completionHandler();
        return;
    }

    auto* window = GTK_WINDOW(topLevel);
    if (gtk_widget_get_mapped(topLevel) && !gtk_window_is_maximized(window)) {
        completionHandler();
        return;
    }

    webkitWebViewMonitorWindowState(view, window, WindowStateEvent::Type::Restore, WTFMove(completionHandler));
    if (gtk_window_is_maximized(window))
        gtk_window_unmaximize(window);
    if (!gtk_widget_get_mapped(topLevel))
        gtk_window_unminimize(window);

#if ENABLE(DEVELOPER_MODE)
    // Xvfb doesn't support maximize, so we resize the window to the default size.
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11) {
        const char* underXvfb = g_getenv("UNDER_XVFB");
        if (!g_strcmp0(underXvfb, "yes")) {
            int x, y;
            gtk_window_get_default_size(window, &x, &y);
            gtk_window_resize(window, x, y);
        }
    }
#endif
    gtk_widget_show(topLevel);
}

/**
 * webkit_web_view_new:
 *
 * Creates a new #WebKitWebView with the default #WebKitWebContext and
 * no #WebKitUserContentManager associated with it.
 * See also webkit_web_view_new_with_context(),
 * webkit_web_view_new_with_user_content_manager(), and
 * webkit_web_view_new_with_settings().
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new()
{
    return webkit_web_view_new_with_context(webkit_web_context_get_default());
}

/**
 * webkit_web_view_new_with_context:
 * @context: the #WebKitWebContext to be used by the #WebKitWebView
 *
 * Creates a new #WebKitWebView with the given #WebKitWebContext and
 * no #WebKitUserContentManager associated with it.
 * See also webkit_web_view_new_with_user_content_manager() and
 * webkit_web_view_new_with_settings().
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new_with_context(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "is-ephemeral", webkit_web_context_is_ephemeral(context),
        "web-context", context,
        nullptr));
}

/**
 * webkit_web_view_new_with_related_view: (constructor)
 * @web_view: the related #WebKitWebView
 *
 * Creates a new #WebKitWebView sharing the same web process with @web_view.
 * This method doesn't have any effect when %WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS
 * process model is used, because a single web process is shared for all the web views in the
 * same #WebKitWebContext. When using %WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES process model,
 * this method should always be used when creating the #WebKitWebView in the #WebKitWebView::create signal.
 * You can also use this method to implement other process models based on %WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES,
 * like for example, sharing the same web process for all the views in the same security domain.
 *
 * The newly created #WebKitWebView will also have the same #WebKitUserContentManager,
 * #WebKitSettings, and #WebKitWebsitePolicies as @web_view.
 *
 * Returns: (transfer full): The newly created #WebKitWebView widget
 *
 * Since: 2.4
 */
GtkWidget* webkit_web_view_new_with_related_view(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "settings", webkit_web_view_get_settings(webView),
        "related-view", webView,
        "website-policies", webkit_web_view_get_website_policies(webView),
        nullptr));
}

/**
 * webkit_web_view_new_with_settings:
 * @settings: a #WebKitSettings
 *
 * Creates a new #WebKitWebView with the given #WebKitSettings.
 * See also webkit_web_view_new_with_context(), and
 * webkit_web_view_new_with_user_content_manager().
 *
 * Returns: The newly created #WebKitWebView widget
 *
 * Since: 2.6
 */
GtkWidget* webkit_web_view_new_with_settings(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), nullptr);
    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "settings", settings, nullptr));
}

/**
 * webkit_web_view_new_with_user_content_manager:
 * @user_content_manager: a #WebKitUserContentManager.
 *
 * Creates a new #WebKitWebView with the given #WebKitUserContentManager.
 * The content loaded in the view may be affected by the content injected
 * in the view by the user content manager.
 *
 * Returns: The newly created #WebKitWebView widget
 *
 * Since: 2.6
 */
GtkWidget* webkit_web_view_new_with_user_content_manager(WebKitUserContentManager* userContentManager)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(userContentManager), nullptr);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "user-content-manager", userContentManager, nullptr));
}

/**
 * webkit_web_view_set_background_color:
 * @web_view: a #WebKitWebView
 * @rgba: a #GdkRGBA
 *
 * Sets the color that will be used to draw the @web_view background before
 * the actual contents are rendered. Note that if the web page loaded in @web_view
 * specifies a background color, it will take precedence over the @rgba color.
 * By default the @web_view background color is opaque white.
 * Note that the parent window must have a RGBA visual and
 * #GtkWidget:app-paintable property set to %TRUE for backgrounds colors to work.
 *
 * <informalexample><programlisting>
 * static void browser_window_set_background_color (BrowserWindow *window,
 *                                                  const GdkRGBA *rgba)
 * {
 *     WebKitWebView *web_view;
 *     GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
 *     GdkVisual *rgba_visual = gdk_screen_get_rgba_visual (screen);
 *
 *     if (!rgba_visual)
 *          return;
 *
 *     gtk_widget_set_visual (GTK_WIDGET (window), rgba_visual);
 *     gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
 *
 *     web_view = browser_window_get_web_view (window);
 *     webkit_web_view_set_background_color (web_view, rgba);
 * }
 * </programlisting></informalexample>
 *
 * Since: 2.8
 */
void webkit_web_view_set_background_color(WebKitWebView* webView, const GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(rgba);

    auto& page = *webkitWebViewBaseGetPage(reinterpret_cast<WebKitWebViewBase*>(webView));
    page.setBackgroundColor(WebCore::Color(*rgba));
}

/**
 * webkit_web_view_get_background_color:
 * @web_view: a #WebKitWebView
 * @rgba: (out): a #GdkRGBA to fill in with the background color
 *
 * Gets the color that is used to draw the @web_view background before
 * the actual contents are rendered.
 * For more information see also webkit_web_view_set_background_color()
 *
 * Since: 2.8
 */
void webkit_web_view_get_background_color(WebKitWebView* webView, GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(rgba);

    auto& page = *webkitWebViewBaseGetPage(reinterpret_cast<WebKitWebViewBase*>(webView));
    *rgba = page.backgroundColor().value_or(WebCore::Color::white);
}

guint createShowOptionMenuSignal(WebKitWebViewClass* webViewClass)
{
    /**
     * WebKitWebView::show-option-menu:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @menu: the #WebKitOptionMenu
     * @event: the #GdkEvent that triggered the menu, or %NULL
     * @rectangle: the option element area
     *
     * This signal is emitted when a select element in @web_view needs to display a
     * dropdown menu. This signal can be used to show a custom menu, using @menu to get
     * the details of all items that should be displayed. The area of the element in the
     * #WebKitWebView is given as @rectangle parameter, it can be used to position the
     * menu. If this was triggered by a user interaction, like a mouse click,
     * @event parameter provides the #GdkEvent.
     * To handle this signal asynchronously you should keep a ref of the @menu.
     *
     * The default signal handler will pop up a #GtkMenu.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.18
     */
    return g_signal_new(
        "show-option-menu",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, show_option_menu),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 3,
        WEBKIT_TYPE_OPTION_MENU,
        GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
        GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);
}
