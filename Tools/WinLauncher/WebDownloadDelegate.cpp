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

#include "stdafx.h"
#include "WebDownloadDelegate.h"

#include <shlobj.h>

WebDownloadDelegate::WebDownloadDelegate()
    : m_refCount(1)
{
}

WebDownloadDelegate::~WebDownloadDelegate()
{
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;

    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDownloadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebDownloadDelegate))
        *ppvObject = static_cast<IWebDownloadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebDownloadDelegate::AddRef(void)
{
    m_refCount++;
    return m_refCount;
}

ULONG STDMETHODCALLTYPE WebDownloadDelegate::Release(void)
{
    m_refCount--;
    int refCount = m_refCount;
    if (!refCount)
        delete this;
    return refCount;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::decideDestinationWithSuggestedFilename(IWebDownload* download, BSTR filename)
{
    wchar_t desktopDirectory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_DESKTOP, 0, 0, desktopDirectory)))
        return E_FAIL;

    _bstr_t destination = _bstr_t(desktopDirectory) + L"\\" + ::PathFindFileNameW(filename);

    if (::PathFileExists(destination))
        return E_FAIL;

    download->setDestination(destination, TRUE);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didCancelAuthenticationChallenge(IWebDownload* download, IWebURLAuthenticationChallenge* challenge)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didCreateDestination(IWebDownload* download, BSTR destination)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didFailWithError(IWebDownload* download, IWebError* error)
{
    download->Release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didReceiveAuthenticationChallenge(IWebDownload* download, IWebURLAuthenticationChallenge* challenge)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didReceiveDataOfLength(IWebDownload* download, unsigned length)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didReceiveResponse(IWebDownload* download, IWebURLResponse* response)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::shouldDecodeSourceDataOfMIMEType(IWebDownload* download, BSTR encodingType, BOOL* shouldDecode)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::willResumeWithResponse(IWebDownload* download, IWebURLResponse* response, long long fromByte)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::willSendRequest(IWebDownload* download, IWebMutableURLRequest* request, IWebURLResponse* redirectResponse, IWebMutableURLRequest** finalRequest)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didBegin(IWebDownload* download)
{
    download->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownloadDelegate::didFinish(IWebDownload* download)
{
    download->Release();
    return S_OK;
}
