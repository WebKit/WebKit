/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * @file    ewk_context.h
 * @brief   Describes the context API.
 *
 * @note ewk_context encapsulates all pages related to specific use of WebKit.
 *
 * Applications have the option of creating a context different than the default one
 * and use it for a group of pages. All pages in the same context share the same
 * preferences, visited link set, local storage, etc.
 *
 * A process model can be specified per context. The default one is the shared model
 * where the web-engine process is shared among the pages in the context. The second
 * model allows each page to use a separate web-engine process. This latter model is
 * currently not supported by WebKit2/EFL.
 *
 */

#ifndef ewk_context_h
#define ewk_context_h

#include "ewk_application_cache_manager.h"
#include "ewk_cookie_manager.h"
#include "ewk_database_manager.h"
#include "ewk_download_job.h"
#include "ewk_error.h"
#include "ewk_favicon_database.h"
#include "ewk_navigation_data.h"
#include "ewk_storage_manager.h"
#include "ewk_url_scheme_request.h"
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Creates a type name for Ewk_Download_Job_Error.
typedef struct Ewk_Download_Job_Error Ewk_Download_Job_Error;

/**
 * @brief Structure containing details about a download failure.
 */
struct Ewk_Download_Job_Error {
    Ewk_Download_Job *download_job; /**< download that failed */
    Ewk_Error *error; /**< download error */
};

/**
 * Declare Ewk_Context as Ewk_Object.
 *
 * @see Ewk_Object
 */
#ifdef __cplusplus
typedef class EwkObject Ewk_Context;
#else
typedef struct EwkObject Ewk_Context;
#endif

/**
 * \enum    Ewk_Cache_Model
 *
 * @brief   Contains option for cache model
 */
enum Ewk_Cache_Model {
    /// Use the smallest cache capacity.
    EWK_CACHE_MODEL_DOCUMENT_VIEWER,
    /// Use bigger cache capacity than EWK_CACHE_MODEL_DOCUMENT_VIEWER.
    EWK_CACHE_MODEL_DOCUMENT_BROWSER,
    /// Use the biggest cache capacity.
    EWK_CACHE_MODEL_PRIMARY_WEBBROWSER
};

/// Creates a type name for the Ewk_Cache_Model.
typedef enum Ewk_Cache_Model Ewk_Cache_Model;

/**
 * \enum    Ewk_TLS_Error_Policy
 *
 * @brief   Contains option for TLS error policy
 */
enum Ewk_TLS_Error_Policy {
    EWK_TLS_ERROR_POLICY_FAIL, // Fail on TLS errors.
    EWK_TLS_ERROR_POLICY_IGNORE // Ignore TLS errors.
};

// Creates a type name for the Ewk_TLS_Error_Policy.
typedef enum Ewk_TLS_Error_Policy Ewk_TLS_Error_Policy;

/**
 * @typedef Ewk_Url_Scheme_Request_Cb Ewk_Url_Scheme_Request_Cb
 * @brief Callback type for use with ewk_context_url_scheme_register().
 */
typedef void (*Ewk_Url_Scheme_Request_Cb) (Ewk_Url_Scheme_Request *request, void *user_data);

/**
 * @typedef Ewk_History_Navigation_Cb Ewk_History_Navigation_Cb
 * @brief Type definition for a function that will be called back when @a view did navigation (loaded new URL).
 */
typedef void (*Ewk_History_Navigation_Cb)(const Evas_Object *view, Ewk_Navigation_Data *navigation_data, void *user_data);

/**
 * @typedef Ewk_History_Client_Redirection_Cb Ewk_History_Client_Redirection_Cb
 * @brief Type definition for a function that will be called back when @a view performed a client redirect.
 */
typedef void (*Ewk_History_Client_Redirection_Cb)(const Evas_Object *view, const char *source_url, const char *destination_url, void *user_data);

/**
 * @typedef Ewk_History_Server_Redirection_Cb Ewk_History_Server_Redirection_Cb
 * @brief Type definition for a function that will be called back when @a view performed a server redirect.
 */
typedef void (*Ewk_History_Server_Redirection_Cb)(const Evas_Object *view, const char *source_url, const char *destination_url, void *user_data);

/**
 * @typedef Ewk_History_Title_Update_Cb Ewk_History_Title_Update_Cb
 * @brief Type definition for a function that will be called back when history title is updated.
 */
typedef void (*Ewk_History_Title_Update_Cb)(const Evas_Object *view, const char *title, const char *url, void *user_data);

/**
 * @typedef Ewk_Download_Requested_Cb Ewk_Download_Requested_Cb
 * @brief Type definition for a function that will be called back when new download job is requested.
 */
