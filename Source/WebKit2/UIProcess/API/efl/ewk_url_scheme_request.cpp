/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ewk_url_scheme_request.h"

#include "GOwnPtrSoup.h"
#include "WKData.h"
#include "WKString.h"
#include "ewk_url_scheme_request_private.h"

using namespace WebKit;

Ewk_Url_Scheme_Request::Ewk_Url_Scheme_Request(WKSoupRequestManagerRef manager, WKURLRef url, uint64_t requestID)
    : m_wkRequestManager(manager)
    , m_url(url)
    , m_requestID(requestID)
{
    GOwnPtr<SoupURI> soupURI(soup_uri_new(m_url));
    m_scheme = soupURI->scheme;
    m_path = soupURI->path;
}

uint64_t Ewk_Url_Scheme_Request::id() const
{
    return m_requestID;
}

const char* Ewk_Url_Scheme_Request::url() const
{
    return m_url;
}

const char* Ewk_Url_Scheme_Request::scheme() const
{
    return m_scheme;
}

const char* Ewk_Url_Scheme_Request::path() const
{
    return m_path;
}

void Ewk_Url_Scheme_Request::finish(const void* contentData, uint64_t contentLength, const char* mimeType)
{
    WKRetainPtr<WKDataRef> wkData(AdoptWK, WKDataCreate(contentLength ? reinterpret_cast<const unsigned char*>(contentData) : 0, contentLength));
    WKRetainPtr<WKStringRef> wkMimeType = mimeType ? adoptWK(WKStringCreateWithUTF8CString(mimeType)) : 0;

    // In case of empty reply an empty WKDataRef is sent to the WebProcess.
    WKSoupRequestManagerDidHandleURIRequest(m_wkRequestManager.get(), wkData.get(), contentLength, wkMimeType.get(), m_requestID);
}

Ewk_Url_Scheme_Request* ewk_url_scheme_request_ref(Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);
    request->ref();

    return request;
}

void ewk_url_scheme_request_unref(Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN(request);

    request->deref();
}

const char* ewk_url_scheme_request_scheme_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->scheme();
}

const char* ewk_url_scheme_request_url_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->url();
}

const char* ewk_url_scheme_request_path_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->path();
}

Eina_Bool ewk_url_scheme_request_finish(Ewk_Url_Scheme_Request* request, const void* contentData, uint64_t contentLength, const char* mimeType)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, false);

    request->finish(contentData, contentLength, mimeType);

    return true;
}
