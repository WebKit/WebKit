/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#include "WebFrameLoaderClient.h"

#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NavigationActionData.h"
#include "WebFrame.h"
#include "WebMouseEvent.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/FrameLoader.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/PolicyChecker.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#define WebFrameLoaderClient_PREFIX_PARAMETERS "%p - [webFrame=%p, webFrameID=%" PRIu64 ", webPage=%p, webPageID=%" PRIu64 "] WebFrameLoaderClient::"
#define WebFrameLoaderClient_WEBFRAME (&webFrame())
#define WebFrameLoaderClient_WEBFRAMEID (webFrame().frameID().object().toUInt64())
#define WebFrameLoaderClient_WEBPAGE (webFrame().page())
#define WebFrameLoaderClient_WEBPAGEID (WebFrameLoaderClient_WEBPAGE ? WebFrameLoaderClient_WEBPAGE->identifier().toUInt64() : 0)

#define WebFrameLoaderClient_RELEASE_LOG(fmt, ...) RELEASE_LOG_FORWARDABLE(Network, fmt, WebFrameLoaderClient_WEBFRAMEID, WebFrameLoaderClient_WEBPAGEID, ##__VA_ARGS__)
#define WebFrameLoaderClient_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR_FORWARDABLE(Network, fmt, WebFrameLoaderClient_WEBFRAMEID, WebFrameLoaderClient_WEBPAGEID, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WebFrameLoaderClient::WebFrameLoaderClient(Ref<WebFrame>&& frame, ScopeExit<Function<void()>>&& frameInvalidator)
    : m_frame(WTFMove(frame))
    , m_frameInvalidator(WTFMove(frameInvalidator))
{
}

WebFrameLoaderClient::~WebFrameLoaderClient() = default;

