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

#ifndef WebApplicationCache_h
#define WebApplicationCache_h

#include "WebKit.h"

namespace WebCore {
class ApplicationCacheStorage;
}

class DECLSPEC_UUID("1119E970-4B13-4B9A-A049-41096104B689") WebApplicationCache final : public IWebApplicationCache {
public:
    static WebApplicationCache* createInstance();

    static WebCore::ApplicationCacheStorage& storage();

protected:
    WebApplicationCache();
    ~WebApplicationCache();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebApplicationCache
    virtual HRESULT STDMETHODCALLTYPE maximumSize(/*[out, retval]*/ long long*);
    virtual HRESULT STDMETHODCALLTYPE setMaximumSize(/*[in]*/ long long);

    virtual HRESULT STDMETHODCALLTYPE defaultOriginQuota(/*[out, retval]*/ long long*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultOriginQuota(/*[in]*/ long long);

    virtual HRESULT STDMETHODCALLTYPE diskUsageForOrigin(/*[in]*/ IWebSecurityOrigin*, /*[out, retval]*/ long long*);

    virtual HRESULT STDMETHODCALLTYPE deleteAllApplicationCaches();
    virtual HRESULT STDMETHODCALLTYPE deleteCacheForOrigin(/*[in]*/ IWebSecurityOrigin*);

    virtual HRESULT STDMETHODCALLTYPE originsWithCache(/*[out, retval]*/ IPropertyBag**);

protected:
    ULONG m_refCount;
};

#endif
