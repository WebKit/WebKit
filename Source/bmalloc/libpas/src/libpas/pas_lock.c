/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_lock.h"
#if PAS_OS(DARWIN)
#include <mach/mach_traps.h>
#include <mach/thread_switch.h>
#endif

#if PAS_USE_SPINLOCKS

PAS_NEVER_INLINE void pas_lock_lock_slow(pas_lock* lock)
{
    static const size_t a_lot = 256;

    if (pas_compare_and_swap_bool_strong(&lock->is_spinning, false, true)) {
        size_t index;
        bool did_acquire;

        did_acquire = false;

        for (index = a_lot; index--;) {
            if (!pas_compare_and_swap_bool_strong(&lock->lock, false, true)) {
                did_acquire = true;
                break;
            }
        }

        lock->is_spinning = false;

        if (did_acquire)
            return;
    }

    while (!pas_compare_and_swap_bool_weak(&lock->lock, false, true)) {
#if PAS_OS(DARWIN)
        const mach_msg_timeout_t timeoutInMS = 1;
        thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, timeoutInMS);
#else
        sched_yield();
#endif
    }
}

#endif /* PAS_USE_SPINLOCKS */

#endif /* LIBPAS_ENABLED */
