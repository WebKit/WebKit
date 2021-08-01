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

#include <JavaScriptCore/JSBase.h>
#include <webkit2/WebKitAuthenticationRequest.h>
#include <webkit2/WebKitBackForwardList.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitColorChooserRequest.h>
#include <webkit2/WebKitEditorState.h>
#include <webkit2/WebKitFileChooserRequest.h>
#include <webkit2/WebKitFindController.h>
#include <webkit2/WebKitFormSubmissionRequest.h>
#include <webkit2/WebKitForwardDeclarations.h>
#include <webkit2/WebKitHitTestResult.h>
#include <webkit2/WebKitInputMethodContext.h>
#include <webkit2/WebKitJavascriptResult.h>
#include <webkit2/WebKitNavigationAction.h>
#include <webkit2/WebKitNotification.h>
#include <webkit2/WebKitOptionMenu.h>
#include <webkit2/WebKitPermissionRequest.h>
#include <webkit2/WebKitPolicyDecision.h>
#include <webkit2/WebKitScriptDialog.h>
#include <webkit2/WebKitSettings.h>
#include <webkit2/WebKitURIRequest.h>
#include <webkit2/WebKitUserContentManager.h>
#include <webkit2/WebKitUserMessage.h>
#include <webkit2/WebKitWebContext.h>
#include <webkit2/WebKitWebInspector.h>
#include <webkit2/WebKitWebResource.h>
#include <webkit2/WebKitWebsitePolicies.h>
#include <webkit2/WebKitWebViewBase.h>
#include <webkit2/WebKitWebViewSessionState.h>
#include <webkit2/WebKitWindowProperties.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW            (webkit_web_view_get_type())
#define WEBKIT_WEB_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebView))
#define WEBKIT_IS_WEB_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_WEB_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))
#define WEBKIT_IS_WEB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_WEB_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))

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
 * #WebKitWebView::decide-policy.
 */
typedef enum {
    WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION,
    WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION,
    WEBKIT_POLICY_DECISION_TYPE_RESPONSE,
} WebKitPolicyDecisionType;

/**
 * WebKitLoadEvent:
 * @WEBKIT_LOAD_STARTED: A new load request has been made.
 * No data has been received yet, empty structures have
 * been allocated to perform the load; the load may still
 * fail due to transport issues such as not being able to
 * resolve a name, or connect to a port.
 * @WEBKIT_LOAD_REDIRECTED: A provisional data source received
 * a server redirect.
 * @WEBKIT_LOAD_COMMITTED: The content started arriving for a page load.
 * The necessary transport requirements are established, and the
 * load is being performed.
 * @WEBKIT_LOAD_FINISHED: Load completed. All resources are done loading
 * or there was an error during the load operation.
 *
 * Enum values used to denote the different events that happen during a
 * #WebKitWebView load operation.
 */
typedef enum {
    WEBKIT_LOAD_STARTED,
    WEBKIT_LOAD_REDIRECTED,
    WEBKIT_LOAD_COMMITTED,
    WEBKIT_LOAD_FINISHED
} WebKitLoadEvent;

/**
 * WebKitSaveMode:
 * @WEBKIT_SAVE_MODE_MHTML: Save the current page using the MHTML format.
 *
 * Enum values to specify the different ways in which a #WebKitWebView
 * can save its current web page into a self-contained file.
 */
typedef enum {
    WEBKIT_SAVE_MODE_MHTML
} WebKitSaveMode;

/**
 * WebKitInsecureContentEvent:
 * @WEBKIT_INSECURE_CONTENT_RUN: Insecure content has been detected by
 * trying to execute any kind of logic (e.g. a script) from an
 * untrusted source.
 * @WEBKIT_INSECURE_CONTENT_DISPLAYED: Insecure content has been
 * detected by trying to display any kind of resource (e.g. an image)
 * from an untrusted source.
 *
 * Enum values used to denote the different events which can trigger
 * the detection of insecure content.
 */
