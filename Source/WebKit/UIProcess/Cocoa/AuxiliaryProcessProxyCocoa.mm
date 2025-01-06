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
#import <WebCore/SharedBuffer.h>
#import <WebCore/WebMAudioUtilitiesCocoa.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <mach/task_info.h>
#import <wtf/MonotonicTime.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/VectorCocoa.h>

#if USE(RUNNINGBOARD)
#include "XPCConnectionTerminationWatchdog.h"
#endif

#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebKit {

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
RefPtr<WebCore::SharedBuffer> AuxiliaryProcessProxy::fetchAudioComponentServerRegistrations()
{
    using namespace PAL;

    CFDataRef registrations { nullptr };

    if (!PAL::isAudioToolboxCoreFrameworkAvailable() || !PAL::canLoad_AudioToolboxCore_AudioComponentFetchServerRegistrations())
        return nullptr;
    
    WebCore::registerOpusDecoderIfNeeded();
    WebCore::registerVorbisDecoderIfNeeded();

    if (noErr != AudioComponentFetchServerRegistrations(&registrations) || !registrations)
        return nullptr;

    return WebCore::SharedBuffer::create(adoptCF(registrations).get());
}
#endif

Vector<String> AuxiliaryProcessProxy::platformOverrideLanguages() const
{
    static const NeverDestroyed<Vector<String>> overrideLanguages = makeVector<String>([[NSUserDefaults standardUserDefaults] valueForKey:@"AppleLanguages"]);
    return overrideLanguages;
}

void AuxiliaryProcessProxy::platformStartConnectionTerminationWatchdog()
{
#if USE(RUNNINGBOARD)
    // Deploy a watchdog in the UI process, since the child process may be suspended.
    // If 30s is insufficient for any outstanding activity to complete cleanly, then it will be killed.
    ASSERT(m_connection && m_connection->xpcConnection());
    XPCConnectionTerminationWatchdog::startConnectionTerminationWatchdog(*this, 30_s);
#endif
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

} // namespace WebKit
