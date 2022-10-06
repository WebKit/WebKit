/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "Debugger.h"
#include "EvalCodeBlock.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutableInlines.h"
#include "GlobalExecutable.h"
#include "IsoCellSetInlines.h"
#include "JIT.h"
#include "JSCellInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSTemplateObjectDescriptor.h"
#include "LLIntEntrypoint.h"
#include "ModuleProgramCodeBlock.h"
#include "ParserError.h"
#include "ProgramCodeBlock.h"
#include "VMInlines.h"

namespace JSC {

const ClassInfo ScriptExecutable::s_info = { "ScriptExecutable"_s, &ExecutableBase::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ScriptExecutable) };

ScriptExecutable::ScriptExecutable(Structure* structure, VM& vm, const SourceCode& source, LexicalScopeFeatures lexicalScopeFeatures, DerivedContextType derivedContextType, bool isInArrowFunctionContext, bool isInsideOrdinaryFunction, EvalContextType evalContextType, Intrinsic intrinsic)
    : ExecutableBase(vm, structure)
    , m_source(source)
    , m_intrinsic(intrinsic)
    , m_features(NoFeatures)
    , m_lexicalScopeFeatures(lexicalScopeFeatures)
    , m_hasCapturedVariables(false)
    , m_neverInline(false)
    , m_neverOptimize(false)
    , m_neverFTLOptimize(false)
    , m_isArrowFunctionContext(isInArrowFunctionContext)
    , m_canUseOSRExitFuzzing(true)
    , m_codeForGeneratorBodyWasGenerated(false)
    , m_isInsideOrdinaryFunction(isInsideOrdinaryFunction)
    , m_derivedContextType(static_cast<unsigned>(derivedContextType))
    , m_evalContextType(static_cast<unsigned>(evalContextType))
{
}

void ScriptExecutable::destroy(JSCell* cell)
{
    static_cast<ScriptExecutable*>(cell)->ScriptExecutable::~ScriptExecutable();
}

