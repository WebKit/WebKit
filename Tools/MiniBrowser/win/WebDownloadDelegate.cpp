/*
* Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#pragma warning(disable: 4091)

#include "stdafx.h"
#include "WebDownloadDelegate.h"

#include "WebKitLegacyBrowserWindow.h"
#include <shlobj.h>

WebDownloadDelegate::WebDownloadDelegate(WebKitLegacyBrowserWindow& client)
    : m_client(client)
{
}

WebDownloadDelegate::~WebDownloadDelegate()
{
}

HRESULT WebDownloadDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;

    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDownloadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebDownloadDelegate))
        *ppvObject = static_cast<IWebDownloadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebDownloadDelegate::AddRef()
{
    return m_client.AddRef();
}

ULONG WebDownloadDelegate::Release()
{
    return m_client.Release();
}

HRESULT WebDownloadDelegate::decideDestinationWithSuggestedFilename(_In_opt_ IWebDownload* download, _In_ BSTR filename)
{
    if (!download)
        return E_POINTER;

    wchar_t desktopDirectory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_DESKTOP, 0, 0, desktopDirectory)))
        return E_FAIL;

    _bstr_t destination = _bstr_t(desktopDirectory) + L"\\" + ::PathFindFileNameW(filename);

    if (::PathFileExists(destination))
        return E_FAIL;

    download->setDestination(destination, TRUE);
    return S_OK;
}

HRESULT WebDownloadDelegate::didCancelAuthenticationChallenge(_In_opt_ IWebDownload* download, _In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::didCreateDestination(_In_opt_ IWebDownload* download, _In_ BSTR destination)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::didFailWithError(_In_opt_ IWebDownload* download, _In_opt_ IWebError* error)
{
    if (!download)
        return E_POINTER;

    download->Release();
    return S_OK;
}

HRESULT WebDownloadDelegate::didReceiveAuthenticationChallenge(_In_opt_ IWebDownload* download, _In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::didReceiveDataOfLength(_In_opt_ IWebDownload* download, unsigned length)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::didReceiveResponse(_In_opt_ IWebDownload* download, _In_opt_ IWebURLResponse* response)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::shouldDecodeSourceDataOfMIMEType(_In_opt_ IWebDownload* download, _In_ BSTR encodingType, _Out_ BOOL* shouldDecode)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::willResumeWithResponse(_In_opt_ IWebDownload* download, _In_opt_ IWebURLResponse* response, long long fromByte)
{
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::willSendRequest(_In_opt_ IWebDownload* download, _In_opt_ IWebMutableURLRequest* request,
    _In_opt_ IWebURLResponse* redirectResponse, _COM_Outptr_opt_ IWebMutableURLRequest** finalRequest)
{
    if (!finalRequest)
        return E_POINTER;
    *finalRequest = nullptr;
    return E_NOTIMPL;
}

HRESULT WebDownloadDelegate::didBegin(_In_opt_ IWebDownload* download)
{
    if (!download)
        return E_POINTER;

    download->AddRef();
    return S_OK;
}

HRESULT WebDownloadDelegate::didFinish(_In_opt_ IWebDownload* download)
{
    if (!download)
        return E_POINTER;

    download->Release();
    return S_OK;
}
