/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameTree.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/RemoteFrame.h>

namespace WebKit {

WebRemoteFrameClient::WebRemoteFrameClient(Ref<WebFrame>&& frame, ScopeExit<Function<void()>>&& frameInvalidator)
    : WebFrameLoaderClient(WTFMove(frame))
    , m_frameInvalidator(WTFMove(frameInvalidator))
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

    if (RefPtr parent = coreFrame->tree().parent()) {
        coreFrame->tree().detachFromParent();
        parent->tree().removeChild(*coreFrame);
    }
    m_frame->invalidate();
}

void WebRemoteFrameClient::sizeDidChange(WebCore::IntSize size)
{
    m_frame->updateRemoteFrameSize(size);
}

void WebRemoteFrameClient::postMessageToRemote(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData> targetOrigin, const WebCore::MessageWithMessagePorts& message)
{
    if (auto* page = m_frame->page())
        page->send(Messages::WebPageProxy::PostMessageToRemote(source, sourceOrigin, target, targetOrigin, message));
}

void WebRemoteFrameClient::changeLocation(WebCore::FrameLoadRequest&& request)
{
    // FIXME: FrameLoadRequest and NavigationAction can probably be refactored to share more. <rdar://116202911>
    WebCore::NavigationAction action(request.requester(), request.resourceRequest(), request.initiatedByMainFrame(), request.isRequestFromClientOrUserInput());
    // FIXME: action.request and request are probably duplicate information. <rdar://116203126>
    // FIXME: PolicyCheckIdentifier should probably be pushed to another layer. <rdar://116203008>
    // FIXME: Get more parameters correct and add tests for each one. <rdar://116203354>
    dispatchDecidePolicyForNavigationAction(action, action.originalRequest(), WebCore::ResourceResponse(), nullptr, WebCore::PolicyDecisionMode::Asynchronous, WebCore::PolicyCheckIdentifier::generate(), [protectedFrame = Ref { m_frame }, request = WTFMove(request)] (WebCore::PolicyAction policyAction, WebCore::PolicyCheckIdentifier responseIdentifier) mutable {
        // FIXME: Check responseIdentifier. <rdar://116203008>
        // WebPage::loadRequest will make this load happen if needed.
        // FIXME: What if PolicyAction::Ignore is sent. Is everything in the right state? We probably need to make sure the load event still happens on the parent frame. <rdar://116203453>
    });
}

String WebRemoteFrameClient::renderTreeAsText(size_t baseIndent, OptionSet<WebCore::RenderAsTextFlag> behavior)
{
    RefPtr page = m_frame->page();
    if (!page)
        return "Test Error - Missing page"_s;
    auto sendResult = page->sendSync(Messages::WebPageProxy::RenderTreeAsText(m_frame->frameID(), baseIndent, behavior));
    if (!sendResult.succeeded())
        return "Test Error - sending WebPageProxy::RenderTreeAsText failed"_s;
    auto [result] = sendResult.takeReply();
    return result;
}

void WebRemoteFrameClient::broadcastFrameRemovalToOtherProcesses()
{
    WebFrameLoaderClient::broadcastFrameRemovalToOtherProcesses();
}

void WebRemoteFrameClient::close()
{
    // FIXME: <rdar://117381050> Consider if this needs the same logic as WebChromeClient::closeWindow, or refactor to share code.
    if (auto* page = m_frame->page())
        page->send(Messages::WebPageProxy::CloseRemoteFrame(m_frame->frameID()));
}

void WebRemoteFrameClient::focus()
{
    if (auto* page = m_frame->page())
        page->send(Messages::WebPageProxy::FocusRemoteFrame(m_frame->frameID()));
}

}
