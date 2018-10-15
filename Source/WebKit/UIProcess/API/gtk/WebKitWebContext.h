/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebKitWebContext_h
#define WebKitWebContext_h

#include <glib-object.h>
#include <webkit2/WebKitAutomationSession.h>
#include <webkit2/WebKitCookieManager.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitDownload.h>
#include <webkit2/WebKitFaviconDatabase.h>
#include <webkit2/WebKitNetworkProxySettings.h>
#include <webkit2/WebKitSecurityManager.h>
#include <webkit2/WebKitURISchemeRequest.h>
#include <webkit2/WebKitWebsiteDataManager.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_CONTEXT            (webkit_web_context_get_type())
#define WEBKIT_WEB_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContext))
#define WEBKIT_WEB_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContextClass))
#define WEBKIT_IS_WEB_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_CONTEXT))
#define WEBKIT_IS_WEB_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_CONTEXT))
#define WEBKIT_WEB_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContextClass))

/**
 * WebKitCacheModel:
 * @WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER: Disable the cache completely, which
 *   substantially reduces memory usage. Useful for applications that only
 *   access a single local file, with no navigation to other pages. No remote
 *   resources will be cached.
 * @WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER: A cache model optimized for viewing
 *   a series of local files -- for example, a documentation viewer or a website
 *   designer. WebKit will cache a moderate number of resources.
 * @WEBKIT_CACHE_MODEL_WEB_BROWSER: Improve document load speed substantially
 *   by caching a very large number of resources and previously viewed content.
 *
 * Enum values used for determining the #WebKitWebContext cache model.
 */
typedef enum {
    WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER,
    WEBKIT_CACHE_MODEL_WEB_BROWSER,
    WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER
} WebKitCacheModel;

/**
 * WebKitProcessModel:
 * @WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS: Use a single process to
 *   perform content rendering. The process is shared among all the
 *   #WebKitWebView instances created by the application: if the process
 *   hangs or crashes all the web views in the application will be affected.
 *   This is the default process model, and it should suffice for most cases.
 * @WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES: Use one process
 *   for each #WebKitWebView, while still allowing for some of them to
 *   share a process in certain situations. The main advantage
 *   of this process model is that the rendering process for a web view
 *   can crash while the rest of the views keep working normally. This
 *   process model is indicated for applications which may use a number
 *   of web views and the content of in each must not interfere with the
 *   rest — for example a full-fledged web browser with support for
 *   multiple tabs.
 *
 * Enum values used for determining the #WebKitWebContext process model.
 *
 * Since: 2.4
 */
typedef enum {
    WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS,
    WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES,
} WebKitProcessModel;

/**
 * WebKitTLSErrorsPolicy:
 * @WEBKIT_TLS_ERRORS_POLICY_IGNORE: Ignore TLS errors.
 * @WEBKIT_TLS_ERRORS_POLICY_FAIL: TLS errors will emit
 *   #WebKitWebView::load-failed-with-tls-errors and, if the signal is handled,
 *   finish the load. In case the signal is not handled,
 *   #WebKitWebView::load-failed is emitted before the load finishes.
 *
 * Enum values used to denote the TLS errors policy.
 */
typedef enum {
    WEBKIT_TLS_ERRORS_POLICY_IGNORE,
    WEBKIT_TLS_ERRORS_POLICY_FAIL
} WebKitTLSErrorsPolicy;

/**
 * WebKitNetworkProxyMode:
 * @WEBKIT_NETWORK_PROXY_MODE_DEFAULT: Use the default proxy of the system.
 * @WEBKIT_NETWORK_PROXY_MODE_NO_PROXY: Do not use any proxy.
 * @WEBKIT_NETWORK_PROXY_MODE_CUSTOM: Use custom proxy settings.
 *
 * Enum values used to set the network proxy mode.
 *
 * Since: 2.16
 */
typedef enum {
    WEBKIT_NETWORK_PROXY_MODE_DEFAULT,
    WEBKIT_NETWORK_PROXY_MODE_NO_PROXY,
    WEBKIT_NETWORK_PROXY_MODE_CUSTOM
} WebKitNetworkProxyMode;

/**
 * WebKitURISchemeRequestCallback:
 * @request: the #WebKitURISchemeRequest
 * @user_data: user data passed to the callback
 *
 * Type definition for a function that will be called back when an URI request is
 * made for a user registered URI scheme.
 */
typedef void (* WebKitURISchemeRequestCallback) (WebKitURISchemeRequest *request,
                                                 gpointer                user_data);

typedef struct _WebKitWebContext        WebKitWebContext;
typedef struct _WebKitWebContextClass   WebKitWebContextClass;
typedef struct _WebKitWebContextPrivate WebKitWebContextPrivate;

struct _WebKitWebContext {
    GObject parent;

    /*< private >*/
    WebKitWebContextPrivate *priv;
};

struct _WebKitWebContextClass {
    GObjectClass parent;

