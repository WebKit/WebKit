/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VNodeTracker.h"

#include "Logging.h"
#include <algorithm>
#include <sys/sysctl.h>
#include <sys/types.h>

namespace WebCore {

void VNodeTracker::platformInitialize()
{
    unsigned systemMaxVNodes = 0;
    size_t len = sizeof(systemMaxVNodes);

    // Query the maximum number of vnodes on the system and use 15% of that value as soft limit for this process,
    // and 20% of that value as hard limit.
    if (sysctlbyname("kern.maxvnodes", &systemMaxVNodes, &len, nullptr, 0))
        return;

    LOG(MemoryPressure, "System vnode limit is %u", systemMaxVNodes);

    m_softVNodeLimit = std::max(m_softVNodeLimit, static_cast<unsigned>(systemMaxVNodes * 0.15));
    m_hardVNodeLimit = std::max(m_hardVNodeLimit, static_cast<unsigned>(systemMaxVNodes * 0.2));
}

} // namespace WebCore
