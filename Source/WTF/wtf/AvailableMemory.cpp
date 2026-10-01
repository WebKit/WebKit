/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#include <wtf/AvailableMemory.h>

#include <wtf/MathExtras.h>
#include <wtf/MemoryFootprint.h>

#if OS(DARWIN)
#include <mach/mach.h>
#include <wtf/RAMSize.h>
#elif OS(UNIX)
#if OS(FREEBSD) || OS(LINUX)
#include <sys/sysinfo.h>
#endif
#include <unistd.h>
#elif OS(WINDOWS)
#include <windows.h>
#endif

namespace WTF {

#if PLATFORM(WATCHOS)
static constexpr size_t availableMemoryGuess = 120 * MB;
#elif PLATFORM(APPLETV)
static constexpr size_t availableMemoryGuess = 840 * MB;
#else
static constexpr size_t availableMemoryGuess = 1536 * MB;
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
static std::optional<size_t> jetsamLimit()
{
    // Jetsam limits can differ based on whether a process is in the foreground or the background.
    // This computation isn't currently affected because WebKit processes currently use the same
    // limit for FG and BG.
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &count) != KERN_SUCCESS)
        return std::nullopt;
    if (count < TASK_VM_INFO_REV4_COUNT || !vmInfo.limit_bytes_remaining)
        return std::nullopt;
    return static_cast<size_t>(vmInfo.phys_footprint + vmInfo.limit_bytes_remaining);
}
#endif

static size_t computeAvailableMemory()
{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    // Pretend we have a device-like amount of memory to make cache sizes behave like on device.
    return availableMemoryGuess;
#elif OS(DARWIN)
    size_t sizeAccordingToKernel = ramSizeDisregardingJetsamLimit();
    if (!sizeAccordingToKernel)
        return availableMemoryGuess;
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    sizeAccordingToKernel = std::min(sizeAccordingToKernel, jetsamLimit().value_or(availableMemoryGuess));
#endif

    // Round up the memory size to a multiple of 128MB because max_mem may not be exactly 512MB
    // (for example) and we have code that depends on those boundaries.
    return roundUpToMultipleOf<128 * MB>(sizeAccordingToKernel);
#elif OS(FREEBSD) || OS(LINUX)
    struct sysinfo info;
    if (!sysinfo(&info))
        return info.totalram * info.mem_unit;
    return availableMemoryGuess;
#elif OS(UNIX) || OS(HAIKU)
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages == -1 || pageSize == -1)
        return availableMemoryGuess;
    return pages * pageSize;
#elif OS(WINDOWS)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    bool result = GlobalMemoryStatusEx(&status);
    if (!result)
        return availableMemoryGuess;
    return status.ullTotalPhys;
#else
    return availableMemoryGuess;
#endif
}

size_t availableMemory()
{
    static size_t availableMemory = computeAvailableMemory();
    return availableMemory;
}

double percentAvailableMemoryInUse()
{
    return std::min(1.0, static_cast<double>(memoryFootprint()) / static_cast<double>(availableMemory()));
}

} // namespace WTF
