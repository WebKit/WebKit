/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "AvailableMemory.h"

#include "Environment.h"
#if BPLATFORM(IOS_FAMILY)
#include "MemoryStatusSPI.h"
#endif
#include "PerProcess.h"
#include "Scavenger.h"
#include "Sizes.h"
#include <array>
#include <mutex>
#if BOS(DARWIN)
#if BPLATFORM(IOS_FAMILY)
#import <algorithm>
#endif
#import <dispatch/dispatch.h>
#import <mach/host_info.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <math.h>
#elif BOS(UNIX)
#if BOS(FREEBSD) || BOS(LINUX)
#include <sys/sysinfo.h>
#endif
#if BOS(LINUX)
#include <algorithm>
#include <fcntl.h>
#elif BOS(FREEBSD)
#include "VMAllocate.h"
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#endif
#include <unistd.h>
#endif

namespace bmalloc {

static constexpr size_t availableMemoryGuess = 512 * bmalloc::MB;

#if BOS(DARWIN)
static size_t memorySizeAccordingToKernel()
{
#if BPLATFORM(IOS_FAMILY_SIMULATOR)
    BUNUSED_PARAM(availableMemoryGuess);
    // Pretend we have 1024MB of memory to make cache sizes behave like on device.
    return 1024 * bmalloc::MB;
#else
    host_basic_info_data_t hostInfo;

    mach_port_t host = mach_host_self();
    mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
    kern_return_t r = host_info(host, HOST_BASIC_INFO, (host_info_t)&hostInfo, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (r != KERN_SUCCESS)
        return availableMemoryGuess;

    if (hostInfo.max_mem > std::numeric_limits<size_t>::max())
        return std::numeric_limits<size_t>::max();

    return static_cast<size_t>(hostInfo.max_mem);
#endif
}
#endif

#if BPLATFORM(IOS_FAMILY)
static size_t jetsamLimit()
{
    memorystatus_memlimit_properties_t properties;
    pid_t pid = getpid();
    if (memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES, pid, 0, &properties, sizeof(properties)))
        return 840 * bmalloc::MB;
    if (properties.memlimit_active < 0)
        return std::numeric_limits<size_t>::max();
    return static_cast<size_t>(properties.memlimit_active) * bmalloc::MB;
}
#endif

#if BOS(LINUX)
struct LinuxMemory {
    static const LinuxMemory& singleton()
    {
        static LinuxMemory s_singleton;
        static std::once_flag s_onceFlag;
        std::call_once(s_onceFlag,
            [] {
                s_singleton.pageSize = sysconf(_SC_PAGE_SIZE);
                s_singleton.statmFd = open("/proc/self/statm", O_RDONLY | O_CLOEXEC);
            });
        return s_singleton;
    }

    size_t footprint() const
    {
        if (statmFd == -1)
            return 0;

        std::array<char, 256> statmBuffer;
        ssize_t numBytes = pread(statmFd, statmBuffer.data(), statmBuffer.size(), 0);
        if (numBytes <= 0)
            return 0;

        std::array<char, 32> rssBuffer;
        {
            auto begin = std::find(statmBuffer.begin(), statmBuffer.end(), ' ');
            if (begin == statmBuffer.end())
                return 0;

            std::advance(begin, 1);
            auto end = std::find(begin, statmBuffer.end(), ' ');
            if (end == statmBuffer.end())
                return 0;

            auto last = std::copy_n(begin, std::min<size_t>(31, std::distance(begin, end)), rssBuffer.begin());
            *last = '\0';
        }

        unsigned long dirtyPages = strtoul(rssBuffer.data(), nullptr, 10);
        return dirtyPages * pageSize;
    }

    long pageSize { 0 };
    int statmFd { -1 };
};
#endif

static size_t computeAvailableMemory()
{
#if BOS(DARWIN)
    size_t sizeAccordingToKernel = memorySizeAccordingToKernel();
#if BPLATFORM(IOS_FAMILY)
    sizeAccordingToKernel = std::min(sizeAccordingToKernel, jetsamLimit());
#endif
    size_t multiple = 128 * bmalloc::MB;

    // Round up the memory size to a multiple of 128MB because max_mem may not be exactly 512MB
    // (for example) and we have code that depends on those boundaries.
    return ((sizeAccordingToKernel + multiple - 1) / multiple) * multiple;
#elif BOS(FREEBSD) || BOS(LINUX)
    struct sysinfo info;
    if (!sysinfo(&info))
        return info.totalram * info.mem_unit;
    return availableMemoryGuess;
#elif BOS(UNIX)
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages == -1 || pageSize == -1)
        return availableMemoryGuess;
    return pages * pageSize;
#else
    return availableMemoryGuess;
#endif
}

size_t availableMemory()
{
    static size_t availableMemory;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        availableMemory = computeAvailableMemory();
    });
    return availableMemory;
}

#if BPLATFORM(IOS_FAMILY) || BOS(LINUX) || BOS(FREEBSD)
MemoryStatus memoryStatus()
{
#if BPLATFORM(IOS_FAMILY)
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t vmSize = TASK_VM_INFO_COUNT;
    
    size_t memoryFootprint = 0;
    if (KERN_SUCCESS == task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)(&vmInfo), &vmSize))
        memoryFootprint = static_cast<size_t>(vmInfo.phys_footprint);
#elif BOS(LINUX)
    auto& memory = LinuxMemory::singleton();
    size_t memoryFootprint = memory.footprint();
#elif BOS(FREEBSD)
    struct kinfo_proc info;
    size_t infolen = sizeof(info);

    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    size_t memoryFootprint = 0;
    if (!sysctl(mib, 4, &info, &infolen, nullptr, 0))
        memoryFootprint = static_cast<size_t>(info.ki_rssize) * vmPageSize();
#endif

    double percentInUse = static_cast<double>(memoryFootprint) / static_cast<double>(availableMemory());
    double percentAvailableMemoryInUse = std::min(percentInUse, 1.0);
    return MemoryStatus(memoryFootprint, percentAvailableMemoryInUse);
}
#endif

} // namespace bmalloc
