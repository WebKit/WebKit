/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#if ENABLE(JIT) && USE(JSVALUE64)

#include "BaselineJITRegisters.h"
#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "Opcode.h"
#include "SimpleRegisterAllocator.h"
#include "VirtualRegister.h"

#include <wtf/Scope.h>

namespace JSC::LOL {

// TODO: Pack this.
struct Location {
    GPRReg gpr() const { return regs.gpr(); }
    void dumpInContext(PrintStream& out, const auto*) const
    {
        if (!isFlushed)
            out.print("!"_s);
    }

    JSValueRegs regs { InvalidGPRReg };
    bool isFlushed { false };
};

template<size_t useCount, size_t defCount, size_t scratchCount = 0>
struct AllocationBindings {
    std::array<JSValueRegs, useCount> uses;
    std::array<JSValueRegs, defCount> defs;
    std::array<JSValueRegs, scratchCount> scratches;
};

template<typename Backend>
class RegisterAllocator {
public:
#ifdef NDEBUG
    static constexpr bool verbose = false;
#else
    static constexpr bool verbose = true;
#endif

    static constexpr GPRReg s_scratch = GPRInfo::nonPreservedNonArgumentGPR0;

    struct GPRBank {
        using JITBackend = RegisterAllocator;
        using Register = GPRReg;
        static constexpr Register invalidRegister = InvalidGPRReg;
        // FIXME: Make this more precise
        static constexpr unsigned numberOfRegisters = 32;
        static constexpr Width defaultWidth = widthForBytes(sizeof(CPURegister));
    };
    using SpillHint = uint32_t;
    using RegisterBinding = VirtualRegister;
    template<typename> friend class JSC::SimpleRegisterAllocator;

    RegisterAllocator(Backend& backend, CodeBlock* codeBlock)
        : m_numVars(codeBlock->numVars())
        , m_constantsOffset(codeBlock->numCalleeLocals())
        , m_headersOffset(m_constantsOffset + codeBlock->constantRegisters().size())
        , m_numArguments(codeBlock->numParameters())
        , m_locations(codeBlock->numCalleeLocals() + codeBlock->constantRegisters().size() + CallFrame::headerSizeInRegisters + codeBlock->numParameters())
        , m_backend(backend)
    {
        RegisterSet gprs = RegisterSet::allGPRs();
        gprs.exclude(RegisterSet::specialRegisters());
        gprs.exclude(RegisterSet::macroClobberedGPRs());
        gprs.exclude(RegisterSet::vmCalleeSaveRegisters());
        gprs.remove(s_scratch);
        m_allocator.initialize(gprs, verbose ? "LOL"_s : ASCIILiteral());
    }

    RegisterSet allocatedRegisters() const { return m_allocator.allocatedRegisters(); }
    Location locationOf(VirtualRegister operand) const { return const_cast<RegisterAllocator<Backend>*>(this)->locationOfImpl(operand); }
    VirtualRegister bindingFor(GPRReg reg) const { return m_allocator.bindingFor(reg); }

    // In general, it's somewhat important that these don't change how they allocate based on profiling data as when someone
    // replays that profiling data could have changed and the register state they'd get would be out of sync with reality.
    // returns an AllocationBindings struct with the allocated registers + any scratches (if needed).
    // FIXME: We should be able to verify that the register allocation state is consistent by saving it on the CodeBlock somewhere and validating when we replay.
#define DECLARE_SPECIALIZATION(Op) ALWAYS_INLINE auto allocate(Backend& jit, const Op& instruction, BytecodeIndex);
    FOR_EACH_BYTECODE_STRUCT(DECLARE_SPECIALIZATION)
#undef DECLARE_SPECIALIZATION

    void flushAllRegisters(Backend&) { m_allocator.flushAllRegisters(*this); }

    void dump(PrintStream& out) const { m_allocator.dumpInContext(out, this); }

