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
    GtkWidget *uriEntry;
    GtkWidget *statusBar;
    WKViewRef webView;

    guint statusBarContextId;

    gchar *title;
    gdouble loadProgress;
};

struct _BrowserWindowClass {
    GtkWindowClass parent;
};

static void browserWindowLoaderClientInit(BrowserWindow*);
static void browserWindowUIClientInit(BrowserWindow*);

static gint windowCount = 0;

G_DEFINE_TYPE(BrowserWindow, browser_window, GTK_TYPE_WINDOW)

static void activateUriEntryCallback(BrowserWindow* window)
{
    const gchar *uri = gtk_entry_get_text(GTK_ENTRY(window->uriEntry));
    WKPageLoadURL(WKViewGetPage(window->webView), WKURLCreateWithUTF8CString(uri));
}

static void goBackCallback(BrowserWindow* window)
{
    WKPageGoBack(WKViewGetPage(window->webView));
}

static void goForwardCallback(BrowserWindow* window)
{
    WKPageGoForward(WKViewGetPage(window->webView));
}

static void browserWindowFinalize(GObject* gObject)
{
    BrowserWindow* window = BROWSER_WINDOW(gObject);

    g_free(window->title);

    G_OBJECT_CLASS(browser_window_parent_class)->finalize(gObject);

    if (g_atomic_int_dec_and_test(&windowCount))
        gtk_main_quit();
}

static void browserWindowGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* pspec)
{
    BrowserWindow* window = BROWSER_WINDOW(object);

    switch (propId) {
    case PROP_VIEW:
        g_value_set_object(value, (gpointer)browser_window_get_view(window));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserWindowSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* pspec)
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

static void browser_window_init(BrowserWindow* window)
{
    g_atomic_int_inc(&windowCount);

    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    window->uriEntry = gtk_entry_new();
    g_signal_connect_swapped(window->uriEntry, "activate", G_CALLBACK(activateUriEntryCallback), (gpointer)window);

    GtkWidget *toolbar = gtk_toolbar_new();
#if GTK_CHECK_VERSION(2, 15, 0)
    gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);
#else
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_HORIZONTAL);
#endif
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem *item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    g_signal_connect_swapped(item, "clicked", G_CALLBACK(goBackCallback), (gpointer)window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));

    item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
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

    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    window->mainBox = vbox;
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_widget_show(toolbar);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);
}

static void browserWindowConstructed(GObject* gObject)
{
    BrowserWindow* window = BROWSER_WINDOW(gObject);

    gtk_box_pack_start(GTK_BOX(window->mainBox), GTK_WIDGET(window->webView), TRUE, TRUE, 0);
    gtk_widget_show(GTK_WIDGET(window->webView));

    window->statusBar = gtk_statusbar_new();
    window->statusBarContextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(window->statusBar), "Link Hover");
    gtk_box_pack_start(GTK_BOX(window->mainBox), window->statusBar, FALSE, FALSE, 0);
    gtk_widget_show(window->statusBar);

    browserWindowLoaderClientInit(window);
    browserWindowUIClientInit(window);
}

