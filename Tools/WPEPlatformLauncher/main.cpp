/*
 * WPEPlatform Launcher
 *
 * Minimal WPE WebKit browser using only the new WPEPlatform API.
 * Based on WebKit MiniBrowser/wpe, with all legacy libwpe code removed.
 *
 * Features:
 *   - URL loading (command line argument or default page)
 *   - Keyboard shortcuts (Ctrl+Q, Ctrl+R, Alt+Arrow, F11, Ctrl+Shift+I)
 *   - Pointer/click events (handled automatically by WPEPlatform)
 *   - HTML-based context menu (right-click menu rendered as DOM overlay)
 *   - Navigation policy handling (link clicks)
 *   - Fullscreen mode
 *   - Page title tracking
 *
 * Usage:
 *   ./WPEPlatformLauncher [URL]
 *   ./WPEPlatformLauncher --fullscreen https://example.com
 *   ./WPEPlatformLauncher --size=1920x1080 https://example.com
 */

#include "cmakeconfig.h"

#include <wpe/webkit.h>
#include <jsc/jsc.h>
#include <glib.h>

/* --- Global state --- */

static const char* defaultTitle = "WPEPlatform Launcher";
static const char** uriArguments;
static gboolean fullscreenMode;
static guint windowWidth = 0;
static guint windowHeight = 0;

/* Saved state for context menu action handling */
static WebKitWebView* savedWebView = nullptr;
static WebKitHitTestResult* savedHitTestResult = nullptr;

/* --- Command line option parsing --- */

static gboolean parseWindowSize(const char*, const char* value, gpointer, GError** error)
{
    if (!value)
        return FALSE;

    g_auto(GStrv) sizes = g_strsplit(value, "x", 2);
    if (!sizes || !sizes[0] || !sizes[1]) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            "Invalid size format \"%s\" - use \"WIDTHxHEIGHT\"", value);
        return FALSE;
    }

    guint64 w = 0, h = 0;
    if (!g_ascii_string_to_unsigned(sizes[0], 10, 1, G_MAXUINT, &w, nullptr)
        || !g_ascii_string_to_unsigned(sizes[1], 10, 1, G_MAXUINT, &h, nullptr)) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            "Invalid size values in \"%s\"", value);
        return FALSE;
    }

    windowWidth = static_cast<guint>(w);
    windowHeight = static_cast<guint>(h);
    return TRUE;
}

static const GOptionEntry commandLineOptions[] =
{
    { "fullscreen", 'f', 0, G_OPTION_ARG_NONE, &fullscreenMode,
      "Start in fullscreen mode", nullptr },
    { "size", 's', 0, G_OPTION_ARG_CALLBACK, reinterpret_cast<gpointer>(parseWindowSize),
      "Window size, e.g. --size=1280x720", nullptr },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &uriArguments,
      nullptr, "[URL]" },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
};

/* --- WPEPlatform event handlers --- */

/*
 * Keyboard event handler.
 *
 * Pointer/click events do NOT need a handler here.
 * WPEPlatform automatically forwards pointer events (mouse move, click,
 * scroll) from the underlying display system (Wayland/DRM) to WebKit.
 * WebKit then handles hit-testing on web content (links, buttons, etc.)
 * internally. We only intercept keyboard events for shortcuts.
 */