void ScriptExecutable::clearCode(IsoCellSet& clearableCodeSet)
{
    m_jitCodeForCall = nullptr;
    m_jitCodeForConstruct = nullptr;
    m_jitCodeForCallWithArityCheck = CodePtr<JSEntryPtrTag>();
    m_jitCodeForConstructWithArityCheck = CodePtr<JSEntryPtrTag>();

    switch (type()) {
    case FunctionExecutableType: {
        FunctionExecutable* executable = static_cast<FunctionExecutable*>(this);
        executable->m_codeBlockForCall.clear();
        executable->m_codeBlockForConstruct.clear();
        break;
    }
    case EvalExecutableType: {
        EvalExecutable* executable = static_cast<EvalExecutable*>(this);
        executable->m_codeBlock.clear();
        executable->m_unlinkedCodeBlock.clear();
        break;
    }
    case ProgramExecutableType: {
        ProgramExecutable* executable = static_cast<ProgramExecutable*>(this);
        executable->m_codeBlock.clear();
        executable->m_unlinkedCodeBlock.clear();
        break;
    }
    case ModuleProgramExecutableType: {
        ModuleProgramExecutable* executable = static_cast<ModuleProgramExecutable*>(this);
        executable->m_codeBlock.clear();
        executable->m_unlinkedCodeBlock.clear();
        executable->m_moduleEnvironmentSymbolTable.clear();
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    ASSERT(&Heap::ScriptExecutableSpaceAndSets::clearableCodeSetFor(*subspace()) == &clearableCodeSet);
    clearableCodeSet.remove(this);
}

void ScriptExecutable::installCode(CodeBlock* codeBlock)
{
    installCode(codeBlock->vm(), codeBlock, codeBlock->codeType(), codeBlock->specializationKind());
}

void ScriptExecutable::installCode(VM& vm, CodeBlock* genericCodeBlock, CodeType codeType, CodeSpecializationKind kind)
{
    if (genericCodeBlock)
        CODEBLOCK_LOG_EVENT(genericCodeBlock, "installCode", ());
    
    CodeBlock* oldCodeBlock = nullptr;
    
    switch (codeType) {
    case GlobalCode: {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        ProgramCodeBlock* codeBlock = static_cast<ProgramCodeBlock*>(genericCodeBlock);
        
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = executable->replaceCodeBlockWith(vm, codeBlock);
        break;
    }

    case ModuleCode: {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        ModuleProgramCodeBlock* codeBlock = static_cast<ModuleProgramCodeBlock*>(genericCodeBlock);

        ASSERT(kind == CodeForCall);

        oldCodeBlock = executable->replaceCodeBlockWith(vm, codeBlock);
        break;
    }

    case EvalCode: {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* codeBlock = static_cast<EvalCodeBlock*>(genericCodeBlock);
        
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = executable->replaceCodeBlockWith(vm, codeBlock);
        break;
    }
        
    case FunctionCode: {
        FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
        FunctionCodeBlock* codeBlock = static_cast<FunctionCodeBlock*>(genericCodeBlock);
        
        oldCodeBlock = executable->replaceCodeBlockWith(vm, kind, codeBlock);
        break;
    }
    }

    switch (kind) {
    case CodeForCall:
        m_jitCodeForCall = genericCodeBlock ? genericCodeBlock->jitCode() : nullptr;
        m_jitCodeForCallWithArityCheck = nullptr;
        break;
    case CodeForConstruct:
        m_jitCodeForConstruct = genericCodeBlock ? genericCodeBlock->jitCode() : nullptr;
        m_jitCodeForConstructWithArityCheck = nullptr;
        break;
    }

    auto& clearableCodeSet = Heap::ScriptExecutableSpaceAndSets::clearableCodeSetFor(*subspace());
    if (hasClearableCode())
        clearableCodeSet.add(this);
    else
        clearableCodeSet.remove(this);

    if (genericCodeBlock) {
        RELEASE_ASSERT(genericCodeBlock->ownerExecutable() == this);
        RELEASE_ASSERT(JITCode::isExecutableScript(genericCodeBlock->jitType()));

        genericCodeBlock->m_isJettisoned = false;
        
        dataLogLnIf(Options::verboseOSR(), "Installing ", *genericCodeBlock);
        
        if (UNLIKELY(vm.m_perBytecodeProfiler))
            vm.m_perBytecodeProfiler->ensureBytecodesFor(genericCodeBlock);
        
        Debugger* debugger = genericCodeBlock->globalObject()->debugger();
        if (UNLIKELY(debugger))
            debugger->registerCodeBlock(genericCodeBlock);
    }

    if (oldCodeBlock)
        oldCodeBlock->unlinkIncomingCalls();

    vm.writeBarrier(this);
}

bool ScriptExecutable::hasClearableCode() const
{
    if (m_jitCodeForCall
        || m_jitCodeForConstruct
        || m_jitCodeForCallWithArityCheck
        || m_jitCodeForConstructWithArityCheck)
        return true;

    if (structure()->classInfoForCells() == FunctionExecutable::info()) {
        auto* executable = static_cast<const FunctionExecutable*>(this);
        if (executable->eitherCodeBlock())
            return true;

    } else if (structure()->classInfoForCells() == EvalExecutable::info()) {
        auto* executable = static_cast<const EvalExecutable*>(this);
        if (executable->m_codeBlock || executable->m_unlinkedCodeBlock)
            return true;

    } else if (structure()->classInfoForCells() == ProgramExecutable::info()) {
        auto* executable = static_cast<const ProgramExecutable*>(this);
        if (executable->m_codeBlock || executable->m_unlinkedCodeBlock)
            return true;

    } else if (structure()->classInfoForCells() == ModuleProgramExecutable::info()) {
        auto* executable = static_cast<const ModuleProgramExecutable*>(this);
        if (executable->m_codeBlock
            || executable->m_unlinkedCodeBlock
            || executable->m_moduleEnvironmentSymbolTable)
            return true;
    }
    return false;
}

CodeBlock* ScriptExecutable::newCodeBlockFor(CodeSpecializationKind kind, JSFunction* function, JSScope* scope)
{
    VM& vm = scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ASSERT(vm.heap.isDeferred());
    ASSERT(endColumn() != UINT_MAX);

    JSGlobalObject* globalObject = scope->globalObject();

    if (classInfo() == EvalExecutable::info()) {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_codeBlock);
        RELEASE_ASSERT(!function);
        RELEASE_AND_RETURN(throwScope, EvalCodeBlock::create(vm, executable, executable->unlinkedCodeBlock(), scope));
    }

    if (classInfo() == ProgramExecutable::info()) {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_codeBlock);
        RELEASE_ASSERT(!function);
        RELEASE_AND_RETURN(throwScope, ProgramCodeBlock::create(vm, executable, executable->unlinkedCodeBlock(), scope));
    }

    if (classInfo() == ModuleProgramExecutable::info()) {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_codeBlock);
        RELEASE_ASSERT(!function);
        RELEASE_AND_RETURN(throwScope, ModuleProgramCodeBlock::create(vm, executable, executable->unlinkedCodeBlock(), scope));
    }

    RELEASE_ASSERT(classInfo() == FunctionExecutable::info());
    RELEASE_ASSERT(function);
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
    RELEASE_ASSERT(!executable->codeBlockFor(kind));
    ParserError error;
    OptionSet<CodeGenerationMode> codeGenerationMode = globalObject->defaultCodeGenerationMode();
    // We continue using the same CodeGenerationMode for Generators because live generator objects can
    // keep the state which is only valid with the CodeBlock compiled with the same CodeGenerationMode.
    if (isGeneratorOrAsyncFunctionBodyParseMode(executable->parseMode())) {
        if (!m_codeForGeneratorBodyWasGenerated) {
            m_codeGenerationModeForGeneratorBody = codeGenerationMode;
            m_codeForGeneratorBodyWasGenerated = true;
        } else
            codeGenerationMode = m_codeGenerationModeForGeneratorBody;
    }
    UnlinkedFunctionCodeBlock* unlinkedCodeBlock = 
        executable->m_unlinkedExecutable->unlinkedCodeBlockFor(
            vm, executable->source(), kind, codeGenerationMode, error, 
            executable->parseMode());
    recordParse(
        executable->m_unlinkedExecutable->features(), 
        executable->m_unlinkedExecutable->lexicalScopeFeatures(),
        executable->m_unlinkedExecutable->hasCapturedVariables(),
        lastLine(), endColumn()); 
    if (!unlinkedCodeBlock) {
        throwException(globalObject, throwScope, error.toErrorObject(globalObject, executable->source()));
        return nullptr;
    }

    RELEASE_AND_RETURN(throwScope, FunctionCodeBlock::create(vm, executable, unlinkedCodeBlock, scope));
}

