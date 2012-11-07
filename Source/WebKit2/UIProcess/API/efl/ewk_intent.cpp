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
#include "ewk_intent.h"

#include "WKAPICast.h"
#include "WKArray.h"
#include "WKDictionary.h"
#include "WKString.h"
#include "WKURL.h"
#include "ewk_intent_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

#if ENABLE(WEB_INTENTS)
EwkIntent::EwkIntent(WKIntentDataRef intentRef)
    : m_wkIntent(intentRef)
    , m_action(AdoptWK, WKIntentDataCopyAction(intentRef))
    , m_type(AdoptWK, WKIntentDataCopyType(intentRef))
    , m_service(AdoptWK, WKIntentDataCopyService(intentRef))
{ }

WebIntentData* EwkIntent::webIntentData() const
{
    return toImpl(m_wkIntent.get());
}

const char* EwkIntent::action() const
{
    return m_action;
}

const char* EwkIntent::type() const
{
    return m_type;
}

const char* EwkIntent::service() const
{
    return m_service;
}

WKRetainPtr<WKArrayRef> EwkIntent::suggestions() const
{
    return adoptWK(WKIntentDataCopySuggestions(m_wkIntent.get()));
}

String EwkIntent::extra(const char* key) const
{
    WKRetainPtr<WKStringRef> keyRef = adoptWK(WKStringCreateWithUTF8CString(key));
    WKRetainPtr<WKStringRef> wkValue(AdoptWK, WKIntentDataCopyExtraValue(m_wkIntent.get(), keyRef.get()));
    return toImpl(wkValue.get())->string();
}

WKRetainPtr<WKArrayRef> EwkIntent::extraKeys() const
{
    WKRetainPtr<WKDictionaryRef> wkExtras(AdoptWK, WKIntentDataCopyExtras(m_wkIntent.get()));
    return adoptWK(WKDictionaryCopyKeys(wkExtras.get()));
}
#endif

const char* ewk_intent_action_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntent, intent, impl, 0);
    return impl->action();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return 0;
#endif
}

const char* ewk_intent_type_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntent, intent, impl, 0);
    return impl->type();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return 0;
#endif
}

const char* ewk_intent_service_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntent, intent, impl, 0);
    return impl->service();
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return 0;
#endif
}

Eina_List* ewk_intent_suggestions_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntent, intent, impl, 0);
    Eina_List* listOfSuggestions = 0;
    WKRetainPtr<WKArrayRef> wkSuggestions = impl->suggestions();
    const size_t numSuggestions = WKArrayGetSize(wkSuggestions.get());
    for (size_t i = 0; i < numSuggestions; ++i) {
        WKURLRef wkSuggestion = static_cast<WKURLRef>(WKArrayGetItemAtIndex(wkSuggestions.get(), i));
        listOfSuggestions = eina_list_append(listOfSuggestions, eina_stringshare_add(toImpl(wkSuggestion)->string().utf8().data()));
    }

    return listOfSuggestions;

#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return 0;
#endif
}

const char* ewk_intent_extra_get(const Ewk_Intent* intent, const char* key)
{
#if ENABLE(WEB_INTENTS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntent, intent, impl, 0);
    String value = impl->extra(key);

    if (value.isEmpty())
        return 0;

    return eina_stringshare_add(value.utf8().data());
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return 0;
#endif
}

Eina_List* ewk_intent_extra_names_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkIntent, intent, impl, 0);
    Eina_List* listOfKeys = 0;
    WKRetainPtr<WKArrayRef> wkKeys = impl->extraKeys();
    const size_t numKeys = WKArrayGetSize(wkKeys.get());
    for (size_t i = 0; i < numKeys; ++i) {
        WKStringRef wkKey = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkKeys.get(), i));
        listOfKeys = eina_list_append(listOfKeys, eina_stringshare_add(toImpl(wkKey)->string().utf8().data()));
    }

    return listOfKeys;
#else
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return 0;
#endif
}
