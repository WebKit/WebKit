/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "PageClientImpl.h"
#include "WPEUtilities.h"
#include "WebInspectorUIProxy.h"
#include "WebKitColorPrivate.h"
#include "WebKitScriptDialogPrivate.h"
#include "WebKitWebViewPrivate.h"

#if ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

gboolean webkitWebViewAuthenticate(WebKitWebView*, WebKitAuthenticationRequest*)
{
    return FALSE;
}

gboolean webkitWebViewScriptDialog(WebKitWebView* webview, WebKitScriptDialog* dialog)
{
    if (webkit_web_view_is_controlled_by_automation(webview)) {
        webkit_script_dialog_ref(dialog);
        dialog->isUserHandled = false;
    }

    return FALSE;
}

gboolean webkitWebViewRunFileChooser(WebKitWebView*, WebKitFileChooserRequest*)
{
    return FALSE;
}

void webkitWebViewMaximizeWindow(WebKitWebView*, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void webkitWebViewMinimizeWindow(WebKitWebView*, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void webkitWebViewRestoreWindow(WebKitWebView*, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

/**
 * webkit_web_view_new:
 * @backend: (transfer full) (nullable): wrapped WPE view backend which
 *    will determine the behaviour of the new [class@WebView], or %NULL to use the WPE platform API.
 *
 * Creates a new web view with a default configuration.
 *
 * The new view will use the default [class@WebContext] and will not
 * have an associated [class@UserContentManager].
 *
 * See also [id@webkit_web_view_new_with_context],
 * [id@webkit_web_view_new_with_user_content_manager]), and
 * [id@webkit_web_view_new_with_settings].
 *
 * Returns: The newly created web view.
 */
WebKitWebView* webkit_web_view_new(WebKitWebViewBackend* backend)
{
#if ENABLE(WPE_PLATFORM)
    g_return_val_if_fail(!backend || !WKWPE::isUsingWPEPlatformAPI(), nullptr);
#else
    g_return_val_if_fail(backend, nullptr);
#endif

    return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "backend", backend,
        "web-context", webkit_web_context_get_default(),
        nullptr));
}

#if !ENABLE(2022_GLIB_API)
/**
 * webkit_web_view_new_with_context:
 * @backend: (transfer full) (not nullable): wrapped WPE view backend which
 *    will determine the behaviour of the new [class@WebView].
 * @context: the web context the new [class@WebView] will use.
 *
 * Creates a new web view with a given context.
 *
 * The new web view will use the given [class@WebContext] and will not have
 * an associated [class@UserContentManager].
 *
 * See also [id@webkit_web_view_new_with_user_content_manager] and
 * [id@webkit_web_view_new_with_settings].
 *
 * Returns: The newly created web view.
 */
WebKitWebView* webkit_web_view_new_with_context(WebKitWebViewBackend* backend, WebKitWebContext* context)
{
    g_return_val_if_fail(backend, nullptr);
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), nullptr);

    return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "backend", backend,
#if !ENABLE(2022_GLIB_API)
        "is-ephemeral", webkit_web_context_is_ephemeral(context),
#endif
        "web-context", context,
        nullptr));
}
#endif

#if !ENABLE(2022_GLIB_API)
/**
 * webkit_web_view_new_with_related_view: (constructor)
 * @backend: (transfer full) (not nullable): wrapped WPE view backend which
 *    will determine the behaviour of the new [class@WebView].
 * @web_view: the related web view.
 *
 * Creates a new web view sharing the same configuration and web process as another.
 *
 * A related view should always be set when creating a [class@WebView] in a handler
 * for the [signal@WebView::create] signal.
 *
 * The new view will also have the same [class@UserContentManager] and
 * [class@Settings] as the related @web_view.
 *
 * Returns: (transfer full): The newly created web view.
 *
 * Since: 2.4
 */
WebKitWebView* webkit_web_view_new_with_related_view(WebKitWebViewBackend* backend, WebKitWebView* webView)
{
    g_return_val_if_fail(backend, nullptr);
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "backend", backend,
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "settings", webkit_web_view_get_settings(webView),
        "related-view", webView,
        nullptr));
}

/**
 * webkit_web_view_new_with_settings:
 * @backend: (transfer full) (not nullable): wrapped WPE view backend which
 *    will determine the behaviour of the new [class@WebView].
 * @settings: settings for the new view.
 *
 * Creates a new web view with the given settings.
 *
 * See also [id@webkit_web_view_new_with_context], and
 * [id@webkit_web_view_new_with_user_content_manager].
 *
 * Returns: (transfer full): The newly created web view.
 *
 * Since: 2.6
 */
WebKitWebView* webkit_web_view_new_with_settings(WebKitWebViewBackend* backend, WebKitSettings* settings)
{
    g_return_val_if_fail(backend, nullptr);
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), nullptr);

    return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "backend", backend,
        "settings", settings,
        nullptr));
}