CodeBlock* ScriptExecutable::newReplacementCodeBlockFor(
    CodeSpecializationKind kind)
{
    VM& vm = this->vm();
    if (classInfo() == EvalExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* baseline = static_cast<EvalCodeBlock*>(
            executable->codeBlock()->baselineVersion());
        EvalCodeBlock* result = EvalCodeBlock::create(vm,
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(vm, baseline);
        return result;
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        ProgramCodeBlock* baseline = static_cast<ProgramCodeBlock*>(
            executable->codeBlock()->baselineVersion());
        ProgramCodeBlock* result = ProgramCodeBlock::create(vm,
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(vm, baseline);
        return result;
    }

    if (classInfo() == ModuleProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        ModuleProgramCodeBlock* baseline = static_cast<ModuleProgramCodeBlock*>(
            executable->codeBlock()->baselineVersion());
        ModuleProgramCodeBlock* result = ModuleProgramCodeBlock::create(vm,
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(vm, baseline);
        return result;
    }

    RELEASE_ASSERT(classInfo() == FunctionExecutable::info());
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
    FunctionCodeBlock* baseline = static_cast<FunctionCodeBlock*>(
        executable->codeBlockFor(kind)->baselineVersion());
    FunctionCodeBlock* result = FunctionCodeBlock::create(vm,
        CodeBlock::CopyParsedBlock, *baseline);
    result->setAlternative(vm, baseline);
    return result;
}

static void setupLLInt(CodeBlock* codeBlock)
{
    LLInt::setEntrypoint(codeBlock);
}

static void setupJIT(VM& vm, CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    CompilationResult result = JIT::compile(vm, codeBlock, JITCompilationMustSucceed);
    RELEASE_ASSERT(result == CompilationSuccessful);
#else
    UNUSED_PARAM(vm);
    UNUSED_PARAM(codeBlock);
    UNREACHABLE_FOR_PLATFORM();
#endif
}

void ScriptExecutable::prepareForExecutionImpl(VM& vm, JSFunction* function, JSScope* scope, CodeSpecializationKind kind, CodeBlock*& resultCodeBlock)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    DeferGCForAWhile deferGC(vm);

    if (UNLIKELY(vm.getAndClearFailNextNewCodeBlock())) {
        JSGlobalObject* globalObject = scope->globalObject();
        throwException(globalObject, throwScope, createError(globalObject, "Forced Failure"_s));
        return;
    }

    CodeBlock* codeBlock = newCodeBlockFor(kind, function, scope);
    RETURN_IF_EXCEPTION(throwScope, void());

    ASSERT(codeBlock);
    resultCodeBlock = codeBlock;

    if (Options::validateBytecode())
        codeBlock->validate();

    bool installedUnlinkedBaselineCode = false;
#if ENABLE(JIT)
    if (RefPtr<BaselineJITCode> baselineRef = codeBlock->unlinkedCodeBlock()->m_unlinkedBaselineCode) {
        codeBlock->setupWithUnlinkedBaselineCode(baselineRef.releaseNonNull());
        installedUnlinkedBaselineCode = true;
    }
#endif
    if (!installedUnlinkedBaselineCode) {
        if (Options::useLLInt())
            setupLLInt(codeBlock);
        else
            setupJIT(vm, codeBlock);
    }

    installCode(vm, codeBlock, codeBlock->codeType(), codeBlock->specializationKind());
}

ScriptExecutable* ScriptExecutable::topLevelExecutable()
{
    switch (type()) {
    case FunctionExecutableType:
        return jsCast<FunctionExecutable*>(this)->topLevelExecutable();
    default:
        return this;
    }
}

JSArray* ScriptExecutable::createTemplateObject(JSGlobalObject* globalObject, JSTemplateObjectDescriptor* descriptor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemplateObjectMap& templateObjectMap = ensureTemplateObjectMap(vm);
    TemplateObjectMap::AddResult result;
    {
        Locker locker { cellLock() };
        result = templateObjectMap.add(descriptor->endOffset(), WriteBarrier<JSArray>());
    }
    if (JSArray* array = result.iterator->value.get())
        return array;
    JSArray* templateObject = descriptor->createTemplateObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    result.iterator->value.set(vm, this, templateObject);
    return templateObject;
}

auto ScriptExecutable::ensureTemplateObjectMapImpl(std::unique_ptr<TemplateObjectMap>& dest) -> TemplateObjectMap&
{
    if (dest)
        return *dest;
    auto result = makeUnique<TemplateObjectMap>();
    WTF::storeStoreFence();
    dest = WTFMove(result);
    return *dest;
}

auto ScriptExecutable::ensureTemplateObjectMap(VM& vm) -> TemplateObjectMap&
{
    switch (type()) {
    case FunctionExecutableType:
        return static_cast<FunctionExecutable*>(this)->ensureTemplateObjectMap(vm);
    case EvalExecutableType:
        return static_cast<EvalExecutable*>(this)->ensureTemplateObjectMap(vm);
    case ProgramExecutableType:
        return static_cast<ProgramExecutable*>(this)->ensureTemplateObjectMap(vm);
    case ModuleProgramExecutableType:
    default:
        ASSERT(type() == ModuleProgramExecutableType);
        return static_cast<ModuleProgramExecutable*>(this)->ensureTemplateObjectMap(vm);
    }
}

CodeBlockHash ScriptExecutable::hashFor(CodeSpecializationKind kind) const
{
    return CodeBlockHash(source(), kind);
}

std::optional<int> ScriptExecutable::overrideLineNumber(VM&) const
{
    if (inherits<FunctionExecutable>())
        return jsCast<const FunctionExecutable*>(this)->overrideLineNumber();
    return std::nullopt;
}

unsigned ScriptExecutable::typeProfilingStartOffset(VM& vm) const
{
    if (inherits<FunctionExecutable>())
        return jsCast<const FunctionExecutable*>(this)->typeProfilingStartOffset(vm);
    if (inherits<EvalExecutable>())
        return UINT_MAX;
    return 0;
}

unsigned ScriptExecutable::typeProfilingEndOffset(VM& vm) const
{
    if (inherits<FunctionExecutable>())
        return jsCast<const FunctionExecutable*>(this)->typeProfilingEndOffset(vm);
    if (inherits<EvalExecutable>())
        return UINT_MAX;
    return source().length() - 1;
}

void ScriptExecutable::recordParse(CodeFeatures features, LexicalScopeFeatures lexicalScopeFeatures, bool hasCapturedVariables, int lastLine, unsigned endColumn)
{
    switch (type()) {
    case FunctionExecutableType:
        // Since UnlinkedFunctionExecutable holds the information to calculate lastLine and endColumn, we do not need to remember them in ScriptExecutable's fields.
        jsCast<FunctionExecutable*>(this)->recordParse(features, lexicalScopeFeatures, hasCapturedVariables);
        return;
    default:
        jsCast<GlobalExecutable*>(this)->recordParse(features, lexicalScopeFeatures, hasCapturedVariables, lastLine, endColumn);
        return;
    }
}

int ScriptExecutable::lastLine() const
{
    switch (type()) {
    case FunctionExecutableType:
        return jsCast<const FunctionExecutable*>(this)->lastLine();
    default:
        return jsCast<const GlobalExecutable*>(this)->lastLine();
    }
    return 0;
}

unsigned ScriptExecutable::endColumn() const
{
    switch (type()) {
    case FunctionExecutableType:
        return jsCast<const FunctionExecutable*>(this)->endColumn();
    default:
        return jsCast<const GlobalExecutable*>(this)->endColumn();
    }
    return 0;
}

template<typename Visitor>
void ScriptExecutable::runConstraint(const ConcurrentJSLocker& locker, Visitor& visitor, CodeBlock* codeBlock)
{
    ASSERT(codeBlock);
    codeBlock->propagateTransitions(locker, visitor);
    codeBlock->determineLiveness(locker, visitor);
}

template void ScriptExecutable::runConstraint(const ConcurrentJSLocker&, AbstractSlotVisitor&, CodeBlock*);
template void ScriptExecutable::runConstraint(const ConcurrentJSLocker&, SlotVisitor&, CodeBlock*);

template<typename Visitor>
void ScriptExecutable::visitCodeBlockEdge(Visitor& visitor, CodeBlock* codeBlock)
{
    ASSERT(codeBlock);

    ConcurrentJSLocker locker(codeBlock->m_lock);

    if (codeBlock->shouldVisitStrongly(locker, visitor))
        visitor.appendUnbarriered(codeBlock);

    if (JITCode::isOptimizingJIT(codeBlock->jitType())) {
        // If we jettison ourselves we'll install our alternative, so make sure that it
        // survives GC even if we don't.
        visitor.append(codeBlock->m_alternative);
    }

    // NOTE: There are two sides to this constraint, with different requirements for correctness.
    // Because everything is ultimately protected with weak references and jettisoning, it's
    // always "OK" to claim that something is dead prematurely and it's "OK" to keep things alive.
    // But both choices could lead to bad perf - either recomp cycles or leaks.
    //
    // Determining CodeBlock liveness: This part is the most consequential. We want to keep the
    // output constraint active so long as we think that we may yet prove that the CodeBlock is
    // live but we haven't done it yet.
    //
    // Marking Structures if profitable: It's important that we do a pass of this. Logically, this
    // seems like it is a constraint of CodeBlock. But we have always first run this as a result
    // of the edge being marked even before we determine the liveness of the CodeBlock. This
    // allows a CodeBlock to mark itself by first proving that all of the Structures it weakly
    // depends on could be strongly marked. (This part is also called propagateTransitions.)
    //
    // As a weird caveat, we only fixpoint the constraints so long as the CodeBlock is not live.
    // This means that we may overlook structure marking opportunities created by other marking
    // that happens after the CodeBlock is marked. This was an accidental policy decision from a
    // long time ago, but it is probably OK, since it's only worthwhile to keep fixpointing the
    // structure marking if we still have unmarked structures after the first round. We almost
    // never will because we will mark-if-profitable based on the owning global object being
    // already marked. We mark it just in case that hadn't happened yet. And if the CodeBlock is
    // not yet marked because it weakly depends on a structure that we did not yet mark, then we
    // will keep fixpointing until the end.
    visitor.appendUnbarriered(codeBlock->globalObject());
    runConstraint(locker, visitor, codeBlock);
}

template void ScriptExecutable::visitCodeBlockEdge(AbstractSlotVisitor&, CodeBlock*);
template void ScriptExecutable::visitCodeBlockEdge(SlotVisitor&, CodeBlock*);

} // namespace JSC
