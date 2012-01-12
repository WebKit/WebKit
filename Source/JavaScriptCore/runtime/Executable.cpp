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

#include "BytecodeGenerator.h"
#include "CodeBlock.h"
#include "DFGDriver.h"
#include "JIT.h"
#include "JITDriver.h"
#include "Parser.h"
#include "UStringBuilder.h"
#include "Vector.h"

namespace JSC {

const ClassInfo ExecutableBase::s_info = { "Executable", 0, 0, 0, CREATE_METHOD_TABLE(ExecutableBase) };

void ExecutableBase::destroy(JSCell* cell)
{
    jsCast<ExecutableBase*>(cell)->ExecutableBase::~ExecutableBase();
}

inline void ExecutableBase::clearCode()
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
#endif

const ClassInfo NativeExecutable::s_info = { "NativeExecutable", &ExecutableBase::s_info, 0, 0, CREATE_METHOD_TABLE(NativeExecutable) };

void NativeExecutable::destroy(JSCell* cell)
{
    jsCast<NativeExecutable*>(cell)->NativeExecutable::~NativeExecutable();
}

#if ENABLE(DFG_JIT)
Intrinsic NativeExecutable::intrinsic() const
{
    return m_intrinsic;
}
#endif

#if ENABLE(JIT)
// Utility method used for jettisoning code blocks.
template<typename T>
static void jettisonCodeBlock(JSGlobalData& globalData, OwnPtr<T>& codeBlock)
{
    ASSERT(codeBlock->getJITType() != JITCode::BaselineJIT);
    ASSERT(codeBlock->alternative());
    OwnPtr<T> codeBlockToJettison = codeBlock.release();
    codeBlock = static_pointer_cast<T>(codeBlockToJettison->releaseAlternative());
    codeBlockToJettison->unlinkIncomingCalls();
    globalData.heap.jettisonDFGCodeBlock(static_pointer_cast<CodeBlock>(codeBlockToJettison.release()));
}
#endif

void NativeExecutable::finalize(JSCell* cell)
{
    jsCast<NativeExecutable*>(cell)->clearCode();
}

const ClassInfo ScriptExecutable::s_info = { "ScriptExecutable", &ExecutableBase::s_info, 0, 0, CREATE_METHOD_TABLE(ScriptExecutable) };

void ScriptExecutable::destroy(JSCell* cell)
{
    jsCast<ScriptExecutable*>(cell)->ScriptExecutable::~ScriptExecutable();
}

const ClassInfo EvalExecutable::s_info = { "EvalExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(EvalExecutable) };

EvalExecutable::EvalExecutable(ExecState* exec, const SourceCode& source, bool inStrictContext)
    : ScriptExecutable(exec->globalData().evalExecutableStructure.get(), exec, source, inStrictContext)
{
}

void EvalExecutable::destroy(JSCell* cell)
{
    jsCast<EvalExecutable*>(cell)->EvalExecutable::~EvalExecutable();
}

const ClassInfo ProgramExecutable::s_info = { "ProgramExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(ProgramExecutable) };

ProgramExecutable::ProgramExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec->globalData().programExecutableStructure.get(), exec, source, false)
{
}

void ProgramExecutable::destroy(JSCell* cell)
{
    jsCast<ProgramExecutable*>(cell)->ProgramExecutable::~ProgramExecutable();
}

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable", &ScriptExecutable::s_info, 0, 0, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(JSGlobalData& globalData, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, bool inStrictContext)
    : ScriptExecutable(globalData.functionExecutableStructure.get(), globalData, source, inStrictContext)
    , m_numCapturedVariables(0)
    , m_forceUsesArguments(forceUsesArguments)
    , m_parameters(parameters)
    , m_name(name)
    , m_symbolTable(0)
{
}

FunctionExecutable::FunctionExecutable(ExecState* exec, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, bool inStrictContext)
    : ScriptExecutable(exec->globalData().functionExecutableStructure.get(), exec, source, inStrictContext)
    , m_numCapturedVariables(0)
    , m_forceUsesArguments(forceUsesArguments)
    , m_parameters(parameters)
    , m_name(name)
    , m_symbolTable(0)
{
}

void FunctionExecutable::destroy(JSCell* cell)
{
    jsCast<FunctionExecutable*>(cell)->FunctionExecutable::~FunctionExecutable();
}

JSObject* EvalExecutable::compileOptimized(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    ASSERT(exec->globalData().dynamicGlobalObject);
    ASSERT(!!m_evalCodeBlock);
    JSObject* error = 0;
    if (m_evalCodeBlock->getJITType() != JITCode::topTierJIT())
        error = compileInternal(exec, scopeChainNode, JITCode::nextTierJIT(m_evalCodeBlock->getJITType()));
    ASSERT(!!m_evalCodeBlock);
    return error;
}

JSObject* EvalExecutable::compileInternal(ExecState* exec, ScopeChainNode* scopeChainNode, JITCode::JITType jitType)
{
    SamplingRegion samplingRegion(jitType == JITCode::BaselineJIT ? "Baseline Compilation (TOTAL)" : "DFG Compilation (TOTAL)");
    
#if !ENABLE(JIT)
    UNUSED_PARAM(jitType);
#endif
    JSObject* exception = 0;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    
    if (!!m_evalCodeBlock && m_evalCodeBlock->canProduceCopyWithBytecode()) {
        BytecodeDestructionBlocker blocker(m_evalCodeBlock.get());
        OwnPtr<EvalCodeBlock> newCodeBlock = adoptPtr(new EvalCodeBlock(CodeBlock::CopyParsedBlock, *m_evalCodeBlock));
        newCodeBlock->setAlternative(static_pointer_cast<CodeBlock>(m_evalCodeBlock.release()));
        m_evalCodeBlock = newCodeBlock.release();
    } else {
        if (!lexicalGlobalObject->evalEnabled())
            return throwError(exec, createEvalError(exec, "Eval is disabled"));
        RefPtr<EvalNode> evalNode = parse<EvalNode>(globalData, lexicalGlobalObject, m_source, 0, isStrictMode() ? JSParseStrict : JSParseNormal, EvalNode::isFunctionNode ? JSParseFunctionCode : JSParseProgramCode, lexicalGlobalObject->debugger(), exec, &exception);
        if (!evalNode) {
            ASSERT(exception);
            return exception;
        }
        recordParse(evalNode->features(), evalNode->hasCapturedVariables(), evalNode->lineNo(), evalNode->lastLine());
        
        JSGlobalObject* globalObject = scopeChainNode->globalObject.get();
        
        OwnPtr<CodeBlock> previousCodeBlock = m_evalCodeBlock.release();
        ASSERT((jitType == JITCode::bottomTierJIT()) == !previousCodeBlock);
        m_evalCodeBlock = adoptPtr(new EvalCodeBlock(this, globalObject, source().provider(), scopeChainNode->localDepth(), previousCodeBlock.release()));
        OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(evalNode.get(), scopeChainNode, m_evalCodeBlock->symbolTable(), m_evalCodeBlock.get(), !!m_evalCodeBlock->alternative() ? OptimizingCompilation : FirstCompilation)));
        if ((exception = generator->generate())) {
            m_evalCodeBlock = static_pointer_cast<EvalCodeBlock>(m_evalCodeBlock->releaseAlternative());
            evalNode->destroyData();
            return exception;
        }
        
        evalNode->destroyData();
        m_evalCodeBlock->copyPostParseDataFromAlternative();
    }

#if ENABLE(JIT)
    if (!jitCompileIfAppropriate(exec, m_evalCodeBlock, m_jitCodeForCall, jitType))
        return 0;
#endif

#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (!m_jitCodeForCall)
        Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_evalCodeBlock));
    else
