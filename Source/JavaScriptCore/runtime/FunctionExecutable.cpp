/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
#include "FunctionExecutable.h"

#include "CodeBlock.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutableInlines.h"
#include "FunctionOverrides.h"
#include "IsoCellSetInlines.h"
#include "JSCJSValueInlines.h"

namespace JSC {

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable"_s, &ScriptExecutable::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, Intrinsic intrinsic, bool isInsideOrdinaryFunction)
    : ScriptExecutable(vm.functionExecutableStructure.get(), vm, source, unlinkedExecutable->lexicalScopeFeatures(), unlinkedExecutable->derivedContextType(), false, isInsideOrdinaryFunction || !unlinkedExecutable->isArrowFunction(), EvalContextType::None, intrinsic)
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
    CodeBlock* codeBlock = nullptr;
    if (kind == CodeForCall)
        codeBlock = codeBlockForCall();
    else {
        RELEASE_ASSERT(kind == CodeForConstruct);
        codeBlock = codeBlockForConstruct();
    }
    if (!codeBlock)
        return nullptr;
    return static_cast<FunctionCodeBlock*>(codeBlock->baselineAlternative());
}

template<typename Visitor>
static inline bool shouldKeepInConstraintSet(Visitor& visitor, CodeBlock* codeBlockForCall, CodeBlock* codeBlockForConstruct)
{
    // If either CodeBlock is not marked yet, we will run output-constraints.
    return (codeBlockForCall && !visitor.isMarked(codeBlockForCall)) || (codeBlockForConstruct && !visitor.isMarked(codeBlockForConstruct));
}

template<typename Visitor>
void FunctionExecutable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    VM& vm = visitor.vm();
    FunctionExecutable* thisObject = jsCast<FunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_topLevelExecutable);
    visitor.append(thisObject->m_unlinkedExecutable);
    if (RareData* rareData = thisObject->m_rareData.get()) {
        visitor.append(rareData->m_cachedPolyProtoStructureID);
        visitor.append(rareData->m_asString);
        if (TemplateObjectMap* map = rareData->m_templateObjectMap.get()) {
            Locker locker { thisObject->cellLock() };
            for (auto& entry : *map)
                visitor.append(entry.value);
        }
    }

    // Since FunctionExecutable's finalizer always needs to be run, we do not track FunctionExecutable via finalizerSet.
    auto* codeBlockForCall = thisObject->m_codeBlockForCall.get();
    if (codeBlockForCall)
        visitCodeBlockEdge(visitor, codeBlockForCall);
    auto* codeBlockForConstruct = thisObject->m_codeBlockForConstruct.get();
    if (codeBlockForConstruct)
        visitCodeBlockEdge(visitor, codeBlockForConstruct);

    if (shouldKeepInConstraintSet(visitor, codeBlockForCall, codeBlockForConstruct))
        vm.heap.functionExecutableSpaceAndSet.outputConstraintsSet.add(thisObject);
}

DEFINE_VISIT_CHILDREN(FunctionExecutable);

template<typename Visitor>
void FunctionExecutable::visitOutputConstraintsImpl(JSCell* cell, Visitor& visitor)
{
    VM& vm = visitor.vm();
    auto* executable = jsCast<FunctionExecutable*>(cell);
    auto* codeBlockForCall = executable->m_codeBlockForCall.get();
    if (codeBlockForCall) {
        if (!visitor.isMarked(codeBlockForCall))
            runConstraint(NoLockingNecessary, visitor, codeBlockForCall);
    }
    auto* codeBlockForConstruct = executable->codeBlockForConstruct();
    if (codeBlockForConstruct) {
        if (!visitor.isMarked(codeBlockForConstruct))
            runConstraint(NoLockingNecessary, visitor, codeBlockForConstruct);
    }

    if (!shouldKeepInConstraintSet(visitor, codeBlockForCall, codeBlockForConstruct))
        vm.heap.functionExecutableSpaceAndSet.outputConstraintsSet.remove(executable);
}

DEFINE_VISIT_OUTPUT_CONSTRAINTS(FunctionExecutable);

FunctionExecutable* FunctionExecutable::fromGlobalCode(
    const Identifier& name, JSGlobalObject* globalObject, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber, std::optional<int> functionConstructorParametersEndPosition)
{
    UnlinkedFunctionExecutable* unlinkedExecutable = 
        UnlinkedFunctionExecutable::fromGlobalCode(
            name, globalObject, source, exception, overrideLineNumber, functionConstructorParametersEndPosition);
    if (!unlinkedExecutable)
        return nullptr;

    return unlinkedExecutable->link(globalObject->vm(), nullptr, source, overrideLineNumber);
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

JSString* FunctionExecutable::toStringSlow(JSGlobalObject* globalObject)
{
    VM& vm = getVM(globalObject);
    ASSERT(m_rareData && !m_rareData->m_asString);

    auto throwScope = DECLARE_THROW_SCOPE(vm);

    const auto& cache = [&](JSString* asString) {
        WTF::storeStoreFence();
        m_rareData->m_asString.set(vm, this, asString);
        return asString;
    };

    const auto& cacheIfNoException = [&](JSValue value) -> JSString* {
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        return cache(::JSC::asString(value));
    };

    if (isBuiltinFunction())
        return cacheIfNoException(jsMakeNontrivialString(globalObject, "function ", name().string(), "() {\n    [native code]\n}"));

    if (isClass())
        return cache(jsString(vm, classSource().view()));

    ASCIILiteral functionHeader = ""_s;
    switch (parseMode()) {
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
        functionHeader = "function* "_s;
        break;

    case SourceParseMode::NormalFunctionMode:
    case SourceParseMode::GetterMode:
    case SourceParseMode::SetterMode:
    case SourceParseMode::MethodMode:
    case SourceParseMode::ProgramMode:
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
    case SourceParseMode::GeneratorBodyMode:
    case SourceParseMode::AsyncGeneratorBodyMode:
    case SourceParseMode::AsyncFunctionBodyMode:
    case SourceParseMode::AsyncArrowFunctionBodyMode:
        functionHeader = "function "_s;
        break;

    case SourceParseMode::ArrowFunctionMode:
    case SourceParseMode::ClassFieldInitializerMode:
    case SourceParseMode::ClassStaticBlockMode:
        break;

    case SourceParseMode::AsyncFunctionMode:
    case SourceParseMode::AsyncMethodMode:
        functionHeader = "async function "_s;
        break;

    case SourceParseMode::AsyncArrowFunctionMode:
        functionHeader = "async "_s;
        break;

    case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
        functionHeader = "async function* "_s;
        break;
    }

    StringView src = source().provider()->getRange(
        parametersStartOffset(),
        parametersStartOffset() + source().length());

    auto name = this->name().string();
    if (name == vm.propertyNames->starDefaultPrivateName.string())
        name = emptyAtom();
    return cacheIfNoException(jsMakeNontrivialString(globalObject, functionHeader, WTFMove(name), src));
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
