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
#include "ewk_intent.h"

#include "Intent.h"
#include "ewk_intent_private.h"
#include "ewk_private.h"
#include <KURL.h>
#include <eina_safety_checks.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

/**
 * \struct  _Ewk_Intent
 * @brief   Contains the intent data.
 */
struct _Ewk_Intent {
#if ENABLE(WEB_INTENTS)
    WebCore::Intent* core; /**< WebCore object which is responsible for the intent */
#endif
    const char* action;
    const char* type;
    const char* service;
};

#define EWK_INTENT_CORE_GET_OR_RETURN(intent, core_, ...)      \
    if (!(intent)) {                                           \
        CRITICAL("intent is NULL.");                           \
        return __VA_ARGS__;                                    \
    }                                                          \
    if (!(intent)->core) {                                     \
        CRITICAL("intent->core is NULL.");                     \
        return __VA_ARGS__;                                    \
    }                                                          \
    WebCore::Intent* core_ = (intent)->core

const char* ewk_intent_action_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);

    // hide the following optimization from outside
    Ewk_Intent* ewkIntent = const_cast<Ewk_Intent*>(intent);
    eina_stringshare_replace(&ewkIntent->action,
                             core->action().utf8().data());
    return ewkIntent->action;
#else
    return 0;
#endif
}

const char* ewk_intent_type_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);

    // hide the following optimization from outside
    Ewk_Intent* ewkIntent = const_cast<Ewk_Intent*>(intent);
    eina_stringshare_replace(&ewkIntent->type,
                             core->type().utf8().data());
    return ewkIntent->type;
#else
    return 0;
#endif
}

const char* ewk_intent_service_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);

    // hide the following optimization from outside
    Ewk_Intent* ewkIntent = const_cast<Ewk_Intent*>(intent);
    eina_stringshare_replace(&ewkIntent->service,
                             core->service().string().utf8().data());
    return ewkIntent->service;
#else
    return 0;
#endif
}

Eina_List* ewk_intent_suggestions_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);

    Eina_List* listOfSuggestions = 0;
    Vector<WebCore::KURL>::const_iterator it;
    for (it = core->suggestions().begin(); it != core->suggestions().end(); ++it)
        listOfSuggestions = eina_list_append(listOfSuggestions, eina_stringshare_add(it->string().utf8().data()));
    return listOfSuggestions;
#else
    return 0;
#endif
}

const char* ewk_intent_extra_get(const Ewk_Intent* intent, const char* key)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);
    WTF::HashMap<String, String>::const_iterator val = core->extras().find(String::fromUTF8(key));
    if (val == core->extras().end())
        return 0;
    return eina_stringshare_add(val->value.utf8().data());
#else
    return 0;
#endif
}

Eina_List* ewk_intent_extra_names_get(const Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);
    Eina_List* listOfNames = 0;
    WTF::HashMap<String, String>::const_iterator::Keys it = core->extras().begin().keys();
    for (; it != core->extras().end().keys(); ++it)
        listOfNames = eina_list_append(listOfNames, eina_stringshare_add(it->utf8().data()));
    return listOfNames;
#else
    return 0;
#endif
}

#if ENABLE(WEB_INTENTS)
/**
 * @internal
 *
 * Creates a new Ewk_Intent object.
 *
 * @param core WebCore::Intent instance to use internally.
 * @return a new allocated the Ewk_Intent object on success or @c 0 on failure
 */
Ewk_Intent* ewk_intent_new(WebCore::Intent* core)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(core, 0);
    Ewk_Intent* intent = new Ewk_Intent;
    memset(intent, 0, sizeof(Ewk_Intent));
    intent->core = core;

    return intent;
}

/**
 * @internal
 *
 * Free given Ewk_Intent instance.
 *
 * @param intent the object to free.
 */
void ewk_intent_free(Ewk_Intent* intent)
{
    EINA_SAFETY_ON_NULL_RETURN(intent);

    eina_stringshare_del(intent->action);
    eina_stringshare_del(intent->type);
    eina_stringshare_del(intent->service);
    delete intent;
}

namespace EWKPrivate {

WebCore::Intent* coreIntent(const Ewk_Intent* intent)
{
    EWK_INTENT_CORE_GET_OR_RETURN(intent, core, 0);
    return core;
}

} // namespace EWKPrivate
#endif
