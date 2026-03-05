/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "Options.h"
#include <wtf/DataLog.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/text/ASCIILiteral.h>

namespace JSC { namespace B3 { namespace Air {

#define PHASE_STATS_MEMBER(name) unsigned name { 0 };
#define PHASE_STATS_PRINT(name) out.print("\n   " #name ": ", name);

#define DEFINE_PHASE_STATS(className, forEachMacro)                     \
    WTF_FORBID_HEAP_ALLOCATION;                                         \
    WTF_MAKE_NONCOPYABLE(className);                                    \
public:                                                                 \
    forEachMacro(PHASE_STATS_MEMBER)                                    \
                                                                        \
    void dump(PrintStream& out) const {                                 \
        forEachMacro(PHASE_STATS_PRINT)                                 \
    }                                                                   \
                                                                        \
    bool collectingStats() { return Options::airDumpPhaseStats(); }     \
                                                                        \
    ~className() {                                                      \
        if (collectingStats())                                          \
            dataLogLn(name(), " stats:", pointerDump(this));            \
    }                                                                   \

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
