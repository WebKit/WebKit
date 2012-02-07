/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitWebView_h
#define WebKitWebView_h

#include <webkit2/WebKitBackForwardList.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitWebContext.h>
#include <webkit2/WebKitSettings.h>
#include <webkit2/WebKitURIRequest.h>
#include <webkit2/WebKitWebViewBase.h>
#include <webkit2/WebKitWindowProperties.h>
#include <webkit2/WebKitPolicyDecision.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW            (webkit_web_view_get_type())
#define WEBKIT_WEB_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebView))
#define WEBKIT_IS_WEB_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_WEB_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))
#define WEBKIT_IS_WEB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_WEB_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))

typedef struct _WebKitWebView WebKitWebView;
typedef struct _WebKitWebViewClass WebKitWebViewClass;
typedef struct _WebKitWebViewPrivate WebKitWebViewPrivate;

/**
 * WebKitPolicyDecisionType:
 * @WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION: This type of policy decision
 *   is requested when WebKit is about to navigate to a new page in either the
 *   main frame or a subframe. Acceptable policy decisions are either
 *   webkit_policy_decision_use() or webkit_policy_decision_ignore(). This
 *   type of policy decision is always a #WebKitNavigationPolicyDecision.
 * @WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION: This type of policy decision
 *   is requested when WebKit is about to create a new window. Acceptable policy
 *   decisions are either webkit_policy_decision_use() or
 *   webkit_policy_decision_ignore(). This type of policy decision is always
 *   a #WebKitNavigationPolicyDecision. These decisions are useful for implementing
 *   special actions for new windows, such as forcing the new window to open
 *   in a tab when a keyboard modifier is active or handling a special
 *   target attribute on &lt;a&gt; elements.
 * @WEBKIT_POLICY_DECISION_TYPE_RESPONSE: This type of decision is used when WebKit has
 *   received a response for a network resource and is about to start the load.
 *   Note that these resources include all subresources of a page such as images
 *   and stylesheets as well as main documents. Appropriate policy responses to
 *   this decision are webkit_policy_decision_use(), webkit_policy_decision_ignore(),
 *   or webkit_policy_decision_download(). This type of policy decision is always
 *   a #WebKitResponsePolicyDecision. This decision is useful for forcing
 *   some types of resources to be downloaded rather than rendered in the WebView
 *   or to block the transfer of resources entirely.
 *
 * Enum values used for determining the type of a policy decision during
 * WebKitWebView::decide-policy.
 */
typedef enum {
    WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION,
    WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION,
    WEBKIT_POLICY_DECISION_TYPE_RESPONSE,
} WebKitPolicyDecisionType;

/**
 * WebKitLoadEvent
 * @WEBKIT_LOAD_STARTED: A new load request has been made.
 * No data has been received yet, empty structures have
 * been allocated to perform the load; the load may still
 * fail due to transport issues such as not being able to
 * resolve a name, or connect to a port.
 * @WEBKIT_LOAD_REDIRECTED: A provisional data source received
 * a server redirect.
 * @WEBKIT_LOAD_COMMITTED: The content started arriving for a page load.
 * The necessary transport requirements are stabilished, and the
 * load is being performed.
 * @WEBKIT_LOAD_FINISHED: Load completed. All resources are done loading
 * or there was an error during the load operation.
 */
typedef enum {
    WEBKIT_LOAD_STARTED,
    WEBKIT_LOAD_REDIRECTED,
    WEBKIT_LOAD_COMMITTED,
    WEBKIT_LOAD_FINISHED
} WebKitLoadEvent;

struct _WebKitWebView {
    WebKitWebViewBase parent;

    /*< private >*/
    WebKitWebViewPrivate *priv;
};

struct _WebKitWebViewClass {
    WebKitWebViewBaseClass parent;

    void       (* load_changed)   (WebKitWebView             *web_view,
                                   WebKitLoadEvent            load_event);
    gboolean   (* load_failed)    (WebKitWebView             *web_view,
                                   WebKitLoadEvent            load_event,
                                   const gchar               *failing_uri,
                                   GError                    *error);

