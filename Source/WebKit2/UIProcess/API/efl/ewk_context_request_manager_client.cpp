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

#include "WKBase.h"
#include "WKSoupRequestManager.h"
#include "WKURL.h"
#include "ewk_context_private.h"
#include "ewk_context_request_manager_client_private.h"
#include "ewk_url_scheme_request.h"
#include "ewk_url_scheme_request_private.h"

static inline Ewk_Context* toEwkContext(const void* clientInfo)
{
    return static_cast<Ewk_Context*>(const_cast<void*>(clientInfo));
}

static void didReceiveURIRequest(WKSoupRequestManagerRef soupRequestManagerRef, WKURLRef urlRef, WKPageRef, uint64_t requestID, const void* clientInfo)
{
    RefPtr<Ewk_Url_Scheme_Request> schemeRequest = Ewk_Url_Scheme_Request::create(soupRequestManagerRef, urlRef, requestID);
    ewk_context_url_scheme_request_received(toEwkContext(clientInfo), schemeRequest.get());
}

void ewk_context_request_manager_client_attach(Ewk_Context* context)
{
    WKSoupRequestManagerClient wkRequestManagerClient;
    memset(&wkRequestManagerClient, 0, sizeof(WKSoupRequestManagerClient));

    wkRequestManagerClient.version = kWKSoupRequestManagerClientCurrentVersion;
    wkRequestManagerClient.clientInfo = context;
    wkRequestManagerClient.didReceiveURIRequest = didReceiveURIRequest;

    WKSoupRequestManagerSetClient(ewk_context_request_manager_get(context), &wkRequestManagerClient);
}