typedef enum {
    WEBKIT_INSECURE_CONTENT_RUN,
    WEBKIT_INSECURE_CONTENT_DISPLAYED
} WebKitInsecureContentEvent;

/**
 * WebKitSnapshotOptions:
 * @WEBKIT_SNAPSHOT_OPTIONS_NONE: Do not include any special options.
 * @WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING: Whether to include in the
 * snapshot the highlight of the selected content.
 * @WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND: Do not fill the background with white before
 * rendering the snapshot. Since 2.8
 *
 * Enum values used to specify options when taking a snapshot
 * from a #WebKitWebView.
 */
typedef enum {
  WEBKIT_SNAPSHOT_OPTIONS_NONE = 0,
  WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING = 1 << 0,
  WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND = 1 << 1,
} WebKitSnapshotOptions;

/**
 * WebKitSnapshotRegion:
 * @WEBKIT_SNAPSHOT_REGION_VISIBLE: Specifies a snapshot only for the area that is
 * visible in the webview
 * @WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT: A snapshot of the entire document.
 *
 * Enum values used to specify the region from which to get a #WebKitWebView
 * snapshot
 */
typedef enum {
  WEBKIT_SNAPSHOT_REGION_VISIBLE = 0,
  WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT,
} WebKitSnapshotRegion;

/**
 * WebKitWebProcessTerminationReason:
 * @WEBKIT_WEB_PROCESS_CRASHED: the web process crashed.
 * @WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT: the web process exceeded the memory limit.
 * @WEBKIT_WEB_PROCESS_TERMINATED_BY_API: the web process termination was requested by an API call. Since: 2.34
 *
 * Enum values used to specify the reason why the web process terminated abnormally.
 *
 * Since: 2.20
 */
typedef enum {
    WEBKIT_WEB_PROCESS_CRASHED,
    WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT,
    WEBKIT_WEB_PROCESS_TERMINATED_BY_API
} WebKitWebProcessTerminationReason;

/**
 * WebKitMediaCaptureState:
 * @WEBKIT_MEDIA_CAPTURE_STATE_NONE: Media capture is disabled.
 * @WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE: Media capture is active.
 * @WEBKIT_MEDIA_CAPTURE_STATE_MUTED: Media capture is muted.
 *
 * Enum values used to specify the capture state of a media device.
 *
 * Since: 2.34
 */
typedef enum {
    WEBKIT_MEDIA_CAPTURE_STATE_NONE,
    WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE,
    WEBKIT_MEDIA_CAPTURE_STATE_MUTED,
} WebKitMediaCaptureState;

struct _WebKitWebView {
    WebKitWebViewBase parent;

    /*< private >*/
    WebKitWebViewPrivate *priv;
};

struct _WebKitWebViewClass {
    WebKitWebViewBaseClass parent;

    void       (* load_changed)                (WebKitWebView               *web_view,
                                                WebKitLoadEvent              load_event);
    gboolean   (* load_failed)                 (WebKitWebView               *web_view,
                                                WebKitLoadEvent              load_event,
                                                const gchar                 *failing_uri,
                                                GError                      *error);

    GtkWidget *(* create)                      (WebKitWebView               *web_view,
                                                WebKitNavigationAction      *navigation_action);
    void       (* ready_to_show)               (WebKitWebView               *web_view);
    void       (* run_as_modal)                (WebKitWebView               *web_view);
    void       (* close)                       (WebKitWebView               *web_view);

    gboolean   (* script_dialog)               (WebKitWebView               *web_view,
                                                WebKitScriptDialog          *dialog)  ;

