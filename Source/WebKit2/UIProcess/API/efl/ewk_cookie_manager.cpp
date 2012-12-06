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
#include "ewk_cookie_manager.h"

#include "WKAPICast.h"
#include "WKArray.h"
#include "WKString.h"
#include "WebCookieManagerProxy.h"
#include "ewk_cookie_manager_private.h"
#include "ewk_error_private.h"
#include "ewk_private.h"
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

EwkCookieManager::EwkCookieManager(PassRefPtr<WebCookieManagerProxy> cookieManager)
    : m_cookieManager(cookieManager)
{
    ASSERT(m_cookieManager);

    WKCookieManagerClient wkCookieManagerClient = {
        kWKCookieManagerClientCurrentVersion,
        this, // clientInfo
        cookiesDidChange
    };
    WKCookieManagerSetClient(toAPI(m_cookieManager.get()), &wkCookieManagerClient);
}

EwkCookieManager::~EwkCookieManager()
{
    if (isWatchingForChanges())
        m_cookieManager->stopObservingCookieChanges();
}

void EwkCookieManager::setPersistentStorage(const String& filename, SoupCookiePersistentStorageType storage)
{
    bool isWatchingChanges = isWatchingForChanges();
    if (isWatchingChanges)
        m_cookieManager->stopObservingCookieChanges();

    m_cookieManager->setCookiePersistentStorage(filename, storage);

    if (isWatchingChanges)
        m_cookieManager->startObservingCookieChanges();
}

void EwkCookieManager::setHTTPAcceptPolicy(WKHTTPCookieAcceptPolicy policy)
{
    m_cookieManager->setHTTPCookieAcceptPolicy(toHTTPCookieAcceptPolicy(policy));
}

void EwkCookieManager::clearHostnameCookies(const String& hostname)
{
    m_cookieManager->deleteCookiesForHostname(hostname);
}

void EwkCookieManager::clearAllCookies()
{
    m_cookieManager->deleteAllCookies();
}

void EwkCookieManager::watchChanges(const Cookie_Change_Handler& changeHandler)
{
    m_changeHandler = changeHandler;

    if (changeHandler.callback)
        m_cookieManager->startObservingCookieChanges();
    else
        m_cookieManager->stopObservingCookieChanges();
}

bool EwkCookieManager::isWatchingForChanges() const
{
    return static_cast<bool>(m_changeHandler.callback);
}

void EwkCookieManager::getHostNamesWithCookies(WKCookieManagerGetCookieHostnamesFunction callback, void* userData) const
{
    m_cookieManager->getHostnamesWithCookies(ArrayCallback::create(userData, callback));
}

void EwkCookieManager::getHTTPAcceptPolicy(WKCookieManagerGetHTTPCookieAcceptPolicyFunction callback, void* userData) const
{
    m_cookieManager->getHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicyCallback::create(userData, callback));
}

void EwkCookieManager::cookiesDidChange(WKCookieManagerRef, const void* clientInfo)
{
    EwkCookieManager* manager = static_cast<EwkCookieManager*>(const_cast<void*>(clientInfo));

    if (!manager->isWatchingForChanges())
        return;

    manager->m_changeHandler.callback(manager->m_changeHandler.userData);
}

// Ewk_Cookie_Persistent_Storage enum validation.
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_PERSISTENT_STORAGE_TEXT, SoupCookiePersistentStorageText);
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_PERSISTENT_STORAGE_SQLITE, SoupCookiePersistentStorageSQLite);

void ewk_cookie_manager_persistent_storage_set(Ewk_Cookie_Manager* manager, const char* filename, Ewk_Cookie_Persistent_Storage storage)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);
    EINA_SAFETY_ON_NULL_RETURN(filename);

    manager->setPersistentStorage(String::fromUTF8(filename), static_cast<SoupCookiePersistentStorageType>(storage));
}

// Ewk_Cookie_Accept_Policy enum validation.
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_ACCEPT_POLICY_ALWAYS, kWKHTTPCookieAcceptPolicyAlways);
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_ACCEPT_POLICY_NEVER, kWKHTTPCookieAcceptPolicyNever);
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_ACCEPT_POLICY_NO_THIRD_PARTY, kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain);

