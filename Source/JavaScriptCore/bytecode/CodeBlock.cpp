/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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
#include "BytecodeGenerator.h"
#include "BytecodeLivenessAnalysis.h"
#include "BytecodeStructs.h"
#include "BytecodeUseDef.h"
#include "CallLinkStatus.h"
#include "CodeBlockInlines.h"
#include "CodeBlockSet.h"
#include "DFGCapabilities.h"
#include "DFGCommon.h"
#include "DFGDriver.h"
#include "DFGJITCode.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "EvalCodeBlock.h"
#include "FullCodeOrigin.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutableDump.h"
#include "GetPutInfo.h"
#include "InlineCallFrame.h"
#include "Instruction.h"
#include "InstructionStream.h"
#include "InterpreterInlines.h"
#include "IsoCellSetInlines.h"
#include "JIT.h"
#include "JITMathIC.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSSet.h"
#include "JSString.h"
#include "JSTemplateObjectDescriptor.h"
#include "LLIntData.h"
#include "LLIntEntrypoint.h"
#include "LLIntPrototypeLoadAdaptiveStructureWatchpoint.h"
#include "LowLevelInterpreter.h"
#include "MetadataTable.h"
#include "ModuleProgramCodeBlock.h"
#include "ObjectAllocationProfileInlines.h"
#include "OpcodeInlines.h"
#include "PCToCodeOriginMap.h"
#include "PolymorphicAccess.h"
#include "ProfilerDatabase.h"
#include "ProgramCodeBlock.h"
#include "ReduceWhitespace.h"
#include "Repatch.h"
#include "SlotVisitorInlines.h"
#include "StackVisitor.h"
#include "StructureStubInfo.h"
#include "TypeLocationCache.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/BagToHashMap.h>
#include <wtf/CommaPrinter.h>
#include <wtf/Forward.h>
#include <wtf/SimpleStats.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/UniquedStringImpl.h>

#if ENABLE(ASSEMBLER)
#include "RegisterAtOffsetList.h"
#endif

#if ENABLE(DFG_JIT)
#include "DFGOperations.h"
#endif

#if ENABLE(FTL_JIT)
#include "FTLJITCode.h"
#endif

