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
#include "Parser.h"
#include "StrongInlines.h"
#include "UnlinkedCodeBlock.h"

namespace JSC {

CodeCache::CodeCache()
    : m_randomGenerator(static_cast<uint32_t>(randomNumber() * UINT32_MAX))
{
}

CodeCache::~CodeCache()
{
}

CodeCache::CodeBlockKey CodeCache::makeCodeBlockKey(const SourceCode& source, CodeCache::CodeType type, JSParserStrictness strictness)
{
    return std::make_pair(source.toString(), (type << 1) | strictness);
}

template <typename T> struct CacheTypes { };

template <> struct CacheTypes<UnlinkedProgramCodeBlock> {
    typedef JSC::ProgramNode RootNode;
    static const CodeCache::CodeType codeType = CodeCache::ProgramType;
};

template <> struct CacheTypes<UnlinkedEvalCodeBlock> {
    typedef JSC::EvalNode RootNode;
    static const CodeCache::CodeType codeType = CodeCache::EvalType;
};

template <class UnlinkedCodeBlockType, class ExecutableType>
UnlinkedCodeBlockType* CodeCache::getCodeBlock(JSGlobalData& globalData, ExecutableType* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    CodeBlockKey key = makeCodeBlockKey(source, CacheTypes<UnlinkedCodeBlockType>::codeType, strictness);
    bool storeInCache = false;
    if (debuggerMode == DebuggerOff && profilerMode == ProfilerOff) {
        CodeBlockIndicesMap::iterator result = m_cachedCodeBlockIndices.find(key);
        if (result != m_cachedCodeBlockIndices.end()) {
            UnlinkedCodeBlockType* unlinkedCode = jsCast<UnlinkedCodeBlockType*>(m_cachedCodeBlocks[result->value].second.get());
            unsigned firstLine = source.firstLine() + unlinkedCode->firstLine();
            executable->recordParse(unlinkedCode->codeFeatures(), unlinkedCode->hasCapturedVariables(), firstLine, firstLine + unlinkedCode->lineCount());
            return unlinkedCode;
        }
        storeInCache = true;
    }

    typedef typename CacheTypes<UnlinkedCodeBlockType>::RootNode RootNode;
    RefPtr<RootNode> rootNode = parse<RootNode>(&globalData, source, 0, Identifier(), strictness, JSParseProgramCode, error);
    if (!rootNode)
        return 0;
    executable->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->lineNo(), rootNode->lastLine());

    UnlinkedCodeBlockType* unlinkedCode = UnlinkedCodeBlockType::create(&globalData, executable->executableInfo());
    unlinkedCode->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), rootNode->lineNo() - source.firstLine(), rootNode->lastLine() - rootNode->lineNo());
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(globalData, rootNode.get(), unlinkedCode, debuggerMode, profilerMode)));
    error = generator->generate();
    rootNode->destroyData();
    if (error.m_type != ParserError::ErrorNone)
        return 0;

    if (storeInCache) {
        size_t index = m_randomGenerator.getUint32() % kMaxCodeBlockEntries;
        if (m_cachedCodeBlocks[index].second)
            m_cachedCodeBlockIndices.remove(m_cachedCodeBlocks[index].first);
        m_cachedCodeBlockIndices.set(key, index);
        m_cachedCodeBlocks[index].second.set(globalData, unlinkedCode);
        m_cachedCodeBlocks[index].first = key;
    }

    return unlinkedCode;
}

UnlinkedProgramCodeBlock* CodeCache::getProgramCodeBlock(JSGlobalData& globalData, ProgramExecutable* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return getCodeBlock<UnlinkedProgramCodeBlock>(globalData, executable, source, strictness, debuggerMode, profilerMode, error);
}

UnlinkedEvalCodeBlock* CodeCache::getEvalCodeBlock(JSGlobalData& globalData, EvalExecutable* executable, const SourceCode& source, JSParserStrictness strictness, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return getCodeBlock<UnlinkedEvalCodeBlock>(globalData, executable, source, strictness, debuggerMode, profilerMode, error);
}

