/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WasmCallee.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCalleeRegistry.h"
#include "WasmCallingConvention.h"

namespace JSC { namespace Wasm {

Callee::Callee(Wasm::CompilationMode compilationMode)
    : m_compilationMode(compilationMode)
{
    CalleeRegistry::singleton().registerCallee(this);
}

Callee::Callee(Wasm::CompilationMode compilationMode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : m_compilationMode(compilationMode)
    , m_indexOrName(index, WTFMove(name))
{
    CalleeRegistry::singleton().registerCallee(this);
}

Callee::~Callee()
{
    CalleeRegistry::singleton().unregisterCallee(this);
}

void Callee::dump(PrintStream& out) const
{
    out.print(makeString(m_indexOrName));
}

JITCallee::JITCallee(Wasm::CompilationMode compilationMode, Entrypoint&& entrypoint)
    : Callee(compilationMode)
    , m_entrypoint(WTFMove(entrypoint))
{
}

JITCallee::JITCallee(Wasm::CompilationMode compilationMode, Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
    : Callee(compilationMode, index, WTFMove(name))
    , m_wasmToWasmCallsites(WTFMove(unlinkedCalls))
    , m_entrypoint(WTFMove(entrypoint))
{
}

void LLIntCallee::setEntrypoint(MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint)
{
    m_entrypoint = entrypoint;
}

MacroAssemblerCodePtr<WasmEntryPtrTag> LLIntCallee::entrypoint() const
{
    return m_entrypoint;
}

RegisterAtOffsetList* LLIntCallee::calleeSaveRegisters()
{
    static LazyNeverDestroyed<RegisterAtOffsetList> calleeSaveRegisters;
    static std::once_flag initializeFlag;
    std::call_once(initializeFlag, [] {
        RegisterSet registers;
        registers.set(GPRInfo::regCS0); // Wasm::Instance
#if CPU(X86_64)
        registers.set(GPRInfo::regCS2); // PB
#elif CPU(ARM64)
        registers.set(GPRInfo::regCS7); // PB
#else
#error Unsupported architecture.
#endif
        ASSERT(registers.numberOfSetRegisters() == numberOfLLIntCalleeSaveRegisters);
        calleeSaveRegisters.construct(WTFMove(registers));
    });
    return &calleeSaveRegisters.get();
}

std::tuple<void*, void*> LLIntCallee::range() const
{
    return { nullptr, nullptr };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