    // FIXME: Do we even need this, we could just unbind the scratches immediately after picking them since we can't add more allocations for the same instruction.
    template<size_t useCount, size_t defCount, size_t scratchCount>
    ALWAYS_INLINE void releaseScratches(const AllocationBindings<useCount, defCount, scratchCount>& allocations)
    {
#if ASSERT_ENABLED
        m_needsReleaseScratches = false;
#endif

        for (JSValueRegs scratch : allocations.scratches) {
            ASSERT(!bindingFor(scratch.gpr()).isValid());
            m_allocator.unlock(scratch.gpr());
            m_allocator.unbind(scratch.gpr());
        }
    }

private:
    struct AllocationHint {
        AllocationHint() = default;
        AllocationHint(VirtualRegister b, JSValueRegs h = JSValueRegs())
            : m_binding(b), m_hint(h) { }

        VirtualRegister m_binding;
        JSValueRegs m_hint;
    };

    template<size_t scratchCount, size_t useCount, size_t defCount>
    ALWAYS_INLINE AllocationBindings<useCount, defCount, scratchCount> allocateImpl(Backend& jit, const auto& instruction, BytecodeIndex index, const std::array<AllocationHint, useCount>& uses, const std::array<AllocationHint, defCount>& defs)
    {
#if ASSERT_ENABLED
        ASSERT(!m_needsReleaseScratches);
        auto setter = makeScopeExit([&] {
            m_needsReleaseScratches = true;
        });
#endif
        m_allocator.assertAllValidRegistersAreUnlocked();
        // TODO: More validation.
        UNUSED_PARAM(instruction);
        // Bump the spill count for our uses so we don't spill them when allocating below.
        for (auto operand : uses) {
            if (auto current = locationOf(operand.m_binding).regs)
                m_allocator.setSpillHint(current.gpr(), index.offset());
        }

        auto doAllocate = [&](AllocationHint operand, bool isDef) ALWAYS_INLINE_LAMBDA {
            ASSERT_IMPLIES(isDef, operand.m_binding.isLocal() || operand.m_binding.isArgument());
            Location& location = locationOfImpl(operand.m_binding);
            if (location.regs) {
                // Uses might be dirty from a previous instruction, so don't touch them.
                if (isDef)
                    location.isFlushed = false;
                return location.regs;
            }

            // TODO: Consider LRU insertion policy here (i.e. 0 for hint). Might need locking so these don't spill on the next allocation in the same bytecode.
            location.regs = JSValueRegs(m_allocator.allocate(*this, operand.m_binding, index.offset(), operand.m_hint.payloadGPR()));
            location.isFlushed = !isDef;

            if (!isDef)
                jit.fill(operand.m_binding, location.regs.gpr());
            return location.regs;
        };

        AllocationBindings<useCount, defCount, scratchCount> result;
        for (size_t i = 0; i < uses.size(); ++i)
            result.uses[i] = doAllocate(uses[i], false);

        for (size_t i = 0; i < defs.size(); ++i)
            result.defs[i] = doAllocate(defs[i], true);

        for (size_t i = 0; i < result.scratches.size(); ++i) {
            GPRReg scratch = m_allocator.allocate(*this, VirtualRegister(), 0);
            result.scratches[i] = JSValueRegs(scratch);
            // Lock the register so it doesn't get spilled subsequently.
            m_allocator.lock(scratch);
        }

        return result;
    }

    template<size_t scratchCount = 0>
    ALWAYS_INLINE auto allocateUnaryOp(Backend& jit, const auto& instruction, BytecodeIndex index, VirtualRegister source)
    {
        std::array<AllocationHint, 1> uses = { source };
        std::array<AllocationHint, 1> defs = { instruction.m_dst };
        return allocateImpl<scratchCount>(jit, instruction, index, uses, defs);
    }

    template<size_t scratchCount = 0>
    ALWAYS_INLINE auto allocateBinaryOp(Backend& jit, const auto& instruction, BytecodeIndex index)
    {
        std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
        std::array<AllocationHint, 1> defs = { instruction.m_dst };
        return allocateImpl<scratchCount>(jit, instruction, index, uses, defs);
    }

