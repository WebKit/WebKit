/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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
#include "AuxiliaryProcessProxy.h"

#include "AuxiliaryProcessCreationParameters.h"
#include "AuxiliaryProcessMessages.h"
#include "Connection.h"
#include "Logging.h"
#include "MessageNames.h"
#include "OverrideLanguages.h"
#include "StreamClientConnection.h"
#include "UIProcessLogInitialization.h"
#include "WebPageProxy.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessProxy.h"
#include <algorithm>
#include <ranges>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "CoreIPCSecureCoding.h"
#include "SandboxUtilities.h"
#include <sys/sysctl.h>
#include <wtf/spi/darwin/SandboxSPI.h>
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#include <pal/spi/ios/MobileGestaltSPI.h>
#endif

#if PLATFORM(VISION)
#include <WebCore/ThermalMitigationNotifier.h>
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
#include "ExtensionCapabilityGrant.h"
#endif

#include "WebPageProxyMessages.h"

namespace WebKit {

static HashMap<IPC::Connection::UniqueID, WeakPtr<AuxiliaryProcessProxy>>& connectionToProcessMap()
{
    static MainRunLoopNeverDestroyed<HashMap<IPC::Connection::UniqueID, WeakPtr<AuxiliaryProcessProxy>>> map;
    return map.get();
}

static Seconds adjustedTimeoutForThermalState(Seconds timeout)
{
#if PLATFORM(VISION)
    return WebCore::ThermalMitigationNotifier::isThermalMitigationEnabled() ? (timeout * 20) : timeout;
#else
    return timeout;
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(AuxiliaryProcessProxy);

AuxiliaryProcessProxy::AuxiliaryProcessProxy(ShouldTakeUIBackgroundAssertion shouldTakeUIBackgroundAssertion, AlwaysRunsAtBackgroundPriority alwaysRunsAtBackgroundPriority, Seconds responsivenessTimeout)
    : m_responsivenessTimer(ResponsivenessTimer::create(*this, adjustedTimeoutForThermalState(responsivenessTimeout)))
    , m_alwaysRunsAtBackgroundPriority(alwaysRunsAtBackgroundPriority == AlwaysRunsAtBackgroundPriority::Yes)
    , m_throttler(*this, shouldTakeUIBackgroundAssertion == ShouldTakeUIBackgroundAssertion::Yes)
{
}

AuxiliaryProcessProxy::~AuxiliaryProcessProxy()
{
    if (state() != State::Terminated)
        platformStartConnectionTerminationWatchdog();

    protectedThrottler()->didDisconnectFromProcess();

    if (RefPtr connection = m_connection)
        connection->invalidate();

    if (RefPtr processLauncher = std::exchange(m_processLauncher, nullptr))
        processLauncher->invalidate();

    replyToPendingMessages();

#if ENABLE(EXTENSION_CAPABILITIES)
    ASSERT(m_extensionCapabilityGrants.isEmpty());
#endif
    if (m_connection) {
        ASSERT(connectionToProcessMap().get(m_connection->uniqueID()) == this);
        connectionToProcessMap().remove(m_connection->uniqueID());
    }
}

void AuxiliaryProcessProxy::populateOverrideLanguagesLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions) const
{
    LOG(Language, "WebProcessProxy is getting launch options.");
    auto overrideLanguages = WebKit::overrideLanguages();
    if (overrideLanguages.isEmpty()) {
        LOG(Language, "overrideLanguages() reports empty. Calling platformOverrideLanguages()");
        overrideLanguages = platformOverrideLanguages();
    }
    if (!overrideLanguages.isEmpty()) {
        StringBuilder languageString;
        for (size_t i = 0; i < overrideLanguages.size(); ++i) {
            if (i)
                languageString.append(',');
            languageString.append(overrideLanguages[i]);
        }
        LOG_WITH_STREAM(Language, stream << "Setting WebProcess's launch OverrideLanguages to " << languageString);
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("OverrideLanguages"_s, languageString.toString());
    } else
        LOG(Language, "overrideLanguages is still empty. Not setting WebProcess's launch OverrideLanguages.");
}

void AuxiliaryProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    if (const char* userDirectorySuffix = getenv("DIRHELPER_USER_DIR_SUFFIX")) {
        if (auto userDirectorySuffixString = String::fromUTF8(userDirectorySuffix); !userDirectorySuffixString.isNull())
            launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("user-directory-suffix"_s, userDirectorySuffixString);
    }

