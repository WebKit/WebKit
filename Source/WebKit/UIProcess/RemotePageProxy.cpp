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
#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "HandleMessage.h"
#include "NativeWebMouseEvent.h"
#include "NavigationActionData.h"
#include "RemotePageDrawingAreaProxy.h"
#include "RemotePageVisitedLinkStoreRegistration.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/RemoteUserInputEventData.h>

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

    m_process->addRemotePageProxy(*this);

    page.addRemotePageProxy(domain, *this);
}

void RemotePageProxy::injectPageIntoNewProcess()
{
    RefPtr page = m_page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!page->mainFrame()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* drawingArea = page->drawingArea();
    RELEASE_ASSERT(drawingArea);

    m_drawingArea = makeUnique<RemotePageDrawingAreaProxy>(*drawingArea, m_process);
    m_visitedLinkStoreRegistration = makeUnique<RemotePageVisitedLinkStoreRegistration>(*page, m_process);

    auto parameters = page->creationParameters(m_process, *drawingArea);
    parameters.subframeProcessPageParameters = WebPageCreationParameters::SubframeProcessPageParameters {
        URL(page->currentURL()),
        page->mainFrame()->frameTreeCreationParameters()
    };
    parameters.isProcessSwap = true; // FIXME: This should be a parameter to creationParameters rather than doctoring up the parameters afterwards. <rdar://116201784>
    parameters.topContentInset = 0;
    m_process->send(Messages::WebProcess::CreateWebPage(m_webPageID, WTFMove(parameters)), 0);
}

void RemotePageProxy::processDidTerminate()
{
    for (auto& frame : m_frames)
        frame.remoteProcessDidTerminate();
}

void RemotePageProxy::addFrame(WebFrameProxy& frame)
{
    m_frames.add(frame);
}

void RemotePageProxy::removeFrame(WebFrameProxy& frame)
{
    m_frames.remove(frame);
}

RemotePageProxy::~RemotePageProxy()
{
    m_process->removeRemotePageProxy(*this);
    if (m_page)
        m_page->removeRemotePageProxy(m_domain);
}

void RemotePageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForResponse::name()) {
        IPC::handleMessageAsync<Messages::WebPageProxy::DecidePolicyForResponse>(connection, decoder, this, &RemotePageProxy::decidePolicyForResponse);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidCommitLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCommitLoadForFrame>(connection, decoder, this, &RemotePageProxy::didCommitLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionAsync::name()) {
        IPC::handleMessageAsync<Messages::WebPageProxy::DecidePolicyForNavigationActionAsync>(connection, decoder, this, &RemotePageProxy::decidePolicyForNavigationActionAsync);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidChangeProvisionalURLForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidChangeProvisionalURLForFrame>(connection, decoder, this, &RemotePageProxy::didChangeProvisionalURLForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidFailProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidFailProvisionalLoadForFrame>(connection, decoder, this, &RemotePageProxy::didFailProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::HandleMessage::name()) {
        IPC::handleMessage<Messages::WebPageProxy::HandleMessage>(connection, decoder, this, &RemotePageProxy::handleMessage);
        return;
    }

    if (m_page)
        m_page->didReceiveMessage(connection, decoder);
}

void RemotePageProxy::handleMessage(const String& messageName, const WebKit::UserData& messageBody)
{
    if (!m_page)
        return;
    m_page->handleMessageShared(m_process, messageName, messageBody);
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
    if (!frame)
        return;
    frame->commitProvisionalFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
}

void RemotePageProxy::decidePolicyForNavigationActionAsync(FrameInfoData&& frameInfo, uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!m_page)
        return completionHandler({ });
    m_page->decidePolicyForNavigationActionAsyncShared(m_process.copyRef(), m_webPageID, WTFMove(frameInfo), navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(completionHandler));
}

void RemotePageProxy::decidePolicyForNavigationActionSync(FrameInfoData&& frameInfo, uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!m_page)
        return completionHandler({ });
    m_page->decidePolicyForNavigationActionSyncShared(m_process.copyRef(), m_webPageID, WTFMove(frameInfo), navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(completionHandler));
}

void RemotePageProxy::didFailProvisionalLoadForFrame(FrameInfoData&& frameInfo, WebCore::ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError& error, WebCore::WillContinueLoading willContinueLoading, const UserData& userData, WebCore::WillInternallyHandleFailure willInternallyHandleFailure)
{
    if (!m_page)
        return;

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return;

    m_page->didFailProvisionalLoadForFrameShared(m_process.copyRef(), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, provisionalURL, error, willContinueLoading, userData, willInternallyHandleFailure);
}

void RemotePageProxy::didChangeProvisionalURLForFrame(WebCore::FrameIdentifier frameID, uint64_t navigationID, URL&& url)
{
    if (!m_page)
        return;
    m_page->didChangeProvisionalURLForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(url));
}

bool RemotePageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionSync::name())
        return IPC::handleMessageSynchronous<Messages::WebPageProxy::DecidePolicyForNavigationActionSync>(connection, decoder, encoder, this, &RemotePageProxy::decidePolicyForNavigationActionSync);

    if (m_page)
        return m_page->didReceiveSyncMessage(connection, decoder, encoder);

    return false;
}

Ref<WebProcessProxy> RemotePageProxy::protectedProcess() const
{
    return m_process;
}

RefPtr<WebPageProxy> RemotePageProxy::protectedPage() const
{
    return m_page.get();
}

WebPageProxy* RemotePageProxy::page() const
{
    return m_page.get();
}

}
