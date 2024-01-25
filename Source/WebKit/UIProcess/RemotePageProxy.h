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

#pragma once

#include "MessageReceiver.h"
#include "NavigationActionData.h"
#include "WebPageProxyMessageReceiverRegistration.h"
#include "WebProcessProxy.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/WeakHashSet.h>

namespace IPC {
class Connection;
class Decoder;
class Encoder;
template<typename> struct ConnectionSendSyncResult;
}

namespace WebCore {
enum class FrameLoadType : uint8_t;
enum class HasInsecureContent : bool;
enum class MouseEventPolicy : uint8_t;

class CertificateInfo;
class ResourceResponse;
class ResourceRequest;
}

namespace WebKit {

class NativeWebMouseEvent;
class RemotePageDrawingAreaProxy;
class RemotePageVisitedLinkStoreRegistration;
class UserData;
class WebFrameProxy;
class WebPageProxy;
class WebProcessProxy;

struct FrameInfoData;
struct FrameTreeCreationParameters;
struct NavigationActionData;

class RemotePageProxy : public RefCounted<RemotePageProxy>, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemotePageProxy> create(WebPageProxy& page, WebProcessProxy& process, const WebCore::RegistrableDomain& domain, WebPageProxyMessageReceiverRegistration* registrationToTransfer = nullptr) { return adoptRef(*new RemotePageProxy(page, process, domain, registrationToTransfer)); }
    ~RemotePageProxy();

    WebPageProxy* page() const;
    RefPtr<WebPageProxy> protectedPage() const;
    void addFrame(WebFrameProxy&);
    void removeFrame(WebFrameProxy&);

    template<typename M> void send(M&&);
    template<typename M> IPC::ConnectionSendSyncResult<M> sendSync(M&& message);
    template<typename M, typename C> void sendWithAsyncReply(M&&, C&&);
    template<typename M, typename C> void sendWithAsyncReply(M&&, C&&, const ObjectIdentifierGenericBase&);

    void injectPageIntoNewProcess();
    void processDidTerminate(WebCore::ProcessIdentifier);

    WebPageProxyMessageReceiverRegistration& messageReceiverRegistration() { return m_messageReceiverRegistration; }

    WebProcessProxy& process() { return m_process.get(); }
    Ref<WebProcessProxy> protectedProcess() const;
    WebCore::PageIdentifier pageID() const { return m_webPageID; }
    const WebCore::RegistrableDomain& domain() const { return m_domain; }

private:
    RemotePageProxy(WebPageProxy&, WebProcessProxy&, const WebCore::RegistrableDomain&, WebPageProxyMessageReceiverRegistration*);
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void decidePolicyForResponse(FrameInfoData&&, uint64_t navigationID, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&&);
    void didCommitLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);
    void decidePolicyForNavigationActionAsync(NavigationActionData&&, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForNavigationActionSync(NavigationActionData&&, CompletionHandler<void(PolicyDecision&&)>&&);
    void didFailProvisionalLoadForFrame(FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError&, WebCore::WillContinueLoading, const UserData&, WebCore::WillInternallyHandleFailure);
    void didChangeProvisionalURLForFrame(WebCore::FrameIdentifier, uint64_t, URL&&);
    void handleMessage(const String& messageName, const UserData& messageBody);

    const WebCore::PageIdentifier m_webPageID;
    const Ref<WebProcessProxy> m_process;
    WeakPtr<WebPageProxy> m_page;
    const WebCore::RegistrableDomain m_domain;
    std::unique_ptr<RemotePageDrawingAreaProxy> m_drawingArea;
    std::unique_ptr<RemotePageVisitedLinkStoreRegistration> m_visitedLinkStoreRegistration;
    WebPageProxyMessageReceiverRegistration m_messageReceiverRegistration;
    WeakHashSet<WebFrameProxy> m_frames;
};

template<typename M> void RemotePageProxy::send(M&& message)
{
    m_process->send(std::forward<M>(message), m_webPageID, { });
}

template<typename M>
IPC::ConnectionSendSyncResult<M> RemotePageProxy::sendSync(M&& message)
{
    return m_process->sendSync(std::forward<M>(message), m_webPageID);
}

template<typename M, typename C> void RemotePageProxy::sendWithAsyncReply(M&& message, C&& completionHandler)
{
    sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler), m_webPageID);
}

template<typename M, typename C> void RemotePageProxy::sendWithAsyncReply(M&& message, C&& completionHandler, const ObjectIdentifierGenericBase& destinationID)
{
    m_process->sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler), destinationID.toUInt64());
}

}