#endif
    Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_evalCodeBlock) + m_jitCodeForCall.size());
#else
    Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_evalCodeBlock));
#endif

    return 0;
}

#if ENABLE(JIT)
void EvalExecutable::jettisonOptimizedCode(JSGlobalData& globalData)
{
    jettisonCodeBlock(globalData, m_evalCodeBlock);
    m_jitCodeForCall = m_evalCodeBlock->getJITCode();
    ASSERT(!m_jitCodeForCallWithArityCheck);
}
#endif

void EvalExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    EvalExecutable* thisObject = jsCast<EvalExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (thisObject->m_evalCodeBlock)
        thisObject->m_evalCodeBlock->visitAggregate(visitor);
}

void EvalExecutable::unlinkCalls()
{
#if ENABLE(JIT)
    if (!m_jitCodeForCall)
        return;
    ASSERT(m_evalCodeBlock);
    m_evalCodeBlock->unlinkCalls();
#endif
}

void EvalExecutable::finalize(JSCell* cell)
{
    jsCast<EvalExecutable*>(cell)->clearCode();
}

inline void EvalExecutable::clearCode()
{
    if (m_evalCodeBlock) {
        m_evalCodeBlock->clearEvalCache();
        m_evalCodeBlock.clear();
    }
    Base::clearCode();
}

