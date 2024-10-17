/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)

#include "AuxiliaryProcessProxy.h"
#include "ProcessLauncher.h"
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/PageIdentifier.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/TZoneMalloc.h>

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
#include "LayerHostingContext.h"
#endif

namespace WebKit {

class WebProcessProxy;
class WebsiteDataStore;
struct ModelProcessConnectionParameters;
struct ModelProcessCreationParameters;
struct SharedPreferencesForWebProcess;

class ModelProcessProxy final : public AuxiliaryProcessProxy {
    WTF_MAKE_TZONE_ALLOCATED(ModelProcessProxy);
    WTF_MAKE_NONCOPYABLE(ModelProcessProxy);
    friend LazyNeverDestroyed<ModelProcessProxy>;
public:
    static Ref<ModelProcessProxy> getOrCreate();
    static ModelProcessProxy* singletonIfCreated();
    ~ModelProcessProxy();

    void createModelProcessConnection(WebProcessProxy&, IPC::Connection::Handle&& connectionIdentifier, ModelProcessConnectionParameters&&);
    void sharedPreferencesForWebProcessDidChange(WebProcessProxy&, SharedPreferencesForWebProcess&&, CompletionHandler<void()>&&);

    void updateProcessAssertion();

    void terminateForTesting();
    void webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&&);

    void removeSession(PAL::SessionID);

private:
    explicit ModelProcessProxy();

    void terminateWebProcess(WebCore::ProcessIdentifier);

    Type type() const final { return Type::Model; }

    void addSession(const WebsiteDataStore&);

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "Model"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    void modelProcessExited(ProcessTerminationReason);

    // ProcessThrottlerClient
    ASCIILiteral clientName() const final { return "ModelProcess"_s; }
    void sendPrepareToSuspend(IsSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&&) final;
    void sendProcessDidResume(ResumeReason) final;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier&&) override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() final;

    void processIsReadyToExit();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextForVisibilityPropagation(WebPageProxyIdentifier, WebCore::PageIdentifier, LayerHostingContextID);
#endif

    ModelProcessCreationParameters processCreationParameters();

    RefPtr<ProcessThrottler::Activity> m_activityFromWebProcesses;

    HashSet<PAL::SessionID> m_sessionIDs;
};

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