namespace JSC {

const ClassInfo CodeBlock::s_info = {
    "CodeBlock", nullptr, nullptr, nullptr,
    CREATE_METHOD_TABLE(CodeBlock)
};

CString CodeBlock::inferredName() const
{
    switch (codeType()) {
    case GlobalCode:
        return "<global>";
    case EvalCode:
        return "<eval>";
    case FunctionCode:
        return jsCast<FunctionExecutable*>(ownerExecutable())->inferredName().utf8();
    case ModuleCode:
        return "<module>";
    default:
        CRASH();
        return CString("", 0);
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
        m_hash = CodeBlockHash(ownerScriptExecutable()->source(), specializationKind());
    }
    return m_hash;
}

CString CodeBlock::sourceCodeForTools() const
{
    if (codeType() != FunctionCode)
        return ownerScriptExecutable()->source().toUTF8();
    
    SourceProvider* provider = source();
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(ownerExecutable());
    UnlinkedFunctionExecutable* unlinked = executable->unlinkedExecutable();
    unsigned unlinkedStartOffset = unlinked->startOffset();
    unsigned linkedStartOffset = executable->source().startOffset();
    int delta = linkedStartOffset - unlinkedStartOffset;
    unsigned rangeStart = delta + unlinked->unlinkedFunctionNameStart();
    unsigned rangeEnd = delta + unlinked->startOffset() + unlinked->sourceLength();
    return toCString(
        "function ",
        provider->source().substring(rangeStart, rangeEnd - rangeStart).utf8());
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

void CodeBlock::dumpAssumingJITType(PrintStream& out, JITCode::JITType jitType) const
{
    out.print(inferredName(), "#", hashAsStringIfPossible());
    out.print(":[", RawPointer(this), "->");
    if (!!m_alternative)
        out.print(RawPointer(alternative()), "->");
    out.print(RawPointer(ownerExecutable()), ", ", jitType, codeType());

    if (codeType() == FunctionCode)
        out.print(specializationKind());
    out.print(", ", instructionCount());
    if (this->jitType() == JITCode::BaselineJIT && m_shouldAlwaysBeInlined)
        out.print(" (ShouldAlwaysBeInlined)");
    if (ownerScriptExecutable()->neverInline())
        out.print(" (NeverInline)");
    if (ownerScriptExecutable()->neverOptimize())
        out.print(" (NeverOptimize)");
    else if (ownerScriptExecutable()->neverFTLOptimize())
        out.print(" (NeverFTLOptimize)");
    if (ownerScriptExecutable()->didTryToEnterInLoop())
        out.print(" (DidTryToEnterInLoop)");
    if (ownerScriptExecutable()->isStrictMode())
        out.print(" (StrictMode)");
    if (m_didFailJITCompilation)
        out.print(" (JITFail)");
    if (this->jitType() == JITCode::BaselineJIT && m_didFailFTLCompilation)
        out.print(" (FTLFail)");
    if (this->jitType() == JITCode::BaselineJIT && m_hasBeenCompiledWithFTL)
        out.print(" (HadFTLReplacement)");
    out.print("]");
}

void CodeBlock::dump(PrintStream& out) const
{
    dumpAssumingJITType(out, jitType());
}

void CodeBlock::dumpSource()
{
    dumpSource(WTF::dataFile());
}

void CodeBlock::dumpSource(PrintStream& out)
{
    ScriptExecutable* executable = ownerScriptExecutable();
    if (executable->isFunctionExecutable()) {
        FunctionExecutable* functionExecutable = reinterpret_cast<FunctionExecutable*>(executable);
        StringView source = functionExecutable->source().provider()->getRange(
            functionExecutable->parametersStartOffset(),
            functionExecutable->typeProfilingEndOffset() + 1); // Type profiling end offset is the character before the '}'.
        
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
    BytecodeDumper<CodeBlock>::dumpBlock(this, instructions(), out, statusMap);
}

void CodeBlock::dumpBytecode(PrintStream& out, const InstructionStream::Ref& it, const ICStatusMap& statusMap)
{
    BytecodeDumper<CodeBlock>::dumpBytecode(this, out, it, statusMap);
}

void CodeBlock::dumpBytecode(PrintStream& out, unsigned bytecodeOffset, const ICStatusMap& statusMap)
{
    const auto it = instructions().at(bytecodeOffset);
    dumpBytecode(out, it, statusMap);
}

namespace {

class PutToScopeFireDetail : public FireDetail {
public:
    PutToScopeFireDetail(CodeBlock* codeBlock, const Identifier& ident)
        : m_codeBlock(codeBlock)
        , m_ident(ident)
    {
    }
    
    void dump(PrintStream& out) const override
    {
        out.print("Linking put_to_scope in ", FunctionExecutableDump(jsCast<FunctionExecutable*>(m_codeBlock->ownerExecutable())), " for ", m_ident);
    }
    
private:
    CodeBlock* m_codeBlock;
    const Identifier& m_ident;
};

} // anonymous namespace

CodeBlock::CodeBlock(VM* vm, Structure* structure, CopyParsedBlockTag, CodeBlock& other)
    : JSCell(*vm, structure)
    , m_globalObject(other.m_globalObject)
    , m_shouldAlwaysBeInlined(true)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
    , m_didFailJITCompilation(false)
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isConstructor(other.m_isConstructor)
    , m_isStrictMode(other.m_isStrictMode)
    , m_codeType(other.m_codeType)
    , m_numCalleeLocals(other.m_numCalleeLocals)
    , m_numVars(other.m_numVars)
    , m_numberOfArgumentsToSkip(other.m_numberOfArgumentsToSkip)
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_unlinkedCode(*other.vm(), this, other.m_unlinkedCode.get())
    , m_ownerExecutable(*other.vm(), this, other.m_ownerExecutable.get())
    , m_poisonedVM(other.m_poisonedVM)
    , m_instructionCount(other.m_instructionCount)
    , m_instructions(other.m_instructions)
    , m_thisRegister(other.m_thisRegister)
    , m_scopeRegister(other.m_scopeRegister)
    , m_hash(other.m_hash)
    , m_source(other.m_source)
    , m_sourceOffset(other.m_sourceOffset)
    , m_firstLineColumnOffset(other.m_firstLineColumnOffset)
    , m_constantRegisters(other.m_constantRegisters)
    , m_constantsSourceCodeRepresentation(other.m_constantsSourceCodeRepresentation)
    , m_functionDecls(other.m_functionDecls)
    , m_functionExprs(other.m_functionExprs)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_metadata(other.m_metadata)
    , m_creationTime(MonotonicTime::now())
{
    ASSERT(heap()->isDeferred());
    ASSERT(m_scopeRegister.isLocal());

    setNumParameters(other.numParameters());
    
    vm->heap.codeBlockSet().add(this);
}

void CodeBlock::finishCreation(VM& vm, CopyParsedBlockTag, CodeBlock& other)
{
    Base::finishCreation(vm);
    finishCreationCommon(vm);

    optimizeAfterWarmUp();
    jitAfterWarmUp();

    if (other.m_rareData) {
        createRareDataIfNecessary();
        
        m_rareData->m_exceptionHandlers = other.m_rareData->m_exceptionHandlers;
        m_rareData->m_switchJumpTables = other.m_rareData->m_switchJumpTables;
        m_rareData->m_stringSwitchJumpTables = other.m_rareData->m_stringSwitchJumpTables;
    }
}

CodeBlock::CodeBlock(VM* vm, Structure* structure, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock,
    JSScope* scope, RefPtr<SourceProvider>&& sourceProvider, unsigned sourceOffset, unsigned firstLineColumnOffset)
    : JSCell(*vm, structure)
    , m_globalObject(*vm, this, scope->globalObject(*vm))
    , m_shouldAlwaysBeInlined(true)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
    , m_didFailJITCompilation(false)
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isConstructor(unlinkedCodeBlock->isConstructor())
    , m_isStrictMode(unlinkedCodeBlock->isStrictMode())
    , m_codeType(unlinkedCodeBlock->codeType())
    , m_numCalleeLocals(unlinkedCodeBlock->numCalleeLocals())
    , m_numVars(unlinkedCodeBlock->numVars())
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_unlinkedCode(*vm, this, unlinkedCodeBlock)
    , m_ownerExecutable(*vm, this, ownerExecutable)
    , m_poisonedVM(vm)
    , m_instructions(&unlinkedCodeBlock->instructions())
    , m_thisRegister(unlinkedCodeBlock->thisRegister())
    , m_scopeRegister(unlinkedCodeBlock->scopeRegister())
    , m_source(WTFMove(sourceProvider))
    , m_sourceOffset(sourceOffset)
    , m_firstLineColumnOffset(firstLineColumnOffset)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_metadata(unlinkedCodeBlock->metadata().link())
    , m_creationTime(MonotonicTime::now())
{
    ASSERT(heap()->isDeferred());
    ASSERT(m_scopeRegister.isLocal());

    ASSERT(m_source);
    setNumParameters(unlinkedCodeBlock->numParameters());
    
    vm->heap.codeBlockSet().add(this);
}

// The main purpose of this function is to generate linked bytecode from unlinked bytecode. The process
// of linking is taking an abstract representation of bytecode and tying it to a GlobalObject and scope
// chain. For example, this process allows us to cache the depth of lexical environment reads that reach
// outside of this CodeBlock's compilation unit. It also allows us to generate particular constants that
// we can't generate during unlinked bytecode generation. This process is not allowed to generate control
// flow or introduce new locals. The reason for this is we rely on liveness analysis to be the same for
// all the CodeBlocks of an UnlinkedCodeBlock. We rely on this fact by caching the liveness analysis
// inside UnlinkedCodeBlock.
bool CodeBlock::finishCreation(VM& vm, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock,
    JSScope* scope)
{
    Base::finishCreation(vm);
    finishCreationCommon(vm);

    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (vm.typeProfiler() || vm.controlFlowProfiler())
        vm.functionHasExecutedCache()->removeUnexecutedRange(ownerExecutable->sourceID(), ownerExecutable->typeProfilingStartOffset(), ownerExecutable->typeProfilingEndOffset());

    setConstantRegisters(unlinkedCodeBlock->constantRegisters(), unlinkedCodeBlock->constantsSourceCodeRepresentation());
    RETURN_IF_EXCEPTION(throwScope, false);

    setConstantIdentifierSetRegisters(vm, unlinkedCodeBlock->constantIdentifierSets());
    RETURN_IF_EXCEPTION(throwScope, false);

    if (unlinkedCodeBlock->usesGlobalObject())
        m_constantRegisters[unlinkedCodeBlock->globalObjectRegister().toConstantIndex()].set(vm, this, m_globalObject.get());

    for (unsigned i = 0; i < LinkTimeConstantCount; i++) {
        LinkTimeConstant type = static_cast<LinkTimeConstant>(i);
        if (unsigned registerIndex = unlinkedCodeBlock->registerIndexForLinkTimeConstant(type))
            m_constantRegisters[registerIndex].set(vm, this, m_globalObject->jsCellForLinkTimeConstant(type));
    }

    // We already have the cloned symbol table for the module environment since we need to instantiate
    // the module environments before linking the code block. We replace the stored symbol table with the already cloned one.
    if (UnlinkedModuleProgramCodeBlock* unlinkedModuleProgramCodeBlock = jsDynamicCast<UnlinkedModuleProgramCodeBlock*>(vm, unlinkedCodeBlock)) {
        SymbolTable* clonedSymbolTable = jsCast<ModuleProgramExecutable*>(ownerExecutable)->moduleEnvironmentSymbolTable();
        if (vm.typeProfiler()) {
            ConcurrentJSLocker locker(clonedSymbolTable->m_lock);
            clonedSymbolTable->prepareForTypeProfiling(locker);
        }
        replaceConstant(unlinkedModuleProgramCodeBlock->moduleEnvironmentSymbolTableConstantRegisterOffset(), clonedSymbolTable);
    }

    bool shouldUpdateFunctionHasExecutedCache = vm.typeProfiler() || vm.controlFlowProfiler();
    m_functionDecls = RefCountedArray<WriteBarrier<FunctionExecutable>>(unlinkedCodeBlock->numberOfFunctionDecls());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionDecls(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionDecl(i);
        if (shouldUpdateFunctionHasExecutedCache)
            vm.functionHasExecutedCache()->insertUnexecutedRange(ownerExecutable->sourceID(), unlinkedExecutable->typeProfilingStartOffset(), unlinkedExecutable->typeProfilingEndOffset());
        m_functionDecls[i].set(vm, this, unlinkedExecutable->link(vm, ownerExecutable->source()));
    }

    m_functionExprs = RefCountedArray<WriteBarrier<FunctionExecutable>>(unlinkedCodeBlock->numberOfFunctionExprs());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionExprs(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionExpr(i);
        if (shouldUpdateFunctionHasExecutedCache)
            vm.functionHasExecutedCache()->insertUnexecutedRange(ownerExecutable->sourceID(), unlinkedExecutable->typeProfilingStartOffset(), unlinkedExecutable->typeProfilingEndOffset());
        m_functionExprs[i].set(vm, this, unlinkedExecutable->link(vm, ownerExecutable->source()));
    }

    if (unlinkedCodeBlock->hasRareData()) {
        createRareDataIfNecessary();
        if (size_t count = unlinkedCodeBlock->numberOfExceptionHandlers()) {
            m_rareData->m_exceptionHandlers.resizeToFit(count);
            for (size_t i = 0; i < count; i++) {
                const UnlinkedHandlerInfo& unlinkedHandler = unlinkedCodeBlock->exceptionHandler(i);
                HandlerInfo& handler = m_rareData->m_exceptionHandlers[i];
#if ENABLE(JIT)
                MacroAssemblerCodePtr<BytecodePtrTag> codePtr = m_instructions->at(unlinkedHandler.target)->isWide()
                    ? LLInt::getWideCodePtr<BytecodePtrTag>(op_catch)
                    : LLInt::getCodePtr<BytecodePtrTag>(op_catch);
                handler.initialize(unlinkedHandler, CodeLocationLabel<ExceptionHandlerPtrTag>(codePtr.retagged<ExceptionHandlerPtrTag>()));
#else
                handler.initialize(unlinkedHandler);
#endif
            }
        }

        if (size_t count = unlinkedCodeBlock->numberOfStringSwitchJumpTables()) {
            m_rareData->m_stringSwitchJumpTables.grow(count);
            for (size_t i = 0; i < count; i++) {
                UnlinkedStringJumpTable::StringOffsetTable::iterator ptr = unlinkedCodeBlock->stringSwitchJumpTable(i).offsetTable.begin();
                UnlinkedStringJumpTable::StringOffsetTable::iterator end = unlinkedCodeBlock->stringSwitchJumpTable(i).offsetTable.end();
                for (; ptr != end; ++ptr) {
                    OffsetLocation offset;
                    offset.branchOffset = ptr->value.branchOffset;
                    m_rareData->m_stringSwitchJumpTables[i].offsetTable.add(ptr->key, offset);
                }
            }
        }

        if (size_t count = unlinkedCodeBlock->numberOfSwitchJumpTables()) {
            m_rareData->m_switchJumpTables.grow(count);
            for (size_t i = 0; i < count; i++) {
                UnlinkedSimpleJumpTable& sourceTable = unlinkedCodeBlock->switchJumpTable(i);
                SimpleJumpTable& destTable = m_rareData->m_switchJumpTables[i];
                destTable.branchOffsets = sourceTable.branchOffsets;
                destTable.min = sourceTable.min;
            }
        }
    }

#if !ENABLE(C_LOOP)
    setCalleeSaveRegisters(RegisterSet::llintBaselineCalleeSaveRegisters());
#endif

    // Bookkeep the strongly referenced module environments.
    HashSet<JSModuleEnvironment*> stronglyReferencedModuleEnvironments;

    auto link_profile = [&](const auto& instruction, auto /*bytecode*/, auto& metadata) {
        m_numberOfNonArgumentValueProfiles++;
        metadata.profile.m_bytecodeOffset = instruction.offset();
    };

    auto link_arrayProfile = [&](const auto& instruction, auto /*bytecode*/, auto& metadata) {
        metadata.arrayProfile.m_bytecodeOffset = instruction.offset();
    };

    auto link_objectAllocationProfile = [&](const auto& /*instruction*/, auto bytecode, auto& metadata) {
        metadata.objectAllocationProfile.initializeProfile(vm, m_globalObject.get(), this, m_globalObject->objectPrototype(), bytecode.inlineCapacity);
    };

    auto link_arrayAllocationProfile = [&](const auto& /*instruction*/, auto bytecode, auto& metadata) {
        metadata.arrayAllocationProfile.initializeIndexingMode(bytecode.recommendedIndexingType);
    };

    auto link_hitCountForLLIntCaching = [&](const auto& /*instruction*/, auto /*bytecode*/, auto& metadata) {
        metadata.hitCountForLLIntCaching = Options::prototypeHitCountForLLIntCaching();
    };

#define LINK_FIELD(__field) \
    WTF_LAZY_JOIN(link_, __field)(instruction, bytecode, metadata);

#define INITIALIZE_METADATA(__op) \
    auto bytecode = instruction->as<__op>(); \
    auto& metadata = bytecode.metadata(this); \
    new (&metadata) __op::Metadata { bytecode }; \

#define CASE(__op) case __op::opcodeID

#define LINK(...) \
    CASE(WTF_LAZY_FIRST(__VA_ARGS__)): { \
        INITIALIZE_METADATA(WTF_LAZY_FIRST(__VA_ARGS__)) \
        WTF_LAZY_HAS_REST(__VA_ARGS__)({ \
            WTF_LAZY_FOR_EACH_TERM(LINK_FIELD,  WTF_LAZY_REST_(__VA_ARGS__)) \
        }) \
        break; \
    }

    for (const auto& instruction : *m_instructions) {
        OpcodeID opcodeID = instruction->opcodeID();
        m_instructionCount += opcodeLengths[opcodeID];
        switch (opcodeID) {
        LINK(OpHasIndexedProperty, arrayProfile)

        LINK(OpCallVarargs, arrayProfile, profile)
        LINK(OpTailCallVarargs, arrayProfile, profile)
        LINK(OpTailCallForwardArguments, arrayProfile, profile)
        LINK(OpConstructVarargs, arrayProfile, profile)
        LINK(OpGetByVal, arrayProfile, profile)

        LINK(OpGetDirectPname, profile)
        LINK(OpGetByIdWithThis, profile)
        LINK(OpTryGetById, profile)
        LINK(OpGetByIdDirect, profile)
        LINK(OpGetByValWithThis, profile)
        LINK(OpGetFromArguments, profile)
        LINK(OpToNumber, profile)
        LINK(OpToObject, profile)
        LINK(OpGetArgument, profile)
        LINK(OpToThis, profile)
        LINK(OpBitand, profile)
        LINK(OpBitor, profile)

        LINK(OpGetById, profile, hitCountForLLIntCaching)

        LINK(OpCall, profile, arrayProfile)
        LINK(OpTailCall, profile, arrayProfile)
        LINK(OpCallEval, profile, arrayProfile)
        LINK(OpConstruct, profile, arrayProfile)

        LINK(OpInByVal, arrayProfile)
        LINK(OpPutByVal, arrayProfile)
        LINK(OpPutByValDirect, arrayProfile)

        LINK(OpNewArray)
        LINK(OpNewArrayWithSize)
        LINK(OpNewArrayBuffer, arrayAllocationProfile)

        LINK(OpNewObject, objectAllocationProfile)

        LINK(OpPutById)
        LINK(OpCreateThis)

        LINK(OpAdd)
        LINK(OpMul)
        LINK(OpDiv)
        LINK(OpSub)
        LINK(OpBitxor)

        LINK(OpNegate)

        LINK(OpJneqPtr)

        LINK(OpCatch)
        LINK(OpProfileControlFlow)

        case op_resolve_scope: {
            INITIALIZE_METADATA(OpResolveScope)

            const Identifier& ident = identifier(bytecode.var);
            RELEASE_ASSERT(bytecode.resolveType != LocalClosureVar);

            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), bytecode.localScopeDepth, scope, ident, Get, bytecode.resolveType, InitializationMode::NotInitialization);
            RETURN_IF_EXCEPTION(throwScope, false);

            metadata.resolveType = op.type;
            metadata.localScopeDepth = op.depth;
            if (op.lexicalEnvironment) {
                if (op.type == ModuleVar) {
                    // Keep the linked module environment strongly referenced.
                    if (stronglyReferencedModuleEnvironments.add(jsCast<JSModuleEnvironment*>(op.lexicalEnvironment)).isNewEntry)
                        addConstant(op.lexicalEnvironment);
                    metadata.lexicalEnvironment.set(vm, this, op.lexicalEnvironment);
                } else
                    metadata.symbolTable.set(vm, this, op.lexicalEnvironment->symbolTable());
            } else if (JSScope* constantScope = JSScope::constantScopeForCodeBlock(op.type, this))
                metadata.constantScope.set(vm, this, constantScope);
            else
                metadata.globalObject = nullptr;
            break;
        }

        case op_get_from_scope: {
            INITIALIZE_METADATA(OpGetFromScope)

            link_profile(instruction, bytecode, metadata);
            metadata.watchpointSet = nullptr;

            ASSERT(!isInitialization(bytecode.getPutInfo.initializationMode()));
            if (bytecode.getPutInfo.resolveType() == LocalClosureVar) {
                metadata.getPutInfo = GetPutInfo(bytecode.getPutInfo.resolveMode(), ClosureVar, bytecode.getPutInfo.initializationMode());
                break;
            }

            const Identifier& ident = identifier(bytecode.var);
            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), bytecode.localScopeDepth, scope, ident, Get, bytecode.getPutInfo.resolveType(), InitializationMode::NotInitialization);
            RETURN_IF_EXCEPTION(throwScope, false);

            metadata.getPutInfo = GetPutInfo(bytecode.getPutInfo.resolveMode(), op.type, bytecode.getPutInfo.initializationMode());
            if (op.type == ModuleVar)
                metadata.getPutInfo = GetPutInfo(bytecode.getPutInfo.resolveMode(), ClosureVar, bytecode.getPutInfo.initializationMode());
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks || op.type == GlobalLexicalVar || op.type == GlobalLexicalVarWithVarInjectionChecks)
                metadata.watchpointSet = op.watchpointSet;
            else if (op.structure)
                metadata.structure.set(vm, this, op.structure);
            metadata.operand = op.operand;
            break;
        }

