/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#include "BatchedTransitionOptimizer.h"
#include "CodeBlock.h"
#include "Debugger.h"
#include "EvalCodeBlock.h"
#include "FunctionCodeBlock.h"
#include "GlobalExecutable.h"
#include "IsoCellSetInlines.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "JSTemplateObjectDescriptor.h"
#include "LLIntEntrypoint.h"
#include "ModuleProgramCodeBlock.h"
#include "Parser.h"
#include "ProgramCodeBlock.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo ScriptExecutable::s_info = { "ScriptExecutable", &ExecutableBase::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ScriptExecutable) };

ScriptExecutable::ScriptExecutable(Structure* structure, VM& vm, const SourceCode& source, bool isInStrictContext, DerivedContextType derivedContextType, bool isInArrowFunctionContext, EvalContextType evalContextType, Intrinsic intrinsic)
    : ExecutableBase(vm, structure)
    , m_source(source)
    , m_intrinsic(intrinsic)
    , m_features(isInStrictContext ? StrictModeFeature : 0)
    , m_hasCapturedVariables(false)
    , m_neverInline(false)
    , m_neverOptimize(false)
    , m_neverFTLOptimize(false)
    , m_isArrowFunctionContext(isInArrowFunctionContext)
    , m_canUseOSRExitFuzzing(true)
    , m_codeForGeneratorBodyWasGenerated(false)
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
    m_jitCodeForCallWithArityCheck = MacroAssemblerCodePtr<JSEntryPtrTag>();
    m_jitCodeForConstructWithArityCheck = MacroAssemblerCodePtr<JSEntryPtrTag>();

    switch (type()) {
    case FunctionExecutableType: {
        FunctionExecutable* executable = static_cast<FunctionExecutable*>(this);
        executable->m_codeBlockForCall.clear();
        executable->m_codeBlockForConstruct.clear();
        break;
    }
    case EvalExecutableType: {
        EvalExecutable* executable = static_cast<EvalExecutable*>(this);
        executable->m_evalCodeBlock.clear();
        executable->m_unlinkedEvalCodeBlock.clear();
        break;
    }
    case ProgramExecutableType: {
        ProgramExecutable* executable = static_cast<ProgramExecutable*>(this);
        executable->m_programCodeBlock.clear();
        executable->m_unlinkedProgramCodeBlock.clear();
        break;
    }
    case ModuleProgramExecutableType: {
        ModuleProgramExecutable* executable = static_cast<ModuleProgramExecutable*>(this);
        executable->m_moduleProgramCodeBlock.clear();
        executable->m_unlinkedModuleProgramCodeBlock.clear();
        executable->m_moduleEnvironmentSymbolTable.clear();
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    ASSERT(&VM::SpaceAndSet::setFor(*subspace()) == &clearableCodeSet);
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
        
        oldCodeBlock = ExecutableToCodeBlockEdge::deactivateAndUnwrap(executable->m_programCodeBlock.get());
        executable->m_programCodeBlock.setMayBeNull(vm, this, ExecutableToCodeBlockEdge::wrapAndActivate(codeBlock));
        break;
    }

    case ModuleCode: {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        ModuleProgramCodeBlock* codeBlock = static_cast<ModuleProgramCodeBlock*>(genericCodeBlock);

        ASSERT(kind == CodeForCall);

        oldCodeBlock = ExecutableToCodeBlockEdge::deactivateAndUnwrap(executable->m_moduleProgramCodeBlock.get());
        executable->m_moduleProgramCodeBlock.setMayBeNull(vm, this, ExecutableToCodeBlockEdge::wrapAndActivate(codeBlock));
        break;
    }

    case EvalCode: {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* codeBlock = static_cast<EvalCodeBlock*>(genericCodeBlock);
        
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = ExecutableToCodeBlockEdge::deactivateAndUnwrap(executable->m_evalCodeBlock.get());
        executable->m_evalCodeBlock.setMayBeNull(vm, this, ExecutableToCodeBlockEdge::wrapAndActivate(codeBlock));
        break;
    }
        
    case FunctionCode: {
        FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
        FunctionCodeBlock* codeBlock = static_cast<FunctionCodeBlock*>(genericCodeBlock);
        
        switch (kind) {
        case CodeForCall:
            oldCodeBlock = ExecutableToCodeBlockEdge::deactivateAndUnwrap(executable->m_codeBlockForCall.get());
            executable->m_codeBlockForCall.setMayBeNull(vm, this, ExecutableToCodeBlockEdge::wrapAndActivate(codeBlock));
            break;
        case CodeForConstruct:
            oldCodeBlock = ExecutableToCodeBlockEdge::deactivateAndUnwrap(executable->m_codeBlockForConstruct.get());
            executable->m_codeBlockForConstruct.setMayBeNull(vm, this, ExecutableToCodeBlockEdge::wrapAndActivate(codeBlock));
            break;
        }
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

    auto& clearableCodeSet = VM::SpaceAndSet::setFor(*subspace());
    if (hasClearableCode(vm))
        clearableCodeSet.add(this);
    else
        clearableCodeSet.remove(this);

    if (genericCodeBlock) {
        RELEASE_ASSERT(genericCodeBlock->ownerExecutable() == this);
        RELEASE_ASSERT(JITCode::isExecutableScript(genericCodeBlock->jitType()));
        
        if (UNLIKELY(Options::verboseOSR()))
            dataLog("Installing ", *genericCodeBlock, "\n");
        
        if (UNLIKELY(vm.m_perBytecodeProfiler))
            vm.m_perBytecodeProfiler->ensureBytecodesFor(genericCodeBlock);
        
        Debugger* debugger = genericCodeBlock->globalObject()->debugger();
        if (UNLIKELY(debugger))
            debugger->registerCodeBlock(genericCodeBlock);
    }

    if (oldCodeBlock)
        oldCodeBlock->unlinkIncomingCalls();

    vm.heap.writeBarrier(this);
}

bool ScriptExecutable::hasClearableCode(VM& vm) const
{
    if (m_jitCodeForCall
        || m_jitCodeForConstruct
        || m_jitCodeForCallWithArityCheck
        || m_jitCodeForConstructWithArityCheck)
        return true;

    if (structure(vm)->classInfo() == FunctionExecutable::info()) {
        auto* executable = static_cast<const FunctionExecutable*>(this);
        if (executable->m_codeBlockForCall || executable->m_codeBlockForConstruct)
            return true;

    } else if (structure(vm)->classInfo() == EvalExecutable::info()) {
        auto* executable = static_cast<const EvalExecutable*>(this);
        if (executable->m_evalCodeBlock || executable->m_unlinkedEvalCodeBlock)
            return true;

    } else if (structure(vm)->classInfo() == ProgramExecutable::info()) {
        auto* executable = static_cast<const ProgramExecutable*>(this);
        if (executable->m_programCodeBlock || executable->m_unlinkedProgramCodeBlock)
            return true;

    } else if (structure(vm)->classInfo() == ModuleProgramExecutable::info()) {
        auto* executable = static_cast<const ModuleProgramExecutable*>(this);
        if (executable->m_moduleProgramCodeBlock
            || executable->m_unlinkedModuleProgramCodeBlock
            || executable->m_moduleEnvironmentSymbolTable)
            return true;
    }
    return false;
}

CodeBlock* ScriptExecutable::newCodeBlockFor(
    CodeSpecializationKind kind, JSFunction* function, JSScope* scope, Exception*& exception)
{
    VM& vm = scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ASSERT(vm.heap.isDeferred());
    ASSERT(endColumn() != UINT_MAX);

    JSGlobalObject* globalObject = scope->globalObject(vm);
    ExecState* exec = globalObject->globalExec();

    if (classInfo(vm) == EvalExecutable::info()) {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_evalCodeBlock);
        RELEASE_ASSERT(!function);
        auto codeBlock = EvalCodeBlock::create(vm,
            executable, executable->m_unlinkedEvalCodeBlock.get(), scope);
        EXCEPTION_ASSERT(throwScope.exception() || codeBlock);
        if (!codeBlock) {
            exception = throwException(
                exec, throwScope,
                createOutOfMemoryError(exec));
            return nullptr;
        }
        return codeBlock;
    }
    
    if (classInfo(vm) == ProgramExecutable::info()) {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_programCodeBlock);
        RELEASE_ASSERT(!function);
        auto codeBlock = ProgramCodeBlock::create(vm,
            executable, executable->m_unlinkedProgramCodeBlock.get(), scope);
        EXCEPTION_ASSERT(throwScope.exception() || codeBlock);
        if (!codeBlock) {
            exception = throwException(
                exec, throwScope,
                createOutOfMemoryError(exec));
            return nullptr;
        }
        return codeBlock;
    }

    if (classInfo(vm) == ModuleProgramExecutable::info()) {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_moduleProgramCodeBlock);
        RELEASE_ASSERT(!function);
        auto codeBlock = ModuleProgramCodeBlock::create(vm,
            executable, executable->m_unlinkedModuleProgramCodeBlock.get(), scope);
        EXCEPTION_ASSERT(throwScope.exception() || codeBlock);
        if (!codeBlock) {
            exception = throwException(
                exec, throwScope,
                createOutOfMemoryError(exec));
            return nullptr;
        }
        return codeBlock;
    }

    RELEASE_ASSERT(classInfo(vm) == FunctionExecutable::info());
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
        executable->m_unlinkedExecutable->hasCapturedVariables(),
        lastLine(), endColumn()); 
    if (!unlinkedCodeBlock) {
        exception = throwException(
            globalObject->globalExec(), throwScope,
            error.toErrorObject(globalObject, executable->source()));
        return nullptr;
    }

    RELEASE_AND_RETURN(throwScope, FunctionCodeBlock::create(vm, executable, unlinkedCodeBlock, scope));
}

