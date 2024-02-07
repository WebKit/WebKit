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

#include "config.h"
#include "ModelProcessProxy.h"

#if ENABLE(MODEL_PROCESS)

#include "DrawingAreaProxy.h"
#include "Logging.h"
#include "ModelProcessConnectionParameters.h"
#include "ModelProcessCreationParameters.h"
#include "ModelProcessMessages.h"
#include "ModelProcessProxyMessages.h"
#include "ProvisionalPageProxy.h"
#include "WebPageGroup.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/LogInitialization.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/CompletionHandler.h>
#include <wtf/LogInitialization.h>
#include <wtf/MachSendRight.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, this->connection())

namespace WebKit {
using namespace WebCore;

static WeakPtr<ModelProcessProxy>& singleton()
{
    static NeverDestroyed<WeakPtr<ModelProcessProxy>> singleton;
    return singleton;
}

Ref<ModelProcessProxy> ModelProcessProxy::getOrCreate()
{
    ASSERT(RunLoop::isMain());
    if (auto& existingModelProcess = singleton()) {
        ASSERT(existingModelProcess->state() != State::Terminated);
        return *existingModelProcess;
    }
    Ref modelProcess = adoptRef(*new ModelProcessProxy);
    singleton() = modelProcess;
    return modelProcess;
}

ModelProcessProxy* ModelProcessProxy::singletonIfCreated()
{
    return singleton().get();
}

ModelProcessProxy::ModelProcessProxy()
    : AuxiliaryProcessProxy()
    , m_throttler(*this, WebProcessPool::anyProcessPoolNeedsUIBackgroundAssertion())
{
    connect();

    ModelProcessCreationParameters parameters;
    parameters.auxiliaryProcessParameters = auxiliaryProcessParameters();
    parameters.parentPID = getCurrentProcessID();

    // Initialize the model process.
    send(Messages::ModelProcess::InitializeModelProcess(WTFMove(parameters)), 0);

    updateProcessAssertion();
}

ModelProcessProxy::~ModelProcessProxy() = default;

void ModelProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Model;
    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);
}

void ModelProcessProxy::connectionWillOpen(IPC::Connection&)
{
}

void ModelProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void ModelProcessProxy::createModelProcessConnection(WebProcessProxy& webProcessProxy, IPC::Connection::Handle&& connectionIdentifier, ModelProcessConnectionParameters&& parameters)
{
    if (auto* store = webProcessProxy.websiteDataStore())
        addSession(*store);

    RELEASE_LOG(ProcessSuspension, "%p - ModelProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    startResponsivenessTimer(UseLazyStop::No);
    sendWithAsyncReply(Messages::ModelProcess::CreateModelConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID(), WTFMove(connectionIdentifier), WTFMove(parameters) }, [this, weakThis = WeakPtr { *this }]() mutable {
        if (!weakThis)
            return;
        stopResponsivenessTimer();
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void ModelProcessProxy::modelProcessExited(ProcessTerminationReason reason)
{
    Ref protectedThis { *this };

    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
    case ProcessTerminationReason::ExceededCPULimit:
    case ProcessTerminationReason::RequestedByClient:
    case ProcessTerminationReason::IdleExit:
    case ProcessTerminationReason::Unresponsive:
    case ProcessTerminationReason::Crash:
        RELEASE_LOG_ERROR(Process, "%p - ModelProcessProxy::modelProcessExited: reason=%{public}s", this, processTerminationReasonToString(reason));
        break;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
        ASSERT_NOT_REACHED();
        break;
    }

    if (singleton() == this)
        singleton() = nullptr;

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->modelProcessExited(processID(), reason);
}

void ModelProcessProxy::processIsReadyToExit()
{
    RELEASE_LOG(Process, "%p - ModelProcessProxy::processIsReadyToExit:", this);
    terminate();
    modelProcessExited(ProcessTerminationReason::IdleExit); // May cause |this| to get deleted.
}

void ModelProcessProxy::addSession(const WebsiteDataStore& store)
{
    if (!canSendMessage())
        return;

    if (m_sessionIDs.contains(store.sessionID()))
        return;

    send(Messages::ModelProcess::AddSession { store.sessionID() }, 0);
    m_sessionIDs.add(store.sessionID());
}

void ModelProcessProxy::removeSession(PAL::SessionID sessionID)
{
    if (!canSendMessage())
        return;

    if (m_sessionIDs.remove(sessionID))
        send(Messages::ModelProcess::RemoveSession { sessionID }, 0);
}

void ModelProcessProxy::terminateForTesting()
{
    processIsReadyToExit();
}

void ModelProcessProxy::webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    sendWithAsyncReply(Messages::ModelProcess::WebProcessConnectionCountForTesting(), WTFMove(completionHandler));
}

void ModelProcessProxy::didClose(IPC::Connection&)
{
    RELEASE_LOG_ERROR(Process, "%p - ModelProcessProxy::didClose:", this);
    modelProcessExited(ProcessTerminationReason::Crash); // May cause |this| to get deleted.
}

void ModelProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    logInvalidMessage(connection, messageName);

    WebProcessPool::didReceiveInvalidMessage(messageName);

    // Terminate the model process.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void ModelProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!connectionIdentifier) {
        modelProcessExited(ProcessTerminationReason::Crash);
        return;
    }

