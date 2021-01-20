/*
 * Copyright (C) 2006-2007, 2015 Apple, Inc.  All rights reserved.
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
#include "WebHTMLRepresentation.h"

#include "WebKit.h"
#include "WebFrame.h"
#include "WebKitStatisticsPrivate.h"
#include <WebCore/BString.h>
#include <WebCore/Frame.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/TextResourceDecoder.h>

using namespace WebCore;

// WebHTMLRepresentation ------------------------------------------------------

WebHTMLRepresentation::WebHTMLRepresentation()
{
    WebHTMLRepresentationCount++;
    gClassCount++;
    gClassNameCount().add("WebHTMLRepresentation");
}

WebHTMLRepresentation::~WebHTMLRepresentation()
{
    if (m_frame) {
        m_frame->Release();
        m_frame = nullptr;
    }

    WebHTMLRepresentationCount--;
    gClassCount--;
    gClassNameCount().remove("WebHTMLRepresentation");
}

WebHTMLRepresentation* WebHTMLRepresentation::createInstance(WebFrame* frame)
{
    WebHTMLRepresentation* instance = new WebHTMLRepresentation();
    instance->m_frame = frame;
    frame->AddRef();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebHTMLRepresentation::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHTMLRepresentation*>(this);
    else if (IsEqualGUID(riid, IID_IWebHTMLRepresentation))
        *ppvObject = static_cast<IWebHTMLRepresentation*>(this);
    else if (IsEqualGUID(riid, IID_IWebDocumentRepresentation))
        *ppvObject = static_cast<IWebDocumentRepresentation*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebHTMLRepresentation::AddRef()
{
    return ++m_refCount;
}

ULONG WebHTMLRepresentation::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHTMLRepresentation --------------------------------------------------------------------

HRESULT WebHTMLRepresentation::supportedMIMETypes(__deref_opt_inout BSTR* /*types*/, _Inout_ int* /*cTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::supportedNonImageMIMETypes(__deref_opt_inout BSTR* /*types*/, _Inout_ int* /*cTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::supportedImageMIMETypes(__deref_opt_inout BSTR* /*types*/, _Inout_ int* /*cTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::attributedStringFromDOMNodes(_In_opt_ IDOMNode* /*startNode*/,
    int /*startOffset*/, _In_opt_ IDOMNode* /*endNode*/, int /*endOffset*/, _COM_Outptr_opt_ IDataObject** attributedString)
{
    ASSERT_NOT_REACHED();
    if (!attributedString)
        return E_POINTER;
    *attributedString = nullptr;
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::elementWithName(_In_ BSTR name, _In_opt_ IDOMElement* form, _COM_Outptr_opt_ IDOMElement** element)
{
    if (!element)
        return E_POINTER;
    *element = nullptr;
    if (!m_frame)
        return E_FAIL;

    return m_frame->elementWithName(name, form, element);
}
    
HRESULT WebHTMLRepresentation::elementDoesAutoComplete(_In_opt_ IDOMElement* element, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    BOOL doesAutoComplete;
    HRESULT hr = m_frame->elementDoesAutoComplete(element, &doesAutoComplete);
    *result = doesAutoComplete ? TRUE : FALSE;
    return hr;
}
    
HRESULT WebHTMLRepresentation::elementIsPassword(_In_opt_ IDOMElement* element, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    bool isPassword;
    HRESULT hr = m_frame->elementIsPassword(element, &isPassword);
    *result = isPassword ?  TRUE : FALSE;
    return hr;
}
    
HRESULT WebHTMLRepresentation::formForElement(_In_opt_ IDOMElement* element, _COM_Outptr_opt_ IDOMElement** form)
{
    if (!form)
        return E_POINTER;
    *form = nullptr;
    if (!m_frame)
        return E_FAIL;

    return m_frame->formForElement(element, form);
}
    
HRESULT WebHTMLRepresentation::currentForm(_COM_Outptr_opt_ IDOMElement** form)
{
    if (!form)
        return E_POINTER;
    *form = nullptr;
    if (!m_frame)
        return E_FAIL;

    return m_frame->currentForm(form);
}
    
HRESULT WebHTMLRepresentation::controlsInForm(_In_opt_ IDOMElement* form, __deref_inout_opt IDOMElement** controls, _Out_ int* cControls)
{
    return m_frame->controlsInForm(form, controls, cControls);
}
    
HRESULT WebHTMLRepresentation::deprecatedSearchForLabels(__inout_ecount_full(cLabels) BSTR* labels, int cLabels,
    _In_opt_ IDOMElement* beforeElement, __deref_opt_out BSTR* result)
{
    return m_frame->searchForLabelsBeforeElement(labels, cLabels, beforeElement, 0, 0, result);
}
    
HRESULT WebHTMLRepresentation::matchLabels(__inout_ecount_full(cLabels) BSTR* labels, int cLabels, _In_opt_ IDOMElement* againstElement,
    __deref_opt_out BSTR* result)
{
    return m_frame->matchLabelsAgainstElement(labels, cLabels, againstElement, result);
}

HRESULT WebHTMLRepresentation::searchForLabels(__inout_ecount_full(cLabels) BSTR* labels, unsigned cLabels, _In_opt_ IDOMElement* beforeElement,
    _Out_ unsigned* resultDistance, _Out_ BOOL* resultIsInCellAbove, __deref_opt_out BSTR* result)
{
    return m_frame->searchForLabelsBeforeElement(labels, cLabels, beforeElement, resultDistance, resultIsInCellAbove, result);
}

// IWebDocumentRepresentation ----------------------------------------------------------------

HRESULT WebHTMLRepresentation::setDataSource(_In_opt_ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::receivedData(_In_opt_ IStream* /*data*/, _In_opt_ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::receivedError(_In_opt_ IWebError* /*error*/, _In_opt_ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::finishedLoadingWithDataSource(_In_opt_ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT WebHTMLRepresentation::canProvideDocumentSource(_Out_ BOOL* result)
{
    bool canProvideSource;
    HRESULT hr = this->m_frame->canProvideDocumentSource(&canProvideSource);
    *result = canProvideSource ? TRUE : FALSE;
    return hr;
}
    
HRESULT WebHTMLRepresentation::documentSource(__deref_opt_out BSTR* source)
{
    if (!source)
        return E_FAIL;

    *source = nullptr;

    HRESULT hr = S_OK;

    COMPtr<IWebDataSource> dataSource;
    hr = m_frame->dataSource(&dataSource);
    if (FAILED(hr))
        return hr;

    COMPtr<IStream> data;
    hr = dataSource->data(&data);
    if (FAILED(hr))
        return hr;

    STATSTG stat;
    hr = data->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr))
        return hr;

    if (stat.cbSize.HighPart || !stat.cbSize.LowPart)
        return E_FAIL;

    Vector<char> dataBuffer(stat.cbSize.LowPart);
    ULONG read;
    
    hr = data->Read(dataBuffer.data(), static_cast<ULONG>(dataBuffer.size()), &read);
    if (FAILED(hr))
        return hr;

    WebCore::Frame* frame = core(m_frame);
    if (!frame)
        return E_FAIL;

    WebCore::Document* doc = frame->document();
    if (!doc)
        return E_FAIL;

    WebCore::TextResourceDecoder* decoder = doc->decoder();
    if (!decoder)
        return E_FAIL;

    *source = WebCore::BString(decoder->encoding().decode(dataBuffer.data(), dataBuffer.size())).release();
    return S_OK;
}
    
HRESULT WebHTMLRepresentation::title(__deref_opt_out BSTR* /*docTitle*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