/**
 * webkit_web_view_new_with_user_content_manager:
 * @backend: (transfer full) (not nullable): wrapped WPE view backend which
 *    will determine the behaviour of the new [class@WebView].
 * @user_content_manager: the user content manager for the new view.
 *
 * Creates a new web view with the given user content manager.
 *
 * The content loaded in the new [class@WebView] may be affected by the
 * configuration of the given [class@UserContentManager].
 *
 * Returns: (transfer full): The newly created web view.
 *
 * Since: 2.6
 */
WebKitWebView* webkit_web_view_new_with_user_content_manager(WebKitWebViewBackend* backend, WebKitUserContentManager* userContentManager)
{
    g_return_val_if_fail(backend, nullptr);
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(userContentManager), nullptr);

    return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "backend", backend,
        "user-content-manager", userContentManager,
        nullptr));
}
#endif

/**
 * webkit_web_view_set_background_color:
 * @web_view: a #WebKitWebView
 * @color: a #WebKitColor
 *
 * Sets the color that will be used to draw the @web_view background before
 * the actual contents are rendered. Note that if the web page loaded in @web_view
 * specifies a background color, it will take precedence over the background color.
 * By default the @web_view background color is opaque white.
 *
 * Since: 2.24
 */
void webkit_web_view_set_background_color(WebKitWebView* webView, WebKitColor* backgroundColor)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(backgroundColor);

    auto& page = webkitWebViewGetPage(webView);
    auto color = webkitColorToWebCoreColor(backgroundColor);
    page.setBackgroundColor(color);
#if ENABLE(WPE_PLATFORM)
    if (auto* view = static_cast<WebKit::PageClientImpl&>(*page.pageClient()).wpeView()) {
        if (color.isOpaque()) {
            WPERectangle rect { 0, 0, wpe_view_get_width(view), wpe_view_get_height(view) };
            wpe_view_set_opaque_rectangles(view, &rect, 1);
        } else
            wpe_view_set_opaque_rectangles(view, nullptr, 0);
    }
#endif
}

/**
 * webkit_web_view_get_background_color:
 * @web_view: a #WebKitWebView
 * @color: (out): a #WebKitColor to fill in with the background color
 *
 * Gets the color that is used to draw the @web_view background before the
 * actual contents are rendered. For more information see also
 * webkit_web_view_set_background_color().
 *
 * Since: 2.24
 */
void webkit_web_view_get_background_color(WebKitWebView* webView, WebKitColor* color)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    auto& page = webkitWebViewGetPage(webView);

    auto& webCoreColor = page.backgroundColor();
    webkitColorFillFromWebCoreColor(webCoreColor.value_or(WebCore::Color::white), color);
}

guint createShowOptionMenuSignal(WebKitWebViewClass* webViewClass)
{
    /**
     * WebKitWebView::show-option-menu:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @menu: the #WebKitOptionMenu
     * @rectangle: the option element area
     *
     * This signal is emitted when a select element in @web_view needs to display a
     * dropdown menu. This signal can be used to show a custom menu, using @menu to get
     * the details of all items that should be displayed. The area of the element in the
     * #WebKitWebView is given as @rectangle parameter, it can be used to position the
     * menu.
     * To handle this signal asynchronously you should keep a ref of the @menu.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.28
     */
    return g_signal_new(
        "show-option-menu",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, show_option_menu),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 2,
        WEBKIT_TYPE_OPTION_MENU,
        WEBKIT_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);
}

#if ENABLE(WPE_PLATFORM)
/**
 * webkit_web_view_toggle_inspector:
 * @web_view: a #WebKitWebView
 *
 * Show or hide the web inspector of @web_view
 * Note that local inspector is only supported by
 * WPEWebKit when using WPE Platform API.
 *
 * Since: 2.46
 */
void webkit_web_view_toggle_inspector(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    auto& page = webkitWebViewGetPage(webView);
    if (!page.wpeView()) {
        g_warning("Local inspector is only supported by WPEWebKit when using WPE Platform API");
        return;
    }

    auto* inspector = page.inspector();
    if (!inspector)
        return;

    if (inspector->isVisible())
        inspector->close();
    else
        inspector->show();
}
#endif

/**
 * webkit_web_view_get_theme_color:
 * @web_view: a #WebKitWebView
 * @color: (out): a #WebKitColor to fill in with the theme color
 *
 * Gets the theme color that is specified by the content in the @web_view.
 * If the @web_view doesn't have a theme color it will fill the @color
 * with transparent black content.
 *
 * Returns: Whether the currently loaded page defines a theme color.
 *
 * Since: 2.50
 */
gboolean webkit_web_view_get_theme_color(WebKitWebView* webView, WebKitColor* color)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    auto& page = webkitWebViewGetPage(webView);

    if (!page.themeColor().isValid()) {
        WebCore::Color tmpColor(WebCore::Color::transparentBlack);
        webkitColorFillFromWebCoreColor(tmpColor, color);
        return FALSE;
    }

    webkitColorFillFromWebCoreColor(page.themeColor(), color);
    return TRUE;
}
