/*
 * Copyright (C) 2025 Sony Interactive Entertainment Inc.
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
#include "AutomationSessionClientWin.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "APINavigationAction.h"
#include "APIPageConfiguration.h"
#include "APIUIClient.h"
#include "APIWindowFeatures.h"
#include "BrowsingContextGroup.h"
#include "WebAutomationSession.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <wtf/RunLoop.h>

namespace WebKit {

AutomationSessionClient::AutomationSessionClient(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
    : m_sessionIdentifier(sessionIdentifier)
    , m_capabilities(capabilities)
{
}

void AutomationSessionClient::requestNewPageWithOptions(WebKit::WebAutomationSession& session, API::AutomationSessionBrowsingContextOptions options, CompletionHandler<void(WebKit::WebPageProxy*)>&& completionHandler)
{
    RefPtr<WebProcessPool> processPool = session.protectedProcessPool();
    if (processPool && processPool->processes().size()) {
        auto& processProxy = processPool->processes()[0];
        if (processProxy->pageCount()) {
            Ref firstPage = *processProxy->pages().begin();
            Ref configuration = firstPage->configuration().copy();

            WebCore::WindowFeatures windowFeatures;
            // FIXME: Attributes of the window of firstPage should be set to windowFeatures.
            //        That way, the application can use them in WKPageUIClient.createNewPage().
            //        https://webkit.org/b/290979
            configuration->setWindowFeatures(WTFMove(windowFeatures));
            configuration->setRelatedPage(firstPage);
            configuration->setControlledByAutomation(true);

            NavigationActionData navigationActionData {
                WebCore::NavigationType::Other, /* navigationType */
                { }, /* modifiers */
                WebMouseEventButton::None, /* mouseButton */
                WebMouseEventSyntheticClickType::NoTap, /* syntheticClickType */
                std::nullopt, /* userGestureTokenIdentifier */
                std::nullopt, /* userGestureAuthorizationToken */
                false, /* canHandleRequest */
                WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow, /* shouldOpenExternalURLsPolicy */
                { }, /* downloadAttribute */
                { }, /* clickLocationInRootViewCoordinates */
                { }, /* redirectResponse */
                false, /* isRequestFromClientOrUserInput */
                false, /* treatAsSameOriginNavigation */
                false, /* hasOpenedFrames */
                false, /* openedByDOMWithOpener */
                false, /* hasOpener */
                false, /* isPerformingHTTPFallback */
                false, /* isInitialFrameSrcLoad */
                false, /* isContentExtensionRedirect */
                { }, /* openedMainFrameName */
                std::nullopt, /* targetBackForwardItemIdentifier */
                std::nullopt, /* sourceBackForwardItemIdentifier */
                WebCore::LockHistory::No, /* lockHistory */
                WebCore::LockBackForwardList::No, /* lockBackForwardList */
                { }, /* clientRedirectSourceForHistory */
                { }, /* effectiveSandboxFlags */
                WebCore::ReferrerPolicy::EmptyString, /* effectiveReferrerPolicy */
                std::nullopt, /* ownerPermissionsPolicy */
                std::nullopt, /* privateClickMeasurement */
                { }, /* advancedPrivacyProtections */
                { }, /* originatorAdvancedPrivacyProtections */
                legacyEmptyFrameInfo(WebCore::ResourceRequest()), /* originatingFrameInfoData */
                std::nullopt, /* originatingPageID */
                legacyEmptyFrameInfo(WebCore::ResourceRequest()), /* frameInfo */
                std::nullopt, /* navigationID */
                { }, /* originalRequest */
                { }, /* request */
                { }, /* invalidURLString */
                std::nullopt, /* requester */
            };

            auto userInitiatedActivity = API::UserInitiatedAction::create();
            Ref navigationAction = API::NavigationAction::create(WTFMove(navigationActionData), nullptr, nullptr, String(), WebCore::ResourceRequest(), URL(), false, WTFMove(userInitiatedActivity));

            firstPage->uiClient().createNewPage(firstPage, WTFMove(configuration), WTFMove(navigationAction), [completionHandler = WTFMove(completionHandler)](auto&& newPage) mutable {
                if (newPage) {
                    newPage->setControlledByAutomation(true);
                    completionHandler(newPage.get());
                } else
                    completionHandler(nullptr);
            });
            return;
        }
    }
    completionHandler(nullptr);
}

static HWND getRootWindowHandle(WebKit::WebPageProxy& page)
{
    return ::GetAncestor(reinterpret_cast<HWND>(page.viewWidget()), GA_ROOT);
}

void AutomationSessionClient::requestMaximizeWindowOfPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    ::ShowWindow(getRootWindowHandle(page), SW_MAXIMIZE);
    completionHandler();
}

void AutomationSessionClient::requestHideWindowOfPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    ::ShowWindow(getRootWindowHandle(page), SW_MINIMIZE);
    completionHandler();
}

void AutomationSessionClient::requestRestoreWindowOfPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    ::ShowWindow(getRootWindowHandle(page), SW_RESTORE);
    completionHandler();
}

void AutomationSessionClient::didDisconnectFromRemote(WebKit::WebAutomationSession& session)
{
    session.setClient(nullptr);

    RunLoop::mainSingleton().dispatch([&session] {
        auto processPool = session.protectedProcessPool();
        if (processPool) {
            processPool->setAutomationSession(nullptr);
            processPool->setPagesControlledByAutomation(false);
        }
    });
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
