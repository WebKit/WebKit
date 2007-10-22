/*
 * Copyright (C) 2007 Apple, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "DebuggerClient.h"

#include "DebuggerDocument.h"
#include "ServerConnection.h"

#include <WebKit/IWebView.h>
#include <JavaScriptCore/JSContextRef.h>

DebuggerClient::DebuggerClient()
    : m_webViewLoaded(false)
{
}

DebuggerClient::DebuggerClient(const std::wstring& serverName)
    : m_webViewLoaded(false)
{
    initWithServerName(serverName);
}

void DebuggerClient::initWithServerName(const std::wstring& serverName)
{
    m_debuggerDocument.set(new DebuggerDocument(ServerConnection::initWithServerName(serverName)));
}

// IUnknown ------------------------------
HRESULT STDMETHODCALLTYPE DebuggerClient::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE DebuggerClient::AddRef(void)
{   // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 2;
}

ULONG STDMETHODCALLTYPE DebuggerClient::Release(void)
{   // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 2;
}

// IWebFrameLoadDelegate ------------------------------
HRESULT STDMETHODCALLTYPE DebuggerClient::didFinishLoadForFrame(
    /* [in] */ IWebView*,
    /* [in] */ IWebFrame* frame)
{
    // FIXME: Check the IWebView passed in vs the m_webView which should be used?
    // note: this is Drosera's own WebView, not the one in the app that we are attached to.
    m_webViewLoaded = true;

    COMPtr<IWebFrame> mainFrame;
    HRESULT hr = m_webView->mainFrame(&mainFrame);
    if (FAILED(hr))
        return hr;

    if (mainFrame != frame)
        return S_OK;

    // FIXME: This cannot be implemented until IWebFrame has a globalContext
    //    ServerConnection* server = m_debuggerDocument->server();
    //    if (server)
    //        server->setGlobalContext(mainFrame->globalContext());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DebuggerClient::windowScriptObjectAvailable( 
    /* [in] */ IWebView*,
    /* [in] */ JSContextRef context,
    /* [in] */ JSObjectRef windowObject)
{
    JSValueRef exception = 0;
    if (m_debuggerDocument)
        m_debuggerDocument->windowScriptObjectAvailable(context, windowObject, &exception);

    if (exception)
        return E_FAIL;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DebuggerClient::didReceiveTitle(
    /* [in] */ IWebView*,
    /* [in] */ BSTR,
    /* [in] */ IWebFrame*)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DebuggerClient::createWebViewWithRequest( 
        /* [in] */ IWebView*,
        /* [in] */ IWebURLRequest*,
        /* [retval][out] */ IWebView**)
{
    return S_OK;
}


// IWebUIDelegate ------------------------------
HRESULT STDMETHODCALLTYPE DebuggerClient::runJavaScriptAlertPanelWithMessage(  // For debugging purposes
    /* [in] */ IWebView*,
    /* [in] */ BSTR message)
{
#ifndef NDEBUG
    fwprintf(stderr, L"%s\n", message ? message : L"");
#else
    (void)message;
#endif
    return S_OK;
}