        case op_put_to_scope: {
            INITIALIZE_METADATA(OpPutToScope)

            if (bytecode.getPutInfo.resolveType() == LocalClosureVar) {
                // Only do watching if the property we're putting to is not anonymous.
                if (bytecode.var != UINT_MAX) {
                    SymbolTable* symbolTable = jsCast<SymbolTable*>(getConstant(bytecode.symbolTableOrScopeDepth));
                    const Identifier& ident = identifier(bytecode.var);
                    ConcurrentJSLocker locker(symbolTable->m_lock);
                    auto iter = symbolTable->find(locker, ident.impl());
                    ASSERT(iter != symbolTable->end(locker));
                    iter->value.prepareToWatch();
                    metadata.watchpointSet = iter->value.watchpointSet();
                } else
                    metadata.watchpointSet = nullptr;
                break;
            }

            const Identifier& ident = identifier(bytecode.var);
            metadata.watchpointSet = nullptr;
            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), bytecode.symbolTableOrScopeDepth, scope, ident, Put, bytecode.getPutInfo.resolveType(), bytecode.getPutInfo.initializationMode());
            RETURN_IF_EXCEPTION(throwScope, false);

            metadata.getPutInfo = GetPutInfo(bytecode.getPutInfo.resolveMode(), op.type, bytecode.getPutInfo.initializationMode());
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks || op.type == GlobalLexicalVar || op.type == GlobalLexicalVarWithVarInjectionChecks)
                metadata.watchpointSet = op.watchpointSet;
            else if (op.type == ClosureVar || op.type == ClosureVarWithVarInjectionChecks) {
                if (op.watchpointSet)
                    op.watchpointSet->invalidate(vm, PutToScopeFireDetail(this, ident));
            } else if (op.structure)
                metadata.structure.set(vm, this, op.structure);
            metadata.operand = op.operand;
            break;
        }

        case op_profile_type: {
            RELEASE_ASSERT(vm.typeProfiler());

            INITIALIZE_METADATA(OpProfileType)

            size_t instructionOffset = instruction.offset() + instruction->size() - 1;
            unsigned divotStart, divotEnd;
            GlobalVariableID globalVariableID = 0;
            RefPtr<TypeSet> globalTypeSet;
            bool shouldAnalyze = m_unlinkedCode->typeProfilerExpressionInfoForBytecodeOffset(instructionOffset, divotStart, divotEnd);
            SymbolTable* symbolTable = nullptr;

            switch (bytecode.flag) {
            case ProfileTypeBytecodeClosureVar: {
                const Identifier& ident = identifier(bytecode.identifier);
                unsigned localScopeDepth = bytecode.symbolTableOrScopeDepth;
                // Even though type profiling may be profiling either a Get or a Put, we can always claim a Get because
                // we're abstractly "read"ing from a JSScope.
                ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), localScopeDepth, scope, ident, Get, bytecode.resolveType, InitializationMode::NotInitialization);
                RETURN_IF_EXCEPTION(throwScope, false);

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
                int symbolTableIndex = bytecode.symbolTableOrScopeDepth;
                SymbolTable* symbolTable = jsCast<SymbolTable*>(getConstant(symbolTableIndex));
                const Identifier& ident = identifier(bytecode.identifier);
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

            if (bytecode.flag == ProfileTypeBytecodeFunctionReturnStatement)
                location->m_divotForFunctionOffsetIfReturnStatement = ownerExecutable->typeProfilingStartOffset();

            if (shouldAnalyze && isNewLocation)
                vm.typeProfiler()->insertNewLocation(location);

            metadata.typeLocation = location;
            break;
        }

        case op_debug: {
            if (instruction->as<OpDebug>().debugHookType == DidReachBreakpoint)
                m_hasDebuggerStatement = true;
            break;
        }

        case op_create_rest: {
            int numberOfArgumentsToSkip = instruction->as<OpCreateRest>().numParametersToSkip;
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

    if (vm.controlFlowProfiler())
        insertBasicBlockBoundariesForControlFlowProfiler();

    // Set optimization thresholds only after m_instructions is initialized, since these
    // rely on the instruction count (and are in theory permitted to also inspect the
    // instruction stream to more accurate assess the cost of tier-up).
    optimizeAfterWarmUp();
    jitAfterWarmUp();

    // If the concurrent thread will want the code block's hash, then compute it here
    // synchronously.
    if (Options::alwaysComputeHash())
        hash();

    if (Options::dumpGeneratedBytecodes())
        dumpBytecode();

    if (m_metadata)
        vm.heap.reportExtraMemoryAllocated(m_metadata->sizeInBytes());

    return true;
}

void CodeBlock::finishCreationCommon(VM& vm)
{
    m_ownerEdge.set(vm, this, ExecutableToCodeBlockEdge::create(vm, this));
}

CodeBlock::~CodeBlock()
{
    VM& vm = *m_poisonedVM;

    vm.heap.codeBlockSet().remove(this);
    
    if (UNLIKELY(vm.m_perBytecodeProfiler))
        vm.m_perBytecodeProfiler->notifyDestruction(this);

    if (!vm.heap.isShuttingDown() && unlinkedCodeBlock()->didOptimize() == MixedTriState)
        unlinkedCodeBlock()->setDidOptimize(FalseTriState);

#if ENABLE(VERBOSE_VALUE_PROFILE)
    dumpValueProfiles();
#endif

    // We may be destroyed before any CodeBlocks that refer to us are destroyed.
    // Consider that two CodeBlocks become unreachable at the same time. There
    // is no guarantee about the order in which the CodeBlocks are destroyed.
    // So, if we don't remove incoming calls, and get destroyed before the
    // CodeBlock(s) that have calls into us, then the CallLinkInfo vector's
    // destructor will try to remove nodes from our (no longer valid) linked list.
    unlinkIncomingCalls();
    
    // Note that our outgoing calls will be removed from other CodeBlocks'
    // m_incomingCalls linked lists through the execution of the ~CallLinkInfo
    // destructors.

#if ENABLE(JIT)
    for (auto iter = m_stubInfos.begin(); !!iter; ++iter) {
        StructureStubInfo* stub = *iter;
        stub->aboutToDie();
        stub->deref();
    }
#endif // ENABLE(JIT)
}

void CodeBlock::setConstantIdentifierSetRegisters(VM& vm, const Vector<ConstantIndentifierSetEntry>& constants)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* globalObject = m_globalObject.get();
    ExecState* exec = globalObject->globalExec();

    for (const auto& entry : constants) {
        const IdentifierSet& set = entry.first;

        Structure* setStructure = globalObject->setStructure();
        RETURN_IF_EXCEPTION(scope, void());
        JSSet* jsSet = JSSet::create(exec, vm, setStructure, set.size());
        RETURN_IF_EXCEPTION(scope, void());

        for (auto setEntry : set) {
            JSString* jsString = jsOwnedString(&vm, setEntry.get()); 
            jsSet->add(exec, jsString);
            RETURN_IF_EXCEPTION(scope, void());
        }
        m_constantRegisters[entry.second].set(vm, this, jsSet);
    }
}

void CodeBlock::setConstantRegisters(const Vector<WriteBarrier<Unknown>>& constants, const Vector<SourceCodeRepresentation>& constantsSourceCodeRepresentation)
{
    VM& vm = *m_poisonedVM;
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* globalObject = m_globalObject.get();
    ExecState* exec = globalObject->globalExec();

    ASSERT(constants.size() == constantsSourceCodeRepresentation.size());
    size_t count = constants.size();
    m_constantRegisters.resizeToFit(count);
    bool hasTypeProfiler = !!vm.typeProfiler();
    for (size_t i = 0; i < count; i++) {
        JSValue constant = constants[i].get();

        if (!constant.isEmpty()) {
            if (constant.isCell()) {
                JSCell* cell = constant.asCell();
                if (SymbolTable* symbolTable = jsDynamicCast<SymbolTable*>(vm, cell)) {
                    if (hasTypeProfiler) {
                        ConcurrentJSLocker locker(symbolTable->m_lock);
                        symbolTable->prepareForTypeProfiling(locker);
                    }

                    SymbolTable* clone = symbolTable->cloneScopePart(vm);
                    if (wasCompiledWithDebuggingOpcodes())
                        clone->setRareDataCodeBlock(this);

                    constant = clone;
                } else if (auto* descriptor = jsDynamicCast<JSTemplateObjectDescriptor*>(vm, cell)) {
                    auto* templateObject = descriptor->createTemplateObject(exec);
                    RETURN_IF_EXCEPTION(scope, void());
                    constant = templateObject;
                }
            }
        }

        m_constantRegisters[i].set(vm, this, constant);
    }

    m_constantsSourceCodeRepresentation = constantsSourceCodeRepresentation;
}

void CodeBlock::setAlternative(VM& vm, CodeBlock* alternative)
{
    RELEASE_ASSERT(alternative);
    RELEASE_ASSERT(alternative->jitCode());
    m_alternative.set(vm, this, alternative);
}

void CodeBlock::setNumParameters(int newValue)
{
    m_numParameters = newValue;

    m_argumentValueProfiles = RefCountedArray<ValueProfile>(vm()->canUseJIT() ? newValue : 0);
}

CodeBlock* CodeBlock::specialOSREntryBlockOrNull()
{
#if ENABLE(FTL_JIT)
    if (jitType() != JITCode::DFGJIT)
        return 0;
    DFG::JITCode* jitCode = m_jitCode->dfg();
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
        extraMemoryAllocated += thisObject->m_metadata->sizeInBytes();
    if (thisObject->m_jitCode)
        extraMemoryAllocated += thisObject->m_jitCode->size();
    return Base::estimatedSize(cell, vm) + extraMemoryAllocated;
}

void CodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    CodeBlock* thisObject = jsCast<CodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    visitor.append(thisObject->m_ownerEdge);
    thisObject->visitChildren(visitor);
}

void CodeBlock::visitChildren(SlotVisitor& visitor)
{
    ConcurrentJSLocker locker(m_lock);
    if (CodeBlock* otherBlock = specialOSREntryBlockOrNull())
        visitor.appendUnbarriered(otherBlock);

    size_t extraMemory = 0;
    if (m_metadata)
        extraMemory += m_metadata->sizeInBytes();
    if (m_jitCode)
        extraMemory += m_jitCode->size();
    visitor.reportExtraMemoryVisited(extraMemory);

    stronglyVisitStrongReferences(locker, visitor);
    stronglyVisitWeakReferences(locker, visitor);
    
    VM::SpaceAndFinalizerSet::finalizerSetFor(*subspace()).add(this);
}

bool CodeBlock::shouldVisitStrongly(const ConcurrentJSLocker& locker)
{
    if (Options::forceCodeBlockLiveness())
        return true;

    if (shouldJettisonDueToOldAge(locker))
        return false;

    // Interpreter and Baseline JIT CodeBlocks don't need to be jettisoned when
    // their weak references go stale. So if a basline JIT CodeBlock gets
    // scanned, we can assume that this means that it's live.
    if (!JITCode::isOptimizingJIT(jitType()))
        return true;

    return false;
}

bool CodeBlock::shouldJettisonDueToWeakReference()
{
    if (!JITCode::isOptimizingJIT(jitType()))
        return false;
    return !Heap::isMarked(this);
}

static Seconds timeToLive(JITCode::JITType jitType)
{
    if (UNLIKELY(Options::useEagerCodeBlockJettisonTiming())) {
        switch (jitType) {
        case JITCode::InterpreterThunk:
            return 10_ms;
        case JITCode::BaselineJIT:
            return 30_ms;
        case JITCode::DFGJIT:
            return 40_ms;
        case JITCode::FTLJIT:
            return 120_ms;
        default:
            return Seconds::infinity();
        }
    }

    switch (jitType) {
    case JITCode::InterpreterThunk:
        return 5_s;
    case JITCode::BaselineJIT:
        // Effectively 10 additional seconds, since BaselineJIT and
        // InterpreterThunk share a CodeBlock.
        return 15_s;
    case JITCode::DFGJIT:
        return 20_s;
    case JITCode::FTLJIT:
        return 60_s;
    default:
        return Seconds::infinity();
    }
}

