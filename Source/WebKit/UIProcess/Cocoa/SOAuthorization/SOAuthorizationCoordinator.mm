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
#import "SOAuthorizationCoordinator.h"

#if HAVE(APP_SSO)

#import "PopUpSOAuthorizationSession.h"
#import "RedirectSOAuthorizationSession.h"
#import "SubFrameSOAuthorizationSession.h"
#import "WKSOAuthorizationDelegate.h"
#import <WebCore/ResourceRequest.h>
#import <pal/cocoa/AppSSOSoftLink.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/AuthKitSPI.h>
#import <wtf/Function.h>

namespace WebKit {

SOAuthorizationCoordinator::SOAuthorizationCoordinator()
{
#if PLATFORM(MAC)
    // In the case of base system, which doesn't have AppSSO.framework.
    if (!PAL::getSOAuthorizationClass())
        return;
#endif
    m_soAuthorization = adoptNS([PAL::allocSOAuthorizationInstance() init]);
    m_soAuthorizationDelegate = adoptNS([[WKSOAuthorizationDelegate alloc] init]);
    m_soAuthorization.get().delegate = m_soAuthorizationDelegate.get();
    [NSURLSession _disableAppSSO];
}

bool SOAuthorizationCoordinator::canAuthorize(const URL& url) const
{
    return m_soAuthorization && [PAL::getSOAuthorizationClass() canPerformAuthorizationWithURL:url responseCode:0];
}

void SOAuthorizationCoordinator::tryAuthorize(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Function<void(bool)>&& completionHandler)
{
    if (!canAuthorize(navigationAction->request().url())) {
        completionHandler(false);
        return;
    }

    // SubFrameSOAuthorizationSession should only be allowed for Apple first parties.
    bool subframeNavigation = navigationAction->targetFrame() && !navigationAction->targetFrame()->isMainFrame();
    if (subframeNavigation && (!page.mainFrame() || ![AKAuthorizationController isURLFromAppleOwnedDomain:page.mainFrame()->url()])) {
        completionHandler(false);
        return;
    }

    auto session = subframeNavigation ? SubFrameSOAuthorizationSession::create(m_soAuthorization.get(), WTFMove(navigationAction), page, WTFMove(completionHandler)) : RedirectSOAuthorizationSession::create(m_soAuthorization.get(), WTFMove(navigationAction), page, WTFMove(completionHandler));
    [m_soAuthorizationDelegate setSession:WTFMove(session)];
}

void SOAuthorizationCoordinator::tryAuthorize(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
    bool subframeNavigation = navigationAction->sourceFrame() && !navigationAction->sourceFrame()->isMainFrame();
    if (subframeNavigation || !navigationAction->isProcessingUserGesture() || !canAuthorize(navigationAction->request().url())) {
        uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
        return;
    }

    auto session = PopUpSOAuthorizationSession::create(m_soAuthorization.get(), page, WTFMove(navigationAction), WTFMove(newPageCallback), WTFMove(uiClientCallback));
    [m_soAuthorizationDelegate setSession:WTFMove(session)];
}

} // namespace WebKit

#endif