    friend class SimpleRegisterAllocator<GPRBank>;
    void flush(GPRReg gpr, VirtualRegister binding)
    {
        Location& location = locationOfImpl(binding);
        ASSERT(location.gpr() == gpr);
        m_backend.flush(location, gpr, binding);
        location = Location();
    }

    Location& locationOfImpl(VirtualRegister operand)
    {
        ASSERT(operand.isValid());
        // Locals are first since they are the most common and we want to be able to access them without loading offsets.
        if (operand.isLocal())
            return m_locations[operand.toLocal()];
        if (operand.isConstant())
            return m_locations[operand.toConstantIndex() + m_constantsOffset];
        ASSERT(operand.isArgument() || operand.isHeader());
        // arguments just naturally follow the headers.
        return m_locations[operand.offset() + m_headersOffset];
    }

    const uint32_t m_numVars;
    const uint32_t m_constantsOffset;
    const uint32_t m_headersOffset;
    const uint32_t m_numArguments;
    // This is laid out as [ locals, constants, headers, arguments ]
    FixedVector<Location> m_locations;
    SimpleRegisterAllocator<GPRBank> m_allocator;
    Backend& m_backend;
#if ASSERT_ENABLED
    bool m_needsReleaseScratches { false };
#endif
};

class ReplayBackend {
public:
    ReplayBackend() = default;
    ALWAYS_INLINE void flush(const Location&, GPRReg, VirtualRegister) { }
    ALWAYS_INLINE void fill(VirtualRegister, GPRReg) { }
};

using ReplayRegisterAllocator = RegisterAllocator<ReplayBackend>;

#define FOR_EACH_UNARY_OP(macro) \
    macro(OpToNumber, m_operand, 0) \
    macro(OpNegate, m_operand, 0) \
    macro(OpToString, m_operand, 0) \
    macro(OpToObject, m_operand, 0) \
    macro(OpToNumeric, m_operand, 0) \
    macro(OpToPropertyKey, m_src, 0) \
    macro(OpToPropertyKeyOrNumber, m_src, 0) \
    macro(OpToPrimitive, m_src, 0) \
    macro(OpBitnot, m_operand, 0) \
    macro(OpResolveScope, m_scope, 1) \
    macro(OpGetFromScope, m_scope, 1) \
    macro(OpGetPrototypeOf, m_value, 0) \
    macro(OpCreateThis, m_callee, 3) \
    macro(OpIsEmpty, m_operand, 0) \
    macro(OpTypeofIsUndefined, m_operand, 0) \
    macro(OpTypeofIsFunction, m_operand, 0) \
    macro(OpIsUndefinedOrNull, m_operand, 0) \
    macro(OpIsBoolean, m_operand, 0) \
    macro(OpIsNumber, m_operand, 0) \
    macro(OpIsBigInt, m_operand, 0) \
    macro(OpIsObject, m_operand, 0) \
    macro(OpIsCellWithType, m_operand, 0) \
    macro(OpHasStructureWithFlags, m_operand, 0)

#define ALLOCATE_USE_DEFS_FOR_UNARY_OP(Struct, operand, scratchCount) \
template<typename Backend> \
auto RegisterAllocator<Backend>::allocate(Backend& jit, const Struct& instruction, BytecodeIndex index) \
{ \
    return allocateUnaryOp<scratchCount>(jit, instruction, index, instruction.operand); \
}

FOR_EACH_UNARY_OP(ALLOCATE_USE_DEFS_FOR_UNARY_OP)

#undef ALLOCATE_USE_DEFS_FOR_UNARY_OP
#undef FOR_EACH_UNARY_OP

#define FOR_EACH_BINARY_OP(macro) \
    macro(OpAdd) \
    macro(OpMul) \
    macro(OpSub) \
    macro(OpEq) \
    macro(OpNeq) \
    macro(OpLess) \
    macro(OpLesseq) \
    macro(OpGreater) \
    macro(OpGreatereq) \
    macro(OpLshift) \
    macro(OpRshift) \
    macro(OpUrshift) \
    macro(OpBitand) \
    macro(OpBitor) \
    macro(OpBitxor)

#define ALLOCATE_USE_DEFS_FOR_BINARY_OP(Struct) \
template<typename Backend> \
auto RegisterAllocator<Backend>::allocate(Backend& jit, const Struct& instruction, BytecodeIndex index) \
{ \
    return allocateBinaryOp(jit, instruction, index); \
}

FOR_EACH_BINARY_OP(ALLOCATE_USE_DEFS_FOR_BINARY_OP)

#undef ALLOCATE_USE_DEFS_FOR_BINARY_OP
#undef FOR_EACH_BINARY_OP

#define FOR_EACH_FLUSHING_UNARY_OP(macro) \
    macro(OpJtrue, m_condition, BaselineJITRegisters::JTrue::valueJSR) \
    macro(OpJfalse, m_condition, BaselineJITRegisters::JFalse::valueJSR) \
    macro(OpJeqNull, m_value, JSValueRegs()) \
    macro(OpJneqNull, m_value, JSValueRegs()) \
    macro(OpJundefinedOrNull, m_value, JSValueRegs()) \
    macro(OpJnundefinedOrNull, m_value, JSValueRegs()) \
    macro(OpJeqPtr, m_value, JSValueRegs()) \
    macro(OpJneqPtr, m_value, JSValueRegs()) \
    macro(OpThrow, m_value, BaselineJITRegisters::Throw::thrownValueJSR) \
    macro(OpSwitchImm, m_scrutinee, JSValueRegs()) \
    macro(OpSwitchChar, m_scrutinee, JSValueRegs()) \
    macro(OpSwitchString, m_scrutinee, BaselineJITRegisters::SwitchString::scrutineeJSR)

#define ALLOCATE_FOR_FLUSHING_UNARY_OP(Struct, operand, hint) \
template<typename Backend> \
auto RegisterAllocator<Backend>::allocate(Backend& jit, const Struct& instruction, BytecodeIndex index) \
{ \
    std::array<AllocationHint, 1> uses = { AllocationHint(instruction.operand, hint) }; \
    std::array<AllocationHint, 0> defs = { }; \
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs); \
    m_allocator.flushAllRegisters(*this); \
    return result; \
}