bool CodeBlock::shouldJettisonDueToOldAge(const ConcurrentJSLocker&)
{
    if (Heap::isMarked(this))
        return false;

    if (UNLIKELY(Options::forceCodeBlockToJettisonDueToOldAge()))
        return true;
    
    if (timeSinceCreation() < timeToLive(jitType()))
        return false;
    
    return true;
}

#if ENABLE(DFG_JIT)
static bool shouldMarkTransition(DFG::WeakReferenceTransition& transition)
{
    if (transition.m_codeOrigin && !Heap::isMarked(transition.m_codeOrigin.get()))
        return false;
    
    if (!Heap::isMarked(transition.m_from.get()))
        return false;
    
    return true;
}
#endif // ENABLE(DFG_JIT)

void CodeBlock::propagateTransitions(const ConcurrentJSLocker&, SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);

    VM& vm = *m_poisonedVM;

    if (jitType() == JITCode::InterpreterThunk) {
        const Vector<InstructionStream::Offset>& propertyAccessInstructions = m_unlinkedCode->propertyAccessInstructions();
        for (size_t i = 0; i < propertyAccessInstructions.size(); ++i) {
            auto instruction = m_instructions->at(propertyAccessInstructions[i]);
            if (instruction->is<OpPutById>()) {
                auto& metadata = instruction->as<OpPutById>().metadata(this);
                StructureID oldStructureID = metadata.oldStructure;
                StructureID newStructureID = metadata.newStructure;
                if (!oldStructureID || !newStructureID)
                    continue;
                Structure* oldStructure =
                    vm.heap.structureIDTable().get(oldStructureID);
                Structure* newStructure =
                    vm.heap.structureIDTable().get(newStructureID);
                if (Heap::isMarked(oldStructure))
                    visitor.appendUnbarriered(newStructure);
                continue;
            }
        }
    }

#if ENABLE(JIT)
    if (JITCode::isJIT(jitType())) {
        for (auto iter = m_stubInfos.begin(); !!iter; ++iter)
            (*iter)->propagateTransitions(visitor);
    }
#endif // ENABLE(JIT)
    
