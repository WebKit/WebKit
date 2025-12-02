/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "WebRemoteFrameClient.h"

#include "MessageSenderInlines.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "WebFrameProxyMessages.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/FocusControllerTypes.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameTree.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/NodeDocument.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/RemoteFrame.h>

namespace WebKit {
using namespace WebCore;

WebRemoteFrameClient::WebRemoteFrameClient(Ref<WebFrame>&& frame, ScopeExit<Function<void()>>&& frameInvalidator)
    : WebFrameLoaderClient(WTFMove(frame), WTFMove(frameInvalidator))
{
}

WebRemoteFrameClient::~WebRemoteFrameClient() = default;

void WebRemoteFrameClient::frameDetached()
{
    RefPtr coreFrame = m_frame->coreRemoteFrame();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr ownerElement = coreFrame->ownerElement();

    if (RefPtr parent = coreFrame->tree().parent()) {
        coreFrame->tree().detachFromParent();
        parent->tree().removeChild(*coreFrame);
    }
    m_frame->invalidate();

    if (ownerElement)
        ownerElement->protectedDocument()->checkCompleted();
}

void WebRemoteFrameClient::sizeDidChange(IntSize size)
{
    m_frame->updateRemoteFrameSize(size);
}

void WebRemoteFrameClient::paintContents(GraphicsContext& context, const IntRect& rect)
{
    RefPtr page = m_frame->page();
    if (!page)
        return;
    page->paintRemoteFrameContents(m_frame->frameID(), rect, context);
}

void WebRemoteFrameClient::postMessageToRemote(FrameIdentifier source, const String& sourceOrigin, FrameIdentifier target, std::optional<SecurityOriginData> targetOrigin, const MessageWithMessagePorts& message)
{
    if (RefPtr page = m_frame->page())
        page->send(Messages::WebPageProxy::PostMessageToRemote(source, sourceOrigin, target, targetOrigin, message));
}

void WebRemoteFrameClient::changeLocation(FrameLoadRequest&& request)
{
    NavigationAction action(request);
    // FIXME: action.request and request are probably duplicate information. <rdar://116203126>
    // FIXME: Get more parameters correct and add tests for each one. <rdar://116203354>
    dispatchDecidePolicyForNavigationAction(action, action.originalRequest(), ResourceResponse(), nullptr, { }, { }, { }, { }, IsPerformingHTTPFallback::No, { }, PolicyDecisionMode::Asynchronous, [protectedFrame = Ref { m_frame }, request = WTFMove(request)] (PolicyAction policyAction) mutable {
        // WebPage::loadRequest will make this load happen if needed.
        // FIXME: What if PolicyAction::Ignore is sent. Is everything in the right state? We probably need to make sure the load event still happens on the parent frame. <rdar://116203453>
    });
}

String WebRemoteFrameClient::renderTreeAsText(size_t baseIndent, OptionSet<RenderAsTextFlag> behavior)
{
    RefPtr page = m_frame->page();
    if (!page)
        return "Test Error - Missing page"_s;
    auto sendResult = page->sendSync(Messages::WebPageProxy::RenderTreeAsTextForTesting(m_frame->frameID(), baseIndent, behavior), IPC::Timeout::infinity(), IPC::SendSyncOption::MaintainOrderingWithAsyncMessages);
    if (!sendResult.succeeded())
        return "Test Error - sending WebPageProxy::RenderTreeAsTextForTesting failed"_s;
    auto [result] = sendResult.takeReply();
    return result;
}

String WebRemoteFrameClient::layerTreeAsText(size_t baseIndent, OptionSet<LayerTreeAsTextOptions> options)
{
    RefPtr page = m_frame->page();
    if (!page)
        return "Test Error - Missing page"_s;
    options.add(LayerTreeAsTextOptions::IncludeRootLayers);
    auto sendResult = page->sendSync(Messages::WebPageProxy::LayerTreeAsTextForTesting(m_frame->frameID(), baseIndent, options));
    if (!sendResult.succeeded())
        return "Test Error - sending WebPageProxy::LayerTreeAsTextForTesting failed"_s;
    auto [result] = sendResult.takeReply();
    return result;
}

void WebRemoteFrameClient::unbindRemoteAccessibilityFrames(int processIdentifier)
{
#if PLATFORM(COCOA)
    // Make sure AppKit system knows about our remote UI process status now.
    if (RefPtr page = m_frame->page())
        page->accessibilityManageRemoteElementStatus(false, processIdentifier);
#else
    UNUSED_PARAM(processIdentifier);
#endif
}

void WebRemoteFrameClient::updateRemoteFrameAccessibilityOffset(WebCore::FrameIdentifier frameID, WebCore::IntPoint offset)
{
    if (RefPtr page = m_frame->page())
        page->send(Messages::WebPageProxy::UpdateRemoteFrameAccessibilityOffset(frameID, offset));
}

void WebRemoteFrameClient::bindRemoteAccessibilityFrames(int processIdentifier, WebCore::FrameIdentifier frameID, Vector<uint8_t>&& dataToken, CompletionHandler<void(Vector<uint8_t>, int)>&& completionHandler)
{
    RefPtr page = m_frame->page();
    if (!page) {
        completionHandler({ }, 0);
        return;
    }

    auto sendResult = page->sendSync(Messages::WebPageProxy::BindRemoteAccessibilityFrames(processIdentifier, frameID, WTFMove(dataToken)));
    if (!sendResult.succeeded()) {
        completionHandler({ }, 0);
        return;
    }

    auto [resultToken, processIdentifierResult] = sendResult.takeReply();

#if PLATFORM(MAC)
    // Make sure AppKit system knows about our remote UI process status now.
    page->accessibilityManageRemoteElementStatus(true, processIdentifierResult);
#endif
    completionHandler(resultToken, processIdentifierResult);
}

void WebRemoteFrameClient::closePage()
{
    if (RefPtr page = m_frame->page())
        page->sendClose();
}

void WebRemoteFrameClient::focus()
{
    if (RefPtr page = m_frame->page())
        page->send(Messages::WebPageProxy::FocusRemoteFrame(m_frame->frameID()));
}

void WebRemoteFrameClient::unfocus()
{
    if (RefPtr page = m_frame->page())
        page->send(Messages::WebPageProxy::SetFocus(false));
}

void WebRemoteFrameClient::documentURLForConsoleLog(CompletionHandler<void(const URL&)>&& completionHandler)
{
    if (RefPtr page = m_frame->page())
        page->sendWithAsyncReply(Messages::WebPageProxy::DocumentURLForConsoleLog(m_frame->frameID()), WTFMove(completionHandler));
    else
        completionHandler({ });
}

void WebRemoteFrameClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& navigationAction, const ResourceRequest& request, const ResourceResponse& redirectResponse,
    FormState* formState, const String& clientRedirectSourceForHistory, std::optional<WebCore::NavigationIdentifier> navigationID, std::optional<HitTestResult>&& hitTestResult, bool hasOpener, IsPerformingHTTPFallback isPerformingHTTPFallback, SandboxFlags sandboxFlags, PolicyDecisionMode policyDecisionMode, FramePolicyFunction&& function)
{
    WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(navigationAction, request, redirectResponse, formState, clientRedirectSourceForHistory, navigationID, WTFMove(hitTestResult), hasOpener, isPerformingHTTPFallback, sandboxFlags, policyDecisionMode, WTFMove(function));
}

