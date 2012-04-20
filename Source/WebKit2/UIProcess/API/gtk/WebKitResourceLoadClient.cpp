/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitResourceLoadClient.h"

#include "WebContext.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebResourcePrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebURLResponse.h"
#include <WebKit2/WKString.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

static void didInitiateLoadForResource(WKPageRef, WKFrameRef wkFrame, uint64_t resourceIdentifier, WKURLRequestRef wkRequest, bool pageIsProvisionallyLoading, const void* clientInfo)
{
    GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(toImpl(wkRequest)->resourceRequest()));
    webkitWebViewResourceLoadStarted(WEBKIT_WEB_VIEW(clientInfo), wkFrame, resourceIdentifier, request.get(), pageIsProvisionallyLoading);
}

static void didSendRequestForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLRequestRef wkRequest, WKURLResponseRef wkRedirectResponse, const void* clientInfo)
{
    GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(WEBKIT_WEB_VIEW(clientInfo), resourceIdentifier);
    if (!resource)
        return;

    GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(toImpl(wkRequest)->resourceRequest()));
    GRefPtr<WebKitURIResponse> redirectResponse = wkRedirectResponse ? adoptGRef(webkitURIResponseCreateForResourceResponse(toImpl(wkRedirectResponse)->resourceResponse())) : 0;
    webkitWebResourceSentRequest(resource.get(), request.get(), redirectResponse.get());
}

static void didReceiveResponseForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLResponseRef wkResponse, const void* clientInfo)
{
    GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(WEBKIT_WEB_VIEW(clientInfo), resourceIdentifier);
    if (!resource)
        return;

    GRefPtr<WebKitURIResponse> response = adoptGRef(webkitURIResponseCreateForResourceResponse(toImpl(wkResponse)->resourceResponse()));
    webkitWebResourceSetResponse(resource.get(), response.get());
}

static void didReceiveContentLengthForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, uint64_t contentLength, const void* clientInfo)
{
    GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(WEBKIT_WEB_VIEW(clientInfo), resourceIdentifier);
    if (!resource)
        return;

    webkitWebResourceNotifyProgress(resource.get(), contentLength);
}

static void didFinishLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, const void* clientInfo)
{
    GRefPtr<WebKitWebResource> resource = webkitWebViewResourceLoadFinished(WEBKIT_WEB_VIEW(clientInfo), resourceIdentifier);
    if (!resource)
        return;

    webkitWebResourceFinished(resource.get());
}

static void didFailLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKErrorRef wkError, const void* clientInfo)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(clientInfo);
    GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(webView, resourceIdentifier);
    if (!resource)
        return;

    const ResourceError& resourceError = toImpl(wkError)->platformError();
    GOwnPtr<GError> webError(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
                                                 resourceError.errorCode(),
                                                 resourceError.localizedDescription().utf8().data()));
    webkitWebResourceFailed(resource.get(), webError.get());
    webkitWebViewRemoveLoadingWebResource(webView, resourceIdentifier);
}

void attachResourceLoadClientToView(WebKitWebView* webView)
{
    WKPageResourceLoadClient wkResourceLoadClient = {
        kWKPageResourceLoadClientCurrentVersion,
        webView, // ClientInfo
        didInitiateLoadForResource,
        didSendRequestForResource,
        didReceiveResponseForResource,
        didReceiveContentLengthForResource,
        didFinishLoadForResource,
        didFailLoadForResource,
    };
    WKPageRef wkPage = toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView)));
    WKPageSetPageResourceLoadClient(wkPage, &wkResourceLoadClient);
}
