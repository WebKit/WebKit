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

#ifndef WEBKIT_PRIVATE_H
#define WEBKIT_PRIVATE_H

/*
 * Internal class. This class knows the shared secret of WebKitWebFrame,
 * WebKitNetworkRequest and WebKitWebView.
 * They are using WebCore which musn't be exposed to the outer world.
 */

#include "webkitdefines.h"
#include "webkitsettings.h"
#include "webkitwebview.h"
#include "webkitwebframe.h"
#include "webkitnetworkrequest.h"

#include "Settings.h"
#include "Page.h"
#include "Frame.h"
#include "FrameLoaderClient.h"

namespace WebKit {
    void apply(WebKitSettings*,WebCore::Settings*);
    WebKitSettings* create(WebCore::Settings*);
    WebKitWebFrame*  getFrameFromView(WebKitWebView*);
    WebKitWebView* getViewFromFrame(WebKitWebFrame*);

    WebCore::Frame* core(WebKitWebFrame*);
    WebKitWebFrame* kit(WebCore::Frame*);
    WebCore::Page* core(WebKitWebView*);
    WebKitWebView* kit(WebCore::Page*);
}

extern "C" {
    #define WEBKIT_WEB_VIEW_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebViewPrivate))
    typedef struct _WebKitWebViewPrivate WebKitWebViewPrivate;
    struct _WebKitWebViewPrivate {
        WebCore::Page* corePage;
        WebCore::Settings* settings;

        WebKitWebFrame* mainFrame;
        WebCore::String applicationNameForUserAgent;
        WebCore::String* userAgent;

        HashSet<GtkWidget*> children;
        bool editable;
    };

    #define WEBKIT_WEB_FRAME_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_FRAME, WebKitWebFramePrivate))
    typedef struct _WebKitWebFramePrivate WebKitWebFramePrivate;
    struct _WebKitWebFramePrivate {
        WebCore::Frame* frame;
        WebCore::FrameLoaderClient* client;
        WebKitWebView* webView;

        gchar* name;
        gchar* title;
        gchar* location;
    };

    #define WEBKIT_NETWORK_REQUEST_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequestPrivate))
    typedef struct _WebKitNetworkRequestPrivate WebKitNetworkRequestPrivate;
    struct _WebKitNetworkRequestPrivate {
        gchar* uri;
    };

    WebKitWebFrame* webkit_web_frame_init_with_web_view(WebKitWebView*, WebCore::HTMLFrameOwnerElement*);


    // TODO: Move these to webkitwebframe.h once these functions are fully
    // implemented and their API has been discussed.

    WEBKIT_API GSList*
    webkit_web_frame_get_children (WebKitWebFrame* frame);

    WEBKIT_API gchar*
    webkit_web_frame_get_inner_text (WebKitWebFrame* frame);
}

#endif
