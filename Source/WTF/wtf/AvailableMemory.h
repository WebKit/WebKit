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

#pragma once

#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

WTF_EXPORT_PRIVATE size_t availableMemory();

#if PLATFORM(IOS_FAMILY) || OS(LINUX) || OS(FREEBSD)
struct MemoryStatus {
    MemoryStatus(size_t memoryFootprint, double percentAvailableMemoryInUse)
        : memoryFootprint(memoryFootprint)
        , percentAvailableMemoryInUse(percentAvailableMemoryInUse)
    {
    }

    size_t memoryFootprint;
    double percentAvailableMemoryInUse;
};

WTF_EXPORT_PRIVATE MemoryStatus memoryStatus();

inline double percentAvailableMemoryInUse()
{
    auto memoryUse = memoryStatus();
    return memoryUse.percentAvailableMemoryInUse;
}
#endif

inline bool isUnderMemoryPressure()
{
#if PLATFORM(IOS_FAMILY) || OS(LINUX) || OS(FREEBSD)
    constexpr double memoryPressureThreshold = 0.75;
    return percentAvailableMemoryInUse() > memoryPressureThreshold;
#else
    return false;
#endif
}

} // namespace WTF
