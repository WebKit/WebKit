/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CodeBlock.h"

#include "ArithProfile.h"
#include "BasicBlockLocation.h"
#include "BytecodeDumper.h"
#include "BytecodeLivenessAnalysisInlines.h"
#include "BytecodeOperandsForCheckpoint.h"
#include "BytecodeStructs.h"
#include "CodeBlockInlines.h"
#include "CodeBlockSet.h"
#include "ControlFlowProfiler.h"
#include "DFGCapabilities.h"
#include "DFGCommon.h"
#include "DFGJITCode.h"
#include "EvalCodeBlock.h"
#include "FullCodeOrigin.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutableDump.h"
#include "GetPutInfo.h"
#include "InlineCallFrame.h"
#include "Instruction.h"
#include "InstructionStream.h"
#include "IsoCellSetInlines.h"
#include "JIT.h"
#include "JITMathIC.h"
#include "JITWorklist.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSSet.h"
#include "JSString.h"
#include "JSTemplateObjectDescriptor.h"
#include "LLIntData.h"
#include "LLIntEntrypoint.h"
#include "LLIntExceptions.h"
#include "LLIntPrototypeLoadAdaptiveStructureWatchpoint.h"
#include "MetadataTable.h"
#include "ModuleProgramCodeBlock.h"
#include "ObjectAllocationProfileInlines.h"
#include "PCToCodeOriginMap.h"
#include "ProfilerDatabase.h"
#include "ProgramCodeBlock.h"
#include "ReduceWhitespace.h"
#include "SlotVisitorInlines.h"
#include "StackVisitor.h"
#include "StructureStubInfo.h"
#include "TypeLocationCache.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/Forward.h>
#include <wtf/SimpleStats.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/UniquedStringImpl.h>

#if ENABLE(ASSEMBLER)
#include "RegisterAtOffsetList.h"
#endif

#if ENABLE(FTL_JIT)
#include "FTLJITCode.h"
#endif

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CodeBlockRareData);

const ClassInfo CodeBlock::s_info = {
    "CodeBlock"_s, nullptr, nullptr, nullptr,
    CREATE_METHOD_TABLE(CodeBlock)
};

CString CodeBlock::inferredName() const
{
    switch (codeType()) {
    case GlobalCode:
        return "<global>"_span;
    case EvalCode:
        return "<eval>"_span;
    case FunctionCode:
        return jsCast<FunctionExecutable*>(ownerExecutable())->ecmaName().utf8();
    case ModuleCode:
        return "<module>"_span;
    default:
        CRASH();
        return ""_span;
    }
}

bool CodeBlock::hasHash() const
{
    return !!m_hash;
}

bool CodeBlock::isSafeToComputeHash() const
{
    return !isCompilationThread();
}

CodeBlockHash CodeBlock::hash() const
{
    if (!m_hash) {
        RELEASE_ASSERT(isSafeToComputeHash());
        m_hash = CodeBlockHash(ownerExecutable()->source(), specializationKind());
    }
    return m_hash;
}

CString CodeBlock::sourceCodeForTools() const
{
    if (codeType() != FunctionCode)
        return ownerExecutable()->source().toUTF8();

    FunctionExecutable* executable = jsCast<FunctionExecutable*>(ownerExecutable());
    return executable->source().provider()->getRange(
        executable->functionStart(),
        executable->parametersStartOffset() + executable->source().length()).utf8();
}

CString CodeBlock::sourceCodeOnOneLine() const
{
    return reduceWhitespace(sourceCodeForTools());
}

CString CodeBlock::hashAsStringIfPossible() const
{
    if (hasHash() || isSafeToComputeHash())
        return toCString(hash());
    return "<no-hash>";
}

void CodeBlock::dumpAssumingJITType(PrintStream& out, JITType jitType) const
{
    out.print(inferredName(), "#", hashAsStringIfPossible());
    out.print(":[", RawPointer(this), "->");
    if (!!m_alternative)
        out.print(RawPointer(alternative()), "->");
    out.print(RawPointer(ownerExecutable()), ", ", jitType, codeType());

    if (codeType() == FunctionCode)
        out.print(specializationKind());
    out.print(", ", instructionsSize());
    if (this->jitType() == JITType::BaselineJIT && m_shouldAlwaysBeInlined)
        out.print(" (ShouldAlwaysBeInlined)");
    if (ownerExecutable()->neverInline())
        out.print(" (NeverInline)");
    if (ownerExecutable()->neverOptimize())
        out.print(" (NeverOptimize)");
    else if (ownerExecutable()->neverFTLOptimize())
        out.print(" (NeverFTLOptimize)");
    if (ownerExecutable()->didTryToEnterInLoop())
        out.print(" (DidTryToEnterInLoop)");
    if (ownerExecutable()->isInStrictContext())
        out.print(" (StrictMode)");
    if (m_didFailJITCompilation)
        out.print(" (JITFail)");
    if (this->jitType() == JITType::BaselineJIT && m_didFailFTLCompilation)
        out.print(" (FTLFail)");
    if (this->jitType() == JITType::BaselineJIT && m_hasBeenCompiledWithFTL)
        out.print(" (HadFTLReplacement)");
    out.print("]");
}

void CodeBlock::dump(PrintStream& out) const
{
    dumpAssumingJITType(out, jitType());
}

void CodeBlock::dumpSimpleName(PrintStream& out) const
{
    out.print(inferredName(), "#", hashAsStringIfPossible());
}

void CodeBlock::dumpSource()
{
    dumpSource(WTF::dataFile());
}

void CodeBlock::dumpSource(PrintStream& out)
{
    ScriptExecutable* executable = ownerExecutable();
    if (executable->isFunctionExecutable()) {
        auto functionExecutable = static_cast<FunctionExecutable*>(executable);
        StringView source = functionExecutable->source().provider()->getRange(
            functionExecutable->parametersStartOffset(),
            functionExecutable->functionEnd() + 1); // Type profiling end offset is the character before the '}'.
        
        out.print("function ", inferredName(), source);
        return;
    }
    out.print(executable->source().view());
}

void CodeBlock::dumpBytecode()
{
    dumpBytecode(WTF::dataFile());
}

void CodeBlock::dumpBytecode(PrintStream& out)
{
    ICStatusMap statusMap;
    getICStatusMap(statusMap);
    BytecodeGraph graph(this, this->instructions());
    CodeBlockBytecodeDumper<CodeBlock>::dumpGraph(this, instructions(), graph, out, statusMap);
}

void CodeBlock::dumpBytecode(PrintStream& out, const JSInstructionStream::Ref& it, const ICStatusMap& statusMap)
{
    BytecodeDumper<CodeBlock>::dumpBytecode(this, out, it, statusMap);
}

void CodeBlock::dumpBytecode(PrintStream& out, unsigned bytecodeOffset, const ICStatusMap& statusMap)
{
    const auto it = instructions().at(bytecodeOffset);
    dumpBytecode(out, it, statusMap);
}

namespace {

class PutToScopeFireDetail final : public FireDetail {
public:
    PutToScopeFireDetail(CodeBlock* codeBlock, const Identifier& ident)
        : m_codeBlock(codeBlock)
        , m_ident(ident)
    {
    }
    
    void dump(PrintStream& out) const final
    {
        out.print("Linking put_to_scope in ", FunctionExecutableDump(jsCast<FunctionExecutable*>(m_codeBlock->ownerExecutable())), " for ", m_ident);
    }
    
private:
    CodeBlock* const m_codeBlock;
    const Identifier& m_ident;
};

} // anonymous namespace

CodeBlock::CodeBlock(VM& vm, Structure* structure, CopyParsedBlockTag, CodeBlock& other)
    : JSCell(vm, structure)
    , m_globalObject(other.m_globalObject)
    , m_shouldAlwaysBeInlined(true)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
    , m_didFailJITCompilation(false)
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isJettisoned(false)
    , m_numCalleeLocals(other.m_numCalleeLocals)
    , m_numVars(other.m_numVars)
    , m_numberOfArgumentsToSkip(other.m_numberOfArgumentsToSkip)
    , m_couldBeTainted(other.m_couldBeTainted)
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_bytecodeCost(other.m_bytecodeCost)
    , m_scopeRegister(other.m_scopeRegister)
    , m_hash(other.m_hash)
    , m_unlinkedCode(other.vm(), this, other.m_unlinkedCode.get())
    , m_ownerExecutable(other.vm(), this, other.m_ownerExecutable.get())
    , m_vm(other.m_vm)
    , m_instructionsRawPointer(other.m_instructionsRawPointer)
    , m_metadata(other.m_metadata)
    , m_constantRegisters(other.m_constantRegisters)
    , m_functionDecls(other.m_functionDecls)
    , m_functionExprs(other.m_functionExprs)
    , m_creationTime(ApproximateTime::now())
#if ASSERT_ENABLED
    , m_magic(CODEBLOCK_MAGIC)
#endif
{
    ASSERT(heap()->isDeferred());
    ASSERT(m_scopeRegister.isLocal());

    ASSERT(source().provider());
    constexpr bool allocateArgumentValueProfiles = false;
    setNumParameters(other.numParameters(), allocateArgumentValueProfiles);

    ASSERT(m_couldBeTainted == (taintednessToTriState(source().provider()->sourceTaintedOrigin()) != TriState::False));
    vm.heap.codeBlockSet().add(this);
}

void CodeBlock::finishCreation(VM& vm, CopyParsedBlockTag, CodeBlock& other)
{
    Base::finishCreation(vm);

    if (other.m_rareData) {
        createRareDataIfNecessary();
        m_rareData->m_exceptionHandlers = other.m_rareData->m_exceptionHandlers;
    }
}

CodeBlock::CodeBlock(VM& vm, Structure* structure, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock, JSScope* scope)
    : JSCell(vm, structure)
    , m_globalObject(scope->globalObject(), WriteBarrierEarlyInit)
    , m_shouldAlwaysBeInlined(true)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
    , m_didFailJITCompilation(false)
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isJettisoned(false)
    , m_numCalleeLocals(unlinkedCodeBlock->numCalleeLocals())
    , m_numVars(unlinkedCodeBlock->numVars())
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_scopeRegister(unlinkedCodeBlock->scopeRegister())
    , m_unlinkedCode(unlinkedCodeBlock, WriteBarrierEarlyInit)
    , m_ownerExecutable(ownerExecutable, WriteBarrierEarlyInit)
    , m_vm(&vm)
    , m_instructionsRawPointer(unlinkedCodeBlock->instructions().rawPointer())
    , m_metadata(unlinkedCodeBlock->metadata().link())
    , m_creationTime(ApproximateTime::now())
#if ASSERT_ENABLED
    , m_magic(CODEBLOCK_MAGIC)
#endif
{
    ASSERT(heap()->isDeferred());
    ASSERT(m_scopeRegister.isLocal());

    ASSERT(source().provider());
    constexpr bool allocateArgumentValueProfiles = true;
    setNumParameters(unlinkedCodeBlock->numParameters(), allocateArgumentValueProfiles);

    m_couldBeTainted = source().provider()->couldBeTainted();
    vm.heap.codeBlockSet().add(this);
}

// The main purpose of this function is to generate linked bytecode from unlinked bytecode. The process
// of linking is taking an abstract representation of bytecode and tying it to a GlobalObject and scope
// chain. For example, this process allows us to cache the depth of lexical environment reads that reach
// outside of this CodeBlock's compilation unit. It also allows us to generate particular constants that
// we can't generate during unlinked bytecode generation. This process is not allowed to generate control
// flow or introduce new locals. The reason for this is we rely on liveness analysis to be the same for
// all the CodeBlocks of an UnlinkedCodeBlock. We rely on this fact by caching the liveness analysis
// inside UnlinkedCodeBlock. Also, Baseline JIT code is shared between all CodeBlocks of an UnlinkedCodeBlock,
// so the bytecode must remain the same between CodeBlocks sharing an UnlinkedCodeBlock.
bool CodeBlock::finishCreation(VM& vm, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock,
    JSScope* scope)
{
    Base::finishCreation(vm);

    ASSERT(vm.heap.isDeferred());

    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (m_unlinkedCode->wasCompiledWithTypeProfilerOpcodes() || m_unlinkedCode->wasCompiledWithControlFlowProfilerOpcodes())
        vm.functionHasExecutedCache()->removeUnexecutedRange(ownerExecutable->sourceID(), ownerExecutable->typeProfilingStartOffset(), ownerExecutable->typeProfilingEndOffset());

    ScriptExecutable* topLevelExecutable = ownerExecutable->topLevelExecutable();
    // We wait to initialize template objects until the end of finishCreation beecause it can
    // throw. We rely on linking to put the CodeBlock into a coherent state, so we can't throw
    // until we're all done linking.
    Vector<unsigned> templateObjectIndices = setConstantRegisters(unlinkedCodeBlock->constantRegisters(), unlinkedCodeBlock->constantsSourceCodeRepresentation());

    // We already have the cloned symbol table for the module environment since we need to instantiate
    // the module environments before linking the code block. We replace the stored symbol table with the already cloned one.
    if (UnlinkedModuleProgramCodeBlock* unlinkedModuleProgramCodeBlock = jsDynamicCast<UnlinkedModuleProgramCodeBlock*>(unlinkedCodeBlock)) {
        SymbolTable* clonedSymbolTable = jsCast<ModuleProgramExecutable*>(ownerExecutable)->moduleEnvironmentSymbolTable();
        if (m_unlinkedCode->wasCompiledWithTypeProfilerOpcodes()) {
            ConcurrentJSLocker locker(clonedSymbolTable->m_lock);
            clonedSymbolTable->prepareForTypeProfiling(locker);
        }
        replaceConstant(VirtualRegister(unlinkedModuleProgramCodeBlock->moduleEnvironmentSymbolTableConstantRegisterOffset()), clonedSymbolTable);
    }

    bool shouldUpdateFunctionHasExecutedCache = m_unlinkedCode->wasCompiledWithTypeProfilerOpcodes() || m_unlinkedCode->wasCompiledWithControlFlowProfilerOpcodes();
    m_functionDecls = FixedVector<WriteBarrier<FunctionExecutable>>(unlinkedCodeBlock->numberOfFunctionDecls());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionDecls(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionDecl(i);
        if (shouldUpdateFunctionHasExecutedCache)
            vm.functionHasExecutedCache()->insertUnexecutedRange(ownerExecutable->sourceID(), unlinkedExecutable->unlinkedFunctionStart(), unlinkedExecutable->unlinkedFunctionEnd());
        m_functionDecls[i].set(vm, this, unlinkedExecutable->link(vm, topLevelExecutable, ownerExecutable->source(), std::nullopt, NoIntrinsic, ownerExecutable->isInsideOrdinaryFunction()));
    }

    m_functionExprs = FixedVector<WriteBarrier<FunctionExecutable>>(unlinkedCodeBlock->numberOfFunctionExprs());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionExprs(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionExpr(i);
        if (shouldUpdateFunctionHasExecutedCache)
            vm.functionHasExecutedCache()->insertUnexecutedRange(ownerExecutable->sourceID(), unlinkedExecutable->unlinkedFunctionStart(), unlinkedExecutable->unlinkedFunctionEnd());
        m_functionExprs[i].set(vm, this, unlinkedExecutable->link(vm, topLevelExecutable, ownerExecutable->source(), std::nullopt, NoIntrinsic, ownerExecutable->isInsideOrdinaryFunction()));
    }

    if (unlinkedCodeBlock->numberOfExceptionHandlers()) {
        createRareDataIfNecessary();

        if (size_t count = unlinkedCodeBlock->numberOfExceptionHandlers()) {
            m_rareData->m_exceptionHandlers.resizeToFit(count);
            for (size_t i = 0; i < count; i++) {
                const UnlinkedHandlerInfo& unlinkedHandler = unlinkedCodeBlock->exceptionHandler(i);
                HandlerInfo& handler = m_rareData->m_exceptionHandlers[i];
#if ENABLE(JIT)
                auto& instruction = *instructions().at(unlinkedHandler.target).ptr();
                handler.initialize(unlinkedHandler, CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::handleCatch(instruction.width()).code()));
#else
                handler.initialize(unlinkedHandler);
#endif
            }
        }
    }

    // Bookkeep the strongly referenced module environments.
    HashSet<JSModuleEnvironment*> stronglyReferencedModuleEnvironments;

    auto link_objectAllocationProfile = [&](const auto& /*instruction*/, auto bytecode, auto& metadata) {
        metadata.m_objectAllocationProfile.initializeProfile(vm, m_globalObject.get(), this, m_globalObject->objectPrototype(), bytecode.m_inlineCapacity);
    };

    auto link_arrayAllocationProfile = [&](const auto& /*instruction*/, auto bytecode, auto& metadata) {
        metadata.m_arrayAllocationProfile.initializeIndexingMode(bytecode.m_recommendedIndexingType);
    };

    auto link_callLinkInfo = [&](const auto& instruction, auto bytecode, auto& metadata) {
        metadata.m_callLinkInfo.initialize(vm, this, CallLinkInfo::callTypeFor(decltype(bytecode)::opcodeID), CodeOrigin { instruction.index() });
    };

#define LINK_FIELD(__field) \
    WTF_LAZY_JOIN(link_, __field)(instruction, bytecode, metadata);

#define INITIALIZE_METADATA(__op) \
    auto bytecode = instruction->as<__op>(); \
    auto& metadata = bytecode.metadata(this); \
    new (&metadata) __op::Metadata { bytecode }; \

#define LINK_IMPL(...) \
        INITIALIZE_METADATA(WTF_LAZY_FIRST(__VA_ARGS__)) \
        WTF_LAZY_HAS_REST(__VA_ARGS__)({ \
            WTF_LAZY_FOR_EACH_TERM(LINK_FIELD,  WTF_LAZY_REST_(__VA_ARGS__)) \
        }) \

