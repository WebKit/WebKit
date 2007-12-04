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

#ifndef WEBKIT_WEB_VIEW_H
#define WEBKIT_WEB_VIEW_H

#include <gtk/gtk.h>
#include <JavaScriptCore/JSBase.h>

#include "webkitdefines.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW            (webkit_web_view_get_type())
#define WEBKIT_WEB_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebView))
#define WEBKIT_WEB_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))
#define WEBKIT_IS_WEB_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_IS_WEB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_WEB_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))


typedef enum {
    WEBKIT_NAVIGATION_RESPONSE_ACCEPT,
    WEBKIT_NAVIGATION_RESPONSE_IGNORE,
    WEBKIT_NAVIGATION_RESPONSE_DOWNLOAD
} WebKitNavigationResponse;



struct _WebKitWebView {
    GtkContainer parent;
};

struct _WebKitWebViewClass {
    GtkContainerClass parent;

    /*
     * default handler/virtual methods
     * DISCUSS: create_web_view needs a request and should we make this a signal with default handler? this would
     * require someone doing a g_signal_stop_emission_by_name
     * WebUIDelegate has nothing for create_frame, WebPolicyDelegate as well...
     */
    WebKitWebView*  (*create_web_view)  (WebKitWebView* web_view);

    /*
     * TODO: FIXME: Create something like WebPolicyDecisionListener_Protocol instead
     */
    WebKitNavigationResponse (*navigation_requested) (WebKitWebView* web_view, WebKitWebFrame* frame, WebKitNetworkRequest* request);

    void (*window_object_cleared) (WebKitWebView* web_view, WebKitWebFrame* frame, JSGlobalContextRef context, JSObjectRef window_object);
    gchar*   (*choose_file) (WebKitWebView* web_view, WebKitWebFrame* frame, const gchar* old_file);
    gboolean (*script_alert) (WebKitWebView* web_view, WebKitWebFrame* frame, const gchar* alert_message);
    gboolean (*script_confirm) (WebKitWebView* web_view, WebKitWebFrame* frame, const gchar* confirm_message, gboolean* did_confirm);
    gboolean (*script_prompt) (WebKitWebView* web_view, WebKitWebFrame* frame, const gchar* message, const gchar* default_value, gchar** value);
    gboolean (*console_message) (WebKitWebView* web_view, const gchar* message, unsigned int line_number, const gchar* source_id);
    void (*select_all) (WebKitWebView* web_view);
    void (*cut_clipboard) (WebKitWebView* web_view);
    void (*copy_clipboard) (WebKitWebView* web_view);
    void (*paste_clipboard) (WebKitWebView* web_view);

    /*
     * internal
     */
    void   (*set_scroll_adjustments) (WebKitWebView*, GtkAdjustment*, GtkAdjustment*);
};

WEBKIT_API GType
webkit_web_view_get_type (void);

WEBKIT_API GtkWidget*
webkit_web_view_new (void);

WEBKIT_API gboolean
webkit_web_view_can_go_backward (WebKitWebView* web_view);

WEBKIT_API gboolean
webkit_web_view_can_go_forward (WebKitWebView* web_view);

WEBKIT_API void
webkit_web_view_go_backward (WebKitWebView* web_view);

WEBKIT_API void
webkit_web_view_go_forward (WebKitWebView* web_view);

WEBKIT_API void
webkit_web_view_stop_loading (WebKitWebView* web_view);

WEBKIT_API void
webkit_web_view_open (WebKitWebView* web_view, const gchar* uri);

WEBKIT_API void
webkit_web_view_reload (WebKitWebView *web_view);

WEBKIT_API void
webkit_web_view_load_string (WebKitWebView* web_view, const gchar* content, const gchar* content_mime_type, const gchar* content_encoding, const gchar* base_uri);

WEBKIT_API void
webkit_web_view_load_html_string (WebKitWebView* web_view, const gchar* content, const gchar* base_uri);

WEBKIT_API WebKitWebFrame*
webkit_web_view_get_main_frame (WebKitWebView* web_view);

WEBKIT_API void
webkit_web_view_execute_script (WebKitWebView* web_view, const gchar* script);

WEBKIT_API gboolean
webkit_web_view_get_editable (WebKitWebView* web_view);

WEBKIT_API void
webkit_web_view_set_editable (WebKitWebView* web_view, gboolean flag);

G_END_DECLS

#endif
