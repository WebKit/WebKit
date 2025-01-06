/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include "InPlaceInterpreter.h"
#include "JSToWasm.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "LLIntThunks.h"
#include "NativeCalleeRegistry.h"
#include "VMInspector.h"
#include "WasmCallingConvention.h"
#include "WasmModuleInformation.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::Wasm {

WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(Callee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(JITCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(JSEntrypointCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(WasmToJSCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(JSToWasmICCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(OptimizingJITCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(OMGCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(OMGOSREntryCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(BBQCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(IPIntCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(LLIntCallee);

Callee::Callee(Wasm::CompilationMode compilationMode)
    : NativeCallee(NativeCallee::Category::Wasm, ImplementationVisibility::Private)
    , m_compilationMode(compilationMode)
    , m_index(0xBADBADBA)
{
}

Callee::Callee(Wasm::CompilationMode compilationMode, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : NativeCallee(NativeCallee::Category::Wasm, ImplementationVisibility::Public)
    , m_compilationMode(compilationMode)
    , m_index(index)
    , m_indexOrName(index, WTFMove(name))
{
}

void Callee::reportToVMsForDestruction()
{
    // We don't know which VMs a Module has ever run on so we just report to all of them.
    Locker locker(VMInspector::singleton().getLock());
    VMInspector::singleton().iterate([&] (VM& vm) {
        vm.heap.reportWasmCalleePendingDestruction(Ref(*this));
        return IterationStatus::Continue;
    });
}

template<typename Func>
inline void Callee::runWithDowncast(const Func& func)
{
    switch (m_compilationMode) {
    case CompilationMode::IPIntMode:
        func(static_cast<IPIntCallee*>(this));
        break;
    case CompilationMode::LLIntMode:
        func(static_cast<LLIntCallee*>(this));
        break;
    case CompilationMode::JSToWasmEntrypointMode:
        func(static_cast<JSEntrypointCallee*>(this));
        break;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    case CompilationMode::BBQMode:
        func(static_cast<BBQCallee*>(this));
        break;
#else
    case CompilationMode::BBQMode:
        break;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    case CompilationMode::OMGMode:
        func(static_cast<OMGCallee*>(this));
        break;
    case CompilationMode::OMGForOSREntryMode:
        func(static_cast<OMGOSREntryCallee*>(this));
        break;
#else
    case CompilationMode::OMGMode:
    case CompilationMode::OMGForOSREntryMode:
        break;
#endif
    case CompilationMode::JSToWasmICMode:
#if ENABLE(JIT)
        func(static_cast<JSToWasmICCallee*>(this));
#endif
        break;
    case CompilationMode::WasmToJSMode:
        func(static_cast<WasmToJSCallee*>(this));
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

void Callee::destroy(Callee* callee)
{
    callee->runWithDowncast([](auto* derived) {
        std::destroy_at(derived);
        std::decay_t<decltype(*derived)>::freeAfterDestruction(derived);
    });
}

const HandlerInfo* Callee::handlerForIndex(JSWebAssemblyInstance& instance, unsigned index, const Tag* tag)
{
    ASSERT(hasExceptionHandlers());
    return HandlerInfo::handlerForIndex(instance, m_exceptionHandlers, index, tag);
}

JITCallee::JITCallee(Wasm::CompilationMode compilationMode)
    : Callee(compilationMode)
{
}

JITCallee::JITCallee(Wasm::CompilationMode compilationMode, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : Callee(compilationMode, index, WTFMove(name))
{
}

#if ENABLE(JIT)
void JITCallee::setEntrypoint(Wasm::Entrypoint&& entrypoint)
{
    m_entrypoint = WTFMove(entrypoint);
    NativeCalleeRegistry::singleton().registerCallee(this);
}

void JSToWasmICCallee::setEntrypoint(MacroAssemblerCodeRef<JSEntryPtrTag>&& entrypoint)
{
    ASSERT(!m_jsToWasmICEntrypoint);
    m_jsToWasmICEntrypoint = WTFMove(entrypoint);
    NativeCalleeRegistry::singleton().registerCallee(this);
}
#endif

WasmToJSCallee::WasmToJSCallee()
    : Callee(Wasm::CompilationMode::WasmToJSMode)
{
    NativeCalleeRegistry::singleton().registerCallee(this);
}

WasmToJSCallee& WasmToJSCallee::singleton()
{
    static LazyNeverDestroyed<Ref<WasmToJSCallee>> callee;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&]() {
        callee.construct(adoptRef(*new WasmToJSCallee));
    });
    return callee.get().get();
}

IPIntCallee::IPIntCallee(FunctionIPIntMetadataGenerator& generator, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : Callee(Wasm::CompilationMode::IPIntMode, index, WTFMove(name))
    , m_functionIndex(generator.m_functionIndex)
    , m_signatures(WTFMove(generator.m_signatures))
    , m_bytecode(generator.m_bytecode.data() + generator.m_bytecodeOffset)
    , m_bytecodeEnd(m_bytecode + (generator.m_bytecode.size() - generator.m_bytecodeOffset - 1))
    , m_metadataVector(WTFMove(generator.m_metadata))
    , m_metadata(m_metadataVector.data())
    , m_argumINTBytecode(WTFMove(generator.m_argumINTBytecode))
    , m_argumINTBytecodePointer(m_argumINTBytecode.data())
    , m_uINTBytecode(WTFMove(generator.m_uINTBytecode))
    , m_uINTBytecodePointer(m_uINTBytecode.data())
    , m_highestReturnStackOffset(generator.m_highestReturnStackOffset)
    , m_localSizeToAlloc(roundUpToMultipleOf<2>(generator.m_numLocals))
    , m_numRethrowSlotsToAlloc(generator.m_numAlignedRethrowSlots)
    , m_numLocals(generator.m_numLocals)
    , m_numArgumentsOnStack(generator.m_numArgumentsOnStack)
    , m_maxFrameSizeInV128(generator.m_maxFrameSizeInV128)
    , m_tierUpCounter(WTFMove(generator.m_tierUpCounter))
{
    if (size_t count = generator.m_exceptionHandlers.size()) {
        m_exceptionHandlers = FixedVector<HandlerInfo>(count);
        for (size_t i = 0; i < count; i++) {
            const UnlinkedHandlerInfo& unlinkedHandler = generator.m_exceptionHandlers[i];
            HandlerInfo& handler = m_exceptionHandlers[i];
            void* ptr = nullptr;
            switch (unlinkedHandler.m_type) {
            case HandlerType::Catch:
                ptr = reinterpret_cast<void*>(ipint_catch_entry);
                break;
            case HandlerType::CatchAll:
            case HandlerType::Delegate:
                ptr = reinterpret_cast<void*>(ipint_catch_all_entry);
                break;
            case HandlerType::TryTableCatch:
                ptr = reinterpret_cast<void*>(ipint_table_catch_entry);
                break;
            case HandlerType::TryTableCatchRef:
                ptr = reinterpret_cast<void*>(ipint_table_catch_ref_entry);
                break;
            case HandlerType::TryTableCatchAll:
                ptr = reinterpret_cast<void*>(ipint_table_catch_all_entry);
                break;
            case HandlerType::TryTableCatchAllRef:
                ptr = reinterpret_cast<void*>(ipint_table_catch_allref_entry);
                break;
            }
            void* untagged = CodePtr<CFunctionPtrTag>::fromTaggedPtr(ptr).untaggedPtr();
            void* retagged = nullptr;
#if ENABLE(JIT_CAGE)
            if (Options::useJITCage())
#else
            if (false)
#endif
                retagged = tagCodePtr<ExceptionHandlerPtrTag>(untagged);
            else
                retagged = WTF::tagNativeCodePtrImpl<ExceptionHandlerPtrTag>(untagged);
            assertIsTaggedWith<ExceptionHandlerPtrTag>(retagged);

            CodeLocationLabel<ExceptionHandlerPtrTag> target(retagged);
            handler.initialize(unlinkedHandler, target);
        }
    }
}

void IPIntCallee::setEntrypoint(CodePtr<WasmEntryPtrTag> entrypoint)
{
    ASSERT(!m_entrypoint);
    m_entrypoint = entrypoint;
    NativeCalleeRegistry::singleton().registerCallee(this);
}

RegisterAtOffsetList* IPIntCallee::calleeSaveRegistersImpl()
{
    static LazyNeverDestroyed<RegisterAtOffsetList> calleeSaveRegisters;
    static std::once_flag initializeFlag;
    std::call_once(initializeFlag, [] {
        RegisterSet registers;
        registers.add(GPRInfo::regCS0, IgnoreVectors); // JSWebAssemblyInstance
#if CPU(X86_64)
        registers.add(GPRInfo::regCS1, IgnoreVectors); // PM (pointer to metadata)
        registers.add(GPRInfo::regCS2, IgnoreVectors); // PB
#elif CPU(ARM64) || CPU(RISCV64)
        registers.add(GPRInfo::regCS6, IgnoreVectors); // PM
        registers.add(GPRInfo::regCS7, IgnoreVectors); // PB
#elif CPU(ARM)
        registers.add(GPRInfo::regCS0, IgnoreVectors); // PM
        registers.add(GPRInfo::regCS1, IgnoreVectors); // PB
#else
#error Unsupported architecture.
#endif
        ASSERT(registers.numberOfSetRegisters() == numberOfIPIntCalleeSaveRegisters);
        calleeSaveRegisters.construct(WTFMove(registers));
    });
    return &calleeSaveRegisters.get();
}

LLIntCallee::LLIntCallee(FunctionCodeBlockGenerator& generator, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
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
            switch (unlinkedHandler.m_type) {
            case HandlerType::Catch:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmCatch(instruction.width()).code());
                break;
            case HandlerType::CatchAll:
            case HandlerType::Delegate:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmCatchAll(instruction.width()).code());
                break;
            case HandlerType::TryTableCatch:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmTryTable(wasm_try_table_catch, instruction.width()).code());
                break;
            case HandlerType::TryTableCatchRef:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmTryTable(wasm_try_table_catchref, instruction.width()).code());
                break;
            case HandlerType::TryTableCatchAll:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmTryTable(wasm_try_table_catchall, instruction.width()).code());
                break;
            case HandlerType::TryTableCatchAllRef:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleWasmTryTable(wasm_try_table_catchallref, instruction.width()).code());
                break;
            }

            handler.initialize(unlinkedHandler, target);
        }
    }
}

void LLIntCallee::setEntrypoint(CodePtr<WasmEntryPtrTag> entrypoint)
{
    ASSERT(!m_entrypoint);
    m_entrypoint = entrypoint;
    NativeCalleeRegistry::singleton().registerCallee(this);
}

RegisterAtOffsetList* LLIntCallee::calleeSaveRegistersImpl()
{
    static LazyNeverDestroyed<RegisterAtOffsetList> calleeSaveRegisters;
    static std::once_flag initializeFlag;
    std::call_once(initializeFlag, [] {
        RegisterSet registers;
        registers.add(GPRInfo::regCS0, IgnoreVectors); // JSWebAssemblyInstance
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

#if ENABLE(WEBASSEMBLY_OMGJIT)
void OptimizingJITCallee::addCodeOrigin(unsigned firstInlineCSI, unsigned lastInlineCSI, const Wasm::ModuleInformation& info, uint32_t functionIndex)
{
    if (!nameSections.size())
        nameSections.append(info.nameSection);
    // The inline frame list is stored in postorder. For example:
    // A { B() C() D { E() } F() } -> B C E D F A
#if ASSERT_ENABLED
    ASSERT(firstInlineCSI <= lastInlineCSI);
    for (unsigned i = 0; i + 1 < codeOrigins.size(); ++i)
        ASSERT(codeOrigins[i].lastInlineCSI <= codeOrigins[i + 1].lastInlineCSI);
    for (unsigned i = 0; i < codeOrigins.size(); ++i)
        ASSERT(codeOrigins[i].lastInlineCSI <= lastInlineCSI);
    ASSERT(nameSections.size() == 1);
    ASSERT(nameSections[0].ptr() == info.nameSection.ptr());
#endif
    codeOrigins.append({ firstInlineCSI, lastInlineCSI, functionIndex, 0 });
}

IndexOrName OptimizingJITCallee::getOrigin(unsigned csi, unsigned depth, bool& isInlined) const
{
    isInlined = false;
    auto iter = std::lower_bound(codeOrigins.begin(), codeOrigins.end(), WasmCodeOrigin { 0, csi, 0, 0 }, [&](const auto& a, const auto& b) {
        return b.lastInlineCSI - a.lastInlineCSI;
    });
    if (!iter || iter == codeOrigins.end())
        iter = codeOrigins.begin();
    while (iter != codeOrigins.end()) {
        if (iter->firstInlineCSI <= csi && iter->lastInlineCSI >= csi && !(depth--)) {
            isInlined = true;
            return IndexOrName(iter->functionIndex, nameSections[iter->moduleIndex]->get(iter->functionIndex));
        }
        ++iter;
    }

    return indexOrName();
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

JSEntrypointCallee::JSEntrypointCallee(TypeIndex typeIndex, bool usesSIMD)
    : Callee(Wasm::CompilationMode::JSToWasmEntrypointMode)
    , m_typeIndex(typeIndex)
{
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);

    RegisterAtOffsetList savedResultRegisters = wasmFrameConvention.computeResultsOffsetList();
    size_t totalFrameSize = wasmFrameConvention.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.sizeOfAreaInBytes();
    totalFrameSize += JSEntrypointCallee::RegisterStackSpaceAligned;
    totalFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(totalFrameSize);
    m_frameSize = totalFrameSize;

#if ENABLE(JIT)
    if (Options::useWasmJIT()) {
#else
    if (false) {
#endif
        if (Options::useWasmIPInt())
            if (usesSIMD)
                m_wasmFunctionPrologue = LLInt::inPlaceInterpreterSIMDEntryThunk().code().retagged<WasmEntryPtrTag>();
            else
                m_wasmFunctionPrologue = LLInt::inPlaceInterpreterEntryThunk().code().retagged<WasmEntryPtrTag>();
        else {
            if (usesSIMD)
                m_wasmFunctionPrologue = LLInt::wasmFunctionEntryThunkSIMD().code().retagged<WasmEntryPtrTag>();
            else
                m_wasmFunctionPrologue = LLInt::wasmFunctionEntryThunk().code().retagged<WasmEntryPtrTag>();
        }
    } else {
        if (Options::useWasmIPInt())
            m_wasmFunctionPrologue = CodePtr<CFunctionPtrTag>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(ipint_trampoline)).retagged<WasmEntryPtrTag>();
        else {
            if (usesSIMD)
                m_wasmFunctionPrologue = CodePtr<CFunctionPtrTag>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(wasm_function_prologue_simd_trampoline)).retagged<WasmEntryPtrTag>();
            else
                m_wasmFunctionPrologue = CodePtr<CFunctionPtrTag>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(wasm_function_prologue_trampoline)).retagged<WasmEntryPtrTag>();
        }
    }
}

CodePtr<WasmEntryPtrTag> JSEntrypointCallee::entrypointImpl() const
{
#if ENABLE(JIT)
    if (Options::useWasmJIT())
        return createJSToWasmJITShared().retaggedCode<WasmEntryPtrTag>();
#endif
    return LLInt::getCodeFunctionPtr<CFunctionPtrTag>(js_to_wasm_wrapper_entry);
}

RegisterAtOffsetList* JSEntrypointCallee::calleeSaveRegistersImpl()
{
    // This must be the same to JSToWasm's callee save registers.
    // The reason is that we may use m_replacementCallee which can be set at any time.
    // So, we must store the same callee save registers at the same location to the JIT version.
    static LazyNeverDestroyed<RegisterAtOffsetList> calleeSaveRegisters;
    static std::once_flag initializeFlag;
    std::call_once(initializeFlag, [] {
        RegisterSet registers = RegisterSetBuilder::wasmPinnedRegisters();
#if CPU(X86_64)
#elif CPU(ARM64) || CPU(RISCV64)
        ASSERT(registers.numberOfSetRegisters() == 3);
#elif CPU(ARM)
#else
#error Unsupported architecture.
#endif
        calleeSaveRegisters.construct(WTFMove(registers));
    });
    ASSERT(WTF::roundUpToMultipleOf<stackAlignmentBytes()>(calleeSaveRegisters->sizeOfAreaInBytes()) == SpillStackSpaceAligned);
    return &calleeSaveRegisters.get();
}

#if ENABLE(WEBASSEMBLY_BBQJIT)

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

BBQCallee::~BBQCallee()
{
    if (Options::freeRetiredWasmCode() && m_osrEntryCallee) {
        ASSERT(m_osrEntryCallee->hasOneRef());
        m_osrEntryCallee->reportToVMsForDestruction();
    }
}

#endif

} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
