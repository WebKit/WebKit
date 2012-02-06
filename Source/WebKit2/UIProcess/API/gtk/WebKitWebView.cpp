/*
 * Copyright (C) 2011 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#include "WebKitBackForwardListPrivate.h"
#include "WebKitEnumTypes.h"
#include "WebKitError.h"
#include "WebKitLoaderClient.h"
#include "WebKitMarshal.h"
#include "WebKitPolicyClient.h"
#include "WebKitPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitUIClient.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include "WebPageProxy.h"
#include <WebCore/DragIcon.h>
#include <WebCore/GtkUtilities.h>
#include <glib/gi18n-lib.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

enum {
    LOAD_CHANGED,
    LOAD_FAILED,

    CREATE,
    READY_TO_SHOW,
    CLOSE,

    SCRIPT_ALERT,
    SCRIPT_CONFIRM,
    SCRIPT_PROMPT,

    DECIDE_POLICY,

    LAST_SIGNAL
};

enum {
    PROP_0,

    PROP_WEB_CONTEXT,
    PROP_TITLE,
    PROP_ESTIMATED_LOAD_PROGRESS,
    PROP_URI,
    PROP_ZOOM_LEVEL
};

struct _WebKitWebViewPrivate {
    WebKitWebContext* context;
    CString title;
    CString customTextEncoding;
    double estimatedLoadProgress;
    CString activeURI;
    bool replacingContent;

    GRefPtr<WebKitBackForwardList> backForwardList;
    GRefPtr<WebKitSettings> settings;
    GRefPtr<WebKitWindowProperties> windowProperties;
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitWebView, webkit_web_view, WEBKIT_TYPE_WEB_VIEW_BASE)

static gboolean webkitWebViewLoadFail(WebKitWebView* webView, WebKitLoadEvent, const char* failingURI, GError* error)
{
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)
        || g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)
        || g_error_matches(error, WEBKIT_PLUGIN_ERROR, WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD))
        return FALSE;

    GOwnPtr<char> htmlString(g_strdup_printf("<html><body>%s</body></html>", error->message));
    webkit_web_view_replace_content(webView, htmlString.get(), failingURI, 0);

    return TRUE;
}

static GtkWidget* webkitWebViewCreate(WebKitWebView*)
{
    return 0;
}

static GtkWidget* webkitWebViewCreateJavaScriptDialog(WebKitWebView* webView, GtkMessageType type, GtkButtonsType buttons, int defaultResponse, const char* message)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(webView));
    GtkWidget* dialog = gtk_message_dialog_new(widgetIsOnscreenToplevelWindow(parent) ? GTK_WINDOW(parent) : 0,
                                               GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, "%s", message);
    GOwnPtr<char> title(g_strdup_printf("JavaScript - %s", webkit_web_view_get_uri(webView)));
    gtk_window_set_title(GTK_WINDOW(dialog), title.get());
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), defaultResponse);

    return dialog;
}

static gboolean webkitWebViewScriptAlert(WebKitWebView* webView, const char* message)
{
    GtkWidget* dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, GTK_RESPONSE_CLOSE, message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return TRUE;
}

static gboolean webkitWebViewScriptConfirm(WebKitWebView* webView, const char* message, gboolean* confirmed)
{
    GtkWidget* dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, message);
    *confirmed = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
    gtk_widget_destroy(dialog);
    return TRUE;
}

static gboolean webkitWebViewScriptPrompt(WebKitWebView* webView, const char* message, const char* defaultText, char** text)
{
    GtkWidget* dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, message);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), defaultText);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_show(entry);

    *text = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) ? g_strdup(gtk_entry_get_text(GTK_ENTRY(entry))) : 0;
    return TRUE;
}

static gboolean webkitWebViewDecidePolicy(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType)
{
    webkit_policy_decision_use(decision);
    return TRUE;
}

static void webkitWebViewConstructed(GObject* object)
{
    if (G_OBJECT_CLASS(webkit_web_view_parent_class)->constructed)
        G_OBJECT_CLASS(webkit_web_view_parent_class)->constructed(object);

    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    WebKitWebViewPrivate* priv = webView->priv;
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(webView);

    webkitWebViewBaseCreateWebPage(webViewBase, webkitWebContextGetWKContext(priv->context), 0);

    attachLoaderClientToView(webView);
    attachUIClientToView(webView);
    attachPolicyClientToPage(webView);

    WebPageProxy* page = webkitWebViewBaseGetPage(webViewBase);
    priv->backForwardList = adoptGRef(webkitBackForwardListCreate(WKPageGetBackForwardList(toAPI(page))));
    priv->settings = adoptGRef(webkit_settings_new());
    webkitSettingsAttachSettingsToPage(priv->settings.get(), toAPI(page));
}

static void webkitWebViewSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
    case PROP_WEB_CONTEXT:
        webView->priv->context = WEBKIT_WEB_CONTEXT(g_value_get_object(value));
        break;
    case PROP_ZOOM_LEVEL:
        webkit_web_view_set_zoom_level(webView, g_value_get_double(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
    case PROP_WEB_CONTEXT:
        g_value_take_object(value, webView->priv->context);
        break;
    case PROP_TITLE:
        g_value_set_string(value, webView->priv->title.data());
        break;
    case PROP_ESTIMATED_LOAD_PROGRESS:
        g_value_set_double(value, webkit_web_view_get_estimated_load_progress(webView));
        break;
    case PROP_URI:
        g_value_set_string(value, webkit_web_view_get_uri(webView));
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_double(value, webkit_web_view_get_zoom_level(webView));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewFinalize(GObject* object)
{
    WEBKIT_WEB_VIEW(object)->priv->~WebKitWebViewPrivate();
    G_OBJECT_CLASS(webkit_web_view_parent_class)->finalize(object);
}

static void webkit_web_view_init(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(webView, WEBKIT_TYPE_WEB_VIEW, WebKitWebViewPrivate);
    webView->priv = priv;
    new (priv) WebKitWebViewPrivate();

    webView->priv->windowProperties = adoptGRef(webkitWindowPropertiesCreate());
}

static gboolean webkitWebViewAccumulatorObjectHandled(GSignalInvocationHint*, GValue* returnValue, const GValue* handlerReturn, gpointer)
{
    void* object = g_value_get_object(handlerReturn);
    if (object)
        g_value_set_object(returnValue, object);

    return !object;
}

static void webkit_web_view_class_init(WebKitWebViewClass* webViewClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webViewClass);

    gObjectClass->constructed = webkitWebViewConstructed;
    gObjectClass->set_property = webkitWebViewSetProperty;
    gObjectClass->get_property = webkitWebViewGetProperty;
    gObjectClass->finalize = webkitWebViewFinalize;

    webViewClass->load_failed = webkitWebViewLoadFail;
    webViewClass->create = webkitWebViewCreate;
    webViewClass->script_alert = webkitWebViewScriptAlert;
    webViewClass->script_confirm = webkitWebViewScriptConfirm;
    webViewClass->script_prompt = webkitWebViewScriptPrompt;
    webViewClass->decide_policy = webkitWebViewDecidePolicy;

    g_type_class_add_private(webViewClass, sizeof(WebKitWebViewPrivate));

    /**
     * WebKitWebView:web-context:
     *
     * The #WebKitWebContext of the view.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_WEB_CONTEXT,
                                    g_param_spec_object("web-context",
                                                        _("Web Context"),
                                                        _("The web context for the view"),
                                                        WEBKIT_TYPE_WEB_CONTEXT,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebView:title:
     *
     * The main frame document title of this #WebKitWebView. If
     * the title has not been received yet, it will be %NULL.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_TITLE,
                                    g_param_spec_string("title",
                                                        _("Title"),
                                                        _("Main frame document title"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView:estimated-load-progress:
     *
     * An estimate of the percent completion for the current loading operation.
     * This value will range from 0.0 to 1.0 and, once a load completes,
     * will remain at 1.0 until a new load starts, at which point it
     * will be reset to 0.0.
     * The value is an estimate based on the total number of bytes expected
     * to be received for a document, including all its possible subresources
     * and child documents.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ESTIMATED_LOAD_PROGRESS,
                                    g_param_spec_double("estimated-load-progress",
                                                        _("Estimated Load Progress"),
                                                        _("An estimate of the percent completion for a document load"),
                                                        0.0, 1.0, 0.0,
                                                        WEBKIT_PARAM_READABLE));
    /**
     * WebKitWebView:uri:
     *
     * The current active URI of the #WebKitWebView.
     * See webkit_web_view_get_uri() for more details.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_URI,
                                    g_param_spec_string("uri",
                                                        _("URI"),
                                                        _("The current active URI of the view"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView::load-changed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @load_event: the #WebKitLoadEvent
     *
     * Emitted when the a load operation in @web_view changes.
     * The signal is always emitted with %WEBKIT_LOAD_STARTED when a
     * new load request is made and %WEBKIT_LOAD_FINISHED when the load
     * finishes successfully or due to an error. When the ongoing load
     * operation fails #WebKitWebView::load-failed signal is emitted
     * before #WebKitWebView::load-changed is emitted with
     * %WEBKIT_LOAD_FINISHED.
     * If a redirection is received from the server, this signal is emitted
     * with %WEBKIT_LOAD_REDIRECTED after the initial emission with
     * %WEBKIT_LOAD_STARTED and before %WEBKIT_LOAD_COMMITTED.
     * When the page content starts arriving the signal is emitted with
     * %WEBKIT_LOAD_COMMITTED event.
     *
     * You can handle this signal and use a switch to track any ongoing
     * load operation.
     *
     * <informalexample><programlisting>
     * static void web_view_load_changed (WebKitWebView  *web_view,
     *                                    WebKitLoadEvent load_event,
     *                                    gpointer        user_data)
     * {
     *     switch (load_event) {
     *     case WEBKIT_LOAD_STARTED:
     *         /<!-- -->* New load, we have now a provisional URI *<!-- -->/
     *         provisional_uri = webkit_web_view_get_uri (web_view);
     *         /<!-- -->* Here we could start a spinner or update the
     *          <!-- -->* location bar with the provisional URI *<!-- -->/
     *         break;
     *     case WEBKIT_LOAD_REDIRECTED:
     *         redirected_uri = webkit_web_view_get_uri (web_view);
     *         break;
     *     case WEBKIT_LOAD_COMMITTED:
     *         /<!-- -->* The load is being performed. Current URI is
     *          <!-- -->* the final one and it won't change unless a new
     *          <!-- -->* load is requested or a navigation within the
     *          <!-- -->* same page is performed *<!-- -->/
     *         uri = webkit_web_view_get_uri (web_view);
     *         break;
     *     case WEBKIT_LOAD_FINISHED:
     *         /<!-- -->* Load finished, we can now stop the spinner *<!-- -->/
     *         break;
     *     }
     * }
     * </programlisting></informalexample>
     */
    signals[LOAD_CHANGED] =
        g_signal_new("load-changed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, load_changed),
                     0, 0,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE, 1,
                     WEBKIT_TYPE_LOAD_EVENT);

    /**
     * WebKitWebView::load-failed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @load_event: the #WebKitLoadEvent of the load operation
     * @failing_uri: the URI that failed to load
     * @error: the #GError that was triggered
     *
     * Emitted when an error occurs during a load operation.
     * If the error happened when starting to load data for a page
     * @load_event will be %WEBKIT_LOAD_STARTED. If it happened while
     * loading a committed data source @load_event will be %WEBKIT_LOAD_COMMITTED.
     * Since a load error causes the load operation to finish, the signal
     * WebKitWebView::load-changed will always be emitted with
     * %WEBKIT_LOAD_FINISHED event right after this one.
     *
     * By default, if the signal is not handled, a stock error page will be displayed.
     * You need to handle the signal if you want to provide your own error page.
     */
    signals[LOAD_FAILED] =
        g_signal_new("load-failed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, load_failed),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__ENUM_STRING_POINTER,
                     G_TYPE_BOOLEAN, 3,
                     WEBKIT_TYPE_LOAD_EVENT,
                     G_TYPE_STRING,
                     G_TYPE_POINTER);

    /**
     * WebKitWebView:zoom-level:
     *
     * The zoom level of the #WebKitWebView content.
     * See webkit_web_view_set_zoom_level() for more details.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ZOOM_LEVEL,
                                    g_param_spec_double("zoom-level",
                                                        "Zoom level",
                                                        "The zoom level of the view content",
                                                        0, G_MAXDOUBLE, 1,
                                                        WEBKIT_PARAM_READWRITE));

    /**
     * WebKitWebView::create:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted when the creation of a new #WebKitWebView is requested.
     * If this signal is handled the signal handler should return the
     * newly created #WebKitWebView.
     *
     * The new #WebKitWebView should not be displayed to the user
     * until the #WebKitWebView::ready-to-show signal is emitted.
     *
     * Returns: (transfer full): a newly allocated #WebKitWebView widget
     *    or %NULL to propagate the event further.
     */
    signals[CREATE] =
        g_signal_new("create",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, create),
                     webkitWebViewAccumulatorObjectHandled, 0,
                     webkit_marshal_OBJECT__VOID,
                     GTK_TYPE_WIDGET, 0);

    /**
     * WebKitWebView::ready-to-show:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::create on the newly created #WebKitWebView
     * when it should be displayed to the user. When this signal is emitted
     * all the information about how the window should look, including
     * size, position, whether the location, status and scrollbars
     * should be displayed, is already set on the #WebKitWindowProperties
     * of @web_view. See also webkit_web_view_get_window_properties().
     */
    signals[READY_TO_SHOW] =
        g_signal_new("ready-to-show",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, ready_to_show),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebView::close:
     * @webView: the #WebKitWebView on which the signal is emitted
     *
     * Emitted when closing a #WebKitWebView is requested. This occurs when a
     * call is made from JavaScript's <function>window.close</function> function.
     * It is the owner's responsibility to handle this signal to hide or
     * destroy the #WebKitWebView, if necessary.
     */
    signals[CLOSE] =
        g_signal_new("close",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, close),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebView::script-alert:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @message: the message text
     *
     * Emitted when JavaScript code calls <function>window.alert</function>. If the
     * signal is not handled a message dialog with a single Close button will be
     * shown with the message text.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[SCRIPT_ALERT] =
            g_signal_new("script-alert",
                         G_TYPE_FROM_CLASS(webViewClass),
                         G_SIGNAL_RUN_LAST,
                         G_STRUCT_OFFSET(WebKitWebViewClass, script_alert),
                         g_signal_accumulator_true_handled, 0,
                         webkit_marshal_BOOLEAN__STRING,
                         G_TYPE_BOOLEAN, 1,
                         G_TYPE_STRING);

    /**
     * WebKitWebView::script-confirm:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @message: the message text
     * @confirmed: (out): return location for confirm dialog response
     *
     * Emitted when JavaScript code calls <function>confirm</function>. If the
     * signal is not handled a message dialog with OK and Cancel buttons will be
     * shown with the message text. If OK button is clicked @confirmed will be
     * set to %TRUE, otherwise it will be %FALSE.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[SCRIPT_CONFIRM] =
            g_signal_new("script-confirm",
                         G_TYPE_FROM_CLASS(webViewClass),
                         G_SIGNAL_RUN_LAST,
                         G_STRUCT_OFFSET(WebKitWebViewClass, script_confirm),
                         g_signal_accumulator_true_handled, 0,
                         webkit_marshal_BOOLEAN__STRING_POINTER,
                         G_TYPE_BOOLEAN, 2,
                         G_TYPE_STRING, G_TYPE_POINTER);

    /**
     * WebKitWebView::script-prompt:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @message: the message text
     * @default (allow-none): the default text
     * @text: (out): return location for prompt dialog text response
     *
     * Emitted when JavaScript code calls <function>prompt</function>. If the
     * signal is not handled a message dialog with OK and Cancel buttons and
     * a text entry will be shown with the message text. If OK button is clicked
     * @text will contain the text entered by the user, otherwise it will be %NULL.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[SCRIPT_PROMPT] =
            g_signal_new("script-prompt",
                         G_TYPE_FROM_CLASS(webViewClass),
                         G_SIGNAL_RUN_LAST,
                         G_STRUCT_OFFSET(WebKitWebViewClass, script_prompt),
                         g_signal_accumulator_true_handled, 0,
                         webkit_marshal_BOOLEAN__STRING_STRING_POINTER,
                         G_TYPE_BOOLEAN, 3,
                         G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

    /**
     * WebKitPolicyClient::decide-policy
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @decision: the #WebKitNavigationPolicyDecision
     * @decision_type: a #WebKitPolicyDecisionType denoting the type of @decision
     *
     * This signal is emitted when WebKit is requesting the client to decide a policy
     * decision, such as whether to navigate to a page, open a new window or whether or
     * not to download a resource. The #WebKitNavigationPolicyDecision passed in the
     * @decision argument is a generic type, but should be casted to a more
     * specific type when making the decision. For example:
     *
     * <informalexample><programlisting>
     * static gboolean
     * decide_policy_cb (WebKitWebView *web_view,
     *                   WebKitPolicyDecision *decision,
     *                   WebKitPolicyDecisionType type)
     * {
     *     switch (type) {
     *     case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
     *         WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
     *         /<!-- -->* Make a policy decision here. *<!-- -->/
     *         break;
     *     case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
     *         WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
     *         /<!-- -->* Make a policy decision here. *<!-- -->/
     *         break;
     *     case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
     *         WebKitResponsePolicyDecision *response = WEBKIT_RESPONSE_POLICY_DECISION (decision);
     *         /<!-- -->* Make a policy decision here. *<!-- -->/
     *         break;
     *     default:
     *         /<!-- -->* Making no decision results in webkit_policy_decision_use(). *<!-- -->/
     *         return FALSE;
     *     }
     *     return TRUE;
     * }
     * </programlisting></informalexample>
     *
     * It is possible to make policy decision asynchronously, by simply calling g_object_ref()
     * on the @decision argument and returning %TRUE to block the default signal handler.
     * If the last reference is removed on a #WebKitPolicyDecision and no decision has been
     * made explicitly, webkit_policy_decision_use() will be the default policy decision. The
     * default signal handler will simply call webkit_policy_decision_use(). Only the first
     * policy decision chosen for a given #WebKitPolicyDecision will have any affect.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[DECIDE_POLICY] =
        g_signal_new("decide-policy",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, decide_policy),
                     g_signal_accumulator_true_handled, 0 /* accumulator data */,
                     webkit_marshal_BOOLEAN__OBJECT_ENUM,
                     G_TYPE_BOOLEAN, 2, /* number of parameters */
                     WEBKIT_TYPE_POLICY_DECISION,
                     WEBKIT_TYPE_POLICY_DECISION_TYPE);
}

void webkitWebViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent)
{
    if (webView->priv->replacingContent) {
        if (loadEvent == WEBKIT_LOAD_FINISHED)
            webView->priv->replacingContent = false;
        return;
    }

    if (loadEvent != WEBKIT_LOAD_FINISHED)
        webkitWebViewUpdateURI(webView);
    g_signal_emit(webView, signals[LOAD_CHANGED], 0, loadEvent);
}

void webkitWebViewLoadFailed(WebKitWebView* webView, WebKitLoadEvent loadEvent, const char* failingURI, GError *error)
{
    if (webView->priv->replacingContent)
        return;

    gboolean returnValue;
    g_signal_emit(webView, signals[LOAD_FAILED], 0, loadEvent, failingURI, error, &returnValue);
    g_signal_emit(webView, signals[LOAD_CHANGED], 0, WEBKIT_LOAD_FINISHED);
}

void webkitWebViewSetTitle(WebKitWebView* webView, const CString& title)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->title == title)
        return;

    priv->title = title;
    g_object_notify(G_OBJECT(webView), "title");
}

void webkitWebViewSetEstimatedLoadProgress(WebKitWebView* webView, double estimatedLoadProgress)
{
    if (webView->priv->replacingContent)
        return;

    if (webView->priv->estimatedLoadProgress == estimatedLoadProgress)
        return;

    webView->priv->estimatedLoadProgress = estimatedLoadProgress;
    g_object_notify(G_OBJECT(webView), "estimated-load-progress");
}