static gboolean onViewEvent(WPEView* view, WPEEvent* event, WebKitWebView* webView)
{
    if (wpe_event_get_event_type(event) != WPE_EVENT_KEYBOARD_KEY_DOWN)
        return FALSE;

    auto modifiers = wpe_event_get_modifiers(event);
    auto keyval = wpe_event_keyboard_get_keyval(event);

    /* Ctrl+Q: Quit */
    if ((modifiers & WPE_MODIFIER_KEYBOARD_CONTROL) && keyval == WPE_KEY_q) {
        g_application_quit(g_application_get_default());
        return TRUE;
    }

    /* Ctrl+R: Reload */
    if ((modifiers & WPE_MODIFIER_KEYBOARD_CONTROL) && keyval == WPE_KEY_r) {
        webkit_web_view_reload(webView);
        return TRUE;
    }

    /* Ctrl+Shift+I: Toggle web inspector */
    if ((modifiers & WPE_MODIFIER_KEYBOARD_CONTROL)
        && (modifiers & WPE_MODIFIER_KEYBOARD_SHIFT)
        && keyval == WPE_KEY_I) {
        webkit_web_view_toggle_inspector(webView);
        return TRUE;
    }

    /* Alt+Left: Navigate back */
    if ((modifiers & WPE_MODIFIER_KEYBOARD_ALT)
        && (keyval == WPE_KEY_Left || keyval == WPE_KEY_KP_Left)) {
        if (webkit_web_view_can_go_back(webView)) {
            webkit_web_view_go_back(webView);
            return TRUE;
        }
    }

    /* Alt+Right: Navigate forward */
    if ((modifiers & WPE_MODIFIER_KEYBOARD_ALT)
        && (keyval == WPE_KEY_Right || keyval == WPE_KEY_KP_Right)) {
        if (webkit_web_view_can_go_forward(webView)) {
            webkit_web_view_go_forward(webView);
            return TRUE;
        }
    }

    /* F11: Toggle fullscreen */
    if (keyval == WPE_KEY_F11) {
        if (auto* toplevel = wpe_view_get_toplevel(view)) {
            if (wpe_toplevel_get_state(toplevel) & WPE_TOPLEVEL_STATE_FULLSCREEN)
                wpe_toplevel_unfullscreen(toplevel);
            else
                wpe_toplevel_fullscreen(toplevel);
            return TRUE;
        }
    }

    /* Escape: Leave immersive mode (e.g. HTML5 fullscreen video) */
    if (keyval == WPE_KEY_Escape) {
        if (webkit_web_view_is_immersive_mode_enabled(webView)) {
            webkit_web_view_leave_immersive_mode(webView);
            return TRUE;
        }
    }

    return FALSE;
}

/* --- WebKit signal handlers --- */

/* Update window title when page title changes */
static void onTitleChanged(WebKitWebView* webView, GParamSpec*, WPEView* view)
{
    const char* title = webkit_web_view_get_title(webView);
    wpe_toplevel_set_title(wpe_view_get_toplevel(view),
        title ? title : defaultTitle);
}

/* Log page load progress */
static void onLoadChanged(WebKitWebView* webView, WebKitLoadEvent event, gpointer)
{
    const char* uri = webkit_web_view_get_uri(webView);
    switch (event) {
    case WEBKIT_LOAD_STARTED:
        g_print("[Load Started]   %s\n", uri);
        break;
    case WEBKIT_LOAD_REDIRECTED:
        g_print("[Redirected]     %s\n", uri);
        break;
    case WEBKIT_LOAD_COMMITTED:
        g_print("[Committed]      %s\n", uri);
        break;
    case WEBKIT_LOAD_FINISHED:
        g_print("[Load Finished]  %s\n", uri);
        break;
    }
}

/*
 * Navigation policy decision handler.
 *
 * Called when a navigation is about to happen — for example, when the user
 * clicks a link or JavaScript triggers window.location. This is where you
 * can inspect the clicked URL and decide to allow or block navigation.
 */
static gboolean onDecidePolicy(WebKitWebView*, WebKitPolicyDecision* decision,
    WebKitPolicyDecisionType type, gpointer)
{
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        auto* navDecision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        auto* action = webkit_navigation_policy_decision_get_navigation_action(navDecision);
        auto* request = webkit_navigation_action_get_request(action);
        const char* uri = webkit_uri_request_get_uri(request);

        /* Log the navigation type */
        auto navType = webkit_navigation_action_get_navigation_type(action);
        const char* typeStr;
        switch (navType) {
        case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED:
            typeStr = "Link Clicked";
            break;
        case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED:
            typeStr = "Form Submitted";
            break;
        case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD:
            typeStr = "Back/Forward";
            break;
        case WEBKIT_NAVIGATION_TYPE_RELOAD:
            typeStr = "Reload";
            break;
        case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
            typeStr = "Form Resubmitted";
            break;
        case WEBKIT_NAVIGATION_TYPE_OTHER:
        default:
            typeStr = "Other";
            break;
        }

        g_print("[Navigate: %s] %s\n", typeStr, uri);
    }

    /* Allow all navigations */
    webkit_policy_decision_use(decision);
    return TRUE;
}

