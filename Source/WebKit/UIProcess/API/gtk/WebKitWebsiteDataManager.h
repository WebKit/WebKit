/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#ifndef WebKitWebsiteDataManager_h
#define WebKitWebsiteDataManager_h

#include <gio/gio.h>
#include <webkit2/WebKitCookieManager.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitMemoryPressureSettings.h>
#include <webkit2/WebKitNetworkProxySettings.h>
#include <webkit2/WebKitWebsiteData.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEBSITE_DATA_MANAGER            (webkit_website_data_manager_get_type())
#define WEBKIT_WEBSITE_DATA_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEBSITE_DATA_MANAGER, WebKitWebsiteDataManager))
#define WEBKIT_IS_WEBSITE_DATA_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEBSITE_DATA_MANAGER))
#define WEBKIT_WEBSITE_DATA_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEBSITE_DATA_MANAGER, WebKitWebsiteDataManagerClass))
#define WEBKIT_IS_WEBSITE_DATA_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEBSITE_DATA_MANAGER))
#define WEBKIT_WEBSITE_DATA_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEBSITE_DATA_MANAGER, WebKitWebsiteDataManagerClass))

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

typedef struct _WebKitWebsiteDataManager        WebKitWebsiteDataManager;
typedef struct _WebKitWebsiteDataManagerClass   WebKitWebsiteDataManagerClass;
typedef struct _WebKitWebsiteDataManagerPrivate WebKitWebsiteDataManagerPrivate;

struct _WebKitWebsiteDataManager {
    GObject parent;

    WebKitWebsiteDataManagerPrivate *priv;
};

struct _WebKitWebsiteDataManagerClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_website_data_manager_get_type                                  (void);

WEBKIT_API WebKitWebsiteDataManager *
webkit_website_data_manager_new                                       (const gchar              *first_option_name,
                                                                       ...);
WEBKIT_API WebKitWebsiteDataManager *
webkit_website_data_manager_new_ephemeral                             (void);

