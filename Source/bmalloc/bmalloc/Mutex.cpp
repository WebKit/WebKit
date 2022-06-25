/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "Mutex.h"

#include "ScopeExit.h"
#if BOS(DARWIN)
#include <mach/mach_traps.h>
#include <mach/thread_switch.h>
#endif
#include <thread>

namespace bmalloc {

static inline void yield()
{
#if BOS(DARWIN)
    constexpr mach_msg_timeout_t timeoutInMS = 1;
    thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, timeoutInMS);
#else
    sched_yield();
#endif
}

void Mutex::lockSlowCase()
{
    // The longest critical section in bmalloc is much shorter than the
    // time it takes to make a system call to yield to the OS scheduler.
    // So, we try again a lot before we yield.
    static constexpr size_t aLot = 256;
    
    if (!m_isSpinning.exchange(true)) {
        auto clear = makeScopeExit([&] { m_isSpinning.store(false); });

        for (size_t i = 0; i < aLot; ++i) {
            if (try_lock())
                return;
        }
    }

    // Avoid spinning pathologically.
    while (!try_lock())
        yield();
}

} // namespace bmalloc
