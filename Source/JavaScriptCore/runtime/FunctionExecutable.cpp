/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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
#include "FunctionCodeBlock.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "LLIntEntrypoint.h"
#include "Parser.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable", &ScriptExecutable::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, unsigned lastLine, unsigned endColumn, Intrinsic intrinsic)
    : ScriptExecutable(vm.functionExecutableStructure.get(), vm, source, unlinkedExecutable->isInStrictContext(), unlinkedExecutable->derivedContextType(), false, EvalContextType::None, intrinsic)
    , m_unlinkedExecutable(vm, this, unlinkedExecutable)
{
    RELEASE_ASSERT(!source.isNull());
    ASSERT(source.length());
    m_lastLine = lastLine;
    ASSERT(endColumn != UINT_MAX);
    m_endColumn = endColumn;
    if (VM::canUseJIT())
        new (&m_singletonFunction) WriteBarrier<InferredValue>();
    else
        m_singletonFunctionState = ClearWatchpoint;
}

void FunctionExecutable::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    if (VM::canUseJIT())
        m_singletonFunction.set(vm, this, InferredValue::create(vm));
}

void FunctionExecutable::destroy(JSCell* cell)
{
    static_cast<FunctionExecutable*>(cell)->FunctionExecutable::~FunctionExecutable();
}

FunctionCodeBlock* FunctionExecutable::baselineCodeBlockFor(CodeSpecializationKind kind)
{
    ExecutableToCodeBlockEdge* edge;
    if (kind == CodeForCall)
        edge = m_codeBlockForCall.get();
    else {
        RELEASE_ASSERT(kind == CodeForConstruct);
        edge = m_codeBlockForConstruct.get();
    }
    if (!edge)
        return 0;
    return static_cast<FunctionCodeBlock*>(edge->codeBlock()->baselineAlternative());
}

void FunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    FunctionExecutable* thisObject = jsCast<FunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_codeBlockForCall);
    visitor.append(thisObject->m_codeBlockForConstruct);
    visitor.append(thisObject->m_unlinkedExecutable);
    if (VM::canUseJIT())
        visitor.append(thisObject->m_singletonFunction);
    visitor.append(thisObject->m_cachedPolyProtoStructure);
}

FunctionExecutable* FunctionExecutable::fromGlobalCode(
    const Identifier& name, ExecState& exec, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber, Optional<int> functionConstructorParametersEndPosition)
{
    UnlinkedFunctionExecutable* unlinkedExecutable = 
        UnlinkedFunctionExecutable::fromGlobalCode(
            name, exec, source, exception, overrideLineNumber, functionConstructorParametersEndPosition);
    if (!unlinkedExecutable)
        return nullptr;

    return unlinkedExecutable->link(exec.vm(), source, overrideLineNumber);
}

FunctionExecutable::RareData& FunctionExecutable::ensureRareDataSlow()
{
    ASSERT(!m_rareData);
    m_rareData = std::make_unique<RareData>();
    m_rareData->m_parametersStartOffset = m_unlinkedExecutable->parametersStartOffset();
    m_rareData->m_typeProfilingStartOffset = m_unlinkedExecutable->typeProfilingStartOffset();
    m_rareData->m_typeProfilingEndOffset = m_unlinkedExecutable->typeProfilingEndOffset();
    return *m_rareData;
}

} // namespace JSC
