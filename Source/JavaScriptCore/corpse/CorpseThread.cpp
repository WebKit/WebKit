/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CorpseThread.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include "CorpseError.h"
#include "CorpseProcess.h"
#include "CorpseRegion.h"
#include "CorpseSnapshot.h"

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#include <mach/thread_act.h>
#include <mach/thread_info.h>
#include <mach/thread_status.h>
#include <optional>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Corpse {

static std::optional<Address> readStackPointer(thread_act_t thread)
{
#if CPU(ARM64)
    arm_thread_state64_t state = { };
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    if (thread_get_state(thread, ARM_THREAD_STATE64, reinterpret_cast<thread_state_t>(&state), &count) != KERN_SUCCESS)
        return std::nullopt; // May fail e.g. for Rosetta.
    // The target's pc/lr/sp/fp arrive signed for its own ptrauth context. On an
    // arm64e build the accessors would try to authenticate them against ours and
    // trap (EXC_BAD_ACCESS / EXC_ARM_PAC_FAIL), so strip the signatures first.
    // Stripping also marks the state as unsigned, so the accessor reads it raw.
    arm_thread_state64_ptrauth_strip(state);
    return Address(arm_thread_state64_get_sp(state));
#elif CPU(X86_64)
    x86_thread_state64_t state = { };
    mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
    if (thread_get_state(thread, x86_THREAD_STATE64, reinterpret_cast<thread_state_t>(&state), &count) != KERN_SUCCESS)
        return std::nullopt;
    return Address(state.__rsp);
#else
    UNUSED_PARAM(thread);
    return std::nullopt;
#endif
}

const char* Thread::runStateDescription() const
{
    switch (m_runState) {
    case TH_STATE_RUNNING:
        return "running";
    case TH_STATE_STOPPED:
        return "stopped";
    case TH_STATE_WAITING:
        return "waiting";
    case TH_STATE_UNINTERRUPTIBLE:
        return "uninterruptible";
    case TH_STATE_HALTED:
        return "halted";
    default:
        return "unknown";
    }
}

Vector<Thread> Thread::collect(const Snapshot& snapshot)
{
    Vector<Thread> result;

    if (!snapshot.isValid()) {
        Error::report("Cannot read threads from an invalid snapshot");
        return result;
    }
    mach_port_t task = snapshot.corpsePort();

    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t threadCount = 0;
    kern_return_t kr = task_threads(task, &threads, &threadCount);
    if (kr != KERN_SUCCESS) {
        pid_t pid = snapshot.process()->pid();
        Error::report("Could not read the thread list for pid %d: %s (0x%x)",
            static_cast<int>(pid), mach_error_string(kr), kr);
        return result;
    }

    result.reserveCapacity(threadCount);

    // A translated target executes as arm64 whatever its own architecture is, so its
    // threads' stack pointers belong to Rosetta's runtime rather than to the program.
    // Those addresses do land in real mappings, so reporting the region around one
    // would name a plausible but wrong stack; report no stack instead.
    Process* process = snapshot.process();
    bool isTranslated = process->isTranslated();
    if (isTranslated) {
        Error::report("Thread stacks for pid %d are not available: the process runs"
            " under Rosetta translation, whose thread state does not describe the program",
            static_cast<int>(process->pid()));
    }

    unsigned unreadableStates = 0;
    for (mach_msg_type_number_t i = 0; i < threadCount; ++i) {
        Thread thread;

        thread_identifier_info_data_t identifierInfo;
        mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
        if (thread_info(threads[i], THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&identifierInfo), &count) == KERN_SUCCESS)
            thread.m_id = identifierInfo.thread_id;

        thread_basic_info_data_t basicInfo;
        count = THREAD_BASIC_INFO_COUNT;
        if (thread_info(threads[i], THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&basicInfo), &count) == KERN_SUCCESS) {
            thread.m_runState = basicInfo.run_state;
            thread.m_suspendCount = basicInfo.suspend_count;
            thread.m_userTimeUsec = static_cast<uint64_t>(basicInfo.user_time.seconds) * 1000000
                + basicInfo.user_time.microseconds;
            thread.m_systemTimeUsec = static_cast<uint64_t>(basicInfo.system_time.seconds) * 1000000
                + basicInfo.system_time.microseconds;
        }

        // Extended info is the only flavor that reports the pthread name.
        thread_extended_info_data_t extendedInfo;
        count = THREAD_EXTENDED_INFO_COUNT;
        if (thread_info(threads[i], THREAD_EXTENDED_INFO, reinterpret_cast<thread_info_t>(&extendedInfo), &count) == KERN_SUCCESS) {
            extendedInfo.pth_name[sizeof(extendedInfo.pth_name) - 1] = '\0';
            thread.m_name = extendedInfo.pth_name;
        }

        // The stack is the region the stack pointer points into.
        if (!isTranslated) {
            if (auto stackPointer = readStackPointer(threads[i])) {
                thread.m_stackPointer = *stackPointer;
                if (auto region = Region::findContaining(task, thread.m_stackPointer))
                    thread.m_stackRegion = *region;
            } else
                ++unreadableStates;
        }

        result.append(thread);
    }

    // Failing to read the thread state leaves a thread with no stack, which on its
    // own looks the same as a thread that has none. Say which it was.
    if (unreadableStates) {
        Error::report("Could not read the thread state of %u of %u threads in pid %d:"
            " this build cannot read the target's architecture",
            unreadableStates, static_cast<unsigned>(threadCount),
            static_cast<int>(process->pid()));
    }

    // task_threads hands us a right to each thread plus the array itself.
    for (mach_msg_type_number_t i = 0; i < threadCount; ++i)
        mach_port_deallocate(mach_task_self(), threads[i]);
    mach_vm_size_t threadsSize = threadCount * sizeof(thread_act_t);
    mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(threads), threadsSize);

    return result;
}
} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
