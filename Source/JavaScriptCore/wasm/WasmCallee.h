/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "NativeCallee.h"
#include "RegisterAtOffsetList.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmFunctionIPIntMetadataGenerator.h"
#include "WasmHandlerInfo.h"
#include "WasmIPIntGenerator.h"
#include "WasmIPIntTierUpCounter.h"
#include "WasmIndexOrName.h"
#include "WasmLLIntTierUpCounter.h"
#include "WasmTierUpCount.h"
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/FixedVector.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

class Callee : public NativeCallee {
    WTF_MAKE_TZONE_ALLOCATED(Callee);
public:
    IndexOrName indexOrName() const { return m_indexOrName; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    CodePtr<WasmEntryPtrTag> entrypoint() const;
    RegisterAtOffsetList* calleeSaveRegisters();
    std::tuple<void*, void*> range() const;

    const HandlerInfo* handlerForIndex(Instance&, unsigned, const Tag*);

    bool hasExceptionHandlers() const { return !m_exceptionHandlers.isEmpty(); }

    void dump(PrintStream&) const;

    static void destroy(Callee*);

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

    template<typename Func>
    void runWithDowncast(const Func&);
    template<typename Func>
    void runWithDowncast(const Func&) const;

private:
    const CompilationMode m_compilationMode;
    const IndexOrName m_indexOrName;

protected:
    FixedVector<HandlerInfo> m_exceptionHandlers;
};

#if ENABLE(JIT)
class JITCallee : public Callee {
    WTF_MAKE_TZONE_ALLOCATED(JITCallee);
public:
    friend class Callee;
    FixedVector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

protected:
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

    std::tuple<void*, void*> rangeImpl() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return &m_entrypoint.calleeSaveRegisters; }

    void setEntrypoint(Wasm::Entrypoint&&);

    FixedVector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
    Wasm::Entrypoint m_entrypoint;
};

class JSEntrypointCallee final : public JITCallee {
    WTF_MAKE_TZONE_ALLOCATED(JSEntrypointCallee);
public:
    static Ref<JSEntrypointCallee> create()
    {
        return adoptRef(*new JSEntrypointCallee);
    }

    using JITCallee::setEntrypoint;

private:
    JSEntrypointCallee()
        : JITCallee(Wasm::CompilationMode::JSEntrypointMode)
    {
    }
};

#else

class JSEntrypointCallee final : public Callee {
    WTF_MAKE_TZONE_ALLOCATED(JSEntrypointCallee);
public:
    friend class Callee;

    static Ref<JSEntrypointCallee> create()
    {
        return adoptRef(*new JSEntrypointCallee);
    }

private:
    JSEntrypointCallee()
        : Callee(Wasm::CompilationMode::JSEntrypointMode)
    {
    }

    std::tuple<void*, void*> rangeImpl() const
    {
        return { nullptr, nullptr };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
};
#endif // ENABLE(JIT)

class WasmToJSCallee final : public Callee {
    WTF_MAKE_TZONE_ALLOCATED(WasmToJSCallee);
public:
    friend class Callee;

    static Ref<WasmToJSCallee> create()
    {
        return adoptRef(*new WasmToJSCallee);
    }

private:
    WasmToJSCallee();

    std::tuple<void*, void*> rangeImpl() const
    {
        return { nullptr, nullptr };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
};

#if ENABLE(JIT)
class JSToWasmICCallee final : public JITCallee {
    WTF_MAKE_TZONE_ALLOCATED(JSToWasmICCallee);
public:
    static Ref<JSToWasmICCallee> create()
    {
        return adoptRef(*new JSToWasmICCallee);
    }

    using JITCallee::setEntrypoint;

private:
    JSToWasmICCallee()
        : JITCallee(Wasm::CompilationMode::JSToWasmICMode)
    {
    }
};
#endif

#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)

struct WasmCodeOrigin {
    unsigned firstInlineCSI;
    unsigned lastInlineCSI;
    unsigned functionIndex;
    unsigned moduleIndex;
};

class OptimizingJITCallee : public JITCallee {
    WTF_MAKE_TZONE_ALLOCATED(OptimizingJITCallee);
public:
    const StackMap& stackmap(CallSiteIndex) const;

