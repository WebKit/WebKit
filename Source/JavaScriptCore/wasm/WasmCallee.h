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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "JITCompilation.h"
#include "RegisterAtOffsetList.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmHandlerInfo.h"
#include "WasmIndexOrName.h"
#include "WasmLLIntTierUpCounter.h"
#include "WasmTierUpCount.h"
#include <wtf/FixedVector.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

class Callee : public ThreadSafeRefCounted<Callee> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IndexOrName indexOrName() const { return m_indexOrName; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    CodePtr<WasmEntryPtrTag> entrypoint() const;
    RegisterAtOffsetList* calleeSaveRegisters();
    std::tuple<void*, void*> range() const;

    const HandlerInfo* handlerForIndex(Instance&, unsigned, const Tag*);

    bool hasExceptionHandlers() const { return !m_exceptionHandlers.isEmpty(); }

    void dump(PrintStream&) const;

    JS_EXPORT_PRIVATE void operator delete(Callee*, std::destroying_delete_t);

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

    template<typename Func>
    void runWithDowncast(const Func&);
    template<typename Func>
    void runWithDowncast(const Func&) const;

private:
    CompilationMode m_compilationMode;
    IndexOrName m_indexOrName;

protected:
    FixedVector<HandlerInfo> m_exceptionHandlers;
};

class JITCallee : public Callee {
public:
    friend class Callee;
    FixedVector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

protected:
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, Wasm::Entrypoint&&);
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, Wasm::Entrypoint&&, size_t, std::pair<const Name*, RefPtr<NameSection>>&&, Vector<UnlinkedWasmToWasmCall>&&);

    std::tuple<void*, void*> rangeImpl() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return &m_entrypoint.calleeSaveRegisters; }

private:
    FixedVector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
    Wasm::Entrypoint m_entrypoint;
};

class EmbedderEntrypointCallee final : public JITCallee {
public:
    static Ref<EmbedderEntrypointCallee> create(Wasm::Entrypoint&& entrypoint)
    {
        return adoptRef(*new EmbedderEntrypointCallee(WTFMove(entrypoint)));
    }

private:
    EmbedderEntrypointCallee(Wasm::Entrypoint&& entrypoint)
        : JITCallee(Wasm::CompilationMode::EmbedderEntrypointMode, WTFMove(entrypoint))
    {
    }
};


#if ENABLE(WEBASSEMBLY_B3JIT)
class OptimizingJITCallee : public JITCallee {
public:
    const StackMap& stackmap(CallSiteIndex) const;

protected:
    OptimizingJITCallee(Wasm::CompilationMode mode, Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& unlinkedExceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
        : JITCallee(mode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls))
        , m_stackmaps(WTFMove(stackmaps))
    {
        RELEASE_ASSERT(unlinkedExceptionHandlers.size() == exceptionHandlerLocations.size());
        linkExceptionHandlers(WTFMove(unlinkedExceptionHandlers), WTFMove(exceptionHandlerLocations));
    }

private:
    void linkExceptionHandlers(Vector<UnlinkedHandlerInfo>, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>);

    StackMaps m_stackmaps;
};

class OMGCallee final : public OptimizingJITCallee {
public:
    static Ref<OMGCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        return adoptRef(*new OMGCallee(WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations)));
    }

private:
    OMGCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
        : OptimizingJITCallee(Wasm::CompilationMode::OMGMode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations))
    {
    }
};

class OSREntryCallee final : public OptimizingJITCallee {
public:
    static Ref<OSREntryCallee> create(CompilationMode compilationMode, Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        return adoptRef(*new OSREntryCallee(compilationMode, WTFMove(entrypoint), index, WTFMove(name), osrEntryScratchBufferSize, loopIndex, WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations)));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }

private:
    OSREntryCallee(CompilationMode compilationMode, Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
        : OptimizingJITCallee(compilationMode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations))
        , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
        , m_loopIndex(loopIndex)
    {
    }

    unsigned m_osrEntryScratchBufferSize;
    uint32_t m_loopIndex;
};

class BBQCallee final : public OptimizingJITCallee {
public:
    static Ref<BBQCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations, Vector<CodeLocationLabel<WasmEntryPtrTag>>&& loopEntrypoints, unsigned osrEntryScratchBufferSize)
    {
        return adoptRef(*new BBQCallee(WTFMove(entrypoint), index, WTFMove(name), WTFMove(tierUpCount), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations), WTFMove(loopEntrypoints), osrEntryScratchBufferSize));
    }

    OSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OSREntryCallee>&& osrEntryCallee, MemoryMode)
    {
        m_osrEntryCallee = WTFMove(osrEntryCallee);
    }

    bool didStartCompilingOSREntryCallee() const { return m_didStartCompilingOSREntryCallee; }
    void setDidStartCompilingOSREntryCallee(bool value) { m_didStartCompilingOSREntryCallee = value; }

    OMGCallee* replacement() { return m_replacement.get(); }
    void setReplacement(Ref<OMGCallee>&& replacement)
    {
        m_replacement = WTFMove(replacement);
    }

    TierUpCount* tierUpCount() { return m_tierUpCount.get(); }

    const Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints() { return m_loopEntrypoints; }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }

private:
    BBQCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations, Vector<CodeLocationLabel<WasmEntryPtrTag>>&& loopEntrypoints, unsigned osrEntryScratchBufferSize)
        : OptimizingJITCallee(Wasm::CompilationMode::BBQMode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations))
        , m_tierUpCount(WTFMove(tierUpCount))
        , m_loopEntrypoints(WTFMove(loopEntrypoints))
        , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
    {
    }

    RefPtr<OSREntryCallee> m_osrEntryCallee;
    RefPtr<OMGCallee> m_replacement;
    std::unique_ptr<TierUpCount> m_tierUpCount;
    Vector<CodeLocationLabel<WasmEntryPtrTag>> m_loopEntrypoints;
    unsigned m_osrEntryScratchBufferSize { 0 };
    bool m_didStartCompilingOSREntryCallee { false };
};
#endif

