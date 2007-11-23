/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "webkitgtkframe.h"
#include "webkitgtkpage.h"
#include "webkitgtkprivate.h"

#include "FrameLoader.h"
#include "FrameLoaderClientGtk.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

#include <JavaScriptCore/APICast.h>

using namespace WebKit;
using namespace WebCore;

extern "C" {

extern void webkit_marshal_VOID__STRING_STRING (GClosure*     closure,
                                                GValue*       return_value,
                                                guint         n_param_values,
                                                const GValue* param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

enum {
    CLEARED,
    LOAD_DONE,
    TITLE_CHANGED,
    HOVERING_OVER_LINK,
    LAST_SIGNAL
};

static guint webkit_frame_signals[LAST_SIGNAL] = { 0, };

static void webkit_frame_real_title_changed(WebKitFrame* frame, gchar* title, gchar* location);

G_DEFINE_TYPE(WebKitFrame, webkit_frame, G_TYPE_OBJECT)

static void webkit_frame_finalize(GObject* object)
{
    WebKitFramePrivate* privateData = WEBKIT_FRAME_GET_PRIVATE(WEBKIT_FRAME(object));
    privateData->frame->loader()->cancelAndClear();
    g_free(privateData->title);
    g_free(privateData->location);
    delete privateData->frame;

    G_OBJECT_CLASS(webkit_frame_parent_class)->finalize(object);
}

static void webkit_frame_class_init(WebKitFrameClass* frameClass)
{
    g_type_class_add_private(frameClass, sizeof(WebKitFramePrivate));

    /*
     * signals
     */
    webkit_frame_signals[CLEARED] = g_signal_new("cleared",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    webkit_frame_signals[LOAD_DONE] = g_signal_new("load_done",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE, 1,
            G_TYPE_BOOLEAN);

    webkit_frame_signals[TITLE_CHANGED] = g_signal_new("title_changed",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitFrameClass, title_changed),
            NULL,
            NULL,
            webkit_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING, G_TYPE_STRING);

    webkit_frame_signals[HOVERING_OVER_LINK] = g_signal_new("hovering_over_link",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING, G_TYPE_STRING);

    frameClass->title_changed = webkit_frame_real_title_changed;

    /*
     * implementations of virtual methods
     */
    G_OBJECT_CLASS(frameClass)->finalize = webkit_frame_finalize;
}

static void webkit_frame_real_title_changed(WebKitFrame* frame, gchar* title, gchar* location)
{
    WebKitFramePrivate* frameData = WEBKIT_FRAME_GET_PRIVATE(frame);
    g_free(frameData->title);
    g_free(frameData->location);
    frameData->title = g_strdup(title);
    frameData->location = g_strdup(location);
}

static void webkit_frame_init(WebKitFrame* frame)
{
}

GObject* webkit_frame_new(WebKitPage* page)
{
    WebKitFrame* frame = WEBKIT_FRAME(g_object_new(WEBKIT_TYPE_FRAME, NULL));
    WebKitFramePrivate* frameData = WEBKIT_FRAME_GET_PRIVATE(frame);
    WebKitPagePrivate* pageData = WEBKIT_PAGE_GET_PRIVATE(page);

    frameData->client = new WebKit::FrameLoaderClient(frame);
    frameData->frame = new Frame(pageData->page, 0, frameData->client);

    FrameView* frameView = new FrameView(frameData->frame);
    frameView->setContainingWindow(GTK_CONTAINER(page));
    frameView->setGtkAdjustments(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)),
                                 GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)));
    frameData->frame->setView(frameView);
    frameData->frame->init();
    frameData->page = page;
    frameData->title = 0;
    frameData->location = 0;

    return G_OBJECT(frame);
}

GObject* webkit_frame_init_with_page(WebKitPage* page, HTMLFrameOwnerElement* element)
{
    WebKitFrame* frame = WEBKIT_FRAME(g_object_new(WEBKIT_TYPE_FRAME, NULL));
    WebKitFramePrivate* frameData = WEBKIT_FRAME_GET_PRIVATE(frame);
    WebKitPagePrivate* pageData = WEBKIT_PAGE_GET_PRIVATE(page);

    frameData->client = new WebKit::FrameLoaderClient(frame);
    frameData->frame = new Frame(pageData->page, element, frameData->client);

    FrameView* frameView = new FrameView(frameData->frame);
    frameView->setContainingWindow(GTK_CONTAINER(page));
    frameData->frame->setView(frameView);
    frameView->deref();
    frameData->frame->init();
    frameData->page = page;

    return G_OBJECT(frame);
}

