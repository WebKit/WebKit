/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebDocumentLoader.h"
#include "WebFrame.h"
#include "WebMouseEvent.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/FrameLoader.h>
#include <WebCore/PolicyChecker.h>

#define WebFrameLoaderClient_PREFIX_PARAMETERS "%p - [webFrame=%p, webFrameID=%" PRIu64 ", webPage=%p, webPageID=%" PRIu64 "] WebFrameLoaderClient::"
#define WebFrameLoaderClient_WEBFRAME (&webFrame())
#define WebFrameLoaderClient_WEBFRAMEID (webFrame().frameID().object().toUInt64())
#define WebFrameLoaderClient_WEBPAGE (webFrame().page())
#define WebFrameLoaderClient_WEBPAGEID (WebFrameLoaderClient_WEBPAGE ? WebFrameLoaderClient_WEBPAGE->identifier().toUInt64() : 0)

#define WebFrameLoaderClient_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, WebFrameLoaderClient_PREFIX_PARAMETERS fmt, this, WebFrameLoaderClient_WEBFRAME, WebFrameLoaderClient_WEBFRAMEID, WebFrameLoaderClient_WEBPAGE, WebFrameLoaderClient_WEBPAGEID, ##__VA_ARGS__)
#define WebFrameLoaderClient_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, WebFrameLoaderClient_PREFIX_PARAMETERS fmt, this, WebFrameLoaderClient_WEBFRAME, WebFrameLoaderClient_WEBFRAMEID, WebFrameLoaderClient_WEBPAGE, WebFrameLoaderClient_WEBPAGEID, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WebFrameLoaderClient::WebFrameLoaderClient(Ref<WebFrame>&& frame)
    : m_frame(WTFMove(frame))
{
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& navigationAction, const ResourceRequest& request, const ResourceResponse& redirectResponse,
    FormState* formState, PolicyDecisionMode policyDecisionMode, WebCore::PolicyCheckIdentifier requestIdentifier, FramePolicyFunction&& function)
{
    auto* webPage = m_frame->page();
    if (!webPage) {
        WebFrameLoaderClient_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because there's no web page");
        function(PolicyAction::Ignore, requestIdentifier);
        return;
    }

    LOG(Loading, "WebProcess %i - dispatchDecidePolicyForNavigationAction to request url %s", getCurrentProcessID(), request.url().string().utf8().data());

    // Always ignore requests with empty URLs.
    if (request.isEmpty()) {
        WebFrameLoaderClient_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because request is empty");
        function(PolicyAction::Ignore, requestIdentifier);
        return;
    }

    uint64_t listenerID = m_frame->setUpPolicyListener(requestIdentifier, WTFMove(function), WebFrame::ForNavigationAction::Yes);

    ASSERT(navigationAction.requester());
    auto& requester = navigationAction.requester().value();

    auto* requestingFrame = requester.globalFrameIdentifier && requester.globalFrameIdentifier->frameID ? WebProcess::singleton().webFrame(requester.globalFrameIdentifier->frameID) : nullptr;
    std::optional<WebCore::FrameIdentifier> originatingFrameID;
    std::optional<WebCore::FrameIdentifier> parentFrameID;
    if (requestingFrame) {
        originatingFrameID = requestingFrame->frameID();
        if (auto* parentFrame = requestingFrame->parentFrame())
            parentFrameID = parentFrame->frameID();
    }

    FrameInfoData originatingFrameInfoData {
        navigationAction.initiatedByMainFrame() == InitiatedByMainFrame::Yes,
        FrameType::Local,
        ResourceRequest { requester.url },
        requester.securityOrigin->data(),
        { },
        WTFMove(originatingFrameID),
        WTFMove(parentFrameID),
        getCurrentProcessID()
    };

    std::optional<WebPageProxyIdentifier> originatingPageID;
    if (requester.globalFrameIdentifier && requester.globalFrameIdentifier->pageID) {
        if (auto* webPage = WebProcess::singleton().webPage(requester.globalFrameIdentifier->pageID))
            originatingPageID = webPage->webPageProxyIdentifier();
    }

    // FIXME: Move all this DocumentLoader stuff to the caller, pass in the results.
    RefPtr coreFrame = m_frame->coreLocalFrame();

    // FIXME: When we receive a redirect after the navigation policy has been decided for the initial request,
    // the provisional load's DocumentLoader needs to receive navigation policy decisions. We need a better model for this state.
    RefPtr<WebDocumentLoader> documentLoader;
    if (coreFrame)
        documentLoader = WebDocumentLoader::loaderForWebsitePolicies(*coreFrame);

    auto& mouseEventData = navigationAction.mouseEventData();
    NavigationActionData navigationActionData {
        navigationAction.type(),
        modifiersForNavigationAction(navigationAction),
        mouseButton(navigationAction),
        syntheticClickType(navigationAction),
        WebProcess::singleton().userGestureTokenIdentifier(navigationAction.userGestureToken()),
        navigationAction.userGestureToken() ? navigationAction.userGestureToken()->authorizationToken() : std::nullopt,
        webPage->canHandleRequest(request),
        navigationAction.shouldOpenExternalURLsPolicy(),
        navigationAction.downloadAttribute(),
        mouseEventData ? mouseEventData->locationInRootViewCoordinates : FloatPoint(),
        !redirectResponse.isNull(), /* isRedirect */
        navigationAction.treatAsSameOriginNavigation(),
        navigationAction.hasOpenedFrames(),
        navigationAction.openedByDOMWithOpener(),
        coreFrame && !!coreFrame->loader().opener(), /* hasOpener */
        requester.securityOrigin->data(),
        navigationAction.targetBackForwardItemIdentifier(),
        navigationAction.sourceBackForwardItemIdentifier(),
        navigationAction.lockHistory(),
        navigationAction.lockBackForwardList(),
        documentLoader ? documentLoader->clientRedirectSourceForHistory() : String(),
        coreFrame ? coreFrame->loader().effectiveSandboxFlags() : SandboxFlags(),
        navigationAction.privateClickMeasurement(),
        requestingFrame ? requestingFrame->advancedPrivacyProtections() : OptionSet<AdvancedPrivacyProtections> { },
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        WebHitTestResultData::fromNavigationActionAndLocalFrame(navigationAction, coreFrame.get()),
#endif
    };

    // Notify the UIProcess.
    if (policyDecisionMode == PolicyDecisionMode::Synchronous) {
        auto sendResult = webPage->sendSync(Messages::WebPageProxy::DecidePolicyForNavigationActionSync(m_frame->frameID(), m_frame->isMainFrame(), m_frame->info(), requestIdentifier, documentLoader ? documentLoader->navigationID() : 0, navigationActionData, originatingFrameInfoData, originatingPageID, navigationAction.resourceRequest(), request, IPC::FormDataReference { request.httpBody() }, redirectResponse));
        if (!sendResult.succeeded()) {
            WebFrameLoaderClient_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because of failing to send sync IPC with error %" PUBLIC_LOG_STRING, IPC::errorAsString(sendResult.error));
            m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { requestIdentifier });
            return;
        }

        auto [policyDecision] = sendResult.takeReply();
        WebFrameLoaderClient_RELEASE_LOG(Network, "dispatchDecidePolicyForNavigationAction: Got policyAction %u from sync IPC", (unsigned)policyDecision.policyAction);
        m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { policyDecision.identifier, policyDecision.isNavigatingToAppBoundDomain, policyDecision.policyAction, 0, policyDecision.downloadID });
        return;
    }

    ASSERT(policyDecisionMode == PolicyDecisionMode::Asynchronous);
    if (!webPage->send(Messages::WebPageProxy::DecidePolicyForNavigationActionAsync(m_frame->frameID(), m_frame->info(), requestIdentifier, documentLoader ? documentLoader->navigationID() : 0, navigationActionData, originatingFrameInfoData, originatingPageID, navigationAction.resourceRequest(), request, IPC::FormDataReference { request.httpBody() }, redirectResponse, listenerID))) {
        WebFrameLoaderClient_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because of failing to send async IPC");
        m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { requestIdentifier });
    }
}

}
