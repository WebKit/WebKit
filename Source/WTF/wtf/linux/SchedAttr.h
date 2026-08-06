/*
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#if OS(LINUX)

#include <stdint.h>

namespace WTF {

// Mirrors struct sched_attr from <linux/sched/types.h>, the argument of the sched_setattr and
// sched_getattr syscalls. We do not include that header because some versions of it also define
// struct sched_param, which clashes with the definition glibc provides in <sched.h>. glibc does
// not expose sched_attr itself, so the layout is replicated here. The member names deliberately
// match the kernel ones, so that the correspondence stays easy to check.
struct SchedAttr {
    uint32_t size;

    uint32_t sched_policy;
    uint64_t sched_flags;

    // SCHED_NORMAL, SCHED_BATCH
    int32_t sched_nice;

    // SCHED_FIFO, SCHED_RR
    uint32_t sched_priority;

    // SCHED_DEADLINE
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;

    // Utilization hints
    uint32_t sched_util_min;
    uint32_t sched_util_max;
};

// The kernel tells the versions of this structure apart by their size, so the layout must keep
// matching SCHED_ATTR_SIZE_VER1. Every member is naturally aligned, which makes the size the same
// on every architecture.
static_assert(sizeof(SchedAttr) == 56, "SchedAttr must match SCHED_ATTR_SIZE_VER1");

} // namespace WTF

using WTF::SchedAttr;

#endif // OS(LINUX)
