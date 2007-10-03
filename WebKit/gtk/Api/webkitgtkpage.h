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

#ifndef WEBKIT_PAGE_H
#define WEBKIT_PAGE_H

#include <gtk/gtk.h>

#include "webkitgtkdefines.h"
#include "webkitgtksettings.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_PAGE            (webkit_page_get_type())
#define WEBKIT_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_PAGE, WebKitPage))
#define WEBKIT_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_PAGE, WebKitPageClass))
#define WEBKIT_IS_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_PAGE))
#define WEBKIT_IS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_PAGE))
#define WEBKIT_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_PAGE, WebKitPageClass))


typedef enum {
    WEBKIT_ACCEPT_NAVIGATION_REQUEST,
    WEBKIT_IGNORE_NAVIGATION_REQUEST,
    WEBKIT_DOWNLOAD_NAVIGATION_REQUEST
} WEBKIT_NAVIGATION_REQUEST_RESPONSE;



struct _WebKitPage {
    GtkContainer parent;
};

struct _WebKitPageClass {
    GtkContainerClass parent;

    /*
     * default handler/virtual methods
     * DISCUSS: create_page needs a request and should we make this a signal with default handler? this would
     * require someone doing a g_signal_stop_emission_by_name
     * WebUIDelegate has nothing for create_frame, WebPolicyDelegate as well...
     */
    WebKitPage*  (*create_page)  (WebKitPage* page);

    /*
     * TODO: FIXME: Create something like WebPolicyDecisionListener_Protocol instead
     */
    WEBKIT_NAVIGATION_REQUEST_RESPONSE (*navigation_requested) (WebKitPage* page, WebKitFrame* frame, WebKitNetworkRequest* request);

    /*
     * TODO: DISCUSS: Should these be signals as well?
     */
    gchar* (*choose_file) (WebKitPage* page, WebKitFrame* frame, const gchar* old_file);
    void   (*java_script_alert) (WebKitPage* page, WebKitFrame* frame, const gchar* alert_message);
    gboolean   (*java_script_confirm) (WebKitPage* page, WebKitFrame* frame, const gchar* confirm_message);
    gchar* (*java_script_prompt) (WebKitPage* page, WebKitFrame* frame, const gchar* message, const gchar* default_value);
    void   (*java_script_console_message) (WebKitPage*, const gchar* message, unsigned int line_number, const gchar* source_id);

    /*
     * internal
     */
    void   (*set_scroll_adjustments) (WebKitPage*, GtkAdjustment*, GtkAdjustment*);
};

WEBKIT_API GType
webkit_page_get_type (void);

WEBKIT_API GtkWidget*
webkit_page_new (void);

WEBKIT_API void
webkit_page_set_settings (WebKitPage* page, WebKitSettings* settings);

WEBKIT_API WebKitSettings*
webkit_page_get_settings (WebKitPage* page);

WEBKIT_API gboolean
webkit_page_can_cut (WebKitPage* page);

WEBKIT_API gboolean
webkit_page_can_copy (WebKitPage* page);

WEBKIT_API gboolean
webkit_page_can_paste (WebKitPage* page);

WEBKIT_API void
webkit_page_cut (WebKitPage* page);

WEBKIT_API void
webkit_page_copy (WebKitPage* page);

WEBKIT_API void
webkit_page_paste (WebKitPage* page);

WEBKIT_API gboolean
webkit_page_can_go_backward (WebKitPage* page);

WEBKIT_API gboolean
webkit_page_can_go_forward (WebKitPage* page);

WEBKIT_API void
webkit_page_go_backward (WebKitPage* page);

WEBKIT_API void
webkit_page_go_forward (WebKitPage* page);

WEBKIT_API void
webkit_page_stop_loading (WebKitPage* page);

WEBKIT_API void
webkit_page_open (WebKitPage* page, const gchar* url);

WEBKIT_API void
webkit_page_reload (WebKitPage *page);

WEBKIT_API void
webkit_page_load_string (WebKitPage* page, const gchar* content, const gchar* content_mime_type, const gchar* content_encoding, const gchar* base_url);

WEBKIT_API void
webkit_page_load_html_string (WebKitPage* page, const gchar* content, const gchar* base_url);

WEBKIT_API WebKitFrame*
webkit_page_get_main_frame (WebKitPage* page);

WEBKIT_API void
webkit_page_execute_script(WebKitPage* page, const gchar* script);
G_END_DECLS

#endif
