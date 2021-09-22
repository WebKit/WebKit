/*
 * Copyright (C) 2013, 2017 Igalia S.L.
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
#include "WebKitInjectedBundleClient.h"

#include "APIInjectedBundleClient.h"
#include "WebImage.h"
#include "WebKitPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebResourcePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/glib/GUniquePtr.h>

using namespace WebKit;
using namespace WebCore;

class WebKitInjectedBundleClient final : public API::InjectedBundleClient {
public:
    explicit WebKitInjectedBundleClient(WebKitWebContext* webContext)
        : m_webContext(webContext)
    {
    }

private:
    static void didReceiveWebViewMessageFromInjectedBundle(WebKitWebView* webView, const char* messageName, API::Dictionary& message)
    {
        if (g_str_equal(messageName, "DidInitiateLoadForResource")) {
            WebFrameProxy* frame = static_cast<WebFrameProxy*>(message.get(String::fromUTF8("Frame")));
            if (!frame)
                return;

            API::UInt64* resourceIdentifier = static_cast<API::UInt64*>(message.get(String::fromUTF8("Identifier")));
            if (!resourceIdentifier)
                return;

            API::URLRequest* webRequest = static_cast<API::URLRequest*>(message.get(String::fromUTF8("Request")));
            if (!webRequest)
                return;

            GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(webRequest->resourceRequest()));
            webkitWebViewResourceLoadStarted(webView, *frame, resourceIdentifier->value(), request.get());
        } else if (g_str_equal(messageName, "DidSendRequestForResource")) {
            API::UInt64* resourceIdentifier = static_cast<API::UInt64*>(message.get(String::fromUTF8("Identifier")));
            if (!resourceIdentifier)
                return;

            GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(webView, resourceIdentifier->value());
            if (!resource)
                return;

            API::URLRequest* webRequest = static_cast<API::URLRequest*>(message.get(String::fromUTF8("Request")));
            if (!webRequest)
                return;

            GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(webRequest->resourceRequest()));
            API::URLResponse* webRedirectResponse = static_cast<API::URLResponse*>(message.get(String::fromUTF8("RedirectResponse")));
            GRefPtr<WebKitURIResponse> redirectResponse = webRedirectResponse ? adoptGRef(webkitURIResponseCreateForResourceResponse(webRedirectResponse->resourceResponse())) : nullptr;
            webkitWebResourceSentRequest(resource.get(), request.get(), redirectResponse.get());
        } else if (g_str_equal(messageName, "DidReceiveResponseForResource")) {
            API::UInt64* resourceIdentifier = static_cast<API::UInt64*>(message.get(String::fromUTF8("Identifier")));
            if (!resourceIdentifier)
                return;

            GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(webView, resourceIdentifier->value());
            if (!resource)
                return;

            API::URLResponse* webResponse = static_cast<API::URLResponse*>(message.get(String::fromUTF8("Response")));
            if (!webResponse)
                return;

            GRefPtr<WebKitURIResponse> response = adoptGRef(webkitURIResponseCreateForResourceResponse(webResponse->resourceResponse()));
            webkitWebResourceSetResponse(resource.get(), response.get());
        } else if (g_str_equal(messageName, "DidReceiveContentLengthForResource")) {
            API::UInt64* resourceIdentifier = static_cast<API::UInt64*>(message.get(String::fromUTF8("Identifier")));
            if (!resourceIdentifier)
                return;

            GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(webView, resourceIdentifier->value());
            if (!resource)
                return;

            API::UInt64* contentLength = static_cast<API::UInt64*>(message.get(String::fromUTF8("ContentLength")));
            if (!contentLength)
                return;

            webkitWebResourceNotifyProgress(resource.get(), contentLength->value());
        } else if (g_str_equal(messageName, "DidFinishLoadForResource")) {
            API::UInt64* resourceIdentifier = static_cast<API::UInt64*>(message.get(String::fromUTF8("Identifier")));
            if (!resourceIdentifier)
                return;

            GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(webView, resourceIdentifier->value());
            if (!resource)
                return;

            webkitWebResourceFinished(resource.get());
            webkitWebViewRemoveLoadingWebResource(webView, resourceIdentifier->value());
        } else if (g_str_equal(messageName, "DidFailLoadForResource")) {
            API::UInt64* resourceIdentifier = static_cast<API::UInt64*>(message.get(String::fromUTF8("Identifier")));
            if (!resourceIdentifier)
                return;

            GRefPtr<WebKitWebResource> resource = webkitWebViewGetLoadingWebResource(webView, resourceIdentifier->value());
            if (!resource)
                return;

            API::Error* webError = static_cast<API::Error*>(message.get(String::fromUTF8("Error")));
            if (!webError)
                return;

            const ResourceError& platformError = webError->platformError();
            GUniquePtr<GError> resourceError(g_error_new_literal(g_quark_from_string(platformError.domain().utf8().data()),
                toWebKitError(platformError.errorCode()), platformError.localizedDescription().utf8().data()));
            if (platformError.tlsErrors())
                webkitWebResourceFailedWithTLSErrors(resource.get(), static_cast<GTlsCertificateFlags>(platformError.tlsErrors()), platformError.certificate());
            else
                webkitWebResourceFailed(resource.get(), resourceError.get());

            webkitWebViewRemoveLoadingWebResource(webView, resourceIdentifier->value());
#if PLATFORM(GTK)
        } else if (g_str_equal(messageName, "DidGetSnapshot")) {
            API::UInt64* callbackID = static_cast<API::UInt64*>(message.get("CallbackID"));
            if (!callbackID)
                return;

            WebImage* image = static_cast<WebImage*>(message.get("Snapshot"));
            webKitWebViewDidReceiveSnapshot(webView, callbackID->value(), image);
#endif
        }
    }

    void didReceiveMessageFromInjectedBundle(WebProcessPool&, const String& messageName, API::Object* messageBody) override
    {
        if (messageBody->type() != API::Object::Type::Dictionary)
            return;

        API::Dictionary& message = *static_cast<API::Dictionary*>(messageBody);

        CString messageNameUTF8 = messageName.utf8();
        if (g_str_has_prefix(messageNameUTF8.data(), "WebPage.")) {
            WebPageProxy* page = static_cast<WebPageProxy*>(message.get(String::fromUTF8("Page")));
            WebKitWebView* webView = webkitWebContextGetWebViewForPage(m_webContext, page);
            if (!webView)
                return;

            didReceiveWebViewMessageFromInjectedBundle(webView, messageNameUTF8.data() + strlen("WebPage."), message);
        }
    }

    RefPtr<API::Object> getInjectedBundleInitializationUserData(WebProcessPool&) override
    {
        GRefPtr<GVariant> data = webkitWebContextInitializeWebExtensions(m_webContext);
        GUniquePtr<gchar> dataString(g_variant_print(data.get(), TRUE));
        return API::String::create(String::fromUTF8(dataString.get()));
    }

    WebKitWebContext* m_webContext;
};

void attachInjectedBundleClientToContext(WebKitWebContext* webContext)
{
    webkitWebContextGetProcessPool(webContext).setInjectedBundleClient(makeUnique<WebKitInjectedBundleClient>(webContext));
}