void WebRemoteFrameClient::updateSandboxFlags(WebCore::SandboxFlags sandboxFlags)
{
    WebFrameLoaderClient::updateSandboxFlags(sandboxFlags);
}

void WebRemoteFrameClient::updateOpener(const WebCore::Frame& newOpener)
{
    WebFrameLoaderClient::updateOpener(newOpener);
}

void WebRemoteFrameClient::setPrinting(bool printing, FloatSize pageSize, FloatSize originalPageSize, float maximumShrinkRatio, AdjustViewSize adjustViewSize)
{
    WebFrameLoaderClient::setPrinting(printing, pageSize, originalPageSize, maximumShrinkRatio, adjustViewSize);
}

void WebRemoteFrameClient::broadcastAllFrameTreeSyncDataToOtherProcesses(FrameTreeSyncData& data)
{
    WebFrameLoaderClient::broadcastAllFrameTreeSyncDataToOtherProcesses(data);
}

void WebRemoteFrameClient::broadcastFrameTreeSyncDataToOtherProcesses(const FrameTreeSyncSerializationData& data)
{
    WebFrameLoaderClient::broadcastFrameTreeSyncDataToOtherProcesses(data);
}

void WebRemoteFrameClient::applyWebsitePolicies(WebsitePoliciesData&& websitePolicies)
{
    RefPtr coreFrame = m_frame->coreRemoteFrame();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    coreFrame->setCustomUserAgent(WTFMove(websitePolicies.customUserAgent));
    coreFrame->setCustomUserAgentAsSiteSpecificQuirks(WTFMove(websitePolicies.customUserAgentAsSiteSpecificQuirks));
    coreFrame->setAdvancedPrivacyProtections(websitePolicies.advancedPrivacyProtections);
    coreFrame->setCustomNavigatorPlatform(WTFMove(websitePolicies.customNavigatorPlatform));
    coreFrame->setAutoplayPolicy(core(websitePolicies.autoplayPolicy));
}

void WebRemoteFrameClient::updateScrollingMode(ScrollbarMode scrollingMode)
{
    if (RefPtr page = m_frame->page())
        page->send(Messages::WebPageProxy::UpdateScrollingMode(m_frame->frameID(), scrollingMode));
}

void WebRemoteFrameClient::reportMixedContentViolation(bool blocked, const URL& target)
{
    if (RefPtr page = m_frame->page())
        page->send(Messages::WebPageProxy::ReportMixedContentViolation(m_frame->frameID(), blocked, target));
}

void WebRemoteFrameClient::findFocusableElementDescendingIntoRemoteFrame(WebCore::FocusDirection direction, const WebCore::FocusEventData& focusEventData, CompletionHandler<void(WebCore::FoundElementInRemoteFrame)>&& completionHandler)
{
    m_frame->sendWithAsyncReply(Messages::WebFrameProxy::FindFocusableElementDescendingIntoRemoteFrame(direction, focusEventData), WTFMove(completionHandler));
}

}
