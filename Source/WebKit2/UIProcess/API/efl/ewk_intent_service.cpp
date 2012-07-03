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
#if ENABLE(WEB_INTENTS_TAG)
    WKRetainPtr<WKIntentServiceInfoRef> wkService;
#endif
    const char* action;
    const char* type;
    const char* href;
    const char* title;
    const char* disposition;
};

#define EWK_INTENT_SERVICE_WK_GET_OR_RETURN(service, wkService_, ...) \
    if (!(service)) { \
        EINA_LOG_CRIT("service is NULL."); \
        return __VA_ARGS__; \
    } \
    if (!(service)->wkService) { \
        EINA_LOG_CRIT("service->wkService is NULL."); \
        return __VA_ARGS__; \
    } \
    WKIntentServiceInfoRef wkService_ = (service)->wkService.get()

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

    eina_stringshare_del(service->action);
    eina_stringshare_del(service->type);
    eina_stringshare_del(service->href);
    eina_stringshare_del(service->title);
    eina_stringshare_del(service->disposition);
    free(service);
#endif
}

const char* ewk_intent_service_action_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_INTENT_SERVICE_WK_GET_OR_RETURN(service, wkService, 0);

    WKRetainPtr<WKStringRef> wkAction(AdoptWK, WKIntentServiceInfoCopyAction(wkService));
    Ewk_Intent_Service* ewkIntentService = const_cast<Ewk_Intent_Service*>(service);
    eina_stringshare_replace(&ewkIntentService->action, toImpl(wkAction.get())->string().utf8().data());

    return service->action;
#else
    return 0;
#endif
}

const char* ewk_intent_service_type_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_INTENT_SERVICE_WK_GET_OR_RETURN(service, wkService, 0);

    WKRetainPtr<WKStringRef> wkType(AdoptWK, WKIntentServiceInfoCopyType(wkService));
    Ewk_Intent_Service* ewkIntentService = const_cast<Ewk_Intent_Service*>(service);
    eina_stringshare_replace(&ewkIntentService->type, toImpl(wkType.get())->string().utf8().data());

    return service->type;
#else
    return 0;
#endif
}

const char* ewk_intent_service_href_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_INTENT_SERVICE_WK_GET_OR_RETURN(service, wkService, 0);

    WKRetainPtr<WKURLRef> wkHref(AdoptWK, WKIntentServiceInfoCopyHref(wkService));
    Ewk_Intent_Service* ewkIntentService = const_cast<Ewk_Intent_Service*>(service);
    eina_stringshare_replace(&ewkIntentService->href, toImpl(wkHref.get())->string().utf8().data());

    return service->href;
#else
    return 0;
#endif
}

const char* ewk_intent_service_title_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_INTENT_SERVICE_WK_GET_OR_RETURN(service, wkService, 0);

    WKRetainPtr<WKStringRef> wkTitle(AdoptWK, WKIntentServiceInfoCopyTitle(wkService));
    Ewk_Intent_Service* ewkIntentService = const_cast<Ewk_Intent_Service*>(service);
    eina_stringshare_replace(&ewkIntentService->title, toImpl(wkTitle.get())->string().utf8().data());

    return service->title;
#else
    return 0;
#endif
}

const char* ewk_intent_service_disposition_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_INTENT_SERVICE_WK_GET_OR_RETURN(service, wkService, 0);

    WKRetainPtr<WKStringRef> wkDisposition(AdoptWK, WKIntentServiceInfoCopyDisposition(wkService));
    Ewk_Intent_Service* ewkIntentService = const_cast<Ewk_Intent_Service*>(service);
    eina_stringshare_replace(&ewkIntentService->disposition, toImpl(wkDisposition.get())->string().utf8().data());

    return service->disposition;
#else
    return 0;
#endif
}

#if ENABLE(WEB_INTENTS_TAG)
Ewk_Intent_Service* ewk_intent_service_new(WKIntentServiceInfoRef wkService)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(wkService, 0);

    Ewk_Intent_Service* ewkIntentService = static_cast<Ewk_Intent_Service*>(calloc(1, sizeof(Ewk_Intent_Service)));
    ewkIntentService->__ref = 1;
    ewkIntentService->wkService = wkService;

    return ewkIntentService;
}
#endif
