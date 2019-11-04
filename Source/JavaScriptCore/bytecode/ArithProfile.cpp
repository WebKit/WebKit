/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ArithProfile.h"

#include "CCallHelpers.h"
#include "JSCInlines.h"

namespace JSC {

#if ENABLE(JIT)
void ArithProfile::emitObserveResult(CCallHelpers& jit, JSValueRegs regs, TagRegistersMode mode)
{
    if (!shouldEmitSetDouble() && !shouldEmitSetNonNumeric() && !shouldEmitSetBigInt())
        return;

    CCallHelpers::JumpList done;
    CCallHelpers::JumpList nonNumeric;

    done.append(jit.branchIfInt32(regs, mode));
    CCallHelpers::Jump notDouble = jit.branchIfNotDoubleKnownNotInt32(regs, mode);
    emitSetDouble(jit);
    done.append(jit.jump());

    notDouble.link(&jit);

    nonNumeric.append(jit.branchIfNotCell(regs, mode));
    nonNumeric.append(jit.branchIfNotBigInt(regs.payloadGPR()));
    emitSetBigInt(jit);
    done.append(jit.jump());

    nonNumeric.link(&jit);
    emitSetNonNumeric(jit);

    done.link(&jit);
}

bool ArithProfile::shouldEmitSetDouble() const
{
    uint32_t mask = ArithProfile::Int32Overflow | ArithProfile::Int52Overflow | ArithProfile::NegZeroDouble | ArithProfile::NonNegZeroDouble;
    return (m_bits & mask) != mask;
}

void ArithProfile::emitSetDouble(CCallHelpers& jit) const
{
    if (shouldEmitSetDouble())
        jit.or32(CCallHelpers::TrustedImm32(ArithProfile::Int32Overflow | ArithProfile::Int52Overflow | ArithProfile::NegZeroDouble | ArithProfile::NonNegZeroDouble), CCallHelpers::AbsoluteAddress(addressOfBits()));
}

bool ArithProfile::shouldEmitSetNonNumeric() const
{
    uint32_t mask = ArithProfile::NonNumeric;
    return (m_bits & mask) != mask;
}

bool ArithProfile::shouldEmitSetBigInt() const
{
    uint32_t mask = ArithProfile::BigInt;
    return (m_bits & mask) != mask;
}

void ArithProfile::emitSetNonNumeric(CCallHelpers& jit) const
{
    if (shouldEmitSetNonNumeric())
        jit.or32(CCallHelpers::TrustedImm32(ArithProfile::NonNumeric), CCallHelpers::AbsoluteAddress(addressOfBits()));
}

void ArithProfile::emitSetBigInt(CCallHelpers& jit) const
{
    if (shouldEmitSetBigInt())
        jit.or32(CCallHelpers::TrustedImm32(ArithProfile::BigInt), CCallHelpers::AbsoluteAddress(addressOfBits()));
}
#endif // ENABLE(JIT)

} // namespace JSC

namespace WTF {
    
using namespace JSC;

void printInternal(PrintStream& out, const ArithProfile& profile)
{
    const char* separator = "";

    out.print("Result:<");
    if (!profile.didObserveNonInt32()) {
        out.print("Int32");
        separator = "|";
    } else {
        if (profile.didObserveNegZeroDouble()) {
            out.print(separator, "NegZeroDouble");
            separator = "|";
        }
        if (profile.didObserveNonNegZeroDouble()) {
            out.print(separator, "NonNegZeroDouble");
            separator = "|";
        }
        if (profile.didObserveNonNumeric()) {
            out.print(separator, "NonNumeric");
            separator = "|";
        }
        if (profile.didObserveInt32Overflow()) {
            out.print(separator, "Int32Overflow");
            separator = "|";
        }
        if (profile.didObserveInt52Overflow()) {
            out.print(separator, "Int52Overflow");
            separator = "|";
        }
        if (profile.didObserveBigInt()) {
            out.print(separator, "BigInt");
            separator = "|";
        }
    }
    if (profile.tookSpecialFastPath())
        out.print(separator, "Took special fast path.");
    out.print(">");

    out.print(" LHS ObservedType:<");
    out.print(profile.lhsObservedType());
    out.print("> RHS ObservedType:<");
    out.print(profile.rhsObservedType());
    out.print(">");

    out.print(" LHS ResultType:<", RawPointer(bitwise_cast<void*>(static_cast<uintptr_t>(profile.lhsResultType().bits()))));
    out.print("> RHS ResultType:<", RawPointer(bitwise_cast<void*>(static_cast<uintptr_t>(profile.rhsResultType().bits()))));
    out.print(">");
}

void printInternal(PrintStream& out, const JSC::ObservedType& observedType)
{
    const char* separator = "";
    if (observedType.sawInt32()) {
        out.print(separator, "Int32");
        separator = "|";
    }
    if (observedType.sawNumber()) {
        out.print(separator, "Number");
        separator = "|";
    }
    if (observedType.sawNonNumber()) {
        out.print(separator, "NonNumber");
        separator = "|";
    }
}

} // namespace WTF