FOR_EACH_FLUSHING_UNARY_OP(ALLOCATE_FOR_FLUSHING_UNARY_OP)

#undef ALLOCATE_FOR_FLUSHING_UNARY_OP
#undef FOR_EACH_FLUSHING_UNARY_OP

#define FOR_EACH_FLUSHING_BINARY_OP(macro) \
    macro(OpJeq) \
    macro(OpJneq) \
    macro(OpJless) \
    macro(OpJlesseq) \
    macro(OpJgreater) \
    macro(OpJgreatereq) \
    macro(OpJnless) \
    macro(OpJnlesseq) \
    macro(OpJngreater) \
    macro(OpJngreatereq) \
    macro(OpJstricteq) \
    macro(OpJnstricteq) \
    macro(OpJbelow) \
    macro(OpJbeloweq)

#define ALLOCATE_FOR_FLUSHING_BINARY_OP(Struct) \
template<typename Backend> \
auto RegisterAllocator<Backend>::allocate(Backend& jit, const Struct& instruction, BytecodeIndex index) \
{ \
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs }; \
    std::array<AllocationHint, 0> defs = { }; \
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs); \
    m_allocator.flushAllRegisters(*this); \
    return result; \
}

FOR_EACH_FLUSHING_BINARY_OP(ALLOCATE_FOR_FLUSHING_BINARY_OP)