typedef void (*Ewk_Download_Requested_Cb)(Ewk_Download_Job *download, void *user_data);

/**
 * @typedef Ewk_Download_Failed_Cb Ewk_Download_Failed_Cb
 * @brief Type definition for a function that will be called back when a download job has failed.
 */
typedef void (*Ewk_Download_Failed_Cb)(Ewk_Download_Job_Error *error, void *user_data);

/**
 * @typedef Ewk_Download_Cancelled_Cb Ewk_Download_Cancelled_Cb
 * @brief Type definition for a function that will be called back when a download job is cancelled.
 */
typedef void (*Ewk_Download_Cancelled_Cb)(Ewk_Download_Job *download, void *user_data);

/**
 * @typedef Ewk_Download_Finished_Cb Ewk_Download_Finished_Cb
 * @brief Type definition for a function that will be called back when a download job is finished.
 */
typedef void (*Ewk_Download_Finished_Cb)(Ewk_Download_Job *download, void *user_data);

/**
 * @typedef Ewk_Context_History_Client_Visited_Links_Populate_Cb Ewk_Context_History_Client_Visited_Links_Populate_Cb
 * @brief Type definition for a function that will be called back when client is asked to provide visited links from a client-managed storage.
 *
 * @see ewk_context_visited_link_add
 */
typedef void (*Ewk_History_Populate_Visited_Links_Cb)(void *user_data);

/**
 * Callback to receive message from extension.
 *
 * @param name name of message from extension
 * @param body body of message from extendion
 * @param user_data user_data will be passsed when receiving message from extension
 *
 * @see ewk_context_message_from_extensions_callback_set
 * @see ewk_extension_message_post
 */
typedef void (*Ewk_Context_Message_From_Extension_Cb)(const char *name, const Eina_Value *body, void *user_data);

/**
 * Gets default Ewk_Context instance.
 *
 * The returned Ewk_Context object @b should not be unref'ed if application
 * does not call ewk_context_ref() for that.
 *
 * @return Ewk_Context object.
 */
EAPI Ewk_Context *ewk_context_default_get(void);

/**
 * Creates a new Ewk_Context.
 *
 * The returned Ewk_Context object @b should be unref'ed after use.
 *
 * @return Ewk_Context object on success or @c NULL on failure
 *
 * @see ewk_object_unref
 * @see ewk_context_new_with_extensions_path
 */
EAPI Ewk_Context *ewk_context_new(void);

/**
 * Creates a new Ewk_Context.
 *
 * The returned Ewk_Context object @b should be unref'ed after use.
 *
 * @param path directory path of extensions
 *
 * @return Ewk_Context object on success or @c NULL on failure
 *
 * @note All shared objects which have ewk_extension_init() in the given @a path will be loaded.
 *
 * @see ewk_object_unref
 * @see ewk_context_new
 * @see Ewk_Extension_Initialize_Function
 */
EAPI Ewk_Context *ewk_context_new_with_extensions_path(const char *path);

/**
 * Gets the application cache manager instance for this @a context.
 *
 * @param context context object to query.
 *
 * @return Ewk_Cookie_Manager object instance or @c NULL in case of failure.
 */
EAPI Ewk_Application_Cache_Manager *ewk_context_application_cache_manager_get(const Ewk_Context *context);

/**
 * Gets the cookie manager instance for this @a context.
 *
 * @param context context object to query.
 *
 * @return Ewk_Cookie_Manager object instance or @c NULL in case of failure.
 */
EAPI Ewk_Cookie_Manager *ewk_context_cookie_manager_get(const Ewk_Context *context);

/**
 * Gets the database manager instance for this @a context.
 *
 * @param context context object to query
 *
 * @return Ewk_Database_Manager object instance or @c NULL in case of failure
 */
EAPI Ewk_Database_Manager *ewk_context_database_manager_get(const Ewk_Context *context);

/**
 * Sets the favicon database directory for this @a context.
 *
 * Sets the directory path to be used to store the favicons database
 * for @a context on disk. Passing @c NULL as @a directory_path will
 * result in using the default directory for the platform.
 *
 * Calling this method also means enabling the favicons database for
 * its use from the applications, it is therefore expected to be
 * called only once. Further calls for the same instance of
 * @a context will not have any effect.
 *
 * @param context context object to update
 * @param directory_path database directory path to set
 *
 * @return @c EINA_TRUE if successful, @c EINA_FALSE otherwise
 */
EAPI Eina_Bool ewk_context_favicon_database_directory_set(Ewk_Context *context, const char *directory_path);

