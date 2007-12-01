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

#ifndef WEBKIT_WEB_FRAME_H
#define WEBKIT_WEB_FRAME_H

#include <glib-object.h>
#include <gdk/gdk.h>

#include "webkitdefines.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_FRAME            (webkit_web_frame_get_type())
#define WEBKIT_WEB_FRAME(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_FRAME, WebKitWebFrame))
#define WEBKIT_WEB_FRAME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_FRAME, WebKitWebFrameClass))
#define WEBKIT_IS_WEB_FRAME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_FRAME))
#define WEBKIT_IS_WEB_FRAME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_FRAME))
#define WEBKIT_WEB_FRAME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_FRAME, WebKitWebFrameClass))


struct _WebKitWebFrame {
    GObject parent;
};

struct _WebKitWebFrameClass {
    GObjectClass parent;

    void (*title_changed) (WebKitWebFrame* frame, gchar* title, gchar* location);

    /*
     * protected virtual methods
     */
    void (*mouse_move_event)  (WebKitWebFrame* frame, GdkEvent* move_event);
    void (*mouse_press_event) (WebKitWebFrame* frame, GdkEvent* press_event);
    void (*mouse_release_event) (WebKitWebFrame* frame, GdkEvent* mouse_release_event);
    void (*mouse_double_click_event) (WebKitWebFrame* frame, GdkEvent* double_click_event);
    void (*mouse_wheel_event) (WebKitWebFrame* frame, GdkEvent* wheel_event);
};

WEBKIT_API GType
webkit_web_frame_get_type (void);

WEBKIT_API WebKitWebFrame*
webkit_web_frame_new (WebKitWebView* web_view);

WEBKIT_API WebKitWebView*
webkit_web_frame_get_web_view (WebKitWebFrame* frame);

WEBKIT_API const gchar*
webkit_web_frame_get_name (WebKitWebFrame* frame);

WEBKIT_API const gchar*
webkit_web_frame_get_title (WebKitWebFrame* frame);

WEBKIT_API const gchar*
webkit_web_frame_get_location (WebKitWebFrame* frame);

WEBKIT_API WebKitWebFrame*
webkit_web_frame_get_parent (WebKitWebFrame* frame);

WEBKIT_API void
webkit_web_frame_load_request (WebKitWebFrame* frame, WebKitNetworkRequest* request);

WEBKIT_API void
webkit_web_frame_stop_loading (WebKitWebFrame* frame);

WEBKIT_API void
webkit_web_frame_reload (WebKitWebFrame* frame);

WEBKIT_API WebKitWebFrame*
webkit_web_frame_find_frame (WebKitWebFrame* frame, const gchar* name);

WEBKIT_API JSGlobalContextRef
webkit_web_frame_get_global_context (WebKitWebFrame* frame);

G_END_DECLS

#endif
