/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006, 2007 Vladimir Olexa (vladimir.olexa@gmail.com)
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
#include "ServerConnection.h"

#include "DebuggerDocument.h"

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <JavaScriptCore/RetainPtr.h>
#include <WebKit/ForEachCoClass.h>
#include <WebKit/IWebScriptCallFrame.h>
#include <WebKit/IWebScriptDebugServer.h>
#include <WebKit/WebKit.h>

#include <iostream>

ServerConnection::ServerConnection()
    : m_globalContext(0)
    , m_serverConnected(false)
{
    OleInitialize(0);
    attemptToCreateServerConnection();
}

ServerConnection::~ServerConnection()
{
    if (m_server)
        m_server->removeListener(this);

    if (m_globalContext)
        JSGlobalContextRelease(m_globalContext);
}

void ServerConnection::attemptToCreateServerConnection(JSGlobalContextRef globalContextRef)
{
    COMPtr<IWebScriptDebugServer> tempServer;

    CLSID clsid = CLSID_NULL;
    if (FAILED(CLSIDFromProgID(PROGID(WebScriptDebugServer), &clsid)))
        return;

    if (FAILED(CoCreateInstance(clsid, 0, CLSCTX_LOCAL_SERVER, IID_IWebScriptDebugServer, (void**)&tempServer)))
        return;

    if (FAILED(tempServer->sharedWebScriptDebugServer(&m_server)))
        return;

    if (FAILED(m_server->addListener(this))) {
        m_server = 0;
        return;
    }

    m_serverConnected = true;

    if (globalContextRef)
        m_globalContext = JSGlobalContextRetain(globalContextRef);
}

void ServerConnection::setGlobalContext(JSGlobalContextRef globalContextRef)
{
    m_globalContext = JSGlobalContextRetain(globalContextRef);
}

// Pause & Step

void ServerConnection::pause()
{
    if (m_server)
        m_server->pause();
}

void ServerConnection::resume()
{
    if (m_server)
        m_server->resume();
}

void ServerConnection::stepInto()
{
    if (m_server)
        m_server->step();
}

// IUnknown --------------------------------------------------
HRESULT STDMETHODCALLTYPE ServerConnection::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IWebScriptDebugListener))
        *ppvObject = static_cast<IWebScriptDebugListener*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE ServerConnection::AddRef(void)
{
    // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 1;
}

