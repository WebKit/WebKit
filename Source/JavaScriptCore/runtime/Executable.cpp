/*
 * Copyright (C) 2009, 2010, 2013, 2015-2016 Apple Inc. All rights reserved.
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
#include "Executable.h"

#include "BatchedTransitionOptimizer.h"
#include "CodeBlock.h"
#include "Debugger.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "JSWASMModule.h"
#include "LLIntEntrypoint.h"
#include "Parser.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo ExecutableBase::s_info = { "Executable", 0, 0, CREATE_METHOD_TABLE(ExecutableBase) };

void ExecutableBase::destroy(JSCell* cell)
{
    static_cast<ExecutableBase*>(cell)->ExecutableBase::~ExecutableBase();
}

void ExecutableBase::clearCode()
{
#if ENABLE(JIT)
    m_jitCodeForCall = nullptr;
    m_jitCodeForConstruct = nullptr;
    m_jitCodeForCallWithArityCheck = MacroAssemblerCodePtr();
    m_jitCodeForConstructWithArityCheck = MacroAssemblerCodePtr();
#endif
    m_numParametersForCall = NUM_PARAMETERS_NOT_COMPILED;
    m_numParametersForConstruct = NUM_PARAMETERS_NOT_COMPILED;

    if (classInfo() == FunctionExecutable::info()) {
        FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
        executable->m_codeBlockForCall.clear();
        executable->m_codeBlockForConstruct.clear();
        return;
    }

    if (classInfo() == EvalExecutable::info()) {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        executable->m_evalCodeBlock.clear();
        executable->m_unlinkedEvalCodeBlock.clear();
        return;
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        executable->m_programCodeBlock.clear();
        executable->m_unlinkedProgramCodeBlock.clear();
        return;
    }

    if (classInfo() == ModuleProgramExecutable::info()) {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        executable->m_moduleProgramCodeBlock.clear();
        executable->m_unlinkedModuleProgramCodeBlock.clear();
        executable->m_moduleEnvironmentSymbolTable.clear();
        return;
    }
    
#if ENABLE(WEBASSEMBLY)
    if (classInfo() == WebAssemblyExecutable::info()) {
        WebAssemblyExecutable* executable = jsCast<WebAssemblyExecutable*>(this);
        executable->m_codeBlockForCall.clear();
        return;
    }
#endif

    ASSERT(classInfo() == NativeExecutable::info());
}

const ClassInfo NativeExecutable::s_info = { "NativeExecutable", &ExecutableBase::s_info, 0, CREATE_METHOD_TABLE(NativeExecutable) };

NativeExecutable* NativeExecutable::create(VM& vm, PassRefPtr<JITCode> callThunk, NativeFunction function, PassRefPtr<JITCode> constructThunk, NativeFunction constructor, Intrinsic intrinsic, const String& name)
{
    NativeExecutable* executable;
    executable = new (NotNull, allocateCell<NativeExecutable>(vm.heap)) NativeExecutable(vm, function, constructor, intrinsic);
    executable->finishCreation(vm, callThunk, constructThunk, name);
    return executable;
}

void NativeExecutable::destroy(JSCell* cell)
{
    static_cast<NativeExecutable*>(cell)->NativeExecutable::~NativeExecutable();
}

Structure* NativeExecutable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(vm, globalObject, proto, TypeInfo(CellType, StructureFlags), info());
}

void NativeExecutable::finishCreation(VM& vm, PassRefPtr<JITCode> callThunk, PassRefPtr<JITCode> constructThunk, const String& name)
{
    Base::finishCreation(vm);
    m_jitCodeForCall = callThunk;
    m_jitCodeForConstruct = constructThunk;
    m_jitCodeForCallWithArityCheck = m_jitCodeForCall->addressForCall(MustCheckArity);
    m_jitCodeForConstructWithArityCheck = m_jitCodeForConstruct->addressForCall(MustCheckArity);
    m_name = name;
}

NativeExecutable::NativeExecutable(VM& vm, NativeFunction function, NativeFunction constructor, Intrinsic intrinsic)
    : ExecutableBase(vm, vm.nativeExecutableStructure.get(), NUM_PARAMETERS_IS_HOST, intrinsic)
    , m_function(function)
    , m_constructor(constructor)
{
}

const ClassInfo ScriptExecutable::s_info = { "ScriptExecutable", &ExecutableBase::s_info, 0, CREATE_METHOD_TABLE(ScriptExecutable) };

ScriptExecutable::ScriptExecutable(Structure* structure, VM& vm, const SourceCode& source, bool isInStrictContext, DerivedContextType derivedContextType, bool isInArrowFunctionContext, EvalContextType evalContextType, Intrinsic intrinsic)
    : ExecutableBase(vm, structure, NUM_PARAMETERS_NOT_COMPILED, intrinsic)
    , m_features(isInStrictContext ? StrictModeFeature : 0)
    , m_didTryToEnterInLoop(false)
    , m_hasCapturedVariables(false)
    , m_neverInline(false)
    , m_neverOptimize(false)
    , m_neverFTLOptimize(false)
    , m_isArrowFunctionContext(isInArrowFunctionContext)
    , m_canUseOSRExitFuzzing(true)
    , m_derivedContextType(static_cast<unsigned>(derivedContextType))
    , m_evalContextType(static_cast<unsigned>(evalContextType))
    , m_overrideLineNumber(-1)
    , m_firstLine(-1)
    , m_lastLine(-1)
    , m_startColumn(UINT_MAX)
    , m_endColumn(UINT_MAX)
    , m_typeProfilingStartOffset(UINT_MAX)
    , m_typeProfilingEndOffset(UINT_MAX)
    , m_source(source)
{
}

void ScriptExecutable::destroy(JSCell* cell)
{
    static_cast<ScriptExecutable*>(cell)->ScriptExecutable::~ScriptExecutable();
}

void ScriptExecutable::installCode(CodeBlock* codeBlock)
{
    installCode(*codeBlock->vm(), codeBlock, codeBlock->codeType(), codeBlock->specializationKind());
}

void ScriptExecutable::installCode(VM& vm, CodeBlock* genericCodeBlock, CodeType codeType, CodeSpecializationKind kind)
{
    ASSERT(vm.heap.isDeferred());
    
    if (genericCodeBlock)
        CODEBLOCK_LOG_EVENT(genericCodeBlock, "installCode", ());
    
    CodeBlock* oldCodeBlock = nullptr;
    
    switch (codeType) {
    case GlobalCode: {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        ProgramCodeBlock* codeBlock = static_cast<ProgramCodeBlock*>(genericCodeBlock);
        
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = executable->m_programCodeBlock.get();
        executable->m_programCodeBlock.setMayBeNull(vm, this, codeBlock);
        break;
    }

    case ModuleCode: {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        ModuleProgramCodeBlock* codeBlock = static_cast<ModuleProgramCodeBlock*>(genericCodeBlock);

        ASSERT(kind == CodeForCall);

        oldCodeBlock = executable->m_moduleProgramCodeBlock.get();
        executable->m_moduleProgramCodeBlock.setMayBeNull(vm, this, codeBlock);
        break;
    }

    case EvalCode: {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* codeBlock = static_cast<EvalCodeBlock*>(genericCodeBlock);
        
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = executable->m_evalCodeBlock.get();
        executable->m_evalCodeBlock.setMayBeNull(vm, this, codeBlock);
        break;
    }
        
    case FunctionCode: {
        FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
        FunctionCodeBlock* codeBlock = static_cast<FunctionCodeBlock*>(genericCodeBlock);
        
        switch (kind) {
        case CodeForCall:
            oldCodeBlock = executable->m_codeBlockForCall.get();
            executable->m_codeBlockForCall.setMayBeNull(vm, this, codeBlock);
            break;
        case CodeForConstruct:
            oldCodeBlock = executable->m_codeBlockForConstruct.get();
            executable->m_codeBlockForConstruct.setMayBeNull(vm, this, codeBlock);
            break;
        }
        break;
    }
    }

    switch (kind) {
    case CodeForCall:
        m_jitCodeForCall = genericCodeBlock ? genericCodeBlock->jitCode() : nullptr;
        m_jitCodeForCallWithArityCheck = MacroAssemblerCodePtr();
        m_numParametersForCall = genericCodeBlock ? genericCodeBlock->numParameters() : NUM_PARAMETERS_NOT_COMPILED;
        break;
    case CodeForConstruct:
        m_jitCodeForConstruct = genericCodeBlock ? genericCodeBlock->jitCode() : nullptr;
        m_jitCodeForConstructWithArityCheck = MacroAssemblerCodePtr();
        m_numParametersForConstruct = genericCodeBlock ? genericCodeBlock->numParameters() : NUM_PARAMETERS_NOT_COMPILED;
        break;
    }

    if (genericCodeBlock) {
        RELEASE_ASSERT(genericCodeBlock->ownerExecutable() == this);
        RELEASE_ASSERT(JITCode::isExecutableScript(genericCodeBlock->jitType()));
        
        if (Options::verboseOSR())
            dataLog("Installing ", *genericCodeBlock, "\n");
        
        if (vm.m_perBytecodeProfiler)
            vm.m_perBytecodeProfiler->ensureBytecodesFor(genericCodeBlock);
        
        if (Debugger* debugger = genericCodeBlock->globalObject()->debugger())
            debugger->registerCodeBlock(genericCodeBlock);
    }

    if (oldCodeBlock)
        oldCodeBlock->unlinkIncomingCalls();

    vm.heap.writeBarrier(this);
}

CodeBlock* ScriptExecutable::newCodeBlockFor(
    CodeSpecializationKind kind, JSFunction* function, JSScope* scope, JSObject*& exception)
{
    VM* vm = scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(*vm);

    ASSERT(vm->heap.isDeferred());
    ASSERT(startColumn() != UINT_MAX);
    ASSERT(endColumn() != UINT_MAX);

    if (classInfo() == EvalExecutable::info()) {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_evalCodeBlock);
        RELEASE_ASSERT(!function);
        return EvalCodeBlock::create(vm,
            executable, executable->m_unlinkedEvalCodeBlock.get(), scope,
            executable->source().provider());
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_programCodeBlock);
        RELEASE_ASSERT(!function);
        return ProgramCodeBlock::create(vm,
            executable, executable->m_unlinkedProgramCodeBlock.get(), scope,
            executable->source().provider(), executable->source().startColumn());
    }

    if (classInfo() == ModuleProgramExecutable::info()) {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_moduleProgramCodeBlock);
        RELEASE_ASSERT(!function);
        return ModuleProgramCodeBlock::create(vm,
            executable, executable->m_unlinkedModuleProgramCodeBlock.get(), scope,
            executable->source().provider(), executable->source().startColumn());
    }

    RELEASE_ASSERT(classInfo() == FunctionExecutable::info());
    RELEASE_ASSERT(function);
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
    RELEASE_ASSERT(!executable->codeBlockFor(kind));
    JSGlobalObject* globalObject = scope->globalObject();
    ParserError error;
    DebuggerMode debuggerMode = globalObject->hasInteractiveDebugger() ? DebuggerOn : DebuggerOff;
    UnlinkedFunctionCodeBlock* unlinkedCodeBlock = 
        executable->m_unlinkedExecutable->unlinkedCodeBlockFor(
            *vm, executable->m_source, kind, debuggerMode, error, 
            executable->parseMode());
    recordParse(
        executable->m_unlinkedExecutable->features(), 
        executable->m_unlinkedExecutable->hasCapturedVariables(), firstLine(), 
        lastLine(), startColumn(), endColumn()); 
    if (!unlinkedCodeBlock) {
        exception = throwException(
            globalObject->globalExec(), throwScope,
            error.toErrorObject(globalObject, executable->m_source));
        return nullptr;
    }

    SourceProvider* provider = executable->source().provider();
    unsigned sourceOffset = executable->source().startOffset();
    unsigned startColumn = executable->source().startColumn();

    return FunctionCodeBlock::create(vm,
        executable, unlinkedCodeBlock, scope, provider, sourceOffset, startColumn);
}

CodeBlock* ScriptExecutable::newReplacementCodeBlockFor(
    CodeSpecializationKind kind)
{
    if (classInfo() == EvalExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* baseline = static_cast<EvalCodeBlock*>(
            executable->m_evalCodeBlock->baselineVersion());
        EvalCodeBlock* result = EvalCodeBlock::create(vm(),
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(*vm(), baseline);
        return result;
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        ProgramCodeBlock* baseline = static_cast<ProgramCodeBlock*>(
            executable->m_programCodeBlock->baselineVersion());
        ProgramCodeBlock* result = ProgramCodeBlock::create(vm(),
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(*vm(), baseline);
        return result;
    }

    if (classInfo() == ModuleProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(this);
        ModuleProgramCodeBlock* baseline = static_cast<ModuleProgramCodeBlock*>(
            executable->m_moduleProgramCodeBlock->baselineVersion());
        ModuleProgramCodeBlock* result = ModuleProgramCodeBlock::create(vm(),
            CodeBlock::CopyParsedBlock, *baseline);
        result->setAlternative(*vm(), baseline);
        return result;
    }

    RELEASE_ASSERT(classInfo() == FunctionExecutable::info());
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
    FunctionCodeBlock* baseline = static_cast<FunctionCodeBlock*>(
        executable->codeBlockFor(kind)->baselineVersion());
    FunctionCodeBlock* result = FunctionCodeBlock::create(vm(),
        CodeBlock::CopyParsedBlock, *baseline);
    result->setAlternative(*vm(), baseline);
    return result;
}

static void setupLLInt(VM& vm, CodeBlock* codeBlock)
{
    LLInt::setEntrypoint(vm, codeBlock);
}

static void setupJIT(VM& vm, CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    CompilationResult result = JIT::compile(&vm, codeBlock, JITCompilationMustSucceed);
    RELEASE_ASSERT(result == CompilationSuccessful);
#else
    UNUSED_PARAM(vm);
    UNUSED_PARAM(codeBlock);
    UNREACHABLE_FOR_PLATFORM();
#endif
}

JSObject* ScriptExecutable::prepareForExecutionImpl(
    ExecState* exec, JSFunction* function, JSScope* scope, CodeSpecializationKind kind, CodeBlock*& resultCodeBlock)
{
    VM& vm = exec->vm();
    DeferGCForAWhile deferGC(vm.heap);

    if (vm.getAndClearFailNextNewCodeBlock())
        return createError(exec->callerFrame(), ASCIILiteral("Forced Failure"));

    JSObject* exception = 0;
    CodeBlock* codeBlock = newCodeBlockFor(kind, function, scope, exception);
    resultCodeBlock = codeBlock;
    if (!codeBlock) {
        RELEASE_ASSERT(exception);
        return exception;
    }
    
    if (Options::validateBytecode())
        codeBlock->validate();
    
    if (Options::useLLInt())
        setupLLInt(vm, codeBlock);
    else
        setupJIT(vm, codeBlock);
    
    installCode(*codeBlock->vm(), codeBlock, codeBlock->codeType(), codeBlock->specializationKind());
    return nullptr;
}

const ClassInfo EvalExecutable::s_info = { "EvalExecutable", &ScriptExecutable::s_info, 0, CREATE_METHOD_TABLE(EvalExecutable) };

EvalExecutable* EvalExecutable::create(ExecState* exec, const SourceCode& source, bool isInStrictContext, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (!globalObject->evalEnabled()) {
        throwException(exec, scope, createEvalError(exec, globalObject->evalDisabledErrorMessage()));
        return 0;
    }

    EvalExecutable* executable = new (NotNull, allocateCell<EvalExecutable>(*exec->heap())) EvalExecutable(exec, source, isInStrictContext, derivedContextType, isArrowFunctionContext, evalContextType);
    executable->finishCreation(vm);

    UnlinkedEvalCodeBlock* unlinkedEvalCode = globalObject->createEvalCodeBlock(exec, executable, variablesUnderTDZ);
    if (!unlinkedEvalCode)
        return 0;

    executable->m_unlinkedEvalCodeBlock.set(vm, executable, unlinkedEvalCode);

    return executable;
}

EvalExecutable::EvalExecutable(ExecState* exec, const SourceCode& source, bool inStrictContext, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType)
    : ScriptExecutable(exec->vm().evalExecutableStructure.get(), exec->vm(), source, inStrictContext, derivedContextType, isArrowFunctionContext, evalContextType, NoIntrinsic)
{
}

void EvalExecutable::destroy(JSCell* cell)
{
    static_cast<EvalExecutable*>(cell)->EvalExecutable::~EvalExecutable();
}

const ClassInfo ProgramExecutable::s_info = { "ProgramExecutable", &ScriptExecutable::s_info, 0, CREATE_METHOD_TABLE(ProgramExecutable) };

ProgramExecutable::ProgramExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec->vm().programExecutableStructure.get(), exec->vm(), source, false, DerivedContextType::None, false, EvalContextType::None, NoIntrinsic)
{
    m_typeProfilingStartOffset = 0;
    m_typeProfilingEndOffset = source.length() - 1;
    if (exec->vm().typeProfiler() || exec->vm().controlFlowProfiler())
        exec->vm().functionHasExecutedCache()->insertUnexecutedRange(sourceID(), m_typeProfilingStartOffset, m_typeProfilingEndOffset);
}

void ProgramExecutable::destroy(JSCell* cell)
{
    static_cast<ProgramExecutable*>(cell)->ProgramExecutable::~ProgramExecutable();
}

const ClassInfo ModuleProgramExecutable::s_info = { "ModuleProgramExecutable", &ScriptExecutable::s_info, 0, CREATE_METHOD_TABLE(ModuleProgramExecutable) };

ModuleProgramExecutable::ModuleProgramExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec->vm().moduleProgramExecutableStructure.get(), exec->vm(), source, false, DerivedContextType::None, false, EvalContextType::None, NoIntrinsic)
{
    m_typeProfilingStartOffset = 0;
    m_typeProfilingEndOffset = source.length() - 1;
    if (exec->vm().typeProfiler() || exec->vm().controlFlowProfiler())
        exec->vm().functionHasExecutedCache()->insertUnexecutedRange(sourceID(), m_typeProfilingStartOffset, m_typeProfilingEndOffset);
}

ModuleProgramExecutable* ModuleProgramExecutable::create(ExecState* exec, const SourceCode& source)
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    ModuleProgramExecutable* executable = new (NotNull, allocateCell<ModuleProgramExecutable>(*exec->heap())) ModuleProgramExecutable(exec, source);
    executable->finishCreation(exec->vm());

    UnlinkedModuleProgramCodeBlock* unlinkedModuleProgramCode = globalObject->createModuleProgramCodeBlock(exec, executable);
    if (!unlinkedModuleProgramCode)
        return nullptr;
    executable->m_unlinkedModuleProgramCodeBlock.set(exec->vm(), executable, unlinkedModuleProgramCode);

    executable->m_moduleEnvironmentSymbolTable.set(exec->vm(), executable, jsCast<SymbolTable*>(unlinkedModuleProgramCode->constantRegister(unlinkedModuleProgramCode->moduleEnvironmentSymbolTableConstantRegisterOffset()).get())->cloneScopePart(exec->vm()));

    return executable;
}

void ModuleProgramExecutable::destroy(JSCell* cell)
{
    static_cast<ModuleProgramExecutable*>(cell)->ModuleProgramExecutable::~ModuleProgramExecutable();
}

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable", &ScriptExecutable::s_info, 0, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, unsigned firstLine, unsigned lastLine, unsigned startColumn, unsigned endColumn, Intrinsic intrinsic)
    : ScriptExecutable(vm.functionExecutableStructure.get(), vm, source, unlinkedExecutable->isInStrictContext(), unlinkedExecutable->derivedContextType(), false, EvalContextType::None, intrinsic)
    , m_unlinkedExecutable(vm, this, unlinkedExecutable)
{
    RELEASE_ASSERT(!source.isNull());
    ASSERT(source.length());
    m_firstLine = firstLine;
    m_lastLine = lastLine;
    ASSERT(startColumn != UINT_MAX);
    ASSERT(endColumn != UINT_MAX);
    m_startColumn = startColumn;
    m_endColumn = endColumn;
    m_parametersStartOffset = unlinkedExecutable->parametersStartOffset();
    m_typeProfilingStartOffset = unlinkedExecutable->typeProfilingStartOffset();
    m_typeProfilingEndOffset = unlinkedExecutable->typeProfilingEndOffset();
}

void FunctionExecutable::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    m_singletonFunction.set(vm, this, InferredValue::create(vm));
}

void FunctionExecutable::destroy(JSCell* cell)
{
    static_cast<FunctionExecutable*>(cell)->FunctionExecutable::~FunctionExecutable();
}

inline const char* samplingDescription(JITCode::JITType jitType)
{
    switch (jitType) {
    case JITCode::InterpreterThunk:
        return "Interpreter Compilation (TOTAL)";
    case JITCode::BaselineJIT:
        return "Baseline Compilation (TOTAL)";
    case JITCode::DFGJIT:
        return "DFG Compilation (TOTAL)";
    case JITCode::FTLJIT:
        return "FTL Compilation (TOTAL)";
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

void EvalExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    EvalExecutable* thisObject = jsCast<EvalExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ScriptExecutable::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_unlinkedEvalCodeBlock);
    if (thisObject->m_evalCodeBlock)
        thisObject->m_evalCodeBlock->visitWeakly(visitor);
}

JSObject* ProgramExecutable::checkSyntax(ExecState* exec)
{
    ParserError error;
    VM* vm = &exec->vm();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    std::unique_ptr<ProgramNode> programNode = parse<ProgramNode>(
        vm, m_source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::NotStrict, JSParserCommentMode::Classic, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, error);
    if (programNode)
        return 0;
    ASSERT(error.isValid());
    return error.toErrorObject(lexicalGlobalObject, m_source);
}

JSObject* ProgramExecutable::initializeGlobalProperties(VM& vm, CallFrame* callFrame, JSScope* scope)
{
    RELEASE_ASSERT(scope);
    JSGlobalObject* globalObject = scope->globalObject();
    RELEASE_ASSERT(globalObject);
    ASSERT(&globalObject->vm() == &vm);

    JSObject* exception = 0;
    UnlinkedProgramCodeBlock* unlinkedCodeBlock = globalObject->createProgramCodeBlock(callFrame, this, &exception);
    if (exception)
        return exception;

    JSGlobalLexicalEnvironment* globalLexicalEnvironment = globalObject->globalLexicalEnvironment();
    const VariableEnvironment& variableDeclarations = unlinkedCodeBlock->variableDeclarations();
    const VariableEnvironment& lexicalDeclarations = unlinkedCodeBlock->lexicalDeclarations();
    // The ES6 spec says that no vars/global properties/let/const can be duplicated in the global scope.
    // This carried out section 15.1.8 of the ES6 spec: http://www.ecma-international.org/ecma-262/6.0/index.html#sec-globaldeclarationinstantiation
    {
        ExecState* exec = globalObject->globalExec();
        // Check for intersection of "var" and "let"/"const"/"class"
        for (auto& entry : lexicalDeclarations) {
            if (variableDeclarations.contains(entry.key))
                return createSyntaxError(exec, makeString("Can't create duplicate variable: '", String(entry.key.get()), "'"));
        }

        // Check if any new "let"/"const"/"class" will shadow any pre-existing global property names, or "var"/"let"/"const" variables.
        // It's an error to introduce a shadow.
        for (auto& entry : lexicalDeclarations) {
            if (globalObject->hasProperty(exec, entry.key.get())) {
                // The ES6 spec says that just RestrictedGlobalProperty can't be shadowed
                // This carried out section 8.1.1.4.14 of the ES6 spec: http://www.ecma-international.org/ecma-262/6.0/index.html#sec-hasrestrictedglobalproperty
                PropertyDescriptor descriptor;
                globalObject->getOwnPropertyDescriptor(exec, entry.key.get(), descriptor);
                
                if (descriptor.value() != jsUndefined() && !descriptor.configurable())
                    return createSyntaxError(exec, makeString("Can't create duplicate variable that shadows a global property: '", String(entry.key.get()), "'"));
            }
                
            if (globalLexicalEnvironment->hasProperty(exec, entry.key.get())) {
                if (UNLIKELY(entry.value.isConst() && !vm.globalConstRedeclarationShouldThrow() && !isStrictMode())) {
                    // We only allow "const" duplicate declarations under this setting.
                    // For example, we don't "let" variables to be overridden by "const" variables.
                    if (globalLexicalEnvironment->isConstVariable(entry.key.get()))
                        continue;
                }
                return createSyntaxError(exec, makeString("Can't create duplicate variable: '", String(entry.key.get()), "'"));
            }
        }

        // Check if any new "var"s will shadow any previous "let"/"const"/"class" names.
        // It's an error to introduce a shadow.
        if (!globalLexicalEnvironment->isEmpty()) {
            for (auto& entry : variableDeclarations) {
                if (globalLexicalEnvironment->hasProperty(exec, entry.key.get()))
                    return createSyntaxError(exec, makeString("Can't create duplicate variable: '", String(entry.key.get()), "'"));
            }
        }
    }


    m_unlinkedProgramCodeBlock.set(vm, this, unlinkedCodeBlock);

    BatchedTransitionOptimizer optimizer(vm, globalObject);

    for (size_t i = 0, numberOfFunctions = unlinkedCodeBlock->numberOfFunctionDecls(); i < numberOfFunctions; ++i) {
        UnlinkedFunctionExecutable* unlinkedFunctionExecutable = unlinkedCodeBlock->functionDecl(i);
        ASSERT(!unlinkedFunctionExecutable->name().isEmpty());
        globalObject->addFunction(callFrame, unlinkedFunctionExecutable->name());
        if (vm.typeProfiler() || vm.controlFlowProfiler()) {
            vm.functionHasExecutedCache()->insertUnexecutedRange(sourceID(), 
                unlinkedFunctionExecutable->typeProfilingStartOffset(), 
                unlinkedFunctionExecutable->typeProfilingEndOffset());
        }
    }

    for (auto& entry : variableDeclarations) {
        ASSERT(entry.value.isVar());
        globalObject->addVar(callFrame, Identifier::fromUid(&vm, entry.key.get()));
    }

    {
        JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(globalObject->globalScope());
        SymbolTable* symbolTable = globalLexicalEnvironment->symbolTable();
        ConcurrentJITLocker locker(symbolTable->m_lock);
        for (auto& entry : lexicalDeclarations) {
            if (UNLIKELY(entry.value.isConst() && !vm.globalConstRedeclarationShouldThrow() && !isStrictMode())) {
                if (symbolTable->contains(locker, entry.key.get()))
                    continue;
            }
            ScopeOffset offset = symbolTable->takeNextScopeOffset(locker);
            SymbolTableEntry newEntry(VarOffset(offset), entry.value.isConst() ? ReadOnly : 0);
            newEntry.prepareToWatch();
            symbolTable->add(locker, entry.key.get(), newEntry);
            
            ScopeOffset offsetForAssert = globalLexicalEnvironment->addVariables(1, jsTDZValue());
            RELEASE_ASSERT(offsetForAssert == offset);
        }
    }
    return 0;
}

void ProgramExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ProgramExecutable* thisObject = jsCast<ProgramExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ScriptExecutable::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_unlinkedProgramCodeBlock);
    if (thisObject->m_programCodeBlock)
        thisObject->m_programCodeBlock->visitWeakly(visitor);
}

void ModuleProgramExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ModuleProgramExecutable* thisObject = jsCast<ModuleProgramExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ScriptExecutable::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_unlinkedModuleProgramCodeBlock);
    visitor.append(&thisObject->m_moduleEnvironmentSymbolTable);
    if (thisObject->m_moduleProgramCodeBlock)
        thisObject->m_moduleProgramCodeBlock->visitWeakly(visitor);
}

FunctionCodeBlock* FunctionExecutable::baselineCodeBlockFor(CodeSpecializationKind kind)
{
    FunctionCodeBlock* result;
    if (kind == CodeForCall)
        result = m_codeBlockForCall.get();
    else {
        RELEASE_ASSERT(kind == CodeForConstruct);
        result = m_codeBlockForConstruct.get();
    }
    if (!result)
        return 0;
    return static_cast<FunctionCodeBlock*>(result->baselineAlternative());
}

void FunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    FunctionExecutable* thisObject = jsCast<FunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (thisObject->m_codeBlockForCall)
        thisObject->m_codeBlockForCall->visitWeakly(visitor);
    if (thisObject->m_codeBlockForConstruct)
        thisObject->m_codeBlockForConstruct->visitWeakly(visitor);
    visitor.append(&thisObject->m_unlinkedExecutable);
    visitor.append(&thisObject->m_singletonFunction);
}

FunctionExecutable* FunctionExecutable::fromGlobalCode(
    const Identifier& name, ExecState& exec, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber)
{
    UnlinkedFunctionExecutable* unlinkedExecutable = 
        UnlinkedFunctionExecutable::fromGlobalCode(
            name, exec, source, exception, overrideLineNumber);
    if (!unlinkedExecutable)
        return nullptr;

    return unlinkedExecutable->link(exec.vm(), source, overrideLineNumber);
}

#if ENABLE(WEBASSEMBLY)
const ClassInfo WebAssemblyExecutable::s_info = { "WebAssemblyExecutable", &ExecutableBase::s_info, 0, CREATE_METHOD_TABLE(WebAssemblyExecutable) };

WebAssemblyExecutable::WebAssemblyExecutable(VM& vm, const SourceCode& source, JSWASMModule* module, unsigned functionIndex)
    : ExecutableBase(vm, vm.webAssemblyExecutableStructure.get(), NUM_PARAMETERS_NOT_COMPILED, NoIntrinsic)
    , m_source(source)
    , m_module(vm, this, module)
    , m_functionIndex(functionIndex)
{
}

void WebAssemblyExecutable::destroy(JSCell* cell)
{
    static_cast<WebAssemblyExecutable*>(cell)->WebAssemblyExecutable::~WebAssemblyExecutable();
}

void WebAssemblyExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyExecutable* thisObject = jsCast<WebAssemblyExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ExecutableBase::visitChildren(thisObject, visitor);
    if (thisObject->m_codeBlockForCall)
        thisObject->m_codeBlockForCall->visitWeakly(visitor);
    visitor.append(&thisObject->m_module);
}
#endif

void ExecutableBase::dump(PrintStream& out) const
{
    ExecutableBase* realThis = const_cast<ExecutableBase*>(this);
    
    if (classInfo() == NativeExecutable::info()) {
        NativeExecutable* native = jsCast<NativeExecutable*>(realThis);
        out.print("NativeExecutable:", RawPointer(bitwise_cast<void*>(native->function())), "/", RawPointer(bitwise_cast<void*>(native->constructor())));
        return;
    }
    
    if (classInfo() == EvalExecutable::info()) {
        EvalExecutable* eval = jsCast<EvalExecutable*>(realThis);
        if (CodeBlock* codeBlock = eval->codeBlock())
            out.print(*codeBlock);
        else
            out.print("EvalExecutable w/o CodeBlock");
        return;
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        ProgramExecutable* eval = jsCast<ProgramExecutable*>(realThis);
        if (CodeBlock* codeBlock = eval->codeBlock())
            out.print(*codeBlock);
        else
            out.print("ProgramExecutable w/o CodeBlock");
        return;
    }

    if (classInfo() == ModuleProgramExecutable::info()) {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(realThis);
        if (CodeBlock* codeBlock = executable->codeBlock())
            out.print(*codeBlock);
        else
            out.print("ModuleProgramExecutable w/o CodeBlock");
        return;
    }
    
    FunctionExecutable* function = jsCast<FunctionExecutable*>(realThis);
    if (!function->eitherCodeBlock())
        out.print("FunctionExecutable w/o CodeBlock");
    else {
        CommaPrinter comma("/");
        if (function->codeBlockForCall())
            out.print(comma, *function->codeBlockForCall());
        if (function->codeBlockForConstruct())
            out.print(comma, *function->codeBlockForConstruct());
    }
}

CodeBlockHash ExecutableBase::hashFor(CodeSpecializationKind kind) const
{
    if (this->classInfo() == NativeExecutable::info())
        return jsCast<const NativeExecutable*>(this)->hashFor(kind);
    
    return jsCast<const ScriptExecutable*>(this)->hashFor(kind);
}

CodeBlockHash NativeExecutable::hashFor(CodeSpecializationKind kind) const
{
    if (kind == CodeForCall)
        return CodeBlockHash(static_cast<unsigned>(bitwise_cast<size_t>(m_function)));
    
    RELEASE_ASSERT(kind == CodeForConstruct);
    return CodeBlockHash(static_cast<unsigned>(bitwise_cast<size_t>(m_constructor)));
}

CodeBlockHash ScriptExecutable::hashFor(CodeSpecializationKind kind) const
{
    return CodeBlockHash(source(), kind);
}

}
