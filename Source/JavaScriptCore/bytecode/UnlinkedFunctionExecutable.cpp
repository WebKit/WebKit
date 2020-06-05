/*
 * Copyright (C) 2012-2019 Apple Inc. All Rights Reserved.
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
#include "UnlinkedFunctionExecutable.h"

#include "BuiltinExecutables.h"
#include "BytecodeGenerator.h"
#include "CachedTypes.h"
#include "ClassInfo.h"
#include "CodeCache.h"
#include "Debugger.h"
#include "ExecutableInfo.h"
#include "FunctionOverrides.h"
#include "IsoCellSetInlines.h"
#include "Parser.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "UnlinkedFunctionCodeBlock.h"
#include <wtf/Optional.h>

namespace JSC {

static_assert(sizeof(UnlinkedFunctionExecutable) <= 128, "UnlinkedFunctionExecutable should fit in a 128-byte cell to keep allocated blocks count to only one after initializing JSGlobalObject.");

const ClassInfo UnlinkedFunctionExecutable::s_info = { "UnlinkedFunctionExecutable", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(UnlinkedFunctionExecutable) };

static UnlinkedFunctionCodeBlock* generateUnlinkedFunctionCodeBlock(
    VM& vm, UnlinkedFunctionExecutable* executable, const SourceCode& source,
    CodeSpecializationKind kind, OptionSet<CodeGenerationMode> codeGenerationMode,
    UnlinkedFunctionKind functionKind, ParserError& error, SourceParseMode parseMode)
{
    JSParserBuiltinMode builtinMode = executable->isBuiltinFunction() ? JSParserBuiltinMode::Builtin : JSParserBuiltinMode::NotBuiltin;
    JSParserStrictMode strictMode = executable->isInStrictContext() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    JSParserScriptMode scriptMode = executable->scriptMode();
    ASSERT(isFunctionParseMode(executable->parseMode()));
    Vector<JSTextPosition>* instanceFieldLocations = executable->instanceFieldLocations();
    std::unique_ptr<FunctionNode> function = parse<FunctionNode>(
        vm, source, executable->name(), builtinMode, strictMode, scriptMode, executable->parseMode(), executable->superBinding(), error, nullptr, ConstructorKind::None, DerivedContextType::None, EvalContextType::None, nullptr, nullptr, instanceFieldLocations);

    if (!function) {
        ASSERT(error.isValid());
        return nullptr;
    }

    function->finishParsing(executable->name(), executable->functionMode());
    executable->recordParse(function->features(), function->hasCapturedVariables());

    bool isClassContext = executable->superBinding() == SuperBinding::Needed;

    UnlinkedFunctionCodeBlock* result = UnlinkedFunctionCodeBlock::create(vm, FunctionCode, ExecutableInfo(function->usesEval(), kind == CodeForConstruct, functionKind == UnlinkedBuiltinFunction, executable->constructorKind(), scriptMode, executable->superBinding(), parseMode, executable->derivedContextType(), executable->needsClassFieldInitializer(), false, isClassContext, EvalContextType::FunctionEvalContext), codeGenerationMode);

    VariableEnvironment parentScopeTDZVariables = executable->parentScopeTDZVariables();
    ECMAMode ecmaMode = executable->isInStrictContext() ? ECMAMode::strict() : ECMAMode::sloppy();
    error = BytecodeGenerator::generate(vm, function.get(), source, result, codeGenerationMode, &parentScopeTDZVariables, ecmaMode);

    if (error.isValid())
        return nullptr;
    vm.codeCache()->updateCache(executable, source, kind, result);
    return result;
}

UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(VM& vm, Structure* structure, const SourceCode& parentSource, FunctionMetadataNode* node, UnlinkedFunctionKind kind, ConstructAbility constructAbility, JSParserScriptMode scriptMode, Optional<CompactVariableMap::Handle> parentScopeTDZVariables, DerivedContextType derivedContextType, NeedsClassFieldInitializer needsClassFieldInitializer, bool isBuiltinDefaultClassConstructor)
    : Base(vm, structure)
    , m_firstLineOffset(node->firstLine() - parentSource.firstLine().oneBasedInt())
    , m_isInStrictContext(node->isInStrictContext())
    , m_lineCount(node->lastLine() - node->firstLine())
    , m_hasCapturedVariables(false)
    , m_unlinkedFunctionNameStart(node->functionNameStart() - parentSource.startOffset())
    , m_isBuiltinFunction(kind == UnlinkedBuiltinFunction)
    , m_unlinkedBodyStartColumn(node->startColumn())
    , m_isBuiltinDefaultClassConstructor(isBuiltinDefaultClassConstructor)
    , m_unlinkedBodyEndColumn(m_lineCount ? node->endColumn() : node->endColumn() - node->startColumn())
    , m_constructAbility(static_cast<unsigned>(constructAbility))
    , m_startOffset(node->source().startOffset() - parentSource.startOffset())
    , m_scriptMode(static_cast<unsigned>(scriptMode))
    , m_sourceLength(node->source().length())
    , m_superBinding(static_cast<unsigned>(node->superBinding()))
    , m_parametersStartOffset(node->parametersStart())
    , m_isCached(false)
    , m_typeProfilingStartOffset(node->functionKeywordStart())
    , m_typeProfilingEndOffset(node->startStartOffset() + node->source().length() - 1)
    , m_parameterCount(node->parameterCount())
    , m_features(0)
    , m_sourceParseMode(node->parseMode())
    , m_constructorKind(static_cast<unsigned>(node->constructorKind()))
    , m_functionMode(static_cast<unsigned>(node->functionMode()))
    , m_derivedContextType(static_cast<unsigned>(derivedContextType))
    , m_isGeneratedFromCache(false)
    , m_needsClassFieldInitializer(static_cast<unsigned>(needsClassFieldInitializer))
    , m_unlinkedCodeBlockForCall()
    , m_unlinkedCodeBlockForConstruct()
    , m_name(node->ident())
    , m_ecmaName(node->ecmaName())
{
    // Make sure these bitfields are adequately wide.
    ASSERT(m_constructAbility == static_cast<unsigned>(constructAbility));
    ASSERT(m_constructorKind == static_cast<unsigned>(node->constructorKind()));
    ASSERT(m_functionMode == static_cast<unsigned>(node->functionMode()));
    ASSERT(m_scriptMode == static_cast<unsigned>(scriptMode));
    ASSERT(m_superBinding == static_cast<unsigned>(node->superBinding()));
    ASSERT(m_derivedContextType == static_cast<unsigned>(derivedContextType));
    ASSERT(!(m_isBuiltinDefaultClassConstructor && constructorKind() == ConstructorKind::None));
    ASSERT(!m_needsClassFieldInitializer || (isClassConstructorFunction() || derivedContextType == DerivedContextType::DerivedConstructorContext));
    if (!node->classSource().isNull())
        setClassSource(node->classSource());
    if (parentScopeTDZVariables)
        ensureRareData().m_parentScopeTDZVariables = WTFMove(*parentScopeTDZVariables);
}

UnlinkedFunctionExecutable::~UnlinkedFunctionExecutable()
{
    if (m_isCached)
        m_decoder.~RefPtr();
}

void UnlinkedFunctionExecutable::destroy(JSCell* cell)
{
    static_cast<UnlinkedFunctionExecutable*>(cell)->~UnlinkedFunctionExecutable();
}

void UnlinkedFunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedFunctionExecutable* thisObject = jsCast<UnlinkedFunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    if (thisObject->codeBlockEdgeMayBeWeak()) {
        auto markIfProfitable = [&] (WriteBarrier<UnlinkedFunctionCodeBlock>& unlinkedCodeBlock) {
            if (!unlinkedCodeBlock)
                return;
            if (unlinkedCodeBlock->didOptimize() == TriState::True)
                visitor.append(unlinkedCodeBlock);
            else if (unlinkedCodeBlock->age() < UnlinkedCodeBlock::maxAge)
                visitor.append(unlinkedCodeBlock);
        };
        markIfProfitable(thisObject->m_unlinkedCodeBlockForCall);
        markIfProfitable(thisObject->m_unlinkedCodeBlockForConstruct);
    } else if (!thisObject->m_isCached) {
        visitor.append(thisObject->m_unlinkedCodeBlockForCall);
        visitor.append(thisObject->m_unlinkedCodeBlockForConstruct);
    }
}

SourceCode UnlinkedFunctionExecutable::linkedSourceCode(const SourceCode& passedParentSource) const
{
    const SourceCode& parentSource = !m_isBuiltinDefaultClassConstructor ? passedParentSource : BuiltinExecutables::defaultConstructorSourceCode(constructorKind());
    unsigned startColumn = linkedStartColumn(parentSource.startColumn().oneBasedInt());
    unsigned startOffset = parentSource.startOffset() + m_startOffset;
    unsigned firstLine = parentSource.firstLine().oneBasedInt() + m_firstLineOffset;
    return SourceCode(parentSource.provider(), startOffset, startOffset + m_sourceLength, firstLine, startColumn);
}

FunctionExecutable* UnlinkedFunctionExecutable::link(VM& vm, ScriptExecutable* topLevelExecutable, const SourceCode& passedParentSource, Optional<int> overrideLineNumber, Intrinsic intrinsic, bool isInsideOrdinaryFunction)
{
    SourceCode source = linkedSourceCode(passedParentSource);
    FunctionOverrides::OverrideInfo overrideInfo;
    bool hasFunctionOverride = false;
    if (UNLIKELY(Options::functionOverrides()))
        hasFunctionOverride = FunctionOverrides::initializeOverrideFor(source, overrideInfo);

    FunctionExecutable* result = FunctionExecutable::create(vm, topLevelExecutable, source, this, intrinsic, isInsideOrdinaryFunction);
    if (overrideLineNumber)
        result->setOverrideLineNumber(*overrideLineNumber);

    if (UNLIKELY(hasFunctionOverride))
        result->overrideInfo(overrideInfo);

    return result;
}

UnlinkedFunctionExecutable* UnlinkedFunctionExecutable::fromGlobalCode(
    const Identifier& name, JSGlobalObject* globalObject, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber, Optional<int> functionConstructorParametersEndPosition)
{
    ParserError error;
    VM& vm = globalObject->vm();
    CodeCache* codeCache = vm.codeCache();
    OptionSet<CodeGenerationMode> codeGenerationMode = globalObject->defaultCodeGenerationMode();
    UnlinkedFunctionExecutable* executable = codeCache->getUnlinkedGlobalFunctionExecutable(vm, name, source, codeGenerationMode, functionConstructorParametersEndPosition, error);

    if (globalObject->hasDebugger())
        globalObject->debugger()->sourceParsed(globalObject, source.provider(), error.line(), error.message());

    if (error.isValid()) {
        exception = error.toErrorObject(globalObject, source, overrideLineNumber);
        return nullptr;
    }

    return executable;
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutable::unlinkedCodeBlockFor(
    VM& vm, const SourceCode& source, CodeSpecializationKind specializationKind, 
    OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, SourceParseMode parseMode)
{
    if (m_isCached)
        decodeCachedCodeBlocks(vm);
    switch (specializationKind) {
    case CodeForCall:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_unlinkedCodeBlockForCall.get())
            return codeBlock;
        break;
    case CodeForConstruct:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_unlinkedCodeBlockForConstruct.get())
            return codeBlock;
        break;
    }

    UnlinkedFunctionCodeBlock* result = generateUnlinkedFunctionCodeBlock(
        vm, this, source, specializationKind, codeGenerationMode, 
        isBuiltinFunction() ? UnlinkedBuiltinFunction : UnlinkedNormalFunction, 
        error, parseMode);
    
    if (error.isValid())
        return nullptr;

    switch (specializationKind) {
    case CodeForCall:
        m_unlinkedCodeBlockForCall.set(vm, this, result);
        break;
    case CodeForConstruct:
        m_unlinkedCodeBlockForConstruct.set(vm, this, result);
        break;
    }
    vm.unlinkedFunctionExecutableSpace.set.add(this);
    return result;
}

void UnlinkedFunctionExecutable::decodeCachedCodeBlocks(VM& vm)
{
    ASSERT(m_isCached);
    ASSERT(m_decoder);
    ASSERT(m_cachedCodeBlockForCallOffset || m_cachedCodeBlockForConstructOffset);

    RefPtr<Decoder> decoder = WTFMove(m_decoder);
    int32_t cachedCodeBlockForCallOffset = m_cachedCodeBlockForCallOffset;
    int32_t cachedCodeBlockForConstructOffset = m_cachedCodeBlockForConstructOffset;

    DeferGC deferGC(vm.heap);

    // No need to clear m_unlinkedCodeBlockForCall here, since we moved the decoder out of the same slot
    if (cachedCodeBlockForCallOffset)
        decodeFunctionCodeBlock(*decoder, cachedCodeBlockForCallOffset, m_unlinkedCodeBlockForCall, this);
    if (cachedCodeBlockForConstructOffset)
        decodeFunctionCodeBlock(*decoder, cachedCodeBlockForConstructOffset, m_unlinkedCodeBlockForConstruct, this);
    else
        m_unlinkedCodeBlockForConstruct.clear();

    WTF::storeStoreFence();
    m_isCached = false;
    vm.heap.writeBarrier(this);
}

UnlinkedFunctionExecutable::RareData& UnlinkedFunctionExecutable::ensureRareDataSlow()
{
    ASSERT(!m_rareData);
    m_rareData = makeUnique<RareData>();
    return *m_rareData;
}

void UnlinkedFunctionExecutable::setInvalidTypeProfilingOffsets()
{
    m_typeProfilingStartOffset = std::numeric_limits<unsigned>::max();
    m_typeProfilingEndOffset = std::numeric_limits<unsigned>::max();
}

void UnlinkedFunctionExecutable::finalizeUnconditionally(VM& vm)
{
    if (codeBlockEdgeMayBeWeak()) {
        bool isCleared = false;
        bool isStillValid = false;
        auto clearIfDead = [&] (WriteBarrier<UnlinkedFunctionCodeBlock>& unlinkedCodeBlock) {
            if (!unlinkedCodeBlock)
                return;
            if (!vm.heap.isMarked(unlinkedCodeBlock.get())) {
                unlinkedCodeBlock.clear();
                isCleared = true;
            } else
                isStillValid = true;
        };
        clearIfDead(m_unlinkedCodeBlockForCall);
        clearIfDead(m_unlinkedCodeBlockForConstruct);
        if (isCleared && !isStillValid)
            vm.unlinkedFunctionExecutableSpace.set.remove(this);
    }
}

} // namespace JSC
