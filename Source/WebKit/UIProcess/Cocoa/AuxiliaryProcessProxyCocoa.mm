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

#import "config.h"
#import "AuxiliaryProcessProxy.h"

#import "AuxiliaryProcessMessages.h"
#import "ProcessAssertion.h"
#import "XPCUtilities.h"
#import <WebCore/SharedBuffer.h>
#import <WebCore/WebMAudioUtilitiesCocoa.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <mach/task_info.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RunLoop.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
#import "LaunchLogMessages.h"
#import "LogStreamMessages.h"
#import "Logging.h"
#import "XPCEndpoint.h"
#import <wtf/OSObjectPtr.h>
#import <wtf/darwin/XPCExtras.h>
#import <wtf/darwin/XPCObjectPtr.h>
#import <wtf/spi/cocoa/OSLogSPI.h>
#endif

#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebKit {

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
static RetainPtr<CFDataRef> safeAudioComponentFetchServerRegistrations()
{
    SUPPRESS_UNRETAINED_LOCAL CFDataRef registrations { nullptr };
    if (PAL::AudioComponentFetchServerRegistrations(&registrations) != noErr || !registrations)
        return nullptr;
    return adoptCF(registrations);
}

RefPtr<WebCore::SharedBuffer> AuxiliaryProcessProxy::fetchAudioComponentServerRegistrations()
{
    using namespace PAL;

    if (!PAL::isAudioToolboxCoreFrameworkAvailable() || !PAL::canLoad_AudioToolboxCore_AudioComponentFetchServerRegistrations())
        return nullptr;
    
    WebCore::registerOpusDecoderIfNeeded();
    WebCore::registerVorbisDecoderIfNeeded();

    auto registrations = safeAudioComponentFetchServerRegistrations();
    if (!registrations)
        return nullptr;

    return WebCore::SharedBuffer::create(registrations.get());
}
#endif

Vector<String> AuxiliaryProcessProxy::platformOverrideLanguages() const
{
    static const NeverDestroyed<Vector<String>> overrideLanguages = makeVector<String>(retainPtr([[NSUserDefaults standardUserDefaults] stringArrayForKey:@"AppleLanguages"]).get());
    return overrideLanguages;
}

// This may be called from the AuxiliaryProcessProxy destructor, so do not pass the `this` pointer
// to other functions, or call any virtual functions.
void AuxiliaryProcessProxy::platformStartConnectionTerminationWatchdog()
{
#if USE(RUNNINGBOARD)
    if (m_startedTerminationWatchdog)
        return;

    m_startedTerminationWatchdog = true;

    // Deploy a watchdog in the UI process, since the child process may be suspended.
    // If 30s is insufficient for any outstanding activity to complete cleanly, then it will be killed.
    static constexpr ASCIILiteral reason = "XPCConnectionTerminationWatchdog"_s;

#if USE(EXTENSIONKIT_PROCESS_TERMINATION)
    auto maybeExtensionProcess = extensionProcess();
    if (!maybeExtensionProcess)
        return;

    Ref assertion = ProcessAndUIAssertion::create(processID(), reason, ProcessAssertionType::Background, environmentIdentifier(), extensionProcess());
    auto terminationHandler = [assertion = WTF::move(assertion), extensionProcess = WTF::move(*maybeExtensionProcess)] {
        extensionProcess.invalidate();
    };
#else
    if (!m_connection)
        return;

    OSObjectPtr<xpc_connection_t> xpcConnection = m_connection->xpcConnection();
    if (!xpcConnection)
        return;

    Ref assertion = ProcessAndUIAssertion::create(processID(), reason, ProcessAssertionType::Background, environmentIdentifier());
    auto terminationHandler = [assertion = WTF::move(assertion), xpcConnection = WTF::move(xpcConnection)]() {
        terminateWithReason(xpcConnection.get(), ReasonCode::WatchdogTimerFired, reason);
    };
#endif

    RunLoop::mainSingleton().dispatchAfter(30_s, WTF::move(terminationHandler));
#endif // USE(RUNNINGBOARD)
}

#if USE(EXTENSIONKIT)
std::optional<ExtensionProcess> AuxiliaryProcessProxy::extensionProcess() const
{
    if (!m_processLauncher)
        return std::nullopt;
    return m_processLauncher->extensionProcess();
}

LaunchGrant* AuxiliaryProcessProxy::launchGrant() const
{
    return m_processLauncher ? m_processLauncher->launchGrant() : nullptr;
}
#endif

