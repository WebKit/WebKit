/*
    Copyright (C) 2012 Intel Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef ewk_intent_h
#define ewk_intent_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for _Ewk_Intent */
typedef struct _Ewk_Intent Ewk_Intent;

/**
 * Query action for this intent.
 *
 * @param intent intent item to query.
 *
 * @return the action pointer, that may be @c NULL. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
EAPI const char *ewk_intent_action_get(const Ewk_Intent *intent);

/**
 * Query type for this intent.
 *
 * @param intent intent item to query.
 *
 * @return the type pointer, that may be @c NULL. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
EAPI const char *ewk_intent_type_get(const Ewk_Intent *intent);

/**
 * Query service for this intent.
 *
 * @param intent intent item to query.
 *
 * @return the service pointer, that may be @c NULL. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
EAPI const char *ewk_intent_service_get(const Ewk_Intent *intent);

/**
 * Query suggestions for this intent.
 *
 * This function provides a list of (absolute) suggested Service URLs of which the Client
 * is aware and which can handle the intent.
 *
 * @param intent intent item to query.
 *
 * @return @c Eina_List with suggested service URLs on success, or @c NULL on failure,
 *         the Eina_List and its items should be freed after use. Use free() to free the
 *         items.
 */
EAPI Eina_List *ewk_intent_suggestions_get(const Ewk_Intent *intent);

/**
 * Retrieves the value (if any) from the extra data dictionary this intent was constructed with.
 *
 * The returned string should be freed by eina_stringshare_del() after use.
 *
 * @param intent intent item to query.
 * @param key key to query in the dictionary.
 *
 * @return a newly allocated string or @c NULL in case of error or if the key does not exist.
 */
EAPI const char *ewk_intent_extra_get(const Ewk_Intent *intent, const char *key);

/**
 * Retrieve a list of the names of extra metadata associated with the intent.
 *
 * @param intent intent item to query.
 *
 * @return @c Eina_List with names of extra metadata on success, or @c NULL on failure,
 *         the Eina_List and its items should be freed after use. Use eina_stringshare_del()
 *         to free the items.
 */
EAPI Eina_List *ewk_intent_extra_names_get(const Ewk_Intent *intent);

#ifdef __cplusplus
}
#endif
#endif // ewk_intent_h
