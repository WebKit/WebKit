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

#include "AuxiliaryProcessMessages.h"
#include "Logging.h"
#include "OverrideLanguages.h"
#include "UIProcessLogInitialization.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/RunLoop.h>

#if PLATFORM(COCOA)
#include "SandboxUtilities.h"
#include <sys/sysctl.h>
#include <wtf/spi/darwin/SandboxSPI.h>
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#import <pal/spi/ios/MobileGestaltSPI.h>
#endif

namespace WebKit {

AuxiliaryProcessProxy::AuxiliaryProcessProxy(bool alwaysRunsAtBackgroundPriority, Seconds responsivenessTimeout)
    : m_responsivenessTimer(*this, responsivenessTimeout)
    , m_alwaysRunsAtBackgroundPriority(alwaysRunsAtBackgroundPriority)
{
}

AuxiliaryProcessProxy::~AuxiliaryProcessProxy()
{
    if (m_connection)
        m_connection->invalidate();

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = nullptr;
    }

    replyToPendingMessages();
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
    launchOptions.processIdentifier = m_processIdentifier;

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
#if ENABLE(BUBBLEWRAP_SANDBOX)
    case ProcessLauncher::ProcessType::DBusProxy:
        ASSERT_NOT_REACHED();
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
    ProcessLauncher::LaunchOptions launchOptions;
    getLaunchOptions(launchOptions);
    m_processLauncher = ProcessLauncher::create(this, WTFMove(launchOptions));
}

void AuxiliaryProcessProxy::terminate()
{
    RELEASE_LOG(Process, "AuxiliaryProcessProxy::terminate: PID=%d", processIdentifier());

#if PLATFORM(COCOA)
    if (m_connection && m_connection->kill())
        return;
#endif

    // FIXME: We should really merge process launching into IPC connection creation and get rid of the process launcher.
    if (m_processLauncher)
        m_processLauncher->terminateProcess();
}

AuxiliaryProcessProxy::State AuxiliaryProcessProxy::state() const
{
    if (m_processLauncher && m_processLauncher->isLaunching())
        return AuxiliaryProcessProxy::State::Launching;

    if (!m_connection)
        return AuxiliaryProcessProxy::State::Terminated;

    return AuxiliaryProcessProxy::State::Running;
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

    auto pid = processIdentifier();
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
        asyncReplyHandler->completionHandler = [activity = throttler().backgroundActivity({ }), completionHandler = WTFMove(completionHandler)](IPC::Decoder* decoder) mutable {
            completionHandler(decoder);
        };
    }

    switch (state()) {
    case State::Launching:
        // If we're waiting for the child process to launch, we need to stash away the messages so we can send them once we have a connection.
        m_pendingMessages.append({ WTFMove(encoder), sendOptions, WTFMove(asyncReplyHandler) });
        return true;

    case State::Running:
        if (asyncReplyHandler) {
            if (connection()->sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(*asyncReplyHandler), sendOptions))
                return true;
        } else {
            if (connection()->sendMessage(WTFMove(encoder), sendOptions))
                return true;
        }
        break;

    case State::Terminated:
        break;
    }

    if (asyncReplyHandler && asyncReplyHandler->completionHandler) {
        RunLoop::current().dispatch([completionHandler = WTFMove(asyncReplyHandler->completionHandler)]() mutable {
            completionHandler(nullptr);
        });
    }
    
    return false;
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

void AuxiliaryProcessProxy::didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);
    ASSERT(isMainRunLoop());

    auto launchTime = MonotonicTime::now() - m_processStart;
    if (launchTime > 1_s)
        RELEASE_LOG_FAULT(Process, "%s process (%p) took %f seconds to launch", processName().characters(), this, launchTime.value());
    
    if (!connectionIdentifier)
        return;

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    m_lifetimeActivity = throttler().foregroundActivity("Lifetime Activity"_s).moveToUniquePtr();
#endif

    m_connection = IPC::Connection::createServerConnection(connectionIdentifier);

    connectionWillOpen(*m_connection);
    m_connection->open(*this);

    for (auto&& pendingMessage : std::exchange(m_pendingMessages, { })) {
        if (!shouldSendPendingMessage(pendingMessage))
            continue;
        if (pendingMessage.asyncReplyHandler)
            m_connection->sendMessageWithAsyncReply(WTFMove(pendingMessage.encoder), WTFMove(*pendingMessage.asyncReplyHandler), pendingMessage.sendOptions);
        else
            m_connection->sendMessage(WTFMove(pendingMessage.encoder), pendingMessage.sendOptions);
    }
}