JSObject* ProgramExecutable::checkSyntax(ExecState* exec)
{
    JSObject* exception = 0;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> programNode = parse<ProgramNode>(globalData, lexicalGlobalObject, m_source, 0, JSParseNormal, ProgramNode::isFunctionNode ? JSParseFunctionCode : JSParseProgramCode, lexicalGlobalObject->debugger(), exec, &exception);
    if (programNode)
        return 0;
    ASSERT(exception);
    return exception;
}

JSObject* ProgramExecutable::compileOptimized(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    ASSERT(exec->globalData().dynamicGlobalObject);
    ASSERT(!!m_programCodeBlock);
    JSObject* error = 0;
    if (m_programCodeBlock->getJITType() != JITCode::topTierJIT())
        error = compileInternal(exec, scopeChainNode, JITCode::nextTierJIT(m_programCodeBlock->getJITType()));
    ASSERT(!!m_programCodeBlock);
    return error;
}

JSObject* ProgramExecutable::compileInternal(ExecState* exec, ScopeChainNode* scopeChainNode, JITCode::JITType jitType)
{
    SamplingRegion samplingRegion(jitType == JITCode::BaselineJIT ? "Baseline Compilation (TOTAL)" : "DFG Compilation (TOTAL)");
    
#if !ENABLE(JIT)
    UNUSED_PARAM(jitType);
#endif
    JSObject* exception = 0;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    
    if (!!m_programCodeBlock && m_programCodeBlock->canProduceCopyWithBytecode()) {
        BytecodeDestructionBlocker blocker(m_programCodeBlock.get());
        OwnPtr<ProgramCodeBlock> newCodeBlock = adoptPtr(new ProgramCodeBlock(CodeBlock::CopyParsedBlock, *m_programCodeBlock));
        newCodeBlock->setAlternative(static_pointer_cast<CodeBlock>(m_programCodeBlock.release()));
        m_programCodeBlock = newCodeBlock.release();
    } else {
        RefPtr<ProgramNode> programNode = parse<ProgramNode>(globalData, lexicalGlobalObject, m_source, 0, isStrictMode() ? JSParseStrict : JSParseNormal, ProgramNode::isFunctionNode ? JSParseFunctionCode : JSParseProgramCode, lexicalGlobalObject->debugger(), exec, &exception);
        if (!programNode) {
            ASSERT(exception);
            return exception;
        }
        recordParse(programNode->features(), programNode->hasCapturedVariables(), programNode->lineNo(), programNode->lastLine());

        JSGlobalObject* globalObject = scopeChainNode->globalObject.get();
    
        OwnPtr<CodeBlock> previousCodeBlock = m_programCodeBlock.release();
        ASSERT((jitType == JITCode::bottomTierJIT()) == !previousCodeBlock);
        m_programCodeBlock = adoptPtr(new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider(), previousCodeBlock.release()));
        OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(programNode.get(), scopeChainNode, &globalObject->symbolTable(), m_programCodeBlock.get(), !!m_programCodeBlock->alternative() ? OptimizingCompilation : FirstCompilation)));
        if ((exception = generator->generate())) {
            m_programCodeBlock = static_pointer_cast<ProgramCodeBlock>(m_programCodeBlock->releaseAlternative());
            programNode->destroyData();
            return exception;
        }

        programNode->destroyData();
        m_programCodeBlock->copyPostParseDataFromAlternative();
    }

#if ENABLE(JIT)
    if (!jitCompileIfAppropriate(exec, m_programCodeBlock, m_jitCodeForCall, jitType))
        return 0;
#endif

#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (!m_jitCodeForCall)
        Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_programCodeBlock));
    else
#endif
        Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_programCodeBlock) + m_jitCodeForCall.size());
#else
    Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_programCodeBlock));
#endif

    return 0;
}

