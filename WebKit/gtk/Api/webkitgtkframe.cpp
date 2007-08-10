/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
#include "FrameLoaderClientGdk.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"

using namespace WebCore;

extern "C" {

extern void webkit_gtk_marshal_VOID__STRING_STRING (GClosure*     closure,
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

static guint webkit_gtk_frame_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitGtkFrame, webkit_gtk_frame, G_TYPE_OBJECT)

static void webkit_gtk_frame_finalize(GObject* object)
{
    WebKitGtkFramePrivate* privateData = WEBKIT_GTK_FRAME_GET_PRIVATE(WEBKIT_GTK_FRAME(object));
    privateData->frame->loader()->cancelAndClear();
    delete privateData->frame;

    G_OBJECT_CLASS(webkit_gtk_frame_parent_class)->finalize(object);
}

static void webkit_gtk_frame_class_init(WebKitGtkFrameClass* frameClass)
{
    g_type_class_add_private(frameClass, sizeof(WebKitGtkFramePrivate));

    /*
     * signals
     */
    webkit_gtk_frame_signals[CLEARED] = g_signal_new("cleared",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    webkit_gtk_frame_signals[LOAD_DONE] = g_signal_new("load_done",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE, 1,
            G_TYPE_BOOLEAN);

    webkit_gtk_frame_signals[TITLE_CHANGED] = g_signal_new("title_changed",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_gtk_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING, G_TYPE_STRING);

    webkit_gtk_frame_signals[HOVERING_OVER_LINK] = g_signal_new("hovering_over_link",
            G_TYPE_FROM_CLASS(frameClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_gtk_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING, G_TYPE_STRING);

    /*
     * implementations of virtual methods
     */
    G_OBJECT_CLASS(frameClass)->finalize = webkit_gtk_frame_finalize;
}

static void webkit_gtk_frame_init(WebKitGtkFrame* frame)
{
}

GObject* webkit_gtk_frame_new(WebKitGtkPage* page)
{
    WebKitGtkFrame* frame = WEBKIT_GTK_FRAME(g_object_new(WEBKIT_GTK_TYPE_FRAME, NULL));
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(frame);
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);

    frameData->client = new FrameLoaderClientGdk(frame);
    frameData->frame = new Frame(pageData->page, 0, frameData->client);

    FrameView* frameView = new FrameView(frameData->frame);
    frameView->setContainingWindow(GTK_LAYOUT(page));
    frameView->setGtkAdjustments(gtk_layout_get_hadjustment(GTK_LAYOUT(page)),
                                 gtk_layout_get_vadjustment(GTK_LAYOUT(page)));
    frameData->frame->setView(frameView);
    frameData->frame->init();
    frameData->page = page;

    return G_OBJECT(frame);
}

GObject* webkit_gtk_frame_init_with_page(WebKitGtkPage* page, HTMLFrameOwnerElement* element)
{
    WebKitGtkFrame* frame = WEBKIT_GTK_FRAME(g_object_new(WEBKIT_GTK_TYPE_FRAME, NULL));
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(frame);
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);

    frameData->client = new FrameLoaderClientGdk(frame);
    frameData->frame = new Frame(pageData->page, element, frameData->client);

    FrameView* frameView = new FrameView(frameData->frame);
    frameView->setContainingWindow(GTK_LAYOUT(page));
    frameData->frame->setView(frameView);
    frameView->deref();
    frameData->frame->init();
    frameData->page = page;

    return G_OBJECT(frame);
} 

WebKitGtkPage*
webkit_gtk_frame_get_page(WebKitGtkFrame* frame)
{
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(frame);
    return frameData->page;
}
}
