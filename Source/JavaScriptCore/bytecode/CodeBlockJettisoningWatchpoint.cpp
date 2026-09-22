/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "CodeBlockJettisoningWatchpoint.h"

#include "CodeBlockInlines.h"
#include "DFGCommon.h"
#include "Options.h"

namespace JSC {

void CodeBlockJettisoningWatchpoint::fireInternal(VM&, const FireDetail& detail)
{
    ASSERT(!m_owner->wasDestructed());
    // If CodeBlock is no longer live, we do not fire it.
    // This works since CodeBlock is the owner of this watchpoint. When it gets destroyed, then this watchpoint also gets destroyed.
    // Only problematic case is, (1) CodeBlock is dead, but (2) destructor is not called yet.
    if (m_owner->isPendingDestruction())
        return;

    if (DFG::shouldDumpDisassembly())
        dataLog("Firing watchpoint ", RawPointer(this), " on ", *m_owner, "\n");

    // MEASUREMENT SWITCH (2026-08-22): counting the reoptimization raises this function's compile threshold, so
    // repeated watchpoint jettisons push it to colder tiers. For a field-type withdrawal that penalty is arguably
    // wrong on the merits -- the withdrawal is an external event (a store elsewhere contradicted the claim), and
    // claims are MONOTONE, so the recompile provably cannot reuse the claim and has nothing to learn. This flag
    // relaxes it for EVERY watchpoint kind, which makes it an upper bound rather than a targeted fix: if the global
    // relaxation does not help, a field-type-specific one cannot either.
    m_owner->jettison(Profiler::JettisonDueToUnprofiledWatchpoint,
        Options::countReoptimizationOnWatchpointJettison() ? CountReoptimization : DontCountReoptimization, &detail);
}

} // namespace JSC

