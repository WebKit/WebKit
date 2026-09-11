/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGOSRExitBase.h"
#include "DFGVariableEventStream.h"
#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "MethodOfGettingAValueProfile.h"
#include "Operands.h"
#include "OperationResult.h"
#include "ValueRecovery.h"
#include <wtf/FixedVector.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

class ArrayProfile;
class CCallHelpers;

namespace Probe {
class Context;
} // namespace Probe

namespace Profiler {
class OSRExit;
} // namespace Profiler

namespace DFG {

class BasicBlock;
class SpeculativeJIT;
struct Node;

// This enum describes the types of additional recovery that
// may need be performed should a speculation check fail.
enum SpeculationRecoveryType : uint8_t {
    SpeculativeAdd,
    SpeculativeAddSelf,
    SpeculativeAddImmediate,
    BooleanSpeculationCheck
};

// === SpeculationRecovery ===
//
// This class provides additional information that may be associated with a
// speculation check - for example 
class SpeculationRecovery {
public:
    SpeculationRecovery(SpeculationRecoveryType type, GPRReg dest, GPRReg src)
        : m_src(src)
        , m_dest(dest)
        , m_type(type)
    {
        ASSERT(m_type == SpeculativeAdd || m_type == SpeculativeAddSelf || m_type == BooleanSpeculationCheck);
    }

    SpeculationRecovery(SpeculationRecoveryType type, GPRReg dest, int32_t immediate)
        : m_immediate(immediate)
        , m_dest(dest)
        , m_type(type)
    {
        ASSERT(m_type == SpeculativeAddImmediate);
    }

    SpeculationRecoveryType type() { return m_type; }
    GPRReg dest() { return m_dest; }
    GPRReg src() { return m_src; }
    int32_t immediate() { return m_immediate; }

private:
    // different recovery types may required different additional information here.
    union {
        GPRReg m_src;
        int32_t m_immediate;
    };
    GPRReg m_dest;

    // Indicates the type of additional recovery to be performed.
    SpeculationRecoveryType m_type;
};

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCompileOSRExit, void, (CallFrame*, void*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationDebugPrintSpeculationFailure, void, (Probe::Context&));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationMaterializeOSRExitSideState, void, (VM*, InlineCallFrame* exitInlineCallFrame, uint32_t exitBytecodeIndexBits, EncodedJSValue*));

// === OSRExit ===
//
// This structure describes how to exit the speculative path by
// going into baseline code.
struct OSRExit : public OSRExitBase {
    using OSRExitBase::OSRExitBase;
    OSRExit(ExitKind, JSValueSource, MethodOfGettingAValueProfile, SpeculativeJIT*, unsigned streamIndex, unsigned recoveryIndex = UINT_MAX);

    CodeLocationLabel<JSInternalPtrTag> m_patchableJumpLocation;

    JSValueSource m_jsValueSource;
    MethodOfGettingAValueProfile m_valueProfile;
    
    unsigned m_recoveryIndex { UINT_MAX };

    CodeLocationJump<JSInternalPtrTag> codeLocationForRepatch() const { return CodeLocationJump<JSInternalPtrTag>(m_patchableJumpLocation); }

    unsigned m_streamIndex { 0 };
    void considerAddingAsFrequentExitSite(CodeBlock* profiledCodeBlock)
    {
        OSRExitBase::considerAddingAsFrequentExitSite(profiledCodeBlock, ExitFromDFG);
    }

    static void compileExit(CCallHelpers&, VM&, const OSRExit&, const Operands<ValueRecovery>&, SpeculationRecovery*, uint32_t osrExitIndex);

private:
    static void emitRestoreArguments(CCallHelpers&, VM&, const Operands<ValueRecovery>&);
};

// Stores the OSRExits of a DFG JITCode as a byte stream, in chunks of exitsPerChunk exits.
// The deltas below are taken against the previous exit of the chunk; the first exit of a
// chunk is taken against a PreviousExit with no InlineCallFrame, BytecodeIndex 0 and every
// other field 0. Exception handler exits are listed in m_exceptionHandlerExits with their
// m_exceptionHandlerCallSiteIndex, which is not in the stream.
//
//   byte        ExitKind
//   LEB128      flags:
//     bits 0-1  origin tag of m_codeOrigin against the previous exit's m_codeOrigin
//     bits 2-3  origin tag of m_codeOriginForExitProfile against m_codeOrigin
//     bit 4     m_wasHoisted
//     bit 5     m_jsValueSource is set
//     bit 6     m_jsValueSource is an address
//     bit 7     m_valueProfile is set
//     bits 8-9  origin tag of m_valueProfile's code origin against m_codeOriginForExitProfile
//     bit 10    m_recoveryIndex is set
//   the payload of each origin tag, in the order of the flags:
//     0         the same as the previous one: nothing
//     1         the same InlineCallFrame: LEB128 signed delta of the BytecodeIndex bits
//     2         8-byte InlineCallFrame pointer, then the LEB128 signed delta
//   LEB128      signed delta of m_streamIndex
//   LEB128      signed delta of m_dfgNodeIndex
//   LEB128      signed delta of m_patchableJumpLocation's offset from the code start (linked DFG only)
//   [byte]      bit 5: the GPRReg of m_jsValueSource
//   [LEB128]    bit 6: its offset
//   [byte]      bit 7: the Kind of m_valueProfile, then its origin payload and its LEB128 operand bits
//   [LEB128]    bit 10: m_recoveryIndex
//   [LEB128]    WillThrowOutOfMemoryError: m_exitCallSiteIndex
class OSRExitStream {
public:
    struct ExceptionHandlerExit {
        CallSiteIndex callSiteIndex;
        uint32_t exitIndex;
    };

    OSRExitStream() = default;
    OSRExitStream(const Vector<OSRExit>&, CodeLocationLabel<JSInternalPtrTag> codeStart);

    unsigned size() const { return m_size; }
    OSRExit at(unsigned index) const;
    std::span<const ExceptionHandlerExit> exceptionHandlerExits() const LIFETIME_BOUND { return m_exceptionHandlerExits.span(); }

private:
    static constexpr unsigned exitsPerChunk = 16;

    struct PreviousExit {
        CodeOrigin codeOrigin { BytecodeIndex(0) };
        unsigned streamIndex { 0 };
        uint32_t dfgNodeIndex { 0 };
        unsigned patchableJumpOffset { 0 };
    };

    OSRExit decode(size_t& offset, PreviousExit&) const;

    FixedVector<uint8_t> m_bytes;
    FixedVector<uint32_t> m_chunkOffsets;
    FixedVector<ExceptionHandlerExit> m_exceptionHandlerExits;
    CodeLocationLabel<JSInternalPtrTag> m_codeStart;
    unsigned m_size { 0 };
};

struct SpeculationFailureDebugInfo {
    WTF_MAKE_STRUCT_SEQUESTERED_ARENA_ALLOCATED(SpeculationFailureDebugInfo);
    CodeBlock* codeBlock;
    ExitKind kind;
    uint32_t exitIndex;
    BytecodeIndex bytecodeIndex;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