    gboolean   (* decide_policy)               (WebKitWebView               *web_view,
                                                WebKitPolicyDecision        *decision,
                                                WebKitPolicyDecisionType     type);
    gboolean   (* permission_request)          (WebKitWebView               *web_view,
                                                WebKitPermissionRequest     *permission_request);
    void       (* mouse_target_changed)        (WebKitWebView               *web_view,
                                                WebKitHitTestResult         *hit_test_result,
                                                guint                        modifiers);
    gboolean   (* print)                       (WebKitWebView               *web_view,
                                                WebKitPrintOperation        *print_operation);
    void       (* resource_load_started)       (WebKitWebView               *web_view,
                                                WebKitWebResource           *resource,
                                                WebKitURIRequest            *request);
    gboolean   (* enter_fullscreen)            (WebKitWebView               *web_view);
    gboolean   (* leave_fullscreen)            (WebKitWebView               *web_view);
    gboolean   (* run_file_chooser)            (WebKitWebView               *web_view,
                                                WebKitFileChooserRequest    *request);
    gboolean   (* context_menu)                (WebKitWebView               *web_view,
                                                WebKitContextMenu           *context_menu,
                                                GdkEvent                    *event,
                                                WebKitHitTestResult         *hit_test_result);
    void       (* context_menu_dismissed)      (WebKitWebView               *web_view);
    void       (* submit_form)                 (WebKitWebView               *web_view,
                                                WebKitFormSubmissionRequest *request);
    void       (* insecure_content_detected)   (WebKitWebView               *web_view,
                                                WebKitInsecureContentEvent   event);
    gboolean   (* web_process_crashed)         (WebKitWebView               *web_view);

    gboolean   (* authenticate)                (WebKitWebView               *web_view,
                                                WebKitAuthenticationRequest *request);
    gboolean   (* load_failed_with_tls_errors) (WebKitWebView               *web_view,
                                                const gchar                 *failing_uri,
                                                GTlsCertificate             *certificate,
                                                GTlsCertificateFlags         errors);
    gboolean   (* show_notification)           (WebKitWebView               *web_view,
                                                WebKitNotification          *notification);
    gboolean   (* run_color_chooser)           (WebKitWebView               *web_view,
                                                WebKitColorChooserRequest   *request);
    gboolean   (* show_option_menu)            (WebKitWebView               *web_view,
                                                GdkRectangle                *rectangle,
                                                WebKitOptionMenu            *menu);
    void       (* web_process_terminated)      (WebKitWebView               *web_view,
                                                WebKitWebProcessTerminationReason reason);
    gboolean   (* user_message_received)       (WebKitWebView               *web_view,
                                                WebKitUserMessage           *message);

    void (*_webkit_reserved0) (void);
};

WEBKIT_API GType
webkit_web_view_get_type                             (void);

WEBKIT_API GtkWidget *
webkit_web_view_new                                  (void);

WEBKIT_API GtkWidget *
webkit_web_view_new_with_context                     (WebKitWebContext          *context);

WEBKIT_API GtkWidget *
webkit_web_view_new_with_settings                    (WebKitSettings            *settings);

WEBKIT_API GtkWidget *
webkit_web_view_new_with_related_view                (WebKitWebView             *web_view);

WEBKIT_API GtkWidget *
webkit_web_view_new_with_user_content_manager        (WebKitUserContentManager  *user_content_manager);