    void (* download_started)                    (WebKitWebContext        *context,
                                                  WebKitDownload          *download);
    void (* initialize_web_extensions)           (WebKitWebContext        *context);
    void (* initialize_notification_permissions) (WebKitWebContext        *context);
    void (* automation_started)                  (WebKitWebContext        *context,
                                                  WebKitAutomationSession *session);

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_web_context_get_type                         (void);

WEBKIT_API WebKitWebContext *
webkit_web_context_get_default                      (void);

WEBKIT_API WebKitWebContext *
webkit_web_context_new                              (void);

WEBKIT_API WebKitWebContext *
webkit_web_context_new_ephemeral                    (void);

WEBKIT_API WebKitWebContext *
webkit_web_context_new_with_website_data_manager    (WebKitWebsiteDataManager      *manager);

WEBKIT_API WebKitWebsiteDataManager *
webkit_web_context_get_website_data_manager         (WebKitWebContext              *context);

WEBKIT_API gboolean
webkit_web_context_is_ephemeral                     (WebKitWebContext              *context);

WEBKIT_API gboolean
webkit_web_context_is_automation_allowed            (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_automation_allowed           (WebKitWebContext              *context,
                                                     gboolean                       allowed);
WEBKIT_API void
webkit_web_context_set_cache_model                  (WebKitWebContext              *context,
                                                     WebKitCacheModel               cache_model);
WEBKIT_API WebKitCacheModel
webkit_web_context_get_cache_model                  (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_web_process_count_limit      (WebKitWebContext              *context,
                                                     guint                          limit);

WEBKIT_API guint
webkit_web_context_get_web_process_count_limit      (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_clear_cache                      (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_network_proxy_settings       (WebKitWebContext              *context,
                                                     WebKitNetworkProxyMode         proxy_mode,
                                                     WebKitNetworkProxySettings    *proxy_settings);

WEBKIT_API WebKitDownload *
webkit_web_context_download_uri                     (WebKitWebContext              *context,
                                                     const gchar                   *uri);

WEBKIT_API WebKitCookieManager *
webkit_web_context_get_cookie_manager               (WebKitWebContext              *context);

WEBKIT_API WebKitFaviconDatabase *
webkit_web_context_get_favicon_database             (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_favicon_database_directory   (WebKitWebContext              *context,
                                                     const gchar                   *path);
WEBKIT_API const gchar *
webkit_web_context_get_favicon_database_directory   (WebKitWebContext              *context);

WEBKIT_API WebKitSecurityManager *
webkit_web_context_get_security_manager             (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_additional_plugins_directory (WebKitWebContext              *context,
                                                     const gchar                   *directory);

WEBKIT_API void
webkit_web_context_get_plugins                      (WebKitWebContext              *context,
                                                     GCancellable                  *cancellable,
                                                     GAsyncReadyCallback            callback,
                                                     gpointer                       user_data);

WEBKIT_API GList *
webkit_web_context_get_plugins_finish               (WebKitWebContext              *context,
                                                     GAsyncResult                  *result,
                                                     GError                       **error);
WEBKIT_API void
webkit_web_context_register_uri_scheme              (WebKitWebContext              *context,
                                                     const gchar                   *scheme,
                                                     WebKitURISchemeRequestCallback callback,
                                                     gpointer                       user_data,
                                                     GDestroyNotify                 user_data_destroy_func);

WEBKIT_API void
webkit_web_context_set_sandbox_enabled              (WebKitWebContext              *context,
                                                     gboolean                       enabled);

WEBKIT_API gboolean
webkit_web_context_get_sandbox_enabled              (WebKitWebContext              *context);

WEBKIT_API gboolean
webkit_web_context_get_spell_checking_enabled       (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_spell_checking_enabled       (WebKitWebContext              *context,
                                                     gboolean                       enabled);
WEBKIT_API const gchar * const *
webkit_web_context_get_spell_checking_languages     (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_spell_checking_languages     (WebKitWebContext              *context,
                                                     const gchar * const           *languages);

WEBKIT_API void
webkit_web_context_set_preferred_languages          (WebKitWebContext              *context,
                                                     const gchar * const           *languages);

WEBKIT_API void
webkit_web_context_set_tls_errors_policy            (WebKitWebContext              *context,
                                                     WebKitTLSErrorsPolicy          policy);

WEBKIT_API WebKitTLSErrorsPolicy
webkit_web_context_get_tls_errors_policy            (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_set_web_extensions_directory     (WebKitWebContext              *context,
                                                     const gchar                   *directory);

WEBKIT_API void
webkit_web_context_set_web_extensions_initialization_user_data
                                                    (WebKitWebContext              *context,
                                                     GVariant                      *user_data);

WEBKIT_API void
webkit_web_context_prefetch_dns                     (WebKitWebContext              *context,
                                                     const gchar                   *hostname);

WEBKIT_DEPRECATED_FOR(webkit_web_context_new_with_website_data_manager) void
webkit_web_context_set_disk_cache_directory         (WebKitWebContext              *context,
                                                     const gchar                   *directory);

WEBKIT_API void
webkit_web_context_allow_tls_certificate_for_host   (WebKitWebContext              *context,
                                                     GTlsCertificate               *certificate,
                                                     const gchar                   *host);

WEBKIT_API void
webkit_web_context_set_process_model                (WebKitWebContext              *context,
                                                     WebKitProcessModel             process_model);

WEBKIT_API WebKitProcessModel
webkit_web_context_get_process_model                (WebKitWebContext              *context);

WEBKIT_API void
webkit_web_context_initialize_notification_permissions
                                                    (WebKitWebContext              *context,
                                                     GList                         *allowed_origins,
                                                     GList                         *disallowed_origins);

G_END_DECLS

#endif