static void browser_window_class_init(BrowserWindowClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserWindowConstructed;
    gobjectClass->get_property = browserWindowGetProperty;
    gobjectClass->set_property = browserWindowSetProperty;
    gobjectClass->finalize = browserWindowFinalize;

    g_object_class_install_property(gobjectClass,
                                    PROP_VIEW,
                                    g_param_spec_object("view",
                                                        "View",
                                                        "The web view of this window",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static char* WKStringGetCString(WKStringRef string)
{
    size_t length = WKStringGetMaximumUTF8CStringSize(string);
    char *buffer = (char *) g_malloc(length);
    WKStringGetUTF8CString(string, buffer, length);
    return buffer;
}

static char* WKURLGetCString(WKURLRef url)
{
    WKStringRef urlString = WKURLCopyString(url);
    char *urlText = WKStringGetCString(urlString);
    WKRelease(urlString);
    return urlText;
}

static void browserWindowUpdateTitle(BrowserWindow* window)
{
    GString *string = g_string_new(window->title);
    gdouble loadProgress = window->loadProgress * 100;
    g_string_append(string, " - WebKit Launcher");
    if (loadProgress < 100)
        g_string_append_printf(string, " (%f%%)", loadProgress);
    gchar *title = g_string_free(string, FALSE);
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
}

static void browserWindowSetTitle(BrowserWindow* window, const gchar* title)
{
    if (!g_strcmp0(window->title, title))
        return;

    g_free(window->title);
    window->title = g_strdup(title);
    browserWindowUpdateTitle(window);
}

static void browserWindowSetLoadProgress(BrowserWindow* window, gdouble progress)
{
    window->loadProgress = progress;
    browserWindowUpdateTitle(window);
}

static void browserWindowUpdateURL(BrowserWindow* window, WKURLRef url)
{
    if (!url) {
        gtk_entry_set_text(GTK_ENTRY(window->uriEntry), "");
        return;
    }

    char *urlText = WKURLGetCString(url);
    gtk_entry_set_text(GTK_ENTRY(window->uriEntry), urlText);
    g_free(urlText);
}

// Loader client.
static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKURLRef url = WKFrameCopyProvisionalURL(frame);
    browserWindowUpdateURL(BROWSER_WINDOW(clientInfo), url);
    if (url)
        WKRelease(url);
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKURLRef url = WKFrameCopyProvisionalURL(frame);
    browserWindowUpdateURL(BROWSER_WINDOW(clientInfo), url);
    if (url)
        WKRelease(url);
}

static void didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKURLRef url = WKFrameCopyProvisionalURL(frame);
    browserWindowUpdateURL(BROWSER_WINDOW(clientInfo), url);
    if (url)
        WKRelease(url);
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKURLRef url = WKFrameCopyURL(frame);
    browserWindowUpdateURL(BROWSER_WINDOW(clientInfo), url);
    if (url)
        WKRelease(url);
}

static void didFinishDocumentLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    BrowserWindow* window = BROWSER_WINDOW(clientInfo);
    gtk_widget_grab_focus(GTK_WIDGET(window->webView));
}

static void didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
}

static void didReceiveTitleForFrame(WKPageRef page, WKStringRef title, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    char *titleText = WKStringGetCString(title);
    browserWindowSetTitle(BROWSER_WINDOW(clientInfo), titleText);
    g_free(titleText);
}

static void didFirstLayoutForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
}

static void didFirstVisuallyNonEmptyLayoutForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
}

static void didRemoveFrameFromHierarchy(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
}

static void didStartProgress(WKPageRef page, const void* clientInfo)
{
    browserWindowSetLoadProgress(BROWSER_WINDOW(clientInfo), 0.);
}

static void didChangeProgress(WKPageRef page, const void* clientInfo)
{
    browserWindowSetLoadProgress(BROWSER_WINDOW(clientInfo), WKPageGetEstimatedProgress(page));
}

static void didFinishProgress(WKPageRef page, const void* clientInfo)
{
    browserWindowSetLoadProgress(BROWSER_WINDOW(clientInfo), 1.);
}

static void didBecomeUnresponsive(WKPageRef page, const void* clientInfo)
{
}

static void didBecomeResponsive(WKPageRef page, const void* clientInfo)
{
}

static void browserWindowLoaderClientInit(BrowserWindow* window)
{
    WKPageLoaderClient loadClient = {
        0,       /* version */
        window,  /* clientInfo */
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        didFinishDocumentLoadForFrame,
        didFinishLoadForFrame,
        didFailLoadWithErrorForFrame,
        0,       /* didSameDocumentNavigationForFrame */
        didReceiveTitleForFrame,
        didFirstLayoutForFrame,
        didFirstVisuallyNonEmptyLayoutForFrame,
        didRemoveFrameFromHierarchy,
        0,       /* didDisplayInsecureContentForFrame */
        0,       /* didRunInsecureContentForFrame */
        0,       /* canAuthenticateAgainstProtectionSpaceInFrame */
        0,       /* didReceiveAuthenticationChallengeInFrame */
        didStartProgress,
        didChangeProgress,
        didFinishProgress,
        didBecomeUnresponsive,
        didBecomeResponsive,
        0,       /* processDidCrash */
        0,       /* didChangeBackForwardList */
        0,       /* shouldGoToBackForwardListItem */
        0        /* didFailToInitializePlugin */
    };
    WKPageSetPageLoaderClient(WKViewGetPage(window->webView), &loadClient);
}

// UI Client.
static WKPageRef createNewPage(WKPageRef page, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton button, const void *clientInfo)
{
    WKViewRef webView = WKViewCreate(WKPageGetContext(page), 0);
    BrowserWindow* window = BROWSER_WINDOW(browser_window_new(webView));
    return WKRetain(WKViewGetPage(window->webView));
}

static void showPage(WKPageRef page, const void *clientInfo)
{
    gtk_widget_show(GTK_WIDGET(clientInfo));
}

static void closePage(WKPageRef page, const void *clientInfo)
{
    gtk_widget_destroy(GTK_WIDGET(clientInfo));
}