#if ENABLE(JIT)
void ProgramExecutable::jettisonOptimizedCode(JSGlobalData& globalData)
{
    jettisonCodeBlock(globalData, m_programCodeBlock);
    m_jitCodeForCall = m_programCodeBlock->getJITCode();
    ASSERT(!m_jitCodeForCallWithArityCheck);
}
#endif

void ProgramExecutable::unlinkCalls()
{
#if ENABLE(JIT)
    if (!m_jitCodeForCall)
        return;
    ASSERT(m_programCodeBlock);
    m_programCodeBlock->unlinkCalls();
#endif
}

void ProgramExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ProgramExecutable* thisObject = jsCast<ProgramExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (thisObject->m_programCodeBlock)
        thisObject->m_programCodeBlock->visitAggregate(visitor);
}

void ProgramExecutable::finalize(JSCell* cell)
{
    jsCast<ProgramExecutable*>(cell)->clearCode();
}

inline void ProgramExecutable::clearCode()
{
    if (m_programCodeBlock) {
        m_programCodeBlock->clearEvalCache();
        m_programCodeBlock.clear();
    }
    Base::clearCode();
}

FunctionCodeBlock* FunctionExecutable::baselineCodeBlockFor(CodeSpecializationKind kind)
{
    FunctionCodeBlock* result;
    if (kind == CodeForCall)
        result = m_codeBlockForCall.get();
    else {
        ASSERT(kind == CodeForConstruct);
        result = m_codeBlockForConstruct.get();
    }
    if (!result)
        return 0;
    while (result->alternative())
        result = static_cast<FunctionCodeBlock*>(result->alternative());
    ASSERT(result);
    ASSERT(result->getJITType() == JITCode::BaselineJIT);
    return result;
}

JSObject* FunctionExecutable::compileOptimizedForCall(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    ASSERT(exec->globalData().dynamicGlobalObject);
    ASSERT(!!m_codeBlockForCall);
    JSObject* error = 0;
    if (m_codeBlockForCall->getJITType() != JITCode::topTierJIT())
        error = compileForCallInternal(exec, scopeChainNode, JITCode::nextTierJIT(m_codeBlockForCall->getJITType()));
    ASSERT(!!m_codeBlockForCall);
    return error;
}

JSObject* FunctionExecutable::compileOptimizedForConstruct(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    ASSERT(exec->globalData().dynamicGlobalObject);
    ASSERT(!!m_codeBlockForConstruct);
    JSObject* error = 0;
    if (m_codeBlockForConstruct->getJITType() != JITCode::topTierJIT())
        error = compileForConstructInternal(exec, scopeChainNode, JITCode::nextTierJIT(m_codeBlockForConstruct->getJITType()));
    ASSERT(!!m_codeBlockForConstruct);
    return error;
}

FunctionCodeBlock* FunctionExecutable::codeBlockWithBytecodeFor(CodeSpecializationKind kind)
{
    FunctionCodeBlock* codeBlock = baselineCodeBlockFor(kind);
    if (codeBlock->canProduceCopyWithBytecode())
        return codeBlock;
    return 0;
}

PassOwnPtr<FunctionCodeBlock> FunctionExecutable::produceCodeBlockFor(ScopeChainNode* scopeChainNode, CompilationKind compilationKind, CodeSpecializationKind specializationKind, JSObject*& exception)
{
    if (!!codeBlockFor(specializationKind) && codeBlockFor(specializationKind)->canProduceCopyWithBytecode()) {
        BytecodeDestructionBlocker blocker(codeBlockFor(specializationKind).get());
        return adoptPtr(new FunctionCodeBlock(CodeBlock::CopyParsedBlock, *codeBlockFor(specializationKind)));
    }
    
    exception = 0;
    JSGlobalData* globalData = scopeChainNode->globalData;
    JSGlobalObject* globalObject = scopeChainNode->globalObject.get();
    RefPtr<FunctionBodyNode> body = parse<FunctionBodyNode>(globalData, globalObject, m_source, m_parameters.get(), isStrictMode() ? JSParseStrict : JSParseNormal, FunctionBodyNode::isFunctionNode ? JSParseFunctionCode : JSParseProgramCode, 0, 0, &exception);

    if (!body) {
        ASSERT(exception);
        return nullptr;
    }
    if (m_forceUsesArguments)
        body->setUsesArguments();
    body->finishParsing(m_parameters, m_name);
    recordParse(body->features(), body->hasCapturedVariables(), body->lineNo(), body->lastLine());

    OwnPtr<FunctionCodeBlock> result;
    ASSERT((compilationKind == FirstCompilation) == !codeBlockFor(specializationKind));
    result = adoptPtr(new FunctionCodeBlock(this, FunctionCode, globalObject, source().provider(), source().startOffset(), specializationKind == CodeForConstruct));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), scopeChainNode, result->symbolTable(), result.get(), compilationKind)));
    exception = generator->generate();
    body->destroyData();
    if (exception)
        return nullptr;

    result->copyPostParseDataFrom(codeBlockFor(specializationKind).get());
    return result.release();
}

