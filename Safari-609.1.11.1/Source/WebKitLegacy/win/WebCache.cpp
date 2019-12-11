/*
 * Copyright (C) 2006-2007, 2015 Apple Inc.  All rights reserved.
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
#include "WebCache.h"

#include "CFDictionaryPropertyBag.h"
#include "WebApplicationCache.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/BString.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/CrossOriginPreflightResultCache.h>

#if USE(CURL)
#include <WebCore/CurlCacheManager.h>
#elif USE(CFURLCONNECTION)
#include <CFNetwork/CFURLCachePriv.h>
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

using namespace WebCore;

// WebCache ---------------------------------------------------------------------------

WebCache::WebCache()
{
    gClassCount++;
    gClassNameCount().add("WebCache");
}

WebCache::~WebCache()
{
    gClassCount--;
    gClassNameCount().remove("WebCache");
}

WebCache* WebCache::createInstance()
{
    WebCache* instance = new WebCache();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebCache::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebCache*>(this);
    else if (IsEqualGUID(riid, IID_IWebCache))
        *ppvObject = static_cast<WebCache*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebCache::AddRef()
{
    return ++m_refCount;
}

ULONG WebCache::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebCache ------------------------------------------------------------------------------

HRESULT WebCache::statistics(_Inout_ int* count, _Inout_opt_ IPropertyBag** s)
{
    if (!count || (s && *count < 6))
        return E_FAIL;

    *count = 6;
    if (!s)
        return S_OK;

    WebCore::MemoryCache::Statistics stat = WebCore::MemoryCache::singleton().getStatistics();

#if USE(CF)
    static CFStringRef imagesKey = CFSTR("images");
    static CFStringRef stylesheetsKey = CFSTR("style sheets");
    static CFStringRef xslKey = CFSTR("xsl");
    static CFStringRef scriptsKey = CFSTR("scripts");
#if !ENABLE(XSLT)
    const int zero = 0;
#endif

    // Object Counts.
    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFNumberRef> value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.images.count));
    CFDictionaryAddValue(dictionary.get(), imagesKey, value.get());
    
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.cssStyleSheets.count));
    CFDictionaryAddValue(dictionary.get(), stylesheetsKey, value.get());
    
#if ENABLE(XSLT)
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.xslStyleSheets.count));
#else
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &zero));
#endif
    CFDictionaryAddValue(dictionary.get(), xslKey, value.get());

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.scripts.count));
    CFDictionaryAddValue(dictionary.get(), scriptsKey, value.get());

    COMPtr<CFDictionaryPropertyBag> propBag = CFDictionaryPropertyBag::createInstance();
    propBag->setDictionary(dictionary.get());
    s[0] = propBag.leakRef();

    // Object Sizes.
    dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.images.size));
    CFDictionaryAddValue(dictionary.get(), imagesKey, value.get());
    
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.cssStyleSheets.size));
    CFDictionaryAddValue(dictionary.get(), stylesheetsKey, value.get());
    
#if ENABLE(XSLT)
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.xslStyleSheets.size));
#else
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &zero));
#endif
    CFDictionaryAddValue(dictionary.get(), xslKey, value.get());

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.scripts.size));
    CFDictionaryAddValue(dictionary.get(), scriptsKey, value.get());

    propBag = CFDictionaryPropertyBag::createInstance();
    propBag->setDictionary(dictionary.get());
    s[1] = propBag.leakRef();

    // Live Sizes.
    dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.images.liveSize));
    CFDictionaryAddValue(dictionary.get(), imagesKey, value.get());
    
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.cssStyleSheets.liveSize));
    CFDictionaryAddValue(dictionary.get(), stylesheetsKey, value.get());
    
#if ENABLE(XSLT)
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.xslStyleSheets.liveSize));
#else
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &zero));
#endif
    CFDictionaryAddValue(dictionary.get(), xslKey, value.get());

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.scripts.liveSize));
    CFDictionaryAddValue(dictionary.get(), scriptsKey, value.get());

    propBag = CFDictionaryPropertyBag::createInstance();
    propBag->setDictionary(dictionary.get());
    s[2] = propBag.leakRef();

    // Decoded Sizes.
    dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.images.decodedSize));
    CFDictionaryAddValue(dictionary.get(), imagesKey, value.get());
    
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.cssStyleSheets.decodedSize));
    CFDictionaryAddValue(dictionary.get(), stylesheetsKey, value.get());
    
#if ENABLE(XSLT)
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.xslStyleSheets.decodedSize));
#else
    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &zero));
#endif
    CFDictionaryAddValue(dictionary.get(), xslKey, value.get());

    value = adoptCF(CFNumberCreate(0, kCFNumberIntType, &stat.scripts.decodedSize));
    CFDictionaryAddValue(dictionary.get(), scriptsKey, value.get());

    propBag = CFDictionaryPropertyBag::createInstance();
    propBag->setDictionary(dictionary.get());
    s[3] = propBag.leakRef();

    return S_OK;
#else
    return E_FAIL;
#endif
}

HRESULT WebCache::empty()
{
    auto& memoryCache = WebCore::MemoryCache::singleton();
    if (memoryCache.disabled())
        return S_OK;
    memoryCache.setDisabled(true);
    memoryCache.setDisabled(false);

    // Empty the application cache.
    WebApplicationCache::storage().empty();

    // Empty the Cross-Origin Preflight cache
    WebCore::CrossOriginPreflightResultCache::singleton().clear();

    return S_OK;
}

HRESULT WebCache::setDisabled(BOOL disabled)
{
    WebCore::MemoryCache::singleton().setDisabled(!!disabled);
    return S_OK;
}

HRESULT WebCache::disabled(_Out_ BOOL* disabled)
{
    if (!disabled)
        return E_POINTER;
    *disabled = WebCore::MemoryCache::singleton().disabled();
    return S_OK;
}

HRESULT WebCache::cacheFolder(__deref_out_opt BSTR* location)
{
#if USE(CURL)
    String cacheFolder = WebCore::CurlCacheManager::singleton().cacheDirectory();
    *location = WebCore::BString(cacheFolder).release();
    return S_OK;
#elif USE(CFURLCONNECTION)
    RetainPtr<CFURLCacheRef> cache = adoptCF(CFURLCacheCopySharedURLCache());
    RetainPtr<CFStringRef> cfurlCacheDirectory = adoptCF(_CFURLCacheCopyCacheDirectory(cache.get()));
    *location = BString(cfurlCacheDirectory.get()).release();
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebCache::setCacheFolder(_In_ BSTR location)
{
#if USE(CURL)
    String cacheFolder(location, SysStringLen(location));
    WebCore::CurlCacheManager::singleton().setCacheDirectory(cacheFolder);
    return S_OK;
#elif USE(CFURLCONNECTION)
    RetainPtr<CFURLCacheRef> cache = adoptCF(CFURLCacheCopySharedURLCache());
    CFIndex memoryCapacity = CFURLCacheMemoryCapacity(cache.get());
    CFIndex diskCapacity = CFURLCacheDiskCapacity(cache.get());
    RetainPtr<CFURLCacheRef> newCache = adoptCF(CFURLCacheCreate(kCFAllocatorDefault, memoryCapacity, diskCapacity, String(location).createCFString().get()));
    CFURLCacheSetSharedURLCache(newCache.get());
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}
