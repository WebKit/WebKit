/*
 * Copyright (C) 2007, 2008 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nuanti Ltd.
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

#include "webkitwebframe.h"
#include "webkitwebview.h"
#include "webkitmarshal.h"
#include "webkitprivate.h"

#include "AnimationController.h"
#include "CString.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGtk.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "JSDOMWindow.h"
#include "PrintContext.h"
#include "RenderView.h"
#include "RenderTreeAsText.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "SubstituteData.h"

#include <JavaScriptCore/APICast.h>

/**
 * SECTION:webkitwebframe
 * @short_description: The content of a #WebKitWebView
 *
 * A #WebKitWebView contains a main #WebKitWebFrame. A #WebKitWebFrame
 * contains the content of one URI. The URI and name of the frame can
 * be retrieved, the load status and progress can be observed using the
 * signals and can be controlled using the methods of the #WebKitWebFrame.
 * A #WebKitWebFrame can have any number of children and one child can
 * be found by using #webkit_web_frame_find_frame.
 *
 * <informalexample><programlisting>
 * /<!-- -->* Get the frame from the #WebKitWebView *<!-- -->/
 * WebKitWebFrame *frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(my_view));
 * g_print("The URI of this frame is '%s'", webkit_web_frame_get_uri (frame));
 * </programlisting></informalexample>
 */

using namespace WebKit;
using namespace WebCore;
using namespace std;

extern "C" {

enum {
    CLEARED,
    LOAD_COMMITTED,
    LOAD_DONE,
    TITLE_CHANGED,
    HOVERING_OVER_LINK,
    LAST_SIGNAL
};

enum {
    PROP_0,

    PROP_NAME,
    PROP_TITLE,
    PROP_URI
};

static guint webkit_web_frame_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitWebFrame, webkit_web_frame, G_TYPE_OBJECT)