static GtkWidget* createMessageDialog(GtkWindow *parent, GtkMessageType type, GtkButtonsType buttons, gint defaultResponse, WKStringRef message, WKFrameRef frame)
{
    char *messageText = WKStringGetCString(message);
    GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, "%s", messageText);
    g_free(messageText);

    WKURLRef url = WKFrameCopyURL(frame);
    char *urlText = WKURLGetCString(url);
    WKRelease(url);
    gchar *title = g_strdup_printf("JavaScript - %s", urlText);
    g_free(urlText);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    g_free(title);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), defaultResponse);

    return dialog;
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef message, WKFrameRef frame, const void *clientInfo)
{
    GtkWidget *dialog = createMessageDialog(GTK_WINDOW(clientInfo), GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, GTK_RESPONSE_CLOSE, message, frame);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static bool runJavaScriptConfirm(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    GtkWidget *dialog = createMessageDialog(GTK_WINDOW(clientInfo), GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, message, frame);
    bool returnValue = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK);
    gtk_widget_destroy(dialog);

    return returnValue;
}

static WKStringRef runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void* clientInfo)
{
    GtkWidget *dialog = createMessageDialog(GTK_WINDOW(clientInfo), GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, message, frame);

    GtkWidget *entry = gtk_entry_new();
    char *value = WKStringGetCString(defaultValue);
    gtk_entry_set_text(GTK_ENTRY(entry), value);
    g_free(value);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_show(entry);

    WKStringRef returnValue = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) ? WKStringCreateWithUTF8CString(gtk_entry_get_text(GTK_ENTRY(entry))) : 0;
    gtk_widget_destroy(dialog);

    return returnValue;
}

static void mouseDidMoveOverElement(WKPageRef page, WKEventModifiers modifiers, WKTypeRef userData, const void *clientInfo)
{
    BrowserWindow *window = BROWSER_WINDOW(clientInfo);
    gtk_statusbar_pop(GTK_STATUSBAR(window->statusBar), window->statusBarContextId);

    if (!userData)
        return;

    if (WKGetTypeID(userData) != WKURLGetTypeID())
        return;

    gchar *link = WKURLGetCString((WKURLRef)userData);
    gtk_statusbar_push(GTK_STATUSBAR(window->statusBar), window->statusBarContextId, link);
    g_free(link);
}

static void browserWindowUIClientInit(BrowserWindow *window)
{
    WKPageUIClient uiClient = {
        0,      /* version */
        window, /* clientInfo */
        createNewPage,
        showPage,
        closePage,
        0,      /* takeFocus */
        0,      /* focus */
        0,      /* unfocus */
        runJavaScriptAlert,
        runJavaScriptConfirm,
        runJavaScriptPrompt,
        0,      /* setStatusText */
        mouseDidMoveOverElement,
        0,      /* missingPluginButtonClicked */
        0,      /* didNotHandleKeyEvent */
        0,      /* didNotHandleWheelEvent */
        0,      /* toolbarsAreVisible */
        0,      /* setToolbarsAreVisible */
        0,      /* menuBarIsVisible */
        0,      /* setMenuBarIsVisible */
        0,      /* statusBarIsVisible */
        0,      /* setStatusBarIsVisible */
        0,      /* isResizable */
        0,      /* setIsResizable */
        0,      /* getWindowFrame */
        0,      /* setWindowFrame */
        0,      /* runBeforeUnloadConfirmPanel */
        0,      /* didDraw */
        0,      /* pageDidScroll */
        0,      /* exceededDatabaseQuota */
        0,      /* runOpenPanel */
        0,      /* decidePolicyForGeolocationPermissionRequest */
        0,      /* headerHeight */
        0,      /* footerHeight */
        0,      /* drawHeader */
        0,      /* drawFooter */
        0,      /* printFrame */
        0,      /* runModal */
        0,      /* didCompleteRubberBandForMainFrame */
        0,      /* saveDataToFileInDownloadsFolder */
        0       /* shouldInterruptJavaScript */
    };
    WKPageSetPageUIClient(WKViewGetPage(window->webView), &uiClient);
}

// Public API.
GtkWidget* browser_window_new(WKViewRef view)
{
    g_return_val_if_fail(GTK_IS_WIDGET(view), 0);

    return GTK_WIDGET(g_object_new(BROWSER_TYPE_WINDOW,
                                   "type", GTK_WINDOW_TOPLEVEL,
                                   "view", view, NULL));
}

WKViewRef browser_window_get_view(BrowserWindow* window)
{
    g_return_val_if_fail(BROWSER_IS_WINDOW(window), 0);

    return window->webView;
}