    if (m_alwaysRunsAtBackgroundPriority)
        launchOptions.extraInitializationData.add<HashTranslatorASCIILiteral>("always-runs-at-background-priority"_s, "true"_s);

#if ENABLE(DEVELOPER_MODE) && (PLATFORM(GTK) || PLATFORM(WPE))
    const char* varname;
    switch (launchOptions.processType) {
    case ProcessLauncher::ProcessType::Web:
        varname = "WEB_PROCESS_CMD_PREFIX";
        break;
    case ProcessLauncher::ProcessType::Network:
        varname = "NETWORK_PROCESS_CMD_PREFIX";
        break;
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        varname = "GPU_PROCESS_CMD_PREFIX";
        break;
#endif
#if ENABLE(MODEL_PROCESS)
    case ProcessLauncher::ProcessType::Model:
        varname = "MODEL_PROCESS_CMD_PREFIX";
        break;
#endif
#if ENABLE(BUBBLEWRAP_SANDBOX)
    case ProcessLauncher::ProcessType::DBusProxy:
        RELEASE_ASSERT_NOT_REACHED();
        break;
#endif
    }
    const char* processCmdPrefix = getenv(varname);
    if (processCmdPrefix && *processCmdPrefix)
        launchOptions.processCmdPrefix = String::fromUTF8(processCmdPrefix);
#endif // ENABLE(DEVELOPER_MODE) && (PLATFORM(GTK) || PLATFORM(WPE))

    populateOverrideLanguagesLaunchOptions(launchOptions);

    platformGetLaunchOptions(launchOptions);
}

void AuxiliaryProcessProxy::connect()
{
    ASSERT(!m_processLauncher);
    m_processStart = MonotonicTime::now();
    ProcessLauncher::LaunchOptions launchOptions { m_processIdentifier };
    getLaunchOptions(launchOptions);
    m_processLauncher = ProcessLauncher::create(this, WTFMove(launchOptions));
}

void AuxiliaryProcessProxy::terminate()
{
    RELEASE_LOG(Process, "AuxiliaryProcessProxy::terminate: PID=%d", processID());

    if (state() != State::Terminated)
        platformStartConnectionTerminationWatchdog();

#if PLATFORM(COCOA) && !USE(EXTENSIONKIT_PROCESS_TERMINATION)
    if (RefPtr connection = m_connection) {
        if (connection->kill())
            return;
    }
#endif

    // FIXME: We should really merge process launching into IPC connection creation and get rid of the process launcher.
    if (RefPtr processLauncher = m_processLauncher)
        processLauncher->terminateProcess();
}