#define CASE(__op) case __op::opcodeID

#define LINK(...) \
    CASE(WTF_LAZY_FIRST(__VA_ARGS__)): { \
        LINK_IMPL(__VA_ARGS__) \
        break; \
    }

    const auto& instructionStream = instructions();
    for (const auto& instruction : instructionStream) {
        OpcodeID opcodeID = instruction->opcodeID();
        static_assert(OpcodeIDWidthBySize<JSOpcodeTraits, OpcodeSize::Wide32>::opcodeIDSize == 1);
        m_bytecodeCost += opcodeLengths[opcodeID] + 1;
        switch (opcodeID) {
        LINK(OpGetByVal)
        LINK(OpGetPrivateName)

        LINK(OpTryGetById)
        LINK(OpGetByIdDirect)
        LINK(OpGetByValWithThis)
        LINK(OpToThis)

        LINK(OpGetById)
        LINK(OpGetLength)

        LINK(OpEnumeratorNext)
        LINK(OpEnumeratorInByVal)
        LINK(OpEnumeratorHasOwnProperty)
        LINK(OpEnumeratorGetByVal)

        LINK(OpInByVal)
        LINK(OpPutByVal)
        LINK(OpPutByValDirect)
        LINK(OpEnumeratorPutByVal)
        LINK(OpPutPrivateName)

        LINK(OpSetPrivateBrand)
        LINK(OpCheckPrivateBrand)

        LINK(OpNewArray)
        LINK(OpNewArrayWithSize)
        LINK(OpNewArrayBuffer, arrayAllocationProfile)

        LINK(OpNewObject, objectAllocationProfile)

        LINK(OpPutById)
        LINK(OpCreateThis)
        LINK(OpCreatePromise)
        LINK(OpCreateGenerator)

        LINK(OpJneqPtr)

        LINK(OpCatch)
        LINK(OpProfileControlFlow)

        LINK(OpCall, callLinkInfo)
        LINK(OpTailCall, callLinkInfo)
        LINK(OpCallDirectEval, callLinkInfo)
        LINK(OpConstruct, callLinkInfo)
        LINK(OpSuperConstruct, callLinkInfo)
        LINK(OpIteratorOpen, callLinkInfo)
        LINK(OpIteratorNext, callLinkInfo)
        LINK(OpCallVarargs, callLinkInfo)
        LINK(OpTailCallVarargs, callLinkInfo)
        LINK(OpTailCallForwardArguments, callLinkInfo)
        LINK(OpConstructVarargs, callLinkInfo)
        LINK(OpSuperConstructVarargs, callLinkInfo)
        LINK(OpCallIgnoreResult, callLinkInfo)

        case op_new_array_with_species: {
            INITIALIZE_METADATA(OpNewArrayWithSpecies)
            break;
        }

        case op_resolve_scope: {
            INITIALIZE_METADATA(OpResolveScope)

            const Identifier& ident = identifier(bytecode.m_var);
            RELEASE_ASSERT(bytecode.m_resolveType != ResolvedClosureVar);

            ResolveOp op = JSScope::abstractResolve(m_globalObject.get(), bytecode.m_localScopeDepth, scope, ident, Get, bytecode.m_resolveType, InitializationMode::NotInitialization);

            metadata.m_resolveType = op.type;
            metadata.m_localScopeDepth = op.depth;
            if (op.lexicalEnvironment) {
                if (op.type == ModuleVar) {
                    // Keep the linked module environment strongly referenced.
                    if (stronglyReferencedModuleEnvironments.add(jsCast<JSModuleEnvironment*>(op.lexicalEnvironment)).isNewEntry)
                        addConstant(ConcurrentJSLocker(m_lock), op.lexicalEnvironment);
                    metadata.m_lexicalEnvironment.set(vm, this, op.lexicalEnvironment);
                } else
                    metadata.m_symbolTable.set(vm, this, op.lexicalEnvironment->symbolTable());
            } else if (JSScope* constantScope = JSScope::constantScopeForCodeBlock(op.type, this)) {
                metadata.m_constantScope.set(vm, this, constantScope);
                if (op.type == GlobalProperty || op.type == GlobalPropertyWithVarInjectionChecks)
                    metadata.m_globalLexicalBindingEpoch = m_globalObject->globalLexicalBindingEpoch();
            } else
                metadata.m_globalObject.clear();
            break;
        }

        case op_get_from_scope: {
            INITIALIZE_METADATA(OpGetFromScope)

            metadata.m_watchpointSet = nullptr;

            ASSERT(!isInitialization(bytecode.m_getPutInfo.initializationMode()));
            if (bytecode.m_getPutInfo.resolveType() == ResolvedClosureVar) {
                metadata.m_getPutInfo = GetPutInfo(bytecode.m_getPutInfo.resolveMode(), ClosureVar, bytecode.m_getPutInfo.initializationMode(), bytecode.m_getPutInfo.ecmaMode());
                break;
            }

            const Identifier& ident = identifier(bytecode.m_var);
            ResolveOp op = JSScope::abstractResolve(m_globalObject.get(), bytecode.m_localScopeDepth, scope, ident, Get, bytecode.m_getPutInfo.resolveType(), InitializationMode::NotInitialization);

            metadata.m_getPutInfo = GetPutInfo(bytecode.m_getPutInfo.resolveMode(), op.type, bytecode.m_getPutInfo.initializationMode(), bytecode.m_getPutInfo.ecmaMode());
            if (op.type == ModuleVar)
                metadata.m_getPutInfo = GetPutInfo(bytecode.m_getPutInfo.resolveMode(), ClosureVar, bytecode.m_getPutInfo.initializationMode(), bytecode.m_getPutInfo.ecmaMode());
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks || op.type == GlobalLexicalVar || op.type == GlobalLexicalVarWithVarInjectionChecks)
                metadata.m_watchpointSet = op.watchpointSet;
            else if (op.structure)
                metadata.m_structure.set(vm, this, op.structure);
            metadata.m_operand = op.operand;
            break;
        }

        case op_put_to_scope: {
            INITIALIZE_METADATA(OpPutToScope)

            if (bytecode.m_getPutInfo.resolveType() == ResolvedClosureVar) {
                // Only do watching if the property we're putting to is not anonymous.
                if (bytecode.m_var != UINT_MAX) {
                    SymbolTable* symbolTable = jsCast<SymbolTable*>(getConstant(bytecode.m_symbolTableOrScopeDepth.symbolTable()));
                    const Identifier& ident = identifier(bytecode.m_var);
                    ConcurrentJSLocker locker(symbolTable->m_lock);
                    auto iter = symbolTable->find(locker, ident.impl());
                    ASSERT(iter != symbolTable->end(locker));
                    if (bytecode.m_getPutInfo.initializationMode() == InitializationMode::ScopedArgumentInitialization) {
                        ASSERT(bytecode.m_value.isArgument());
                        unsigned argumentIndex = bytecode.m_value.toArgument() - 1;
                        symbolTable->prepareToWatchScopedArgument(iter->value, argumentIndex);
                    } else
                        iter->value.prepareToWatch();
                    metadata.m_watchpointSet = iter->value.watchpointSet();
                } else
                    metadata.m_watchpointSet = nullptr;
                break;
            }

            const Identifier& ident = identifier(bytecode.m_var);
            metadata.m_watchpointSet = nullptr;
            ResolveOp op = JSScope::abstractResolve(m_globalObject.get(), bytecode.m_symbolTableOrScopeDepth.scopeDepth(), scope, ident, Put, bytecode.m_getPutInfo.resolveType(), bytecode.m_getPutInfo.initializationMode());

            metadata.m_getPutInfo = GetPutInfo(bytecode.m_getPutInfo.resolveMode(), op.type, bytecode.m_getPutInfo.initializationMode(), bytecode.m_getPutInfo.ecmaMode());
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks || op.type == GlobalLexicalVar || op.type == GlobalLexicalVarWithVarInjectionChecks)
                metadata.m_watchpointSet = op.watchpointSet;
            else if (op.type == ClosureVar || op.type == ClosureVarWithVarInjectionChecks) {
                if (op.watchpointSet)
                    op.watchpointSet->invalidate(vm, PutToScopeFireDetail(this, ident));
            } else if (op.structure)
                metadata.m_structure.set(vm, this, op.structure);
            metadata.m_operand = op.operand;
            break;
        }

        case op_profile_type: {
            RELEASE_ASSERT(m_unlinkedCode->wasCompiledWithTypeProfilerOpcodes());

            INITIALIZE_METADATA(OpProfileType)

            size_t instructionOffset = instruction.offset() + instruction->size() - 1;
            unsigned divotStart, divotEnd;
            GlobalVariableID globalVariableID = 0;
            RefPtr<TypeSet> globalTypeSet;
            bool shouldAnalyze = m_unlinkedCode->typeProfilerExpressionInfoForBytecodeOffset(instructionOffset, divotStart, divotEnd);
            SymbolTable* symbolTable = nullptr;

            switch (bytecode.m_flag) {
            case ProfileTypeBytecodeClosureVar: {
                const Identifier& ident = identifier(bytecode.m_identifier);
                unsigned localScopeDepth = bytecode.m_symbolTableOrScopeDepth.scopeDepth();
                // Even though type profiling may be profiling either a Get or a Put, we can always claim a Get because
                // we're abstractly "read"ing from a JSScope.
                ResolveOp op = JSScope::abstractResolve(m_globalObject.get(), localScopeDepth, scope, ident, Get, bytecode.m_resolveType, InitializationMode::NotInitialization);

                if (op.type == ClosureVar || op.type == ModuleVar)
                    symbolTable = op.lexicalEnvironment->symbolTable();
                else if (op.type == GlobalVar)
                    symbolTable = m_globalObject.get()->symbolTable();

                UniquedStringImpl* impl = (op.type == ModuleVar) ? op.importedName.get() : ident.impl();
                if (symbolTable) {
                    ConcurrentJSLocker locker(symbolTable->m_lock);
                    // If our parent scope was created while profiling was disabled, it will not have prepared for profiling yet.
                    symbolTable->prepareForTypeProfiling(locker);
                    globalVariableID = symbolTable->uniqueIDForVariable(locker, impl, vm);
                    globalTypeSet = symbolTable->globalTypeSetForVariable(locker, impl, vm);
                } else
                    globalVariableID = TypeProfilerNoGlobalIDExists;

                break;
            }
            case ProfileTypeBytecodeLocallyResolved: {
                SymbolTable* symbolTable = jsCast<SymbolTable*>(getConstant(bytecode.m_symbolTableOrScopeDepth.symbolTable()));
                const Identifier& ident = identifier(bytecode.m_identifier);
                ConcurrentJSLocker locker(symbolTable->m_lock);
                // If our parent scope was created while profiling was disabled, it will not have prepared for profiling yet.
                globalVariableID = symbolTable->uniqueIDForVariable(locker, ident.impl(), vm);
                globalTypeSet = symbolTable->globalTypeSetForVariable(locker, ident.impl(), vm);

                break;
            }
            case ProfileTypeBytecodeDoesNotHaveGlobalID: 
            case ProfileTypeBytecodeFunctionArgument: {
                globalVariableID = TypeProfilerNoGlobalIDExists;
                break;
            }
            case ProfileTypeBytecodeFunctionReturnStatement: {
                RELEASE_ASSERT(ownerExecutable->isFunctionExecutable());
                globalTypeSet = jsCast<FunctionExecutable*>(ownerExecutable)->returnStatementTypeSet();
                globalVariableID = TypeProfilerReturnStatement;
                if (!shouldAnalyze) {
                    // Because a return statement can be added implicitly to return undefined at the end of a function,
                    // and these nodes don't emit expression ranges because they aren't in the actual source text of
                    // the user's program, give the type profiler some range to identify these return statements.
                    // Currently, the text offset that is used as identification is "f" in the function keyword
                    // and is stored on TypeLocation's m_divotForFunctionOffsetIfReturnStatement member variable.
                    divotStart = divotEnd = ownerExecutable->typeProfilingStartOffset();
                    shouldAnalyze = true;
                }
                break;
            }
            }

            std::pair<TypeLocation*, bool> locationPair = vm.typeProfiler()->typeLocationCache()->getTypeLocation(globalVariableID,
                ownerExecutable->sourceID(), divotStart, divotEnd, WTFMove(globalTypeSet), &vm);
            TypeLocation* location = locationPair.first;
            bool isNewLocation = locationPair.second;

            if (bytecode.m_flag == ProfileTypeBytecodeFunctionReturnStatement)
                location->m_divotForFunctionOffsetIfReturnStatement = ownerExecutable->typeProfilingStartOffset();

            if (shouldAnalyze && isNewLocation)
                vm.typeProfiler()->insertNewLocation(location);

            metadata.m_typeLocation = location;
            break;
        }

        case op_debug: {
            if (instruction->as<OpDebug>().m_debugHookType == DidReachDebuggerStatement)
                m_hasDebuggerStatement = true;
            break;
        }

        case op_create_rest: {
            int numberOfArgumentsToSkip = instruction->as<OpCreateRest>().m_numParametersToSkip;
            ASSERT_UNUSED(numberOfArgumentsToSkip, numberOfArgumentsToSkip >= 0);
            // This is used when rematerializing the rest parameter during OSR exit in the FTL JIT.");
            m_numberOfArgumentsToSkip = numberOfArgumentsToSkip;
            break;
        }

        default:
            break;
        }
    }

#undef CASE
#undef INITIALIZE_METADATA
#undef LINK_FIELD
#undef LINK

    if (m_unlinkedCode->wasCompiledWithControlFlowProfilerOpcodes())
        insertBasicBlockBoundariesForControlFlowProfiler();

    // If the concurrent thread will want the code block's hash, then compute it here
    // synchronously.
    if (Options::alwaysComputeHash())
        hash();

    if (Options::dumpGeneratedBytecodes())
        dumpBytecode();

    if (m_metadata)
        vm.heap.reportExtraMemoryAllocated(this, m_metadata->sizeInBytesForGC());

    initializeTemplateObjects(topLevelExecutable, templateObjectIndices);
    RETURN_IF_EXCEPTION(throwScope, false);

    return true;
}

#if ENABLE(JIT)
void CodeBlock::setupWithUnlinkedBaselineCode(Ref<BaselineJITCode> jitCode)
{
    setJITCode(jitCode.copyRef());

    {
        const auto& jitCodeMap = this->jitCodeMap();
        for (size_t i = 0; i < numberOfExceptionHandlers(); ++i) {
            HandlerInfo& handler = exceptionHandler(i);
            // FIXME: <rdar://problem/39433318>.
            handler.nativeCode = jitCodeMap.find(BytecodeIndex(handler.target)).retagged<ExceptionHandlerPtrTag>();
        }
    }

    {
        ASSERT(!m_jitData);
        auto baselineJITData = BaselineJITData::create(jitCode->m_unlinkedStubInfos.size(), jitCode->m_constantPool.size(), this);

        for (unsigned index = 0; index < jitCode->m_unlinkedStubInfos.size(); ++index) {
            BaselineUnlinkedStructureStubInfo& unlinkedStubInfo = jitCode->m_unlinkedStubInfos[index];
            auto& stubInfo = baselineJITData->stubInfo(index);
            stubInfo.initializeFromUnlinkedStructureStubInfo(vm(), this, unlinkedStubInfo);
        }

        for (size_t i = 0; i < jitCode->m_constantPool.size(); ++i) {
            auto entry = jitCode->m_constantPool.at(i);
            switch (entry.type()) {
            case JITConstantPool::Type::FunctionDecl: {
                unsigned index = bitwise_cast<uintptr_t>(entry.pointer());
                baselineJITData->trailingSpan()[i] = functionDecl(index);
                break;
            }
            case JITConstantPool::Type::FunctionExpr: {
                unsigned index = bitwise_cast<uintptr_t>(entry.pointer());
                baselineJITData->trailingSpan()[i] = functionExpr(index);
                break;
            }
            }
        }
        setBaselineJITData(WTFMove(baselineJITData));

        // Set optimization thresholds only after instructions is initialized and JITData is initialized, since these
        // rely on the instruction count (and are in theory permitted to also inspect the instruction stream to more accurate assess the cost of tier-up).
        // And the data is stored in JITData.
        optimizeAfterWarmUp();
    }

    switch (codeType()) {
    case GlobalCode:
    case ModuleCode:
    case EvalCode:
        m_shouldAlwaysBeInlined = false;
        break;
    case FunctionCode:
        // We could have already set it to false because we detected an uninlineable call.
        // Don't override that observation.
        m_shouldAlwaysBeInlined &= canInline(capabilityLevel()) && DFG::mightInlineFunction(JITType::FTLJIT, this);
        break;
    }

    if (jitCode->m_isShareable && !unlinkedCodeBlock()->m_unlinkedBaselineCode && Options::useBaselineJITCodeSharing())
        unlinkedCodeBlock()->m_unlinkedBaselineCode = WTFMove(jitCode);
}
#endif // ENABLE(JIT)