#undef ALLOCATE_FOR_FLUSHING_BINARY_OP
#undef FOR_EACH_FLUSHING_BINARY_OP

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpPutToScope& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_scope, instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    return allocateImpl<1>(jit, instruction, index, uses, defs); // 1 scratch for metadata
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpGetArgument& instruction, BytecodeIndex index)
{
    // The argument value might be in a register if it's less than the number of lexical arguments and was previously written. That's rare so just flush the current value back to the stack in that case.
    if (static_cast<uint32_t>(instruction.m_index) < m_numArguments) {
        VirtualRegister argument = virtualRegisterForArgumentIncludingThis(instruction.m_index);
        Location& location = locationOfImpl(argument);
        if (location.regs)
            jit.flush(location, location.regs.payloadGPR(), argument);
    }


    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 1> defs = { instruction.m_dst };
    return allocateImpl<0>(jit, instruction, index, uses, defs);
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpArgumentCount& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 1> defs = { instruction.m_dst };
    return allocateImpl<0>(jit, instruction, index, uses, defs);
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpMov& instruction, BytecodeIndex index)
{
    // TODO: Consider other heuristics here such as transferring ownership (optionally only if all registers are full)
    std::array<AllocationHint, 1> uses = { instruction.m_src };
    std::array<AllocationHint, 1> defs = { instruction.m_dst };
    return allocateImpl<0>(jit, instruction, index, uses, defs);
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJmp& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewObject& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 1> defs = { instruction.m_dst };
    return allocateImpl<2>(jit, instruction, index, uses, defs);
}

// These ops always call C++ operations, so we just flush everything
// TODO: Inline the allocation so there's a fast path that doesn't require flushing.
template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpCreateLexicalEnvironment& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_scope, instruction.m_symbolTable };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpCreateDirectArguments& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpCreateScopedArguments& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpCreateClonedArguments& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

// new_* ops - these all call C++ operations and flush everything
template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewArray& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewArrayWithSize& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_length };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewFunc& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewFuncExp& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewGeneratorFunc& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewGeneratorFuncExp& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewAsyncFunc& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewAsyncFuncExp& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewAsyncGeneratorFunc& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewAsyncGeneratorFuncExp& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_scope };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewRegExp& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpInc& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_srcDst };
    std::array<AllocationHint, 1> defs = { instruction.m_srcDst };
    return allocateImpl<0>(jit, instruction, index, uses, defs);
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpDec& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_srcDst };
    std::array<AllocationHint, 1> defs = { instruction.m_srcDst };
    return allocateImpl<0>(jit, instruction, index, uses, defs);
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpMod& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 1> defs = { instruction.m_dst };

    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
#if CPU(X86_64)
    // TODO: FIX X86 clobbering rules for eax/edx/ecx. This is inefficient and hacky.
    m_allocator.flushAllRegisters(*this);
    Location& dstLocation = locationOfImpl(instruction.m_dst);
    dstLocation.regs = result.defs[0];
    ASSERT(!dstLocation.isFlushed);
    m_allocator.bind(result.defs[0].payloadGPR(), instruction.m_dst, index.offset());
#endif
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpDiv& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 1> defs = { instruction.m_dst };

    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
#if CPU(X86_64)
    // TODO: FIX X86 clobbering rules for eax/edx/ecx. This is inefficient and hacky.
    m_allocator.flushAllRegisters(*this);
    Location& dstLocation = locationOfImpl(instruction.m_dst);
    dstLocation.regs = result.defs[0];
    ASSERT(!dstLocation.isFlushed);
    m_allocator.bind(result.defs[0].payloadGPR(), instruction.m_dst, index.offset());
#endif
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpToThis& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_srcDst };
    std::array<AllocationHint, 1> defs = { instruction.m_srcDst };
    return allocateImpl<0>(jit, instruction, index, uses, defs);
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpRet& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { AllocationHint(instruction.m_value, JSRInfo::returnValueJSR) };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    // unbind everything without flushing since we're returning anyway.
    for (auto reg : m_allocator.allocatedRegisters()) {
        VirtualRegister binding = bindingFor(reg.gpr());
        Location& location = locationOfImpl(binding);
        location = Location();
        m_allocator.unbind(reg.gpr());
    }
    return result;
}

} // namespace JSC

#endif