String AuxiliaryProcessProxy::stateString() const
{
    auto currentState = state();
    switch (currentState) {
    case AuxiliaryProcessProxy::State::Launching:
        return "Launching"_s;
    case AuxiliaryProcessProxy::State::Running:
        return "Running"_s;
    case AuxiliaryProcessProxy::State::Terminated:
        return "Terminated"_s;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool AuxiliaryProcessProxy::wasTerminated() const
{
    switch (state()) {
    case AuxiliaryProcessProxy::State::Launching:
        return false;
    case AuxiliaryProcessProxy::State::Terminated:
        return true;
    case AuxiliaryProcessProxy::State::Running:
        break;
    }

    auto pid = processID();
    if (!pid)
        return true;

#if PLATFORM(COCOA)
    // Use kill() with a signal of 0 to make sure there is indeed still a process with the given PID.
    // This is needed because it sometimes takes a little bit of time for us to get notified that a process
    // was terminated.
    return kill(pid, 0) && errno == ESRCH;
#else
    return false;
#endif
}

bool AuxiliaryProcessProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions, std::optional<IPC::Connection::AsyncReplyHandler> asyncReplyHandler, ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity)
{
    // FIXME: We should turn this into a RELEASE_ASSERT().
    ASSERT(isMainRunLoop());
    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedThis = Ref { *this }, encoder = WTFMove(encoder), sendOptions, asyncReplyHandler = WTFMove(asyncReplyHandler), shouldStartProcessThrottlerActivity]() mutable {
            protectedThis->sendMessage(WTFMove(encoder), sendOptions, WTFMove(asyncReplyHandler), shouldStartProcessThrottlerActivity);
        });
        return true;
    }

    if (asyncReplyHandler && canSendMessage() && shouldStartProcessThrottlerActivity == ShouldStartProcessThrottlerActivity::Yes) {
        auto completionHandler = WTFMove(asyncReplyHandler->completionHandler);
        asyncReplyHandler->completionHandler = [activity = protectedThrottler()->quietBackgroundActivity(description(encoder->messageName())), completionHandler = WTFMove(completionHandler)](IPC::Connection* connection, IPC::Decoder* decoder) mutable {
            completionHandler(connection, decoder);
        };
    }

    switch (state()) {
    case State::Launching:
        // If we're waiting for the child process to launch, we need to stash away the messages so we can send them once we have a connection.
        m_pendingMessages.append({ WTFMove(encoder), sendOptions, WTFMove(asyncReplyHandler) });
        return true;

    case State::Running:
        if (asyncReplyHandler) {
            if (protectedConnection()->sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(*asyncReplyHandler), sendOptions) == IPC::Error::NoError)
                return true;
        } else {
            if (protectedConnection()->sendMessage(WTFMove(encoder), sendOptions) == IPC::Error::NoError)
                return true;
        }
        break;

    case State::Terminated:
        break;
    }

    if (asyncReplyHandler && asyncReplyHandler->completionHandler) {
        RunLoop::currentSingleton().dispatch([completionHandler = WTFMove(asyncReplyHandler->completionHandler)]() mutable {
            completionHandler(nullptr, nullptr);
        });
    }

    return false;
}

bool AuxiliaryProcessProxy::sendMessageAfterResuming(Vector<uint8_t>&& coalescingKey, UniqueRef<IPC::Encoder>&& encoder)
{
    ASSERT(m_isSuspended);

    if (!canSendMessage())
        return false;

    LOG(ProcessSuspension, "%p - AuxiliaryProcessProxy::sendMessageAfterResuming: deferring sending message %s to destination %" PRIu64 " in pid %i because it is suspended", this, description(encoder->messageName()).characters(), encoder->destinationID(), processID());

    m_messagesToSendOnResume.set(WTFMove(coalescingKey), std::make_pair(m_messagesToSendOnResumeIndex++, encoder.moveToUniquePtr()));
    return true;
}

void AuxiliaryProcessProxy::addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void AuxiliaryProcessProxy::addMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void AuxiliaryProcessProxy::removeMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