CodeBlock::~CodeBlock()
{
    VM& vm = *m_vm;

    if (JITCode::isBaselineCode(jitType())) {
        if (m_metadata) {
            m_metadata->forEach<OpCatch>([&](auto& metadata) {
                if (metadata.m_buffer)
                    ValueProfileAndVirtualRegisterBuffer::destroy(std::exchange(metadata.m_buffer, nullptr));
            });
        }
    }

#if ENABLE(DFG_JIT)
    // The JITCode (and its corresponding DFG::CommonData) may outlive the CodeBlock by
    // a short amount of time after the CodeBlock is destructed. For example, the
    // Interpreter::execute methods will ref JITCode before invoking it. This can
    // result in the JITCode having a non-zero refCount when its owner CodeBlock is
    // destructed.
    //
    // Hence, we cannot rely on DFG::CommonData destruction to clear these now invalid
    // watchpoints in a timely manner. We'll ensure they are cleared here eagerly.
    //
    // We only need to do this for a DFG/FTL CodeBlock because only these will have a
    // DFG:CommonData. Hence, the LLInt and Baseline will not have any of these watchpoints.
    //
    // Note also that the LLIntPrototypeLoadAdaptiveStructureWatchpoint is also related
    // to the CodeBlock. However, its lifecycle is tied directly to the CodeBlock, and
    // will be automatically cleared when the CodeBlock destructs.

    if (JSC::JITCode::isOptimizingJIT(jitType()))
        jitCode()->dfgCommon()->clearWatchpoints();
#endif

    if (UNLIKELY(vm.m_perBytecodeProfiler))
        vm.m_perBytecodeProfiler->notifyDestruction(this);

    if (LIKELY(!vm.heap.isShuttingDown())) {
        // FIXME: This check should really not be necessary, see https://webkit.org/b/272787
        ASSERT(!m_metadata || m_metadata->unlinkedMetadata());
        if (m_metadata && !m_metadata->isDestroyed()) {
            auto unlinkedMetadata = m_metadata->unlinkedMetadata();
            if (unlinkedMetadata->didOptimize() == TriState::Indeterminate)
                unlinkedMetadata->setDidOptimize(TriState::False);
        }
    }

#if ENABLE(VERBOSE_VALUE_PROFILE)
    dumpValueProfiles();
#endif

    // We may be destroyed before any CodeBlocks that refer to us are destroyed.
    // Consider that two CodeBlocks become unreachable at the same time. There
    // is no guarantee about the order in which the CodeBlocks are destroyed.
    // So, if we don't remove incoming calls, and get destroyed before the
    // CodeBlock(s) that have calls into us, then the CallLinkInfo vector's
    // destructor will try to remove nodes from our (no longer valid) linked list.
    unlinkOrUpgradeIncomingCalls(vm, nullptr);
    
    // Note that our outgoing calls will be removed from other CodeBlocks'
    // m_incomingCalls linked lists through the execution of the ~CallLinkInfo
    // destructors.

#if ENABLE(JIT)
    forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
        stubInfo.aboutToDie();
        stubInfo.deref();
        return IterationStatus::Continue;
    });
    if (JSC::JITCode::isOptimizingJIT(jitType())) {
#if ENABLE(DFG_JIT)
        if (auto* jitData = dfgJITData()) {
            m_jitData = nullptr;
            delete jitData;
        }
#endif
    } else {
        if (auto* jitData = baselineJITData()) {
            m_jitData = nullptr;
            delete jitData;
        }
    }
#endif // ENABLE(JIT)
}

bool CodeBlock::isConstantOwnedByUnlinkedCodeBlock(VirtualRegister reg) const
{
    // This needs to correspond to what we do inside setConstantRegisters.
    switch (unlinkedCodeBlock()->constantSourceCodeRepresentation(reg)) {
    case SourceCodeRepresentation::Integer:
    case SourceCodeRepresentation::Double:
        return true;
    case SourceCodeRepresentation::Other: {
        JSValue value = unlinkedCodeBlock()->getConstant(reg);
        if (!value || !value.isCell())
            return true;
        JSCell* cell = value.asCell();
        if (cell->inherits<SymbolTable>() || cell->inherits<JSTemplateObjectDescriptor>())
            return false;
        return true;
    }
    case SourceCodeRepresentation::LinkTimeConstant:
        return false;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

Vector<unsigned> CodeBlock::setConstantRegisters(const FixedVector<WriteBarrier<Unknown>>& constants, const FixedVector<SourceCodeRepresentation>& constantsSourceCodeRepresentation)
{
    VM& vm = *m_vm;
    JSGlobalObject* globalObject = m_globalObject.get();

    Vector<unsigned> templateObjectIndices;

    ASSERT(constants.size() == constantsSourceCodeRepresentation.size());
    size_t count = constants.size();
    {
        ConcurrentJSLocker locker(m_lock);
        m_constantRegisters.resizeToFit(count);
    }
    for (size_t i = 0; i < count; i++) {
        JSValue constant = constants[i].get();
        SourceCodeRepresentation representation = constantsSourceCodeRepresentation[i];
        switch (representation) {
        case SourceCodeRepresentation::LinkTimeConstant:
            constant = globalObject->linkTimeConstant(static_cast<LinkTimeConstant>(constant.asInt32AsAnyInt()));
            ASSERT(constant.isCell()); // Unlinked Baseline JIT requires this.
            break;
        case SourceCodeRepresentation::Other:
        case SourceCodeRepresentation::Integer:
        case SourceCodeRepresentation::Double:
            if (!constant.isEmpty()) {
                if (constant.isCell()) {
                    JSCell* cell = constant.asCell();
                    if (SymbolTable* symbolTable = jsDynamicCast<SymbolTable*>(cell)) {
                        if (m_unlinkedCode->wasCompiledWithTypeProfilerOpcodes()) {
                            ConcurrentJSLocker locker(symbolTable->m_lock);
                            symbolTable->prepareForTypeProfiling(locker);
                        }

                        SymbolTable* clone = symbolTable->cloneScopePart(vm);
                        if (wasCompiledWithDebuggingOpcodes())
                            clone->setRareDataCodeBlock(this);

                        constant = clone;
                    } else if (jsDynamicCast<JSTemplateObjectDescriptor*>(cell))
                        templateObjectIndices.append(i);
                }
            }
            break;
        }
        m_constantRegisters[i].set(vm, this, constant);
    }

    return templateObjectIndices;
}

void CodeBlock::initializeTemplateObjects(ScriptExecutable* topLevelExecutable, const Vector<unsigned>& templateObjectIndices)
{
    auto scope = DECLARE_THROW_SCOPE(vm());
    for (unsigned i : templateObjectIndices) {
        auto* descriptor = jsCast<JSTemplateObjectDescriptor*>(m_constantRegisters[i].get());
        auto* templateObject = topLevelExecutable->createTemplateObject(globalObject(), descriptor);
        RETURN_IF_EXCEPTION(scope, void());
        m_constantRegisters[i].set(vm(), this, templateObject);
    }
}

void CodeBlock::setAlternative(VM& vm, CodeBlock* alternative)
{
    RELEASE_ASSERT(alternative);
    RELEASE_ASSERT(alternative->jitCode());
    ASSERT(JITCode::isBaselineCode(alternative->jitType()) || alternative->jitType() == JITType::None);
    m_alternative.set(vm, this, alternative);
}

void CodeBlock::setNumParameters(unsigned newValue, bool allocateArgumentValueProfiles)
{
    m_numParameters = newValue;
    m_argumentValueProfiles = FixedVector<ArgumentValueProfile>((Options::useJIT() && allocateArgumentValueProfiles) ? newValue : 0);
}

CodeBlock* CodeBlock::specialOSREntryBlockOrNull()
{
#if ENABLE(FTL_JIT)
    if (jitType() != JITType::DFGJIT)
        return nullptr;
    auto* jitCode = m_jitCode->dfg();
    return jitCode->osrEntryBlock();
#else // ENABLE(FTL_JIT)
    return 0;
#endif // ENABLE(FTL_JIT)
}

size_t CodeBlock::estimatedSize(JSCell* cell, VM& vm)
{
    CodeBlock* thisObject = jsCast<CodeBlock*>(cell);
    size_t extraMemoryAllocated = 0;
    if (thisObject->m_metadata)
        extraMemoryAllocated += thisObject->m_metadata->sizeInBytesForGC();
    RefPtr<JSC::JITCode> jitCode = thisObject->m_jitCode;
    if (jitCode && !jitCode->isShared())
        extraMemoryAllocated += jitCode->size();
    return Base::estimatedSize(cell, vm) + extraMemoryAllocated;
}

template<typename Func>
inline void CodeBlock::forEachStructureStubInfo(Func func)
{
    UNUSED_PARAM(func);
#if ENABLE(JIT)
    if (JSC::JITCode::isOptimizingJIT(jitType())) {
#if ENABLE(DFG_JIT)
        for (auto* stubInfo : jitCode()->dfgCommon()->m_stubInfos) {
            if (func(*stubInfo) == IterationStatus::Done)
                return;
        }
        if (auto* jitData = dfgJITData()) {
            for (auto& stubInfo : jitData->stubInfos()) {
                if (func(stubInfo) == IterationStatus::Done)
                    return;
            }
        }
#endif
    } else {
        if (auto* jitData = baselineJITData()) {
            for (auto& stubInfo : jitData->stubInfos()) {
                if (func(stubInfo) == IterationStatus::Done)
                    return;
            }
        }
    }
#endif // ENABLE(JIT)
}

template<typename Visitor>
void CodeBlock::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    CodeBlock* thisObject = jsCast<CodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    thisObject->visitChildren(visitor);
}

DEFINE_VISIT_CHILDREN(CodeBlock);

template<typename Visitor>
void CodeBlock::visitChildren(Visitor& visitor)
{
    ConcurrentJSLocker locker(m_lock);

    // In CodeBlock::shouldVisitStrongly() we may have decided to skip visiting this
    // codeBlock. However, if we end up visiting it anyway due to other references,
    // we can clear this flag and allow the verifier GC to visit it as well.
    m_visitChildrenSkippedDueToOldAge = false;
    if (CodeBlock* otherBlock = specialOSREntryBlockOrNull())
        visitor.appendUnbarriered(otherBlock);

    size_t extraMemory = 0;
    if (m_metadata)
        extraMemory += m_metadata->sizeInBytesForGC();
    if (m_jitCode && !m_jitCode->isShared())
        extraMemory += m_jitCode->size();
    visitor.reportExtraMemoryVisited(extraMemory);

    stronglyVisitStrongReferences(locker, visitor);
    stronglyVisitWeakReferences(locker, visitor);
    
    Heap::CodeBlockSpaceAndSet::setFor(*subspace()).add(this);
}

template<typename Visitor>
bool CodeBlock::shouldVisitStrongly(const ConcurrentJSLocker& locker, Visitor& visitor)
{
    if (Options::forceCodeBlockLiveness())
        return true;

    if (shouldJettisonDueToOldAge(locker, visitor)) {
        if (Options::verifyGC())
            m_visitChildrenSkippedDueToOldAge = true;
        return false;
    }

    if (UNLIKELY(m_visitChildrenSkippedDueToOldAge)) {
        RELEASE_ASSERT(Options::verifyGC());
        return false;
    }

    // Interpreter and Baseline JIT CodeBlocks don't need to be jettisoned when
    // their weak references go stale. So if a basline JIT CodeBlock gets
    // scanned, we can assume that this means that it's live.
    if (!JSC::JITCode::isOptimizingJIT(jitType()))
        return true;

    return false;
}

template bool CodeBlock::shouldVisitStrongly(const ConcurrentJSLocker&, AbstractSlotVisitor&);
template bool CodeBlock::shouldVisitStrongly(const ConcurrentJSLocker&, SlotVisitor&);

bool CodeBlock::shouldJettisonDueToWeakReference(VM& vm)
{
    if (!JSC::JITCode::isOptimizingJIT(jitType()))
        return false;
    return !vm.heap.isMarked(this);
}

static Seconds timeToLive(JITType jitType)
{
    if (UNLIKELY(Options::useEagerCodeBlockJettisonTiming())) {
        switch (jitType) {
        case JITType::InterpreterThunk:
            return 10_ms;
        case JITType::BaselineJIT:
            return 30_ms;
        case JITType::DFGJIT:
            return 40_ms;
        case JITType::FTLJIT:
            return 120_ms;
        default:
            return Seconds::infinity();
        }
    }

    switch (jitType) {
    case JITType::InterpreterThunk:
        return 5_s;
    case JITType::BaselineJIT:
        // Effectively 10 additional seconds, since BaselineJIT and
        // InterpreterThunk share a CodeBlock.
        return 15_s;
    case JITType::DFGJIT:
        return 20_s;
    case JITType::FTLJIT:
        return 60_s;
    default:
        return Seconds::infinity();
    }
}

template<typename Visitor>
ALWAYS_INLINE bool CodeBlock::shouldJettisonDueToOldAge(const ConcurrentJSLocker&, Visitor& visitor)
{
    if (visitor.isMarked(this))
        return false;

    if (UNLIKELY(Options::forceCodeBlockToJettisonDueToOldAge()))
        return true;
    
    if (timeSinceCreation() < timeToLive(jitType()))
        return false;
    
    return true;
}

#if ENABLE(DFG_JIT)
template<typename Visitor>
static inline bool shouldMarkTransition(Visitor& visitor, DFG::WeakReferenceTransition& transition)
{
    if (transition.m_codeOrigin && !visitor.isMarked(transition.m_codeOrigin.get()))
        return false;
    
    if (!visitor.isMarked(transition.m_from.get()))
        return false;
    
    return true;
}

BytecodeIndex CodeBlock::bytecodeIndexForExit(BytecodeIndex exitIndex) const
{
    if (exitIndex.checkpoint()) {
        const auto& instruction = instructions().at(exitIndex);
        exitIndex = instruction.next().index();
    }
    return exitIndex;
}
#endif // ENABLE(DFG_JIT)

template<typename Visitor>
void CodeBlock::propagateTransitions(const ConcurrentJSLocker&, Visitor& visitor)
{
    typename Visitor::SuppressGCVerifierScope suppressScope(visitor);

    if (jitType() == JITType::InterpreterThunk) {
        if (m_metadata) {
            m_metadata->forEach<OpPutById>([&] (auto& metadata) {
                StructureID oldStructureID = metadata.m_oldStructureID;
                StructureID newStructureID = metadata.m_newStructureID;
                if (!oldStructureID || !newStructureID)
                    return;

                Structure* oldStructure = oldStructureID.decode();
                if (visitor.isMarked(oldStructure)) {
                    Structure* newStructure = newStructureID.decode();
                    visitor.appendUnbarriered(newStructure);
                }
            });

            m_metadata->forEach<OpPutPrivateName>([&] (auto& metadata) {
                StructureID oldStructureID = metadata.m_oldStructureID;
                StructureID newStructureID = metadata.m_newStructureID;
                if (!oldStructureID || !newStructureID)
                    return;

                JSCell* property = metadata.m_property.get();
                ASSERT(property);
                if (!visitor.isMarked(property))
                    return;

                Structure* oldStructure = oldStructureID.decode();
                if (visitor.isMarked(oldStructure)) {
                    Structure* newStructure = newStructureID.decode();
                    visitor.appendUnbarriered(newStructure);
                }
            });

            m_metadata->forEach<OpSetPrivateBrand>([&] (auto& metadata) {
                StructureID oldStructureID = metadata.m_oldStructureID;
                StructureID newStructureID = metadata.m_newStructureID;
                if (!oldStructureID || !newStructureID)
                    return;

                JSCell* brand = metadata.m_brand.get();
                ASSERT(brand);
                if (!visitor.isMarked(brand))
                    return;

                Structure* oldStructure = oldStructureID.decode();
                if (visitor.isMarked(oldStructure)) {
                    Structure* newStructure = newStructureID.decode();
                    visitor.appendUnbarriered(newStructure);
                }
            });
        }
    }

#if ENABLE(JIT)
    forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
        stubInfo.propagateTransitions(visitor);
        return IterationStatus::Continue;
    });
#endif // ENABLE(JIT)
    
#if ENABLE(DFG_JIT)
    if (JSC::JITCode::isOptimizingJIT(jitType())) {
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();

        if (auto* statuses = dfgCommon->recordedStatuses.get())
            statuses->markIfCheap(visitor);

        for (StructureID structureID : dfgCommon->m_weakStructureReferences)
            structureID.decode()->markIfCheap(visitor);

        for (auto& transition : dfgCommon->m_transitions) {
            if (shouldMarkTransition(visitor, transition)) {
                // If the following three things are live, then the target of the
                // transition is also live:
                //
                // - This code block. We know it's live already because otherwise
                //   we wouldn't be scanning ourselves.
                //
                // - The code origin of the transition. Transitions may arise from
                //   code that was inlined. They are not relevant if the user's
                //   object that is required for the inlinee to run is no longer
                //   live.
                //
                // - The source of the transition. The transition checks if some
                //   heap location holds the source, and if so, stores the target.
                //   Hence the source must be live for the transition to be live.
                //
                // We also short-circuit the liveness if the structure is harmless
                // to mark (i.e. its global object and prototype are both already
                // live).

                visitor.append(transition.m_to);
            }
        }
    }
#endif // ENABLE(DFG_JIT)
}

template void CodeBlock::propagateTransitions(const ConcurrentJSLocker&, AbstractSlotVisitor&);
template void CodeBlock::propagateTransitions(const ConcurrentJSLocker&, SlotVisitor&);

