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

#include "LLIntExceptions.h"
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

template<typename Func>
inline void Callee::runWithDowncast(const Func& func)
{
    switch (m_compilationMode) {
    case CompilationMode::LLIntMode:
        func(static_cast<LLIntCallee*>(this));
        break;
#if ENABLE(WEBASSEMBLY_B3JIT)
    case CompilationMode::BBQMode:
        func(static_cast<BBQCallee*>(this));
        break;
    case CompilationMode::BBQForOSREntryMode:
        func(static_cast<OSREntryCallee*>(this));
        break;
    case CompilationMode::OMGMode:
        func(static_cast<OMGCallee*>(this));
        break;
    case CompilationMode::OMGForOSREntryMode:
        func(static_cast<OSREntryCallee*>(this));
        break;
#else
    case CompilationMode::BBQMode:
    case CompilationMode::BBQForOSREntryMode:
    case CompilationMode::OMGMode:
    case CompilationMode::OMGForOSREntryMode:
        break;
#endif
    case CompilationMode::EmbedderEntrypointMode:
        func(static_cast<EmbedderEntrypointCallee*>(this));
        break;
    }
}

template<typename Func>
inline void Callee::runWithDowncast(const Func& func) const
{
    const_cast<Callee*>(this)->runWithDowncast(func);
}

void Callee::dump(PrintStream& out) const
{
    out.print(makeString(m_indexOrName));
}

CodePtr<WasmEntryPtrTag> Callee::entrypoint() const
{
    CodePtr<WasmEntryPtrTag> codePtr;
    runWithDowncast([&](auto* derived) {
        codePtr = derived->entrypointImpl();
    });
    return codePtr;
}

std::tuple<void*, void*> Callee::range() const
{
    std::tuple<void*, void*> result;
    runWithDowncast([&](auto* derived) {
        result = derived->rangeImpl();
    });
    return result;
}

RegisterAtOffsetList* Callee::calleeSaveRegisters()
{
    RegisterAtOffsetList* result = nullptr;
    runWithDowncast([&](auto* derived) {
        result = derived->calleeSaveRegistersImpl();
    });
    return result;
}

void Callee::operator delete(Callee* callee, std::destroying_delete_t)
{
    CalleeRegistry::singleton().unregisterCallee(callee);
    callee->runWithDowncast([](auto* derived) {
        std::destroy_at(derived);
        std::decay_t<decltype(*derived)>::freeAfterDestruction(derived);
    });
}

const HandlerInfo* Callee::handlerForIndex(Instance& instance, unsigned index, const Tag* tag)
{
    ASSERT(hasExceptionHandlers());
    return HandlerInfo::handlerForIndex(instance, m_exceptionHandlers, index, tag);
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

LLIntCallee::LLIntCallee(FunctionCodeBlockGenerator& generator, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : Callee(Wasm::CompilationMode::LLIntMode, index, WTFMove(name))
    , m_functionIndex(generator.m_functionIndex)
    , m_numVars(generator.m_numVars)
    , m_numCalleeLocals(generator.m_numCalleeLocals)
    , m_numArguments(generator.m_numArguments)
    , m_constantTypes(WTFMove(generator.m_constantTypes))
    , m_constants(WTFMove(generator.m_constants))
    , m_instructions(WTFMove(generator.m_instructions))
    , m_instructionsRawPointer(generator.m_instructionsRawPointer)
    , m_jumpTargets(WTFMove(generator.m_jumpTargets))
    , m_signatures(WTFMove(generator.m_signatures))
    , m_outOfLineJumpTargets(WTFMove(generator.m_outOfLineJumpTargets))
    , m_tierUpCounter(WTFMove(generator.m_tierUpCounter))
    , m_jumpTables(WTFMove(generator.m_jumpTables))
{
    if (size_t count = generator.numberOfExceptionHandlers()) {
        m_exceptionHandlers = FixedVector<HandlerInfo>(count);
        for (size_t i = 0; i < count; i++) {
            const UnlinkedHandlerInfo& unlinkedHandler = generator.exceptionHandler(i);
            HandlerInfo& handler = m_exceptionHandlers[i];
            auto& instruction = *m_instructions->at(unlinkedHandler.m_target).ptr();
            CodeLocationLabel<ExceptionHandlerPtrTag> target;
            if (unlinkedHandler.m_type == HandlerType::Catch)
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmCatch(instruction.width()).code());
            else
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmCatchAll(instruction.width()).code());

            handler.initialize(unlinkedHandler, target);
        }
    }
}

RegisterAtOffsetList* LLIntCallee::calleeSaveRegistersImpl()
{
    static LazyNeverDestroyed<RegisterAtOffsetList> calleeSaveRegisters;
    static std::once_flag initializeFlag;
    std::call_once(initializeFlag, [] {
        RegisterSet registers;
        registers.add(GPRInfo::regCS0, IgnoreVectors); // Wasm::Instance
#if CPU(X86_64)
        registers.add(GPRInfo::regCS2, IgnoreVectors); // PB
#elif CPU(ARM64) || CPU(RISCV64)
        registers.add(GPRInfo::regCS7, IgnoreVectors); // PB
#elif CPU(ARM)
        registers.add(GPRInfo::regCS1, IgnoreVectors); // PB
#else
#error Unsupported architecture.
#endif
        ASSERT(registers.numberOfSetRegisters() == numberOfLLIntCalleeSaveRegisters);
        calleeSaveRegisters.construct(WTFMove(registers));
    });
    return &calleeSaveRegisters.get();
}

WasmInstructionStream::Offset LLIntCallee::outOfLineJumpOffset(WasmInstructionStream::Offset bytecodeOffset)
{
    ASSERT(m_outOfLineJumpTargets.contains(bytecodeOffset));
    return m_outOfLineJumpTargets.get(bytecodeOffset);
}

const WasmInstruction* LLIntCallee::outOfLineJumpTarget(const WasmInstruction* pc)
{
    int offset = bytecodeOffset(pc);
    int target = outOfLineJumpOffset(offset);
    return m_instructions->at(offset + target).ptr();
}

#if ENABLE(WEBASSEMBLY_B3JIT)
void OptimizingJITCallee::linkExceptionHandlers(Vector<UnlinkedHandlerInfo> unlinkedExceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations)
{
    size_t count = unlinkedExceptionHandlers.size();
    m_exceptionHandlers = FixedVector<HandlerInfo>(count);
    for (size_t i = 0; i < count; i++) {
        HandlerInfo& handler = m_exceptionHandlers[i];
        const UnlinkedHandlerInfo& unlinkedHandler = unlinkedExceptionHandlers[i];
        CodeLocationLabel<ExceptionHandlerPtrTag> location = exceptionHandlerLocations[i];
        handler.initialize(unlinkedHandler, location);
    }
}

const StackMap& OptimizingJITCallee::stackmap(CallSiteIndex callSiteIndex) const
{
    auto iter = m_stackmaps.find(callSiteIndex);
    if (iter == m_stackmaps.end()) {
        for (auto pair : m_stackmaps) {
            dataLog(pair.key.bits(), ": ");
            for (auto value : pair.value)
                dataLog(value, ", ");
            dataLogLn("");
        }
    }
    RELEASE_ASSERT(iter != m_stackmaps.end());
    return iter->value;
}
#endif

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