#if USE(RUNNINGBOARD)
    m_throttler.didConnectToProcess(*this);
#endif

#if PLATFORM(COCOA)
    if (auto networkProcess = NetworkProcessProxy::defaultNetworkProcess())
        networkProcess->sendXPCEndpointToProcess(*this);
#endif

    beginResponsivenessChecks();

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->modelProcessDidFinishLaunching(processID());
}

void ModelProcessProxy::updateProcessAssertion()
{
    bool hasAnyForegroundWebProcesses = false;
    bool hasAnyBackgroundWebProcesses = false;

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        hasAnyForegroundWebProcesses |= processPool->hasForegroundWebProcesses();
        hasAnyBackgroundWebProcesses |= processPool->hasBackgroundWebProcesses();
    }

    if (hasAnyForegroundWebProcesses) {
        if (!ProcessThrottler::isValidForegroundActivity(m_activityFromWebProcesses))
            m_activityFromWebProcesses = throttler().foregroundActivity("Model for foreground view(s)"_s);
        return;
    }
    if (hasAnyBackgroundWebProcesses) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_activityFromWebProcesses))
            m_activityFromWebProcesses = throttler().backgroundActivity("Model for background view(s)"_s);
        return;
    }

    // Use std::exchange() instead of a simple nullptr assignment to avoid re-entering this
    // function during the destructor of the ProcessThrottler activity, before setting
    // m_activityFromWebProcesses.
    std::exchange(m_activityFromWebProcesses, nullptr);
}

void ModelProcessProxy::sendPrepareToSuspend(IsSuspensionImminent isSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::ModelProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes, MonotonicTime::now() + Seconds(remainingRunTime)), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void ModelProcessProxy::sendProcessDidResume(ResumeReason)
{
    if (canSendMessage())
        send(Messages::ModelProcess::ProcessDidResume(), 0);
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void ModelProcessProxy::didCreateContextForVisibilityPropagation(WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier pageID, LayerHostingContextID contextID)
{
    RELEASE_LOG(Process, "ModelProcessProxy::didCreateContextForVisibilityPropagation: webPageProxyID: %" PRIu64 ", pagePID: %" PRIu64 ", contextID: %u", webPageProxyID.toUInt64(), pageID.toUInt64(), contextID);
    auto page = WebProcessProxy::webPage(webPageProxyID);
    if (!page) {
        RELEASE_LOG(Process, "ModelProcessProxy::didCreateContextForVisibilityPropagation() No WebPageProxy with this identifier");
        return;
    }
    if (page->webPageID() == pageID) {
        page->didCreateContextInModelProcessForVisibilityPropagation(contextID);
        return;
    }
    auto* provisionalPage = page->provisionalPageProxy();
    if (provisionalPage && provisionalPage->webPageID() == pageID) {
        provisionalPage->didCreateContextInModelProcessForVisibilityPropagation(contextID);
        return;
    }
    RELEASE_LOG(Process, "ModelProcessProxy::didCreateContextForVisibilityPropagation() There was a WebPageProxy for this identifier, but it had the wrong WebPage identifier.");
}
#endif

void ModelProcessProxy::didBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "ModelProcessProxy::didBecomeUnresponsive: ModelProcess with PID %d became unresponsive, terminating it", processID());
    terminate();
    modelProcessExited(ProcessTerminationReason::Unresponsive);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(MODEL_PROCESS)