template<typename Visitor>
void CodeBlock::determineLiveness(const ConcurrentJSLocker&, Visitor& visitor)
{
    UNUSED_PARAM(visitor);
    
#if ENABLE(DFG_JIT)
    VM& vm = *m_vm;
    UNUSED_VARIABLE(vm);
    if (visitor.isMarked(this))
        return;
    
    // In rare and weird cases, this could be called on a baseline CodeBlock. One that I found was
    // that we might decide that the CodeBlock should be jettisoned due to old age, so the
    // isMarked check doesn't protect us.
    if (!JSC::JITCode::isOptimizingJIT(jitType()))
        return;
    
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
    // Now check all of our weak references. If all of them are live, then we
    // have proved liveness and so we scan our strong references. If at end of
    // GC we still have not proved liveness, then this code block is toast.
    bool allAreLiveSoFar = true;
    for (unsigned i = 0; i < dfgCommon->m_weakReferences.size(); ++i) {
        JSCell* reference = dfgCommon->m_weakReferences[i].get();
        ASSERT(!jsDynamicCast<CodeBlock*>(reference));
        if (!visitor.isMarked(reference)) {
            allAreLiveSoFar = false;
            break;
        }
    }
    if (allAreLiveSoFar) {
        for (StructureID structureID : dfgCommon->m_weakStructureReferences) {
            Structure* structure = structureID.decode();
            if (!visitor.isMarked(structure)) {
                allAreLiveSoFar = false;
                break;
            }
        }
    }
    
    // If some weak references are dead, then this fixpoint iteration was
    // unsuccessful.
    if (!allAreLiveSoFar)
        return;
    
    // All weak references are live. Record this information so we don't
    // come back here again, and scan the strong references.
    visitor.appendUnbarriered(this);
#endif // ENABLE(DFG_JIT)
}

template void CodeBlock::determineLiveness(const ConcurrentJSLocker&, AbstractSlotVisitor&);
template void CodeBlock::determineLiveness(const ConcurrentJSLocker&, SlotVisitor&);

void CodeBlock::finalizeLLIntInlineCaches()
{
    VM& vm = *m_vm;

    if (m_metadata) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=166418
        // We need to add optimizations for op_resolve_scope_for_hoisting_func_decl_in_eval to do link time scope resolution.

        auto clearIfNeeded = [&] (GetByIdModeMetadata& modeMetadata, ASCIILiteral opName) {
            if (modeMetadata.mode != GetByIdMode::Default)
                return;
            StructureID oldStructureID = modeMetadata.defaultMode.structureID;
            if (!oldStructureID || vm.heap.isMarked(oldStructureID.decode()))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing ", opName, " LLInt property access.");
            LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(modeMetadata);
        };

        m_metadata->forEach<OpIteratorOpen>([&] (auto& metadata) {
            clearIfNeeded(metadata.m_modeMetadata, "iterator open"_s);
        });

        m_metadata->forEach<OpIteratorNext>([&] (auto& metadata) {
            clearIfNeeded(metadata.m_doneModeMetadata, "iterator next"_s);
            clearIfNeeded(metadata.m_valueModeMetadata, "iterator next"_s);
        });

        m_metadata->forEach<OpInstanceof>([&] (auto& metadata) {
            clearIfNeeded(metadata.m_hasInstanceModeMetadata, "instanceof"_s);
            clearIfNeeded(metadata.m_prototypeModeMetadata, "instanceof"_s);
        });

        m_metadata->forEach<OpGetById>([&] (auto& metadata) {
            clearIfNeeded(metadata.m_modeMetadata, "get by id"_s);
        });

        m_metadata->forEach<OpGetLength>([&] (auto& metadata) {
            clearIfNeeded(metadata.m_modeMetadata, "get length"_s);
        });

        m_metadata->forEach<OpTryGetById>([&] (auto& metadata) {
            StructureID oldStructureID = metadata.m_structureID;
            if (!oldStructureID || vm.heap.isMarked(oldStructureID.decode()))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing try_get_by_id LLInt property access.");
            metadata.m_structureID = StructureID();
            metadata.m_offset = 0;
        });

        m_metadata->forEach<OpGetByIdDirect>([&] (auto& metadata) {
            StructureID oldStructureID = metadata.m_structureID;
            if (!oldStructureID || vm.heap.isMarked(oldStructureID.decode()))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing get_by_id_direct LLInt property access.");
            metadata.m_structureID = StructureID();
            metadata.m_offset = 0;
        });

        m_metadata->forEach<OpGetPrivateName>([&] (auto& metadata) {
            JSCell* property = metadata.m_property.get();
            StructureID structureID = metadata.m_structureID;

            if ((!property || vm.heap.isMarked(property)) && (!structureID || vm.heap.isMarked(structureID.decode())))
                return;

            dataLogLnIf(Options::verboseOSR(), "Clearing LLInt private property access.");
            metadata.m_structureID = StructureID();
            metadata.m_offset = 0;
            metadata.m_property.clear();
        });

        m_metadata->forEach<OpPutById>([&] (auto& metadata) {
            StructureID oldStructureID = metadata.m_oldStructureID;
            StructureID newStructureID = metadata.m_newStructureID;
            StructureChain* chain = metadata.m_structureChain.get();
            if ((!oldStructureID || vm.heap.isMarked(oldStructureID.decode()))
                && (!newStructureID || vm.heap.isMarked(newStructureID.decode()))
                && (!chain || vm.heap.isMarked(chain)))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing LLInt put transition.");
            metadata.m_oldStructureID = StructureID();
            metadata.m_offset = 0;
            metadata.m_newStructureID = StructureID();
            metadata.m_structureChain.clear();
        });

        m_metadata->forEach<OpPutPrivateName>([&] (auto& metadata) {
            StructureID oldStructureID = metadata.m_oldStructureID;
            StructureID newStructureID = metadata.m_newStructureID;
            JSCell* property = metadata.m_property.get();
            if ((!oldStructureID || vm.heap.isMarked(oldStructureID.decode()))
                && (!property || vm.heap.isMarked(property))
                && (!newStructureID || vm.heap.isMarked(newStructureID.decode())))
                return;

            dataLogLnIf(Options::verboseOSR(), "Clearing LLInt put_private_name transition.");
            metadata.m_oldStructureID = StructureID();
            metadata.m_offset = 0;
            metadata.m_newStructureID = StructureID();
            metadata.m_property.clear();
        });

        m_metadata->forEach<OpSetPrivateBrand>([&] (auto& metadata) {
            StructureID oldStructureID = metadata.m_oldStructureID;
            StructureID newStructureID = metadata.m_newStructureID;
            JSCell* brand = metadata.m_brand.get();
            if ((!oldStructureID || vm.heap.isMarked(oldStructureID.decode()))
                && (!brand || vm.heap.isMarked(brand))
                && (!newStructureID || vm.heap.isMarked(newStructureID.decode())))
                return;

            dataLogLnIf(Options::verboseOSR(), "Clearing LLInt set_private_brand transition.");
            metadata.m_oldStructureID = StructureID();
            metadata.m_newStructureID = StructureID();
            metadata.m_brand.clear();
        });

        m_metadata->forEach<OpCheckPrivateBrand>([&] (auto& metadata) {
            StructureID structureID = metadata.m_structureID;
            JSCell* brand = metadata.m_brand.get();
            if ((!structureID || vm.heap.isMarked(structureID.decode()))
                && (!brand || vm.heap.isMarked(brand)))
                return;

            dataLogLnIf(Options::verboseOSR(), "Clearing LLInt check_private_brand transition.");
            metadata.m_structureID = StructureID();
            metadata.m_brand.clear();
        });

        m_metadata->forEach<OpToThis>([&] (auto& metadata) {
            if (!metadata.m_cachedStructureID || vm.heap.isMarked(metadata.m_cachedStructureID.decode()))
                return;
            if (Options::verboseOSR()) {
                Structure* structure = metadata.m_cachedStructureID.decode();
                dataLogF("Clearing LLInt to_this with structure %p.\n", structure);
            }
            metadata.m_cachedStructureID = StructureID();
            metadata.m_toThisStatus = merge(metadata.m_toThisStatus, ToThisClearedByGC);
        });

        auto clearCachedCalleeIfNecessary = [&](auto& metadata, ASCIILiteral name) {
            auto& cacheWriteBarrier = metadata.m_cachedCallee;
            if (!cacheWriteBarrier || cacheWriteBarrier.unvalidatedGet() == JSCell::seenMultipleCalleeObjects())
                return;
            JSCell* cachedFunction = cacheWriteBarrier.get();
            if (vm.heap.isMarked(cachedFunction))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing LLInt ", name, " with cached callee ", RawPointer(cachedFunction), ".");
            cacheWriteBarrier.clear();
        };

        m_metadata->forEach<OpCreateThis>([&](auto& metadata) {
            clearCachedCalleeIfNecessary(metadata, "op_create_this"_s);
        });
        m_metadata->forEach<OpCreatePromise>([&](auto& metadata) {
            clearCachedCalleeIfNecessary(metadata, "op_create_promise"_s);
        });
        m_metadata->forEach<OpCreateGenerator>([&](auto& metadata) {
            clearCachedCalleeIfNecessary(metadata, "op_create_generator"_s);
        });
        m_metadata->forEach<OpCreateAsyncGenerator>([&](auto& metadata) {
            clearCachedCalleeIfNecessary(metadata, "op_create_async_generator"_s);
        });
        m_metadata->forEach<OpSuperConstruct>([&](auto& metadata) {
            clearCachedCalleeIfNecessary(metadata, "op_super_construct"_s);
        });
        m_metadata->forEach<OpSuperConstructVarargs>([&](auto& metadata) {
            clearCachedCalleeIfNecessary(metadata, "op_super_construct_varargs"_s);
        });

        m_metadata->forEach<OpResolveScope>([&] (auto& metadata) {
            // Right now this isn't strictly necessary. Any symbol tables that this will refer to
            // are for outer functions, and we refer to those functions strongly, and they refer
            // to the symbol table strongly. But it's nice to be on the safe side.
            WriteBarrierBase<SymbolTable>& symbolTable = metadata.m_symbolTable;
            if (!symbolTable || vm.heap.isMarked(symbolTable.get()))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing dead symbolTable ", RawPointer(symbolTable.get()));
            symbolTable.clear();
        });

        auto handleGetPutFromScope = [&] (auto& metadata) {
            GetPutInfo getPutInfo = metadata.m_getPutInfo;
            if (getPutInfo.resolveType() == GlobalVar || getPutInfo.resolveType() == GlobalVarWithVarInjectionChecks
                || getPutInfo.resolveType() == ResolvedClosureVar || getPutInfo.resolveType() == GlobalLexicalVar || getPutInfo.resolveType() == GlobalLexicalVarWithVarInjectionChecks)
                return;
            WriteBarrierBase<Structure>& structure = metadata.m_structure;
            if (!structure || vm.heap.isMarked(structure.get()))
                return;
            dataLogLnIf(Options::verboseOSR(), "Clearing scope access with structure ", RawPointer(structure.get()));
            structure.clear();
        };

        m_metadata->forEach<OpGetFromScope>(handleGetPutFromScope);
        m_metadata->forEach<OpPutToScope>(handleGetPutFromScope);
    }

    // We can't just remove all the sets when we clear the caches since we might have created a watchpoint set
    // then cleared the cache without GCing in between.
    m_llintGetByIdWatchpointMap.removeIf([&] (const StructureWatchpointMap::KeyValuePairType& pair) -> bool {
        auto clear = [&] () {
            BytecodeIndex bytecodeIndex = std::get<1>(pair.key);
            auto& instruction = instructions().at(bytecodeIndex.offset());
            OpcodeID opcode = instruction->opcodeID();
            switch (opcode) {
            case op_get_by_id: {
                dataLogLnIf(Options::verboseOSR(), "Clearing LLInt property access.");
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(instruction->as<OpGetById>().metadata(this).m_modeMetadata);
                break;
            }
            case op_get_length: {
                dataLogLnIf(Options::verboseOSR(), "Clearing LLInt property access.");
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(instruction->as<OpGetLength>().metadata(this).m_modeMetadata);
                break;
            }
            case op_iterator_open: {
                dataLogLnIf(Options::verboseOSR(), "Clearing LLInt iterator open property access.");
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(instruction->as<OpIteratorOpen>().metadata(this).m_modeMetadata);
                break;
            }
            case op_iterator_next: {
                dataLogLnIf(Options::verboseOSR(), "Clearing LLInt iterator next property access.");
                // FIXME: We don't really want to clear both caches here but it's kinda annoying to figure out which one this is referring to...
                // See: https://bugs.webkit.org/show_bug.cgi?id=210693
                auto& metadata = instruction->as<OpIteratorNext>().metadata(this);
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(metadata.m_doneModeMetadata);
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(metadata.m_valueModeMetadata);
                break;
            }
            case op_instanceof: {
                dataLogLnIf(Options::verboseOSR(), "Clearing LLInt instanceof property access.");
                auto& metadata = instruction->as<OpInstanceof>().metadata(this);
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(metadata.m_hasInstanceModeMetadata);
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(metadata.m_prototypeModeMetadata);
                break;
            }
            default:
                break;
            }
            return true;
        };

        if (!vm.heap.isMarked(std::get<0>(pair.key).decode()))
            return clear();

        for (const LLIntPrototypeLoadAdaptiveStructureWatchpoint& watchpoint : pair.value) {
            if (!watchpoint.key().isStillLive(vm))
                return clear();
        }

        return false;
    });
}

#if ENABLE(JIT)
void CodeBlock::finalizeJITInlineCaches()
{
#if ENABLE(DFG_JIT)
    if (JSC::JITCode::isOptimizingJIT(jitType())) {
        for (auto* callLinkInfo : m_jitCode->dfgCommon()->m_callLinkInfos)
            callLinkInfo->visitWeak(vm());
        for (auto* callLinkInfo : m_jitCode->dfgCommon()->m_directCallLinkInfos)
            callLinkInfo->visitWeak(vm());
        if (auto* jitData = dfgJITData()) {
            for (auto& callLinkInfo : jitData->callLinkInfos())
                callLinkInfo.visitWeak(vm());
        }
    }
#endif

    forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
        ConcurrentJSLockerBase locker(NoLockingNecessary);
        stubInfo.visitWeakReferences(locker, this);
        return IterationStatus::Continue;
    });
}
#endif

void CodeBlock::finalizeUnconditionally(VM& vm, CollectionScope)
{
    UNUSED_PARAM(vm);

    // CodeBlock::finalizeUnconditionally is called for all live CodeBlocks.
    // We do not need to call updateAllPredictions for DFG / FTL since the same thing happens in LLInt / Baseline CodeBlock for them.
    if (JITCode::isBaselineCode(jitType()))
        updateAllPredictions();

    if (JITCode::couldBeInterpreted(jitType())) {
        finalizeLLIntInlineCaches();
        // If the CodeBlock is DFG or FTL, CallLinkInfo in metadata is not related.
        forEachLLIntOrBaselineCallLinkInfo([&](DataOnlyCallLinkInfo& callLinkInfo) {
            callLinkInfo.visitWeak(vm);
        });
    }

#if ENABLE(JIT)
    if (!!jitCode())
        finalizeJITInlineCaches();
#endif

#if ENABLE(DFG_JIT)
    if (JSC::JITCode::isOptimizingJIT(jitType())) {
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
        if (auto* statuses = dfgCommon->recordedStatuses.get())
            statuses->finalize(vm);

        if (auto* jitData = dfgJITData())
            jitData->finalizeUnconditionally();
    }
#endif // ENABLE(DFG_JIT)

    auto updateActivity = [&] {
        if (!VM::useUnlinkedCodeBlockJettisoning())
            return;
        auto* jitCode = m_jitCode.get();
        float count = 0;
        bool alwaysActive = false;
        switch (JSC::JITCode::jitTypeFor(jitCode)) {
        case JITType::None:
        case JITType::HostCallThunk:
            return;
        case JITType::InterpreterThunk:
            count = m_unlinkedCode->llintExecuteCounter().count();
            break;
        case JITType::BaselineJIT:
#if ENABLE(JIT)
            if (auto* jitData = baselineJITData())
                count = jitData->executeCounter().count();
#endif
            break;
        case JITType::DFGJIT:
#if ENABLE(FTL_JIT)
            if (auto* jitData = dfgJITData())
                count = jitData->tierUpCounter().count();
#else
            alwaysActive = true;
#endif
            break;
        case JITType::FTLJIT:
            alwaysActive = true;
            break;
        }
        if (alwaysActive || m_previousCounter < count) {
            // CodeBlock is active right now, so resetting UnlinkedCodeBlock's age.
            m_unlinkedCode->resetAge();
        }
        m_previousCounter = count;
    };
    updateActivity();

    Heap::CodeBlockSpaceAndSet::setFor(*subspace()).remove(this);

    // In CodeBlock::shouldVisitStrongly() we may have decided to skip visiting this
    // codeBlock. By the time we get here, we're done with the verifier GC. So, let's
    // reset this flag for the next GC cycle.
    m_visitChildrenSkippedDueToOldAge = false;
}

void CodeBlock::destroy(JSCell* cell)
{
    auto cb = static_cast<CodeBlock*>(cell);
    ASSERT(!cb->wasDestructed());
    cb->~CodeBlock();
#if ASSERT_ENABLED
    cb->m_magic = 0;
#endif
    ASSERT(cb->wasDestructed());
}