CodeBlock* ScriptExecutable::newReplacementCodeBlockFor(
    CodeSpecializationKind kind)
{
    VM& vm = this->vm();
    if (classInfo(vm) == EvalExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* baseline = static_cast<EvalCodeBlock*>(
            executable->codeBlock()->baselineVersion());
        EvalCodeBlock* result = EvalCodeBlock::create(vm,
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(vm, baseline);
        return result;
    }
    
    if (classInfo(vm) == ProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        ProgramCodeBlock* baseline = static_cast<ProgramCodeBlock*>(
            executable->codeBlock()->baselineVersion());
        ProgramCodeBlock* result = ProgramCodeBlock::create(vm,
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(vm, baseline);
        return result;
    }

    if (classInfo(vm) == ModuleProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        ModuleProgramCodeBlock* baseline = static_cast<ModuleProgramCodeBlock*>(
            executable->codeBlock()->baselineVersion());
        ModuleProgramCodeBlock* result = ModuleProgramCodeBlock::create(vm,
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(vm, baseline);
        return result;
    }

    RELEASE_ASSERT(classInfo(vm) == FunctionExecutable::info());
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

Exception* ScriptExecutable::prepareForExecutionImpl(
    VM& vm, JSFunction* function, JSScope* scope, CodeSpecializationKind kind, CodeBlock*& resultCodeBlock)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    DeferGCForAWhile deferGC(vm.heap);

    if (UNLIKELY(vm.getAndClearFailNextNewCodeBlock())) {
        auto& state = *scope->globalObject(vm)->globalExec();
        return throwException(&state, throwScope, createError(&state, "Forced Failure"_s));
    }

    Exception* exception = nullptr;
    CodeBlock* codeBlock = newCodeBlockFor(kind, function, scope, exception);
    resultCodeBlock = codeBlock;
    EXCEPTION_ASSERT(!!throwScope.exception() == !codeBlock);
    if (UNLIKELY(!codeBlock))
        return exception;
    
    if (Options::validateBytecode())
        codeBlock->validate();
    
    if (Options::useLLInt())
        setupLLInt(codeBlock);
    else
        setupJIT(vm, codeBlock);
    
    installCode(vm, codeBlock, codeBlock->codeType(), codeBlock->specializationKind());
    return nullptr;
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

JSArray* ScriptExecutable::createTemplateObject(ExecState* exec, JSTemplateObjectDescriptor* descriptor)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemplateObjectMap& templateObjectMap = ensureTemplateObjectMap(vm);
    TemplateObjectMap::AddResult result;
    {
        auto locker = holdLock(cellLock());
        result = templateObjectMap.add(descriptor->endOffset(), WriteBarrier<JSArray>());
    }
    if (JSArray* array = result.iterator->value.get())
        return array;
    JSArray* templateObject = descriptor->createTemplateObject(exec);
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

Optional<int> ScriptExecutable::overrideLineNumber(VM& vm) const
{
    if (inherits<FunctionExecutable>(vm))
        return jsCast<const FunctionExecutable*>(this)->overrideLineNumber();
    return WTF::nullopt;
}

unsigned ScriptExecutable::typeProfilingStartOffset(VM& vm) const
{
    if (inherits<FunctionExecutable>(vm))
        return jsCast<const FunctionExecutable*>(this)->typeProfilingStartOffset(vm);
    if (inherits<EvalExecutable>(vm))
        return UINT_MAX;
    return 0;
}

unsigned ScriptExecutable::typeProfilingEndOffset(VM& vm) const
{
    if (inherits<FunctionExecutable>(vm))
        return jsCast<const FunctionExecutable*>(this)->typeProfilingEndOffset(vm);
    if (inherits<EvalExecutable>(vm))
        return UINT_MAX;
    return source().length() - 1;
}

void ScriptExecutable::recordParse(CodeFeatures features, bool hasCapturedVariables, int lastLine, unsigned endColumn)
{
    switch (type()) {
    case FunctionExecutableType:
        // Since UnlinkedFunctionExecutable holds the information to calculate lastLine and endColumn, we do not need to remember them in ScriptExecutable's fields.
        jsCast<FunctionExecutable*>(this)->recordParse(features, hasCapturedVariables);
        return;
    default:
        jsCast<GlobalExecutable*>(this)->recordParse(features, hasCapturedVariables, lastLine, endColumn);
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

} // namespace JSC
