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

#include "SoupCookiePersistentStorageType.h"
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

static void cookiesDidChange(WKCookieManagerRef, const void* clientInfo);

Ewk_Cookie_Manager::Ewk_Cookie_Manager(WKCookieManagerRef cookieManagerRef)
    : wkCookieManager(cookieManagerRef)
{
    WKCookieManagerClient wkCookieManagerClient = {
        kWKCookieManagerClientCurrentVersion,
        this, // clientInfo
        cookiesDidChange
    };
    WKCookieManagerSetClient(wkCookieManager.get(), &wkCookieManagerClient);
}

Ewk_Cookie_Manager::~Ewk_Cookie_Manager()
{
    if (changeHandler.callback)
        WKCookieManagerStopObservingCookieChanges(wkCookieManager.get());
}

#define EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager_, ...)    \
    if (!(manager)) {                                                    \
        EINA_LOG_CRIT("manager is NULL.");                               \
        return __VA_ARGS__;                                              \
    }                                                                    \
    if (!(manager)->wkCookieManager) {                                   \
        EINA_LOG_CRIT("manager->wkCookieManager is NULL.");              \
        return __VA_ARGS__;                                              \
    }                                                                    \
    WKCookieManagerRef wkManager_ = (manager)->wkCookieManager.get()

// Ewk_Cookie_Accept_Policy enum validation
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_ACCEPT_POLICY_ALWAYS, kWKHTTPCookieAcceptPolicyAlways);
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_ACCEPT_POLICY_NEVER, kWKHTTPCookieAcceptPolicyNever);
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_ACCEPT_POLICY_NO_THIRD_PARTY, kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain);

// Ewk_Cookie_Persistent_Storage enum validation
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_PERSISTENT_STORAGE_TEXT, SoupCookiePersistentStorageText);
COMPILE_ASSERT_MATCHING_ENUM(EWK_COOKIE_PERSISTENT_STORAGE_SQLITE, SoupCookiePersistentStorageSQLite);

static void cookiesDidChange(WKCookieManagerRef, const void* clientInfo)
{
    Ewk_Cookie_Manager* manager = static_cast<Ewk_Cookie_Manager*>(const_cast<void*>(clientInfo));

    if (!manager->changeHandler.callback)
        return;

    manager->changeHandler.callback(manager->changeHandler.userData);
}

void ewk_cookie_manager_persistent_storage_set(Ewk_Cookie_Manager* manager, const char* filename, Ewk_Cookie_Persistent_Storage storage)
{
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);
    EINA_SAFETY_ON_NULL_RETURN(filename);

    if (manager->changeHandler.callback)
        WKCookieManagerStopObservingCookieChanges(wkManager);

    toImpl(wkManager)->setCookiePersistentStorage(String::fromUTF8(filename), storage);

    if (manager->changeHandler.callback)
        WKCookieManagerStartObservingCookieChanges(wkManager);
}

void ewk_cookie_manager_accept_policy_set(Ewk_Cookie_Manager* manager, Ewk_Cookie_Accept_Policy policy)
{
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);

    WKCookieManagerSetHTTPCookieAcceptPolicy(wkManager, static_cast<WKHTTPCookieAcceptPolicy>(policy));
}

struct Get_Policy_Async_Data {
    Ewk_Cookie_Manager_Async_Policy_Get_Cb callback;
    void* userData;
};

static void getAcceptPolicyCallback(WKHTTPCookieAcceptPolicy policy, WKErrorRef wkError, void* data)
{
    Get_Policy_Async_Data* callbackData = static_cast<Get_Policy_Async_Data*>(data);
    OwnPtr<Ewk_Error> ewkError = Ewk_Error::create(wkError);

    callbackData->callback(static_cast<Ewk_Cookie_Accept_Policy>(policy), ewkError.get(), callbackData->userData);

    delete callbackData;
}

void ewk_cookie_manager_async_accept_policy_get(const Ewk_Cookie_Manager* manager, Ewk_Cookie_Manager_Async_Policy_Get_Cb callback, void* data)
{
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    Get_Policy_Async_Data* callbackData = new Get_Policy_Async_Data;
    callbackData->callback = callback;
    callbackData->userData = data;

    WKCookieManagerGetHTTPCookieAcceptPolicy(wkManager, callbackData, getAcceptPolicyCallback);
}

struct Get_Hostnames_Async_Data {
    Ewk_Cookie_Manager_Async_Hostnames_Get_Cb callback;
    void* userData;
};

static void getHostnamesWithCookiesCallback(WKArrayRef wkHostnames, WKErrorRef wkError, void* context)
{
    Eina_List* hostnames = 0;
    Get_Hostnames_Async_Data* callbackData = static_cast<Get_Hostnames_Async_Data*>(context);
    OwnPtr<Ewk_Error> ewkError = Ewk_Error::create(wkError);

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
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    Get_Hostnames_Async_Data* callbackData = new Get_Hostnames_Async_Data;
    callbackData->callback = callback;
    callbackData->userData = data;

    WKCookieManagerGetHostnamesWithCookies(wkManager, callbackData, getHostnamesWithCookiesCallback);
}

void ewk_cookie_manager_hostname_cookies_clear(Ewk_Cookie_Manager* manager, const char* hostName)
{
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);
    EINA_SAFETY_ON_NULL_RETURN(hostName);

    WKRetainPtr<WKStringRef> wkHostName(AdoptWK, WKStringCreateWithUTF8CString(hostName));
    WKCookieManagerDeleteCookiesForHostname(wkManager, wkHostName.get());
}

void ewk_cookie_manager_cookies_clear(Ewk_Cookie_Manager* manager)
{
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);

    WKCookieManagerDeleteAllCookies(wkManager);
}

void ewk_cookie_manager_changes_watch(Ewk_Cookie_Manager* manager, Ewk_Cookie_Manager_Changes_Watch_Cb callback, void* data)
{
    EWK_COOKIE_MANAGER_WK_GET_OR_RETURN(manager, wkManager);

    manager->changeHandler = Cookie_Change_Handler(callback, data);

    if (callback)
        WKCookieManagerStartObservingCookieChanges(wkManager);
    else
        WKCookieManagerStopObservingCookieChanges(wkManager);
}
