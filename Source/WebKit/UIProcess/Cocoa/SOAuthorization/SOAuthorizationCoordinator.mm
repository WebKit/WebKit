/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#import "APIFrameHandle.h"
#import "APINavigationAction.h"
#import "APIPageConfiguration.h"
#import "PopUpSOAuthorizationSession.h"
#import "RedirectSOAuthorizationSession.h"
#import "SubFrameSOAuthorizationSession.h"
#import "WKSOAuthorizationDelegate.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <WebCore/ResourceRequest.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/AuthKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Function.h>
#import <wtf/ThreadAssertions.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cocoa/AppSSOSoftLink.h>

#define AUTHORIZATIONCOORDINATOR_RELEASE_LOG(fmt, ...) RELEASE_LOG(AppSSO, "%p - SOAuthorizationCoordinator::" fmt, this, ##__VA_ARGS__)
#define AUTHORIZATIONCOORDINATOR_RELEASE_LOG_STATIC(fmt, ...) RELEASE_LOG(AppSSO, "SOAuthorizationCoordinator::" fmt, ##__VA_ARGS__)
#define AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(AppSSO, "%p - SOAuthorizationCoordinator::" fmt, this, ##__VA_ARGS__)
#define AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR_STATIC(fmt, ...) RELEASE_LOG_ERROR(AppSSO, "SOAuthorizationCoordinator::" fmt, ##__VA_ARGS__)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SOAuthorizationCoordinator);

SOAuthorizationCoordinator::SOAuthorizationCoordinator()
{
    m_hasAppSSO = !!PAL::getSOAuthorizationClassSingleton();
#if PLATFORM(MAC)
    // In the case of base system, which doesn't have AppSSO.framework.
    if (!m_hasAppSSO)
        return;
#endif
    m_soAuthorizationDelegate = adoptNS([[WKSOAuthorizationDelegate alloc] init]);
    [NSURLSession _disableAppSSO];
}

void SOAuthorizationCoordinator::canAuthorize(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_hasAppSSO) {
        completionHandler(false);
        return;
    }
    if ([PAL::getSOAuthorizationClassSingleton() respondsToSelector:@selector(canPerformAuthorizationWithURL:responseCode:callerBundleIdentifier:useInternalExtensions:completion:)]) {
        [PAL::getSOAuthorizationClassSingleton() canPerformAuthorizationWithURL:url.createNSURL().get() responseCode:0 callerBundleIdentifier:nil useInternalExtensions:YES completion:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (BOOL result) mutable {
            ensureOnMainRunLoop([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        }).get()];
        return;
    }
    completionHandler([PAL::getSOAuthorizationClassSingleton() canPerformAuthorizationWithURL:url.createNSURL().get() responseCode:0]);
}

void SOAuthorizationCoordinator::tryAuthorize(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Function<void(bool)>&& completionHandler)
{
    AUTHORIZATIONCOORDINATOR_RELEASE_LOG("tryAuthorize");
    canAuthorize(navigationAction->request().url(), [completionHandler = WTFMove(completionHandler), navigationAction = WTFMove(navigationAction), page = Ref { page }, delegate = m_soAuthorizationDelegate] (bool result) mutable {
        if (!result) {
            AUTHORIZATIONCOORDINATOR_RELEASE_LOG_STATIC("tryAuthorize: The requested URL is not registered for AppSSO handling. No further action needed.");
            completionHandler(false);
            return;
        }

        // SubFrameSOAuthorizationSession should only be allowed for Apple first parties.
        RefPtr targetFrame = navigationAction->targetFrame();
        bool subframeNavigation = targetFrame && !targetFrame->isMainFrame();
        if (subframeNavigation && (!page->mainFrame() || ![AKAuthorizationController isURLFromAppleOwnedDomain:page->mainFrame()->url().createNSURL().get()])) {
            AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR_STATIC("tryAuthorize: Attempting to perform subframe navigation for non-Apple authorization URL.");
            completionHandler(false);
            return;
        }

        auto session = subframeNavigation ? SubFrameSOAuthorizationSession::create(delegate, WTFMove(navigationAction), page.get(), WTFMove(completionHandler), targetFrame->handle()->frameID()) : RedirectSOAuthorizationSession::create(delegate, WTFMove(navigationAction), page.get(), WTFMove(completionHandler));
        [delegate setSession:WTFMove(session)];
    });
}

void SOAuthorizationCoordinator::tryAuthorize(Ref<API::PageConfiguration>&& configuration, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
    AUTHORIZATIONCOORDINATOR_RELEASE_LOG("tryAuthorize (2)");
    bool subframeNavigation = navigationAction->sourceFrame() && !navigationAction->sourceFrame()->isMainFrame();
    if (subframeNavigation) {
        AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR("tryAuthorize (2): Attempting to perform subframe navigation.");
        uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
        return;
    }

    if (!navigationAction->isProcessingUserGesture()) {
        AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR("tryAuthorize (2): Attempting to perform auth without a user gesture.");
        uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
        return;
    }

    canAuthorize(navigationAction->request().url(), [uiClientCallback = WTFMove(uiClientCallback), navigationAction = WTFMove(navigationAction), configuration = WTFMove(configuration), page = Ref { page }, delegate = m_soAuthorizationDelegate, newPageCallback = WTFMove(newPageCallback)] (bool result) mutable {
        if (!result) {
            AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR_STATIC("tryAuthorize (2): Attempting to perform subframe navigation.");
            uiClientCallback(WTFMove(navigationAction), WTFMove(newPageCallback));
            return;
        }

        auto session = PopUpSOAuthorizationSession::create(WTFMove(configuration), delegate, page.get(), WTFMove(navigationAction), WTFMove(newPageCallback), WTFMove(uiClientCallback));
        [delegate setSession:WTFMove(session)];
    });
}

} // namespace WebKit

#undef AUTHORIZATIONCOORDINATOR_RELEASE_LOG_ERROR
#undef AUTHORIZATIONCOORDINATOR_RELEASE_LOG

#endif