WEBKIT_API gboolean
webkit_web_view_is_ephemeral                         (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_is_controlled_by_automation          (WebKitWebView             *web_view);

WEBKIT_API WebKitAutomationBrowsingContextPresentation
webkit_web_view_get_automation_presentation_type     (WebKitWebView             *web_view);

WEBKIT_API WebKitWebsiteDataManager *
webkit_web_view_get_website_data_manager             (WebKitWebView             *web_view);

WEBKIT_API WebKitWebContext *
webkit_web_view_get_context                          (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_try_close                            (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_load_uri                             (WebKitWebView             *web_view,
                                                      const gchar               *uri);

WEBKIT_API void
webkit_web_view_load_html                            (WebKitWebView             *web_view,
                                                      const gchar               *content,
                                                      const gchar               *base_uri);
WEBKIT_API void
webkit_web_view_load_alternate_html                  (WebKitWebView             *web_view,
                                                      const gchar               *content,
                                                      const gchar               *content_uri,
                                                      const gchar               *base_uri);
WEBKIT_API void
webkit_web_view_load_plain_text                      (WebKitWebView             *web_view,
                                                      const gchar               *plain_text);

WEBKIT_API void
webkit_web_view_load_bytes                           (WebKitWebView             *web_view,
                                                      GBytes                    *bytes,
                                                      const gchar               *mime_type,
                                                      const gchar               *encoding,
                                                      const gchar               *base_uri);

WEBKIT_API void
webkit_web_view_load_request                         (WebKitWebView             *web_view,
                                                      WebKitURIRequest          *request);

WEBKIT_API void
webkit_web_view_stop_loading                         (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_is_loading                           (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_is_playing_audio                     (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_is_muted                         (WebKitWebView             *web_view,
                                                      gboolean                   muted);

WEBKIT_API gboolean
webkit_web_view_get_is_muted                         (WebKitWebView             *web_view);

WEBKIT_API guint64
webkit_web_view_get_page_id                          (WebKitWebView             *web_view);

WEBKIT_API const gchar *
webkit_web_view_get_title                            (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_reload                               (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_reload_bypass_cache                  (WebKitWebView             *web_view);

WEBKIT_API gdouble
webkit_web_view_get_estimated_load_progress          (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_go_back                              (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_can_go_back                          (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_go_forward                           (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_can_go_forward                       (WebKitWebView             *web_view);

WEBKIT_API WebKitBackForwardList *
webkit_web_view_get_back_forward_list                (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_go_to_back_forward_list_item         (WebKitWebView             *web_view,
                                                      WebKitBackForwardListItem *list_item);
WEBKIT_API const gchar *
webkit_web_view_get_uri                              (WebKitWebView             *web_view);

WEBKIT_API cairo_surface_t *
webkit_web_view_get_favicon                          (WebKitWebView             *web_view);

WEBKIT_API const gchar *
webkit_web_view_get_custom_charset                   (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_custom_charset                   (WebKitWebView             *web_view,
                                                      const gchar               *charset);

WEBKIT_API void
webkit_web_view_set_settings                         (WebKitWebView             *web_view,
                                                      WebKitSettings            *settings);

WEBKIT_API WebKitSettings *
webkit_web_view_get_settings                         (WebKitWebView             *web_view);

WEBKIT_API WebKitWindowProperties *
webkit_web_view_get_window_properties                (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_zoom_level                       (WebKitWebView             *web_view,
                                                      gdouble                    zoom_level);
WEBKIT_API gdouble
webkit_web_view_get_zoom_level                       (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_can_execute_editing_command          (WebKitWebView             *web_view,
                                                      const gchar               *command,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);

WEBKIT_API gboolean
webkit_web_view_can_execute_editing_command_finish   (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API void
webkit_web_view_execute_editing_command              (WebKitWebView             *web_view,
                                                      const gchar               *command);

WEBKIT_API void
webkit_web_view_execute_editing_command_with_argument(WebKitWebView             *web_view,
                                                      const char                *command,
                                                      const char                *argument);

WEBKIT_API WebKitFindController *
webkit_web_view_get_find_controller                  (WebKitWebView             *web_view);

WEBKIT_DEPRECATED JSGlobalContextRef
webkit_web_view_get_javascript_global_context        (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_run_javascript                       (WebKitWebView             *web_view,
                                                      const gchar               *script,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);
WEBKIT_API WebKitJavascriptResult *
webkit_web_view_run_javascript_finish                (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API void
webkit_web_view_run_javascript_in_world              (WebKitWebView             *web_view,
                                                      const gchar               *script,
                                                      const gchar               *world_name,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);
WEBKIT_API WebKitJavascriptResult *
webkit_web_view_run_javascript_in_world_finish       (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API void
webkit_web_view_run_javascript_from_gresource        (WebKitWebView             *web_view,
                                                      const gchar               *resource,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);

WEBKIT_API WebKitJavascriptResult *
webkit_web_view_run_javascript_from_gresource_finish (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API WebKitWebResource *
webkit_web_view_get_main_resource                    (WebKitWebView             *web_view);

WEBKIT_API WebKitWebInspector *
webkit_web_view_get_inspector                        (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_can_show_mime_type                   (WebKitWebView             *web_view,
                                                      const gchar               *mime_type);

WEBKIT_API void
webkit_web_view_save                                 (WebKitWebView             *web_view,
                                                      WebKitSaveMode             save_mode,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);

WEBKIT_API GInputStream *
webkit_web_view_save_finish                          (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API void
webkit_web_view_save_to_file                         (WebKitWebView             *web_view,
                                                      GFile                     *file,
                                                      WebKitSaveMode             save_mode,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);

WEBKIT_API gboolean
webkit_web_view_save_to_file_finish                  (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API WebKitDownload *
webkit_web_view_download_uri                         (WebKitWebView             *web_view,
                                                      const char                *uri);

WEBKIT_API gboolean
webkit_web_view_get_tls_info                         (WebKitWebView             *web_view,
                                                      GTlsCertificate          **certificate,
                                                      GTlsCertificateFlags      *errors);
WEBKIT_API void
webkit_web_view_get_snapshot                         (WebKitWebView             *web_view,
                                                      WebKitSnapshotRegion       region,
                                                      WebKitSnapshotOptions      options,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);

WEBKIT_API cairo_surface_t *
webkit_web_view_get_snapshot_finish                  (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API WebKitUserContentManager *
webkit_web_view_get_user_content_manager             (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_background_color                 (WebKitWebView             *web_view,
                                                      const GdkRGBA             *rgba);

WEBKIT_API void
webkit_web_view_get_background_color                 (WebKitWebView             *web_view,
                                                      GdkRGBA                   *rgba);

WEBKIT_API gboolean
webkit_web_view_is_editable                          (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_editable                         (WebKitWebView             *web_view,
                                                      gboolean                  editable);

WEBKIT_API WebKitEditorState *
webkit_web_view_get_editor_state                     (WebKitWebView             *web_view);

WEBKIT_API WebKitWebViewSessionState *
webkit_web_view_get_session_state                    (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_restore_session_state                (WebKitWebView             *web_view,
                                                      WebKitWebViewSessionState *state);

WEBKIT_API void
webkit_web_view_send_message_to_page                 (WebKitWebView             *web_view,
                                                      WebKitUserMessage         *message,
                                                      GCancellable              *cancellable,
                                                      GAsyncReadyCallback        callback,
                                                      gpointer                   user_data);

WEBKIT_API WebKitUserMessage *
webkit_web_view_send_message_to_page_finish          (WebKitWebView             *web_view,
                                                      GAsyncResult              *result,
                                                      GError                   **error);

WEBKIT_API void
webkit_web_view_set_input_method_context             (WebKitWebView             *web_view,
                                                      WebKitInputMethodContext  *context);

WEBKIT_API WebKitInputMethodContext *
webkit_web_view_get_input_method_context             (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_cors_allowlist                   (WebKitWebView             *web_view,
                                                      const gchar * const       *allowlist);

WEBKIT_API WebKitWebsitePolicies *
webkit_web_view_get_website_policies                 (WebKitWebView             *web_view);

WEBKIT_API gboolean
webkit_web_view_get_is_web_process_responsive        (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_terminate_web_process                (WebKitWebView             *web_view);

WEBKIT_API WebKitMediaCaptureState
webkit_web_view_get_camera_capture_state             (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_camera_capture_state             (WebKitWebView             *web_view,
                                                      WebKitMediaCaptureState    state);

WEBKIT_API WebKitMediaCaptureState
webkit_web_view_get_microphone_capture_state         (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_microphone_capture_state         (WebKitWebView             *web_view,
                                                      WebKitMediaCaptureState    state);

WEBKIT_API WebKitMediaCaptureState
webkit_web_view_get_display_capture_state            (WebKitWebView             *web_view);

WEBKIT_API void
webkit_web_view_set_display_capture_state            (WebKitWebView             *web_view,
                                                      WebKitMediaCaptureState    state);

G_END_DECLS

#endif
