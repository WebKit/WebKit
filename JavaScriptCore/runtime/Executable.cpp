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
#include "JIT.h"
#include "Parser.h"
#include "UStringBuilder.h"
#include "Vector.h"

namespace JSC {

#if ENABLE(JIT)
NativeExecutable::~NativeExecutable()
{
}
#endif

VPtrHackExecutable::~VPtrHackExecutable()
{
}

EvalExecutable::EvalExecutable(ExecState* exec, const SourceCode& source, bool inStrictContext)
    : ScriptExecutable(exec, source, inStrictContext)
{
}

EvalExecutable::~EvalExecutable()
{
}

ProgramExecutable::ProgramExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec, source, false)
{
}

ProgramExecutable::~ProgramExecutable()
{
}

FunctionExecutable::FunctionExecutable(JSGlobalData* globalData, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, bool inStrictContext, int firstLine, int lastLine)
    : ScriptExecutable(globalData, source, inStrictContext)
    , m_numCapturedVariables(0)
    , m_forceUsesArguments(forceUsesArguments)
    , m_parameters(parameters)
    , m_name(name)
    , m_symbolTable(0)
{
    m_firstLine = firstLine;
    m_lastLine = lastLine;
}

FunctionExecutable::FunctionExecutable(ExecState* exec, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, bool inStrictContext, int firstLine, int lastLine)
    : ScriptExecutable(exec, source, inStrictContext)
    , m_numCapturedVariables(0)
    , m_forceUsesArguments(forceUsesArguments)
    , m_parameters(parameters)
    , m_name(name)
    , m_symbolTable(0)
{
    m_firstLine = firstLine;
    m_lastLine = lastLine;
}

FunctionExecutable::~FunctionExecutable()
{
}

JSObject* EvalExecutable::compileInternal(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    JSObject* exception = 0;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<EvalNode> evalNode = globalData->parser->parse<EvalNode>(lexicalGlobalObject, lexicalGlobalObject->debugger(), exec, m_source, 0, isStrictMode() ? JSParseStrict : JSParseNormal, &exception);
    if (!evalNode) {
        ASSERT(exception);
        return exception;
    }
    recordParse(evalNode->features(), evalNode->hasCapturedVariables(), evalNode->lineNo(), evalNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_evalCodeBlock);
    m_evalCodeBlock = adoptPtr(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth()));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(evalNode.get(), scopeChain, m_evalCodeBlock->symbolTable(), m_evalCodeBlock.get())));
    generator->generate();
    
    evalNode->destroyData();

#if ENABLE(JIT)
    if (exec->globalData().canUseJIT()) {
        m_jitCodeForCall = JIT::compile(scopeChainNode->globalData, m_evalCodeBlock.get());
#if !ENABLE(OPCODE_SAMPLING)
        if (!BytecodeGenerator::dumpsGeneratedCode())
            m_evalCodeBlock->discardBytecode();
#endif
    }
#endif

    return 0;
}

JSObject* ProgramExecutable::checkSyntax(ExecState* exec)
{
    JSObject* exception = 0;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> programNode = globalData->parser->parse<ProgramNode>(lexicalGlobalObject, lexicalGlobalObject->debugger(), exec, m_source, 0, JSParseNormal, &exception);
    if (programNode)
        return 0;
    ASSERT(exception);
    return exception;
}

JSObject* ProgramExecutable::compileInternal(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    ASSERT(!m_programCodeBlock);

    JSObject* exception = 0;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> programNode = globalData->parser->parse<ProgramNode>(lexicalGlobalObject, lexicalGlobalObject->debugger(), exec, m_source, 0, isStrictMode() ? JSParseStrict : JSParseNormal, &exception);
    if (!programNode) {
        ASSERT(exception);
        return exception;
    }
    recordParse(programNode->features(), programNode->hasCapturedVariables(), programNode->lineNo(), programNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    m_programCodeBlock = adoptPtr(new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider()));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(programNode.get(), scopeChain, &globalObject->symbolTable(), m_programCodeBlock.get())));
    generator->generate();

    programNode->destroyData();

#if ENABLE(JIT)
    if (exec->globalData().canUseJIT()) {
        m_jitCodeForCall = JIT::compile(scopeChainNode->globalData, m_programCodeBlock.get());
#if !ENABLE(OPCODE_SAMPLING)
        if (!BytecodeGenerator::dumpsGeneratedCode())
            m_programCodeBlock->discardBytecode();
#endif
    }
#endif

   return 0;
}