/* Handle display disconnection */
static void onDisplayDisconnected(WPEDisplay*, GError* error)
{
    g_warning("Display disconnected: %s", error ? error->message : "unknown");
    _exit(1);
}

/* --- HTML Context Menu --- */

/*
 * Context menu CSS (dark theme, suitable for kiosk/embedded UI).
 * All selectors prefixed with __wpe_ to avoid conflicts with page CSS.
 */
static const char* contextMenuCSS = R"CSS(
#__wpe_ctx_overlay {
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    z-index: 2147483646;
}
#__wpe_ctx_menu {
    position: fixed;
    min-width: 180px;
    max-width: 320px;
    background: #2b2b2b;
    border: 1px solid #505050;
    border-radius: 6px;
    padding: 4px 0;
    box-shadow: 0 8px 24px rgba(0,0,0,0.4);
    font-family: system-ui, -apple-system, sans-serif;
    font-size: 13px;
    color: #e0e0e0;
    z-index: 2147483647;
    overflow: hidden;
}
.__wpe_ctx_item {
    padding: 8px 16px;
    cursor: default;
    user-select: none;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
}
.__wpe_ctx_item:hover {
    background: #0060df;
    color: #ffffff;
}
.__wpe_ctx_sep {
    height: 1px;
    background: #404040;
    margin: 4px 8px;
}
)CSS";

/* Dismiss the HTML context menu overlay */
static void dismissContextMenu(WebKitWebView* webView)
{
    webkit_web_view_evaluate_javascript(webView,
        "document.getElementById('__wpe_ctx_overlay')?.remove();"
        "document.getElementById('__wpe_ctx_style')?.remove();",
        -1, nullptr, nullptr, nullptr, nullptr, nullptr);

    g_clear_object(&savedHitTestResult);
}

/*
 * Script message handler: called when user clicks a context menu item.
 * JS sends the WebKitContextMenuAction integer via postMessage().
 */
static void onContextMenuAction(WebKitUserContentManager*,
    JSCValue* value, gpointer)
{
    if (!savedWebView)
        return;

    int actionId = jsc_value_to_int32(value);
    auto* webView = savedWebView;

    /* Dismiss the menu first */
    dismissContextMenu(webView);

    /* Execute the action */
    switch (actionId) {
    case WEBKIT_CONTEXT_MENU_ACTION_GO_BACK:
        if (webkit_web_view_can_go_back(webView))
            webkit_web_view_go_back(webView);
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD:
        if (webkit_web_view_can_go_forward(webView))
            webkit_web_view_go_forward(webView);
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_RELOAD:
        webkit_web_view_reload(webView);
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_STOP:
        webkit_web_view_stop_loading(webView);
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_COPY:
        webkit_web_view_execute_editing_command(webView, "Copy");
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_CUT:
        webkit_web_view_execute_editing_command(webView, "Cut");
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_PASTE:
        webkit_web_view_execute_editing_command(webView, "Paste");
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT:
        webkit_web_view_toggle_inspector(webView);
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK:
        if (savedHitTestResult && webkit_hit_test_result_context_is_link(savedHitTestResult)) {
            const char* linkUri = webkit_hit_test_result_get_link_uri(savedHitTestResult);
            if (linkUri)
                webkit_web_view_load_uri(webView, linkUri);
        }
        break;
    case WEBKIT_CONTEXT_MENU_ACTION_COPY_LINK_TO_CLIPBOARD:
        if (savedHitTestResult && webkit_hit_test_result_context_is_link(savedHitTestResult)) {
            const char* linkUri = webkit_hit_test_result_get_link_uri(savedHitTestResult);
            if (linkUri) {
                g_autofree char* js = g_strdup_printf(
                    "navigator.clipboard.writeText('%s');", linkUri);
                webkit_web_view_evaluate_javascript(webView, js, -1,
                    nullptr, nullptr, nullptr, nullptr, nullptr);
            }
        }
        break;
    default:
        g_print("[ContextMenu] Unhandled action: %d\n", actionId);
        break;
    }
}

