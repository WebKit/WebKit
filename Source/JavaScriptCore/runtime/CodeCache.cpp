/*
 * Copyright (C) 2012, 2016 Apple Inc. All Rights Reserved.
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

#include "CodeCache.h"

#include "BytecodeGenerator.h"
#include "CodeSpecializationKind.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "StrongInlines.h"
#include "UnlinkedCodeBlock.h"

namespace JSC {

const double CodeCacheMap::workingSetTime = 10.0;

void CodeCacheMap::pruneSlowCase()
{
    m_minCapacity = std::max(m_size - m_sizeAtLastPrune, static_cast<int64_t>(0));
    m_sizeAtLastPrune = m_size;
    m_timeAtLastPrune = monotonicallyIncreasingTime();

    if (m_capacity < m_minCapacity)
        m_capacity = m_minCapacity;

    while (m_size > m_capacity || !canPruneQuickly()) {
        MapType::iterator it = m_map.begin();
        m_size -= it->key.length();
        m_map.remove(it);
    }
}

CodeCache::CodeCache()
{
}

CodeCache::~CodeCache()
{
}

template <typename T> struct CacheTypes { };

template <> struct CacheTypes<UnlinkedProgramCodeBlock> {
    typedef JSC::ProgramNode RootNode;
    static const SourceCodeKey::CodeType codeType = SourceCodeKey::ProgramType;
    static const SourceParseMode parseMode = SourceParseMode::ProgramMode;
};

template <> struct CacheTypes<UnlinkedEvalCodeBlock> {
    typedef JSC::EvalNode RootNode;
    static const SourceCodeKey::CodeType codeType = SourceCodeKey::EvalType;
    static const SourceParseMode parseMode = SourceParseMode::ProgramMode;
};

template <> struct CacheTypes<UnlinkedModuleProgramCodeBlock> {
    typedef JSC::ModuleProgramNode RootNode;
    static const SourceCodeKey::CodeType codeType = SourceCodeKey::ModuleType;
    static const SourceParseMode parseMode = SourceParseMode::ModuleEvaluateMode;
};

template <class UnlinkedCodeBlockType, class ExecutableType>
UnlinkedCodeBlockType* CodeCache::getGlobalCodeBlock(VM& vm, ExecutableType* executable, const SourceCode& source, JSParserBuiltinMode builtinMode, JSParserStrictMode strictMode, ThisTDZMode thisTDZMode, bool, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error, const VariableEnvironment* variablesUnderTDZ)
{
    SourceCodeKey key = SourceCodeKey(source, String(), CacheTypes<UnlinkedCodeBlockType>::codeType, builtinMode, strictMode, thisTDZMode);
    SourceCodeValue* cache = m_sourceCode.findCacheAndUpdateAge(key);
    // FIXME: We should do something smart for TDZ instead of just disabling caching.
    // https://bugs.webkit.org/show_bug.cgi?id=154010
    bool canCache = debuggerMode == DebuggerOff && profilerMode == ProfilerOff && !vm.typeProfiler() && !vm.controlFlowProfiler() && !variablesUnderTDZ->size();
    if (cache && canCache) {
        UnlinkedCodeBlockType* unlinkedCodeBlock = jsCast<UnlinkedCodeBlockType*>(cache->cell.get());
        unsigned firstLine = source.firstLine() + unlinkedCodeBlock->firstLine();
        unsigned lineCount = unlinkedCodeBlock->lineCount();
        unsigned startColumn = unlinkedCodeBlock->startColumn() + source.startColumn();
        bool endColumnIsOnStartLine = !lineCount;
        unsigned endColumn = unlinkedCodeBlock->endColumn() + (endColumnIsOnStartLine ? startColumn : 1);
        executable->recordParse(unlinkedCodeBlock->codeFeatures(), unlinkedCodeBlock->hasCapturedVariables(), firstLine, firstLine + lineCount, startColumn, endColumn);
        return unlinkedCodeBlock;
    }

    typedef typename CacheTypes<UnlinkedCodeBlockType>::RootNode RootNode;
    std::unique_ptr<RootNode> rootNode = parse<RootNode>(
        &vm, source, Identifier(), builtinMode, strictMode,
        CacheTypes<UnlinkedCodeBlockType>::parseMode, SuperBinding::NotNeeded, error, nullptr, ConstructorKind::None, thisTDZMode);
    if (!rootNode)
        return nullptr;

    unsigned lineCount = rootNode->lastLine() - rootNode->firstLine();
    unsigned startColumn = rootNode->startColumn() + 1;
    bool endColumnIsOnStartLine = !lineCount;
    unsigned unlinkedEndColumn = rootNode->endColumn();
    unsigned endColumn = unlinkedEndColumn + (endColumnIsOnStartLine ? startColumn : 1);
    unsigned arrowContextFeature = executable->isArrowFunctionContext() ? ArrowFunctionContextFeature : 0;
    executable->recordParse(rootNode->features() | arrowContextFeature, rootNode->hasCapturedVariables(), rootNode->firstLine(), rootNode->lastLine(), startColumn, endColumn);

    UnlinkedCodeBlockType* unlinkedCodeBlock = UnlinkedCodeBlockType::create(&vm, executable->executableInfo());
    unlinkedCodeBlock->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->firstLine() - source.firstLine(), lineCount, unlinkedEndColumn);

    auto generator = std::make_unique<BytecodeGenerator>(vm, rootNode.get(), unlinkedCodeBlock, debuggerMode, profilerMode, variablesUnderTDZ);
    error = generator->generate();
    if (error.isValid())
        return nullptr;

    if (!canCache)
        return unlinkedCodeBlock;

    m_sourceCode.addCache(key, SourceCodeValue(vm, unlinkedCodeBlock, m_sourceCode.age()));
    return unlinkedCodeBlock;
}

UnlinkedProgramCodeBlock* CodeCache::getProgramCodeBlock(VM& vm, ProgramExecutable* executable, const SourceCode& source, JSParserBuiltinMode builtinMode, JSParserStrictMode strictMode, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    VariableEnvironment emptyParentTDZVariables;
    return getGlobalCodeBlock<UnlinkedProgramCodeBlock>(vm, executable, source, builtinMode, strictMode, ThisTDZMode::CheckIfNeeded, false, debuggerMode, profilerMode, error, &emptyParentTDZVariables);
}

UnlinkedEvalCodeBlock* CodeCache::getEvalCodeBlock(VM& vm, EvalExecutable* executable, const SourceCode& source, JSParserBuiltinMode builtinMode, JSParserStrictMode strictMode, ThisTDZMode thisTDZMode, bool isArrowFunctionContext, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error, const VariableEnvironment* variablesUnderTDZ)
{
    return getGlobalCodeBlock<UnlinkedEvalCodeBlock>(vm, executable, source, builtinMode, strictMode, thisTDZMode, isArrowFunctionContext, debuggerMode, profilerMode, error, variablesUnderTDZ);
}

UnlinkedModuleProgramCodeBlock* CodeCache::getModuleProgramCodeBlock(VM& vm, ModuleProgramExecutable* executable, const SourceCode& source, JSParserBuiltinMode builtinMode, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    VariableEnvironment emptyParentTDZVariables;
    return getGlobalCodeBlock<UnlinkedModuleProgramCodeBlock>(vm, executable, source, builtinMode, JSParserStrictMode::Strict, ThisTDZMode::CheckIfNeeded, false, debuggerMode, profilerMode, error, &emptyParentTDZVariables);
}

// FIXME: There's no need to add the function's name to the key here. It's already in the source code.
UnlinkedFunctionExecutable* CodeCache::getFunctionExecutableFromGlobalCode(VM& vm, const Identifier& name, const SourceCode& source, ParserError& error)
{
    SourceCodeKey key = SourceCodeKey(
        source, name.string(), SourceCodeKey::FunctionType, 
        JSParserBuiltinMode::NotBuiltin, 
        JSParserStrictMode::NotStrict);
    SourceCodeValue* cache = m_sourceCode.findCacheAndUpdateAge(key);
    if (cache)
        return jsCast<UnlinkedFunctionExecutable*>(cache->cell.get());

    JSTextPosition positionBeforeLastNewline;
    std::unique_ptr<ProgramNode> program = parse<ProgramNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::NotStrict, SourceParseMode::ProgramMode, SuperBinding::NotNeeded,
        error, &positionBeforeLastNewline);
    if (!program) {
        RELEASE_ASSERT(error.isValid());
        return nullptr;
    }

    // This function assumes an input string that would result in a single function declaration.
    StatementNode* statement = program->singleStatement();
    ASSERT(statement);
    ASSERT(statement->isBlock());
    if (!statement || !statement->isBlock())
        return nullptr;

    StatementNode* funcDecl = static_cast<BlockNode*>(statement)->singleStatement();
    ASSERT(funcDecl);
    ASSERT(funcDecl->isFuncDeclNode());
    if (!funcDecl || !funcDecl->isFuncDeclNode())
        return nullptr;

    FunctionMetadataNode* metadata = static_cast<FuncDeclNode*>(funcDecl)->metadata();
    ASSERT(metadata);
    if (!metadata)
        return nullptr;
    
    metadata->setEndPosition(positionBeforeLastNewline);
    // The Function constructor only has access to global variables, so no variables will be under TDZ.
    VariableEnvironment emptyTDZVariables;
    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(&vm, source, metadata, UnlinkedNormalFunction, ConstructAbility::CanConstruct, emptyTDZVariables, DerivedContextType::None);

    functionExecutable->m_nameValue.set(vm, functionExecutable, jsString(&vm, name.string()));

    m_sourceCode.addCache(key, SourceCodeValue(vm, functionExecutable, m_sourceCode.age()));
    return functionExecutable;
}

}