JSObject* FunctionExecutable::compileForCallInternal(ExecState* exec, ScopeChainNode* scopeChainNode, JITCode::JITType jitType)
{
    SamplingRegion samplingRegion(jitType == JITCode::BaselineJIT ? "Baseline Compilation (TOTAL)" : "DFG Compilation (TOTAL)");
    
#if !ENABLE(JIT)
    UNUSED_PARAM(exec);
    UNUSED_PARAM(jitType);
    UNUSED_PARAM(exec);
#endif
    ASSERT((jitType == JITCode::bottomTierJIT()) == !m_codeBlockForCall);
    JSObject* exception;
    OwnPtr<FunctionCodeBlock> newCodeBlock = produceCodeBlockFor(scopeChainNode, !!m_codeBlockForCall ? OptimizingCompilation : FirstCompilation, CodeForCall, exception);
    if (!newCodeBlock)
        return exception;

    newCodeBlock->setAlternative(static_pointer_cast<CodeBlock>(m_codeBlockForCall.release()));
    m_codeBlockForCall = newCodeBlock.release();
    
    m_numParametersForCall = m_codeBlockForCall->numParameters();
    ASSERT(m_numParametersForCall);
    m_numCapturedVariables = m_codeBlockForCall->m_numCapturedVars;
    m_symbolTable = m_codeBlockForCall->sharedSymbolTable();

#if ENABLE(JIT)
    if (!jitCompileFunctionIfAppropriate(exec, m_codeBlockForCall, m_jitCodeForCall, m_jitCodeForCallWithArityCheck, m_symbolTable, jitType))
        return 0;
#endif

#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (!m_jitCodeForCall)
        Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_codeBlockForCall));
    else
#endif
        Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_codeBlockForCall) + m_jitCodeForCall.size());
#else
    Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_codeBlockForCall));
#endif

    return 0;
}

JSObject* FunctionExecutable::compileForConstructInternal(ExecState* exec, ScopeChainNode* scopeChainNode, JITCode::JITType jitType)
{
    SamplingRegion samplingRegion(jitType == JITCode::BaselineJIT ? "Baseline Compilation (TOTAL)" : "DFG Compilation (TOTAL)");
    
#if !ENABLE(JIT)
    UNUSED_PARAM(jitType);
    UNUSED_PARAM(exec);
#endif
    
    ASSERT((jitType == JITCode::bottomTierJIT()) == !m_codeBlockForConstruct);
    JSObject* exception;
    OwnPtr<FunctionCodeBlock> newCodeBlock = produceCodeBlockFor(scopeChainNode, !!m_codeBlockForConstruct ? OptimizingCompilation : FirstCompilation, CodeForConstruct, exception);
    if (!newCodeBlock)
        return exception;

    newCodeBlock->setAlternative(static_pointer_cast<CodeBlock>(m_codeBlockForConstruct.release()));
    m_codeBlockForConstruct = newCodeBlock.release();
    
    m_numParametersForConstruct = m_codeBlockForConstruct->numParameters();
    ASSERT(m_numParametersForConstruct);
    m_numCapturedVariables = m_codeBlockForConstruct->m_numCapturedVars;
    m_symbolTable = m_codeBlockForConstruct->sharedSymbolTable();

#if ENABLE(JIT)
    if (!jitCompileFunctionIfAppropriate(exec, m_codeBlockForConstruct, m_jitCodeForConstruct, m_jitCodeForConstructWithArityCheck, m_symbolTable, jitType))
        return 0;
#endif

#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (!m_jitCodeForConstruct)
        Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_codeBlockForConstruct));
    else
