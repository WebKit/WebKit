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
#include "WebMutableURLRequest.h"

#include "WebKit.h"
#include "MarshallingHelpers.h"
#include "WebKit.h"
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/FormData.h>
#include <WebCore/ResourceHandle.h>
#include <wtf/text/CString.h>
#include <wtf/RetainPtr.h>

#if USE(CF)
#include <WebCore/CertificateCFWin.h>
#endif

#if USE(CFURLCONNECTION)
#include <CFNetwork/CFURLRequestPriv.h>
#endif

using namespace WebCore;

// IWebURLRequest ----------------------------------------------------------------

WebMutableURLRequest::WebMutableURLRequest(bool isMutable)
    : m_isMutable(isMutable)
{
    gClassCount++;
    gClassNameCount().add("WebMutableURLRequest");
}

WebMutableURLRequest* WebMutableURLRequest::createInstance()
{
    WebMutableURLRequest* instance = new WebMutableURLRequest(true);
    instance->AddRef();
    return instance;
}

WebMutableURLRequest* WebMutableURLRequest::createInstance(IWebMutableURLRequest* req)
{
    WebMutableURLRequest* instance = new WebMutableURLRequest(true);
    instance->AddRef();
    instance->m_request = static_cast<WebMutableURLRequest*>(req)->m_request;
    return instance;
}

WebMutableURLRequest* WebMutableURLRequest::createInstance(const ResourceRequest& request)
{
    WebMutableURLRequest* instance = new WebMutableURLRequest(true);
    instance->AddRef();
    instance->m_request = request;
    return instance;
}

WebMutableURLRequest* WebMutableURLRequest::createImmutableInstance()
{
    WebMutableURLRequest* instance = new WebMutableURLRequest(false);
    instance->AddRef();
    return instance;
}

WebMutableURLRequest* WebMutableURLRequest::createImmutableInstance(const ResourceRequest& request)
{
    WebMutableURLRequest* instance = new WebMutableURLRequest(false);
    instance->AddRef();
    instance->m_request = request;
    return instance;
}

WebMutableURLRequest::~WebMutableURLRequest()
{
    gClassCount--;
    gClassNameCount().remove("WebMutableURLRequest");
}

// IUnknown -------------------------------------------------------------------

HRESULT WebMutableURLRequest::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, CLSID_WebMutableURLRequest))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebURLRequest*>(this);
    else if (IsEqualGUID(riid, IID_IWebMutableURLRequest) && m_isMutable)
        *ppvObject = static_cast<IWebMutableURLRequest*>(this);
    else if (IsEqualGUID(riid, __uuidof(IWebMutableURLRequestPrivate)) && m_isMutable)
        *ppvObject = static_cast<IWebMutableURLRequestPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLRequest))
        *ppvObject = static_cast<IWebURLRequest*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebMutableURLRequest::AddRef()
{
    return ++m_refCount;
}

ULONG WebMutableURLRequest::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebURLRequest --------------------------------------------------------------------

