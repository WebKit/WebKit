/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "CodeLocation.h"
#include "DFGAbstractValue.h"
#include "DFGFlushFormat.h"
#include "MacroAssemblerCodeRef.h"
#include "Operands.h"
#include <wtf/BitVector.h>
#include <wtf/FixedVector.h>

namespace JSC {

class CallFrame;
class CodeBlock;
class TrackedReferences;

namespace DFG {

#if ENABLE(DFG_JIT)
struct OSREntryReshuffling {
    OSREntryReshuffling() { }
    
    OSREntryReshuffling(int fromOffset, int toOffset)
        : fromOffset(fromOffset)
        , toOffset(toOffset)
    {
    }
    
    int fromOffset;
    int toOffset;
};

// Stores the AbstractValue of every operand of an OSR entry (arguments, then locals) as a
// byte stream: a tag byte followed by an optional payload.
//
//   0x00-0x7F  (tag + 1) consecutive BytecodeTop values
//   0x80-0xFF  an explicit value: LEB128 SpeculatedType follows, then the payloads the tag bits select
//     bit 0    a constant: 8-byte EncodedJSValue follows
//     bit 1    array modes are ALL_ARRAY_MODES (otherwise none)
//     bit 2    explicit array modes: LEB128 ArrayModes follows, overriding bit 1
//     bit 3    structures are TOP or clobbered (otherwise none)
//     bit 4    a structure list: LEB128 count and 4-byte StructureIDs follow, overriding bit 3
class OSREntryExpectedValues {
public:
    OSREntryExpectedValues() = default;
    explicit OSREntryExpectedValues(const FixedOperands<AbstractValue>&);

    FixedOperands<AbstractValue> decode() const;
    void validateReferences(const TrackedReferences&) const;

private:
    FixedVector<uint8_t> m_bytes;
    unsigned m_numberOfArguments { 0 };
    unsigned m_numberOfLocals { 0 };
};

struct OSREntryData {
    BytecodeIndex m_bytecodeIndex;
    CodeLocationLabel<OSREntryPtrTag> m_machineCode;
    OSREntryExpectedValues m_expectedValues;
    // Use bitvectors here because they tend to only require one word.
    BitVector m_localsForcedDouble;
    BitVector m_localsForcedAnyInt;
    FixedVector<OSREntryReshuffling> m_reshufflings;
    BitVector m_machineStackUsed;
    
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
};

inline BytecodeIndex getOSREntryDataBytecodeIndex(OSREntryData* osrEntryData)
{
    return osrEntryData->m_bytecodeIndex;
}

struct CatchEntrypointData {
    // We use this when doing OSR entry at catch. We prove the arguments
    // are of the expected type before entering at a catch block.
    CodePtr<ExceptionHandlerPtrTag> machineCode;
    FixedVector<FlushFormat> argumentFormats;
    BytecodeIndex bytecodeIndex;
};

// Returns a pointer to a data buffer that the OSR entry thunk will recognize and
// parse. If this returns null, it means 
void* prepareOSREntry(VM&, CallFrame*, CodeBlock*, BytecodeIndex);

// If null is returned, we can't OSR enter. If it's not null, it's the PC to jump to.
CodePtr<ExceptionHandlerPtrTag> prepareCatchOSREntry(VM&, CallFrame*, CodeBlock* baselineCodeBlock, CodeBlock* optimizedCodeBlock, BytecodeIndex);
#else
inline CodePtr<ExceptionHandlerPtrTag> prepareOSREntry(VM&, CallFrame*, CodeBlock*, BytecodeIndex) { return nullptr; }
#endif

} } // namespace JSC::DFG