std::optional<NavigationActionData> WebFrameLoaderClient::navigationActionData(const NavigationAction& navigationAction, const ResourceRequest& request, const ResourceResponse& redirectResponse, const String& clientRedirectSourceForHistory, std::optional<WebCore::NavigationIdentifier> navigationID, std::optional<WebCore::HitTestResult>&& hitTestResult, bool hasOpener, IsPerformingHTTPFallback isPerformingHTTPFallback, SandboxFlags sandboxFlags) const
{
    RefPtr webPage = m_frame->page();
    if (!webPage) {
        WebFrameLoaderClient_RELEASE_LOG_ERROR(WEBFRAMELOADERCLIENT_NAVIGATIONACTIONDATA_NO_WEBPAGE);
        return std::nullopt;
    }

    // Always ignore requests with empty URLs.
    if (request.isEmpty()) {
        WebFrameLoaderClient_RELEASE_LOG_ERROR(WEBFRAMELOADERCLIENT_NAVIGATIONACTIONDATA_EMPTY_REQUEST);
        return std::nullopt;
    }

    if (!navigationAction.requester()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    auto& requester = *navigationAction.requester();
    if (!requester.frameID) {
        WebFrameLoaderClient_RELEASE_LOG_ERROR(WEBFRAMELOADERCLIENT_NAVIGATIONACTIONDATA_NO_FRAME);
        return std::nullopt;
    }

    RefPtr requestingFrame = WebProcess::singleton().webFrame(*requester.frameID);
    auto originatingFrameID = *requester.frameID;
    std::optional<WebCore::FrameIdentifier> parentFrameID;
    if (auto parentFrame = requestingFrame ? requestingFrame->parentFrame() : nullptr)
        parentFrameID = parentFrame->frameID();

    RefPtr coreLocalFrame = m_frame->coreLocalFrame();
    RefPtr document = coreLocalFrame ? coreLocalFrame->document() : nullptr;

    FrameInfoData originatingFrameInfoData {
        navigationAction.initiatedByMainFrame() == InitiatedByMainFrame::Yes,
        FrameType::Local,
        ResourceRequest { requester.url },
        requester.securityOrigin->data(),
        { },
        WTFMove(originatingFrameID),
        WTFMove(parentFrameID),
        document ? std::optional { document->identifier() } : std::nullopt,
        getCurrentProcessID(),
        requestingFrame ? requestingFrame->isFocused() : false
    };

    std::optional<WebPageProxyIdentifier> originatingPageID;
    if (auto* webPage = requester.pageID ? WebProcess::singleton().webPage(*requester.pageID) : nullptr)
        originatingPageID = webPage->webPageProxyIdentifier();

    // FIXME: When we receive a redirect after the navigation policy has been decided for the initial request,
    // the provisional load's DocumentLoader needs to receive navigation policy decisions. We need a better model for this state.

    std::optional<WebCore::OwnerPermissionsPolicyData> ownerPermissionsPolicy;
    if (RefPtr coreFrame = m_frame->coreFrame())
        ownerPermissionsPolicy = coreFrame->ownerPermissionsPolicy();

    auto& mouseEventData = navigationAction.mouseEventData();
    return NavigationActionData {
        navigationAction.type(),
        modifiersForNavigationAction(navigationAction),
        mouseButton(navigationAction),
        syntheticClickType(navigationAction),
        WebProcess::singleton().userGestureTokenIdentifier(navigationAction.requester()->pageID, navigationAction.userGestureToken()),
        navigationAction.userGestureToken() ? navigationAction.userGestureToken()->authorizationToken() : std::nullopt,
        webPage->canHandleRequest(request),
        navigationAction.shouldOpenExternalURLsPolicy(),
        navigationAction.downloadAttribute(),
        mouseEventData ? mouseEventData->locationInRootViewCoordinates : FloatPoint(),
        redirectResponse,
        navigationAction.isRequestFromClientOrUserInput(),
        navigationAction.treatAsSameOriginNavigation(),
        navigationAction.hasOpenedFrames(),
        navigationAction.openedByDOMWithOpener(),
        hasOpener,
        isPerformingHTTPFallback == IsPerformingHTTPFallback::Yes,
        { },
        requester.securityOrigin->data(),
        requester.topOrigin->data(),
        navigationAction.targetBackForwardItemIdentifier(),
        navigationAction.sourceBackForwardItemIdentifier(),
        navigationAction.lockHistory(),
        navigationAction.lockBackForwardList(),
        clientRedirectSourceForHistory,
        sandboxFlags,
        WTFMove(ownerPermissionsPolicy),
        navigationAction.privateClickMeasurement(),
        requestingFrame ? requestingFrame->advancedPrivacyProtections() : OptionSet<AdvancedPrivacyProtections> { },
        requestingFrame ? requestingFrame->originatorAdvancedPrivacyProtections() : OptionSet<AdvancedPrivacyProtections> { },
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        hitTestResult ? std::optional(WebKit::WebHitTestResultData(WTFMove(*hitTestResult), false)) : std::nullopt,
#endif
        WTFMove(originatingFrameInfoData),
        originatingPageID,
        m_frame->info(),
        navigationID,
        navigationAction.originalRequest(),
        request
    };
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& navigationAction, const ResourceRequest& request, const ResourceResponse& redirectResponse, FormState*, const String& clientRedirectSourceForHistory, std::optional<WebCore::NavigationIdentifier> navigationID, std::optional<WebCore::HitTestResult>&& hitTestResult, bool hasOpener, IsPerformingHTTPFallback isPerformingHTTPFallback, SandboxFlags sandboxFlags, PolicyDecisionMode policyDecisionMode, FramePolicyFunction&& function)
{
    LOG(Loading, "WebProcess %i - dispatchDecidePolicyForNavigationAction to request url %s", getCurrentProcessID(), request.url().string().utf8().data());

    auto navigationActionData = this->navigationActionData(navigationAction, request, redirectResponse, clientRedirectSourceForHistory, navigationID, WTFMove(hitTestResult), hasOpener, isPerformingHTTPFallback, sandboxFlags);
    if (!navigationActionData)
        return function(PolicyAction::Ignore);

    RefPtr webPage = m_frame->page();

    uint64_t listenerID = m_frame->setUpPolicyListener(WTFMove(function), WebFrame::ForNavigationAction::Yes);

    // Notify the UIProcess.
    if (policyDecisionMode == PolicyDecisionMode::Synchronous) {
        bool shouldUseSyncIPCForFragmentNavigations = false;
#if PLATFORM(COCOA)
        shouldUseSyncIPCForFragmentNavigations = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::AsyncFragmentNavigationPolicyDecision);
#endif
        if (navigationAction.processingUserGesture() || navigationAction.isFromNavigationAPI() || shouldUseSyncIPCForFragmentNavigations) {
            auto sendResult = webPage->sendSync(Messages::WebPageProxy::DecidePolicyForNavigationActionSync(*navigationActionData));
            if (!sendResult.succeeded()) {
                WebFrameLoaderClient_RELEASE_LOG_ERROR(WEBFRAMELOADERCLIENT_DISPATCHDECIDEPOLICYFORNAVIGATIONACTION_SYNC_IPC_FAILED, (uint8_t)sendResult.error());
                m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { });
                return;
            }

            auto [policyDecision] = sendResult.takeReply();
            WebFrameLoaderClient_RELEASE_LOG(WEBFRAMELOADERCLIENT_DISPATCHDECIDEPOLICYFORNAVIGATIONACTION_GOT_POLICYACTION_FROM_SYNC_IPC, (unsigned)policyDecision.policyAction);
            m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { policyDecision.isNavigatingToAppBoundDomain, policyDecision.policyAction, { }, policyDecision.downloadID });
            return;
        }
        webPage->sendWithAsyncReply(Messages::WebPageProxy::DecidePolicyForNavigationActionAsync(*navigationActionData), [] (PolicyDecision&&) { });
        m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { std::nullopt, PolicyAction::Use });
        return;
    }

    ASSERT(policyDecisionMode == PolicyDecisionMode::Asynchronous);
    webPage->sendWithAsyncReply(Messages::WebPageProxy::DecidePolicyForNavigationActionAsync(*navigationActionData), [weakFrame = WeakPtr { m_frame }, listenerID, webPageID = WebFrameLoaderClient_WEBPAGEID] (PolicyDecision&& policyDecision) {
        RefPtr frame = weakFrame.get();
        if (!frame)
            return;

        RELEASE_LOG_ERROR_FORWARDABLE(Network, WEBFRAMELOADERCLIENT_DISPATCHDECIDEPOLICYFORNAVIGATIONACTION_GOT_POLICYACTION_FROM_ASYNC_IPC, frame->frameID().object().toUInt64(), webPageID, (unsigned)policyDecision.policyAction);

        frame->didReceivePolicyDecision(listenerID, WTFMove(policyDecision));
    });
}

void WebFrameLoaderClient::updateSandboxFlags(SandboxFlags sandboxFlags)
{
    if (RefPtr webPage = m_frame->page())
        webPage->send(Messages::WebPageProxy::UpdateSandboxFlags(m_frame->frameID(), sandboxFlags));
}

void WebFrameLoaderClient::updateOpener(const WebCore::Frame& newOpener)
{
    if (RefPtr webPage = m_frame->page())
        webPage->send(Messages::WebPageProxy::UpdateOpener(m_frame->frameID(), newOpener.frameID()));
}

}
