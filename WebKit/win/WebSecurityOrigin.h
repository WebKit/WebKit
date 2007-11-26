/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef WebSecurityOrigin_h
#define WebSecurityOrigin_h

#include "IWebSecurityOrigin.h"
#include <WebCore/SecurityOriginData.h>

class WebSecurityOrigin : public IWebSecurityOrigin {
public:
    static WebSecurityOrigin* createInstance(const WebCore::SecurityOriginData& securityOriginData);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void** ppvObject);

    virtual ULONG STDMETHODCALLTYPE AddRef();

    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebSecurityOrigin
    virtual HRESULT STDMETHODCALLTYPE protocol( 
        /* [retval][out] */ BSTR* result);
        
    virtual HRESULT STDMETHODCALLTYPE domain( 
        /* [retval][out] */ BSTR* result);
        
    virtual HRESULT STDMETHODCALLTYPE port( 
        /* [retval][out] */ unsigned short* result);
        
    virtual HRESULT STDMETHODCALLTYPE usage( 
        /* [retval][out] */ unsigned long long* result);
        
    virtual HRESULT STDMETHODCALLTYPE quota( 
        /* [retval][out] */ unsigned long long* result);
        
    virtual HRESULT STDMETHODCALLTYPE setQuota( 
        /* [in] */ unsigned long long quota);
private:
    WebSecurityOrigin(const WebCore::SecurityOriginData&);
    ~WebSecurityOrigin();

    ULONG m_refCount;
    WebCore::SecurityOriginData m_securityOriginData;
};

#endif // WebSecurityOrigin_h