std::optional<AuxiliaryProcessProxy::TaskInfo> AuxiliaryProcessProxy::taskInfo() const
{
    auto pid = processID();
    if (!pid)
        return std::nullopt;

    mach_port_t task = MACH_PORT_NULL;
    if (task_name_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS)
        return std::nullopt;

    auto scope = makeScopeExit([task]() {
        mach_port_deallocate(mach_task_self(), task);
    });

    mach_task_basic_info_data_t basicInfo;
    mach_msg_type_number_t basicInfoCount = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(task, MACH_TASK_BASIC_INFO, (task_info_t)&basicInfo, &basicInfoCount) != KERN_SUCCESS)
        return std::nullopt;

    task_absolutetime_info_data_t timeInfo;
    mach_msg_type_number_t timeInfoCount = TASK_ABSOLUTETIME_INFO_COUNT;

    if (task_info(task, TASK_ABSOLUTETIME_INFO, (task_info_t)&timeInfo, &timeInfoCount) != KERN_SUCCESS)
        return std::nullopt;

    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t vmInfoCount = TASK_VM_INFO_REV1_COUNT;

    if (task_info(task, TASK_VM_INFO, (task_info_t)&vmInfo, &vmInfoCount) != KERN_SUCCESS)
        return std::nullopt;

    // ProcessThrottler's "suspend" state is a bit of a misnomer, because it could mean that the
    // process either is holding the "Suspended" assertion (in which case it's actually still not
    // task_suspended), or it is holding no assertions (in which case it's actually suspended).
    // Also, other processes (like NetworkProcess) can acquire assertions on the process that
    // UIProcess doesn't know about and which prevent the process from suspending.
    //
    // So we only tell the client that the task is suspended if we're sure that the task is actually
    // suspended (by consulting suspend_count).
    auto state = throttler().currentState();
    if (state == ProcessThrottleState::Suspended && !basicInfo.suspend_count)
        state = ProcessThrottleState::Background;

    return TaskInfo {
        pid,
        state,
        MonotonicTime::fromMachAbsoluteTime(timeInfo.total_user).secondsSinceEpoch(),
        MonotonicTime::fromMachAbsoluteTime(timeInfo.total_system).secondsSinceEpoch(),
        static_cast<size_t>(vmInfo.phys_footprint)
    };
}

#if ENABLE(CFPREFS_DIRECT_MODE)
void AuxiliaryProcessProxy::notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    send(Messages::AuxiliaryProcess::PreferenceDidUpdate(domain, key, encodedValue), 0);
}
#endif

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
AuxiliaryProcessProxy::LogXPCEventHandler::LogXPCEventHandler(const AuxiliaryProcessProxy& process)
    : m_process(process)
{
}

bool AuxiliaryProcessProxy::LogXPCEventHandler::handleXPCEvent(xpc_object_t event)
{
    auto messageName = xpcDictionaryGetString(event, XPCEndpoint::xpcMessageNameKey);
    if (messageName == logMessageName) {
        RefPtr process = m_process.get();
        if (!process)
            return true;

        MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(m_logEndpointEnabled, process->connection(), false);

        auto subsystem = xpcDictionaryGetString(event, subsystemKey);
        auto category = xpcDictionaryGetString(event, categoryKey);
        auto messageString = xpcDictionaryGetString(event, messageStringKey);
        auto logType = xpc_dictionary_get_uint64(event, logTypeKey);
        auto pid = xpc_connection_get_pid(protect(xpc_dictionary_get_remote_connection(event)));

        OSObjectPtr<os_log_t> osLog;
        if (!subsystem.isEmpty() && !category.isEmpty())
            osLog = adoptOSObject(os_log_create(subsystem.utf8().data(), category.utf8().data()));

        process->didReceiveLogsDuringLaunchForTesting();
        logWithProcessNamePrefix(osLog ? osLog.get() : OS_LOG_DEFAULT, static_cast<os_log_type_t>(logType), process->processName(), static_cast<int>(pid), messageString.utf8().data());
    } else if (messageName == disableLogMessageName) {
        RefPtr process = m_process.get();
        if (!process)
            return true;
        m_logEndpointEnabled = false;
        RELEASE_LOG(Process, "Log endpoint is disabled");
    }
    return false;
}

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
void AuxiliaryProcessProxy::createLogStream(IPC::StreamServerConnectionHandle&& serverConnection, LogStreamIdentifier identifier, CompletionHandler<void(IPC::Semaphore& streamWakeUpSemaphore, IPC::Semaphore& streamClientWaitSemaphore)>&& completionHandler)
{
    MESSAGE_CHECK_BASE(!m_logStream.get(), connection());
    m_logStream = LogStream::create(*this, processName(), WTF::move(serverConnection), identifier, WTF::move(completionHandler));
}
#else
void AuxiliaryProcessProxy::createLogStream(LogStreamIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK_BASE(!m_logStream.get(), connection());
    Ref logStream = LogStream::create(*this, processName(), protect(connection()), identifier);
    addMessageReceiver(Messages::LogStream::messageReceiverName(), logStream->identifier(), logStream);
    m_logStream = WTF::move(logStream);
    completionHandler();
}
#endif

void AuxiliaryProcessProxy::stopLogStream()
{
    if (!m_logStream.get())
        return;
#if !ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    removeMessageReceiver(Messages::LogStream::messageReceiverName(), m_logStream->identifier());
#endif
    m_logStream.reset();
}
#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

} // namespace WebKit
