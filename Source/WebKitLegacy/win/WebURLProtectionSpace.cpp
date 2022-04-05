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

#include "WebKit.h"
#include "WebKitDLL.h"
#include "WebURLProtectionSpace.h"

#include <WebCore/BString.h>

using namespace WebCore;

// WebURLProtectionSpace ----------------------------------------------------------------

WebURLProtectionSpace::WebURLProtectionSpace(const ProtectionSpace& protectionSpace)
    : m_protectionSpace(protectionSpace)
{
    gClassCount++;
    gClassNameCount().add("WebURLProtectionSpace"_s);
}

WebURLProtectionSpace::~WebURLProtectionSpace()
{
    gClassCount--;
    gClassNameCount().remove("WebURLProtectionSpace"_s);
}

WebURLProtectionSpace* WebURLProtectionSpace::createInstance()
{
    WebURLProtectionSpace* instance = new WebURLProtectionSpace(ProtectionSpace());
    instance->AddRef();
    return instance;
}

WebURLProtectionSpace* WebURLProtectionSpace::createInstance(const ProtectionSpace& protectionSpace)
{
    WebURLProtectionSpace* instance = new WebURLProtectionSpace(protectionSpace);
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebURLProtectionSpace::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, CLSID_WebURLProtectionSpace))
        *ppvObject = static_cast<WebURLProtectionSpace*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLProtectionSpace))
        *ppvObject = static_cast<IWebURLProtectionSpace*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebURLProtectionSpace::AddRef()
{
    return ++m_refCount;
}

ULONG WebURLProtectionSpace::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebURLProtectionSpace -------------------------------------------------------------------

HRESULT WebURLProtectionSpace::authenticationMethod(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    switch (m_protectionSpace.authenticationScheme()) {
    case ProtectionSpace::AuthenticationScheme::Default:
        *result = SysAllocString(WebURLAuthenticationMethodDefault);
        break;
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
        *result = SysAllocString(WebURLAuthenticationMethodHTTPBasic);
        break;
    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
        *result = SysAllocString(WebURLAuthenticationMethodHTTPDigest);
        break;
    case ProtectionSpace::AuthenticationScheme::HTMLForm:
        *result = SysAllocString(WebURLAuthenticationMethodHTMLForm);
        break;
    default:
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }
    return S_OK;
}

HRESULT WebURLProtectionSpace::host(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    BString str = m_protectionSpace.host();
    *result = str.release();
    return S_OK;
}

static ProtectionSpace::AuthenticationScheme coreScheme(BSTR authenticationMethod)
{
    auto scheme = ProtectionSpace::AuthenticationScheme::Default;
    if (BString(authenticationMethod) == BString(WebURLAuthenticationMethodDefault))
        scheme = ProtectionSpace::AuthenticationScheme::Default;
    else if (BString(authenticationMethod) == BString(WebURLAuthenticationMethodHTTPBasic))
        scheme = ProtectionSpace::AuthenticationScheme::HTTPBasic;
    else if (BString(authenticationMethod) == BString(WebURLAuthenticationMethodHTTPDigest))
        scheme = ProtectionSpace::AuthenticationScheme::HTTPDigest;
    else if (BString(authenticationMethod) == BString(WebURLAuthenticationMethodHTMLForm))
        scheme = ProtectionSpace::AuthenticationScheme::HTMLForm;
    else
        ASSERT_NOT_REACHED();
    return scheme;
}

HRESULT WebURLProtectionSpace::initWithHost(_In_ BSTR host, int port, _In_ BSTR protocol, _In_ BSTR realm, _In_ BSTR authenticationMethod)
{
    static BString& webURLProtectionSpaceHTTPBString = *new BString(WebURLProtectionSpaceHTTP);
    static BString& webURLProtectionSpaceHTTPSBString = *new BString(WebURLProtectionSpaceHTTPS);
    static BString& webURLProtectionSpaceFTPBString = *new BString(WebURLProtectionSpaceFTP);
    static BString& webURLProtectionSpaceFTPSBString = *new BString(WebURLProtectionSpaceFTPS);

    auto serverType = ProtectionSpace::ServerType::HTTP;
    if (BString(protocol) == webURLProtectionSpaceHTTPBString)
        serverType = ProtectionSpace::ServerType::HTTP;
    else if (BString(protocol) == webURLProtectionSpaceHTTPSBString)
        serverType = ProtectionSpace::ServerType::HTTPS;
    else if (BString(protocol) == webURLProtectionSpaceFTPBString)
        serverType = ProtectionSpace::ServerType::FTP;
    else if (BString(protocol) == webURLProtectionSpaceFTPSBString)
        serverType = ProtectionSpace::ServerType::FTPS;

    m_protectionSpace = ProtectionSpace(String(host, SysStringLen(host)), port, serverType, 
        String(realm, SysStringLen(realm)), coreScheme(authenticationMethod));

    return S_OK;
}

