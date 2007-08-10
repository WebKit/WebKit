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

#ifndef WEBKIT_GTK_PRIVATE_H
#define WEBKIT_GTK_PRIVATE_H

/*
 * Internal class. This class knows the shared secret of WebKitGtkFrame,
 * WebKitGtkNetworkRequest and WebKitGtkPage.
 * They are using WebCore which musn't be exposed to the outer world.
 */

#include "webkitgtksettings.h"
#include "webkitgtkpage.h"
#include "webkitgtkframe.h"
#include "webkitgtknetworkrequest.h"


#include "Settings.h"
#include "Page.h"
#include "Frame.h"
#include "FrameLoaderClient.h"

namespace WebKit {
    void apply(WebKitGtkSettings*,WebCore::Settings*);
    WebKitGtkSettings* create(WebCore::Settings*);
    WebKitGtkFrame*  getFrameFromPage(WebKitGtkPage*);
    WebKitGtkPage* getPageFromFrame(WebKitGtkFrame*);

    WebCore::Frame* core(WebKitGtkFrame*);
    WebKitGtkFrame* kit(WebCore::Frame*);
    WebCore::Page* core(WebKitGtkPage*);
    WebKitGtkPage* kit(WebCore::Page*);
}

extern "C" {
    #define WEBKIT_GTK_PAGE_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_GTK_TYPE_PAGE, WebKitGtkPagePrivate))
    typedef struct _WebKitGtkPagePrivate WebKitGtkPagePrivate;
    struct _WebKitGtkPagePrivate {
        WebCore::Page* page;
        WebCore::Settings* settings;

        WebKitGtkFrame* mainFrame;
        WebCore::String applicationNameForUserAgent;
        WebCore::String* userAgent;

        HashSet<GtkWidget*> children;
    };
    
    #define WEBKIT_GTK_FRAME_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_GTK_TYPE_FRAME, WebKitGtkFramePrivate))
    typedef struct _WebKitGtkFramePrivate WebKitGtkFramePrivate;
    struct _WebKitGtkFramePrivate {
        WebCore::Frame* frame;
        WebCore::FrameLoaderClient* client;
        WebKitGtkPage* page;
    };


    GObject* webkit_gtk_frame_init_with_page(WebKitGtkPage*, WebCore::HTMLFrameOwnerElement*);
}

#endif
