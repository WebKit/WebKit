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
#include "StringBuilder.h"
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

EvalExecutable::EvalExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec, source)
{
}

EvalExecutable::~EvalExecutable()
{
}

ProgramExecutable::ProgramExecutable(ExecState* exec, const SourceCode& source)
    : ScriptExecutable(exec, source)
{
}

ProgramExecutable::~ProgramExecutable()
{
}

FunctionExecutable::FunctionExecutable(JSGlobalData* globalData, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, int firstLine, int lastLine)
    : ScriptExecutable(globalData, source)
    , m_numVariables(0)
    , m_forceUsesArguments(forceUsesArguments)
    , m_parameters(parameters)
    , m_name(name)
    , m_symbolTable(0)
{
    m_firstLine = firstLine;
    m_lastLine = lastLine;
}

FunctionExecutable::FunctionExecutable(ExecState* exec, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, int firstLine, int lastLine)
    : ScriptExecutable(exec, source)
    , m_numVariables(0)
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
    RefPtr<EvalNode> evalNode = globalData->parser->parse<EvalNode>(globalData, lexicalGlobalObject, lexicalGlobalObject->debugger(), exec, m_source, &exception);
    if (!evalNode) {
        ASSERT(exception);
        return exception;
    }
    recordParse(evalNode->features(), evalNode->hasCapturedVariables(), evalNode->lineNo(), evalNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_evalCodeBlock);
    m_evalCodeBlock = adoptPtr(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth()));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(evalNode.get(), globalObject->debugger(), scopeChain, m_evalCodeBlock->symbolTable(), m_evalCodeBlock.get())));
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
    RefPtr<ProgramNode> programNode = globalData->parser->parse<ProgramNode>(globalData, lexicalGlobalObject, lexicalGlobalObject->debugger(), exec, m_source, &exception);
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
    RefPtr<ProgramNode> programNode = globalData->parser->parse<ProgramNode>(globalData, lexicalGlobalObject, lexicalGlobalObject->debugger(), exec, m_source, &exception);
    if (!programNode) {
        ASSERT(exception);
        return exception;
    }
    recordParse(programNode->features(), programNode->hasCapturedVariables(), programNode->lineNo(), programNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    m_programCodeBlock = adoptPtr(new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider()));
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(programNode.get(), globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_programCodeBlock.get())));
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
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(globalData, exec->lexicalGlobalObject(), 0, 0, m_source, &exception);
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
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), globalObject->debugger(), scopeChain, m_codeBlockForCall->symbolTable(), m_codeBlockForCall.get())));
    generator->generate();
    m_numParametersForCall = m_codeBlockForCall->m_numParameters;
    ASSERT(m_numParametersForCall);
    m_numVariables = m_codeBlockForCall->m_numVars;
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
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(globalData, exec->lexicalGlobalObject(), 0, 0, m_source, &exception);
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
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), globalObject->debugger(), scopeChain, m_codeBlockForConstruct->symbolTable(), m_codeBlockForConstruct.get())));
    generator->generate();
    m_numParametersForConstruct = m_codeBlockForConstruct->m_numParameters;
    ASSERT(m_numParametersForConstruct);
    m_numVariables = m_codeBlockForConstruct->m_numVars;
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

PassOwnPtr<ExceptionInfo> FunctionExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    JSObject* exception = 0;
    RefPtr<FunctionBodyNode> newFunctionBody = globalData->parser->parse<FunctionBodyNode>(globalData, 0, 0, 0, m_source, &exception);
    if (!newFunctionBody)
        return PassOwnPtr<ExceptionInfo>();
    if (m_forceUsesArguments)
        newFunctionBody->setUsesArguments();
    newFunctionBody->finishParsing(m_parameters, m_name);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<CodeBlock> newCodeBlock(adoptPtr(new FunctionCodeBlock(this, FunctionCode, globalObject, source().provider(), source().startOffset(), codeBlock->m_isConstructor)));
    globalData->functionCodeBlockBeingReparsed = newCodeBlock.get();

    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(newFunctionBody.get(), globalObject->debugger(), scopeChain, newCodeBlock->symbolTable(), newCodeBlock.get())));
    generator->setRegeneratingForExceptionInfo(static_cast<FunctionCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
    if (globalData->canUseJIT()) {
        JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get(), 0, codeBlock->m_isConstructor ? generatedJITCodeForConstruct().start() : generatedJITCodeForCall().start());
        ASSERT(codeBlock->m_isConstructor ? newJITCode.size() == generatedJITCodeForConstruct().size() : newJITCode.size() == generatedJITCodeForCall().size());
    }
#endif

    globalData->functionCodeBlockBeingReparsed = 0;

    return newCodeBlock->extractExceptionInfo();
}

PassOwnPtr<ExceptionInfo> EvalExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    JSObject* exception = 0;
    RefPtr<EvalNode> newEvalBody = globalData->parser->parse<EvalNode>(globalData, 0, 0, 0, m_source, &exception);
    if (!newEvalBody)
        return PassOwnPtr<ExceptionInfo>();

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<EvalCodeBlock> newCodeBlock(adoptPtr(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth())));

    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(newEvalBody.get(), globalObject->debugger(), scopeChain, newCodeBlock->symbolTable(), newCodeBlock.get())));
    generator->setRegeneratingForExceptionInfo(static_cast<EvalCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
    if (globalData->canUseJIT()) {
        JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get(), 0, generatedJITCodeForCall().start());
        ASSERT(newJITCode.size() == generatedJITCodeForCall().size());
    }
#endif

    return newCodeBlock->extractExceptionInfo();
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
    RefPtr<ProgramNode> program = exec->globalData().parser->parse<ProgramNode>(&exec->globalData(), lexicalGlobalObject, debugger, exec, source, exception);
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

    return FunctionExecutable::create(&exec->globalData(), functionName, body->source(), body->usesArguments(), body->parameters(), body->lineNo(), body->lastLine());
}

UString FunctionExecutable::paramString() const
{
    FunctionParameters& parameters = *m_parameters;
    StringBuilder builder;
    for (size_t pos = 0; pos < parameters.size(); ++pos) {
        if (!builder.isEmpty())
            builder.append(", ");
        builder.append(parameters[pos].ustring());
    }
    return builder.build();
}

PassOwnPtr<ExceptionInfo> ProgramExecutable::reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*)
{
    // CodeBlocks for program code are transient and therefore do not gain from from throwing out their exception information.
    return PassOwnPtr<ExceptionInfo>();
}

}