WebKitPage*
webkit_frame_get_page(WebKitFrame* frame)
{
    WebKitFramePrivate* frameData = WEBKIT_FRAME_GET_PRIVATE(frame);
    return frameData->page;
}

gchar* webkit_frame_get_title(WebKitFrame* frame)
{
    WebKitFramePrivate* frameData = WEBKIT_FRAME_GET_PRIVATE(frame);
    return frameData->title;
}

gchar* webkit_frame_get_location(WebKitFrame* frame)
{
    WebKitFramePrivate* frameData = WEBKIT_FRAME_GET_PRIVATE(frame);
    return frameData->location;
}

/**
 * webkit_frame_get_parent:
 * @frame: a #WebKitFrame
 *
 * Returns the @frame's parent frame, or %NULL if it has none.
 *
 * Return value: the parent #WebKitFrame or %NULL in case there is none
 */
WebKitFrame* webkit_frame_get_parent(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    g_return_val_if_fail(coreFrame, NULL);

    return kit(coreFrame->tree()->parent());
}

/**
 * webkit_frame_load_request:
 * @frame: a #WebKitFrame
 * @request: a #WebKitNetworkRequest
 *
 * Connects to a given URL by initiating an asynchronous client request.
 *
 * Creates a provisional data source that will transition to a committed data
 * source once any data has been received. Use webkit_frame_stop_loading() to
 * stop the load. This function is typically invoked on the main frame.
 */
void webkit_frame_load_request(WebKitFrame* frame, WebKitNetworkRequest* request)
{
    g_return_if_fail(WEBKIT_IS_FRAME(frame));
    g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));

    Frame* coreFrame = core(frame);
    g_return_if_fail(coreFrame);

    // TODO: Use the ResourceRequest carried by WebKitNetworkRequest when it gets implemented.
    DeprecatedString string = DeprecatedString::fromUtf8(webkit_network_request_get_url(request));
    coreFrame->loader()->load(ResourceRequest(KURL(string)));
}

/**
 * webkit_frame_stop_loading:
 * @frame: a #WebKitFrame
 *
 * Stops any pending loads on @frame's data source, and those of its children.
 */
void webkit_frame_stop_loading(WebKitFrame* frame)
{
    g_return_if_fail(WEBKIT_IS_FRAME(frame));

    Frame* coreFrame = core(frame);
    g_return_if_fail(coreFrame);

    coreFrame->loader()->stopAllLoaders();
}

/**
 * webkit_frame_reload:
 * @frame: a #WebKitFrame
 *
 * Reloads the initial request.
 */
void webkit_frame_reload(WebKitFrame* frame)
{
    g_return_if_fail(WEBKIT_IS_FRAME(frame));

    Frame* coreFrame = core(frame);
    g_return_if_fail(coreFrame);

    coreFrame->loader()->reload();
}

/**
 * webkit_frame_find_frame:
 * @frame: a #WebKitFrame
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
 * Return value: the found #WebKitFrame or %NULL in case none is found
 */
WebKitFrame* webkit_frame_find_frame(WebKitFrame* frame, const gchar* name)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), NULL);
    g_return_val_if_fail(name, NULL);

    Frame* coreFrame = core(frame);
    g_return_val_if_fail(coreFrame, NULL);

    String nameString = String::fromUTF8(name);
    return kit(coreFrame->tree()->find(AtomicString(nameString)));
}

/**
 * webkit_frame_get_global_context:
 * @frame: a #WebKitFrame
 *
 * Gets the global JavaScript execution context. Use this function to bridge
 * between the WebKit and JavaScriptCore APIs.
 *
 * Return value: the global JavaScript context
 */
JSGlobalContextRef webkit_frame_get_global_context(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), NULL);

    Frame* coreFrame = core(frame);
    g_return_val_if_fail(coreFrame, NULL);

    return toGlobalRef(coreFrame->scriptProxy()->interpreter()->globalExec());
}

}