UnlinkedFunctionCodeBlock* CodeCache::generateFunctionCodeBlock(JSGlobalData& globalData, UnlinkedFunctionExecutable* executable, const SourceCode& source, CodeSpecializationKind kind, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    RefPtr<FunctionBodyNode> body = parse<FunctionBodyNode>(&globalData, source, executable->parameters(), executable->name(), executable->isInStrictContext() ? JSParseStrict : JSParseNormal, JSParseFunctionCode, error);

    if (!body) {
        ASSERT(error.m_type != ParserError::ErrorNone);
        return 0;
    }

    if (executable->forceUsesArguments())
        body->setUsesArguments();
    body->finishParsing(executable->parameters(), executable->name(), executable->functionNameIsInScopeToggle());
    executable->recordParse(body->features(), body->hasCapturedVariables(), body->lineNo(), body->lastLine());
    
    UnlinkedFunctionCodeBlock* result = UnlinkedFunctionCodeBlock::create(&globalData, FunctionCode, ExecutableInfo(body->needsActivation(), body->usesEval(), body->isStrictMode(), kind == CodeForConstruct));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(globalData, body.get(), result, debuggerMode, profilerMode)));
    error = generator->generate();
    body->destroyData();
    if (error.m_type != ParserError::ErrorNone)
        return 0;
    return result;
}

UnlinkedFunctionCodeBlock* CodeCache::getFunctionCodeBlock(JSGlobalData& globalData, UnlinkedFunctionExecutable* executable, const SourceCode& source, CodeSpecializationKind kind, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    return generateFunctionCodeBlock(globalData, executable, source, kind, debuggerMode, profilerMode, error);
}

CodeCache::GlobalFunctionKey CodeCache::makeGlobalFunctionKey(const SourceCode& source, const String& name)
{
    return GlobalFunctionKey(source.toString(), name);
}

UnlinkedFunctionExecutable* CodeCache::getFunctionExecutableFromGlobalCode(JSGlobalData& globalData, const Identifier& name, const SourceCode& source, ParserError& error)
{
    GlobalFunctionKey key = makeGlobalFunctionKey(source, name.string());
    GlobalFunctionIndicesMap::iterator result = m_cachedGlobalFunctionIndices.find(key);
    if (result != m_cachedGlobalFunctionIndices.end())
        return m_cachedGlobalFunctions[result->value].second.get();

    RefPtr<ProgramNode> program = parse<ProgramNode>(&globalData, source, 0, Identifier(), JSParseNormal, JSParseProgramCode, error);
    if (!program) {
        ASSERT(error.m_type != ParserError::ErrorNone);
        return 0;
    }

    // This function assumes an input string that would result in a single anonymous function expression.
    StatementNode* exprStatement = program->singleStatement();
    ASSERT(exprStatement);
    ASSERT(exprStatement->isExprStatement());
    ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
    ASSERT(funcExpr);
    ASSERT(funcExpr->isFuncExprNode());
    FunctionBodyNode* body = static_cast<FuncExprNode*>(funcExpr)->body();
    ASSERT(body);
    ASSERT(body->ident().isNull());

    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(&globalData, source, body);
    functionExecutable->m_nameValue.set(globalData, functionExecutable, jsString(&globalData, name.string()));

    size_t index = m_randomGenerator.getUint32() % kMaxGlobalFunctionEntries;
    if (m_cachedGlobalFunctions[index].second)
        m_cachedGlobalFunctionIndices.remove(m_cachedGlobalFunctions[index].first);
    m_cachedGlobalFunctionIndices.set(key, index);
    m_cachedGlobalFunctions[index].second.set(globalData, functionExecutable);
    m_cachedGlobalFunctions[index].first = key;

    return functionExecutable;
}

}