/**
 * Gets the favicon database instance for this @a context.
 *
 * @param context context object to query.
 *
 * @return Ewk_Favicon_Database object instance or @c NULL in case of failure.
 */
EAPI Ewk_Favicon_Database *ewk_context_favicon_database_get(const Ewk_Context *context);

/**
 * Gets the storage manager instance for this @a context.
 *
 * @param context context object to query.
 *
 * @return Ewk_Storage_Manager object instance or @c NULL in case of failure.
 */
EAPI Ewk_Storage_Manager *ewk_context_storage_manager_get(const Ewk_Context *context);

/**
 * Register @a scheme in @a context.
 *
 * When an URL request with @a scheme is made in the #Ewk_Context, the callback
 * function provided will be called with a #Ewk_Url_Scheme_Request.
 *
 * It is possible to handle URL scheme requests asynchronously, by calling ewk_object_ref() on the
 * #Ewk_Url_Scheme_Request and calling ewk_url_scheme_request_finish() later when the data of
 * the request is available.
 *
 * To replace registered callback with new callback, calls ewk_context_url_scheme_register()
 * with new callback again.
 *
 * @param context a #Ewk_Context object.
 * @param scheme the network scheme to register
 * @param callback the function to be called when an URL request with @a scheme is made.
 * @param user_data data to pass to callback function
 *
 * @code
 * static void custom_url_scheme_request_cb(Ewk_Url_Scheme_Request *request, void *user_data)
 * {
 *     const char *scheme;
 *     char *contents_data = NULL;
 *     unsigned contents_length = 0;
 *
 *     scheme = ewk_url_scheme_request_scheme_get(request);
 *     if (!strcmp(scheme, "myapp")) {
 *         // Initialize contents_data with a welcome page, and set its length to contents_length
 *     } else {
 *         Eina_Strbuf *buf = eina_strbuf_new();
 *         eina_strbuf_append_printf(buf, "&lt;html&gt;&lt;body&gt;&lt;p&gt;Invalid application: %s&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;", scheme);
 *         contents_data = eina_strbuf_string_steal(buf);
 *         contents_length = strlen(contents_data);
 *         eina_strbuf_free(buf);
 *     }
 *     ewk_url_scheme_request_finish(request, contents_data, contents_length, "text/html");
 *     free(contents_data);
 * }
 * @endcode
 */
EAPI Eina_Bool ewk_context_url_scheme_register(Ewk_Context *context, const char *scheme, Ewk_Url_Scheme_Request_Cb callback, void *user_data);

/**
 * Sets history callbacks for the given @a context.
 *
 * To stop listening for history events, you may call this function with @c
 * NULL for the callbacks.
 *
 * @param context context object to set history callbacks
 * @param navigate_func The function to call when @c ewk_view did navigation (may be @c NULL).
 * @param client_redirect_func The function to call when @c ewk_view performed a client redirect (may be @c NULL).
 * @param server_redirect_func The function to call when @c ewk_view performed a server redirect (may be @c NULL).
 * @param title_update_func The function to call when history title is updated (may be @c NULL).
 * @param populate_visited_links_func The function is called when client is asked to provide visited links from a
 *        client-managed storage (may be @c NULL).
 * @param data User data (may be @c NULL).
 */
EAPI void ewk_context_history_callbacks_set(Ewk_Context *context,
                                            Ewk_History_Navigation_Cb navigate_func,
                                            Ewk_History_Client_Redirection_Cb client_redirect_func,
                                            Ewk_History_Server_Redirection_Cb server_redirect_func,
                                            Ewk_History_Title_Update_Cb title_update_func,
                                            Ewk_History_Populate_Visited_Links_Cb populate_visited_links_func,
                                            void *data);

/**
 * Sets download callbacks for the given @a context.
 *
 * To stop listening for download events, you may call this function with @c
 * NULL for the callbacks.
 *
 * @param context context object to set download callbacks
 * @param download_requested_func the function to call when new download is requested (may be @c NULL).
 * @param download_failed_func the function to call when a download job has failed (may be @c NULL).
 * @param download_cancelled_func the function to call when a download job is cancelled (may be @c NULL).
 * @param download_finished_func the function to call when a download job is finished (may be @c NULL).
 * @param data User data (may be @c NULL).
 */
EAPI void ewk_context_download_callbacks_set(Ewk_Context *context,
                                             Ewk_Download_Requested_Cb download_requested_func,
                                             Ewk_Download_Failed_Cb download_failed_func,
                                             Ewk_Download_Cancelled_Cb download_cancelled_func,
                                             Ewk_Download_Finished_Cb download_finished_func,
                                             void* data);