void AuxiliaryProcessProxy::replyToPendingMessages()
{
    ASSERT(isMainRunLoop());
    for (auto& pendingMessage : std::exchange(m_pendingMessages, { })) {
        if (pendingMessage.asyncReplyHandler)
            pendingMessage.asyncReplyHandler->completionHandler(nullptr);
    }
}

void AuxiliaryProcessProxy::shutDownProcess()
{
    switch (state()) {
    case State::Launching:
        m_processLauncher->invalidate();
        m_processLauncher = nullptr;
        break;
    case State::Running:
        platformStartConnectionTerminationWatchdog();
        break;
    case State::Terminated:
        return;
    }

    if (!m_connection)
        return;

    processWillShutDown(*m_connection);

    if (canSendMessage())
        send(Messages::AuxiliaryProcess::ShutDown(), 0);

    m_connection->invalidate();
    m_connection = nullptr;
    m_responsivenessTimer.invalidate();
}

void AuxiliaryProcessProxy::setProcessSuppressionEnabled(bool processSuppressionEnabled)
{
#if PLATFORM(COCOA)
    if (state() != State::Running)
        return;

    connection()->send(Messages::AuxiliaryProcess::SetProcessSuppressionEnabled(processSuppressionEnabled), 0);
#else
    UNUSED_PARAM(processSuppressionEnabled);
#endif
}

void AuxiliaryProcessProxy::connectionWillOpen(IPC::Connection&)
{
}

void AuxiliaryProcessProxy::logInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from the %" PUBLIC_LOG_STRING " process.", description(messageName), processName().characters());
}

bool AuxiliaryProcessProxy::platformIsBeingDebugged() const
{
#if PLATFORM(COCOA)
    // If the UI process is sandboxed and lacks 'process-info-pidinfo', it cannot find out whether other processes are being debugged.
    if (currentProcessIsSandboxed() && !!sandbox_check(getpid(), "process-info-pidinfo", SANDBOX_CHECK_NO_REPORT))
        return false;

    struct kinfo_proc info;
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, processIdentifier() };
    size_t size = sizeof(info);
    if (sysctl(mib, WTF_ARRAY_LENGTH(mib), &info, &size, nullptr, 0) == -1)
        return false;

    return info.kp_proc.p_flag & P_TRACED;
#else
    return false;
#endif
}

void AuxiliaryProcessProxy::stopResponsivenessTimer()
{
    responsivenessTimer().stop();
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
        responsivenessTimer().startWithLazyStop();
    else
        responsivenessTimer().start();
}

bool AuxiliaryProcessProxy::mayBecomeUnresponsive()
{
    return !platformIsBeingDebugged();
}

void AuxiliaryProcessProxy::didBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "AuxiliaryProcessProxy::didBecomeUnresponsive: %" PUBLIC_LOG_STRING " process with PID %d became unresponsive", processName().characters(), processIdentifier());
}

void AuxiliaryProcessProxy::checkForResponsiveness(CompletionHandler<void()>&& responsivenessHandler, UseLazyStop useLazyStop)
{
    startResponsivenessTimer(useLazyStop);
    sendWithAsyncReply(Messages::AuxiliaryProcess::MainThreadPing(), [weakThis = WeakPtr { *this }, responsivenessHandler = WTFMove(responsivenessHandler)]() mutable {
        // Schedule an asynchronous task because our completion handler may have been called as a result of the AuxiliaryProcessProxy
        // being in the middle of destruction.
        RunLoop::main().dispatch([weakThis = WTFMove(weakThis), responsivenessHandler = WTFMove(responsivenessHandler)]() mutable {
            if (weakThis)
                weakThis->stopResponsivenessTimer();

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
    return parameters;
}

std::optional<SandboxExtension::Handle> AuxiliaryProcessProxy::createMobileGestaltSandboxExtensionIfNeeded() const
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

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
void AuxiliaryProcessProxy::setRunningBoardThrottlingEnabled()
{
    m_lifetimeActivity = nullptr;
}
#endif

} // namespace WebKit
