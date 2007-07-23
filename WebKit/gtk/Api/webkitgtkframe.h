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

#ifndef WEBKIT_GTK_FRAME_H
#define WEBKIT_GTK_FRAME_H

#include <glib-object.h>
#include <gdk/gdk.h>

#include "webkitgtkdefines.h"

G_BEGIN_DECLS

#define WEBKIT_GTK_TYPE_FRAME            (webkit_gtk_frame_get_type())
#define WEBKIT_GTK_FRAME(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_GTK_TYPE_FRAME, WebKitGtkFrame))
#define WEBKIT_GTK_FRAME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_GTK_TYPE_FRAME, WebKitGtkFrameClass))
#define WEBKIT_GTK_IS_FRAME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_GTK_TYPE_FRAME))
#define WEBKIT_GTK_IS_FRAME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_GTK_TYPE_FRAME))
#define WEBKIT_GTK_FRAME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_GTK_TYPE_FRAME, WebKitGtkFrameClass))

typedef struct _WebKitGtkPage  WebKitGtkPage;
typedef struct _WebKitGtkFrame WebKitGtkFrame;
typedef struct _WebKitGtkFrameClass WebKitGtkFrameClass;

struct _WebKitGtkFrame {
    GObject parent;
};

struct _WebKitGtkFrameClass {
    GObjectClass parent;

    /*
     * protected virtual methods
     */
    void (*mouse_move_event)  (WebKitGtkFrame* frame, GdkEvent* move_event);
    void (*mouse_press_event) (WebKitGtkFrame* frame, GdkEvent* press_event);
    void (*mouse_release_event) (WebKitGtkFrame* frame, GdkEvent* mouse_release_event);
    void (*mouse_double_click_event) (WebKitGtkFrame* frame, GdkEvent* double_click_event);
    void (*mouse_wheel_event) (WebKitGtkFrame* frame, GdkEvent* wheel_event);
};

WEBKIT_GTK_API GType
webkit_gtk_frame_get_type (void);

WEBKIT_GTK_API GObject*
webkit_gtk_frame_new (WebKitGtkPage *page);

WEBKIT_GTK_API WebKitGtkPage*
webkit_gtk_frame_get_page (WebKitGtkFrame* frame);

WEBKIT_GTK_API gchar*
webkit_gtk_frame_get_markup (WebKitGtkFrame* frame);

WEBKIT_GTK_API gchar*
webkit_gtk_frame_get_inner_text (WebKitGtkFrame* frame);

WEBKIT_GTK_API gchar*
webkit_gtk_frame_get_selected_text (WebKitGtkFrame* frame);

WEBKIT_GTK_API gchar*
webkit_gtk_frame_get_title (WebKitGtkFrame* frame);

WEBKIT_GTK_API GSList*
webkit_gtk_frame_get_child_frames (WebKitGtkFrame* frame);

WEBKIT_GTK_API GdkPoint*
webkit_gtk_frame_get_position (WebKitGtkFrame* frame);

WEBKIT_GTK_API GdkRectangle*
webkit_gtk_frame_get_rectangle (WebKitGtkFrame* frame);

WEBKIT_GTK_API WebKitGtkPage*
webkit_gtk_frame_get_page (WebKitGtkFrame* frame);

G_END_DECLS

#endif
