/*
 * Copyright (C) 2007, 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "WebInspector.h"

#include "WebInspectorClient.h"
#include "WebKitDLL.h"
#include "WebView.h"
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>
#include <wtf/Assertions.h>

using namespace WebCore;

WebInspector* WebInspector::createInstance(WebView* inspectedWebView, WebInspectorClient* inspectorClient)
{
    WebInspector* inspector = new WebInspector(inspectedWebView, inspectorClient);
    inspector->AddRef();
    return inspector;
}

WebInspector::WebInspector(WebView* inspectedWebView, WebInspectorClient* inspectorClient)
    : m_inspectedWebView(inspectedWebView)
    , m_inspectorClient(inspectorClient)
{
    ASSERT_ARG(inspectedWebView, inspectedWebView);

    gClassCount++;
    gClassNameCount().add("WebInspector");
}

WebInspector::~WebInspector()
{
    gClassCount--;
    gClassNameCount().remove("WebInspector");
}

WebInspectorFrontendClient* WebInspector::frontendClient()
{
    return m_inspectorClient ? m_inspectorClient->frontendClient() : nullptr;
}

void WebInspector::inspectedWebViewClosed()
{
    m_inspectedWebView = nullptr;
    m_inspectorClient = nullptr;
}

HRESULT WebInspector::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IWebInspector))
        *ppvObject = static_cast<IWebInspector*>(this);
    else if (IsEqualGUID(riid, IID_IWebInspectorPrivate))
        *ppvObject = static_cast<IWebInspectorPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebInspector*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebInspector::AddRef()
{
    return ++m_refCount;
}

ULONG WebInspector::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

HRESULT WebInspector::show()
{
    if (m_inspectedWebView)
        if (Page* page = m_inspectedWebView->page())
            page->inspectorController().show();

    return S_OK;
}

HRESULT WebInspector::showConsole()
{
    if (frontendClient())
        frontendClient()->showConsole();

    return S_OK;
}

HRESULT WebInspector::unused1()
{
    return S_OK;
}

HRESULT WebInspector::close()
{
    if (frontendClient())
        frontendClient()->closeWindow();

    return S_OK;
}

HRESULT WebInspector::attach()
{
    return S_OK;
}

HRESULT WebInspector::detach()
{
    return S_OK;
}

HRESULT WebInspector::isDebuggingJavaScript(_Out_ BOOL* isDebugging)
{
    if (!isDebugging)
        return E_POINTER;

    *isDebugging = FALSE;

    if (!frontendClient())
        return S_OK;

    *isDebugging = frontendClient()->isDebuggingEnabled();
    return S_OK;
}

HRESULT WebInspector::toggleDebuggingJavaScript()
{
    show();

    if (!frontendClient())
        return S_OK;

    if (frontendClient()->isDebuggingEnabled())
        frontendClient()->setDebuggingEnabled(false);
    else
        frontendClient()->setDebuggingEnabled(true);

    return S_OK;
}

HRESULT WebInspector::isProfilingJavaScript(_Out_ BOOL* isProfiling)
{
    if (!isProfiling)
        return E_POINTER;

    *isProfiling = FALSE;

    if (!frontendClient())
        return S_OK;

    *isProfiling = frontendClient()->isProfilingJavaScript();

    return S_OK;
}

HRESULT WebInspector::toggleProfilingJavaScript()
{
    show();

    if (!frontendClient())
        return S_OK;

    if (frontendClient()->isProfilingJavaScript())
        frontendClient()->stopProfilingJavaScript();
    else
        frontendClient()->startProfilingJavaScript();

    return S_OK;
}

HRESULT WebInspector::evaluateInFrontend(_In_ BSTR bScript)
{
    if (!m_inspectedWebView)
        return S_OK;

    Page* inspectedPage = m_inspectedWebView->page();
    if (!inspectedPage)
        return S_OK;

    String script(bScript, SysStringLen(bScript));
    inspectedPage->inspectorController().evaluateForTestInFrontend(script);
    return S_OK;
}

HRESULT WebInspector::isTimelineProfilingEnabled(_Out_ BOOL* isEnabled)
{
    if (!isEnabled)
        return E_POINTER;

    *isEnabled = FALSE;

    if (!frontendClient())
        return S_OK;

    *isEnabled = frontendClient()->isTimelineProfilingEnabled();
    return S_OK;
}

HRESULT WebInspector::setTimelineProfilingEnabled(BOOL enabled)
{
    show();

    if (!frontendClient())
        return S_OK;

    frontendClient()->setTimelineProfilingEnabled(enabled);
    return S_OK;
}