HRESULT WebURLProtectionSpace::initWithProxyHost(_In_ BSTR host, int port, _In_ BSTR proxyType, _In_ BSTR realm, _In_ BSTR authenticationMethod)
{
    static BString& webURLProtectionSpaceHTTPProxyBString = *new BString(WebURLProtectionSpaceHTTPProxy);
    static BString& webURLProtectionSpaceHTTPSProxyBString = *new BString(WebURLProtectionSpaceHTTPSProxy);
    static BString& webURLProtectionSpaceFTPProxyBString = *new BString(WebURLProtectionSpaceFTPProxy);
    static BString& webURLProtectionSpaceSOCKSProxyBString = *new BString(WebURLProtectionSpaceSOCKSProxy);

    auto serverType = ProtectionSpace::ServerType::ProxyHTTP;
    if (BString(proxyType) == webURLProtectionSpaceHTTPProxyBString)
        serverType = ProtectionSpace::ServerType::ProxyHTTP;
    else if (BString(proxyType) == webURLProtectionSpaceHTTPSProxyBString)
        serverType = ProtectionSpace::ServerType::ProxyHTTPS;
    else if (BString(proxyType) == webURLProtectionSpaceFTPProxyBString)
        serverType = ProtectionSpace::ServerType::ProxyFTP;
    else if (BString(proxyType) == webURLProtectionSpaceSOCKSProxyBString)
        serverType = ProtectionSpace::ServerType::ProxySOCKS;
    else
        ASSERT_NOT_REACHED();

    m_protectionSpace = ProtectionSpace(String(host, SysStringLen(host)), port, serverType, 
        String(realm, SysStringLen(realm)), coreScheme(authenticationMethod));

    return S_OK;
}

HRESULT WebURLProtectionSpace::isProxy(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = m_protectionSpace.isProxy();
    return S_OK;
}

HRESULT WebURLProtectionSpace::port(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = m_protectionSpace.port();
    return S_OK;
}

HRESULT WebURLProtectionSpace::protocol(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    switch (m_protectionSpace.serverType()) {
    case ProtectionSpace::ServerType::HTTP:
        *result = SysAllocString(WebURLProtectionSpaceHTTP);
        break;
    case ProtectionSpace::ServerType::HTTPS:
        *result = SysAllocString(WebURLProtectionSpaceHTTPS);
        break;
    case ProtectionSpace::ServerType::FTP:
        *result = SysAllocString(WebURLProtectionSpaceFTP);
        break;
    case ProtectionSpace::ServerType::FTPS:
        *result = SysAllocString(WebURLProtectionSpaceFTPS);
        break;
    default:
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }
    return S_OK;
}

HRESULT WebURLProtectionSpace::proxyType(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    switch (m_protectionSpace.serverType()) {
    case ProtectionSpace::ServerType::ProxyHTTP:
        *result = SysAllocString(WebURLProtectionSpaceHTTPProxy);
        break;
    case ProtectionSpace::ServerType::ProxyHTTPS:
        *result = SysAllocString(WebURLProtectionSpaceHTTPSProxy);
        break;
    case ProtectionSpace::ServerType::ProxyFTP:
        *result = SysAllocString(WebURLProtectionSpaceFTPProxy);
        break;
    case ProtectionSpace::ServerType::ProxySOCKS:
        *result = SysAllocString(WebURLProtectionSpaceSOCKSProxy);
        break;
    default:
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }
    return S_OK;
}

HRESULT WebURLProtectionSpace::realm(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    BString bstring = m_protectionSpace.realm();
    *result = bstring.release();
    return S_OK;
}

HRESULT WebURLProtectionSpace::receivesCredentialSecurely(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = m_protectionSpace.receivesCredentialSecurely();
    return S_OK;
}

// WebURLProtectionSpace -------------------------------------------------------------------
const ProtectionSpace& WebURLProtectionSpace::protectionSpace() const
{
    return m_protectionSpace;
}