void AuxiliaryProcessProxy::removeMessageReceiver(IPC::ReceiverName messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

bool AuxiliaryProcessProxy::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool AuxiliaryProcessProxy::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void AuxiliaryProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier&& connectionIdentifier)
{
    ASSERT(!m_connection);
    ASSERT(isMainRunLoop());

    auto launchTime = MonotonicTime::now() - m_processStart;
    if (launchTime > 1_s)
        RELEASE_LOG_FAULT(Process, "%s process (%p) took %f seconds to launch", processName().characters(), this, launchTime.value());
    
    if (!connectionIdentifier)
        return;

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    m_lifetimeActivity = protectedThrottler()->foregroundActivity("Lifetime Activity"_s);
    m_boostedJetsamAssertion = ProcessAssertion::create(*this, "Jetsam Boost"_s, ProcessAssertionType::BoostedJetsam);
#endif

    RefPtr connection = IPC::Connection::createServerConnection(WTFMove(connectionIdentifier), Thread::QOS::UserInteractive);
    m_connection = connection.copyRef();
    auto addResult = connectionToProcessMap().add(m_connection->uniqueID(), *this);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    connectionWillOpen(*connection);
    connection->open(*this);
    connection->setOutgoingMessageQueueIsGrowingLargeCallback([weakThis = WeakPtr { *this }] {
        ensureOnMainRunLoop([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->outgoingMessageQueueIsGrowingLarge();
        });
    });

    for (auto&& pendingMessage : std::exchange(m_pendingMessages, { })) {
        if (!shouldSendPendingMessage(pendingMessage))
            continue;
        if (pendingMessage.asyncReplyHandler)
            connection->sendMessageWithAsyncReply(WTFMove(pendingMessage.encoder), WTFMove(*pendingMessage.asyncReplyHandler), pendingMessage.sendOptions);
        else
            connection->sendMessage(WTFMove(pendingMessage.encoder), pendingMessage.sendOptions);
    }

#if USE(RUNNINGBOARD)
    protectedThrottler()->didConnectToProcess(*this);
#if USE(EXTENSIONKIT)
    ASSERT(launcher);
    if (launcher)
        launcher->releaseLaunchGrant();
#endif // USE(EXTENSIONKIT)
#endif // USE(RUNNINGBOARD)
}

void AuxiliaryProcessProxy::outgoingMessageQueueIsGrowingLarge()
{
#if USE(RUNNINGBOARD)
    wakeUpTemporarilyForIPC();
#endif
}

#if USE(RUNNINGBOARD)
void AuxiliaryProcessProxy::wakeUpTemporarilyForIPC()
{
    // If we keep trying to send IPC to a suspended process, the outgoing message queue may grow large and result
    // in increased memory usage. To avoid this, we allow the process to stay alive for 1 second after draining
    // its message queue.
    auto completionHandler = [activity = protectedThrottler()->backgroundActivity("IPC sending due to large outgoing queue"_s)]() mutable {
        RunLoop::mainSingleton().dispatchAfter(1_s, [activity = WTFMove(activity)]() { });
    };
    sendWithAsyncReply(Messages::AuxiliaryProcess::MainThreadPing(), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}
#endif

void AuxiliaryProcessProxy::replyToPendingMessages()
{
    ASSERT(isMainRunLoop());
    for (auto& pendingMessage : std::exchange(m_pendingMessages, { })) {
        if (pendingMessage.asyncReplyHandler)
            pendingMessage.asyncReplyHandler->completionHandler(nullptr, nullptr);
    }
}

void AuxiliaryProcessProxy::shutDownProcess()
{
    auto scopeExit = WTF::makeScopeExit([protectedThis = Ref { *this }] {
        protectedThis->protectedThrottler()->didDisconnectFromProcess();
    });

    switch (state()) {
    case State::Launching: {
        RefPtr processLauncher = std::exchange(m_processLauncher, nullptr);
        processLauncher->invalidate();
        break;
    }
    case State::Running:
        platformStartConnectionTerminationWatchdog();
        break;
    case State::Terminated:
        return;
    }

    RefPtr connection = m_connection;
    if (!connection)
        return;

    processWillShutDown(*connection);

    if (canSendMessage())
        send(Messages::AuxiliaryProcess::ShutDown(), 0);

    connection->invalidate();
    ASSERT(connectionToProcessMap().get(connection->uniqueID()) == this);
    connectionToProcessMap().remove(connection->uniqueID());
    m_connection = nullptr;
    protectedResponsivenessTimer()->invalidate();
}

AuxiliaryProcessProxy* AuxiliaryProcessProxy::fromConnection(const IPC::Connection& connection)
{
    return connectionToProcessMap().get(connection.uniqueID());
}

void AuxiliaryProcessProxy::setProcessSuppressionEnabled(bool processSuppressionEnabled)
{
#if PLATFORM(COCOA)
    if (state() != State::Running)
        return;

    protectedConnection()->send(Messages::AuxiliaryProcess::SetProcessSuppressionEnabled(processSuppressionEnabled), 0);
#else
    UNUSED_PARAM(processSuppressionEnabled);
#endif
}

void AuxiliaryProcessProxy::connectionWillOpen(IPC::Connection&)
{
}

void AuxiliaryProcessProxy::logInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from the %" PUBLIC_LOG_STRING " process with PID %d", description(messageName).characters(), processName().characters(), processID());
}