void webkitWebViewUpdateURI(WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKURLRef> wkURL(AdoptWK, WKPageCopyActiveURL(toAPI(page)));
    CString activeURI;
    if (wkURL)
        activeURI = toImpl(wkURL.get())->string().utf8();

    if (webView->priv->activeURI == activeURI)
        return;

    webView->priv->activeURI = activeURI;
    g_object_notify(G_OBJECT(webView), "uri");
}

WKPageRef webkitWebViewCreateNewPage(WebKitWebView* webView, WKDictionaryRef wkWindowFeatures)
{
    WebKitWebView* newWebView;
    g_signal_emit(webView, signals[CREATE], 0, &newWebView);
    if (!newWebView)
        return 0;

    webkitWindowPropertiesUpdateFromWKWindowFeatures(newWebView->priv->windowProperties.get(), wkWindowFeatures);

    return static_cast<WKPageRef>(WKRetain(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(newWebView)))));
}

void webkitWebViewReadyToShowPage(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[READY_TO_SHOW], 0, NULL);
}

void webkitWebViewClosePage(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[CLOSE], 0, NULL);
}

void webkitWebViewRunJavaScriptAlert(WebKitWebView* webView, const CString& message)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_ALERT], 0, message.data(), &returnValue);
}

bool webkitWebViewRunJavaScriptConfirm(WebKitWebView* webView, const CString& message)
{
    gboolean returnValue, confirmed;
    g_signal_emit(webView, signals[SCRIPT_CONFIRM], 0, message.data(), &confirmed, &returnValue);
    return confirmed;
}

