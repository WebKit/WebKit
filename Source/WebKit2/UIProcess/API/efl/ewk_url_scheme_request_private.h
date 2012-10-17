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

#ifndef ewk_url_scheme_request_private_h
#define ewk_url_scheme_request_private_h

#include "GOwnPtrSoup.h"
#include "WKAPICast.h"
#include "WKBase.h"
#include "WKEinaSharedString.h"
#include "WKRetainPtr.h"
#include "WKSoupRequestManager.h"

/**
 * \struct  _Ewk_Url_Scheme_Request
 * @brief   Contains the URL scheme request data.
 */
struct Ewk_Url_Scheme_Request : public RefCounted<Ewk_Url_Scheme_Request> {
    WKRetainPtr<WKSoupRequestManagerRef> wkRequestManager;
    WKEinaSharedString url;
    uint64_t requestID;
    WKEinaSharedString scheme;
    WKEinaSharedString path;

    static PassRefPtr<Ewk_Url_Scheme_Request> create(WKSoupRequestManagerRef manager, WKURLRef url, uint64_t requestID)
    {
        if (!manager || !url)
            return 0;

        return adoptRef(new Ewk_Url_Scheme_Request(manager, url, requestID));
    }

private:
    Ewk_Url_Scheme_Request(WKSoupRequestManagerRef manager, WKURLRef urlRef, uint64_t requestID)
        : wkRequestManager(manager)
        , url(urlRef)
        , requestID(requestID)
    {
        GOwnPtr<SoupURI> soupURI(soup_uri_new(url));
        scheme = soupURI->scheme;
        path = soupURI->path;        
    }
};

#endif // ewk_url_scheme_request_private_h
