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
#include "WKRetainPtr.h"
#include "WKSoupRequestManager.h"
#include "WKString.h"
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * \struct  _Ewk_Url_Scheme_Request
 * @brief   Contains the URL scheme request data.
 */
struct _Ewk_Url_Scheme_Request {
    unsigned int __ref; /**< the reference count of the object */
    WKRetainPtr<WKSoupRequestManagerRef> wkRequestManager;
    const char* url;
    uint64_t requestID;
    const char* scheme;
    const char* path;

    _Ewk_Url_Scheme_Request(WKSoupRequestManagerRef manager, const char* _url, uint64_t _requestID)
        : __ref(1)
        , wkRequestManager(manager)
        , url(eina_stringshare_add(_url))
        , requestID(_requestID)
    {
        GOwnPtr<SoupURI> soupURI(soup_uri_new(_url));
        scheme = eina_stringshare_add(soupURI->scheme);
        path = eina_stringshare_add(soupURI->path);
    }

    ~_Ewk_Url_Scheme_Request()
    {
        ASSERT(!__ref);
        eina_stringshare_del(url);
        eina_stringshare_del(scheme);
        eina_stringshare_del(path);
    }
};

Ewk_Url_Scheme_Request* ewk_url_scheme_request_ref(Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);
    ++request->__ref;

    return request;
}

void ewk_url_scheme_request_unref(Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN(request);

    if (--request->__ref)
        return;

    delete request;
}

const char* ewk_url_scheme_request_scheme_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->scheme;
}

const char* ewk_url_scheme_request_url_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->url;
}

const char* ewk_url_scheme_request_path_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->path;
}

/**
 * @internal
 * Returns the #Ewk_Url_Scheme_Request identifier.
 */
uint64_t ewk_url_scheme_request_id_get(const Ewk_Url_Scheme_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->requestID;
}

Eina_Bool ewk_url_scheme_request_finish(const Ewk_Url_Scheme_Request* request, const void* contentData, unsigned int contentLength, const char* mimeType)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, false);

    WKRetainPtr<WKDataRef> wkData(AdoptWK, WKDataCreate(contentLength ? reinterpret_cast<const unsigned char*>(contentData) : 0, contentLength));
    WKRetainPtr<WKStringRef> wkMimeType = mimeType ? adoptWK(WKStringCreateWithUTF8CString(mimeType)) : 0;

    // In case of empty reply an empty WKDataRef is sent to the WebProcess.
    WKSoupRequestManagerDidHandleURIRequest(request->wkRequestManager.get(), wkData.get(), contentLength, wkMimeType.get(), request->requestID);

    return true;
}

/**
 * @internal
 * Constructs a Ewk_Url_Scheme_Request.
 */
Ewk_Url_Scheme_Request* ewk_url_scheme_request_new(WKSoupRequestManagerRef requestManager, WKURLRef url, uint64_t requestID)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(requestManager, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, 0);

    return new Ewk_Url_Scheme_Request(requestManager, toImpl(url)->string().utf8().data(), requestID);
}