static void webkit_web_frame_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    WebKitWebFrame* frame = WEBKIT_WEB_FRAME(object);

    switch(prop_id) {
    case PROP_NAME:
        g_value_set_string(value, webkit_web_frame_get_name(frame));
        break;
    case PROP_TITLE:
        g_value_set_string(value, webkit_web_frame_get_title(frame));
        break;
    case PROP_URI:
        g_value_set_string(value, webkit_web_frame_get_uri(frame));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Called from the FrameLoaderClient when it is destroyed. Normally
// the unref in the FrameLoaderClient is destroying this object as
// well but due reference counting a user might have added a reference...
void webkit_web_frame_core_frame_gone(WebKitWebFrame* frame)
{
    ASSERT(WEBKIT_IS_WEB_FRAME(frame));
    frame->priv->coreFrame = 0;
}

static void webkit_web_frame_finalize(GObject* object)
{
    WebKitWebFrame* frame = WEBKIT_WEB_FRAME(object);
    WebKitWebFramePrivate* priv = frame->priv;

    if (priv->coreFrame) {
        priv->coreFrame->loader()->cancelAndClear();
        priv->coreFrame = 0;
    }

    g_free(priv->name);
    g_free(priv->title);
    g_free(priv->uri);

    G_OBJECT_CLASS(webkit_web_frame_parent_class)->finalize(object);
}

static void webkit_web_frame_class_init(WebKitWebFrameClass* frameClass)
{
    webkit_init();

    /*
     * signals
     */
    webkit_web_frame_signals[CLEARED] = g_signal_new("cleared",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    webkit_web_frame_signals[LOAD_COMMITTED] = g_signal_new("load-committed",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    webkit_web_frame_signals[LOAD_DONE] = g_signal_new("load-done",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE, 1,
            G_TYPE_BOOLEAN);

    webkit_web_frame_signals[TITLE_CHANGED] = g_signal_new("title-changed",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_marshal_VOID__STRING,
            G_TYPE_NONE, 1,
            G_TYPE_STRING);

    webkit_web_frame_signals[HOVERING_OVER_LINK] = g_signal_new("hovering-over-link",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING, G_TYPE_STRING);

    /*
     * implementations of virtual methods
     */
    GObjectClass* objectClass = G_OBJECT_CLASS(frameClass);
    objectClass->finalize = webkit_web_frame_finalize;
    objectClass->get_property = webkit_web_frame_get_property;

    /*
     * properties
     */
    g_object_class_install_property(objectClass, PROP_NAME,
                                    g_param_spec_string("name",
                                                        "Name",
                                                        "The name of the frame",
                                                        NULL,
                                                        WEBKIT_PARAM_READABLE));

    g_object_class_install_property(objectClass, PROP_TITLE,
                                    g_param_spec_string("title",
                                                        "Title",
                                                        "The document title of the frame",
                                                        NULL,
                                                        WEBKIT_PARAM_READABLE));

    g_object_class_install_property(objectClass, PROP_URI,
                                    g_param_spec_string("uri",
                                                        "URI",
                                                        "The current URI of the contents displayed by the frame",
                                                        NULL,
                                                        WEBKIT_PARAM_READABLE));

    g_type_class_add_private(frameClass, sizeof(WebKitWebFramePrivate));
}

static void webkit_web_frame_init(WebKitWebFrame* frame)
{
    WebKitWebFramePrivate* priv = WEBKIT_WEB_FRAME_GET_PRIVATE(frame);

    // TODO: Move constructor code here.
    frame->priv = priv;
}

/**
 * webkit_web_frame_new:
 * @web_view: the controlling #WebKitWebView
 *
 * Creates a new #WebKitWebFrame initialized with a controlling #WebKitWebView.
 *
 * Returns: a new #WebKitWebFrame
 *
 * Deprecated: 1.0.2: #WebKitWebFrame can only be used to inspect existing
 * frames.
 **/
WebKitWebFrame* webkit_web_frame_new(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebFrame* frame = WEBKIT_WEB_FRAME(g_object_new(WEBKIT_TYPE_WEB_FRAME, NULL));
    WebKitWebFramePrivate* priv = frame->priv;
    WebKitWebViewPrivate* viewPriv = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);

    priv->webView = webView;
    WebKit::FrameLoaderClient* client = new WebKit::FrameLoaderClient(frame);
    priv->coreFrame = Frame::create(viewPriv->corePage, 0, client).get();
    priv->coreFrame->init();

    return frame;
}

PassRefPtr<Frame> webkit_web_frame_init_with_web_view(WebKitWebView* webView, HTMLFrameOwnerElement* element)
{
    WebKitWebFrame* frame = WEBKIT_WEB_FRAME(g_object_new(WEBKIT_TYPE_WEB_FRAME, NULL));
    WebKitWebFramePrivate* priv = frame->priv;
    WebKitWebViewPrivate* viewPriv = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);

    priv->webView = webView;
    WebKit::FrameLoaderClient* client = new WebKit::FrameLoaderClient(frame);

    RefPtr<Frame> coreFrame = Frame::create(viewPriv->corePage, element, client);
    priv->coreFrame = coreFrame.get();

    return coreFrame.release();
}

/**
 * webkit_web_frame_get_title:
 * @frame: a #WebKitWebFrame
 *
 * Returns the @frame's document title
 *
 * Return value: the title of @frame
 */
G_CONST_RETURN gchar* webkit_web_frame_get_title(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    WebKitWebFramePrivate* priv = frame->priv;
    return priv->title;
}

/**
 * webkit_web_frame_get_uri:
 * @frame: a #WebKitWebFrame
 *
 * Returns the current URI of the contents displayed by the @frame
 *
 * Return value: the URI of @frame
 */
G_CONST_RETURN gchar* webkit_web_frame_get_uri(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    WebKitWebFramePrivate* priv = frame->priv;
    return priv->uri;
}

/**
 * webkit_web_frame_get_web_view:
 * @frame: a #WebKitWebFrame
 *
 * Returns the #WebKitWebView that manages this #WebKitWebFrame.
 *
 * The #WebKitWebView returned manages the entire hierarchy of #WebKitWebFrame
 * objects that contains @frame.
 *
 * Return value: the #WebKitWebView that manages @frame
 */
WebKitWebView* webkit_web_frame_get_web_view(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    WebKitWebFramePrivate* priv = frame->priv;
    return priv->webView;
}

/**
 * webkit_web_frame_get_name:
 * @frame: a #WebKitWebFrame
 *
 * Returns the @frame's name
 *
 * Return value: the name of @frame
 */
G_CONST_RETURN gchar* webkit_web_frame_get_name(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    WebKitWebFramePrivate* priv = frame->priv;

    if (priv->name)
        return priv->name;

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return "";

    String string = coreFrame->tree()->name();
    priv->name = g_strdup(string.utf8().data());
    return priv->name;
}

/**
 * webkit_web_frame_get_parent:
 * @frame: a #WebKitWebFrame
 *
 * Returns the @frame's parent frame, or %NULL if it has none.
 *
 * Return value: the parent #WebKitWebFrame or %NULL in case there is none
 */
WebKitWebFrame* webkit_web_frame_get_parent(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return NULL;

    return kit(coreFrame->tree()->parent());
}

/**
 * webkit_web_frame_load_uri:
 * @frame: a #WebKitWebFrame
 * @uri: an URI string
 *
 * Requests loading of the specified URI string.
 *
 * Since: 1.1.1
 */
void webkit_web_frame_load_uri(WebKitWebFrame* frame, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));
    g_return_if_fail(uri);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    coreFrame->loader()->load(ResourceRequest(KURL(KURL(), String::fromUTF8(uri))), false);
}

