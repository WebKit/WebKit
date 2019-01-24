/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "WebKitDLL.h"
#include "WebApplicationCache.h"

#include "CFDictionaryPropertyBag.h"
#include "MarshallingHelpers.h"
#include "WebPreferences.h"
#include "WebSecurityOrigin.h"
#include <WebCore/ApplicationCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/COMPtr.h>
#include <WebCore/SecurityOrigin.h>
#include <comutil.h>
#include <wtf/FileSystem.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

static CFStringRef WebKitLocalCacheDefaultsKey = CFSTR("WebKitLocalCache");

// WebApplicationCache ---------------------------------------------------------------------------

WebApplicationCache::WebApplicationCache()
    : m_refCount(0)
{
    gClassCount++;
    gClassNameCount().add("WebApplicationCache");
}

WebApplicationCache::~WebApplicationCache()
{
    gClassCount--;
    gClassNameCount().remove("WebApplicationCache");
}

WebApplicationCache* WebApplicationCache::createInstance()
{
    WebApplicationCache* instance = new WebApplicationCache();
    instance->AddRef();
    return instance;
}

static String applicationCachePath()
{
    String path = FileSystem::localUserSpecificStorageDirectory();

#if USE(CF)
    auto cacheDirectoryPreference = adoptCF(CFPreferencesCopyAppValue(WebKitLocalCacheDefaultsKey, WebPreferences::applicationId()));
    if (cacheDirectoryPreference && CFStringGetTypeID() == CFGetTypeID(cacheDirectoryPreference.get()))
        path = static_cast<CFStringRef>(cacheDirectoryPreference.get());
#endif

    return path;
}

WebCore::ApplicationCacheStorage& WebApplicationCache::storage()
{
    static ApplicationCacheStorage& storage = ApplicationCacheStorage::create(applicationCachePath(), "ApplicationCache").leakRef();

    return storage;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebApplicationCache::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebApplicationCache*>(this);
    else if (IsEqualGUID(riid, IID_IWebApplicationCache))
        *ppvObject = static_cast<WebApplicationCache*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebApplicationCache::AddRef()
{
    return ++m_refCount;
}

ULONG WebApplicationCache::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebApplicationCache ------------------------------------------------------------------------------

HRESULT WebApplicationCache::maximumSize(long long* size)
{
    if (!size)
        return E_POINTER;

    *size = storage().maximumSize();
    return S_OK;
}

HRESULT WebApplicationCache::setMaximumSize(long long size)
{
    storage().deleteAllEntries();
    storage().setMaximumSize(size);
    return S_OK;
}

HRESULT WebApplicationCache::defaultOriginQuota(long long* quota)
{
    if (!quota)
        return E_POINTER;

    *quota = storage().defaultOriginQuota();
    return S_OK;
}

HRESULT WebApplicationCache::setDefaultOriginQuota(long long quota)
{
    storage().setDefaultOriginQuota(quota);
    return S_OK;
}

HRESULT WebApplicationCache::diskUsageForOrigin(IWebSecurityOrigin* origin, long long* size)
{
    if (!origin || !size)
        return E_POINTER;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    *size = storage().diskUsageForOrigin(*webSecurityOrigin->securityOrigin());
    return S_OK;
}

HRESULT WebApplicationCache::deleteAllApplicationCaches()
{
    storage().deleteAllCaches();
    return S_OK;
}


HRESULT WebApplicationCache::deleteCacheForOrigin(IWebSecurityOrigin* origin)
{
    if (!origin)
        return E_POINTER;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    storage().deleteCacheForOrigin(*webSecurityOrigin->securityOrigin());
    return S_OK;
}

HRESULT WebApplicationCache::originsWithCache(IPropertyBag** origins)
{
    if (!origins)
        return E_POINTER;

    auto coreOrigins = storage().originsWithCache();

    RetainPtr<CFMutableArrayRef> arrayItem = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, coreOrigins.size(), &MarshallingHelpers::kIUnknownArrayCallBacks));

    for (auto& coreOrigin : coreOrigins)
        CFArrayAppendValue(arrayItem.get(), reinterpret_cast<void*>(WebSecurityOrigin::createInstance(coreOrigin.ptr())));

    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(
        CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFStringRef> key = adoptCF(MarshallingHelpers::BSTRToCFStringRef(_bstr_t(L"origins")));
    CFDictionaryAddValue(dictionary.get(), key.get(), arrayItem.get());

    COMPtr<CFDictionaryPropertyBag> result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary.get());
    *origins = result.leakRef();

    return S_OK;
}
