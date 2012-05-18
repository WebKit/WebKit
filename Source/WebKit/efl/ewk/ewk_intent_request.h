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

#ifndef ewk_intent_request_h
#define ewk_intent_request_h

#include "ewk_intent.h"

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for _Ewk_Intent */
typedef struct _Ewk_Intent_Request Ewk_Intent_Request;

/**
 * Increases the reference count of the given object.
 *
 * @param request the intent request object to increase the reference count
 */
EAPI void ewk_intent_request_ref(Ewk_Intent_Request *request);

/**
 * Decreases the reference count of the given object, possibly freeing it.
 *
 * When the reference count it's reached 0, the intent request is freed.
 *
 * @param request the intent request object to decrease the reference count
 */
EAPI void ewk_intent_request_unref(Ewk_Intent_Request *request);

/**
 * Query intent for this request.
 *
 * @param request request item to query.
 *
 * @return A pointer to the intent instance.
 */
EAPI Ewk_Intent *ewk_intent_request_intent_get(const Ewk_Intent_Request *request);

/**
 * Report request success.
 *
 * The payload data will be passed to the success callback registered by the client.
 *
 * @param request request item.
 * @param result payload data.
 */
EAPI void ewk_intent_request_result_post(Ewk_Intent_Request *request, const char *result);

/**
 * Report request failure.
 *
 * The payload data will be passed to the error callback registered by the client.
 *
 * @param request request item.
 * @param failure payload data.
 */
EAPI void ewk_intent_request_failure_post(Ewk_Intent_Request *request, const char *failure);

#ifdef __cplusplus
}
#endif
#endif // ewk_intent_request_h
