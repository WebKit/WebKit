/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
#include "ExecutionHarness.h"
#include "JIT.h"
#include "JITDriver.h"
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

const ClassInfo EvalExecutable::s_info = { "EvalExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(EvalExecutable) };

EvalExecutable* EvalExecutable::create(ExecState* exec, const SourceCode& source, bool isInStrictContext) 
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (!globalObject->evalEnabled()) {
        throwError(exec, createEvalError(exec, globalObject->evalDisabledErrorMessage()));
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

#if ENABLE(DFG_JIT)
JSObject* EvalExecutable::compileOptimized(ExecState* exec, JSScope* scope, CompilationResult& result, unsigned bytecodeIndex)
{
    ASSERT(exec->vm().dynamicGlobalObject);
    ASSERT(!!m_evalCodeBlock);
    JSObject* error = 0;
    if (!JITCode::isOptimizingJIT(m_evalCodeBlock->jitType()))
        error = compileInternal(exec, scope, JITCode::nextTierJIT(m_evalCodeBlock->jitType()), &result, bytecodeIndex);
    else
        result = CompilationNotNeeded;
    ASSERT(!!m_evalCodeBlock);
    return error;
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(JIT)
CompilationResult EvalExecutable::jitCompile(ExecState* exec)
{
    return jitCompileIfAppropriate(exec, m_evalCodeBlock.get(), m_jitCodeForCall, JITCode::bottomTierJIT(), UINT_MAX, JITCompilationCanFail);
}
#endif

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

JSObject* EvalExecutable::compileInternal(ExecState* exec, JSScope* scope, JITCode::JITType jitType, CompilationResult* result, unsigned bytecodeIndex)
{
    SamplingRegion samplingRegion(samplingDescription(jitType));
    
    if (result)
        *result = CompilationFailed;
    
    RefPtr<EvalCodeBlock> newCodeBlock;
    
    if (!!m_evalCodeBlock) {
        newCodeBlock = adoptRef(new EvalCodeBlock(CodeBlock::CopyParsedBlock, *m_evalCodeBlock));
        newCodeBlock->setAlternative(static_pointer_cast<CodeBlock>(m_evalCodeBlock));
    } else {
        newCodeBlock = adoptRef(new EvalCodeBlock(this, m_unlinkedEvalCodeBlock.get(), scope, source().provider()));
        ASSERT((jitType == JITCode::bottomTierJIT()) == !m_evalCodeBlock);
    }

    CompilationResult theResult = prepareForExecution(
        exec, m_evalCodeBlock, newCodeBlock.get(), m_jitCodeForCall, jitType, bytecodeIndex);
    if (result)
        *result = theResult;

    return 0;
}

#if ENABLE(DFG_JIT)
CompilationResult EvalExecutable::replaceWithDeferredOptimizedCode(PassRefPtr<DFG::Plan> plan)
{
    return JSC::replaceWithDeferredOptimizedCode(
        plan, m_evalCodeBlock, m_jitCodeForCall, 0, 0);
}
#endif // ENABLE(DFG_JIT)

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

#if ENABLE(DFG_JIT)
JSObject* ProgramExecutable::compileOptimized(ExecState* exec, JSScope* scope, CompilationResult& result, unsigned bytecodeIndex)
{
    RELEASE_ASSERT(exec->vm().dynamicGlobalObject);
    ASSERT(!!m_programCodeBlock);
    JSObject* error = 0;
    if (!JITCode::isOptimizingJIT(m_programCodeBlock->jitType()))
        error = compileInternal(exec, scope, JITCode::nextTierJIT(m_programCodeBlock->jitType()), &result, bytecodeIndex);
    else
        result = CompilationNotNeeded;
    ASSERT(!!m_programCodeBlock);
    return error;
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(JIT)
CompilationResult ProgramExecutable::jitCompile(ExecState* exec)
{
    return jitCompileIfAppropriate(exec, m_programCodeBlock.get(), m_jitCodeForCall, JITCode::bottomTierJIT(), UINT_MAX, JITCompilationCanFail);
}
#endif

JSObject* ProgramExecutable::compileInternal(ExecState* exec, JSScope* scope, JITCode::JITType jitType, CompilationResult* result, unsigned bytecodeIndex)
{
    SamplingRegion samplingRegion(samplingDescription(jitType));
    
    if (result)
        *result = CompilationFailed;
    
    RefPtr<ProgramCodeBlock> newCodeBlock;
    
    if (!!m_programCodeBlock) {
        newCodeBlock = adoptRef(new ProgramCodeBlock(CodeBlock::CopyParsedBlock, *m_programCodeBlock));
        newCodeBlock->setAlternative(static_pointer_cast<CodeBlock>(m_programCodeBlock));
    } else {
        newCodeBlock = adoptRef(new ProgramCodeBlock(this, m_unlinkedProgramCodeBlock.get(), scope, source().provider(), source().startColumn()));
    }

    CompilationResult theResult = prepareForExecution(
        exec, m_programCodeBlock, newCodeBlock.get(), m_jitCodeForCall, jitType, bytecodeIndex);
    if (result)
        *result = theResult;

    return 0;
}

#if ENABLE(DFG_JIT)
CompilationResult ProgramExecutable::replaceWithDeferredOptimizedCode(PassRefPtr<DFG::Plan> plan)
{
    return JSC::replaceWithDeferredOptimizedCode(
        plan, m_programCodeBlock, m_jitCodeForCall, 0, 0);
}
#endif // ENABLE(DFG_JIT)

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

#if ENABLE(DFG_JIT)
JSObject* FunctionExecutable::compileOptimizedForCall(ExecState* exec, JSScope* scope, CompilationResult& result, unsigned bytecodeIndex)
{
    RELEASE_ASSERT(exec->vm().dynamicGlobalObject);
    ASSERT(!!m_codeBlockForCall);
    JSObject* error = 0;
    if (!JITCode::isOptimizingJIT(m_codeBlockForCall->jitType()))
        error = compileForCallInternal(exec, scope, JITCode::nextTierJIT(m_codeBlockForCall->jitType()), &result, bytecodeIndex);
    else
        result = CompilationNotNeeded;
    ASSERT(!!m_codeBlockForCall);
    return error;
}

JSObject* FunctionExecutable::compileOptimizedForConstruct(ExecState* exec, JSScope* scope, CompilationResult& result, unsigned bytecodeIndex)
{
    RELEASE_ASSERT(exec->vm().dynamicGlobalObject);
    ASSERT(!!m_codeBlockForConstruct);
    JSObject* error = 0;
    if (!JITCode::isOptimizingJIT(m_codeBlockForConstruct->jitType()))
        error = compileForConstructInternal(exec, scope, JITCode::nextTierJIT(m_codeBlockForConstruct->jitType()), &result, bytecodeIndex);
    else
        result = CompilationNotNeeded;
    ASSERT(!!m_codeBlockForConstruct);
    return error;
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(JIT)
CompilationResult FunctionExecutable::jitCompileForCall(ExecState* exec)
{
    return jitCompileFunctionIfAppropriate(exec, m_codeBlockForCall.get(), m_jitCodeForCall, m_jitCodeForCallWithArityCheck, JITCode::bottomTierJIT(), UINT_MAX, JITCompilationCanFail);
}

CompilationResult FunctionExecutable::jitCompileForConstruct(ExecState* exec)
{
    return jitCompileFunctionIfAppropriate(exec, m_codeBlockForConstruct.get(), m_jitCodeForConstruct, m_jitCodeForConstructWithArityCheck, JITCode::bottomTierJIT(), UINT_MAX, JITCompilationCanFail);
}
#endif

PassRefPtr<FunctionCodeBlock> FunctionExecutable::produceCodeBlockFor(JSScope* scope, CodeSpecializationKind specializationKind, JSObject*& exception)
{
    RefPtr<FunctionCodeBlock> alternative = codeBlockFor(specializationKind);
    
    if (!!alternative) {
        RefPtr<FunctionCodeBlock> result = adoptRef(new FunctionCodeBlock(CodeBlock::CopyParsedBlock, *codeBlockFor(specializationKind)));
        result->setAlternative(alternative);
        return result.release();
    }

    VM* vm = scope->vm();
    JSGlobalObject* globalObject = scope->globalObject();
    ParserError error;
    DebuggerMode debuggerMode = globalObject->hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = globalObject->hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedFunctionCodeBlock* unlinkedCodeBlock = m_unlinkedExecutable->codeBlockFor(*vm, m_source, specializationKind, debuggerMode, profilerMode, error);
    recordParse(m_unlinkedExecutable->features(), m_unlinkedExecutable->hasCapturedVariables(), lineNo(), lastLine(), startColumn());

    if (!unlinkedCodeBlock) {
        exception = error.toErrorObject(globalObject, m_source);
        return 0;
    }

    SourceProvider* provider = source().provider();
    unsigned sourceOffset = source().startOffset();
    unsigned startColumn = source().startColumn();

    return adoptRef(new FunctionCodeBlock(this, unlinkedCodeBlock, scope, provider, sourceOffset, startColumn));
}


JSObject* FunctionExecutable::compileForCallInternal(ExecState* exec, JSScope* scope, JITCode::JITType jitType, CompilationResult* result, unsigned bytecodeIndex)
{
    SamplingRegion samplingRegion(samplingDescription(jitType));
    
    if (result)
        *result = CompilationFailed;
    
    ASSERT((jitType == JITCode::bottomTierJIT()) == !m_codeBlockForCall);
    JSObject* exception = 0;
    
    RefPtr<FunctionCodeBlock> newCodeBlock = produceCodeBlockFor(scope, CodeForCall, exception);
    if (!newCodeBlock)
        return exception;
    
    CompilationResult theResult = prepareFunctionForExecution(
        exec, m_codeBlockForCall, newCodeBlock.get(), m_jitCodeForCall,
        m_jitCodeForCallWithArityCheck, m_numParametersForCall, jitType,
        bytecodeIndex, CodeForCall);
    if (result)
        *result = theResult;
    return 0;
}

#if ENABLE(DFG_JIT)
CompilationResult FunctionExecutable::replaceWithDeferredOptimizedCodeForCall(PassRefPtr<DFG::Plan> plan)
{
    return JSC::replaceWithDeferredOptimizedCode(
        plan, m_codeBlockForCall, m_jitCodeForCall, &m_jitCodeForCallWithArityCheck,
        &m_numParametersForCall);
}
#endif // ENABLE(DFG_JIT)

JSObject* FunctionExecutable::compileForConstructInternal(ExecState* exec, JSScope* scope, JITCode::JITType jitType, CompilationResult* result, unsigned bytecodeIndex)
{
    SamplingRegion samplingRegion(samplingDescription(jitType));
    
    if (result)
        *result = CompilationFailed;
    
    ASSERT((jitType == JITCode::bottomTierJIT()) == !m_codeBlockForConstruct);
    JSObject* exception = 0;
    RefPtr<FunctionCodeBlock> newCodeBlock = produceCodeBlockFor(scope, CodeForConstruct, exception);
    if (!newCodeBlock)
        return exception;

    CompilationResult theResult = prepareFunctionForExecution(
        exec, m_codeBlockForConstruct, newCodeBlock.get(), m_jitCodeForConstruct,
        m_jitCodeForConstructWithArityCheck, m_numParametersForConstruct, jitType,
        bytecodeIndex, CodeForConstruct);
    if (result)
        *result = theResult;

    return 0;
}

#if ENABLE(DFG_JIT)
CompilationResult FunctionExecutable::replaceWithDeferredOptimizedCodeForConstruct(PassRefPtr<DFG::Plan> plan)
{
    return JSC::replaceWithDeferredOptimizedCode(
        plan, m_codeBlockForConstruct, m_jitCodeForConstruct,
        &m_jitCodeForConstructWithArityCheck, &m_numParametersForConstruct);
}
#endif // ENABLE(DFG_JIT)

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