/**
 * webkit_web_frame_load_string:
 * @frame: a #WebKitWebFrame
 * @content: an URI string
 * @mime_type: the MIME type, or %NULL
 * @encoding: the encoding, or %NULL
 * @base_uri: the base URI for relative locations
 *
 * Requests loading of the given @content with the specified @mime_type,
 * @encoding and @base_uri.
 *
 * If @mime_type is %NULL, "text/html" is assumed.
 *
 * If @encoding is %NULL, "UTF-8" is assumed.
 *
 * Since: 1.1.1
 */
void webkit_web_frame_load_string(WebKitWebFrame* frame, const gchar* content, const gchar* contentMimeType, const gchar* contentEncoding, const gchar* baseUri)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));
    g_return_if_fail(content);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    KURL url(KURL(), baseUri ? String::fromUTF8(baseUri) : "");
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(content, strlen(content));
    SubstituteData substituteData(sharedBuffer.release(), contentMimeType ? String(contentMimeType) : "text/html", contentEncoding ? String(contentEncoding) : "UTF-8", blankURL(), url);

    coreFrame->loader()->load(ResourceRequest(url), substituteData, false);
}

/**
 * webkit_web_frame_load_request:
 * @frame: a #WebKitWebFrame
 * @request: a #WebKitNetworkRequest
 *
 * Connects to a given URI by initiating an asynchronous client request.
 *
 * Creates a provisional data source that will transition to a committed data
 * source once any data has been received. Use webkit_web_frame_stop_loading() to
 * stop the load. This function is typically invoked on the main frame.
 */
void webkit_web_frame_load_request(WebKitWebFrame* frame, WebKitNetworkRequest* request)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));
    g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    // TODO: Use the ResourceRequest carried by WebKitNetworkRequest when it is implemented.
    String string = String::fromUTF8(webkit_network_request_get_uri(request));
    coreFrame->loader()->load(ResourceRequest(KURL(KURL(), string)), false);
}

/**
 * webkit_web_frame_stop_loading:
 * @frame: a #WebKitWebFrame
 *
 * Stops any pending loads on @frame's data source, and those of its children.
 */
void webkit_web_frame_stop_loading(WebKitWebFrame* frame)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    coreFrame->loader()->stopAllLoaders();
}

/**
 * webkit_web_frame_reload:
 * @frame: a #WebKitWebFrame
 *
 * Reloads the initial request.
 */
void webkit_web_frame_reload(WebKitWebFrame* frame)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    coreFrame->loader()->reload();
}

/**
 * webkit_web_frame_find_frame:
 * @frame: a #WebKitWebFrame
 * @name: the name of the frame to be found
 *
 * For pre-defined names, returns @frame if @name is "_self" or "_current",
 * returns @frame's parent frame if @name is "_parent", and returns the main
 * frame if @name is "_top". Also returns @frame if it is the main frame and
 * @name is either "_parent" or "_top". For other names, this function returns
 * the first frame that matches @name. This function searches @frame and its
 * descendents first, then @frame's parent and its children moving up the
 * hierarchy until a match is found. If no match is found in @frame's
 * hierarchy, this function will search for a matching frame in other main
 * frame hierarchies. Returns %NULL if no match is found.
 *
 * Return value: the found #WebKitWebFrame or %NULL in case none is found
 */
WebKitWebFrame* webkit_web_frame_find_frame(WebKitWebFrame* frame, const gchar* name)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);
    g_return_val_if_fail(name, NULL);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return NULL;

    String nameString = String::fromUTF8(name);
    return kit(coreFrame->tree()->find(AtomicString(nameString)));
}

/**
 * webkit_web_frame_get_global_context:
 * @frame: a #WebKitWebFrame
 *
 * Gets the global JavaScript execution context. Use this function to bridge
 * between the WebKit and JavaScriptCore APIs.
 *
 * Return value: the global JavaScript context
 */
JSGlobalContextRef webkit_web_frame_get_global_context(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return NULL;

    return toGlobalRef(coreFrame->script()->globalObject()->globalExec());
}

/**
 * webkit_web_frame_get_children:
 * @frame: a #WebKitWebFrame
 *
 * Return value: child frames of @frame
 */
