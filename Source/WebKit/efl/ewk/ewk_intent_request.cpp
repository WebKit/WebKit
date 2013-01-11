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

#include "config.h"
#include "ewk_intent_request.h"

#include "IntentRequest.h"
#include "SerializedScriptValue.h"
#include "ewk_intent_private.h"
#include "ewk_private.h"

/**
 * \struct  _Ewk_Intent_Request
 * @brief   Contains the intent request data.
 */
struct _Ewk_Intent_Request {
    unsigned int __ref; /**< the reference count of the object */
#if ENABLE(WEB_INTENTS)
    RefPtr<WebCore::IntentRequest> core;
#endif
    Ewk_Intent* intent;
};

#define EWK_INTENT_REQUEST_CORE_GET_OR_RETURN(request, core_, ...)      \
    if (!(request)) {                                                   \
        CRITICAL("request is NULL.");                                   \
        return __VA_ARGS__;                                             \
    }                                                                   \
    if (!(request)->core) {                                             \
        CRITICAL("request->core is NULL.");                             \
        return __VA_ARGS__;                                             \
    }                                                                   \
    RefPtr<WebCore::IntentRequest> core_ = (request)->core

void ewk_intent_request_ref(Ewk_Intent_Request* request)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN(request);
    ++request->__ref;
#else
    UNUSED_PARAM(request);
#endif
}

void ewk_intent_request_unref(Ewk_Intent_Request* request)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN(request);

    if (--request->__ref)
        return;

    ewk_intent_free(request->intent);
    delete request;
#else
    UNUSED_PARAM(request);
#endif
}

Ewk_Intent* ewk_intent_request_intent_get(const Ewk_Intent_Request* request)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->intent;
#else
    UNUSED_PARAM(request);
    return 0;
#endif
}

#if ENABLE(WEB_INTENTS)
/**
 * @internal
 *
 * Report request success.
 *
 * The serialized payload data will be passed to the success callback registered by the
 * client.
 *
 * @param request request item.
 * @param result serialized payload data.
 */
void ewk_intent_request_result_post(Ewk_Intent_Request* request, PassRefPtr<WebCore::SerializedScriptValue> result)
{
    EWK_INTENT_REQUEST_CORE_GET_OR_RETURN(request, core);

    core->postResult(result.get());
}

/**
 * @internal
 *
 * Report request failure.
 *
 * The serialized payload data will be passed to the error callback registered by the
 * client.
 *
 * @param request request item.
 * @param failure serialized payload data.
 */
void ewk_intent_request_failure_post(Ewk_Intent_Request* request, PassRefPtr<WebCore::SerializedScriptValue> failure)
{
    EWK_INTENT_REQUEST_CORE_GET_OR_RETURN(request, core);

    core->postFailure(failure.get());
}

/**
 * @internal
 *
 * Creates a new Ewk_Intent_Request object.
 *
 * @param core WebCore::IntentRequest instance to use internally.
 * @return a new allocated the Ewk_Intent_Request object on sucess or @c 0 on failure
 */
Ewk_Intent_Request* ewk_intent_request_new(PassRefPtr<WebCore::IntentRequest> core)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(core, 0);
    Ewk_Intent_Request* request = new Ewk_Intent_Request;
    request->__ref = 1;
    request->core = core;
    request->intent = ewk_intent_new(request->core->intent());

    return request;
}
#endif
