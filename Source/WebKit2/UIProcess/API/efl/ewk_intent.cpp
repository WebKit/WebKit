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
Ewk_Intent::Ewk_Intent(WKIntentDataRef intentRef)
    : m_wkIntent(intentRef)
    , m_action(AdoptWK, WKIntentDataCopyAction(intentRef))
    , m_type(AdoptWK, WKIntentDataCopyType(intentRef))
    , m_service(AdoptWK, WKIntentDataCopyService(intentRef))
{ }

WebIntentData* Ewk_Intent::webIntentData() const
{
    return toImpl(m_wkIntent.get());
}

const char* Ewk_Intent::action() const
{
    return m_action;
}

const char* Ewk_Intent::type() const
{
    return m_type;
}

const char* Ewk_Intent::service() const
{
    return m_service;
}

WKRetainPtr<WKArrayRef> Ewk_Intent::suggestions() const
{
    return adoptWK(WKIntentDataCopySuggestions(m_wkIntent.get()));
}

String Ewk_Intent::extra(const char* key) const
{
    WKRetainPtr<WKStringRef> keyRef = adoptWK(WKStringCreateWithUTF8CString(key));
    WKRetainPtr<WKStringRef> wkValue(AdoptWK, WKIntentDataCopyExtraValue(m_wkIntent.get(), keyRef.get()));
    return toImpl(wkValue.get())->string();
}

WKRetainPtr<WKArrayRef> Ewk_Intent::extraKeys() const
{
    WKRetainPtr<WKDictionaryRef> wkExtras(AdoptWK, WKIntentDataCopyExtras(m_wkIntent.get()));
    return adoptWK(WKDictionaryCopyKeys(wkExtras.get()));
}
#endif

Ewk_Intent* ewk_intent_ref(Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

    intent->ref();
#endif

    return intent;
}

void ewk_intent_unref(Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN(intent);

    intent->deref();
#endif
}

const char* ewk_intent_action_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

#if ENABLE(WEB_INTENTS)
    return intent->action();
#endif
}

const char* ewk_intent_type_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

#if ENABLE(WEB_INTENTS)
    return intent->type();
#endif
}

const char* ewk_intent_service_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

#if ENABLE(WEB_INTENTS)
    return intent->service();
#endif
}

Eina_List* ewk_intent_suggestions_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

#if ENABLE(WEB_INTENTS)
    Eina_List* listOfSuggestions = 0;
    WKRetainPtr<WKArrayRef> wkSuggestions = intent->suggestions();
    const size_t numSuggestions = WKArrayGetSize(wkSuggestions.get());
    for (size_t i = 0; i < numSuggestions; ++i) {
        WKURLRef wkSuggestion = static_cast<WKURLRef>(WKArrayGetItemAtIndex(wkSuggestions.get(), i));
        listOfSuggestions = eina_list_append(listOfSuggestions, eina_stringshare_add(toImpl(wkSuggestion)->string().utf8().data()));
    }

    return listOfSuggestions;

#else
    return 0;
#endif
}

const char* ewk_intent_extra_get(const Ewk_Intent* intent, const char* key)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

#if ENABLE(WEB_INTENTS)
    String value = intent->extra(key);

    if (value.isEmpty())
        return 0;

    return eina_stringshare_add(value.utf8().data());
#else
    return 0;
#endif
}

Eina_List* ewk_intent_extra_names_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

#if ENABLE(WEB_INTENTS)
    Eina_List* listOfKeys = 0;
    WKRetainPtr<WKArrayRef> wkKeys = intent->extraKeys();
    const size_t numKeys = WKArrayGetSize(wkKeys.get());
    for (size_t i = 0; i < numKeys; ++i) {
        WKStringRef wkKey = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkKeys.get(), i));
        listOfKeys = eina_list_append(listOfKeys, eina_stringshare_add(toImpl(wkKey)->string().utf8().data()));
    }

    return listOfKeys;
#else
    return 0;
#endif
}
