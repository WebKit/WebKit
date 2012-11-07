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
#include "WKRetainPtr.h"
#include "WKURL.h"
#include "ewk_intent_service_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

#if ENABLE(WEB_INTENTS_TAG)
EwkIntentService::EwkIntentService(WKIntentServiceInfoRef serviceRef)
    : m_action(AdoptWK, WKIntentServiceInfoCopyAction(serviceRef))
    , m_type(AdoptWK, WKIntentServiceInfoCopyType(serviceRef))
    , m_href(AdoptWK, WKIntentServiceInfoCopyHref(serviceRef))
    , m_title(AdoptWK, WKIntentServiceInfoCopyTitle(serviceRef))
    , m_disposition(AdoptWK, WKIntentServiceInfoCopyDisposition(serviceRef))
{ }

const char* EwkIntentService::action() const
{
    return m_action;
}

const char* EwkIntentService::type() const
{
    return m_type;
}

const char* EwkIntentService::href() const
{
    return m_href;
}

const char* EwkIntentService::title() const
{
    return m_title;
}

const char* EwkIntentService::disposition() const
{
    return m_disposition;
}
#endif

const char* ewk_intent_service_action_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntentService, service, impl, 0);
    return impl->action();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);
    return 0;
#endif
}

const char* ewk_intent_service_type_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntentService, service, impl, 0);
    return impl->type();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);
    return 0;
#endif
}

const char* ewk_intent_service_href_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntentService, service, impl, 0);
    return impl->href();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);
    return 0;
#endif
}

const char* ewk_intent_service_title_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntentService, service, impl, 0);
    return impl->title();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);
    return 0;
#endif
}

const char* ewk_intent_service_disposition_get(const Ewk_Intent_Service* service)
{
#if ENABLE(WEB_INTENTS_TAG)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntentService, service, impl, 0);
    return impl->disposition();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(service, 0);
    return 0;
#endif
}
