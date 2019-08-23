/*
 * Copyright (C) 2006, 2008, 2013-2015 Apple Inc.  All rights reserved.
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
#include "MiniBrowserWebHost.h"

#include "WebKitLegacyBrowserWindow.h"
#include <WebKitLegacy/WebKit.h>

typedef _com_ptr_t<_com_IIID<IWebDataSource, &__uuidof(IWebDataSource)>> IWebDataSourcePtr;

HRESULT MiniBrowserWebHost::didCommitLoadForFrame(_In_opt_ IWebView* webView, _In_opt_ IWebFrame* frame)
{
    return didChangeLocationWithinPageForFrame(webView, frame);
}

HRESULT MiniBrowserWebHost::didFailProvisionalLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError *error, _In_opt_ IWebFrame*)
{
    _bstr_t errorDescription;
    HRESULT hr = E_POINTER;
    if (error)
        hr = error->localizedDescription(errorDescription.GetAddress());
    if (FAILED(hr))
        errorDescription = L"Failed to load page and to localize error description.";

    if (_wcsicmp(errorDescription, L"Cancelled"))
        ::MessageBoxW(0, static_cast<LPCWSTR>(errorDescription), L"Error", MB_APPLMODAL | MB_OK);

    return S_OK;
}

HRESULT MiniBrowserWebHost::didChangeLocationWithinPageForFrame(_In_opt_ IWebView* webView, _In_opt_ IWebFrame* frame)
{
    IWebFrame2Ptr frame2(frame);
    if (!frame2)
        return E_NOINTERFACE;
    BOOL isMainFrame;
    HRESULT hr = frame2->isMainFrame(&isMainFrame);
    if (FAILED(hr))
        return hr;
    if (!isMainFrame)
        return S_OK;
    _bstr_t url;
    hr = webView->mainFrameURL(url.GetAddress());
    if (FAILED(hr))
        return hr;
    m_client->m_client.activeURLChanged(url.GetBSTR());
    return S_OK;
}

HRESULT MiniBrowserWebHost::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG MiniBrowserWebHost::AddRef()
{
    return m_client->AddRef();
}

ULONG MiniBrowserWebHost::Release()
{
    return m_client->Release();
}

HRESULT MiniBrowserWebHost::didFinishLoadForFrame(_In_opt_ IWebView* webView, _In_opt_ IWebFrame* frame)
{
    if (!frame || !webView)
        return E_POINTER;

    if (m_client)
        m_client->showLastVisitedSites(*webView);

    return S_OK;
}

HRESULT MiniBrowserWebHost::didStartProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{
    return S_OK;
}

HRESULT MiniBrowserWebHost::didFailLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*)
{
    return S_OK;
}

HRESULT MiniBrowserWebHost::didHandleOnloadEventsForFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame* frame)
{
    IWebDataSourcePtr dataSource;
    HRESULT hr = frame->dataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        hr = frame->provisionalDataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        return hr;

    IWebMutableURLRequestPtr request;
    hr = dataSource->request(&request.GetInterfacePtr());
    if (FAILED(hr) || !request)
        return hr;

    _bstr_t frameURL;
    hr = request->mainDocumentURL(frameURL.GetAddress());
    if (FAILED(hr))
        return hr;

    return S_OK;
}

HRESULT MiniBrowserWebHost::didFirstLayoutInFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{
    if (!frame)
        return E_POINTER;

    IWebFrame2Ptr frame2;
    if (FAILED(frame->QueryInterface(&frame2.GetInterfacePtr())))
        return S_OK;

    return S_OK;
}

HRESULT MiniBrowserWebHost::onNotify(_In_opt_ IWebNotification* notification)
{
    _bstr_t name;
    HRESULT hr = notification->name(name.GetAddress());
    if (FAILED(hr))
        return hr;
    if (name == _bstr_t(WebViewProgressEstimateChangedNotification)) {
        IUnknownPtr object;
        hr = notification->getObject(&object.GetInterfacePtr());
        if (FAILED(hr))
            return hr;
        IWebViewPtr webView(object);
        if (!webView)
            return E_NOINTERFACE;
        double progress;
        hr = webView->estimatedProgress(&progress);
        if (FAILED(hr))
            return hr;
        m_client->m_client.progressChanged(progress);
    } else if (name == _bstr_t(WebViewProgressFinishedNotification))
        m_client->m_client.progressFinished();
    
    return S_OK;
}
