/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/RegisterSet.h>
#include <array>

#if ENABLE(JIT)

namespace JSC {

// Designed as a flexible register allocator for our non-Air backends. Since those backends emit one bytecode or IR node at a time in isolation this API is designed around that. The expected usage of this API will look something like:
//
// 1) Start a new bytecode or IR node
// 2) Lock some number of operands so they don't get evicted.
// 3) Allocate any scratches and result registers (possibly locking them).
// 4) Emit assembly.
// 5) Unlock anything that was locked.
//
// Since a bytecode/IR node could have the same operand more than once it's expected that the number of locks on a register matches the number of unlocks before the then next bytecode/IR node begins (thus any subsequent locks).
template<typename RegisterBank>
class SimpleRegisterAllocator {
    WTF_MAKE_NONCOPYABLE(SimpleRegisterAllocator);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    using Register = RegisterBank::Register;
    static_assert(std::is_same_v<Register, GPRReg> || std::is_same_v<Register, FPRReg>);
    using JITBackend = RegisterBank::JITBackend;
    static constexpr Register invalidRegister = RegisterBank::invalidRegister;
    // Usually used by JITBackends to track where:
    //    1) The type of binding it is (e.g. scratch vs value)
    //    2) Where this value should be flushed to when spilled.
    using RegisterBinding = JITBackend::RegisterBinding;
    using RegisterBindings = std::array<RegisterBinding, RegisterBank::numberOfRegisters>;
    using SpillHint = JITBackend::SpillHint;
    static_assert(std::unsigned_integral<SpillHint>);
    static constexpr SpillHint invalidRegisterHint = std::numeric_limits<SpillHint>::max();

    static constexpr Width registerWidth = RegisterBank::defaultWidth;

    SimpleRegisterAllocator() = default;

    Register allocate(JITBackend& backend, RegisterBinding&& binding, std::optional<SpillHint> hint, Register hintReg = invalidRegister)
    {
        Register result = hintReg;
        if (result == invalidRegister || !m_freeRegisters.contains(result, registerWidth))
            result = m_freeRegisters.isEmpty() ? evictRegister(backend) : nextFreeRegister();
        bind(result, WTFMove(binding), hint);
        return result;
    }

    // Pass nullopt as the hint if you don't want to touch the spill order.
    void bind(Register reg, RegisterBinding&& binding, std::optional<SpillHint> hint)
    {
        ASSERT(m_validRegisters.contains(reg, registerWidth));
        ASSERT(m_freeRegisters.contains(reg, registerWidth));
        ASSERT(m_bindings[reg] == RegisterBinding());
        dataLogLnIf(!m_logPrefix.isNull(), m_logPrefix, "\tBinding ", reg, " to ", binding);
        m_freeRegisters.remove(reg);
        m_bindings[reg] = WTFMove(binding);
        if (hint)
            m_spiller.setHint(reg, hint.value());
    }

    void unbind(Register reg, ASCIILiteral reason = "Unbinding"_s)
    {
        ASSERT(m_validRegisters.contains(reg, registerWidth));
        ASSERT(!m_spiller.isLocked(reg));
        dataLogLnIf(!m_logPrefix.isNull(), m_logPrefix, "\t", reason, " ", reg, " currently bound to ", m_bindings[reg]);
        m_freeRegisters.add(reg, registerWidth);
        m_bindings[reg] = RegisterBinding();
    }

    void flushIf(JITBackend& backend, const Invocable<bool(const RegisterBinding&)> auto& functor)
    {
        for (Reg r : m_validRegisters) {
            Register reg = fromJSCReg(r);
            if (m_freeRegisters.contains(reg, registerWidth)) {
                ASSERT(m_bindings[reg] == RegisterBinding());
                continue;
            }

            if (!functor(m_bindings[reg]))
                continue;

            backend.flush(reg, m_bindings[reg]);
            unbind(reg, "Flushing"_s);
        }
    }

    void flushAllRegisters(JITBackend& backend)
    {
        flushIf(backend, [&](const RegisterBinding&) ALWAYS_INLINE_LAMBDA { return true; });
    }

    void clobber(JITBackend& backend, Register reg)
    {
        if (m_validRegisters.contains(reg, registerWidth) && !m_freeRegisters.contains(reg, registerWidth)) {
            backend.flush(reg, m_bindings[reg]);
            unbind(reg, "Clobbering"_s);
        }
    }