ULONG STDMETHODCALLTYPE ServerConnection::Release(void)
{
    // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 1;
}
// IWebScriptDebugListener -----------------------------------
HRESULT STDMETHODCALLTYPE ServerConnection::didLoadMainResourceForDataSource(
    /* [in] */ IWebView*,
    /* [in] */ IWebDataSource* dataSource)
{
    HRESULT ret = S_OK;
    if (!m_globalContext || !dataSource)
        return ret;

    // Get document source
    COMPtr<IWebDocumentRepresentation> rep;
    ret = dataSource->representation(&rep);
    if (FAILED(ret))
        return ret;

    BOOL canProvideDocumentSource = FALSE;
    ret = rep->canProvideDocumentSource(&canProvideDocumentSource);
    if (FAILED(ret))
        return ret;

    BSTR documentSource = 0;
    if (canProvideDocumentSource)
        ret = rep->documentSource(&documentSource);

    if (FAILED(ret) || !documentSource)
        return ret;

    JSRetainPtr<JSStringRef> documentSourceJS(Adopt, JSStringCreateWithBSTR(documentSource));
    SysFreeString(documentSource);

    // Get URL
    COMPtr<IWebURLResponse> response;
    ret = dataSource->response(&response);
    if (FAILED(ret))
        return ret;

    BSTR url = 0;
    ret = response->URL(&url);
    if (FAILED(ret))
        return ret;

    JSRetainPtr<JSStringRef> urlJS(Adopt, JSStringCreateWithBSTR(url));
    SysFreeString(url);

    DebuggerDocument::updateFileSource(m_globalContext, documentSourceJS.get(), urlJS.get());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE ServerConnection::didParseSource(
    /* [in] */ IWebView*,
    /* [in] */ BSTR sourceCode,
    /* [in] */ UINT baseLineNumber,
    /* [in] */ BSTR url,
    /* [in] */ int sourceID,
    /* [in] */ IWebFrame* webFrame)
{
    HRESULT ret = S_OK;
    if (!m_globalContext || !sourceCode)
        return ret;

    COMPtr<IWebDataSource> dataSource;
    ret = webFrame->dataSource(&dataSource);
    if (FAILED(ret))
        return ret;

    COMPtr<IWebURLResponse> response;
    ret = dataSource->response(&response);
    if (FAILED(ret))
        return ret;

    BSTR responseURL;
    ret = response->URL(&responseURL);
    if (FAILED(ret))
        return ret;

    BSTR documentSource = 0;
    if (!url || !wcscmp(responseURL, url)) {
        COMPtr<IWebDocumentRepresentation> rep;
        ret = dataSource->representation(&rep);
        if (FAILED(ret))
            return ret;

        BOOL canProvideDocumentSource;
        rep->canProvideDocumentSource(&canProvideDocumentSource);
        if (FAILED(ret))
            return ret;

        if (canProvideDocumentSource) {
            ret = rep->documentSource(&documentSource);
            if (FAILED(ret))
                return ret;
        }

        if (!url) {
            ret = response->URL(&url);
            if (FAILED(ret))
                return ret;
        }
    }
    SysFreeString(responseURL);

    JSRetainPtr<JSStringRef> sourceJS(Adopt, JSStringCreateWithBSTR(sourceCode));
    JSRetainPtr<JSStringRef> documentSourceJS(Adopt, JSStringCreateWithBSTR(documentSource));
    SysFreeString(documentSource);
    JSRetainPtr<JSStringRef> urlJS(Adopt, JSStringCreateWithBSTR(url));
    JSValueRef sidJS = JSValueMakeNumber(m_globalContext, sourceID);
    JSValueRef baseLineJS = JSValueMakeNumber(m_globalContext, baseLineNumber);

    DebuggerDocument::didParseScript(m_globalContext, sourceJS.get(), documentSourceJS.get(), urlJS.get(), sidJS, baseLineJS);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE ServerConnection::failedToParseSource(
    /* [in] */ IWebView*,
    /* [in] */ BSTR,
    /* [in] */ UINT,
    /* [in] */ BSTR,
    /* [in] */ BSTR,
    /* [in] */ IWebFrame*)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ServerConnection::didEnterCallFrame(
    /* [in] */ IWebView*,
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame*)
{
    HRESULT ret = S_OK;
    if (!m_globalContext)
        return ret;

    m_currentFrame = frame;

    JSValueRef sidJS = JSValueMakeNumber(m_globalContext, sourceID);
    JSValueRef linenoJS = JSValueMakeNumber(m_globalContext, lineNumber);

    DebuggerDocument::didEnterCallFrame(m_globalContext, sidJS, linenoJS);

    return ret;
}

HRESULT STDMETHODCALLTYPE ServerConnection::willExecuteStatement(
    /* [in] */ IWebView*,
    /* [in] */ IWebScriptCallFrame*,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame*)
{
    HRESULT ret = S_OK;
    if (!m_globalContext)
        return ret;

    JSValueRef sidJS = JSValueMakeNumber(m_globalContext, sourceID);
    JSValueRef linenoJS = JSValueMakeNumber(m_globalContext, lineNumber);

    DebuggerDocument::willExecuteStatement(m_globalContext, sidJS, linenoJS);
    return ret;
}

HRESULT STDMETHODCALLTYPE ServerConnection::willLeaveCallFrame(
    /* [in] */ IWebView*,
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame*)
{
    HRESULT ret = S_OK;
    if (!m_globalContext)
        return ret;

    JSValueRef sidJS = JSValueMakeNumber(m_globalContext, sourceID);
    JSValueRef linenoJS = JSValueMakeNumber(m_globalContext, lineNumber);

    DebuggerDocument::willLeaveCallFrame(m_globalContext, sidJS, linenoJS);

    m_currentFrame = frame;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE ServerConnection::exceptionWasRaised(
    /* [in] */ IWebView*,
    /* [in] */ IWebScriptCallFrame*,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame*)
{
    HRESULT ret = S_OK;
    if (!m_globalContext)
        return ret;

    JSValueRef sidJS = JSValueMakeNumber(m_globalContext, sourceID);
    JSValueRef linenoJS = JSValueMakeNumber(m_globalContext, lineNumber);

    DebuggerDocument::exceptionWasRaised(m_globalContext, sidJS, linenoJS);

    return ret;
}

HRESULT STDMETHODCALLTYPE ServerConnection::serverDidDie()
{
    m_server = 0;
    m_currentFrame = 0;
    m_serverConnected = false;
    return S_OK;
}

// Stack & Variables

IWebScriptCallFrame* ServerConnection::currentFrame() const
{
    return m_currentFrame.get();
}

COMPtr<IWebScriptCallFrame> ServerConnection::getCallerFrame(int callFrame) const
{
    COMPtr<IWebScriptCallFrame> cframe = currentFrame();
    for (int count = 0; count < callFrame; count++) {
        COMPtr<IWebScriptCallFrame> callerFrame;
        if (FAILED(cframe->caller(&callerFrame)))
            return 0;

        cframe = callerFrame;
    }

    return cframe;
}