#if ENABLE(DFG_JIT)
    if (JITCode::isOptimizingJIT(jitType())) {
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
        
        dfgCommon->recordedStatuses.markIfCheap(visitor);
        
        for (auto& weakReference : dfgCommon->weakStructureReferences)
            weakReference->markIfCheap(visitor);

        for (auto& transition : dfgCommon->transitions) {
            if (shouldMarkTransition(transition)) {
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

void CodeBlock::determineLiveness(const ConcurrentJSLocker&, SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);
    
#if ENABLE(DFG_JIT)
    if (Heap::isMarked(this))
        return;
    
    // In rare and weird cases, this could be called on a baseline CodeBlock. One that I found was
    // that we might decide that the CodeBlock should be jettisoned due to old age, so the
    // isMarked check doesn't protect us.
    if (!JITCode::isOptimizingJIT(jitType()))
        return;
    
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
    // Now check all of our weak references. If all of them are live, then we
    // have proved liveness and so we scan our strong references. If at end of
    // GC we still have not proved liveness, then this code block is toast.
    bool allAreLiveSoFar = true;
    for (unsigned i = 0; i < dfgCommon->weakReferences.size(); ++i) {
        JSCell* reference = dfgCommon->weakReferences[i].get();
        ASSERT(!jsDynamicCast<CodeBlock*>(*reference->vm(), reference));
        if (!Heap::isMarked(reference)) {
            allAreLiveSoFar = false;
            break;
        }
    }
    if (allAreLiveSoFar) {
        for (unsigned i = 0; i < dfgCommon->weakStructureReferences.size(); ++i) {
            if (!Heap::isMarked(dfgCommon->weakStructureReferences[i].get())) {
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

void CodeBlock::finalizeLLIntInlineCaches()
{
    VM& vm = *m_poisonedVM;
    const Vector<InstructionStream::Offset>& propertyAccessInstructions = m_unlinkedCode->propertyAccessInstructions();

    auto handleGetPutFromScope = [](auto& metadata) {
        GetPutInfo getPutInfo = metadata.getPutInfo;
        if (getPutInfo.resolveType() == GlobalVar || getPutInfo.resolveType() == GlobalVarWithVarInjectionChecks 
            || getPutInfo.resolveType() == LocalClosureVar || getPutInfo.resolveType() == GlobalLexicalVar || getPutInfo.resolveType() == GlobalLexicalVarWithVarInjectionChecks)
            return;
        WriteBarrierBase<Structure>& structure = metadata.structure;
        if (!structure || Heap::isMarked(structure.get()))
            return;
        if (Options::verboseOSR())
            dataLogF("Clearing scope access with structure %p.\n", structure.get());
        structure.clear();
    };

    for (size_t size = propertyAccessInstructions.size(), i = 0; i < size; ++i) {
        const auto curInstruction = m_instructions->at(propertyAccessInstructions[i]);
        switch (curInstruction->opcodeID()) {
        case op_get_by_id: {
            auto& metadata = curInstruction->as<OpGetById>().metadata(this);
            if (metadata.mode != GetByIdMode::Default)
                break;
            StructureID oldStructureID = metadata.modeMetadata.defaultMode.structure;
            if (!oldStructureID || Heap::isMarked(vm.heap.structureIDTable().get(oldStructureID)))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt property access.\n");
            LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(metadata);
            break;
        }
        case op_get_by_id_direct: {
            auto& metadata = curInstruction->as<OpGetByIdDirect>().metadata(this);
            StructureID oldStructureID = metadata.structure;
            if (!oldStructureID || Heap::isMarked(vm.heap.structureIDTable().get(oldStructureID)))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt property access.\n");
            metadata.structure = 0;
            metadata.offset = 0;
            break;
        }
        case op_put_by_id: {
            auto& metadata = curInstruction->as<OpPutById>().metadata(this);
            StructureID oldStructureID = metadata.oldStructure;
            StructureID newStructureID = metadata.newStructure;
            StructureChain* chain = metadata.structureChain.get();
            if ((!oldStructureID || Heap::isMarked(vm.heap.structureIDTable().get(oldStructureID)))
                && (!newStructureID || Heap::isMarked(vm.heap.structureIDTable().get(newStructureID)))
                && (!chain || Heap::isMarked(chain)))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt put transition.\n");
            metadata.oldStructure = 0;
            metadata.offset = 0;
            metadata.newStructure = 0;
            metadata.structureChain.clear();
            break;
        }
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=166418
        // We need to add optimizations for op_resolve_scope_for_hoisting_func_decl_in_eval to do link time scope resolution.
        case op_resolve_scope_for_hoisting_func_decl_in_eval:
            break;
        case op_to_this: {
            auto& metadata = curInstruction->as<OpToThis>().metadata(this);
            if (!metadata.cachedStructure || Heap::isMarked(metadata.cachedStructure.get()))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt to_this with structure %p.\n", metadata.cachedStructure.get());
            metadata.cachedStructure.clear();
            metadata.toThisStatus = merge(metadata.toThisStatus, ToThisClearedByGC);
            break;
        }
        case op_create_this: {
            auto& metadata = curInstruction->as<OpCreateThis>().metadata(this);
            auto& cacheWriteBarrier = metadata.cachedCallee;
            if (!cacheWriteBarrier || cacheWriteBarrier.unvalidatedGet() == JSCell::seenMultipleCalleeObjects())
                break;
            JSCell* cachedFunction = cacheWriteBarrier.get();
            if (Heap::isMarked(cachedFunction))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt create_this with cached callee %p.\n", cachedFunction);
            cacheWriteBarrier.clear();
            break;
        }
        case op_resolve_scope: {
            // Right now this isn't strictly necessary. Any symbol tables that this will refer to
            // are for outer functions, and we refer to those functions strongly, and they refer
            // to the symbol table strongly. But it's nice to be on the safe side.
            auto& metadata = curInstruction->as<OpResolveScope>().metadata(this);
            WriteBarrierBase<SymbolTable>& symbolTable = metadata.symbolTable;
            if (!symbolTable || Heap::isMarked(symbolTable.get()))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing dead symbolTable %p.\n", symbolTable.get());
            symbolTable.clear();
            break;
        }
        case op_get_from_scope:
            handleGetPutFromScope(curInstruction->as<OpGetFromScope>().metadata(this));
            break;
        case op_put_to_scope:
            handleGetPutFromScope(curInstruction->as<OpPutToScope>().metadata(this));
            break;
        default:
            OpcodeID opcodeID = curInstruction->opcodeID();
            ASSERT_WITH_MESSAGE_UNUSED(opcodeID, false, "Unhandled opcode in CodeBlock::finalizeUnconditionally, %s(%d) at bc %u", opcodeNames[opcodeID], opcodeID, propertyAccessInstructions[i]);
        }
    }

    // We can't just remove all the sets when we clear the caches since we might have created a watchpoint set
    // then cleared the cache without GCing in between.
    m_llintGetByIdWatchpointMap.removeIf([&] (const StructureWatchpointMap::KeyValuePairType& pair) -> bool {
        auto clear = [&] () {
            const Instruction* instruction = std::get<1>(pair.key);
            OpcodeID opcode = instruction->opcodeID();
            if (opcode == op_get_by_id) {
                if (Options::verboseOSR())
                    dataLogF("Clearing LLInt property access.\n");
                LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(instruction->as<OpGetById>().metadata(this));
            }
            return true;
        };

        if (!Heap::isMarked(std::get<0>(pair.key)))
            return clear();

        for (const LLIntPrototypeLoadAdaptiveStructureWatchpoint* watchpoint : pair.value) {
            if (!watchpoint->key().isStillLive())
                return clear();
        }

        return false;
    });

    forEachLLIntCallLinkInfo([&](LLIntCallLinkInfo& callLinkInfo) {
        if (callLinkInfo.isLinked() && !Heap::isMarked(callLinkInfo.callee.get())) {
            if (Options::verboseOSR())
                dataLog("Clearing LLInt call from ", *this, "\n");
            callLinkInfo.unlink();
        }
        if (!!callLinkInfo.lastSeenCallee && !Heap::isMarked(callLinkInfo.lastSeenCallee.get()))
            callLinkInfo.lastSeenCallee.clear();
    });
}

void CodeBlock::finalizeBaselineJITInlineCaches()
{
#if ENABLE(JIT)
    for (auto iter = callLinkInfosBegin(); !!iter; ++iter)
        (*iter)->visitWeak(*vm());

    for (auto iter = m_stubInfos.begin(); !!iter; ++iter) {
        StructureStubInfo& stubInfo = **iter;
        stubInfo.visitWeakReferences(this);
    }
#endif
}

void CodeBlock::finalizeUnconditionally(VM&)
{
    updateAllPredictions();
    
    if (JITCode::couldBeInterpreted(jitType()))
        finalizeLLIntInlineCaches();

#if ENABLE(JIT)
    if (!!jitCode())
        finalizeBaselineJITInlineCaches();
#endif

#if ENABLE(DFG_JIT)
    if (JITCode::isOptimizingJIT(jitType())) {
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
        dfgCommon->recordedStatuses.finalize();
    }
#endif // ENABLE(DFG_JIT)

    VM::SpaceAndFinalizerSet::finalizerSetFor(*subspace()).remove(this);
}

void CodeBlock::getICStatusMap(const ConcurrentJSLocker&, ICStatusMap& result)
{
#if ENABLE(JIT)
    if (JITCode::isJIT(jitType())) {
        for (StructureStubInfo* stubInfo : m_stubInfos)
            result.add(stubInfo->codeOrigin, ICStatus()).iterator->value.stubInfo = stubInfo;
        for (CallLinkInfo* callLinkInfo : m_callLinkInfos)
            result.add(callLinkInfo->codeOrigin(), ICStatus()).iterator->value.callLinkInfo = callLinkInfo;
        for (ByValInfo* byValInfo : m_byValInfos)
            result.add(CodeOrigin(byValInfo->bytecodeIndex), ICStatus()).iterator->value.byValInfo = byValInfo;
        if (JITCode::isOptimizingJIT(jitType())) {
            DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
            for (auto& pair : dfgCommon->recordedStatuses.calls)
                result.add(pair.first, ICStatus()).iterator->value.callStatus = pair.second.get();
            for (auto& pair : dfgCommon->recordedStatuses.gets)
                result.add(pair.first, ICStatus()).iterator->value.getStatus = pair.second.get();
            for (auto& pair : dfgCommon->recordedStatuses.puts)
                result.add(pair.first, ICStatus()).iterator->value.putStatus = pair.second.get();
            for (auto& pair : dfgCommon->recordedStatuses.ins)
                result.add(pair.first, ICStatus()).iterator->value.inStatus = pair.second.get();
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
StructureStubInfo* CodeBlock::addStubInfo(AccessType accessType)
{
    ConcurrentJSLocker locker(m_lock);
    return m_stubInfos.add(accessType);
}

JITAddIC* CodeBlock::addJITAddIC(ArithProfile* arithProfile, const Instruction* instruction)
{
    return m_addICs.add(arithProfile, instruction);
}

JITMulIC* CodeBlock::addJITMulIC(ArithProfile* arithProfile, const Instruction* instruction)
{
    return m_mulICs.add(arithProfile, instruction);
}

JITSubIC* CodeBlock::addJITSubIC(ArithProfile* arithProfile, const Instruction* instruction)
{
    return m_subICs.add(arithProfile, instruction);
}

JITNegIC* CodeBlock::addJITNegIC(ArithProfile* arithProfile, const Instruction* instruction)
{
    return m_negICs.add(arithProfile, instruction);
}

StructureStubInfo* CodeBlock::findStubInfo(CodeOrigin codeOrigin)
{
    for (StructureStubInfo* stubInfo : m_stubInfos) {
        if (stubInfo->codeOrigin == codeOrigin)
            return stubInfo;
    }
    return nullptr;
}

ByValInfo* CodeBlock::addByValInfo()
{
    ConcurrentJSLocker locker(m_lock);
    return m_byValInfos.add();
}

CallLinkInfo* CodeBlock::addCallLinkInfo()
{
    ConcurrentJSLocker locker(m_lock);
    return m_callLinkInfos.add();
}

CallLinkInfo* CodeBlock::getCallLinkInfoForBytecodeIndex(unsigned index)
{
    for (auto iter = m_callLinkInfos.begin(); !!iter; ++iter) {
        if ((*iter)->codeOrigin() == CodeOrigin(index))
            return *iter;
    }
    return nullptr;
}

void CodeBlock::resetJITData()
{
    RELEASE_ASSERT(!JITCode::isJIT(jitType()));
    ConcurrentJSLocker locker(m_lock);
    
    // We can clear these because no other thread will have references to any stub infos, call
    // link infos, or by val infos if we don't have JIT code. Attempts to query these data
    // structures using the concurrent API (getICStatusMap and friends) will return nothing if we
    // don't have JIT code.
    m_stubInfos.clear();
    m_callLinkInfos.clear();
    m_byValInfos.clear();
    
    // We can clear this because the DFG's queries to these data structures are guarded by whether
    // there is JIT code.
    m_rareCaseProfiles.clear();
}
#endif

void CodeBlock::visitOSRExitTargets(const ConcurrentJSLocker&, SlotVisitor& visitor)
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

void CodeBlock::stronglyVisitStrongReferences(const ConcurrentJSLocker& locker, SlotVisitor& visitor)
{
    UNUSED_PARAM(locker);
    
    visitor.append(m_globalObject);
    visitor.append(m_ownerExecutable); // This is extra important since it causes the ExecutableToCodeBlockEdge to be marked.
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
    for (ByValInfo* byValInfo : m_byValInfos)
        visitor.append(byValInfo->cachedSymbol);
#endif

#if ENABLE(DFG_JIT)
    if (JITCode::isOptimizingJIT(jitType()))
        visitOSRExitTargets(locker, visitor);
#endif
}

void CodeBlock::stronglyVisitWeakReferences(const ConcurrentJSLocker&, SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);

#if ENABLE(DFG_JIT)
    if (!JITCode::isOptimizingJIT(jitType()))
        return;
    
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();

    for (auto& transition : dfgCommon->transitions) {
        if (!!transition.m_codeOrigin)
            visitor.append(transition.m_codeOrigin); // Almost certainly not necessary, since the code origin should also be a weak reference. Better to be safe, though.
        visitor.append(transition.m_from);
        visitor.append(transition.m_to);
    }

    for (auto& weakReference : dfgCommon->weakReferences)
        visitor.append(weakReference);

    for (auto& weakStructureReference : dfgCommon->weakStructureReferences)
        visitor.append(weakStructureReference);

    dfgCommon->livenessHasBeenProved = true;
#endif    
}

CodeBlock* CodeBlock::baselineAlternative()
{
#if ENABLE(JIT)
    CodeBlock* result = this;
    while (result->alternative())
        result = result->alternative();
    RELEASE_ASSERT(result);
    RELEASE_ASSERT(JITCode::isBaselineCode(result->jitType()) || result->jitType() == JITCode::None);
    return result;
#else
    return this;
#endif
}

CodeBlock* CodeBlock::baselineVersion()
{
#if ENABLE(JIT)
    JITCode::JITType selfJITType = jitType();
    if (JITCode::isBaselineCode(selfJITType))
        return this;
    CodeBlock* result = replacement();
    if (!result) {
        if (JITCode::isOptimizingJIT(selfJITType)) {
            // The replacement can be null if we've had a memory clean up and the executable
            // has been purged of its codeBlocks (see ExecutableBase::clearCode()). Regardless,
            // the current codeBlock is still live on the stack, and as an optimizing JIT
            // codeBlock, it will keep its baselineAlternative() alive for us to fetch below.
            result = this;
        } else {
            // This can happen if we're creating the original CodeBlock for an executable.
            // Assume that we're the baseline CodeBlock.
            RELEASE_ASSERT(selfJITType == JITCode::None);
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
bool CodeBlock::hasOptimizedReplacement(JITCode::JITType typeToReplace)
{
    CodeBlock* replacement = this->replacement();
    return replacement && JITCode::isHigherTier(replacement->jitType(), typeToReplace);
}

bool CodeBlock::hasOptimizedReplacement()
{
    return hasOptimizedReplacement(jitType());
}
#endif

HandlerInfo* CodeBlock::handlerForBytecodeOffset(unsigned bytecodeOffset, RequiredHandler requiredHandler)
{
    RELEASE_ASSERT(bytecodeOffset < instructions().size());
    return handlerForIndex(bytecodeOffset, requiredHandler);
}

HandlerInfo* CodeBlock::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    if (!m_rareData)
        return 0;
    return HandlerInfo::handlerForIndex(m_rareData->m_exceptionHandlers, index, requiredHandler);
}

CallSiteIndex CodeBlock::newExceptionHandlingCallSiteIndex(CallSiteIndex originalCallSite)
{
#if ENABLE(DFG_JIT)
    RELEASE_ASSERT(JITCode::isOptimizingJIT(jitType()));
    RELEASE_ASSERT(canGetCodeOrigin(originalCallSite));
    ASSERT(!!handlerForIndex(originalCallSite.bits()));
    CodeOrigin originalOrigin = codeOrigin(originalCallSite);
    return m_jitCode->dfgCommon()->addUniqueCallSiteIndex(originalOrigin);
#else
    // We never create new on-the-fly exception handling
    // call sites outside the DFG/FTL inline caches.
    UNUSED_PARAM(originalCallSite);
    RELEASE_ASSERT_NOT_REACHED();
    return CallSiteIndex(0u);
#endif
}



void CodeBlock::ensureCatchLivenessIsComputedForBytecodeOffset(InstructionStream::Offset bytecodeOffset)
{
    auto instruction = m_instructions->at(bytecodeOffset);
    OpCatch op = instruction->as<OpCatch>();
    auto& metadata = op.metadata(this);
    if (!!metadata.buffer) {
#if !ASSERT_DISABLED
        ConcurrentJSLocker locker(m_lock);
        bool found = false;
        for (auto& profile : m_catchProfiles) {
            if (profile.get() == metadata.buffer) {
                found = true;
                break;
            }
        }
        ASSERT(found);
#endif
        return;
    }

    ensureCatchLivenessIsComputedForBytecodeOffsetSlow(op, bytecodeOffset);
}

void CodeBlock::ensureCatchLivenessIsComputedForBytecodeOffsetSlow(const OpCatch& op, InstructionStream::Offset bytecodeOffset)
{
    BytecodeLivenessAnalysis& bytecodeLiveness = livenessAnalysis();

    // We get the live-out set of variables at op_catch, not the live-in. This
    // is because the variables that the op_catch defines might be dead, and
    // we can avoid profiling them and extracting them when doing OSR entry
    // into the DFG.

    auto nextOffset = m_instructions->at(bytecodeOffset).next().offset();
    FastBitVector liveLocals = bytecodeLiveness.getLivenessInfoAtBytecodeOffset(this, nextOffset);
    Vector<VirtualRegister> liveOperands;
    liveOperands.reserveInitialCapacity(liveLocals.bitCount());
    liveLocals.forEachSetBit([&] (unsigned liveLocal) {
        liveOperands.append(virtualRegisterForLocal(liveLocal));
    });

    for (int i = 0; i < numParameters(); ++i)
        liveOperands.append(virtualRegisterForArgument(i));

    auto profiles = std::make_unique<ValueProfileAndOperandBuffer>(liveOperands.size());
    RELEASE_ASSERT(profiles->m_size == liveOperands.size());
    for (unsigned i = 0; i < profiles->m_size; ++i)
        profiles->m_buffer.get()[i].m_operand = liveOperands[i].offset();

    // The compiler thread will read this pointer value and then proceed to dereference it
    // if it is not null. We need to make sure all above stores happen before this store so
    // the compiler thread reads fully initialized data.
    WTF::storeStoreFence(); 

    op.metadata(this).buffer = profiles.get();

    {
        ConcurrentJSLocker locker(m_lock);
        m_catchProfiles.append(WTFMove(profiles));
    }
}

void CodeBlock::removeExceptionHandlerForCallSite(CallSiteIndex callSiteIndex)
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

unsigned CodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    RELEASE_ASSERT(bytecodeOffset < instructions().size());
    return ownerScriptExecutable()->firstLine() + m_unlinkedCode->lineNumberForBytecodeOffset(bytecodeOffset);
}

unsigned CodeBlock::columnNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    int divot;
    int startOffset;
    int endOffset;
    unsigned line;
    unsigned column;
    expressionRangeForBytecodeOffset(bytecodeOffset, divot, startOffset, endOffset, line, column);
    return column;
}

void CodeBlock::expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column) const
{
    m_unlinkedCode->expressionRangeForBytecodeOffset(bytecodeOffset, divot, startOffset, endOffset, line, column);
    divot += m_sourceOffset;
    column += line ? 1 : firstLineColumnOffset();
    line += ownerScriptExecutable()->firstLine();
}

bool CodeBlock::hasOpDebugForLineAndColumn(unsigned line, unsigned column)
{
    for (const auto& it : *m_instructions) {
        if (it->is<OpDebug>()) {
            int unused;
            unsigned opDebugLine;
            unsigned opDebugColumn;
            expressionRangeForBytecodeOffset(it.offset(), unused, unused, unused, opDebugLine, opDebugColumn);
            if (line == opDebugLine && (column == Breakpoint::unspecifiedColumn || column == opDebugColumn))
                return true;
        }
    }
    return false;
}

void CodeBlock::shrinkToFit(ShrinkMode shrinkMode)
{
    ConcurrentJSLocker locker(m_lock);

    m_rareCaseProfiles.shrinkToFit();
    
    if (shrinkMode == EarlyShrink) {
        m_constantRegisters.shrinkToFit();
        m_constantsSourceCodeRepresentation.shrinkToFit();
        
        if (m_rareData) {
            m_rareData->m_switchJumpTables.shrinkToFit();
            m_rareData->m_stringSwitchJumpTables.shrinkToFit();
        }
    } // else don't shrink these, because we would have already pointed pointers into these tables.
}

#if ENABLE(JIT)
void CodeBlock::linkIncomingCall(ExecState* callerFrame, CallLinkInfo* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingCalls.push(incoming);
}

void CodeBlock::linkIncomingPolymorphicCall(ExecState* callerFrame, PolymorphicCallNode* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingPolymorphicCalls.push(incoming);
}
#endif // ENABLE(JIT)

void CodeBlock::unlinkIncomingCalls()
{
    while (m_incomingLLIntCalls.begin() != m_incomingLLIntCalls.end())
        m_incomingLLIntCalls.begin()->unlink();
#if ENABLE(JIT)
    while (m_incomingCalls.begin() != m_incomingCalls.end())
        m_incomingCalls.begin()->unlink(*vm());
    while (m_incomingPolymorphicCalls.begin() != m_incomingPolymorphicCalls.end())
        m_incomingPolymorphicCalls.begin()->unlink(*vm());
#endif // ENABLE(JIT)
}

void CodeBlock::linkIncomingCall(ExecState* callerFrame, LLIntCallLinkInfo* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingLLIntCalls.push(incoming);
}

CodeBlock* CodeBlock::newReplacement()
{
    return ownerScriptExecutable()->newReplacementCodeBlockFor(specializationKind());
}

#if ENABLE(JIT)
CodeBlock* CodeBlock::replacement()
{
    const ClassInfo* classInfo = this->classInfo(*vm());

    if (classInfo == FunctionCodeBlock::info())
        return jsCast<FunctionExecutable*>(ownerExecutable())->codeBlockFor(m_isConstructor ? CodeForConstruct : CodeForCall);

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
    const ClassInfo* classInfo = this->classInfo(*vm());

    if (classInfo == FunctionCodeBlock::info()) {
        if (m_isConstructor)
            return DFG::functionForConstructCapabilityLevel(this);
        return DFG::functionForCallCapabilityLevel(this);
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
    
    CODEBLOCK_LOG_EVENT(this, "jettison", ("due to ", reason, ", counting = ", mode == CountReoptimization, ", detail = ", pointerDump(detail)));

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
            for (auto& transition : dfgCommon->transitions) {
                JSCell* origin = transition.m_codeOrigin.get();
                JSCell* from = transition.m_from.get();
                JSCell* to = transition.m_to.get();
                if ((!origin || Heap::isMarked(origin)) && Heap::isMarked(from))
                    continue;
                dataLog("    Transition under ", RawPointer(origin), ", ", RawPointer(from), " -> ", RawPointer(to), ".\n");
            }
            for (unsigned i = 0; i < dfgCommon->weakReferences.size(); ++i) {
                JSCell* weak = dfgCommon->weakReferences[i].get();
                if (Heap::isMarked(weak))
                    continue;
                dataLog("    Weak reference ", RawPointer(weak), ".\n");
            }
        }
    }
#endif // ENABLE(DFG_JIT)

    VM& vm = *m_poisonedVM;
    DeferGCForAWhile deferGC(*heap());
    
    // We want to accomplish two things here:
    // 1) Make sure that if this CodeBlock is on the stack right now, then if we return to it
    //    we should OSR exit at the top of the next bytecode instruction after the return.
    // 2) Make sure that if we call the owner executable, then we shouldn't call this CodeBlock.

#if ENABLE(DFG_JIT)
    if (reason != Profiler::JettisonDueToOldAge) {
        Profiler::Compilation* compilation = jitCode()->dfgCommon()->compilation.get();
        if (UNLIKELY(compilation))
            compilation->setJettisonReason(reason, detail);
        
        // This accomplishes (1), and does its own book-keeping about whether it has already happened.
        if (!jitCode()->dfgCommon()->invalidate()) {
            // We've already been invalidated.
            RELEASE_ASSERT(this != replacement() || (vm.heap.isCurrentThreadBusy() && !Heap::isMarked(ownerScriptExecutable())));
            return;
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
    if (vm.heap.isCurrentThreadBusy() && !Heap::isMarked(ownerScriptExecutable()))
        return;

    // This accomplishes (2).
    ownerScriptExecutable()->installCode(vm, alternative(), codeType(), specializationKind());

#if ENABLE(DFG_JIT)
    if (DFG::shouldDumpDisassembly())
        dataLog("    Did install baseline version of ", *this, "\n");
#endif // ENABLE(DFG_JIT)
}

JSGlobalObject* CodeBlock::globalObjectFor(CodeOrigin codeOrigin)
{
    if (!codeOrigin.inlineCallFrame)
        return globalObject();
    return codeOrigin.inlineCallFrame->baselineCodeBlock->globalObject();
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

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        CallFrame* currentCallFrame = visitor->callFrame();

        if (currentCallFrame == m_startCallFrame)
            m_foundStartCallFrame = true;

        if (m_foundStartCallFrame) {
            if (visitor->callFrame()->codeBlock() == m_codeBlock) {
                m_didRecurse = true;
                return StackVisitor::Done;
            }

            if (!m_depthToCheck--)
                return StackVisitor::Done;
        }

        return StackVisitor::Continue;
    }

    bool didRecurse() const { return m_didRecurse; }

private:
    CallFrame* m_startCallFrame;
    CodeBlock* m_codeBlock;
    mutable unsigned m_depthToCheck;
    mutable bool m_foundStartCallFrame;
    mutable bool m_didRecurse;
};

void CodeBlock::noticeIncomingCall(ExecState* callerFrame)
{
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    
    if (Options::verboseCallLink())
        dataLog("Noticing call link from ", pointerDump(callerCodeBlock), " to ", *this, "\n");
    
#if ENABLE(DFG_JIT)
    if (!m_shouldAlwaysBeInlined)
        return;
    
    if (!callerCodeBlock) {
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is native.\n");
        return;
    }

    if (!hasBaselineJITProfiling())
        return;

    if (!DFG::mightInlineFunction(this))
        return;

    if (!canInline(capabilityLevelState()))
        return;
    
    if (!DFG::isSmallEnoughToInlineCodeInto(callerCodeBlock)) {
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is too large.\n");
        return;
    }

    if (callerCodeBlock->jitType() == JITCode::InterpreterThunk) {
        // If the caller is still in the interpreter, then we can't expect inlining to
        // happen anytime soon. Assume it's profitable to optimize it separately. This
        // ensures that a function is SABI only if it is called no more frequently than
        // any of its callers.
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is in LLInt.\n");
        return;
    }
    
    if (JITCode::isOptimizingJIT(callerCodeBlock->jitType())) {
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI bcause caller was already optimized.\n");
        return;
    }
    
    if (callerCodeBlock->codeType() != FunctionCode) {
        // If the caller is either eval or global code, assume that that won't be
        // optimized anytime soon. For eval code this is particularly true since we
        // delay eval optimization by a *lot*.
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is not a function.\n");
        return;
    }

    // Recursive calls won't be inlined.
    RecursionCheckFunctor functor(callerFrame, this, Options::maximumInliningDepth());
    vm()->topCallFrame->iterate(functor);

    if (functor.didRecurse()) {
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because recursion was detected.\n");
        m_shouldAlwaysBeInlined = false;
        return;
    }
    
    if (callerCodeBlock->capabilityLevelState() == DFG::CapabilityLevelNotSet) {
        dataLog("In call from ", FullCodeOrigin(callerCodeBlock, callerFrame->codeOrigin()), " to ", *this, ": caller's DFG capability level is not set.\n");
        CRASH();
    }
    
    if (canCompile(callerCodeBlock->capabilityLevelState()))
        return;
    
    if (Options::verboseCallLink())
        dataLog("    Clearing SABI because the caller is not a DFG candidate.\n");
    
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
void CodeBlock::setCalleeSaveRegisters(RegisterSet calleeSaveRegisters)
{
    m_calleeSaveRegisters = std::make_unique<RegisterAtOffsetList>(calleeSaveRegisters);
}

void CodeBlock::setCalleeSaveRegisters(std::unique_ptr<RegisterAtOffsetList> registerAtOffsetList)
{
    m_calleeSaveRegisters = WTFMove(registerAtOffsetList);
}
    
static size_t roundCalleeSaveSpaceAsVirtualRegisters(size_t calleeSaveRegisters)
{

    return (WTF::roundUpToMultipleOf(sizeof(Register), calleeSaveRegisters * sizeof(CPURegister)) / sizeof(Register));

}

size_t CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters()
{
    return roundCalleeSaveSpaceAsVirtualRegisters(numberOfLLIntBaselineCalleeSaveRegisters());
}

size_t CodeBlock::calleeSaveSpaceAsVirtualRegisters()
{
    return roundCalleeSaveSpaceAsVirtualRegisters(m_calleeSaveRegisters->size());
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
    return ((replacement && JITCode::isOptimizingJIT(replacement->jitType())) ? 1 : 0) + m_reoptimizationRetryCounter;
}

int32_t CodeBlock::codeTypeThresholdMultiplier() const
{
    if (codeType() == EvalCode)
        return Options::evalThresholdMultiplier();
    
    return 1;
}

double CodeBlock::optimizationThresholdScalingFactor()
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
    
    double instructionCount = this->instructionCount();
    
    ASSERT(instructionCount); // Make sure this is called only after we have an instruction stream; otherwise it'll just return the value of d, which makes no sense.
    
    double result = d + a * sqrt(instructionCount + b) + c * instructionCount;
    
    result *= codeTypeThresholdMultiplier();
    
    if (Options::verboseOSR()) {
        dataLog(
            *this, ": instruction count is ", instructionCount,
            ", scaling execution counter by ", result, " * ", codeTypeThresholdMultiplier(),
            "\n");
    }
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
    if (DFG::Worklist* worklist = DFG::existingGlobalDFGWorklistOrNull()) {
        if (worklist->compilationState(DFG::CompilationKey(this, DFG::DFGMode))
            == DFG::Worklist::Compiled) {
            optimizeNextInvocation();
            return true;
        }
    }
#endif
    
    return m_jitExecuteCounter.checkIfThresholdCrossedAndSet(this);
}

#if ENABLE(DFG_JIT)
auto CodeBlock::updateOSRExitCounterAndCheckIfNeedToReoptimize(DFG::OSRExitState& exitState) -> OptimizeAction
{
    DFG::OSRExitBase& exit = exitState.exit;
    if (!exitKindMayJettison(exit.m_kind)) {
        // FIXME: We may want to notice that we're frequently exiting
        // at an op_catch that we didn't compile an entrypoint for, and
        // then trigger a reoptimization of this CodeBlock:
        // https://bugs.webkit.org/show_bug.cgi?id=175842
        return OptimizeAction::None;
    }

    exit.m_count++;
    m_osrExitCounter++;

    CodeBlock* baselineCodeBlock = exitState.baselineCodeBlock;
    ASSERT(baselineCodeBlock == baselineAlternative());
    if (UNLIKELY(baselineCodeBlock->jitExecuteCounter().hasCrossedThreshold()))
        return OptimizeAction::ReoptimizeNow;

    // We want to figure out if there's a possibility that we're in a loop. For the outermost
    // code block in the inline stack, we handle this appropriately by having the loop OSR trigger
    // check the exit count of the replacement of the CodeBlock from which we are OSRing. The
    // problem is the inlined functions, which might also have loops, but whose baseline versions
    // don't know where to look for the exit count. Figure out if those loops are severe enough
    // that we had tried to OSR enter. If so, then we should use the loop reoptimization trigger.
    // Otherwise, we should use the normal reoptimization trigger.

    bool didTryToEnterInLoop = false;
    for (InlineCallFrame* inlineCallFrame = exit.m_codeOrigin.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->directCaller.inlineCallFrame) {
        if (inlineCallFrame->baselineCodeBlock->ownerScriptExecutable()->didTryToEnterInLoop()) {
            didTryToEnterInLoop = true;
            break;
        }
    }

    uint32_t exitCountThreshold = didTryToEnterInLoop
        ? exitCountThresholdForReoptimizationFromLoop()
        : exitCountThresholdForReoptimization();

    if (m_osrExitCounter > exitCountThreshold)
        return OptimizeAction::ReoptimizeNow;

    // Too few fails. Adjust the execution counter such that the target is to only optimize after a while.
    baselineCodeBlock->m_jitExecuteCounter.setNewThresholdForOSRExit(exitState.activeThreshold, exitState.memoryUsageAdjustedThreshold);
    return OptimizeAction::None;
}
#endif

void CodeBlock::optimizeNextInvocation()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing next invocation.\n");
    m_jitExecuteCounter.setNewThreshold(0, this);
}

void CodeBlock::dontOptimizeAnytimeSoon()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Not optimizing anytime soon.\n");
    m_jitExecuteCounter.deferIndefinitely();
}

void CodeBlock::optimizeAfterWarmUp()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing after warm-up.\n");
#if ENABLE(DFG_JIT)
    m_jitExecuteCounter.setNewThreshold(
        adjustedCounterValue(Options::thresholdForOptimizeAfterWarmUp()), this);
#endif
}

void CodeBlock::optimizeAfterLongWarmUp()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing after long warm-up.\n");
#if ENABLE(DFG_JIT)
    m_jitExecuteCounter.setNewThreshold(
        adjustedCounterValue(Options::thresholdForOptimizeAfterLongWarmUp()), this);
#endif
}

void CodeBlock::optimizeSoon()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing soon.\n");
#if ENABLE(DFG_JIT)
    m_jitExecuteCounter.setNewThreshold(
        adjustedCounterValue(Options::thresholdForOptimizeSoon()), this);
#endif
}

void CodeBlock::forceOptimizationSlowPathConcurrently()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Forcing slow path concurrently.\n");
    m_jitExecuteCounter.forceSlowPathConcurrently();
}

#if ENABLE(DFG_JIT)
void CodeBlock::setOptimizationThresholdBasedOnCompilationResult(CompilationResult result)
{
    JITCode::JITType type = jitType();
    if (type != JITCode::BaselineJIT) {
        dataLog(*this, ": expected to have baseline code but have ", type, "\n");
        CRASH_WITH_INFO(bitwise_cast<uintptr_t>(jitCode().get()), type);
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
        RELEASE_ASSERT(replacement && JITCode::isOptimizingJIT(replacement->jitType()));
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
    ASSERT(JITCode::isOptimizingJIT(jitType()));
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

ArrayProfile* CodeBlock::getArrayProfile(const ConcurrentJSLocker&, unsigned bytecodeOffset)
{
    auto instruction = m_instructions->at(bytecodeOffset);
    switch (instruction->opcodeID()) {
#define CASE(Op) \
    case Op::opcodeID: \
        return &instruction->as<Op>().metadata(this).arrayProfile;

    FOR_EACH_OPCODE_WITH_ARRAY_PROFILE(CASE)
#undef CASE

    case OpGetById::opcodeID: {
        auto bytecode = instruction->as<OpGetById>();
        auto& metadata = bytecode.metadata(this);
        if (metadata.mode == GetByIdMode::ArrayLength)
            return &metadata.modeMetadata.arrayLengthMode.arrayProfile;
        break;
    }

    default:
        break;
    }

    for (auto& m_arrayProfile : m_arrayProfiles) {
        if (m_arrayProfile.bytecodeOffset() == bytecodeOffset)
            return &m_arrayProfile;
    }
    return 0;
}

ArrayProfile* CodeBlock::getArrayProfile(unsigned bytecodeOffset)
{
    ConcurrentJSLocker locker(m_lock);
    return getArrayProfile(locker, bytecodeOffset);
}

ArrayProfile* CodeBlock::addArrayProfile(const ConcurrentJSLocker&, unsigned bytecodeOffset)
{
    m_arrayProfiles.append(ArrayProfile(bytecodeOffset));
    return &m_arrayProfiles.last();
}

ArrayProfile* CodeBlock::addArrayProfile(unsigned bytecodeOffset)
{
    ConcurrentJSLocker locker(m_lock);
    return addArrayProfile(locker, bytecodeOffset);
}

ArrayProfile* CodeBlock::getOrAddArrayProfile(const ConcurrentJSLocker& locker, unsigned bytecodeOffset)
{
    ArrayProfile* result = getArrayProfile(locker, bytecodeOffset);
    if (result)
        return result;
    return addArrayProfile(locker, bytecodeOffset);
}

ArrayProfile* CodeBlock::getOrAddArrayProfile(unsigned bytecodeOffset)
{
    ConcurrentJSLocker locker(m_lock);
    return getOrAddArrayProfile(locker, bytecodeOffset);
}


#if ENABLE(DFG_JIT)
Vector<CodeOrigin, 0, UnsafeVectorOverflow>& CodeBlock::codeOrigins()
{
    return m_jitCode->dfgCommon()->codeOrigins;
}

size_t CodeBlock::numberOfDFGIdentifiers() const
{
    if (!JITCode::isOptimizingJIT(jitType()))
        return 0;
    
    return m_jitCode->dfgCommon()->dfgIdentifiers.size();
}

const Identifier& CodeBlock::identifier(int index) const
{
    size_t unlinkedIdentifiers = m_unlinkedCode->numberOfIdentifiers();
    if (static_cast<unsigned>(index) < unlinkedIdentifiers)
        return m_unlinkedCode->identifier(index);
    ASSERT(JITCode::isOptimizingJIT(jitType()));
    return m_jitCode->dfgCommon()->dfgIdentifiers[index - unlinkedIdentifiers];
}
#endif // ENABLE(DFG_JIT)

void CodeBlock::updateAllPredictionsAndCountLiveness(unsigned& numberOfLiveNonArgumentValueProfiles, unsigned& numberOfSamplesInProfiles)
{
    ConcurrentJSLocker locker(m_lock);

    numberOfLiveNonArgumentValueProfiles = 0;
    numberOfSamplesInProfiles = 0; // If this divided by ValueProfile::numberOfBuckets equals numberOfValueProfiles() then value profiles are full.

    forEachValueProfile([&](ValueProfile& profile) {
        unsigned numSamples = profile.totalNumberOfSamples();
        if (numSamples > ValueProfile::numberOfBuckets)
            numSamples = ValueProfile::numberOfBuckets; // We don't want profiles that are extremely hot to be given more weight.
        numberOfSamplesInProfiles += numSamples;
        if (profile.m_bytecodeOffset < 0) {
            profile.computeUpdatedPrediction(locker);
            return;
        }
        if (profile.numberOfSamples() || profile.m_prediction != SpecNone)
            numberOfLiveNonArgumentValueProfiles++;
        profile.computeUpdatedPrediction(locker);
    });

    for (auto& profileBucket : m_catchProfiles) {
        profileBucket->forEach([&] (ValueProfileAndOperand& profile) {
            profile.m_profile.computeUpdatedPrediction(locker);
        });
    }
    
#if ENABLE(DFG_JIT)
    m_lazyOperandValueProfiles.computeUpdatedPredictions(locker);
#endif
}

void CodeBlock::updateAllValueProfilePredictions()
{
    unsigned ignoredValue1, ignoredValue2;
    updateAllPredictionsAndCountLiveness(ignoredValue1, ignoredValue2);
}

void CodeBlock::updateAllArrayPredictions()
{
    ConcurrentJSLocker locker(m_lock);
    
    forEachArrayProfile([&](ArrayProfile& profile) {
        profile.computeUpdatedPrediction(locker, this);
    });
    
    forEachArrayAllocationProfile([&](ArrayAllocationProfile& profile) {
        profile.updateProfile();
    });
}

void CodeBlock::updateAllPredictions()
{
    updateAllValueProfilePredictions();
    updateAllArrayPredictions();
}

bool CodeBlock::shouldOptimizeNow()
{
    if (Options::verboseOSR())
        dataLog("Considering optimizing ", *this, "...\n");

    if (m_optimizationDelayCounter >= Options::maximumOptimizationDelay())
        return true;
    
    updateAllArrayPredictions();
    
    unsigned numberOfLiveNonArgumentValueProfiles;
    unsigned numberOfSamplesInProfiles;
    updateAllPredictionsAndCountLiveness(numberOfLiveNonArgumentValueProfiles, numberOfSamplesInProfiles);

    if (Options::verboseOSR()) {
        dataLogF(
            "Profile hotness: %lf (%u / %u), %lf (%u / %u)\n",
            (double)numberOfLiveNonArgumentValueProfiles / numberOfNonArgumentValueProfiles(),
            numberOfLiveNonArgumentValueProfiles, numberOfNonArgumentValueProfiles(),
            (double)numberOfSamplesInProfiles / ValueProfile::numberOfBuckets / numberOfNonArgumentValueProfiles(),
            numberOfSamplesInProfiles, ValueProfile::numberOfBuckets * numberOfNonArgumentValueProfiles());
    }

    if ((!numberOfNonArgumentValueProfiles() || (double)numberOfLiveNonArgumentValueProfiles / numberOfNonArgumentValueProfiles() >= Options::desiredProfileLivenessRate())
        && (!totalNumberOfValueProfiles() || (double)numberOfSamplesInProfiles / ValueProfile::numberOfBuckets / totalNumberOfValueProfiles() >= Options::desiredProfileFullnessRate())
        && static_cast<unsigned>(m_optimizationDelayCounter) + 1 >= Options::minimumOptimizationDelay())
        return true;
    
    ASSERT(m_optimizationDelayCounter < std::numeric_limits<uint8_t>::max());
    m_optimizationDelayCounter++;
    optimizeAfterWarmUp();
    return false;
}

#if ENABLE(DFG_JIT)
void CodeBlock::tallyFrequentExitSites()
{
    ASSERT(JITCode::isOptimizingJIT(jitType()));
    ASSERT(alternative()->jitType() == JITCode::BaselineJIT);
    
    CodeBlock* profiledBlock = alternative();
    
    switch (jitType()) {
    case JITCode::DFGJIT: {
        DFG::JITCode* jitCode = m_jitCode->dfg();
        for (auto& exit : jitCode->osrExit)
            exit.considerAddingAsFrequentExitSite(profiledBlock);
        break;
    }

#if ENABLE(FTL_JIT)
    case JITCode::FTLJIT: {
        // There is no easy way to avoid duplicating this code since the FTL::JITCode::osrExit
        // vector contains a totally different type, that just so happens to behave like
        // DFG::JITCode::osrExit.
        FTL::JITCode* jitCode = m_jitCode->ftl();
        for (unsigned i = 0; i < jitCode->osrExit.size(); ++i) {
            FTL::OSRExit& exit = jitCode->osrExit[i];
            exit.considerAddingAsFrequentExitSite(profiledBlock);
        }
        break;
    }
#endif
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(VERBOSE_VALUE_PROFILE)
void CodeBlock::dumpValueProfiles()
{
    dataLog("ValueProfile for ", *this, ":\n");
    forEachValueProfile([](ValueProfile& profile) {
        if (profile.m_bytecodeOffset < 0) {
            ASSERT(profile.m_bytecodeOffset == -1);
            dataLogF("   arg = %u: ", i);
        } else
            dataLogF("   bc = %d: ", profile.m_bytecodeOffset);
        if (!profile.numberOfSamples() && profile.m_prediction == SpecNone) {
            dataLogF("<empty>\n");
            continue;
        }
        profile.dump(WTF::dataFile());
        dataLogF("\n");
    });
    dataLog("RareCaseProfile for ", *this, ":\n");
    for (unsigned i = 0; i < numberOfRareCaseProfiles(); ++i) {
        RareCaseProfile* profile = rareCaseProfile(i);
        dataLogF("   bc = %d: %u\n", profile->m_bytecodeOffset, profile->m_counter);
    }
}
#endif // ENABLE(VERBOSE_VALUE_PROFILE)

unsigned CodeBlock::frameRegisterCount()
{
    switch (jitType()) {
    case JITCode::InterpreterThunk:
        return LLInt::frameRegisterCountFor(this);

#if ENABLE(JIT)
    case JITCode::BaselineJIT:
        return JIT::frameRegisterCountFor(this);
#endif // ENABLE(JIT)

#if ENABLE(DFG_JIT)
    case JITCode::DFGJIT:
    case JITCode::FTLJIT:
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
    VM* vm = m_poisonedVM.unpoisoned();
    // This will be called from CodeBlock::CodeBlock before either m_poisonedVM or the
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
    
    double doubleResult = multiplier * instructionCount();
    
    // Be even more paranoid: silently reject values that won't fit into a size_t. If
    // the function is so huge that we can't even fit it into virtual memory then we
    // should probably have some other guards in place to prevent us from even getting
    // to this point.
    if (doubleResult > std::numeric_limits<size_t>::max())
        return 0;
    
    return static_cast<size_t>(doubleResult);
}

String CodeBlock::nameForRegister(VirtualRegister virtualRegister)
{
    for (auto& constantRegister : m_constantRegisters) {
        if (constantRegister.get().isEmpty())
            continue;
        if (SymbolTable* symbolTable = jsDynamicCast<SymbolTable*>(*vm(), constantRegister.get())) {
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
    if (virtualRegister == thisRegister())
        return "this"_s;
    if (virtualRegister.isArgument())
        return String::format("arguments[%3d]", virtualRegister.toArgument());

    return "";
}

ValueProfile* CodeBlock::tryGetValueProfileForBytecodeOffset(int bytecodeOffset)
{
    auto instruction = m_instructions->at(bytecodeOffset);
    switch (instruction->opcodeID()) {

#define CASE(Op) \
    case Op::opcodeID: \
        return &instruction->as<Op>().metadata(this).profile;

        FOR_EACH_OPCODE_WITH_VALUE_PROFILE(CASE)

#undef CASE

    default:
        return nullptr;

    }
}

SpeculatedType CodeBlock::valueProfilePredictionForBytecodeOffset(const ConcurrentJSLocker& locker, int bytecodeOffset)
{
    if (ValueProfile* valueProfile = tryGetValueProfileForBytecodeOffset(bytecodeOffset))
        return valueProfile->computeUpdatedPrediction(locker);
    return SpecNone;
}

ValueProfile& CodeBlock::valueProfileForBytecodeOffset(int bytecodeOffset)
{
    return *tryGetValueProfileForBytecodeOffset(bytecodeOffset);
}

void CodeBlock::validate()
{
    BytecodeLivenessAnalysis liveness(this); // Compute directly from scratch so it doesn't effect CodeBlock footprint.
    
    FastBitVector liveAtHead = liveness.getLivenessInfoAtBytecodeOffset(this, 0);
    
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
     
    for (const auto& instruction : *m_instructions) {
        OpcodeID opcode = instruction->opcodeID();
        if (!!baselineAlternative()->handlerForBytecodeOffset(instruction.offset())) {
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
    if (JITCode::isOptimizingJIT(jitType()))
        jettison(Profiler::JettisonDueToDebuggerBreakpoint);
}

void CodeBlock::setSteppingMode(CodeBlock::SteppingMode mode)
{
    m_steppingMode = mode;
    if (mode == SteppingModeEnabled && JITCode::isOptimizingJIT(jitType()))
        jettison(Profiler::JettisonDueToDebuggerStepping);
}

int CodeBlock::outOfLineJumpOffset(const Instruction* pc)
{
    int offset = bytecodeOffset(pc);
    return m_unlinkedCode->outOfLineJumpOffset(offset);
}

const Instruction* CodeBlock::outOfLineJumpTarget(const Instruction* pc)
{
    int offset = bytecodeOffset(pc);
    int target = m_unlinkedCode->outOfLineJumpOffset(offset);
    return m_instructions->at(offset + target).ptr();
}

RareCaseProfile* CodeBlock::addRareCaseProfile(int bytecodeOffset)
{
    m_rareCaseProfiles.append(RareCaseProfile(bytecodeOffset));
    return &m_rareCaseProfiles.last();
}

RareCaseProfile* CodeBlock::rareCaseProfileForBytecodeOffset(int bytecodeOffset)
{
    return tryBinarySearch<RareCaseProfile, int>(
        m_rareCaseProfiles, m_rareCaseProfiles.size(), bytecodeOffset,
        getRareCaseProfileBytecodeOffset);
}

unsigned CodeBlock::rareCaseProfileCountForBytecodeOffset(int bytecodeOffset)
{
    RareCaseProfile* profile = rareCaseProfileForBytecodeOffset(bytecodeOffset);
    if (profile)
        return profile->m_counter;
    return 0;
}

ArithProfile* CodeBlock::arithProfileForBytecodeOffset(InstructionStream::Offset bytecodeOffset)
{
    return arithProfileForPC(m_instructions->at(bytecodeOffset).ptr());
}

ArithProfile* CodeBlock::arithProfileForPC(const Instruction* pc)
{
    switch (pc->opcodeID()) {
    case op_negate:
        return &pc->as<OpNegate>().metadata(this).arithProfile;
    case op_bitxor:
        return &pc->as<OpBitxor>().metadata(this).arithProfile;
    case op_add:
        return &pc->as<OpAdd>().metadata(this).arithProfile;
    case op_mul:
        return &pc->as<OpMul>().metadata(this).arithProfile;
    case op_sub:
        return &pc->as<OpSub>().metadata(this).arithProfile;
    case op_div:
        return &pc->as<OpDiv>().metadata(this).arithProfile;
    default:
        break;
    }

    return nullptr;
}

bool CodeBlock::couldTakeSpecialFastCase(InstructionStream::Offset bytecodeOffset)
{
    if (!hasBaselineJITProfiling())
        return false;
    ArithProfile* profile = arithProfileForBytecodeOffset(bytecodeOffset);
    if (!profile)
        return false;
    return profile->tookSpecialFastPath();
}

#if ENABLE(JIT)
DFG::CapabilityLevel CodeBlock::capabilityLevel()
{
    DFG::CapabilityLevel result = computeCapabilityLevel();
    m_capabilityLevelState = result;
    return result;
}
#endif

void CodeBlock::insertBasicBlockBoundariesForControlFlowProfiler()
{
    if (!unlinkedCodeBlock()->hasOpProfileControlFlowBytecodeOffsets())
        return;
    const Vector<InstructionStream::Offset>& bytecodeOffsets = unlinkedCodeBlock()->opProfileControlFlowBytecodeOffsets();
    for (size_t i = 0, offsetsLength = bytecodeOffsets.size(); i < offsetsLength; i++) {
        // Because op_profile_control_flow is emitted at the beginning of every basic block, finding 
        // the next op_profile_control_flow will give us the text range of a single basic block.
        size_t startIdx = bytecodeOffsets[i];
        auto instruction = m_instructions->at(startIdx);
        RELEASE_ASSERT(instruction->opcodeID() == op_profile_control_flow);
        auto bytecode = instruction->as<OpProfileControlFlow>();
        auto& metadata = bytecode.metadata(this);
        int basicBlockStartOffset = bytecode.textOffset;
        int basicBlockEndOffset;
        if (i + 1 < offsetsLength) {
            size_t endIdx = bytecodeOffsets[i + 1];
            auto endInstruction = m_instructions->at(endIdx);
            RELEASE_ASSERT(endInstruction->opcodeID() == op_profile_control_flow);
            basicBlockEndOffset = endInstruction->as<OpProfileControlFlow>().textOffset - 1;
        } else {
            basicBlockEndOffset = m_sourceOffset + ownerScriptExecutable()->source().length() - 1; // Offset before the closing brace.
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
            metadata.basicBlockLocation = vm()->controlFlowProfiler()->dummyBasicBlock();
            continue;
        }

        BasicBlockLocation* basicBlockLocation = vm()->controlFlowProfiler()->getBasicBlockLocation(ownerScriptExecutable()->sourceID(), basicBlockStartOffset, basicBlockEndOffset);

        // Find all functions that are enclosed within the range: [basicBlockStartOffset, basicBlockEndOffset]
        // and insert these functions' start/end offsets as gaps in the current BasicBlockLocation.
        // This is necessary because in the original source text of a JavaScript program, 
        // function literals form new basic blocks boundaries, but they aren't represented 
        // inside the CodeBlock's instruction stream.
        auto insertFunctionGaps = [basicBlockLocation, basicBlockStartOffset, basicBlockEndOffset] (const WriteBarrier<FunctionExecutable>& functionExecutable) {
            const UnlinkedFunctionExecutable* executable = functionExecutable->unlinkedExecutable();
            int functionStart = executable->typeProfilingStartOffset();
            int functionEnd = executable->typeProfilingEndOffset();
            if (functionStart >= basicBlockStartOffset && functionEnd <= basicBlockEndOffset)
                basicBlockLocation->insertGap(functionStart, functionEnd);
        };

        for (const WriteBarrier<FunctionExecutable>& executable : m_functionDecls)
            insertFunctionGaps(executable);
        for (const WriteBarrier<FunctionExecutable>& executable : m_functionExprs)
            insertFunctionGaps(executable);

        metadata.basicBlockLocation = basicBlockLocation;
    }
}

#if ENABLE(JIT)
void CodeBlock::setPCToCodeOriginMap(std::unique_ptr<PCToCodeOriginMap>&& map) 
{ 
    m_pcToCodeOriginMap = WTFMove(map);
}

std::optional<CodeOrigin> CodeBlock::findPC(void* pc)
{
    if (m_pcToCodeOriginMap) {
        if (std::optional<CodeOrigin> codeOrigin = m_pcToCodeOriginMap->findPC(pc))
            return codeOrigin;
    }

    for (auto iter = m_stubInfos.begin(); !!iter; ++iter) {
        StructureStubInfo* stub = *iter;
        if (stub->containsPC(pc))
            return std::optional<CodeOrigin>(stub->codeOrigin);
    }

    if (std::optional<CodeOrigin> codeOrigin = m_jitCode->findPC(this, pc))
        return codeOrigin;

    return std::nullopt;
}
#endif // ENABLE(JIT)

std::optional<unsigned> CodeBlock::bytecodeOffsetFromCallSiteIndex(CallSiteIndex callSiteIndex)
{
    std::optional<unsigned> bytecodeOffset;
    JITCode::JITType jitType = this->jitType();
    if (jitType == JITCode::InterpreterThunk || jitType == JITCode::BaselineJIT) {
#if USE(JSVALUE64)
        bytecodeOffset = callSiteIndex.bits();
#else
        Instruction* instruction = bitwise_cast<Instruction*>(callSiteIndex.bits());
        bytecodeOffset = this->bytecodeOffset(instruction);
#endif
    } else if (jitType == JITCode::DFGJIT || jitType == JITCode::FTLJIT) {
#if ENABLE(DFG_JIT)
        RELEASE_ASSERT(canGetCodeOrigin(callSiteIndex));
        CodeOrigin origin = codeOrigin(callSiteIndex);
        bytecodeOffset = origin.bytecodeIndex;
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    return bytecodeOffset;
}

int32_t CodeBlock::thresholdForJIT(int32_t threshold)
{
    switch (unlinkedCodeBlock()->didOptimize()) {
    case MixedTriState:
        return threshold;
    case FalseTriState:
        return threshold * 4;
    case TrueTriState:
        return threshold / 2;
    }
    ASSERT_NOT_REACHED();
    return threshold;
}

void CodeBlock::jitAfterWarmUp()
{
    m_llintExecuteCounter.setNewThreshold(thresholdForJIT(Options::thresholdForJITAfterWarmUp()), this);
}

void CodeBlock::jitSoon()
{
    m_llintExecuteCounter.setNewThreshold(thresholdForJIT(Options::thresholdForJITSoon()), this);
}

bool CodeBlock::hasInstalledVMTrapBreakpoints() const
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    // This function may be called from a signal handler. We need to be
    // careful to not call anything that is not signal handler safe, e.g.
    // we should not perturb the refCount of m_jitCode.
    if (!JITCode::isOptimizingJIT(jitType()))
        return false;
    return m_jitCode->dfgCommon()->hasInstalledVMTrapsBreakpoints();
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
    if (!JITCode::isOptimizingJIT(jitType()))
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

    auto countICs = [&] (CodeBlock* codeBlock) {
        for (JITAddIC* addIC : codeBlock->m_addICs) {
            numAdds++;
            totalAddSize += addIC->codeSize();
        }

        for (JITMulIC* mulIC : codeBlock->m_mulICs) {
            numMuls++;
            totalMulSize += mulIC->codeSize();
        }

        for (JITNegIC* negIC : codeBlock->m_negICs) {
            numNegs++;
            totalNegSize += negIC->codeSize();
        }

        for (JITSubIC* subIC : codeBlock->m_subICs) {
            numSubs++;
            totalSubSize += subIC->codeSize();
        }
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
