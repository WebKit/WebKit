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

#ifndef WebHTMLRepresentation_H
#define WebHTMLRepresentation_H

#include "WebKit.h"

class WebFrame;

class WebHTMLRepresentation final : public IWebHTMLRepresentation, IWebDocumentRepresentation
{
public:
    static WebHTMLRepresentation* createInstance(WebFrame* frame);
protected:
    WebHTMLRepresentation();
    ~WebHTMLRepresentation();
public:

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebHTMLRepresentation
    virtual HRESULT STDMETHODCALLTYPE supportedMIMETypes(__deref_opt_inout BSTR* types, _Inout_ int* cTypes);
    virtual HRESULT STDMETHODCALLTYPE supportedNonImageMIMETypes(__deref_opt_inout BSTR* types, _Inout_ int* cTypes);
    virtual HRESULT STDMETHODCALLTYPE supportedImageMIMETypes(__deref_opt_inout BSTR* types, _Inout_ int* cTypes);
    virtual HRESULT STDMETHODCALLTYPE attributedStringFromDOMNodes(_In_opt_ IDOMNode* startNode, int startOffset,
        _In_opt_ IDOMNode* endNode, int endOffset, _COM_Outptr_opt_ IDataObject** attributedString);
    virtual HRESULT STDMETHODCALLTYPE elementWithName(_In_ BSTR name, _In_opt_ IDOMElement* form, _COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE elementDoesAutoComplete(_In_opt_ IDOMElement*, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE elementIsPassword(_In_opt_ IDOMElement*, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE formForElement(_In_opt_ IDOMElement*, _COM_Outptr_opt_ IDOMElement** form);
    virtual HRESULT STDMETHODCALLTYPE currentForm(_COM_Outptr_opt_ IDOMElement** form);
    virtual HRESULT STDMETHODCALLTYPE controlsInForm(_In_opt_ IDOMElement* form, __deref_inout_opt IDOMElement** controls, _Out_ int* cControls);
    
    /* Deprecated. Use the variant that includes resultDistance and resultIsInCellAbove instead. */
    virtual HRESULT STDMETHODCALLTYPE deprecatedSearchForLabels(__inout_ecount_full(cLabels) BSTR* labels, int cLabels, _In_opt_ IDOMElement* beforeElement, __deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE matchLabels(__inout_ecount_full(cLabels) BSTR* labels, int cLabels, _In_opt_ IDOMElement* againstElement, __deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE searchForLabels(__inout_ecount_full(cLabels) BSTR* labels, unsigned cLabels, _In_opt_ IDOMElement* beforeElement,
        _Out_ unsigned* resultDistance, _Out_ BOOL* resultIsInCellAbove, __deref_opt_out BSTR* result);
    
    // IWebDocumentRepresentation
    virtual HRESULT STDMETHODCALLTYPE setDataSource(_In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE receivedData(_In_opt_ IStream* data, _In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE receivedError(_In_opt_ IWebError*, _In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE finishedLoadingWithDataSource(_In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE canProvideDocumentSource(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE documentSource(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR*);

protected:
    ULONG m_refCount { 0 };
    WebFrame* m_frame { nullptr };
};

#endif
