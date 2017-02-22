/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#include "MemoryMeasure.h"

#include <stdio.h>
#include <mach/mach.h>

static const int measureMemoryFailed = -1;

namespace WebKit {

bool MemoryMeasure::m_isLoggingEnabled;

void MemoryMeasure::enableLogging(bool enabled)
{
    m_isLoggingEnabled = enabled;
}

bool MemoryMeasure::isLoggingEnabled()
{
    return m_isLoggingEnabled;
}

long MemoryMeasure::taskMemory()
{
    if (!MemoryMeasure::isLoggingEnabled())
        return 0;

    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t err = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &vmInfo, &count);
    if (err != KERN_SUCCESS)
        return measureMemoryFailed;

    return (long) vmInfo.internal;
}

MemoryMeasure::~MemoryMeasure()
{
    if (!MemoryMeasure::isLoggingEnabled())
        return;

    long currentMemory = taskMemory();
    if (currentMemory == measureMemoryFailed || m_initialMemory == measureMemoryFailed) {
        NSLog(@"%s (Unable to get dirty memory information for process)\n", m_logString);
        return;
    }

    long memoryDiff = currentMemory - m_initialMemory;
    if (memoryDiff < 0)
        NSLog(@"%s Dirty memory reduced by %ld bytes (from %ld to %ld)\n", m_logString, (memoryDiff * -1), m_initialMemory, currentMemory);
    else if (memoryDiff > 0)
        NSLog(@"%s Dirty memory increased by %ld bytes (from %ld to %ld)\n", m_logString, memoryDiff, m_initialMemory, currentMemory);
    else
        NSLog(@"%s No change in dirty memory used by process (at %ld bytes)\n", m_logString, currentMemory);
}

}

#endif // PLATFORM(IOS)
