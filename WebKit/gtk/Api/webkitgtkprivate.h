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
 * Internal class. This class knows the shared secret of WebKitFrame,
 * WebKitNetworkRequest and WebKitPage.
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
    void apply(WebKitSettings*,WebCore::Settings*);
    WebKitSettings* create(WebCore::Settings*);
    WebKitFrame*  getFrameFromPage(WebKitPage*);
    WebKitPage* getPageFromFrame(WebKitFrame*);

    WebCore::Frame* core(WebKitFrame*);
    WebKitFrame* kit(WebCore::Frame*);
    WebCore::Page* core(WebKitPage*);
    WebKitPage* kit(WebCore::Page*);
}

extern "C" {
    #define WEBKIT_PAGE_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_PAGE, WebKitPagePrivate))
    typedef struct _WebKitPagePrivate WebKitPagePrivate;
    struct _WebKitPagePrivate {
        WebCore::Page* page;
        WebCore::Settings* settings;

        WebKitFrame* mainFrame;
        WebCore::String applicationNameForUserAgent;
        WebCore::String* userAgent;

        HashSet<GtkWidget*> children;
    };
    
    #define WEBKIT_FRAME_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_FRAME, WebKitFramePrivate))
    typedef struct _WebKitFramePrivate WebKitFramePrivate;
    struct _WebKitFramePrivate {
        WebCore::Frame* frame;
        WebCore::FrameLoaderClient* client;
        WebKitPage* page;

        gchar* title;
        gchar* location;
    };


    GObject* webkit_frame_init_with_page(WebKitPage*, WebCore::HTMLFrameOwnerElement*);
}

#endif
