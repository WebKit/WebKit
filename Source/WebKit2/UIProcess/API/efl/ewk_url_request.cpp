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
#include "ewk_url_request.h"

#include "WKAPICast.h"
#include "WKRetainPtr.h"
#include "WKURL.h"
#include "WKURLRequest.h"
#include "WebURLRequest.h"
#include "WebURLRequestEfl.h"
#include "ewk_url_request_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * \struct  _Ewk_Url_Request
 * @brief   Contains the URL request data.
 */
struct _Ewk_Url_Request {
    unsigned int __ref; /**< the reference count of the object */
    WKRetainPtr<WKURLRequestRef> wkRequest;

    const char* url;
    const char* first_party;
    const char* http_method;
};

#define EWK_URL_REQUEST_WK_GET_OR_RETURN(request, wkRequest_, ...)    \
    if (!(request)) {                                                 \
        EINA_LOG_CRIT("request is NULL.");                            \
        return __VA_ARGS__;                                           \
    }                                                                 \
    if (!(request)->wkRequest) {                                      \
        EINA_LOG_CRIT("request->wkRequest is NULL.");                 \
        return __VA_ARGS__;                                           \
    }                                                                 \
    WKURLRequestRef wkRequest_ = (request)->wkRequest.get()

void ewk_url_request_ref(Ewk_Url_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN(request);
    ++request->__ref;
}

void ewk_url_request_unref(Ewk_Url_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN(request);

    if (--request->__ref)
        return;

    eina_stringshare_del(request->url);
    eina_stringshare_del(request->first_party);
    eina_stringshare_del(request->http_method);
    free(request);
}

const char* ewk_url_request_url_get(const Ewk_Url_Request* request)
{
    EWK_URL_REQUEST_WK_GET_OR_RETURN(request, wkRequest, 0);

    WKRetainPtr<WKURLRef> wkUrl(AdoptWK, WKURLRequestCopyURL(wkRequest));
    Ewk_Url_Request* ewkRequest = const_cast<Ewk_Url_Request*>(request);
    eina_stringshare_replace(&ewkRequest->url, toImpl(wkUrl.get())->string().utf8().data());

    return request->url;
}

const char* ewk_request_cookies_first_party_get(const Ewk_Url_Request* request)
{
    EWK_URL_REQUEST_WK_GET_OR_RETURN(request, wkRequest, 0);

    Ewk_Url_Request* ewkRequest = const_cast<Ewk_Url_Request*>(request);
    eina_stringshare_replace(&ewkRequest->first_party, toImpl(wkRequest)->resourceRequest().firstPartyForCookies().string().utf8().data());

    return request->first_party;
}

const char* ewk_url_request_http_method_get(const Ewk_Url_Request* request)
{
    EWK_URL_REQUEST_WK_GET_OR_RETURN(request, wkRequest, 0);

    Ewk_Url_Request* ewkRequest = const_cast<Ewk_Url_Request*>(request);
    eina_stringshare_replace(&ewkRequest->http_method, toImpl(wkRequest)->resourceRequest().httpMethod().utf8().data());

    return request->http_method;
}

/**
 * @internal
 * Constructs a Ewk_Url_Request from a WKURLRequest.
 */
Ewk_Url_Request* ewk_url_request_new(WKURLRequestRef wkUrlRequest)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(wkUrlRequest, 0);

    Ewk_Url_Request* ewkUrlRequest = static_cast<Ewk_Url_Request*>(calloc(1, sizeof(Ewk_Url_Request)));
    ewkUrlRequest->__ref = 1;
    ewkUrlRequest->wkRequest = wkUrlRequest;

    return ewkUrlRequest;
}
