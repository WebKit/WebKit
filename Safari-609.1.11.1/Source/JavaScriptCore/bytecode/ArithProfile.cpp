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
template<typename BitfieldType>
void ArithProfile<BitfieldType>::emitObserveResult(CCallHelpers& jit, JSValueRegs regs, TagRegistersMode mode)
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

template<typename BitfieldType>
bool ArithProfile<BitfieldType>::shouldEmitSetDouble() const
{
    BitfieldType mask = ObservedResults::Int32Overflow | ObservedResults::Int52Overflow | ObservedResults::NegZeroDouble | ObservedResults::NonNegZeroDouble;
    return (m_bits & mask) != mask;
}

template<typename BitfieldType>
void ArithProfile<BitfieldType>::emitSetDouble(CCallHelpers& jit) const
{
    if (shouldEmitSetDouble())
        emitUnconditionalSet(jit, ObservedResults::Int32Overflow | ObservedResults::Int52Overflow | ObservedResults::NegZeroDouble | ObservedResults::NonNegZeroDouble);
}

template<typename BitfieldType>
bool ArithProfile<BitfieldType>::shouldEmitSetNonNumeric() const
{
    BitfieldType mask = ObservedResults::NonNumeric;
    return (m_bits & mask) != mask;
}

template<typename BitfieldType>
void ArithProfile<BitfieldType>::emitSetNonNumeric(CCallHelpers& jit) const
{
    if (shouldEmitSetNonNumeric())
        emitUnconditionalSet(jit, ObservedResults::NonNumeric);
}

template<typename BitfieldType>
bool ArithProfile<BitfieldType>::shouldEmitSetBigInt() const
{
    BitfieldType mask = ObservedResults::BigInt;
    return (m_bits & mask) != mask;
}

template<typename BitfieldType>
void ArithProfile<BitfieldType>::emitSetBigInt(CCallHelpers& jit) const
{
    if (shouldEmitSetBigInt())
        emitUnconditionalSet(jit, ObservedResults::BigInt);
}

template<typename BitfieldType>
void ArithProfile<BitfieldType>::emitUnconditionalSet(CCallHelpers& jit, BitfieldType mask) const
{
    static_assert(std::is_same<BitfieldType, uint16_t>::value);
    jit.or16(CCallHelpers::TrustedImm32(mask), CCallHelpers::AbsoluteAddress(addressOfBits()));
}

// Generate the implementations of the functions above for UnaryArithProfile/BinaryArithProfile
// If changing the size of either, add the corresponding lines here.
template class ArithProfile<uint16_t>;
#endif // ENABLE(JIT)

} // namespace JSC

namespace WTF {
    
using namespace JSC;

template <typename T>
void printInternal(PrintStream& out, const ArithProfile<T>& profile)
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
    out.print(">");
}

void printInternal(PrintStream& out, const UnaryArithProfile& profile)
{
    printInternal(out, static_cast<ArithProfile<UnaryArithProfileBase>>(profile));

    out.print(" Arg ObservedType:<");
    out.print(profile.argObservedType());
    out.print(">");
}

void printInternal(PrintStream& out, const BinaryArithProfile& profile)
{
    printInternal(out, static_cast<ArithProfile<UnaryArithProfileBase>>(profile));

    if (profile.tookSpecialFastPath())
        out.print(" Took special fast path.");

    out.print(" LHS ObservedType:<");
    out.print(profile.lhsObservedType());
    out.print("> RHS ObservedType:<");
    out.print(profile.rhsObservedType());
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
