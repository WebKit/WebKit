/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef WebDataSource_H
#define WebDataSource_H

#include "IWebDataSource.h"

class WebFrame;
class WebMutableURLRequest;

class WebDataSource : public IWebDataSource
{
public:
    static WebDataSource* createInstance(WebFrame* frame);
protected:
    WebDataSource();
    ~WebDataSource();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebDataSource
    virtual HRESULT STDMETHODCALLTYPE initWithRequest( 
        /* [in] */ IWebURLRequest *request);
    
    virtual HRESULT STDMETHODCALLTYPE data( 
        /* [retval][out] */ IStream **stream);
    
    virtual HRESULT STDMETHODCALLTYPE representation( 
        /* [retval][out] */ IWebDocumentRepresentation **rep);
    
    virtual HRESULT STDMETHODCALLTYPE webFrame( 
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE initialRequest( 
        /* [retval][out] */ IWebURLRequest **request);
    
    virtual HRESULT STDMETHODCALLTYPE request( 
        /* [retval][out] */ IWebMutableURLRequest **request);
    
    virtual HRESULT STDMETHODCALLTYPE response( 
        /* [retval][out] */ IWebURLResponse **response);
    
    virtual HRESULT STDMETHODCALLTYPE textEncodingName( 
        /* [retval][out] */ BSTR *name);
    
    virtual HRESULT STDMETHODCALLTYPE isLoading( 
        /* [retval][out] */ BOOL *loading);
    
    virtual HRESULT STDMETHODCALLTYPE pageTitle( 
        /* [retval][out] */ BSTR *title);
    
    virtual HRESULT STDMETHODCALLTYPE unreachableURL( 
        /* [retval][out] */ BSTR *url);
    
    virtual HRESULT STDMETHODCALLTYPE webArchive( 
        /* [retval][out] */ IWebArchive *archive);
    
    virtual HRESULT STDMETHODCALLTYPE mainResource( 
        /* [retval][out] */ IWebResource *resource);
    
    virtual HRESULT STDMETHODCALLTYPE subresources( 
        /* [out] */ int *resourceCount,
        /* [retval][out] */ IWebResource ***resources);
    
    virtual HRESULT STDMETHODCALLTYPE subresourceForURL( 
        /* [in] */ BSTR url,
        /* [retval][out] */ IWebResource *resource);
    
    virtual HRESULT STDMETHODCALLTYPE addSubresource( 
        /* [in] */ IWebResource *subresource);

    // WebDataSource

protected:
    ULONG m_refCount;
    IWebMutableURLRequest* m_initialRequest;
    IWebMutableURLRequest* m_request;
    WebFrame* m_frame;
};

#endif
