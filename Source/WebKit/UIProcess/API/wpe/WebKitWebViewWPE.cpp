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
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/PlatformRenderTheme.h>

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

#if USE(LIBWPE)
/**
 * webkit_web_view_new:
 * @backend: (transfer full) (nullable): wrapped WPE view backend which
 *    will determine the behavior of the new [class@WebView], or %NULL to use the WPE platform API.
 *
 * Creates a new web view with a default configuration.
 *
 * The new view will use the default [class@WebContext] and will not
 * have an associated [class@UserContentManager].
 *
 * Set the [property@WebView:web-context],
 * [property@WebView:user-content-manager] or [property@WebView:settings]
 * properties at construction to use a different configuration.
 *
 * Returns: The newly created web view.
 *
 * Deprecated: 2.56: Use g_object_new() instead.
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

#endif // USE(LIBWPE)

/**
 * webkit_web_view_set_background_color:
 * @web_view: a #WebKitWebView
 * @color: (nullable): a #WebKitColor, or %NULL to reset to the default system appearance color
 *
 * Sets the color that will be used to draw the @web_view background before
 * the actual contents are rendered. Note that if the web page loaded in @web_view
 * specifies a background color, it will take precedence over the @color.
 * By default the @web_view background color is based on the system appearance.
 *
 * Since 2.56, %NULL can be passed to reset the background color to the default system appearance color.
 *
 * Since: 2.24
 */
void webkit_web_view_set_background_color(WebKitWebView* webView, WebKitColor* backgroundColor)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    auto& page = webkitWebViewGetPage(webView);
    if (!backgroundColor) {
        page.setBackgroundColor(std::nullopt);
#if ENABLE(WPE_PLATFORM)
        if (auto* view = static_cast<WebKit::PageClientImpl&>(*page.pageClient()).wpeView())
            wpe_view_set_opaque_rectangles(view, nullptr, 0);
#endif
        return;
    }

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
 * actual contents are rendered. By default the @web_view background color
 * is based on the system appearance. For more information see also
 * webkit_web_view_set_background_color().
 *
 * Since: 2.24
 */
void webkit_web_view_get_background_color(WebKitWebView* webView, WebKitColor* color)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(color);

    auto& page = webkitWebViewGetPage(webView);
    if (auto webCoreColor = page.backgroundColor()) {
        webkitColorFillFromWebCoreColor(*webCoreColor, color);
        return;
    }

    OptionSet<WebCore::StyleColorOptions> styleColorOptions;
    if (page.useDarkAppearance())
        styleColorOptions.add(WebCore::StyleColorOptions::UseDarkAppearance);
    webkitColorFillFromWebCoreColor(WebCore::RenderTheme::singleton().systemColor(WebCore::CSSValueID::Canvas, styleColorOptions), color);
}

guint createContextMenuSignal(WebKitWebViewClass* webViewClass)
{
    /**
     * WebKitWebView::context-menu:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @context_menu: the proposed #WebKitContextMenu
     * @hit_test_result: a #WebKitHitTestResult
     *
     * Emitted when a context menu is about to be displayed to give the application
     * a chance to customize the proposed menu, prevent the menu from being displayed,
     * or build its own context menu.
     *
     * - To customize the proposed menu you can use webkit_context_menu_prepend(),
     *  webkit_context_menu_append() or webkit_context_menu_insert() to add new
     *  #WebKitContextMenuItem objects to @context_menu, webkit_context_menu_move_item()
     *  to reorder existing items, or webkit_context_menu_remove() to remove an
     *  existing item. The signal handler should return %FALSE, and the menu represented
     *  by @context_menu will be shown.
     * - To prevent the menu from being displayed you can just connect to this signal
     *  and return %TRUE so that the proposed menu will not be shown.
     * - To build your own menu, you can remove all items from the proposed menu with
     *  webkit_context_menu_remove_all(), add your own items and return %FALSE so
     *  that the menu will be shown. You can also ignore the proposed #WebKitContextMenu,
     *  build your own menu and return %TRUE to prevent the proposed menu from being shown.
     * - If you just want the default menu to be shown always, simply don't connect to this
     *  signal because showing the proposed context menu is the default behavior.
     *
     * If the signal handler returns %FALSE the context menu represented by @context_menu
     * will be shown, if it returns %TRUE the context menu will not be shown.
     *
     * The proposed #WebKitContextMenu passed in @context_menu argument is only valid
     * during the signal emission.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    return g_signal_new(
        "context-menu",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, context_menu),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN,
        2,
        WEBKIT_TYPE_CONTEXT_MENU,
        WEBKIT_TYPE_HIT_TEST_RESULT);
}

guint createRunColorChooserSignal(WebKitWebViewClass* webViewClass)
{
    /**
     * WebKitWebView::run-color-chooser:
     * @web_view: the [class@WebView] on which the signal is emitted
     * @request: a [class@ColorChooserRequest]
     *
     * This signal is emitted when the user interacts with a <input
     * type='color' /> HTML element, requesting from WebKit to show
     * a dialog to select a color. To let the application know the details of
     * the color chooser, as well as to allow the client application to either
     * cancel the request or perform an actual color selection, the signal will
     * pass an instance of the [class@ColorChooserRequest] in the @request
     * argument.
     *
     * It is possible to handle this request asynchronously by increasing the
     * reference count of the request.
     *
     * WPE does not provide a default color chooser, so the request is finished
     * keeping the initial color when the signal is not handled.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.56
     */
    return g_signal_new(
        "run-color-chooser",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, run_color_chooser),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_COLOR_CHOOSER_REQUEST);
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
 * Show or hide the web inspector of @web_view.
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
