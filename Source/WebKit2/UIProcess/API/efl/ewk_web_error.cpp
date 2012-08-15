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
#include "ewk_web_error.h"

#include "ErrorsEfl.h"
#include "WKEinaSharedString.h"
#include "WKString.h"
#include "WKURL.h"
#include "ewk_web_error_private.h"
#include <WKAPICast.h>
#include <WKError.h>
#include <WKRetainPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

struct _Ewk_Web_Error {
    WKRetainPtr<WKErrorRef> wkError;

    WKEinaSharedString url;
    WKEinaSharedString description;

    _Ewk_Web_Error(WKErrorRef errorRef)
        : wkError(errorRef)
        , url(AdoptWK, WKErrorCopyFailingURL(errorRef))
        , description(AdoptWK, WKErrorCopyLocalizedDescription(errorRef))
    { }

    ~_Ewk_Web_Error()
    {
    }
};

#define EWK_WEB_ERROR_WK_GET_OR_RETURN(error, wkError_, ...)    \
    if (!(error)) {                                           \
        EINA_LOG_CRIT("error is NULL.");                      \
        return __VA_ARGS__;                                    \
    }                                                          \
    if (!(error)->wkError) {                                 \
        EINA_LOG_CRIT("error->wkError is NULL.");            \
        return __VA_ARGS__;                                    \
    }                                                          \
    WKErrorRef wkError_ = (error)->wkError.get()

void ewk_web_error_free(Ewk_Web_Error* error)
{
    EINA_SAFETY_ON_NULL_RETURN(error);

    delete error;
}

Ewk_Web_Error_Type ewk_web_error_type_get(const Ewk_Web_Error* error)
{
    EWK_WEB_ERROR_WK_GET_OR_RETURN(error, wkError, EWK_WEB_ERROR_TYPE_NONE);

    WKRetainPtr<WKStringRef> wkDomain(AdoptWK, WKErrorCopyDomain(wkError));
    WTF::String errorDomain = toWTFString(wkDomain.get());

    if (errorDomain == errorDomainNetwork)
        return EWK_WEB_ERROR_TYPE_NETWORK;
    if (errorDomain == errorDomainPolicy)
        return EWK_WEB_ERROR_TYPE_POLICY;
    if (errorDomain == errorDomainPlugin)
        return EWK_WEB_ERROR_TYPE_PLUGIN;
    if (errorDomain == errorDomainDownload)
        return EWK_WEB_ERROR_TYPE_DOWNLOAD;
    if (errorDomain == errorDomainPrint)
        return EWK_WEB_ERROR_TYPE_PRINT;
    return EWK_WEB_ERROR_TYPE_INTERNAL;
}

const char* ewk_web_error_url_get(const Ewk_Web_Error* error)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(error, 0);

    return error->url;
}

int ewk_web_error_code_get(const Ewk_Web_Error* error)
{
    EWK_WEB_ERROR_WK_GET_OR_RETURN(error, wkError, 0);

    return WKErrorGetErrorCode(wkError);
}

const char* ewk_web_error_description_get(const Ewk_Web_Error* error)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(error, 0);

    return error->description;
}

Eina_Bool ewk_web_error_cancellation_get(const Ewk_Web_Error* error)
{
    EWK_WEB_ERROR_WK_GET_OR_RETURN(error, wkError, false);

    return toImpl(wkError)->platformError().isCancellation();
}

Ewk_Web_Error* ewk_web_error_new(WKErrorRef error)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(error, 0);

    return new Ewk_Web_Error(error);
}
