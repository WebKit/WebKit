/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
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
#include "WebHTMLRepresentation.h"

#include "DOMCore.h"
#include "WebFrame.h"
#include "WebKitStatisticsPrivate.h"
#pragma warning(push, 0)
#include <WebCore/HTMLInputElement.h>
#pragma warning(pop)

using namespace WebCore;

// WebHTMLRepresentation ------------------------------------------------------

WebHTMLRepresentation::WebHTMLRepresentation()
: m_refCount(0)
, m_frame(0)
{
    WebHTMLRepresentationCount++;
    gClassCount++;
}

WebHTMLRepresentation::~WebHTMLRepresentation()
{
    if (m_frame) {
        m_frame->Release();
        m_frame = 0;
    }

    WebHTMLRepresentationCount--;
    gClassCount--;
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

HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
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

ULONG STDMETHODCALLTYPE WebHTMLRepresentation::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHTMLRepresentation::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHTMLRepresentation --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::supportedMIMETypes( 
        /* [out][in] */ BSTR* /*types*/,
        /* [out][in] */ int* /*cTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::supportedNonImageMIMETypes( 
        /* [out][in] */ BSTR* /*types*/,
        /* [out][in] */ int* /*cTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::supportedImageMIMETypes( 
        /* [out][in] */ BSTR* /*types*/,
        /* [out][in] */ int* /*cTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::attributedStringFromDOMNodes( 
        /* [in] */ IDeprecatedDOMNode* /*startNode*/,
        /* [in] */ int /*startOffset*/,
        /* [in] */ IDeprecatedDOMNode* /*endNode*/,
        /* [in] */ int /*endOffset*/,
        /* [retval][out] */ IDataObject** /*attributedString*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::elementWithName( 
        /* [in] */ BSTR name,
        /* [in] */ IDeprecatedDOMElement* form,
        /* [retval][out] */ IDeprecatedDOMElement** element)
{
    if (!m_frame)
        return E_FAIL;

    return m_frame->elementWithName(name, form, element);
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::elementDoesAutoComplete( 
        /* [in] */ IDeprecatedDOMElement* element,
        /* [retval][out] */ BOOL* result)
{
    bool doesAutoComplete;
    HRESULT hr = m_frame->elementDoesAutoComplete(element, &doesAutoComplete);
    *result = doesAutoComplete ? TRUE : FALSE;
    return hr;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::elementIsPassword( 
        /* [in] */ IDeprecatedDOMElement* element,
        /* [retval][out] */ BOOL* result)
{
    bool isPassword;
    HRESULT hr = m_frame->elementIsPassword(element, &isPassword);
    *result = isPassword ?  TRUE : FALSE;
    return hr;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::formForElement( 
        /* [in] */ IDeprecatedDOMElement* element,
        /* [retval][out] */ IDeprecatedDOMElement** form)
{
    if (!m_frame)
        return E_FAIL;

    return m_frame->formForElement(element, form);
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::currentForm( 
        /* [retval][out] */ IDeprecatedDOMElement** form)
{
    if (!m_frame)
        return E_FAIL;

    return m_frame->currentForm(form);
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::controlsInForm( 
        /* [in] */ IDeprecatedDOMElement* form,
        /* [out][in] */ IDeprecatedDOMElement** controls,
        /* [out][in] */ int* cControls)
{
    return m_frame->controlsInForm(form, controls, cControls);
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::searchForLabels( 
        /* [size_is][in] */ BSTR* labels,
        /* [in] */ int cLabels,
        /* [in] */ IDeprecatedDOMElement* beforeElement,
        /* [retval][out] */ BSTR* result)
{
    return m_frame->searchForLabelsBeforeElement(labels, cLabels, beforeElement, result);
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::matchLabels( 
        /* [size_is][in] */ BSTR* labels,
        /* [in] */ int cLabels,
        /* [in] */ IDeprecatedDOMElement* againstElement,
        /* [retval][out] */ BSTR* result)
{
    return m_frame->matchLabelsAgainstElement(labels, cLabels, againstElement, result);
}

// IWebDocumentRepresentation ----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::setDataSource( 
        /* [in] */ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::receivedData( 
        /* [in] */ IStream* /*data*/,
        /* [in] */ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::receivedError( 
        /* [in] */ IWebError* /*error*/,
        /* [in] */ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::finishedLoadingWithDataSource( 
        /* [in] */ IWebDataSource* /*dataSource*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::canProvideDocumentSource( 
        /* [retval][out] */ BOOL* result)
{
    bool canProvideSource;
    HRESULT hr = this->m_frame->canProvideDocumentSource(&canProvideSource);
    *result = canProvideSource ? TRUE : FALSE;
    return hr;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::documentSource( 
        /* [retval][out] */ BSTR* /*source*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebHTMLRepresentation::title( 
        /* [retval][out] */ BSTR* /*docTitle*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
