/*
 * Copyright (C) 2007-2008, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebKit.h"
#include <WebCore/DatabaseManagerClient.h>

namespace WebCore {
struct SecurityOriginData;
}

class WebDatabaseManager final : public IWebDatabaseManager2, private WebCore::DatabaseManagerClient {
public:
    static WebDatabaseManager* createInstance();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebDatabaseManager
    virtual HRESULT STDMETHODCALLTYPE sharedWebDatabaseManager(_COM_Outptr_opt_ IWebDatabaseManager** result);
    virtual HRESULT STDMETHODCALLTYPE origins(_COM_Outptr_opt_ IEnumVARIANT** result);
    virtual HRESULT STDMETHODCALLTYPE databasesWithOrigin(_In_opt_ IWebSecurityOrigin*, _COM_Outptr_opt_ IEnumVARIANT** result);   
    virtual HRESULT STDMETHODCALLTYPE detailsForDatabase(_In_ BSTR databaseName, _In_opt_ IWebSecurityOrigin*, _COM_Outptr_opt_ IPropertyBag** result);
    virtual HRESULT STDMETHODCALLTYPE deleteAllDatabases();        
    virtual HRESULT STDMETHODCALLTYPE deleteOrigin(_In_opt_ IWebSecurityOrigin*);       
    virtual HRESULT STDMETHODCALLTYPE deleteDatabase(_In_ BSTR databaseName, _In_opt_ IWebSecurityOrigin*);
    virtual HRESULT STDMETHODCALLTYPE setQuota(_In_ BSTR origin, unsigned long long quota);

    // IWebDatabaseManager2
    virtual HRESULT STDMETHODCALLTYPE deleteAllIndexedDatabases();
    virtual HRESULT STDMETHODCALLTYPE setIDBPerOriginQuota(unsigned long long);

    // DatabaseManagerClient
    virtual void dispatchDidModifyOrigin(const WebCore::SecurityOriginData&);
    virtual void dispatchDidModifyDatabase(const WebCore::SecurityOriginData&, const WTF::String& databaseName);

private:
    WebDatabaseManager();
    ~WebDatabaseManager();

    ULONG m_refCount { 0 };
};

void WebKitInitializeWebDatabasesIfNecessary();
