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
 * @file ewk_intent_service.h
 * @brief Describes the Ewk Intent Service API.
 */

#ifndef ewk_intent_service_h
#define ewk_intent_service_h

#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Declare Ewk_Intent_Service as Ewk_Object.
 *
 * @see Ewk_Object
 */
typedef struct Ewk_Object Ewk_Intent_Service;

/**
 * Query action for this intent service.
 *
 * @param service service item to query.
 *
 * @return the action pointer, that may be @c NULL. This pointer is
 * guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use
 * eina_stringshare_ref() instead of eina_stringshare_add() or
 * strdup().
 */
EAPI const char *ewk_intent_service_action_get(const Ewk_Intent_Service *service);

/**
 * Query type for this intent service.
 *
 * @param service service item to query.
 *
 * @return the type pointer, that may be @c NULL. This pointer is
 * guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use
 * eina_stringshare_ref() instead of eina_stringshare_add() or
 * strdup().
 */
EAPI const char *ewk_intent_service_type_get(const Ewk_Intent_Service *service);

/**
 * Query URL for this intent service.
 *
 * @param service service item to query.
 *
 * @return the URL pointer, that may be @c NULL. This pointer is
 * guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use
 * eina_stringshare_ref() instead of eina_stringshare_add() or
 * strdup().
 */
EAPI const char *ewk_intent_service_href_get(const Ewk_Intent_Service *service);

/**
 * Query title for this intent service.
 *
 * @param service service item to query.
 *
 * @return the title pointer, that may be @c NULL. This pointer is
 * guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use
 * eina_stringshare_ref() instead of eina_stringshare_add() or
 * strdup().
 */
EAPI const char *ewk_intent_service_title_get(const Ewk_Intent_Service *service);

/**
 * Query disposition for this intent service.
 *
 * @param service service item to query.
 *
 * @return the disposition pointer, that may be @c NULL. This pointer is
 * guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use
 * eina_stringshare_ref() instead of eina_stringshare_add() or
 * strdup().
 */
EAPI const char *ewk_intent_service_disposition_get(const Ewk_Intent_Service *service);

#ifdef __cplusplus
}
#endif

#endif // ewk_intent_service_h