/*
 * Build JavaScript that creates the HTML context menu.
 * Reads items from WebKitContextMenu and generates DOM elements.
 * Returns a newly allocated string (caller must g_free).
 */
static gchar* buildContextMenuJS(WebKitContextMenu* contextMenu)
{
    GList* items = webkit_context_menu_get_items(contextMenu);
    if (!items)
        return nullptr;

    /* Get menu position */
    gint posX = 0, posY = 0;
    webkit_context_menu_get_position(contextMenu, &posX, &posY);

    GString* js = g_string_new("(function() {\n");

    /* Remove existing menu if any */
    g_string_append(js,
        "var old = document.getElementById('__wpe_ctx_overlay');\n"
        "if (old) old.remove();\n"
        "var oldStyle = document.getElementById('__wpe_ctx_style');\n"
        "if (oldStyle) oldStyle.remove();\n");

    /* Inject CSS */
    g_string_append_printf(js,
        "var style = document.createElement('style');\n"
        "style.id = '__wpe_ctx_style';\n"
        "style.textContent = `%s`;\n"
        "document.head.appendChild(style);\n",
        contextMenuCSS);

    /* Create overlay (click to dismiss) */
    g_string_append(js,
        "var overlay = document.createElement('div');\n"
        "overlay.id = '__wpe_ctx_overlay';\n"
        "overlay.addEventListener('click', function(e) {\n"
        "  if (e.target === overlay) {\n"
        "    overlay.remove();\n"
        "    style.remove();\n"
        "  }\n"
        "});\n");

    /* Create menu container */
    g_string_append(js,
        "var menu = document.createElement('div');\n"
        "menu.id = '__wpe_ctx_menu';\n");

    /* Add menu items */
    for (GList* l = items; l; l = l->next) {
        auto* item = WEBKIT_CONTEXT_MENU_ITEM(l->data);

        if (webkit_context_menu_item_is_separator(item)) {
            g_string_append(js,
                "var sep = document.createElement('div');\n"
                "sep.className = '__wpe_ctx_sep';\n"
                "menu.appendChild(sep);\n");
            continue;
        }

        /* Skip submenu items for now */
        if (webkit_context_menu_item_get_submenu(item))
            continue;

        const char* title = webkit_context_menu_item_get_title(item);
        if (!title)
            continue;

        auto action = webkit_context_menu_item_get_stock_action(item);

        /* Escape single quotes in title for JS string */
        g_autofree char* escapedTitle = g_strescape(title, nullptr);

        g_string_append_printf(js,
            "(function() {\n"
            "  var item = document.createElement('div');\n"
            "  item.className = '__wpe_ctx_item';\n"
            "  item.textContent = \"%s\";\n"
            "  item.addEventListener('click', function() {\n"
            "    window.webkit.messageHandlers.contextMenuAction.postMessage(%d);\n"
            "    overlay.remove();\n"
            "    style.remove();\n"
            "  });\n"
            "  menu.appendChild(item);\n"
            "})();\n",
            escapedTitle, static_cast<int>(action));
    }

    /* Position the menu and add to DOM */
    g_string_append_printf(js,
        "overlay.appendChild(menu);\n"
        "document.body.appendChild(overlay);\n"
        /* Set initial position */
        "menu.style.left = '%dpx';\n"
        "menu.style.top = '%dpx';\n"
        /* Adjust if menu goes off-screen */
        "var rect = menu.getBoundingClientRect();\n"
        "if (rect.right > window.innerWidth)\n"
        "  menu.style.left = Math.max(0, window.innerWidth - rect.width - 4) + 'px';\n"
        "if (rect.bottom > window.innerHeight)\n"
        "  menu.style.top = Math.max(0, window.innerHeight - rect.height - 4) + 'px';\n",
        posX, posY);

    g_string_append(js, "})();\n");

    return g_string_free(js, FALSE);
}

/*
 * WebKit context-menu signal handler.
 * Intercepts the default context menu and shows an HTML-based one instead.
 *
 * WPE signature: (WebKitWebView, WebKitContextMenu, gpointer event [always NULL],
 *                  WebKitHitTestResult, gpointer user_data)
 */