WKStringRef webkitWebViewRunJavaScriptPrompt(WebKitWebView* webView, const CString& message, const CString& defaultText)
{
    gboolean returnValue;
    GOwnPtr<char> text;
    g_signal_emit(webView, signals[SCRIPT_PROMPT], 0, message.data(), defaultText.data(), &text.outPtr(), &returnValue);
    return text ? WKStringCreateWithUTF8CString(text.get()) : 0;
}

void webkitWebViewMakePolicyDecision(WebKitWebView* webView, WebKitPolicyDecisionType type, WebKitPolicyDecision* decision)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[DECIDE_POLICY], 0, decision, type, &returnValue);
}

/**
 * webkit_web_view_new:
 *
 * Creates a new #WebKitWebView with the default #WebKitWebContext.
 * See also webkit_web_view_new_with_context().
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
 * Creates a new #WebKitWebView with the given #WebKitWebContext.
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new_with_context(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", context, NULL));
}

/**
 * webkit_web_view_get_context:
 * @web_view: a #WebKitWebView
 *
 * Gets the web context of @web_view.
 *
 * Returns: (transfer none): the #WebKitWebContext of the view
 */
WebKitWebContext* webkit_web_view_get_context(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->context;
}

/**
 * webkit_web_view_load_uri:
 * @web_view: a #WebKitWebView
 * @uri: an URI string
 *
 * Requests loading of the specified URI string.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_uri(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    WKRetainPtr<WKURLRef> url(AdoptWK, WKURLCreateWithUTF8CString(uri));
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageLoadURL(toAPI(page), url.get());
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_load_html:
 * @web_view: a #WebKitWebView
 * @content: The HTML string to load
 * @base_uri: (allow-none): The base URI for relative locations or %NULL
 *
 * Load the given @content string with the specified @base_uri.
 * Relative URLs in the @content will be resolved against @base_uri.
 * When @base_uri is %NULL, it defaults to "about:blank". The mime type
 * of the document will be "text/html". You can monitor the load operation
 * by connecting to #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_html(WebKitWebView* webView, const gchar* content, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKStringRef> contentRef(AdoptWK,  WKStringCreateWithUTF8CString(content));
    WKRetainPtr<WKURLRef> baseURIRef = baseURI ? adoptWK(WKURLCreateWithUTF8CString(baseURI)) : 0;
    WKPageLoadHTMLString(toAPI(page), contentRef.get(), baseURIRef.get());
}

/**
 * webkit_web_view_load_plain_text:
 * @web_view: a #WebKitWebView
 * @plain_text: The plain text to load
 *
 * Load the specified @plain_text string into @web_view. The mime type of
 * document will be "text/plain". You can monitor the load
 * operation by connecting to #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_plain_text(WebKitWebView* webView, const gchar* plainText)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(plainText);

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKStringRef> plainTextRef(AdoptWK, WKStringCreateWithUTF8CString(plainText));
    WKPageLoadPlainTextString(toAPI(page), plainTextRef.get());
}

/**
 * webkit_web_view_load_request:
 * @web_view: a #WebKitWebView
 * @request: a #WebKitURIRequest to load
 *
 * Requests loading of the specified #WebKitURIRequest.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_request(WebKitWebView* webView, WebKitURIRequest* request)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_URI_REQUEST(request));

    WKRetainPtr<WKURLRef> wkURL(AdoptWK, WKURLCreateWithUTF8CString(webkit_uri_request_get_uri(request)));
    WKRetainPtr<WKURLRequestRef> wkRequest(AdoptWK, WKURLRequestCreateWithWKURL(wkURL.get()));
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageLoadURLRequest(toAPI(page), wkRequest.get());
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_replace_content:
 * @web_view: a #WebKitWebView
 * @content: the new content to display as the main page of the @web_view
 * @content_uri: the URI for the page content
 * @base_uri: (allow-none): the base URI for relative locations or %NULL
 *
 * Replace the content of @web_view with @content using @content_uri as page URI.
 * This allows clients to display page-loading errors in the #WebKitWebView itself.
 * This is typically called from #WebKitWebView::load-failed signal. The URI passed in
 * @base_uri has to be an absolute URI. The mime type of the document will be "text/html".
 * Signals #WebKitWebView::load-changed and #WebKitWebView::load-failed are not emitted
 * when replacing content of a #WebKitWebView using this method.
 */
