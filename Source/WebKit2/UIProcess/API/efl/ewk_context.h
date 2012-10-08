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

#include "ewk_cookie_manager.h"
#include "ewk_url_scheme_request.h"
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for @a _Ewk_Context. */
typedef struct _Ewk_Context Ewk_Context;

/**
 * @typedef Ewk_Url_Scheme_Request_Cb Ewk_Url_Scheme_Request_Cb
 * @brief Callback type for use with ewk_context_url_scheme_register().
 */
typedef void (*Ewk_Url_Scheme_Request_Cb) (Ewk_Url_Scheme_Request *request, void *user_data);

/**
 * @typedef Ewk_Vibration_Client_Vibrate_Cb Ewk_Vibration_Client_Vibrate_Cb
 * @brief Type definition for a function that will be called back when vibrate
 * request receiveed from the vibration controller.
 */
typedef void (*Ewk_Vibration_Client_Vibrate_Cb)(uint64_t vibration_time, void *user_data);

/**
 * @typedef Ewk_Vibration_Client_Vibration_Cancel_Cb Ewk_Vibration_Client_Vibration_Cancel_Cb
 * @brief Type definition for a function that will be called back when cancel
 * vibration request receiveed from the vibration controller.
 */
typedef void (*Ewk_Vibration_Client_Vibration_Cancel_Cb)(void *user_data);

/**
 * Increases the reference count of the given object.
 *
 * @param context context object to increase the reference count
 *
 * @return Ewk_Context object on success or @c NULL on failure
 */
EAPI Ewk_Context *ewk_context_ref(Ewk_Context *context);

/**
 * Decreases the reference count of the given object, possibly freeing it.
 *
 * When the reference count it's reached 0, the Ewk_Context is freed.
 *
 * @param context context object to decrease the reference count
 */
EAPI void ewk_context_unref(Ewk_Context *context);

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
 * @see ewk_context_unref
 * @see ewk_context_new_with_injected_bundle_path
 */
EAPI Ewk_Context *ewk_context_new(void);

/**
 * Creates a new Ewk_Context.
 *
 * The returned Ewk_Context object @b should be unref'ed after use.
 *
 * @param path path of injected bundle library
 *
 * @return Ewk_Context object on success or @c NULL on failure
 *
 * @see ewk_context_unref
 * @see ewk_context_new
 */
EAPI Ewk_Context *ewk_context_new_with_injected_bundle_path(const char *path);

/**
 * Gets the cookie manager instance for this @a context.
 *
 * @param context context object to query.
 *
 * @return Ewk_Cookie_Manager object instance or @c NULL in case of failure.
 */
EAPI Ewk_Cookie_Manager *ewk_context_cookie_manager_get(const Ewk_Context *context);

/**
 * Register @a scheme in @a context.
 *
 * When an URL request with @a scheme is made in the #Ewk_Context, the callback
 * function provided will be called with a #Ewk_Url_Scheme_Request.
 *
 * It is possible to handle URL scheme requests asynchronously, by calling ewk_url_scheme_ref() on the
 * #Ewk_Url_Scheme_Request and calling ewk_url_scheme_request_finish() later when the data of
 * the request is available.
 *
 * @param context a #Ewk_Context object.
 * @param scheme the network scheme to register
 * @param callback the function to be called when an URL request with @a scheme is made.
 * @param user_data data to pass to callback function
 *
 * @code
 * static void about_url_scheme_request_cb(Ewk_Url_Scheme_Request *request, void *user_data)
 * {
 *     const char *path;
 *     char *contents_data = NULL;
 *     unsigned int contents_length = 0;
 *
 *     path = ewk_url_scheme_request_path_get(request);
 *     if (!strcmp(path, "plugins")) {
 *         // Initialize contents_data with the contents of plugins about page, and set its length to contents_length
 *     } else if (!strcmp(path, "memory")) {
 *         // Initialize contents_data with the contents of memory about page, and set its length to contents_length
 *     } else if (!strcmp(path, "applications")) {
 *         // Initialize contents_data with the contents of application about page, and set its length to contents_length
 *     } else {
 *         Eina_Strbuf *buf = eina_strbuf_new();
 *         eina_strbuf_append_printf(buf, "&lt;html&gt;&lt;body&gt;&lt;p&gt;Invalid about:%s page&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;", path);
 *         contents_data = eina_strbuf_string_steal(buf);
 *         contents_length = strlen(contents);
 *         eina_strbuf_free(buf);
 *     }
 *     ewk_url_scheme_request_finish(request, contents_data, contents_length, "text/html");
 *     free(contents_data);
 * }
 * @endcode
 */
EAPI Eina_Bool ewk_context_url_scheme_register(Ewk_Context *context, const char *scheme, Ewk_Url_Scheme_Request_Cb callback, void *user_data);

/**
 * Sets vibration client callbacks to handle the tactile feedback in the form of
 * vibration in the client application when the content asks for vibration.
 *
 * To stop listening for vibration events, you may call this function with @c
 * NULL for the callbacks.
 *
 * @param context context object to set vibration client callbacks.
 * @param vibrate The function to call when the vibrate request received from the
 *        controller (may be @c NULL).
 * @param cancel The function to call when the cancel vibration request received
 *        from the controller (may be @c NULL).
 * @param data User data (may be @c NULL).
 */
EAPI void ewk_context_vibration_client_callbacks_set(Ewk_Context *context, Ewk_Vibration_Client_Vibrate_Cb vibrate, Ewk_Vibration_Client_Vibration_Cancel_Cb cancel, void *data);

#ifdef __cplusplus
}
#endif

#endif // ewk_context_h
