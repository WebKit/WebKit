/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    ewk_intent.h
 * @brief   Describes the Ewk Intent API.
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
 * Increases the reference count of the given object.
 *
 * @param intent the intent object to increase the reference count
 */
EAPI void ewk_intent_ref(Ewk_Intent *intent);

/**
 * Decreases the reference count of the given object, possibly freeing it.
 *
 * When the reference count reaches 0, the intent is freed.
 *
 * @param intent the intent object to decrease the reference count
 */
EAPI void ewk_intent_unref(Ewk_Intent *intent);

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
 *         the Eina_List and its items should be freed after use. Use eina_stringshare_del()
 *         to free the items.
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
 * The item of a returned list should be freed by eina_stringshare_del() after use.
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
