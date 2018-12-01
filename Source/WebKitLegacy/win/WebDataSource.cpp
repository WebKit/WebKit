/*
 * Copyright (C) 2006-2007, 2013, 2015 Apple Inc.  All rights reserved.
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
#include "WebDataSource.h"

#include "WebKit.h"
#include "MemoryStream.h"
#include "WebDocumentLoader.h"
#include "WebError.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebKit.h"
#include "WebHTMLRepresentation.h"
#include "WebKitStatisticsPrivate.h"
#include "WebMutableURLRequest.h"
#include "WebResource.h"
#include "WebURLResponse.h"
#include <WebCore/BString.h>
#include <WebCore/CachedResourceLoader.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <wtf/URL.h>

using namespace WebCore;

// WebDataSource ----------------------------------------------------------------

// {F230854D-7091-428a-8DB5-37CABA44C105}
const GUID IID_WebDataSource = { 0x5c2f9099, 0xe65e, 0x4a0f, { 0x9c, 0xa0, 0x6a, 0xd6, 0x92, 0x52, 0xa6, 0x2a } };

WebDataSource::WebDataSource(WebDocumentLoader* loader)
    : m_loader(loader)
{
    WebDataSourceCount++;
    gClassCount++;
    gClassNameCount().add("WebDataSource");
}

WebDataSource::~WebDataSource()
{
    if (m_loader)
        m_loader->detachDataSource();
    WebDataSourceCount--;
    gClassCount--;
    gClassNameCount().remove("WebDataSource");
}

WebDataSource* WebDataSource::createInstance(WebDocumentLoader* loader)
{
    WebDataSource* instance = new WebDataSource(loader);
    instance->AddRef();
    return instance;
}

WebDocumentLoader* WebDataSource::documentLoader() const
{
    return m_loader.get();
}

// IWebDataSourcePrivate ------------------------------------------------------

HRESULT WebDataSource::overrideEncoding(_Deref_opt_out_ BSTR* /*encoding*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebDataSource::setOverrideEncoding(_In_ BSTR /*encoding*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebDataSource::mainDocumentError(_COM_Outptr_opt_ IWebError** error)
{
    if (!error) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *error = nullptr;

    if (!m_loader)
        return E_UNEXPECTED;

    if (m_loader->mainDocumentError().isNull())
        return S_OK;

    *error = WebError::createInstance(m_loader->mainDocumentError());
    return S_OK;
}

HRESULT WebDataSource::setDeferMainResourceDataLoad(BOOL flag)
{
    if (!m_loader)
        return E_UNEXPECTED;

    m_loader->setDeferMainResourceDataLoad(flag);
    return S_OK;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebDataSource::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_WebDataSource))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDataSource*>(this);
    else if (IsEqualGUID(riid, IID_IWebDataSource))
        *ppvObject = static_cast<IWebDataSource*>(this);
    else if (IsEqualGUID(riid, IID_IWebDataSourcePrivate))
        *ppvObject = static_cast<IWebDataSourcePrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebDataSource::AddRef()
{
    return ++m_refCount;
}

ULONG WebDataSource::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebDataSource --------------------------------------------------------------

HRESULT WebDataSource::initWithRequest(_In_opt_ IWebURLRequest* /*request*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebDataSource::data(_COM_Outptr_opt_ IStream** stream)
{
    if (!stream)
        return E_POINTER;
    *stream = nullptr;
    if (!m_loader)
        return E_UNEXPECTED;

    return MemoryStream::createInstance(m_loader->mainResourceData()).copyRefTo(stream);
}

HRESULT WebDataSource::representation(_COM_Outptr_opt_ IWebDocumentRepresentation** rep)
{
    if (!rep)
        return E_POINTER;
    *rep = nullptr;
    if (!m_loader)
        return E_UNEXPECTED;

    HRESULT hr = S_OK;
    if (!m_representation) {
        WebHTMLRepresentation* htmlRep = WebHTMLRepresentation::createInstance(static_cast<WebFrameLoaderClient&>(m_loader->frameLoader()->client()).webFrame());
        hr = htmlRep->QueryInterface(IID_IWebDocumentRepresentation, (void**) &m_representation);
        htmlRep->Release();
    }

    return m_representation.copyRefTo(rep);
}

HRESULT WebDataSource::webFrame(_COM_Outptr_opt_ IWebFrame** frame)
{
    if (!frame)
        return E_POINTER;
    *frame = nullptr;
    if (!m_loader)
        return E_UNEXPECTED;

    *frame = static_cast<WebFrameLoaderClient&>(m_loader->frameLoader()->client()).webFrame();
    (*frame)->AddRef();
    return S_OK;
}

HRESULT WebDataSource::initialRequest(_COM_Outptr_opt_ IWebURLRequest** request)
{
    if (!request)
        return E_POINTER;
    *request = nullptr;
    if (!m_loader)
        return E_UNEXPECTED;

    *request = WebMutableURLRequest::createInstance(m_loader->originalRequest());
    return S_OK;
}

HRESULT WebDataSource::request(_COM_Outptr_opt_ IWebMutableURLRequest** request)
{
    if (!request)
        return E_POINTER;
    *request = nullptr;
    if (!m_loader)
        return E_UNEXPECTED;

    *request = WebMutableURLRequest::createInstance(m_loader->request());
    return S_OK;
}

HRESULT WebDataSource::response(_COM_Outptr_opt_ IWebURLResponse** response)
{
    if (!response)
        return E_POINTER;
    *response = nullptr;
    if (!m_loader)
        return E_UNEXPECTED;

    *response = WebURLResponse::createInstance(m_loader->response());
    return S_OK;
}

HRESULT WebDataSource::textEncodingName(_Deref_opt_out_ BSTR* name)
{
    if (!name)
        return E_POINTER;

    if (!m_loader)
        return E_UNEXPECTED;

    String encoding = m_loader->overrideEncoding();
    if (encoding.isNull())
        encoding = m_loader->response().textEncodingName();

    *name = BString(encoding).release();

    return S_OK;
}

HRESULT WebDataSource::isLoading(_Out_ BOOL* loading)
{
    if (!loading)
        return E_POINTER;

    if (!m_loader)
        return E_UNEXPECTED;

    *loading = m_loader->isLoadingInAPISense();
    return S_OK;
}

HRESULT WebDataSource::pageTitle(_Deref_opt_out_ BSTR* title)
{
    if (!title)
        return E_POINTER;

    if (!m_loader)
        return E_UNEXPECTED;

    *title = BString(m_loader->title().string).release();
    return S_OK;
}

HRESULT WebDataSource::unreachableURL(_Deref_opt_out_ BSTR* url)
{
    if (!url)
        return E_POINTER;

    if (!m_loader)
        return E_UNEXPECTED;

    URL unreachableURL = m_loader->unreachableURL();
    BString urlString(unreachableURL.string());

    *url = urlString.release();
    return S_OK;
}

HRESULT WebDataSource::webArchive(_COM_Outptr_opt_ IWebArchive** archive)
{
    ASSERT_NOT_REACHED();
    if (!archive)
        return E_POINTER;
    *archive = nullptr;
    return E_NOTIMPL;
}

HRESULT WebDataSource::mainResource(_COM_Outptr_opt_ IWebResource** resource)
{
    ASSERT_NOT_REACHED();
    if (!resource)
        return E_POINTER;
    *resource = nullptr;
    return E_NOTIMPL;
}

HRESULT WebDataSource::subresources(_COM_Outptr_opt_ IEnumVARIANT** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebDataSource::subresourceForURL(_In_ BSTR url, _COM_Outptr_opt_ IWebResource** resource)
{
    if (!resource) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *resource = nullptr;

    if (!m_loader)
        return E_UNEXPECTED;
    
    Document* doc = m_loader->frameLoader()->frame().document();
    if (!doc)
        return E_FAIL;

    CachedResource* cachedResource = doc->cachedResourceLoader().cachedResource(String(url));

    if (!cachedResource)
        return E_FAIL;

    *resource = WebResource::createInstance(cachedResource->resourceBuffer(), cachedResource->response());
    return S_OK;
}

HRESULT WebDataSource::addSubresource(_In_opt_ IWebResource* /*subresource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
