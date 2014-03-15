/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SystemMemory.h"

#include <mach/mach.h>
#include <sys/sysctl.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>

namespace WebCore {

int systemMemoryLevel()
{
#if PLATFORM(IOS_SIMULATOR)
    return 35;
#else
    static int memoryFreeLevel = -1;
    static double previousCheckTime; 
    double time = currentTime();
    if (time - previousCheckTime < 0.1)
        return memoryFreeLevel;
    previousCheckTime = time;
    size_t size = sizeof(memoryFreeLevel);
    sysctlbyname("kern.memorystatus_level", &memoryFreeLevel, &size, nullptr, 0);
    return memoryFreeLevel;
#endif
}

#if !PLATFORM(IOS_SIMULATOR)
static host_basic_info_data_t gHostBasicInfo;

static void initCapabilities(void)
{
    // Discover our CPU type
    mach_port_t host = mach_host_self();
    mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
    kern_return_t returnValue = host_info(host, HOST_BASIC_INFO, reinterpret_cast<host_info_t>(&gHostBasicInfo), &count);
    mach_port_deallocate(mach_task_self(), host);
    if (returnValue != KERN_SUCCESS)
        LOG_ERROR("%s : host_info(%d) : %s.\n", __FUNCTION__, returnValue, mach_error_string(returnValue));
}
#endif

size_t systemTotalMemory()
{
#if PLATFORM(IOS_SIMULATOR)
    return 512 * 1024 * 1024;
#else
    // FIXME: Consider using C++11 thread primitives.
    static pthread_once_t initControl = PTHREAD_ONCE_INIT;

    pthread_once(&initControl, initCapabilities);
    // The value in gHostBasicInfo.max_mem is often lower than the amount we're
    // interested in (e.g., does this device have at least 256MB of RAM?)
    // Round the value to the nearest power of 2.
    return static_cast<size_t>(exp2(ceil(log2(gHostBasicInfo.max_mem))));
#endif
}

} // namespace WebCore