bool AuxiliaryProcessProxy::platformIsBeingDebugged() const
{
#if PLATFORM(COCOA)
    // If the UI process is sandboxed and lacks 'process-info-pidinfo', it cannot find out whether other processes are being debugged.
    if (currentProcessIsSandboxed() && !!sandbox_check(getpid(), "process-info-pidinfo", SANDBOX_CHECK_NO_REPORT))
        return false;

    struct kinfo_proc info;
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, processID() };
    size_t size = sizeof(info);
    if (sysctl(mib, std::size(mib), &info, &size, nullptr, 0) == -1)
        return false;

    return info.kp_proc.p_flag & P_TRACED;
#else
    return false;
#endif
}

void AuxiliaryProcessProxy::stopResponsivenessTimer()
{
    protectedResponsivenessTimer()->stop();
}

void AuxiliaryProcessProxy::beginResponsivenessChecks()
{
    m_didBeginResponsivenessChecks = true;
    if (m_delayedResponsivenessCheck)
        startResponsivenessTimer(*std::exchange(m_delayedResponsivenessCheck, std::nullopt));
}

void AuxiliaryProcessProxy::startResponsivenessTimer(UseLazyStop useLazyStop)
{
    if (!m_didBeginResponsivenessChecks) {
        if (!m_delayedResponsivenessCheck)
            m_delayedResponsivenessCheck = useLazyStop;
        return;
    }

    if (useLazyStop == UseLazyStop::Yes)
        protectedResponsivenessTimer()->startWithLazyStop();
    else
        protectedResponsivenessTimer()->start();
}

bool AuxiliaryProcessProxy::mayBecomeUnresponsive()
{
    return !(platformIsBeingDebugged() || throttler().isSuspended());
}

void AuxiliaryProcessProxy::didBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "AuxiliaryProcessProxy::didBecomeUnresponsive: %" PUBLIC_LOG_STRING " process with PID %d became unresponsive", processName().characters(), processID());
}

void AuxiliaryProcessProxy::checkForResponsiveness(CompletionHandler<void()>&& responsivenessHandler, UseLazyStop useLazyStop)
{
    startResponsivenessTimer(useLazyStop);
    sendWithAsyncReply(Messages::AuxiliaryProcess::MainThreadPing(), [weakThis = WeakPtr { *this }, responsivenessHandler = WTFMove(responsivenessHandler)]() mutable {
        // Schedule an asynchronous task because our completion handler may have been called as a result of the AuxiliaryProcessProxy
        // being in the middle of destruction.
        RunLoop::mainSingleton().dispatch([weakThis = WTFMove(weakThis), responsivenessHandler = WTFMove(responsivenessHandler)]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->stopResponsivenessTimer();

            if (responsivenessHandler)
                responsivenessHandler();
        });
    });
}