GSList* webkit_web_frame_get_children(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return NULL;

    GSList* children = NULL;
    for (Frame* child = coreFrame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        FrameLoader* loader = child->loader();
        WebKit::FrameLoaderClient* client = static_cast<WebKit::FrameLoaderClient*>(loader->client());
        if (client)
          children = g_slist_append(children, client->webFrame());
    }

    return children;
}

/**
 * webkit_web_frame_get_inner_text:
 * @frame: a #WebKitWebFrame
 *
 * Return value: inner text of @frame
 */
gchar* webkit_web_frame_get_inner_text(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return g_strdup("");

    FrameView* view = coreFrame->view();

    if (view && view->layoutPending())
        view->layout();

    Element* documentElement = coreFrame->document()->documentElement();
    String string =  documentElement->innerText();
    return g_strdup(string.utf8().data());
}

/**
 * webkit_web_frame_dump_render_tree:
 * @frame: a #WebKitWebFrame
 *
 * Return value: Non-recursive render tree dump of @frame
 */
gchar* webkit_web_frame_dump_render_tree(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return g_strdup("");

    FrameView* view = coreFrame->view();

    if (view && view->layoutPending())
        view->layout();

    String string = externalRepresentation(coreFrame->contentRenderer());
    return g_strdup(string.utf8().data());
}

#if GTK_CHECK_VERSION(2,10,0)

static void begin_print(GtkPrintOperation* op, GtkPrintContext* context, gpointer user_data)
{
    PrintContext* printContext = reinterpret_cast<PrintContext*>(user_data);

    float width = gtk_print_context_get_width(context);
    float height = gtk_print_context_get_height(context);
    FloatRect printRect = FloatRect(0, 0, width, height);

    printContext->begin(width);

    // TODO: Margin adjustments and header/footer support
    float headerHeight = 0;
    float footerHeight = 0;
    float pageHeight; // height of the page adjusted by margins
    printContext->computePageRects(printRect, headerHeight, footerHeight, 1.0, pageHeight);
    gtk_print_operation_set_n_pages(op, printContext->pageCount());
}

static void draw_page(GtkPrintOperation* op, GtkPrintContext* context, gint page_nr, gpointer user_data)
{
    PrintContext* printContext = reinterpret_cast<PrintContext*>(user_data);

    cairo_t* cr = gtk_print_context_get_cairo_context(context);
    GraphicsContext ctx(cr);
    float width = gtk_print_context_get_width(context);
    printContext->spoolPage(ctx, page_nr, width);
}

static void end_print(GtkPrintOperation* op, GtkPrintContext* context, gpointer user_data)
{
    PrintContext* printContext = reinterpret_cast<PrintContext*>(user_data);
    printContext->end();
}

void webkit_web_frame_print(WebKitWebFrame* frame)
{
    GtkWidget* topLevel = gtk_widget_get_toplevel(GTK_WIDGET(webkit_web_frame_get_web_view(frame)));
    if (!GTK_WIDGET_TOPLEVEL(topLevel))
        topLevel = NULL;

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    PrintContext printContext(coreFrame);

    GtkPrintOperation* op = gtk_print_operation_new();
    g_signal_connect(op, "begin-print", G_CALLBACK(begin_print), &printContext);
    g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), &printContext);
    g_signal_connect(op, "end-print", G_CALLBACK(end_print), &printContext);
    GError *error = NULL;
    gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(topLevel), &error);
    g_object_unref(op);

    if (error) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(topLevel),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "%s", error->message);
        g_error_free(error);

        g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
        gtk_widget_show(dialog);
    }
}

#else

void webkit_web_frame_print(WebKitWebFrame*)
{
    g_warning("Printing support is not available in older versions of GTK+");
}

#endif

bool webkit_web_frame_pause_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element)
{
    ASSERT(core(frame));
    Element* coreElement = core(frame)->document()->getElementById(AtomicString(element));
    if (!coreElement || !coreElement->renderer())
        return false;
    return core(frame)->animation()->pauseAnimationAtTime(coreElement->renderer(), AtomicString(name), time);
}

bool webkit_web_frame_pause_transition(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element)
{
    ASSERT(core(frame));
    Element* coreElement = core(frame)->document()->getElementById(AtomicString(element));
    if (!coreElement || !coreElement->renderer())
        return false;
    return core(frame)->animation()->pauseTransitionAtTime(coreElement->renderer(), AtomicString(name), time);
}

unsigned int webkit_web_frame_number_of_active_animations(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return 0;

    AnimationController* controller = coreFrame->animation();
    if (!controller)
        return 0;

    return controller->numberOfActiveAnimations();
}

}
