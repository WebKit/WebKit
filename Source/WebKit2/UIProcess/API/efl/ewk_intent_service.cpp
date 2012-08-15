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

#include "config.h"
#include "ewk_intent_service.h"

#include "IntentServiceInfo.h"
#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKIntentServiceInfo.h"
#include "WKRetainPtr.h"
#include "WKURL.h"
#include "ewk_intent_service_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * \struct _Ewk_Intent_Service
 * @brief Contains the intent service data.
 */
struct _Ewk_Intent_Service {
    unsigned int __ref; /**< the reference count of the object */

    WKEinaSharedString action;
    WKEinaSharedString type;
    WKEinaSharedString href;
    WKEinaSharedString title;
    WKEinaSharedString disposition;

    _Ewk_Intent_Service(WKIntentServiceInfoRef serviceRef)
        : __ref(1)
#if ENABLE(WEB_INTENTS_TAG)
        , action(AdoptWK, WKIntentServiceInfoCopyAction(serviceRef))
        , type(AdoptWK, WKIntentServiceInfoCopyType(serviceRef))
        , href(AdoptWK, WKIntentServiceInfoCopyHref(serviceRef))
        , title(AdoptWK, WKIntentServiceInfoCopyTitle(serviceRef))
        , disposition(AdoptWK, WKIntentServiceInfoCopyDisposition(serviceRef))
#endif
    { }

    ~_Ewk_Intent_Service()
    {
        ASSERT(!__ref);
    }
};

void ewk_intent_service_ref(Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EINA_SAFETY_ON_NULL_RETURN(service);
    ++service->__ref;
#endif
}

void ewk_intent_service_unref(Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EINA_SAFETY_ON_NULL_RETURN(service);

    if (--service->__ref)
        return;

    delete service;
#endif
}

const char* ewk_intent_service_action_get(const Ewk_Intent_Service* service)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);

    return service->action;
}

const char* ewk_intent_service_type_get(const Ewk_Intent_Service* service)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);

    return service->type;
}

const char* ewk_intent_service_href_get(const Ewk_Intent_Service* service)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);

    return service->href;
}

const char* ewk_intent_service_title_get(const Ewk_Intent_Service* service)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);

    return service->title;
}

const char* ewk_intent_service_disposition_get(const Ewk_Intent_Service* service)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);

    return service->disposition;
}

#if ENABLE(WEB_INTENTS_TAG)
Ewk_Intent_Service* ewk_intent_service_new(WKIntentServiceInfoRef wkService)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(wkService, 0);

    return new Ewk_Intent_Service(wkService);
}
#endif
