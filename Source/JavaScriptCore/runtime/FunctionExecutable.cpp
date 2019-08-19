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
#include "FunctionOverrides.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "LLIntEntrypoint.h"
#include "Parser.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable", &ScriptExecutable::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, Intrinsic intrinsic)
    : ScriptExecutable(vm.functionExecutableStructure.get(), vm, source, unlinkedExecutable->isInStrictContext(), unlinkedExecutable->derivedContextType(), false, EvalContextType::None, intrinsic)
    , m_unlinkedExecutable(vm, this, unlinkedExecutable)
{
    RELEASE_ASSERT(!source.isNull());
    ASSERT(source.length());
}

void FunctionExecutable::finishCreation(VM& vm, ScriptExecutable* topLevelExecutable)
{
    Base::finishCreation(vm);
    m_topLevelExecutable.set(vm, this, topLevelExecutable ? topLevelExecutable : this);
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
    visitor.append(thisObject->m_topLevelExecutable);
    visitor.append(thisObject->m_codeBlockForCall);
    visitor.append(thisObject->m_codeBlockForConstruct);
    visitor.append(thisObject->m_unlinkedExecutable);
    if (RareData* rareData = thisObject->m_rareData.get()) {
        visitor.append(rareData->m_cachedPolyProtoStructure);
        if (TemplateObjectMap* map = rareData->m_templateObjectMap.get()) {
            auto locker = holdLock(thisObject->cellLock());
            for (auto& entry : *map)
                visitor.append(entry.value);
        }
    }
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

    return unlinkedExecutable->link(exec.vm(), nullptr, source, overrideLineNumber);
}

FunctionExecutable::RareData& FunctionExecutable::ensureRareDataSlow()
{
    ASSERT(!m_rareData);
    auto rareData = makeUnique<RareData>();
    rareData->m_lineCount = lineCount();
    rareData->m_endColumn = endColumn();
    rareData->m_parametersStartOffset = parametersStartOffset();
    rareData->m_typeProfilingStartOffset = typeProfilingStartOffset();
    rareData->m_typeProfilingEndOffset = typeProfilingEndOffset();
    WTF::storeStoreFence();
    m_rareData = WTFMove(rareData);
    return *m_rareData;
}

void FunctionExecutable::overrideInfo(const FunctionOverrideInfo& overrideInfo)
{
    auto& rareData = ensureRareData();
    m_source = overrideInfo.sourceCode;
    rareData.m_lineCount = overrideInfo.lineCount;
    rareData.m_endColumn = overrideInfo.endColumn;
    rareData.m_parametersStartOffset = overrideInfo.parametersStartOffset;
    rareData.m_typeProfilingStartOffset = overrideInfo.typeProfilingStartOffset;
    rareData.m_typeProfilingEndOffset = overrideInfo.typeProfilingEndOffset;
}

auto FunctionExecutable::ensureTemplateObjectMap(VM&) -> TemplateObjectMap&
{
    RareData& rareData = ensureRareData();
    return ensureTemplateObjectMapImpl(rareData.m_templateObjectMap);
}

} // namespace JSC
