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
#include "ewk_url_response.h"

#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKURLResponse.h"
#include "ewk_url_response_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * \struct  _Ewk_Url_Response
 * @brief   Contains the URL response data.
 */
struct _Ewk_Url_Response {
    unsigned int __ref; /**< the reference count of the object */
    WebCore::ResourceResponse coreResponse;

    WKEinaSharedString url;
    WKEinaSharedString mimeType;

    _Ewk_Url_Response(const WebCore::ResourceResponse& _coreResponse)
        : __ref(1)
        , coreResponse(_coreResponse)
        , url(AdoptWK, WKURLResponseCopyURL(toAPI(coreResponse)))
        , mimeType(AdoptWK, WKURLResponseCopyMIMEType(toAPI(coreResponse)))
    { }

    ~_Ewk_Url_Response()
    {
        ASSERT(!__ref);
    }
};

void ewk_url_response_ref(Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN(response);
    ++response->__ref;
}

void ewk_url_response_unref(Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN(response);

    if (--response->__ref)
        return;

    delete response;
}

const char* ewk_url_response_url_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->url;
}

int ewk_url_response_status_code_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->coreResponse.httpStatusCode();
}

const char* ewk_url_response_mime_type_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->mimeType;
}

unsigned long ewk_url_response_content_length_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->coreResponse.expectedContentLength();
}

/**
 * @internal
 * Constructs a Ewk_Url_Response from a WebCore::ResourceResponse.
 */
Ewk_Url_Response* ewk_url_response_new(const WebCore::ResourceResponse& coreResponse)
{
    return new Ewk_Url_Response(coreResponse);
}
