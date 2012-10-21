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

#include "ewk_url_response_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

Ewk_Url_Response::Ewk_Url_Response(const WebCore::ResourceResponse& coreResponse)
    : m_coreResponse(coreResponse)
    , m_url(AdoptWK, WKURLResponseCopyURL(WebKit::toAPI(coreResponse)))
    , m_mimeType(AdoptWK, WKURLResponseCopyMIMEType(WebKit::toAPI(coreResponse)))
{ }

int Ewk_Url_Response::httpStatusCode() const
{
    return m_coreResponse.httpStatusCode();
}

const char* Ewk_Url_Response::url() const
{
    return m_url;
}

const char* Ewk_Url_Response::mimeType() const
{
    return m_mimeType;
}

unsigned long Ewk_Url_Response::contentLength() const
{
    return m_coreResponse.expectedContentLength();
}

Ewk_Url_Response* ewk_url_response_ref(Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);
    response->ref();

    return response;
}

void ewk_url_response_unref(Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN(response);

    response->deref();
}

const char* ewk_url_response_url_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->url();
}

int ewk_url_response_status_code_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->httpStatusCode();
}

const char* ewk_url_response_mime_type_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->mimeType();
}

unsigned long ewk_url_response_content_length_get(const Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(response, 0);

    return response->contentLength();
}