    GtkWidget *(* create)         (WebKitWebView             *web_view);
    void       (* ready_to_show)  (WebKitWebView             *web_view);
    void       (* close)          (WebKitWebView             *web_view);

    gboolean   (* script_alert)   (WebKitWebView             *web_view,
                                   const gchar               *message);
    gboolean   (* script_confirm) (WebKitWebView             *web_view,
                                   const gchar               *message,
                                   gboolean                  *confirmed);
    gboolean   (* script_prompt)  (WebKitWebView             *web_view,
                                   const gchar               *message,
                                   const gchar               *default_text,
                                   gchar                    **text);
    gboolean   (* decide_policy)  (WebKitWebView             *web_view,
                                   WebKitPolicyDecision      *decision,
                                   WebKitPolicyDecisionType  type);

    /* Padding for future expansion */
    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
    void (*_webkit_reserved4) (void);
    void (*_webkit_reserved5) (void);
    void (*_webkit_reserved6) (void);
    void (*_webkit_reserved7) (void);
};

WEBKIT_API GType
webkit_web_view_get_type                           (void);

WEBKIT_API GtkWidget *
webkit_web_view_new                                (void);

WEBKIT_API GtkWidget *
webkit_web_view_new_with_context                   (WebKitWebContext          *context);

WEBKIT_API WebKitWebContext *
webkit_web_view_get_context                        (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_load_uri                           (WebKitWebView             *web_view,
                                                    const gchar               *uri);

WEBKIT_API void
webkit_web_view_load_html                          (WebKitWebView             *web_view,
                                                    const gchar               *content,
                                                    const gchar               *base_uri);

WEBKIT_API void
webkit_web_view_load_plain_text                    (WebKitWebView             *web_view,
                                                    const gchar               *plain_text);

WEBKIT_API void
webkit_web_view_load_request                       (WebKitWebView             *web_view,
                                                    WebKitURIRequest          *request);

WEBKIT_API void
webkit_web_view_stop_loading                       (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_replace_content                    (WebKitWebView             *web_view,
                                                    const gchar               *content,
                                                    const gchar               *content_uri,
                                                    const gchar               *base_uri);

WEBKIT_API const gchar *
webkit_web_view_get_title                          (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_reload                             (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_reload_bypass_cache                (WebKitWebView             *web_view);

WEBKIT_API gdouble
webkit_web_view_get_estimated_load_progress        (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_go_back                            (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_can_go_back                        (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_go_forward                         (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_can_go_forward                     (WebKitWebView             *web_view);

WEBKIT_API WebKitBackForwardList *
webkit_web_view_get_back_forward_list              (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_go_to_back_forward_list_item       (WebKitWebView             *web_view,
                                                    WebKitBackForwardListItem *list_item);
WEBKIT_API const gchar *
webkit_web_view_get_uri                            (WebKitWebView             *web_view);

WEBKIT_API const gchar *
webkit_web_view_get_custom_charset                 (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_custom_charset                 (WebKitWebView             *web_view,
                                                    const gchar               *charset);

WEBKIT_API void
webkit_web_view_set_settings                       (WebKitWebView             *web_view,
                                                    WebKitSettings            *settings);

WEBKIT_API WebKitSettings *
webkit_web_view_get_settings                       (WebKitWebView             *web_view);

WEBKIT_API WebKitWindowProperties *
webkit_web_view_get_window_properties              (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_zoom_level                     (WebKitWebView             *web_view,
                                                    gdouble                    zoom_level);
WEBKIT_API gdouble
webkit_web_view_get_zoom_level                     (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_can_execute_editing_command        (WebKitWebView             *web_view,
                                                    const gchar               *command,
                                                    GAsyncReadyCallback        callback,
                                                    gpointer                   user_data);

WEBKIT_API gboolean
webkit_web_view_can_execute_editing_command_finish (WebKitWebView             *web_view,
                                                    GAsyncResult              *result,
                                                    GError                   **error);

WEBKIT_API void
webkit_web_view_execute_editing_command            (WebKitWebView             *web_view,
                                                    const gchar               *command);
G_END_DECLS

#endif
