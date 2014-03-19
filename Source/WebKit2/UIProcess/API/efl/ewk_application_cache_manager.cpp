/*
 * Copyright (C) 2014 Samsung Electronics
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
#include "ewk_application_cache_manager.h"

#include "WKArray.h"
#include "ewk_application_cache_manager_private.h"
#include "ewk_object.h"
#include "ewk_security_origin_private.h"

using namespace WebKit;

EwkApplicationCacheManager::EwkApplicationCacheManager(WKApplicationCacheManagerRef applicationCacheManager)
    : m_applicationCacheManager(applicationCacheManager)
{
}

EwkApplicationCacheManager::~EwkApplicationCacheManager()
{
}

struct EwkApplicationCacheOriginsAsyncData {
    Ewk_Application_Cache_Origins_Async_Get_Cb callback;
    void* userData;

    EwkApplicationCacheOriginsAsyncData(Ewk_Application_Cache_Origins_Async_Get_Cb callback, void* userData)
        : callback(callback)
        , userData(userData)
    { }
};

static void getApplicationCacheOriginsCallback(WKArrayRef wkOrigins, WKErrorRef, void* context)
{
    Eina_List* origins = nullptr;
    auto callbackData = std::unique_ptr<EwkApplicationCacheOriginsAsyncData>(static_cast<EwkApplicationCacheOriginsAsyncData*>(context));

    const size_t originsCount = WKArrayGetSize(wkOrigins);
    for (size_t i = 0; i < originsCount; ++i) {
        WKSecurityOriginRef securityOriginRef = static_cast<WKSecurityOriginRef>(WKArrayGetItemAtIndex(wkOrigins, i));
        origins = eina_list_append(origins, EwkSecurityOrigin::create(securityOriginRef).leakRef());
    }

    callbackData->callback(origins, callbackData->userData);
}

void ewk_application_cache_manager_origins_async_get(const Ewk_Application_Cache_Manager* manager, Ewk_Application_Cache_Origins_Async_Get_Cb callback, void* userData)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    EwkApplicationCacheOriginsAsyncData* callbackData = new EwkApplicationCacheOriginsAsyncData(callback, userData);
    WKApplicationCacheManagerGetApplicationCacheOrigins(manager->impl(), callbackData, getApplicationCacheOriginsCallback);
}

Eina_Bool ewk_application_cache_manager_delete_all(Ewk_Application_Cache_Manager* manager)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(manager, false);

    WKApplicationCacheManagerDeleteAllEntries(manager->impl());

    return true;
}

Eina_Bool ewk_application_cache_manager_delete(Ewk_Application_Cache_Manager* manager, Ewk_Security_Origin* origin)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(manager, false);
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkSecurityOrigin, origin, impl, false);

    WKApplicationCacheManagerDeleteEntriesForOrigin(manager->impl(), impl->wkSecurityOrigin());

    return true;
}
