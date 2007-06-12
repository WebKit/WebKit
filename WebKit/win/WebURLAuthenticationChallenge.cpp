/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitDLL.h"
#include <initguid.h>
#include "WebURLAuthenticationChallenge.h"

#include "COMPtr.h"
#include "WebError.h"
#include "WebKit.h"
#include "WebURLAuthenticationChallengeSender.h"
#include "WebURLCredential.h"
#include "WebURLProtectionSpace.h"
#include "WebURLResponse.h"
#include "WebKit.h"

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/ResourceHandle.h>
#pragma warning(pop)

using namespace WebCore;

// WebURLAuthenticationChallenge ----------------------------------------------------------------

WebURLAuthenticationChallenge::WebURLAuthenticationChallenge(const AuthenticationChallenge& authenticationChallenge)
    : m_refCount(0)
    , m_authenticationChallenge(authenticationChallenge)
{
    gClassCount++;
}

WebURLAuthenticationChallenge::~WebURLAuthenticationChallenge()
{
    gClassCount--;
}

WebURLAuthenticationChallenge* WebURLAuthenticationChallenge::createInstance()
{
    WebURLAuthenticationChallenge* instance = new WebURLAuthenticationChallenge(AuthenticationChallenge());
    instance->AddRef();
    return instance;
}

WebURLAuthenticationChallenge* WebURLAuthenticationChallenge::createInstance(const AuthenticationChallenge& authenticationChallenge)
{
    WebURLAuthenticationChallenge* instance = new WebURLAuthenticationChallenge(authenticationChallenge);
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_WebURLAuthenticationChallenge))
        *ppvObject = static_cast<WebURLAuthenticationChallenge*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLAuthenticationChallenge))
        *ppvObject = static_cast<IWebURLAuthenticationChallenge*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebURLAuthenticationChallenge::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebURLAuthenticationChallenge::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebURLAuthenticationChallenge -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::initWithProtectionSpace(
    /* [in] */ IWebURLProtectionSpace* space, 
    /* [in] */ IWebURLCredential* proposedCredential, 
    /* [in] */ int previousFailureCount, 
    /* [in] */ IWebURLResponse* failureResponse, 
    /* [in] */ IWebError* error, 
    /* [in] */ IWebURLAuthenticationChallengeSender* sender)
{
    LOG_ERROR("Calling the ala carte init for WebURLAuthenticationChallenge - is this really what you want to do?");

    if (!space || !proposedCredential || !failureResponse || !sender)
        return E_POINTER;

    HRESULT hr = S_OK;
    COMPtr<WebURLProtectionSpace> webSpace;
    hr = space->QueryInterface(CLSID_WebURLProtectionSpace, (void**)&webSpace);
    if (FAILED(hr))
        return hr;

    COMPtr<WebURLCredential> webCredential;
    hr = proposedCredential->QueryInterface(CLSID_WebURLCredential, (void**)&webCredential);
    if (FAILED(hr))
        return hr;

    COMPtr<WebURLResponse> webResponse;
    hr = failureResponse->QueryInterface(IID_WebURLResponse, (void**)&webResponse);
    if (FAILED(hr))
        return hr;

    COMPtr<WebError> webError;
    hr = error->QueryInterface(CLSID_WebError, (void**)&webError);
    if (FAILED(hr))
        return hr;
    
    COMPtr<WebURLAuthenticationChallengeSender> webSender;
    hr = sender->QueryInterface(IID_WebURLAuthenticationChallengeSender, (void**)&webSender);
    if (FAILED(hr))
        return hr;

    // FIXME: After we change AuthenticationChallenge to use "ResourceHandle" as the abstract "Sender" or "Source of this Auth Challenge", then we'll
    // construct the AuthenticationChallenge with that as obtained from the webSender

    m_authenticationChallenge = AuthenticationChallenge(webSpace->protectionSpace(), webCredential->credential(),
                                    previousFailureCount, webResponse->resourceResponse(), webError->resourceError());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::initWithAuthenticationChallenge(
    /* [in] */ IWebURLAuthenticationChallenge* challenge, 
    /* [in] */ IWebURLAuthenticationChallengeSender* sender)
{
    if (!challenge || !sender)
        return E_POINTER;

    HRESULT hr = S_OK;

    COMPtr<WebURLAuthenticationChallenge> webChallenge;
    hr = challenge->QueryInterface(IID_WebURLAuthenticationChallenge, (void**)&webChallenge);
    if (FAILED(hr))
        return hr;

    COMPtr<WebURLAuthenticationChallengeSender> webSender;
    hr = sender->QueryInterface(IID_WebURLAuthenticationChallengeSender, (void**)&webSender);
    if (FAILED(hr))
        return hr;

    m_authenticationChallenge = AuthenticationChallenge(webChallenge->authenticationChallenge().cfURLAuthChallengeRef(), webSender->resourceHandle());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::error(
    /* [out, retval] */ IWebError** result)
{
    *result = WebError::createInstance(m_authenticationChallenge.error());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::failureResponse(
    /* [out, retval] */ IWebURLResponse** result)
{
    *result = WebURLResponse::createInstance(m_authenticationChallenge.failureResponse());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::previousFailureCount(
    /* [out, retval] */ UINT* result)
{
    *result = m_authenticationChallenge.previousFailureCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::proposedCredential(
    /* [out, retval] */ IWebURLCredential** result)
{
    *result = WebURLCredential::createInstance(m_authenticationChallenge.proposedCredential());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::protectionSpace(
    /* [out, retval] */ IWebURLProtectionSpace** result)
{
    *result = WebURLProtectionSpace::createInstance(m_authenticationChallenge.protectionSpace());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLAuthenticationChallenge::sender(
    /* [out, retval] */ IWebURLAuthenticationChallengeSender** sender)
{
    ResourceHandle* handle = m_authenticationChallenge.sourceHandle();
    *sender = WebURLAuthenticationChallengeSender::createInstance(handle);

    return S_OK;
}

// WebURLAuthenticationChallenge -------------------------------------------------------------------
const AuthenticationChallenge& WebURLAuthenticationChallenge::authenticationChallenge() const
{
    return m_authenticationChallenge;
}
