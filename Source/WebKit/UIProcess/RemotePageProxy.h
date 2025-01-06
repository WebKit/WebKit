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
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/Site.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
class Decoder;
class Encoder;
template<typename> class ConnectionSendSyncResult;
}

namespace WebCore {
enum class CrossOriginOpenerPolicyValue : uint8_t;
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
class WebProcessActivityState;
class WebProcessProxy;

struct FrameInfoData;
struct FrameTreeCreationParameters;
struct NavigationActionData;

enum class ProcessTerminationReason : uint8_t;

class RemotePageProxy : public IPC::MessageReceiver, public RefCounted<RemotePageProxy> {
    WTF_MAKE_TZONE_ALLOCATED(RemotePageProxy);
public:
    static Ref<RemotePageProxy> create(WebPageProxy&, WebProcessProxy&, const WebCore::Site&, WebPageProxyMessageReceiverRegistration* = nullptr);
    ~RemotePageProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    WebPageProxy* page() const;
    RefPtr<WebPageProxy> protectedPage() const;

    void injectPageIntoNewProcess();
    void removePageFromProcess();
    void processDidTerminate(WebProcessProxy&, ProcessTerminationReason);

    WebPageProxyMessageReceiverRegistration& messageReceiverRegistration() { return m_messageReceiverRegistration; }

    WebProcessProxy& process() { return m_process.get(); }
    Ref<WebProcessProxy> protectedProcess() const;
    WebProcessProxy& siteIsolatedProcess() const { return m_process.get(); }
    WebCore::PageIdentifier pageID() const { return m_webPageID; } // FIXME: Remove this in favor of identifierInSiteIsolatedProcess.
    WebCore::PageIdentifier identifierInSiteIsolatedProcess() const { return m_webPageID; }
    const WebCore::Site& site() const { return m_site; }

    WebProcessActivityState& processActivityState();

private:
    RemotePageProxy(WebPageProxy&, WebProcessProxy&, const WebCore::Site&, WebPageProxyMessageReceiverRegistration*);
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void decidePolicyForResponse(FrameInfoData&&, std::optional<WebCore::NavigationIdentifier>, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, const String& downloadAttribute, bool isShowingInitialAboutBlank, WebCore::CrossOriginOpenerPolicyValue activeDocumentCOOPValue, CompletionHandler<void(PolicyDecision&&)>&&);
    void didCommitLoadForFrame(IPC::Connection&, WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);
    void decidePolicyForNavigationActionAsync(NavigationActionData&&, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForNavigationActionSync(NavigationActionData&&, CompletionHandler<void(PolicyDecision&&)>&&);
    void didFailProvisionalLoadForFrame(FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, const String& provisionalURL, const WebCore::ResourceError&, WebCore::WillContinueLoading, const UserData&, WebCore::WillInternallyHandleFailure);
    void didStartProvisionalLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, URL&&, URL&& unreachableURL, const UserData&, WallTime);
    void didChangeProvisionalURLForFrame(WebCore::FrameIdentifier, std::optional<WebCore::NavigationIdentifier>, URL&&);
    void handleMessage(const String& messageName, const UserData& messageBody);

    const WebCore::PageIdentifier m_webPageID;
    const Ref<WebProcessProxy> m_process;
    WeakPtr<WebPageProxy> m_page;
    const WebCore::Site m_site;
    RefPtr<RemotePageDrawingAreaProxy> m_drawingArea;
    std::unique_ptr<RemotePageVisitedLinkStoreRegistration> m_visitedLinkStoreRegistration;
    WebPageProxyMessageReceiverRegistration m_messageReceiverRegistration;
    UniqueRef<WebProcessActivityState> m_processActivityState;
};

}
