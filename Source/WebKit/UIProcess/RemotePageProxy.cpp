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
#include "RemotePageProxy.h"

#include "APIWebsitePolicies.h"
#include "DrawingAreaProxy.h"
#include "FrameInfoData.h"
#include "HandleMessage.h"
#include "RemotePageDrawingAreaProxy.h"
#include "RemotePageVisitedLinkStoreRegistration.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

RemotePageProxy::RemotePageProxy(WebPageProxy& page, WebProcessProxy& process, const WebCore::RegistrableDomain& domain, WebPageProxyMessageReceiverRegistration* registrationToTransfer)
    : m_webPageID(page.webPageID())
    , m_process(process)
    , m_page(page)
    , m_domain(domain)
{
    if (registrationToTransfer)
        m_messageReceiverRegistration.transferMessageReceivingFrom(*registrationToTransfer, *this);
    else
        m_messageReceiverRegistration.startReceivingMessages(m_process, m_webPageID, *this);
    page.addRemotePageProxy(domain, *this);
}

void RemotePageProxy::injectPageIntoNewProcess()
{
    auto* page = m_page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* drawingArea = page->drawingArea();
    RELEASE_ASSERT(drawingArea);

    m_drawingArea = makeUnique<RemotePageDrawingAreaProxy>(*drawingArea, m_process);
    m_visitedLinkStoreRegistration = makeUnique<RemotePageVisitedLinkStoreRegistration>(*page, m_process);

    auto parameters = page->creationParameters(m_process, *drawingArea);
    parameters.subframeProcessFrameTreeCreationParameters = page->frameTreeCreationParameters();
    parameters.isProcessSwap = true; // FIXME: This should be a parameter to creationParameters rather than doctoring up the parameters afterwards.
    parameters.topContentInset = 0;
    m_process->send(Messages::WebProcess::CreateWebPage(m_webPageID, parameters), 0);
}

RemotePageProxy::~RemotePageProxy()
{
    if (m_page)
        m_page->removeRemotePageProxy(m_domain);
}

void RemotePageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    // FIXME: This needs to be handled correctly in a way that doesn't cause assertions or crashes..
    if (decoder.messageName() == Messages::WebPageProxy::DidCreateContextInWebProcessForVisibilityPropagation::name())
        return;
#endif

    // FIXME: Removing this will be necessary to getting layout tests to work with site isolation.
    if (decoder.messageName() == Messages::WebPageProxy::HandleMessage::name())
        return;

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForResponse::name()) {
        IPC::handleMessageAsync<Messages::WebPageProxy::DecidePolicyForResponse>(connection, decoder, this, &RemotePageProxy::decidePolicyForResponse);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidCommitLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCommitLoadForFrame>(connection, decoder, this, &RemotePageProxy::didCommitLoadForFrame);
        return;
    }

    if (m_page)
        m_page->didReceiveMessage(connection, decoder);
}

void RemotePageProxy::decidePolicyForResponse(FrameInfoData&& frameInfo, uint64_t navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!m_page)
        return completionHandler({ });
    m_page->decidePolicyForResponseShared(m_process.copyRef(), m_page->webPageID(), WTFMove(frameInfo), navigationID, response, request, canShowMIMEType, downloadAttribute, WTFMove(completionHandler));
}

void RemotePageProxy::didCommitLoadForFrame(WebCore::FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    m_process->didCommitProvisionalLoad();
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (frame)
        frame->commitProvisionalFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
}

bool RemotePageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (m_page)
        return m_page->didReceiveSyncMessage(connection, decoder, encoder);
    return false;
}

}
