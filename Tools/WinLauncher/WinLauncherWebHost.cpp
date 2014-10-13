/*
 * Copyright (C) 2006, 2008, 2013-2014 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2011 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Appcelerator, Inc. All rights reserved.
 * Copyright (C) 2013 Alex Christensen. All rights reserved.
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

#include "stdafx.h"
#include "WinLauncherWebHost.h"

#include "DOMDefaultImpl.h"
#include "PageLoadTestClient.h"
#include "WinLauncher.h"
#include <WebKit/WebKit.h>

class SimpleEventListener : public DOMEventListener {
public:
    SimpleEventListener(LPWSTR type)
    {
        wcsncpy_s(m_eventType, 100, type, 100);
        m_eventType[99] = 0;
    }

    virtual HRESULT STDMETHODCALLTYPE handleEvent(IDOMEvent* evt)
    {
        wchar_t message[255];
        wcscpy_s(message, 255, m_eventType);
        wcscat_s(message, 255, L" event fired!");
        ::MessageBox(0, message, L"Event Handler", MB_OK);
        return S_OK;
    }

private:
    wchar_t m_eventType[100];
};

typedef _com_ptr_t<_com_IIID<IWebFrame, &__uuidof(IWebFrame)>> IWebFramePtr;
typedef _com_ptr_t<_com_IIID<IWebDataSource, &__uuidof(IWebDataSource)>> IWebDataSourcePtr;
typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;

HRESULT WinLauncherWebHost::updateAddressBar(IWebView& webView)
{
    IWebFramePtr mainFrame;
    HRESULT hr = webView.mainFrame(&mainFrame.GetInterfacePtr());
    if (FAILED(hr))
        return 0;

    IWebDataSourcePtr dataSource;
    hr = mainFrame->dataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        hr = mainFrame->provisionalDataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        return 0;

    IWebMutableURLRequestPtr request;
    hr = dataSource->request(&request.GetInterfacePtr());
    if (FAILED(hr) || !request)
        return 0;

    _bstr_t frameURL;
    hr = request->mainDocumentURL(frameURL.GetAddress());
    if (FAILED(hr))
        return 0;

    if (frameURL.length()) {
        m_client->pageLoadTestClient().setPageURL(frameURL);
        m_client->pageLoadTestClient().didCommitLoad();
    }

    loadURL(frameURL);

    return 0;
}

void WinLauncherWebHost::loadURL(_bstr_t& url)
{
    ::SendMessage(m_hURLBarWnd, static_cast<UINT>(WM_SETTEXT), 0, reinterpret_cast<LPARAM>(url.GetBSTR()));
}

HRESULT WinLauncherWebHost::didFailProvisionalLoadWithError(IWebView*, IWebError *error, IWebFrame*)
{
    _bstr_t errorDescription;
    HRESULT hr = error->localizedDescription(errorDescription.GetAddress());
    if (FAILED(hr))
        errorDescription = L"Failed to load page and to localize error description.";

    if (_wcsicmp(errorDescription, L"Cancelled"))
        ::MessageBoxW(0, static_cast<LPCWSTR>(errorDescription), L"Error", MB_APPLMODAL | MB_OK);

    return S_OK;
}

HRESULT WinLauncherWebHost::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WinLauncherWebHost::AddRef()
{
    return ++m_refCount;
}

ULONG WinLauncherWebHost::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

typedef _com_ptr_t<_com_IIID<IDOMDocument, &__uuidof(IDOMDocument)>> IDOMDocumentPtr;
typedef _com_ptr_t<_com_IIID<IDOMElement, &__uuidof(IDOMElement)>> IDOMElementPtr;
typedef _com_ptr_t<_com_IIID<IDOMEventTarget, &__uuidof(IDOMEventTarget)>> IDOMEventTargetPtr;

HRESULT WinLauncherWebHost::didFinishLoadForFrame(IWebView* webView, IWebFrame* frame)
{
    if (!frame || !webView)
        return E_POINTER;

    BOOL mainFrame;
    if (SUCCEEDED(frame->isMainFrame(&mainFrame))) {
        if (mainFrame)
            m_client->pageLoadTestClient().didFinishLoad();
    }

    IDOMDocumentPtr doc;
    frame->DOMDocument(&doc.GetInterfacePtr());

    IDOMElementPtr element;
    IDOMEventTargetPtr target;

    if (m_client)
        m_client->showLastVisitedSites(*webView);

    // The following is for the test page:
    HRESULT hr = doc->getElementById(L"webkit logo", &element.GetInterfacePtr());
    if (!SUCCEEDED(hr))
        return hr;

    hr = element->QueryInterface(IID_IDOMEventTarget, reinterpret_cast<void**>(&target.GetInterfacePtr()));
    if (!SUCCEEDED(hr))
        return hr;

    hr = target->addEventListener(L"click", new SimpleEventListener (L"webkit logo click"), FALSE);
    if (!SUCCEEDED(hr))
        return hr;

    return hr;
}

HRESULT WinLauncherWebHost::didStartProvisionalLoadForFrame(IWebView*, IWebFrame* frame)
{
    if (!frame)
        return E_FAIL;

    m_client->pageLoadTestClient().didStartProvisionalLoad(*frame);
    return S_OK;
}

HRESULT WinLauncherWebHost::didFailLoadWithError(IWebView*, IWebError*, IWebFrame*)
{
    m_client->pageLoadTestClient().didFailLoad();
    return S_OK;
}

HRESULT WinLauncherWebHost::didHandleOnloadEventsForFrame(IWebView* sender, IWebFrame* frame)
{
    IWebDataSourcePtr dataSource;
    HRESULT hr = frame->dataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        hr = frame->provisionalDataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        return 0;

    IWebMutableURLRequestPtr request;
    hr = dataSource->request(&request.GetInterfacePtr());
    if (FAILED(hr) || !request)
        return 0;

    _bstr_t frameURL;
    hr = request->mainDocumentURL(frameURL.GetAddress());
    if (FAILED(hr))
        return 0;

    if (frameURL.length())
        m_client->pageLoadTestClient().didHandleOnLoadEvents();

    return S_OK;
}

HRESULT WinLauncherWebHost::didFirstLayoutInFrame(IWebView*, IWebFrame* frame)
{
    BOOL mainFrame;
    if (SUCCEEDED(frame->isMainFrame(&mainFrame))) {
        if (mainFrame)
            m_client->pageLoadTestClient().didFirstLayoutForMainFrame();
    }

    return S_OK;
}
