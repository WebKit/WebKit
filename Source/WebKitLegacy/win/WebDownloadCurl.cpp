/*
 * Copyright (C) 2008 Brent Fulgham <bfulgham@gmail.com>. All Rights Reserved.
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

#include "WebKitDLL.h"
#include "WebDownload.h"

#include "DefaultDownloadDelegate.h"
#include "MarshallingHelpers.h"
#include "WebError.h"
#include "WebKit.h"
#include "WebKitLogging.h"
#include "WebMutableURLRequest.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLCredential.h"
#include "WebURLResponse.h"

#include <wtf/text/CString.h>

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <WebCore/BString.h>
#include <WebCore/CurlDownload.h>
#include <WebCore/FileSystem.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/TextEncoding.h>

using namespace WebCore;

// WebDownload ----------------------------------------------------------------

void WebDownload::init(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response, IWebDownloadDelegate* delegate)
{
    // Stop previous request
    if (handle)
        handle->setDefersLoading(true);

    m_request.adoptRef(WebMutableURLRequest::createInstance(request));

    m_delegate = delegate;

    m_download = adoptRef(new CurlDownload());
    m_download->init(*this, handle, request, response);
}

void WebDownload::init(const URL& url, IWebDownloadDelegate* delegate)
{
    m_delegate = delegate;

    m_download = adoptRef(new CurlDownload());
    m_download->init(*this, url);
}

// IWebDownload -------------------------------------------------------------------

HRESULT WebDownload::initWithRequest(
        /* [in] */ IWebURLRequest* request, 
        /* [in] */ IWebDownloadDelegate* delegate)
{
    if (!request)
        return E_POINTER;

    COMPtr<WebMutableURLRequest> webRequest;
    if (FAILED(request->QueryInterface(&webRequest)))
        return E_FAIL;

    BString url;

    if (!SUCCEEDED(request->URL(&url)))
        return E_FAIL;

    ResourceRequest resourceRequest;
    resourceRequest.setURL(URL(ParsedURLString, String(url)));

    const HTTPHeaderMap& headerMap = webRequest->httpHeaderFields();
    for (HTTPHeaderMap::const_iterator it = headerMap.begin(); it != headerMap.end(); ++it)
        resourceRequest.setHTTPHeaderField(it->key, it->value);

    init(nullptr, resourceRequest, ResourceResponse(), delegate);

    return S_OK;
}

HRESULT WebDownload::initToResumeWithBundle(
        /* [in] */ BSTR bundlePath, 
        /* [in] */ IWebDownloadDelegate* delegate)
{
   notImplemented();
   return E_FAIL;
}

HRESULT WebDownload::start()
{
    if (!m_download)
        return E_FAIL;

    m_download->start();

    if (m_delegate)
        m_delegate->didBegin(this);

    return S_OK;
}

HRESULT WebDownload::cancel()
{
    if (!m_download)
        return E_FAIL;

    if (!m_download->cancel())
        return E_FAIL;

    m_download->setListener(nullptr);
    m_download = nullptr;

    return S_OK;
}

HRESULT WebDownload::cancelForResume()
{
   notImplemented();
   return E_FAIL;
}

HRESULT WebDownload::deletesFileUponFailure(
        /* [out, retval] */ BOOL* result)
{
    if (!m_download)
        return E_FAIL;

    *result = m_download->deletesFileUponFailure() ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebDownload::setDeletesFileUponFailure(
        /* [in] */ BOOL deletesFileUponFailure)
{
    if (!m_download)
        return E_FAIL;

    m_download->setDeletesFileUponFailure(deletesFileUponFailure);
    return S_OK;
}

HRESULT WebDownload::setDestination(
        /* [in] */ BSTR path, 
        /* [in] */ BOOL allowOverwrite)
{
    if (!m_download)
        return E_FAIL;

    size_t len = wcslen(path);
    m_destination = String(path, len);
    m_download->setDestination(m_destination);
    return S_OK;
}

// IWebURLAuthenticationChallengeSender -------------------------------------------------------------------

HRESULT WebDownload::cancelAuthenticationChallenge(
        /* [in] */ IWebURLAuthenticationChallenge*)
{
   notImplemented();
   return E_FAIL;
}

HRESULT WebDownload::continueWithoutCredentialForAuthenticationChallenge(
        /* [in] */ IWebURLAuthenticationChallenge* challenge)
{
   notImplemented();
   return E_FAIL;
}

HRESULT WebDownload::useCredential(
        /* [in] */ IWebURLCredential* credential, 
        /* [in] */ IWebURLAuthenticationChallenge* challenge)
{
   notImplemented();
   return E_FAIL;
}

void WebDownload::didReceiveResponse(const ResourceResponse& response)
{
    COMPtr<WebDownload> protect = this;

    if (m_delegate) {
        COMPtr<WebURLResponse> webResponse(AdoptCOM, WebURLResponse::createInstance(response));
        m_delegate->didReceiveResponse(this, webResponse.get());

        String suggestedFilename = response.suggestedFilename();
        if (suggestedFilename.isEmpty())
            suggestedFilename = FileSystem::pathGetFileName(response.url().string());
        suggestedFilename = decodeURLEscapeSequences(suggestedFilename);
        BString suggestedFilenameBSTR(suggestedFilename);
        m_delegate->decideDestinationWithSuggestedFilename(this, suggestedFilenameBSTR);
    }
}

void WebDownload::didReceiveDataOfLength(int size)
{
    COMPtr<WebDownload> protect = this;

    if (m_delegate)
        m_delegate->didReceiveDataOfLength(this, size);
}

void WebDownload::didFinish()
{
    COMPtr<WebDownload> protect = this;

    if (m_delegate)
        m_delegate->didFinish(this);
}

void WebDownload::didFail()
{
    COMPtr<WebDownload> protect = this;

    if (m_delegate)
        m_delegate->didFailWithError(this, 0);
}
