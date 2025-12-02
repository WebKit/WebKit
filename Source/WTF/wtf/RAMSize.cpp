/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include <wtf/RAMSize.h>

#include <mutex>

#if OS(WINDOWS)
#include <windows.h>
#elif USE(SYSTEM_MALLOC)
#if OS(LINUX) || OS(FREEBSD)
#include <sys/sysinfo.h>
#elif OS(UNIX) || OS(HAIKU)
#include <unistd.h>
#endif // OS(LINUX) || OS(FREEBSD) || OS(UNIX) || OS(HAIKU)
#else
#include <bmalloc/bmalloc.h>
#endif

#if OS(DARWIN)
#include <mach/mach.h>
#endif

namespace WTF {

#if OS(WINDOWS)
static constexpr size_t ramSizeGuess = 512 * MB;
#endif

static size_t computeRAMSize()
{
#if OS(WINDOWS)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    bool result = GlobalMemoryStatusEx(&status);
    if (!result)
        return ramSizeGuess;
    return status.ullTotalPhys;
#elif USE(SYSTEM_MALLOC)
#if OS(LINUX) || OS(FREEBSD)
    struct sysinfo si;
    sysinfo(&si);
    return si.totalram * si.mem_unit;
#elif OS(UNIX) || OS(HAIKU)
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    return pages * pageSize;
#else
#error "Missing a platform specific way of determining the available RAM"
#endif // OS(LINUX) || OS(FREEBSD) || OS(UNIX) || OS(HAIKU)
#else
    return bmalloc::api::availableMemory();
#endif
}

size_t ramSize()
{
    static size_t ramSize = computeRAMSize();
    return ramSize;
}

#if OS(DARWIN)
size_t ramSizeDisregardingJetsamLimit()
{
    host_basic_info_data_t hostInfo;

    mach_port_t host = mach_host_self();
    mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
    kern_return_t r = host_info(host, HOST_BASIC_INFO, (host_info_t)&hostInfo, &count);
    if (mach_port_deallocate(mach_task_self(), host) != KERN_SUCCESS)
        return 0;
    if (r != KERN_SUCCESS)
        return 0;

    if (hostInfo.max_mem > std::numeric_limits<size_t>::max())
        return std::numeric_limits<size_t>::max();

    return static_cast<size_t>(hostInfo.max_mem);
}
#endif

} // namespace WTF