#endif
    Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_codeBlockForConstruct) + m_jitCodeForConstruct.size());
#else
    Heap::heap(this)->reportExtraMemoryCost(sizeof(*m_codeBlockForConstruct));
#endif

    return 0;
}

#if ENABLE(JIT)
void FunctionExecutable::jettisonOptimizedCodeForCall(JSGlobalData& globalData)
{
    jettisonCodeBlock(globalData, m_codeBlockForCall);
    m_jitCodeForCall = m_codeBlockForCall->getJITCode();
    m_jitCodeForCallWithArityCheck = m_codeBlockForCall->getJITCodeWithArityCheck();
}

void FunctionExecutable::jettisonOptimizedCodeForConstruct(JSGlobalData& globalData)
{
    jettisonCodeBlock(globalData, m_codeBlockForConstruct);
    m_jitCodeForConstruct = m_codeBlockForConstruct->getJITCode();
    m_jitCodeForConstructWithArityCheck = m_codeBlockForConstruct->getJITCodeWithArityCheck();
}
#endif

void FunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    FunctionExecutable* thisObject = jsCast<FunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (thisObject->m_nameValue)
        visitor.append(&thisObject->m_nameValue);
    if (thisObject->m_codeBlockForCall)
        thisObject->m_codeBlockForCall->visitAggregate(visitor);
    if (thisObject->m_codeBlockForConstruct)
        thisObject->m_codeBlockForConstruct->visitAggregate(visitor);
}

void FunctionExecutable::discardCode()
{
#if ENABLE(JIT)
    // These first two checks are to handle the rare case where
    // we are trying to evict code for a function during its
    // codegen.
    if (!m_jitCodeForCall && m_codeBlockForCall)
        return;
    if (!m_jitCodeForConstruct && m_codeBlockForConstruct)
        return;
#endif
    clearCode();
}

void FunctionExecutable::finalize(JSCell* cell)
{
    jsCast<FunctionExecutable*>(cell)->clearCode();
}

inline void FunctionExecutable::clearCode()
{
    if (m_codeBlockForCall) {
        m_codeBlockForCall->clearEvalCache();
        m_codeBlockForCall.clear();
    }
    if (m_codeBlockForConstruct) {
        m_codeBlockForConstruct->clearEvalCache();
        m_codeBlockForConstruct.clear();
    }
    Base::clearCode();
}

void FunctionExecutable::unlinkCalls()
{
#if ENABLE(JIT)
    if (!!m_jitCodeForCall) {
        ASSERT(m_codeBlockForCall);
        m_codeBlockForCall->unlinkCalls();
    }
    if (!!m_jitCodeForConstruct) {
        ASSERT(m_codeBlockForConstruct);
        m_codeBlockForConstruct->unlinkCalls();
    }
#endif
}

FunctionExecutable* FunctionExecutable::fromGlobalCode(const Identifier& functionName, ExecState* exec, Debugger* debugger, const SourceCode& source, JSObject** exception)
{
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> program = parse<ProgramNode>(&exec->globalData(), lexicalGlobalObject, source, 0, JSParseNormal, ProgramNode::isFunctionNode ? JSParseFunctionCode : JSParseProgramCode, debugger, exec, exception);
    if (!program) {
        ASSERT(*exception);
        return 0;
    }

    // Uses of this function that would not result in a single function expression are invalid.
    StatementNode* exprStatement = program->singleStatement();
    ASSERT(exprStatement);
    ASSERT(exprStatement->isExprStatement());
    ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
    ASSERT(funcExpr);
    ASSERT(funcExpr->isFuncExprNode());
    FunctionBodyNode* body = static_cast<FuncExprNode*>(funcExpr)->body();
    ASSERT(body);

    return FunctionExecutable::create(exec->globalData(), functionName, body->source(), body->usesArguments(), body->parameters(), body->isStrictMode(), body->lineNo(), body->lastLine());
}

UString FunctionExecutable::paramString() const
{
    FunctionParameters& parameters = *m_parameters;
    UStringBuilder builder;
    for (size_t pos = 0; pos < parameters.size(); ++pos) {
        if (!builder.isEmpty())
            builder.append(", ");
        builder.append(parameters[pos].ustring());
    }
    return builder.toUString();
}

}
