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

#ifndef RequestManagerClientEfl_h
#define RequestManagerClientEfl_h

#include "WebProcessPool.h"
#include "WebSoupCustomProtocolRequestManager.h"
#include "ewk_context_private.h"
#include "ewk_url_scheme_request_private.h"

#include <WebKit/WKBase.h>
#include <WebKit/WKRetainPtr.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebKitURISchemeHandler: public RefCounted<WebKitURISchemeHandler> {
public:
    WebKitURISchemeHandler()
        : m_callback(nullptr)
        , m_userData(nullptr)
    {
    }

    WebKitURISchemeHandler(Ewk_Url_Scheme_Request_Cb callback, void* userData)
        : m_callback(callback)
        , m_userData(userData)
    {
    }

    bool hasCallback()
    {
        return m_callback;
    }

    void performCallback(EwkUrlSchemeRequest* request)
    {
        ASSERT(m_callback);

        m_callback(request, m_userData);
    }

private:
    Ewk_Url_Scheme_Request_Cb m_callback;
    void* m_userData;
};

typedef HashMap<String, RefPtr<WebKitURISchemeHandler> > URISchemeHandlerMap;
typedef HashMap<uint64_t, RefPtr<EwkUrlSchemeRequest> > URISchemeRequestMap;

class RequestManagerClientEfl {
public:
    explicit RequestManagerClientEfl(WKContextRef);

    void registerURLSchemeHandler(const String& scheme, Ewk_Url_Scheme_Request_Cb callback, void* userData);

private:
    static void startLoading(WKSoupCustomProtocolRequestManagerRef, uint64_t customProtocolID, WKURLRequestRef, const void* clientInfo);
    static void stopLoading(WKSoupCustomProtocolRequestManagerRef, uint64_t customProtocolID, const void* clientInfo);

    WKRetainPtr<WKSoupCustomProtocolRequestManagerRef> m_requestManager;
    URISchemeHandlerMap m_uriSchemeHandlers;
    URISchemeRequestMap m_uriSchemeRequests;
};

} // namespace WebKit

#endif // RequestManagerClientEfl_h
