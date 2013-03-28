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
#include "Operations.h"
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

    while (m_size > m_capacity) {
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
UnlinkedCodeBlockType* CodeCache::getCodeBlock(JSGlobalData& globalData, JSScope* scope, ExecutableType* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    SourceCodeKey key = SourceCodeKey(source, String(), CacheTypes<UnlinkedCodeBlockType>::codeType, strictness);
    CodeCacheMap::AddResult addResult = m_sourceCode.add(key, SourceCodeValue());
    bool canCache = debuggerMode == DebuggerOff && profilerMode == ProfilerOff;
    if (!addResult.isNewEntry && canCache) {
        UnlinkedCodeBlockType* unlinkedCode = jsCast<UnlinkedCodeBlockType*>(addResult.iterator->value.cell.get());
        unsigned firstLine = source.firstLine() + unlinkedCode->firstLine();
        executable->recordParse(unlinkedCode->codeFeatures(), unlinkedCode->hasCapturedVariables(), firstLine, firstLine + unlinkedCode->lineCount());
        return unlinkedCode;
    }

    typedef typename CacheTypes<UnlinkedCodeBlockType>::RootNode RootNode;
    RefPtr<RootNode> rootNode = parse<RootNode>(&globalData, source, 0, Identifier(), strictness, JSParseProgramCode, error);
    if (!rootNode) {
        m_sourceCode.remove(addResult.iterator);
        return 0;
    }
    executable->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->lineNo(), rootNode->lastLine());

    UnlinkedCodeBlockType* unlinkedCode = UnlinkedCodeBlockType::create(&globalData, executable->executableInfo());
    unlinkedCode->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->lineNo() - source.firstLine(), rootNode->lastLine() - rootNode->lineNo());
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(globalData, scope, rootNode.get(), unlinkedCode, debuggerMode, profilerMode)));
    error = generator->generate();
    rootNode->destroyData();
    if (error.m_type != ParserError::ErrorNone) {
        m_sourceCode.remove(addResult.iterator);
        return 0;
    }

    if (!canCache) {
        m_sourceCode.remove(addResult.iterator);
        return unlinkedCode;
    }

    addResult.iterator->value = SourceCodeValue(globalData, unlinkedCode, m_sourceCode.age());
    return unlinkedCode;
}

UnlinkedProgramCodeBlock* CodeCache::getProgramCodeBlock(JSGlobalData& globalData, ProgramExecutable* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return getCodeBlock<UnlinkedProgramCodeBlock>(globalData, 0, executable, source, strictness, debuggerMode, profilerMode, error);
}

UnlinkedEvalCodeBlock* CodeCache::getEvalCodeBlock(JSGlobalData& globalData, JSScope* scope, EvalExecutable* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return getCodeBlock<UnlinkedEvalCodeBlock>(globalData, scope, executable, source, strictness, debuggerMode, profilerMode, error);
}

UnlinkedFunctionExecutable* CodeCache::getFunctionExecutableFromGlobalCode(JSGlobalData& globalData, const Identifier& name, const SourceCode& source, ParserError& error)
{
    SourceCodeKey key = SourceCodeKey(source, name.string(), SourceCodeKey::FunctionType, JSParseNormal);
    CodeCacheMap::AddResult addResult = m_sourceCode.add(key, SourceCodeValue());
    if (!addResult.isNewEntry)
        return jsCast<UnlinkedFunctionExecutable*>(addResult.iterator->value.cell.get());

    RefPtr<ProgramNode> program = parse<ProgramNode>(&globalData, source, 0, Identifier(), JSParseNormal, JSParseProgramCode, error);
    if (!program) {
        ASSERT(error.m_type != ParserError::ErrorNone);
        m_sourceCode.remove(addResult.iterator);
        return 0;
    }

    // This function assumes an input string that would result in a single anonymous function expression.
    StatementNode* exprStatement = program->singleStatement();
    ASSERT(exprStatement);
    ASSERT(exprStatement->isExprStatement());
    ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
    ASSERT(funcExpr);
    RELEASE_ASSERT(funcExpr->isFuncExprNode());
    FunctionBodyNode* body = static_cast<FuncExprNode*>(funcExpr)->body();
    ASSERT(body);
    ASSERT(body->ident().isNull());

    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(&globalData, source, body);
    functionExecutable->m_nameValue.set(globalData, functionExecutable, jsString(&globalData, name.string()));

    addResult.iterator->value = SourceCodeValue(globalData, functionExecutable, m_sourceCode.age());
    return functionExecutable;
}

}
