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

#include "WKAPICast.h"
#include "WKFrame.h"
#include "WKPage.h"
#include "WKRetainPtr.h"
#include "WKURL.h"
#include "WKURLRequest.h"
#include "WKURLResponse.h"
#include "ewk_error_private.h"
#include "ewk_resource.h"
#include "ewk_resource_private.h"
#include "ewk_url_request.h"
#include "ewk_url_request_private.h"
#include "ewk_url_response.h"
#include "ewk_url_response_private.h"
#include "ewk_view_private.h"
#include "ewk_view_resource_load_client_private.h"
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

static inline Evas_Object* toEwkView(const void* clientInfo)
{
    return static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
}

static void didInitiateLoadForResource(WKPageRef, WKFrameRef wkFrame, uint64_t resourceIdentifier, WKURLRequestRef wkRequest, bool pageIsProvisionallyLoading, const void* clientInfo)
{
    bool isMainResource = (WKFrameIsMainFrame(wkFrame) && pageIsProvisionallyLoading);
    WKRetainPtr<WKURLRef> wkUrl(AdoptWK, WKURLRequestCopyURL(wkRequest));

    Ewk_Resource* resource = ewk_resource_new(toImpl(wkUrl.get())->string().utf8().data(), isMainResource);
    Ewk_Url_Request* request = ewk_url_request_new(wkRequest);
    ewk_view_resource_load_initiated(toEwkView(clientInfo), resourceIdentifier, resource, request);
    ewk_resource_unref(resource);
    ewk_url_request_unref(request);
}

static void didSendRequestForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLRequestRef wkRequest, WKURLResponseRef wkRedirectResponse, const void* clientInfo)
{
    Ewk_Url_Request* request = ewk_url_request_new(wkRequest);
    Ewk_Url_Response* redirectResponse = wkRedirectResponse ? ewk_url_response_new(toImpl(wkRedirectResponse)->resourceResponse()) : 0;
    ewk_view_resource_request_sent(toEwkView(clientInfo), resourceIdentifier, request, redirectResponse);
    ewk_url_request_unref(request);
    if (redirectResponse)
        ewk_url_response_unref(redirectResponse);
}

static void didReceiveResponseForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLResponseRef wkResponse, const void* clientInfo)
{
    Ewk_Url_Response* response = ewk_url_response_new(toImpl(wkResponse)->resourceResponse());
    ewk_view_resource_load_response(toEwkView(clientInfo), resourceIdentifier, response);
    ewk_url_response_unref(response);
}

static void didFinishLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, const void* clientInfo)
{
    ewk_view_resource_load_finished(toEwkView(clientInfo), resourceIdentifier);
}

static void didFailLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKErrorRef wkError, const void* clientInfo)
{
    Ewk_Error* ewkError = ewk_error_new(wkError);
    ewk_view_resource_load_failed(toEwkView(clientInfo), resourceIdentifier, ewkError);
    ewk_view_resource_load_finished(toEwkView(clientInfo), resourceIdentifier);
    ewk_error_free(ewkError);
}

void ewk_view_resource_load_client_attach(WKPageRef pageRef, Evas_Object* ewkView)
{
    WKPageResourceLoadClient wkResourceLoadClient;
    memset(&wkResourceLoadClient, 0, sizeof(WKPageResourceLoadClient));
    wkResourceLoadClient.version = kWKPageResourceLoadClientCurrentVersion;
    wkResourceLoadClient.clientInfo = ewkView;
    wkResourceLoadClient.didInitiateLoadForResource = didInitiateLoadForResource;
    wkResourceLoadClient.didSendRequestForResource = didSendRequestForResource;
    wkResourceLoadClient.didReceiveResponseForResource = didReceiveResponseForResource;
    wkResourceLoadClient.didFinishLoadForResource = didFinishLoadForResource;
    wkResourceLoadClient.didFailLoadForResource = didFailLoadForResource;

    WKPageSetPageResourceLoadClient(pageRef, &wkResourceLoadClient);
}
