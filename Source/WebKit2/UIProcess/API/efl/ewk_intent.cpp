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
#include "WKEinaSharedString.h"
#include "WKIntentData.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKURL.h"
#include "ewk_intent_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * \struct  _Ewk_Intent
 * @brief   Contains the intent data.
 */
struct _Ewk_Intent {
    unsigned int __ref; /**< the reference count of the object */
#if ENABLE(WEB_INTENTS)
    WKRetainPtr<WKIntentDataRef> wkIntent;
#endif
    WKEinaSharedString action;
    WKEinaSharedString type;
    WKEinaSharedString service;

    _Ewk_Intent(WKIntentDataRef intentRef)
        : __ref(1)
#if ENABLE(WEB_INTENTS)
        , wkIntent(intentRef)
        , action(AdoptWK, WKIntentDataCopyAction(intentRef))
        , type(AdoptWK, WKIntentDataCopyType(intentRef))
        , service(AdoptWK, WKIntentDataCopyService(intentRef))
#endif
    { }

    ~_Ewk_Intent()
    {
        ASSERT(!__ref);
    }
};

#define EWK_INTENT_WK_GET_OR_RETURN(intent, wkIntent_, ...)    \
    if (!(intent)) {                                           \
        EINA_LOG_CRIT("intent is NULL.");                      \
        return __VA_ARGS__;                                    \
    }                                                          \
    if (!(intent)->wkIntent) {                                 \
        EINA_LOG_CRIT("intent->wkIntent is NULL.");            \
        return __VA_ARGS__;                                    \
    }                                                          \
    WKIntentDataRef wkIntent_ = (intent)->wkIntent.get()

void ewk_intent_ref(Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN(intent);
    ++intent->__ref;
#endif
}

void ewk_intent_unref(Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EINA_SAFETY_ON_NULL_RETURN(intent);

    if (--intent->__ref)
        return;

    delete intent;
#endif
}

const char* ewk_intent_action_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

    return intent->action;
}

const char* ewk_intent_type_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

    return intent->type;
}

const char* ewk_intent_service_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);

    return intent->service;
}

Eina_List* ewk_intent_suggestions_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_WK_GET_OR_RETURN(intent, wkIntent, 0);

    Eina_List* listOfSuggestions = 0;
    WKRetainPtr<WKArrayRef> wkSuggestions(AdoptWK, WKIntentDataCopySuggestions(wkIntent));
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
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_WK_GET_OR_RETURN(intent, wkIntent, 0);

    WKRetainPtr<WKStringRef> keyRef = adoptWK(WKStringCreateWithUTF8CString(key));
    WKRetainPtr<WKStringRef> wkValue(AdoptWK, WKIntentDataCopyExtraValue(wkIntent, keyRef.get()));
    String value = toImpl(wkValue.get())->string();
    if (value.isEmpty())
        return 0;

    return eina_stringshare_add(value.utf8().data());
#else
    return 0;
#endif
}

Eina_List* ewk_intent_extra_names_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_WK_GET_OR_RETURN(intent, wkIntent, 0);

    Eina_List* listOfKeys = 0;
    WKRetainPtr<WKDictionaryRef> wkExtras(AdoptWK, WKIntentDataCopyExtras(wkIntent));
    WKRetainPtr<WKArrayRef> wkKeys(AdoptWK, WKDictionaryCopyKeys(wkExtras.get()));
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

#if ENABLE(WEB_INTENTS)
Ewk_Intent* ewk_intent_new(WKIntentDataRef intentData)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intentData, 0);

    return new Ewk_Intent(intentData);
}

WKIntentDataRef ewk_intent_WKIntentDataRef_get(const Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, 0);
    return intent->wkIntent.get();
}
#endif