/**
 * Registers the given @a visited_url as visited link in @a context visited link cache.
 *
 * This function shall be invoked as a response to @c populateVisitedLinks callback of the history cient.
 *
 * @param context context object to add visited link data
 * @param visited_url visited url
 *
 * @see Ewk_Context_History_Client
 */
EAPI void ewk_context_visited_link_add(Ewk_Context *context, const char *visited_url);

/**
 * Set @a cache_model as the cache model for @a context.
 *
 * By default, it is EWK_CACHE_MODEL_DOCUMENT_VIEWER.
 *
 * @param context context object to update.
 * @param cache_model a #Ewk_Cache_Model.
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_context_cache_model_set(Ewk_Context *context, Ewk_Cache_Model cache_model);

/**
 * Gets the cache model for @a context.
 *
 * @param context context object to query.
 *
 * @return the cache model for the @a context.
 */
EAPI Ewk_Cache_Model ewk_context_cache_model_get(const Ewk_Context *context);

/**
 * Sets additional plugin path for @a context.
 *
 * @param context context object to set additional plugin path
 * @param path the path to be used for plugins
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_context_additional_plugin_path_set(Ewk_Context *context, const char *path);

/**
 * Clears HTTP caches in local storage and all resources cached in memory 
 * such as images, CSS, JavaScript, XSL, and fonts for @a context.
 *
 * @param context context object to clear all resource caches
 */
EAPI void ewk_context_resource_cache_clear(Ewk_Context *context);

/**
 * Posts message to extensions asynchronously.
 *
 * @note body only supports @c EINA_VALUE_TYPE_STRINGSHARE or @c EINA_VALUE_TYPE_STRING,
 * now.
 *
 * @param context context object to post message to extensions
 * @param name message name
 * @param body message body
 *
 * @return @c EINA_TRUE on success of @c EINA_FALSE on failure or when the type
 * of @p body is not supported.
 */
EAPI Eina_Bool ewk_context_message_post_to_extensions(Ewk_Context *context, const char *name, const Eina_Value *body);

/**
 * Sets callback for received extension message.
 *
 * Client can pass @c NULL for callback to stop listening for messages.
 *
 * @param context context object
 * @param callback callback for received injected bundle message or @c NULL
 * @param user_data user data
 */
EAPI void ewk_context_message_from_extensions_callback_set(Ewk_Context *context, Ewk_Context_Message_From_Extension_Cb callback, void *user_data);

/**
 * Gets TLS error policy for @a context.
 *
 * @param context context object to get TLS error policy
 *
 * @return The TLS errors policy for the context
 */
EAPI Ewk_TLS_Error_Policy ewk_context_tls_error_policy_get(const Ewk_Context *context);

/**
 * Sets TLS error policy for @a context.
 *
 * Sets how TLS certificate errors should be handled. The available policies are listed in #Ewk_TLS_Error_Policy enumeration.
 *
 * @param context context object to set TLS error policy
 * @param tls_error_policy a #Ewk_TLS_Error_Policy
 */
EAPI void ewk_context_tls_error_policy_set(Ewk_Context *context, Ewk_TLS_Error_Policy tls_error_policy);

/**
 * Sets the list of preferred languages.
 *
 * Sets the list of preferred langages. This list will be used to build the "Accept-Language"
 * header that will be included in the network requests.
 * Client can pass @c NULL for languages to clear what is set before.
 *
 * @param languages the list of preferred languages (char* as data) or @c NULL
 *
 * @note all contexts will be affected.
 */
EAPI void ewk_context_preferred_languages_set(Eina_List *languages);


/**
 * Allows accepting the specified TLS certificate for the speficied host.
 *
 * @param context context object to allow accepting a specific certificate for a specific host
 * @param pem the certificate to be accepted in PEM format
 * @param host the host for which the certificate is to be accepted
 */
EAPI void ewk_context_tls_certificate_for_host_allow(Ewk_Context *context, const char *pem, const char *host);

/**
 * Sets the maximum number of web processes that can be created at the same time for the @a context.
 * The default value is 0 and means no limit.
 *
 * @param context context object to set webprocess count limit
 * @param count the maximum number of web processes
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_context_web_process_count_limit_set(Ewk_Context *context, unsigned count);

/**
 * Gets the maximum number of web processes that can be created at the same time for the @a context.
 *
 * @param context context object to get webprocess count limit
 *
 * @return the maximum number of web processes, or 0 if there isn't a limit
 */
EAPI unsigned ewk_context_web_process_count_limit_get(const Ewk_Context *context);

#ifdef __cplusplus
}
#endif

#endif // ewk_context_h