HRESULT WebMutableURLRequest::requestWithURL(_In_ BSTR /*theURL*/, WebURLRequestCachePolicy /*cachePolicy*/, double /*timeoutInterval*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebMutableURLRequest::allHTTPHeaderFields(_COM_Outptr_opt_ IPropertyBag** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebMutableURLRequest::cachePolicy(_Out_ WebURLRequestCachePolicy* result)
{
    if (!result)
        return E_POINTER;
    *result = kit(m_request.cachePolicy());
    return S_OK;
}

HRESULT WebMutableURLRequest::HTTPBody(_COM_Outptr_opt_ IStream** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebMutableURLRequest::HTTPBodyStream(_COM_Outptr_opt_ IStream** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebMutableURLRequest::HTTPMethod(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    BString httpMethod = BString(m_request.httpMethod());
    *result = httpMethod.release();
    return S_OK;
}

HRESULT WebMutableURLRequest::HTTPShouldHandleCookies(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    bool shouldHandleCookies = m_request.allowCookies();

    *result = shouldHandleCookies ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebMutableURLRequest::initWithURL(_In_ BSTR url, WebURLRequestCachePolicy cachePolicy, double timeoutInterval)
{
    m_request.setURL(MarshallingHelpers::BSTRToKURL(url));
    m_request.setCachePolicy(core(cachePolicy));
    m_request.setTimeoutInterval(timeoutInterval);

    return S_OK;
}

HRESULT WebMutableURLRequest::mainDocumentURL(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = MarshallingHelpers::URLToBSTR(m_request.firstPartyForCookies());
    return S_OK;
}

HRESULT WebMutableURLRequest::timeoutInterval(_Out_ double* result)
{
    if (!result)
        return E_POINTER;
    *result = m_request.timeoutInterval();
    return S_OK;
}

HRESULT WebMutableURLRequest::URL(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = MarshallingHelpers::URLToBSTR(m_request.url());
    return S_OK;
}

HRESULT WebMutableURLRequest::valueForHTTPHeaderField(_In_ BSTR field, __deref_opt_out BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = BString(m_request.httpHeaderField(String(field, SysStringLen(field)))).release();
    return S_OK;
}

HRESULT WebMutableURLRequest::isEmpty(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = m_request.isEmpty();
    return S_OK;
}

HRESULT WebMutableURLRequest::isEqual(_In_opt_ IWebURLRequest* other, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    COMPtr<WebMutableURLRequest> requestImpl(Query, other);

    if (!requestImpl) {
        *result = FALSE;
        return S_OK;
    }

    *result = m_request == requestImpl->resourceRequest();
    return S_OK;
}


// IWebMutableURLRequest --------------------------------------------------------

HRESULT WebMutableURLRequest::addValue(_In_ BSTR value, _In_ BSTR field)
{
    m_request.addHTTPHeaderField(WTF::AtomString(value, SysStringLen(value)), String(field, SysStringLen(field)));
    return S_OK;
}

HRESULT WebMutableURLRequest::setAllHTTPHeaderFields(_In_opt_ IPropertyBag* /*headerFields*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebMutableURLRequest::setCachePolicy(WebURLRequestCachePolicy policy)
{
    m_request.setCachePolicy(core(policy));
    return S_OK;
}

HRESULT WebMutableURLRequest::setHTTPBody(_In_opt_ IStream* data)
{
    if (!data)
        return E_POINTER;

    STATSTG stat;
    if (FAILED(data->Stat(&stat, STATFLAG_NONAME)))
        return E_FAIL;

    if (stat.cbSize.HighPart || !stat.cbSize.LowPart)
        return E_FAIL;

    size_t length = stat.cbSize.LowPart;
    Vector<char> vector(length);
    ULONG bytesRead = 0;
    if (FAILED(data->Read(vector.data(), stat.cbSize.LowPart, &bytesRead)))
        return E_FAIL;
    m_request.setHTTPBody(FormData::create(WTFMove(vector)));
    return S_OK;
}

HRESULT WebMutableURLRequest::setHTTPBodyStream(_In_opt_ IStream* data)
{
    return setHTTPBody(data);
}

HRESULT WebMutableURLRequest::setHTTPMethod(_In_ BSTR method)
{
    m_request.setHTTPMethod(String(method));
    return S_OK;
}

HRESULT WebMutableURLRequest::setHTTPShouldHandleCookies(BOOL handleCookies)
{
    m_request.setAllowCookies(handleCookies);
    return S_OK;
}

HRESULT WebMutableURLRequest::setMainDocumentURL(_In_ BSTR theURL)
{
    m_request.setFirstPartyForCookies(MarshallingHelpers::BSTRToKURL(theURL));
    return S_OK;
}

HRESULT WebMutableURLRequest::setTimeoutInterval(double timeoutInterval)
{
    m_request.setTimeoutInterval(timeoutInterval);
    return S_OK;
}

HRESULT WebMutableURLRequest::setURL(_In_ BSTR url)
{
    m_request.setURL(MarshallingHelpers::BSTRToKURL(url));
    return S_OK;
}

HRESULT WebMutableURLRequest::setValue(_In_ BSTR value, _In_ BSTR field)
{
    String valueString(value, SysStringLen(value));
    String fieldString(field, SysStringLen(field));
    m_request.setHTTPHeaderField(fieldString, valueString);
    return S_OK;
}

HRESULT WebMutableURLRequest::setAllowsAnyHTTPSCertificate()
{
    ResourceHandle::setHostAllowsAnyHTTPSCertificate(m_request.url().host().toString());

    return S_OK;
}

HRESULT WebMutableURLRequest::setClientCertificate(ULONG_PTR cert)
{
    if (!cert)
        return E_POINTER;

    PCCERT_CONTEXT certContext = reinterpret_cast<PCCERT_CONTEXT>(cert);
#if USE(CF)
    RetainPtr<CFDataRef> certData = WebCore::copyCertificateToData(certContext);
    ResourceHandle::setClientCertificate(m_request.url().host().toString(), certData.get());
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

CFURLRequestRef WebMutableURLRequest::cfRequest()
{
    return m_request.cfURLRequest(UpdateHTTPBody);
}

HRESULT WebMutableURLRequest::mutableCopy(_COM_Outptr_opt_ IWebMutableURLRequest** result)
{
    if (!result)
        return E_POINTER;

#if USE(CFURLCONNECTION)
    RetainPtr<CFMutableURLRequestRef> mutableRequest = adoptCF(CFURLRequestCreateMutableCopy(kCFAllocatorDefault, m_request.cfURLRequest(UpdateHTTPBody)));
    *result = createInstance(ResourceRequest(mutableRequest.get()));
    return S_OK;
#else
    *result = createInstance(m_request);
    return S_OK;
#endif
}

// IWebMutableURLRequest ----------------------------------------------------

void WebMutableURLRequest::setFormData(RefPtr<FormData>&& data)
{
    m_request.setHTTPBody(WTFMove(data));
}

const RefPtr<FormData> WebMutableURLRequest::formData() const
{
    return m_request.httpBody();
}

const HTTPHeaderMap& WebMutableURLRequest::httpHeaderFields() const
{
    return m_request.httpHeaderFields();
}

const ResourceRequest& WebMutableURLRequest::resourceRequest() const
{
    return m_request;
}