    RegisterSet validRegisters() const { return m_validRegisters; }
    RegisterSet freeRegisters() const { return m_freeRegisters; }
    const RegisterBinding& bindingFor(Register reg) { return m_bindings[reg]; }
    // FIXME: We should really compress this since it's copied by slow paths to know how to restore the correct state.
    RegisterBindings copyBindings() const { return m_bindings; }
    void assertAllValidRegistersAreUnlocked() const
    {
#if ASSERT_ENABLED
        for (Reg reg : m_validRegisters)
            ASSERT(!m_spiller.isLocked(fromJSCReg(reg)));
#endif
    }

    void setSpillHint(Register reg, SpillHint hint)
    {
        ASSERT(m_bindings[reg] != RegisterBinding());
        ASSERT(m_validRegisters.contains(reg, registerWidth));
        m_spiller.setHint(reg, hint);
    }

    void lock(Register reg) { m_spiller.lock(reg); }
    void unlock(Register reg) { m_spiller.unlock(reg); }
    bool isLocked(Register reg) const { return m_spiller.isLocked(reg); }

    void initialize(RegisterSet registers, ASCIILiteral logPrefix = ASCIILiteral())
    {
        ASSERT(m_validRegisters.isEmpty());
        m_validRegisters = registers;
        m_freeRegisters = registers;
        m_spiller.initialize(registers);
        m_logPrefix = logPrefix;
        dataLogLnIf(!m_logPrefix.isNull(), m_logPrefix, "\tUsing ", enumTypeName<Register>(), "s ", m_validRegisters);
    }

private:
    Register nextFreeRegister()
    {
        auto next = m_freeRegisters.begin();
        ASSERT(next != m_freeRegisters.end());
        Register reg = fromJSCReg(*next);
        ASSERT(m_bindings[reg] == RegisterBinding());
        return reg;
    }

    Register evictRegister(JITBackend& backend)
    {
        Register result = m_spiller.findMinHintRegisterToSpill();
        ASSERT(m_bindings[result] != RegisterBinding());
        dataLogLnIf(!m_logPrefix.isNull(), m_logPrefix, "\tEvicting ", result, " currently bound to ", m_bindings[result]);
        backend.flush(result, m_bindings[result]);
        m_bindings[result] = RegisterBinding();
        m_freeRegisters.add(result, registerWidth);
        return result;
    }

    static constexpr Register fromJSCReg(Reg reg)
    {
        // This pattern avoids an explicit template specialization in class scope, which GCC does not support.
        if constexpr (std::is_same_v<Register, GPRReg>) {
            ASSERT(reg.isGPR());
            return reg.gpr();
        } else if constexpr (std::is_same_v<Register, FPRReg>) {
            ASSERT(reg.isFPR());
            return reg.fpr();
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    class Spiller {
    public:
        Spiller()
        {
            m_hints.fill(invalidRegisterHint);
            m_lockCounts.fill(0);
        }

        void initialize(RegisterSet registers)
        {
            registers.forEach([&] (JSC::Reg r) {
                m_hints[fromJSCReg(r)] = 0;
            });
        }

        Register findMinHintRegisterToSpill()
        {
            int32_t minIndex = -1;
            SpillHint minHint = invalidRegisterHint;
            for (unsigned i = 0; i < m_hints.size(); ++i) {
                if (!m_lockCounts[i] && m_hints[i] < minHint) {
                    minHint = m_hints[i];
                    minIndex = i;
                }
            }

            RELEASE_ASSERT_WITH_MESSAGE(minIndex >= 0, "No remaining allocatable registers in Spiller");
            // This should be guaranteed by the above assert but doesn't hurt to double check.
            ASSERT(minHint != invalidRegisterHint);
            return static_cast<Register>(minIndex);
        }

        void setHint(Register reg, SpillHint newHint)
        {
            ASSERT(newHint < invalidRegisterHint);
            m_hints[reg] = newHint;
        }

        bool isLocked(Register reg) const { return m_lockCounts[reg]; }
        void lock(Register reg) { m_lockCounts[reg]++; }
        void unlock(Register reg) { m_lockCounts[reg]--; }

        std::array<SpillHint, RegisterBank::numberOfRegisters> m_hints;
        std::array<unsigned, RegisterBank::numberOfRegisters> m_lockCounts;
    };

    ASCIILiteral m_logPrefix; // non-empty means log.
    RegisterSet m_validRegisters;
    RegisterSet m_freeRegisters;
    Spiller m_spiller;
    RegisterBindings m_bindings;
};

} // namespace JSC

#endif // ENABLE(JIT)