void webkit_web_view_replace_content(WebKitWebView* webView, const gchar* content, const gchar* contentURI, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);
    g_return_if_fail(contentURI);

    webView->priv->replacingContent = true;

    WKRetainPtr<WKStringRef> htmlString(AdoptWK, WKStringCreateWithUTF8CString(content));
    WKRetainPtr<WKURLRef> contentURL(AdoptWK, WKURLCreateWithUTF8CString(contentURI));
    WKRetainPtr<WKURLRef> baseURL = baseURI ? adoptWK(WKURLCreateWithUTF8CString(baseURI)) : 0;
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageLoadAlternateHTMLString(toAPI(page), htmlString.get(), baseURL.get(), contentURL.get());
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_get_title:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:title property.
 * You can connect to notify::title signal of @web_view to
 * be notified when the title has been received.
 *
 * Returns: The main frame document title of @web_view.
 */
const gchar* webkit_web_view_get_title(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->title.data();
}

/**
 * webkit_web_view_reload:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view.
 * See also webkit_web_view_reload_bypass_cache().
 */
void webkit_web_view_reload(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageReload(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_reload_bypass_cache:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view without
 * using any cached data.
 */
void webkit_web_view_reload_bypass_cache(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageReloadFromOrigin(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_stop_loading:
 * @web_view: a #WebKitWebView
 *
 * Stops any ongoing loading operation in @web_view.
 * This method does nothing if no content is being loaded.
 * If there is a loading operation in progress, it will be cancelled and
 * #WebKitWebView::load-failed signal will be emitted with
 * %WEBKIT_NETWORK_ERROR_CANCELLED error.
 */
void webkit_web_view_stop_loading(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageStopLoading(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_go_back:
 * @web_view: a #WebKitWebView
 *
 * Loads the previous history item.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_back(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageGoBack(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_can_go_back:
 * @web_view: a #WebKitWebView
 *
 * Determines whether @web_view has a previous history item.
 *
 * Returns: %TRUE if able to move back or %FALSE otherwise.
 */
gboolean webkit_web_view_can_go_back(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return WKPageCanGoBack(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Loads the next history item.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_forward(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageGoForward(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_can_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Determines whether @web_view has a next history item.
 *
 * Returns: %TRUE if able to move forward or %FALSE otherwise.
 */
gboolean webkit_web_view_can_go_forward(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return WKPageCanGoForward(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_get_uri:
 * @web_view: a #WebKitWebView
 *
 * Returns the current active URI of @web_view. The active URI might change during
 * a load operation:
 *
 * <orderedlist>
 * <listitem><para>
 *   When nothing has been loaded yet on @web_view the active URI is %NULL.
 * </para></listitem>
 * <listitem><para>
 *   When a new load operation starts the active URI is the requested URI:
 *   <itemizedlist>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_uri(),
 *     the requested URI is the given one.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_html(),
 *     the requested URI is "about:blank".
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_go_back() or
 *     webkit_web_view_go_forward(), the requested URI is the original URI
 *     of the previous/next item in the #WebKitBackForwardList of @web_view.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by
 *     webkit_web_view_go_to_back_forward_list_item(), the requested URI
 *     is the opriginal URI of the given #WebKitBackForwardListItem.
 *   </para></listitem>
 *   </itemizedlist>
 * </para></listitem>
 * <listitem><para>
 *   If there is a server redirection during the load operation,
 *   the active URI is the redirected URI. When the signal
 *   #WebKitWebView::load-changed is emitted with %WEBKIT_LOAD_REDIRECTED
 *   event, the active URI is already updated to the redirected URI.
 * </para></listitem>
 * <listitem><para>
 *   When the signal #WebKitWebView::load-changed is emitted
 *   with %WEBKIT_LOAD_COMMITTED event, the active URI is the final
 *   one and it will not change unless a new load operation is started
 *   or a navigation action within the same page is performed.
 * </para></listitem>
 * <listitem><para>
 *   When the page content is replaced using webkit_web_view_replace_content(),
 *   the active URI is the content_uri provided.
 * </para></listitem>
 * </orderedlist>
 *
 * You can monitor the active URI by connecting to the notify::uri
 * signal of @web_view.
 *
 * Returns: the current active URI of @web_view or %NULL
 *    if nothing has been loaded yet.
 */
const gchar* webkit_web_view_get_uri(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->activeURI.data();
}

/**
 * webkit_web_view_get_custom_charset:
 * @web_view: a #WebKitWebView
 *
 * Returns the current custom character encoding name of @web_view.
 *
 * Returns: the current custom character encoding name or %NULL if no
 *    custom character encoding has been set.
 */
const gchar* webkit_web_view_get_custom_charset(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKStringRef> wkCustomEncoding(AdoptWK, WKPageCopyCustomTextEncodingName(toAPI(page)));
    if (WKStringIsEmpty(wkCustomEncoding.get()))
        return 0;

    webView->priv->customTextEncoding = toImpl(wkCustomEncoding.get())->string().utf8();
    return webView->priv->customTextEncoding.data();
}

/**
 * webkit_web_view_set_custom_charset:
 * @web_view: a #WebKitWebView
 * @charset: (allow-none): a character encoding name or %NULL
 *
 * Sets the current custom character encoding override of @web_view. The custom
 * character encoding will override any text encoding detected via HTTP headers or
 * META tags. Calling this method will stop any current load operation and reload the
 * current page. Setting the custom character encoding to %NULL removes the character
 * encoding override.
 */
void webkit_web_view_set_custom_charset(WebKitWebView* webView, const gchar* charset)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKStringRef> wkEncodingName = charset ? adoptWK(WKStringCreateWithUTF8CString(charset)) : 0;
    WKPageSetCustomTextEncodingName(toAPI(page), wkEncodingName.get());
}

/**
 * webkit_web_view_get_estimated_load_progress:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:estimated-load-progress property.
 * You can monitor the estimated progress of a load operation by
 * connecting to the notify::estimated-load-progress signal of @web_view.
 *
 * Returns: an estimate of the of the percent complete for a document
 *     load as a range from 0.0 to 1.0.
 */
gdouble webkit_web_view_get_estimated_load_progress(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    return webView->priv->estimatedLoadProgress;
}

/**
 * webkit_web_view_get_back_forward_list:
 * @web_view: a #WebKitWebView
 *
 * Obtains the #WebKitBackForwardList associated with the given #WebKitWebView. The
 * #WebKitBackForwardList is owned by the #WebKitWebView.
 *
 * Returns: (transfer none): the #WebKitBackForwardList
 */
WebKitBackForwardList* webkit_web_view_get_back_forward_list(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->backForwardList.get();
}

/**
 * webkit_web_view_go_to_back_forward_list_item:
 * @web_view: a #WebKitWebView
 * @list_item: a #WebKitBackForwardListItem
 *
 * Loads the specific history item @list_item.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_to_back_forward_list_item(WebKitWebView* webView, WebKitBackForwardListItem* listItem)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_BACK_FORWARD_LIST_ITEM(listItem));

    WKPageGoToBackForwardListItem(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))),
                                  webkitBackForwardListItemGetWKItem(listItem));
    webkitWebViewUpdateURI(webView);
}

/**
 * webkit_web_view_set_settings:
 * @web_view: a #WebKitWebView
 * @settings: a #WebKitSettings
 *
 * Sets the #WebKitSettings to be applied to @web_view. The
 * existing #WebKitSettings of @web_view will be replaced by
 * @settings. New settings are applied immediately on @web_view.
 * The same #WebKitSettings object can be shared
 * by multiple #WebKitWebView<!-- -->s.
 */
void webkit_web_view_set_settings(WebKitWebView* webView, WebKitSettings* settings)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    if (webView->priv->settings == settings)
        return;

    webView->priv->settings = settings;
    webkitSettingsAttachSettingsToPage(settings, toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_get_settings:
 * @web_view: a #WebKitWebView
 *
 * Gets the #WebKitSettings currently applied to @web_view.
 * If no other #WebKitSettings have been explicitly applied to
 * @web_view with webkit_web_view_set_settings(), the default
 * #WebKitSettings will be returned. This method always returns
 * a valid #WebKitSettings object.
 * To modify any of the @web_view settings, you can either create
 * a new #WebKitSettings object with webkit_settings_new(), setting
 * the desired preferences, and then replace the existing @web_view
 * settings with webkit_web_view_set_settings() or get the existing
 * @web_view settings and update it directly. #WebKitSettings objects
 * can be shared by multiple #WebKitWebView<!-- -->s, so modifying
 * the settings of a #WebKitWebView would affect other
 * #WebKitWebView<!-- -->s using the same #WebKitSettings.
 *
 * Returns: (transfer none): the #WebKitSettings attached to @web_view
 */
WebKitSettings* webkit_web_view_get_settings(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->settings.get();
}

/**
 * webkit_web_view_get_window_properties:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWindowProperties object containing the properties
 * that the window containing @web_view should have.
 *
 * Returns: (transfer none): the #WebKitWindowProperties of @web_view
 */
WebKitWindowProperties* webkit_web_view_get_window_properties(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->windowProperties.get();
}

/**
 * webkit_web_view_set_zoom_level:
 * @web_view: a #WebKitWebView
 * @zoom_level: the zoom level
 *
 * Set the zoom level of @web_view, i.e. the factor by which the
 * view contents are scaled with respect to their original size.
 */
void webkit_web_view_set_zoom_level(WebKitWebView* webView, gdouble zoomLevel)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageRef wkPage = toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView)));
    if (WKPageGetPageZoomFactor(wkPage) == zoomLevel)
        return;

    WKPageSetPageZoomFactor(wkPage, zoomLevel);
    g_object_notify(G_OBJECT(webView), "zoom-level");
}

/**
 * webkit_web_view_get_zoom_level:
 * @web_view: a #WebKitWebView
 *
 * Get the zoom level of @web_view, i.e. the factor by which the
 * view contents are scaled with respect to their original size.
 *
 * Returns: the current zoom level of @web_view
 */
gdouble webkit_web_view_get_zoom_level(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 1);

    WKPageRef wkPage = toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView)));
    return WKPageGetPageZoomFactor(wkPage);
}
