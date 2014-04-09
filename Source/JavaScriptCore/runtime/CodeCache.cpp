/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
};

template <> struct CacheTypes<UnlinkedEvalCodeBlock> {
    typedef JSC::EvalNode RootNode;
    static const SourceCodeKey::CodeType codeType = SourceCodeKey::EvalType;
};

template <class UnlinkedCodeBlockType, class ExecutableType>
UnlinkedCodeBlockType* CodeCache::getGlobalCodeBlock(VM& vm, ExecutableType* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    SourceCodeKey key = SourceCodeKey(source, String(), CacheTypes<UnlinkedCodeBlockType>::codeType, strictness);
    CodeCacheMap::AddResult addResult = m_sourceCode.add(key, SourceCodeValue());
    bool canCache = debuggerMode == DebuggerOff && profilerMode == ProfilerOff;
    if (!addResult.isNewEntry && canCache) {
        UnlinkedCodeBlockType* unlinkedCodeBlock = jsCast<UnlinkedCodeBlockType*>(addResult.iterator->value.cell.get());
        unsigned firstLine = source.firstLine() + unlinkedCodeBlock->firstLine();
        unsigned lineCount = unlinkedCodeBlock->lineCount();
        unsigned startColumn = unlinkedCodeBlock->startColumn() + source.startColumn();
        bool endColumnIsOnStartLine = !lineCount;
        unsigned endColumn = unlinkedCodeBlock->endColumn() + (endColumnIsOnStartLine ? startColumn : 1);
        executable->recordParse(unlinkedCodeBlock->codeFeatures(), unlinkedCodeBlock->hasCapturedVariables(), firstLine, firstLine + lineCount, startColumn, endColumn);
        return unlinkedCodeBlock;
    }

    typedef typename CacheTypes<UnlinkedCodeBlockType>::RootNode RootNode;
    RefPtr<RootNode> rootNode = parse<RootNode>(&vm, source, 0, Identifier(), strictness, JSParseProgramCode, JSNotFunctionKind, error);
    if (!rootNode) {
        m_sourceCode.remove(addResult.iterator);
        return 0;
    }
    unsigned lineCount = rootNode->lastLine() - rootNode->lineNo();
    unsigned startColumn = rootNode->startColumn() + 1;
    bool endColumnIsOnStartLine = !lineCount;
    unsigned unlinkedEndColumn = rootNode->endColumn();
    unsigned endColumn = unlinkedEndColumn + (endColumnIsOnStartLine ? startColumn : 1);
    executable->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->lineNo(), rootNode->lastLine(), startColumn, endColumn);

    UnlinkedCodeBlockType* unlinkedCodeBlock = UnlinkedCodeBlockType::create(&vm, executable->executableInfo());
    unlinkedCodeBlock->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->lineNo() - source.firstLine(), lineCount, unlinkedEndColumn);

    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(vm, rootNode.get(), unlinkedCodeBlock, debuggerMode, profilerMode)));
    error = generator->generate();
    rootNode->destroyData();
    if (error.m_type != ParserError::ErrorNone) {
        m_sourceCode.remove(addResult.iterator);
        return 0;
    }

    if (!canCache) {
        m_sourceCode.remove(addResult.iterator);
        return unlinkedCodeBlock;
    }

    addResult.iterator->value = SourceCodeValue(vm, unlinkedCodeBlock, m_sourceCode.age());
    return unlinkedCodeBlock;
}

UnlinkedProgramCodeBlock* CodeCache::getProgramCodeBlock(VM& vm, ProgramExecutable* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return getGlobalCodeBlock<UnlinkedProgramCodeBlock>(vm, executable, source, strictness, debuggerMode, profilerMode, error);
}

UnlinkedEvalCodeBlock* CodeCache::getEvalCodeBlock(VM& vm, EvalExecutable* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return getGlobalCodeBlock<UnlinkedEvalCodeBlock>(vm, executable, source, strictness, debuggerMode, profilerMode, error);
}

UnlinkedFunctionExecutable* CodeCache::getFunctionExecutableFromGlobalCode(VM& vm, const Identifier& name, const SourceCode& source, ParserError& error)
{
    SourceCodeKey key = SourceCodeKey(source, name.string(), SourceCodeKey::FunctionType, JSParseNormal);
    CodeCacheMap::AddResult addResult = m_sourceCode.add(key, SourceCodeValue());
    if (!addResult.isNewEntry)
        return jsCast<UnlinkedFunctionExecutable*>(addResult.iterator->value.cell.get());

    JSTextPosition positionBeforeLastNewline;
    RefPtr<ProgramNode> program = parse<ProgramNode>(&vm, source, 0, Identifier(), JSParseNormal, JSParseProgramCode, JSNotFunctionKind, error, &positionBeforeLastNewline);
    if (!program) {
        RELEASE_ASSERT(error.m_type != ParserError::ErrorNone);
        m_sourceCode.remove(addResult.iterator);
        return 0;
    }

    // This function assumes an input string that would result in a single anonymous function expression.
    StatementNode* exprStatement = program->singleStatement();
    RELEASE_ASSERT(exprStatement);
    RELEASE_ASSERT(exprStatement->isExprStatement());
    ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
    RELEASE_ASSERT(funcExpr);
    RELEASE_ASSERT(funcExpr->isFuncExprNode());
    FunctionBodyNode* body = static_cast<FuncExprNode*>(funcExpr)->body();
    RELEASE_ASSERT(!program->hasCapturedVariables());
    
    body->setEndPosition(positionBeforeLastNewline);
    RELEASE_ASSERT(body);
    RELEASE_ASSERT(body->ident().isNull());

    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(&vm, source, body, true, UnlinkedNormalFunction);
    functionExecutable->m_nameValue.set(vm, functionExecutable, jsString(&vm, name.string()));

    addResult.iterator->value = SourceCodeValue(vm, functionExecutable, m_sourceCode.age());
    return functionExecutable;
}

}
