/*
 * Copyright (C) 2009, 2010, 2013 Apple Inc. All rights reserved.
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
#include "BytecodeGenerator.h"
#include "CodeBlock.h"
#include "DFGDriver.h"
#include "JIT.h"
#include "LLIntEntrypoint.h"
#include "Operations.h"
#include "Parser.h"
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

const ClassInfo ExecutableBase::s_info = { "Executable", 0, 0, 0, CREATE_METHOD_TABLE(ExecutableBase) };

#if ENABLE(JIT)
void ExecutableBase::destroy(JSCell* cell)
{
    static_cast<ExecutableBase*>(cell)->ExecutableBase::~ExecutableBase();
}
#endif

void ExecutableBase::clearCode()
{
#if ENABLE(JIT)
    m_jitCodeForCall.clear();
    m_jitCodeForConstruct.clear();
    m_jitCodeForCallWithArityCheck = MacroAssemblerCodePtr();
    m_jitCodeForConstructWithArityCheck = MacroAssemblerCodePtr();
#endif
    m_numParametersForCall = NUM_PARAMETERS_NOT_COMPILED;
    m_numParametersForConstruct = NUM_PARAMETERS_NOT_COMPILED;
}

#if ENABLE(DFG_JIT)
Intrinsic ExecutableBase::intrinsic() const
{
    if (const NativeExecutable* nativeExecutable = jsDynamicCast<const NativeExecutable*>(this))
        return nativeExecutable->intrinsic();
    return NoIntrinsic;
}
#else
Intrinsic ExecutableBase::intrinsic() const
{
    return NoIntrinsic;
}
#endif

const ClassInfo NativeExecutable::s_info = { "NativeExecutable", &ExecutableBase::s_info, 0, 0, CREATE_METHOD_TABLE(NativeExecutable) };

#if ENABLE(JIT)
void NativeExecutable::destroy(JSCell* cell)
{
    static_cast<NativeExecutable*>(cell)->NativeExecutable::~NativeExecutable();
}
#endif

#if ENABLE(DFG_JIT)
Intrinsic NativeExecutable::intrinsic() const
{
    return m_intrinsic;
}
#endif

#if ENABLE(JIT)
// Utility method used for jettisoning code blocks.
template<typename T>
static void jettisonCodeBlock(VM& vm, RefPtr<T>& codeBlock)
{
    ASSERT(JITCode::isOptimizingJIT(codeBlock->jitType()));
    ASSERT(codeBlock->alternative());
    RefPtr<T> codeBlockToJettison = codeBlock.release();
    codeBlock = static_pointer_cast<T>(codeBlockToJettison->releaseAlternative());
    codeBlockToJettison->unlinkIncomingCalls();
    vm.heap.jettisonDFGCodeBlock(static_pointer_cast<CodeBlock>(codeBlockToJettison.release()));
}
#endif

const ClassInfo ScriptExecutable::s_info = { "ScriptExecutable", &ExecutableBase::s_info, 0, 0, CREATE_METHOD_TABLE(ScriptExecutable) };

#if ENABLE(JIT)
void ScriptExecutable::destroy(JSCell* cell)
{
    static_cast<ScriptExecutable*>(cell)->ScriptExecutable::~ScriptExecutable();
}
#endif

void ScriptExecutable::installCode(CodeBlock* genericCodeBlock)
{
    RELEASE_ASSERT(genericCodeBlock->ownerExecutable() == this);
    RELEASE_ASSERT(JITCode::isExecutableScript(genericCodeBlock->jitType()));
    
    VM& vm = *genericCodeBlock->vm();
    
    if (vm.m_perBytecodeProfiler)
        vm.m_perBytecodeProfiler->ensureBytecodesFor(genericCodeBlock);
    
    ASSERT(vm.heap.isDeferred());
    
    if (JITCode::isJIT(genericCodeBlock->jitType())) {
        vm.heap.reportExtraMemoryCost(
            sizeof(CodeBlock) + genericCodeBlock->jitCode()->size());
    } else
        vm.heap.reportExtraMemoryCost(sizeof(CodeBlock));
    
    CodeSpecializationKind kind = genericCodeBlock->specializationKind();
    
    RefPtr<CodeBlock> oldCodeBlock;
    
    switch (kind) {
    case CodeForCall:
        m_jitCodeForCall = genericCodeBlock->jitCode();
        m_jitCodeForCallWithArityCheck = genericCodeBlock->jitCodeWithArityCheck();
        m_numParametersForCall = genericCodeBlock->numParameters();
        break;
    case CodeForConstruct:
        m_jitCodeForConstruct = genericCodeBlock->jitCode();
        m_jitCodeForConstructWithArityCheck = genericCodeBlock->jitCodeWithArityCheck();
        m_numParametersForConstruct = genericCodeBlock->numParameters();
        break;
    }
    
    switch (genericCodeBlock->codeType()) {
    case GlobalCode: {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        ProgramCodeBlock* codeBlock = static_cast<ProgramCodeBlock*>(genericCodeBlock);
        
        ASSERT(!codeBlock->jitCodeWithArityCheck());
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = executable->m_programCodeBlock;
        executable->m_programCodeBlock = codeBlock;
        break;
    }
        
    case EvalCode: {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        EvalCodeBlock* codeBlock = static_cast<EvalCodeBlock*>(genericCodeBlock);
        
        ASSERT(!codeBlock->jitCodeWithArityCheck());
        ASSERT(kind == CodeForCall);
        
        oldCodeBlock = executable->m_evalCodeBlock;
        executable->m_evalCodeBlock = codeBlock;
        break;
    }
        
    case FunctionCode: {
        FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
        FunctionCodeBlock* codeBlock = static_cast<FunctionCodeBlock*>(genericCodeBlock);
        
        switch (kind) {
        case CodeForCall:
            oldCodeBlock = executable->m_codeBlockForCall;
            executable->m_codeBlockForCall = codeBlock;
            break;
        case CodeForConstruct:
            oldCodeBlock = executable->m_codeBlockForConstruct;
            executable->m_codeBlockForConstruct = codeBlock;
            break;
        }
        break;
    } }

    if (oldCodeBlock)
        oldCodeBlock->unlinkIncomingCalls();
}

PassRefPtr<CodeBlock> ScriptExecutable::newCodeBlockFor(
    CodeSpecializationKind kind, JSScope* scope, JSObject*& exception)
{
    VM* vm = scope->vm();

    ASSERT(vm->heap.isDeferred());
    
    if (classInfo() == EvalExecutable::info()) {
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_evalCodeBlock);
        return adoptRef(new EvalCodeBlock(
            executable, executable->m_unlinkedEvalCodeBlock.get(), scope,
            executable->source().provider()));
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        RELEASE_ASSERT(kind == CodeForCall);
        RELEASE_ASSERT(!executable->m_programCodeBlock);
        return adoptRef(new ProgramCodeBlock(
            executable, executable->m_unlinkedProgramCodeBlock.get(), scope,
            executable->source().provider(), executable->source().startColumn()));
    }
    
    RELEASE_ASSERT(classInfo() == FunctionExecutable::info());
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
    RELEASE_ASSERT(!executable->codeBlockFor(kind));
    JSGlobalObject* globalObject = scope->globalObject();
    ParserError error;
    DebuggerMode debuggerMode = globalObject->hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = globalObject->hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedFunctionCodeBlock* unlinkedCodeBlock =
        executable->m_unlinkedExecutable->codeBlockFor(
            *vm, executable->m_source, kind, debuggerMode, profilerMode, error);
    if (!unlinkedCodeBlock) {
        exception = vm->throwException(
            globalObject->globalExec(),
            error.toErrorObject(globalObject, executable->m_source));
        return 0;
    }
    
    SourceProvider* provider = executable->source().provider();
    unsigned sourceOffset = executable->source().startOffset();
    unsigned startColumn = executable->source().startColumn();

    return adoptRef(new FunctionCodeBlock(
        executable, unlinkedCodeBlock, scope, provider, sourceOffset, startColumn));
}

PassRefPtr<CodeBlock> ScriptExecutable::newReplacementCodeBlockFor(
    CodeSpecializationKind kind)
{
    if (classInfo() == EvalExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        EvalExecutable* executable = jsCast<EvalExecutable*>(this);
        RefPtr<EvalCodeBlock> result = adoptRef(new EvalCodeBlock(
            CodeBlock::CopyParsedBlock, *executable->m_evalCodeBlock));
        result->setAlternative(executable->m_evalCodeBlock);
        return result;
    }
    
    if (classInfo() == ProgramExecutable::info()) {
        RELEASE_ASSERT(kind == CodeForCall);
        ProgramExecutable* executable = jsCast<ProgramExecutable*>(this);
        RefPtr<ProgramCodeBlock> result = adoptRef(new ProgramCodeBlock(
            CodeBlock::CopyParsedBlock, *executable->m_programCodeBlock));
        result->setAlternative(executable->m_programCodeBlock);
        return result;
    }

    RELEASE_ASSERT(classInfo() == FunctionExecutable::info());
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(this);
    RefPtr<FunctionCodeBlock> result = adoptRef(new FunctionCodeBlock(
        CodeBlock::CopyParsedBlock, *executable->codeBlockFor(kind)));
    result->setAlternative(executable->codeBlockFor(kind));
    return result;
}

JSObject* ScriptExecutable::prepareForExecutionImpl(
    ExecState* exec, JSScope* scope, CodeSpecializationKind kind)
{
    VM& vm = exec->vm();
    DeferGC deferGC(vm.heap);
    
    JSObject* exception = 0;
    RefPtr<CodeBlock> codeBlock = newCodeBlockFor(kind, scope, exception);
    if (!codeBlock) {
        RELEASE_ASSERT(exception);
        return exception;
    }
    
#if ENABLE(LLINT)
    LLInt::setEntrypoint(vm, codeBlock.get());
#else
    CompilationResult result = JIT::compile(&vm, codeBlock.get(), JITCompilationMustSucceed);
    RELEASE_ASSERT(result == CompilationSuccessful);
#endif

    installCode(codeBlock.get());
    return 0;
}

const ClassInfo EvalExecutable::s_info = { "EvalExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(EvalExecutable) };

EvalExecutable* EvalExecutable::create(ExecState* exec, const SourceCode& source, bool isInStrictContext) 
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (!globalObject->evalEnabled()) {
        exec->vm().throwException(exec, createEvalError(exec, globalObject->evalDisabledErrorMessage()));
        return 0;
    }

    EvalExecutable* executable = new (NotNull, allocateCell<EvalExecutable>(*exec->heap())) EvalExecutable(exec, source, isInStrictContext);
    executable->finishCreation(exec->vm());

    UnlinkedEvalCodeBlock* unlinkedEvalCode = globalObject->createEvalCodeBlock(exec, executable);
    if (!unlinkedEvalCode)
        return 0;

    executable->m_unlinkedEvalCodeBlock.set(exec->vm(), executable, unlinkedEvalCode);

    return executable;
}

EvalExecutable::EvalExecutable(ExecState* exec, const SourceCode& source, bool inStrictContext)
    : ScriptExecutable(exec->vm().evalExecutableStructure.get(), exec, source, inStrictContext)
{
}

void EvalExecutable::destroy(JSCell* cell)
{
    static_cast<EvalExecutable*>(cell)->EvalExecutable::~EvalExecutable();
}

const ClassInfo ProgramExecutable::s_info = { "ProgramExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(ProgramExecutable) };

ProgramExecutable::ProgramExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec->vm().programExecutableStructure.get(), exec, source, false)
{
}

void ProgramExecutable::destroy(JSCell* cell)
{
    static_cast<ProgramExecutable*>(cell)->ProgramExecutable::~ProgramExecutable();
}

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, unsigned firstLine, unsigned lastLine, unsigned startColumn)
    : ScriptExecutable(vm.functionExecutableStructure.get(), vm, source, unlinkedExecutable->isInStrictContext())
    , m_unlinkedExecutable(vm, this, unlinkedExecutable)
{
    RELEASE_ASSERT(!source.isNull());
    ASSERT(source.length());
    m_firstLine = firstLine;
    m_lastLine = lastLine;
    m_startColumn = startColumn;
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

#if ENABLE(JIT)
void EvalExecutable::jettisonOptimizedCode(VM& vm)
{
    jettisonCodeBlock(vm, m_evalCodeBlock);
    m_jitCodeForCall = m_evalCodeBlock->jitCode();
    ASSERT(!m_jitCodeForCallWithArityCheck);
}
#endif // ENABLE(JIT)

void EvalExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    EvalExecutable* thisObject = jsCast<EvalExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (thisObject->m_evalCodeBlock)
        thisObject->m_evalCodeBlock->visitAggregate(visitor);
    visitor.append(&thisObject->m_unlinkedEvalCodeBlock);
}

void EvalExecutable::unlinkCalls()
{
#if ENABLE(JIT)
    if (!m_jitCodeForCall)
        return;
    RELEASE_ASSERT(m_evalCodeBlock);
    m_evalCodeBlock->unlinkCalls();
#endif
}

void EvalExecutable::clearCode()
{
    m_evalCodeBlock.clear();
    m_unlinkedEvalCodeBlock.clear();
    Base::clearCode();
}

JSObject* ProgramExecutable::checkSyntax(ExecState* exec)
{
    ParserError error;
    VM* vm = &exec->vm();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> programNode = parse<ProgramNode>(vm, m_source, 0, Identifier(), JSParseNormal, ProgramNode::isFunctionNode ? JSParseFunctionCode : JSParseProgramCode, error);
    if (programNode)
        return 0;
    ASSERT(error.m_type != ParserError::ErrorNone);
    return error.toErrorObject(lexicalGlobalObject, m_source);
}

#if ENABLE(JIT)
void ProgramExecutable::jettisonOptimizedCode(VM& vm)
{
    jettisonCodeBlock(vm, m_programCodeBlock);
    m_jitCodeForCall = m_programCodeBlock->jitCode();
    ASSERT(!m_jitCodeForCallWithArityCheck);
}
#endif

void ProgramExecutable::unlinkCalls()
{
#if ENABLE(JIT)
    if (!m_jitCodeForCall)
        return;
    RELEASE_ASSERT(m_programCodeBlock);
    m_programCodeBlock->unlinkCalls();
#endif
}

JSObject* ProgramExecutable::initializeGlobalProperties(VM& vm, CallFrame* callFrame, JSScope* scope)
{
    RELEASE_ASSERT(scope);
    JSGlobalObject* globalObject = scope->globalObject();
    RELEASE_ASSERT(globalObject);
    ASSERT(&globalObject->vm() == &vm);

    JSObject* exception = 0;
    UnlinkedProgramCodeBlock* unlinkedCode = globalObject->createProgramCodeBlock(callFrame, this, &exception);
    if (exception)
        return exception;

    m_unlinkedProgramCodeBlock.set(vm, this, unlinkedCode);

    BatchedTransitionOptimizer optimizer(vm, globalObject);

    const UnlinkedProgramCodeBlock::VariableDeclations& variableDeclarations = unlinkedCode->variableDeclarations();
    const UnlinkedProgramCodeBlock::FunctionDeclations& functionDeclarations = unlinkedCode->functionDeclarations();

    CallFrame* globalExec = globalObject->globalExec();

    for (size_t i = 0; i < functionDeclarations.size(); ++i) {
        UnlinkedFunctionExecutable* unlinkedFunctionExecutable = functionDeclarations[i].second.get();
        JSValue value = JSFunction::create(globalExec, unlinkedFunctionExecutable->link(vm, m_source, lineNo(), 0), scope);
        globalObject->addFunction(callFrame, functionDeclarations[i].first, value);
    }

    for (size_t i = 0; i < variableDeclarations.size(); ++i) {
        if (variableDeclarations[i].second & DeclarationStacks::IsConstant)
            globalObject->addConst(callFrame, variableDeclarations[i].first);
        else
            globalObject->addVar(callFrame, variableDeclarations[i].first);
    }
    return 0;
}

void ProgramExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ProgramExecutable* thisObject = jsCast<ProgramExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    ScriptExecutable::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_unlinkedProgramCodeBlock);
    if (thisObject->m_programCodeBlock)
        thisObject->m_programCodeBlock->visitAggregate(visitor);
}

void ProgramExecutable::clearCode()
{
    m_programCodeBlock.clear();
    m_unlinkedProgramCodeBlock.clear();
    Base::clearCode();
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
    while (result->alternative())
        result = static_cast<FunctionCodeBlock*>(result->alternative());
    RELEASE_ASSERT(result);
    ASSERT(JITCode::isBaselineCode(result->jitType()));
    return result;
}

#if ENABLE(JIT)
void FunctionExecutable::jettisonOptimizedCodeForCall(VM& vm)
{
    jettisonCodeBlock(vm, m_codeBlockForCall);
    m_jitCodeForCall = m_codeBlockForCall->jitCode();
    m_jitCodeForCallWithArityCheck = m_codeBlockForCall->jitCodeWithArityCheck();
}

void FunctionExecutable::jettisonOptimizedCodeForConstruct(VM& vm)
{
    jettisonCodeBlock(vm, m_codeBlockForConstruct);
    m_jitCodeForConstruct = m_codeBlockForConstruct->jitCode();
    m_jitCodeForConstructWithArityCheck = m_codeBlockForConstruct->jitCodeWithArityCheck();
}
#endif

void FunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    FunctionExecutable* thisObject = jsCast<FunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (thisObject->m_codeBlockForCall)
        thisObject->m_codeBlockForCall->visitAggregate(visitor);
    if (thisObject->m_codeBlockForConstruct)
        thisObject->m_codeBlockForConstruct->visitAggregate(visitor);
    visitor.append(&thisObject->m_unlinkedExecutable);
}

void FunctionExecutable::clearCodeIfNotCompiling()
{
    if (isCompiling())
        return;
    clearCode();
}

void FunctionExecutable::clearUnlinkedCodeForRecompilationIfNotCompiling()
{
    if (isCompiling())
        return;
    m_unlinkedExecutable->clearCodeForRecompilation();
}

void FunctionExecutable::clearCode()
{
    m_codeBlockForCall.clear();
    m_codeBlockForConstruct.clear();
    Base::clearCode();
}

void FunctionExecutable::unlinkCalls()
{
#if ENABLE(JIT)
    if (!!m_jitCodeForCall) {
        RELEASE_ASSERT(m_codeBlockForCall);
        m_codeBlockForCall->unlinkCalls();
    }
    if (!!m_jitCodeForConstruct) {
        RELEASE_ASSERT(m_codeBlockForConstruct);
        m_codeBlockForConstruct->unlinkCalls();
    }
#endif
}

FunctionExecutable* FunctionExecutable::fromGlobalCode(const Identifier& name, ExecState* exec, Debugger* debugger, const SourceCode& source, JSObject** exception)
{
    UnlinkedFunctionExecutable* unlinkedFunction = UnlinkedFunctionExecutable::fromGlobalCode(name, exec, debugger, source, exception);
    if (!unlinkedFunction)
        return 0;
    unsigned firstLine = source.firstLine() + unlinkedFunction->firstLineOffset();
    unsigned startOffset = source.startOffset() + unlinkedFunction->startOffset();
    unsigned startColumn = source.startColumn();
    unsigned sourceLength = unlinkedFunction->sourceLength();
    SourceCode functionSource(source.provider(), startOffset, startOffset + sourceLength, firstLine, startColumn);
    return FunctionExecutable::create(exec->vm(), functionSource, unlinkedFunction, firstLine, unlinkedFunction->lineCount(), startColumn);
}

String FunctionExecutable::paramString() const
{
    return m_unlinkedExecutable->paramString();
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
