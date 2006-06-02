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

#ifndef WebFrame_H
#define WebFrame_H

#include "DOMCore.h"
#include "IWebFrame.h"
#include "WebDataSource.h"

#pragma warning(push, 0)
#include "TransferJobClient.h"
#include "FrameWin.h"
#include "PlatformString.h"
#pragma warning(pop)

namespace WebCore {
    class Frame;
}

typedef enum {
    WebFrameLoadTypeStandard,
    WebFrameLoadTypeBack,
    WebFrameLoadTypeForward,
    WebFrameLoadTypeIndexedBackForward, // a multi-item hop in the backforward list
    WebFrameLoadTypeReload,
    WebFrameLoadTypeReloadAllowingStaleData,
    WebFrameLoadTypeSame,               // user loads same URL again (but not reload button)
    WebFrameLoadTypeInternal,
    WebFrameLoadTypeReplace
} WebFrameLoadType;

class WebFrame : public IWebFrame, public WebCore::TransferJobClient, public WebCore::FrameWinClient
{
public:
    static WebFrame* createInstance();
protected:
    WebFrame();
    ~WebFrame();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    //IWebFrame
    virtual HRESULT STDMETHODCALLTYPE initWithName( 
        /* [in] */ BSTR name,
        /* [in] */ IWebFrameView *view,
        /* [in] */ IWebView *webView);
    
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *frameName);
    
    virtual HRESULT STDMETHODCALLTYPE webView( 
        /* [retval][out] */ IWebView *view);
    
    virtual HRESULT STDMETHODCALLTYPE frameView( 
        /* [retval][out] */ IWebFrameView *view);
    
    virtual HRESULT STDMETHODCALLTYPE DOMDocument( 
        /* [retval][out] */ IDOMDocument** document);
    
    virtual HRESULT STDMETHODCALLTYPE frameElement( 
        /* [retval][out] */ IDOMHTMLElement *frameElement);
    
    virtual HRESULT STDMETHODCALLTYPE loadRequest( 
        /* [in] */ IWebURLRequest *request);
    
    virtual HRESULT STDMETHODCALLTYPE loadData( 
        /* [in] */ IStream *data,
        /* [in] */ BSTR mimeType,
        /* [in] */ BSTR textEncodingName,
        /* [in] */ BSTR url);
    
    virtual HRESULT STDMETHODCALLTYPE loadHTMLString( 
        /* [in] */ BSTR string,
        /* [in] */ BSTR baseURL);
    
    virtual HRESULT STDMETHODCALLTYPE loadAlternateHTMLString( 
        /* [in] */ BSTR str,
        /* [in] */ BSTR baseURL,
        /* [in] */ BSTR unreachableURL);
    
    virtual HRESULT STDMETHODCALLTYPE loadArchive( 
        /* [in] */ IWebArchive *archive);
    
    virtual HRESULT STDMETHODCALLTYPE dataSource( 
        /* [retval][out] */ IWebDataSource **source);
    
    virtual HRESULT STDMETHODCALLTYPE provisionalDataSource( 
        /* [retval][out] */ IWebDataSource **source);
    
    virtual HRESULT STDMETHODCALLTYPE stopLoading( void);
    
    virtual HRESULT STDMETHODCALLTYPE reload( void);
    
    virtual HRESULT STDMETHODCALLTYPE findFrameNamed( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE parentFrame( 
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE childFrames( 
        /* [out] */ int *frameCount,
        /* [retval][out] */ IWebFrame ***frames);

    // TransferJobClient
    virtual void receivedRedirect(WebCore::TransferJob*, const KURL&);
    virtual void receivedResponse(WebCore::TransferJob*, WebCore::PlatformResponse);
    virtual void receivedData(WebCore::TransferJob*, const char*, int);
    virtual void receivedAllData(WebCore::TransferJob*);
    virtual void receivedAllData(WebCore::TransferJob*, WebCore::PlatformData);

    // FrameWinClient
    virtual void openURL(const DeprecatedString&, bool lockHistory);
    virtual void submitForm(const WebCore::String& method, const KURL&, const WebCore::FormData*);
    virtual void setTitle(const WebCore::String& title);
    virtual void setStatusText(const WebCore::String& title);

    // WebFrame
    void paint();
    WebCore::Frame* impl();
    HRESULT loadDataSource(WebDataSource* dataSource);
    bool loading();
    HRESULT goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType);
    HRESULT loadItem(IWebHistoryItem* item, WebFrameLoadType withLoadType);

protected:
    int getObjectCacheSize();


protected:
    class WebFramePrivate;
    WebFramePrivate*    d;
    ULONG               m_refCount;
    IWebDataSource*      m_dataSource;
    IWebDataSource*      m_provisionalDataSource;
    WebFrameLoadType    m_loadType;
    bool                m_quickRedirectComing;
};

#endif
