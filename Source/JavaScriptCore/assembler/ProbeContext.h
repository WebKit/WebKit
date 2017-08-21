/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "MacroAssembler.h"
#include "ProbeStack.h"

namespace JSC {
namespace Probe {

struct CPUState {
    using RegisterID = MacroAssembler::RegisterID;
    using SPRegisterID = MacroAssembler::SPRegisterID;
    using FPRegisterID = MacroAssembler::FPRegisterID;

    static inline const char* gprName(RegisterID id) { return MacroAssembler::gprName(id); }
    static inline const char* sprName(SPRegisterID id) { return MacroAssembler::sprName(id); }
    static inline const char* fprName(FPRegisterID id) { return MacroAssembler::fprName(id); }
    inline uintptr_t& gpr(RegisterID);
    inline uintptr_t& spr(SPRegisterID);
    inline double& fpr(FPRegisterID);

    template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    T gpr(RegisterID) const;
    template<typename T, typename std::enable_if<std::is_pointer<T>::value>::type* = nullptr>
    T gpr(RegisterID) const;
    template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    T spr(SPRegisterID) const;
    template<typename T, typename std::enable_if<std::is_pointer<T>::value>::type* = nullptr>
    T spr(SPRegisterID) const;
    template<typename T> T fpr(FPRegisterID) const;

    void*& pc();
    void*& fp();
    void*& sp();
    template<typename T> T pc() const;
    template<typename T> T fp() const;
    template<typename T> T sp() const;

    uintptr_t gprs[MacroAssembler::numberOfRegisters()];
    uintptr_t sprs[MacroAssembler::numberOfSPRegisters()];
    double fprs[MacroAssembler::numberOfFPRegisters()];
};

inline uintptr_t& CPUState::gpr(RegisterID id)
{
    ASSERT(id >= MacroAssembler::firstRegister() && id <= MacroAssembler::lastRegister());
    return gprs[id];
}

inline uintptr_t& CPUState::spr(SPRegisterID id)
{
    ASSERT(id >= MacroAssembler::firstSPRegister() && id <= MacroAssembler::lastSPRegister());
    return sprs[id];
}

inline double& CPUState::fpr(FPRegisterID id)
{
    ASSERT(id >= MacroAssembler::firstFPRegister() && id <= MacroAssembler::lastFPRegister());
    return fprs[id];
}

template<typename T, typename std::enable_if<std::is_integral<T>::value>::type*>
T CPUState::gpr(RegisterID id) const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return static_cast<T>(cpu->gpr(id));
}

template<typename T, typename std::enable_if<std::is_pointer<T>::value>::type*>
T CPUState::gpr(RegisterID id) const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return reinterpret_cast<T>(cpu->gpr(id));
}

template<typename T, typename std::enable_if<std::is_integral<T>::value>::type*>
T CPUState::spr(SPRegisterID id) const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return static_cast<T>(cpu->spr(id));
}

template<typename T, typename std::enable_if<std::is_pointer<T>::value>::type*>
T CPUState::spr(SPRegisterID id) const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return reinterpret_cast<T>(cpu->spr(id));
}

template<typename T>
T CPUState::fpr(FPRegisterID id) const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return bitwise_cast<T>(cpu->fpr(id));
}

inline void*& CPUState::pc()
{
#if CPU(X86) || CPU(X86_64)
    return *reinterpret_cast<void**>(&spr(X86Registers::eip));
#elif CPU(ARM64)
    return *reinterpret_cast<void**>(&spr(ARM64Registers::pc));
#elif CPU(ARM_THUMB2) || CPU(ARM_TRADITIONAL)
    return *reinterpret_cast<void**>(&gpr(ARMRegisters::pc));
#elif CPU(MIPS)
    RELEASE_ASSERT_NOT_REACHED();
#else
#error "Unsupported CPU"
#endif
}

inline void*& CPUState::fp()
{
#if CPU(X86) || CPU(X86_64)
    return *reinterpret_cast<void**>(&gpr(X86Registers::ebp));
#elif CPU(ARM64)
    return *reinterpret_cast<void**>(&gpr(ARM64Registers::fp));
#elif CPU(ARM_THUMB2) || CPU(ARM_TRADITIONAL)
    return *reinterpret_cast<void**>(&gpr(ARMRegisters::fp));
#elif CPU(MIPS)
    return *reinterpret_cast<void**>(&gpr(MIPSRegisters::fp));
#else
#error "Unsupported CPU"
#endif
}

inline void*& CPUState::sp()
{
#if CPU(X86) || CPU(X86_64)
    return *reinterpret_cast<void**>(&gpr(X86Registers::esp));
#elif CPU(ARM64)
    return *reinterpret_cast<void**>(&gpr(ARM64Registers::sp));
#elif CPU(ARM_THUMB2) || CPU(ARM_TRADITIONAL)
    return *reinterpret_cast<void**>(&gpr(ARMRegisters::sp));
#elif CPU(MIPS)
    return *reinterpret_cast<void**>(&gpr(MIPSRegisters::sp));
#else
#error "Unsupported CPU"
#endif
}

template<typename T>
T CPUState::pc() const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return reinterpret_cast<T>(cpu->pc());
}

template<typename T>
T CPUState::fp() const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return reinterpret_cast<T>(cpu->fp());
}

template<typename T>
T CPUState::sp() const
{
    CPUState* cpu = const_cast<CPUState*>(this);
    return reinterpret_cast<T>(cpu->sp());
}

struct State;
typedef void (*StackInitializationFunction)(State*);

struct State {
    Probe::Function probeFunction;
    void* arg;
    StackInitializationFunction initializeStackFunction;
    void* initializeStackArg;
    CPUState cpu;
};

class Context {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using RegisterID = MacroAssembler::RegisterID;
    using SPRegisterID = MacroAssembler::SPRegisterID;
    using FPRegisterID = MacroAssembler::FPRegisterID;

    Context(State* state)
        : m_state(state)
        , arg(state->arg)
        , cpu(state->cpu)
    { }

    uintptr_t& gpr(RegisterID id) { return m_state->cpu.gpr(id); }
    uintptr_t& spr(SPRegisterID id) { return m_state->cpu.spr(id); }
    double& fpr(FPRegisterID id) { return m_state->cpu.fpr(id); }
    const char* gprName(RegisterID id) { return m_state->cpu.gprName(id); }
    const char* sprName(SPRegisterID id) { return m_state->cpu.sprName(id); }
    const char* fprName(FPRegisterID id) { return m_state->cpu.fprName(id); }

    void*& pc() { return m_state->cpu.pc(); }
    void*& fp() { return m_state->cpu.fp(); }
    void*& sp() { return m_state->cpu.sp(); }

    template<typename T> T pc() { return m_state->cpu.pc<T>(); }
    template<typename T> T fp() { return m_state->cpu.fp<T>(); }
    template<typename T> T sp() { return m_state->cpu.sp<T>(); }

    Stack& stack()
    {
        ASSERT(m_stack.isValid());
        return m_stack;
    };

    bool hasWritesToFlush() { return m_stack.hasWritesToFlush(); }
    Stack* releaseStack() { return new Stack(WTFMove(m_stack)); }

private:
    State* m_state;
public:
    void* arg;
    CPUState& cpu;

private:
    Stack m_stack;

    friend JS_EXPORT_PRIVATE void* probeStateForContext(Context&); // Not for general use. This should only be for writing tests.
};

void executeProbe(State*);

} // namespace Probe

} // namespace JSC