static gboolean onContextMenu(WebKitWebView* webView,
    WebKitContextMenu* contextMenu, gpointer /* event */,
    WebKitHitTestResult* hitTestResult, gpointer)
{
    /* Save state for action handler */
    savedWebView = webView;
    g_clear_object(&savedHitTestResult);
    savedHitTestResult = WEBKIT_HIT_TEST_RESULT(g_object_ref(hitTestResult));

    /* Build and inject the HTML menu */
    g_autofree char* js = buildContextMenuJS(contextMenu);
    if (js) {
        webkit_web_view_evaluate_javascript(webView, js, -1,
            nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    /* Return TRUE to suppress the default context menu */
    return TRUE;
}

/* --- Application activation --- */

static void activate(GApplication* application, gpointer)
{
    g_application_hold(application);

    /* 1. Network session */
    auto* networkSession = webkit_network_session_new(nullptr, nullptr);

    /* 2. WebKit settings */
    auto* settings = webkit_settings_new_with_settings(
        "enable-developer-extras", TRUE,
        "enable-webgl", TRUE,
        nullptr);

    /* 3. Web context */
    auto* webContext = WEBKIT_WEB_CONTEXT(g_object_new(
        WEBKIT_TYPE_WEB_CONTEXT, nullptr));

    /* 4. User content manager (for context menu script message handler) */
    auto* userContentManager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(
        userContentManager, "contextMenuAction", nullptr);
    g_signal_connect(userContentManager,
        "script-message-received::contextMenuAction",
        G_CALLBACK(onContextMenuAction), nullptr);

    /* 5. Create WebView — WPEPlatform backend is created automatically */
    auto* webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webContext,
        "network-session", networkSession,
        "settings", settings,
        "user-content-manager", userContentManager,
        nullptr));
    g_object_unref(settings);
    g_object_unref(userContentManager);

    /* 6. Setup WPEPlatform view */
    auto* wpeView = webkit_web_view_get_wpe_view(webView);
    if (wpeView) {
        /* Display disconnect handler */
        g_signal_connect(wpe_view_get_display(wpeView), "disconnected",
            G_CALLBACK(onDisplayDisconnected), nullptr);

        /* Toplevel (window) setup */
        auto* toplevel = wpe_view_get_toplevel(wpeView);
        if (windowWidth > 0 && windowHeight > 0)
            wpe_toplevel_resize(toplevel, windowWidth, windowHeight);
        wpe_toplevel_set_title(toplevel, defaultTitle);

        if (fullscreenMode)
            wpe_toplevel_fullscreen(toplevel);

        /* Keyboard event handler */
        g_signal_connect(wpeView, "event",
            G_CALLBACK(onViewEvent), webView);

        /* Title change tracking */
        g_signal_connect(webView, "notify::title",
            G_CALLBACK(onTitleChanged), wpeView);
    }

    /* 7. Connect WebKit signals */
    g_signal_connect(webView, "load-changed",
        G_CALLBACK(onLoadChanged), nullptr);
    g_signal_connect(webView, "decide-policy",
        G_CALLBACK(onDecidePolicy), nullptr);
    g_signal_connect(webView, "context-menu",
        G_CALLBACK(onContextMenu), nullptr);

    /* 8. Load URL */
    if (uriArguments && uriArguments[0]) {
        GFile* file = g_file_new_for_commandline_arg(uriArguments[0]);
        char* url = g_file_get_uri(file);
        g_object_unref(file);
        webkit_web_view_load_uri(webView, url);
        g_free(url);
    } else {
        webkit_web_view_load_uri(webView, "https://wpewebkit.org");
    }

    g_object_unref(webContext);
    g_clear_object(&networkSession);
}

/* --- Entry point --- */

int main(int argc, char* argv[])
{
    /* Parse command line options */
    GOptionContext* context = g_option_context_new(nullptr);
    g_option_context_add_main_entries(context, commandLineOptions, nullptr);

    GError* error = nullptr;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Cannot parse arguments: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);
        return 1;
    }
    g_option_context_free(context);

    /* Create and run GApplication */
    auto* app = g_application_new("org.example.WPEPlatformLauncher",
        G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    g_application_run(app, 0, nullptr);
    g_object_unref(app);

    return 0;
}
