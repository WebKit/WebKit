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

#ifndef WEBKIT_GTK_PAGE_H
#define WEBKIT_GTK_PAGE_H

#include <gtk/gtk.h>
#include "webkitgtksettings.h"

G_BEGIN_DECLS

#define WEBKIT_GTK_TYPE_PAGE            (webkit_gtk_page_get_type())
#define WEBKIT_GTK_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_GTK_TYPE_PAGE, WebKitGtkPage))
#define WEBKIT_GTK_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_GTK_TYPE_PAGE, WebKitGtkPageClass))
#define WEBKIT_GTK_IS_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_GTK_TYPE_PAGE))
#define WEBKIT_GTK_IS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_GTK_TYPE_PAGE))
#define WEBKIT_GTK_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_GTK_TYPE_PAGE, WebKitGtkPageClass))


typedef enum {
    WEBKIT_GTK_ACCEPT_NAVIGATION_REQUEST,
    WEBKIT_GTK_IGNORE_NAVIGATION_REQUEST,
    WEBKIT_GTK_DOWNLOAD_NAVIGATION_REQUEST
} WEBKIT_GTK_NAVIGATION_REQUEST_RESPONSE;


typedef struct _WebKitGtkFrame WebKitGtkFrame;
typedef struct _WebKitGtkFrameData WebKitGtkFrameData;
typedef struct _WebKitGtkNetworkRequest WebKitGtkNetworkRequest;
typedef struct _WebKitGtkPage WebKitGtkPage;
typedef struct _WebKitGtkPageClass WebKitGtkPageClass;

/*
 * FIXME: Will be different with multiple frames. Once we support multiple Frames
 * this widget will not be a GtkLayout but we will just allocate a GdkWindow of
 * the  size of the viewport and we won't be able to use gdk_window_move.
 */
struct _WebKitGtkPage {
    GtkLayout parent;
};

struct _WebKitGtkPageClass {
    GtkLayoutClass parent;

    /*
     * default handler/virtual methods
     * DISCUSS: create_page needs a request and should we make this a signal with default handler? this would
     * require someone doing a g_signal_stop_emission_by_name
     * WebUIDelegate has nothing for create_frame, WebPolicyDelegate as well...
     */
    WebKitGtkFrame* (*create_frame) (WebKitGtkPage* page, WebKitGtkFrame* parent_frame, WebKitGtkFrameData* frame_data);
    WebKitGtkPage*  (*create_page)  (WebKitGtkPage* page);

    /*
     * TODO: FIXME: Create something like WebPolicyDecisionListener_Protocol instead
     */
    WEBKIT_GTK_NAVIGATION_REQUEST_RESPONSE (*navigation_requested) (WebKitGtkPage* page, WebKitGtkFrame* frame, WebKitGtkNetworkRequest* request);

    /*
     * TODO: DISCUSS: Should these be signals as well?
     */
    gchar* (*choose_file) (WebKitGtkPage* page, WebKitGtkFrame* frame, const gchar* old_file);
    void   (*java_script_alert) (WebKitGtkPage* page, WebKitGtkFrame* frame, const gchar* alert_message);
    gboolean   (*java_script_confirm) (WebKitGtkPage* page, WebKitGtkFrame* frame, const gchar* confirm_message);
    gchar* (*java_script_prompt) (WebKitGtkPage* page, WebKitGtkFrame* frame, const gchar* message, const gchar* default_value);
    void   (*java_script_console_message) (WebKitGtkPage*, const gchar* message, unsigned int line_number, const gchar* source_id);
};

WEBKIT_GTK_API GType
webkit_gtk_page_get_type (void);

WEBKIT_GTK_API GtkWidget*
webkit_gtk_page_new (void);

WEBKIT_GTK_API void
webkit_gtk_page_set_settings (WebKitGtkPage* page, WebKitGtkSettings* settings);

WEBKIT_GTK_API WebKitGtkSettings*
webkit_gtk_page_get_settings (WebKitGtkPage* page);

WEBKIT_GTK_API gboolean
webkit_gtk_page_can_cut (WebKitGtkPage* page);

WEBKIT_GTK_API gboolean
webkit_gtk_page_can_copy (WebKitGtkPage* page);

WEBKIT_GTK_API gboolean
webkit_gtk_page_can_copy (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_cut (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_copy (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_paste (WebKitGtkPage* page);

WEBKIT_GTK_API gboolean
webkit_gtk_page_can_go_backward (WebKitGtkPage* page);

WEBKIT_GTK_API gboolean
webkit_gtk_page_can_go_forward (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_go_backward (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_go_forward (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_stop_loading (WebKitGtkPage* page);

WEBKIT_GTK_API void
webkit_gtk_page_open (WebKitGtkPage* page, const gchar* url);

WEBKIT_GTK_API void
webkit_gtk_page_reload (WebKitGtkPage *page);

WEBKIT_GTK_API void
webkit_gtk_page_load_string (WebKitGtkPage* page, const gchar* content, const gchar* content_mime_type, const gchar* content_encoding, const gchar* base_url);

WEBKIT_GTK_API void
webkit_gtk_page_load_html_string (WebKitGtkPage* page, const gchar* content, const gchar* base_url);

WEBKIT_GTK_API WebKitGtkFrame*
webkit_gtk_page_get_main_frame (WebKitGtkPage* page);

G_END_DECLS

#endif