    void addCodeOrigin(unsigned firstInlineCSI, unsigned lastInlineCSI, const Wasm::ModuleInformation&, uint32_t functionIndex);
    IndexOrName getOrigin(unsigned csi, unsigned depth, bool& isInlined) const;

protected:
    OptimizingJITCallee(Wasm::CompilationMode mode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
        : JITCallee(mode, index, WTFMove(name))
    {
    }

    void setEntrypoint(Wasm::Entrypoint&& entrypoint, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& unlinkedExceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        m_wasmToWasmCallsites = WTFMove(unlinkedCalls);
        m_stackmaps = WTFMove(stackmaps);
        RELEASE_ASSERT(unlinkedExceptionHandlers.size() == exceptionHandlerLocations.size());
        linkExceptionHandlers(WTFMove(unlinkedExceptionHandlers), WTFMove(exceptionHandlerLocations));
        JITCallee::setEntrypoint(WTFMove(entrypoint));
    }

private:
    void linkExceptionHandlers(Vector<UnlinkedHandlerInfo>, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>);

    StackMaps m_stackmaps;
    Vector<WasmCodeOrigin, 0> codeOrigins;
    Vector<Ref<NameSection>, 0> nameSections;
};

class OSREntryCallee final : public OptimizingJITCallee {
    WTF_MAKE_TZONE_ALLOCATED(OSREntryCallee);
public:
    static Ref<OSREntryCallee> create(CompilationMode compilationMode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, uint32_t loopIndex)
    {
        return adoptRef(*new OSREntryCallee(compilationMode, index, WTFMove(name), loopIndex));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }


    void setEntrypoint(Wasm::Entrypoint&& entrypoint, unsigned osrEntryScratchBufferSize, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        m_osrEntryScratchBufferSize = osrEntryScratchBufferSize;
        OptimizingJITCallee::setEntrypoint(WTFMove(entrypoint), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations));
    }

    void setStackCheckSize(unsigned stackCheckSize) { m_stackCheckSize = stackCheckSize; }
    unsigned stackCheckSize() const { return m_stackCheckSize; }

private:
    OSREntryCallee(CompilationMode compilationMode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, uint32_t loopIndex)
        : OptimizingJITCallee(compilationMode, index, WTFMove(name))
        , m_loopIndex(loopIndex)
    {
    }

    unsigned m_osrEntryScratchBufferSize { 0 };
    uint32_t m_loopIndex;
    unsigned m_stackCheckSize { 0 };
};

#endif // ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)

#if ENABLE(WEBASSEMBLY_OMGJIT)

class OMGCallee final : public OptimizingJITCallee {
public:
    static Ref<OMGCallee> create(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new OMGCallee(index, WTFMove(name)));
    }

    using OptimizingJITCallee::setEntrypoint;

private:
    OMGCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
        : OptimizingJITCallee(Wasm::CompilationMode::OMGMode, index, WTFMove(name))
    {
    }
};

#endif // ENABLE(WEBASSEMBLY_OMGJIT)

#if ENABLE(WEBASSEMBLY_BBQJIT)


class BBQCallee final : public OptimizingJITCallee {
    WTF_MAKE_TZONE_ALLOCATED(BBQCallee);
public:
    static constexpr unsigned extraOSRValuesForLoopIndex = 1;

    static Ref<BBQCallee> create(size_t index, std::pair<const Name*,
        RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, SavedFPWidth savedFPWidth)
    {
        return adoptRef(*new BBQCallee(index, WTFMove(name), WTFMove(tierUpCount), savedFPWidth));
    }

    OSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OSREntryCallee>&& osrEntryCallee, MemoryMode)
    {
        m_osrEntryCallee = WTFMove(osrEntryCallee);
    }

    bool didStartCompilingOSREntryCallee() const { return m_didStartCompilingOSREntryCallee; }
    void setDidStartCompilingOSREntryCallee(bool value) { m_didStartCompilingOSREntryCallee = value; }

#if ENABLE(WEBASSEMBLY_OMGJIT)
    OMGCallee* replacement() { return m_replacement.get(); }
    void setReplacement(Ref<OMGCallee>&& replacement)
    {
        m_replacement = WTFMove(replacement);
    }
#else

#endif

    TierUpCount* tierUpCount() { return m_tierUpCount.get(); }

    std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint() { return m_sharedLoopEntrypoint; }
    const Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints() { return m_loopEntrypoints; }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    SavedFPWidth savedFPWidth() const { return m_savedFPWidth; }

    void setEntrypoint(Wasm::Entrypoint&& entrypoint, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations, Vector<CodeLocationLabel<WasmEntryPtrTag>>&& loopEntrypoints, std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint, unsigned osrEntryScratchBufferSize)
    {
        m_sharedLoopEntrypoint = sharedLoopEntrypoint;
        m_loopEntrypoints = WTFMove(loopEntrypoints);
        m_osrEntryScratchBufferSize = osrEntryScratchBufferSize;
        OptimizingJITCallee::setEntrypoint(WTFMove(entrypoint), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations));
        m_switchJumpTables.shrinkToFit();
    }

    EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>* addJumpTable(unsigned size)
    {
        m_switchJumpTables.append(EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>::create(size));
        return m_switchJumpTables.last().ptr();
    }

    void setStackCheckSize(unsigned stackCheckSize) { m_stackCheckSize = stackCheckSize; }
    unsigned stackCheckSize() const { return m_stackCheckSize; }

