/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "AuxiliaryProcessProxy.h"
#include "GPUProcessProxyMessagesReplies.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessProxyMessagesReplies.h"
#include <memory>
#include <pal/SessionID.h>

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
#include "LayerHostingContext.h"
#endif

namespace WebCore {
struct MockMediaDevice;
}

namespace WebKit {

class WebProcessProxy;
class WebsiteDataStore;
struct GPUProcessConnectionParameters;
struct GPUProcessCreationParameters;

class GPUProcessProxy final : public AuxiliaryProcessProxy, private ProcessThrottlerClient, public RefCounted<GPUProcessProxy> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(GPUProcessProxy);
    friend LazyNeverDestroyed<GPUProcessProxy>;
public:
    static Ref<GPUProcessProxy> getOrCreate();
    static GPUProcessProxy* singletonIfCreated();
    ~GPUProcessProxy();

    void getGPUProcessConnection(WebProcessProxy&, const GPUProcessConnectionParameters&, Messages::WebProcessProxy::GetGPUProcessConnectionDelayedReply&&);

    ProcessThrottler& throttler() final { return m_throttler; }
    void updateProcessAssertion();

#if ENABLE(MEDIA_STREAM)
    void setUseMockCaptureDevices(bool);
    void setOrientationForMediaCapture(uint64_t orientation);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier, CompletionHandler<void()>&&);

    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String&);
    void resetMockMediaDevices();
#endif

    void removeSession(PAL::SessionID);

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID contextIDForVisibilityPropagation() const;
#endif

private:
    explicit GPUProcessProxy();

    void addSession(const WebsiteDataStore&);

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "GPU"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    void gpuProcessCrashed();

    // ProcessThrottlerClient
    ASCIILiteral clientName() const final { return "GPUProcess"_s; }
    void sendPrepareToSuspend(IsSuspensionImminent, CompletionHandler<void()>&&) final;
    void sendProcessDidResume() final;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    void terminateWebProcess(WebCore::ProcessIdentifier);

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextForVisibilityPropagation(LayerHostingContextID);
#endif

    ProcessThrottler m_throttler;
    ProcessThrottler::ActivityVariant m_activityFromWebProcesses;
#if ENABLE(MEDIA_STREAM)
    bool m_useMockCaptureDevices { false };
    uint64_t m_orientation { 0 };
#endif
    HashSet<PAL::SessionID> m_sessionIDs;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID m_contextIDForVisibilityPropagation { 0 };
    Vector<WeakPtr<WebProcessProxy>> m_processesPendingVisibilityPropagationNotification;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
