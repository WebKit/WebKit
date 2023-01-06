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
#include "ProvisionalFrameProxy.h"

#include "APIWebsitePolicies.h"
#include "FrameInfoData.h"
#include "HandleMessage.h"
#include "LoadParameters.h"
#include "WebFrameProxy.h"
#include "WebFrameProxyMessages.h"
#include "WebPageMessages.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"

#include <WebCore/FrameIdentifier.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>

namespace WebKit {

ProvisionalFrameProxy::ProvisionalFrameProxy(WebFrameProxy& frame, Ref<WebProcessProxy>&& process, const WebCore::ResourceRequest& request)
    : m_frame(frame)
    , m_process(WTFMove(process))
    , m_visitedLinkStore(frame.page()->visitedLinkStore())
    , m_pageID(frame.page()->webPageID()) // FIXME: Generate a new one? This can conflict. And we probably want something like ProvisionalPageProxy to respond to messages anyways.
    , m_webPageID(frame.page()->identifier())
{
    m_process->markProcessAsRecentlyUsed();
    m_process->addProvisionalFrameProxy(*this);

    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID, *this);

    m_process->addMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frame.frameID().object(), *this);

    auto& page = *m_frame.page();
    auto parameters = page.creationParameters(m_process, *page.drawingArea());
    parameters.isProcessSwap = true; // FIXME: This should be a parameter to creationParameters rather than doctoring up the parameters afterwards.
    parameters.mainFrameIdentifier = frame.frameID();
    m_process->send(Messages::WebProcess::CreateWebPage(m_pageID, parameters), 0);
    m_process->addVisitedLinkStoreUser(page.visitedLinkStore(), page.identifier());

    LoadParameters loadParameters;
    loadParameters.request = request;
    loadParameters.shouldTreatAsContinuingLoad = WebCore::ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;
    // FIXME: Add more parameters as appropriate.

    // FIXME: Do we need a LoadRequestWaitingForProcessLaunch version?
    m_process->send(Messages::WebPage::LoadRequest(loadParameters), m_pageID);
}

ProvisionalFrameProxy::~ProvisionalFrameProxy()
{
    if (!m_wasCommitted) {
        m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID);
        m_process->removeMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frame.frameID().object());
        if (m_process->hasConnection())
            send(Messages::WebPage::Close(), m_pageID);
    }
    m_process->removeVisitedLinkStoreUser(m_visitedLinkStore.get(), m_webPageID);
    m_process->removeProvisionalFrameProxy(*this);
}

void ProvisionalFrameProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForResponse::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DecidePolicyForResponse>(connection, decoder, this, &ProvisionalFrameProxy::decidePolicyForResponse);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidCommitLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCommitLoadForFrame>(connection, decoder, this, &ProvisionalFrameProxy::didCommitLoadForFrame);
        return;
    }

    if (auto* page = m_frame.page())
        page->didReceiveMessage(connection, decoder);
}

void ProvisionalFrameProxy::decidePolicyForResponse(WebCore::FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::PolicyCheckIdentifier identifier, uint64_t navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, uint64_t listenerID)
{
    if (auto* page = m_frame.page())
        page->decidePolicyForResponseShared(m_process.copyRef(), m_pageID, frameID, WTFMove(frameInfo), identifier, navigationID, response, request, canShowMIMEType, downloadAttribute, listenerID);
}

void ProvisionalFrameProxy::didCommitLoadForFrame(WebCore::FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_pageID);
    m_process->removeMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frame.frameID().object());
    m_wasCommitted = true;

    m_frame.commitProvisionalFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
}

IPC::Connection* ProvisionalFrameProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t ProvisionalFrameProxy::messageSenderDestinationID() const
{
    // FIXME: This identifier was generated in another process and can collide with identifiers in this frame's process.
    return m_frame.frameID().object().toUInt64();
}

}