void CodeBlock::getICStatusMap(const ConcurrentJSLocker&, ICStatusMap& result)
{
    if (JITCode::couldBeInterpreted(jitType())) {
        forEachLLIntOrBaselineCallLinkInfo([&](DataOnlyCallLinkInfo& callLinkInfo) {
            result.add(callLinkInfo.codeOrigin(), ICStatus()).iterator->value.callLinkInfo = &callLinkInfo;
        });
    }
#if ENABLE(JIT)
    if (JITCode::isJIT(jitType())) {
        forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
            result.add(stubInfo.codeOrigin, ICStatus()).iterator->value.stubInfo = &stubInfo;
            return IterationStatus::Continue;
        });
        if (JSC::JITCode::isOptimizingJIT(jitType())) {
#if ENABLE(DFG_JIT)
            DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
            for (auto* callLinkInfo : dfgCommon->m_callLinkInfos)
                result.add(callLinkInfo->codeOrigin(), ICStatus()).iterator->value.callLinkInfo = callLinkInfo;
            if (auto* jitData = dfgJITData()) {
                for (auto& callLinkInfo : jitData->callLinkInfos())
                    result.add(callLinkInfo.codeOrigin(), ICStatus()).iterator->value.callLinkInfo = &callLinkInfo;
            }
            if (auto* statuses = dfgCommon->recordedStatuses.get()) {
                for (auto& pair : statuses->calls)
                    result.add(pair.first, ICStatus()).iterator->value.callStatus = pair.second.get();
                for (auto& pair : statuses->gets)
                    result.add(pair.first, ICStatus()).iterator->value.getStatus = pair.second.get();
                for (auto& pair : statuses->puts)
                    result.add(pair.first, ICStatus()).iterator->value.putStatus = pair.second.get();
                for (auto& pair : statuses->ins)
                    result.add(pair.first, ICStatus()).iterator->value.inStatus = pair.second.get();
                for (auto& pair : statuses->deletes)
                    result.add(pair.first, ICStatus()).iterator->value.deleteStatus = pair.second.get();
            }
#endif
        }
    }
#else
    UNUSED_PARAM(result);
#endif
}

void CodeBlock::getICStatusMap(ICStatusMap& result)
{
    ConcurrentJSLocker locker(m_lock);
    getICStatusMap(locker, result);
}

#if ENABLE(JIT)
StructureStubInfo* CodeBlock::findStubInfo(CodeOrigin codeOrigin)
{
    ConcurrentJSLocker locker(m_lock);
    StructureStubInfo* result = nullptr;
    forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
        if (stubInfo.codeOrigin == codeOrigin) {
            result = &stubInfo;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return result;
}
#endif

template<typename Visitor>
void CodeBlock::visitOSRExitTargets(const ConcurrentJSLocker&, Visitor& visitor)
{
    // We strongly visit OSR exits targets because we don't want to deal with
    // the complexity of generating an exit target CodeBlock on demand and
    // guaranteeing that it matches the details of the CodeBlock we compiled
    // the OSR exit against.

    visitor.append(m_alternative);

#if ENABLE(DFG_JIT)
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
    if (dfgCommon->inlineCallFrames) {
        for (auto* inlineCallFrame : *dfgCommon->inlineCallFrames) {
            ASSERT(inlineCallFrame->baselineCodeBlock);
            visitor.append(inlineCallFrame->baselineCodeBlock);
        }
    }
#endif
}

template<typename Visitor>
void CodeBlock::stronglyVisitStrongReferences(const ConcurrentJSLocker& locker, Visitor& visitor)
{
    UNUSED_PARAM(locker);
    
    visitor.append(m_globalObject);
    visitor.append(m_ownerExecutable); // This is extra important since it causes the Executable -> CodeBlock edge activated.
    visitor.append(m_unlinkedCode);
    if (m_rareData)
        m_rareData->m_directEvalCodeCache.visitAggregate(visitor);
    visitor.appendValues(m_constantRegisters.data(), m_constantRegisters.size());
    for (auto& functionExpr : m_functionExprs)
        visitor.append(functionExpr);
    for (auto& functionDecl : m_functionDecls)
        visitor.append(functionDecl);
    forEachObjectAllocationProfile([&](ObjectAllocationProfile& objectAllocationProfile) {
        objectAllocationProfile.visitAggregate(visitor);
    });

#if ENABLE(JIT)
    forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
        stubInfo.visitAggregate(visitor);
        return IterationStatus::Continue;
    });
    if (JSC::JITCode::isOptimizingJIT(jitType())) {
#if ENABLE(DFG_JIT)
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
        if (auto* statuses = dfgCommon->recordedStatuses.get())
            statuses->visitAggregate(visitor);
        visitOSRExitTargets(locker, visitor);
#endif
    }
#endif
}

template<typename Visitor>
void CodeBlock::stronglyVisitWeakReferences(const ConcurrentJSLocker&, Visitor& visitor)
{
    UNUSED_PARAM(visitor);

#if ENABLE(DFG_JIT)
    if (!JSC::JITCode::isOptimizingJIT(jitType()))
        return;
    
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();

    for (auto& transition : dfgCommon->m_transitions) {
        if (!!transition.m_codeOrigin)
            visitor.append(transition.m_codeOrigin); // Almost certainly not necessary, since the code origin should also be a weak reference. Better to be safe, though.
        visitor.append(transition.m_from);
        visitor.append(transition.m_to);
    }

    for (auto& weakReference : dfgCommon->m_weakReferences)
        visitor.append(weakReference);

    for (StructureID structureID : dfgCommon->m_weakStructureReferences)
        visitor.appendUnbarriered(structureID.decode());
#endif    
}

CodeBlock* CodeBlock::baselineAlternative()
{
#if ENABLE(JIT)
    CodeBlock* result = this;
    if (result->alternative())
        result = result->alternative();
    RELEASE_ASSERT(result);
    RELEASE_ASSERT(JITCode::isBaselineCode(result->jitType()) || result->jitType() == JITType::None);
    return result;
#else
    return this;
#endif
}

CodeBlock* CodeBlock::baselineVersion()
{
#if ENABLE(JIT)
    JITType selfJITType = jitType();
    if (JITCode::isBaselineCode(selfJITType))
        return this;
    CodeBlock* result = replacement();
    if (!result) {
        if (JSC::JITCode::isOptimizingJIT(selfJITType)) {
            // The replacement can be null if we've had a memory clean up and the executable
            // has been purged of its codeBlocks (see ExecutableBase::clearCode()). Regardless,
            // the current codeBlock is still live on the stack, and as an optimizing JIT
            // codeBlock, it will keep its baselineAlternative() alive for us to fetch below.
            result = this;
        } else {
            // This can happen if we're creating the original CodeBlock for an executable.
            // Assume that we're the baseline CodeBlock.
            RELEASE_ASSERT(selfJITType == JITType::None);
            return this;
        }
    }
    result = result->baselineAlternative();
    ASSERT(result);
    return result;
#else
    return this;
#endif
}

#if ENABLE(JIT)
CodeBlock* CodeBlock::optimizedReplacement(JITType typeToReplace)
{
    CodeBlock* replacement = this->replacement();
    if (!replacement)
        return nullptr;
    if (JITCode::isHigherTier(replacement->jitType(), typeToReplace))
        return replacement;
    return nullptr;
}

CodeBlock* CodeBlock::optimizedReplacement()
{
    return optimizedReplacement(jitType());
}

bool CodeBlock::hasOptimizedReplacement(JITType typeToReplace)
{
    return !!optimizedReplacement(typeToReplace);
}

bool CodeBlock::hasOptimizedReplacement()
{
    return hasOptimizedReplacement(jitType());
}
#endif

HandlerInfo* CodeBlock::handlerForBytecodeIndex(BytecodeIndex bytecodeIndex, RequiredHandler requiredHandler)
{
    RELEASE_ASSERT(bytecodeIndex.offset() < instructions().size());
    return handlerForIndex(bytecodeIndex.offset(), requiredHandler);
}

HandlerInfo* CodeBlock::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    if (!m_rareData)
        return nullptr;
    return HandlerInfo::handlerForIndex<HandlerInfo>(m_rareData->m_exceptionHandlers, index, requiredHandler);
}

DisposableCallSiteIndex CodeBlock::newExceptionHandlingCallSiteIndex(CallSiteIndex originalCallSite)
{
#if ENABLE(DFG_JIT)
    RELEASE_ASSERT(JSC::JITCode::isOptimizingJIT(jitType()));
    RELEASE_ASSERT(canGetCodeOrigin(originalCallSite));
    ASSERT(!!handlerForIndex(originalCallSite.bits()));
    CodeOrigin originalOrigin = codeOrigin(originalCallSite);
    return m_jitCode->dfgCommon()->codeOrigins->addDisposableCallSiteIndex(originalOrigin);
#else
    // We never create new on-the-fly exception handling
    // call sites outside the DFG/FTL inline caches.
    UNUSED_PARAM(originalCallSite);
    RELEASE_ASSERT_NOT_REACHED();
    return DisposableCallSiteIndex(0u);
#endif
}



void CodeBlock::ensureCatchLivenessIsComputedForBytecodeIndex(BytecodeIndex bytecodeIndex)
{
    ASSERT(JITCode::isBaselineCode(jitType()));
    auto& instruction = instructions().at(bytecodeIndex);
    OpCatch op = instruction->as<OpCatch>();
    auto& metadata = op.metadata(this);
    if (!!metadata.m_buffer)
        return;

    ensureCatchLivenessIsComputedForBytecodeIndexSlow(op, bytecodeIndex);
}

void CodeBlock::ensureCatchLivenessIsComputedForBytecodeIndexSlow(const OpCatch& op, BytecodeIndex bytecodeIndex)
{
    BytecodeLivenessAnalysis& bytecodeLiveness = livenessAnalysis();

    // We get the live-out set of variables at op_catch, not the live-in. This
    // is because the variables that the op_catch defines might be dead, and
    // we can avoid profiling them and extracting them when doing OSR entry
    // into the DFG.

    auto nextOffset = instructions().at(bytecodeIndex).next().offset();
    FastBitVector liveLocals = bytecodeLiveness.getLivenessInfoAtInstruction(this, BytecodeIndex(nextOffset));
    Vector<VirtualRegister> liveOperands;
    liveOperands.reserveInitialCapacity(liveLocals.bitCount());
    liveLocals.forEachSetBit([&] (unsigned liveLocal) {
        liveOperands.append(virtualRegisterForLocal(liveLocal));
    });

    for (unsigned i = 0; i < numParameters(); ++i)
        liveOperands.append(virtualRegisterForArgumentIncludingThis(i));

    auto* profiles = ValueProfileAndVirtualRegisterBuffer::create(liveOperands.size());
    RELEASE_ASSERT(profiles->size() == liveOperands.size());
    for (unsigned i = 0; i < profiles->size(); ++i)
        profiles->data()[i].m_operand = liveOperands[i];

    createRareDataIfNecessary();

    // The compiler thread will read this pointer value and then proceed to dereference it
    // if it is not null. We need to make sure all above stores happen before this store so
    // the compiler thread reads fully initialized data.
    WTF::storeStoreFence();

    op.metadata(this).m_buffer = profiles;
}

