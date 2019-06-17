/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RedirectSOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "DataReference.h"
#import <WebCore/ResourceResponse.h>

namespace WebKit {

Ref<SOAuthorizationSession> RedirectSOAuthorizationSession::create(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler)
{
    return adoptRef(*new RedirectSOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, WTFMove(completionHandler)));
}

RedirectSOAuthorizationSession::RedirectSOAuthorizationSession(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler)
    : NavigationSOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, InitiatingAction::Redirect, WTFMove(completionHandler))
{
}

void RedirectSOAuthorizationSession::fallBackToWebPathInternal()
{
    invokeCallback(false);
}

void RedirectSOAuthorizationSession::abortInternal()
{
    invokeCallback(true);
}

void RedirectSOAuthorizationSession::completeInternal(WebCore::ResourceResponse&& response, NSData *data)
{
    auto* pagePtr = page();
    if ((response.httpStatusCode() != 302 && response.httpStatusCode() != 200) || !pagePtr) {
        fallBackToWebPathInternal();
        return;
    }
    invokeCallback(true);
    if (response.httpStatusCode() == 302) {
#if PLATFORM(IOS)
        auto* navigationActionPtr = navigationAction();
        ASSERT(navigationActionPtr);
        // MobileSafari has a WBSURLSpoofingMitigator, which will not display the provisional URL for navigations without user gestures.
        // For slow loads that are initiated from the MobileSafari Favorites screen, the aforementioned behavior will create a period
        // after authentication completion where the new request to the application site loads with a blank URL and blank page. To
        // workaround this issue, we load an html page that does a client side redirection to the application site on behalf of the
        // request URL, instead of directly loading a new request. The html page should be super fast to load and therefore will not
        // show an empty URL or a blank page. These changes ensure a relevant URL bar and useful page content during the load.
        if (!navigationActionPtr->isProcessingUserGesture()) {
            pagePtr->setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision();
            auto html = makeString("<script>location = '", response.httpHeaderFields().get(WebCore::HTTPHeaderName::Location), "'</script>").utf8();
            auto data = IPC::DataReference(reinterpret_cast<const uint8_t*>(html.data()), html.length());
            pagePtr->loadData(data, "text/html"_s, "UTF-8"_s, navigationActionPtr->request().url());
            return;
        }
#endif
        pagePtr->loadRequest(WebCore::ResourceRequest(response.httpHeaderFields().get(WebCore::HTTPHeaderName::Location)));
    }
    if (response.httpStatusCode() == 200) {
        pagePtr->setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision();
        pagePtr->loadData(IPC::DataReference(static_cast<const uint8_t*>(data.bytes), data.length), "text/html"_s, "UTF-8"_s, response.url().string());
    }
}

void RedirectSOAuthorizationSession::beforeStart()
{
}

} // namespace WebKit

#endif