JSObject* FunctionExecutable::compileForCallInternal(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    JSObject* exception = 0;
    JSGlobalData* globalData = scopeChainNode->globalData;
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(exec->lexicalGlobalObject(), 0, 0, m_source, m_parameters.get(), isStrictMode() ? JSParseStrict : JSParseNormal, &exception);
    if (!body) {
        ASSERT(exception);
        return exception;
    }
    if (m_forceUsesArguments)
        body->setUsesArguments();
    body->finishParsing(m_parameters, m_name);
    recordParse(body->features(), body->hasCapturedVariables(), body->lineNo(), body->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_codeBlockForCall);
    m_codeBlockForCall = adoptPtr(new FunctionCodeBlock(this, FunctionCode, globalObject, source().provider(), source().startOffset(), false));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), scopeChain, m_codeBlockForCall->symbolTable(), m_codeBlockForCall.get())));
    generator->generate();
    m_numParametersForCall = m_codeBlockForCall->m_numParameters;
    ASSERT(m_numParametersForCall);
    m_numCapturedVariables = m_codeBlockForCall->m_numCapturedVars;
    m_symbolTable = m_codeBlockForCall->sharedSymbolTable();

    body->destroyData();

#if ENABLE(JIT)
    if (exec->globalData().canUseJIT()) {
        m_jitCodeForCall = JIT::compile(scopeChainNode->globalData, m_codeBlockForCall.get(), &m_jitCodeForCallWithArityCheck);
#if !ENABLE(OPCODE_SAMPLING)
        if (!BytecodeGenerator::dumpsGeneratedCode())
            m_codeBlockForCall->discardBytecode();
#endif
    }
#endif

    return 0;
}

JSObject* FunctionExecutable::compileForConstructInternal(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    JSObject* exception = 0;
    JSGlobalData* globalData = scopeChainNode->globalData;
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(exec->lexicalGlobalObject(), 0, 0, m_source, m_parameters.get(), isStrictMode() ? JSParseStrict : JSParseNormal, &exception);
    if (!body) {
        ASSERT(exception);
        return exception;
    }
    if (m_forceUsesArguments)
        body->setUsesArguments();
    body->finishParsing(m_parameters, m_name);
    recordParse(body->features(), body->hasCapturedVariables(), body->lineNo(), body->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_codeBlockForConstruct);
    m_codeBlockForConstruct = adoptPtr(new FunctionCodeBlock(this, FunctionCode, globalObject, source().provider(), source().startOffset(), true));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), scopeChain, m_codeBlockForConstruct->symbolTable(), m_codeBlockForConstruct.get())));
    generator->generate();
    m_numParametersForConstruct = m_codeBlockForConstruct->m_numParameters;
    ASSERT(m_numParametersForConstruct);
    m_numCapturedVariables = m_codeBlockForConstruct->m_numCapturedVars;
    m_symbolTable = m_codeBlockForConstruct->sharedSymbolTable();

    body->destroyData();

#if ENABLE(JIT)
    if (exec->globalData().canUseJIT()) {
        m_jitCodeForConstruct = JIT::compile(scopeChainNode->globalData, m_codeBlockForConstruct.get(), &m_jitCodeForConstructWithArityCheck);
#if !ENABLE(OPCODE_SAMPLING)
        if (!BytecodeGenerator::dumpsGeneratedCode())
            m_codeBlockForConstruct->discardBytecode();
#endif
    }
#endif

    return 0;
}

void FunctionExecutable::markAggregate(MarkStack& markStack)
{
    if (m_codeBlockForCall)
        m_codeBlockForCall->markAggregate(markStack);
    if (m_codeBlockForConstruct)
        m_codeBlockForConstruct->markAggregate(markStack);
}

void FunctionExecutable::recompile(ExecState*)
{
    m_codeBlockForCall.clear();
    m_codeBlockForConstruct.clear();
    m_numParametersForCall = NUM_PARAMETERS_NOT_COMPILED;
    m_numParametersForConstruct = NUM_PARAMETERS_NOT_COMPILED;
#if ENABLE(JIT)
    m_jitCodeForCall = JITCode();
    m_jitCodeForConstruct = JITCode();
#endif
}

PassRefPtr<FunctionExecutable> FunctionExecutable::fromGlobalCode(const Identifier& functionName, ExecState* exec, Debugger* debugger, const SourceCode& source, JSObject** exception)
{
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> program = exec->globalData().parser->parse<ProgramNode>(lexicalGlobalObject, debugger, exec, source, 0, JSParseNormal, exception);
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

    return FunctionExecutable::create(&exec->globalData(), functionName, body->source(), body->usesArguments(), body->parameters(), body->isStrictMode(), body->lineNo(), body->lastLine());
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
