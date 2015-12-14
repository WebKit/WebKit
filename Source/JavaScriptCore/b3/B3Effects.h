/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef B3Effects_h
#define B3Effects_h

#if ENABLE(B3_JIT)

#include "B3HeapRange.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace B3 {

struct Effects {
    // True if this cannot continue execution in the current block.
    bool terminal { false };

    // True if this value can cause execution to terminate abruptly, and that this abrupt termination is
    // observable. An example of how this gets used is to limit the hoisting of controlDependent values.
    // Note that if exitsSideways is set to true but reads is bottom, then B3 is free to assume that
    // after abrupt termination of this procedure, none of the heap will be read. That's usually false,
    // so make sure that reads corresponds to the set of things that are readable after this function
    // terminates abruptly.
    bool exitsSideways { false };

    // True if the instruction may change semantics if hoisted above some control flow.
    bool controlDependent { false };

    // True if this writes to the SSA state. Operations that write SSA state don't write to anything
    // in "memory" but they have a side-effect anyway. This is for modeling Upsilons. You can ignore
    // this if you have your own way of modeling Upsilons or if you intend to just rebuild them
    // anyway.
    bool writesSSAState { false };

    // True if this reads from the SSA state. This is only used for Phi.
    bool readsSSAState { false };

    HeapRange writes;
    HeapRange reads;

    static Effects none()
    {
        return Effects();
    }

    static Effects forCall()
    {
        Effects result;
        result.exitsSideways = true;
        result.controlDependent = true;
        result.writes = HeapRange::top();
        result.reads = HeapRange::top();
        return result;
    }

    bool mustExecute() const
    {
        return terminal || exitsSideways || writesSSAState || writes;
    }

    // Returns true if reordering instructions with these respective effects would change program
    // behavior in an observable way.
    bool interferes(const Effects&) const;

    void dump(PrintStream& out) const;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Effects_h