void ewk_cookie_manager_accept_policy_set(Ewk_Cookie_Manager* manager, Ewk_Cookie_Accept_Policy policy)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);

    manager->setHTTPAcceptPolicy(static_cast<WKHTTPCookieAcceptPolicy>(policy));
}

struct Get_Policy_Async_Data {
    Ewk_Cookie_Manager_Async_Policy_Get_Cb callback;
    void* userData;

    Get_Policy_Async_Data(Ewk_Cookie_Manager_Async_Policy_Get_Cb callback, void* userData)
        : callback(callback)
        , userData(userData)
    { }
};

static void getAcceptPolicyCallback(WKHTTPCookieAcceptPolicy policy, WKErrorRef wkError, void* data)
{
    Get_Policy_Async_Data* callbackData = static_cast<Get_Policy_Async_Data*>(data);
    OwnPtr<EwkError> ewkError = EwkError::create(wkError);

    callbackData->callback(static_cast<Ewk_Cookie_Accept_Policy>(policy), ewkError.get(), callbackData->userData);

    delete callbackData;
}

void ewk_cookie_manager_async_accept_policy_get(const Ewk_Cookie_Manager* manager, Ewk_Cookie_Manager_Async_Policy_Get_Cb callback, void* data)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    Get_Policy_Async_Data* callbackData = new Get_Policy_Async_Data(callback, data);
    manager->getHTTPAcceptPolicy(getAcceptPolicyCallback, callbackData);
}

struct Get_Hostnames_Async_Data {
    Ewk_Cookie_Manager_Async_Hostnames_Get_Cb callback;
    void* userData;

    Get_Hostnames_Async_Data(Ewk_Cookie_Manager_Async_Hostnames_Get_Cb callback, void* userData)
        : callback(callback)
        , userData(userData)
    { }
};

static void getHostnamesWithCookiesCallback(WKArrayRef wkHostnames, WKErrorRef wkError, void* context)
{
    Eina_List* hostnames = 0;
    Get_Hostnames_Async_Data* callbackData = static_cast<Get_Hostnames_Async_Data*>(context);
    OwnPtr<EwkError> ewkError = EwkError::create(wkError);

    const size_t hostnameCount = WKArrayGetSize(wkHostnames);
    for (size_t i = 0; i < hostnameCount; ++i) {
        WKStringRef wkHostname = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkHostnames, i));
        String hostname = toImpl(wkHostname)->string();
        if (hostname.isEmpty())
            continue;
        hostnames = eina_list_append(hostnames, eina_stringshare_add(hostname.utf8().data()));
    }

    callbackData->callback(hostnames, ewkError.get(), callbackData->userData);

    void* item;
    EINA_LIST_FREE(hostnames, item)
      eina_stringshare_del(static_cast<Eina_Stringshare*>(item));

    delete callbackData;
}

void ewk_cookie_manager_async_hostnames_with_cookies_get(const Ewk_Cookie_Manager* manager, Ewk_Cookie_Manager_Async_Hostnames_Get_Cb callback, void* data)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    Get_Hostnames_Async_Data* callbackData = new Get_Hostnames_Async_Data(callback, data);
    manager->getHostNamesWithCookies(getHostnamesWithCookiesCallback, callbackData);
}

void ewk_cookie_manager_hostname_cookies_clear(Ewk_Cookie_Manager* manager, const char* hostname)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);
    EINA_SAFETY_ON_NULL_RETURN(hostname);

    manager->clearHostnameCookies(String::fromUTF8(hostname));
}

void ewk_cookie_manager_cookies_clear(Ewk_Cookie_Manager* manager)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);

    manager->clearAllCookies();
}

void ewk_cookie_manager_changes_watch(Ewk_Cookie_Manager* manager, Ewk_Cookie_Manager_Changes_Watch_Cb callback, void* data)
{
    EINA_SAFETY_ON_NULL_RETURN(manager);

    manager->watchChanges(Cookie_Change_Handler(callback, data));
}
