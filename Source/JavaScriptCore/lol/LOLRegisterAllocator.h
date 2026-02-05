/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "Opcode.h"
#include "SimpleRegisterAllocator.h"
#include "VirtualRegister.h"

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
        , m_locations(codeBlock->numCalleeLocals() + codeBlock->constantRegisters().size() + CallFrame::headerSizeInRegisters + codeBlock->numParameters())
        , m_backend(backend)
    {
        RegisterSetBuilder gprs = RegisterSetBuilder::allGPRs();
        gprs.exclude(RegisterSetBuilder::specialRegisters());
        gprs.exclude(RegisterSetBuilder::macroClobberedGPRs());
        gprs.exclude(RegisterSetBuilder::vmCalleeSaveRegisters());
        gprs.remove(s_scratch);
        m_allocator.initialize(gprs.buildAndValidate(), verbose ? "LOL"_s : ASCIILiteral());
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
        for (JSValueRegs scratch : allocations.scratches) {
            ASSERT(!bindingFor(scratch.gpr()).isValid());
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
        // TODO: Validation.
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

        // TODO: Maybe lock the register here for debugging purposes.
        for (size_t i = 0; i < result.scratches.size(); ++i)
            result.scratches[i] = JSValueRegs(m_allocator.allocate(*this, VirtualRegister(), 0));

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

    // Only used for debugging.
    const uint32_t m_numVars;
    const uint32_t m_constantsOffset;
    const uint32_t m_headersOffset;
    // This is laid out as [ locals, constants, headers, arguments ]
    FixedVector<Location> m_locations;
    SimpleRegisterAllocator<GPRBank> m_allocator;
    Backend& m_backend;
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
    macro(OpBitnot, m_operand, 0) \
    macro(OpResolveScope, m_scope, 1) \
    macro(OpGetFromScope, m_scope, 1) \
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

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpPutToScope& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_scope, instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    return allocateImpl<1>(jit, instruction, index, uses, defs); // 1 scratch for metadata
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
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJeq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJneq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
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
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJtrue& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { AllocationHint(instruction.m_condition, BaselineJITRegisters::JTrue::valueJSR) };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJfalse& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { AllocationHint(instruction.m_condition, BaselineJITRegisters::JFalse::valueJSR) };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJeqNull& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJneqNull& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJundefinedOrNull& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJnundefinedOrNull& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJeqPtr& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJneqPtr& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 1> uses = { instruction.m_value };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJless& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJlesseq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJgreater& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJgreatereq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJnless& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJnlesseq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJngreater& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJngreatereq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJstricteq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJnstricteq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJbelow& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
}

template<typename Backend>
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpJbeloweq& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 2> uses = { instruction.m_lhs, instruction.m_rhs };
    std::array<AllocationHint, 0> defs = { };
    auto result = allocateImpl<0>(jit, instruction, index, uses, defs);
    m_allocator.flushAllRegisters(*this);
    return result;
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
auto RegisterAllocator<Backend>::allocate(Backend& jit, const OpNewObject& instruction, BytecodeIndex index)
{
    std::array<AllocationHint, 0> uses = { };
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

} // namespace JSC

#endif