WEBKIT_API gboolean
webkit_website_data_manager_is_ephemeral                              (WebKitWebsiteDataManager* manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_base_data_directory                   (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_base_cache_directory                  (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_local_storage_directory               (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_disk_cache_directory                  (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_offline_application_cache_directory   (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_indexeddb_directory                   (WebKitWebsiteDataManager *manager);

WEBKIT_DEPRECATED const gchar *
webkit_website_data_manager_get_websql_directory                      (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_hsts_cache_directory                  (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_itp_directory                         (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_service_worker_registrations_directory(WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_dom_cache_directory                   (WebKitWebsiteDataManager *manager);

WEBKIT_API WebKitCookieManager *
webkit_website_data_manager_get_cookie_manager                        (WebKitWebsiteDataManager *manager);

WEBKIT_API void
webkit_website_data_manager_set_itp_enabled                           (WebKitWebsiteDataManager *manager,
                                                                       gboolean                  enabled);

WEBKIT_API gboolean
webkit_website_data_manager_get_itp_enabled                           (WebKitWebsiteDataManager *manager);

WEBKIT_API void
webkit_website_data_manager_set_persistent_credential_storage_enabled (WebKitWebsiteDataManager *manager,
                                                                       gboolean                  enabled);

WEBKIT_API gboolean
webkit_website_data_manager_get_persistent_credential_storage_enabled (WebKitWebsiteDataManager *manager);

WEBKIT_API void
webkit_website_data_manager_set_tls_errors_policy                     (WebKitWebsiteDataManager *manager,
                                                                       WebKitTLSErrorsPolicy     policy);

WEBKIT_API WebKitTLSErrorsPolicy
webkit_website_data_manager_get_tls_errors_policy                     (WebKitWebsiteDataManager *manager);

WEBKIT_API void
webkit_website_data_manager_set_network_proxy_settings                (WebKitWebsiteDataManager *manager,
                                                                       WebKitNetworkProxyMode    proxy_mode,
                                                                       WebKitNetworkProxySettings *proxy_settings);

WEBKIT_API void
webkit_website_data_manager_fetch                                     (WebKitWebsiteDataManager *manager,
                                                                       WebKitWebsiteDataTypes    types,
                                                                       GCancellable             *cancellable,
                                                                       GAsyncReadyCallback       callback,
                                                                       gpointer                  user_data);

WEBKIT_API GList *
webkit_website_data_manager_fetch_finish                              (WebKitWebsiteDataManager *manager,
                                                                       GAsyncResult             *result,
                                                                       GError                  **error);
WEBKIT_API void
webkit_website_data_manager_remove                                    (WebKitWebsiteDataManager *manager,
                                                                       WebKitWebsiteDataTypes    types,
                                                                       GList                    *website_data,
                                                                       GCancellable             *cancellable,
                                                                       GAsyncReadyCallback       callback,
                                                                       gpointer                  user_data);
WEBKIT_API gboolean
webkit_website_data_manager_remove_finish                             (WebKitWebsiteDataManager *manager,
                                                                       GAsyncResult             *result,
                                                                       GError                  **error);

WEBKIT_API void
webkit_website_data_manager_clear                                      (WebKitWebsiteDataManager *manager,
                                                                        WebKitWebsiteDataTypes    types,
                                                                        GTimeSpan                 timespan,
                                                                        GCancellable             *cancellable,
                                                                        GAsyncReadyCallback       callback,
                                                                        gpointer                  user_data);

WEBKIT_API gboolean
webkit_website_data_manager_clear_finish                               (WebKitWebsiteDataManager *manager,
                                                                        GAsyncResult             *result,
                                                                        GError                  **error);

WEBKIT_API void
webkit_website_data_manager_set_memory_pressure_settings               (WebKitMemoryPressureSettings *settings);


#define WEBKIT_TYPE_ITP_FIRST_PARTY   (webkit_itp_first_party_get_type())

typedef struct _WebKitITPFirstParty WebKitITPFirstParty;

WEBKIT_API GType
webkit_itp_first_party_get_type                        (void);

WEBKIT_API WebKitITPFirstParty *
webkit_itp_first_party_ref                             (WebKitITPFirstParty      *itp_first_party);

WEBKIT_API void
webkit_itp_first_party_unref                           (WebKitITPFirstParty      *itp_first_party);

WEBKIT_API const char *
webkit_itp_first_party_get_domain                      (WebKitITPFirstParty      *itp_first_party);

WEBKIT_API gboolean
webkit_itp_first_party_get_website_data_access_allowed (WebKitITPFirstParty      *itp_first_party);

WEBKIT_API GDateTime *
webkit_itp_first_party_get_last_update_time            (WebKitITPFirstParty      *itp_first_party);


#define WEBKIT_TYPE_ITP_THIRD_PARTY   (webkit_itp_third_party_get_type())

typedef struct _WebKitITPThirdParty WebKitITPThirdParty;

WEBKIT_API GType
webkit_itp_third_party_get_type                        (void);

WEBKIT_API WebKitITPThirdParty *
webkit_itp_third_party_ref                             (WebKitITPThirdParty      *itp_third_party);

WEBKIT_API void
webkit_itp_third_party_unref                           (WebKitITPThirdParty      *itp_third_party);

WEBKIT_API const char *
webkit_itp_third_party_get_domain                      (WebKitITPThirdParty      *itp_third_party);

WEBKIT_API GList *
webkit_itp_third_party_get_first_parties               (WebKitITPThirdParty      *itp_third_party);


WEBKIT_API void
webkit_website_data_manager_get_itp_summary            (WebKitWebsiteDataManager *manager,
                                                        GCancellable             *cancellable,
                                                        GAsyncReadyCallback       callback,
                                                        gpointer                  user_data);
WEBKIT_API GList *
webkit_website_data_manager_get_itp_summary_finish     (WebKitWebsiteDataManager *manager,
                                                        GAsyncResult             *result,
                                                        GError                  **error);

G_END_DECLS

#endif
