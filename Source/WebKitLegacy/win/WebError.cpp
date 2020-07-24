/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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
#include "WebError.h"
#include "WebKit.h"

#include <WebCore/BString.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if USE(CFURLCONNECTION)
#include <pal/spi/win/CFNetworkSPIWin.h>
#endif

using namespace WebCore;

// WebError ---------------------------------------------------------------------

WebError::WebError(const ResourceError& error, IPropertyBag* userInfo)
    : m_userInfo(userInfo)
    , m_error(error)
{
    gClassCount++;
    gClassNameCount().add("WebError");
}

WebError::~WebError()
{
    gClassCount--;
    gClassNameCount().remove("WebError");
}

WebError* WebError::createInstance(const ResourceError& error, IPropertyBag* userInfo)
{
    WebError* instance = new WebError(error, userInfo);
    instance->AddRef();
    return instance;
}

WebError* WebError::createInstance()
{
    return createInstance(ResourceError());
}

// IUnknown -------------------------------------------------------------------

HRESULT WebError::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebError*>(this);
    else if (IsEqualGUID(riid, CLSID_WebError))
        *ppvObject = static_cast<WebError*>(this);
    else if (IsEqualGUID(riid, IID_IWebError))
        *ppvObject = static_cast<IWebError*>(this);
    else if (IsEqualGUID(riid, IID_IWebErrorPrivate))
        *ppvObject = static_cast<IWebErrorPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebError::AddRef()
{
    return ++m_refCount;
}

ULONG WebError::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebError ------------------------------------------------------------------

HRESULT WebError::init(_In_ BSTR domain, int code, _In_ BSTR url)
{
    m_error = ResourceError(String(domain, SysStringLen(domain)), code, URL(URL(), String(url, SysStringLen(url))), String());
    return S_OK;
}
  
HRESULT WebError::code(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = m_error.errorCode();
    return S_OK;
}
        
HRESULT WebError::domain(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_error.domain()).release();
    return S_OK;
}
               
HRESULT WebError::localizedDescription(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_error.localizedDescription()).release();

#if USE(CFURLCONNECTION)
    if (!*result) {
        if (int code = m_error.errorCode())
            *result = BString(_CFNetworkErrorGetLocalizedDescription(code)).release();
    }
#endif

    return S_OK;
}

        
HRESULT WebError::localizedFailureReason(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
        
HRESULT WebError::localizedRecoveryOptions(__deref_opt_out IEnumVARIANT** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
        
HRESULT WebError::localizedRecoverySuggestion(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
       
HRESULT WebError::recoverAttempter(__deref_opt_out IUnknown** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
        
HRESULT WebError::userInfo(_COM_Outptr_opt_ IPropertyBag** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;

    if (!m_userInfo)
        return E_FAIL;

    return m_userInfo.copyRefTo(result);
}

HRESULT WebError::failingURL(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_error.failingURL()).release();
    return S_OK;
}

HRESULT WebError::isPolicyChangeError(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = m_error.domain() == String(WebKitErrorDomain)
        && m_error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange;
    return S_OK;
}

HRESULT WebError::sslPeerCertificate(_Out_ ULONG_PTR* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;

#if USE(CFURLCONNECTION)
    if (!m_cfErrorUserInfoDict) {
        // copy userinfo from CFErrorRef
        CFErrorRef cfError = m_error;
        m_cfErrorUserInfoDict = adoptCF(CFErrorCopyUserInfo(cfError));
    }

    if (!m_cfErrorUserInfoDict)
        return E_FAIL;

    const void* data = ResourceError::getSSLPeerCertificateDataBytePtr(m_cfErrorUserInfoDict.get());
    if (!data)
        return E_FAIL;
    *result = reinterpret_cast<ULONG_PTR>(const_cast<void*>(data));
#endif
    return *result ? S_OK : E_FAIL;
}

const ResourceError& WebError::resourceError() const
{
    return m_error;
}
