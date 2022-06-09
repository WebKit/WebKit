/*
 * Copyright (C) 2007, 2015 Apple Inc. All rights reserved.
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

#include "WebSecurityOrigin.h"
#include "WebKitDLL.h"

#include "MarshallingHelpers.h"
#include <WebCore/BString.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/URL.h>

using namespace WebCore;

// WebSecurityOrigin ---------------------------------------------------------------
WebSecurityOrigin* WebSecurityOrigin::createInstance(SecurityOrigin* securityOrigin)
{
    WebSecurityOrigin* origin = new WebSecurityOrigin(securityOrigin);
    origin->AddRef();
    return origin;
}

WebSecurityOrigin::WebSecurityOrigin(SecurityOrigin* securityOrigin)
    : m_securityOrigin(securityOrigin)
{
    gClassCount++;
    gClassNameCount().add("WebSecurityOrigin");
}

WebSecurityOrigin::~WebSecurityOrigin()
{
    gClassCount--;
    gClassNameCount().remove("WebSecurityOrigin");
}

// IUnknown ------------------------------------------------------------------------
HRESULT WebSecurityOrigin::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebSecurityOrigin2*>(this);
    else if (IsEqualGUID(riid, IID_IWebSecurityOrigin))
        *ppvObject = static_cast<IWebSecurityOrigin*>(this);
    else if (IsEqualGUID(riid, IID_IWebSecurityOrigin2))
        *ppvObject = static_cast<IWebSecurityOrigin2*>(this);
    else if (IsEqualGUID(riid, __uuidof(this)))
        *ppvObject = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebSecurityOrigin::AddRef()
{
    return ++m_refCount;
}

ULONG WebSecurityOrigin::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

// IWebSecurityOrigin --------------------------------------------------------------

HRESULT WebSecurityOrigin::protocol(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_securityOrigin->protocol()).release();

    return S_OK;
}
        
HRESULT WebSecurityOrigin::host(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_securityOrigin->host()).release();

    return S_OK;
}
      
HRESULT WebSecurityOrigin::port(_Out_ unsigned short* result)
{
    if (!result)
        return E_POINTER;

    *result = m_securityOrigin->port().value_or(0);

    return S_OK;
}
        
HRESULT WebSecurityOrigin::usage(_Out_ unsigned long long* result)
{
    if (!result)
        return E_POINTER;

    *result = DatabaseTracker::singleton().usage(m_securityOrigin->data());

    return S_OK;
}
        
HRESULT WebSecurityOrigin::quota(_Out_ unsigned long long* result)
{
    if (!result)
        return E_POINTER;

    *result = DatabaseTracker::singleton().quota(m_securityOrigin->data());
    return S_OK;
}
        
HRESULT WebSecurityOrigin::setQuota(unsigned long long quota) 
{
    DatabaseTracker::singleton().setQuota(m_securityOrigin->data(), quota);

    return S_OK;
}

// IWebSecurityOrigin2 --------------------------------------------------------------

HRESULT WebSecurityOrigin::initWithURL(_In_ BSTR urlBstr)
{
    m_securityOrigin = WebCore::SecurityOrigin::create(MarshallingHelpers::BSTRToKURL(urlBstr));

    return S_OK;
}