AuxiliaryProcessCreationParameters AuxiliaryProcessProxy::auxiliaryProcessParameters()
{
    AuxiliaryProcessCreationParameters parameters;
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    parameters.wtfLoggingChannels = UIProcess::wtfLogLevelString();
    parameters.webCoreLoggingChannels = UIProcess::webCoreLogLevelString();
    parameters.webKitLoggingChannels = UIProcess::webKitLogLevelString();
#endif

#if PLATFORM(COCOA)
    auto* exemptClassNames = SecureCoding::classNamesExemptFromSecureCodingCrash();
    if (exemptClassNames)
        parameters.classNamesExemptFromSecureCodingCrash = WTF::makeUnique<HashSet<String>>(*exemptClassNames);
#endif

#if ENABLE(CORE_IPC_SIGNPOSTS)
    parameters.shouldEnableIPCSignposts = IPC::Connection::signpostsEnabled();
    parameters.shouldEnableStreamingIPCSignposts = IPC::StreamClientConnection::signpostsEnabled();
#endif

    return parameters;
}

std::optional<SandboxExtensionHandle> AuxiliaryProcessProxy::createMobileGestaltSandboxExtensionIfNeeded() const
{
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    if (_MGCacheValid())
        return std::nullopt;
    
    RELEASE_LOG_FAULT(Sandbox, "MobileGestalt cache is invalid! Creating a sandbox extension to repopulate cache in memory.");

    return SandboxExtension::createHandleForMachLookup("com.apple.mobilegestalt.xpc"_s, std::nullopt);
#else
    return std::nullopt;
#endif
}

#if !PLATFORM(COCOA)
Vector<String> AuxiliaryProcessProxy::platformOverrideLanguages() const
{
    return { };
}

void AuxiliaryProcessProxy::platformStartConnectionTerminationWatchdog()
{
}

#endif

void AuxiliaryProcessProxy::requestRemoteProcessTermination()
{
    terminate();
}

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
void AuxiliaryProcessProxy::setRunningBoardThrottlingEnabled()
{
    m_lifetimeActivity = nullptr;
}

bool AuxiliaryProcessProxy::runningBoardThrottlingEnabled()
{
    return !m_lifetimeActivity;
}
#endif

void AuxiliaryProcessProxy::didChangeThrottleState(ProcessThrottleState state)
{
    bool isNowSuspended = state == ProcessThrottleState::Suspended;
    if (m_isSuspended == isNowSuspended)
        return;
    m_isSuspended = isNowSuspended;

    if (!isNowSuspended && !m_messagesToSendOnResume.isEmpty()) {
        using PairType = std::pair<unsigned, std::unique_ptr<IPC::Encoder>>;
        Vector<PairType> indexMessagePairs;
        indexMessagePairs.reserveInitialCapacity(m_messagesToSendOnResume.size());

        for (auto& indexMessagePair : m_messagesToSendOnResume.values())
            indexMessagePairs.append(WTFMove(indexMessagePair));

        // Send messages in the order that they were enqueued after coalescing.
        std::ranges::sort(indexMessagePairs, { }, &PairType::first);

        for (auto& indexMessagePair : indexMessagePairs) {
            auto& encoder = indexMessagePair.second;
            LOG(ProcessSuspension, "%p - AuxiliaryProcessProxy::didChangeThrottleState: sending deferred message %s to destination %" PRIu64 " in pid %i because it resumed", this, description(encoder->messageName()).characters(), encoder->destinationID(), processID());
            sendMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(encoder)), { });
        }

        m_messagesToSendOnResume.clear();
        m_messagesToSendOnResumeIndex = 0;
    }
}

AuxiliaryProcessProxy::InitializationActivityAndGrant AuxiliaryProcessProxy::initializationActivityAndGrant()
{
    return {
        protectedThrottler()->foregroundActivity("Process initialization"_s)
#if USE(EXTENSIONKIT)
        , launchGrant()
#endif
    };
}

} // namespace WebKit