void CodeBlock::removeExceptionHandlerForCallSite(DisposableCallSiteIndex callSiteIndex)
{
    RELEASE_ASSERT(m_rareData);
    Vector<HandlerInfo>& exceptionHandlers = m_rareData->m_exceptionHandlers;
    unsigned index = callSiteIndex.bits();
    for (size_t i = 0; i < exceptionHandlers.size(); ++i) {
        HandlerInfo& handler = exceptionHandlers[i];
        if (handler.start <= index && handler.end > index) {
            exceptionHandlers.remove(i);
            return;
        }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

LineColumn CodeBlock::lineColumnForBytecodeIndex(BytecodeIndex bytecodeIndex) const
{
    RELEASE_ASSERT(bytecodeIndex.offset() < instructions().size());
    auto lineColumn = m_unlinkedCode->lineColumnForBytecodeIndex(bytecodeIndex);
    lineColumn.column += lineColumn.line ? 1 : firstLineColumnOffset();
    lineColumn.line += ownerExecutable()->firstLine();
    return lineColumn;
}

ExpressionInfo::Entry CodeBlock::expressionInfoForBytecodeIndex(BytecodeIndex bytecodeIndex) const
{
    auto entry = m_unlinkedCode->expressionInfoForBytecodeIndex(bytecodeIndex);
    entry.divot += sourceOffset();
    entry.lineColumn.column += entry.lineColumn.line ? 1 : firstLineColumnOffset();
    entry.lineColumn.line += ownerExecutable()->firstLine();
    return entry;
}

bool CodeBlock::hasOpDebugForLineAndColumn(unsigned line, std::optional<unsigned> column)
{
    const auto& instructionStream = instructions();
    for (const auto& it : instructionStream) {
        if (it->is<OpDebug>()) {
            auto lineColumn = lineColumnForBytecodeIndex(it.index());
            if (line == lineColumn.line && (!column || column == lineColumn.column))
                return true;
        }
    }
    return false;
}

void CodeBlock::shrinkToFit(const ConcurrentJSLocker&, ShrinkMode shrinkMode)
{
#if USE(JSVALUE32_64)
    // Only 32bit Baseline JIT is touching m_constantRegisters address directly.
    if (shrinkMode == ShrinkMode::EarlyShrink)
        m_constantRegisters.shrinkToFit();
#else
    UNUSED_PARAM(shrinkMode);
    m_constantRegisters.shrinkToFit();
#endif
}

void CodeBlock::linkIncomingCall(JSCell* caller, CallLinkInfoBase* incoming)
{
    if (caller)
        noticeIncomingCall(caller);
    m_incomingCalls.push(incoming);
}

void CodeBlock::unlinkOrUpgradeIncomingCalls(VM& vm, CodeBlock* newCodeBlock)
{
    SentinelLinkedList<CallLinkInfoBase, BasicRawSentinelNode<CallLinkInfoBase>> toBeRemoved;
    toBeRemoved.takeFrom(m_incomingCalls);

    // Note that upgrade may relink CallLinkInfo into newCodeBlock, and it is possible that |this| and newCodeBlock are the same.
    // This happens when newCodeBlock is installed by upgrading LLInt to Baseline. In that case, |this|'s m_incomingCalls will
    // be accumulated correctly.
    while (!toBeRemoved.isEmpty())
        toBeRemoved.begin()->unlinkOrUpgrade(vm, this, newCodeBlock);
}

CodeBlock* CodeBlock::newReplacement()
{
    return ownerExecutable()->newReplacementCodeBlockFor(specializationKind());
}

#if ENABLE(JIT)
CodeBlock* CodeBlock::replacement()
{
    const ClassInfo* classInfo = this->classInfo();

    if (classInfo == FunctionCodeBlock::info())
        return jsCast<FunctionExecutable*>(ownerExecutable())->codeBlockFor(isConstructor() ? CodeForConstruct : CodeForCall);

    if (classInfo == EvalCodeBlock::info())
        return jsCast<EvalExecutable*>(ownerExecutable())->codeBlock();

    if (classInfo == ProgramCodeBlock::info())
        return jsCast<ProgramExecutable*>(ownerExecutable())->codeBlock();

    if (classInfo == ModuleProgramCodeBlock::info())
        return jsCast<ModuleProgramExecutable*>(ownerExecutable())->codeBlock();

    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

DFG::CapabilityLevel CodeBlock::computeCapabilityLevel()
{
    const ClassInfo* classInfo = this->classInfo();

    if (classInfo == FunctionCodeBlock::info()) {
        if (isConstructor())
            return DFG::functionForConstructCapabilityLevel(JITType::FTLJIT, this);
        return DFG::functionForCallCapabilityLevel(JITType::FTLJIT, this);
    }

    if (classInfo == EvalCodeBlock::info())
        return DFG::evalCapabilityLevel(this);

    if (classInfo == ProgramCodeBlock::info())
        return DFG::programCapabilityLevel(this);

    if (classInfo == ModuleProgramCodeBlock::info())
        return DFG::programCapabilityLevel(this);

    RELEASE_ASSERT_NOT_REACHED();
    return DFG::CannotCompile;
}

#endif // ENABLE(JIT)

void CodeBlock::jettison(Profiler::JettisonReason reason, ReoptimizationMode mode, const FireDetail* detail)
{
#if !ENABLE(DFG_JIT)
    UNUSED_PARAM(mode);
    UNUSED_PARAM(detail);
#endif

    VM& vm = *m_vm;

    m_isJettisoned = true;

    CodeBlock* codeBlock = this; // Placate GCC for use in CODEBLOCK_LOG_EVENT  (does not like this).
    CODEBLOCK_LOG_EVENT(codeBlock, "jettison", ("due to ", reason, ", counting = ", mode == CountReoptimization, ", detail = ", pointerDump(detail)));

    RELEASE_ASSERT(reason != Profiler::NotJettisoned);
    
#if ENABLE(DFG_JIT)
    if (DFG::shouldDumpDisassembly()) {
        dataLog("Jettisoning ", *this);
        if (mode == CountReoptimization)
            dataLog(" and counting reoptimization");
        dataLog(" due to ", reason);
        if (detail)
            dataLog(", ", *detail);
        dataLog(".\n");
    }
    
    if (reason == Profiler::JettisonDueToWeakReference) {
        if (DFG::shouldDumpDisassembly()) {
            dataLog(*this, " will be jettisoned because of the following dead references:\n");
            DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
            for (auto& transition : dfgCommon->m_transitions) {
                JSCell* origin = transition.m_codeOrigin.get();
                JSCell* from = transition.m_from.get();
                JSCell* to = transition.m_to.get();
                if ((!origin || vm.heap.isMarked(origin)) && vm.heap.isMarked(from))
                    continue;
                dataLog("    Transition under ", RawPointer(origin), ", ", RawPointer(from), " -> ", RawPointer(to), ".\n");
            }
            for (unsigned i = 0; i < dfgCommon->m_weakReferences.size(); ++i) {
                JSCell* weak = dfgCommon->m_weakReferences[i].get();
                if (vm.heap.isMarked(weak))
                    continue;
                dataLog("    Weak reference ", RawPointer(weak), ".\n");
            }
        }
    }
#endif // ENABLE(DFG_JIT)

    DeferGCForAWhile deferGC(vm);
    
    // We want to accomplish two things here:
    // 1) Make sure that if this CodeBlock is on the stack right now, then if we return to it
    //    we should OSR exit at the top of the next bytecode instruction after the return.
    // 2) Make sure that if we call the owner executable, then we shouldn't call this CodeBlock.

#if ENABLE(DFG_JIT)
    if (JSC::JITCode::isOptimizingJIT(jitType()))
        jitCode()->dfgCommon()->clearWatchpoints();
    
    if (reason != Profiler::JettisonDueToOldAge) {
        Profiler::Compilation* compilation = jitCode()->dfgCommon()->compilation.get();
        if (UNLIKELY(compilation))
            compilation->setJettisonReason(reason, detail);
        
        // This accomplishes (1), and does its own book-keeping about whether it has already happened.
        if (auto* jitData = dfgJITData()) {
            if (jitData->isInvalidated()) {
                // We've already been invalidated.
                RELEASE_ASSERT(this != replacement() || (vm.heap.currentThreadIsDoingGCWork() && !vm.heap.isMarked(ownerExecutable())));
                return;
            }
            jitData->invalidate();
        }
        if (!jitCode()->isUnlinked()) {
            if (!jitCode()->dfgCommon()->invalidateLinkedCode()) {
                // We've already been invalidated.
                RELEASE_ASSERT(this != replacement() || (vm.heap.currentThreadIsDoingGCWork() && !vm.heap.isMarked(ownerExecutable())));
                return;
            }
        }
    }
    
    if (DFG::shouldDumpDisassembly())
        dataLog("    Did invalidate ", *this, "\n");
    
    // Count the reoptimization if that's what the user wanted.
    if (mode == CountReoptimization) {
        // FIXME: Maybe this should call alternative().
        // https://bugs.webkit.org/show_bug.cgi?id=123677
        baselineAlternative()->countReoptimization();
        if (DFG::shouldDumpDisassembly())
            dataLog("    Did count reoptimization for ", *this, "\n");
    }
    
    if (this != replacement()) {
        // This means that we were never the entrypoint. This can happen for OSR entry code
        // blocks.
        return;
    }

    if (alternative())
        alternative()->optimizeAfterWarmUp();

    if (reason != Profiler::JettisonDueToOldAge && reason != Profiler::JettisonDueToVMTraps)
        tallyFrequentExitSites();
#endif // ENABLE(DFG_JIT)

    // Jettison can happen during GC. We don't want to install code to a dead executable
    // because that would add a dead object to the remembered set.
    if (vm.heap.currentThreadIsDoingGCWork() && !vm.heap.isMarked(ownerExecutable()))
        return;

    // This accomplishes (2).
    ownerExecutable()->installCode(vm, alternative(), codeType(), specializationKind(), reason);

#if ENABLE(DFG_JIT)
    if (DFG::shouldDumpDisassembly())
        dataLog("    Did install baseline version of ", *this, "\n");
#endif // ENABLE(DFG_JIT)
}

JSGlobalObject* CodeBlock::globalObjectFor(CodeOrigin codeOrigin)
{
    auto* inlineCallFrame = codeOrigin.inlineCallFrame();
    if (!inlineCallFrame)
        return globalObject();
    return inlineCallFrame->baselineCodeBlock->globalObject();
}

class RecursionCheckFunctor {
public:
    RecursionCheckFunctor(CallFrame* startCallFrame, CodeBlock* codeBlock, unsigned depthToCheck)
        : m_startCallFrame(startCallFrame)
        , m_codeBlock(codeBlock)
        , m_depthToCheck(depthToCheck)
        , m_foundStartCallFrame(false)
        , m_didRecurse(false)
    { }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        CallFrame* currentCallFrame = visitor->callFrame();

        if (currentCallFrame == m_startCallFrame)
            m_foundStartCallFrame = true;

        if (m_foundStartCallFrame) {
            if (visitor->codeBlock() == m_codeBlock) {
                m_didRecurse = true;
                return IterationStatus::Done;
            }

            if (!m_depthToCheck--)
                return IterationStatus::Done;
        }

        return IterationStatus::Continue;
    }

    bool didRecurse() const { return m_didRecurse; }

private:
    CallFrame* const m_startCallFrame;
    CodeBlock* const m_codeBlock;
    mutable unsigned m_depthToCheck;
    mutable bool m_foundStartCallFrame;
    mutable bool m_didRecurse;
};

void CodeBlock::noticeIncomingCall(JSCell* caller)
{
    RELEASE_ASSERT(!m_isJettisoned);

    CodeBlock* callerCodeBlock = jsDynamicCast<CodeBlock*>(caller);
    
    dataLogLnIf(Options::verboseCallLink(), "Noticing call link from ", pointerDump(callerCodeBlock), " to ", *this);
    
#if ENABLE(DFG_JIT)
    if (!m_shouldAlwaysBeInlined)
        return;
    
    if (!callerCodeBlock) {
        m_shouldAlwaysBeInlined = false;
        dataLogLnIf(Options::verboseCallLink(), "    Clearing SABI because caller is native.");
        return;
    }

    if (!hasBaselineJITProfiling())
        return;

    if (!DFG::mightInlineFunction(JITType::FTLJIT, this))
        return;

    if (!canInline(capabilityLevelState()))
        return;
    
    if (!DFG::isSmallEnoughToInlineCodeInto(callerCodeBlock)) {
        m_shouldAlwaysBeInlined = false;
        dataLogLnIf(Options::verboseCallLink(), "    Clearing SABI because caller is too large.");
        return;
    }

    if (callerCodeBlock->jitType() == JITType::InterpreterThunk) {
        // If the caller is still in the interpreter, then we can't expect inlining to
        // happen anytime soon. Assume it's profitable to optimize it separately. This
        // ensures that a function is SABI only if it is called no more frequently than
        // any of its callers.
        m_shouldAlwaysBeInlined = false;
        dataLogLnIf(Options::verboseCallLink(), "    Clearing SABI because caller is in LLInt.");
        return;
    }
    
    if (JSC::JITCode::isOptimizingJIT(callerCodeBlock->jitType())) {
        m_shouldAlwaysBeInlined = false;
        dataLogLnIf(Options::verboseCallLink(), "    Clearing SABI bcause caller was already optimized.");
        return;
    }
    
    if (callerCodeBlock->codeType() != FunctionCode) {
        // If the caller is either eval or global code, assume that that won't be
        // optimized anytime soon. For eval code this is particularly true since we
        // delay eval optimization by a *lot*.
        m_shouldAlwaysBeInlined = false;
        dataLogLnIf(Options::verboseCallLink(), "    Clearing SABI because caller is not a function.");
        return;
    }

    // Recursive calls won't be inlined.
    if (callerCodeBlock->capabilityLevelState() == DFG::CapabilityLevelNotSet) {
        dataLog("In call from ", FullCodeOrigin(callerCodeBlock, CodeOrigin { }), " to ", *this, ": caller's DFG capability level is not set.\n");
        CRASH();
    }
    
    if (canCompile(callerCodeBlock->capabilityLevelState()))
        return;
    
    dataLogLnIf(Options::verboseCallLink(), "    Clearing SABI because the caller is not a DFG candidate.");
    
    m_shouldAlwaysBeInlined = false;
#endif
}

unsigned CodeBlock::reoptimizationRetryCounter() const
{
#if ENABLE(JIT)
    ASSERT(m_reoptimizationRetryCounter <= Options::reoptimizationRetryCounterMax());
    return m_reoptimizationRetryCounter;
#else
    return 0;
#endif // ENABLE(JIT)
}

#if !ENABLE(C_LOOP)
size_t CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters()
{
    return WTF::roundUpToMultipleOf<sizeof(Register)>(numberOfLLIntBaselineCalleeSaveRegisters() * sizeof(CPURegister)) / sizeof(Register);
}

size_t CodeBlock::calleeSaveSpaceAsVirtualRegisters(const RegisterAtOffsetList& calleeSaveRegisters)
{
    return WTF::roundUpToMultipleOf<sizeof(Register)>(calleeSaveRegisters.sizeOfAreaInBytes()) / sizeof(Register);
}
#endif

#if ENABLE(JIT)

void CodeBlock::countReoptimization()
{
    m_reoptimizationRetryCounter++;
    if (m_reoptimizationRetryCounter > Options::reoptimizationRetryCounterMax())
        m_reoptimizationRetryCounter = Options::reoptimizationRetryCounterMax();
}

unsigned CodeBlock::numberOfDFGCompiles()
{
    ASSERT(JITCode::isBaselineCode(jitType()));
    if (Options::testTheFTL()) {
        if (m_didFailFTLCompilation)
            return 1000000;
        return (m_hasBeenCompiledWithFTL ? 1 : 0) + m_reoptimizationRetryCounter;
    }
    CodeBlock* replacement = this->replacement();
    return ((replacement && JSC::JITCode::isOptimizingJIT(replacement->jitType())) ? 1 : 0) + m_reoptimizationRetryCounter;
}

const BaselineExecutionCounter& CodeBlock::baselineExecuteCounter()
{
    if (auto* jitData = baselineJITData())
        return jitData->executeCounter();
    static BaselineExecutionCounter dummy { };
    return dummy;
}

int32_t CodeBlock::codeTypeThresholdMultiplier() const
{
    if (codeType() == EvalCode)
        return Options::evalThresholdMultiplier();
    
    return 1;
}

double CodeBlock::optimizationThresholdScalingFactor() const
{
    // This expression arises from doing a least-squares fit of
    //
    // F[x_] =: a * Sqrt[x + b] + Abs[c * x] + d
    //
    // against the data points:
    //
    //    x       F[x_]
    //    10       0.9          (smallest reasonable code block)
    //   200       1.0          (typical small-ish code block)
    //   320       1.2          (something I saw in 3d-cube that I wanted to optimize)
    //  1268       5.0          (something I saw in 3d-cube that I didn't want to optimize)
    //  4000       5.5          (random large size, used to cause the function to converge to a shallow curve of some sort)
    // 10000       6.0          (similar to above)
    //
    // I achieve the minimization using the following Mathematica code:
    //
    // MyFunctionTemplate[x_, a_, b_, c_, d_] := a*Sqrt[x + b] + Abs[c*x] + d
    //
    // samples = {{10, 0.9}, {200, 1}, {320, 1.2}, {1268, 5}, {4000, 5.5}, {10000, 6}}
    //
    // solution = 
    //     Minimize[Plus @@ ((MyFunctionTemplate[#[[1]], a, b, c, d] - #[[2]])^2 & /@ samples),
    //         {a, b, c, d}][[2]]
    //
    // And the code below (to initialize a, b, c, d) is generated by:
    //
    // Print["const double " <> ToString[#[[1]]] <> " = " <>
    //     If[#[[2]] < 0.00001, "0.0", ToString[#[[2]]]] <> ";"] & /@ solution
    //
    // We've long known the following to be true:
    // - Small code blocks are cheap to optimize and so we should do it sooner rather
    //   than later.
    // - Large code blocks are expensive to optimize and so we should postpone doing so,
    //   and sometimes have a large enough threshold that we never optimize them.
    // - The difference in cost is not totally linear because (a) just invoking the
    //   DFG incurs some base cost and (b) for large code blocks there is enough slop
    //   in the correlation between instruction count and the actual compilation cost
    //   that for those large blocks, the instruction count should not have a strong
    //   influence on our threshold.
    //
    // I knew the goals but I didn't know how to achieve them; so I picked an interesting
    // example where the heuristics were right (code block in 3d-cube with instruction
    // count 320, which got compiled early as it should have been) and one where they were
    // totally wrong (code block in 3d-cube with instruction count 1268, which was expensive
    // to compile and didn't run often enough to warrant compilation in my opinion), and
    // then threw in additional data points that represented my own guess of what our
    // heuristics should do for some round-numbered examples.
    //
    // The expression to which I decided to fit the data arose because I started with an
    // affine function, and then did two things: put the linear part in an Abs to ensure
    // that the fit didn't end up choosing a negative value of c (which would result in
    // the function turning over and going negative for large x) and I threw in a Sqrt
    // term because Sqrt represents my intution that the function should be more sensitive
    // to small changes in small values of x, but less sensitive when x gets large.
    
    // Note that the current fit essentially eliminates the linear portion of the
    // expression (c == 0.0).
    const double a = 0.061504;
    const double b = 1.02406;
    const double c = 0.0;
    const double d = 0.825914;
    
    double bytecodeCost = this->bytecodeCost();
    
    ASSERT(bytecodeCost); // Make sure this is called only after we have an instruction stream; otherwise it'll just return the value of d, which makes no sense.
    
    double result = d + a * sqrt(bytecodeCost + b) + c * bytecodeCost;
    
    result *= codeTypeThresholdMultiplier();
    
    dataLogLnIf(Options::verboseOSR(),
        *this, ": bytecode cost is ", bytecodeCost,
        ", scaling execution counter by ", result, " * ", codeTypeThresholdMultiplier());
    return result;
}

static int32_t clipThreshold(double threshold)
{
    if (threshold < 1.0)
        return 1;
    
    if (threshold > static_cast<double>(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    
    return static_cast<int32_t>(threshold);
}

int32_t CodeBlock::adjustedCounterValue(int32_t desiredThreshold)
{
    return clipThreshold(
        static_cast<double>(desiredThreshold) *
        optimizationThresholdScalingFactor() *
        (1 << reoptimizationRetryCounter()));
}

bool CodeBlock::checkIfOptimizationThresholdReached()
{
#if ENABLE(DFG_JIT)
    if (JITWorklist* worklist = JITWorklist::existingGlobalWorklistOrNull()) {
        if (worklist->compilationState(JITCompilationKey(this, JITCompilationMode::DFG)) == JITWorklist::Compiled) {
            optimizeNextInvocation();
            return true;
        }
    }
#endif

    if (auto* jitData = baselineJITData())
        return jitData->executeCounter().checkIfThresholdCrossedAndSet(this);
    return false;
}

void CodeBlock::optimizeNextInvocation()
{
    dataLogLnIf(Options::verboseOSR(), *this, ": Optimizing next invocation.");
    if (auto* jitData = baselineJITData())
        jitData->executeCounter().setNewThreshold(0, this);
}

void CodeBlock::dontOptimizeAnytimeSoon()
{
    dataLogLnIf(Options::verboseOSR(), *this, ": Not optimizing anytime soon.");
    if (auto* jitData = baselineJITData())
        jitData->executeCounter().deferIndefinitely();
}

void CodeBlock::optimizeAfterWarmUp()
{
    dataLogLnIf(Options::verboseOSR(), *this, ": Optimizing after warm-up.");
#if ENABLE(DFG_JIT)
    if (auto* jitData = baselineJITData())
        jitData->executeCounter().setNewThreshold(adjustedCounterValue(Options::thresholdForOptimizeAfterWarmUp()), this);
#endif
}

void CodeBlock::optimizeAfterLongWarmUp()
{
    dataLogLnIf(Options::verboseOSR(), *this, ": Optimizing after long warm-up.");
#if ENABLE(DFG_JIT)
    if (auto* jitData = baselineJITData())
        jitData->executeCounter().setNewThreshold(adjustedCounterValue(Options::thresholdForOptimizeAfterLongWarmUp()), this);
#endif
}

void CodeBlock::optimizeSoon()
{
    dataLogLnIf(Options::verboseOSR(), *this, ": Optimizing soon.");
#if ENABLE(DFG_JIT)
    if (auto* jitData = baselineJITData())
        jitData->executeCounter().setNewThreshold(adjustedCounterValue(Options::thresholdForOptimizeSoon()), this);
#endif
}

void CodeBlock::forceOptimizationSlowPathConcurrently()
{
    dataLogLnIf(Options::verboseOSR(), *this, ": Forcing slow path concurrently.");
    if (auto* jitData = baselineJITData())
        jitData->executeCounter().forceSlowPathConcurrently();
}

#if ENABLE(DFG_JIT)
void CodeBlock::setOptimizationThresholdBasedOnCompilationResult(CompilationResult result)
{
    JITType type = jitType();
    if (type != JITType::BaselineJIT) {
        dataLogLn(*this, ": expected to have baseline code but have ", type);
        CRASH_WITH_INFO(bitwise_cast<uintptr_t>(jitCode().get()), static_cast<uint8_t>(type));
    }
    
    CodeBlock* replacement = this->replacement();
    bool hasReplacement = (replacement && replacement != this);
    if ((result == CompilationSuccessful) != hasReplacement) {
        dataLog(*this, ": we have result = ", result, " but ");
        if (replacement == this)
            dataLog("we are our own replacement.\n");
        else
            dataLog("our replacement is ", pointerDump(replacement), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    switch (result) {
    case CompilationSuccessful:
        RELEASE_ASSERT(replacement && JSC::JITCode::isOptimizingJIT(replacement->jitType()));
        optimizeNextInvocation();
        return;
    case CompilationFailed:
        dontOptimizeAnytimeSoon();
        return;
    case CompilationDeferred:
        // We'd like to do dontOptimizeAnytimeSoon() but we cannot because
        // forceOptimizationSlowPathConcurrently() is inherently racy. It won't
        // necessarily guarantee anything. So, we make sure that even if that
        // function ends up being a no-op, we still eventually retry and realize
        // that we have optimized code ready.
        optimizeAfterWarmUp();
        return;
    case CompilationInvalidated:
        // Retry with exponential backoff.
        countReoptimization();
        optimizeAfterWarmUp();
        return;
    }
    
    dataLog("Unrecognized result: ", static_cast<int>(result), "\n");
    RELEASE_ASSERT_NOT_REACHED();
}

#endif
    
uint32_t CodeBlock::adjustedExitCountThreshold(uint32_t desiredThreshold)
{
    ASSERT(JSC::JITCode::isOptimizingJIT(jitType()));
    // Compute this the lame way so we don't saturate. This is called infrequently
    // enough that this loop won't hurt us.
    unsigned result = desiredThreshold;
    for (unsigned n = baselineVersion()->reoptimizationRetryCounter(); n--;) {
        unsigned newResult = result << 1;
        if (newResult < result)
            return std::numeric_limits<uint32_t>::max();
        result = newResult;
    }
    return result;
}

uint32_t CodeBlock::exitCountThresholdForReoptimization()
{
    return adjustedExitCountThreshold(Options::osrExitCountForReoptimization() * codeTypeThresholdMultiplier());
}

uint32_t CodeBlock::exitCountThresholdForReoptimizationFromLoop()
{
    return adjustedExitCountThreshold(Options::osrExitCountForReoptimizationFromLoop() * codeTypeThresholdMultiplier());
}

bool CodeBlock::shouldReoptimizeNow()
{
    return osrExitCounter() >= exitCountThresholdForReoptimization();
}

bool CodeBlock::shouldReoptimizeFromLoopNow()
{
    return osrExitCounter() >= exitCountThresholdForReoptimizationFromLoop();
}
#endif

ArrayProfile* CodeBlock::getArrayProfile(const ConcurrentJSLocker&, BytecodeIndex bytecodeIndex)
{
    auto instruction = instructions().at(bytecodeIndex);

    if (instruction->opcodeID() == op_iterator_next)
        return &instruction->as<OpIteratorNext>().metadata(this).m_iterableProfile;

    switch (instruction->opcodeID()) {
#define CASE(Op) \
    case Op::opcodeID: \
        return &instruction->as<Op>().metadata(this).m_arrayProfile;

    FOR_EACH_OPCODE_WITH_SIMPLE_ARRAY_PROFILE(CASE)

#undef CASE

    default:
        break;
    }

    return nullptr;
}

#if ENABLE(DFG_JIT)
DFG::CodeOriginPool& CodeBlock::codeOrigins()
{
    return m_jitCode->dfgCommon()->codeOrigins.get();
}

size_t CodeBlock::numberOfDFGIdentifiers() const
{
    if (!JSC::JITCode::isOptimizingJIT(jitType()))
        return 0;
    
    return m_jitCode->dfgCommon()->m_dfgIdentifiers.size();
}

const Identifier& CodeBlock::identifier(int index) const
{
    UnlinkedCodeBlock* unlinkedCode = m_unlinkedCode.get();
    size_t unlinkedIdentifiers = unlinkedCode->numberOfIdentifiers();
    if (static_cast<unsigned>(index) < unlinkedIdentifiers)
        return unlinkedCode->identifier(index);
    ASSERT(JSC::JITCode::isOptimizingJIT(jitType()));
    return m_jitCode->dfgCommon()->m_dfgIdentifiers[index - unlinkedIdentifiers];
}
#endif // ENABLE(DFG_JIT)

#if ASSERT_ENABLED
bool CodeBlock::hasIdentifier(UniquedStringImpl* uid)
{
    UnlinkedCodeBlock* unlinkedCode = m_unlinkedCode.get();
    size_t unlinkedIdentifiers = unlinkedCode->numberOfIdentifiers();
#if ENABLE(DFG_JIT)
    size_t numberOfDFGIdentifiers = this->numberOfDFGIdentifiers();
    size_t numberOfIdentifiers = unlinkedIdentifiers + numberOfDFGIdentifiers;
#else
    size_t numberOfIdentifiers = unlinkedIdentifiers;
#endif

    if (numberOfIdentifiers > 100) {
        if (m_cachedIdentifierUids.size() != numberOfIdentifiers) {
            Locker locker(m_cachedIdentifierUidsLock);
            createRareDataIfNecessary();
            HashSet<UniquedStringImpl*> cachedIdentifierUids;
            cachedIdentifierUids.reserveInitialCapacity(numberOfIdentifiers);
            for (unsigned index = 0; index < unlinkedIdentifiers; ++index) {
                const Identifier& identifier = unlinkedCode->identifier(index);
                cachedIdentifierUids.add(identifier.impl());
            }
#if ENABLE(DFG_JIT)
            if (numberOfDFGIdentifiers) {
                ASSERT(JSC::JITCode::isOptimizingJIT(jitType()));
                auto& dfgIdentifiers = m_jitCode->dfgCommon()->m_dfgIdentifiers;
                for (unsigned index = 0; index < numberOfDFGIdentifiers; ++index) {
                    const Identifier& identifier = dfgIdentifiers[index];
                    cachedIdentifierUids.add(identifier.impl());
                }
            }
#endif
            WTF::storeStoreFence();
            m_cachedIdentifierUids = WTFMove(cachedIdentifierUids);
        }
        return m_cachedIdentifierUids.contains(uid);
    }

    for (unsigned index = 0; index < unlinkedIdentifiers; ++index) {
        const Identifier& identifier = unlinkedCode->identifier(index);
        if (identifier.impl() == uid)
            return true;
    }
#if ENABLE(DFG_JIT)
    ASSERT(JSC::JITCode::isOptimizingJIT(jitType()));
    auto& dfgIdentifiers = m_jitCode->dfgCommon()->m_dfgIdentifiers;
    for (unsigned index = 0; index < numberOfDFGIdentifiers; ++index) {
        const Identifier& identifier = dfgIdentifiers[index];
        if (identifier.impl() == uid)
            return true;
    }
#endif
    return false;
}
#endif

void CodeBlock::updateAllNonLazyValueProfilePredictionsAndCountLiveness(const ConcurrentJSLocker& locker, unsigned& numberOfLiveNonArgumentValueProfiles, unsigned& numberOfSamplesInProfiles)
{
    numberOfLiveNonArgumentValueProfiles = 0;
    numberOfSamplesInProfiles = 0; // If this divided by ValueProfile::numberOfBuckets equals numberOfValueProfiles() then value profiles are full.

    unsigned index = 0;
    UnlinkedCodeBlock* unlinkedCodeBlock = this->unlinkedCodeBlock();
    bool isBuiltinFunction = unlinkedCodeBlock->isBuiltinFunction();
    auto unlinkedValueProfiles = unlinkedCodeBlock->unlinkedValueProfiles().mutableSpan();
    forEachValueProfile([&](auto& profile, bool isArgument) {
        unsigned numSamples = profile.totalNumberOfSamples();
        using Profile = std::remove_reference_t<decltype(profile)>;
        static_assert(Profile::numberOfBuckets == 1);
        if (numSamples > Profile::numberOfBuckets)
            numSamples = Profile::numberOfBuckets; // We don't want profiles that are extremely hot to be given more weight.
        numberOfSamplesInProfiles += numSamples;
        if (isArgument) {
            profile.computeUpdatedPrediction(locker);
            if (!isBuiltinFunction)
                unlinkedValueProfiles[index].update(profile);
            ++index;
            return;
        }
        if (profile.numberOfSamples() || profile.isSampledBefore())
            numberOfLiveNonArgumentValueProfiles++;
        profile.computeUpdatedPrediction(locker);
        if (!isBuiltinFunction)
            unlinkedValueProfiles[index].update(profile);
        ++index;
    });

    if (m_metadata) {
        m_metadata->forEach<OpCatch>([&](auto& metadata) {
            if (metadata.m_buffer) {
                metadata.m_buffer->forEach([&](ValueProfileAndVirtualRegister& profile) {
                    profile.computeUpdatedPrediction(locker);
                });
            }
        });
    }
}

void CodeBlock::updateAllNonLazyValueProfilePredictions(const ConcurrentJSLocker& locker)
{
    unsigned ignoredValue1, ignoredValue2;
    updateAllNonLazyValueProfilePredictionsAndCountLiveness(locker, ignoredValue1, ignoredValue2);
}

void CodeBlock::updateAllLazyValueProfilePredictions(const ConcurrentJSLocker& locker)
{
#if USE(JSVALUE32_64)
    // JSVALUE64 does not need a lock.
    ASSERT(m_lock.isLocked());
#endif
#if ENABLE(DFG_JIT)
    lazyValueProfiles().computeUpdatedPredictions(locker, this);
#else
    UNUSED_PARAM(locker);
#endif
}

void CodeBlock::updateAllArrayProfilePredictions()
{
    if (!m_metadata)
        return;

    unsigned index = 0;
    UnlinkedCodeBlock* unlinkedCodeBlock = this->unlinkedCodeBlock();
    bool isBuiltinFunction = unlinkedCodeBlock->isBuiltinFunction();
    auto unlinkedArrayProfiles = unlinkedCodeBlock->unlinkedArrayProfiles().mutableSpan();
    auto process = [&] (ArrayProfile& profile) {
        profile.computeUpdatedPrediction(this);
        if (!isBuiltinFunction)
            unlinkedArrayProfiles[index].update(profile);
        ++index;
    };

#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { process(metadata.m_arrayProfile); });

    FOR_EACH_OPCODE_WITH_SIMPLE_ARRAY_PROFILE(VISIT)

#undef VISIT

    m_metadata->forEach<OpIteratorNext>([&] (auto& metadata) {
        process(metadata.m_iterableProfile);
    });
}

void CodeBlock::updateAllArrayAllocationProfilePredictions()
{
    forEachArrayAllocationProfile([&](ArrayAllocationProfile& profile) {
        profile.updateProfile();
    });
}

void CodeBlock::updateAllPredictions()
{
    {
        ConcurrentJSLocker locker(valueProfileLock());
        updateAllNonLazyValueProfilePredictions(locker);
        updateAllLazyValueProfilePredictions(locker);
    }
    updateAllArrayAllocationProfilePredictions();
    updateAllArrayProfilePredictions();
}

bool CodeBlock::shouldOptimizeNowFromBaseline()
{
    dataLogLnIf(Options::verboseOSR(), "Considering optimizing ", *this, "...");

    if (m_optimizationDelayCounter >= Options::maximumOptimizationDelay())
        return true;
    
    unsigned numberOfLiveNonArgumentValueProfiles;
    unsigned numberOfSamplesInProfiles;
    {
        ConcurrentJSLocker locker(valueProfileLock());
        updateAllNonLazyValueProfilePredictionsAndCountLiveness(locker, numberOfLiveNonArgumentValueProfiles, numberOfSamplesInProfiles);
        updateAllLazyValueProfilePredictions(locker);
    }
    updateAllArrayAllocationProfilePredictions();
    updateAllArrayProfilePredictions();

    double livenessRate = 1.0;
    if (numberOfNonArgumentValueProfiles())
        livenessRate = static_cast<double>(numberOfLiveNonArgumentValueProfiles) / numberOfNonArgumentValueProfiles();
    double fullnessRate = 1.0;
    if (totalNumberOfValueProfiles())
        fullnessRate = static_cast<double>(numberOfSamplesInProfiles) / ValueProfile::numberOfBuckets / totalNumberOfValueProfiles();

    if (Options::verboseOSR()) {
        dataLogF(
            "Profile hotness: %lf (%u / %u), %lf (%u / %u)\n",
            livenessRate,
            numberOfLiveNonArgumentValueProfiles, numberOfNonArgumentValueProfiles(),
            fullnessRate,
            numberOfSamplesInProfiles, ValueProfile::numberOfBuckets * numberOfNonArgumentValueProfiles());
    }

    if (livenessRate >= Options::desiredProfileLivenessRate() && fullnessRate >= Options::desiredProfileFullnessRate() && static_cast<unsigned>(m_optimizationDelayCounter) + 1 >= Options::minimumOptimizationDelay())
        return true;

    auto* codeBlock = this;
    CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("insufficient profiling (", livenessRate,  " / ", fullnessRate, ") for ", numberOfNonArgumentValueProfiles(), " ", totalNumberOfValueProfiles()));

    ASSERT(m_optimizationDelayCounter < std::numeric_limits<uint8_t>::max());
    m_optimizationDelayCounter++;
    optimizeAfterWarmUp();
    return false;
}

#if ENABLE(DFG_JIT)
void CodeBlock::tallyFrequentExitSites()
{
    ASSERT(JSC::JITCode::isOptimizingJIT(jitType()));
    ASSERT(JITCode::isBaselineCode(alternative()->jitType()));
    
    CodeBlock* profiledBlock = alternative();
    
    switch (jitType()) {
    case JITType::DFGJIT: {
        auto* jitCode = m_jitCode->dfg();
        for (auto& exit : jitCode->m_osrExit)
            exit.considerAddingAsFrequentExitSite(profiledBlock);
        break;
    }

#if ENABLE(FTL_JIT)
    case JITType::FTLJIT: {
        // There is no easy way to avoid duplicating this code since the FTL::JITCode::m_osrExit
        // vector contains a totally different type, that just so happens to behave like
        // DFG::JITCode::m_osrExit.
        auto* jitCode = m_jitCode->ftl();
        for (auto& exit : jitCode->m_osrExit)
            exit.considerAddingAsFrequentExitSite(profiledBlock);
        break;
    }
#endif
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}
#endif // ENABLE(DFG_JIT)

void CodeBlock::notifyLexicalBindingUpdate()
{
    JSGlobalObject* globalObject = m_globalObject.get();
    JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(globalObject->globalScope());
    SymbolTable* symbolTable = globalLexicalEnvironment->symbolTable();

    ConcurrentJSLocker locker(m_lock);

    auto isShadowed = [&] (UniquedStringImpl* uid) {
        ConcurrentJSLocker locker(symbolTable->m_lock);
        return symbolTable->contains(locker, uid);
    };

    const auto& instructionStream = instructions();
    for (const auto& instruction : instructionStream) {
        OpcodeID opcodeID = instruction->opcodeID();
        switch (opcodeID) {
        case op_resolve_scope: {
            auto bytecode = instruction->as<OpResolveScope>();
            auto& metadata = bytecode.metadata(this);
            ResolveType originalResolveType = metadata.m_resolveType;
            if (originalResolveType == GlobalProperty || originalResolveType == GlobalPropertyWithVarInjectionChecks) {
                const Identifier& ident = identifier(bytecode.m_var);
                if (isShadowed(ident.impl()))
                    metadata.m_globalLexicalBindingEpoch = 0;
                else
                    metadata.m_globalLexicalBindingEpoch = globalObject->globalLexicalBindingEpoch();
            }
            break;
        }
        default:
            break;
        }
    }
}

#if ENABLE(VERBOSE_VALUE_PROFILE)
void CodeBlock::dumpValueProfiles()
{
    dataLog("ValueProfile for ", *this, ":\n");
    forEachValueProfile([](auto& profile, bool isArgument) {
        if (isArgument)
            dataLogF("   arg: ");
        else
            dataLogF("   bc: ");
        if (!profile.numberOfSamples() && profile.m_prediction == SpecNone) {
            dataLogF("<empty>\n");
            continue;
        }
        profile.dump(WTF::dataFile());
        dataLogF("\n");
    });
}
#endif // ENABLE(VERBOSE_VALUE_PROFILE)

unsigned CodeBlock::frameRegisterCount()
{
    switch (jitType()) {
    case JITType::InterpreterThunk:
        return LLInt::frameRegisterCountFor(this);

#if ENABLE(JIT)
    case JITType::BaselineJIT:
        return JIT::frameRegisterCountFor(this->unlinkedCodeBlock());
#endif // ENABLE(JIT)

#if ENABLE(DFG_JIT)
    case JITType::DFGJIT:
    case JITType::FTLJIT:
        return jitCode()->dfgCommon()->frameRegisterCount;
#endif // ENABLE(DFG_JIT)
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

int CodeBlock::stackPointerOffset()
{
    return virtualRegisterForLocal(frameRegisterCount() - 1).offset();
}

size_t CodeBlock::predictedMachineCodeSize()
{
    VM* vm = m_vm;
    // This will be called from CodeBlock::CodeBlock before either m_vm or the
    // instructions have been initialized. It's OK to return 0 because what will really
    // matter is the recomputation of this value when the slow path is triggered.
    if (!vm)
        return 0;
    
    if (!*vm->machineCodeBytesPerBytecodeWordForBaselineJIT)
        return 0; // It's as good of a prediction as we'll get.
    
    // Be conservative: return a size that will be an overestimation 84% of the time.
    double multiplier = vm->machineCodeBytesPerBytecodeWordForBaselineJIT->mean() +
        vm->machineCodeBytesPerBytecodeWordForBaselineJIT->standardDeviation();
    
    // Be paranoid: silently reject bogus multipiers. Silently doing the "wrong" thing
    // here is OK, since this whole method is just a heuristic.
    if (multiplier < 0 || multiplier > 1000)
        return 0;
    
    double doubleResult = multiplier * bytecodeCost();
    
    // Be even more paranoid: silently reject values that won't fit into a size_t. If
    // the function is so huge that we can't even fit it into virtual memory then we
    // should probably have some other guards in place to prevent us from even getting
    // to this point.
    if (doubleResult >= maxPlusOne<size_t>)
        return 0;
    
    return static_cast<size_t>(doubleResult);
}

String CodeBlock::nameForRegister(VirtualRegister virtualRegister)
{
    for (auto& constantRegister : m_constantRegisters) {
        if (constantRegister.get().isEmpty())
            continue;
        if (SymbolTable* symbolTable = jsDynamicCast<SymbolTable*>(constantRegister.get())) {
            ConcurrentJSLocker locker(symbolTable->m_lock);
            auto end = symbolTable->end(locker);
            for (auto ptr = symbolTable->begin(locker); ptr != end; ++ptr) {
                if (ptr->value.varOffset() == VarOffset(virtualRegister)) {
                    // FIXME: This won't work from the compilation thread.
                    // https://bugs.webkit.org/show_bug.cgi?id=115300
                    return ptr->key.get();
                }
            }
        }
    }
    StringPrintStream out;
    out.print(virtualRegister);
    return out.toString();
}

ValueProfile* CodeBlock::tryGetValueProfileForBytecodeIndex(BytecodeIndex bytecodeIndex)
{
    auto instruction = instructions().at(bytecodeIndex);
    switch (instruction->opcodeID()) {

#define CASE(Op) \
    case Op::opcodeID: \
        return &m_metadata->valueProfilesEnd()[-static_cast<ptrdiff_t>(instruction->as<Op>().m_valueProfile)];

        FOR_EACH_OPCODE_WITH_VALUE_PROFILE(CASE)

#undef CASE

    case op_iterator_open:
        return &m_metadata->valueProfilesEnd()[-static_cast<ptrdiff_t>(valueProfileOffsetFor(instruction->as<OpIteratorOpen>(), bytecodeIndex.checkpoint()))];
    case op_iterator_next:
        return &m_metadata->valueProfilesEnd()[-static_cast<ptrdiff_t>(valueProfileOffsetFor(instruction->as<OpIteratorNext>(), bytecodeIndex.checkpoint()))];
    case op_instanceof:
        return &m_metadata->valueProfilesEnd()[-static_cast<ptrdiff_t>(valueProfileOffsetFor(instruction->as<OpInstanceof>(), bytecodeIndex.checkpoint()))];

    default:
        return nullptr;

    }
}

SpeculatedType CodeBlock::valueProfilePredictionForBytecodeIndex(const ConcurrentJSLocker& locker, BytecodeIndex bytecodeIndex, JSValue* specFailValue)
{
    if (ValueProfile* valueProfile = tryGetValueProfileForBytecodeIndex(bytecodeIndex)) {
        if (specFailValue)
            valueProfile->computeUpdatedPredictionForExtraValue(locker, *specFailValue);
        return valueProfile->computeUpdatedPrediction(locker);
    }
    return SpecNone;
}

ValueProfile& CodeBlock::valueProfileForBytecodeIndex(BytecodeIndex bytecodeIndex)
{
    ValueProfile* profile = tryGetValueProfileForBytecodeIndex(bytecodeIndex);
    ASSERT(profile);
    return *profile;
}

void CodeBlock::validate()
{
    BytecodeLivenessAnalysis liveness(this); // Compute directly from scratch so it doesn't effect CodeBlock footprint.
    
    FastBitVector liveAtHead = liveness.getLivenessInfoAtInstruction(this, BytecodeIndex(0));
    
    if (liveAtHead.numBits() != static_cast<size_t>(m_numCalleeLocals)) {
        beginValidationDidFail();
        dataLog("    Wrong number of bits in result!\n");
        dataLog("    Result: ", liveAtHead, "\n");
        dataLog("    Bit count: ", liveAtHead.numBits(), "\n");
        endValidationDidFail();
    }
    
    for (unsigned i = m_numCalleeLocals; i--;) {
        VirtualRegister reg = virtualRegisterForLocal(i);
        
        if (liveAtHead[i]) {
            beginValidationDidFail();
            dataLog("    Variable ", reg, " is expected to be dead.\n");
            dataLog("    Result: ", liveAtHead, "\n");
            endValidationDidFail();
        }
    }

    const auto& instructionStream = instructions();
    for (const auto& instruction : instructionStream) {
        OpcodeID opcode = instruction->opcodeID();
        if (!!baselineAlternative()->handlerForBytecodeIndex(BytecodeIndex(instruction.offset()))) {
            if (opcode == op_catch || opcode == op_enter) {
                // op_catch/op_enter logically represent an entrypoint. Entrypoints are not allowed to be
                // inside of a try block because they are responsible for bootstrapping state. And they
                // are never allowed throw an exception because of this. We rely on this when compiling
                // in the DFG. Because an entrypoint never throws, the bytecode generator will never
                // allow once inside a try block.
                beginValidationDidFail();
                dataLog("    entrypoint not allowed inside a try block.");
                endValidationDidFail();
            }
        }
    }
}

void CodeBlock::beginValidationDidFail()
{
    dataLog("Validation failure in ", *this, ":\n");
    dataLog("\n");
}

void CodeBlock::endValidationDidFail()
{
    dataLog("\n");
    dumpBytecode();
    dataLog("\n");
    dataLog("Validation failure.\n");
    RELEASE_ASSERT_NOT_REACHED();
}

void CodeBlock::addBreakpoint(unsigned numBreakpoints)
{
    m_numBreakpoints += numBreakpoints;
    ASSERT(m_numBreakpoints);
    if (JSC::JITCode::isOptimizingJIT(jitType()))
        jettison(Profiler::JettisonDueToDebuggerBreakpoint);
}

void CodeBlock::setSteppingMode(CodeBlock::SteppingMode mode)
{
    m_steppingMode = mode;
    if (mode == SteppingModeEnabled && JSC::JITCode::isOptimizingJIT(jitType()))
        jettison(Profiler::JettisonDueToDebuggerStepping);
}

int CodeBlock::outOfLineJumpOffset(const JSInstruction* pc)
{
    int offset = bytecodeOffset(pc);
    return m_unlinkedCode->outOfLineJumpOffset(offset);
}

const JSInstruction* CodeBlock::outOfLineJumpTarget(const JSInstruction* pc)
{
    int offset = bytecodeOffset(pc);
    int target = m_unlinkedCode->outOfLineJumpOffset(offset);
    return instructions().at(offset + target).ptr();
}

BinaryArithProfile* CodeBlock::binaryArithProfileForBytecodeIndex(BytecodeIndex bytecodeIndex)
{
    return binaryArithProfileForPC(instructions().at(bytecodeIndex.offset()).ptr());
}

UnaryArithProfile* CodeBlock::unaryArithProfileForBytecodeIndex(BytecodeIndex bytecodeIndex)
{
    return unaryArithProfileForPC(instructions().at(bytecodeIndex.offset()).ptr());
}

BinaryArithProfile* CodeBlock::binaryArithProfileForPC(const JSInstruction* pc)
{
    switch (pc->opcodeID()) {

#define CASE(Op) \
    case Op::opcodeID: \
        return &unlinkedCodeBlock()->binaryArithProfile(pc->as<Op>().m_profileIndex);

        FOR_EACH_OPCODE_WITH_BINARY_ARITH_PROFILE(CASE)

#undef CASE

    default:
        break;
    }

    return nullptr;
}

UnaryArithProfile* CodeBlock::unaryArithProfileForPC(const JSInstruction* pc)
{
    switch (pc->opcodeID()) {

#define CASE(Op) \
    case Op::opcodeID: \
        return &unlinkedCodeBlock()->unaryArithProfile(pc->as<Op>().m_profileIndex);

        FOR_EACH_OPCODE_WITH_UNARY_ARITH_PROFILE(CASE)

#undef CASE

    default:
        return nullptr;

    }
}

bool CodeBlock::couldTakeSpecialArithFastCase(BytecodeIndex bytecodeIndex)
{
    if (!hasBaselineJITProfiling())
        return false;
    BinaryArithProfile* profile = binaryArithProfileForBytecodeIndex(bytecodeIndex);
    if (!profile)
        return false;
    return profile->tookSpecialFastPath();
}

#if ENABLE(JIT)
DFG::CapabilityLevel CodeBlock::capabilityLevel()
{
    DFG::CapabilityLevel result = capabilityLevelState();
    if (result != DFG::CapabilityLevelNotSet)
        return result;

    result = computeCapabilityLevel();
    m_capabilityLevelState = result;
    return result;
}
#endif

void CodeBlock::insertBasicBlockBoundariesForControlFlowProfiler()
{
    if (!unlinkedCodeBlock()->hasOpProfileControlFlowBytecodeOffsets())
        return;
    const FixedVector<JSInstructionStream::Offset>& bytecodeOffsets = unlinkedCodeBlock()->opProfileControlFlowBytecodeOffsets();
    for (size_t i = 0, offsetsLength = bytecodeOffsets.size(); i < offsetsLength; i++) {
        // Because op_profile_control_flow is emitted at the beginning of every basic block, finding
        // the next op_profile_control_flow will give us the text range of a single basic block.
        size_t startIdx = bytecodeOffsets[i];
        auto instruction = instructions().at(startIdx);
        RELEASE_ASSERT(instruction->opcodeID() == op_profile_control_flow);
        auto bytecode = instruction->as<OpProfileControlFlow>();
        auto& metadata = bytecode.metadata(this);
        int basicBlockStartOffset = bytecode.m_textOffset;
        int basicBlockEndOffset;
        if (i + 1 < offsetsLength) {
            size_t endIdx = bytecodeOffsets[i + 1];
            auto endInstruction = instructions().at(endIdx);
            RELEASE_ASSERT(endInstruction->opcodeID() == op_profile_control_flow);
            basicBlockEndOffset = endInstruction->as<OpProfileControlFlow>().m_textOffset - 1;
        } else {
            basicBlockEndOffset = sourceOffset() + ownerExecutable()->source().length() - 1; // Offset before the closing brace.
            basicBlockStartOffset = std::min(basicBlockStartOffset, basicBlockEndOffset); // Some start offsets may be at the closing brace, ensure it is the offset before.
        }

        // The following check allows for the same textual JavaScript basic block to have its bytecode emitted more
        // than once and still play nice with the control flow profiler. When basicBlockStartOffset is larger than 
        // basicBlockEndOffset, it indicates that the bytecode generator has emitted code for the same AST node 
        // more than once (for example: ForInNode, Finally blocks in TryNode, etc). Though these are different 
        // basic blocks at the bytecode level, they are generated from the same textual basic block in the JavaScript 
        // program. The condition: 
        // (basicBlockEndOffset < basicBlockStartOffset) 
        // is encountered when op_profile_control_flow lies across the boundary of these duplicated bytecode basic 
        // blocks and the textual offset goes from the end of the duplicated block back to the beginning. These 
        // ranges are dummy ranges and are ignored. The duplicated bytecode basic blocks point to the same 
        // internal data structure, so if any of them execute, it will record the same textual basic block in the 
        // JavaScript program as executing.
        // At the bytecode level, this situation looks like:
        // j: op_profile_control_flow (from j->k, we have basicBlockEndOffset < basicBlockStartOffset)
        // ...
        // k: op_profile_control_flow (we want to skip over the j->k block and start fresh at offset k as the start of a new basic block k->m).
        // ...
        // m: op_profile_control_flow
        if (basicBlockEndOffset < basicBlockStartOffset) {
            RELEASE_ASSERT(i + 1 < offsetsLength); // We should never encounter dummy blocks at the end of a CodeBlock.
            metadata.m_basicBlockLocation = vm().controlFlowProfiler()->dummyBasicBlock();
            continue;
        }

        BasicBlockLocation* basicBlockLocation = vm().controlFlowProfiler()->getBasicBlockLocation(ownerExecutable()->sourceID(), basicBlockStartOffset, basicBlockEndOffset);

        // Find all functions that are enclosed within the range: [basicBlockStartOffset, basicBlockEndOffset]
        // and insert these functions' start/end offsets as gaps in the current BasicBlockLocation.
        // This is necessary because in the original source text of a JavaScript program, 
        // function literals form new basic blocks boundaries, but they aren't represented 
        // inside the CodeBlock's instruction stream.
        auto insertFunctionGaps = [basicBlockLocation, basicBlockStartOffset, basicBlockEndOffset] (const WriteBarrier<FunctionExecutable>& functionExecutable) {
            const UnlinkedFunctionExecutable* executable = functionExecutable->unlinkedExecutable();
            int functionStart = executable->unlinkedFunctionStart();
            int functionEnd = executable->unlinkedFunctionEnd();
            if (functionStart >= basicBlockStartOffset && functionEnd <= basicBlockEndOffset)
                basicBlockLocation->insertGap(functionStart, functionEnd);
        };

        for (const WriteBarrier<FunctionExecutable>& executable : m_functionDecls)
            insertFunctionGaps(executable);
        for (const WriteBarrier<FunctionExecutable>& executable : m_functionExprs)
            insertFunctionGaps(executable);

        metadata.m_basicBlockLocation = basicBlockLocation;
    }
}

#if ENABLE(JIT)
std::optional<CodeOrigin> CodeBlock::findPC(void* pc)
{
    if (auto* pcToCodeOriginMap = m_jitCode->pcToCodeOriginMap()) {
        if (std::optional<CodeOrigin> codeOrigin = pcToCodeOriginMap->findPC(pc))
            return codeOrigin;
    }

    {
        ConcurrentJSLocker locker(m_lock);
        std::optional<CodeOrigin> result;
        forEachStructureStubInfo([&](StructureStubInfo& stubInfo) {
            if (stubInfo.containsPC(pc)) {
                result = stubInfo.codeOrigin;
                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        });
        if (result)
            return result;
    }

    return m_jitCode->findPC(this, pc);
}
#endif // ENABLE(JIT)

std::optional<BytecodeIndex> CodeBlock::bytecodeIndexFromCallSiteIndex(CallSiteIndex callSiteIndex)
{
    std::optional<BytecodeIndex> bytecodeIndex;
    JITType jitType = this->jitType();
    if (jitType == JITType::InterpreterThunk || jitType == JITType::BaselineJIT)
        bytecodeIndex = callSiteIndex.bytecodeIndex();
    else if (jitType == JITType::DFGJIT || jitType == JITType::FTLJIT) {
#if ENABLE(DFG_JIT)
        RELEASE_ASSERT(canGetCodeOrigin(callSiteIndex));
        CodeOrigin origin = codeOrigin(callSiteIndex);
        bytecodeIndex = origin.bytecodeIndex();
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    return bytecodeIndex;
}

void CodeBlock::jitSoon()
{
    m_unlinkedCode->llintExecuteCounter().setNewThreshold(unlinkedCodeBlock()->thresholdForJIT(Options::thresholdForJITSoon()), this);
}

void CodeBlock::jitNextInvocation()
{
    m_unlinkedCode->llintExecuteCounter().setNewThreshold(0, this);
}

bool CodeBlock::useDataIC() const
{
#if ENABLE(DFG_JIT)
    if (jitType() == JITType::DFGJIT) {
        if (auto* jitCode = m_jitCode.get())
            return static_cast<const DFG::JITCode*>(jitCode)->isUnlinked();
    }
#endif
#if ENABLE(FTL_JIT)
    if (jitType() == JITType::FTLJIT)
        return Options::useDataICInFTL();
#endif
    return true;
}

CodePtr<JSEntryPtrTag> CodeBlock::addressForCallConcurrently(const ConcurrentJSLocker&, ArityCheckMode arityCheck) const
{
    if (!m_jitCode)
        return nullptr;
    return m_jitCode->addressForCall(arityCheck);
}

bool CodeBlock::hasInstalledVMTrapsBreakpoints() const
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    // This function may be called from a signal handler. We need to be
    // careful to not call anything that is not signal handler safe, e.g.
    // we should not perturb the refCount of m_jitCode.
    if (!canInstallVMTrapBreakpoints())
        return false;
    return m_jitCode->dfgCommon()->hasInstalledVMTrapsBreakpoints();
#else
    return false;
#endif
}

bool CodeBlock::canInstallVMTrapBreakpoints() const
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    // This function may be called from a signal handler. We need to be
    // careful to not call anything that is not signal handler safe, e.g.
    // we should not perturb the refCount of m_jitCode.
    if (!JSC::JITCode::isOptimizingJIT(jitType()))
        return false;
    if (m_jitCode->isUnlinked())
        return false;
    return true;
#else
    return false;
#endif
}

bool CodeBlock::installVMTrapBreakpoints()
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    // This function may be called from a signal handler. We need to be
    // careful to not call anything that is not signal handler safe, e.g.
    // we should not perturb the refCount of m_jitCode.
    if (!canInstallVMTrapBreakpoints())
        return false;
    auto& commonData = *m_jitCode->dfgCommon();
    commonData.installVMTrapBreakpoints(this);
    return true;
#else
    UNREACHABLE_FOR_PLATFORM();
    return false;
#endif
}

void CodeBlock::dumpMathICStats()
{
#if ENABLE(MATH_IC_STATS)
    double numAdds = 0.0;
    double totalAddSize = 0.0;
    double numMuls = 0.0;
    double totalMulSize = 0.0;
    double numNegs = 0.0;
    double totalNegSize = 0.0;
    double numSubs = 0.0;
    double totalSubSize = 0.0;

    auto countICs = [&] (CodeBlock*) {
        // FIXME: We need to re-implement this using JITCode.
    };
    heap()->forEachCodeBlock(countICs);

    dataLog("Num Adds: ", numAdds, "\n");
    dataLog("Total Add size in bytes: ", totalAddSize, "\n");
    dataLog("Average Add size: ", totalAddSize / numAdds, "\n");
    dataLog("\n");
    dataLog("Num Muls: ", numMuls, "\n");
    dataLog("Total Mul size in bytes: ", totalMulSize, "\n");
    dataLog("Average Mul size: ", totalMulSize / numMuls, "\n");
    dataLog("\n");
    dataLog("Num Negs: ", numNegs, "\n");
    dataLog("Total Neg size in bytes: ", totalNegSize, "\n");
    dataLog("Average Neg size: ", totalNegSize / numNegs, "\n");
    dataLog("\n");
    dataLog("Num Subs: ", numSubs, "\n");
    dataLog("Total Sub size in bytes: ", totalSubSize, "\n");
    dataLog("Average Sub size: ", totalSubSize / numSubs, "\n");

    dataLog("-----------------------\n");
#endif
}

void setPrinter(Printer::PrintRecord& record, CodeBlock* codeBlock)
{
    Printer::setPrinter(record, toCString(codeBlock));
}

} // namespace JSC

namespace WTF {
    
void printInternal(PrintStream& out, JSC::CodeBlock* codeBlock)
{
    if (UNLIKELY(!codeBlock)) {
        out.print("<null codeBlock>");
        return;
    }
    out.print(*codeBlock);
}
    
} // namespace WTF