class LLIntCallee final : public Callee {
    friend LLIntOffsetsExtractor;
    friend class Callee;
public:
    static Ref<LLIntCallee> create(FunctionCodeBlockGenerator& generator, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new LLIntCallee(generator, index, WTFMove(name)));
    }

    void setEntrypoint(CodePtr<WasmEntryPtrTag> entrypoint)
    {
        ASSERT(!m_entrypoint);
        m_entrypoint = entrypoint;
    }

    uint32_t functionIndex() const { return m_functionIndex; }
    unsigned numVars() const { return m_numVars; }
    unsigned numCalleeLocals() const { return m_numCalleeLocals; }
    uint32_t numArguments() const { return m_numArguments; }
    const FixedVector<Type>& constantTypes() const { return m_constantTypes; }
    const FixedVector<uint64_t>& constants() const { return m_constants; }
    const WasmInstructionStream& instructions() const { return *m_instructions; }

    ALWAYS_INLINE uint64_t getConstant(VirtualRegister reg) const { return m_constants[reg.toConstantIndex()]; }
    ALWAYS_INLINE Type getConstantType(VirtualRegister reg) const
    {
        ASSERT(Options::dumpGeneratedWasmBytecodes());
        return m_constantTypes[reg.toConstantIndex()];
    }

    WasmInstructionStream::Offset numberOfJumpTargets() { return m_jumpTargets.size(); }
    WasmInstructionStream::Offset lastJumpTarget() { return m_jumpTargets.last(); }

    const WasmInstruction* outOfLineJumpTarget(const WasmInstruction*);
    WasmInstructionStream::Offset outOfLineJumpOffset(WasmInstructionStream::Offset);
    WasmInstructionStream::Offset outOfLineJumpOffset(const WasmInstructionStream::Ref& instruction)
    {
        return outOfLineJumpOffset(instruction.offset());
    }

    inline WasmInstructionStream::Offset bytecodeOffset(const WasmInstruction* returnAddress)
    {
        const auto* instructionsBegin = m_instructions->at(0).ptr();
        const auto* instructionsEnd = reinterpret_cast<const WasmInstruction*>(reinterpret_cast<uintptr_t>(instructionsBegin) + m_instructions->size());
        RELEASE_ASSERT(returnAddress >= instructionsBegin && returnAddress < instructionsEnd);
        return returnAddress - instructionsBegin;
    }

    LLIntTierUpCounter& tierUpCounter() { return m_tierUpCounter; }

    const TypeDefinition& signature(unsigned index) const
    {
        return *m_signatures[index];
    }

    const JumpTable& jumpTable(unsigned tableIndex) const;
    unsigned numberOfJumpTables() const;

#if ENABLE(WEBASSEMBLY_B3JIT)
    JITCallee* replacement(MemoryMode mode) { return m_replacements[static_cast<uint8_t>(mode)].get(); }
    void setReplacement(Ref<OptimizingJITCallee>&& replacement, MemoryMode mode)
    {
        m_replacements[static_cast<uint8_t>(mode)] = WTFMove(replacement);
    }

    OSREntryCallee* osrEntryCallee(MemoryMode mode) { return m_osrEntryCallees[static_cast<uint8_t>(mode)].get(); }
    void setOSREntryCallee(Ref<OSREntryCallee>&& osrEntryCallee, MemoryMode mode)
    {
        m_osrEntryCallees[static_cast<uint8_t>(mode)] = WTFMove(osrEntryCallee);
    }
#endif

    using OutOfLineJumpTargets = HashMap<WasmInstructionStream::Offset, int>;

private:
    LLIntCallee(FunctionCodeBlockGenerator&, size_t index, std::pair<const Name*, RefPtr<NameSection>>&&);

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint; }
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; };
    JS_EXPORT_PRIVATE RegisterAtOffsetList* calleeSaveRegistersImpl();

    uint32_t m_functionIndex { 0 };

    // Used for the number of WebAssembly locals, as in https://webassembly.github.io/spec/core/syntax/modules.html#syntax-local
    unsigned m_numVars { 0 };
    // Number of VirtualRegister. The naming is unfortunate, but has to match UnlinkedCodeBlock
    unsigned m_numCalleeLocals { 0 };
    uint32_t m_numArguments { 0 };
    FixedVector<Type> m_constantTypes;
    FixedVector<uint64_t> m_constants;
    std::unique_ptr<WasmInstructionStream> m_instructions;
    const void* m_instructionsRawPointer { nullptr };
    FixedVector<WasmInstructionStream::Offset> m_jumpTargets;
    FixedVector<const TypeDefinition*> m_signatures;
    OutOfLineJumpTargets m_outOfLineJumpTargets;
    LLIntTierUpCounter m_tierUpCounter;
    FixedVector<JumpTable> m_jumpTables;

#if ENABLE(WEBASSEMBLY_B3JIT)
    RefPtr<OptimizingJITCallee> m_replacements[numberOfMemoryModes];
    RefPtr<OSREntryCallee> m_osrEntryCallees[numberOfMemoryModes];
#endif
    CodePtr<WasmEntryPtrTag> m_entrypoint;
};

using LLIntCallees = ThreadSafeRefCountedFixedVector<Ref<LLIntCallee>>;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
