/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "ValueProfile.h"

#include "CCallHelpers.h"
#include "JSCInlines.h"

namespace JSC {

#if ENABLE(JIT)
void ResultProfile::emitDetectNumericness(CCallHelpers& jit, JSValueRegs regs, TagRegistersMode mode)
{
    CCallHelpers::Jump isInt32 = jit.branchIfInt32(regs, mode);
    CCallHelpers::Jump notDouble = jit.branchIfNotDoubleKnownNotInt32(regs, mode);
    // FIXME: We could be more precise here.
    emitSetDouble(jit);
    CCallHelpers::Jump done = jit.jump();
    notDouble.link(&jit);
    emitSetNonNumber(jit);
    done.link(&jit);
    isInt32.link(&jit);
}

void ResultProfile::emitSetDouble(CCallHelpers& jit)
{
    jit.or32(CCallHelpers::TrustedImm32(ResultProfile::Int32Overflow | ResultProfile::Int52Overflow | ResultProfile::NegZeroDouble | ResultProfile::NonNegZeroDouble), CCallHelpers::AbsoluteAddress(addressOfFlags()));
}

void ResultProfile::emitSetNonNumber(CCallHelpers& jit)
{
    jit.or32(CCallHelpers::TrustedImm32(ResultProfile::NonNumber), CCallHelpers::AbsoluteAddress(addressOfFlags()));
}
#endif // ENABLE(JIT)

} // namespace JSC

namespace WTF {
    
using namespace JSC;

void printInternal(PrintStream& out, const ResultProfile& profile)
{
    const char* separator = "";

    if (!profile.didObserveNonInt32()) {
        out.print("Int32");
        separator = "|";
    } else {
        if (profile.didObserveNegZeroDouble()) {
            out.print(separator, "NegZeroDouble");
            separator = "|";
        }
        if (profile.didObserveNonNegZeroDouble()) {
            out.print("NonNegZeroDouble");
            separator = "|";
        }
        if (profile.didObserveNonNumber()) {
            out.print("NonNumber");
            separator = "|";
        }
        if (profile.didObserveInt32Overflow()) {
            out.print("Int32Overflow");
            separator = "|";
        }
        if (profile.didObserveInt52Overflow()) {
            out.print("Int52Overflow");
            separator = "|";
        }
    }
    if (profile.specialFastPathCount()) {
        out.print(" special fast path: ");
        out.print(profile.specialFastPathCount());
    }
}

} // namespace WTF