private:
    BBQCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, SavedFPWidth savedFPWidth)
        : OptimizingJITCallee(Wasm::CompilationMode::BBQMode, index, WTFMove(name))
        , m_tierUpCount(WTFMove(tierUpCount))
        , m_savedFPWidth(savedFPWidth)
    {
    }

    RefPtr<OSREntryCallee> m_osrEntryCallee;
#if ENABLE(WEBASSEMBLY_OMGJIT)
    RefPtr<OMGCallee> m_replacement;
#endif
    std::unique_ptr<TierUpCount> m_tierUpCount;
    std::optional<CodeLocationLabel<WasmEntryPtrTag>> m_sharedLoopEntrypoint;
    Vector<CodeLocationLabel<WasmEntryPtrTag>> m_loopEntrypoints;
    unsigned m_osrEntryScratchBufferSize { 0 };
    unsigned m_stackCheckSize { 0 };
    bool m_didStartCompilingOSREntryCallee { false };
    SavedFPWidth m_savedFPWidth { SavedFPWidth::DontSaveVectors };
    Vector<UniqueRef<EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>>> m_switchJumpTables;
};
#endif


class IPIntCallee final : public Callee {
    WTF_MAKE_TZONE_ALLOCATED(IPIntCallee);
    friend class JSC::LLIntOffsetsExtractor;
    friend class Callee;
public:
    static Ref<IPIntCallee> create(FunctionIPIntMetadataGenerator& generator, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new IPIntCallee(generator, index, WTFMove(name)));
    }

    uint32_t functionIndex() const { return m_functionIndex; }
    void setEntrypoint(CodePtr<WasmEntryPtrTag>);
    const uint8_t* getBytecode() const { return m_bytecode; }
    const uint8_t* getMetadata() const { return m_metadata; }

    const TypeDefinition& signature(unsigned index) const
    {
        return *m_signatures[index];
    }

    IPIntTierUpCounter& tierUpCounter() { return m_tierUpCounter; }

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
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
    IPIntCallee(FunctionIPIntMetadataGenerator&, size_t index, std::pair<const Name*, RefPtr<NameSection>>&&);

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint; }
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; };
    JS_EXPORT_PRIVATE RegisterAtOffsetList* calleeSaveRegistersImpl();

    uint32_t m_functionIndex { 0 };
#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<OptimizingJITCallee> m_replacements[numberOfMemoryModes];
    RefPtr<OSREntryCallee> m_osrEntryCallees[numberOfMemoryModes];
#endif
    CodePtr<WasmEntryPtrTag> m_entrypoint;
    FixedVector<const TypeDefinition*> m_signatures;
public:
    // I couldn't figure out how to stop LLIntOffsetsExtractor.cpp from yelling at me.
    // So just making these public.
    const uint8_t* m_bytecode;
    const uint32_t m_bytecodeLength;
    Vector<uint8_t> m_metadataVector;
    const uint8_t* m_metadata;
    Vector<uint8_t> m_argumINTBytecode;
    const uint8_t* m_argumINTBytecodePointer;
    const uint32_t m_returnMetadata;

    unsigned m_localSizeToAlloc;
    unsigned m_numRethrowSlotsToAlloc;
    unsigned m_numLocals;
    unsigned m_numArgumentsOnStack;

    IPIntTierUpCounter m_tierUpCounter;
};

class LLIntCallee final : public Callee {
    WTF_MAKE_TZONE_ALLOCATED(LLIntCallee);
    friend JSC::LLIntOffsetsExtractor;
    friend class Callee;
public:
    static Ref<LLIntCallee> create(FunctionCodeBlockGenerator& generator, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new LLIntCallee(generator, index, WTFMove(name)));
    }

    void setEntrypoint(CodePtr<WasmEntryPtrTag>);

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

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
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

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<OptimizingJITCallee> m_replacements[numberOfMemoryModes];
    RefPtr<OSREntryCallee> m_osrEntryCallees[numberOfMemoryModes];
#endif
    CodePtr<WasmEntryPtrTag> m_entrypoint;
};

using LLIntCallees = ThreadSafeRefCountedFixedVector<Ref<LLIntCallee>>;
using IPIntCallees = ThreadSafeRefCountedFixedVector<Ref<IPIntCallee>>;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
